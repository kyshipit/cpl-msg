/**
 * master_worker实现
 * master监听accept，新连接轮询派发给worker；退出完全由主线程驱动
 */
#include "net/master_worker.h"
#include "core/log.h"
#include <unistd.h>
#include <sys/socket.h>
#include <sys/epoll.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/eventfd.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// 设置fd为非阻塞
static int set_nonblock(int fd)
{
    int flags = fcntl(fd, F_GETFL,0);
    if(flags<0) return -1;
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// 轮询选择下一个worker reactor
static Reactor* select_next_worker(MasterWorker *mw)
{
    assert(mw != NULL);
    uint32_t idx = mw->round_idx % mw->worker_cnt;
    mw->round_idx++;
    return mw->workers[idx];
}

// tcp连接销毁回调；短连接场景直接销毁session
static void conn_close_cb(TcpConn *conn, void *ud)
{
    assert(ud != NULL);
    MasterWorker *mw = (MasterWorker *)ud;
    mw->online_conn_cnt--;
    Session *sess = session_find_by_conn(conn->sess_mgr, conn);
    if(sess != NULL)
    {
        session_destroy(conn->sess_mgr, sess);
    }
}

// master reactor accept回调，接收新tcp连接
static void master_accept_cb(int fd, uint32_t rev, void *ud)
{
    assert(ud != NULL);
    MasterWorker *mw = (MasterWorker *)ud;
    if(!(rev & EPOLLIN)) return;
    struct sockaddr_in cli;
    socklen_t len = sizeof(cli);
    for(;;)
    {
        int cfd = accept4(fd, (struct sockaddr *)&cli, &len, SOCK_NONBLOCK);
        if(cfd <0)
        {
            if(errno == EAGAIN || errno == EWOULDBLOCK) break;
            LOG_WARN("accept error");
            break;
        }
        if(mw->online_conn_cnt >= MAX_ONLINE_CONN)
        {
            LOG_WARN("max online conn reach, reject new client");
            close(cfd);
            continue;
        }
        Reactor *wr = select_next_worker(mw);
        TcpConn *conn = tcp_conn_create(cfd, mw->svc_ctx, mw->svc_ctx->mp, conn_close_cb, mw);
        if(!conn)
        {
            close(cfd);
            continue;
        }
        conn->reactor = wr;
        conn->sess_mgr = mw->sess_mgr;
        session_create(mw->sess_mgr, conn);
        mw->online_conn_cnt++;
        if(reactor_add(wr, conn->fd, EPOLLIN|EPOLLET, reactor_io_cb, conn) <0)
        {
            tcp_conn_destroy(conn);
            mw->online_conn_cnt--;
        }
    }
}

// 初始化master‑worker
bool master_worker_init(MasterWorker *mw, ServiceCtx *svc, uint16_t port, uint32_t worker_num)
{
    assert(mw != NULL && svc != NULL);
    memset(mw,0,sizeof(MasterWorker));
    mw->svc_ctx = svc;
    mw->port = port;
    mw->worker_cnt = worker_num;
    mw->sess_mgr = svc->sess_mgr;
    mw->destroyed = false;
    mw->online_conn_cnt = 0;

    int lfd = socket(AF_INET, SOCK_STREAM,0);
    if(lfd<0)
    {
        LOG_ERROR("socket create fail");
        return false;
    }
    set_nonblock(lfd);
    int opt=1;
    setsockopt(lfd,SOL_SOCKET,SO_REUSEADDR,&opt,sizeof(opt));
    struct sockaddr_in addr={0};
    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);
    addr.sin_addr.s_addr=INADDR_ANY;
    if(bind(lfd,(struct sockaddr*)&addr,sizeof(addr))<0)
    {
        LOG_ERROR("bind port %u fail",port);
        close(lfd);
        return false;
    }
    if(listen(lfd,128)<0)
    {
        LOG_ERROR("listen fail");
        close(lfd);
        return false;
    }
    mw->listen_fd = lfd;

    for(uint32_t i=0;i<worker_num;i++)
    {
        Reactor *r = malloc(sizeof(Reactor));
        memset(r,0,sizeof(Reactor));
        r->epfd = epoll_create1(0);
        r->efd = eventfd(0, EFD_NONBLOCK);
        r->destroyed = false;
        reactor_add(r, r->efd, EPOLLIN, NULL, NULL);
        if(reactor_run(r) !=0)
        {
            reactor_destroy(r);
            free(r);
            close(lfd);
            return false;
        }
        mw->workers[i]=r;
    }

    Reactor *master_r = malloc(sizeof(Reactor));
    memset(master_r,0,sizeof(Reactor));
    master_r->epfd = epoll_create1(0);
    master_r->efd = eventfd(0, EFD_NONBLOCK);
    master_r->destroyed = false;
    reactor_add(master_r, master_r->efd, EPOLLIN, NULL, NULL);
    reactor_add(master_r, mw->listen_fd, EPOLLIN, master_accept_cb, mw);
    reactor_run(master_r);
    mw->master_reactor = master_r;

    LOG_INFO("master‑worker init ok, port=%u, worker=%u", port, worker_num);
    return true;
}

// 预留运行接口
void master_worker_run(MasterWorker *mw)
{
    (void)mw;
}

// 销毁master‑worker全部资源：主线程上下文，先关监听，再回收线程
void master_worker_destroy(MasterWorker *mw)
{
    if(!mw || mw->destroyed)
        return;
    // 修复：先关闭监听fd，停止接受新连接
    if(mw->listen_fd >= 0)
    {
        close(mw->listen_fd);
        mw->listen_fd = -1;
    }
    // 停止所有worker线程
    for(uint32_t i=0;i<mw->worker_cnt;i++)
    {
        if(mw->workers[i])
        {
            reactor_stop(mw->workers[i], false);
            reactor_destroy(mw->workers[i]);
            free(mw->workers[i]);
            mw->workers[i] = NULL;
        }
    }
    // 停止master reactor线程
    if(mw->master_reactor)
    {
        reactor_stop(mw->master_reactor, false);
        reactor_destroy(mw->master_reactor);
        free(mw->master_reactor);
        mw->master_reactor = NULL;
    }
    session_mgr_destroy(mw->sess_mgr);
    mw->destroyed = true;
    LOG_INFO("master‑worker destroy done");
}
