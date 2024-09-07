#include <gflags/gflags.h>
#include "../common/elasticlient.hpp"

DEFINE_bool(mode, false, "日志器运行模式");
DEFINE_string(filename, "", "日志器输出文件名");
DEFINE_int32(level, 1, "日志器输出最低等级");
DEFINE_string(etcdHost, "http://127.0.0.1:2379", "服务注册中心地址");
DEFINE_string(serviceBasedir, "/service", "注册监控key值根目录");
DEFINE_string(serviceInstance, "/userInfo", "注册监控key值根目录");

int main(int argc, char* argv[])
{
    google::ParseCommandLineFlags(&argc, &argv, true);
    initLogger(FLAGS_mode, FLAGS_filename, FLAGS_level);

    std::shared_ptr<elasticlient::Client> client(new elasticlient::Client({"http://127.0.0.1:9200/"}));
    ESIndex es(client, "user", "_doc");
    es.append("name", "text").append("id", "keyword").append("description", "text", "ik_max_word", false).create();
}