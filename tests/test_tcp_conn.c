/**
 * @file test_tcp_conn.c
 * @brief tcp_conn单元测试（模拟fd，这里演示接口调用，真实需要epoll）
 * 编译：gcc -std=c11 -Wall -Wextra -g -DDEBUG tests/test_tcp_conn.c src/tcp_conn/tcp_conn.c src/parser/parser.c src/cmd_dispatch/cmd_dispatch.c src/packet/packet.c src/mempool/mempool.c src/log/log.c -o test_tcp_conn
 */
#include "../src/tcp_conn/tcp_conn.h"
#include "../src/cmd_dispatch/cmd_dispatch.h"
#include "../src/mempool/mempool.h"
#include "../src/log/log.h"
#include <stdio.h>

static void on_close(struct TcpConn *conn, void *ud)
{
    (void)ud;
    LOG_INFO("conn close callback triggered, fd=%d", conn->fd);
}

int main(void)
{
    printf("===== Test tcp_conn module start =====\n");
    mempool_init();
    cmd_dispatch_register_table();

    // 注意：这里fd=-1仅用于编译测试，真实业务必须是accept返回合法fd
    struct TcpConn *conn = tcp_conn_create(-1, on_close, NULL);
    if (!conn)
    {
        LOG_ERROR("create conn fail");
        return -1;
    }

    // 测试发包入缓冲区
    tcp_conn_send_pkt(conn, CMD_ECHO, 2001, (const uint8_t *)"hello tcp_conn", 13);
    LOG_VAR(conn->send_len, conn->send_len);

    // 超时检测
    uint64_t now = 0;
    bool timeout = tcp_conn_check_timeout(conn, now);
    LOG_VAR(timeout, timeout);

    tcp_conn_destroy(conn);
    printf("===== Test tcp_conn module pass =====\n");
    return 0;
}
