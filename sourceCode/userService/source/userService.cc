#include "userService.hpp"

DEFINE_bool(runMode, false, "程序的运行模式，false-调试； true-发布；");
DEFINE_string(logFile, "", "发布模式下指定日志的输出文件");
DEFINE_int32(logLevel, 0, "发布模式下指定日志输出等级");

DEFINE_string(registryHost, "http://127.0.0.1:2379", "服务注册中心地址");
DEFINE_string(baseService, "/service", "服务监控根目录");
DEFINE_string(instanceName, "/userService/instance", "当前实例名称");
DEFINE_string(accessHost, "127.0.0.1:8001", "当前实例的外部访问地址");

DEFINE_string(file_service, "/service/fileService", "文件管理子服务名称");

DEFINE_string(Rhost, "127.0.0.1", "服务器地址");
DEFINE_int32(Rport, 6379, "服务器端口");
DEFINE_int32(Rdb, 0, "库的编号");
DEFINE_bool(RkeepAlive, true, "是否启动长连接保活");

DEFINE_string(Mhost, "127.0.0.1", "mysql服务器地址");
DEFINE_int32(Mport, 3306, "mysql服务器端口");
DEFINE_string(Mdb, "chatOnline", "mysql默认库");
DEFINE_string(Muser, "root", "mysql用户名");
DEFINE_string(Mpwd, "111111", "mysql密码");
DEFINE_string(Mcharset, "utf8", "mysql客户端字符集");
DEFINE_int32(MmaxPool, 3, "mysql连接池最大连接数");

DEFINE_string(Ehost, "http://127.0.0.1:9200/", "es服务器URL");

DEFINE_int32(listenPort, 8001, "Rpc服务器监听端口");
DEFINE_int32(rpcTimeout, -1, "Rpc调用超时时间");
DEFINE_int32(rpcThreads, 1, "Rpc的IO线程数量");

DEFINE_string(dms_key_id, "XXX", "短信平台密钥ID");
DEFINE_string(dms_key_secret, "XXX", "短信平台密钥");

int main(int argc, char *argv[])
{
    google::ParseCommandLineFlags(&argc, &argv, true);
    hjb::initLogger(FLAGS_runMode, FLAGS_logFile, FLAGS_logLevel);

    hjb::UserServerBuilder usb;
    usb.makeDms(FLAGS_dms_key_id, FLAGS_dms_key_secret);

    usb.makeEs({FLAGS_Ehost});
    
    usb.makeMysql(FLAGS_Muser, FLAGS_Mpwd, FLAGS_Mhost,
                  FLAGS_Mdb, FLAGS_Mcharset, FLAGS_Mport, FLAGS_MmaxPool);
    
    usb.makeRedis(FLAGS_Rhost, FLAGS_Rport, FLAGS_Rdb, FLAGS_RkeepAlive);
    
    usb.makeEtcdDis(FLAGS_registryHost, FLAGS_baseService, FLAGS_file_service);
    
    usb.makeRpcServer(FLAGS_listenPort, FLAGS_rpcTimeout, FLAGS_rpcThreads);
    
    usb.makeEtcdReg(FLAGS_registryHost, FLAGS_baseService + FLAGS_instanceName, FLAGS_accessHost);
    
    auto server = usb.build();
    
    server->start();
    
    return 0;
}