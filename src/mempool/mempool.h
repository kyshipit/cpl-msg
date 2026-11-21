/**
 * @file mempool.h
 * @brief MiniMsg-C 固定块内存池
 * @learning 静态全局变量、单链表管理空闲块、内存复用、防double free、内存统计
 * 相比直接malloc：减少系统调用开销、便于内存泄漏统计、适合协议小包场景
 * 【注意UB】禁止free内存池返回的指针；只能调用mempool_free；越界写会破坏链表指针
 */
#ifndef MEMPOOL_H
#define MEMPOOL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 单个内存块大小：适配我们的数据包（包头8 + 变长包体），这里固定块256字节，足够聊天文本包
#define MEMPOOL_BLOCK_SIZE    256U
// 内存池总块数量
#define MEMPOOL_BLOCK_CNT     32U

/**
 * @brief 内存块头部控制信息
 * 块头埋在内存前面，用来标记是否已分配、链表指针
 */
typedef struct MemBlockHeader {
    bool used;                  // true=已分配，false=空闲
    struct MemBlockHeader *next; // 空闲链表下一个块
} MemBlockHeader;

/**
 * @brief 内存池统计信息，调试观察用
 */
typedef struct {
    uint32_t total;     // 总块数
    uint32_t free;      // 空闲块数
    uint32_t alloc_cnt;  // 累计分配次数
    uint32_t free_cnt;   // 累计释放次数
} MempoolStat;

/**
 * @brief 初始化内存池，构建空闲链表
 * 必须在使用mempool_alloc之前调用
 */
void mempool_init(void);

/**
 * @brief 从内存池分配一块内存（返回可用内存起始地址，跳过header）
 * @return 成功返回指针，失败返回NULL
 */
void *mempool_alloc(void);

/**
 * @brief 归还内存到内存池
 * @param ptr mempool_alloc返回的指针
 * 【注意UB】不能传入非mempool_alloc得到的指针，不能重复free同一个块
 */
void mempool_free(void *ptr);

/**
 * @brief 获取内存池统计信息
 * @param stat 输出统计结构体
 */
void mempool_get_stat(MempoolStat *stat);

#ifdef __cplusplus
}
#endif

#endif
