#include "logger.h"
#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

Logger& Logger::get_instance() {
    static Logger instance;
    return instance;
}

Logger::Logger() : log_level_(INFO), is_async_(false), exit_flag_(false) {
    log_file_.open("server.log", std::ios::out | std::ios::app);
    if (!log_file_.is_open()) {
        std::cerr << "Failed to open log file." << std::endl;
    }
}

Logger::~Logger() {
    if (is_async_) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            exit_flag_ = true;
            cond_.notify_all();
        }
        if (write_thread_.joinable()) {
            write_thread_.join();
        }
    }
    if (log_file_.is_open()) {
        log_file_.close();
    }
}

void Logger::set_level(LogLevel level) {
    log_level_ = level;
}

void Logger::set_async(bool is_async) {
    is_async_ = is_async;
    if (is_async_ && !write_thread_.joinable()) {
        write_thread_ = std::thread(&Logger::async_write, this);
    }
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < log_level_) {
        return;
    }

    // 获取当前时间
    auto now = std::chrono::system_clock::now();
    std::time_t now_time = std::chrono::system_clock::to_time_t(now);
    std::tm* tm_now = std::localtime(&now_time);

    // 格式化时间和日志级别
    std::ostringstream oss;
    oss << "[" << std::put_time(tm_now, "%Y-%m-%d %H:%M:%S") << "] ";

    switch (level) {
        case DEBUG:
            oss << "[DEBUG] ";
            break;
        case INFO:
            oss << "[INFO ] ";
            break;
        case WARN:
            oss << "[WARN ] ";
            break;
        case ERROR:
            oss << "[ERROR] ";
            break;
    }

    oss << message << std::endl;
    std::string log_entry = oss.str();

    if (is_async_) {
        // 异步模式，将日志加入队列
        {
            std::lock_guard<std::mutex> lock(mtx_);
            log_queue_.push(log_entry);
            cond_.notify_one();
        }
    } else {
        // 同步模式，直接写日志
        std::lock_guard<std::mutex> lock(mtx_);
        if (log_file_.is_open()) {
            log_file_ << log_entry;
            log_file_.flush();
        }
        // 同时输出到控制台
        std::cout << log_entry;
    }
}

void Logger::async_write() {
    while (true) {
        std::unique_lock<std::mutex> lock(mtx_);
        cond_.wait(lock, [this]() { return !log_queue_.empty() || exit_flag_; });

        while (!log_queue_.empty()) {
            const std::string& log_entry = log_queue_.front();
            if (log_file_.is_open()) {
                log_file_ << log_entry;
                log_file_.flush();
            }
            // 同时输出到控制台
            std::cout << log_entry;
            log_queue_.pop();
        }

        if (exit_flag_) {
            break;
        }
    }
}
