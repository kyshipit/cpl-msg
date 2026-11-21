#include "service_ctx/service_ctx.h"
#include "log/log.h"
#include <stdio.h>
#include <stdlib.h>

int main(void)
{
    ServiceCtx ctx;
    if(!service_ctx_init(&ctx, 8888))
    {
        LOG_ERROR("service ctx init failed");
        return 1;
    }
    LOG_INFO("MiniMsg server start, port=8888");
    service_ctx_run(&ctx);
    service_ctx_destroy(&ctx);
    return 0;
}
