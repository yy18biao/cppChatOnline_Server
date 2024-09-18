// 聊天消息
#pragma once

#include <iostream>
#include <string>
#include <cstddef>
#include <odb/nullable.hxx>
#include <odb/core.hxx>
#include <boost/date_time/posix_time/posix_time.hpp>

#pragma db object table("Message")
class Message
{
private:
    friend class odb::access; // 设置为友元类才可以让odb进行访问私有成员

#pragma db id auto     // 设置主键和自增长
    unsigned long _id; // 主键id

#pragma db type("varchar(64)") index unique // 创建普通索引
    std::string _messageId;                 // 消息id

#pragma db type("varchar(64)") index // 创建普通索引
    std::string _chatSessionId;          // 会话id

#pragma db type("varchar(64)")
    std::string _userId; // 用户id

    unsigned char _messageType; // 消息类型 0-文本，1-图片，2-文件，3-语音

    odb::nullable<std::string> _content; // 文本消息正文

#pragma db type("varchar(64)")
    odb::nullable<std::string> _fileId; // 文件消息时的文件id

#pragma db type("varchar(128)")
    odb::nullable<std::string> _fileName; // 文件消息的文件名称

    odb::nullable<unsigned long> _fileSize; // 文件消息的文件大小

#pragma db type("TIMESTAMP")
    boost::posix_time::ptime _createTime; // 消息的产生时间

public:
    Message() {}

    Message(const std::string &messageId,
            const std::string &chatSessionId,
            const std::string &userId,
            const unsigned char messageType,
            const boost::posix_time::ptime &ctime)
        : _userId(userId), _chatSessionId(chatSessionId),
          _messageId(messageId), _createTime(ctime), _messageType(messageType)
    {
    }

    // 各个成员的访问与设置接口
    void messageId(const std::string &messageId) { _messageId = messageId; }
    std::string messageId() const { return _messageId; }

    void userId(const std::string &userId) { _userId = userId; }
    std::string userId() const { return _userId; }

    void chatSessionId(const std::string &chatSessionId) { _chatSessionId = chatSessionId; }
    std::string chatSessionId() const  { return _chatSessionId; }

    void messageType(const unsigned char messageType) { _messageType = messageType; }
    unsigned char messageType() const { return _messageType; }

    boost::posix_time::ptime createTime() const { return _createTime; }
    void createTime(const boost::posix_time::ptime &createTime) { _createTime = createTime; }

    std::string content() const
    {
        if (!_content)
            return std::string();
        return *_content;
    }
    void content(const std::string &content) { _content = content; }

    std::string fileId() const
    {
        if (!_fileId)
            return std::string();
        return *_fileId;
    }
    void fileId(const std::string &fileId) { _fileId = fileId; }

    std::string fileName() const
    {
        if (!_fileName)
            return std::string();
        return *_fileName;
    }
    void fileName(const std::string &fileName) { _fileName = fileName; }

    unsigned long fileSize() const
    {
        if (!_fileSize)
            return 0;
        return *_fileSize;
    }
    void fileSize(unsigned long fileSize) { _fileSize = fileSize; }
};

// odb -d mysql --std c++11 --generate-query --generate-schema --profile boost/date-time message.hxx