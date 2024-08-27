#include <gflags/gflags.h>
#include <butil/logging.h>
#include <butil/time.h>
#include <brpc/channel.h>
#include "main.pb.h"

DEFINE_string(protocol, "baidu_std", "通信协议类型");
DEFINE_string(server_host, "127.0.0.1:8000", "服务器地址信息");
DEFINE_int32(timeout_ms, 500, "请求超时时间-毫秒");
DEFINE_int32(max_retry, 3, "请求重试次数"); 

int main(int argc, char* argv[]) {
    // 解析命令行参数
    google::ParseCommandLineFlags(&argc, &argv, true);

    // 创建通道， 可以理解为客户端到服务器的一条通信线路
    brpc::Channel channel;

    // 初始化通道
    brpc::ChannelOptions options;
    options.protocol = FLAGS_protocol;
    options.timeout_ms = FLAGS_timeout_ms;
    options.max_retry = FLAGS_max_retry;
    if (channel.Init(FLAGS_server_host.c_str(), &options) != 0)
    {
        LOG(ERROR) << "初始化通道失败";
        return -1;
    }

    // 通过 stub 进行rpc 调用
    example::EchoService_Stub stub(&channel);

    // 创建请求、响应、控制对象
    example::EchoRequest request;
    example::EchoResponse response;
    brpc::Controller cntl;

    // 构造请求响应
    request.set_message("hello world");

    // NULL表示阻塞等待响应
    stub.Echo(&cntl, &request, &response, NULL);
    if (cntl.Failed())
    {
        std::cout << "请求失败: " << cntl.ErrorText() << std::endl;
        return -1;
    }

    std::cout << "响应：" << response.message() << std::endl;
    
    return 0;
}