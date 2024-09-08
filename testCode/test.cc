#include <gflags/gflags.h>
#include "../common/elasticlient.hpp"

DEFINE_bool(mode, false, "日志器运行模式");
DEFINE_string(filename, "", "日志器输出文件名");
DEFINE_int32(level, 1, "日志器输出最低等级");
DEFINE_string(etcdHost, "http://127.0.0.1:2379", "服务注册中心地址");
DEFINE_string(serviceBasedir, "/service", "注册监控key值根目录");
DEFINE_string(serviceInstance, "/userInfo", "注册监控key值根目录");

int main(int argc, char* argv[])
{
    google::ParseCommandLineFlags(&argc, &argv, true);
    initLogger(FLAGS_mode, FLAGS_filename, FLAGS_level);

    std::shared_ptr<elasticlient::Client> client(new elasticlient::Client({"http://127.0.0.1:9200/"}));

    ESIndex es(client, "user", "_doc");
    es.append("nickname", "text").append("phone", "keyword", "ik_max_word").create();
    std::cout << "index success\n";

    // 数据新增与数据修改 一样的操作 id一样就是修改
    ESInsert ess(client, "user", "_doc");
    ess.append("nickname", "zhangsan").append("phone", "1111111").insert("00001");
    std::cout << "insert success\n";

    // 检索
    ESSearch esss(client, "user", "_doc");
    Json::Value user = esss.appendMustNot("nickname", {"wangwu"}).search();
    if(user.empty() || !user.isArray())
    {
        ERROR("检索出的数据出错");
        return -1;
    }
    int sz = user.size();
    for(int i = 0; i < sz; ++i)
    {
        std::cout << user[i]["_source"]["nickname"].asString() << "\n";
    }
    std::cout << "Search success\n";

    // 删除
    ESRemove essss(client, "user", "_doc");
    essss.remove("00001");

}