/**
 * @file test_packet.c
 * @brief packet模块单元测试：位域、柔性数组、字节序转换
 * 编译：gcc -std=c11 -Wall -Wextra -g tests/test_packet.c src/packet/packet.c -o test_packet
 * 运行：./test_packet
 */
#include "../src/packet/packet.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    printf("===== Test packet module start =====\n");

    // 1. 打印包头大小，验证static_assert生效，现在预期是8
    printf("sizeof(MsgHeader) = %zu bytes\n", sizeof(MsgHeader));

    // 2. 手动构造一个包，堆上分配（柔性数组推荐堆分配）
    uint32_t body_len = 5;
    uint32_t total_sz = packet_total_size(body_len);
    MsgHeader *pkt = (MsgHeader *)malloc(total_sz);
    if (!pkt)
    {
        printf("malloc fail\n");
        return -1;
    }

    // C99指定初始化
    *pkt = (MsgHeader){
        .ver = 1,
        .encrypt = 0,
        .compress = 0,
        .reserved = 0,
        .cmd = CMD_PING,
        .seq = 1001,
        .body_len = body_len
    };
    // 填充包体
    pkt->data[0] = 0x11;
    pkt->data[1] = 0x22;
    pkt->data[2] = 0x33;
    pkt->data[3] = 0x44;
    pkt->data[4] = 0x55;

    printf("Before hton: seq=%u, body_len=%u\n", pkt->seq, pkt->body_len);

    // 主机序 -> 网络序
    packet_hton(pkt);
    printf("After hton:  seq=%u, body_len=%u\n", pkt->seq, pkt->body_len);

    // 网络序 -> 主机序
    packet_ntoh(pkt);
    printf("After ntoh:  seq=%u, body_len=%u\n", pkt->seq, pkt->body_len);

    // 校验位域
    printf("bit field ver = %d\n", pkt->ver);

    free(pkt);
    printf("===== Test packet module pass =====\n");
    return 0;
}
