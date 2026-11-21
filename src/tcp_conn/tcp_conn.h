/**
 * @file tcp_conn.h
 * @brief MiniMsg-C TCP连接层：封装fd、parser、收发缓冲区、心跳超时、连接上下文
 * @learning C面向对象封装、IO多路复用配套连接对象、生命周期管理、心跳超时
 * @note 一条TCP连接对应一个TcpConn实例；和parser、cmd_dispatch联动
 * 【注意UB】禁止外部直接close fd；必须调用tcp_conn_destroy；多线程下外部要加锁保护conn
 */
#ifndef TCP_CONN_H
#define TCP_CONN_H

#include <stdint.h>
#include <stdbool.h>
#include "parser/parser.h"

#ifdef __cplusplus
extern "C" {
#endif

// 循环依赖：仅标签前置，只用指针，不include
struct ServiceCtx; 
// 在 typedef TcpConnCloseCb 之前，先前置标签！
struct TcpConn;   

#define TCP_CONN_HEARTBEAT_TIMEOUT_SEC 10U   // 心跳超时秒数，超时判定连接断开
#define TCP_CONN_SEND_BUF_SIZE 512U          // 发送缓冲区大小


/**
 * @brief 连接关闭回调：连接销毁时通知上层
 * @param conn 连接实例
 * @param user_data 上层私有数据
 */
typedef void (*TcpConnCloseCb)(struct TcpConn *conn, void *user_data);

/**
 * TcpConn：单连接上下文
 */
typedef struct TcpConn {
    int fd;                                     // socket文件描述符
    Parser parser;                              // 本连接专属协议解析器
    uint8_t send_buf[TCP_CONN_SEND_BUF_SIZE];
    uint32_t send_len;                          // 待发送数据长度
    uint64_t last_recv_ts;                      // 最后一次收到数据时间戳（ms）
    bool closed;                                // 标记是否已经关闭
    TcpConnCloseCb close_cb;
    void *close_cb_ud;
    struct ServiceCtx *svc_ctx;                 // 指向根服务上下文 ，✅ 指针，带 struct
    struct SessionMgr *sess_mgr;                //✅新增
} TcpConn;

/**
 * @brief 创建tcp连接对象，绑定fd
 * @param fd 已经accept好的socket fd
 * @param close_cb 连接关闭回调，可为NULL
 * @param cb_ud 回调私有数据
 * @return TcpConn* 成功返回实例，NULL失败
 */
TcpConn *tcp_conn_create(int fd, struct ServiceCtx *svc_ctx, TcpConnCloseCb close_cb, void *cb_ud);

/**
 * @brief 销毁连接：关闭fd、释放资源、触发close回调
 * @param conn 连接指针
 */
void tcp_conn_destroy(TcpConn *conn);

/**
 * @brief 读事件处理：recv数据，送入parser解析
 * @param conn 连接
 * @return true 正常；false 连接异常，需要销毁
 */
bool tcp_conn_on_read(TcpConn *conn);

/**
 * @brief 写事件处理：发送send_buf里缓存的数据
 * @param conn 连接
 */
void tcp_conn_on_write(struct TcpConn *conn);

/**
 * @brief 往连接发送数据包（组装包头+包体，写入send_buf）
 * @param conn 连接
 * @param cmd 命令号
 * @param seq 序列号
 * @param body 包体，NULL则无包体
 * @param body_len 包体长度
 * @return true 写入发送缓冲区成功；false 缓冲区满
 */
bool tcp_conn_send_pkt(TcpConn *conn, uint32_t cmd, uint32_t seq, const uint8_t *body, uint32_t body_len);

/**
 * @brief 心跳检测：判断是否超时
 * @param now_ms 当前时间戳ms
 * @return true 超时，需要关闭
 */
bool tcp_conn_check_timeout(TcpConn *conn, uint64_t now_ms);

#ifdef __cplusplus
}
#endif

#endif
