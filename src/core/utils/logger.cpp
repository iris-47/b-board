#include "logger.h"
#include <iomanip>
#include <ctime>

namespace core {

std::string timeToString(const std::chrono::system_clock::time_point& time) {
    auto time_t = std::chrono::system_clock::to_time_t(time);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(time.time_since_epoch()) % 1000;
    
    std::stringstream ss;
    std::tm tm_buf;
    #ifdef _WIN32
    localtime_s(&tm_buf, &time_t);
    #else
    localtime_r(&time_t, &tm_buf);
    #endif
    
    ss << std::put_time(&tm_buf, "%Y-%m-%d %H:%M:%S") << '.' 
       << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

Logger::Logger()
    : level_(LogLevel::INFO) {
}

Logger::~Logger() {
    closeLogFile();
}

Logger& Logger::instance() {
    static Logger instance;
    return instance;
}

void Logger::setLogFile(const std::string& filename) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 关闭已经打开的文件
    closeLogFile();
    
    // 打开新文件
    output_file_.open(filename, std::ios::app);
    
    if (!output_file_.is_open()) {
        std::cerr << "Failed to open log file: " << filename << std::endl;
    }
}

void Logger::closeLogFile() {
    if (output_file_.is_open()) {
        output_file_.close();
    }
}

const char* Logger::levelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::FATAL: return "FATAL";
        default:              return "UNKNOWN";
    }
}

} // namespace core
