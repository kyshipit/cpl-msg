/**
 * @file service_ctx.h
 * @brief 服务根上下文，整个服务单实例，持有所有顶层资源
 */
#ifndef SERVICE_CTX_H
#define SERVICE_CTX_H

#include <stdint.h>
#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif

// 循环依赖，只标签前置
struct ConnMgr;

/**
 * @brief ServiceCtx顶层服务上下文单实例
 */
typedef struct ServiceCtx {
    struct ConnMgr *conn_mgr;
} ServiceCtx;

/**
 * @brief 初始化整个服务
 * @param ctx ServiceCtx实例
 * @param port TCP监听端口
 * @return true初始化成功 false失败
 */
bool service_ctx_init(ServiceCtx *ctx, uint16_t port);

/**
 * @brief 启动服务主循环（阻塞）
 * @param ctx ServiceCtx实例
 */
bool service_ctx_run(ServiceCtx *ctx);

/**
 * @brief 销毁释放全部服务资源
 * @param ctx ServiceCtx实例
 */
void service_ctx_destroy(ServiceCtx *ctx);

#ifdef __cplusplus
}
#endif
#endif
