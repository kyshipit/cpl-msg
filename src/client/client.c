/**
 * client
 * 交互式客户端，支持键盘输入发送AI_TEXT消息
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include "protocol/packet.h"
#include "protocol/parser.h"

static int g_fd = -1;
static uint16_t g_seq = 0;

// 收到服务端数据包回调打印
static void on_recv_pkt(struct Parser *parser, const MsgHeader *hdr, const uint8_t *body, void *ud)
{
    (void)parser;
    (void)ud;
    if(hdr->cmd == CMD_AI_TEXT)
    {
        printf("\n[AI回复] %.*s\n> ", (int)hdr->body_len, (const char *)body);
        fflush(stdout);
    }
    else
    {
        printf("\n[收到包] cmd=%u seq=%u len=%u\n> ", hdr->cmd, hdr->seq, hdr->body_len);
        fflush(stdout);
    }
}

// 后台收包线程
static void* recv_thread(void *arg)
{
    (void)arg;
    Parser parser;
    parser_init(&parser, NULL, on_recv_pkt, NULL);
    uint8_t rbuf[1024];
    ssize_t n;
    while((n = recv(g_fd, rbuf, sizeof(rbuf), 0)) > 0)
    {
        parser_input(&parser, rbuf, (uint32_t)n);
    }
    parser_reset(&parser);
    printf("\n连接已断开\n");
    exit(0);
    return NULL;
}

// 组装二进制数据包
static int build_pkt(uint8_t *out, uint32_t cmd, uint32_t seq, const char *body)
{
    uint32_t body_len = body ? strlen(body) : 0;
    MsgHeader *hdr = (MsgHeader *)out;
    *hdr = (MsgHeader){
        .ver = 1,
        .encrypt = 0,
        .compress = 0,
        .reserved = 0,
        .cmd = cmd,
        .seq = seq,
        .body_len = body_len
    };
    packet_hton(hdr);
    uint8_t *p = out + sizeof(MsgHeader);
    if (body && body_len > 0)
    {
        memcpy(p, body, body_len);
    }
    return sizeof(MsgHeader) + body_len;
}

// 客户端主函数
int main(int argc, char** argv)
{
    if(argc != 3)
    {
        printf("usage: %s ip port\n", argv[0]);
        return -1;
    }
    const char *ip = argv[1];
    uint16_t port = (uint16_t)atoi(argv[2]);
    g_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in srv = {0};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    inet_pton(AF_INET, ip, &srv.sin_addr);
    if(connect(g_fd, (struct sockaddr *)&srv, sizeof(srv)) < 0)
    {
        perror("connect");
        close(g_fd);
        return -1;
    }
    printf("已连接到服务器，输入消息发送，Ctrl+C退出\n> ");
    fflush(stdout);

    pthread_t tid;
    pthread_create(&tid, NULL, recv_thread, NULL);

    char line[256];
    while(fgets(line, sizeof(line), stdin) != NULL)
    {
        size_t len = strlen(line);
        if(len > 0 && line[len-1] == '\n')
            line[len-1] = '\0';
        if(strlen(line) == 0)
        {
            printf("> ");
            fflush(stdout);
            continue;
        }
        uint8_t buf[512];
        int pkt_len = build_pkt(buf, CMD_AI_TEXT, g_seq++, line);
        send(g_fd, buf, pkt_len, 0);
    }

    close(g_fd);
    return 0;
}
