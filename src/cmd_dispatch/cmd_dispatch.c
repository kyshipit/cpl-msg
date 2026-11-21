/**
 * @file cmd_dispatch.c
 * @brief 命令分发表实现：PING / ECHO / 预留AI_TEXT示例
 */
#include "cmd_dispatch/cmd_dispatch.h"
#include "log/log.h"
#include "tcp_conn/tcp_conn.h"
#include "session/session.h"
#include "service_ctx/service_ctx.h"
#include <string.h>
#include <stdio.h>


// 业务handler声明
static void cmd_ping_handler(const MsgHeader *hdr, const uint8_t *body, void *ud);
static void cmd_echo_handler(const MsgHeader *hdr, const uint8_t *body, void *ud);
static void cmd_unknown_handler(const MsgHeader *hdr, const uint8_t *body, void *ud);
static void cmd_ai_text_handler(const MsgHeader *hdr, const uint8_t *body, void *ud);

// 静态命令路由表
static CmdEntry g_cmd_table[] = {
    {CMD_PING,    cmd_ping_handler,    "PING"},
    {CMD_ECHO,    cmd_echo_handler,    "ECHO"},
    {CMD_AI_TEXT, cmd_ai_text_handler, "AI_TEXT"}, 
};
static const uint32_t g_cmd_table_cnt = sizeof(g_cmd_table) / sizeof(g_cmd_table[0]);

// ping：心跳，简单打印，实际项目这里会回复PONG包
static void cmd_ping_handler(const MsgHeader *hdr, const uint8_t *body, void *ud)
{
    HandlerCtx *hctx = (HandlerCtx *)ud;
    (void)body;
    LOG_INFO("PING fd=%d, seq=%u", hctx->conn->fd, hdr->seq);
    // 回复PONG包
    tcp_conn_send_pkt(hctx->conn, CMD_PING, hdr->seq, NULL, 0);
}

// echo：原样回显包体文本
static void cmd_echo_handler(const MsgHeader *hdr, const uint8_t *body, void *ud)
{
    HandlerCtx *hctx = (HandlerCtx *)ud;
    if (hdr->body_len == 0 || body == NULL)
    {
        LOG_WARN("ECHO body empty");
        return;
    }
    LOG_INFO("dispatch: handle ECHO, seq=%u, body_len=%u, content: %s",
             hdr->seq, hdr->body_len, body);
    
    // 原样回包
    tcp_conn_send_pkt(hctx->conn, CMD_ECHO, hdr->seq, body, hdr->body_len); 
}

// 兜底未知命令
static void cmd_unknown_handler(const MsgHeader *hdr, const uint8_t *body, void *ud)
{
    (void)body;
    (void)ud;
    LOG_WARN("dispatch: unknown cmd %u, seq=%u", hdr->cmd, hdr->seq);
}

static void cmd_ai_text_handler(const MsgHeader *hdr, const uint8_t *body, void *user_data)
{
    HandlerCtx *hctx = (HandlerCtx *)user_data;
    //ServiceCtx *svc = hctx->svc_ctx;
    struct TcpConn *conn = hctx->conn;
    struct SessionMgr *sess_mgr = hctx->sess_mgr;

    if (!hdr || body == NULL || hdr->body_len == 0)
    {
        LOG_WARN("AI_TEXT empty body");
        return;
    }
    // 找到当前连接对应的session
    Session *sess = session_find_by_conn(sess_mgr, conn);
    if (!sess)
    {
        LOG_WARN("no session for this conn");
        return;
    }

    // 用户消息入库
    char user_msg[SESSION_MSG_CONTENT_LEN];
    strncpy(user_msg, (const char *)body, sizeof(user_msg)-1);
    user_msg[sizeof(user_msg)-1] = '\0';
    session_append_msg(sess, true, user_msg);

    // ========= Mock AI 逻辑（后面可以替换成真实LLM调用） =========
    char ai_reply[256];
    snprintf(ai_reply, sizeof(ai_reply), "[MockAI]收到：%s，历史轮数：%u", user_msg, sess->msg_cnt);
    // AI回复存入session
    session_append_msg(sess, false, ai_reply);

    // 回包给客户端，seq复用本次请求seq
    tcp_conn_send_pkt(conn, CMD_AI_TEXT, hdr->seq, (const uint8_t *)ai_reply, strlen(ai_reply));
    LOG_INFO("AI_TEXT reply: %s", ai_reply);
}

bool cmd_dispatch(const MsgHeader *hdr, const uint8_t *body, void *user_data)
{
    if (hdr == NULL)
    {
        LOG_ERROR("dispatch hdr is NULL");
        return false;
    }
    uint32_t i = 0;
    for (; i < g_cmd_table_cnt; i++)
    {
        if (g_cmd_table[i].cmd == hdr->cmd)
        {
            LOG_DEBUG("dispatch match cmd=%u [%s]", hdr->cmd, g_cmd_table[i].name);
            g_cmd_table[i].handler(hdr, body, user_data);
            return true;
        }
    }
    // 没有匹配到
    cmd_unknown_handler(hdr, body, user_data);
    return false;
}

void cmd_dispatch_register_table(void)
{
    // 静态表版本这里是空，动态注册版本才在这里做初始化
    LOG_INFO("cmd dispatch table ready, total %u cmds", g_cmd_table_cnt);
}
