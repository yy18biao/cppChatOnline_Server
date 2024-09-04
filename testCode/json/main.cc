#include <iostream>
#include <jsoncpp/json/json.h>
#include <string>
#include <sstream>
#include <memory>

bool serialize(const Json::Value &val, std::string &content)
{
    // 实例化一个 StreamWriteBuilder 对象
    Json::StreamWriterBuilder jsb;

    // 通过 StreamWriteBuilder 类对象生产一个 StreamWrite 对象
    std::unique_ptr<Json::StreamWriter> jsw(jsb.newStreamWriter());

    // 使用 StreamWrite 对象，对Json::Values中的数据序列化
    // 创建字符串流对象接收反序列化后的结果
    std::stringstream ss;
    if(jsw->write(val, &ss) != 0)
    {
        std::cout << "失败" << std::endl;
        return false;
    }

    content = ss.str();
    return true;
}

bool unSerialize(Json::Value &val, const std::string &content)
{
    // 实例化一个CharReaderBuilder工厂类对象
        Json::CharReaderBuilder jcr;

    // 使用CharReaderBuilder工厂类生产一个CharReader对象
    std::unique_ptr<Json::CharReader> jcb(jcr.newCharReader());

    // 使用CharReader对象进行json格式字符串str的反序列化
    if(!jcb->parse(content.c_str(), content.c_str() + content.size(), &val, nullptr))
    {
        std::cout << "失败" << std::endl;
        return false;
    }

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