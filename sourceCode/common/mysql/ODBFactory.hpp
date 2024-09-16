#pragma once

#include <string>
#include <memory>
#include <cstdlib>
#include <iostream>
#include <odb/database.hxx>
#include <odb/mysql/database.hxx>

#include "../log.hpp"

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

