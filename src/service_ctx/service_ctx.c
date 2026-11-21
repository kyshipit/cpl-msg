/**
 * @file service_ctx.c
 */

#include "service_ctx/service_ctx.h"
#include "log/log.h"
#include "mempool/mempool.h"
#include "cmd_dispatch/cmd_dispatch.h"
#include "conn_mgr/conn_mgr.h"
#include "session/session.h"
#include <string.h>
#include <stdlib.h>

bool service_ctx_init(ServiceCtx *ctx, uint16_t port)
{
    memset(ctx, 0, sizeof(ServiceCtx));
    ctx->conn_mgr = malloc(sizeof(ConnMgr));
    if (!ctx->conn_mgr)
    {
        LOG_ERROR("malloc conn_mgr fail");
        return false;
    }
    // 先清零conn_mgr本身！不要交给conn_mgr_init内部memset
    memset(ctx->conn_mgr, 0, sizeof(ConnMgr));

    // 分配sessionMgr内存
    ctx->conn_mgr->sess_mgr = malloc(sizeof(SessionMgr));
    if(!ctx->conn_mgr->sess_mgr)
    {
        LOG_ERROR("malloc sess_mgr fail");
        free(ctx->conn_mgr);
        ctx->conn_mgr = NULL;
        return false;
    }

    // malloc之后立刻初始化会话管理器
    session_mgr_init(ctx->conn_mgr->sess_mgr);

    mempool_init();
    cmd_dispatch_register_table();

    if (!conn_mgr_init(ctx->conn_mgr, port, ctx))
    {
        LOG_ERROR("conn mgr init fail");
        free(ctx->conn_mgr->sess_mgr);
        free(ctx->conn_mgr);
        ctx->conn_mgr = NULL;
        return false;
    }
    return true;
}

bool service_ctx_run(ServiceCtx *ctx)
{
    if (!ctx || !ctx->conn_mgr) return false;
    conn_mgr_run_loop(ctx->conn_mgr);
    return true;
}

void service_ctx_destroy(ServiceCtx *ctx)
{
    if (!ctx) return;
    if (ctx->conn_mgr)
    {
        conn_mgr_destroy(ctx->conn_mgr);
        free(ctx->conn_mgr->sess_mgr); //释放sess_mgr
        free(ctx->conn_mgr);
        ctx->conn_mgr = NULL;
    }
}