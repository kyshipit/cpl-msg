cpl-msg：二进制协议网关 + 规则AI聊天机器人，可渐进扩展 TinyML 推理、向量检索。

> 基于 C11 实现自定义二进制协议网关，采用状态机处理 TCP 粘包，使用函数指针表实现命令分发；自研固定块内存池减少内存碎片；利用柔性数组承载变长包体；集成规则 AI 聊天模块，实现分词、模糊匹配与多轮对话上下文；项目覆盖指针、内存对齐、预处理宏等 C 进阶难点，支持后续接入 TinyML 推理。



## 📂 目录

```
minimsg-c/
├── README.md               # 项目说明、清单、编译、运行、扩展路线
├── Makefile                # 编译脚本，支持debug/release，分阶段编译
├── src/
│   ├── packet/    协议头定义、序列化/反序列化工具
│   ├── parser/    状态机粘包解析 + 命令分发（函数指针回调表）
│   ├── mempool/   自研固定块内存池，所有数据包从这里分配
│   ├── log/       日志：可变参数宏 + _Generic泛型打印
│   ├── ai/        规则AI：词典、最大正向分词、编辑距离模糊匹配、意图识别
│   ├── session/   对话上下文链表，保存历史对话
│   ├── net/       TCP服务端（select单线程Reactor，可开关）
│   └── main.c     入口，支持两种运行模式
├── tests/         分模块单元测试，单独编译调试单个模块
└── docs/          自查清单
```

> 设计理由：
>
> 1. 按**分层**而不是按功能堆文件：协议→解析→内存→日志→AI→网络
> 2. tests 独立：每个模块单独可跑，新手可以单独跑 `test_parser` 不用碰网络


## ✅ 清单
- 指针体系：一级指针、二级指针、数组指针、指针数组、函数指针
- const 多种组合：`const int *` / `int *const` / `const int *const`
- 结构体、联合体、位域、内存对齐、padding
- 柔性数组成员 flexible array member
- C99：指定初始化、复合字面量、inline、restrict、变长参数宏
- C11：_Static_assert（static_assert）、_Generic泛型选择
- 存储类、作用域、链接属性：static / extern
- 内存管理：malloc/free 缺陷、自研固定块内存池、内存泄漏检测、double free防护
- 预处理：宏、#字符串化、##拼接、条件编译（DEBUG开关）
- 链表操作：二级指针实现链表增删（对话上下文）
- 状态机设计、回调分发（函数指针表）
- 未定义行为意识：边界检查、禁止越界、严格别名、restrict别名约束

> 可选后续扩展（本项目骨架原生支持）
> - TFLite Micro MNIST手写数字推理（CMD_AI_MNIST）
> - 简易KNN向量检索、RAG知识库
> - 多线程版本：_Atomic、_Thread_local
> - 多连接TCP、简单心跳保活



```
## 三、docs/learning_checklist.md
```markdown
## 基础语法与C99/C11特性
- [ ] _Static_assert 编译期断言，校验结构体对齐
- [ ] _Generic 泛型打印宏，自动识别类型
- [ ] inline 内联函数，适合小包校验、工具函数
- [ ] restrict 指针别名限定，拷贝函数优化
- [ ] 指定初始化、复合字面量构造结构体/数组
- [ ] 可变参数宏 __VA_ARGS__，# 字符串化、## 拼接

## 指针核心（本项目最大分水岭）
- [ ] 数组指针 vs 指针数组
- [ ] 二级指针：链表节点删除、缓冲区截断
- [ ] 函数指针、函数指针数组做回调分发
- [ ] const 修饰指针的三种写法区分
- [ ] 野指针、悬空指针的规避思路

## 内存与结构体
- [ ] 位域：协议标志位打包
- [ ] 柔性数组：变长包体，和普通动态数组区别
- [ ] 结构体padding、内存对齐规则
- [ ] static：局部静态、文件内静态全局变量
- [ ] extern：跨文件声明外部接口
- [ ] 自研内存池：分配、归还、统计、防double free

## 算法与架构思想
- [ ] 状态机：粘包解析状态流转
- [ ] 回调分发：解耦协议解析和业务逻辑
- [ ] 单向链表：二级指针实现上下文增删
- [ ] 编辑距离：模糊匹配，容错用户输入
- [ ] 最大正向分词：简单中文分词

## 扩展路线（主线完成后）
- [ ] TFLite Micro MNIST 推理
- [ ] 向量库、KNN余弦相似度检索
- [ ] _Atomic 原子计数、_Thread_local 多线程版本
```


## 五、Makefile（支持 debug、单元测试、分模块编译）

```
CC = gcc
STD = -std=c11
CFLAGS = $(STD) -Wall -Wextra -g -DDEBUG
LDFLAGS = -pthread

TARGET = minimsg

SRC = src/main.c \
      src/packet/packet.c \
      src/parser/parser.c \
      src/mempool/mempool.c \
      src/log/log.c \
      src/ai/ai.c \
      src/session/session.c \
      src/net/net.c

# 单元测试目标
test_packet: tests/test_packet.c src/packet/packet.c src/log/log.c
	$(CC) $(CFLAGS) $^ -o $@

test_parser: tests/test_parser.c src/packet/packet.c src/parser/parser.c src/log/log.c src/mempool/mempool.c
	$(CC) $(CFLAGS) $^ -o $@

test_mempool: tests/test_mempool.c src/mempool/mempool.c src/log/log.c
	$(CC) $(CFLAGS) $^ -o $@

test_ai: tests/test_ai.c src/ai/ai.c src/log/log.c src/session/session.c
	$(CC) $(CFLAGS) $^ -o $@

all: $(TARGET)
$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

clean:
	rm -f $(TARGET) test_packet test_parser test_mempool test_ai
```

## 六、开发顺序（和前面阶段对齐，保证循序渐进）

1. 先写 `packet.h / packet.c` + `test_packet.c`，验证结构体、位域、static_assert
2. 写 `log.h`，实现 `_Generic` 日志宏、可变参数日志（后面所有模块都依赖它）
3. 写 `mempool` + `test_mempool`，吃透内存池、static、内存统计
4. 写 `parser` 状态机 + 函数指针分发 + `test_parser`，模拟粘包（核心难点）
5. 写 `session` 链表（二级指针）
6. 写 `ai` 规则分词、词典、编辑距离、意图匹配 + `test_ai`
7. 对接所有模块到 main 的 `--test` 内存测试模式，跑通端到端
8. 最后开发 `net` TCP 模块，开启 `--server` 模式，nc 交互
9. 主线全部稳定后，再做扩展：TFLite、向量检索、多线程原子


## 7、完整链路汇总
```
main()
└─ service_ctx_init()
   ├─ memset(ctx,0)
   ├─ malloc ConnMgr
   ├─ malloc SessionMgr → session_mgr_init()       // 初始化会话链表
   ├─ mempool_init()                               // 内存池初始化
   ├─ cmd_dispatch_register_table()                // 注册静态cmd表
   └─ conn_mgr_init(mgr, port, ctx)
      ├─ epoll_create1()
      ├─ socket() 创建listen fd，设为非阻塞
      ├─ bind + listen
      ├─ listen fd 添加到epoll
      └─ 返回成功

main()
└─ service_ctx_run()
   └─ conn_mgr_run_loop()          // epoll主循环，阻塞
      ├─ epoll_wait() 等待事件
      ├─ conn_mgr_scan_timeout()   // 定时：1s扫描
      │   ├─ tcp_conn_check_timeout() 检测连接心跳超时，超时调用tcp_conn_destroy
      │   └─ session_scan_expire()     // 扫描过期会话
      └─ 遍历epoll返回事件
          ├─【事件：listen fd可读】
          │   └─ conn_mgr_accept()
          │      ├─ accept4()拿到cfd非阻塞
          │      ├─ tcp_conn_create()
          │      │   ├─ malloc TcpConn
          │      │   ├─ parser_init() 绑定回调 tcp_conn_on_packet
          │      │   └─ 赋值 svc_ctx、sess_mgr指针
          │      ├─ session_create() // 创建对应session会话
          │      └─ conn_mgr_add_conn()
          │         └─ epoll_ctl(EPOLL_CTL_ADD) 将新conn fd加入epoll(EPOLLIN|EPOLLOUT|EPOLLET)
          │
          └─【事件：已连接socket fd】
              ├─ EPOLLIN / EPOLLERR / EPOLLHUP
              │   └─ tcp_conn_on_read()
              │      ├─ recv() 读取字节
              │      └─ parser_input(parser, 原始字节) // 送入状态机
              │          └─ parser状态机收齐完整数据包后触发回调 tcp_conn_on_packet
              │              ├─ 构造 HandlerCtx(svc_ctx, conn, sess_mgr)
              │              └─ cmd_dispatch(hdr, body, &hctx)
              │                  └─ 查表调用业务handler(ping / echo / ai_text)
              │                      └─ tcp_conn_send_pkt() // 组装应答包写入conn->send_buf
              │                          └─ ✨【关键分支】如果入队前send_len=0 → 立刻调用 tcp_conn_on_write() 执行send系统调用
              │
              └─ EPOLLOUT
                  └─ tcp_conn_on_write()
                      └─ send() 发送send_buf内缓存数据
                         └─ send没发完：残留数据留在send_buf；等内核发送缓冲区有空，epoll再触发EPOLLOUT继续发送

```

## 修正点

1. `mempool_init`、`cmd_dispatch_register_table` 不是在`conn_mgr_init`里面，是在上层`service_ctx_init`执行。
2. 主循环定时扫描做两件事：**连接心跳超时 + session 会话过期清理**，你原图只写了扫描连接。
3. **应答发送有两条路径**（非常重要，EPOLLET 特性）

> 
> 如果你没有路径 A，新建 socket 不会触发 EPOLLOUT，数据包就会卡在用户态 send_buf 发不出去，就是之前踩的坑。
   - 路径 A：`tcp_conn_send_pkt`入队之前`send_len == 0` → **直接同步调用`tcp_conn_on_write`发送**（现在 PING/ECHO/AI_TEXT 回复走这条）
   - 路径 B：发送缓冲区本来就有残留数据，新包追加到后面；等内核缓冲区腾出空间，epoll 触发`EPOLLOUT`事件再执行`tcp_conn_on_write`
4. `parser_input`内部状态机收齐完整包之后，才会调用回调 `tcp_conn_on_packet`，再进到`cmd_dispatch`，不是 parser_input 直接调用 dispatch。
5. accept 成功之后，除了创建 tcp_conn，还会同步`session_create`创建会话。
6. 错误分支：`tcp_conn_on_read`读到对端关闭 / 错误 → 返回 false → 上层调用`tcp_conn_destroy`销毁连接，触发 close_cb，回调执行`conn_mgr_remove_conn`把 fd 从 epoll 摘除。

## 销毁链路

```
tcp_conn_destroy(conn)
├─ close(fd)
├─ parser_reset() 归还内存池块
└─ 执行 close_cb(conn, ud) → conn_close_cb → conn_mgr_remove_conn
```

## 数据流

> 
> 客户端字节 → recv → parser 状态机拆包 → cmd 分发 handler → handler 调用 send_pkt 组装应答写入用户层 send_buf；要么当场 send 出去，要么等待 epoll EPOLLOUT 事件后续发送。


# 现状

1. 头文件范式：**风格 2 typedef 结构体，循环依赖头文件只用`struct X;`标签前置，指针写`struct X *`；.c include 完整头文件后使用别名**，编译无 conflicting‑types /unknown‑type‑name。
2. 模块划分：`packet`协议包头、`parser`粘包状态机、`tcp_conn`单连接封装、`conn_mgr`epoll 事件循环、`session`会话历史、`cmd_dispatch`命令分发、`mempool`内存池、`log`日志。
3. 完整链路：客户端发包 → TCP 流式接收 → parser 状态机拆包 → cmd_dispatch 路由到业务 handler → 业务组装应答包入 send_buf → 主动 send 发送 → 客户端收到字节。
4. 内存生命周期：`ServiceCtx` → malloc `ConnMgr` → malloc `SessionMgr`，全部配套 free；`session_mgr_init`初始化链表，不再出现 malloc 脏内存导致的段错误。

### 拓展方向

1. 信号处理 (SIGINT)，跳出 epoll 循环，优雅释放全部资源，现在`service_ctx_run`是死循环，`service_ctx_destroy`永远执行不到。
2. 客户端接入 parser，解析收到的二进制包，打印 cmd、seq、包体文本。
3. 增加离线会话测试：客户端断开，短时间重连，复用旧 session 会话历史。
4. 内存池增加更多统计，做内存泄漏测试。

ET 边缘触发网络编程的经典坑：新建 socket 发送缓冲区本来就空闲，不会产生 EPOLLOUT 事件。**新压入数据，队列为空时必须主动调用一次写函数；写不完残留数据，才靠 epoll 的 EPOLLOUT 事件继续发送**。这个坑很多写 epoll 的新手都会踩。