/**
 * packet实现
 * 包头大小端转换
 */
#include "protocol/packet.h"
#include <arpa/inet.h>

// 主机序转网络大端序，发送前调用
void packet_hton(MsgHeader *hdr)
{
    if (!hdr)
    {
        return;
    }
    hdr->seq      = htons(hdr->seq);
    hdr->body_len = htonl(hdr->body_len);
}

// 网络大端序转主机序，收到完整包头调用
void packet_ntoh(MsgHeader *hdr)
{
    if (!hdr)
    {
        return;
    }
    hdr->seq      = ntohs(hdr->seq);
    hdr->body_len = ntohl(hdr->body_len);
}
