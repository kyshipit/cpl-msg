/**
 * @file log.h
 * @brief 日志模块：可变参数宏、C11 _Generic泛型打印、DEBUG条件编译
 * @learning 可变参数宏 __VA_ARGS__、#字符串化、##拼接、C11 _Generic
 * _Generic 表达式不能直接和字符串字面量在预处理阶段拼接，新手高频报错
 */
#ifndef LOG_H
#define LOG_H

#include <stdio.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    LOG_LEVEL_DEBUG = 0,
    LOG_LEVEL_INFO,
    LOG_LEVEL_WARN,
    LOG_LEVEL_ERROR
} LogLevel;

extern LogLevel g_log_level;

void log_write(LogLevel level, const char *file, int line, const char *fmt, ...);

/**
 * @brief LOG_VAR：自动识别变量类型打印变量名+值
 * C11 _Generic 编译期类型分支
 * 用法：LOG_VAR(seq, seq);
 */
#define LOG_VAR(name, val)                                                              \
    _Generic((val),                                                                     \
        char:                log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, #name " = %c", val), \
        unsigned char:       log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, #name " = %u", val), \
        short:               log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, #name " = %hd", val),\
        unsigned short:      log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, #name " = %hu", val),\
        int:                 log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, #name " = %d", val), \
        unsigned int:        log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, #name " = %u", val), \
        long:                log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, #name " = %ld", val),\
        unsigned long:       log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, #name " = %lu", val),\
        long long:           log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, #name " = %lld", val),\
        unsigned long long:  log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, #name " = %llu", val),\
        float:               log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, #name " = %.4f", val),\
        double:              log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, #name " = %.4lf", val),\
        const char*:         log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, #name " = %s", val),\
        char*:               log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, #name " = %s", val),\
        void*:               log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, #name " = %p", val),\
        default:             log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, #name " = <unknown-type>")\
    )

#ifdef DEBUG
#define LOG_DEBUG(fmt, ...)  log_write(LOG_LEVEL_DEBUG, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...)   log_write(LOG_LEVEL_INFO,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)   log_write(LOG_LEVEL_WARN,  __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...)  log_write(LOG_LEVEL_ERROR, __FILE__, __LINE__, fmt, ##__VA_ARGS__)
#else
#define LOG_DEBUG(fmt, ...)  ((void)0)
#define LOG_INFO(fmt, ...)   ((void)0)
#define LOG_WARN(fmt, ...)   ((void)0)
#define LOG_ERROR(fmt, ...)  ((void)0)
#endif

#ifdef __cplusplus
}
#endif

#endif
