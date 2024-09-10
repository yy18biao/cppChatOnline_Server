#include "./speechApi/speech.h"
#include "log.hpp"

class SpeechClient
{
private:
    aip::Speech _client;

public:
    SpeechClient(const std::string &appId,
                 const std::string &apiKey,
                 const std::string &secretKey)
        : _client(appId, apiKey, secretKey)
    {
    }

    // 识别请求
    std::string recognize(const std::string &data)
    {
        // 无可选参数调用接口
        std::string file_content;
        aip::get_file_content(data.c_str(), &file_content);
        // 请求
        Json::Value result = _client.recognize(file_content, "pcm", 16000, aip::null);
        if (result["err_no"].asInt() != 0)
        {
            ERROR("语音识别失败: {}", result["err_msg"].asString());
            return std::string();
        }

        return result["result"][0].asString();
    }
};