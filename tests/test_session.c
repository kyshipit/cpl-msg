#include "../src/session/session.h"
#include "../src/log/log.h"
#include <stdio.h>

int main(void)
{
    printf("===== Test session module start =====\n");
    struct SessionMgr *mgr;
    session_mgr_init(&mgr);

    Session *s = session_create(&mgr, NULL);
    session_append_msg(s, true, "你好");
    session_append_msg(s, false, "你好呀，有什么可以帮你的？");
    session_append_msg(s, true, "讲个故事");

    LOG_VAR(s->msg_cnt, s->msg_cnt);

    session_destroy(&mgr, s);
    session_mgr_destroy(&mgr);

    printf("===== Test session module pass =====\n");
    return 0;
}
