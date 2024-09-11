#include "speech.h"
#include "log.hpp"

namespace hjb
{
    class SpeechClient
    {
    private:
        aip::Speech _client;

    public:
        using ptr = std::shared_ptr<SpeechClient>;

        SpeechClient(const std::string &appId,
                     const std::string &apiKey,
                     const std::string &secretKey)
            : _client(appId, apiKey, secretKey)
        {
        }

        // 识别请求
        std::string recognize(const std::string &data)
        {         
            // 请求
            Json::Value result = _client.recognize(data, "pcm", 16000, aip::null);
            if (result["err_no"].asInt() != 0)
            {
                ERROR("语音识别失败: {}", result["err_msg"].asString());
                return std::string();
            }

            return result["result"][0].asString();
        }
    };
}