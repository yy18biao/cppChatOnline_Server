#pragma once

#include <iostream>
#include <string>
#include <cstddef>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <odb/nullable.hxx>
#include <odb/core.hxx>

#pragma db object
class Student
{
private:
    friend class odb::access; // 设置为友元类才可以让odb进行访问

#pragma db id auto // 设置主键和自增长
    unsigned long _id;

#pragma db unique // 设置唯一键
    unsigned long _num;

    std::string _name;

    odb::nullable<unsigned short> _age; // 设置可为空

#pragma db index // 创建普通索引
    unsigned long _classId;

public:
    Student(){}

    Student(unsigned long num, const std::string &name, unsigned short age, unsigned long classId)
        : _num(num), _name(name), _age(age), _classId(classId)
    {
    }

    // 各个成员的访问与设置接口
    void num(unsigned long n) { _num = n; }
    unsigned long num() { return _num; }

    void name(const std::string &name) { _name = name; }
    std::string name() { return _name; }

    void age(unsigned short age) { _age = age; }
    odb::nullable<unsigned short> age() { return _age; }

    void classId(unsigned long classId) { _classId = classId; }
    unsigned long classId() { return _classId; }
};

#pragma db object
class Class
{
private:
    friend class odb::access; // 设置为友元类才可以让odb进行访问

#pragma db id auto // 设置主键和自增长
    unsigned long _id;

#pragma db unique // 设置唯一键
    std::string _className;

public:
    Class(){}

    Class(const std::string &className)
        : _className(className)
    {
    }

    // 各个成员的访问与设置接口
    void className(const std::string &className) { _className = className; }
    std::string className() { return _className; }
};

// 创建一个两个表组合的视图，方便查看信息
#pragma db view object(Student) object(Class : Student::_classId == Class::_id) // 给Class类起别名，并将两个表结合
struct class_student
{
    #pragma db column(Student::_id) // 声明该属性对应的是哪个表中的哪个数据
    unsigned long _id;

    #pragma db column(Student::_num) // 声明该属性对应的是哪个表中的哪个数据
    unsigned long _num;

    #pragma db column(Student::_name) // 声明该属性对应的是哪个表中的哪个数据
    std::string _name;

    #pragma db column(Student::_age) // 声明该属性对应的是哪个表中的哪个数据
    odb::nullable<unsigned short> _age; // 设置可为空

    #pragma db column(Class::_className) // 声明该属性对应的是哪个表中的哪个数据
    std::string _className;
};

// 创建一个只查询学生姓名的查询函数
// (?) 表示外部调用时传入的过滤条件
#pragma db view query("select name from Student " + (?))
struct allName
{
    std::string _name;
};
