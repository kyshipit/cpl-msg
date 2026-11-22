/**
 * cmd_dispatch
 * 命令分发层，根据cmd号调用对应业务处理函数
 */
#ifndef CMD_DISPATCH_H
#define CMD_DISPATCH_H
#include <stdint.h>
#include <stdbool.h>
#include "protocol/packet.h"
#ifdef __cplusplus
extern "C" {
#endif

struct TcpConn;
struct ServiceCtx;
struct SessionMgr;

// 业务handler上下文
typedef struct HandlerCtx{
    struct ServiceCtx *svc_ctx;
    struct TcpConn *conn;
    struct SessionMgr *sess_mgr;
} HandlerCtx;

// 命令处理函数原型
typedef void (*CmdHandler)(const struct MsgHeader *hdr, const uint8_t *body, void *user_data);

// 命令表项
typedef struct CmdEntry{
    uint32_t cmd;
    CmdHandler handler;
    const char *name;
} CmdEntry;

// 根据包头分发到对应handler
bool cmd_dispatch(const struct MsgHeader *hdr, const uint8_t *body, void *user_data);

// 打印命令表就绪日志
void cmd_dispatch_register_table(void);

#ifdef __cplusplus
}
#endif
#endif
