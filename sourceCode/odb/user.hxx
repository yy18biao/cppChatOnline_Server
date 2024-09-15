#pragma once

#include <iostream>
#include <string>
#include <cstddef>
#include <odb/nullable.hxx>
#include <odb/core.hxx>

#pragma db object table("userInfo")
class UserInfo
{
private:
        friend class odb::access; // 设置为友元类才可以让odb进行访问私有成员

#pragma db id auto         // 设置主键和自增长
        unsigned long _id; // 用户id

#pragma db type("varchar(64)") index unique // 创建普通索引
        std::string _userId;                // 用户账号

#pragma db type("varchar(64)") index unique // 创建普通索引
        std::string _phone;                 // 用户手机号

#pragma db type("varchar(64)")
        std::string _nickname; // 用户昵称

#pragma db type("varchar(255)")
        std::string _password; // 用户密码

#pragma db type("varchar(64)")
        odb::nullable<std::string> _userPhotoId; // 用户头像

        odb::nullable<std::string> _desc; // 用户个性签名

public:
        UserInfo() {}

        UserInfo(const std::string &userId, 
                 const std::string &nickname, 
                 const std::string &password, 
                 const std::string &phone)
            : _userId(userId), _nickname(nickname), _password(password), _phone(phone)
        {
        }

        // 各个成员的访问与设置接口
        void userId(const std::string &userId) { _userId = userId; }
        std::string userId() { return _userId; }

        void nickname(const std::string &nickname) { _nickname = nickname; }
        std::string nickname() { return _nickname; }

        void phone(const std::string &phone) { _phone = phone; }
        std::string phone() { return _phone; }

        void password(const std::string &password) { _password = password; }
        std::string password() { return _password; }

        void desc(const std::string &desc) { _desc = desc; }
        odb::nullable<std::string> desc() { return _desc; }

        void userPhotoId(const std::string &userPhotoId) { _userPhotoId = userPhotoId; }
        odb::nullable<std::string> userPhotoId() { return _userPhotoId; }
};

// odb -d mysql --std c++11 --generate-query --generate-schema --profile boost/date-time user.hxx