
master‑worker多‑reactor epoll网络服务，模拟AI聊天服务。


```
├── README.md
├── src
│   ├── core        基础工具：日志、固定块内存池
│   ├── protocol    二进制协议头、TCP流式状态机parser，处理粘包分包
│   ├── net         reactor事件循环、tcp连接封装、master‑worker、顶层service上下文
│   ├── session     会话管理，对话历史链表，会话过期回收
│   ├── dispatch    命令分发，查表路由业务handler
│   ├── server      服务入口 main
│   └── client      demo同步客户端，复用parser解析应答包
└── tests
```

```
编译
bash build.sh

运行服务端
./minimsgsvr -p 8888 -w 2 -d

运行客户端
./minimsgcli 127.0.0.1 8888
```


- master‑worker多reactor，accept在master，IO分发到worker，连接绑定固定worker，连接读写无锁
- eventfd + SIGINT 优雅退出，完整资源释放，valgrind可干净退出
- 命令行参数：端口、worker数量、debug日志开关
- 包体最大长度边界防护，防止恶意大包
- 内存池减少malloc调用
- 会话支持离线保留，进程退出主动释放全部会话