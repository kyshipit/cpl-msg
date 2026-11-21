/**
 * @file parser.h
 * @brief MiniMsg-C 协议解析器：流式状态机，处理TCP粘包、分包，函数指针回调分发
 * @learning 状态机、环形/流式缓冲区、TCP粘包处理、函数指针回调、事件驱动
 * @note TCP是流式无边界，不能保证一次recv拿到一个完整包；状态机是标准解决方案
 * 【注意UB】回调内部禁止长时间阻塞；缓冲区越界会破坏状态；回调中不要递归调用parser输入
 */
#ifndef PARSER_H
#define PARSER_H

#include <stdint.h>
#include <stdbool.h>
#include "packet/packet.h"  // ✅ 必须引入，sizeof(MsgHeader) 需要完整类型

#ifdef __cplusplus
extern "C" {
#endif


// 解析状态枚举
typedef enum {
    PARSER_STATE_WAIT_HDR,      // 等待包头（8字节）
    PARSER_STATE_WAIT_BODY,     // 等待包体
    PARSER_STATE_ERROR          // 解析出错，重置状态
} ParserState;

// 最大包体长度保护，防止恶意大包
#define PARSER_MAX_BODY_LEN  1024U

// 前置标签，用于回调原型
struct Parser; 


/**
 * @brief 包解析完成回调函数类型
 * @param parser 解析器实例
 * @param hdr 已经转为主机序的包头指针
 * @param user_data 注册时传入的用户私有数据
 */
// 函数指针回调保留typedef
typedef void (*ParserPktCallback)(struct Parser *parser, const struct MsgHeader *hdr, void *user_data);

/**
 * Parser 实例结构体：保存状态、缓冲区、当前包信息、回调
 * 面向对象C风格：结构体+函数指针，一个parser对应一条连接
 */
typedef struct Parser {
    ParserState state;
    uint8_t hdr_buf[sizeof(MsgHeader)]; // 包头临时缓存
    uint32_t hdr_recv;                  // 已经收到的包头字节数
    uint8_t *body_buf;                  // 包体缓存（从内存池分配）
    uint32_t body_recv;                 // 已经收到的包体字节数
    uint32_t body_need;                 // 当前包需要的总包体字节

    ParserPktCallback pkt_cb;
    void *user_data;
}Parser ;

/**
 * @brief 初始化解析器实例
 * @param parser 指向Parser对象
 * @param cb 收到完整包后的回调
 * @param user_data 回调携带的私有数据
 */
void parser_init(Parser *parser, ParserPktCallback cb, void *user_data);

/**
 * @brief 输入流式数据（recv读到的数据丢进来，自动状态机解析）
 * @param parser 解析器
 * @param data 收到的原始字节（网络序）
 * @param len 字节长度
 */
void parser_input(Parser *parser, const uint8_t *data, uint32_t len);

/**
 * @brief 重置解析器状态（出错、连接断开时调用）
 */
void parser_reset(Parser *parser);

#ifdef __cplusplus
}
#endif

#endif
