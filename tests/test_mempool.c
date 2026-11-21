/**
 * @file test_mempool.c
 * @brief 内存池单元测试：分配、释放、统计、double free检测
 * 编译：gcc -std=c11 -Wall -Wextra -g -DDEBUG tests/test_mempool.c src/mempool/mempool.c src/log/log.c -o test_mempool
 * 运行：./test_mempool
 */
#include "../src/mempool/mempool.h"
#include "../src/log/log.h"
#include <stdio.h>

int main(void)
{
    printf("===== Test mempool module start =====\n");
    mempool_init();

    MempoolStat st;
    mempool_get_stat(&st);
    LOG_VAR(st.free, st.free);

    // 分配3块
    void *p1 = mempool_alloc();
    void *p2 = mempool_alloc();
    void *p3 = mempool_alloc();
    mempool_get_stat(&st);
    LOG_VAR(st.free, st.free);

    mempool_free(p2);
    mempool_get_stat(&st);
    LOG_VAR(st.free, st.free);

    // 测试double free
    mempool_free(p2);

    mempool_free(p1);
    mempool_free(p3);
    mempool_get_stat(&st);
    LOG_VAR(st.free, st.free);

    printf("===== Test mempool module pass =====\n");
    return 0;
}
