/**
 * @file log.c
 * @brief 日志模块实现，可变参数va_list
 */
#include "log.h"
#include <stdarg.h>
#include <time.h>

// 全局日志等级，默认DEBUG
LogLevel g_log_level = LOG_LEVEL_DEBUG;

// 日志等级前缀
static const char *level_str[] = {
    "[DEBUG]",
    "[INFO] ",
    "[WARN] ",
    "[ERROR]"
};

void log_write(LogLevel level, const char *file, int line, const char *fmt, ...)
{
    // 等级过滤：低于设定等级直接丢弃
    if (level < g_log_level)
    {
        return;
    }

    // 【学习点】va_list 可变参数处理，C标准可变参数
    va_list ap;
    va_start(ap, fmt);

    // 获取时间（简单本地时间）
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

    // 打印：时间 + 等级 + 文件:行号 + 内容
    fprintf(stdout,
        "%04d-%02d-%02d %02d:%02d:%02d %s %s:%d | ",
        t->tm_year + 1900, t->tm_mon + 1, t->tm_mday,
        t->tm_hour, t->tm_min, t->tm_sec,
        level_str[level],
        file, line
    );
    vfprintf(stdout, fmt, ap);
    fprintf(stdout, "\n");

    va_end(ap);
}
