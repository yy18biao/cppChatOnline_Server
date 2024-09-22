#include "connection.hpp"
#include "etcd.hpp"
#include "redis.hpp"
#include "channel.hpp"
#include "redis.hpp"
#include "httplib.h"

#include "base.pb.h"
#include "chatSession.pb.h"
#include "file.pb.h"
#include "friend.pb.h"
#include "message.pb.h"
#include "speech.pb.h"
#include "user.pb.h"
#include "gateway.pb.h"
#include "websocket.pb.h"

namespace hjb
{
    class GatewayServer
    {
    private:
        LoginSession::ptr _loginSessionRedis; // 用户redis登录会话操作对象
        LoginStatus::ptr _statusRedis;        // 用户redis登录状态操作对象
        AllServiceChannel::ptr _channels;     // 用户服务信道操作对象
        std::string _fileServiceName;         // 文件服务的名称
        std::string _userServiceName;         // 用户服务的名称
        std::string _messageServiceName;      // 消息服务的名称
        std::string _friendServiceName;       // 好友服务的名称
        std::string _speechServiceName;       // 语音服务的名称
        std::string _chatSessionServiceName;  // 聊天会话服务的名称
        Connection::ptr _connection;          // 连接管理对象
        wserver _wserver;                     // websocket服务器
        httplib::Server _httpServer;          // http服务器
        std::thread _httpThread;              // http服务器的执行线程

    public:
        using ptr = std::shared_ptr<GatewayServer>;

        GatewayServer(const std::shared_ptr<sw::redis::Redis> &redis,
                      const AllServiceChannel::ptr &channels,
                      const std::string &fileServiceName,
                      const std::string &messageServiceName,
                      const std::string &userServiceName,
                      const std::string &friendServiceName,
                      const std::string &speechServiceName,
                      const std::string &chatSessionServiceName,
                      int websocketPort,
                      int httpPort,
                      const EtcdDisClient::ptr &disClient)
            : _loginSessionRedis(std::make_shared<LoginSession>(redis)),
              _statusRedis(std::make_shared<LoginStatus>(redis)),
              _channels(channels),
              _fileServiceName(fileServiceName),
              _userServiceName(userServiceName),
              _messageServiceName(messageServiceName),
              _friendServiceName(friendServiceName),
              _speechServiceName(speechServiceName),
              _chatSessionServiceName(chatSessionServiceName),
              _connection(std::make_shared<Connection>())
        {
            // 搭建websocket服务器
            _wserver.set_access_channels(websocketpp::log::alevel::none);
            _wserver.init_asio();
            _wserver.set_open_handler(std::bind(&GatewayServer::onOpen, this, std::placeholders::_1));
            _wserver.set_close_handler(std::bind(&GatewayServer::onClose, this, std::placeholders::_1));
            _wserver.set_message_handler(std::bind(&GatewayServer::onMessage, this, std::placeholders::_1, std::placeholders::_2));
            _wserver.set_reuse_addr(true);
            _wserver.listen(websocketPort);
            _wserver.start_accept();

            // 搭建http服务器
            _httpServer.Post("/service/user/getVerifyCode",
                             (httplib::Server::Handler)std::bind(&GatewayServer::getVerifyCode,
                                                                 this,
                                                                 std::placeholders::_1,
                                                                 std::placeholders::_2));
            _httpServer.Post("/service/user/reg",
                             (httplib::Server::Handler)std::bind(&GatewayServer::reg,
                                                                 this,
                                                                 std::placeholders::_1,
                                                                 std::placeholders::_2));
            _httpServer.Post("/service/user/userLogin",
                             (httplib::Server::Handler)std::bind(&GatewayServer::userLogin,
                                                                 this,
                                                                 std::placeholders::_1,
                                                                 std::placeholders::_2));
            _httpServer.Post("/service/user/phoneLogin",
                             (httplib::Server::Handler)std::bind(&GatewayServer::phoneLogin,
                                                                 this,
                                                                 std::placeholders::_1,
                                                                 std::placeholders::_2));
            _httpServer.Post("/service/user/getUserInfo",
                             (httplib::Server::Handler)std::bind(&GatewayServer::getUserInfo,
                                                                 this,
                                                                 std::placeholders::_1,
                                                                 std::placeholders::_2));
            _httpServer.Post("/service/user/setPhoto",
                             (httplib::Server::Handler)std::bind(&GatewayServer::setPhoto,
                                                                 this,
                                                                 std::placeholders::_1,
                                                                 std::placeholders::_2));
            _httpServer.Post("/service/user/setNickname",
                             (httplib::Server::Handler)std::bind(&GatewayServer::setNickname,
                                                                 this,
                                                                 std::placeholders::_1,
                                                                 std::placeholders::_2));
            _httpServer.Post("/service/user/setDesc",
                             (httplib::Server::Handler)std::bind(&GatewayServer::setDesc,
                                                                 this,
                                                                 std::placeholders::_1,
                                                                 std::placeholders::_2));
            _httpServer.Post("/service/user/setPhone",
                             (httplib::Server::Handler)std::bind(&GatewayServer::setPhone,
                                                                 this,
                                                                 std::placeholders::_1,
                                                                 std::placeholders::_2));
            _httpServer.Post("/service/friend/getFriends",
                             (httplib::Server::Handler)std::bind(&GatewayServer::getFriends,
                                                                 this,
                                                                 std::placeholders::_1,
                                                                 std::placeholders::_2));
            _httpServer.Post("/service/friend/addFriend",
                             (httplib::Server::Handler)std::bind(&GatewayServer::addFriend,
                                                                 this,
                                                                 std::placeholders::_1,
                                                                 std::placeholders::_2));
            _httpServer.Post("/service/friend/friendAddProcess",
                             (httplib::Server::Handler)std::bind(&GatewayServer::friendAddProcess,
                                                                 this,
                                                                 std::placeholders::_1,
                                                                 std::placeholders::_2));
            _httpServer.Post("/service/friend/friendRemove",
                             (httplib::Server::Handler)std::bind(&GatewayServer::friendRemove,
                                                                 this,
                                                                 std::placeholders::_1,
                                                                 std::placeholders::_2));
            _httpServer.Post("/service/friend/friendSearch",
                             (httplib::Server::Handler)std::bind(&GatewayServer::friendSearch,
                                                                 this,
                                                                 std::placeholders::_1,
                                                                 std::placeholders::_2));
            _httpServer.Post("/service/friend/getFriendApplys",
                             (httplib::Server::Handler)std::bind(&GatewayServer::getFriendApplys,
                                                                 this,
                                                                 std::placeholders::_1,
                                                                 std::placeholders::_2));
            _httpServer.Post("/service/chatSession/getChatSessions",
                             (httplib::Server::Handler)std::bind(&GatewayServer::getChatSessions,
                                                                 this,
                                                                 std::placeholders::_1,
                                                                 std::placeholders::_2));
            _httpServer.Post("/service/chatSession/createChatSession",
                             (httplib::Server::Handler)std::bind(&GatewayServer::createChatSession,
                                                                 this,
                                                                 std::placeholders::_1,
                                                                 std::placeholders::_2));
            _httpServer.Post("/service/chatSession/getChatSessionUser",
                             (httplib::Server::Handler)std::bind(&GatewayServer::getChatSessionUser,
                                                                 this,
                                                                 std::placeholders::_1,
                                                                 std::placeholders::_2));
            _httpServer.Post("/service/chatSession/newMessage",
                             (httplib::Server::Handler)std::bind(&GatewayServer::newMessage,
                                                                 this,
                                                                 std::placeholders::_1,
                                                                 std::placeholders::_2));
            _httpServer.Post("/service/message/getHistoryMessage",
                             (httplib::Server::Handler)std::bind(&GatewayServer::getHistoryMessage,
                                                                 this,
                                                                 std::placeholders::_1,
                                                                 std::placeholders::_2));
            _httpServer.Post("/service/message/getRecentMsg",
                             (httplib::Server::Handler)std::bind(&GatewayServer::getRecentMsg,
                                                                 this,
                                                                 std::placeholders::_1,
                                                                 std::placeholders::_2));
            _httpServer.Post("/service/message/messageSearch",
                             (httplib::Server::Handler)std::bind(&GatewayServer::messageSearch,
                                                                 this,
                                                                 std::placeholders::_1,
                                                                 std::placeholders::_2));
            _httpServer.Post("/service/file/getSingleFile",
                             (httplib::Server::Handler)std::bind(&GatewayServer::getSingleFile,
                                                                 this,
                                                                 std::placeholders::_1,
                                                                 std::placeholders::_2));
            _httpServer.Post("/service/file/getMultiFile",
                             (httplib::Server::Handler)std::bind(&GatewayServer::getMultiFile,
                                                                 this,
                                                                 std::placeholders::_1,
                                                                 std::placeholders::_2));
            _httpServer.Post("/service/file/putSingleFile",
                             (httplib::Server::Handler)std::bind(&GatewayServer::putSingleFile,
                                                                 this,
                                                                 std::placeholders::_1,
                                                                 std::placeholders::_2));
            _httpServer.Post("/service/file/putMultiFile",
                             (httplib::Server::Handler)std::bind(&GatewayServer::putMultiFile,
                                                                 this,
                                                                 std::placeholders::_1,
                                                                 std::placeholders::_2));
            _httpServer.Post("/service/speech/recognition",
                             (httplib::Server::Handler)std::bind(&GatewayServer::recognition,
                                                                 this,
                                                                 std::placeholders::_1,
                                                                 std::placeholders::_2));

            _httpThread = std::thread([ this, httpPort](){
                _httpServer.listen("0.0.0.0", httpPort);
            });
            _httpThread.detach();
        }

        void start()
        {
            _wserver.run();
        }

    private:
        std::shared_ptr<GetUserInfoResp> _GetUserInfo(const std::string &rid, const std::string &uid)
        {
            GetUserInfoReq req;
            auto resp = std::make_shared<GetUserInfoResp>();
            req.set_requestid(rid);
            req.set_userid(uid);

            // 将请求转发给用户子服务进行业务处理
            auto channel = _channels->choose(_userServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到用户管理子服务节点 - {}", req.requestid(), _userServiceName);
                return std::shared_ptr<GetUserInfoResp>();
            }

            UserService_Stub stub(channel.get());
            brpc::Controller cntl;
            stub.GetUserInfo(&cntl, &req, resp.get(), nullptr);
            if (cntl.Failed() || !resp->success())
            {
                ERROR("{} - 用户子服务调用失败：{}", req.requestid(), cntl.ErrorText());
                return std::shared_ptr<GetUserInfoResp>();
            }

            return resp;
        }

        void onOpen(websocketpp::connection_hdl hdl)
        {
            DEBUG("websocket长连接建立成功");
        }

        void onClose(websocketpp::connection_hdl hdl)
        {
            auto conn = _wserver.get_con_from_hdl(hdl);
            std::string uid, sid;
            if (!_connection->client(conn, uid, sid))
            {
                WARN("长连接断开时未找到长连接对应的客户端信息");
                return;
            }
            // 移除登录会话
            _loginSessionRedis->remove(sid);
            // 移除登录状态
            _statusRedis->remove(uid);
            // 移除长连接管理
            _connection->remove(conn);

            DEBUG("{} {} {} 长连接断开成功清理缓存数据", sid, uid, (size_t)conn.get());
        }

        void keepAlive(wserver::connection_ptr conn)
        {
            if (!conn || conn->get_state() != websocketpp::session::state::value::open)
            {
                DEBUG("非正常连接状态，结束连接保活");
                return;
            }

            conn->ping("");
            _wserver.set_timer(60000, std::bind(&GatewayServer::keepAlive, this, conn));
        }

        void onMessage(websocketpp::connection_hdl hdl, wserver::message_ptr msg)
        {
            auto conn = _wserver.get_con_from_hdl(hdl);

            // 针对消息内容进行反序列化
            ClientAuthenticationReq req;
            if (!req.ParseFromString(msg->get_payload()))
            {
                ERROR("长连接身份识别失败：正文反序列化失败");
                _wserver.close(hdl, websocketpp::close::status::unsupported_data, "正文反序列化失败");
                return;
            }

            // 获取会话以及用户id
            std::string sid = req.loginsessionid();
            auto uid = _loginSessionRedis->uid(sid);
            if (!uid)
            {
                ERROR("长连接身份识别失败：未找到会话信息 {}", sid);
                _wserver.close(hdl, websocketpp::close::status::unsupported_data, "未找到会话信息");
                return;
            }

            // 添加长连接管理
            _connection->insert(conn, *uid, sid);
            DEBUG("新增长连接管理：{}-{}-{}", sid, *uid, (size_t)conn.get());
            keepAlive(conn);
        }

        void getVerifyCode(const httplib::Request &request, httplib::Response &response)
        {
            // 取出http请求正文，将正文进行反序列化
            PhoneVerifyCodeReq req;
            PhoneVerifyCodeResp resp;

            // 错误处理函数(出错时调用)
            auto err = [this, &req, &resp, &response](const std::string &errmsg) -> void
            {
                response.set_content(resp.SerializeAsString(), "application/x-protbuf");
                resp.set_success(false);
                resp.set_errmsg(errmsg);
            };

            if (!req.ParseFromString(request.body))
            {
                ERROR("获取短信验证码请求正文反序列化失败");
                return err("获取短信验证码请求正文反序列化失败");
            }

            // 将请求转发给用户子服务进行业务处理
            auto channel = _channels->choose(_userServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到用户管理子服务节点 - {}", req.requestid(), _userServiceName);
                return err("未找到用户管理子服务节点");
            }

            UserService_Stub stub(channel.get());
            brpc::Controller cntl;
            stub.GetPhoneVerifyCode(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 用户子服务调用失败：{}", req.requestid(), cntl.ErrorText());
                return err("用户子服务调用失败");
            }

            // 得到用户子服务的响应后将响应内容进行序列化作为http响应正文
            response.set_content(resp.SerializeAsString(), "application/x-protbuf");
        }

        void reg(const httplib::Request &request, httplib::Response &response)
        {
            // 取出http请求正文，将正文进行反序列化
            UserRegReq req;
            UserRegResp resp;

            // 错误处理函数(出错时调用)
            auto err = [this, &req, &resp, &response](const std::string &errmsg) -> void
            {
                response.set_content(resp.SerializeAsString(), "application/x-protbuf");
               resp.set_success(false);
                resp.set_errmsg(errmsg);
            };

            if (!req.ParseFromString(request.body))
            {
                ERROR("注册用户请求正文反序列化失败");
                return err("注册用户请求正文反序列化失败");
            }

            // 将请求转发给用户子服务进行业务处理
            auto channel = _channels->choose(_userServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到用户管理子服务节点 - {}", req.requestid(), _userServiceName);
                return err("未找到用户管理子服务节点");
            }

            UserService_Stub stub(channel.get());
            brpc::Controller cntl;
            stub.UserRegister(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 用户子服务调用失败：{}", req.requestid(), cntl.ErrorText());
                return err("用户子服务调用失败");
            }

            // 得到用户子服务的响应后将响应内容进行序列化作为http响应正文
            response.set_content(resp.SerializeAsString(), "application/x-protbuf");
        }

        void userLogin(const httplib::Request &request, httplib::Response &response)
        {
            // 取出http请求正文，将正文进行反序列化
            UserLoginReq req;
            UserLoginResp resp;

            // 错误处理函数(出错时调用)
            auto err = [this, &req, &resp, &response](const std::string &errmsg) -> void
            {
                response.set_content(resp.SerializeAsString(), "application/x-protbuf");
                resp.set_success(false);
                resp.set_errmsg(errmsg);
                return;
            };

            if (!req.ParseFromString(request.body))
            {
                ERROR("用户账号登录请求正文反序列化失败");
                return err("用户账号登录请求正文反序列化失败");
            }

            // 将请求转发给用户子服务进行业务处理
            auto channel = _channels->choose(_userServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到用户管理子服务节点 - {}", req.requestid(), _userServiceName);
                return err("未找到用户管理子服务节点");
            }

            UserService_Stub stub(channel.get());
            brpc::Controller cntl;
            stub.UserLogin(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 用户子服务调用失败：{}", req.requestid(), cntl.ErrorText());
                return err("用户子服务调用失败");
            }

            // 得到用户子服务的响应后将响应内容进行序列化作为http响应正文
            response.set_content(resp.SerializeAsString(), "application/x-protbuf");
        }

        void phoneLogin(const httplib::Request &request, httplib::Response &response)
        {
            // 取出http请求正文，将正文进行反序列化
            PhoneLoginReq req;
            PhoneLoginResp resp;

            // 错误处理函数(出错时调用)
            auto err = [this, &req, &resp, &response](const std::string &errmsg) -> void
            {
                response.set_content(resp.SerializeAsString(), "application/x-protbuf");
                resp.set_success(false);
                resp.set_errmsg(errmsg);
                return;
            };

            if (!req.ParseFromString(request.body))
            {
                ERROR("用户手机登录请求正文反序列化失败");
                return err("用户手机登录请求正文反序列化失败");
            }

            // 将请求转发给用户子服务进行业务处理
            auto channel = _channels->choose(_userServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到用户管理子服务节点 - {}", req.requestid(), _userServiceName);
                return err("未找到用户管理子服务节点");
            }

            UserService_Stub stub(channel.get());
            brpc::Controller cntl;
            stub.PhoneLogin(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 用户子服务调用失败：{}", req.requestid(), cntl.ErrorText());
                return err("用户子服务调用失败");
            }

            // 得到用户子服务的响应后将响应内容进行序列化作为http响应正文
            response.set_content(resp.SerializeAsString(), "application/x-protbuf");
        }

        void getUserInfo(const httplib::Request &request, httplib::Response &response)
        {
            // 取出http请求正文，将正文进行反序列化
            GetUserInfoReq req;
            GetUserInfoResp resp;

            // 错误处理函数(出错时调用)
            auto err = [this, &req, &resp, &response](const std::string &errmsg) -> void
            {
                response.set_content(resp.SerializeAsString(), "application/x-protbuf");
                resp.set_success(false);
                resp.set_errmsg(errmsg);
                return;
            };

            if (!req.ParseFromString(request.body))
            {
                ERROR("获取用户信息请求正文反序列化失败");
                return err("获取用户信息请求正文反序列化失败");
            }

            // 客户端身份识别与鉴权
            std::string sid = req.loginsessionid();
            auto uid = _loginSessionRedis->uid(sid);
            if (!uid)
            {
                ERROR("获取登录会话关联用户信息失败 {}", sid);
                return err("获取登录会话关联用户信息失败");
            }
            req.set_userid(*uid);

            // 将请求转发给用户子服务进行业务处理
            auto channel = _channels->choose(_userServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到用户管理子服务节点 - {}", req.requestid(), _userServiceName);
                return err("未找到用户管理子服务节点");
            }

            UserService_Stub stub(channel.get());
            brpc::Controller cntl;
            stub.GetUserInfo(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 用户子服务调用失败：{}", req.requestid(), cntl.ErrorText());
                return err("用户子服务调用失败：");
            }

            // 得到用户子服务的响应后将响应内容进行序列化作为http响应正文
            response.set_content(resp.SerializeAsString(), "application/x-protbuf");
        }

        void setPhoto(const httplib::Request &request, httplib::Response &response)
        {
            // 取出http请求正文，将正文进行反序列化
            SetUserPhotoReq req;
            SetUserPhotoResp resp;

            // 错误处理函数(出错时调用)
            auto err = [this, &req, &resp, &response](const std::string &errmsg) -> void
            {
                response.set_content(resp.SerializeAsString(), "application/x-protbuf");
                resp.set_success(false);
                resp.set_errmsg(errmsg);
                return;
            };

            if (!req.ParseFromString(request.body))
            {
                ERROR("设置用户头像请求正文反序列化失败");
                return err("设置用户头像请求正文反序列化失败");
            }

            // 客户端身份识别与鉴权
            std::string sid = req.loginsessionid();
            auto uid = _loginSessionRedis->uid(sid);
            if (!uid)
            {
                ERROR("获取登录会话关联用户信息失败 {}", sid);
                return err("获取登录会话关联用户信息失败");
            }
            req.set_userid(*uid);

            // 将请求转发给用户子服务进行业务处理
            auto channel = _channels->choose(_userServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到用户管理子服务节点 - {}", req.requestid(), _userServiceName);
                return err("未找到用户管理子服务节点");
            }

            UserService_Stub stub(channel.get());
            brpc::Controller cntl;
            stub.SetUserPhoto(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 用户子服务调用失败：{}", req.requestid(), cntl.ErrorText());
                return err("用户子服务调用失败：");
            }

            // 得到用户子服务的响应后将响应内容进行序列化作为http响应正文
            response.set_content(resp.SerializeAsString(), "application/x-protbuf");
        }

        void setNickname(const httplib::Request &request, httplib::Response &response)
        {
            // 取出http请求正文，将正文进行反序列化
            SetNicknameReq req;
            SetNicknameResp resp;

            // 错误处理函数(出错时调用)
            auto err = [this, &req, &resp, &response](const std::string &errmsg) -> void
            {
                response.set_content(resp.SerializeAsString(), "application/x-protbuf");
                resp.set_success(false);
                resp.set_errmsg(errmsg);
                return;
            };

            if (!req.ParseFromString(request.body))
            {
                ERROR("设置用户昵称请求正文反序列化失败");
                return err("设置用户昵称请求正文反序列化失败");
            }

            // 客户端身份识别与鉴权
            std::string sid = req.loginsessionid();
            auto uid = _loginSessionRedis->uid(sid);
            if (!uid)
            {
                ERROR("获取登录会话关联用户信息失败 {}", sid);
                return err("获取登录会话关联用户信息失败");
            }
            req.set_userid(*uid);

            // 将请求转发给用户子服务进行业务处理
            auto channel = _channels->choose(_userServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到用户管理子服务节点 - {}", req.requestid(), _userServiceName);
                return err("未找到用户管理子服务节点");
            }

            UserService_Stub stub(channel.get());
            brpc::Controller cntl;
            stub.SetUserNickname(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 用户子服务调用失败：{}", req.requestid(), cntl.ErrorText());
                return err("用户子服务调用失败：");
            }

            // 得到用户子服务的响应后将响应内容进行序列化作为http响应正文
            response.set_content(resp.SerializeAsString(), "application/x-protbuf");
        }

        void setDesc(const httplib::Request &request, httplib::Response &response)
        {
            // 取出http请求正文，将正文进行反序列化
            SetUserDescReq req;
            SetUserDescResp resp;

            // 错误处理函数(出错时调用)
            auto err = [this, &req, &resp, &response](const std::string &errmsg) -> void
            {
                response.set_content(resp.SerializeAsString(), "application/x-protbuf");
                resp.set_success(false);
                resp.set_errmsg(errmsg);
                return;
            };

            if (!req.ParseFromString(request.body))
            {
                ERROR("设置用户签名请求正文反序列化失败");
                return err("设置用户签名请求正文反序列化失败");
            }

            // 客户端身份识别与鉴权
            std::string sid = req.loginsessionid();
            auto uid = _loginSessionRedis->uid(sid);
            if (!uid)
            {
                ERROR("获取登录会话关联用户信息失败 {}", sid);
                return err("获取登录会话关联用户信息失败");
            }
            req.set_userid(*uid);

            // 将请求转发给用户子服务进行业务处理
            auto channel = _channels->choose(_userServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到用户管理子服务节点 - {}", req.requestid(), _userServiceName);
                return err("未找到用户管理子服务节点");
            }

            UserService_Stub stub(channel.get());
            brpc::Controller cntl;
            stub.SetUserDescription(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 用户子服务调用失败：{}", req.requestid(), cntl.ErrorText());
                return err("用户子服务调用失败：");
            }

            // 得到用户子服务的响应后将响应内容进行序列化作为http响应正文
            response.set_content(resp.SerializeAsString(), "application/x-protbuf");
        }

        void setPhone(const httplib::Request &request, httplib::Response &response)
        {
            // 取出http请求正文，将正文进行反序列化
            SetPhoneReq req;
            SetPhoneResp resp;

            // 错误处理函数(出错时调用)
            auto err = [this, &req, &resp, &response](const std::string &errmsg) -> void
            {
                response.set_content(resp.SerializeAsString(), "application/x-protbuf");
                resp.set_success(false);
                resp.set_errmsg(errmsg);
                return;
            };

            if (!req.ParseFromString(request.body))
            {
                ERROR("设置用户电话号码请求正文反序列化失败");
                return err("设置用户电话号码请求正文反序列化失败");
            }

            // 客户端身份识别与鉴权
            std::string sid = req.loginsessionid();
            auto uid = _loginSessionRedis->uid(sid);
            if (!uid)
            {
                ERROR("获取登录会话关联用户信息失败 {}", sid);
                return err("获取登录会话关联用户信息失败");
            }
            req.set_userid(*uid);

            // 将请求转发给用户子服务进行业务处理
            auto channel = _channels->choose(_userServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到用户管理子服务节点 - {}", req.requestid(), _userServiceName);
                return err("未找到用户管理子服务节点");
            }

            UserService_Stub stub(channel.get());
            brpc::Controller cntl;
            stub.SetUserPhoneNumber(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 用户子服务调用失败：{}", req.requestid(), cntl.ErrorText());
                return err("用户子服务调用失败：");
            }

            // 得到用户子服务的响应后将响应内容进行序列化作为http响应正文
            response.set_content(resp.SerializeAsString(), "application/x-protbuf");
        }

        void getFriends(const httplib::Request &request, httplib::Response &response)
        {
            // 取出http请求正文，将正文进行反序列化
            GetFriendListReq req;
            GetFriendListResp resp;

            // 错误处理函数(出错时调用)
            auto err = [this, &req, &resp, &response](const std::string &errmsg) -> void
            {
                response.set_content(resp.SerializeAsString(), "application/x-protbuf");
                resp.set_success(false);
                resp.set_errmsg(errmsg);
                return;
            };

            if (!req.ParseFromString(request.body))
            {
                ERROR("获取好友列表请求正文反序列化失败");
                return err("获取好友列表请求正文反序列化失败");
            }

            // 客户端身份识别与鉴权
            std::string sid = req.loginsessionid();
            auto uid = _loginSessionRedis->uid(sid);
            if (!uid)
            {
                ERROR("获取登录会话关联用户信息失败 {}", sid);
                return err("获取登录会话关联用户信息失败");
            }
            req.set_userid(*uid);

            // 将请求转发给用户子服务进行业务处理
            auto channel = _channels->choose(_friendServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到好友管理子服务节点 - {}", req.requestid(), _userServiceName);
                return err("未找到好友管理子服务节点");
            }

            FriendService_Stub stub(channel.get());
            brpc::Controller cntl;
            stub.GetFriendList(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 好友管理子服务调用失败：{}", req.requestid(), cntl.ErrorText());
                return err("好友管理子服务调用失败：");
            }

            // 得到用户子服务的响应后将响应内容进行序列化作为http响应正文
            response.set_content(resp.SerializeAsString(), "application/x-protbuf");
        }

        void addFriend(const httplib::Request &request, httplib::Response &response)
        {
            // 取出http请求正文，将正文进行反序列化
            FriendAddReq req;
            FriendAddResp resp;

            // 错误处理函数(出错时调用)
            auto err = [this, &req, &resp, &response](const std::string &errmsg) -> void
            {
                response.set_content(resp.SerializeAsString(), "application/x-protbuf");
                resp.set_success(false);
                resp.set_errmsg(errmsg);
                return;
            };

            if (!req.ParseFromString(request.body))
            {
                ERROR("好友添加请求正文反序列化失败");
                return err("好友添加请求正文反序列化失败");
            }

            // 客户端身份识别与鉴权
            std::string sid = req.loginsessionid();
            auto uid = _loginSessionRedis->uid(sid);
            if (!uid)
            {
                ERROR("获取登录会话关联用户信息失败 {}", sid);
                return err("获取登录会话关联用户信息失败");
            }
            req.set_userid(*uid);

            // 将请求转发给用户子服务进行业务处理
            auto channel = _channels->choose(_friendServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到好友管理子服务节点 - {}", req.requestid(), _userServiceName);
                return err("未找到好友管理子服务节点");
            }

            FriendService_Stub stub(channel.get());
            brpc::Controller cntl;
            stub.FriendAdd(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 好友管理子服务调用失败：{}", req.requestid(), cntl.ErrorText());
                return err("好友管理子服务调用失败：");
            }

            // 若业务处理完成后，则判断对方是否在线如若在线则实时发送申请消息
            auto conn = _connection->connection(req.friendid());
            if (resp.success() && conn)
            {
                auto user = _GetUserInfo(req.requestid(), *uid);
                if (!user)
                {
                    ERROR("{} 获取当前客户端用户信息失败！", req.requestid());
                    return err("获取当前客户端用户信息失败");
                }
                WebsocketMessage web;
                web.set_type(WebsocketType::FRIEND_ADD_APPLY);
                web.mutable_friendaddapply()->mutable_userinfo()->CopyFrom(user->user());
                conn->send(web.SerializeAsString(), websocketpp::frame::opcode::value::binary);
            }

            // 得到用户子服务的响应后将响应内容进行序列化作为http响应正文
            response.set_content(resp.SerializeAsString(), "application/x-protbuf");
        }

        void friendAddProcess(const httplib::Request &request, httplib::Response &response)
        {
            // 取出http请求正文，将正文进行反序列化
            FriendAddProcessReq req;
            FriendAddProcessResp resp;

            // 错误处理函数(出错时调用)
            auto err = [this, &req, &resp, &response](const std::string &errmsg) -> void
            {
                response.set_content(resp.SerializeAsString(), "application/x-protbuf");
                resp.set_success(false);
                resp.set_errmsg(errmsg);
                return;
            };

            if (!req.ParseFromString(request.body))
            {
                ERROR("处理好友添加请求正文反序列化失败");
                return err("处理好友添加请求正文反序列化失败");
            }

            // 客户端身份识别与鉴权
            std::string sid = req.loginsessionid();
            auto uid = _loginSessionRedis->uid(sid);
            if (!uid)
            {
                ERROR("获取登录会话关联用户信息失败 {}", sid);
                return err("获取登录会话关联用户信息失败");
            }
            req.set_userid(*uid);

            // 将请求转发给用户子服务进行业务处理
            auto channel = _channels->choose(_friendServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到好友管理子服务节点 - {}", req.requestid(), _userServiceName);
                return err("未找到好友管理子服务节点");
            }

            FriendService_Stub stub(channel.get());
            brpc::Controller cntl;
            stub.FriendAddProcess(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 好友管理子服务调用失败：{}", req.requestid(), cntl.ErrorText());
                return err("好友管理子服务调用失败：");
            }

            // 如果处理完成，则判断对方是否在线
            // 如若在线则发送处理通知
            // 若是处理为同意则为在线的用户创建聊天会话
            if (resp.success())
            {
                // 获取双方的用户信息
                auto user = _GetUserInfo(req.requestid(), *uid);
                if (!user)
                {
                    ERROR("{} 获取当前客户端用户信息失败！", req.requestid());
                    return err("获取当前客户端用户信息失败");
                }
                auto friendInfo = _GetUserInfo(req.requestid(), req.applyuserid());
                if (!friendInfo)
                {
                    ERROR("{} 获取当前客户端用户信息失败！", req.requestid());
                    return err("获取当前客户端用户信息失败");
                }

                // 获取双方的长连接
                auto userConn = _connection->connection(*uid);
                auto friendConn = _connection->connection(req.applyuserid());

                // 通知对方
                if (friendConn)
                {
                    WebsocketMessage web;
                    web.set_type(WebsocketType::FRIEND_ADD_PROCESS);
                    auto res = web.mutable_friendprocessresult();
                    res->mutable_userinfo()->CopyFrom(friendInfo->user());
                    res->set_agree(req.agree());
                    friendConn->send(web.SerializeAsString(), websocketpp::frame::opcode::value::binary);
                }
                // 给对方创建聊天会话
                if (req.agree() && friendConn)
                {
                    WebsocketMessage web;
                    web.set_type(WebsocketType::CHAT_SESSION_CREATE);
                    auto session = web.mutable_newchatsessioninfo();
                    session->mutable_chatsessioninfo()->set_singlechatfriendid(*uid);
                    session->mutable_chatsessioninfo()->set_chatsessionid(resp.newchatsessionid());
                    session->mutable_chatsessioninfo()->set_chatsessionname(user->user().nickname());
                    session->mutable_chatsessioninfo()->set_photo(user->user().photo());
                    friendConn->send(web.SerializeAsString(), websocketpp::frame::opcode::value::binary);
                }
                // 给自己创建聊天会话
                if (req.agree() && userConn)
                {
                    WebsocketMessage web;
                    web.set_type(WebsocketType::CHAT_SESSION_CREATE);
                    auto session = web.mutable_newchatsessioninfo();
                    session->mutable_chatsessioninfo()->set_singlechatfriendid(req.applyuserid());
                    session->mutable_chatsessioninfo()->set_chatsessionid(resp.newchatsessionid());
                    session->mutable_chatsessioninfo()->set_chatsessionname(friendInfo->user().nickname());
                    session->mutable_chatsessioninfo()->set_photo(friendInfo->user().photo());
                    friendConn->send(web.SerializeAsString(), websocketpp::frame::opcode::value::binary);
                }
            }

            // 得到用户子服务的响应后将响应内容进行序列化作为http响应正文
            response.set_content(resp.SerializeAsString(), "application/x-protbuf");
        }

        void friendRemove(const httplib::Request &request, httplib::Response &response)
        {
            // 取出http请求正文，将正文进行反序列化
            FriendRemoveReq req;
            FriendRemoveResp resp;

            // 错误处理函数(出错时调用)
            auto err = [this, &req, &resp, &response](const std::string &errmsg) -> void
            {
                response.set_content(resp.SerializeAsString(), "application/x-protbuf");
                resp.set_success(false);
                resp.set_errmsg(errmsg);
                return;
            };

            if (!req.ParseFromString(request.body))
            {
                ERROR("好友删除请求正文反序列化失败");
                return err("好友删除请求正文反序列化失败");
            }

            // 客户端身份识别与鉴权
            std::string sid = req.loginsessionid();
            auto uid = _loginSessionRedis->uid(sid);
            if (!uid)
            {
                ERROR("获取登录会话关联用户信息失败 {}", sid);
                return err("获取登录会话关联用户信息失败");
            }

            // 将请求转发给用户子服务进行业务处理
            auto channel = _channels->choose(_friendServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到好友管理子服务节点 - {}", req.requestid(), _userServiceName);
                return err("未找到好友管理子服务节点");
            }

            FriendService_Stub stub(channel.get());
            brpc::Controller cntl;
            stub.FriendRemove(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 好友管理子服务调用失败：{}", req.requestid(), cntl.ErrorText());
                return err("好友管理子服务调用失败：");
            }

            // 若业务处理完成后，则判断对方是否在线如若在线则实时发送申请消息
            auto conn = _connection->connection(req.friendid());
            if (resp.success() && conn)
            {
                WebsocketMessage web;
                web.set_type(WebsocketType::FRIEND_REMOVE);
                web.mutable_friendremove()->set_userid(*uid);
                conn->send(web.SerializeAsString(), websocketpp::frame::opcode::value::binary);
            }

            // 得到用户子服务的响应后将响应内容进行序列化作为http响应正文
            response.set_content(resp.SerializeAsString(), "application/x-protbuf");
        }

        void getFriendApplys(const httplib::Request &request, httplib::Response &response)
        {
            // 取出http请求正文，将正文进行反序列化
            GetPendingFriendEventListReq req;
            GetPendingFriendEventListResp resp;

            // 错误处理函数(出错时调用)
            auto err = [this, &req, &resp, &response](const std::string &errmsg) -> void
            {
                response.set_content(resp.SerializeAsString(), "application/x-protbuf");
                resp.set_success(false);
                resp.set_errmsg(errmsg);
                return;
            };

            if (!req.ParseFromString(request.body))
            {
                ERROR("获取好友添加请求列表请求正文反序列化失败");
                return err("获取好友添加请求列表请求正文反序列化失败");
            }

            // 客户端身份识别与鉴权
            std::string sid = req.loginsessionid();
            auto uid = _loginSessionRedis->uid(sid);
            if (!uid)
            {
                ERROR("获取登录会话关联用户信息失败 {}", sid);
                return err("获取登录会话关联用户信息失败");
            }

            // 将请求转发给用户子服务进行业务处理
            auto channel = _channels->choose(_friendServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到好友管理子服务节点 - {}", req.requestid(), _userServiceName);
                return err("未找到好友管理子服务节点");
            }

            FriendService_Stub stub(channel.get());
            brpc::Controller cntl;
            stub.GetPendingFriendEventList(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 好友管理子服务调用失败：{}", req.requestid(), cntl.ErrorText());
                return err("好友管理子服务调用失败：");
            }

            // 得到用户子服务的响应后将响应内容进行序列化作为http响应正文
            response.set_content(resp.SerializeAsString(), "application/x-protbuf");
        }

        void friendSearch(const httplib::Request &request, httplib::Response &response)
        {
            // 取出http请求正文，将正文进行反序列化
            FriendSearchReq req;
            FriendSearchResp resp;

            // 错误处理函数(出错时调用)
            auto err = [this, &req, &resp, &response](const std::string &errmsg) -> void
            {
                response.set_content(resp.SerializeAsString(), "application/x-protbuf");
                resp.set_success(false);
                resp.set_errmsg(errmsg);
                return;
            };

            if (!req.ParseFromString(request.body))
            {
                ERROR("好友查询请求正文反序列化失败");
                return err("好友查询请求正文反序列化失败");
            }

            // 客户端身份识别与鉴权
            std::string sid = req.loginsessionid();
            auto uid = _loginSessionRedis->uid(sid);
            if (!uid)
            {
                ERROR("获取登录会话关联用户信息失败 {}", sid);
                return err("获取登录会话关联用户信息失败");
            }

            // 将请求转发给用户子服务进行业务处理
            auto channel = _channels->choose(_friendServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到好友管理子服务节点 - {}", req.requestid(), _userServiceName);
                return err("未找到好友管理子服务节点");
            }

            FriendService_Stub stub(channel.get());
            brpc::Controller cntl;
            stub.FriendSearch(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 好友管理子服务调用失败：{}", req.requestid(), cntl.ErrorText());
                return err("好友管理子服务调用失败：");
            }

            // 得到用户子服务的响应后将响应内容进行序列化作为http响应正文
            response.set_content(resp.SerializeAsString(), "application/x-protbuf");
        }

        void getChatSessions(const httplib::Request &request, httplib::Response &response)
        {
            // 取出http请求正文，将正文进行反序列化
            GetChatSessionListReq req;
            GetChatSessionListResp resp;

            // 错误处理函数(出错时调用)
            auto err = [this, &req, &resp, &response](const std::string &errmsg) -> void
            {
                response.set_content(resp.SerializeAsString(), "application/x-protbuf");
                resp.set_success(false);
                resp.set_errmsg(errmsg);
                return;
            };

            if (!req.ParseFromString(request.body))
            {
                ERROR("获取聊天会话列表请求正文反序列化失败");
                return err("获取聊天会话列表请求正文反序列化失败");
            }

            // 客户端身份识别与鉴权
            std::string sid = req.loginsessionid();
            auto uid = _loginSessionRedis->uid(sid);
            if (!uid)
            {
                ERROR("获取登录会话关联用户信息失败 {}", sid);
                return err("获取登录会话关联用户信息失败");
            }

            // 将请求转发给用户子服务进行业务处理
            auto channel = _channels->choose(_chatSessionServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到聊天会话管理子服务节点 - {}", req.requestid(), _userServiceName);
                return err("未找到聊天会话管理子服务节点");
            }

            ChatSessionService_Stub stub(channel.get());
            brpc::Controller cntl;
            stub.GetChatSessionList(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 聊天会话管理子服务调用失败：{}", req.requestid(), cntl.ErrorText());
                return err("聊天会话管理子服务调用失败：");
            }

            // 得到用户子服务的响应后将响应内容进行序列化作为http响应正文
            response.set_content(resp.SerializeAsString(), "application/x-protbuf");
        }

        void getChatSessionUser(const httplib::Request &request, httplib::Response &response)
        {
            // 取出http请求正文，将正文进行反序列化
            GetChatSessionMemberReq req;
            GetChatSessionMemberResp resp;

            // 错误处理函数(出错时调用)
            auto err = [this, &req, &resp, &response](const std::string &errmsg) -> void
            {
                response.set_content(resp.SerializeAsString(), "application/x-protbuf");
                resp.set_success(false);
                resp.set_errmsg(errmsg);
                return;
            };

            if (!req.ParseFromString(request.body))
            {
                ERROR("获取聊天会话成员请求正文反序列化失败");
                return err("获取聊天会话成员请求正文反序列化失败");
            }

            // 客户端身份识别与鉴权
            std::string sid = req.loginsessionid();
            auto uid = _loginSessionRedis->uid(sid);
            if (!uid)
            {
                ERROR("获取登录会话关联用户信息失败 {}", sid);
                return err("获取登录会话关联用户信息失败");
            }

            // 将请求转发给用户子服务进行业务处理
            auto channel = _channels->choose(_chatSessionServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到聊天会话管理子服务节点 - {}", req.requestid(), _userServiceName);
                return err("未找到聊天会话管理子服务节点");
            }

            ChatSessionService_Stub stub(channel.get());
            brpc::Controller cntl;
            stub.GetChatSessionMember(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 聊天会话管理子服务调用失败：{}", req.requestid(), cntl.ErrorText());
                return err("聊天会话管理子服务调用失败：");
            }

            // 得到用户子服务的响应后将响应内容进行序列化作为http响应正文
            response.set_content(resp.SerializeAsString(), "application/x-protbuf");
        }

        void createChatSession(const httplib::Request &request, httplib::Response &response)
        {
            // 取出http请求正文，将正文进行反序列化
            ChatSessionCreateReq req;
            ChatSessionCreateResp resp;

            // 错误处理函数(出错时调用)
            auto err = [this, &req, &resp, &response](const std::string &errmsg) -> void
            {
                response.set_content(resp.SerializeAsString(), "application/x-protbuf");
                resp.set_success(false);
                resp.set_errmsg(errmsg);
                return;
            };

            if (!req.ParseFromString(request.body))
            {
                ERROR("获取聊天会话成员请求正文反序列化失败");
                return err("获取聊天会话成员请求正文反序列化失败");
            }

            // 客户端身份识别与鉴权
            std::string sid = req.loginsessionid();
            auto uid = _loginSessionRedis->uid(sid);
            if (!uid)
            {
                ERROR("获取登录会话关联用户信息失败 {}", sid);
                return err("获取登录会话关联用户信息失败");
            }

            // 将请求转发给用户子服务进行业务处理
            auto channel = _channels->choose(_chatSessionServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到聊天会话管理子服务节点 - {}", req.requestid(), _userServiceName);
                return err("未找到聊天会话管理子服务节点");
            }

            ChatSessionService_Stub stub(channel.get());
            brpc::Controller cntl;
            stub.ChatSessionCreate(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 聊天会话管理子服务调用失败：{}", req.requestid(), cntl.ErrorText());
                return err("聊天会话管理子服务调用失败：");
            }

            // 业务处理完毕 判断会话成员是否在线 在线的则发送通知
            if (resp.success())
            {
                for (int i = 0; i < req.userids_size(); ++i)
                {
                    auto conn = _connection->connection(req.userids(i));
                    if (!conn)
                        break;

                    WebsocketMessage web;
                    web.set_type(WebsocketType::CHAT_SESSION_CREATE);
                    auto session = web.mutable_newchatsessioninfo();
                    session->mutable_chatsessioninfo()->CopyFrom(resp.chatsessioninfo());
                    conn->send(web.SerializeAsString(), websocketpp::frame::opcode::value::binary);
                }
            }

            resp.clear_chatsessioninfo();
            // 得到用户子服务的响应后将响应内容进行序列化作为http响应正文
            response.set_content(resp.SerializeAsString(), "application/x-protbuf");
        }

        void getHistoryMessage(const httplib::Request &request, httplib::Response &response)
        {
            // 取出http请求正文，将正文进行反序列化
            GetHistoryMsgReq req;
            GetHistoryMsgResp resp;

            // 错误处理函数(出错时调用)
            auto err = [this, &req, &resp, &response](const std::string &errmsg) -> void
            {
                response.set_content(resp.SerializeAsString(), "application/x-protbuf");
                resp.set_success(false);
                resp.set_errmsg(errmsg);
                return;
            };

            if (!req.ParseFromString(request.body))
            {
                ERROR("获取聊天会话历史消息请求正文反序列化失败");
                return err("获取聊天会话历史消息请求正文反序列化失败");
            }

            // 客户端身份识别与鉴权
            std::string sid = req.loginsessionid();
            auto uid = _loginSessionRedis->uid(sid);
            if (!uid)
            {
                ERROR("获取登录会话关联用户信息失败 {}", sid);
                return err("获取登录会话关联用户信息失败");
            }

            // 将请求转发给用户子服务进行业务处理
            auto channel = _channels->choose(_messageServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到消息管理子服务节点 - {}", req.requestid(), _userServiceName);
                return err("未找到消息管理子服务节点");
            }

            MessageService_Stub stub(channel.get());
            brpc::Controller cntl;
            stub.GetHistoryMsg(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 消息管理子服务调用失败：{}", req.requestid(), cntl.ErrorText());
                return err("消息管理子服务调用失败：");
            }

            // 得到用户子服务的响应后将响应内容进行序列化作为http响应正文
            response.set_content(resp.SerializeAsString(), "application/x-protbuf");
        }

        void getRecentMsg(const httplib::Request &request, httplib::Response &response)
        {
            // 取出http请求正文，将正文进行反序列化
            GetRecentMsgReq req;
            GetRecentMsgResp resp;

            // 错误处理函数(出错时调用)
            auto err = [this, &req, &resp, &response](const std::string &errmsg) -> void
            {
                response.set_content(resp.SerializeAsString(), "application/x-protbuf");
                resp.set_success(false);
                resp.set_errmsg(errmsg);
                return;
            };

            if (!req.ParseFromString(request.body))
            {
                ERROR("获取指定条数历史消息请求正文反序列化失败");
                return err("获取指定条数历史消息请求正文反序列化失败");
            }

            // 客户端身份识别与鉴权
            std::string sid = req.loginsessionid();
            auto uid = _loginSessionRedis->uid(sid);
            if (!uid)
            {
                ERROR("获取登录会话关联用户信息失败 {}", sid);
                return err("获取登录会话关联用户信息失败");
            }

            // 将请求转发给用户子服务进行业务处理
            auto channel = _channels->choose(_messageServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到消息管理子服务节点 - {}", req.requestid(), _userServiceName);
                return err("未找到消息管理子服务节点");
            }

            MessageService_Stub stub(channel.get());
            brpc::Controller cntl;
            stub.GetRecentMsg(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 消息管理子服务调用失败：{}", req.requestid(), cntl.ErrorText());
                return err("消息管理子服务调用失败：");
            }

            // 得到用户子服务的响应后将响应内容进行序列化作为http响应正文
            response.set_content(resp.SerializeAsString(), "application/x-protbuf");
        }

        void messageSearch(const httplib::Request &request, httplib::Response &response)
        {
            // 取出http请求正文，将正文进行反序列化
            MsgSearchReq req;
            MsgSearchResp resp;

            // 错误处理函数(出错时调用)
            auto err = [this, &req, &resp, &response](const std::string &errmsg) -> void
            {
                response.set_content(resp.SerializeAsString(), "application/x-protbuf");
                resp.set_success(false);
                resp.set_errmsg(errmsg);
                return;
            };

            if (!req.ParseFromString(request.body))
            {
                ERROR("消息搜索请求正文反序列化失败");
                return err("消息搜索请求正文反序列化失败");
            }

            // 客户端身份识别与鉴权
            std::string sid = req.loginsessionid();
            auto uid = _loginSessionRedis->uid(sid);
            if (!uid)
            {
                ERROR("获取登录会话关联用户信息失败 {}", sid);
                return err("获取登录会话关联用户信息失败");
            }

            // 将请求转发给用户子服务进行业务处理
            auto channel = _channels->choose(_messageServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到消息管理子服务节点 - {}", req.requestid(), _userServiceName);
                return err("未找到消息管理子服务节点");
            }

            MessageService_Stub stub(channel.get());
            brpc::Controller cntl;
            stub.MsgSearch(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 消息管理子服务调用失败：{}", req.requestid(), cntl.ErrorText());
                return err("消息管理子服务调用失败：");
            }

            // 得到用户子服务的响应后将响应内容进行序列化作为http响应正文
            response.set_content(resp.SerializeAsString(), "application/x-protbuf");
        }

        void getSingleFile(const httplib::Request &request, httplib::Response &response)
        {
            // 取出http请求正文，将正文进行反序列化
            GetSingleFileReq req;
            GetSingleFileResp resp;

            // 错误处理函数(出错时调用)
            auto err = [this, &req, &resp, &response](const std::string &errmsg) -> void
            {
                response.set_content(resp.SerializeAsString(), "application/x-protbuf");
                resp.set_success(false);
                resp.set_errmsg(errmsg);
                return;
            };

            if (!req.ParseFromString(request.body))
            {
                ERROR("单个文件下载请求正文反序列化失败");
                return err("单个文件下载请求正文反序列化失败");
            }

            // 客户端身份识别与鉴权
            std::string sid = req.sessionid();
            auto uid = _loginSessionRedis->uid(sid);
            if (!uid)
            {
                ERROR("获取登录会话关联用户信息失败 {}", sid);
                return err("获取登录会话关联用户信息失败");
            }

            // 将请求转发给用户子服务进行业务处理
            auto channel = _channels->choose(_fileServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到文件管理子服务节点 - {}", req.requestid(), _userServiceName);
                return err("未找到文件管理子服务节点");
            }

            FileService_Stub stub(channel.get());
            brpc::Controller cntl;
            stub.GetSingleFile(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 文件管理子服务调用失败：{}", req.requestid(), cntl.ErrorText());
                return err("文件管理子服务调用失败：");
            }

            // 得到用户子服务的响应后将响应内容进行序列化作为http响应正文
            response.set_content(resp.SerializeAsString(), "application/x-protbuf");
        }

        void getMultiFile(const httplib::Request &request, httplib::Response &response)
        {
            // 取出http请求正文，将正文进行反序列化
            GetMultiFileReq req;
            GetMultiFileResp resp;

            // 错误处理函数(出错时调用)
            auto err = [this, &req, &resp, &response](const std::string &errmsg) -> void
            {
                response.set_content(resp.SerializeAsString(), "application/x-protbuf");
                resp.set_success(false);
                resp.set_errmsg(errmsg);
                return;
            };

            if (!req.ParseFromString(request.body))
            {
                ERROR("多个文件下载请求正文反序列化失败");
                return err("多个文件下载请求正文反序列化失败");
            }

            // 客户端身份识别与鉴权
            std::string sid = req.sessionid();
            auto uid = _loginSessionRedis->uid(sid);
            if (!uid)
            {
                ERROR("获取登录会话关联用户信息失败 {}", sid);
                return err("获取登录会话关联用户信息失败");
            }

            // 将请求转发给用户子服务进行业务处理
            auto channel = _channels->choose(_fileServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到文件管理子服务节点 - {}", req.requestid(), _userServiceName);
                return err("未找到文件管理子服务节点");
            }

            FileService_Stub stub(channel.get());
            brpc::Controller cntl;
            stub.GetMultiFile(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 文件管理子服务调用失败：{}", req.requestid(), cntl.ErrorText());
                return err("文件管理子服务调用失败：");
            }

            // 得到用户子服务的响应后将响应内容进行序列化作为http响应正文
            response.set_content(resp.SerializeAsString(), "application/x-protbuf");
        }

        void putSingleFile(const httplib::Request &request, httplib::Response &response)
        {
            // 取出http请求正文，将正文进行反序列化
            PutSingleFileReq req;
            PutSingleFileResp resp;

            // 错误处理函数(出错时调用)
            auto err = [this, &req, &resp, &response](const std::string &errmsg) -> void
            {
                response.set_content(resp.SerializeAsString(), "application/x-protbuf");
                resp.set_success(false);
                resp.set_errmsg(errmsg);
                return;
            };

            if (!req.ParseFromString(request.body))
            {
                ERROR("单个文件上传请求正文反序列化失败");
                return err("单个文件上传请求正文反序列化失败");
            }

            // 客户端身份识别与鉴权
            std::string sid = req.sessionid();
            auto uid = _loginSessionRedis->uid(sid);
            if (!uid)
            {
                ERROR("获取登录会话关联用户信息失败 {}", sid);
                return err("获取登录会话关联用户信息失败");
            }

            // 将请求转发给用户子服务进行业务处理
            auto channel = _channels->choose(_fileServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到文件管理子服务节点 - {}", req.requestid(), _userServiceName);
                return err("未找到文件管理子服务节点");
            }

            FileService_Stub stub(channel.get());
            brpc::Controller cntl;
            stub.PutSingleFile(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 文件管理子服务调用失败：{}", req.requestid(), cntl.ErrorText());
                return err("文件管理子服务调用失败：");
            }

            // 得到用户子服务的响应后将响应内容进行序列化作为http响应正文
            response.set_content(resp.SerializeAsString(), "application/x-protbuf");
        }

        void putMultiFile(const httplib::Request &request, httplib::Response &response)
        {
            // 取出http请求正文，将正文进行反序列化
            PutMultiFileReq req;
            PutMultiFileResp resp;

            // 错误处理函数(出错时调用)
            auto err = [this, &req, &resp, &response](const std::string &errmsg) -> void
            {
                response.set_content(resp.SerializeAsString(), "application/x-protbuf");
                resp.set_success(false);
                resp.set_errmsg(errmsg);
                return;
            };

            if (!req.ParseFromString(request.body))
            {
                ERROR("多个文件上传请求正文反序列化失败");
                return err("多个文件上传请求正文反序列化失败");
            }

            // 客户端身份识别与鉴权
            std::string sid = req.sessionid();
            auto uid = _loginSessionRedis->uid(sid);
            if (!uid)
            {
                ERROR("获取登录会话关联用户信息失败 {}", sid);
                return err("获取登录会话关联用户信息失败");
            }

            // 将请求转发给用户子服务进行业务处理
            auto channel = _channels->choose(_fileServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到文件管理子服务节点 - {}", req.requestid(), _userServiceName);
                return err("未找到文件管理子服务节点");
            }

            FileService_Stub stub(channel.get());
            brpc::Controller cntl;
            stub.PutMultiFile(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 文件管理子服务调用失败：{}", req.requestid(), cntl.ErrorText());
                return err("文件管理子服务调用失败：");
            }

            // 得到用户子服务的响应后将响应内容进行序列化作为http响应正文
            response.set_content(resp.SerializeAsString(), "application/x-protbuf");
        }

        void recognition(const httplib::Request &request, httplib::Response &response)
        {
            // 取出http请求正文，将正文进行反序列化
            SpeechReq req;
            SpeechRsp resp;

            // 错误处理函数(出错时调用)
            auto err = [this, &req, &resp, &response](const std::string &errmsg) -> void
            {
                response.set_content(resp.SerializeAsString(), "application/x-protbuf");
                resp.set_success(false);
                resp.set_errmsg(errmsg);
                return;
            };

            if (!req.ParseFromString(request.body))
            {
                ERROR("语音转文字请求正文反序列化失败");
                return err("语音转文字请求正文反序列化失败");
            }

            // 客户端身份识别与鉴权
            std::string sid = req.sessionid();
            auto uid = _loginSessionRedis->uid(sid);
            if (!uid)
            {
                ERROR("获取登录会话关联用户信息失败 {}", sid);
                return err("获取登录会话关联用户信息失败");
            }

            // 将请求转发给用户子服务进行业务处理
            auto channel = _channels->choose(_speechServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到语音管理子服务节点 - {}", req.requestid(), _userServiceName);
                return err("未找到语音管理子服务节点");
            }

            SpeechService_Stub stub(channel.get());
            brpc::Controller cntl;
            stub.SpeechRecognition(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 语音管理子服务调用失败：{}", req.requestid(), cntl.ErrorText());
                return err("语音管理子服务调用失败：");
            }

            // 得到用户子服务的响应后将响应内容进行序列化作为http响应正文
            response.set_content(resp.SerializeAsString(), "application/x-protbuf");
        }

        void newMessage(const httplib::Request &request, httplib::Response &response)
        {
            // 取出http请求正文，将正文进行反序列化
            NewMessageReq req;
            NewMessageResp resp;
            GetTransmitTargetResp tranResp;

            // 错误处理函数(出错时调用)
            auto err = [this, &req, &resp, &response](const std::string &errmsg) -> void
            {
                response.set_content(resp.SerializeAsString(), "application/x-protbuf");
                resp.set_success(false);
                resp.set_errmsg(errmsg);
                return;
            };

            if (!req.ParseFromString(request.body))
            {
                ERROR("新消息请求正文反序列化失败");
                return err("新消息请求正文反序列化失败");
            }

            // 客户端身份识别与鉴权
            std::string sid = req.loginsessionid();
            auto uid = _loginSessionRedis->uid(sid);
            if (!uid)
            {
                ERROR("获取登录会话关联用户信息失败 {}", sid);
                return err("获取登录会话关联用户信息失败");
            }

            // 将请求转发给用户子服务进行业务处理
            auto channel = _channels->choose(_chatSessionServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到聊天会话管理子服务节点 - {}", req.requestid(), _userServiceName);
                return err("未找到聊天会话管理子服务节点");
            }

            ChatSessionService_Stub stub(channel.get());
            brpc::Controller cntl;
            stub.GetTransmitTarget(&cntl, &req, &tranResp, nullptr);
            if (cntl.Failed() || !tranResp.success())
            {
                ERROR("{} - 聊天会话管理子服务调用失败：{}", req.requestid(), cntl.ErrorText());
                return err("聊天会话管理子服务调用失败：");
            }

            // 业务处理成功后，将消息转发给聊天会话中的所有成员，不需要转发给自己
            if (tranResp.success())
            {
                for (int i = 0; i < tranResp.targetids_size(); ++i)
                {
                    std::string id = tranResp.targetids(i);
                    if (id == *uid)
                        continue;

                    auto conn = _connection->connection(id);
                    if (!conn)
                        continue;
                    WebsocketMessage web;
                    web.set_type(WebsocketType::CHAT_MESSAGE);
                    auto message = web.mutable_newmessageinfo();
                    message->mutable_messageinfo()->CopyFrom(tranResp.message());
                    conn->send(web.SerializeAsString(), websocketpp::frame::opcode::value::binary);
                }
            }

            resp.set_requestid(req.requestid());
            resp.set_success(tranResp.success());
            resp.set_errmsg(tranResp.errmsg());
            // 得到用户子服务的响应后将响应内容进行序列化作为http响应正文
            response.set_content(resp.SerializeAsString(), "application/x-protbuf");
        }
    };

    class GatewayServerBuilder
    {
    private:
        AllServiceChannel::ptr _channels;    // 用户服务信道操作对象
        std::string _fileServiceName;        // 文件服务的名称
        std::string _userServiceName;        // 用户服务的名称
        std::string _messageServiceName;     // 消息服务的名称
        std::string _friendServiceName;      // 好友服务的名称
        std::string _speechServiceName;      // 语音服务的名称
        std::string _chatSessionServiceName; // 聊天会话服务的名称
        int _websocketPort;
        int _httpPort;
        std::shared_ptr<sw::redis::Redis> _redis;
        EtcdDisClient::ptr _disClient;

    public:
        // 构造redis客户端对象
        void makeRedis(const std::string &host,
                       int port,
                       int db,
                       bool keepAlive)
        {
            _redis = RedisClientFactory::create(host, port, db, keepAlive);
        }

        // 构造服务发现客户端和信道管理对象
        void makeEtcdDis(const std::string &regHost,
                         const std::string &baseServiceName,
                         const std::string &fileServiceName,
                         const std::string &friendServiceName,
                         const std::string &messageServiceName,
                         const std::string &userServiceName,
                         const std::string &speechServiceName,
                         const std::string &chatSessionServiceName)
        {
            _fileServiceName = fileServiceName;
            _friendServiceName = friendServiceName;
            _messageServiceName = messageServiceName;
            _userServiceName = userServiceName;
            _speechServiceName = speechServiceName;
            _chatSessionServiceName = chatSessionServiceName;
            _channels = std::make_shared<hjb::AllServiceChannel>();
            _channels->declared(_fileServiceName);
            _channels->declared(_friendServiceName);
            _channels->declared(_messageServiceName);
            _channels->declared(_userServiceName);
            _channels->declared(_speechServiceName);
            _channels->declared(_chatSessionServiceName);

            auto putCb = std::bind(&hjb::AllServiceChannel::onServiceOnline, _channels.get(), std::placeholders::_1, std::placeholders::_2);
            auto delCb = std::bind(&hjb::AllServiceChannel::onServiceOffline, _channels.get(), std::placeholders::_1, std::placeholders::_2);
            _disClient = std::make_shared<hjb::EtcdDisClient>(regHost, baseServiceName, putCb, delCb);
        }

        void makeServerObject(int websocketPort, int httpPort)
        {
            _websocketPort = websocketPort;
            _httpPort = httpPort;
        }

        // 构造RPC服务器对象
        GatewayServer::ptr build()
        {
            if (!_disClient)
            {
                ERROR("未初始化服务发现模块");
                abort();
            }
            if (!_redis)
            {
                ERROR("未初始化redis模块");
                abort();
            }
            if (!_channels)
            {
                ERROR("未初始化信道管理模块");
                abort();
            }

            GatewayServer::ptr server = std::make_shared<GatewayServer>(_redis,
                                                                        _channels,
                                                                        _fileServiceName,
                                                                        _messageServiceName,
                                                                        _userServiceName,
                                                                        _friendServiceName,
                                                                        _speechServiceName,
                                                                        _chatSessionServiceName,
                                                                        _websocketPort,
                                                                        _httpPort,
                                                                        _disClient);
            return server;
        }
    };
}