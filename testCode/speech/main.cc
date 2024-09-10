#include "../../common/speechAsr.hpp"

int main()
{
    std::string app_id = "115537914";
    std::string api_key = "QvYPb4iYSq52A688Nh0BaAqM";
    std::string secret_key = "HWyPo7QGc26dIofx5gDVMI7NClzaP3lT";

    SpeechClient client(app_id, api_key, secret_key);

    client.recognize("16k.pcm");

    return 0;
}