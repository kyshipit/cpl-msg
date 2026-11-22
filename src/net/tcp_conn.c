/**
 * tcp_conn实现
 * tcp连接读写逻辑，对接parser，管理发送缓冲区EPOLLOUT事件
 */
#include "net/reactor.h"
#include "net/tcp_conn.h"
#include "core/log.h"
#include "protocol/packet.h"
#include "dispatch/cmd_dispatch.h"
#include "net/service_ctx.h"
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <assert.h>

// 获取系统单调时钟毫秒时间
static uint64_t get_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

// parser收到完整数据包回调，投递给命令分发模块
static void tcp_conn_on_packet(struct Parser *parser, const MsgHeader *hdr, const uint8_t *body, void *user_data)
{
    (void)parser;
    assert(user_data != NULL);
    TcpConn *conn = (TcpConn *)user_data;
    if (!conn || conn->closed)
    {
        return;
    }
    HandlerCtx hctx = {
        .svc_ctx = conn->svc_ctx,
        .conn = conn,
        .sess_mgr = conn->sess_mgr
    };
    cmd_dispatch(hdr, body, &hctx);
}

// 创建tcp连接实例，初始化parser
TcpConn *tcp_conn_create(int fd, struct ServiceCtx *svc_ctx, Mempool *mp, TcpConnCloseCb close_cb, void *cb_ud)
{
    assert(fd >= 0);
    if (fd < 0)
    {
        LOG_ERROR("tcp_conn_create invalid fd");
        return NULL;
    }
    TcpConn *conn = malloc(sizeof(TcpConn));
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
    conn->svc_ctx = svc_ctx;
    conn->sess_mgr = NULL;
    conn->reactor = NULL;
    conn->last_recv_ts = get_ms();
    conn->send_len = 0;
    parser_init(&conn->parser, mp, tcp_conn_on_packet, conn);
    LOG_INFO("tcp_conn create ok, fd=%d", fd);
    return conn;
}

// 销毁tcp连接，调用reactor_del清理fd_map，关闭socket，执行回调
void tcp_conn_destroy(TcpConn *conn)
{
    if (!conn || conn->closed)
        return;
    LOG_INFO("tcp_conn destroy, fd=%d", conn->fd);
    conn->closed = true;
    if(conn->reactor != NULL)
    {
        reactor_del(conn->reactor, conn->fd);
    }
    close(conn->fd);
    parser_reset(&conn->parser);
    if (conn->close_cb)
    {
        conn->close_cb(conn, conn->close_cb_ud);
    }
    free(conn);
}

// socket读处理，循环recv直到EAGAIN
bool tcp_conn_on_read(TcpConn *conn)
{
    assert(conn != NULL && !conn->closed);
    if (!conn || conn->closed)
        return false;
    uint8_t tmp_buf[256];
    ssize_t n;
    for (;;)
    {
        n = recv(conn->fd, tmp_buf, sizeof(tmp_buf), 0);
        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            LOG_WARN("recv error fd=%d", conn->fd);
            return false;
        }
        if (n == 0)
        {
            LOG_INFO("peer close fd=%d", conn->fd);
            return false;
        }
        conn->last_recv_ts = get_ms();
        parser_input(&conn->parser, tmp_buf, (uint32_t)n);
    }
    return true;
}

// socket写处理，发送发送缓冲区剩余数据；写完关闭EPOLLOUT
void tcp_conn_on_write(TcpConn *conn)
{
    assert(conn != NULL && !conn->closed);
    if (!conn || conn->closed || conn->send_len == 0)
        return;
    for(;;)
    {
        ssize_t n = send(conn->fd, conn->send_buf, conn->send_len, MSG_NOSIGNAL);
        if(n < 0)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            LOG_WARN("send fail fd=%d", conn->fd);
            return;
        }
        if ((size_t)n < conn->send_len)
        {
            memmove(conn->send_buf, conn->send_buf + n, conn->send_len - (uint32_t)n);
            conn->send_len -= (uint32_t)n;
        }
        else
        {
            conn->send_len = 0;
            break;
        }
    }
    // 全部发送完毕，关闭EPOLLOUT，只保留读事件
    if(conn->reactor != NULL && conn->send_len == 0)
    {
        reactor_mod(conn->reactor, conn->fd, EPOLLIN | EPOLLET);
    }
}

// reactor事件回调分发入口
void reactor_io_cb(int fd, uint32_t revents, void *ud)
{
    (void)fd;
    assert(ud != NULL);
    TcpConn *conn = (struct TcpConn *)ud;
    if(conn->closed)
        return;
    if (revents & (EPOLLERR | EPOLLHUP))
    {
        tcp_conn_destroy(conn);
        return;
    }
    if (revents & EPOLLIN)
    {
        bool ok = tcp_conn_on_read(conn);
        if (!ok)
        {
            tcp_conn_destroy(conn);
            return;
        }
    }
    if (revents & EPOLLOUT)
    {
        tcp_conn_on_write(conn);
    }
}

// 组装数据包入发送缓冲区；有残留待发数据则打开EPOLLOUT监听
bool tcp_conn_send_pkt(TcpConn *conn, uint32_t cmd, uint32_t seq, const uint8_t *body, uint32_t body_len)
{
    assert(conn != NULL && !conn->closed);
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
    uint32_t old_len = conn->send_len;
    conn->send_len += total;
    LOG_DEBUG("enqueue pkt cmd=%u seq=%u body_len=%u", cmd, seq, body_len);

    // 之前缓冲区为空，尝试直接写一次
    if(old_len == 0)
    {
        tcp_conn_on_write(conn);
    }
    // 写完还有残留待发送，开启EPOLLOUT等待可写事件
    if(conn->reactor != NULL && conn->send_len > 0)
    {
        reactor_mod(conn->reactor, conn->fd, EPOLLIN | EPOLLOUT | EPOLLET);
    }
    return true;
}

// 判断连接心跳是否超时
bool tcp_conn_check_timeout(TcpConn *conn, uint64_t now_ms)
{
    assert(conn != NULL && !conn->closed);
    if (!conn || conn->closed)
        return false;
    uint64_t delta = now_ms - conn->last_recv_ts;
    if (delta >= TCP_CONN_HEARTBEAT_TIMEOUT_SEC * 1000U)
    {
        LOG_WARN("conn timeout, fd=%d, delta_ms=%lu", conn->fd, delta);
        return true;
    }
    return false;
}
