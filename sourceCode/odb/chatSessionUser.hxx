// 聊天会话与用户的关联表
#pragma once

#include <iostream>
#include <string>
#include <cstddef>
#include <odb/nullable.hxx>
#include <odb/core.hxx>

#pragma db object table("chatSessionUser")
class ChatSessionUser
{
private:
        friend class odb::access; // 设置为友元类才可以让odb进行访问私有成员

#pragma db id auto         // 设置主键和自增长
        unsigned long _id; // 主键id

#pragma db type("varchar(64)") index // 创建普通索引
        std::string _sessionId;      // 会话id

#pragma db type("varchar(64)")
        std::string _userId; // 用户id

public:
        ChatSessionUser() {}

        ChatSessionUser(const std::string &sessionId,
                        const std::string &userId)
            : _userId(userId), _sessionId(sessionId)
        {
        }

        // 各个成员的访问与设置接口
        void userId(const std::string &userId) { _userId = userId; }
        std::string userId() { return _userId; }

        void sessionId(const std::string &sessionId) { _sessionId = sessionId; }
        std::string sessionId() { return _sessionId; }
};

// odb -d mysql --std c++11 --generate-query --generate-schema --profile boost/date-time chatSessionUser.hxx