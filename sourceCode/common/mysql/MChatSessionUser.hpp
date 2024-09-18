#pragma once

#include "ODBFactory.hpp"
#include "chatSessionUser-odb.hxx"
#include "chatSessionUser.hxx"

class ChatSessionUserTable
{
private:
    std::shared_ptr<odb::core::database> _db;

public:
    using ptr = std::shared_ptr<ChatSessionUserTable>;

    ChatSessionUserTable(const std::shared_ptr<odb::core::database> &db)
        : _db(db)
    {
    }

    // 新增单条会话与用户的关联
    bool append(ChatSessionUser &csu)
    {
        try
        {
            // 获取事务操作对象(自动开启)
            odb::transaction trans(_db->begin());

            // 新增数据操作
            _db->persist(csu);

            // 提交事务
            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("新增会话与用户的关联 {}---{} 失败 ：{}", csu.sessionId(), csu.userId(), e.what());
            return false;
        }

        return true;
    }

    // 新增多条会话与用户的关联
    bool append(std::vector<ChatSessionUser> &csus)
    {
        try
        {
            // 获取事务操作对象(自动开启)
            odb::transaction trans(_db->begin());

            for (auto &csu : csus)
                // 新增数据操作
                _db->persist(csu);

            // 提交事务
            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("新增多条会话与用户的关联失败 ：{}", e.what());
            return false;
        }

        return true;
    }

    // 删除单条会话与用户的关联
    bool remove(ChatSessionUser &csu)
    {
        try
        {
            // 获取事务操作对象(自动开启)
            odb::transaction trans(_db->begin());

            // 删除数据操作
            _db->erase_query<ChatSessionUser>(odb::query<ChatSessionUser>::sessionId == csu.sessionId() &&
                                              odb::query<ChatSessionUser>::userId == csu.userId());

            // 提交事务
            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("删除会话与用户的关联 {}---{} 失败 ：{}", csu.sessionId(), csu.userId(), e.what());
            return false;
        }

        return true;
    }

    // 删除指定会话的所有关联
    bool remove(const std::string &sessionId)
    {
        try
        {
            // 获取事务操作对象(自动开启)
            odb::transaction trans(_db->begin());

            // 删除数据操作
            _db->erase_query<ChatSessionUser>(odb::query<ChatSessionUser>::sessionId ==sessionId);

            // 提交事务
            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("删除指定会话 {} 的所有关联失败 ：{}", sessionId, e.what());
            return false;
        }

        return true;
    }

    // 获取指定会话所有关联
    std::vector<std::string> all(const std::string &sessionId)
    {
        std::vector<std::string> userIds;
        try
        {
            // 获取事务操作对象(自动开启)
            odb::transaction trans(_db->begin());

            odb::result<ChatSessionUser> r(_db->query<ChatSessionUser>(odb::query<ChatSessionUser>::sessionId == sessionId));
            for (odb::result<ChatSessionUser>::iterator i(r.begin()); i != r.end(); ++i)
                userIds.push_back(i->userId());
            
            // 提交事务
            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("获取指定会话 {} 所有关联用户失败: {}", sessionId, e.what());
        }

        return userIds;
    }
};