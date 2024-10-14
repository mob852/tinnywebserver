#ifndef LOGGER_H
#define LOGGER_H

#include <string>
#include <mutex>
#include <queue>
#include <thread>
#include <condition_variable>
#include <fstream>
#include <iostream>

enum LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};

class Logger {
public:
    // 获取单例实例
    static Logger& get_instance();

    // 设置日志级别
    void set_level(LogLevel level);

    // 设置是否为异步日志
    void set_async(bool is_async);

    // 记录日志
    void log(LogLevel level, const std::string& message);

    // 禁用拷贝和赋值
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

private:
    Logger();  // 构造函数设为私有
    ~Logger();

    // 异步日志处理函数
    void async_write();

    // 日志级别
    LogLevel log_level_;

    // 是否异步
    bool is_async_;

    // 日志输出文件
    std::ofstream log_file_;

    // 线程安全相关
    std::mutex mtx_;
    std::condition_variable cond_;
    std::queue<std::string> log_queue_;
    std::thread write_thread_;
    bool exit_flag_;  // 标志位，通知线程退出
};

#define LOG_DEBUG(message) Logger::get_instance().log(DEBUG, message)
#define LOG_INFO(message) Logger::get_instance().log(INFO, message)
#define LOG_WARN(message) Logger::get_instance().log(WARN, message)
#define LOG_ERROR(message) Logger::get_instance().log(ERROR, message)

#endif  // LOGGER_H
