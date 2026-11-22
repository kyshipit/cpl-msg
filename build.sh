#!/bin/bash
set -e

INCS="-Isrc"
SERVER_CFLAGS="-std=c11 -D_GNU_SOURCE -Wall -Wextra -g -DDEBUG"
CLIENT_CFLAGS="-std=c11 -D_GNU_SOURCE -Wall -Wextra -g"

SERVER_SRCS=(
src/server/main.c
src/core/log.c
src/core/mempool.c
src/protocol/packet.c
src/protocol/parser.c
src/net/reactor.c
src/net/tcp_conn.c
src/net/master_worker.c
src/net/service_ctx.c
src/session/session.c
src/dispatch/cmd_dispatch.c
)

CLIENT_SRCS=(
src/client/client.c
src/core/log.c
src/core/mempool.c
src/protocol/packet.c
src/protocol/parser.c
src/net/reactor.c
src/net/tcp_conn.c
src/session/session.c
src/dispatch/cmd_dispatch.c
)

echo "build server ..."
gcc ${INCS} ${SERVER_CFLAGS} -o minimsgsvr "${SERVER_SRCS[@]}" -lpthread

echo "build client ..."
gcc ${INCS} ${CLIENT_CFLAGS} -o minimsgcli "${CLIENT_SRCS[@]}" -lpthread

echo "build done"
