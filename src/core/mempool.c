/**
 * mempool实现
 * 空闲链表管理固定大小内存块
 */
#include "core/mempool.h"
#include "core/log.h"
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

// 完整内存块结构，头部+用户可用数据区
typedef struct {
    MemBlockHeader hdr;
    uint8_t data[MEMPOOL_BLOCK_SIZE];
} MemBlock;

struct Mempool {
    uint32_t block_cnt;
    MemBlock *blocks;
    MemBlockHeader *free_list;
    MempoolStat stat;
};

// 新建内存池
Mempool* mempool_create(uint32_t block_cnt)
{
    assert(block_cnt > 0);
    if(block_cnt == 0)
    {
        LOG_WARN("mempool_create block_cnt zero");
        return NULL;
    }
    Mempool *mp = malloc(sizeof(Mempool));
    if(!mp)
        return NULL;
    mp->blocks = malloc(sizeof(MemBlock) * block_cnt);
    if(!mp->blocks)
    {
        free(mp);
        return NULL;
    }
    memset(mp->blocks, 0, sizeof(MemBlock)*block_cnt);
    mp->block_cnt = block_cnt;
    mp->free_list = NULL;
    mp->stat.total = block_cnt;
    mp->stat.free = block_cnt;
    mp->stat.alloc_cnt = 0;
    mp->stat.free_cnt = 0;

    for(int i = (int)block_cnt - 1; i >= 0; i--)
    {
        MemBlockHeader *h = &mp->blocks[i].hdr;
        h->used = false;
        h->next = mp->free_list;
        mp->free_list = h;
    }
    LOG_INFO("mempool_create ok, block_size=%u, block_cnt=%u", MEMPOOL_BLOCK_SIZE, block_cnt);
    return mp;
}

// 销毁内存池
void mempool_destroy(Mempool *mp)
{
    if(!mp)
        return;
    free(mp->blocks);
    free(mp);
    LOG_INFO("mempool_destroy done");
}

// 分配内存块
void *mempool_alloc(Mempool *mp)
{
    assert(mp != NULL);
    if(!mp || mp->free_list == NULL)
    {
        LOG_WARN("mempool alloc fail: no free block");
        return NULL;
    }
    MemBlockHeader *hdr = mp->free_list;
    mp->free_list = hdr->next;
    hdr->used = true;
    mp->stat.free--;
    mp->stat.alloc_cnt++;
    MemBlock *b = (MemBlock *)hdr;
    return &b->data[0];
}

// 归还内存块
void mempool_free(Mempool *mp, void *ptr)
{
    assert(mp != NULL && ptr != NULL);
    if(!mp || ptr == NULL)
    {
        LOG_WARN("mempool_free null ptr");
        return;
    }
    MemBlock *b = (MemBlock*)((uint8_t*)ptr - offsetof(MemBlock, data));
    MemBlockHeader *hdr = &b->hdr;
    if(hdr->used == false)
    {
        LOG_ERROR("mempool double free detected!");
        return;
    }
    hdr->used = false;
    hdr->next = mp->free_list;
    mp->free_list = hdr;
    mp->stat.free++;
    mp->stat.free_cnt++;
    LOG_DEBUG("mempool free block back to freelist");
}

// 获取统计信息
void mempool_get_stat(Mempool *mp, MempoolStat *stat)
{
    assert(mp != NULL && stat != NULL);
    if(!mp || !stat)
        return;
    *stat = mp->stat;
}
