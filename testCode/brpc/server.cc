#include <gflags/gflags.h>
#include <butil/logging.h>
#include <brpc/server.h>
#include <json2pb/pb_to_json.h>
#include "main.pb.h"

// 使用 gflags 定义一些命令行参数
DEFINE_int32(listen_port, 8000, "服务器地址信息");
DEFINE_int32(idle_timeout_s, -1, "空闲连接超时关闭时间：默认-1 表示不关闭");
DEFINE_int32(thread_count, 3, "服务器启动线程数量");

class EchoServiceImpl : public example::EchoService
{
public:
    EchoServiceImpl() {}
    virtual ~EchoServiceImpl() {}
    
    // 重写业务处理函数
    virtual void Echo(google::protobuf::RpcController *cntl_base,
                      const example::EchoRequest *request,
                      example::EchoResponse *response,
                      google::protobuf::Closure *done)
    {
        // 以 ARII 方式自动释放 done 对象
        brpc::ClosureGuard done_guard(done);

        brpc::Controller *cntl = static_cast<brpc::Controller *>(cntl_base);

        // 在发送响应后的回调函数
        cntl->set_after_rpc_resp_fn(std::bind(&EchoServiceImpl::CallAfterRpc,
                                              std::placeholders::_1, std::placeholders::_2,
                                              std::placeholders::_3));
    
        std::cout << "收到请求：" << request->message() << std::endl;

        // 设置响应
        response->set_message(request->message() + " Hello");
    }

    // 接收到请求的回调函数 此时响应已经发回给客户端 但是相关结构还没释放
    static void CallAfterRpc(brpc::Controller *cntl,
                             const google::protobuf::Message *req,
                             const google::protobuf::Message *res)
    {
        std::string req_str;
        std::string res_str;

        // 解析请求和响应
        json2pb::ProtoMessageToJson(*req, &req_str, NULL);
        json2pb::ProtoMessageToJson(*res, &res_str, NULL);

        std::cout << "req:" << req_str << std::endl;
        std::cout << "res:" << res_str << std::endl;
    }
};

int main(int argc, char *argv[])
{
    // 关闭brpc自带的日志
    logging::LoggingSettings log_setting;
    log_setting.logging_dest = logging::LoggingDestination::LOG_TO_NONE;
    logging::InitLogging(log_setting);

    // 解析命令行参数
    google::ParseCommandLineFlags(&argc, &argv, true);

    // 定义服务器
    brpc::Server server;
    // 创建服务对象
    EchoServiceImpl echo_service_impl;
    // 将服务添加到服务器中
    if (server.AddService(&echo_service_impl, brpc::SERVER_DOESNT_OWN_SERVICE) != 0)
    {
        std::cout << "添加服务失败!\n";
        return -1;
    }

    // 运行服务器
    brpc::ServerOptions options;
    options.idle_timeout_sec = FLAGS_idle_timeout_s; // 关闭超时关闭
    options.num_threads = FLAGS_thread_count; // 线程数量默认
    if (server.Start(FLAGS_listen_port, &options) != 0)
    {
        std::cout << "运行服务器失败";
        return -1;
    }

    // 阻塞等待服务端运行
    server.RunUntilAskedToQuit();

    return 0;
}