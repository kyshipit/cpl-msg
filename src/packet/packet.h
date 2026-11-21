/**
 * @file packet.h
 * @brief MiniMsg-C 自定义二进制协议包头定义、序列化辅助函数
 * @note C11 特性：static_assert、柔性数组、位域
 * @learning 位域、柔性数组、编译期对齐校验、网络字节序、结构体packed
 */
#ifndef PACKET_H
#define PACKET_H

#include <stdint.h>
#include <assert.h>

#ifdef __cplusplus
extern "C" {
#endif

// ===================== 命令枚举 =====================
typedef enum {
    CMD_PING     = 0x01,    // 心跳包
    CMD_ECHO     = 0x02,    // 回显包
    CMD_AI_TEXT  = 0x03     // AI文本问答（规则聊天）
} CmdType;

// ===================== 协议包头 =====================
/**
 * MsgHeader：固定包头 + 变长包体柔性数组
 * 【1】位域：用bit打包标志位，节省带宽，协议/寄存器开发高频写法
 * 【2】柔性数组 data[]：只能放在结构体最后；不计入结构体sizeof大小
 * 【3】static_assert：C11编译期断言，校验包头固定长度
 * 【4】__attribute__((packed)) GCC扩展：取消结构体自动对齐填充，保证协议二进制布局稳定
 * 【注意UB】柔性数组不能直接结构体拷贝，只能拷贝包头+后续data内存；不能栈上开大柔性数组
 * 【扩展点】后续TFLite MNIST包可以新增 CMD_AI_MNIST，复用这套包头结构
 */
typedef struct MsgHeader {
    uint8_t ver      :4;    // 协议版本 0~15
    uint8_t encrypt  :1;    // 是否加密 0/1
    uint8_t compress :1;    // 是否压缩 0/1
    uint8_t reserved :2;    // 保留bit，预留扩展

    uint8_t  cmd;           // 命令号 CmdType
    uint16_t seq;           // 序列号，应答和请求配对
    uint32_t body_len;      // 包体data长度（字节）
    uint8_t  data[];        // 柔性数组：变长包体
} __attribute__((packed)) MsgHeader;

// 强制校验包头固定大小：8字节
static_assert(sizeof(MsgHeader) == 8, "MsgHeader must be exactly 8 bytes!");

// ===================== 接口声明 =====================
/**
 * @brief 计算一个完整数据包总字节长度（包头 + 包体）
 * @param body_len 包体data长度
 * @return 整个包占用内存大小
 */
static inline uint32_t packet_total_size(uint32_t body_len)
{
    // inline C99内联函数，适合短小工具函数，减少函数调用开销
    return sizeof(MsgHeader) + body_len;
}

/**
 * @brief 把主机序的包头字段转为网络大端序（发送前调用）
 * @param hdr 非空包头指针
 * 【注意UB】传入hdr必须是合法分配的内存，不能是野指针
 */
void packet_hton(MsgHeader *hdr);

/**
 * @brief 网络大端序转主机序（收到包解析后调用）
 * @param hdr 非空包头指针
 */
void packet_ntoh(MsgHeader *hdr);

#ifdef __cplusplus
}
#endif

#endif
