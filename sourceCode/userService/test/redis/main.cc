#include "../../../common/redis.hpp"
#include <gflags/gflags.h>

DEFINE_string(ip, "127.0.0.1", "服务器地址");
DEFINE_int32(port, 6379, "服务器端口");
DEFINE_int32(db, 0, "库的编号");
DEFINE_bool(keep_alive, true, "是否启动长连接保活");

int main(int argc, char *argv[])
{
    google::ParseCommandLineFlags(&argc, &argv, true);

    auto client = RedisClientFactory::create(FLAGS_ip, FLAGS_port, FLAGS_db, FLAGS_keep_alive);

    // Session session(client);
    // session.append("1", "11");
    // session.append("2", "22");
    // session.append("3", "33");

    // auto res = session.uid("1");
    // if(res)
    //     std::cout << *res << std::endl;

    // session.remove("1");

    VerifyCode c(client);
    c.append("168", "123");
}