#include "../../common/mysql.hpp"

void insert(UserTable &userTb)
{
    auto user1 = std::make_shared<UserInfo>("111", "张三", "123456", "123123124");
    userTb.insert(user1);
    auto user2 = std::make_shared<UserInfo>("222", "张三", "123456", "12312412");
    userTb.insert(user2);
}

void updateById(UserTable &userTb)
{
    auto user = userTb.selectByUserId("111");
    user->desc("hello");
    userTb.update(user);
}
void updateByPhone(UserTable &userTb)
{
    auto user = userTb.selectByPhone("123123124");
    user->password("22223333");
    userTb.update(user);
}
void selectByNickname(UserTable &userTb)
{
    auto user = userTb.selectByNickname("张三");
    for(int i = 0; i < user.size(); ++i)
    {
        std::cout << user[i].userId() << std::endl;
    }
}
void select_users(UserTable &userTb)
{
    std::vector<std::string> ids = {"111", "222"};
    auto res = userTb.selectUsersByUserId(ids);
    for (auto user : res)
    {
        std::cout << user.phone() << std::endl;
    }
}

int main()
{
    auto db = ODBFactory::create("root", "111111", "127.0.0.1", "chatOnline", "utf8", 3306, 1);

    UserTable user(db);

    // insert(user);
    // updateById(user);
    // updateByPhone(user);
    // selectByNickname(user);
     select_users(user);
    return 0;
}