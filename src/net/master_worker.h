/**
 * master_worker
 * master‑worker网络模型，master负责accept，新连接轮询分发worker reactor
 */
#ifndef MASTER_WORKER_H
#define MASTER_WORKER_H
#include <stdint.h>
#include <stdbool.h>
#include "net/reactor.h"
#include "net/tcp_conn.h"
#include "session/session.h"
#include "net/service_ctx.h"
#ifdef __cplusplus
extern "C" {
#endif

#define MAX_WORKER 8
#define MAX_ONLINE_CONN 64

typedef struct MasterWorker {
    int listen_fd;
    uint16_t port;
    uint32_t worker_cnt;
    Reactor *workers[MAX_WORKER];
    Reactor *master_reactor;
    uint32_t round_idx;
    uint32_t online_conn_cnt;
    ServiceCtx *svc_ctx;
    SessionMgr *sess_mgr;
    bool destroyed;
} MasterWorker;

// 初始化master‑worker
bool master_worker_init(MasterWorker *mw, ServiceCtx *svc, uint16_t port, uint32_t worker_num);

// 预留运行接口，当前未实现
void master_worker_run(MasterWorker *mw);

// 销毁master‑worker所有资源
void master_worker_destroy(MasterWorker *mw);

#ifdef __cplusplus
}
#endif
#endif
