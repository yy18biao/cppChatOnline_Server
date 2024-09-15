#include "../../../common/esData.hpp"
#include <gflags/gflags.h>

DEFINE_bool(mode, false, "日志器运行模式");
DEFINE_string(filename, "", "日志器输出文件名");
DEFINE_int32(level, 1, "日志器输出最低等级");
DEFINE_string(host, "http://127.0.0.1:9200/", "es服务器URL");

int main(int argc, char *argv[])
{
    google::ParseCommandLineFlags(&argc, &argv, true);
    hjb::initLogger(FLAGS_mode, FLAGS_filename, FLAGS_level);

    auto client = ESFactory::create({FLAGS_host});

    auto esUser = std::make_shared<ESUser>(client);

    esUser->createIndex();

    esUser->appendData("1", "11", "张三", "199922", "hello", "902183890172");
    esUser->appendData("2", "22", "张三四", "1231234", "hi", "091284390");

    auto res = esUser->search("张", {"11"});
    for (auto &u : res) {
        std::cout << u.phone() << std::endl;
        std::cout << u.nickname() << std::endl;
        std::cout << *u.desc() << std::endl;
        std::cout << *u.userPhotoId() << std::endl;
    }
}