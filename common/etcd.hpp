#include <etcd/Client.hpp>
#include <etcd/KeepAlive.hpp>
#include <etcd/Response.hpp>
#include <etcd/Value.hpp>
#include <etcd/Watcher.hpp>
#include <functional>
#include <memory>
#include <thread>

#include "log.hpp"

class EtcdRegClient
{
private:
    std::shared_ptr<etcd::Client> _client;
    std::shared_ptr<etcd::KeepAlive> _keep; // 租约保活对象
    uint64_t _leaseId;                      // 租约id

public:
    using ptr = std::shared_ptr<EtcdRegClient>;

    EtcdRegClient(const std::string &host)
        : _client(std::make_shared<etcd::Client>(host))
        , _keep(_client->leasekeepalive(3).get())
        , _leaseId(_keep->Lease())
    {
    }

    // 服务注册（etcd添加键值对）
    bool reg(const std::string &key, const std::string &value)
    {
        DEBUG("key {}-{}", key, value);
        auto res = _client->put(key, value, _leaseId).get();
        if (!res.is_ok())
        {
            ERROR("新增失败，{}", res.error_message());
            return false;
        }

        return true;
    }
};

class EtcdDisClient
{
public:
    // 键值对发生变化的回调函数
    using notifyCallBack = std::function<void(std::string, std::string)>;
    using ptr = std::shared_ptr<EtcdDisClient>;

private:
    std::shared_ptr<etcd::Client> _client;
    std::shared_ptr<etcd::Watcher> _watcher;
    notifyCallBack _putCB;
    notifyCallBack _deleteCB;

private:
    // 键值对改变触发回调
    void callback(const etcd::Response &res)
    {
        if (!res.is_ok())
        {
            ERROR("收到错误事件，{}", res.error_message());
            return;
        }

        for (auto const &ev : res.events())
        {
            if (ev.event_type() == etcd::Event::EventType::PUT)
            {
                DEBUG("1");
                if (_putCB)
                    _putCB(ev.kv().key(), ev.kv().as_string());

                DEBUG("新增服务：{}-{}", ev.kv().key(), ev.kv().as_string());
            }
            else if (ev.event_type() == etcd::Event::EventType::DELETE_)
            {
                if (_deleteCB)
                    _deleteCB(ev.prev_kv().key(), ev.prev_kv().as_string());
                DEBUG("删除服务：{}-{}", ev.prev_kv().key(), ev.prev_kv().as_string());
            }
        }
    }

public:
    EtcdDisClient(const std::string &host,
                  const std::string &basedir,
                  const notifyCallBack &putCB,
                  const notifyCallBack &deleteCB)
        : _client(std::make_shared<etcd::Client>(host))
        , _putCB(putCB)
        , _deleteCB(deleteCB)
    {
        // 进行服务发现，获取当前已有数据
        auto res = _client->ls(basedir).get();
        if (!res.is_ok())
        {
            ERROR("收到错误事件，{}", res.error_message());
            abort();
        }

        for (int i = 0; i < res.keys().size(); ++i)
            if (_putCB)
                _putCB(res.key(i), res.value(i).as_string());
        
        // 进行事件监控
        _watcher = std::make_shared<etcd::Watcher>(
            *_client.get(),
            basedir,
            std::bind(&EtcdDisClient::callback, this, std::placeholders::_1), true);
    }
};