/**
 * mempool
 * 固定大小块内存池，堆分配，支持多独立实例
 */
#ifndef MEMPOOL_H
#define MEMPOOL_H
#include <stdint.h>
#include <stdbool.h>
#include <assert.h>
#ifdef __cplusplus
extern "C" {
#endif

// 修复：与parser最大包体对齐，避免越界
#define MEMPOOL_BLOCK_SIZE    1024U

// 内存块链表头部
typedef struct MemBlockHeader {
    bool used;
    struct MemBlockHeader *next;
} MemBlockHeader;

// 内存池统计信息
typedef struct {
    uint32_t total;
    uint32_t free;
    uint32_t alloc_cnt;
    uint32_t free_cnt;
} MempoolStat;

typedef struct Mempool Mempool;

// 创建内存池实例
Mempool* mempool_create(uint32_t block_cnt);

// 销毁内存池，释放全部资源
void mempool_destroy(Mempool *mp);

// 从内存池分配一块内存
void *mempool_alloc(Mempool *mp);

// 将内存块归还内存池
void mempool_free(Mempool *mp, void *ptr);

// 获取内存池统计数据
void mempool_get_stat(Mempool *mp, MempoolStat *stat);

#ifdef __cplusplus
}
#endif
#endif
