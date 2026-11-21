/**
 * @file conn_mgr.c
 * @brief epoll + 连接管理实现
 */
#define _GNU_SOURCE   //`_GNU_SOURCE` 开启全部 Linux 扩展，同时提供 `clock_gettime`、`CLOCK_MONOTONIC`、`accept4`、`SOCK_NONBLOCK`
#include <time.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "conn_mgr/conn_mgr.h"
#include "log/log.h"
#include "tcp_conn/tcp_conn.h"
#include "session/session.h"
#include "service_ctx/service_ctx.h"

static uint64_t get_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

// 未使用静态函数，加unused消除警告
static int set_nonblock(int fd) __attribute__((unused));
static int set_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void conn_close_cb(struct TcpConn *conn, void *ud)
{
    ConnMgr *mgr = (ConnMgr *)ud;
    conn_mgr_remove_conn(mgr, conn);
}

bool conn_mgr_init(struct ConnMgr *mgr, uint16_t port, ServiceCtx *svc_ctx)
{
    //memset(mgr, 0, sizeof(ConnMgr));
    mgr->svc_ctx = svc_ctx;

    mgr->epoll_fd = epoll_create1(0);
    if (mgr->epoll_fd < 0)
    {
        LOG_ERROR("epoll_create1 fail");
        return false;
    }

    int lfd = socket(AF_INET, SOCK_STREAM, 0);
    if (lfd < 0)
    {
        LOG_ERROR("socket create fail");
        close(mgr->epoll_fd);
        return false;
    }
    set_nonblock(lfd);

    int opt = 1;
    setsockopt(lfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in addr = {0};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(lfd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        LOG_ERROR("bind port %u fail", port);
        close(lfd);
        close(mgr->epoll_fd);
        return false;
    }
    if (listen(lfd, 128) < 0)
    {
        LOG_ERROR("listen fail");
        close(lfd);
        close(mgr->epoll_fd);
        return false;
    }
    mgr->listen_fd = lfd;

    struct epoll_event ev;
    ev.data.fd = lfd;
    ev.events = EPOLLIN;
    epoll_ctl(mgr->epoll_fd, EPOLL_CTL_ADD, lfd, &ev);
    mgr->last_scan_ts = get_ms();

    //session_mgr_init(mgr->sess_mgr);  //新加初始化会话管理器

    LOG_INFO("conn_mgr init ok, listen port=%u", port);
    return true;
}

bool conn_mgr_add_conn(struct ConnMgr *mgr, struct TcpConn *conn)
{
    if (mgr->conn_cnt >= CONN_MGR_MAX_CONN)
    {
        LOG_WARN("conn list full, reject new conn");
        return false;
    }
    struct epoll_event ev;
    ev.data.ptr = conn;
    ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
    if (epoll_ctl(mgr->epoll_fd, EPOLL_CTL_ADD, conn->fd, &ev) < 0)
    {
        LOG_ERROR("epoll add conn fd=%d fail", conn->fd);
        return false;
    }
    mgr->conn_list[mgr->conn_cnt++] = conn;
    LOG_INFO("add conn fd=%d, online=%u", conn->fd, mgr->conn_cnt);
    return true;
}

void conn_mgr_remove_conn(struct ConnMgr *mgr, struct TcpConn *conn)
{
    uint32_t i = 0;
    for (; i < mgr->conn_cnt; i++)
    {
        if (mgr->conn_list[i] == conn)
        {
            epoll_ctl(mgr->epoll_fd, EPOLL_CTL_DEL, conn->fd, NULL);
            // 尾部覆盖删除
            mgr->conn_list[i] = mgr->conn_list[mgr->conn_cnt - 1];
            mgr->conn_cnt--;
            LOG_INFO("remove conn fd=%d, online=%u", conn->fd, mgr->conn_cnt);
            return;
        }
    }
}

static void conn_mgr_accept(struct ConnMgr *mgr)
{
    struct sockaddr_in cli;
    socklen_t len = sizeof(cli);
    while (1)
    {
        int cfd = accept4(mgr->listen_fd, (struct sockaddr *)&cli, &len, SOCK_NONBLOCK);
        if (cfd < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            LOG_WARN("accept error");
            break;
        }
        TcpConn *conn = tcp_conn_create(cfd, mgr->svc_ctx, conn_close_cb, mgr);
        if (!conn)
        {
            close(cfd);
            continue;
        }
        conn->sess_mgr = mgr->sess_mgr; // ✅赋值
        session_create(mgr->sess_mgr, conn);  // 新建session，绑定这个conn
        if (!conn_mgr_add_conn(mgr, conn))
        {
            tcp_conn_destroy(conn);
        }
    }
}

static void conn_mgr_scan_timeout(struct ConnMgr *mgr, uint64_t now)
{
    if (now - mgr->last_scan_ts < CONN_MGR_SCAN_INTERVAL_MS)
        return;
    mgr->last_scan_ts = now;

    // 注意：遍历的时候不能直接删，先标记
    uint32_t i = 0;
    while (i < mgr->conn_cnt)
    {
        TcpConn *c = mgr->conn_list[i];
        if (tcp_conn_check_timeout(c, now))
        {
            tcp_conn_destroy(c);
            // destroy内部会回调 conn_close_cb -> remove_conn
        }
        else
        {
            i++;
        }
    }
    // 新增：session过期清理，30分钟无活动
    session_scan_expire(mgr->sess_mgr, now, 30 * 60 * 1000ULL);
}

void conn_mgr_run_loop(struct ConnMgr *mgr)
{
    struct epoll_event events[CONN_MGR_EPOLL_MAX_EVENTS];
    while (1)
    {
        int n = epoll_wait(mgr->epoll_fd, events, CONN_MGR_EPOLL_MAX_EVENTS, 100);
        uint64_t now = get_ms();
        conn_mgr_scan_timeout(mgr, now);

        if (n <= 0)
            continue;

        for (int i = 0; i < n; i++)
        {
            struct epoll_event *ev = &events[i];
            if (ev->data.fd == mgr->listen_fd)
            {
                conn_mgr_accept(mgr);
            }
            else
            {
                TcpConn *conn = (TcpConn *)ev->data.ptr;
                if (ev->events & (EPOLLIN | EPOLLERR | EPOLLHUP))
                {
                    bool ok = tcp_conn_on_read(conn);
                    if (!ok)
                    {
                        tcp_conn_destroy(conn);
                        continue;
                    }
                }
                if (ev->events & EPOLLOUT)
                {
                    tcp_conn_on_write(conn);
                }
            }
        }
    }
}

void conn_mgr_destroy(struct ConnMgr *mgr)
{
    for (uint32_t i = 0; i < mgr->conn_cnt; i++)
    {
        tcp_conn_destroy(mgr->conn_list[i]);
    }
    mgr->conn_cnt = 0;
    close(mgr->listen_fd);
    close(mgr->epoll_fd);
    session_mgr_destroy(mgr->sess_mgr); //销毁全部会话
    LOG_INFO("conn_mgr destroyed");
}
