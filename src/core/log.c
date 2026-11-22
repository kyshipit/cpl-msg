/**
 * log实现
 * 打印时间、文件名行号，低于等级直接丢弃
 */
#include "core/log.h"
#include <stdarg.h>
#include <time.h>

LogLevel g_log_level = LOG_LEVEL_DEBUG;

// 修改全局日志等级
void log_set_level(LogLevel lvl)
{
    g_log_level = lvl;
}

// 输出日志内容
void log_write(LogLevel level, const char *file, int line, const char *fmt, ...)
{
    if (level < g_log_level)
    {
        return;
    }
    static const char *level_str[] = {
        "[DEBUG]",
        "[INFO] ",
        "[WARN] ",
        "[ERROR]"
    };
    va_list ap;
    va_start(ap, fmt);
    time_t now = time(NULL);
    struct tm *t = localtime(&now);

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
