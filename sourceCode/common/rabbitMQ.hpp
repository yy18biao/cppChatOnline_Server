#include <functional>
#include <ev.h>
#include <amqpcpp.h>
#include <amqpcpp/libev.h>
#include "log.hpp"

namespace hjb
{
    class MQClient
    {
    private:
        struct ev_loop *_loop;
        std::unique_ptr<AMQP::LibEvHandler> _handler;
        std::unique_ptr<AMQP::TcpConnection> _connection;
        std::unique_ptr<AMQP::TcpChannel> _channel;
        std::thread _loopThread; // 该服务的执行线程

    private:
        static void watcherCallback(struct ev_loop *loop, ev_async *watcher, int32_t revents)
        {
            ev_break(loop, EVBREAK_ALL);
        }

    public:
        using ptr = std::shared_ptr<MQClient>;

        using MessageCallback = std::function<void(const char *, size_t)>;

        MQClient(const std::string &user,
                 const std::string &pwd,
                 const std::string &host)
        {
            // 实例化底层网络通信框架的IO事件监控句柄
            _loop = EV_DEFAULT;

            // 实例化libEVHandler句柄（将AMQP框架与事件监控关联）
            _handler = std::make_unique<AMQP::LibEvHandler>(_loop);

            // 实例化网络连接对象
            std::string url = "amqp://" + user + ":" + pwd + "@" + host + "/";
            _connection = std::make_unique<AMQP::TcpConnection>(_handler.get(), AMQP::Address(url));

            // 实例化信道对象
            _channel = std::make_unique<AMQP::TcpChannel>(_connection.get());

            // 启动底层网络通信框架，开启IO
            _loopThread = std::thread([&]()
                                      { ev_run(_loop, 0); });
        }

        ~MQClient()
        {
            struct ev_async asyncWatcher;

            ev_async_init(&asyncWatcher, watcherCallback);
            ev_async_start(_loop, &asyncWatcher);
            ev_async_send(_loop, &asyncWatcher);
            if (_loopThread.joinable())
                _loopThread.join();

            _loop = nullptr;
        }

        // 声明以及绑定交换机和队列
        void declare(const std::string &exchange,
                     const std::string &queue,
                     const std::string &routingKey = "routing_key")
        {
            // 声明交换机并设置声明成功与失败的回调函数
            _channel->declareExchange(exchange, AMQP::ExchangeType::direct)
                .onError([](const char *message)
                         { ERROR("声明交换机失败 : {}", message); })
                .onSuccess([]()
                           { DEBUG("声明交换机成功"); });
            // 声明队列并设置声明成功与失败的回调函数
            _channel->declareQueue(queue)
                .onError([](const char *message)
                         { ERROR("声明队列失败 : {}", message); })
                .onSuccess([]()
                           { DEBUG("声明队列成功"); });
            // 绑定交换机和队列
            _channel->bindQueue(exchange, queue, routingKey)
                .onError([](const char *message)
                         { ERROR("绑定交换机和队列失败 : {}", message); })
                .onSuccess([]()
                           { DEBUG("绑定交换机和队列成功"); });
        }

        // 向交换机发布消息
        bool publish(const std::string &exchange,
                     const std::string &msg,
                     const std::string &routingKey = "routing_key")
        {
            if (!_channel->publish(exchange, routingKey, msg))
            {
                ERROR("消息发布失败");
                return false;
            }

            DEBUG("消息发布成功：{}", msg);

            return true;
        }

        // 订阅队列消息
        bool consume(const std::string &queue,
                     const MessageCallback &cb)
        {
            _channel->consume(queue)
                    .onReceived([this, cb](const AMQP::Message &message, uint64_t deliverTag, bool redelivered)
                    {
                        cb(message.body(), message.bodySize());
                        _channel->ack(deliverTag); 
                    })
                    .onError([](const char *message)
                    { ERROR("订阅队列消息失败 : {}", message); return false; });

            return true;
        }
    };
}