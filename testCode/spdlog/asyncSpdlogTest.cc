////// 异步

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/async.h>
#include <iostream>

int main(int argc, char *argv[])
{
    // 设置全局刷新策略
    spdlog::flush_every(std::chrono::seconds(1)); // 1秒

    // 初始化线程配置
    spdlog::init_thread_pool(3072, 1);

    // 创建同步日志器（标准输出/文件）
    auto logger = spdlog::stdout_color_mt<spdlog::async_factory>("logger"); // 标准输出
    auto fileLogger = spdlog::basic_logger_mt<spdlog::async_factory>("fileLogger", "log.txt"); // 文件输出

    // 设置日志器刷新策略和日志器的输出登记
    logger->flush_on(spdlog::level::level_enum::debug); // 遇到debug等级立即刷新
    logger->set_level(spdlog::level::level_enum::debug);
    logger->set_pattern("[%n][%H:%M:%S][%t][%-8l] %v"); // 输出格式
    fileLogger->flush_on(spdlog::level::level_enum::debug); // 遇到debug等级立即刷新
    fileLogger->set_level(spdlog::level::level_enum::debug);
    fileLogger->set_pattern("[%n][%H:%M:%S][%t][%-8l] %v"); // 输出格式

    // 输出
    logger->trace("hello {}", "world"); // {}为占位符
    logger->info("hello {}", "world"); // {}为占位符
    logger->debug("hello {}", "world"); // {}为占位符
    logger->warn("hello {}", "world"); // {}为占位符
    fileLogger->trace("hello {}", "world"); // {}为占位符
    fileLogger->info("hello {}", "world"); // {}为占位符
    fileLogger->debug("hello {}", "world"); // {}为占位符
    fileLogger->warn("hello {}", "world"); // {}为占位符

    std::cout << "over" << std::endl;

    return 0;
}