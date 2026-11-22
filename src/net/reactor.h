/**
 * reactor
 * epoll事件循环，每个实例绑定独立工作线程
 */
#ifndef REACTOR_H
#define REACTOR_H
#include <stdint.h>
#include <stdbool.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <assert.h>
#ifdef __cplusplus
extern "C" {
#endif

#define REACTOR_MAX_EVENTS 128
/* demo限制：fd不可超过1023，生产替换为动态哈希表 */
#define REACTOR_FD_MAP_SIZE 1024

// fd事件回调原型
typedef void (*ReactorCb)(int fd, uint32_t revents, void *ud);

// 保存每个fd绑定的回调与私有参数
typedef struct {
    ReactorCb cb;
    void *ud;
} ReactorFdEntry;

typedef struct Reactor {
    int epfd;
    int efd;
    pthread_t tid;
    bool running;
    bool destroyed;
    ReactorFdEntry fd_map[REACTOR_FD_MAP_SIZE];
} Reactor;

// 向epoll注册fd，绑定回调和用户参数
int reactor_add(Reactor *r, int fd, uint32_t events, ReactorCb cb, void *ud);

// 修改fd监听事件掩码
int reactor_mod(Reactor *r, int fd, uint32_t events);

// 删除fd，清空回调，防止野指针
void reactor_del(Reactor *r, int fd);

// 启动reactor线程
int reactor_run(Reactor *r);

// 停止reactor
// is_self=true：当前执行线程就是该reactor线程，只唤醒，不pthread_join
// is_self=false：外部线程调用，唤醒 + pthread_join等待线程退出
void reactor_stop(Reactor *r, bool is_self);

// 销毁资源：停止线程、关闭fd
void reactor_destroy(Reactor *r);

// tcp连接io统一回调入口
void reactor_io_cb(int fd, uint32_t events, void *arg);

#ifdef __cplusplus
}
#endif
#endif
