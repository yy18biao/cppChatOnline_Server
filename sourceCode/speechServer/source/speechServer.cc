#include "speechServer.hpp"

DEFINE_bool(runMode, false, "程序的运行模式，false-调试； true-发布；");
DEFINE_string(logFile, "", "发布模式下指定日志的输出文件");
DEFINE_int32(logLevel, 0, "发布模式下指定日志输出等级");

DEFINE_string(registryHost, "http://127.0.0.1:2379", "服务注册中心地址");
DEFINE_string(baseService, "/service", "服务监控根目录");
DEFINE_string(instanceName, "/speechService/instance", "当前实例名称");
DEFINE_string(accessHost, "127.0.0.1:7000", "当前实例的外部访问地址");

DEFINE_int32(listenPort, 7000, "Rpc服务器监听端口");
DEFINE_int32(rpcTimeout, -1, "Rpc调用超时时间");
DEFINE_int32(rpcThreads, 1, "Rpc的IO线程数量");

DEFINE_string(S_appId, "115537914", "语音平台应用ID");
DEFINE_string(S_apiKey, "QvYPb4iYSq52A688Nh0BaAqM", "语音平台API密钥");
DEFINE_string(S_secretKey, "HWyPo7QGc26dIofx5gDVMI7NClzaP3lT", "语音平台加密密钥");

int main(int argc, char *argv[])
{
    google::ParseCommandLineFlags(&argc, &argv, true);
    hjb::initLogger(FLAGS_runMode, FLAGS_logFile, FLAGS_logLevel);

    // 建造语音识别服务器的各个客户端
    hjb::SpeechServerBuild ssb;
    ssb.makeSpeechClient(FLAGS_S_appId, FLAGS_S_apiKey, FLAGS_S_secretKey);
    ssb.makeRegClient(FLAGS_registryHost, FLAGS_baseService + FLAGS_instanceName, FLAGS_accessHost);
    ssb.makeRpcServer(FLAGS_listenPort, FLAGS_rpcTimeout, FLAGS_rpcThreads);

    // 建造语音识别服务器
    auto server = ssb.build();
    // 启动语音识别服务器
    server->start();
}