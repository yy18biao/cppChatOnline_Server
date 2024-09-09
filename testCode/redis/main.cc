#include <iostream>
#include <vector>
#include <string>
#include <sw/redis++/redis.h>
#include <gflags/gflags.h>

DEFINE_string(ip, "127.0.0.1", "服务器地址");
DEFINE_int32(port, 6379, "服务器端口");
DEFINE_int32(db, 0, "库的编号");
DEFINE_bool(keep_alive, true, "是否启动长连接保活");



int main(int argc, char *argv[])
{
    google::ParseCommandLineFlags(&argc, &argv, true);

    // 实例化对象，连接redis服务器
    sw::redis::ConnectionOptions opts;
    opts.host = FLAGS_ip;               // ip
    opts.port = FLAGS_port;             // 端口
    opts.db = FLAGS_db;                 // 库的编号
    opts.keep_alive = FLAGS_keep_alive; // 是否长连接保活
    sw::redis::Redis client(opts);      // 实例化对象

    // 添加字符串键值对(在已有相同key的基础上调用则为修改)
    client.set("会话ID1", "会话ID1");
    client.set("会话ID2", "会话ID2");
    client.set("会话ID3", "会话ID3");

    // 获取字符串键值对
    auto user1 = client.get("会话ID1");
    if (user1)
        std::cout << *user1 << "\n";

    auto user2 = client.get("会话ID2");
    if (user2)
        std::cout << *user2 << "\n";

    auto user3 = client.get("会话ID3");
    if (user3)
        std::cout << *user3 << "\n";

    // 删除字符串键值对
    client.del("会话ID2");
    user2 = client.get("会话ID2");
    if (user2)
        std::cout << *user2 << "\n";

    // 控制数据有效时间
    client.set("会话ID1", "11111", std::chrono::milliseconds(1000));

    // 添加列表键值对（头插尾插获取）
    client.rpush("群聊1", "zhangsan");
    client.rpush("群聊1", "lisi");
    client.rpush("群聊1", "wangwu");

    // 获取列表键值对
    std::vector<std::string> users;
    // 0代表从第0下标开始获取， -1代表获取全部
    client.lrange("群聊1", 0, -1, std::back_inserter(users));
    for(auto u : users)
    {
        std::cout << u << "\n";
    }

    return 0;
}