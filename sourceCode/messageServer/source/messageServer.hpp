#include <brpc/server.h>
#include <butil/logging.h>
#include <regex>

#include "log.hpp"
#include "etcd.hpp"
#include "MMessage.hpp"
#include "esData.hpp"
#include "util.hpp"
#include "channel.hpp"
#include "rabbitMQ.hpp"

#include "user.pb.h"
#include "base.pb.h"
#include "file.pb.h"
#include "message.pb.h"

namespace hjb
{
    // 消息存储服务类
    class MessageServiceImpl : public MessageService
    {
    private:
        ESMessage::ptr _es;               // 消息es操作类对象
        MessageTable::ptr _mysql;         // 消息数据库操作对象
        std::string _fileServiceName;     // 文件服务的名称
        std::string _userServiceName;     // 用户服务的名称
        AllServiceChannel::ptr _channels; // 服务信道操作对象

    public:
        MessageServiceImpl(const std::shared_ptr<elasticlient::Client> &es,
                           const std::shared_ptr<odb::core::database> &mysql,
                           const AllServiceChannel::ptr &channels,
                           const std::string &fileServiceName,
                           const std::string &userServiceName)
            : _es(std::make_shared<ESMessage>(es)),
              _mysql(std::make_shared<MessageTable>(mysql)),
              _userServiceName(userServiceName),
              _fileServiceName(fileServiceName),
              _channels(channels)
        {
            // 创建es索引
            _es->createIndex();
        }

        ~MessageServiceImpl() {}

        // 获取历史消息(根据时间)
        virtual void GetHistoryMsg(::google::protobuf::RpcController *controller,
                                   const ::hjb::GetHistoryMsgReq *request,
                                   ::hjb::GetHistoryMsgResp *response,
                                   ::google::protobuf::Closure *done)
        {
            DEBUG("收到获取历史消息(根据时间)请求");

            // 以 ARII 方式自动释放 done 对象
            brpc::ClosureGuard rpc_guard(done);

            // 错误处理函数(出错时调用)
            auto err = [this, response](const std::string &requestId,
                                        const std::string &errmsg) -> void
            {
                response->set_requestid(requestId);
                response->set_success(false);
                response->set_errmsg(errmsg);
                return;
            };

            // 取出请求里的会话id、时间范围
            std::string requestId = request->requestid();
            std::string chatSessionId = request->chatsessionid();
            boost::posix_time::ptime startTime = boost::posix_time::from_time_t(request->starttime());
            boost::posix_time::ptime overTime = boost::posix_time::from_time_t(request->overtime());

            // 向数据库中查询所有的该范围内消息记录
            auto messages = _mysql->range(chatSessionId, startTime, overTime);
            if (messages.empty())
            {
                response->set_requestid(requestId);
                response->set_success(true);
                return;
            }

            // 统计这些历史消息中的所有文件消息的id
            std::unordered_set<std::string> files;
            for (const auto &msg : messages)
            {
                if (msg.fileId().empty())
                    continue;
                files.insert(msg.fileId());
            }
            // 从文件子服务中下载这些文件消息
            std::unordered_map<std::string, std::string> fileDatas;
            if (!_getFiles(requestId, files, fileDatas))
            {
                ERROR("{} 批量文件数据下载失败", requestId);
                return err(requestId, "批量文件数据下载失败");
            }

            // 统计这些历史消息中的所有消息的发送者id
            std::unordered_set<std::string> userIds;
            for (const auto &msg : messages)
            {
                userIds.insert(msg.userId());
            }
                

            // 从用户子服务中获取发送者的用户数据
            std::unordered_map<std::string, UserProto> users;
            if (!_getUsers(requestId, userIds, users))
            {
                ERROR("{} 批量用户数据获取失败", requestId);
                return err(requestId, "批量用户数据获取失败");
            }

            // 组织响应
            // 遍历历史消息，组织对应类型数据
            response->set_requestid(requestId);
            response->set_success(true);
            for (const auto &msg : messages)
            {
                auto message = response->add_messages();
                message->set_messageid(msg.messageId());
                message->set_chatsessionid(msg.chatSessionId());
                message->set_timestamp(boost::posix_time::to_time_t(msg.createTime()));
                message->mutable_sender()->CopyFrom(users[msg.userId()]);
                switch (msg.messageType())
                {
                case MessageType::STRING:
                    message->mutable_message()->set_messagetype(MessageType::STRING);
                    message->mutable_message()->mutable_stringmessage()->set_content(msg.content());
                    break;
                case MessageType::IMAGE:
                    message->mutable_message()->set_messagetype(MessageType::IMAGE);
                    message->mutable_message()->mutable_imagemessage()->set_fileid(msg.fileId());
                    message->mutable_message()->mutable_imagemessage()->set_content(fileDatas[msg.fileId()]);
                    break;
                case MessageType::FILE:
                    message->mutable_message()->set_messagetype(MessageType::FILE);
                    message->mutable_message()->mutable_filemessage()->set_fileid(msg.fileId());
                    message->mutable_message()->mutable_filemessage()->set_filesize(msg.fileSize());
                    message->mutable_message()->mutable_filemessage()->set_filename(msg.fileName());
                    message->mutable_message()->mutable_filemessage()->set_filecontent(fileDatas[msg.fileId()]);
                    break;
                case MessageType::SPEECH:
                    message->mutable_message()->set_messagetype(MessageType::SPEECH);
                    message->mutable_message()->mutable_speechmessage()->set_fileid(msg.fileId());
                    message->mutable_message()->mutable_speechmessage()->set_content(fileDatas[msg.fileId()]);
                    break;
                default:
                    ERROR("消息类型错误");
                    return;
                }
            }
            return;
        }

        // 获取最近历史消息(根据指定条数)
        virtual void GetRecentMsg(::google::protobuf::RpcController *controller,
                                  const ::hjb::GetRecentMsgReq *request,
                                  ::hjb::GetRecentMsgResp *response,
                                  ::google::protobuf::Closure *done)
        {
            DEBUG("收到获取最近历史消息(根据指定条数)请求");

            // 以 ARII 方式自动释放 done 对象
            brpc::ClosureGuard rpc_guard(done);

            // 错误处理函数(出错时调用)
            auto err = [this, response](const std::string &requestId,
                                        const std::string &errmsg) -> void
            {
                response->set_requestid(requestId);
                response->set_success(false);
                response->set_errmsg(errmsg);
                return;
            };

            // 取出请求里的会话id、指定条数
            std::string requestId = request->requestid();
            std::string chatSessionId = request->chatsessionid();
            int64_t count = request->curtimecount();

            // 向数据库中查询所有的该范围内消息记录
            auto messages = _mysql->recent(chatSessionId, count);
            if (messages.empty())
            {
                response->set_requestid(requestId);
                response->set_success(true);
                return;
            }

            // 统计这些历史消息中的所有文件消息的id
            std::unordered_set<std::string> files;
            for (const auto &msg : messages)
            {
                if (msg.fileId().empty())
                    continue;
                files.insert(msg.fileId());
            }
            // 从文件子服务中下载这些文件消息
            std::unordered_map<std::string, std::string> fileDatas;
            if (!_getFiles(requestId, files, fileDatas))
            {
                ERROR("{} 批量文件数据下载失败", requestId);
                return err(requestId, "批量文件数据下载失败");
            }

            // 统计这些历史消息中的所有消息的发送者id
            std::unordered_set<std::string> userIds;
            for (const auto &msg : messages)
                userIds.insert(msg.userId());

            // 从用户子服务中获取发送者的用户数据
            std::unordered_map<std::string, UserProto> users;
            if (!_getUsers(requestId, userIds, users))
            {
                ERROR("{} 批量用户数据获取失败", requestId);
                return err(requestId, "批量用户数据获取失败");
            }

            // 组织响应
            // 遍历历史消息，组织对应类型数据
            response->set_requestid(requestId);
            response->set_success(true);
            for (const auto &msg : messages)
            {
                auto message = response->add_messages();
                message->set_messageid(msg.messageId());
                message->set_chatsessionid(msg.chatSessionId());
                message->set_timestamp(boost::posix_time::to_time_t(msg.createTime()));
                message->mutable_sender()->CopyFrom(users[msg.userId()]);
                switch (msg.messageType())
                {
                case MessageType::STRING:
                    message->mutable_message()->set_messagetype(MessageType::STRING);
                    message->mutable_message()->mutable_stringmessage()->set_content(msg.content());
                    break;
                case MessageType::IMAGE:
                    message->mutable_message()->set_messagetype(MessageType::IMAGE);
                    message->mutable_message()->mutable_imagemessage()->set_fileid(msg.fileId());
                    message->mutable_message()->mutable_imagemessage()->set_content(fileDatas[msg.fileId()]);
                    break;
                case MessageType::FILE:
                    message->mutable_message()->set_messagetype(MessageType::FILE);
                    message->mutable_message()->mutable_filemessage()->set_fileid(msg.fileId());
                    message->mutable_message()->mutable_filemessage()->set_filesize(msg.fileSize());
                    message->mutable_message()->mutable_filemessage()->set_filename(msg.fileName());
                    message->mutable_message()->mutable_filemessage()->set_filecontent(fileDatas[msg.fileId()]);
                    break;
                case MessageType::SPEECH:
                    message->mutable_message()->set_messagetype(MessageType::SPEECH);
                    message->mutable_message()->mutable_speechmessage()->set_fileid(msg.fileId());
                    message->mutable_message()->mutable_speechmessage()->set_content(fileDatas[msg.fileId()]);
                    break;
                default:
                    ERROR("消息类型错误");
                    return;
                }
            }
            return;
        }

        // 获取历史消息(根据指定关键词)仅支持文字消息
        virtual void MsgSearch(::google::protobuf::RpcController *controller,
                               const ::hjb::MsgSearchReq *request,
                               ::hjb::MsgSearchResp *response,
                               ::google::protobuf::Closure *done)
        {
            // 以 ARII 方式自动释放 done 对象
            brpc::ClosureGuard rpc_guard(done);

            // 错误处理函数(出错时调用)
            auto err = [this, response](const std::string &requestId,
                                        const std::string &errmsg) -> void
            {
                response->set_requestid(requestId);
                response->set_success(false);
                response->set_errmsg(errmsg);
                return;
            };

            // 取出请求里的会话id、关键字
            std::string requestId = request->requestid();
            std::string chatSessionId = request->chatsessionid();
            std::string key = request->key();

            // 在es中进行搜索获取消息列表
            auto messages = _es->search(chatSessionId, key);
            if (messages.empty())
            {
                response->set_requestid(requestId);
                response->set_success(true);
                return;
            }

            // 统计这些历史消息中的所有消息的发送者id
            std::unordered_set<std::string> userIds;
            for (const auto &msg : messages)
                userIds.insert(msg.userId());

            // 从用户子服务中获取发送者的用户数据
            std::unordered_map<std::string, UserProto> users;
            if (!_getUsers(requestId, userIds, users))
            {
                ERROR("{} 批量用户数据获取失败", requestId);
                return err(requestId, "批量用户数据获取失败");
            }

            response->set_requestid(requestId);
            response->set_success(true);
            for (const auto &msg : messages)
            {
                auto message = response->add_messages();
                message->set_messageid(msg.messageId());
                message->set_chatsessionid(msg.chatSessionId());
                message->set_timestamp(boost::posix_time::to_time_t(msg.createTime()));
                message->mutable_sender()->CopyFrom(users[msg.userId()]);
                message->mutable_message()->set_messagetype(MessageType::STRING);
                message->mutable_message()->mutable_stringmessage()->set_content(msg.content());
            }

            return;
        }

        // 新消息到来时的存储业务处理
        void onMessage(const char *body, size_t size)
        {
            DEBUG("收到新消息进行存储处理");

            // 对消息内容进行反序列化
            hjb::MessageInfo message;
            if (!message.ParseFromArray(body, size))
            {
                ERROR("新消息反序列化失败");
                return;
            }

            // 针对不同的消息类型做不同的业务处理
            std::string fileId, fileName, content;
            int64_t fileSize;
            switch (message.message().messagetype())
            {
            // 文本消息存储到es中
            case MessageType::STRING:
                content = message.message().stringmessage().content();
                if (!_es->append(
                        message.sender().userid(),
                        message.messageid(),
                        message.timestamp(),
                        message.chatsessionid(),
                        content))
                {
                    ERROR("es存储文本消息失败");
                    return;
                }
                break;

            // 其他消息则将数据存储到文件子服务，并获取文件id
            case MessageType::IMAGE:
            {
                const auto &msg = message.message().imagemessage();
                if (!_putFiles("", msg.content(), msg.content().size(), fileId))
                {
                    ERROR("上传图片到文件子服务失败");
                    return;
                }
            }
            break;
            case MessageType::FILE:
            {
                const auto &msg = message.message().filemessage();
                fileName = msg.filename();
                fileSize = msg.filesize();
                if (!_putFiles(fileName, msg.filecontent(), fileSize, fileId))
                {
                    ERROR("上传文件到文件子服务失败");
                    return;
                }
            }
            break;
            case MessageType::SPEECH:
            {
                const auto &msg = message.message().speechmessage();
                if (!_putFiles("", msg.content(), msg.content().size(), fileId))
                {
                    ERROR("上传语音到文件子服务失败");
                    return;
                }
            }
            break;
            default:
                ERROR("消息类型错误");
                return;
            }

            // 提取消息的关键信息存储到mysql中
            Message msg(message.messageid(), 
                        message.chatsessionid(), 
                        message.sender().userid(), 
                        message.message().messagetype(), 
                        boost::posix_time::from_time_t(message.timestamp()));
            msg.content(content);
            msg.fileId(fileId);
            msg.fileName(fileName);
            msg.fileSize(fileSize);
            if (!_mysql->insert(msg))
            {
                ERROR("向数据库插入新消息失败");
                return;
            }
        }

    private:
        bool _getUsers(const std::string &requestId,
                       const std::unordered_set<std::string> &userIds,
                       std::unordered_map<std::string, UserProto> &users)
        {
            auto channel = _channels->choose(_userServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到用户管理子服务节点 - {}", requestId, _userServiceName);
                return false;
            }
            GetMultiUserInfoReq req;
            GetMultiUserInfoResp resp;
            brpc::Controller cntl;
            UserService_Stub stub(channel.get());
            req.set_requestid(requestId);
            for (const auto &id : userIds)
                req.add_userids(id);

            stub.GetMultiUserInfo(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 用户子服务调用失败：{}", requestId, cntl.ErrorText());
                return false;
            }

            for (auto it = resp.users().begin(); it != resp.users().end(); ++it)
                users.insert(std::make_pair(it->first, it->second));

            return true;
        }

        bool _getFiles(const std::string &requestId,
                       const std::unordered_set<std::string> &files,
                       std::unordered_map<std::string, std::string> &fileDatas)
        {
            auto channel = _channels->choose(_fileServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到文件管理子服务节点 - {}", requestId, _fileServiceName);
                return false;
            }
            GetMultiFileReq req;
            GetMultiFileResp resp;
            brpc::Controller cntl;
            FileService_Stub stub(channel.get());
            req.set_requestid(requestId);
            for (const auto &id : files)
                req.add_fileidlist(id);

            stub.GetMultiFile(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 文件子服务调用失败：{}", requestId, cntl.ErrorText());
                return false;
            }

            for (auto it = resp.filedata().begin(); it != resp.filedata().end(); ++it)
                fileDatas.insert(std::make_pair(it->first, it->second.filecontent()));

            return true;
        }

        bool _putFiles(const std::string &fileName,
                       const std::string &body,
                       const int64_t fileSize,
                       std::string &fileId)
        {
            auto channel = _channels->choose(_fileServiceName);
            if (!channel)
            {
                ERROR("未找到文件管理子服务节点 - {}", _fileServiceName);
                return false;
            }
            PutSingleFileReq req;
            PutSingleFileResp resp;
            brpc::Controller cntl;
            FileService_Stub stub(channel.get());
            req.mutable_filedata()->set_filename(fileName);
            req.mutable_filedata()->set_filesize(fileSize);
            req.mutable_filedata()->set_filecontent(body);

            stub.PutSingleFile(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("文件子服务调用失败：{}", cntl.ErrorText());
                return false;
            }

            fileId = resp.fileinfo().fileid();

            return true;
        }
    };

    class MessageServer
    {
    private:
        hjb::EtcdDisClient::ptr _disClient;
        hjb::EtcdRegClient::ptr _regClient;
        std::shared_ptr<elasticlient::Client> _es;
        std::shared_ptr<odb::core::database> _mysql;
        hjb::MQClient::ptr _rabbit;
        std::shared_ptr<brpc::Server> _brpcServer;

    public:
        using ptr = std::shared_ptr<MessageServer>;

        MessageServer(const hjb::EtcdDisClient::ptr &dis,
                      const hjb::EtcdRegClient::ptr &reg,
                      const std::shared_ptr<elasticlient::Client> &es,
                      const std::shared_ptr<odb::core::database> &mysql,
                      const hjb::MQClient::ptr &rabbit,
                      const std::shared_ptr<brpc::Server> &server)
            : _disClient(dis),
              _regClient(reg),
              _es(es),
              _mysql(mysql),
              _rabbit(rabbit),
              _brpcServer(server)
        {
        }

        ~MessageServer() {}

        // 搭建RPC服务器，并启动服务器
        void start()
        {
            _brpcServer->RunUntilAskedToQuit();
        }
    };

    class MessageServerBuilder
    {
    private:
        hjb::EtcdDisClient::ptr _disClient;
        hjb::EtcdRegClient::ptr _regClient;
        std::shared_ptr<elasticlient::Client> _es;
        std::shared_ptr<odb::core::database> _mysql;
        std::shared_ptr<brpc::Server> _brpcServer;
        std::string _exchange;
        std::string _queue;
        hjb::MQClient::ptr _rabbit;
        std::string _fileServiceName;
        std::string _userServiceName;
        hjb::AllServiceChannel::ptr _channels;

    public:
        // 构造es客户端对象
        void makeEs(const std::vector<std::string> hosts)
        {
            _es = ESFactory::create(hosts);
        }

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

        // 构造rabbitmq客户端对象
        void makeMqClient(const std::string &user,
                          const std::string &passwd,
                          const std::string &host,
                          const std::string &exchange,
                          const std::string &queue,
                          const std::string &binding_key)
        {
            _queue = queue;
            _exchange = exchange;
            _rabbit = std::make_shared<MQClient>(user, passwd, host);
            _rabbit->declare(_exchange, _queue, binding_key); // 绑定交换机和队列
        }

        // 构造服务发现客户端和信道管理对象
        void makeEtcdDis(const std::string &regHost,
                         const std::string &baseServiceName,
                         const std::string &fileServiceName,
                         const std::string &userServiceName)
        {
            _fileServiceName = fileServiceName;
            _userServiceName = userServiceName;
            _channels = std::make_shared<hjb::AllServiceChannel>();
            _channels->declared(fileServiceName);
            _channels->declared(userServiceName);

            auto putCb = std::bind(&hjb::AllServiceChannel::onServiceOnline, _channels.get(), std::placeholders::_1, std::placeholders::_2);
            auto delCb = std::bind(&hjb::AllServiceChannel::onServiceOffline, _channels.get(), std::placeholders::_1, std::placeholders::_2);
            _disClient = std::make_shared<hjb::EtcdDisClient>(regHost, baseServiceName, putCb, delCb);
        }

        // 用于构造服务注册客户端对象
        void makeEtcdReg(const std::string &regHost,
                         const std::string &serviceName,
                         const std::string &accessHost)
        {
            _regClient = std::make_shared<hjb::EtcdRegClient>(regHost);
            _regClient->reg(serviceName, accessHost);
        }

        // 添加并启动rpc服务
        void makeRpcServer(uint16_t port, int32_t timeout, uint8_t threads)
        {
            if (!_es)
            {
                ERROR("未初始化ES搜索引擎模块");
                abort();
            }
            if (!_mysql)
            {
                ERROR("未初始化Mysql数据库模块");
                abort();
            }
            if (!_rabbit)
            {
                ERROR("未初始化RabbitMQ模块");
                abort();
            }
            if (!_channels)
            {
                ERROR("未初始化信道管理模块");
                abort();
            }

            _brpcServer = std::make_shared<brpc::Server>();

            MessageServiceImpl *service = new MessageServiceImpl(_es, _mysql, _channels, _fileServiceName, _userServiceName);
            if (_brpcServer->AddService(service, brpc::ServiceOwnership::SERVER_OWNS_SERVICE) == -1)
            {
                ERROR("添加Rpc服务失败");
                abort();
            }

            brpc::ServerOptions options;
            options.idle_timeout_sec = timeout;
            options.num_threads = threads;
            if (_brpcServer->Start(port, &options) == -1)
            {
                ERROR("服务启动失败");
                abort();
            }

            auto callback = std::bind(&MessageServiceImpl::onMessage, service, std::placeholders::_1, std::placeholders::_2);
            _rabbit->consume(_queue, callback);
        }

        // 构造RPC服务器对象
        MessageServer::ptr build()
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

            MessageServer::ptr server = std::make_shared<MessageServer>(_disClient, _regClient, _es, _mysql, _rabbit, _brpcServer);
            return server;
        }
    };
}