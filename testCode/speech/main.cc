#include "../../common/speechApi/speech.h"

void asr(aip::Speech* client)
{
    // 无可选参数调用接口
    std::string file_content;
    aip::get_file_content("./16k.pcm", &file_content);
    Json::Value result = client->recognize(file_content, "pcm", 16000, aip::null);
    if(result["err_no"].asInt() != 0)
    {
        std::cout << result["err_msg"].asString() << std::endl;
    }

    std::cout << result["result"][0].asString() << std::endl;
}

int main()
{
    std::string app_id = "115537914";
    std::string api_key = "QvYPb4iYSq52A688Nh0BaAqM";
    std::string secret_key = "HWyPo7QGc26dIofx5gDVMI7NClzaP3lT";

    aip::Speech client(app_id, api_key, secret_key);

    asr(&client);

    return 0;
}