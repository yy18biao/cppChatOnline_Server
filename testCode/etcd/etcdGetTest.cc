#include <etcd/Client.hpp>
#include <etcd/KeepAlive.hpp>
#include <etcd/Response.hpp>
#include <etcd/Value.hpp>
#include <etcd/Watcher.hpp>
#include <thread>

// 键值对改变触发回调
void callback(const etcd::Response &res)
{
    if (!res.is_ok())
    {
        std::cout << "收到错误事件" << res.error_message() << "\n";
        return;
    }

    for (auto const &ev : res.events())
    {
        if (ev.event_type() == etcd::Event::EventType::PUT)
        {
            std::cout << "类型：" << ev.event_type() << "\n";
            std::cout << " 之前的值：" << ev.prev_kv().as_string() << "\n";
            std::cout << " 当前的值：" << ev.kv().as_string() << "\n";
        }
        else if(ev.event_type() == etcd::Event::EventType::DELETE_)
        {
            std::cout << " 之前的值：" << ev.prev_kv().as_string() << "\n";
            std::cout << " 当前的值：" << ev.kv().as_string() << "\n";
        }
    }
}

int main(int argc, char *argv[])
{
    std::string host = "http://127.0.0.1:2379";

    // 实例化客户端对象
    etcd::Client client(host);

    // 获取键值对信息
    auto res = client.ls("/service").get();
    if (!res.is_ok())
    {
        std::cout << "获取失败" << res.error_message() << "\n";
        return -1;
    }
    for (int i = 0; i < res.keys().size(); ++i)
    {
        std::cout << res.value(i).as_string() << "\n";
    }

    // 实例化键值对事件监控对象 Watcher
    auto watcher = etcd::Watcher(client, "/service", callback, true);
    watcher.Wait();

    return -1;
}