#pragma once
#include "ODBFactory.hpp"
#include "chatSession.hxx"
#include "chatSession-odb.hxx"
#include "MChatSessionUser.hpp"

class ChatSessionTable
{
private:
    std::shared_ptr<odb::core::database> _db;

public:
    using ptr = std::shared_ptr<ChatSessionTable>;

    ChatSessionTable(const std::shared_ptr<odb::core::database> &db) : _db(db) {}

    // 新增会话
    bool insert(ChatSession &cs)
    {
        try
        {
            odb::transaction trans(_db->begin());

            _db->persist(cs);

            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("新增会话失败 {}---{}", cs.chatSessionName(), e.what());
            return false;
        }
        return true;
    }

    // 删除会话
    bool remove(const std::string &ssid)
    {
        try
        {
            odb::transaction trans(_db->begin());

            // 删除会话信息并删除会话与用户的关联
            _db->erase_query<ChatSession>(odb::query<ChatSession>::chatSessionId == ssid);
            _db->erase_query<ChatSessionUser>(odb::query<ChatSessionUser>::sessionId == ssid);

            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("删除会话失败 {}---{}", ssid, e.what());
            return false;
        }
        return true;
    }

    // 删除指定单聊会话(根据单聊会话的两个成员)
    bool remove(const std::string &uid, const std::string &fid)
    {
        try
        {
            odb::transaction trans(_db->begin());

            // 获取单聊视图中的数据
            auto res = _db->query_one<SingleChatSession>(
                odb::query<SingleChatSession>::csm1::userId == uid &&
                odb::query<SingleChatSession>::csm2::userId == fid &&
                odb::query<SingleChatSession>::css::chatSessionType == ChatSessionType::SINGLE);

            // 根据会话id
            std::string sid = res->chatSessionId;
            _db->erase_query<ChatSession>(odb::query<ChatSession>::chatSessionId == sid);
            _db->erase_query<ChatSessionUser>(odb::query<ChatSessionUser>::sessionId == sid);

            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("删除会话失败 {}---{}:{}", uid, fid, e.what());
            return false;
        }
        return true;
    }

    // 获取会话(根据会话id)
    std::shared_ptr<ChatSession> select(const std::string &ssid)
    {
        std::shared_ptr<ChatSession> res;
        try
        {
            odb::transaction trans(_db->begin());

            res.reset(_db->query_one<ChatSession>(odb::query<ChatSession>::chatSessionId == ssid));

            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("通过会话ID获取会话信息失败 {}---{}", ssid, e.what());
        }
        return res;
    }

    // 获取指定用户的所有单聊会话
    std::vector<SingleChatSession> singleChatSession(const std::string &uid)
    {
        std::vector<SingleChatSession> res;
        try
        {
            odb::transaction trans(_db->begin());

            odb::result<SingleChatSession> r(_db->query<SingleChatSession>(
                odb::query<SingleChatSession>::css::chatSessionType == ChatSessionType::SINGLE &&
                odb::query<SingleChatSession>::csm1::userId == uid &&
                odb::query<SingleChatSession>::csm2::userId != odb::query<SingleChatSession>::csm1::userId));

            for (odb::result<SingleChatSession>::iterator i(r.begin()); i != r.end(); ++i)
            {
                res.push_back(*i);
            }

            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("获取用户 {} 的单聊会话失败:{}", uid, e.what());
        }
        return res;
    }

    // 获取指定用户的所有群聊会话
    std::vector<GroupChatSession> groupChatSession(const std::string &uid)
    {
        std::vector<GroupChatSession> res;
        try
        {
            odb::transaction trans(_db->begin());

            odb::result<GroupChatSession> r(_db->query<GroupChatSession>(
                odb::query<GroupChatSession>::css::chatSessionType == ChatSessionType::GROUP &&
                odb::query<GroupChatSession>::csm::userId == uid));
            for (odb::result<GroupChatSession>::iterator i(r.begin()); i != r.end(); ++i)
            {
                res.push_back(*i);
            }

            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("获取用户 {} 的群聊会话失败:{}", uid, e.what());
        }
        return res;
    }
};