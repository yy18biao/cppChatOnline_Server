#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <iostream>

int main(int argc, char *argv[])
{
    // 设置全局刷新策略
    spdlog::flush_every(std::chrono::seconds(1)); // 1秒

    // 创建同步日志器（标准输出/文件）
    auto logger = spdlog::stdout_color_mt("logger"); // 标准输出
    auto fileLogger = spdlog::basic_logger_mt("fileLogger", "log.txt"); // 文件输出

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

    return 0;
}

// namespace level {
//  enum level_enum : int {
//      trace = SPDLOG_LEVEL_TRACE,
//      debug = SPDLOG_LEVEL_DEBUG,
//      info = SPDLOG_LEVEL_INFO,
//      warn = SPDLOG_LEVEL_WARN,
//      err = SPDLOG_LEVEL_ERROR,
//      critical = SPDLOG_LEVEL_CRITICAL,
//      off = SPDLOG_LEVEL_OFF,
//      n_levels
//  };
// }

// logger->set_pattern("%Y-%m-%d %H:%M:%S [%t] [%-7l] %v");
// %t - 线程 ID（Thread ID）
// %n - 日志器名称（Logger name）
// %l - 日志级别名称（Level name），如 INFO, DEBUG, ERROR 等
// %v - 日志内容（message）
// %Y - 年（Year）
// %m - 月（Month）
// %d - 日（Day）
// %H - 小时（24-hour format）
// %M - 分钟（Minute）
// %S - 秒（Second）

// namespace spdlog {
//     class logger {
//         logger(std::string name);
//         logger(std::string name, sink_ptr single_sink);
//         logger(std::string name, sinks_init_list sinks);
//         void set_level(level::level_enum log_level);
//         void set_formatter(std::unique_ptr<formatter> f);
//         template<typename... Args>
//         void trace(fmt::format_string<Args...> fmt, Args &&...args);
//         template<typename... Args>
//         void debug(fmt::format_string<Args...> fmt, Args &&...args);
//         template<typename... Args>
//         void info(fmt::format_string<Args...> fmt, Args &&...args);
//         template<typename... Args>
//         void warn(fmt::format_string<Args...> fmt, Args &&...args);
//         template<typename... Args>
//         void error(fmt::format_string<Args...> fmt, Args &&...args);
//         template<typename... Args>
//         void critical(fmt::format_string<Args...> fmt, Args &&...args);

//         void flush(); //刷新日志
//         //策略刷新--触发指定等级日志的时候立即刷新日志的输出
//         void flush_on(level::level_enum log_level);
//     };
// }

// 异步日志输出
// class async_logger final : public logger {
//     async_logger(std::string logger_name,
//     sinks_init_list sinks_list,
//     std::weak_ptr<details::thread_pool> tp,
//     async_overflow_policy overflow_policy =
//     async_overflow_policy::block);
//     async_logger(std::string logger_name,
//     sink_ptr single_sink,
//     std::weak_ptr<details::thread_pool> tp,
//     async_overflow_policy overflow_policy =
//     async_overflow_policy::block);

//     // 异步日志输出需要异步工作线程的支持，这里是线程池类
//     class SPDLOG_API thread_pool {
// thread_pool(size_t q_max_items,
// size_t threads_n,
// std::function<void()> on_thread_start,
// std::function<void()> on_thread_stop);
// thread_pool(size_t q_max_items, size_t threads_n,
// std::function<void()> on_thread_start);
// thread_pool(size_t q_max_items, size_t threads_n);
//     };
//  }
// std::shared_ptr<spdlog::details::thread_pool> thread_pool() {
//     return details::registry::instance().get_tp();
// }
// //默认线程池的初始化接口
// inline void init_thread_pool(size_t q_size, size_t thread_count)
// auto async_logger = spdlog::async_logger_mt("async_logger", "logs/async_log.txt");
// async_logger->info("This is an asynchronous info message");