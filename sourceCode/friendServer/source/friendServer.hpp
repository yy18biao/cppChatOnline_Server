#include <brpc/server.h>
#include <butil/logging.h>

#include "log.hpp"
#include "etcd.hpp"
#include "util.hpp"
#include "MFriend.hpp"
#include "MFriendApply.hpp"
#include "MChatSession.hpp"
#include "MChatSessionUser.hpp"
#include "channel.hpp"
#include "rabbitMQ.hpp"
#include "esData.hpp"

#include "base.pb.h"
#include "user.pb.h"
#include "chatSession.pb.h"
#include "friend.pb.h"

namespace hjb
{
    // 好友服务
    class FriendServiceImpl : public FriendService
    {
    private:
        AllServiceChannel::ptr _channels; // 服务信道操作对象
        FriendTable::ptr _friendMysql;
        FriendApplyTable::ptr _friendApplyMysql;
        ChatSessionTable::ptr _chatSessionMysql;
        ChatSessionUserTable::ptr _chatSessionUserMysql;
        ESUser::ptr _es;
        std::string _userServiceName;

    public:
        FriendServiceImpl(const AllServiceChannel::ptr &channels,
                          const std::shared_ptr<odb::core::database> &mysql,
                          const std::shared_ptr<elasticlient::Client> &es,
                          const std::string &userServiceName)
            : _channels(channels),
              _friendMysql(std::make_shared<FriendTable>(mysql)),
              _friendApplyMysql(std::make_shared<FriendApplyTable>(mysql)),
              _chatSessionMysql(std::make_shared<ChatSessionTable>(mysql)),
              _chatSessionUserMysql(std::make_shared<ChatSessionUserTable>(mysql)),
              _es(std::make_shared<ESUser>(es)),
              _userServiceName(userServiceName)
        {
        }

        virtual void GetFriendList(::google::protobuf::RpcController *controller,
                                   const ::hjb::GetFriendListReq *request,
                                   ::hjb::GetFriendListResp *response,
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

            // 从数据库中查询获取用户的好友ID
            auto friendIds = _friendMysql->friends(uid);
            std::unordered_set<std::string> userIds;
            for (auto &id : friendIds)
            {
                userIds.insert(id);
            }

            // 从用户子服务批量获取用户信息
            std::unordered_map<std::string, UserProto> users;
            if (!_getUser(rid, friendIds, users))
            {
                ERROR("{} - 批量获取用户信息失败", rid);
                return err(rid, "批量获取用户信息失败");
            }

            response->set_requestid(rid);
            response->set_success(true);
            for (const auto &user : users)
            {
                auto userinfo = response->add_friends();
                userinfo->CopyFrom(user.second);
            }
        }

        virtual void FriendRemove(::google::protobuf::RpcController *controller,
                                  const ::hjb::FriendRemoveReq *request,
                                  ::hjb::FriendRemoveResp *response,
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
            std::string fid = request->friendid();

            // 从好友关系表中删除好友关系信息
            if (!_friendMysql->remove(uid, fid))
            {
                ERROR("{} - 从数据库删除好友信息失败", rid);
                return err(rid, "从数据库删除好友信息失败");
            }

            // 从会话信息表中删除对应的聊天会话, 同时删除会话成员表中的成员信息
            if (!_chatSessionMysql->remove(uid, fid))
            {
                ERROR("{}- 从数据库删除好友会话信息失败", rid);
                return err(rid, "从数据库删除好友会话信息失败");
            }

            response->set_requestid(rid);
            response->set_success(true);
        }

        virtual void FriendAdd(::google::protobuf::RpcController *controller,
                               const ::hjb::FriendAddReq *request,
                               ::hjb::FriendAddResp *response,
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
            std::string fid = request->friendid();

            // 判断两人是否已经是好友
            if (_friendMysql->exists(uid, fid))
            {
                ERROR("申请好友失败 {}-{}已经是好友关系", uid, fid);
                return err(rid, "已经是好友关系");
            }

            // 是否已经申请过好友
            if (_friendApplyMysql->exists(uid, fid))
            {
                ERROR("申请好友失败-已经申请过对方为好友", uid, fid);
                return err(rid, "已经申请过对方为好友");
            }

            // 向好友申请表中新增申请信息
            std::string eid = uuid();
            FriendApply ev(eid, uid, fid);
            if (!_friendApplyMysql->insert(ev))
            {
                ERROR("{} - 向数据库新增好友申请事件失败", rid);
                return err(rid, "向数据库新增好友申请事件失败");
            }

            response->set_requestid(rid);
            response->set_success(true);
            response->set_eventid(eid);
        }

        virtual void FriendAddProcess(::google::protobuf::RpcController *controller,
                                      const ::hjb::FriendAddProcessReq *request,
                                      ::hjb::FriendAddProcessResp *response,
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
            std::string eid = request->eventid();
            std::string uid = request->userid();
            std::string fid = request->applyuserid();
            bool agree = request->agree();

            // 判断有没有该申请事件
            if (!_friendApplyMysql->exists(fid, uid))
            {
                ERROR("{}- 没有找到{}-{}对应的好友申请事件", rid, fid, uid);
                return err(rid, "没有找到对应的好友申请事件");
            }

            // 获取该申请数据
            std::shared_ptr<FriendApply> fa = _friendApplyMysql->searchApply(eid);

            std::string sid;
            // 同意
            if (agree)
            {
                // 新增好友表
                if (!_friendMysql->insert(uid, fid))
                {
                    ERROR("{}- 新增好友关系信息失败-{}-{}", rid, uid, fid);
                    return err(rid, "新增好友关系信息失败");
                }

                // 新增会话信息
                sid = uuid();
                ChatSession cs(sid, "", ChatSessionType::SINGLE);
                if (!_chatSessionMysql->insert(cs))
                {
                    ERROR("{}- 新增单聊会话信息 {} 失败", rid, sid);
                    return err(rid, "新增单聊会话信息失败");
                }

                // 新增会话和用户关联信息
                ChatSessionUser csu1(sid, uid);
                ChatSessionUser csu2(sid, fid);
                std::vector<ChatSessionUser> csus;
                csus.push_back(csu1);
                csus.push_back(csu2);
                if (!_chatSessionUserMysql->append(csus))
                {
                    ERROR("{}- 新增单聊会话与用户关联 {} 失败", rid, sid);
                    return err(rid, "新增单聊会话与用户关联失败");
                }

                // 修改申请数据
                fa->type(FriendApplyType::AGREE);
                if (!_friendApplyMysql->updateApply(fa))
                {
                    ERROR("{}- 修改申请数据信息 {} 失败", rid, eid);
                    return err(rid, "修改申请数据信息失败");
                }
            }
            else
            {
                // 修改申请数据
                fa->type(FriendApplyType::REFUSE);
                if (!_friendApplyMysql->updateApply(fa))
                {
                    ERROR("{}- 修改申请数据信息 {} 失败", rid, eid);
                    return err(rid, "修改申请数据信息失败");
                }
            }

            response->set_requestid(rid);
            response->set_success(true);
            response->set_newchatsessionid(sid);
        }

        virtual void FriendSearch(::google::protobuf::RpcController *controller,
                                  const ::hjb::FriendSearchReq *request,
                                  ::hjb::FriendSearchResp *response,
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
            std::string key = request->key();

            // 获取好友id
            auto friendIds = _friendMysql->friends(uid);

            // 在es中搜索用户数据(过滤掉已存在的好友和自己)
            std::unordered_set<std::string> userIds;
            friendIds.push_back(uid);
            auto res = _es->search(key, friendIds);
            for (auto &it : res)
            {
                userIds.insert(it.userId());
            }

            // 从用户子服务中获取用户数据
            std::unordered_map<std::string, UserProto> users;
            if (!_getUser(rid, friendIds, users))
            {
                ERROR("{} - 批量获取用户信息失败", rid);
                return err(rid, "批量获取用户信息失败");
            }

            response->set_requestid(rid);
            response->set_success(true);
            for (const auto &user : users)
            {
                auto userinfo = response->add_users();
                userinfo->CopyFrom(user.second);
            }
        }

    private:
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

    class FriendServer
    {
    private:
        EtcdDisClient::ptr _disClient; // 服务发现操作对象
        EtcdRegClient::ptr _regClient; // 服务注册操作对象
        std::shared_ptr<elasticlient::Client> _es;
        std::shared_ptr<odb::core::database> _mysql; // mysql操作对象
        std::shared_ptr<brpc::Server> _brpcServer;   // rpc服务器

    public:
        using ptr = std::shared_ptr<FriendServer>;

        FriendServer(const EtcdDisClient::ptr &dis,
                     const EtcdRegClient::ptr &reg,
                     const std::shared_ptr<elasticlient::Client> &es,
                     const std::shared_ptr<odb::core::database> &mysql,
                     const std::shared_ptr<brpc::Server> &server)
            : _disClient(dis),
              _regClient(reg),
              _es(es),
              _mysql(mysql),
              _brpcServer(server)
        {
        }

        ~FriendServer()
        {
        }

        // 搭建RPC服务器并启动服务器
        void start()
        {
            // 阻塞等待服务器运行
            _brpcServer->RunUntilAskedToQuit();
        }
    };

    class FriendServerBuild
    {
    private:
        std::string _userServiceName;
        EtcdDisClient::ptr _disClient;               // 服务发现操作对象
        std::shared_ptr<brpc::Server> _brpcServer;   // rpc服务器
        EtcdRegClient::ptr _regClient;               // 服务注册操作对象
        std::shared_ptr<odb::core::database> _mysql; // mysql操作对象
        AllServiceChannel::ptr _channels;            // 服务信道操作对象
        std::shared_ptr<elasticlient::Client> _es;

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

        // 构造服务发现客户端和信道管理对象
        void makeEtcdDis(const std::string &regHost,
                         const std::string &baseServiceName,
                         const std::string &userServiceName)
        {
            _userServiceName = userServiceName;
            _channels = std::make_shared<hjb::AllServiceChannel>();
            _channels->declared(_userServiceName);

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

        // 构造RPC服务器对象
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

            if (!_channels)
            {
                ERROR("未初始化信道管理模块");
                abort();
            }

            // 添加rpc服务
            _brpcServer = std::make_shared<brpc::Server>();
            FriendServiceImpl *friendServiceImpl = new FriendServiceImpl(_channels, _mysql, _es, _userServiceName);
            if (_brpcServer->AddService(friendServiceImpl, brpc::ServiceOwnership::SERVER_OWNS_SERVICE) == -1)
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
        FriendServer::ptr build()
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

            return std::make_shared<FriendServer>(_disClient, _regClient, _es, _mysql, _brpcServer);
        }
    };
}