/**
 * tcp_conn
 * 单条tcp连接上下文封装，管理读写缓冲区、parser解析器
 */
#ifndef TCP_CONN_H
#define TCP_CONN_H
#include <stdint.h>
#include <stdbool.h>
#include "protocol/parser.h"
#ifdef __cplusplus
extern "C" {
#endif

struct TcpConn;
struct ServiceCtx;
struct SessionMgr;
struct Reactor;

#define TCP_CONN_HEARTBEAT_TIMEOUT_SEC 10U
#define TCP_CONN_SEND_BUF_SIZE 512U

// 连接关闭回调原型
typedef void (*TcpConnCloseCb)(struct TcpConn *conn, void *user_data);

typedef struct TcpConn {
    int fd;
    Parser parser;
    uint8_t send_buf[TCP_CONN_SEND_BUF_SIZE];
    uint32_t send_len;
    uint64_t last_recv_ts;
    bool closed;
    TcpConnCloseCb close_cb;
    void *close_cb_ud;
    struct ServiceCtx *svc_ctx;
    struct SessionMgr *sess_mgr;
    struct Reactor *reactor;
} TcpConn;

// 创建tcp连接对象，初始化parser
TcpConn *tcp_conn_create(int fd, struct ServiceCtx *svc_ctx, Mempool *mp, TcpConnCloseCb close_cb, void *cb_ud);

// 销毁tcp连接，关闭fd，清理parser，执行关闭回调
void tcp_conn_destroy(TcpConn *conn);

// 处理socket读事件，循环读取直到EAGAIN
bool tcp_conn_on_read(TcpConn *conn);

// 处理socket写事件，发送用户态缓冲区数据
void tcp_conn_on_write(TcpConn *conn);

// 组装数据包追加到发送缓冲区，管理EPOLLOUT事件开关
bool tcp_conn_send_pkt(TcpConn *conn, uint32_t cmd, uint32_t seq, const uint8_t *body, uint32_t body_len);

// 根据当前时间戳检测连接心跳是否超时
bool tcp_conn_check_timeout(TcpConn *conn, uint64_t now_ms);

// reactor IO事件回调入口函数
void reactor_io_cb(int fd, uint32_t events, void *arg);

#ifdef __cplusplus
}
#endif
#endif
