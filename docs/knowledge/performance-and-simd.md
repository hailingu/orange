# 性能优化与 SIMD — 实测驱动的方法学

对应索引:[README.md](README.md) › 文件索引。素材来自
`docs/adr/ADR-0003-local-runtime-path.md`(Request-Head Optimization
Evidence 节)、ADR-0008 与 `service/bench/`。orange 的优化有一条清晰纪律:
**先测、再改、改完对拍,速度永远不凌驾契约**。

## 1. 基准方法学:数字怎么才算数

`service/bench/` 的设计(ADR-0008)每一条都在回答"这个纳秒数可信吗":

| 手段 | 防的失真 |
| --- | --- |
| 夹具(fixture)构造与一次验证遍历放在**计时区外** | 把"准备数据"算进解析时间 |
| 结果观察用**计时后的 JSON 校验和** | 死代码消除:编译器把没被消费的计算整段删掉 |
| 驱动程序 `INTERPROCEDURAL_OPTIMIZATION FALSE`、禁 LTO | 跨模块内联把基准对象变成"另一个函数" |
| 每次测量包含 decoder 构造/结果物化/观察/析构 | 只测热循环、漏掉真实生命周期成本 |
| 全新 decoder 而非容量复用的 reset 循环 | 高估稳态复用的吞吐 |
| 报告烙入编译器/标志/架构(`ORANGE_BENCH_*` 宏) | 脱离上下文比数字 |
| 分配计数是**独立可执行文件**(`http_parser_allocations`) | 分配钩子与计时钩子互相污染 |
| 等价性对拍程序(`http_parser_equivalence`) | 优化改了行为 |
| CI 阈值定位为 **量级护栏**(magnitude guard) | 共享 runner 噪声触发假警报 |

18 个夹具刻意覆盖两个极端与中段:18 字节最小头、434 字节 9 字段头、
8,192 字节 409 字段头、1 MiB 内容的 1 KiB/16 KiB chunk、1 MiB 定长体
16 KiB 传输分片、1 KiB 回显响应序列化、7,000 字节扩展、CR 密集的恶意头
——**平均数会撒谎,分布的两端才会说话**。

## 2. 查找表(LUT):一次实测的重构

**问题**:请求头文法要多次判断"字节属于哪个集合"(tchar?十六进制?
URI 字符?),原实现用 `string_view::find` 在特殊字符集合串里逐个找。

**重构**(`service/src/http_syntax.hpp`):编译期生成 256 项 `uint8_t`
位掩码表(详见 [`cpp-language.md`](cpp-language.md) 第 6 节),每个分类
从"多次集合查找"变成**一次表加载 + 位与**:

```cpp
// 概念示意(实际见 http_syntax.hpp)
inline constexpr ByteClassTable byte_classes{};
constexpr bool is_digit(char c) {
    return byte_classes.masks[byte_index(c)] & ByteClassTable::digit;
}
```

**实测**(Apple clang -O2,arm64,ADR-0003 证据):

| 夹具 | 重构前 | 重构后 |
| --- | --- | --- |
| 短合法请求行 | 31.4 ns | 23.9 ns |
| 200 字节标点密集 target | 419 ns | 126 ns |
| 8 KiB 溢出分类 | 4,651 ns | 2,559 ns |

**验证**:16 输入三向变体对拍(旧实现/新实现/外部贡献)0 不一致 +
全套件绿,行为等价才保留。

这是 LUT 的标准适用场景:**小输入域**(字节只有 256 种)、**高频查询**、
**多谓词可折叠进位掩码**。同类思路:ctype 表、base64 解码表、
UTF-8 分类表。

## 3. SIMD:向量化到底是什么

SIMD(Single Instruction Multiple Data)一条指令同时处理一个"向量"里的
多个数据:arm64 NEON 是 128 位(16 字节/条),x86 AVX2 是 256 位。理想
情况下扫 16 字节的时间接近扫 1 字节。

**orange 的立场很克制:不手写 SIMD,但理解库函数何时替你做了**:

- `std::find(buf.begin(), buf.end(), '\n')` 在 arm64 上由标准库/编译器
  下沉为 **NEON 加速的 `memchr`**——找单字节这种"判断连续内存中哪个
  位置满足谓词"正是 SIMD 最擅长的模式(加载向量 → 广播目标字节 →
  逐 lane 比较 → 提取首个命中掩码位);
- 外部贡献里的**手写 AVX2 分支**在本机 arm64(无 Rosetta)上是死代码,
  "实测更快"的结论因此不可信——**验证优化前先确认它真的在跑**;
- 基准只 `-O2` 不开 `-march=native`:仓库刻意不把二进制绑死单机指令集。

什么时候才值得手写 SIMD:库原语(`memchr`/`memcmp`/`strlen`)覆盖不到
的复合谓词、且剖析证明该循环占大头。在此之前,"表格 + 让编译器做"是
性价比最高的"SIMD"。

## 4. 行搜索策略:一个反直觉的案例

找行结尾,直觉写法是搜 CRLF 两字节序列。旧实现曾因此付出代价:**CR 密集
输入**(8 KiB 全是 `\r`)让基于 `memchr('\r')` 再验证的搜索**反复重启**
——每次命中假 CR 都停一次、验一次、再从下一字节重启,实测退化约 23 µs
(对比正常输入的百纳秒级)。

修复:改为**单字符搜索 `find('\n')` + 一次性回看一字节**:
单字符搜索从头到尾**一次推进、永不重启**,回看只做一次。

配套先行的是**特征化测试**(characterization tests,提交 `211c62b`):
先把"token 不得跨越首个 CRLF、裸 `\r` 留在 token 内"等既有行为用测试
钉死,再动实现,13 输入对拍 0 不一致。

教训:算法级的"怎么扫"常比微优化(指令选择)重要一个数量级;
**最坏情况**(adversarial 输入)必须进夹具。

## 5. 分配与所有权:被拒绝的"更快"

外部贡献的零拷贝 `Request`(字段直接 `string_view` 指进调用方输入)实测
short valid 14.3 ns,快于所有权版本的 23.9 ns。**被拒绝**,理由:

- 解析结果的生命期将绑死在调用方的接收缓冲上——事件循环复用/回收缓冲
  时结果悬垂;
- 违反 ADR-0003 的所有权契约("owned-copy contract")。

替代路径是让**所有权本身更便宜**(service ADR-0004):

- 头部一次性连续存储(`std::string head_`),字段用 `uint16_t` **偏移
  描述符**(4×2 字节/字段)而非每字段独立 string——一次分配代替 N 次;
- 描述符随 SSO 移动安全(见 [`cpp-language.md`](cpp-language.md)
  第 8 节);
- `headers()` 惰性物化视图,遍历零分配;
- 独立的分配计数基准确认"搬家后分配次数不升、有界物化成本"。

性能工作的完整闭环:**契约定义边界 → 基准找热点 → 对拍保行为 →
在边界内变快**。

## 6. 安全与性能同源

三重字节预算(8 KiB/1 MiB/8 KiB)与"每次行搜索以剩余预算为界"既是
DoS 防线也是性能保证(无界重扫不存在,见 [`parser-design.md`](parser-design.md)
第 4 节);CR 密集夹具同时是"最坏情况延迟"与"恶意输入"的测试。安全
上限与性能上限在这里是同一行代码。

## 延伸阅读

- Chandler Carruth《There Are No Shortcuts》——基准反模式清单;
- Agner Fog 的优化手册与指令延迟表;arm64 NEON intrinsics 指南;
- `memchr` 在 glibc/LLVM libc 中的 SIMD 实现(读真实的生产级向量化);
- ADR-0008 决策正文与 `service/bench/README.md`。
