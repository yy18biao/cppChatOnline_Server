#include "ODBFactory.hpp"
#include "message.hxx"
#include "message-odb.hxx"

class MessageTable
{
private:
    std::shared_ptr<odb::core::database> _db;

public:
    using ptr = std::shared_ptr<MessageTable>;

    MessageTable(const std::shared_ptr<odb::core::database> &db)
        : _db(db)
    {
    }

    ~MessageTable() {}

    // 新增消息
    bool insert(Message &message)
    {
        try
        {
            // 获取事务操作对象(自动开启)
            odb::transaction trans(_db->begin());

            _db->persist(message);

            // 提交事务
            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("新增消息失败 {}---{}", message.messageId(), e.what());
            return false;
        }
        return true;
    }

    // 移除某个会话的所有消息记录
    bool remove(const std::string &chatSessionId)
    {
        try
        {
            // 获取事务操作对象(自动开启)
            odb::transaction trans(_db->begin());

            _db->erase_query<Message>(odb::query<Message>::chatSessionId == chatSessionId);

            // 提交事务
            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("删除会话所有消息失败 {}---{}", chatSessionId, e.what());
            return false;
        }
        return true;
    }

    // 获取最近指定条数的消息
    std::vector<Message> recent(const std::string &chatSessionId, int count)
    {
        std::vector<Message> res;
        try
        {
            // 获取事务操作对象(自动开启)
            odb::transaction trans(_db->begin());

            std::stringstream cond;
            cond << "chatSessionId='" << chatSessionId << "' ";
            cond << "order by createTime desc limit " << count;

            odb::result<Message> r(_db->query<Message>(cond.str()));
            for (odb::result<Message>::iterator i(r.begin()); i != r.end(); ++i)
            {
                res.push_back(*i);
            }
            std::reverse(res.begin(), res.end());

            // 提交事务
            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("获取最近消息失败:{}---{}---{}", chatSessionId, count, e.what());
        }
        return res;
    }

    // 获取区间时间内的消息
    std::vector<Message> range(const std::string &chatSessionId,
                               boost::posix_time::ptime &stime,
                               boost::posix_time::ptime &etime)
    {
        std::vector<Message> res;
        try
        {
            // 获取事务操作对象(自动开启)
            odb::transaction trans(_db->begin());

            // 获取指定会话指定时间段的信息
            odb::result<Message> r(_db->query<Message>(odb::query<Message>::chatSessionId == chatSessionId &&
                                                       odb::query<Message>::createTime >= stime &&
                                                       odb::query<Message>::createTime <= etime));
            for (odb::result<Message>::iterator i(r.begin()); i != r.end(); ++i)
            {
                res.push_back(*i);
            }

            // 提交事务
            trans.commit();
        }
        catch (std::exception &e)
        {
            ERROR("获取区间消息失败:{}-{}:{}-{}", chatSessionId,
                  boost::posix_time::to_simple_string(stime),
                  boost::posix_time::to_simple_string(etime), e.what());
        }

        return res;
    }
};