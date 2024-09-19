#pragma once

#include "ODBFactory.hpp"
#include "friend.hxx"
#include "friend-odb.hxx"

class FriendTable
{
private:
    std::shared_ptr<odb::core::database> _db;

public:
    using ptr = std::shared_ptr<FriendTable>;

    FriendTable(const std::shared_ptr<odb::core::database> &db)
        : _db(db)
    {
    }

    // 新增好友关系
    bool insert(const std::string &uid, const std::string &fid)
    {
        try
        {
            Friend r1(uid, fid);
            Friend r2(fid, uid);

            odb::transaction trans(_db->begin());

            _db->persist(r1);
            _db->persist(r2);

            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("新增用户好友关系信息失败 {}---{}:{}", uid, fid, e.what());
            return false;
        }
        return true;
    }

    // 移除关系信息
    bool remove(const std::string &uid, const std::string &fid)
    {
        try
        {
            odb::transaction trans(_db->begin());

            _db->erase_query<Friend>(odb::query<Friend>::userId == uid && odb::query<Friend>::friendId == fid);
            _db->erase_query<Friend>(odb::query<Friend>::userId == fid && odb::query<Friend>::friendId == uid);

            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("删除好友关系信息失败 {}---{}:{}！", uid, fid, e.what());
            return false;
        }
        return true;
    }

    // 判断关系是否存在
    bool exists(const std::string &uid, const std::string &fid)
    {
        bool flag = false;
        try
        {
            odb::transaction trans(_db->begin());

            odb::result<Friend> r = _db->query<Friend>(odb::query<Friend>::userId == uid && odb::query<Friend>::friendId == fid);
            flag = !r.empty();

            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("获取用户好友关系失败:{}---{}:{}", uid, fid, e.what());
        }
        return flag;
    }

    // 获取指定用户的好友ID
    std::vector<std::string> friends(const std::string &uid)
    {
        std::vector<std::string> res;
        try
        {
            odb::transaction trans(_db->begin());

            odb::result<Friend> r(_db->query<Friend>(odb::query<Friend>::userId == uid));
            for (odb::result<Friend>::iterator i(r.begin()); i != r.end(); ++i)
            {
                res.push_back(i->friendId());
            }

            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("获取用户 {} 的所有好友ID失败: {}", uid, e.what());
        }
        return res;
    }
};