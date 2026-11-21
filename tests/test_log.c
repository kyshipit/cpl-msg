/**
 * @file test_log.c
 * @brief log模块单元测试：_Generic、可变参数宏、LOG_VAR泛型打印
 * 编译：gcc -std=c11 -Wall -Wextra -g -DDEBUG tests/test_log.c src/log/log.c -o test_log
 * 运行：./test_log
 */
#include "../src/log/log.h"
#include <stdio.h>

int main(void)
{
    printf("===== Test log module start =====\n");

    LOG_DEBUG("this is debug log");
    LOG_INFO("this is info log");
    LOG_WARN("this is warn log");
    LOG_ERROR("this is error log");

    int seq = 1001;
    uint32_t body_len = 8;
    const char *msg = "hello minimsg";
    double pi = 3.1415926;

    // 【学习点】LOG_VAR 使用 _Generic 自动识别类型
    LOG_VAR(seq, seq);
    LOG_VAR(body_len, body_len);
    LOG_VAR(msg, msg);
    LOG_VAR(pi, pi);

    // 修改全局日志等级，屏蔽DEBUG
    g_log_level = LOG_LEVEL_INFO;
    LOG_DEBUG("this debug log will not show");
    LOG_INFO("info still visible");

    printf("===== Test log module pass =====\n");
    return 0;
}
