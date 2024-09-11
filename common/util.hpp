#include <fstream>
#include <sstream>
#include <string>
#include <atomic>
#include <random>
#include <iomanip>

#include "log.hpp"

namespace hjb
{
    // 生成唯一id
    std::string uuid()
    {
        // 以设备随机数为种子实例化伪随机数对象
        std::random_device sd;
        std::mt19937 generator(sd());
        // 限定数据范围
        std::uniform_int_distribution<int> distribution(0, 255);
        // 生成6个数字
        std::stringstream ss;
        for (int i = 0; i < 6; i++)
        {
            if (i == 2)
                ss << "-";
            ss << std::setw(2) << std::setfill('0') << std::hex << distribution(generator);
        }
        ss << "-";
        // 通过一个静态变量生成一个2字节的编号数字
        static std::atomic<short> idx(0);
        short tmp = idx.fetch_add(1);
        ss << std::setw(4) << std::setfill('0') << std::hex << tmp;
        return ss.str();
    }

    // 文件读操作
    bool readFile(const std::string &filename, std::string &body)
    {
        // 读取文件的所有数据
        std::ifstream ifs(filename, std::ios::binary | std::ios::in);
        if (!ifs.is_open())
        {
            ERROR("打开文件 {} 失败！", filename);
            return false;
        }

        // 跳转到文件末尾
        ifs.seekg(0, std::ios::end); 
        // 获取当前偏移量
        size_t flen = ifs.tellg();
        // 跳转到文件起始
        ifs.seekg(0, std::ios::beg); 

        body.resize(flen);
        ifs.read(&body[0], flen);
        if (!ifs.good())
        {
            ERROR("读取文件 {} 数据失败！", filename);
            ifs.close();
            return false;
        }

        ifs.close();
        return true;
    }

    // 文件写操作
    bool writeFile(const std::string &filename, const std::string &body)
    {
        // 将body中的数据写入filename对应的文件中
        std::ofstream ofs(filename, std::ios::out | std::ios::binary | std::ios::trunc);
        if (!ofs.is_open())
        {
            ERROR("打开文件 {} 失败！", filename);
            return false;
        }

        ofs.write(body.c_str(), body.size());
        if (!ofs.good())
        {
            ERROR("写入文件 {} 数据失败！", filename);
            ofs.close();
            return false;
        }

        ofs.close();
        return true;
    }

}