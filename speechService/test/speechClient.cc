#include <gflags/gflags.h>
#include <thread>

#include "etcd.hpp"
#include "channel.hpp"
#include "speech.h"
#include "speech.pb.h"

DEFINE_bool(runMode, false, "程序的运行模式，false-调试； true-发布；");
DEFINE_string(logFile, "", "发布模式下指定日志的输出文件");
DEFINE_int32(logLevel, 0, "发布模式下指定日志输出等级");

DEFINE_string(serviceHost, "http://127.0.0.1:2379", "服务地址");
DEFINE_string(baseService, "/service", "服务所在根目录");
DEFINE_string(speechService, "/service/speechService", "服务所在目录");

int main(int argc, char *argv[])
{
    google::ParseCommandLineFlags(&argc, &argv, true);
    hjb::initLogger(FLAGS_runMode, FLAGS_logFile, FLAGS_logLevel);

    // 获取rpc信道管理对象
    auto allServiceChannel = std::make_shared<hjb::AllServiceChannel>();

    // 设置语音识别服务为需要关心的服务
    allServiceChannel->declared(FLAGS_speechService);

    // 设置服务上线和下线的回调方法
    auto putCb = std::bind(&hjb::AllServiceChannel::onServiceOnline, allServiceChannel.get(), std::placeholders::_1, std::placeholders::_2);
    auto delCb = std::bind(&hjb::AllServiceChannel::onServiceOffline, allServiceChannel.get(), std::placeholders::_1, std::placeholders::_2);

    // 创建etcd服务发现对象
    auto etcdDisClient = std::make_shared<hjb::EtcdDisClient>(FLAGS_serviceHost, FLAGS_baseService, putCb, delCb);

    // 通过Rpc信道管理对象获取提供语音识别服务的信道
    // 没有获取到则休眠一秒后退出
    auto channel = allServiceChannel->choose(FLAGS_speechService);
    if (!channel)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return -1;
    }

    // 读取语音文件数据
    std::string content;
    aip::get_file_content("16k.pcm", &content);
    std::cout << "文件大小：" << content.size() << std::endl;

    // 通过stub向rpc语音识别服务发起语音识别调用
    ::hjb::SpeechService_Stub stub(channel.get());

    // 创建并编辑请求对象
    ::hjb::SpeechReq req;
    req.set_content(content);
    req.set_requestid("111");

    // 创建控制和响应对象
    brpc::Controller *cntl = new brpc::Controller();
    ::hjb::SpeechRsp *resp = new hjb::SpeechRsp();

    // 发起调用请求
    stub.SpeechRecognition(cntl, &req, resp, nullptr);

    // 判断是否调用成功
    if (cntl->Failed() == true)
    {
        std::cout << "rpc调用失败：" << cntl->ErrorText() << std::endl;
        delete cntl;
        delete resp;
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return -1;
    }

    if (!resp->success())
    {
        std::cout << "响应出错：" << resp->errmsg() << std::endl;
        return -1;
    }

    std::cout << "收到响应: " << resp->requestid() << std::endl;
    std::cout << "响应内容为: " << resp->res() << std::endl;

    return 0;
}