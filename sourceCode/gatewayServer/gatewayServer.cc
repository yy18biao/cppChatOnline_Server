#include "gatewayServer.hpp"

DEFINE_bool(runMode, false, "程序的运行模式，false-调试； true-发布；");
DEFINE_string(logFile, "", "发布模式下指定日志的输出文件");
DEFINE_int32(logLevel, 0, "发布模式下指定日志输出等级");

DEFINE_string(registryHost, "http://127.0.0.1:2379", "服务注册中心地址");
DEFINE_string(baseService, "/service", "服务监控根目录");
DEFINE_string(accessHost, "127.0.0.1:9000", "当前实例的外部访问地址");

DEFINE_int32(http_port, 8400, "HTTP服务器监听端口");
DEFINE_int32(websocket_port, 8500, "Websocket服务器监听端口");

DEFINE_string(fileService, "/service/fileService", "文件管理子服务名称");
DEFINE_string(friendService, "/service/friendService", "好友管理子服务名称");
DEFINE_string(messageService, "/service/messageService", "消息管理子服务名称");
DEFINE_string(userService, "/service/userService", "用户管理子服务名称");
DEFINE_string(speechService, "/service/speechService", "语音管理子服务名称");
DEFINE_string(chatSessionService, "/service/chatSessionService", "聊天会话管理子服务名称");

DEFINE_string(Rhost, "127.0.0.1", "服务器地址");
DEFINE_int32(Rport, 6379, "服务器端口");
DEFINE_int32(Rdb, 0, "库的编号");
DEFINE_bool(RkeepAlive, true, "是否启动长连接保活");

int main(int argc, char *argv[])
{
    google::ParseCommandLineFlags(&argc, &argv, true);
    hjb::initLogger(FLAGS_runMode, FLAGS_logFile, FLAGS_logLevel);

    hjb::GatewayServerBuilder gsb;
    gsb.makeRedis(FLAGS_Rhost, FLAGS_Rport, FLAGS_Rdb, FLAGS_RkeepAlive);
    gsb.makeEtcdDis(FLAGS_registryHost, FLAGS_baseService, FLAGS_fileService, FLAGS_friendService, FLAGS_messageService, FLAGS_userService, FLAGS_speechService, FLAGS_chatSessionService);
    gsb.makeServerObject(FLAGS_websocket_port, FLAGS_http_port);
    auto server = gsb.build();
    server->start();
}