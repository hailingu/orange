# 网络编程 — 从 POSIX socket 到单线程事件循环

对应索引:[README.md](README.md) › 文件索引。

> **实现状态**:解析器(service ADR-0002/0003/0004 与 ADR-0008/0009 的
> 对象)**已实现**;本篇的 socket/事件循环/TLS/静态服务部分是 **ADR-0003
> 已批准、待实现**(T-2/T-3,Implementation Status: Not Started)的设计。
> 以下按"设计 + 知识点"讲解,引用处标注 ADR 的 Scope/AC,不冒充已落地
> 的代码。

## 1. 回环绑定:第一道安全边界

orange 是单学习者本地系统,ADD-0001 CON-02 规定计算服务**仅回环**:
监听 `127.0.0.1` 而非 `0.0.0.0`。监听地址决定谁能连:

- `127.0.0.1:8080` / `127.0.0.1:8443`:只有本机进程可连;
- `0.0.0.0`(INADDR_ANY):所有网卡,包括局域网——家庭/咖啡馆 Wi-Fi 下
  等于把服务暴露给同网段任意主机。

ADR-0003 的 AC-6 把它变成可验证行为:`lsof -nP -iTCP:8080 -iTCP:8443
-sTCP:LISTEN` 必须只列出两个回环监听,且连接本机 LAN IPv4 的两个端口
都必须失败。

**知识点**:`lsof -nP -iTCP -sTCP:LISTEN` 是 macOS/Linux 查看监听套接字
的常规武器(`-n` 跳过 DNS 反解、`-P` 跳过端口名映射,脚本友好)。

## 2. TCP socket 基础(服务端视角)

一次最小 TCP 服务端的生命周期:

```
socket()        创建套接字(TCP: AF_INET + SOCK_STREAM)
bind()          绑定 (127.0.0.1, 8080)
listen()        转为监听态,内核开始维护握手完成队列
accept()        从队列取一个已连接套接字(每个客户端一个新 fd)
recv()/send()   在已连接套接字上读写
close()         释放
```

关键认知:**recv 返回 n 字节 ≠ 语义单元**。TCP 是字节流,没有消息边界;
"一个 HTTP 请求"必须由应用层协议(HTTP 的 Content-Length/chunked)在流上
重新划界——这正是 [`parser-design.md`](parser-design.md) 整篇的前提。

## 3. 并发模型:为什么选单线程 `poll()` 事件循环

ADR-0003 选型:一个线程,`poll()` 多路复用所有打开的套接字。对比:

| 模型 | 优势 | 代价 |
| --- | --- | --- |
| 线程-per-连接 | 代码直白,阻塞式读写 | 每连接一个栈(内存)、锁与竞态面、上下文切换 |
| 单线程 poll/epoll | 无锁、内存小、顺序可推理 | 必须非阻塞 + 显式状态机,编程复杂 |

对单用户本地服务(少量连接:浏览器几个 tab),事件循环的**可推理性**
(单线程天然串行,无数据竞争)比吞吐更重要;而 ADD-0001 INV-16/GO-02 的
"多 tab 并发"由多路复用直接满足。

`poll()` API(POSIX):

```c
struct pollfd { int fd; short events; short revents; };
int poll(struct pollfd *fds, nfds_t nfds, int timeout);
```

- 每次调用传入关心的 fd 集合与事件(`POLLIN` 可读 / `POLLOUT` 可写),
  返回就绪的 fd 数;`timeout` 用来实现空闲超时巡检;
- macOS/Linux 通用;Linux 上更高吞吐的等价物是 `epoll`,Windows 是
  IOCP——API 不同,**readiness 循环的结构相同**。

### readiness 驱动的读写状态机

事件循环的心跳(ADR-0003 T-1 的设计):

```
循环:
  poll(所有连接 fd + 监听 fd, 带超时)
  监听 fd 就绪        → accept() 新连接(非阻塞)
  连接可读(POLLIN)   → recv 进该连接的解码器(RequestDecoder::feed)
                        解析出完整请求 → 生成响应,挂到该连接的写队列
  连接可写(POLLOUT)   → send 冲写队列,写完撤回 POLLOUT 关心
  超时                → 关闭超过 5 秒空闲的 keep-alive 连接
```

要点:

- **非阻塞 socket + 读状态机**:读缓冲空时 `recv` 返回 EWOULDBLOCK,
  不能阻塞线程;解析器的 `incomplete` 对应"这轮读完了,等下一次 POLLIN";
- **写也要状态机**:send 可能只写出部分(内核发送缓冲满),响应剩余部分
  记在连接上,等 POLLOUT——这就是"readiness-driven read/write state
  machines";
- **每连接一个 `RequestDecoder`**:状态由解析器对象持有,事件循环只做
  搬运(见 [`parser-design.md`](parser-design.md) 第 7 节)。

## 4. 队头阻塞与 AC-7

单线程事件循环必须避免:一个连接上的**未完成请求**(客户端只发了一半)
卡住其他连接。这就是 head-of-line blocking 的应用层形态。ADR-0003 AC-7
的规定:连接 A 的 echo 请求体停在一半时,连接 B 的 `GET /api/health`
必须在一秒内完整返回,B 完成后再补发 A 的剩余字节,A 最终拿到精确回显。

设计含义:A 的"等字节"是**状态**,不是**阻塞**——poll 循环天然满足,
反而是"每请求同步处理完"的写法会违反。TLS 监听(OpenSSL 非阻塞 BIO)
同样要在两个监听上分别通过该测试。

## 5. keep-alive 与空闲超时

- keep-alive:连接复用,省握手;有效请求的测试模型就是"一个持久客户端
  在一条连接上连续三轮请求-响应"(AC-1);
- 5 秒空闲超时:空闲连接也占 fd;超时关闭把**描述符积累**限制在有界
  范围(资源上限与背压维度)。测试时超时配成 1 秒加速验证。

## 6. 拒绝后关闭连接(为什么不是继续服务)

ADR-0003(提交 `1f87646` 的提案,CC-5):431/413 拒绝响应带
`Connection: close`,**响应写完即关闭**该连接,不做管线化续读。理由在
[`parser-design.md`](parser-design.md) 第 3、5 节:终局错误后输入流的
下一个"消息边界"不可信。测试要求:被拒连接上**不得**出现对后续管线
请求的响应,EOF 必须发生,客户端**重连**后新请求成功。

## 7. TLS:锁版 OpenSSL 与自签名证书(待实现)

- 依赖管理:`vcpkg.json` 清单锁 OpenSSL 版本——ADR-0003 明令**加密原语
  绝不手写**;
- 术语:**终止(terminate)TLS** = 在服务端把 TLS 会话解密成明文 HTTP,
  交给应用逻辑;
- 本地自签名:首启生成自签名证书存入 gitignored 位置、跨重启复用;浏览器
  需手动信任,`curl` 侧用 `-k` 跳过校验(仅限本地测试语义);
- 非阻塞 TLS:握手是多步状态机( WANT_READ/WANT_WRITE),要并入 poll
  循环而不是单独线程。

## 8. 应用层端点与静态服务(待实现)

- `GET /api/health` → `200 {"status":"ok"}`:最小健康检查,也是 T-3
  状态页的数据源;
- `POST /api/echo` → chunked 响应回显请求体:把"解析 chunked 请求"与
  "生成 chunked 响应"凑成一对;
- 静态服务:扩展名 → `Content-Type` 的小 MIME 表;单区间 `Range` 请求
  (206 Partial Content,`Content-Range` 标注区间)与不可满足区间(416);
- 405(方法不允许)/404:路由分发的两极——路径存在但方法错 vs 路径不存在。

## 9. 客户端与验证工具

- **curl** 是端到端验收的利器:`curl -kfsS https://127.0.0.1:8443/api/health`
  (`-k` 容自签名、`-f` 把 ≥400 变非零退出码、`-S` 显示错误、`-s` 静默),
  退出码与精确体断言直接进 AC-2/AC-4/AC-5;
- AC-8 用真实浏览器 + 网络面板(仅中断 `/api/health`)验证状态页失败
  反馈在 2 秒内出现——浏览器行为无法被 curl 替代的部分才用浏览器。

## 延伸阅读

- man pages:`poll(2)`、`socket(2)`、`accept(2)`、`recv(2)`(注意
  EAGAIN/EWOULDBLOCK 段落);
- 《TCP/IP 详解 卷1》第17-22章( TCP 连接管理到拥塞控制);
- C10K 文档(事件驱动并发的经典论述);
- OpenSSL 官方 `SSL_accept`/non-blocking BIO 指南。
