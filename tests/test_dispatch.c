/**
 * @file test_dispatch.c
 * @brief 命令分发单元测试：PING、ECHO、未知命令
 * 编译：gcc -std=c11 -Wall -Wextra -g -DDEBUG tests/test_dispatch.c src/cmd_dispatch/cmd_dispatch.c src/packet/packet.c src/log/log.c -o test_dispatch
 * 运行：./test_dispatch
 */
#include "../src/cmd_dispatch/cmd_dispatch.h"
#include "../src/packet/packet.h"
#include "../src/log/log.h"
#include <stdio.h>
#include <string.h>

int main(void)
{
    printf("===== Test cmd dispatch module start =====\n");
    cmd_dispatch_register_table();

    // 1. PING 包，无包体
    MsgHeader ping_hdr = {0};
    ping_hdr.cmd = CMD_PING;
    ping_hdr.seq = 1001;
    ping_hdr.body_len = 0;
    cmd_dispatch(&ping_hdr, NULL, NULL);

    // 2. ECHO 包，带文本
    MsgHeader echo_hdr = {0};
    echo_hdr.cmd = CMD_ECHO;
    echo_hdr.seq = 1002;
    echo_hdr.body_len = strlen("hello dispatch");
    const char *echo_body = "hello dispatch";
    cmd_dispatch(&echo_hdr, (const uint8_t *)echo_body, NULL);

    // 3. 未知命令
    MsgHeader unknown_hdr = {0};
    unknown_hdr.cmd = 255;
    unknown_hdr.seq = 1003;
    unknown_hdr.body_len = 0;
    cmd_dispatch(&unknown_hdr, NULL, NULL);

    printf("===== Test cmd dispatch module pass =====\n");
    return 0;
}
