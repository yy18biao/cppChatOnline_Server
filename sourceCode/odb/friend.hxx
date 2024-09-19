// 好友表
#pragma once
#include <string>
#include <cstddef>
#include <odb/nullable.hxx>
#include <odb/core.hxx>

#pragma db object table("friend")
class Friend
{
public:
    Friend() {}
    Friend(const std::string &uid, const std::string &fid) : _userId(uid), _friendId(fid) {}

    std::string userId() const { return _userId; }
    void userId(std::string &userId) { _userId = userId; }

    std::string friendId() const { return _friendId; }
    void friendId(std::string &friendId) { _friendId = friendId; }

private:
    friend class odb::access;
#pragma db id auto
    unsigned long _id;
#pragma db type("varchar(64)") index
    std::string _userId;
#pragma db type("varchar(64)")
    std::string _friendId;
};
// odb -d mysql --std c++11 --generate-query --generate-schema --profile boost/date-time friend.hxx