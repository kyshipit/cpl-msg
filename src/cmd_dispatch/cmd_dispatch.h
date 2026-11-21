/**
 * @file cmd_dispatch.h
 * @brief MiniMsg-C 命令分发层：cmd -> 业务处理函数表，路由分发
 * @learning 函数指针数组、命令注册表、查表分发、解耦协议解析与业务
 * @note parser回调里调用dispatch，根据hdr->cmd找到对应handler执行
 * 【注意UB】handler内部不要阻塞；业务包体指针有效期仅限本次回调内，不要长期持有
 */
#ifndef CMD_DISPATCH_H
#define CMD_DISPATCH_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 循环依赖，只标签前置
struct MsgHeader;
struct ServiceCtx;
struct TcpConn;
struct SessionMgr;


/**
 * @brief HandlerCtx：传给handler的上下文
 * 代替只传TcpConn*，handler同时拿到根服务上下文 + 当前连接
 */
typedef struct HandlerCtx{
    struct ServiceCtx *svc_ctx;
    struct TcpConn *conn;
    struct SessionMgr *sess_mgr;
} HandlerCtx;

/**
 * @brief 业务命令处理函数原型
 * @param hdr 主机序包头
 * @param body 包体数据，body_len为0时可为NULL
 * @param user_data 用户透传上下文，一般为HandlerCtx*
 */
typedef void (*CmdHandler)(const struct MsgHeader *hdr, const uint8_t *body, void *user_data);

/**
 * @brief 命令注册表表项：命令号、处理函数、命令名字符串
 */
typedef struct CmdEntry{
    uint32_t cmd;
    CmdHandler handler;
    const char *name;
} CmdEntry;

/**
 * @brief 命令分发入口，查表调用对应handler
 * @param hdr 主机序包头
 * @param body 包体数据
 * @param user_data HandlerCtx指针
 * @return true找到handler执行；false未知命令
 */
bool cmd_dispatch(const struct MsgHeader *hdr, const uint8_t *body, void *user_data);

/**
 * @brief 注册命令处理函数
 * 静态表在.c里维护，新增命令只需要在表追加一行
 */
void cmd_dispatch_register_table(void);

#ifdef __cplusplus
}
#endif

#endif
