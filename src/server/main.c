/**
 * main
 * 服务程序入口，解析命令行参数，注册信号处理
 */
#include "net/service_ctx.h"
#include "core/log.h"
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

// 信号处理：仅设置退出标志，异步信号安全
static void sigint_handler(int sig)
{
    (void)sig;
    service_ctx_notify_exit();
}

// 打印帮助信息
static void usage(void)
{
    printf("Usage: ./minimsgsvr [-p port] [-w worker_num] [-d]\n");
    printf("  -p port        listen port (default 8888)\n");
    printf("  -w worker_num  worker reactor count (default 2)\n");
    printf("  -d             enable debug log\n");
}

int main(int argc, char **argv)
{
    uint16_t port = 8888;
    uint32_t worker_num = 2;
    bool enable_debug = false;
    int opt;
    while ((opt = getopt(argc, argv, "p:w:d")) != -1)
    {
        switch (opt)
        {
            case 'p':
                port = (uint16_t)atoi(optarg);
                break;
            case 'w':
                worker_num = (uint32_t)atoi(optarg);
                break;
            case 'd':
                enable_debug = true;
                break;
            default:
                usage();
                return 1;
        }
    }
    if (enable_debug)
    {
        log_set_level(LOG_LEVEL_DEBUG);
    }
    else
    {
        log_set_level(LOG_LEVEL_INFO);
    }

    // 注册信号
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = sigint_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    ServiceCtx ctx;
    if (!service_ctx_init(&ctx, port, worker_num))
    {
        LOG_ERROR("service ctx init failed");
        return 1;
    }

    LOG_INFO("MiniMsg server start, port=%u, worker=%u", port, worker_num);
    service_ctx_run(&ctx);
    service_ctx_destroy(&ctx);
    LOG_INFO("service exit clean");
    return 0;
}
