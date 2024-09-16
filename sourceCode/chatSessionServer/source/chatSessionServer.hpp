#include <brpc/server.h>
#include <butil/logging.h>

#include "log.hpp"
#include "etcd.hpp"
#include "util.hpp"
#include "MChatSessionUser.hpp"
#include "channel.hpp"
#include "rabbitMQ.hpp"

#include "base.pb.h"
#include "user.pb.h"
#include "chatSession.pb.h"

namespace hjb
{
    // 聊天会话消息转发服务类
    class MessageTransmitServiceImpl : public MessageTransmitService
    {
    private:
        std::string _userServiceName;        // 用户子服务的名称
        AllServiceChannel::ptr _channels;    // 服务信道操作对象
        ChatSessionUserTable::ptr _csuTable; // 用户会话关联数据表操作对象
        MQClient::ptr _mqClient;             // rabbitMQ操作对象
        std::string _exchange;               // rabbitMQ交换机名称
        std::string _routing_key;            // rabbitMQ规则

    public:
        MessageTransmitServiceImpl(const std::shared_ptr<odb::core::database> &mysql,
                                   const AllServiceChannel::ptr &channels,
                                   const std::string &userServiceName,
                                   const std::string &exchange,
                                   const std::string &routing_key,
                                   const MQClient::ptr &mqClient)
            : _userServiceName(userServiceName),
              _exchange(exchange),
              _routing_key(routing_key),
              _channels(channels),
              _csuTable(std::make_shared<ChatSessionUserTable>(mysql)),
              _mqClient(mqClient)
        {
        }

        ~MessageTransmitServiceImpl()
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
        std::string _serviceName;
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
                         const std::string &serviceName)
        {
            _serviceName = serviceName;
            _channels = std::make_shared<hjb::AllServiceChannel>();
            _channels->declared(_serviceName);

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
            MessageTransmitServiceImpl *messageTransmitService = new MessageTransmitServiceImpl(_mysql, _channels, _serviceName, _exchange, _routing_key, _mqClient);
            if (_brpcServer->AddService(messageTransmitService, brpc::ServiceOwnership::SERVER_OWNS_SERVICE) == -1)
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