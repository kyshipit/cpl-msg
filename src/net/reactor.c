/**
 * reactor实现
 * epoll事件循环，eventfd用于线程退出唤醒；数组fd_map保存每个fd回调
 */
#include "net/reactor.h"
#include "core/log.h"
#include <unistd.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <errno.h>
#include <assert.h>

// reactor工作线程主循环
static void* reactor_thread_entry(void *arg)
{
    assert(arg != NULL);
    Reactor *r = arg;
    struct epoll_event evs[REACTOR_MAX_EVENTS];

    while (r->running)
    {
        int n = epoll_wait(r->epfd, evs, REACTOR_MAX_EVENTS, 100);
        if (n <= 0)
            continue;

        for(int i = 0; i < n; i++)
        {
            int fd = evs[i].data.fd;
            uint32_t rev = evs[i].events;

            if(fd == r->efd)
            {
                uint64_t tmp;
                read(r->efd, &tmp, sizeof(tmp));
                r->running = false;
                // 修复：读到退出事件立刻跳出，不再处理本轮剩余事件
                break;
            }

            if(fd >=0 && fd < REACTOR_FD_MAP_SIZE)
            {
                ReactorFdEntry *ent = &r->fd_map[fd];
                if(ent->cb != NULL)
                {
                    ent->cb(fd, rev, ent->ud);
                }
            }
        }
    }
    LOG_DEBUG("reactor thread exit, tid=%lu", (unsigned long)pthread_self());
    return NULL;
}

// 添加fd到epoll，保存回调和ud到fd_map
int reactor_add(Reactor *r, int fd, uint32_t events, ReactorCb cb, void *ud)
{
    assert(r != NULL);
    if(r == NULL || r->destroyed)
        return -1;
    if(fd <0 || fd >= REACTOR_FD_MAP_SIZE)
    {
        LOG_ERROR("reactor_add fd out of range %d", fd);
        return -1;
    }
    r->fd_map[fd].cb = cb;
    r->fd_map[fd].ud = ud;

    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = events;
    return epoll_ctl(r->epfd, EPOLL_CTL_ADD, fd, &ev);
}

// 修改fd监听的epoll事件掩码
int reactor_mod(Reactor *r, int fd, uint32_t events)
{
    assert(r != NULL);
    if(r == NULL || r->destroyed)
        return -1;
    if(fd <0 || fd >= REACTOR_FD_MAP_SIZE)
        return -1;
    struct epoll_event ev;
    ev.data.fd = fd;
    ev.events = events;
    return epoll_ctl(r->epfd, EPOLL_CTL_MOD, fd, &ev);
}

// 删除fd，清空fd_map存储的回调，避免fd复用时野回调
void reactor_del(Reactor *r, int fd)
{
    if(r == NULL || r->destroyed)
        return;
    if(fd >=0 && fd < REACTOR_FD_MAP_SIZE)
    {
        r->fd_map[fd].cb = NULL;
        r->fd_map[fd].ud = NULL;
    }
    epoll_ctl(r->epfd, EPOLL_CTL_DEL, fd, NULL);
}

// 启动reactor，创建工作线程
int reactor_run(Reactor *r)
{
    assert(r != NULL);
    if(r == NULL || r->destroyed)
        return -1;
    r->running = true;
    int ret = pthread_create(&r->tid, NULL, reactor_thread_entry, r);
    if(ret !=0)
    {
        r->running = false;
        LOG_ERROR("reactor pthread_create fail");
        return -1;
    }
    return 0;
}

// 通过eventfd唤醒停止reactor
// is_self=true：调用上下文就是本reactor线程，只写唤醒，禁止pthread_join防止自等待死锁
void reactor_stop(Reactor *r, bool is_self)
{
    if(r == NULL || r->destroyed || !r->running)
        return;
    uint64_t one = 1;
    write(r->efd, &one, sizeof(one));
    if (!is_self)
    {
        pthread_join(r->tid, NULL);
    }
}

// 销毁reactor，停止线程，关闭epfd、efd
void reactor_destroy(Reactor *r)
{
    if(r == NULL || r->destroyed)
        return;
    reactor_stop(r, false);
    close(r->epfd);
    close(r->efd);
    r->destroyed = true;
    LOG_INFO("reactor destroy done");
}
