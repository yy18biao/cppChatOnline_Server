#include <iostream>
#include <string>
#include <sstream>
#include <memory>
#include <elasticlient/client.h>
#include <cpr/cpr.h>
#include <jsoncpp/json/json.h>

#include "log.hpp"

bool serialize(const Json::Value &val, std::string &content)
{
    // 实例化一个 StreamWriteBuilder 对象
    Json::StreamWriterBuilder jsb;

    // 通过 StreamWriteBuilder 类对象生产一个 StreamWrite 对象
    std::unique_ptr<Json::StreamWriter> jsw(jsb.newStreamWriter());

    // 使用 StreamWrite 对象，对Json::Values中的数据序列化
    // 创建字符串流对象接收反序列化后的结果
    std::stringstream ss;
    if (jsw->write(val, &ss) != 0)
    {
        ERROR("序列化失败！");
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
    if (!jcb->parse(content.c_str(), content.c_str() + content.size(), &val, nullptr))
    {
        ERROR("反序列化失败！");
        return false;
    }

    return true;
}

// 创建索引类
class ESIndex
{
private:
    std::string _name;       // 索引名称
    std::string _type;       // 索引类型
    Json::Value _properties; // 请求Json类型字段中的自定义值
    Json::Value _index;      // 最终的请求Json类型字段
    std::shared_ptr<elasticlient::Client> _client; // es客户端对象

public:
    ESIndex(std::shared_ptr<elasticlient::Client>& client, const std::string &name, const std::string &type)
        : _name(name), _type(type), _client(client)
    {
        // 设置字段里的默认值
        Json::Value analysis;
        Json::Value analyzer;
        Json::Value ik;
        Json::Value tokenizer;
        tokenizer["tokenizer"] = "ik_max_word";
        ik["ik"] = tokenizer;
        analyzer["analyzer"] = ik;
        analysis["analysis"] = analyzer;
        _index["settings"] = analysis;
    }

    // 添加Json字段的内容
    // key为数据名称，type为数据类型，analyzer为分词器类型，enable为是否开启查询
    ESIndex append(const std::string &key,
                const std::string &type = "text",
                const std::string &analyzer = "ik_max_word",
                bool enable = true)
    {
        // 设置字段里的值
        Json::Value fields;
        fields["type"] = type;
        fields["analyzer"] = analyzer;
        if (!enable)
            fields["enable"] = enable;
        _properties[key] = fields;

        return *this;
    }

    // 创建最终字段并发起请求 
    bool create()
    {
        // 创建出最终的字段
        Json::Value mappings;
        mappings["dynamic"] = true;
        mappings["properties"] = _properties;
        _index["mappings"] = mappings;

        std::string body;
        if (!serialize(_index, body))
        {
            ERROR("索引序列化失败");
            return false;
        }

        std::cout << body << "\n";

        // 发起搜索请求
        try
        {
            cpr::Response resp = _client->index(_name, _type, "", body);

            if(resp.status_code < 200 || resp.status_code >= 300)
            {
                ERROR("创建ES索引{}失败，响应状态码异常", _name);
                return false;
            }
        }
        catch (std::exception &e)
        {
            ERROR("创建ES索引{}失败: {}", _name, e.what());
            return false;
        }

        return true;
    }
};