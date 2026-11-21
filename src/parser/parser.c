/**
 * @file parser.c
 * @brief 流式协议状态机解析实现，TCP粘包处理，回调分发
 */
#include "parser.h"
#include "../log/log.h"
#include "../mempool/mempool.h"
#include <string.h>

static void parser_enter_error(Parser *parser)
{
    LOG_WARN("parser enter error state, will reset later");
    parser->state = PARSER_STATE_ERROR;
}

void parser_init(Parser *parser, ParserPktCallback cb, void *user_data)
{
    if (!parser)
    {
        return;
    }
    parser->pkt_cb = cb;
    parser->user_data = user_data;
    parser_reset(parser);
    LOG_DEBUG("parser init ok");
}

void parser_reset(Parser *parser)
{
    if (!parser)
    {
        return;
    }
    // 如果包体缓冲区还持有内存池内存，归还
    if (parser->body_buf != NULL)
    {
        mempool_free(parser->body_buf);
        parser->body_buf = NULL;
    }

    parser->state = PARSER_STATE_WAIT_HDR;
    parser->hdr_recv = 0;
    parser->body_recv = 0;
    parser->body_need = 0;
    memset(parser->hdr_buf, 0, sizeof(parser->hdr_buf));
    LOG_DEBUG("parser reset");
}

static void parser_process_hdr(Parser *parser)
{
    MsgHeader *raw_hdr = (MsgHeader *)parser->hdr_buf;
    // 网络序转主机序
    packet_ntoh(raw_hdr);

    LOG_DEBUG("parse header ok, cmd=%u, body_len=%u", raw_hdr->cmd, raw_hdr->body_len);

    // 包长合法性校验
    if (raw_hdr->body_len > PARSER_MAX_BODY_LEN)
    {
        LOG_ERROR("body_len too large %u > max %u", raw_hdr->body_len, PARSER_MAX_BODY_LEN);
        parser_enter_error(parser);
        return;
    }

    // 没有包体，直接触发回调
    if (raw_hdr->body_len == 0)
    {
        if (parser->pkt_cb)
        {
            parser->pkt_cb(parser, raw_hdr, parser->user_data);
        }
        parser->state = PARSER_STATE_WAIT_HDR;
        parser->hdr_recv = 0;
        return;
    }

    // 分配包体缓存，从内存池拿
    parser->body_buf = mempool_alloc();
    if (parser->body_buf == NULL)
    {
        LOG_ERROR("mempool alloc for body buffer failed");
        parser_enter_error(parser);
        return;
    }

    parser->body_need = raw_hdr->body_len;
    parser->body_recv = 0;
    parser->state = PARSER_STATE_WAIT_BODY;
}

static void parser_process_body(Parser *parser)
{
    // 包体收齐，此时 hdr_buf 仍然保留包头信息
    MsgHeader *hdr = (MsgHeader *)parser->hdr_buf;
    if (parser->pkt_cb)
    {
        parser->pkt_cb(parser, hdr, parser->user_data);
    }

    // 释放包体内存，切回等待包头状态
    mempool_free(parser->body_buf);
    parser->body_buf = NULL;
    parser->state = PARSER_STATE_WAIT_HDR;
    parser->hdr_recv = 0;
}

void parser_input(Parser *parser, const uint8_t *data, uint32_t len)
{
    if (!parser || !data || len == 0)
    {
        return;
    }

    // 错误状态，直接重置丢弃当前数据
    if (parser->state == PARSER_STATE_ERROR)
    {
        LOG_WARN("parser error state, reset and discard data");
        parser_reset(parser);
    }

    uint32_t pos = 0;
    while (pos < len && parser->state != PARSER_STATE_ERROR)
    {
        if (parser->state == PARSER_STATE_WAIT_HDR)
        {
            // 拷贝字节到包头缓冲区，直到收满8字节
            uint32_t copy = sizeof(MsgHeader) - parser->hdr_recv;
            if (copy > len - pos)
            {
                copy = len - pos;
            }
            memcpy(parser->hdr_buf + parser->hdr_recv, data + pos, copy);
            parser->hdr_recv += copy;
            pos += copy;

            if (parser->hdr_recv == sizeof(MsgHeader))
            {
                parser_process_hdr(parser);
            }
        }
        else if (parser->state == PARSER_STATE_WAIT_BODY)
        {
            uint32_t copy = parser->body_need - parser->body_recv;
            if (copy > len - pos)
            {
                copy = len - pos;
            }
            memcpy(parser->body_buf + parser->body_recv, data + pos, copy);
            parser->body_recv += copy;
            pos += copy;

            if (parser->body_recv == parser->body_need)
            {
                parser_process_body(parser);
            }
        }
    }
}

void parser_get_body_ptr(Parser *parser, uint8_t **out_body)
{
    if (!parser || !out_body)
    {
        *out_body = NULL;
        return;
    }
    *out_body = parser->body_buf;
}