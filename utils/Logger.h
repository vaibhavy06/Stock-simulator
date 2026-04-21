#pragma once
#include <string>
#include <fstream>
#include <iostream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>
#include "MinGWThreadFix.h"

#include <chrono>
#include "MinGWThreadFix.h"

#ifdef ERROR
#undef ERROR
#endif

enum class LogLevel { DBG, INF, WRN, ERR };

class Logger {
public:
    static Logger& instance() {
        static Logger inst;
        return inst;
    }

    void init(const std::string& filename, LogLevel minLevel = LogLevel::INF) {
        std::lock_guard<std::mutex> lock(mutex_);
        if (fileOut_.is_open()) fileOut_.close();
        fileOut_.open(filename, std::ios::out | std::ios::trunc);
        minLevel_ = minLevel;
    }

    void log(LogLevel level, const std::string& msg) {
        if (level < minLevel_) return;
        std::lock_guard<std::mutex> lock(mutex_);
        std::string entry = "[" + timestamp() + "] [" + levelStr(level) + "] " + msg;
        std::cout << entry << "\n";
        if (fileOut_.is_open()) { fileOut_ << entry << "\n"; fileOut_.flush(); }
    }

    void info (const std::string& msg) { log(LogLevel::INF,  msg); }
    void warn (const std::string& msg) { log(LogLevel::WRN,  msg); }
    void error(const std::string& msg) { log(LogLevel::ERR, msg); }
    void debug(const std::string& msg) { log(LogLevel::DBG, msg); }

    ~Logger() { if (fileOut_.is_open()) fileOut_.close(); }

private:
    Logger() = default;
    std::mutex   mutex_;
    std::ofstream fileOut_;
    LogLevel     minLevel_ = LogLevel::INF;

    static std::string timestamp() {
        auto now = std::chrono::system_clock::now();
        auto t   = std::chrono::system_clock::to_time_t(now);
        std::ostringstream oss;
        oss << std::put_time(std::localtime(&t), "%H:%M:%S");
        return oss.str();
    }

    static std::string levelStr(LogLevel l) {
        switch (l) {
            case LogLevel::DBG: return "DEBUG";
            case LogLevel::INF:  return "INFO ";
            case LogLevel::WRN:  return "WARN ";
            case LogLevel::ERR: return "ERROR";
        }
        return "?????";
    }
};

// Convenience macro
#define LOG_INFO(msg)  Logger::instance().info(msg)
#define LOG_WARN(msg)  Logger::instance().warn(msg)
#define LOG_ERROR(msg) Logger::instance().error(msg)
#define LOG_DEBUG(msg) Logger::instance().debug(msg)
