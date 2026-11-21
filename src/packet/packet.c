/**
 * @file packet.c
 * @brief 协议包头字节序转换实现
 */
#include "packet.h"
#include <arpa/inet.h>

void packet_hton(MsgHeader *hdr)
{
    if (!hdr)
    {
        return;
    }
    // 【学习点】网络通信标准：网络一律大端；主机序转网络序
    hdr->seq      = htons(hdr->seq);
    hdr->body_len = htonl(hdr->body_len);
}

void packet_ntoh(MsgHeader *hdr)
{
    if (!hdr)
    {
        return;
    }
    // 收到网络流，把大端转为本机CPU字节序
    hdr->seq      = ntohs(hdr->seq);
    hdr->body_len = ntohl(hdr->body_len);
}
