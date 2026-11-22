/**
 * parser
 * TCP流协议解析状态机，处理粘包分包
 */
#ifndef PARSER_H
#define PARSER_H
#include <stdint.h>
#include <stdbool.h>
#include "protocol/packet.h"
#include "core/mempool.h"
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    PARSER_STATE_WAIT_HDR,
    PARSER_STATE_WAIT_BODY,
    PARSER_STATE_ERROR
} ParserState;

#define PARSER_MAX_BODY_LEN  1024U

struct Parser;
typedef void (*ParserPktCallback)(struct Parser *parser, const struct MsgHeader *hdr, const uint8_t *body, void *user_data);

typedef struct Parser {
    ParserState state;
    uint8_t hdr_buf[sizeof(MsgHeader)];
    uint32_t hdr_recv;
    Mempool *mp;
    uint8_t *body_buf;
    uint32_t body_recv;
    uint32_t body_need;
    ParserPktCallback pkt_cb;
    void *user_data;
} Parser;

// 初始化解析器，mp传NULL不使用内存池
void parser_init(Parser *parser, Mempool *mp, ParserPktCallback cb, void *user_data);

// 送入字节流进行解析
void parser_input(Parser *parser, const uint8_t *data, uint32_t len);

// 重置解析器状态，释放缓冲区
void parser_reset(Parser *parser);

#ifdef __cplusplus
}
#endif
#endif
