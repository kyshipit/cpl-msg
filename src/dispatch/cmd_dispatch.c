/**
 * cmd_dispatch实现
 * 静态命令表映射cmd与处理函数
 */
#include "dispatch/cmd_dispatch.h"
#include "core/log.h"
#include "net/tcp_conn.h"
#include "session/session.h"
#include <assert.h>
#include <string.h>

static void cmd_ping_handler(const MsgHeader *hdr, const uint8_t *body, void *ud);
static void cmd_echo_handler(const MsgHeader *hdr, const uint8_t *body, void *ud);
static void cmd_ai_text_handler(const MsgHeader *hdr, const uint8_t *body, void *ud);
static void cmd_unknown_handler(const MsgHeader *hdr, const uint8_t *body, void *ud);

// 业务命令静态注册表
static CmdEntry g_cmd_table[] = {
    {CMD_PING,    cmd_ping_handler,    "PING"},
    {CMD_ECHO,    cmd_echo_handler,    "ECHO"},
    {CMD_AI_TEXT, cmd_ai_text_handler, "AI_TEXT"},
};
static const uint32_t g_cmd_table_cnt = sizeof(g_cmd_table) / sizeof(g_cmd_table[0]);

// ping命令处理，直接回包
static void cmd_ping_handler(const MsgHeader *hdr, const uint8_t *body, void *ud)
{
    HandlerCtx *hctx = (HandlerCtx *)ud;
    assert(hctx != NULL);
    (void)body;
    LOG_INFO("PING fd=%d, seq=%u", hctx->conn->fd, hdr->seq);
    tcp_conn_send_pkt(hctx->conn, CMD_PING, hdr->seq, NULL, 0);
}

// echo命令处理，原样返回包体
static void cmd_echo_handler(const MsgHeader *hdr, const uint8_t *body, void *ud)
{
    HandlerCtx *hctx = (HandlerCtx *)ud;
    assert(hctx != NULL);
    if (hdr->body_len == 0 || body == NULL)
    {
        LOG_WARN("ECHO body empty");
        return;
    }
    LOG_INFO("dispatch: handle ECHO, seq=%u, body_len=%u",
             hdr->seq, hdr->body_len);
    tcp_conn_send_pkt(hctx->conn, CMD_ECHO, hdr->seq, body, hdr->body_len);
}

// ai_text模拟AI对话，保存会话历史，返回模拟AI回复
static void cmd_ai_text_handler(const MsgHeader *hdr, const uint8_t *body, void *ud)
{
    HandlerCtx *hctx = (HandlerCtx *)ud;
    assert(hctx != NULL);
    struct TcpConn *conn = hctx->conn;
    struct SessionMgr *sess_mgr = hctx->sess_mgr;
    if (!hdr || body == NULL || hdr->body_len == 0)
    {
        LOG_WARN("AI_TEXT empty body");
        return;
    }
    Session *sess = session_find_by_conn(sess_mgr, conn);
    if (!sess)
    {
        LOG_WARN("no session for this conn");
        return;
    }
    // 按长度拷贝二进制body，手动补结束符
    char user_msg[SESSION_MSG_CONTENT_LEN];
    uint32_t copy_len = hdr->body_len;
    if(copy_len >= sizeof(user_msg))
        copy_len = sizeof(user_msg) - 1;
    memcpy(user_msg, body, copy_len);
    user_msg[copy_len] = '\0';

    session_append_msg(sess, true, user_msg);
    // 修复：计算真实对话轮数
    uint32_t round = (sess->msg_cnt + 1) / 2;
    char ai_reply[256];
    snprintf(ai_reply, sizeof(ai_reply), "[MockAI]收到：%s，历史轮数：%u", user_msg, round);
    session_append_msg(sess, false, ai_reply);
    tcp_conn_send_pkt(conn, CMD_AI_TEXT, hdr->seq, (const uint8_t *)ai_reply, strlen(ai_reply));
    LOG_INFO("AI_TEXT reply: %s", ai_reply);
}

// 未知命令处理
static void cmd_unknown_handler(const MsgHeader *hdr, const uint8_t *body, void *ud)
{
    (void)hdr;
    (void)body;
    (void)ud;
    LOG_WARN("dispatch: unknown cmd %u, seq=%u", hdr->cmd, hdr->seq);
}

// 分发入口，遍历命令表执行对应handler
bool cmd_dispatch(const struct MsgHeader *hdr, const uint8_t *body, void *user_data)
{
    if (hdr == NULL || user_data == NULL)
    {
        LOG_ERROR("dispatch null pointer");
        return false;
    }
    for (uint32_t i = 0; i < g_cmd_table_cnt; i++)
    {
        if (g_cmd_table[i].cmd == hdr->cmd)
        {
            LOG_DEBUG("dispatch match cmd=%u [%s]", hdr->cmd, g_cmd_table[i].name);
            g_cmd_table[i].handler(hdr, body, user_data);
            return true;
        }
    }
    cmd_unknown_handler(hdr, body, user_data);
    return false;
}

// 打印命令表就绪日志
void cmd_dispatch_register_table(void)
{
    LOG_INFO("cmd dispatch table ready, total %u cmds", g_cmd_table_cnt);
}
