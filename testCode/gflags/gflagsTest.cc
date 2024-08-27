#include <iostream>
#include <gflags/gflags.h>

DEFINE_string(ip, "127.0.0.1", "服务器监听地址");
DEFINE_int32(port, 8000, "服务器监听端口");
DEFINE_bool(debug_enable, true, "是否启动调试模式");

int main(int argc, char* argv[]){
    google::ParseCommandLineFlags(&argc, &argv, true);

    std::cout << FLAGS_ip << " " << FLAGS_port << " " << FLAGS_debug_enable << std::endl;

    return 0;
}