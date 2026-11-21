/**
 * @file session.c
 * @brief session链表实现，对话历史管理
 */
#define _GNU_SOURCE
#include <inttypes.h>
#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "session/session.h"
#include "log/log.h"
#include "tcp_conn/tcp_conn.h"

static uint64_t get_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000 + (uint64_t)ts.tv_nsec / 1000000;
}

// 简单生成sid：用时间+随机数，教学够用
static void gen_sid(char *buf, uint32_t len)
{
    uint64_t t = get_ms();
    //snprintf(buf, len, "sess-%llu", t);
    snprintf(buf, len, "sess-%" PRIu64, t);
}

void session_mgr_init(SessionMgr *mgr)
{
    mgr->head = NULL;
    mgr->total_cnt = 0;
    LOG_INFO("session mgr init");
}

Session *session_create(SessionMgr *mgr, struct TcpConn *conn)
{
    Session *s = malloc(sizeof(Session));
    if (!s)
    {
        LOG_ERROR("malloc session fail");
        return NULL;
    }
    memset(s, 0, sizeof(Session));
    gen_sid(s->sid, SESSION_ID_LEN);
    uint64_t now = get_ms();
    s->create_ts = now;
    s->last_active_ts = now;
    s->bind_conn = conn;
    s->next = mgr->head;
    mgr->head = s;
    mgr->total_cnt++;
    LOG_INFO("session created, sid=%s, bind fd=%d", s->sid, conn ? conn->fd : -1);
    return s;
}

Session *session_find_by_conn(SessionMgr *mgr, struct TcpConn *conn)
{
    Session *p = mgr->head;
    while (p)
    {
        if (p->bind_conn == conn)
        {
            return p;
        }
        p = p->next;
    }
    return NULL;
}

Session *session_find_by_sid(SessionMgr *mgr, const char *sid)
{
    Session *p = mgr->head;
    while (p)
    {
        if (strcmp(p->sid, sid) == 0)
        {
            return p;
        }
        p = p->next;
    }
    return NULL;
}

void session_append_msg(Session *s, bool is_user, const char *text)
{
    if (!s || !text) return;
    s->last_active_ts = get_ms();

    // 满了，整体往前挪，丢弃最早一条
    if (s->msg_cnt >= SESSION_MAX_HISTORY)
    {
        for (uint32_t i = 1; i < SESSION_MAX_HISTORY; i++)
        {
            s->history[i - 1] = s->history[i];
        }
        s->msg_cnt = SESSION_MAX_HISTORY - 1;
    }
    uint32_t idx = s->msg_cnt;
    s->history[idx].is_user = is_user;
    strncpy(s->history[idx].content, text, SESSION_MSG_CONTENT_LEN - 1);
    s->history[idx].content[SESSION_MSG_CONTENT_LEN - 1] = '\0';
    s->msg_cnt++;

    LOG_DEBUG("session append msg, is_user=%d, content=%s", is_user, text);
}

void session_unbind_conn(Session *s)
{
    if (!s) return;
    s->bind_conn = NULL;
    LOG_INFO("session %s unbind conn", s->sid);
}

void session_destroy(SessionMgr *mgr, Session *s)
{
    if (!mgr || !s) return;
    Session **p = &mgr->head;
    while (*p)
    {
        if (*p == s)
        {
            *p = s->next;
            mgr->total_cnt--;
            LOG_INFO("session destroy sid=%s", s->sid);
            free(s);
            return;
        }
        p = &(*p)->next;
    }
}

void session_scan_expire(SessionMgr *mgr, uint64_t now_ms, uint64_t expire_ms)
{
    Session **p = &mgr->head;
    while (*p)
    {
        Session *cur = *p;
        if (now_ms - cur->last_active_ts >= expire_ms)
        {
            LOG_INFO("session expire, sid=%s", cur->sid);
            *p = cur->next;
            mgr->total_cnt--;
            free(cur);
        }
        else
        {
            p = &cur->next;
        }
    }
}

void session_mgr_destroy(SessionMgr *mgr)
{
    Session *p = mgr->head;
    while (p)
    {
        Session *tmp = p;
        p = p->next;
        free(tmp);
    }
    mgr->head = NULL;
    mgr->total_cnt = 0;
    LOG_INFO("session mgr destroyed");
}
