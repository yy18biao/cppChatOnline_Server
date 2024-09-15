#include <iostream>
#include <vector>
#include <string>
#include <sw/redis++/redis.h>

// redisClient工厂(构造redis操作对象)
class RedisClientFactory
{
public:
    static std::shared_ptr<sw::redis::Redis> create(const std::string &host,
                                   int port,
                                   int db,
                                   bool keepAlive)
    {
        sw::redis::ConnectionOptions opts;
        opts.host = host;              // ip
        opts.port = port;              // 端口
        opts.db = db;                  // 库的编号
        opts.keep_alive = keepAlive;   // 是否长连接保活
        auto client = std::make_shared<sw::redis::Redis>(opts); // 实例化对象

        return client;
    }
};

// 用户登录会话类
class LoginSession
{
private:
    std::shared_ptr<sw::redis::Redis> _client;

public:
    using ptr = std::shared_ptr<LoginSession>;

    LoginSession(const std::shared_ptr<sw::redis::Redis> &client) : _client(client) {}

    void append(const std::string &sid, const std::string &uid)
    {
        _client->set(sid, uid);
    }

    void remove(const std::string &sid)
    {
        _client->del(sid);
    }

    // 获取用户id
    sw::redis::OptionalString uid(const std::string &sid)
    {
        return _client->get(sid);
    }
};

// 用户状态类
class LoginStatus
{
private:
    std::shared_ptr<sw::redis::Redis> _client;

public:
    using ptr = std::shared_ptr<LoginStatus>;

    LoginStatus(const std::shared_ptr<sw::redis::Redis> &client) : _client(client) {}

    void append(const std::string &uid)
    {
        _client->set(uid, "");
    }

    void remove(const std::string &uid)
    {
        _client->del(uid);
    }

    // 判断是否存在
    bool exists(const std::string &uid)
    {
        auto res = _client->get(uid);
        if (res)
            return true;
        return false;
    }
};

// 用户验证码类
class VerifyCode
{
private:
    std::shared_ptr<sw::redis::Redis> _client;

public:
    using ptr = std::shared_ptr<VerifyCode>;

    VerifyCode(const std::shared_ptr<sw::redis::Redis> &client) : _client(client) {}

    // 新增验证码(有过期时间 默认五分钟)
    void append(const std::string &cid, 
                const std::string &phone, 
                const std::string &code,
                const std::chrono::seconds &t = std::chrono::seconds(300))
    {
        _client->hset(cid, std::make_pair(phone, code));
        _client->expire(cid, t);
    }

    void remove(const std::string &cid)
    {
        _client->del(cid);
    }

    // 获取验证码
    sw::redis::OptionalString code(const std::string &cid, const std::string &phone)
    {
        return _client->hget(cid, phone);
    }
};