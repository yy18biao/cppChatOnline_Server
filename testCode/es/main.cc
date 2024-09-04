#include <iostream>
#include <string>
#include <elasticlient/client.h>
#include <cpr/cpr.h>


int main()
{
    // 构造es客户端
    elasticlient::Client client({"http://127.0.0.1:9200/"});

    // 发起搜索请求
    try
    {
        cpr::Response resp = client.search("user", "_doc", "{\"query\": { \"match_all\":{} }}");

        // 查看响应
        std::cout << resp.status_code << "\n";
        std::cout << resp.text << "\n";
    } catch(std::exception &e)
    {
        std::cout << e.what() << "\n";
        return -1;
    }
   
}