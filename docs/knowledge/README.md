# orange 技术知识点索引(Learning Notes)

本目录是**非规范的学习笔记**:整理 ADR-0001 至 ADR-0009 的实现过程中实际
用到的 C++、CMake、网络编程、编译工具链、性能优化与 SIMD、测试验证等知识。
所有 ADR、ADD 与 `docs/development/` 标准仍是权威;本目录与它们冲突时以权威
文档为准。正文以中文讲解,代码、命令与标识符保留英文。

orange 项目本身就把"边做边学"作为目标之一,这些笔记对应的就是仓库里真实
落地的代码与决策——每一条知识点都标注了它在哪个 ADR、哪个源文件中出现,
方便从概念直接跳到实现。

## 文件索引

| 详解文件 | 主题域 | 覆盖的知识点 |
| --- | --- | --- |
| [`cpp-language.md`](cpp-language.md) | C++20 语言与标准库 | 标准锁定、`std::variant`/`std::monostate`、`std::string_view` 借用与悬垂(含返回值悬垂复现)、`std::span`、`std::ranges` 惰性视图、`[[nodiscard]]`、编译期查找表(`constexpr`)、Pimpl 与 `std::unique_ptr`(含"不可移动成员放可移动容器")、`OwnedFd` 移动专属 RAII 与 `std::exchange`、移动语义与 SSO、强异常安全的拷贝赋值、私有构造 + `friend` 的"可信构造边界"、有符号 `char` 陷阱、`std::to_chars` 无分配数字格式化、字面量十六进制转义与聚合部分初始化陷阱 |
| [`http-protocol.md`](http-protocol.md) | HTTP/1.1 报文语法 | RFC 9112/9110/3986/6585 的报文文法:请求行、字段行与 token、OWS/obs-text/obs-fold、Content-Length 与 chunked 编码、chunk 扩展与 trailer、错误到状态码的映射(400/413/414/431/501)、响应序列化(`write_response`:状态行与 reason phrase、编码器独占成帧字段、响应侧 splitting 防线、`Connection: close`)、keep-alive、TLS 监听(已批准待实现) |
| [`parser-design.md`](parser-design.md) | 增量解析器设计 | 为什么要增量解析、显式状态机(`ChunkLine`/`HeadFields`)、错误分类学(`ParseError`)、粘性错误与 `reset`、三重字节预算(8 KiB 头 / 1 MiB 体 / 8 KiB chunk 元数据)、`consumed` 记账规则、分片不变性测试、偏移描述符代替保存视图、跨度推进(`feed(span)`)、解析与序列化共享同一字节文法表 |
| [`network-programming.md`](network-programming.md) | 网络编程 | 回环绑定与安全边界、TCP socket 与 errno 风格错误通道、SO_REUSEADDR/临时端口/端口冲突编码错误、单线程事件循环 vs 每连接一线程、readiness 驱动读写状态机、poll 前身与 kqueue 现行实现的差异(内核常驻兴趣集、EVFILT_READ/WRITE、EV_ADD/ENABLE/DISABLE 增量维护、`struct kevent` 与 EV_SET 的 Darwin 陷阱、水平触发语义)、事件超时即停止延迟、背压读暂停、事件源与连接表对齐(ASan 抓到的越界)、SIGPIPE 与 SO_NOSIGPIPE、队头阻塞、keep-alive 与空闲超时、拒绝后关闭(含 RST/FIN 细节)、两阶段 bind 测试法、SO_RCVTIMEO 客户端截止、TLS/OpenSSL 自签名证书、curl/lsof 端到端验证(TLS/路由为已批准待实现) |
| [`build-and-toolchain.md`](build-and-toolchain.md) | 构建、工具链与编译 | CMake 语法与目标设计、`CMAKE_CXX_STANDARD` 三件套、out-of-source 构建、CTest、编译器警告策略、Debug/Release 与 `-O2 -DNDEBUG`、AddressSanitizer/UBSan、vcpkg 清单、clang-format、GitHub Actions 工作流写法、一次性验证输出清理 |
| [`performance-and-simd.md`](performance-and-simd.md) | 性能优化与 SIMD | 基准测试方法学(计时区外构造、校验和防死代码消除、禁 IPO、独立分配计数程序、等价性对拍)、查找表优化实测数据、分支less 分类、`std::find` 底层的 NEON `memchr`、AVX2/Rosetta 相关性、零拷贝 vs 所有权契约的权衡、行搜索策略与重扫描惩罚 |
| [`testing-and-verification.md`](testing-and-verification.md) | 测试与验证 | 红绿重构的真实提交证据、特征化测试、差分/对拍测试、种子化 sanitizer 模糊测试、边界与分片矩阵测试、超大测试分解、Node.js 测试夹具生命周期治理、CI 一次性验证纪律 |

## 知识点到出处的映射

| 知识点 | 出处的 ADR / 记录 | 代码锚点(节选) |
| --- | --- | --- |
| C++20 所有权建模、variant 错误通道 | 项目 ADR-0003(T-1) | `service/src/http_messages.hpp`(`Request`、`ParseError`、`parse_request`) |
| 增量解码、粘性错误、字节预算 | 项目 ADR-0003;工程 ADR-0008/0009 | `service/src/http_request_decoder.hpp`(`RequestDecoder::feed/reset`、三个 `*_cap`) |
| chunk 元数据状态机与跨度推进 | service ADR-0002 | `service/src/http_chunk_line.hpp`(`ChunkLine::feed(span)`) |
| 分片头偏移保留 | service ADR-0003 | `service/src/http_head_fields.hpp`(`HeadFields`)、`http_syntax.hpp` |
| 所有权存储与偏移描述符 | service ADR-0004 | `detail::HeaderFieldOffsets`、`Request::headers()` 惰性范围 |
| RFC 9112/9110 文法与错误映射 | 项目 ADR-0003 | `ParseError` 枚举注释逐条标注 RFC 节号 |
| 响应成帧与状态行序列化 | 项目 ADR-0003(T-1) | `service/src/http_response.hpp`(`write_response`、`Response`、`ResponseError`) |
| 查找表与解析器实测 | 项目 ADR-0003(优化证据)、ADR-0008 | `detail::ByteClassTable`、`service/bench/*` |
| 基准方法学与分配计数 | 项目 ADR-0008 | `service/bench/README.md`、`http_parser_allocations.cpp` |
| sanitizer、CI 一次性验证 | 项目 ADR-0009 | `.github/workflows/http-performance.yml` |
| 测试分解与夹具治理 | service ADR-0001、项目 ADR-0007 | `tools/governance-validator/test/fixtures.mjs` |
| `kqueue()` 事件循环与连接生命周期(poll 前身已迁移) | 项目 ADR-0003(T-1,已实现) | `service/src/http_server.hpp`(`LoopbackServer`、`serve_loopback`)、`service/src/http_response.cpp`(`error_response`) |
| TLS 监听、路由与静态服务 | 项目 ADR-0003(T-2/T-3,已批准待实现) | 规划锚点 `service/src/tls.cpp`、`routes.cpp`(未落地) |

## 建议阅读顺序

1. [`http-protocol.md`](http-protocol.md) — 先建立"解析什么"的协议心智模型。
2. [`parser-design.md`](parser-design.md) — 再看"怎么在字节流上安全地解析"。
3. [`cpp-language.md`](cpp-language.md) — 解析器用到的语言机制集中讲解。
4. [`build-and-toolchain.md`](build-and-toolchain.md) 与 [`testing-and-verification.md`](testing-and-verification.md) — 工程化两翼:怎么构建、怎么证明正确。
5. [`performance-and-simd.md`](performance-and-simd.md) — 在正确性之上做优化。
6. [`network-programming.md`](network-programming.md) — 面向 T-2/T-3 的运行时蓝图。

## 状态与维护

- 笔记描述的是各 ADR **被批准/完成时点**的知识用法;后续实现演进以源代码与
  ADR 证据为准,本目录不承担同步义务。
- 解析器、响应序列化与错误映射、单线程 `kqueue()` 事件循环(poll 前身经
  2026-10-20 失效性修订与重批准后迁移)对应 ADR-0003 的
  T-1(已 Complete),均有实现代码;网络编程一文中仍标注"待实现"的部分
  (TLS、路由/静态服务、状态页)对应 T-2/T-3 子任务,引用的是该 ADR 的
  Scope 与验收检查。
