#include "chatSessionServer.hpp"

DEFINE_bool(runMode, false, "程序的运行模式，false-调试； true-发布；");
DEFINE_string(logFile, "", "发布模式下指定日志的输出文件");
DEFINE_int32(logLevel, 0, "发布模式下指定日志输出等级");

DEFINE_string(registryHost, "http://127.0.0.1:2379", "服务注册中心地址");
DEFINE_string(baseService, "/service", "服务监控根目录");
DEFINE_string(instanceName, "/chatSessionService/instance", "当前实例名称");
DEFINE_string(accessHost, "127.0.0.1:8002", "当前实例的外部访问地址");

DEFINE_string(userService, "/service/userService", "用户管理子服务名称");
DEFINE_string(messageService, "/service/messageService", "用户管理子服务名称");

DEFINE_string(Mhost, "127.0.0.1", "mysql服务器地址");
DEFINE_int32(Mport, 3306, "mysql服务器端口");
DEFINE_string(Mdb, "chatOnline", "mysql默认库");
DEFINE_string(Muser, "root", "mysql用户名");
DEFINE_string(Mpwd, "111111", "mysql密码");
DEFINE_string(Mcharset, "utf8", "mysql客户端字符集");
DEFINE_int32(MmaxPool, 3, "mysql连接池最大连接数");

DEFINE_string(Ehost, "http://127.0.0.1:9200/", "es服务器URL");

DEFINE_int32(listenPort, 8002, "Rpc服务器监听端口");
DEFINE_int32(rpcTimeout, -1, "Rpc调用超时时间");
DEFINE_int32(rpcThreads, 1, "Rpc的IO线程数量");

DEFINE_string(mq_user, "root", "消息队列服务器访问用户名");
DEFINE_string(mq_pwd, "123456", "消息队列服务器访问密码");
DEFINE_string(mq_host, "127.0.0.1:5672", "消息队列服务器访问地址");
DEFINE_string(mq_exchange, "exchange", "持久化消息的发布交换机名称");
DEFINE_string(mq_queue, "message", "持久化消息的发布队列名称");
DEFINE_string(mq_binding_key, "message", "持久化消息的规则");

int main(int argc, char *argv[])
{
    google::ParseCommandLineFlags(&argc, &argv, true);
    hjb::initLogger(FLAGS_runMode, FLAGS_logFile, FLAGS_logLevel);

    hjb::ChatSessionServerBuild cssb;
    cssb.makeMqClient(FLAGS_mq_user, FLAGS_mq_pwd, FLAGS_mq_host, FLAGS_mq_exchange, FLAGS_mq_queue, FLAGS_mq_binding_key);

    cssb.makeMysql(FLAGS_Muser, FLAGS_Mpwd, FLAGS_Mhost,
                   FLAGS_Mdb, FLAGS_Mcharset, FLAGS_Mport, FLAGS_MmaxPool);

    cssb.makeEtcdDis(FLAGS_registryHost, FLAGS_baseService, FLAGS_userService, FLAGS_messageService);

    cssb.makeRpcServer(FLAGS_listenPort, FLAGS_rpcTimeout, FLAGS_rpcThreads);

    cssb.makeEtcdReg(FLAGS_registryHost, FLAGS_baseService + FLAGS_instanceName, FLAGS_accessHost);

    auto server = cssb.build();

    server->start();

    return 0;
}