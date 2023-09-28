#pragma once

#include <iostream>
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <string>
#include <cstring>

// 日志是有日志级别的
enum LOG_EVEL
{
    NORMAL = 1,  // 正常
    WARNING = 2, // 警告
    ERROR = 3,   // 错误
    FATAL = 5   // 致命
};

#define LOG(level, message, ...) Log(level, __FILE__, __LINE__, message, __VA_ARGS__)

const char *gLevelMap[] = {
    "DEBUG",
    "NORMAL",
    " ! WARNING",
    " !! ERROR",
    " !!! FATAL"};

#define LOGFILE "./httpserver.log"

// 输出：日志级别 时间戳 日志信息 错误文件名称 行数
void Log(int level, std::string file_name, const int line, std::string format, ...)
{
    va_list args;                     // c可变参数对象
    char stdBuffer[1024];             // 标准部分缓冲区
    char logBuffer[1024];             // 自定义部分缓冲区
    time_t timestamp = time(nullptr); // 获取时间戳

    // 将format后面的可变参数输入到args中
    va_start(args, format);

    //  将错误等级和时间戳，连接并打印到标准部分缓冲区
    snprintf(stdBuffer, sizeof stdBuffer, "[%s] [%s] [%d] [%ld] ", gLevelMap[level], file_name.c_str(), line, timestamp);

    // 将format和可变参数，连接并打印到自定义部分缓冲区
    vsnprintf(logBuffer, sizeof logBuffer, format.c_str(), args);
    va_end(args);

    // 输出日志到标准输出
    printf("%s%s\n", stdBuffer, logBuffer);

    ////输出日志到日志文件中
    // FILE *fp = fopen(LOGFILE, "a");
    // fprintf(fp, "%s%s\n", stdBuffer, logBuffer);
    // fclose(fp);
}