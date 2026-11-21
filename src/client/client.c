/**
 * @file client.c
 * 简单同步客户端，发送PING / ECHO / AI_TEXT
 * usage: ./client 127.0.0.1 8888
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "packet/packet.h"

int build_pkt(uint8_t *out, uint32_t cmd, uint32_t seq, const char *body)
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

int main(int argc, char** argv)
{
    if(argc != 3)
    {
        printf("usage: %s ip port\n", argv[0]);
        return -1;
    }
    const char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in srv = {0};
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    inet_pton(AF_INET, ip, &srv.sin_addr);
    if(connect(fd, (struct sockaddr *)&srv, sizeof(srv)) < 0)
    {
        perror("connect");
        return -1;
    }
    printf("connected to server\n");

    uint8_t buf[512];
    int len;
    // 1. PING
    len = build_pkt(buf, CMD_PING, 1001, NULL);
    send(fd, buf, len, 0);
    sleep(1);
    // 2. ECHO
    len = build_pkt(buf, CMD_ECHO, 1002, "hello mini msg");
    send(fd, buf, len, 0);
    sleep(1);
    // 3. AI_TEXT
    len = build_pkt(buf, CMD_AI_TEXT, 1003, "你好，介绍一下自己");
    send(fd, buf, len, 0);

    printf("packets sent, wait response...\n");
    char rbuf[512];
    ssize_t n;
    // 循环接收5秒，或者对端关闭就退出
    alarm(5);
    while ((n = recv(fd, rbuf, sizeof(rbuf), 0)) > 0)
    {
        printf("client recv %zd bytes\n", n);
    }
    alarm(0);

    close(fd);
    return 0;
}
