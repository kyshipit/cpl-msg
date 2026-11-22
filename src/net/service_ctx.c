/**
 * service_ctx实现
 * 顶层上下文，session过期扫描线程；主线程pause等待信号，标准退出范式
 */
#include <time.h>
#include "net/service_ctx.h"
#include "core/log.h"
#include "core/mempool.h"
#include "dispatch/cmd_dispatch.h"
#include "net/master_worker.h"
#include "session/session.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <signal.h>
#include <assert.h>

static volatile sig_atomic_t g_exit_requested = 0;

// 获取单调时钟毫秒
static uint64_t get_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

// session过期扫描后台线程，每10秒扫描一次，每秒检查退出标志
static void* session_scan_thread(void *ud)
{
    assert(ud != NULL);
    ServiceCtx *ctx = (ServiceCtx*)ud;
    while(!ctx->destroyed)
    {
        // 拆成1秒粒度，每秒检查一次退出标记，降低退出延迟
        for(int i = 0; i < 10 && !ctx->destroyed; i++)
        {
            sleep(1);
        }
        if(ctx->destroyed)
            break;
        uint64_t now = get_ms();
        session_scan_expire(ctx->sess_mgr, now, 30ULL*60*1000);
    }
    return NULL;
}

// 标记退出，信号处理函数调用，异步信号安全
void service_ctx_notify_exit(void)
{
    g_exit_requested = 1;
}

// 初始化顶层服务上下文
bool service_ctx_init(ServiceCtx *ctx, uint16_t port, uint32_t worker_num)
{
    assert(ctx != NULL);
    memset(ctx, 0, sizeof(ServiceCtx));
    ctx->destroyed = false;
    g_exit_requested = 0;

    ctx->sig_eventfd = eventfd(0, EFD_NONBLOCK);
    if(ctx->sig_eventfd <0)
    {
        LOG_ERROR("service_ctx sig_eventfd create failed");
        return false;
    }

    ctx->mp = mempool_create(32);
    if(!ctx->mp)
    {
        LOG_ERROR("mempool create fail");
        close(ctx->sig_eventfd);
        return false;
    }
    ctx->sess_mgr = malloc(sizeof(SessionMgr));
    if (!ctx->sess_mgr)
    {
        LOG_ERROR("malloc sess_mgr failed");
        mempool_destroy(ctx->mp);
        close(ctx->sig_eventfd);
        ctx->mp = NULL;
        return false;
    }
    session_mgr_init(ctx->sess_mgr);
    cmd_dispatch_register_table();
    ctx->mw = malloc(sizeof(MasterWorker));
    if (!ctx->mw)
    {
        LOG_ERROR("malloc MasterWorker failed");
        free(ctx->sess_mgr);
        mempool_destroy(ctx->mp);
        close(ctx->sig_eventfd);
        ctx->mp = NULL;
        return false;
    }
    if (!master_worker_init(ctx->mw, ctx, port, worker_num))
    {
        LOG_ERROR("master worker init fail");
        free(ctx->mw);
        free(ctx->sess_mgr);
        mempool_destroy(ctx->mp);
        close(ctx->sig_eventfd);
        ctx->mp = NULL;
        return false;
    }
    return true;
}

// 主线程等待退出信号，pause被信号中断后检查标志
bool service_ctx_run(ServiceCtx *ctx)
{
    assert(ctx != NULL);
    if (!ctx || !ctx->mw || ctx->destroyed)
        return false;
    pthread_t scan_tid;
    pthread_create(&scan_tid, NULL, session_scan_thread, ctx);

    while(!g_exit_requested)
    {
        pause();
    }

    // 标记销毁，唤醒扫描线程
    ctx->destroyed = true;
    pthread_join(scan_tid, NULL);
    return true;
}

// 逆序销毁全部服务资源
void service_ctx_destroy(ServiceCtx *ctx)
{
    if (!ctx || ctx->destroyed)
        return;
    ctx->destroyed = true;
    if (ctx->mw)
    {
        master_worker_destroy(ctx->mw);
        free(ctx->mw);
        ctx->mw = NULL;
    }
    if (ctx->sess_mgr)
    {
        free(ctx->sess_mgr);
        ctx->sess_mgr = NULL;
    }
    if(ctx->mp)
    {
        mempool_destroy(ctx->mp);
        ctx->mp = NULL;
    }
    if(ctx->sig_eventfd >=0)
    {
        close(ctx->sig_eventfd);
        ctx->sig_eventfd = -1;
    }
    LOG_INFO("service ctx destroy complete");
}
