# 网络编程 — 从 POSIX socket 到单线程事件循环

对应索引:[README.md](README.md) › 文件索引。

> **实现状态**:解析器与**响应序列化/错误映射**已实现;单线程事件循环
> 已按 2026-10-20 重批准的 ADR-0003 从 `poll()` 迁移到 **kqueue()**
> (`service/src/http_server.cpp`,`5da6348`),行为契约由原真实 socket 套件
> 原样钉住(T-1 Complete,AC-1 Pass;poll 前身红 `cf1eed6` / 绿 `f7ca39c`
> 留作历史)。TLS 监听、路由/静态服务与状态页仍是 T-2/T-3 待实现的设计,
> 引用处标注 ADR 的 Scope/AC,不冒充已落地的代码。

## 1. 回环绑定:第一道安全边界

orange 是单学习者本地系统,ADD-0001 CON-02 规定计算服务**仅回环**:
监听 `127.0.0.1` 而非 `0.0.0.0`。监听地址决定谁能连:

- `127.0.0.1:8080` / `127.0.0.1:8443`:只有本机进程可连;
- `0.0.0.0`(INADDR_ANY):所有网卡,包括局域网——家庭/咖啡馆 Wi-Fi 下
  等于把服务暴露给同网段任意主机。

实现里这是**构造性约束**:`LoopbackServer::bind` 只填
`htonl(INADDR_LOOPBACK)` 的地址,API 根本无法表达"绑到别的地址"。
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

`LoopbackServer::bind` 的四个实现细节,每个都对应一条真实约束:

- **错误通道是 errno 风格的返回值**(`return errno`),不是异常也不是
  输出参数——套接字调用失败时 errno 本身就是编码诊断;ADR-0003 TN-2
  要求端口冲突产生"编码错误"而不是悄悄换端口,套件断言第二个
  `bind(port)` 返回非零(EADDRINUSE);
- **SO_REUSEADDR**:不加它,重启服务会因上一个连接的 TIME_WAIT 残留
  而绑定失败——AC-5"普通重启保持同一 origin"直接依赖这条;
- **port 0 = 临时端口**:测试要拿到内核分配的端口,`getsockname` 在
  bind 之后回填实际端口。两阶段 API(先 `bind` 拿端口、再 `serve`)
  让测试无竞态地知道端口——比"起线程后 sleep 一下再连"诚实;
- **监听 fd 也设 O_NONBLOCK**:accept 才能非阻塞地"排空为止"。

关键认知:**recv 返回 n 字节 ≠ 语义单元**。TCP 是字节流,没有消息边界;
"一个 HTTP 请求"必须由应用层协议(HTTP 的 Content-Length/chunked)在流上
重新划界——这正是 [`parser-design.md`](parser-design.md) 整篇的前提。

## 3. 并发模型:单线程事件循环(poll → kqueue)

ADR-0003 选型:一个线程多路复用所有打开的套接字——最初用 `poll()`,
2026-10-20 经批准失效性修订与重批准后迁到 Darwin 原生的 `kqueue()`
(声明的工具链是 Apple arm64 Darwin)。对比:

| 模型 | 优势 | 代价 |
| --- | --- | --- |
| 线程-per-连接 | 代码直白,阻塞式读写 | 每连接一个栈(内存)、锁与竞态面、上下文切换 |
| 单线程 poll/epoll/kqueue | 无锁、内存小、顺序可推理 | 必须非阻塞 + 显式状态机,编程复杂 |

对单用户本地服务(少量连接:浏览器几个 tab),事件循环的**可推理性**
(单线程天然串行,无数据竞争)比吞吐更重要;而 ADD-0001 INV-16/GO-02 的
"多 tab 并发"由多路复用直接满足。

`poll()` API(POSIX)——本循环的前身,理解多路复用的最佳起点:

```c
struct pollfd { int fd; short events; short revents; };
int poll(struct pollfd *fds, nfds_t nfds, int timeout);
```

- 每次调用传入关心的 fd 集合与事件(`POLLIN` 可读 / `POLLOUT` 可写),
  返回就绪的 fd 数;`timeout` 用来实现空闲超时巡检;
- macOS/Linux 通用;Linux 上更高吞吐的等价物是 `epoll`,Windows 是
  IOCP——API 不同,**readiness 循环的结构相同**;
- poll 的结构性代价:兴趣集不驻内核,每次调用全量重交数组、每次唤醒
  全量重扫——连接数大时,每个事件都付 O(全部) 而非 O(就绪) 的成本。

### kqueue:内核常驻的兴趣集(现行实现)

Darwin 的原生机制(`<sys/event.h>`),与 Linux `epoll` 同族:兴趣登记一次
驻留内核,唤醒只遍历就绪者。三个核心概念:

- **kq 描述符 + knote**:一次 `kqueue()` 得到一个描述符;每个被监视的
  (fd, filter) 对在内核里是一个 knote。filter 是"事件类别"而不是比特位:
  `EVFILT_READ` / `EVFILT_WRITE` 各占一个 knote——poll 的一个事件位对应
  kqueue 的一个过滤器,粒度不同但语义对得上;
- **changelist + eventlist,一次系统调用**:`kevent(kq, 变更表, n, 事件表,
  m, 超时)` 同时提交兴趣变更并等待事件——变更与等待不必分两次陷入内核;
- **EV_ADD / EV_ENABLE / EV_DISABLE**:EV_ADD 注册(幂等 upsert),
  EV_DISABLE 让 knote 沉默但保留登记,EV_ENABLE 恢复——这正是背压
  ("暂停读但不删登记")和"只在有待发数据时关心可写"的现成工具。

两个 Darwin 特有的坑(实现里都防了):

- **`struct kevent` 必须写全称**:`<sys/event.h>` 同时声明了同名 `kevent()`
  **函数**,普通名字查找命中函数而非类型,类型只能用 elaborated 形式
  `struct kevent` 引用,调用则照仓库惯例用 `::kevent(...)`;
- **EV_SET 是宏不是函数**:它对指针参数逐字段赋值,表达式会被求值多次
  ——`EV_SET(&vec.emplace_back(), ...)` 会 push 好几个元素。先 push,
  再 `EV_SET(&vec.back(), ...)`。

水平触发与边沿触发:kqueue 默认**水平触发**(不加 `EV_CLEAR`),就绪
条件持续成立就持续上报——与 poll 语义一致,漏处理只是晚一轮、不丢
事件。本实现刻意不用 EV_CLEAR,让迁移后的就绪行为与 poll 前身完全
相同;边沿触发的编程负担(每次唤醒必须一次排空)留给未来真需要时。

### readiness 驱动的读写状态机

事件循环的心跳(`service/src/http_server.cpp` 的 `serve`,与设计一致):

```
循环直到 stop 标志:
  从连接状态推导本轮变更表:读过滤器在(终止中/对端EOF/背压超线)时
    EV_DISABLE,恢复时 EV_ENABLE;写过滤器仅在有待发数据时存在
    (EV_ADD/EV_ENABLE,发完 EV_DISABLE)
  kevent(…, timeout = min(所有连接的空闲截止, 250ms 上限)) ← 变更+等待一次调用
  监听 fd 的 EVFILT_READ → accept 循环直到 EAGAIN(非阻塞排空)
  连接 EVFILT_READ 就绪  → recv 进该连接的解码器(RequestDecoder::feed)
                           解析出完整请求 → 生成响应,挂到该连接的写队列
  连接 EVFILT_WRITE 就绪 → send 冲写队列,发完撤回写兴趣
  每轮巡检              → 关闭超过空闲时限的 keep-alive 连接
```

与 poll 版的关键差异:兴趣不再每轮全量重建,而是**增量维护**——每条
连接记住自己两个过滤器的档位(未注册/启用/禁用),档位变化才产生变更
表项。"派生状态不落盘"的原则不变,只是落盘粒度从"整张兴趣表"细化
到"每个过滤器的当前档位"。分派侧:返回的事件用 `ident`(即 fd)标识,
而连接对象住在会搬移的 vector 里,所以按 fd 值查找,绝不把连接指针存
进内核;同一连接**先读后写**的顺序保留(读可能排队出本轮要写的字节)。
变更表提交失败以 `EV_ERROR` 事件的形式返回——发生在监听器上致命,
发生在连接上只死那一条。

要点:

- **非阻塞 socket + 读状态机**:读缓冲空时 `recv` 返回 EWOULDBLOCK,
  不能阻塞线程;解析器的 `incomplete` 对应"这轮读完了,等下一次 POLLIN";
- **写也要状态机**:send 可能只写出部分(内核发送缓冲满),响应剩余部分
  记在连接上(`outgoing` + `outgoing_offset`),等 POLLOUT——这就是
  "readiness-driven read/write state machines"。**SIGPIPE 陷阱**:向已
  关闭的对端 send 默认触发 SIGPIPE 杀死进程;Linux 有 `MSG_NOSIGNAL`
  标志,**macOS 没有**,等价物是每个接受的 socket 上
  `setsockopt(SOL_SOCKET, SO_NOSIGPIPE, 1)`——事件循环里必须做,否则
  一个提前断开的测试客户端就能带走整个服务;
- **Nagle 算法与 TCP_NODELAY**:内核默认开启 Nagle——只要还有一个小段
  未被对端确认,后续的小段就先攒着不发;而对端又默认**延迟 ACK**(等自己
  有数据要回或定时器到期才确认)。两者叠加,HTTP 这种"小请求—小响应"的
  keep-alive 交换会被卡住整整一个 ACK 定时器(经典的 write-write-read
  停顿)。服务端在每个接受的 socket 上 `setsockopt(IPPROTO_TCP,
  TCP_NODELAY, 1)` 关掉 Nagle,排队好的响应立刻出发。它的测试方式值得
  记一笔:内核选项只能通过**描述符本身**观察,而把内部辅助函数暴露给
  测试会违反"测试缝必须是边界"的规范——于是套件在**进程内**(事件循环
  跑在测试二进制的一个线程上)用 `getpeername` 扫描描述符表,找到对端是
  本客户端本地端口的那个 socket,即服务端那一侧,再 `getsockopt` 读选项;
  先完成一次请求—响应,保证连接已被 accept 并配置完毕;
- **每连接一个 `RequestDecoder`**:状态由解析器对象持有,事件循环只做
  搬运(见 [`parser-design.md`](parser-design.md) 第 7 节)。解码器类型
  刻意不可移动(声明析构抑制移动),而连接对象要随 vector 扩容/压缩而
  搬迁——解法是 `unique_ptr<RequestDecoder>` 指针持有,状态原地不动
  (与 [`cpp-language.md`](cpp-language.md) 第 7 节 Pimpl 同型);
- **kevent 超时 = 停止标志延迟 + 空闲巡检粒度**:取"所有连接最近的空闲
  截止时刻"与 250ms 上限的较小者,stop 置位后最多 250ms 被观察到,
  空闲关闭也不必等到整 5 秒的tick;
- **背压**:连接积压的未发送输出 + 未解析输入达到 1 MiB 时 EV_DISABLE
  读过滤器,读方停了内存就有界,写方追平后自动恢复——不丢字节,只暂停读。

### 一个 ASan 抓到的真实顺序缺陷(poll 前身时期的历史)

第一版循环在 `poll` 返回、`accept` 排空新连接之后,按 `connections`
的**当前**大小去读 `poll_fds[i+1].revents`——但 pollfd 数组是 poll 之前
构建的,新接受的连接没有对应项,**越界读**了 vector 之后的内存。非
sanitizer 构建恰好读到零值(revents=0,什么也不做)"碰巧通过";ASan
直接报 heap-buffer-overflow。修复:accept 前先记录 `polled =
connections.size()`,只服务"poll 时已存在"的连接,新连接等下一轮。
教训有两条:**事件来源与连接表必须在同一时刻对齐**;**多路复用循环
的越界在普通构建里经常是沉默的**,socket 套件必须跑 sanitizer。kqueue
版天然保持同一不变量:变更表在循环顶构建,本轮 accept 的新连接尚无
注册的过滤器,事件要到下一轮才来。

### 调度单位是事件,不是连接

"多个连接同时到达,循环是排队处理吗?"——是,但排队的单位是**事件**
而非**连接**,这个区分正是它不慢的原因(见下节)。现行 `serve()` 里能
数出三层串行:

1. **批间**:`kevent()` 一次带回最多 `event_capacity`(=64)个就绪
   事件,处理完这一批才回去等下一批;
2. **批内**:对就绪数组逐项分派,轮到连接 A 就把它的 `recv`、解码、
   handler、响应入队一口气做完,才轮到 B——B 的字节在轮到它之前躺在
   内核 socket 缓冲区里,不丢、也不被 A 碰到;
3. **连接内**:`drive_messages` 的 while 循环反复 `feed()`——一次
   `recv` 可能压着几条管线化请求,逐条"解出 → 处理 → `reset()`"。

两个容易误解的点:

- **handler 不在"后面",它在 feed 循环里内联同步执行**:`feed()` 返回
  完整消息的下一行就是 `handler(message)`——同一线程、同一调用栈,
  异常就地转 500(`append_response` 的 catch)。不存在"先解码一批、
  再统一跑业务"的阶段,跨连接也完全串行(A 的 handler 先于 B 的解码);
  单线程模型里没有 worker 线程或任务队列可以交接;
- **真正推迟的只有 socket 写**:响应字节先入连接的 `outgoing` 队列,
  整个读 pass 结束后,写 pass 才统一 `send()` 冲刷。分离的理由:读
  pass 里刚排入的字节恰好由本轮写 pass 冲刷(顺序保证),读 pass 里
  死掉的连接按 fd 查找自动跳过;顺带把管线化的多个响应攒在一起冲,
  摊薄系统调用次数。

所以"几十个连接同时在被服务"是宏观描述:微观上每条连接的一生被切成
微秒级碎片,与其他连接交错推进;连接的**等待**(等下一批字节、等 TCP
传输)发生在内核里,不占循环一纳秒 CPU——循环只在"有活干"时被唤醒。
术语:**并发但不并行**(concurrent, not parallel);每连接的
`RequestDecoder` 就是连接被挂起时"保存的执行状态",等价于协作式多
任务。第 4 节的队头阻塞由此天然满足:A 的"等字节"是 A 自己连接上的
状态,不是循环的阻塞。

### 单线程的吞吐账与两个软肋

"顺序处理会慢吗?"算一笔:一条请求的全部服务端工作是 `recv`(系统
调用,µs 级)→ ≤8 KiB 头扫描(亚 µs)→ 路由匹配(字符串比较,ns
级)→ 序列化(亚 µs)→ `send`(µs 级),合计**几微秒/请求**,单核
理论十几万 req/s——瓶颈先撞到内核网络栈与系统调用,而不是"只有一个
线程"。对比线程-per-连接:零锁(单执行流 + 每连接独立 decoder = 无
共享可变状态)、无上下文切换、cache 友好(热数据不随线程漂移)、内存
省(每连接一个小 `Connection` 结构,而不是每线程 8 MiB 栈)。Redis
核心路径单线程、nginx 单 worker 服务 10 万级并发连接,都是这套模型
天花板的实证。

两个真实的软肋,与行业解法对齐:

| 软肋 | 表现 | 解法 | 行业实例 |
| --- | --- | --- | --- |
| 慢 handler | handler 跑多久,`kevent` 就多久得不到调用,所有连接一起停 | 重活丢线程池,完成后把响应交回循环写出 | Netty `addLast(businessGroup, …)`、Tomcat Executor |
| 单线程用不满多核 | 一个线程最多吃满一个核 | **N 个事件循环,各自拥有连接子集**(不是给共享状态加锁) | nginx `worker_processes auto`、Netty workerGroup、Tomcat 多 Poller |

行业模型对照(定位 orange 的位置):

- **Netty**:boss 组(通常 1 线程)只做 accept;workerGroup 的 N 个
  线程各是一个事件循环,一条连接注册进某个 worker 后终生不搬家——
  boss/worker 分工是**多核分片**,worker 内部仍是纯正的单线程循环;
  业务线程池是显式接线的第三件事(`pipeline.addLast(businessGroup,
  name, handler)`),默认不存在——handler 默认跑在 worker 循环里,
  与 orange 现状相同;
- **Tomcat(Spring Boot 内嵌)**:Acceptor(1 线程)→ Poller(NIO
  selector,1-2 线程)→ Executor 线程池(默认 `maxThreads=200`)。
  Executor 存在的主因是 Servlet 是**同步阻塞**编程模型:Controller
  等 JDBC/RPC 的几百毫秒里线程 park 睡觉(不烧 CPU)、醒来继续——
  线程池吸收的主要是**等待**而非 CPU;线程数超过核数,对纯 CPU 负载
  完全无效;
- **Redis 6.0 的 io-threads 不是分片**:只把 read/write 系统调用
  并行化,命令执行仍单线程串行,与"N 个独立事件循环"是两回事;
- **orange 现状** = Netty 把 boss、一个 worker、business 全压进一个
  线程的形态;handler 纯内存、微秒级,单循环是匹配范围的选择(源码
  注释自己写明 loopback 小连接数)。

### SO_REUSEPORT:N 个事件循环的分片机制(扩展视野,未落地)

orange 现在是单 listener(一次 `bind` + `listen`)。真要吃满多核,
标准路是把 `serve()` 原样复制 N 份、每份一个线程,每条连接**终生只
属于其中一个循环**——分派由内核完成,用户态零锁:

```
客户端 SYN (192.168.1.5:54321 → :8080)
        │ 内核按四元组哈希选一个 listen socket   ← 分片动作发生在内核
        ├──────────┬──────────┬──────────┐
        ▼          ▼          ▼          ▼
    socket-1    socket-2   socket-3   socket-4   ← SO_REUSEPORT:4 个 socket 绑同一端口
   [线程1: 自己的 kqueue + connections + decoder]
   [线程2 …]  [线程3 …]  [线程4 …]
```

机制要点:

- **默认规则是"一个端口一个 socket"**:第二个 socket `bind` 同端口
  报 `EADDRINUSE`。`SO_REUSEPORT`(Linux 3.9 引入,BSD 系更早;
  Darwin 亦有实现,行为细节与 Linux 有差异)是**全或无的显式豁免**:
  组内**所有** socket 都要在 `bind` 之前设置,Linux 还要求同一
  effective UID——防止别的进程抢绑你的服务端口;
- **做分配的不是 `listen()`,也不是任何用户态代码**:内核 TCP 栈在
  SYN 到达时对四元组(源 IP、源端口、目的 IP、目的端口)做哈希,把
  连接放进选中的 socket 的 accept 队列;`listen()` 只负责把队列建
  起来。没有用户态调度器、没有共享的"待分配连接队列",因此没有锁;
- **四元组在连接生命周期内不变** → 同一连接的所有包哈希恒定,永远
  落同一个 socket、同一个线程。这是"连接不迁移、分片后依然零锁"的
  机制根源;不同连接(源端口不同)被哈希摊开到各线程;
- **与 SO_REUSEADDR 一字之差,两回事**:REUSEADDR 解决**重启**时
  上一进程 TIME_WAIT 残留端口不让绑的问题(第 2 节已在用);
  REUSEPORT 允许多个**活跃** listener 共存并分片;
- **历史**:reuseport 之前 nginx 用 `accept_mutex`——N 个 worker
  抢一把锁去 accept,惊群与锁竞争都是开销;内核按流哈希分队列后这些
  消失。Linux 4.11+ 还能挂 BPF 程序自定义分配策略(比如按负载而非
  哈希);
- **进程还是线程**:nginx 选 worker **进程**是为了故障隔离(一个
  worker 崩,master 拉起,其余不受影响),代价是内存不共享;线程版
  共享地址空间更轻,但一个段错误全军覆没;
- **代价**:分片按连接不按请求,哈希可能不均(大流量连接全压一个
  分片),keep-alive 让归属固化。先跑到单循环瓶颈、再分片,是性能
  工程的一般纪律——对 orange,这也是典型的"需要新 ADR"的决策点。

## 4. 队头阻塞与 AC-7

单线程事件循环必须避免:一个连接上的**未完成请求**(客户端只发了一半)
卡住其他连接。这就是 head-of-line blocking 的应用层形态。ADR-0003 AC-7
的规定:连接 A 的 echo 请求体停在一半时,连接 B 的 `GET /api/health`
必须在一秒内完整返回,B 完成后再补发 A 的剩余字节,A 最终拿到精确回显。

设计含义:A 的"等字节"是**状态**,不是**阻塞**——poll 循环天然满足,
反而是"每请求同步处理完"的写法会违反。TLS 监听(OpenSSL 非阻塞 BIO)
同样要在两个监听上分别通过该测试。事件循环套件已用轻量版钉住机制:
连接 A 只发半截请求(无终止空行),连接 B 的完整请求先得到响应,A
补上剩余字节后也得到响应——"等"发生在 A 自己的连接状态里,循环照常
服务 B。AC-7 的 echo 版本(pause 请求体、双监听)属 T-2。

## 5. keep-alive 与空闲超时

- keep-alive:连接复用,省握手;有效请求的测试模型就是"一个持久客户端
  在一条连接上连续三轮请求-响应"(AC-1),外加同一连接上的管线化对
  (一次写两个请求,收两个响应——解码器 `consumed` 之后的剩余字节留给
  下一条消息,这就是增量解码与管线化的接缝);
- 请求带 `Connection: close` 时服务端必须照办:响应加 `Connection:
  close` 字段、发完即关(RFC 9110 7.6.1 的选项列表解析:逗号分隔、
  大小写不敏感);
- 空闲超时:空闲连接也占 fd;超时关闭把**描述符积累**限制在有界范围
  (资源上限与背压维度)。实现细节:活跃时刻在**读和写两个方向**都刷新
  ——客户端停止读响应时连接同样会因"无进度"超时,不会被写不出去的
  字节永远挂住;测试时超时配成 1 秒加速验证,套件证明连接在配置的
  时限被关、活跃连接不受影响。

## 6. 拒绝后关闭连接(为什么不是继续服务)

ADR-0003(CC-5):431/413 拒绝响应带 `Connection: close`,**响应写完即
关闭**该连接,不做管线化续读。理由在 [`parser-design.md`](parser-design.md)
第 3、5 节:终局错误后输入流的下一个"消息边界"不可信。

实现里的完整生命周期(套件逐字节断言):错误响应先入写队列,连接标记
`closing_after_flush` 并**立刻停止读**(读过滤器 EV_DISABLE,后续字节
不再消费),写过滤器把响应冲干净后关闭——客户端观察到"精确的 431/413/400
字节 → EOF",且**同连接上尾随的管线请求得不到任何响应**;重连后新
请求成功。400(malformed)与 500(资源耗尽)同样走"响应即关"的路径,
而处理器返回的正常响应保持 keep-alive。另一个细节:拒绝前必须把socket
里的字节**读完**(一个 16 KiB 的 read 常常覆盖整个恶意输入)——
否则 close 时接收队列有未读数据,内核发 RST 而不是 FIN,客户端可能
在读到响应前就看到连接重置。

## 7. TLS:锁版 OpenSSL 与自签名证书(待实现)

- 依赖管理:`vcpkg.json` 清单锁 OpenSSL 版本——ADR-0003 明令**加密原语
  绝不手写**;
- 术语:**终止(terminate)TLS** = 在服务端把 TLS 会话解密成明文 HTTP,
  交给应用逻辑;
- 本地自签名:首启生成自签名证书存入 gitignored 位置、跨重启复用;浏览器
  需手动信任,`curl` 侧用 `-k` 跳过校验(仅限本地测试语义);
- 非阻塞 TLS:握手是多步状态机( WANT_READ/WANT_WRITE),要并入 kqueue
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

事件循环套件(`service/tests/http_server_tests.cpp`)自己的客户端工具
箱,每一条都是为了"坏服务必须快败而不是挂死套件"(cpp 标准的确定性
测试要求):

- **SO_RCVTIMEO 截止**:每个客户端 socket 设 2 秒接收超时,服务端死锁
  时 recv 返回 EAGAIN 失败,而不是把 CTest 挂到全局超时;
- **线程只是宿主,不是并发模型**:测试用一个线程运行 `serve`(标准
  要求的 stop 标志模式),主线程做客户端——被测的循环本身仍是单线程;
  `stop_and_join` 保证析构总是可重入地停止;
- **期望字节是独立预言**:断言的响应字节串手写在测试里,不经被测的
  `write_response` 生成——序列化器写错时测试才说得出"错在哪";
- **recv_eof 一石二鸟**:读到 EOF 为真;读到**多余字节**为假——正好
  断言"被拒连接上不得有对管线后续请求的响应"。

## 延伸阅读

- man pages:`kqueue(2)`、`kevent(2)`、`poll(2)`、`socket(2)`、`accept(2)`、
  `recv(2)`(注意 EAGAIN/EWOULDBLOCK 段落);Linux 手册 `socket(7)` 的
  SO_REUSEPORT 段(与 Darwin `socket` 手册对照)、nginx 文档
  `worker_processes`/`accept_mutex` 与 Netty `EventLoopGroup` 指南;
- 《TCP/IP 详解 卷1》第17-22章( TCP 连接管理到拥塞控制);
- C10K 文档(事件驱动并发的经典论述);
- OpenSSL 官方 `SSL_accept`/non-blocking BIO 指南;
- 代码入口:`LoopbackServer::bind/serve`、`serve_loopback`
  (`service/src/http_server.cpp`),`error_response`
  (`service/src/http_response.cpp`),真实 socket 套件
  (`service/tests/http_server_tests.cpp`)。
