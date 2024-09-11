#include <vector>
#include <mutex>
#include <unordered_map>
#include <memory>
#include <brpc/channel.h>

#include "log.hpp"

namespace hjb
{ 
    // 单个服务的信道管理类
    class ServiceChannel
    {
    public:
        using ptr = std::shared_ptr<ServiceChannel>;
        using ChannelPtr = std::shared_ptr<brpc::Channel>;

    private:
        std::string _name;                                  // 服务名称
        std::vector<ChannelPtr> _channels;                  // 当前服务对应的所有信道
        std::unordered_map<std::string, ChannelPtr> _hosts; // 主机ip与信道的映射
        int32_t _index;                                     // 当前信道轮转下标计数器
        std::mutex _mutex;

    public:
        ServiceChannel(const std::string &name)
            : _name(name), _index(0)
        {
        }

        // 服务器上线，添加信道
        void append(const std::string &host)
        {
            // 创建信道并初始化
            auto channel = std::make_shared<brpc::Channel>();
            brpc::ChannelOptions options;
            options.protocol = "baidu_std"; // 序列化协议
            options.timeout_ms = -1;        // 一直等待rpc请求
            options.max_retry = 3;          // 请求重试3次
            if (channel->Init(host.c_str(), &options) != 0)
            {
                ERROR("初始化{}-{}节点信道出错", _name, host);
                return;
            }

            std::unique_lock<std::mutex> lock(_mutex);
            _hosts.insert(std::make_pair(host, channel));
            _channels.push_back(channel);
        }

        // 服务器下线，删除信道
        void remove(const std::string &host)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            auto it = _hosts.find(host);
            if (it == _hosts.end())
            {
                WARN("{}-{}节点删除信道操作，无法找到该主机", _name, host);
                return;
            }

            for (auto i = _channels.begin(); i != _channels.end(); ++i)
            {
                if (*i == it->second)
                {
                    _channels.erase(i);
                    break;
                }
            }

            _hosts.erase(it);
        }

        // 获取信道(RR轮转)
        ChannelPtr get()
        {
            std::unique_lock<std::mutex> lock(_mutex);
            if (_channels.size() == 0)
            {
                ERROR("找不到提供 {} 服务的节点！", _name);
                return ChannelPtr();
            }

            int32_t idx = _index++ % _channels.size();
            return _channels[idx];
        }
    };

    // 全部服务的信道管理类
    class AllServiceChannel
    {
    public:
        using ptr = std::shared_ptr<AllServiceChannel>;

    private:
        std::mutex _mutex;
        std::unordered_set<std::string> _followServices;                // 需要关心上下线的服务集合
        std::unordered_map<std::string, ServiceChannel::ptr> _services; // 所有服务和主机的集合

    private:
        // 获取服务节点的名称
        std::string getServiceName(const std::string &instance)
        {
            auto pos = instance.find_last_of('/');
            if (pos == std::string::npos)
                return instance;

            return instance.substr(0, pos);
        }

    public:
        AllServiceChannel()
        {
        }

        // 获取指定服务的节点信道
        ServiceChannel::ChannelPtr choose(const std::string &name)
        {
            std::unique_lock<std::mutex> lock(_mutex);
            
            auto it = _services.find(name);
            if (it == _services.end())
            {
                ERROR("找不到提供 {} 服务的节点！", name);
                return ServiceChannel::ChannelPtr();
            }

            return it->second->get();
        }

        // 声明关注哪些服务的上下线 不关心的就不需要管理
        void declared(const std::string &name)
        {
            std::unique_lock<std::mutex> lock(_mutex);

            _followServices.insert(name);
        }

        // 服务上线时调用的回调接口
        void onServiceOnline(const std::string &instance, const std::string &host)
        {
            std::string name = getServiceName(instance);

            ServiceChannel::ptr service;
            {
                std::unique_lock<std::mutex> lock(_mutex);

                // 不对不关心的服务进行处理
                auto fit = _followServices.find(name);
                if (fit == _followServices.end())
                {
                    return;
                }

                // 先获取管理对象，没有则创建，有则添加节点
                auto it = _services.find(name);
                if (it == _services.end())
                {
                    service = std::make_shared<ServiceChannel>(name);
                    _services.insert(std::make_pair(name, service));
                }
                else
                {
                    service = it->second;
                }
            }

            if (!service)
            {
                ERROR("新增 {} 服务管理节点失败", name);
                return;
            }

            service->append(host);
            DEBUG("{}-{} 服务上线新节点", name, host);
        }

        // 服务下线时调用的回调接口，从服务信道管理中删除指定节点信道
        void onServiceOffline(const std::string &instance, const std::string &host)
        {
            std::string name = getServiceName(instance);

            ServiceChannel::ptr service;
            {
                std::unique_lock<std::mutex> lock(_mutex);

                // 不对不关心的服务进行处理
                auto fit = _followServices.find(name);
                if (fit == _followServices.end())
                {
                    return;
                }

                auto it = _services.find(name);
                if (it == _services.end())
                {
                    WARN("删除{}服务节点时，没有找到管理对象", name);
                    return;
                }
                service = it->second;
            }

            service->remove(host);
            DEBUG("{}-{} 服务下线节点", name, host);
        }
    };
}