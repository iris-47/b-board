#pragma once

#include <string>
#include <sstream>
#include <fstream>
#include <memory>
#include <mutex>
#include <vector>
#include <chrono>
#include <iostream>
#include <iomanip>

namespace core {

enum class LogLevel {
    TRACE,
    DEBUG,
    INFO,
    WARN,
    ERROR,
    FATAL
};

// 简单字符串格式化函数
template<typename... Args>
std::string formatString(const std::string& format, Args&&... args) {
    // 笔记：获取格式化字符串长度的技巧，+ 1是为了给'\0'留空间
    size_t size = snprintf(nullptr, 0, format.c_str(), std::forward<Args>(args)...) + 1;
    // 笔记：使用char[]而不是char来特化，确保正确调用delete[]
    std::unique_ptr<char[]> buf(new char[size]);
    snprintf(buf.get(), size, format.c_str(), std::forward<Args>(args)...);
    return std::string(buf.get(), buf.get() + size - 1);
}

// 格式化时间为字符串
std::string timeToString(const std::chrono::system_clock::time_point& time);

// 日志类
class Logger {
public:
    static Logger& instance();
    
    // 设置日志级别
    void setLevel(LogLevel level) { level_ = level; }
    
    // 获取日志级别
    LogLevel getLevel() const { return level_; }
    
    // 设置日志文件
    void setLogFile(const std::string& filename);
    
    // 关闭日志文件
    void closeLogFile();
    
    // 写日志
    template<typename... Args>
    void log(LogLevel level, const char* file, int line, const char* func, 
             const char* fmt, Args&&... args) {
        if (level < level_) {
            return;
        }
        
        try {
            // 格式化消息
            // 笔记：根据传入的参数，决定将参数以左值引用还是右值引用的方式进行转发（保留原有的左值右值属性）
            std::string msg = formatString(fmt, std::forward<Args>(args)...);
            
            // 获取时间
            auto now = std::chrono::system_clock::now();
            std::string time_str = timeToString(now);
            
            // 获取级别字符串
            const char* level_str = levelToString(level);
            
            // 格式化日志
            std::ostringstream log_stream;
            log_stream << std::left
                << "[" << time_str << "] [" << level_str << "] "
                << file << ":" << line << " ("
                << func <<  ") "
                << msg << "\n";
            std::string log_msg = log_stream.str();
            
            // 写日志
            std::lock_guard<std::mutex> lock(mutex_);

            // 输出到日志文件，如没有指定日志文件，输出到stdout
            if (output_file_.is_open()) {
                output_file_ << log_msg;
                output_file_.flush();
            }else{
                std::cout << log_msg;
            }
            
            // 如果是FATAL级别，直接退出程序
            if (level == LogLevel::FATAL) {
                std::abort();
            }
        } catch (const std::exception& e) {
            std::cerr << "Logger error: " << e.what() << std::endl;
        }
    }
    
private:
    Logger();
    ~Logger();
    
    // 获取级别字符串
    const char* levelToString(LogLevel level) const;
    
    LogLevel level_;                   // 日志级别
    std::mutex mutex_;                 // 互斥锁
    std::ofstream output_file_;        // 输出文件
};

// 日志宏
// 笔记：当可变参数部分（__VA_ARGS__）为空时，##__VA_ARGS__ 会自动移除其前面的逗号，避免因多余逗号导致编译错误。
// ##__VA_ARGS__ 是 GNU 扩展语法（如 GCC、Clang 支持），但并非 C/C++ 标准的一部分。
#define LOG_TRACE(fmt, ...) \
    core::Logger::instance().log(core::LogLevel::TRACE, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define LOG_DEBUG(fmt, ...) \
    core::Logger::instance().log(core::LogLevel::DEBUG, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define LOG_INFO(fmt, ...) \
    core::Logger::instance().log(core::LogLevel::INFO, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define LOG_WARN(fmt, ...) \
    core::Logger::instance().log(core::LogLevel::WARN, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define LOG_ERROR(fmt, ...) \
    core::Logger::instance().log(core::LogLevel::ERROR, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

#define LOG_FATAL(fmt, ...) \
    core::Logger::instance().log(core::LogLevel::FATAL, __FILE__, __LINE__, __func__, fmt, ##__VA_ARGS__)

} // namespace core
