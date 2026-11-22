/**
 * parser实现
 * TCP字节流状态机，支持错误后滑动字节重新同步包头
 */
#include "protocol/parser.h"
#include "core/log.h"
#include <string.h>
#include <assert.h>

// 进入解析错误状态
static void parser_enter_error(Parser *parser)
{
    assert(parser != NULL);
    LOG_WARN("parser enter error state");
    parser->state = PARSER_STATE_ERROR;
}

// 初始化parser实例
void parser_init(Parser *parser, Mempool *mp, ParserPktCallback cb, void *user_data)
{
    assert(parser != NULL && cb != NULL);
    if (!parser)
        return;
    parser->mp = mp;
    parser->pkt_cb = cb;
    parser->user_data = user_data;
    parser_reset(parser);
    LOG_DEBUG("parser init ok");
}

// 重置解析器，释放已分配body缓冲区
void parser_reset(Parser *parser)
{
    assert(parser != NULL);
    if (!parser)
        return;
    if (parser->mp && parser->body_buf != NULL)
    {
        mempool_free(parser->mp, parser->body_buf);
        parser->body_buf = NULL;
    }
    parser->state = PARSER_STATE_WAIT_HDR;
    parser->hdr_recv = 0;
    parser->body_recv = 0;
    parser->body_need = 0;
    memset(parser->hdr_buf, 0, sizeof(parser->hdr_buf));
    LOG_DEBUG("parser reset");
}

// 解析完成包头，准备接收包体
static void parser_process_hdr(Parser *parser)
{
    assert(parser != NULL);
    MsgHeader *raw_hdr = (MsgHeader *)parser->hdr_buf;
    packet_ntoh(raw_hdr);
    LOG_DEBUG("parse header ok, cmd=%u, body_len=%u", raw_hdr->cmd, raw_hdr->body_len);

    if (raw_hdr->body_len > PARSER_MAX_BODY_LEN)
    {
        LOG_ERROR("body_len too large %u > max %u", raw_hdr->body_len, PARSER_MAX_BODY_LEN);
        parser_enter_error(parser);
        return;
    }

    if (raw_hdr->body_len == 0)
    {
        if (parser->pkt_cb)
        {
            parser->pkt_cb(parser, raw_hdr, NULL, parser->user_data);
        }
        parser->state = PARSER_STATE_WAIT_HDR;
        parser->hdr_recv = 0;
        return;
    }

    parser->body_need = raw_hdr->body_len;
    parser->body_recv = 0;
    parser->state = PARSER_STATE_WAIT_BODY;

    if(parser->mp != NULL)
    {
        parser->body_buf = mempool_alloc(parser->mp);
        if (parser->body_buf == NULL)
        {
            LOG_ERROR("mempool alloc for body buffer failed");
            parser_enter_error(parser);
            return;
        }
    }
}

// 包体接收完毕，向上层回调数据包
static void parser_process_body(Parser *parser, const uint8_t *input_body)
{
    assert(parser != NULL);
    MsgHeader *hdr = (MsgHeader *)parser->hdr_buf;
    const uint8_t *body = (parser->mp != NULL) ? parser->body_buf : input_body;

    if (parser->pkt_cb)
    {
        parser->pkt_cb(parser, hdr, body, parser->user_data);
    }

    if(parser->mp != NULL && parser->body_buf != NULL)
    {
        mempool_free(parser->mp, parser->body_buf);
        parser->body_buf = NULL;
    }
    parser->state = PARSER_STATE_WAIT_HDR;
    parser->hdr_recv = 0;
}

// 错误状态下滑动1字节，尝试重新同步包头
static bool parser_try_resync(Parser *parser)
{
    if(parser->hdr_recv < sizeof(MsgHeader))
        return false;
    // 滑动1字节
    memmove(parser->hdr_buf, parser->hdr_buf + 1, sizeof(MsgHeader) - 1);
    parser->hdr_recv--;
    // 收满一个包头则尝试校验
    if(parser->hdr_recv == sizeof(MsgHeader))
    {
        MsgHeader *hdr = (MsgHeader *)parser->hdr_buf;
        // 简单校验：版本号必须为1，包体长度不超上限
        if(hdr->ver == 1 && hdr->body_len <= PARSER_MAX_BODY_LEN)
        {
            parser->state = PARSER_STATE_WAIT_HDR;
            return true;
        }
    }
    return false;
}

// 输入字节流驱动状态机
void parser_input(Parser *parser, const uint8_t *data, uint32_t len)
{
    assert(parser != NULL && data != NULL);
    if (!parser || !data || len == 0)
        return;

    uint32_t pos = 0;
    while (pos < len)
    {
        // 错误状态：持续滑动尝试重同步
        if(parser->state == PARSER_STATE_ERROR)
        {
            if(parser_try_resync(parser))
            {
                continue;
            }
            // 补一个字节继续试
            if(pos < len)
            {
                parser->hdr_buf[parser->hdr_recv++] = data[pos++];
            }
            continue;
        }

        if (parser->state == PARSER_STATE_WAIT_HDR)
        {
            uint32_t copy = sizeof(MsgHeader) - parser->hdr_recv;
            if (copy > len - pos)
                copy = len - pos;
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
                copy = len - pos;

            if(parser->mp != NULL)
            {
                memcpy(parser->body_buf + parser->body_recv, data + pos, copy);
            }
            parser->body_recv += copy;
            pos += copy;

            if (parser->body_recv == parser->body_need)
            {
                const uint8_t *pkt_body_start = data + pos - parser->body_need;
                parser_process_body(parser, pkt_body_start);
            }
        }
    }
}
