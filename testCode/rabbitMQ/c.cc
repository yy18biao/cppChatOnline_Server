#include <iostream>
#include <string>
#include <ev.h>
#include <amqpcpp.h>
#include <amqpcpp/libev.h>
#include <openssl/ssl.h>
#include <openssl/opensslv.h>

// 收到消息后的回调函数
void callback(AMQP::TcpChannel *channel, const AMQP::Message &message, uint64_t delivertTag, bool redelivered)
{
    std::string msg;
    msg.assign(message.body(), message.bodySize());
    std::cout << msg << std::endl;
    channel->ack(delivertTag);
}

int main()
{
    // 实例化底层网络通信框架的IO事件监控句柄
    auto *loop = EV_DEFAULT;

    // 实例化libEVHandler句柄（将AMQP框架与事件监控关联）
    AMQP::LibEvHandler handler(loop);

    // 实例化网络连接对象
    AMQP::Address address("amqp://root:123456@127.0.0.1:5672/");
    AMQP::TcpConnection connection(&handler, address);

    // 实例化信道对象
    AMQP::TcpChannel channel(&connection);

    // 声明交换机并设置声明成功与失败的回调函数
    channel.declareExchange("test-exchange", AMQP::ExchangeType::direct)
            .onError([](const char *message){ std::cout << "声明交换机失败 : "<< message << "\n"; exit(0); })
            .onSuccess([](){ std::cout << "声明交换机成功" << "\n"; }); 

    // 声明队列并设置声明成功与失败的回调函数
    channel.declareQueue("test-queue")
            .onError([](const char *message) { std::cout << "声明队列失败 : "<< message << "\n"; exit(0); })
            .onSuccess([](){ std::cout << "声明队列成功\n"; });

    // 绑定交换机和队列
    channel.bindQueue("test-exchange", "test-queue", "test-queue-key")
            .onError([](const char *message) { std::cout << "绑定交换机和队列失败 : "<< message << "\n"; exit(0); })
            .onSuccess([]() { std::cout << "绑定交换机和队列成功\n"; });

    // 订阅队列消息并设置回调函数
    channel.consume("test-queue", "con-tag")
            .onReceived(std::bind(callback, &channel, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3))
            .onError([](const char *message){ std::cout << "订阅队列消息失败 : "<< message << "\n"; exit(0); }); 
            

    // 启动底层网络通信框架，开启IO
    ev_run(loop, 0);
    return 0;
}