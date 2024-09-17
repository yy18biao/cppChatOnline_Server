#include "elasticlient.hpp"
#include "user.hxx"
#include "message.hxx"

// es工厂(构造es操作对象)
class ESFactory
{
public:
    static std::shared_ptr<elasticlient::Client> create(const std::vector<std::string> hosts)
    {
        return std::make_shared<elasticlient::Client>(hosts);
    }
};

// 用户的es操作类
class ESUser
{
private:
    std::shared_ptr<elasticlient::Client> _client;

public:
    using ptr = std::shared_ptr<ESUser>;

    ESUser(const std::shared_ptr<elasticlient::Client> &client)
        : _client(client) {}

    // 创建索引
    bool createIndex()
    {
        if (!hjb::ESIndex(_client, "user")
                 .append("userId", "keyword", "standard", true)
                 .append("nickname")
                 .append("phone", "keyword", "standard", true)
                 .append("desc", "text", "standard", false)
                 .append("userPhotoId", "keyword", "standard", false)
                 .create())
        {
            ERROR("用户索引创建失败");
            return false;
        }

        INFO("用户索引创建success");
        return true;
    }

    // 添加索引数据
    bool appendData(const std::string &userId,
                    const std::string &nickname,
                    const std::string &phone,
                    const std::string &desc,
                    const std::string &userPhotoId)
    {
        if (!hjb::ESInsert(_client, "user")
                 .append("userId", userId)
                 .append("nickname", nickname)
                 .append("phone", phone)
                 .append("desc", desc)
                 .append("userPhotoId", userPhotoId)
                 .insert(userId))
        {
            ERROR("用户索引数据新增失败 -- {}", userId);
            return false;
        }

        INFO("用户索引数据新增success -- {}", userId);
        return true;
    }

    // 查询索引数据
    // 参数uids为过滤的条件集合
    std::vector<UserInfo> search(const std::string &key, const std::vector<std::string> &uids)
    {
        std::vector<UserInfo> users;

        Json::Value json_user = hjb::ESSearch(_client, "user")
                                    .appendShould("userId.keyword", key)
                                    .appendShould("phone.keyword", key)
                                    .appendShould("nickname", key)
                                    .appendMustNot("userId.keyword", uids)
                                    .search();

        if (!json_user.isArray())
        {
            ERROR("用户搜索结果为空或者结果不是数组类型");
            return users;
        }

        int sz = json_user.size();
        for (int i = 0; i < sz; i++)
        {
            UserInfo user;
            user.userId(json_user["_source"]["userId"].asString());
            user.nickname(json_user[i]["_source"]["nickname"].asString());
            user.desc(json_user[i]["_source"]["desc"].asString());
            user.phone(json_user[i]["_source"]["phone"].asString());
            user.userPhotoId(json_user[i]["_source"]["userPhotoId"].asString());
            users.push_back(user);
        }
        return users;
    }

    // 查询唯一账号条件数据
    std::shared_ptr<UserInfo> searchUserId(const std::string &uid)
    {
        Json::Value json_user = hjb::ESSearch(_client, "user")
                                    .appendShould("userId.keyword", {uid})
                                    .search();

        if (!json_user.isArray())
            return nullptr;

        std::shared_ptr<UserInfo> user = std::make_shared<UserInfo>();
        user->userId(json_user["_source"]["userId"].asString());
        user->nickname(json_user["_source"]["nickname"].asString());
        user->desc(json_user["_source"]["desc"].asString());
        user->phone(json_user["_source"]["phone"].asString());
        user->userPhotoId(json_user["_source"]["userPhotoId"].asString());
        return user;
    }

    // 查询唯一手机号码条件数据
    std::shared_ptr<UserInfo> searchPhone(const std::string &phone)
    {
        Json::Value json_user = hjb::ESSearch(_client, "user")
                                    .appendShould("phone.keyword", {phone})
                                    .search();

        if (!json_user.isArray())
            return nullptr;

        std::shared_ptr<UserInfo> user = std::make_shared<UserInfo>();
        user->userId(json_user["_source"]["userId"].asString());
        user->nickname(json_user["_source"]["nickname"].asString());
        user->desc(json_user["_source"]["desc"].asString());
        user->phone(json_user["_source"]["phone"].asString());
        user->userPhotoId(json_user["_source"]["userPhotoId"].asString());
        return user;
    }
};

// 消息es操作类
class ESMessage
{
private:
    std::shared_ptr<elasticlient::Client> _client;

public:
    using ptr = std::shared_ptr<ESMessage>;

    ESMessage(const std::shared_ptr<elasticlient::Client> &client)
        : _client(client)
    {
    }

    bool createIndex()
    {
        if (!hjb::ESIndex(_client, "message")
                 .append("useId", "keyword", "standard", false)
                 .append("messageId", "keyword", "standard", false)
                 .append("createTime", "long", "standard", false)
                 .append("chatSessionId", "keyword", "standard", true)
                 .append("content")
                 .create())
        {
            ERROR("消息索引创建失败");
            return false;
        }

        INFO("消息索引创建success");
        return true;
    }

    bool append(const std::string &useId,
                const std::string &messageId,
                const long createTime,
                const std::string &chatSessionId,
                const std::string &content)
    {
        if (!hjb::ESInsert(_client, "message")
                 .append("messageId", messageId)
                 .append("createTime", createTime)
                 .append("useId", useId)
                 .append("chatSessionId", chatSessionId)
                 .append("content", content)
                 .insert(messageId))
        {
            ERROR("消息索引数据新增失败 -- {}", messageId);
            return false;
        }

        INFO("消息索引数据新增success -- {}", messageId);
        return true;
    }

    bool remove(const std::string &messageId)
    {
        if (!hjb::ESRemove(_client, "message").remove(messageId))
        {
            ERROR("消息索引数据删除失败 -- {}", messageId);
            return false;
        }

        INFO("消息索引数据删除success -- {}", messageId);
        return true;
    }

    std::vector<Message> search(const std::string &key, const std::string &chatSessionId)
    {
        std::vector<Message> res;
        Json::Value json_user = hjb::ESSearch(_client, "message")
                                    .appendMustTerm("chatSessionId.keyword", chatSessionId)
                                    .appendMustMatch("content", key)
                                    .search();
        if (json_user.isArray() == false)
        {
            ERROR("用户搜索结果为空或者结果不是数组类型");
            return users;
        }

        DEBUG("检索结果数量：{}", json_user.size());
        for (int i = 0; i < json_user.size(); i++)
        {
            Message message;
            message.userId(json_user[i]["_source"]["userId"].asString());
            message.messageId(json_user[i]["_source"]["messageId"].asString());
            message.chatSessionId(json_user[i]["_source"]["chatSessionId"].asString());
            message.content(json_user[i]["_source"]["content"].asString());

            boost::posix_time::ptime ctime(boost::posix_time::from_time_t(json_user[i]["_source"]["createTime"].asInt64()));
            message.createTime(ctime);
            
            res.push_back(message);
        }
        return res;
    }
};