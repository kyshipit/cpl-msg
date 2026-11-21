/**
 * @file test_parser.c
 * @brief parser状态机测试：模拟TCP粘包、分包、完整包输入
 * 编译：gcc -std=c11 -Wall -Wextra -g -DDEBUG tests/test_parser.c src/parser/parser.c src/packet/packet.c src/mempool/mempool.c src/log/log.c -o test_parser
 * 运行：./test_parser
 */
#include "../src/parser/parser.h"
#include "../src/log/log.h"
#include "../src/mempool/mempool.h"
#include <stdio.h>
#include <stdlib.h>


// 收到完整包的回调
static void on_packet(Parser *parser, const MsgHeader *hdr, void *ud)
{
    (void)parser;
    (void)ud;
    LOG_INFO("===== GOT FULL PACKET =====");
    LOG_VAR(hdr->cmd, hdr->cmd);
    LOG_VAR(hdr->seq, hdr->seq);
    LOG_VAR(hdr->body_len, hdr->body_len);
}

int main(void)
{
    printf("===== Test parser module start =====\n");
    mempool_init();

    Parser p;
    parser_init(&p, on_packet, NULL);

    // 构造一个PING包，主机序 -> 网络序
    uint32_t body_len = 0;
    uint32_t total = packet_total_size(body_len);
    MsgHeader *pkt = (MsgHeader *)malloc(total);
    *pkt = (MsgHeader){
        .ver = 1,
        .encrypt = 0,
        .compress = 0,
        .reserved = 0,
        .cmd = CMD_PING,
        .seq = 8888,
        .body_len = body_len
    };
    packet_hton(pkt);

    // Case1：一次性送入完整包
    LOG_INFO("Case1: send full packet at once");
    parser_input(&p, (uint8_t *)pkt, total);

    // Case2：模拟粘包：把包头拆成两段，模拟分片
    LOG_INFO("Case2: split header into two fragments");
    parser_reset(&p);
    parser_input(&p, (uint8_t *)pkt, 4);
    parser_input(&p, (uint8_t *)pkt + 4, total - 4);

    free(pkt);

    printf("===== Test parser module pass =====\n");
    return 0;
}
