/**
 * @file session.h
 * @brief MiniMsg session 对话上下文链表
 * @learning 单向链表、会话绑定、对话历史存储、会话过期清理
 * @note 目前简化：每个TcpConn持有一个Session；断线可选择保留/销毁session
 */
#ifndef SESSION_H
#define SESSION_H

#include <stdint.h>
#include <stdbool.h>


#ifdef __cplusplus
extern "C" {
#endif

// 循环依赖：标签前置
struct TcpConn; 

#define SESSION_ID_LEN  32U
#define SESSION_MAX_HISTORY 16U   // 最多保存多少轮对话
#define SESSION_MSG_CONTENT_LEN 128U

/**
 * @brief 单条聊天消息记录
 */
typedef struct SessionMsg {
    bool is_user;    // true用户消息；false AI回复
    char content[SESSION_MSG_CONTENT_LEN];
} SessionMsg;

/**
 * @brief Session：一次会话，绑定TCP连接，保存对话历史
 */
typedef struct Session {
    char sid[SESSION_ID_LEN];                   // 会话id字符串
    uint64_t create_ts;                         // 创建时间ms
    uint64_t last_active_ts;                    // 最后活跃时间ms
    struct TcpConn *bind_conn;                  // 绑定的tcp连接，可以为NULL(离线)
    uint32_t msg_cnt;                           // 已保存消息数量
    SessionMsg history[SESSION_MAX_HISTORY];    // 历史消息数组
    struct Session *next;                       // 链表下一个会话
} Session;

/**
 * @brief SessionMgr会话管理器，链表管理全部Session
 */
typedef struct SessionMgr {
    struct Session *head;
    uint32_t total_cnt;
} SessionMgr;

/**
 * @brief 初始化会话管理器
 * @param mgr SessionMgr实例
 */
void session_mgr_init(SessionMgr *mgr);

/**
 * @brief 销毁会话管理器，释放全部会话
 * @param mgr SessionMgr实例
 */
void session_mgr_destroy(SessionMgr *mgr);

/**
 * @brief 创建新会话，绑定tcp连接
 * @param mgr SessionMgr实例
 * @param conn 绑定的tcp连接
 * @return 成功Session*，失败NULL
 */
Session *session_create(SessionMgr *mgr, struct TcpConn *conn);

/**
 * @brief 根据tcp连接查找对应的session
 * @param mgr SessionMgr实例
 * @param conn tcp连接
 * @return 找到返回Session*，找不到NULL
 */
Session *session_find_by_conn(SessionMgr *mgr, struct TcpConn *conn);

/**
 * @brief 根据sid字符串查找session
 * @param mgr SessionMgr实例
 * @param sid 会话id字符串
 * @return 找到返回Session*，找不到NULL
 */
Session *session_find_by_sid(SessionMgr *mgr, const char *sid);

/**
 * @brief 向session追加一条聊天消息
 * @param s Session实例
 * @param is_user true用户消息 falseAI回复
 * @param text 消息文本字符串
 */
void session_append_msg(Session *s, bool is_user, const char *text);

/**
 * @brief 解绑session与tcp连接，会话保留变为离线
 * @param s Session实例
 */
void session_unbind_conn(Session *s);

/**
 * @brief 销毁指定session，从链表摘除释放内存
 * @param mgr SessionMgr实例
 * @param s 需要销毁的session
 */
void session_destroy(SessionMgr *mgr, Session *s);

/**
 * @brief 扫描并销毁超时的会话
 * @param mgr SessionMgr实例
 * @param now_ms 当前时间戳ms
 * @param expire_ms 会话过期毫秒阈值
 */
void session_scan_expire(SessionMgr *mgr, uint64_t now_ms, uint64_t expire_ms);

#ifdef __cplusplus
}
#endif
#endif