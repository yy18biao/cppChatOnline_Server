#include <brpc/server.h>
#include <butil/logging.h>

#include "log.hpp"
#include "etcd.hpp"
#include "util.hpp"
#include "MChatSessionUser.hpp"
#include "MChatSession.hpp"
#include "channel.hpp"
#include "rabbitMQ.hpp"

#include "base.pb.h"
#include "user.pb.h"
#include "chatSession.pb.h"
#include "message.pb.h"

namespace hjb
{
    // 聊天会话服务类
    class ChatSessionServiceImpl : public ChatSessionService
    {
    private:
        std::string _userServiceName;        // 用户子服务的名称
        std::string _messageServiceName;     // 消息子服务名称
        AllServiceChannel::ptr _channels;    // 服务信道操作对象
        ChatSessionUserTable::ptr _csuTable; // 用户会话关联数据表操作对象
        MQClient::ptr _mqClient;             // rabbitMQ操作对象
        std::string _exchange;               // rabbitMQ交换机名称
        std::string _routing_key;            // rabbitMQ规则
        ChatSessionTable::ptr _mysql;        // 会话数据表操作对象

    public:
        ChatSessionServiceImpl(const std::shared_ptr<odb::core::database> &mysql,
                                   const AllServiceChannel::ptr &channels,
                                   const std::string &userServiceName,
                                   const std::string &messageServiceName,
                                   const std::string &exchange,
                                   const std::string &routing_key,
                                   const MQClient::ptr &mqClient)
            : _userServiceName(userServiceName),
              _messageServiceName(messageServiceName),
              _exchange(exchange),
              _routing_key(routing_key),
              _channels(channels),
              _csuTable(std::make_shared<ChatSessionUserTable>(mysql)),
              _mysql(std::make_shared<ChatSessionTable>(mysql)),
              _mqClient(mqClient)
        {
        }

        ~ChatSessionServiceImpl()
        {
        }

        // 消息转发业务处理(组织收到的新消息)
        void GetTransmitTarget(google::protobuf::RpcController *cntl_base,
                               const ::hjb::NewMessageReq *request,
                               ::hjb::GetTransmitTargetResp *response,
                               ::google::protobuf::Closure *done)
        {
            // 以 ARII 方式自动释放 done 对象
            brpc::ClosureGuard rpc_guard(done);

            // 错误处理函数(出错时调用)
            auto err = [this, response](const std::string &rid,
                                        const std::string &errmsg) -> void
            {
                response->set_requestid(rid);
                response->set_success(false);
                response->set_errmsg(errmsg);
                return;
            };

            // 获取消息的关键信息
            auto requestId = request->requestid();
            auto userId = request->userid();                    // 消息发送者id
            auto chatSessionId = request->chatsessionid();      // 所属聊天会话id
            const MessageContent &content = request->message(); // 消息数据

            // 向用户子服务获取发送者的用户数据
            auto channel = _channels->choose(_userServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到用户管理子服务节点 - {} - {}", requestId, _userServiceName, userId);
                return err(request->requestid(), "未找到用户管理子服务节点");
            }
            GetUserInfoReq req;
            GetUserInfoResp resp;
            brpc::Controller cntl;
            UserService_Stub stub(channel.get());
            req.set_requestid(requestId);
            req.set_userid(userId);
            stub.GetUserInfo(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 用户子服务调用失败：{}", request->requestid(), cntl.ErrorText());
                return err(request->requestid(), "用户子服务调用失败");
            }

            // 组织最终消息数据
            MessageInfo message;
            message.set_messageid(uuid());
            message.set_chatsessionid(chatSessionId);
            message.set_timestamp(time(nullptr));
            message.mutable_sender()->CopyFrom(resp.user());
            message.mutable_message()->CopyFrom(content);

            // 获取该聊天会话中的所有用户
            auto ids = _csuTable->all(chatSessionId);

            // 将组织好的消息发布到rabbitMQ消息队列
            if (!_mqClient->publish(_exchange, message.SerializeAsString(), _routing_key))
            {
                ERROR("{} - 持久化消息发布失败：{}", requestId, cntl.ErrorText());
                return err(requestId, "持久化消息发布失败");
            }

            // 组织响应
            response->set_requestid(requestId);
            response->set_success(true);
            response->mutable_message()->CopyFrom(message);
            for (const auto &i : ids)
                response->add_targetids(i);
        }

        // 获取聊天会话列表
        void GetChatSessionList(google::protobuf::RpcController *cntl_base,
                                const ::hjb::GetChatSessionListReq *request,
                                ::hjb::GetChatSessionListResp *response,
                                ::google::protobuf::Closure *done)
        {
            // 以 ARII 方式自动释放 done 对象
            brpc::ClosureGuard rpc_guard(done);

            // 错误处理函数(出错时调用)
            auto err = [this, response](const std::string &rid,
                                        const std::string &errmsg) -> void
            {
                response->set_requestid(rid);
                response->set_success(false);
                response->set_errmsg(errmsg);
                return;
            };

            // 提取请求中的相关属性
            std::string requestId = request->requestid();
            std::string userId = request->userid();

            // 从数据库获取当前用户id的所有所在单聊会话
            auto ssessions = _mysql->singleChatSession(userId);

            // 从单聊会话中取出所有的好友id
            // 从用户子服务获取好友的信息
            std::vector<std::string> friendIds;
            for (const auto &session : ssessions)
                friendIds.push_back(session.friendId);
            std::unordered_map<std::string, UserProto> friends;
            if (!_getUser(requestId, friendIds, friends))
            {
                ERROR("{} - 批量获取用户信息失败", requestId);
                return err(requestId, "批量获取用户信息失败");
            }

            // 获取所有群聊会话
            auto groupSessions = _mysql->groupChatSession(userId);

            // 组织响应会话里的信息
            for (const auto &session : ssessions)
            {
                auto sessionInfo = response->add_chatsessioninfos();
                sessionInfo->set_singlechatfriendid(session.friendId);
                sessionInfo->set_chatsessionid(session.chatSessionId);
                sessionInfo->set_chatsessionname(friends[session.friendId].nickname());
                sessionInfo->set_photo(friends[session.friendId].photo());
                // 获取最近一条消息
                MessageInfo msg;
                if (!_getRecentMsg(requestId, session.chatSessionId, msg))
                    continue;
                sessionInfo->mutable_prevmessage()->CopyFrom(msg);
            }
            for (const auto &session : groupSessions)
            {
                auto sessionInfo = response->add_chatsessioninfos();
                sessionInfo->set_chatsessionid(session.chatSessionId);
                sessionInfo->set_chatsessionname(session.chatSessionId);
                // 获取最近一条消息
                MessageInfo msg;
                if (!_getRecentMsg(requestId, session.chatSessionId, msg))
                    continue;
                sessionInfo->mutable_prevmessage()->CopyFrom(msg);
            }

            response->set_requestid(requestId);
            response->set_success(true);
        }

        // 创建聊天会话
        void ChatSessionCreate(google::protobuf::RpcController *cntl_base,
                               const ::hjb::ChatSessionCreateReq *request,
                               ::hjb::ChatSessionCreateResp *response,
                               ::google::protobuf::Closure *done)
        {
            // 以 ARII 方式自动释放 done 对象
            brpc::ClosureGuard rpc_guard(done);

            // 错误处理函数(出错时调用)
            auto err = [this, response](const std::string &rid,
                                        const std::string &errmsg) -> void
            {
                response->set_requestid(rid);
                response->set_success(false);
                response->set_errmsg(errmsg);
                return;
            };

            //
            std::string rid = request->requestid();
            std::string uid = request->userid();
            std::string sname = request->chatsessionname();

            // 生成会话id
            std::string sid = uuid();

            // 向数据库添加会话信息
            ChatSession cs(sid, sname, ChatSessionType::GROUP);
            if (!_mysql->insert(cs))
            {
                ERROR("{} - 向数据库添加会话信息失败: {}", rid, sname);
                return err(rid, "向数据库添加会话信息失败");
            }

            // 添加会话成员信息
            std::vector<ChatSessionUser> csus;
            for (int i = 0; i < request->userids_size(); i++)
            {
                ChatSessionUser csm(sid, request->userids(i));
                csus.push_back(csm);
            }
            if (!_csuTable->append(csus))
            {
                ERROR("{} - 向数据库添加会话成员信息失败: {}", rid, sname);
                return err(rid, "向数据库添加会话成员信息失败");
            }

            response->set_requestid(rid);
            response->set_success(true);
            response->mutable_chatsessioninfo()->set_chatsessionid(sid);
            response->mutable_chatsessioninfo()->set_chatsessionname(sname);
        }

        // 获取会话成员列表
        void GetChatSessionMember(google::protobuf::RpcController *cntl_base,
                                  const ::hjb::GetChatSessionMemberReq *request,
                                  ::hjb::GetChatSessionMemberResp *response,
                                  ::google::protobuf::Closure *done)
        {
            // 以 ARII 方式自动释放 done 对象
            brpc::ClosureGuard rpc_guard(done);

            // 错误处理函数(出错时调用)
            auto err = [this, response](const std::string &rid,
                                        const std::string &errmsg) -> void
            {
                response->set_requestid(rid);
                response->set_success(false);
                response->set_errmsg(errmsg);
                return;
            };

            std::string rid = request->requestid();
            std::string uid = request->userid();
            std::string sid = request->chatsessionid();

            // 从数据库获取会话成员ID列表
            auto userIds = _csuTable->all(sid);

            // 从用户子服务批量获取用户信息
            std::unordered_map<std::string, UserProto> users;

            if (!_getUser(rid, userIds, users))
            {
                ERROR("{} - 从用户子服务获取用户信息失败", rid);
                return err(rid, "从用户子服务获取用户信息失败");
            }

            // 组织响应
            response->set_requestid(rid);
            response->set_success(true);
            for (const auto &user : users)
            {
                auto userinfo = response->add_users();
                userinfo->CopyFrom(user.second);
            }
        }

    private:
        bool _getRecentMsg(const std::string &rid,
                           const std::string &sid,
                           MessageInfo &msg)
        {
            auto channel = _channels->choose(_messageServiceName);
            if (!channel)
            {
                ERROR("{} - 获取消息子服务信道失败", rid);
                return false;
            }

            GetRecentMsgReq req;
            GetRecentMsgResp resp;
            req.set_requestid(rid);
            req.set_chatsessionid(sid);
            req.set_curtimecount(1);
            brpc::Controller cntl;
            MessageService_Stub stub(channel.get());
            stub.GetRecentMsg(&cntl, &req, &resp, nullptr);

            if (cntl.Failed())
            {
                ERROR("{} - 消息存储子服务调用失败: {}", rid, cntl.ErrorText());
                return false;
            }

            if (!resp.success())
            {
                ERROR("{} - 获取会话 {} 最近消息失败: {}", rid, sid, resp.errmsg());
                return false;
            }

            if (resp.messages_size() > 0)
            {
                msg.CopyFrom(resp.messages(0));
                return true;
            }

            return false;
        }

        bool _getUser(const std::string &rid,
                      const std::vector<std::string> &userIds,
                      std::unordered_map<std::string, UserProto> &users)
        {
            auto channel = _channels->choose(_userServiceName);
            if (!channel)
            {
                ERROR("{} - 获取用户子服务信道失败", rid);
                return false;
            }

            GetMultiUserInfoReq req;
            GetMultiUserInfoResp resp;
            req.set_requestid(rid);
            for (auto &id : userIds)
                req.add_userids(id);

            brpc::Controller cntl;
            UserService_Stub stub(channel.get());
            stub.GetMultiUserInfo(&cntl, &req, &resp, nullptr);

            if (cntl.Failed())
            {
                ERROR("{} - 用户子服务调用失败: {}", rid, cntl.ErrorText());
                return false;
            }

            if (!resp.success())
            {
                ERROR("{} - 批量获取用户信息失败: {}", rid, resp.errmsg());
                return false;
            }

            for (const auto &user : resp.users())
                users.insert(std::make_pair(user.first, user.second));

            return true;
        }
    };

    // 聊天会话服务器
    class ChatSessionServer
    {
    private:
        EtcdDisClient::ptr _disClient;               // 服务发现操作对象
        std::shared_ptr<brpc::Server> _brpcServer;   // rpc服务器
        EtcdRegClient::ptr _regClient;               // 服务注册操作对象
        std::shared_ptr<odb::core::database> _mysql; // mysql操作对象

    public:
        using ptr = std::shared_ptr<ChatSessionServer>;

        ChatSessionServer(const hjb::EtcdDisClient::ptr &dis,
                          const hjb::EtcdRegClient::ptr &reg,
                          const std::shared_ptr<odb::core::database> &mysql,
                          const std::shared_ptr<brpc::Server> &server)
            : _disClient(dis),
              _regClient(reg),
              _mysql(mysql),
              _brpcServer(server)
        {
        }

        ~ChatSessionServer()
        {
        }

        // 搭建RPC服务器并启动服务器
        void start()
        {
            // 阻塞等待服务器运行
            _brpcServer->RunUntilAskedToQuit();
        }
    };

    // 聊天会话rpc服务器建造类
    class ChatSessionServerBuild
    {
    private:
        std::string _userServiceName;
        std::string _messageServiceName;
        EtcdDisClient::ptr _disClient;               // 服务发现操作对象
        std::shared_ptr<brpc::Server> _brpcServer;   // rpc服务器
        EtcdRegClient::ptr _regClient;               // 服务注册操作对象
        std::shared_ptr<odb::core::database> _mysql; // mysql操作对象
        AllServiceChannel::ptr _channels;            // 服务信道操作对象
        ChatSessionUserTable::ptr _csuTable;         // 用户会话关联数据表操作对象
        MQClient::ptr _mqClient;                     // rabbitMQ操作对象
        std::string _exchange;                       // rabbitMQ交换机名称
        std::string _routing_key;                    // rabbitMQ规则

    public:
        // 构造mysql客户端对象
        void makeMysql(
            const std::string &user,
            const std::string &pswd,
            const std::string &host,
            const std::string &db,
            const std::string &cset,
            int port,
            int connPools)
        {
            _mysql = ODBFactory::create(user, pswd, host, db, cset, port, connPools);
        }

        // 构造服务发现客户端和信道管理对象
        void makeEtcdDis(const std::string &regHost,
                         const std::string &baseServiceName,
                         const std::string &userServiceName,
                         const std::string &messageServiceName)
        {
            _userServiceName = userServiceName;
            _messageServiceName = messageServiceName;
            _channels = std::make_shared<hjb::AllServiceChannel>();
            _channels->declared(_userServiceName);
            _channels->declared(_messageServiceName);

            auto putCb = std::bind(&hjb::AllServiceChannel::onServiceOnline, _channels.get(), std::placeholders::_1, std::placeholders::_2);
            auto delCb = std::bind(&hjb::AllServiceChannel::onServiceOffline, _channels.get(), std::placeholders::_1, std::placeholders::_2);
            _disClient = std::make_shared<hjb::EtcdDisClient>(regHost, baseServiceName, putCb, delCb);
        }

        // 构造服务注册客户端对象
        void makeEtcdReg(const std::string &regHost,
                         const std::string &serviceName,
                         const std::string &accessHost)
        {
            _regClient = std::make_shared<EtcdRegClient>(regHost);
            _regClient->reg(serviceName, accessHost);
        }

        // 构造rabbitmq客户端对象
        void makeMqClient(const std::string &user,
                          const std::string &passwd,
                          const std::string &host,
                          const std::string &exchange,
                          const std::string &queue,
                          const std::string &routing_key)
        {
            _routing_key = routing_key;
            _exchange = exchange;
            _mqClient = std::make_shared<MQClient>(user, passwd, host);
            _mqClient->declare(_exchange, queue, _routing_key); // 绑定交换机和队列
        }

        // 构造RPC服务器对象
        void makeRpcServer(uint16_t port, int32_t timeout, uint8_t threads)
        {
            if (!_mqClient)
            {
                ERROR("未初始化消息队列客户端模块");
                abort();
            }

            if (!_mysql)
            {
                ERROR("未初始化Mysql数据库模块");
                abort();
            }

            if (!_channels)
            {
                ERROR("未初始化信道管理模块");
                abort();
            }

            // 添加rpc服务
            _brpcServer = std::make_shared<brpc::Server>();
            ChatSessionServiceImpl *chatSessionService = new ChatSessionServiceImpl(_mysql, _channels, _userServiceName, _messageServiceName, _exchange, _routing_key, _mqClient);
            if (_brpcServer->AddService(chatSessionService, brpc::ServiceOwnership::SERVER_OWNS_SERVICE) == -1)
            {
                ERROR("添加Rpc服务失败");
                abort();
            }

            // 启动rpc服务
            brpc::ServerOptions options;
            options.idle_timeout_sec = timeout;
            options.num_threads = threads;
            if (_brpcServer->Start(port, &options) == -1)
            {
                ERROR("服务启动失败");
                abort();
            }
        }

        // 生成语音识别模块服务器
        ChatSessionServer::ptr build()
        {
            if (!_disClient)
            {
                ERROR("未初始化服务发现模块");
                abort();
            }
            if (!_regClient)
            {
                ERROR("未初始化服务注册模块");
                abort();
            }

            if (!_brpcServer)
            {
                ERROR("未初始化RPC服务器模块");
                abort();
            }

            return std::make_shared<ChatSessionServer>(_disClient, _regClient, _mysql, _brpcServer);
        }
    };
}