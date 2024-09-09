#include <gflags/gflags.h>
#include "../common/rabbitMQ.hpp"

DEFINE_bool(mode, false, "日志器运行模式");
DEFINE_string(filename, "", "日志器输出文件名");
DEFINE_int32(level, 1, "日志器输出最低等级");
DEFINE_string(etcdHost, "http://127.0.0.1:2379", "服务注册中心地址");
DEFINE_string(serviceBasedir, "/service", "注册监控key值根目录");
DEFINE_int32(mqPort, 5672, "mq的端口");
DEFINE_string(mqHost, "127.0.0.1:5672", "mq的地址");
DEFINE_string(mqUser, "root", "mq的用户");
DEFINE_string(mqPwd, "123456", "mq的密码");

void callback(const char* body, size_t size)
{
    std::string msg;
    msg.assign(body, size);
    std::cout << msg << std::endl;
}

int main(int argc, char* argv[])
{
    google::ParseCommandLineFlags(&argc, &argv, true);
    initLogger(FLAGS_mode, FLAGS_filename, FLAGS_level);

    MQClient client(FLAGS_mqUser, FLAGS_mqPwd, FLAGS_mqHost);

    client.consume("test-queue", callback);

    std::this_thread::sleep_for(std::chrono::seconds(60));

}