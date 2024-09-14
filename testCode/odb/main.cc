#include <string>
#include <memory>
#include <cstdlib>
#include <iostream>
#include <odb/database.hxx>
#include <odb/mysql/database.hxx>
#include <gflags/gflags.h>
#include "student.hxx"
#include "student-odb.hxx"

DEFINE_string(host, "127.0.0.1", "mysql服务器地址");
DEFINE_int32(port, 3306, "mysql服务器端口");
DEFINE_string(db, "test", "mysql默认库");
DEFINE_string(user, "root", "mysql用户名");
DEFINE_string(pwd, "111111", "mysql密码");
DEFINE_string(charset, "utf8", "mysql客户端字符集");
DEFINE_int32(maxPool, 3, "mysql连接池最大连接数");

int main(int argc, char *argv[])
{
    google::ParseCommandLineFlags(&argc, &argv, true);

    // 构造连接池工厂配置对象
    std::unique_ptr<odb::mysql::connection_pool_factory> cpf(
        new odb::mysql::connection_pool_factory(FLAGS_maxPool, 0));

    // 构造数据库操作对象
    // 连接池工厂配置对象的传参需要传右值
    odb::mysql::database db(FLAGS_user, FLAGS_pwd, FLAGS_db, FLAGS_host, FLAGS_port, "", FLAGS_charset, 0, std::move(cpf));

    try
    {
        // 获取事务操作对象(自动开启)
        odb::transaction trans(db.begin());

        // 新增数据操作
        Class c1("C++");
        Class c2("java");
        db.persist(c1);
        db.persist(c2);
        Student s1(2021, "zhangsan", 20, 1);
        Student s2(2023, "wangwu", 20, 2);
        db.persist(s1);
        db.persist(s2);

        // 查询全部数据操作
        odb::result<Student> r(db.query<Student>());
        if(r.size() < 0)
        {
            std::cout << "没有查询结果" << std::endl;
            return -1;
        }
        auto it = r.begin();
        while (it != r.end())
        {
            std::cout << it->name() << "\n";
            ++it;
        }

        // 查询指定数据操作
        odb::result<Student> rr(db.query<Student>(odb::query<Student>::id == 1));
        if(rr.size() != 1)
        {
            std::cout << "查询结果不唯一" << std::endl;
            return -1;
        }
        Student s = *rr.begin();
        std::cout << s.name() << "\n";

        // 根据视图多表查询
        odb::result<struct class_student> rrr(db.query<struct class_student>());
        if(rrr.size() < 0)
        {
            std::cout << "没有查询结果" << std::endl;
            return -1;
        }
        for(auto it = rrr.begin(); it != rrr.end(); ++it)
        {
            std::cout << it->_name << "\n";
        }

        // 修改数据操作(需要先查询出该对象才可以进行修改)
        s.name("zhaoliu");
        db.update(s);

        // 删除数据操作(可以直接删除，也可以查询出来后再删除)
        // 支持多行删除，一样的操作
        db.erase_query<Student>(odb::query<Student>::id == 1);

        // 提交事务
        trans.commit();
    }
    catch (std::exception &e)
    {
        std::cout << e.what() << std::endl;
    }

    return 0;
}