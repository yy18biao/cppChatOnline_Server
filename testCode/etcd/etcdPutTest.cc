#include <etcd/Client.hpp>
#include <etcd/KeepAlive.hpp>
#include <etcd/Response.hpp>
#include <thread>

int main(int argc, char *argv[])
{
    std::string host = "http://127.0.0.1:2379";

    // 实例化客户端对象
    etcd::Client client(host);

    // 获取租约保活对象
    auto keep = client.leasekeepalive(3).get();
    // 获取租约id
    auto leaseId = keep->Lease();

    // 添加数据
    auto res = client.put("/service/user", "127.0.0.1:8001", leaseId).get();
    auto res2 = client.put("/service/friend", "127.0.0.1:8002", leaseId).get();
    if (!res.is_ok())
    {
        std::cout << "新增失败" << res.error_message() << "\n";
        return -1;
    }
    if (!res2.is_ok())
    {
        std::cout << "新增失败" << res2.error_message() << "\n";
        return -1;
    }

    std::this_thread::sleep_for(std::chrono::seconds(10));

    return 0;
}