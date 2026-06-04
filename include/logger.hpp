#pragma once

#include <iostream>
#include <string>
#include <ctime>
#include <chrono>
#include <cstdio>
#include <cstdarg>
#include <memory>

// 1. 定义日志级别枚举
// 【关键修改】不要直接使用 DEBUG, INFO 等单词，加上前缀 LVL_ 以避免与系统宏冲突
enum class LogLevel {
    LVL_DEBUG = 0,
    LVL_INFO,
    LVL_WARNING,
    LVL_ERROR
};

class Logger {
public:
    // 设置日志级别的函数
    static void setLevel(LogLevel level) {
        currentLevel = level;
    }

    // 获取时间字符串的辅助函数
    static std::string getCurrentTime() {
        auto now = std::chrono::system_clock::now();
        auto time_t_now = std::chrono::system_clock::to_time_t(now);

        struct tm timeInfo;
        // Windows 平台使用 localtime_s (MSVC 与 MinGW 均支持)
        // Linux/macOS 平台使用 localtime_r
#if defined(_WIN32)
        localtime_s(&timeInfo, &time_t_now);
#else
        localtime_r(&time_t_now, &timeInfo);
#endif

        char buffer[80];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeInfo);
        return std::string(buffer);
    }

    // 通用的日志打印函数 (支持 printf 风格的格式化)
    // 第一个参数为格式字符串，后续参数为可变参数
    static void log(LogLevel level, const char* fmt, ...) {
        if (level >= currentLevel) {
            std::string levelStr;
            switch (level) {
                case LogLevel::LVL_DEBUG:   levelStr = "[DEBUG]"; break;
                case LogLevel::LVL_INFO:    levelStr = "[INFO]"; break;
                case LogLevel::LVL_WARNING: levelStr = "[WARNING]"; break;
                case LogLevel::LVL_ERROR:   levelStr = "[ERROR]"; break;
                default:                    levelStr = "[UNKNOWN]"; break;
            }

            // 解析 printf 风格的格式化字符串
            char formatted[2048] = {0};
            va_list args;
            va_start(args, fmt);
            std::vsnprintf(formatted, sizeof(formatted), fmt, args);
            va_end(args);

            std::cout << getCurrentTime() << " " << levelStr << " " << formatted << std::endl;
        }
    }

    // 便捷函数
    static void debug(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        char formatted[2048] = {0};
        std::vsnprintf(formatted, sizeof(formatted), fmt, args);
        va_end(args);
        log(LogLevel::LVL_DEBUG, "%s", formatted);
    }

    static void info(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        char formatted[2048] = {0};
        std::vsnprintf(formatted, sizeof(formatted), fmt, args);
        va_end(args);
        log(LogLevel::LVL_INFO, "%s", formatted);
    }

    static void warning(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        char formatted[2048] = {0};
        std::vsnprintf(formatted, sizeof(formatted), fmt, args);
        va_end(args);
        log(LogLevel::LVL_WARNING, "%s", formatted);
    }

    static void error(const char* fmt, ...) {
        va_list args;
        va_start(args, fmt);
        char formatted[2048] = {0};
        std::vsnprintf(formatted, sizeof(formatted), fmt, args);
        va_end(args);
        log(LogLevel::LVL_ERROR, "%s", formatted);
    }

private:
    static LogLevel currentLevel;
};

// 初始化静态成员变量
inline LogLevel Logger::currentLevel = LogLevel::LVL_DEBUG;

// 兼容旧代码中使用的 LOG_INFO / LOG_WARN / LOG_ERROR / LOG_DEBUG 宏
// 这些宏使用 printf 风格的格式化字符串
#define LOG_DEBUG(fmt, ...) ::Logger::debug(fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)  ::Logger::info(fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  ::Logger::warning(fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) ::Logger::error(fmt, ##__VA_ARGS__)