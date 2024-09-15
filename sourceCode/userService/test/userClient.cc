#include "etcd.hpp"
#include "channel.hpp"
#include "util.hpp"
#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include <thread>
#include "user.pb.h"
#include "base.pb.h"

DEFINE_bool(runMode, false, "程序的运行模式，false-调试； true-发布；");
DEFINE_string(logFile, "", "发布模式下指定日志的输出文件");
DEFINE_int32(logLevel, 0, "发布模式下指定日志输出等级");

DEFINE_string(serviceHost, "http://127.0.0.1:2379", "服务地址");
DEFINE_string(baseService, "/service", "服务所在根目录");
DEFINE_string(userService, "/service/userService", "服务所在目录");

hjb::AllServiceChannel::ptr _channels;

hjb::UserProto userInfo;

std::string sid;
std::string nickname = "黄竞标";

std::string codeId;
void getCode()
{
    auto channel = _channels->choose(FLAGS_userService); // 获取通信信道
    ASSERT_TRUE(channel);

    hjb::PhoneVerifyCodeReq req;
    req.set_requestid(hjb::uuid());
    req.set_phone(userInfo.phone());

    hjb::PhoneVerifyCodeResp resp;
    brpc::Controller cntl;
    hjb::UserService_Stub stub(channel.get());

    stub.GetPhoneVerifyCode(&cntl, &req, &resp, nullptr);
    ASSERT_FALSE(cntl.Failed());
    ASSERT_TRUE(resp.success());

    codeId = resp.verifycodeid();
}

// TEST(用户子服务测试, 用户注册测试)
// {
//     auto channel = _channels->choose(FLAGS_userService); // 获取通信信道
//     ASSERT_TRUE(channel);

//     getCode();

//     hjb::UserRegReq req;
//     req.set_requestid(hjb::uuid());
//     req.set_nickname(userInfo.nickname());
//     req.set_userid(userInfo.userid());
//     req.set_phone(userInfo.phone());
//     req.set_password("a123456");
//     std::string code;
//     std::cin >> code;
//     req.set_verifycode(code);
//     req.set_verifycodeid(codeId);
//     hjb::UserRegResp resp;
//     brpc::Controller cntl;
//     hjb::UserService_Stub stub(channel.get());
//     stub.UserRegister(&cntl, &req, &resp, nullptr);
//     ASSERT_FALSE(cntl.Failed());
//     ASSERT_TRUE(resp.success());
// }

// TEST(用户子服务测试, 用户登录测试) {
//     auto channel = _channels->choose(FLAGS_userService);//获取通信信道
//     ASSERT_TRUE(channel);

//     hjb::UserLoginReq req;
//     req.set_requestid(hjb::uuid());
//     req.set_userid(userInfo.userid());
//     req.set_password("a123456");
//     hjb::UserLoginResp resp;
//     brpc::Controller cntl;
//     hjb::UserService_Stub stub(channel.get());
//     stub.UserLogin(&cntl, &req, &resp, nullptr);
//     ASSERT_FALSE(cntl.Failed());
//     ASSERT_TRUE(resp.success());
//     sid = resp.loginsessionid();
//     std::cout << sid << std::endl;
// }

TEST(用户子服务测试, 用户头像设置测试) {
   auto channel = _channels->choose(FLAGS_userService);//获取通信信道
    ASSERT_TRUE(channel);

    hjb::SetUserPhotoReq req;
    req.set_requestid(hjb::uuid());
    req.set_userid(userInfo.userid());
    req.set_sessionid(sid);
    req.set_photo(userInfo.photo());
    hjb::SetUserPhotoResp resp;
    brpc::Controller cntl;
    hjb::UserService_Stub stub(channel.get());
    stub.SetUserPhoto(&cntl, &req, &resp, nullptr);
    ASSERT_FALSE(cntl.Failed());
    ASSERT_TRUE(resp.success());
}

TEST(用户子服务测试, 用户签名设置测试) {
    auto channel = _channels->choose(FLAGS_userService);//获取通信信道
    ASSERT_TRUE(channel);

    hjb::SetUserDescReq req;
   req.set_requestid(hjb::uuid());
    req.set_userid(userInfo.userid());
    req.set_sessionid(sid);
    req.set_desc(userInfo.desc());
    hjb::SetUserDescResp resp;
    brpc::Controller cntl;
    hjb::UserService_Stub stub(channel.get());
    stub.SetUserDescription(&cntl, &req, &resp, nullptr);
    ASSERT_FALSE(cntl.Failed());
    ASSERT_TRUE(resp.success());
}

TEST(用户子服务测试, 用户昵称设置测试) {
    auto channel = _channels->choose(FLAGS_userService);//获取通信信道
    ASSERT_TRUE(channel);

    hjb::SetNicknameReq req;
    req.set_requestid(hjb::uuid());
    req.set_userid(userInfo.userid());
    req.set_sessionid(sid);
    req.set_nickname(nickname);
    hjb::SetNicknameResp resp;
    brpc::Controller cntl;
    hjb::UserService_Stub stub(channel.get());
    stub.SetUserNickname(&cntl, &req, &resp, nullptr);
    ASSERT_FALSE(cntl.Failed());
    ASSERT_TRUE(resp.success());
}

TEST(用户子服务测试, 用户信息获取测试) {
   auto channel = _channels->choose(FLAGS_userService);//获取通信信道
    ASSERT_TRUE(channel);

    hjb::GetUserInfoReq req;
    req.set_requestid(hjb::uuid());
    req.set_userid(userInfo.userid());
    req.set_sessionid(sid);
    hjb::GetUserInfoResp resp;
    brpc::Controller cntl;
    hjb::UserService_Stub stub(channel.get());
    stub.GetUserInfo(&cntl, &req, &resp, nullptr);
    ASSERT_FALSE(cntl.Failed());
    ASSERT_TRUE(resp.success());
    ASSERT_EQ(userInfo.userid(), resp.user().userid());
    ASSERT_EQ(nickname, resp.user().nickname());
    ASSERT_EQ(userInfo.desc(), resp.user().desc());
    ASSERT_EQ("15889835718", resp.user().phone());
    ASSERT_EQ(userInfo.photo(), resp.user().photo());
}

int main(int argc, char *argv[])
{
    google::ParseCommandLineFlags(&argc, &argv, true);
    hjb::initLogger(FLAGS_runMode, FLAGS_logFile, FLAGS_logLevel);
    testing::InitGoogleTest(&argc, argv);

    // 构造Rpc信道管理对象
    _channels = std::make_shared<hjb::AllServiceChannel>();
    _channels->declared(FLAGS_userService);
    auto putCb = std::bind(&hjb::AllServiceChannel::onServiceOnline, _channels.get(), std::placeholders::_1, std::placeholders::_2);
    auto delCb = std::bind(&hjb::AllServiceChannel::onServiceOffline, _channels.get(), std::placeholders::_1, std::placeholders::_2);

    // 构造服务发现对象
    hjb::EtcdDisClient::ptr dclient = std::make_shared<hjb::EtcdDisClient>(FLAGS_serviceHost, FLAGS_baseService, putCb, delCb);

    userInfo.set_nickname("zhangsan");
    userInfo.set_userid("aa100111");
    userInfo.set_desc("hello");
    userInfo.set_phone("15889835718");
    userInfo.set_photo("touxiang");

    return RUN_ALL_TESTS();
}