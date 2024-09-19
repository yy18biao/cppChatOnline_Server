#include "fileServer.hpp"

DEFINE_bool(runMode, false, "程序的运行模式，false-调试； true-发布；");
DEFINE_string(logFile, "", "发布模式下指定日志的输出文件");
DEFINE_int32(logLevel, 0, "发布模式下指定日志输出等级");

DEFINE_string(registryHost, "http://127.0.0.1:2379", "服务注册中心地址");
DEFINE_string(baseService, "/service", "服务监控根目录");
DEFINE_string(instanceName, "/fileService/instance", "当前实例名称");
DEFINE_string(accessHost, "127.0.0.1:7500", "当前实例的外部访问地址");

DEFINE_int32(listenPort, 7500, "Rpc服务器监听端口");
DEFINE_int32(rpcTimeout, -1, "Rpc调用超时时间");
DEFINE_int32(rpcThreads, 1, "Rpc的IO线程数量");

int main(int argc, char *argv[])
{
    google::ParseCommandLineFlags(&argc, &argv, true);
    hjb::initLogger(FLAGS_runMode, FLAGS_logFile, FLAGS_logLevel);

    // 建造语音识别服务器的各个客户端
    hjb::FileServerBuild fsb;
    fsb.makeRegClient(FLAGS_registryHost, FLAGS_baseService + FLAGS_instanceName, FLAGS_accessHost);
    fsb.makeRpcServer(FLAGS_listenPort, FLAGS_rpcTimeout, FLAGS_rpcThreads);

    // 建造语音识别服务器
    auto server = fsb.build();
    // 启动语音识别服务器
    server->start();
}