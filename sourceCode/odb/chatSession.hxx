// 聊天会话的信息表
#pragma once
#include <string>
#include <cstddef>
#include <odb/nullable.hxx>
#include <odb/core.hxx>
#include "chatSessionUser.hxx"

// 聊天会话的类型
enum class ChatSessionType
{
    SINGLE = 1,
    GROUP = 2
};

#pragma db object table("chatSession")
class ChatSession
{
private:
    friend class odb::access;
#pragma db id auto
    unsigned long _id;

#pragma db type("varchar(64)") index unique
    std::string _chatSessionId;

#pragma db type("varchar(64)")
    std::string _chatSessionName;

#pragma db type("tinyint")
    ChatSessionType _chatSessionType; // 1-单聊； 2-群聊

public:
    ChatSession() {}

    ChatSession(const std::string &id,
                const std::string &name,
                const ChatSessionType type)
        : _chatSessionId(id),
          _chatSessionName(name),
          _chatSessionType(type) {}

    std::string chatSessionId() const { return _chatSessionId; }
    void chatSessionId(std::string &chatSessionId) { _chatSessionId = chatSessionId; }

    std::string chatSessionName() const { return _chatSessionName; }
    void chatSessionName(std::string &chatSessionName) { _chatSessionName = chatSessionName; }

    ChatSessionType chatSessionType() const { return _chatSessionType; }
    void chatSessionType(ChatSessionType chatSessionType) { _chatSessionType = chatSessionType; }
};

// 单聊会话的视图
#pragma db view object(ChatSession = css)                                    \
    object(ChatSessionUser = csm1 : css::_chatSessionId == csm1::_sessionId) \
        object(ChatSessionUser = csm2 : css::_chatSessionId == csm2::_sessionId) query((?))
struct SingleChatSession
{
#pragma db column(css::_chatSessionId)
    std::string chatSessionId;
#pragma db column(csm2::_userId)
    std::string friendId;
};

// 群聊会话的视图
#pragma db view object(ChatSession = css)                                       \
        object(ChatSessionUser = csm : css::_chatSessionId == csm::_sessionId) query((?))
struct GroupChatSession
{
#pragma db column(css::_chatSessionId)
    std::string chatSessionId;
#pragma db column(css::_chatSessionName)
    std::string chatSessionName;
};