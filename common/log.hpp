#pragma once

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>

/// 日志模块 ///

// 全局日志器
std::shared_ptr<spdlog::logger> logger;

// 初始化日志器
// 调试模式下标准输出，发布模式下文件输出
// 根据mode参数判断，true发布，false调试
void initLogger(bool mode, const std::string &filename, int32_t level)
{
    if(!mode)
    {
        // 输出等级设为最低，标准输出
        logger = spdlog::stdout_color_mt("logger");
        logger->flush_on(spdlog::level::level_enum::trace); // 遇到trace等级立即刷新
        logger->set_level(spdlog::level::level_enum::trace);
    }
    else
    {
        // 输出等级根据参数level决定，文件输出
        logger = spdlog::basic_logger_mt("logger", filename);
        logger->flush_on((spdlog::level::level_enum)level); // 遇到level等级立即刷新
        logger->set_level((spdlog::level::level_enum)level);
    }

    // 设置输出格式
    logger->set_pattern("[%n][%H:%M:%S][%t][%-8l]%v");
}


// 使用宏调用日志器实现文件和行号的显示
#define DEBUG(format, ...) logger->debug(std::string("[{}:{}] ") + format, __FILE__, __LINE__, ##__VA_ARGS__);
#define INFO(format, ...) logger->info(std::string("[{}:{}] ") + format, __FILE__, __LINE__, ##__VA_ARGS__);
#define WARN(format, ...) logger->warn(std::string("[{}:{}] ") + format, __FILE__, __LINE__, ##__VA_ARGS__);
#define ERROR(format, ...) logger->error(std::string("[{}:{}] ") + format, __FILE__, __LINE__, ##__VA_ARGS__);
#define CRITICAL(format, ...) logger->critical(std::string("[{}:{}] ") + format, __FILE__, __LINE__, ##__VA_ARGS__);