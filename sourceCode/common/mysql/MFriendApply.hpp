#pragma once

#include "ODBFactory.hpp"
#include "friendApply.hxx"
#include "friendApply-odb.hxx"

class FriendApplyTable
{
private:
    std::shared_ptr<odb::core::database> _db;

public:
    using ptr = std::shared_ptr<FriendApplyTable>;

    FriendApplyTable(const std::shared_ptr<odb::core::database> &db)
        : _db(db)
    {
    }

    bool insert(FriendApply &ev)
    {
        try
        {
            odb::transaction trans(_db->begin());

            _db->persist(ev);

            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("新增好友申请事件失败 {}-{}:{}", ev.userId(), ev.friendId(), e.what());
            return false;
        }
        return true;
    }

    bool exists(const std::string &uid, const std::string &fid)
    {
        bool flag = false;
        try
        {
            odb::transaction trans(_db->begin());

            odb::result<FriendApply> r(_db->query<FriendApply>(odb::query<FriendApply>::userId == uid && odb::query<FriendApply>::friendId == fid));
            flag = !r.empty();

            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("获取好友申请事件失败:{}-{}:{}", uid, fid, e.what());
        }
        return flag;
    }

    bool remove(const std::string &uid, const std::string &fid)
    {
        try
        {
            odb::transaction trans(_db->begin());

            _db->erase_query<FriendApply>(odb::query<FriendApply>::userId == uid && odb::query<FriendApply>::friendId == fid);

            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("删除好友申请事件失败 {}-{}:{}", uid, fid, e.what());
            return false;
        }
        return true;
    }

    // 获取当前指定用户的 所有好友申请者ID
    std::vector<std::string> applyUsers(const std::string &uid)
    {
        std::vector<std::string> res;
        try
        {
            odb::transaction trans(_db->begin());

            odb::result<FriendApply> r(_db->query<FriendApply>(odb::query<FriendApply>::friendId == uid));
            for (odb::result<FriendApply>::iterator i(r.begin()); i != r.end(); ++i)
            {
                res.push_back(i->userId());
            }

            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("获取用户 {} 的好友申请者失败:{}", uid, e.what());
        }
        return res;
    }

    // 获取指定申请id的申请数据
    std::shared_ptr<FriendApply> searchApply(const std::string &eid)
    {
        std::shared_ptr<FriendApply> r;
        try
        {
            odb::transaction trans(_db->begin());

            r.reset(_db->query_one<FriendApply>(odb::query<FriendApply>::eventId == eid));

            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("获取好友申请数据 {} 失败:{}", eid, e.what());
            return nullptr;
        }
        return r;
    }

    // 修改申请数据
    bool updateApply(std::shared_ptr<FriendApply> &fa)
    {
        try
        {
            // 获取事务操作对象(自动开启)
            odb::transaction trans(_db->begin());

            // 新增数据操作
            _db->update(*fa);

            // 提交事务
            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("修改申请数据 {} 失败 ：{}", fa->eventId(), e.what());
            return false;
        }

        return true;
    }
};