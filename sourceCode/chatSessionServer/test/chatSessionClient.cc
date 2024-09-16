#include "etcd.hpp"
#include "channel.hpp"
#include "util.hpp"
#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include <thread>
#include "chatSession.pb.h"

DEFINE_bool(runMode, false, "程序的运行模式，false-调试； true-发布；");
DEFINE_string(logFile, "", "发布模式下指定日志的输出文件");
DEFINE_int32(logLevel, 0, "发布模式下指定日志输出等级");

DEFINE_string(serviceHost, "http://127.0.0.1:2379", "服务地址");
DEFINE_string(baseService, "/service", "服务所在根目录");
DEFINE_string(chatSessionService, "/service/chatSessionService", "服务所在目录");

hjb::AllServiceChannel::ptr _channels;

void stringMessage(const std::string &uid, const std::string &sid, const std::string &msg)
{
    auto channel = _channels->choose(FLAGS_chatSessionService);
    if (!channel)
    {
        std::cout << "获取通信信道失败！" << std::endl;
        return;
    }

    hjb::MessageTransmitService_Stub stub(channel.get());
    hjb::NewMessageReq req;
    hjb::GetTransmitTargetResp resp;
    req.set_requestid(hjb::uuid());
    req.set_userid(uid);
    req.set_chatsessionid(sid);
    req.mutable_message()->set_messagetype(hjb::MessageType::STRING);
    req.mutable_message()->mutable_stringmessage()->set_content(msg);
    brpc::Controller cntl;
    stub.GetTransmitTarget(&cntl, &req, &resp, nullptr);
}

int main(int argc, char *argv[])
{
    google::ParseCommandLineFlags(&argc, &argv, true);
    hjb::initLogger(FLAGS_runMode, FLAGS_logFile, FLAGS_logLevel);

    _channels = std::make_shared<hjb::AllServiceChannel>();
    _channels->declared(FLAGS_chatSessionService);
    auto putCb = std::bind(&hjb::AllServiceChannel::onServiceOnline, _channels.get(), std::placeholders::_1, std::placeholders::_2);
    auto delCb = std::bind(&hjb::AllServiceChannel::onServiceOffline, _channels.get(), std::placeholders::_1, std::placeholders::_2);

    hjb::EtcdDisClient::ptr dclient = std::make_shared<hjb::EtcdDisClient>(FLAGS_serviceHost, FLAGS_baseService, putCb, delCb);

    stringMessage("aa100111", "111", "吃饭了吗？");

    return 0;
}