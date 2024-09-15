#pragma once

#include <string>
#include <memory>
#include <cstdlib>
#include <iostream>
#include <odb/database.hxx>
#include <odb/mysql/database.hxx>

#include "log.hpp"
#include "user-odb.hxx"
#include "user.hxx"

// odb工厂(构造数据库操作对象)
class ODBFactory
{
public:
    // 构造数据库操作对象
    static std::shared_ptr<odb::core::database> create(const std::string &user,
                                                        const std::string &password,
                                                        const std::string &host,
                                                        const std::string &db,
                                                        const std::string &charset,
                                                        int port,
                                                        int poolCount)
    {
        // 构造连接池工厂配置对象
        std::unique_ptr<odb::mysql::connection_pool_factory> cpf(new odb::mysql::connection_pool_factory(poolCount, 0));

        // 构造数据库操作对象
        // 连接池工厂配置对象的传参需要传右值
        auto res = std::make_shared<odb::mysql::database>(user, password, db, host, port, "", charset, 0, std::move(cpf));

        return res;
    }
};

class UserTable
{
private:
    std::shared_ptr<odb::core::database> _db;

public:
    using ptr = std::shared_ptr<UserTable>;

    UserTable(const std::shared_ptr<odb::core::database> &db)
        : _db(db)
    {
    }

    // 新增用户
    void insert(std::shared_ptr<UserInfo> &user)
    {
        try
        {
            // 获取事务操作对象(自动开启)
            odb::transaction trans(_db->begin());

            // 新增数据操作
            _db->persist(*user);

            // 提交事务
            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("新增用户 {} 失败 ：{}", user->userId(), e.what());
        }
    }

    // 修改用户信息(注意传参的用户必须是查询出来的用户)
    void update(std::shared_ptr<UserInfo> &user)
    {
        try
        {
            // 获取事务操作对象(自动开启)
            odb::transaction trans(_db->begin());

            // 新增数据操作
            _db->update(*user);

            // 提交事务
            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("修改用户 {} 失败 ：{}", user->userId(), e.what());
        }
    }

    // 根据id查询用户信息
    std::shared_ptr<UserInfo> selectByUserId(const std::string &userId)
    {
        std::shared_ptr<UserInfo> res;
        try
        {
            // 获取事务操作对象(自动开启)
            odb::transaction trans(_db->begin());

            // 查询指定数据操作
            res.reset(_db->query_one<UserInfo>(odb::query<UserInfo>::userId == userId));

            // 提交事务
            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("查询用户 {} (账号) 失败 ：{}", userId, e.what());
        }

        return res;
    }

    // 根据手机号码查询用户信息
    std::shared_ptr<UserInfo> selectByPhone(const std::string &phone)
    {
        std::shared_ptr<UserInfo> res;
        try
        {
            // 获取事务操作对象(自动开启)
            odb::transaction trans(_db->begin());

            // 查询指定数据操作
            res.reset(_db->query_one<UserInfo>(odb::query<UserInfo>::phone == phone));

            // 提交事务
            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("查询用户 {} (手机号) 失败 ：{}", phone, e.what());
        }

        return res;
    }

    // 根据用户昵称查询用户信息
    std::vector<UserInfo> selectByNickname(const std::string &nickname)
    {
        std::vector<UserInfo> users;
        try
        {
            // 获取事务操作对象(自动开启)
            odb::transaction trans(_db->begin());

            // 查询指定数据操作
            odb::result<UserInfo> res(_db->query<UserInfo>(odb::query<UserInfo>::nickname == nickname));

            auto it = res.begin();
            while (it != res.end())
            {
                users.push_back(*it);
                ++it;
            }


            // 提交事务
            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("查询用户 {} (昵称) 失败 ：{}", nickname, e.what());
        }

        return users;
    }

    // 根据多个id获取多个用户
    std::vector<UserInfo> selectUsersByUserId(const std::vector<std::string> userIds)
    {
        std::vector<UserInfo> users;
        try
        {
            // 获取事务操作对象(自动开启)
            odb::transaction trans(_db->begin());

            // 编辑查询条件
            std::stringstream str;
            str << "userId in (";
            for (const auto &id : userIds)
            {
                str << "'" << id << "',";
            }
            std::string condition = str.str();
            condition.pop_back();
            condition += ")";

            // 查询指定数据操作
            odb::result<UserInfo> res(_db->query<UserInfo>(condition));
            for (odb::result<UserInfo>::iterator i(res.begin()); i != res.end(); ++i)
            {
                users.push_back(*i);
            }

            // 提交事务
            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("查询多个用户失败 ：{}", e.what());
        }

        return users;
    }
};
