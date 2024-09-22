#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include "log.hpp"

typedef websocketpp::server<websocketpp::config::asio> wserver;

namespace hjb
{
    class Connection
    {
    private:
        std::unordered_map<std::string, wserver::connection_ptr> _uid_connections; // 用户id与websocket连接的关联
        // 用户的登录会话id和用户id与websocket连接的关联
        std::unordered_map<wserver::connection_ptr, std::pair<std::string, std::string>> _conns_user;
        std::mutex _mutex;

    public:
        using ptr = std::shared_ptr<Connection>;

        Connection(){}

        // 新增连接
        void insert(const wserver::connection_ptr &conn,
                    const std::string &uid,
                    const std::string &sid)
        {
            std::unique_lock<std::mutex> lock(_mutex);

            _uid_connections.insert(std::make_pair(uid, conn));
            _conns_user.insert(std::make_pair(conn, std::make_pair(uid, sid)));
            DEBUG("长连接建立成功 {}--{}-{}", (size_t)conn.get(), uid, sid);
        }

        // 获取连接
        wserver::connection_ptr connection(const std::string &uid)
        {
            std::unique_lock<std::mutex> lock(_mutex);

            auto it = _uid_connections.find(uid);
            if (it == _uid_connections.end())
            {
                ERROR("未找到 {} 客户端的长连接", uid);
                return wserver::connection_ptr();
            }

            return _uid_connections[uid];
        }

        // 获取客户端的身份信息
        bool client(const wserver::connection_ptr &conn,
                    std::string &uid,
                    std::string &sid)
        {
            std::unique_lock<std::mutex> lock(_mutex);

            auto it = _conns_user.find(conn);
            if (it == _conns_user.end())
            {
                ERROR("未找到长连接 {} 对应的客户端", (size_t)conn.get());
                return false;
            }

            uid = it->second.first;
            sid = it->second.second;
            return true;
        }

        // 移除关联
        void remove(const wserver::connection_ptr &conn)
        {
            std::unique_lock<std::mutex> lock(_mutex);

            auto it = _conns_user.find(conn);
            if (it == _conns_user.end())
            {
                ERROR("未找到长连接 {} 对应的客户端", (size_t)conn.get());
                return;
            }

            std::string uid = it->second.first;

            _conns_user.erase(conn);
            _uid_connections.erase(uid);
        }
    };
}