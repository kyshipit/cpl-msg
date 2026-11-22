/**
 * session
 * 会话管理，保存每个连接对话历史，支持会话过期清理
 */
#ifndef SESSION_H
#define SESSION_H
#include <stdint.h>
#include <stdbool.h>
#ifdef __cplusplus
extern "C" {
#endif

struct TcpConn;

#define SESSION_ID_LEN  32U
#define SESSION_MAX_HISTORY 16U
#define SESSION_MSG_CONTENT_LEN 128U

// 单条聊天消息
typedef struct SessionMsg {
    bool is_user;
    char content[SESSION_MSG_CONTENT_LEN];
} SessionMsg;

// 会话实例，绑定tcp连接，保存对话历史
typedef struct Session {
    char sid[SESSION_ID_LEN];
    uint64_t create_ts;
    uint64_t last_active_ts;
    struct TcpConn *bind_conn;
    uint32_t msg_cnt;
    SessionMsg history[SESSION_MAX_HISTORY];
    struct Session *next;
} Session;

// 会话管理器，链表管理全部session
typedef struct SessionMgr {
    struct Session *head;
    uint32_t total_cnt;
} SessionMgr;

// 初始化会话管理器
void session_mgr_init(SessionMgr *mgr);

// 销毁管理器，释放全部会话
void session_mgr_destroy(SessionMgr *mgr);

// 创建新会话，绑定tcp连接
Session *session_create(SessionMgr *mgr, struct TcpConn *conn);

// 通过tcp连接查找对应session
Session *session_find_by_conn(SessionMgr *mgr, struct TcpConn *conn);

// 通过session id查找会话
Session *session_find_by_sid(SessionMgr *mgr, const char *sid);

// 追加一条聊天消息到会话历史
void session_append_msg(Session *s, bool is_user, const char *text);

// 解除会话与tcp连接绑定
void session_unbind_conn(Session *s);

// 销毁指定会话
void session_destroy(SessionMgr *mgr, Session *s);

// 扫描并清理过期会话
void session_scan_expire(SessionMgr *mgr, uint64_t now_ms, uint64_t expire_ms);

#ifdef __cplusplus
}
#endif
#endif
