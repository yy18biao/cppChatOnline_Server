#include <brpc/server.h>
#include <butil/logging.h>
#include <regex>

#include "log.hpp"
#include "etcd.hpp"
#include "mysql.hpp"
#include "redis.hpp"
#include "esData.hpp"
#include "util.hpp"
#include "dms.hpp"
#include "channel.hpp"

#include "user.pb.h"
#include "base.pb.h"
#include "file.pb.h"
#include "user-odb.hxx"
#include "user.hxx"

namespace hjb
{
    // 用户服务类
    class UserServiceImpl : public UserService
    {
    private:
        ESUser::ptr _es;                  // 用户es操作类对象
        UserTable::ptr _mysql;            // 用户数据库操作对象
        LoginSession::ptr _session;       // 用户redis登录会话操作对象
        LoginStatus::ptr _status;         // 用户redis登录状态操作对象
        VerifyCode::ptr _code;            // 用户redis验证码操作对象
        std::string _fileServiceName;     // 文件服务的名称
        AllServiceChannel::ptr _channels; // 用户服务信道操作对象
        DMSClient::ptr _dms;              // 短信验证码获取操作对象

    public:
        UserServiceImpl(const DMSClient::ptr &dms,
                        const std::shared_ptr<elasticlient::Client> &es,
                        const std::shared_ptr<odb::core::database> &mysql,
                        const std::shared_ptr<sw::redis::Redis> &redis,
                        const AllServiceChannel::ptr &channels,
                        const std::string &fileName)
            : _es(std::make_shared<ESUser>(es)), _mysql(std::make_shared<UserTable>(mysql)), _session(std::make_shared<LoginSession>(redis)), _status(std::make_shared<LoginStatus>(redis)), _code(std::make_shared<VerifyCode>(redis)), _fileServiceName(fileName), _channels(channels), _dms(dms)
        {
            // 创建es索引
            _es->createIndex();
        }

        ~UserServiceImpl() {}

        // 账号合理性判断
        bool checkUserId(const std::string &userId)
        {
            std::regex pattern("^(?![0-9]+$)(?![a-zA-Z]+$)[0-9A-Za-z]{6,18}$");
            if (!std::regex_match(userId, pattern))
            {
                ERROR("账号只能并需要包含一个字母一个数字，并且长度6-10位：{}", userId);
                return false;
            }

            return true;
        }

        // 昵称合理性判断
        bool checkNickname(const std::string &nickname)
        {
            if (nickname.size() > 32)
            {
                ERROR("昵称长度错误: {}", nickname);
                return false;
            }

            return true;
        }

        // 密码合理性判断
        bool checkPassword(const std::string &password)
        {
            std::regex pattern("^(?![0-9]+$)(?![a-zA-Z]+$)[0-9A-Za-z]{6,18}$");
            if (!std::regex_match(password, pattern))
            {
                ERROR("密码只能并需要包含一个字母一个数字，并且长度6-18位：{}", password);
                return false;
            }

            return true;
        }

        // 手机号码合理性判断
        bool checkPhone(const std::string &phone)
        {
            std::regex pattern(R"(\b(13|14|15|17|18)\d{9}\b)");
            if (!std::regex_match(phone, pattern))
            {
                ERROR("不是有效手机号码：{}", phone);
                return false;
            }
            return true;
        }

        // 手机验证码获取
        virtual void GetPhoneVerifyCode(::google::protobuf::RpcController *controller,
                                        const ::hjb::PhoneVerifyCodeReq *request,
                                        ::hjb::PhoneVerifyCodeResp *response,
                                        ::google::protobuf::Closure *done)
        {
            DEBUG("收到验证码获取请求");

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

            // 获取请求中的手机号码
            std::string phone = request->phone();

            // 判断手机号码合理性
            if (!checkPhone(phone))
                return err(request->requestid(), "手机号码设置错误");

            // 生成验证码id和验证码
            std::string codeId = hjb::uuid();
            std::string code = hjb::vcode();

            // 发送验证码
            if (!_dms->send(phone, code))
                return err(request->requestid(), "短信验证码发送失败");

            // 将验证码添加到redis中
            _code->append(codeId, phone, code);

            // 返回响应
            response->set_requestid(request->requestid());
            response->set_success(true);
            response->set_verifycodeid(codeId);
            DEBUG("获取短信验证码处理success");
        }

        // 用户注册
        virtual void UserRegister(::google::protobuf::RpcController *controller,
                                  const ::hjb::UserRegReq *request,
                                  ::hjb::UserRegResp *response,
                                  ::google::protobuf::Closure *done)
        {
            DEBUG("收到用户注册请求");

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

            // 取出用户请求里的账号、手机号、昵称和密码
            std::string userId = request->userid();
            std::string phone = request->phone();
            std::string nickname = request->nickname();
            std::string password = request->password();

            // 判断合理性
            if (!checkUserId(userId))
                return err(request->requestid(), "账号设置错误");
            if (!checkNickname(nickname))
                return err(request->requestid(), "昵称设置错误");
            if (!checkPhone(phone))
                return err(request->requestid(), "手机号码设置错误");
            if (!checkPassword(password))
                return err(request->requestid(), "密码设置错误");

            // 判断账号是否已经存在
            if (_mysql->selectByUserId(userId))
            {
                ERROR("该账号已经存在");
                return err(request->requestid(), "账号已存在");
            }

            // 判断手机号码是否已经存在
            if (_mysql->selectByPhone(phone))
            {
                ERROR("该手机号码已经存在");
                return err(request->requestid(), "手机号码已存在");
            }

            // 判断验证码
            auto vcode = _code->code(request->verifycodeid(), phone);
            if (vcode.value() != request->verifycode())
            {
                ERROR("{} - 验证码错误 - {}-{}", request->requestid(), phone, request->verifycode());
                return err(request->requestid(), "验证码错误");
            }

            // 移除刚验证的验证码
            _code->remove(request->verifycodeid());

            // 新增用户数据库数据
            auto user = std::make_shared<::UserInfo>(userId, nickname, password, phone);
            if (!_mysql->insert(user))
            {
                ERROR("新增用户数据库数据失败 -- {}", userId);
                return err(request->requestid(), "新增用户数据库数据失败");
            }

            // 新增es用户数据
            if (!_es->appendData(userId, nickname, phone, "", ""))
            {
                ERROR("新增用户es数据失败 -- {}", userId);
                return err(request->requestid(), "新增用户es数据失败");
            }

            response->set_requestid(request->requestid());
            response->set_success(true);
        }

        // 账号密码登录
        virtual void UserLogin(::google::protobuf::RpcController *controller,
                               const ::hjb::UserLoginReq *request,
                               ::hjb::UserLoginResp *response,
                               ::google::protobuf::Closure *done)
        {
            DEBUG("收到用户账号登录请求");

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

            // 取出请求中的账号和密码
            std::string userId = request->userid();
            std::string password = request->password();

            // 根据账号从mysql中取出用户信息
            auto user = _mysql->selectByUserId(userId);

            // 判断
            if (!user || password != user->password())
            {
                ERROR("{} - 用户名或密码错误 - {}-{}！", request->requestid(), userId, password);
                return err(request->requestid(), "用户名或密码错误");
            }

            // 判断该用户是否已经登录
            if (_status->exists(userId))
            {
                ERROR("{} - 用户已在其他地方登录 - {}", request->requestid(), userId);
                return err(request->requestid(), "用户已在其他地方登录");
            }

            // 构建登录会话id
            std::string sessionId = hjb::uuid();
            // 添加登录会话信息
            _session->append(sessionId, userId);
            // 添加用户登录状态
            _status->append(userId);

            // 响应
            response->set_requestid(request->requestid());
            response->set_loginsessionid(sessionId);
            response->set_success(true);
        }

        // 手机验证码登录
        virtual void PhoneLogin(::google::protobuf::RpcController *controller,
                                const ::hjb::PhoneLoginReq *request,
                                ::hjb::PhoneLoginResp *response,
                                ::google::protobuf::Closure *done)
        {
            DEBUG("收到用户手机登录请求");

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

            // 取出请求中的手机、验证码、验证码id
            std::string phone = request->phone();
            std::string code = request->verifycode();
            std::string codeId = request->verifycodeid();

            // 判断手机合理性
            if (!checkPhone(phone))
                return err(request->requestid(), "手机号码错误");

            // 判断手机是否已存在
            auto user = _es->searchPhone(phone);
            if (!user)
            {
                ERROR("该手机号码不存在");
                return err(request->requestid(), "该手机号码未注册");
            }

            // 判断验证码
            auto vcode = _code->code(request->verifycodeid(), phone);
            if (vcode != request->verifycode())
            {
                ERROR("{} - 验证码错误 - {}-{}", request->requestid(), phone, request->verifycode());
                return err(request->requestid(), "验证码错误");
            }

            // 移除刚验证的验证码
            _code->remove(request->verifycodeid());

            // 判断该用户是否已经登录
            if (_status->exists(user->userId()))
            {
                ERROR("{} - 用户已在其他地方登录 - {}", request->requestid(), user->userId());
                return err(request->requestid(), "用户已在其他地方登录");
            }

            // 构建登录会话id
            std::string sessionId = hjb::uuid();
            // 添加登录会话信息
            _session->append(sessionId, user->userId());
            // 添加用户登录状态
            _status->append(user->userId());

            // 响应
            response->set_requestid(request->requestid());
            response->set_loginsessionid(sessionId);
            response->set_success(true);
        }

        // 获取单个用户信息
        virtual void GetUserInfo(::google::protobuf::RpcController *controller,
                                 const ::hjb::GetUserInfoReq *request,
                                 ::hjb::GetUserInfoResp *response,
                                 ::google::protobuf::Closure *done)
        {
            DEBUG("收到获取用户信息请求");

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

            auto userId = request->userid();

            // 获取用户数据
            auto user = _mysql->selectByUserId(userId);
            if (!user)
            {
                ERROR("该用户不存在");
                return err(request->requestid(), "该用户不存在");
            }

            // 组织响应里的用户数据
            auto respUser = response->mutable_user();
            respUser->set_userid(userId);
            respUser->set_nickname(user->nickname());
            respUser->set_phone(user->phone());
            if (user->desc())
                respUser->set_desc(*user->desc());

            // 根据用户的文件id获取文件数据
            auto photoId = user->userPhotoId();
            if (photoId)
            {
                // 创建信道连接到文件服务
                auto channel = _channels->choose(_fileServiceName);
                if (!channel)
                {
                    ERROR("{} - 未找到文件管理子服务节点 - {} - {}", request->requestid(), _fileServiceName, userId);
                    return err(request->requestid(), "未找到文件管理子服务节点");
                }

                // 进行rpc请求下载文件数据
                hjb::FileService_Stub stub(channel.get());
                hjb::GetSingleFileReq req;
                hjb::GetSingleFileResp resp;
                req.set_requestid(request->requestid());
                req.set_fileid(*photoId);
                brpc::Controller cntl;
                stub.GetSingleFile(&cntl, &req, &resp, nullptr);
                if (cntl.Failed() || !resp.success())
                {
                    ERROR("{} - 文件子服务调用失败：{}", request->requestid(), cntl.ErrorText());
                    return err(request->requestid(), "文件子服务调用失败");
                }

                respUser->set_photo(resp.filedata().filecontent());
            }

            response->set_requestid(request->requestid());
            response->set_success(true);
        }

        // 获取多个用户信息
        virtual void GetMultiUserInfo(::google::protobuf::RpcController *controller,
                                      const ::hjb::GetMultiUserInfoReq *request,
                                      ::hjb::GetMultiUserInfoResp *response,
                                      ::google::protobuf::Closure *done)
        {
            DEBUG("收到获取多个用户信息请求");

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

            std::vector<std::string> userIds;
            for (int i = 0; i < request->userids_size(); i++)
                userIds.push_back(request->userids(i));

            // 获取用户数据
            auto users = _mysql->selectUsersByUserId(userIds);
            if (users.size() != request->userids_size())
            {
                ERROR("{} - 从数据库查找的用户信息数量不一致 {}-{}", request->requestid(), request->userids_size(), users.size());
                return err(request->requestid(), "从数据库查找的用户信息数量不一致");
            }

            // 创建信道连接到文件服务
            auto channel = _channels->choose(_fileServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到文件管理子服务节点 - {}", request->requestid(), _fileServiceName);
                return err(request->requestid(), "未找到文件管理子服务节点");
            }

            // 进行rpc请求下载文件数据
            hjb::FileService_Stub stub(channel.get());
            hjb::GetMultiFileReq req;
            hjb::GetMultiFileResp resp;
            req.set_requestid(request->requestid());
            brpc::Controller cntl;
            for (auto &user : users)
            {
                if (user.userPhotoId()->empty())
                    continue;

                req.add_fileidlist(*user.userPhotoId());
            }
            stub.GetMultiFile(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() || !resp.success())
            {
                ERROR("{} - 文件子服务调用失败：{}", request->requestid(), cntl.ErrorText());
                return err(request->requestid(), "文件子服务调用失败");
            }

            for (auto &user : users)
            {
                // 本次请求要响应的用户信息map
                auto userMap = response->mutable_users();
                // 批量文件请求响应中的map
                auto fileMap = resp.mutable_filedata();

                UserProto userInfo;
                userInfo.set_userid(user.userId());
                userInfo.set_nickname(user.nickname());
                if (user.desc())
                    userInfo.set_desc(*user.desc());
                userInfo.set_phone(user.phone());

                userInfo.set_photo((*fileMap)[*user.userPhotoId()].filecontent());

                (*userMap)[userInfo.userid()] = userInfo;
            }
            response->set_requestid(request->requestid());
            response->set_success(true);
        }

        // 设置用户头像
        virtual void SetUserPhoto(::google::protobuf::RpcController *controller,
                                  const ::hjb::SetUserPhotoReq *request,
                                  ::hjb::SetUserPhotoResp *response,
                                  ::google::protobuf::Closure *done)
        {
            DEBUG("收到设置用户头像请求");

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

            // 从请求中取出用户id
            std::string uid = request->userid();

            // 获取用户数据
            auto user = _mysql->selectByUserId(uid);
            if (!user)
            {
                ERROR("该用户不存在");
                return err(request->requestid(), "该用户不存在");
            }

            // 上传头像文件到文件子服务
            auto channel = _channels->choose(_fileServiceName);
            if (!channel)
            {
                ERROR("{} - 未找到文件管理子服务节点 - {}", request->requestid(), _fileServiceName);
                return err(request->requestid(), "未找到文件管理子服务节点");
            }

            hjb::FileService_Stub stub(channel.get());
            hjb::PutSingleFileReq req;
            hjb::PutSingleFileResp resp;
            req.set_requestid(request->requestid());
            req.mutable_filedata()->set_filename("");
            req.mutable_filedata()->set_filesize(request->photo().size());
            req.mutable_filedata()->set_filecontent(request->photo());
            brpc::Controller cntl;
            stub.PutSingleFile(&cntl, &req, &resp, nullptr);
            if (cntl.Failed() == true || resp.success() == false)
            {
                ERROR("{} - 文件子服务调用失败：{}", request->requestid(), cntl.ErrorText());
                return err(request->requestid(), "文件子服务调用失败");
            }
            std::string photoId = resp.fileinfo().fileid();

            // 更新数据库
            user->userPhotoId(photoId);
            if (!_mysql->update(user))
            {
                ERROR("{} - 更新数据库用户头像失败 ：{}", request->requestid(), photoId);
                return err(request->requestid(), "更新数据库用户头像失败");
            }

            // 更新es服务器
            if (!_es->appendData(user->userId(), user->phone(),
                                 user->nickname(), *user->desc(), *user->userPhotoId()))
            {
                ERROR("{} - 更新es用户头像失败 ：{}", request->requestid(), photoId);
                return err(request->requestid(), "更新es用户头像失败");
            }

            response->set_requestid(request->requestid());
            response->set_success(true);
        }

        // 设置用户昵称
        virtual void SetUserNickname(::google::protobuf::RpcController *controller,
                                     const ::hjb::SetNicknameReq *request,
                                     ::hjb::SetNicknameResp *response,
                                     ::google::protobuf::Closure *done)
        {
            DEBUG("收到设置用户昵称请求");

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

            // 从请求中取出用户id
            std::string uid = request->userid();

            // 获取用户数据
            auto user = _mysql->selectByUserId(uid);
            if (!user)
            {
                ERROR("该用户不存在");
                return err(request->requestid(), "该用户不存在");
            }

            if (!checkNickname(request->nickname()))
                return err(request->requestid(), "昵称设置错误");

            user->nickname(request->nickname());

            // 更新数据库
            if (!_mysql->update(user))
            {
                ERROR("{} - 更新数据库用户昵称失败 ：{}", request->requestid(), request->nickname());
                return err(request->requestid(), "更新数据库用户昵称失败");
            }

            // 更新es服务器
            if (!_es->appendData(user->userId(), user->phone(),
                                 user->nickname(), *user->desc(), *user->userPhotoId()))
            {
                ERROR("{} - 更新es用户昵称失败 ：{}", request->requestid(), request->nickname());
                return err(request->requestid(), "更新es用户昵称失败");
            }

            response->set_requestid(request->requestid());
            response->set_success(true);
        }

        // 设置用户签名
        virtual void SetUserDescription(::google::protobuf::RpcController *controller,
                                        const ::hjb::SetUserDescReq *request,
                                        ::hjb::SetUserDescResp *response,
                                        ::google::protobuf::Closure *done)
        {
            DEBUG("收到设置用户签名请求");

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

            // 从请求中取出用户id
            std::string uid = request->userid();

            // 获取用户数据
            auto user = _mysql->selectByUserId(uid);
            if (!user)
            {
                ERROR("该用户不存在");
                return err(request->requestid(), "该用户不存在");
            }

            user->desc(request->desc());

            // 更新数据库
            if (!_mysql->update(user))
            {
                ERROR("{} - 更新数据库用户签名失败 ：{}", request->requestid(), request->desc());
                return err(request->requestid(), "更新数据库用户签名失败");
            }

            // 更新es服务器
            if (!_es->appendData(user->userId(), user->phone(),
                                 user->nickname(), *user->desc(), *user->userPhotoId()))
            {
                ERROR("{} - 更新es用户签名失败 ：{}", request->requestid(), request->desc());
                return err(request->requestid(), "更新es用户签名失败");
            }

            response->set_requestid(request->requestid());
            response->set_success(true);
        }

        // 设置用户手机号码
        virtual void SetUserPhoneNumber(::google::protobuf::RpcController *controller,
                                        const ::hjb::SetPhoneReq *request,
                                        ::hjb::SetPhoneResp *response,
                                        ::google::protobuf::Closure *done)
        {
            DEBUG("收到设置用户手机号码请求");

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

            // 从请求中取出用户id
            std::string uid = request->userid();

            // 获取用户数据
            auto user = _mysql->selectByUserId(uid);
            if (!user)
            {
                ERROR("该用户不存在");
                return err(request->requestid(), "该用户不存在");
            }

            if (!checkPhone(request->phone()))
                return err(request->requestid(), "手机设置错误");

            // 判断验证码
            auto vcode = _code->code(request->phoneverifycodeid(), request->phone());
            if (vcode != request->phoneverifycode())
            {
                ERROR("{} - 验证码错误 - {}-{}", request->requestid(), request->phone(), request->phoneverifycodeid());
                return err(request->requestid(), "验证码错误");
            }

            // 移除刚验证的验证码
            _code->remove(request->phoneverifycodeid());

            user->phone(request->phone());

            // 更新数据库
            if (!_mysql->update(user))
            {
                ERROR("{} - 更新数据库用户手机失败 ：{}", request->requestid(), request->phone());
                return err(request->requestid(), "更新数据库用户手机失败");
            }

            // 更新es服务器
            if (!_es->appendData(user->userId(), user->phone(),
                                 user->nickname(), *user->desc(), *user->userPhotoId()))
            {
                ERROR("{} - 更新es用户手机失败 ：{}", request->requestid(), request->phone());
                return err(request->requestid(), "更新es用户手机失败");
            }

            response->set_requestid(request->requestid());
            response->set_success(true);
        }
    };

    class UserServer
    {
    private:
        hjb::EtcdDisClient::ptr _disClient;
        hjb::EtcdRegClient::ptr _regClient;
        std::shared_ptr<elasticlient::Client> _es;
        std::shared_ptr<odb::core::database> _mysql;
        std::shared_ptr<sw::redis::Redis> _redis;
        std::shared_ptr<brpc::Server> _brpcServer;

    public:
        using ptr = std::shared_ptr<UserServer>;

        UserServer(const hjb::EtcdDisClient::ptr &dis,
                   const hjb::EtcdRegClient::ptr &reg,
                   const std::shared_ptr<elasticlient::Client> &es,
                   const std::shared_ptr<odb::core::database> &mysql,
                   std::shared_ptr<sw::redis::Redis> &redis,
                   const std::shared_ptr<brpc::Server> &server)
            : _disClient(dis),
              _regClient(reg),
              _es(es),
              _mysql(mysql),
              _redis(redis),
              _brpcServer(server)
        {
        }

        ~UserServer() {}

        // 搭建RPC服务器，并启动服务器
        void start()
        {
            _brpcServer->RunUntilAskedToQuit();
        }
    };

    class UserServerBuilder
    {
    private:
        hjb::EtcdDisClient::ptr _disClient;
        hjb::EtcdRegClient::ptr _regClient;
        std::shared_ptr<elasticlient::Client> _es;
        std::shared_ptr<odb::core::database> _mysql;
        std::shared_ptr<sw::redis::Redis> _redis;
        std::shared_ptr<brpc::Server> _brpcServer;
        std::shared_ptr<DMSClient> _dms;
        std::string _fileServiceName;
        hjb::AllServiceChannel::ptr _channels;

    public:
        // 构造es客户端对象
        void makeEs(const std::vector<std::string> hosts)
        {
            _es = ESFactory::create(hosts);
        }

        // 构造验证码客户端对象
        void makeDms(const std::string &access_key_id, const std::string &access_key_secret)
        {
            _dms = std::make_shared<DMSClient>(access_key_id, access_key_secret);
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
                         const std::string &fileServiceName)
        {
            _fileServiceName = fileServiceName;
            _channels = std::make_shared<hjb::AllServiceChannel>();
            _channels->declared(fileServiceName);

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
            if (!_redis)
            {
                ERROR("未初始化Redis数据库模块");
                abort();
            }
            if (!_channels)
            {
                ERROR("未初始化信道管理模块");
                abort();
            }
            if (!_dms)
            {
                ERROR("未初始化短信平台模块");
                abort();
            }

            _brpcServer = std::make_shared<brpc::Server>();

            UserServiceImpl *service = new UserServiceImpl(_dms, _es, _mysql, _redis, _channels, _fileServiceName);
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
        }

        // 构造RPC服务器对象
        UserServer::ptr build()
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

            UserServer::ptr server = std::make_shared<UserServer>(_disClient, _regClient, _es, _mysql, _redis, _brpcServer);
            return server;
        }
    };
}