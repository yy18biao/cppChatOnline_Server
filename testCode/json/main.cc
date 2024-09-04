#include <iostream>
#include <jsoncpp/json/json.h>
#include <string>
#include <sstream>
#include <memory>

bool serialize(const Json::Value &val, std::string &content)
{
    // 定义工厂类
    Json::StreamWriterBuilder swb;
    // 使用工厂类创建工具类
    std::unique_ptr<Json::StreamWriter> sw(swb.newStreamWriter());
    // 序列化
    std::stringstream ss;
    if(sw->write(val, &ss) != 0)
    {
        std::cout << "失败" << std::endl;
        return false;
    }

    content = ss.str();
    return true;
}

bool unSerialize(Json::Value &val, const std::string &content)
{
    // 定义工厂类
    Json::CharReaderBuilder crb;
    // 使用工厂类创建工具类
    std::unique_ptr<Json::CharReader> cr(crb.newCharReader());
    // 序列化
    std::stringstream ss;
    if(!cr->parse(content.c_str(), content.c_str() + content.size(), &val, &ss))
    {
        std::cout << "失败" << std::endl;
        return false;
    }

    content = ss.str();
    return true;
}

int main()
{
    char name[] = "zhangsan";
    int age = 18;
    float score[3] = {10, 100, 20};

    Json::Value stu;
    stu["姓名"] = name;
    stu["年龄"] = age;
    stu["成绩"].append(score[0]);
    stu["成绩"].append(score[1]);
    stu["成绩"].append(score[2]);

    // 序列化
    std::string content;
    if(!serialize(stu, content));
    std::cout << content << std::endl;

    // 反序列化
    Json::Value val;
    if(!unSerialize(val, content));
    std::cout << val["姓名"].asString() << std::endl;
    std::cout << val["年龄"].asInt() << std::endl;
    std::cout << val["成绩"][0].asFloat() << std::endl;
    std::cout << val["成绩"][1].asFloat() << std::endl;
    std::cout << val["成绩"][2].asFloat() << std::endl;

    return 0;
}