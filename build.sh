# 编译服务端, 注意需要 `-Isrc` 找头文件
gcc -std=c11 -Wall -Wextra -g -DDEBUG -Isrc \
src/server/main.c \
src/service_ctx/service_ctx.c \
src/conn_mgr/conn_mgr.c \
src/tcp_conn/tcp_conn.c \
src/parser/parser.c \
src/cmd_dispatch/cmd_dispatch.c \
src/packet/packet.c \
src/session/session.c \
src/mempool/mempool.c \
src/log/log.c \
-o minimsgsvr

# 编译客户端
gcc -std=c11 -Wall -Wextra -g -DDEBUG -Isrc \
src/client/client.c \
src/packet/packet.c \
-o miniclient
