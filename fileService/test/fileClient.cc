#include <gflags/gflags.h>
#include <gtest/gtest.h>
#include <thread>
#include "etcd.hpp"
#include "channel.hpp"
#include "log.hpp"
#include "file.pb.h"
#include "base.pb.h"
#include "util.hpp"

DEFINE_bool(runMode, false, "程序的运行模式，false-调试； true-发布；");
DEFINE_string(logFile, "", "发布模式下指定日志的输出文件");
DEFINE_int32(logLevel, 0, "发布模式下指定日志输出等级");

DEFINE_string(serviceHost, "http://127.0.0.1:2379", "服务地址");
DEFINE_string(baseService, "/service", "服务所在根目录");
DEFINE_string(fileService, "/service/fileService", "服务所在目录");

hjb::ServiceChannel::ChannelPtr channel;
std::string SFileId;

TEST(putTest, singleFile)
{
    std::string body;
    ASSERT_TRUE(hjb::readFile("./Makefile", body));

    // 通过stub向rpc语音识别服务发起语音识别调用
    ::hjb::FileService_Stub stub(channel.get());

    // 创建并编辑请求对象
    ::hjb::PutSingleFileReq req;
    req.mutable_filedata()->set_filename("Makefile");
    req.mutable_filedata()->set_filesize(body.size());
    req.mutable_filedata()->set_filecontent(body);
    req.set_requestid("111");

    // 创建控制和响应对象
    brpc::Controller *cntl = new brpc::Controller();
    ::hjb::PutSingleFileResp *resp = new hjb::PutSingleFileResp();

    // 发起调用请求
    stub.PutSingleFile(cntl, &req, resp, nullptr);
    ASSERT_FALSE(cntl->Failed());

    ASSERT_TRUE(resp->success());
    ASSERT_EQ(resp->fileinfo().filesize(), body.size());
    ASSERT_EQ(resp->fileinfo().filename(), "Makefile");
    SFileId = resp->fileinfo().fileid();
    DEBUG("文件ID：{}", SFileId);
}

TEST(getTest, singleFile)
{
    // 通过stub向rpc语音识别服务发起语音识别调用
    ::hjb::FileService_Stub stub(channel.get());

    // 创建并编辑请求对象
    ::hjb::GetSingleFileReq req;
    req.set_requestid("222");
    req.set_fileid(SFileId);

    // 创建控制和响应对象
    brpc::Controller *cntl = new brpc::Controller();
    ::hjb::GetSingleFileResp *resp = new hjb::GetSingleFileResp();

    // 发起调用请求
    stub.GetSingleFile(cntl, &req, resp, nullptr);
    ASSERT_FALSE(cntl->Failed());

    ASSERT_EQ(resp->filedata().fileid(), SFileId);
    hjb::writeFile("stest.txt", resp->filedata().filecontent());
}

std::vector<std::string> putTestId;
TEST(putTest, multiFile)
{
    std::string body1;
    ASSERT_TRUE(hjb::readFile("./base.pb.h", body1));
    std::string body2;
    ASSERT_TRUE(hjb::readFile("./file.pb.h", body2));

    // 通过stub向rpc语音识别服务发起语音识别调用
    ::hjb::FileService_Stub stub(channel.get());

    // 创建并编辑请求对象
    ::hjb::PutMultiFileReq req;
    auto filedata = req.add_filedata();
    filedata->set_filename("base.pb.h");
    filedata->set_filesize(body1.size());
    filedata->set_filecontent(body1);
    filedata = req.add_filedata(); // 再次获取
    filedata->set_filename("file.pb.h");
    filedata->set_filesize(body2.size());
    filedata->set_filecontent(body2);
    req.set_requestid("333");

    // 创建控制和响应对象
    brpc::Controller *cntl = new brpc::Controller();
    ::hjb::PutMultiFileResp *resp = new hjb::PutMultiFileResp();

    // 发起调用请求
    stub.PutMultiFile(cntl, &req, resp, nullptr);
    ASSERT_FALSE(cntl->Failed());

    ASSERT_TRUE(resp->success());
    for (int i = 0; i < resp->fileinfo_size(); i++)
    {
        putTestId.push_back(resp->fileinfo(i).fileid());
        DEBUG("文件ID：{}", putTestId[i]);
    }
}

TEST(getTest, multiFile)
{
    // 通过stub向rpc语音识别服务发起语音识别调用
    ::hjb::FileService_Stub stub(channel.get());

    // 创建并编辑请求对象
    ::hjb::GetMultiFileReq req;
    req.set_requestid("444");
    req.add_fileidlist(putTestId[0]);
    req.add_fileidlist(putTestId[1]);

    // 创建控制和响应对象
    brpc::Controller *cntl = new brpc::Controller();
    ::hjb::GetMultiFileResp *resp = new hjb::GetMultiFileResp();

    // 发起调用请求
    stub.GetMultiFile(cntl, &req, resp, nullptr);
    ASSERT_FALSE(cntl->Failed());
    ASSERT_TRUE(resp->success());

    ASSERT_TRUE(resp->filedata().find(putTestId[0]) != resp->filedata().end());
    ASSERT_TRUE(resp->filedata().find(putTestId[1]) != resp->filedata().end());

    auto map = resp->filedata();
    auto filedata1 = map[putTestId[0]];
    hjb::writeFile("base_download", filedata1.filecontent());
    auto filedata2 = map[putTestId[1]];
    hjb::writeFile("file_download", filedata2.filecontent());
}

int main(int argc, char *argv[])
{
    google::ParseCommandLineFlags(&argc, &argv, true);
    hjb::initLogger(FLAGS_runMode, FLAGS_logFile, FLAGS_logLevel);
    testing::InitGoogleTest(&argc, argv);

    // 获取rpc信道管理对象
    auto allServiceChannel = std::make_shared<hjb::AllServiceChannel>();

    // 设置语音识别服务为需要关心的服务
    allServiceChannel->declared(FLAGS_fileService);

    // 设置服务上线和下线的回调方法
    auto putCb = std::bind(&hjb::AllServiceChannel::onServiceOnline, allServiceChannel.get(), std::placeholders::_1, std::placeholders::_2);
    auto delCb = std::bind(&hjb::AllServiceChannel::onServiceOffline, allServiceChannel.get(), std::placeholders::_1, std::placeholders::_2);

    // 创建etcd服务发现对象
    auto etcdDisClient = std::make_shared<hjb::EtcdDisClient>(FLAGS_serviceHost, FLAGS_baseService, putCb, delCb);

    // 通过Rpc信道管理对象获取提供语音识别服务的信道
    // 没有获取到则休眠一秒后退出
    channel = allServiceChannel->choose(FLAGS_fileService);
    if (!channel)
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        return -1;
    }

    return RUN_ALL_TESTS();
}