#include "elasticlient.hpp"
#include "user.hxx"

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
    ESUser(const std::shared_ptr<elasticlient::Client> &client)
        : _client(client) {}

    // 创建索引
    bool createIndex()
    {
        if (!hjb::ESIndex(_client, "user")
                 .append("id", "keyword", "standard", true)
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
    bool appendData(const std::string &id,
                    const std::string &userId,
                    const std::string &nickname,
                    const std::string &phone,
                    const std::string &desc,
                    const std::string &userPhotoId)
    {
        if (!hjb::ESInsert(_client, "user")
                 .append("id", id)
                 .append("userId", userId)
                 .append("nickname", nickname)
                 .append("phone", phone)
                 .append("desc", desc)
                 .append("userPhotoId", userPhotoId)
                 .insert(id))
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
                                    .appendShould("id.keyword", key)
                                    .appendShould("userId.keyword", key)
                                    .appendShould("nickname", key)
                                    .appendMustNot("userId.keyword", uids)
                                    .search();

        if (!json_user.isArray())
        {
            ERROR("用户搜索结果为空或者结果不是数组类型");
            return users;
        }

        int sz = json_user.size();
        DEBUG("检索结果数量：{}", sz);
        for (int i = 0; i < sz; i++)
        {
            UserInfo user;
            user.nickname(json_user[i]["_source"]["nickname"].asString());
            user.desc(json_user[i]["_source"]["desc"].asString());
            user.phone(json_user[i]["_source"]["phone"].asString());
            user.userPhotoId(json_user[i]["_source"]["userPhotoId"].asString());
            users.push_back(user);
        }
        return users;
    }
};