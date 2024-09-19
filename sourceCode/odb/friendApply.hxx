// 好友申请表
#pragma once
#include <string>
#include <cstddef>
#include <odb/core.hxx>

enum FriendApplyType
{
    AGREE = 1,
    REFUSE = 2,
    PENDING = 3
};

#pragma db object table("friendApply")
class FriendApply
{
public:
    FriendApply() {}
    FriendApply(const std::string &eid,
                const std::string &uid,
                const std::string &pid,
                const FriendApplyType &type = FriendApplyType::PENDING)
        : _userId(uid), _friendId(pid), _eventId(eid), _type(type)
    {
    }

    std::string eventId() const { return _eventId; }
    void eventId(std::string &eventId) { _eventId = eventId; }

    std::string userId() const { return _userId; }
    void userId(std::string &userId) { _userId = userId; }

    std::string friendId() const { return _friendId; }
    void friendId(std::string &friendId) { _friendId = friendId; }

    FriendApplyType type() const { return _type; }
    void type(FriendApplyType type) { _type = type; }

private:
    friend class odb::access;
#pragma db id auto
    unsigned long _id;
#pragma db type("varchar(64)") index unique
    std::string _eventId;
#pragma db type("varchar(64)") index
    std::string _userId;
#pragma db type("varchar(64)") index
    std::string _friendId;
#pragma db type("tinyint")
    FriendApplyType _type;
};
