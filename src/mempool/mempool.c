/**
 * @file mempool.c
 * @brief 固定块内存池实现，空闲链表管理
 */
#include "mempool/mempool.h"
#include "log/log.h"
#include <string.h>
#include <stddef.h>   //offsetof头文件

// static全局变量：仅本文件可见，封装内存池数据，外部无法直接访问
// 每个块空间 = 块头 + 用户可用空间
typedef struct {
    MemBlockHeader hdr;
    uint8_t data[MEMPOOL_BLOCK_SIZE];
} MemBlock;

static MemBlock g_pool[MEMPOOL_BLOCK_CNT];
static MemBlockHeader *g_free_list = NULL;
static MempoolStat g_stat = {0};

void mempool_init(void)
{
    // 初始化空闲链表，从最后一块往前串，方便头插
    g_free_list = NULL;
    memset(g_pool, 0, sizeof(g_pool));
    g_stat.total = MEMPOOL_BLOCK_CNT;
    g_stat.free = MEMPOOL_BLOCK_CNT;
    g_stat.alloc_cnt = 0;
    g_stat.free_cnt = 0;

    for (int i = MEMPOOL_BLOCK_CNT - 1; i >= 0; i--)
    {
        MemBlockHeader *h = &g_pool[i].hdr;
        h->used = false;
        h->next = g_free_list;
        g_free_list = h;
    }
    LOG_INFO("mempool init done, block_size=%u, block_cnt=%u", MEMPOOL_BLOCK_SIZE, MEMPOOL_BLOCK_CNT);
}

void *mempool_alloc(void)
{
    if (g_free_list == NULL)
    {
        LOG_WARN("mempool alloc fail: no free block");
        return NULL;
    }
    // 取出链表头部空闲块
    MemBlockHeader *hdr = g_free_list;
    g_free_list = hdr->next;
    hdr->used = true;

    g_stat.free--;
    g_stat.alloc_cnt++;

    // 返回用户可用data起始地址（跳过header）
    MemBlock *b = (MemBlock *)hdr;
    return &b->data[0];
}

void mempool_free(void *ptr)
{
    if (ptr == NULL)
    {
        LOG_WARN("mempool_free skip null ptr");
        return;
    }
    // 指针回退拿到块头：用户data指针往前偏移得到MemBlock起始地址
    MemBlock *b = (MemBlock *)((uint8_t *)ptr - offsetof(MemBlock, data));
    MemBlockHeader *hdr = &b->hdr;

    // double free防护
    if (hdr->used == false)
    {
        LOG_ERROR("mempool double free detected!");
        return;
    }

    // 标记空闲，头插回空闲链表
    hdr->used = false;
    hdr->next = g_free_list;
    g_free_list = hdr;

    g_stat.free++;
    g_stat.free_cnt++;
    LOG_DEBUG("mempool free block back to freelist");
}

void mempool_get_stat(MempoolStat *stat)
{
    if (!stat)
    {
        return;
    }
    *stat = g_stat;
}
