/**
 * @file tcp_conn.c
 * @brief TCP连接实现：读写事件、发包、心跳超时、生命周期
 */
#define _POSIX_C_SOURCE 200809L
#include <time.h>

#include "tcp_conn.h"
#include "log/log.h"
#include "packet/packet.h"
#include "cmd_dispatch/cmd_dispatch.h"
#include "service_ctx/service_ctx.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>

// 加unused消除未使用警告
static void tcp_conn_on_packet(struct Parser *parser, const MsgHeader *hdr, void *user_data) __attribute__((unused));
// parser收到完整包的静态回调，由tcp_conn注册给parser
static void tcp_conn_on_packet(struct Parser *parser, const MsgHeader *hdr, void *user_data)
{
    TcpConn *conn = (TcpConn *)user_data;
    if (!conn || conn->closed)
    {
        return;
    }
    // 包体在parser的body_buf中
    uint8_t *body = NULL;
    if (hdr->body_len > 0)
    {
        // 拿到parser内部的body_buf
        extern void parser_get_body_ptr(Parser *parser, uint8_t **out_body);
        parser_get_body_ptr(parser, &body);
    }

    HandlerCtx hctx = {
    .svc_ctx = conn->svc_ctx,
    .conn = conn,
    //.sess_mgr = conn->svc_ctx->conn_mgr->sess_mgr  //这里不能访问ConnMgr成员！
    .sess_mgr = conn->sess_mgr  //不再穿透 `conn->svc_ctx->conn_mgr`，完全规避未完整类型报错
    };

    // 交给命令分发层
    cmd_dispatch(hdr, body, &hctx);
}

static uint64_t get_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

struct TcpConn *tcp_conn_create(int fd, ServiceCtx *svc_ctx, TcpConnCloseCb close_cb, void *cb_ud)
{
    if (fd < 0)
    {
        LOG_ERROR("tcp_conn_create invalid fd");
        return NULL;
    }
    struct TcpConn *conn = malloc(sizeof(TcpConn));
    if (!conn)
    {
        LOG_ERROR("malloc TcpConn fail");
        return NULL;
    }
    memset(conn, 0, sizeof(TcpConn));
    conn->fd = fd;
    conn->closed = false;
    conn->close_cb = close_cb;
    conn->close_cb_ud = cb_ud;
    conn->svc_ctx = svc_ctx;  //入参补充 svc_ctx
    conn->last_recv_ts = get_ms();
    conn->send_len = 0;
    

    // 初始化parser，回调绑定tcp_conn_on_packet，user_data是conn自身
    parser_init(&conn->parser, tcp_conn_on_packet, conn);
    LOG_INFO("tcp_conn create ok, fd=%d", fd);
    return conn;
}

void tcp_conn_destroy(struct TcpConn *conn)
{
    if (!conn || conn->closed)
    {
        return;
    }
    LOG_INFO("tcp_conn destroy, fd=%d", conn->fd);
    conn->closed = true;
    close(conn->fd);
    parser_reset(&conn->parser);
    // 触发上层关闭回调
    if (conn->close_cb)
    {
        conn->close_cb(conn, conn->close_cb_ud);
    }
    free(conn);
}

bool tcp_conn_on_read(struct TcpConn *conn)
{
    if (!conn || conn->closed)
    {
        return false;
    }
    uint8_t tmp_buf[256];
    ssize_t n = recv(conn->fd, tmp_buf, sizeof(tmp_buf), 0);
    if (n <= 0)
    {
        if (n == 0)
        {
            LOG_INFO("peer close fd=%d", conn->fd);
        }
        else
        {
            LOG_WARN("recv error fd=%d", conn->fd);
        }
        return false;
    }
    // 更新最后接收时间
    conn->last_recv_ts = get_ms();
    // 送入parser状态机解析
    parser_input(&conn->parser, tmp_buf, (uint32_t)n);
    return true;
}

void tcp_conn_on_write(struct TcpConn *conn)
{
    if (!conn || conn->closed || conn->send_len == 0)
    {
        return;
    }
    ssize_t n = send(conn->fd, conn->send_buf, conn->send_len, MSG_NOSIGNAL);
    if (n <= 0)
    {
        LOG_WARN("send fail fd=%d", conn->fd);
        return;
    }
    // 滑动发送缓冲区
    if ((size_t)n < conn->send_len)
    {
        memmove(conn->send_buf, conn->send_buf + n, conn->send_len - (uint32_t)n);
        conn->send_len -= (uint32_t)n;
    }
    else
    {
        conn->send_len = 0;
    }
}

bool tcp_conn_send_pkt(TcpConn *conn, uint32_t cmd, uint32_t seq, const uint8_t *body, uint32_t body_len)
{
    if (!conn || conn->closed)
    {
        LOG_WARN("send_pkt skip closed conn");
        return false;
    }
    uint32_t total = packet_total_size(body_len);
    if (conn->send_len + total > TCP_CONN_SEND_BUF_SIZE)
    {
        LOG_ERROR("send buf overflow, fd=%d", conn->fd);
        return false;
    }
    uint8_t *p = conn->send_buf + conn->send_len;
    MsgHeader *hdr = (MsgHeader *)p;
    *hdr = (MsgHeader){
        .ver = 1,
        .encrypt = 0,
        .compress = 0,
        .reserved = 0,
        .cmd = cmd,
        .seq = seq,
        .body_len = body_len
    };
    packet_hton(hdr);
    if (body != NULL && body_len > 0)
    {
        memcpy(p + sizeof(MsgHeader), body, body_len);
    }
    conn->send_len += total;
    LOG_DEBUG("enqueue pkt cmd=%u seq=%u body_len=%u", cmd, seq, body_len);

    // 入队前发送缓冲区为空，立刻主动写一次；剩余数据依靠epoll EPOLLOUT(ET)
    if(conn->send_len == total)
    {
        tcp_conn_on_write(conn);
    }
    return true;
}

bool tcp_conn_check_timeout(struct TcpConn *conn, uint64_t now_ms)
{
    if (!conn || conn->closed)
    {
        return false;
    }
    uint64_t delta = now_ms - conn->last_recv_ts;
    if (delta >= TCP_CONN_HEARTBEAT_TIMEOUT_SEC * 1000U)
    {
        LOG_WARN("conn timeout, fd=%d, delta_ms=%llu", conn->fd, delta);
        return true;
    }
    return false;
}
