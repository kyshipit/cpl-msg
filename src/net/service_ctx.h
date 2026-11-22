/**
 * service_ctx
 * 顶层服务根上下文，统一管理内存池、master‑worker、session管理器
 */
#ifndef SERVICE_CTX_H
#define SERVICE_CTX_H
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

struct MasterWorker;
struct SessionMgr;
struct Mempool;

typedef struct ServiceCtx {
    struct MasterWorker *mw;
    struct SessionMgr *sess_mgr;
    struct Mempool *mp;
    int sig_eventfd;
    bool destroyed;
} ServiceCtx;

// 初始化整个服务上下文
bool service_ctx_init(ServiceCtx *ctx, uint16_t port, uint32_t worker_num);

// 启动服务，主线程等待退出信号
bool service_ctx_run(ServiceCtx *ctx);

// 销毁全部服务资源
void service_ctx_destroy(ServiceCtx *ctx);

// 信号处理调用，标记退出
void service_ctx_notify_exit(void);

#ifdef __cplusplus
}
#endif
#endif
