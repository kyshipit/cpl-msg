/**
 * @file conn_mgr.h
 * @brief MiniMsg-C 连接管理器 + epoll事件循环
 * @learning 【学习点】epoll IO多路复用、监听fd管理、连接集合、定时心跳扫描、事件驱动主循环
 * @note 统一管理listen fd、所有TcpConn；负责accept、事件分发、超时清理
 * 【注意UB】不要在handler/回调里做阻塞操作；单线程epoll模型
 */
#ifndef CONN_MGR_H
#define CONN_MGR_H

#include <stdint.h>
#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif

// 循环依赖，只标签前置，禁止typedef，禁止include
struct TcpConn;
struct ServiceCtx;
struct SessionMgr;


#define CONN_MGR_MAX_CONN 64U               // 最大同时连接数
#define CONN_MGR_EPOLL_MAX_EVENTS 16U       // epoll单次最多取出事件数
#define CONN_MGR_SCAN_INTERVAL_MS 1000U     // 心跳扫描间隔（ms）

/**
 * @brief ConnMgr 连接管理器上下文
 */
typedef struct ConnMgr {
    int epoll_fd;
    int listen_fd;
    struct TcpConn *conn_list[CONN_MGR_MAX_CONN];
    uint32_t conn_cnt;
    uint64_t last_scan_ts;                      // 上一次心跳扫描时间ms
    struct SessionMgr *sess_mgr;                // 会话管理器指针
    struct ServiceCtx *svc_ctx;                 // 指向顶层服务上下文
} ConnMgr;

/**
 * @brief 初始化连接管理器，创建listen socket bind listen，epoll初始化
 * @param mgr ConnMgr实例
 * @param port 监听端口
 * @param svc_ctx 顶层服务上下文
 * @return true成功 false失败
 */
bool conn_mgr_init(ConnMgr *mgr, uint16_t port, struct ServiceCtx *svc_ctx);

/**
 * @brief 启动epoll主事件循环，阻塞运行
 * @param mgr ConnMgr实例
 */
void conn_mgr_run_loop(ConnMgr *mgr);

/**
 * @brief 销毁连接管理器，关闭fd，销毁全部tcp连接
 * @param mgr ConnMgr实例
 */
void conn_mgr_destroy(ConnMgr *mgr);

/**
 * @brief 将tcp连接加入管理器，注册epoll事件
 * @param mgr ConnMgr实例
 * @param conn TcpConn实例
 * @return true加入成功 false连接数已满
 */
bool conn_mgr_add_conn(ConnMgr *mgr, struct TcpConn *conn);

/**
 * @brief 从管理器移除tcp连接，epoll删除该fd事件
 * @param mgr ConnMgr实例
 * @param conn TcpConn实例
 */
void conn_mgr_remove_conn(ConnMgr *mgr, struct TcpConn *conn);

#ifdef __cplusplus
}
#endif
#endif
