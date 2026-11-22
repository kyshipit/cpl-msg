/**
 * packet
 * 自定义二进制协议包头，字节序转换工具
 */
#ifndef PACKET_H
#define PACKET_H
#include <stdint.h>
#include <assert.h>
#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    CMD_PING     = 0x01,
    CMD_ECHO     = 0x02,
    CMD_AI_TEXT  = 0x03
} CmdType;

typedef struct MsgHeader {
    uint8_t ver      :4;
    uint8_t encrypt  :1;
    uint8_t compress :1;
    uint8_t reserved :2;
    uint8_t  cmd;
    uint16_t seq;
    uint32_t body_len;
    uint8_t  data[];
} __attribute__((packed)) MsgHeader;

static_assert(sizeof(MsgHeader) == 8, "MsgHeader must be exactly 8 bytes!");

// 计算完整数据包总字节
static inline uint32_t packet_total_size(uint32_t body_len)
{
    return sizeof(MsgHeader) + body_len;
}

// 主机序转网络大端序
void packet_hton(MsgHeader *hdr);

// 网络大端序转回主机序
void packet_ntoh(MsgHeader *hdr);

#ifdef __cplusplus
}
#endif
#endif
