已经完成底座：
✅ 多模块 C 事件驱动 TCP 服务端（epoll‑ET）
✅ 自定义二进制协议、TCP 粘包状态机 parser
✅ 内存池、日志、会话管理（对话历史链表）
✅ 命令分发、PING/ECHO、**MockAI 文本应答**
✅ 客户端 demo，端到端可以收发聊天消息
✅ 会话保存每轮用户 / AI 历史消息

> 
> 现状：AI 部分只是内存里的字符串模拟 `[MockAI]收到：xxx`，**没有真正调用外部大模型**。
> 原始目标：**带 AI 能力的 C 语言练习项目**。

下面分三块：

1. 【还差什么：最小可用真实 AI 版本】（优先做）
2. 【工程层面还缺的点（C 项目健壮性）】
3. 【可选进阶拓展练习（适合 C 练手）】

## 一、AI 能力层面，当前缺失（核心目标差距）

现在是**Mock 假 AI**，只是 snprintf 拼接字符串。要变成真正 AI，C 程序要做：

### 1. HTTP client：C 调用大模型 Open‑style API（通义千问 / DeepSeek / OpenAI 兼容接口）

> 
> C 没有高层 http 库，两种路线：

- 路线 A（简单练手）：使用 `libcurl`，做 http post 请求，发送 json 对话历史，拿到返回 json。
- 路线 B（纯裸 socket，不依赖第三方库）：手写 http socket 请求，工作量大，适合深度 C 练习。

**需要新增逻辑：**

1. 把当前 `Session` 里面保存的对话历史 `history[]`，组装成 AI 接口要求的 messages json 数组

```
[
  {"role":"user","content":"xxx"},
  {"role":"assistant","content":"xxx"}
]
```

2. http post 发送到 LLM 接口，带上 apikey http header。
3. 接收 http 响应，解析 json 取出 AI 回复文本。

### 2. JSON 解析（C 没有内置 json）

两个选择：

1. **cJSON（推荐，轻量级，单文件）**，把 cJSON 源码直接放进项目，不需要系统库安装，非常适合练习项目。
2. 自己手写简易 json 解析（不推荐，耗时间，容易 bug）。

> 
> 当前 cmd_ai_text_handler：
> 现在直接 snprintf 模拟回答；改造之后：

- 取出 session 历史 → 组装 messages json → http 请求 LLM → 拿到回复字符串 → `session_append_msg(sess, false, ai_reply);` → 回包给客户端。

### 3. 错误处理：网络请求失败、API 限流、key 错误、http 非 200 返回、json 解析失败

现在 mock 版本不会网络出错；真实 http 调用会大量失败场景：

- 网络超时、DNS 失败、API 返回 401 (密钥错)、429 限流、500 服务错误。

> 
> 注意：**epoll 是单线程事件循环！绝对不能在 handler 里面做阻塞 http 请求！**
> 👉 这是一个**非常关键的大坑**：
> 如果直接在 `cmd_ai_text_handler` 同步调用 curl/http，整个 epoll 事件循环阻塞，**全部客户端卡死**。

#### 这里你有两个方案（C 事件驱动项目重点练习点）

- 方案 1（简单，适合练习）：**创建工作子线程**，AI http 请求丢给线程池 / 单个工作线程；拿到结果之后，通过管道写回 epoll 主线程，再把 AI 回复推送给客户端。

> 
> 单线程 epoll 主线程只处理 IO，耗时网络 AI 请求交给子线程。

- 方案 2（纯异步无线程）：使用 libcurl multi‑handle 异步接口，全部在 epoll 里面非阻塞跑，难度更高。

> 
> ⚠️ 当前代码直接同步 http 请求 = 整个服务卡住，这是必须解决的架构点。

### 当前 AI 模块现状 vs 目标

表格

| 项目 | 现在 MockAI | 真实 AI 需要 |
| --- | --- | --- |
| 对话历史 | ✅ Session 已经保存 user/assistant 历史 | 直接复用这个 history 数组组装 messages |
| 生成回答 | ❌ snprintf 假回复 | http 调用 LLM + json 序列化 / 反序列化 |
| 阻塞风险 | 无，内存运算 | ❗不能在 handler 阻塞，需要线程 / 异步 http |
| 错误处理 | 无 | 网络、api 鉴权、json 解析各种异常 |

---

## 二、工程层面：这个 C 服务还缺失的通用点（健壮性）

1. **SIGINT 信号优雅退出**
现在 `conn_mgr_run_loop` 死循环，只能 `kill -9`，`service_ctx_destroy` 永远执行不到。

- 需求：捕获 SIGINT (Ctrl‑C)，给 epoll_wait 唤醒，跳出循环，依次释放 mempool、session、conn、socket 资源。

2. 客户端缺少协议 parser
现在客户端只原始 recv 字节，没有解析 MsgHeader；只能看到收到多少字节，不能解析 cmd、seq、包体文本。

> 
> 可以复用`parser.c`给客户端，收到数据送入 parser，回调打印业务包。

3. 配置能力
API‑KEY、LLM 地址、监听端口现在硬编码。可以简单实现读取配置文件 / 命令行参数传入。
4. 内存泄漏检查

- valgrind 检测全套 malloc/free：conn、session、sess_mgr、conn_mgr，mempool。

> 
> 现在断开连接 session 不会销毁，依靠 30min 过期；正常，但是 valgrind 退出的时候还存活的 session 会报内存泄漏（属于业务设计，要注意区分）。

5. 缺少部分边界防护

- 包体大小、json 字符串长度上限，防止超大输入。

## 三、可选进阶 C 练习（看时间，可选）

1. 不使用 libcurl/cJSON，纯手写 http + 简易 json（硬核 C 练习，不推荐作为第一版）
2. 线程池：多个 worker 线程处理 AI 请求，支持并发多用户同时问 AI。
3. 简单持久化：session 对话历史保存到本地文本文件。
4. 协议扩展：支持二进制图片数据包。

# ✨ 最小可完成目标路线（推荐按顺序做，循序渐进，适合 C 练习）

> 
> 在你现在稳定可运行的代码底座之上：

1. 引入 **cJSON**（加入 src/thirdparty/cjson）
2. 在项目增加 libcurl 编译链接，build.sh 增加 `-lcurl`
3. 封装一个工具函数：输入 Session*，输出完整对话 messages json 字符串（使用 cJSON）
4. 封装**子线程 AI 工作模块**：
   - 主线程 epoll；收到 AI_TEXT，把会话信息投递给子线程。
   - 子线程执行 curl http post 调用 LLM 接口，解析 json 拿到回复。
   - 通过管道把结果回传给 epoll 主线程。
   - 主线程收到 AI 结果，执行 session_append_msg，tcp_conn_send_pkt 推送给客户端。
5. 增加错误处理：API 调用失败向客户端返回错误提示包。
6. 实现 SIGINT 优雅退出。
7. （可选）客户端接入 parser，解析打印收到 AI 回答文本。

> 
> 重点学习收获：

- C 调用第三方 REST API
- json 处理
- epoll 单线程 IO 模型如何处理**阻塞型任务**（线程 + 管道通知主线程）
- 多线程和事件循环交互（经典 C 后端知识点）

# 风险提醒（最容易踩坑）

1. ❌ 千万不要在 cmd_dispatch 业务 handler 里面直接做同步阻塞 http 请求，会把整个服务全部卡住。
2. API key 不要硬编码写死源码，命令行参数或者环境变量传入，防止密钥提交 git。
3. 子线程访问 Session/TcpConn 资源要注意生命周期：连接可能在线程处理 AI 请求的中途已经断开销毁，要做有效性判断。