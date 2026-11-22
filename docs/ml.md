```
./
├── build.sh
├── .gitignore
├── README.md
├── src
│   ├── core
│   │   ├── log.c
│   │   └── log.h
│   │   ├── mempool.c
│   │   └── mempool.h
│   ├── protocol
│   │   ├── packet.c
│   │   └── packet.h
│   │   ├── parser.c
│   │   └── parser.h
│   ├── net
│   │   ├── reactor.c
│   │   └── reactor.h
│   │   ├── tcp_conn.c
│   │   └── tcp_conn.h
│   │   ├── service_ctx.c
│   │   └── service_ctx.h
│   │   ├── master_worker.c
│   │   └── master_worker.h
│   ├── session
│   │   ├── session.c
│   │   └── session.h
│   ├── dispatch
│   │   ├── cmd_dispatch.c
│   │   └── cmd_dispatch.h
│   ├── server
│   │   └── main.c
│   └── client
│       └── client.c
└── tests
    ├── test_log.c
    ├── test_mempool.c
    ├── test_packet.c
    ├── test_parser.c
    ├── test_session.c
    └── test_tcp_conn.c
```
