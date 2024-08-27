#include <gflags/gflags.h>
#include "../common/etcd.hpp"

DEFINE_bool(mode, false, "日志器运行模式");
DEFINE_string(filename, "", "日志器输出文件名");
DEFINE_int32(level, 1, "日志器输出最低等级");
DEFINE_string(etcdHost, "http://127.0.0.1:2379", "服务注册中心地址");
DEFINE_string(serviceBasedir, "/service", "注册监控key值根目录");

void online(const std::string &key, const std::string &value)
{
    DEBUG("上线服务：{}-{}", key, value);
}

void offline(const std::string &key, const std::string &value)
{
    DEBUG("下线服务：{}-{}", key, value);
}


int main(int argc, char* argv[])
{
    google::ParseCommandLineFlags(&argc, &argv, true);
    initLogger(FLAGS_mode, FLAGS_filename, FLAGS_level);

    EtcdDisClient::ptr disClient = std::make_shared<EtcdDisClient>(FLAGS_etcdHost,
                                                               FLAGS_serviceBasedir,
                                                               online,
                                                               offline);
  
    std::this_thread::sleep_for(std::chrono::seconds(600));
}