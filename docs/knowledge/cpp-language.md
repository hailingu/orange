# C++20 语言与标准库 — orange 解析器用到的机制

对应索引:[README.md](README.md) › 文件索引。本文讲解 `service/src/` 生产代码
中实际使用的 C++20 机制,每个主题都指向真实代码。

## 1. 标准版本锁定

`service/CMakeLists.txt` 用三行把语言标准钉死在 C++20:

```cmake
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
```

- `REQUIRED ON`:编译器不支持 C++20 时直接报错,而不是**静默降级**到旧标准
  再憋出一堆晦涩的编译错误。
- `EXTENSIONS OFF`:禁用 GNU 方言扩展(如 `__attribute__` 混入标准头),
  保证代码是可移植的标准 C++。

## 2. `std::variant` 作为错误通道

HTTP 解析的返回值天然是"成功或多种失败之一"。orange 用
`std::variant<ParsedRequest, ParseError>` 表达这种和类型(sum type):

```cpp
[[nodiscard]] std::variant<ParsedRequest, ParseError> parse_request(std::string_view raw);
```

(`service/src/http_messages.hpp`)

- 相比异常:错误是**预期结果**而非异常路径,调用方(事件循环)必须显式分派,
  不会漏处理;相比 `std::optional`:能携带**哪一种**失败及其语义
  (`incomplete` 要继续读,`malformed` 要答 400,`headers_too_long` 要答 431)。
- 增量解码器更进一步,用三态变体
  `std::variant<std::monostate, ParsedMessage, ParseError>`:
  `std::monostate` 表示"输入还不够,既没成功也没失败",这是 variant 表示
  "空/进行中"状态的标准手法(`http_request_decoder.hpp` 的 `DecodeResult`)。
- 项目早期版本曾用 `std::optional<Request>`(`parse_request("")` 返回
  `std::nullopt` 的第一条契约),后来随着错误分类细化升级为 variant——
  这是 API 演进的常见路径:先二值,后分类。

## 3. `std::string_view`:借用与悬垂

`string_view` 是不拥有数据的"窗口",零拷贝但要求被视数据活得比视图久。
orange 的用法分两层,边界画得很清楚:

- **解析输入**全程是 `std::string_view raw`——解析器不复制调用方的输入,
  只读。
- **解析结果** `Request` 则**拥有**字符(`std::string head_`),访问器
  `method()`/`target()` 返回的视图借用自 `Request` 自身,头文件注释明确
  写出生命周期契约:"Views borrow from the owning Request...they expire
  when that Request is destroyed, assigned, or moved from."

两个真实教训记录在 ADR-0003 证据里:

1. **悬垂陷阱**:测试辅助 `parsed_of` 一度返回绑定到**临时 variant** 的
   引用,ASan 报 stack-use-after-scope。教训:variant/容器链上的临时对象
   + `string_view` 是悬垂高发区。
2. **性能 ≠ 正确契约**:外部贡献的零拷贝 `string_view` Request 变体实测
   更快(short valid 14.3 ns vs 23.9 ns),但被拒绝——因为它把结果的生命
   期绑到输入缓冲区,破坏所有权契约。见
   [`performance-and-simd.md`](performance-and-simd.md) 第 6 节。

## 4. `std::span` 与 `std::ranges` 惰性视图

`Request::headers()` 是全仓库最"现代 C++"的一段:

```cpp
[[nodiscard]] auto headers() const noexcept {
    return std::span(fields_) | std::views::transform([bytes = std::string_view(head_)](
                                                          detail::HeaderFieldOffsets field) {
               return HeaderField{{bytes.data() + field.name_begin, field.name_size},
                                  {bytes.data() + field.value_begin, field.value_size}};
           });
}
```

### 4.1 先拆开看:这行代码由三层组成

1. **源范围** `std::span(fields_)`:把成员 vector "降维"成一段连续内存的
   非拥有视图;
2. **适配器管道** `| std::views::transform(...)`:在视图上按需逐元素做
   映射;
3. **`auto` 返回**:组合出的视图类型无法命名,按值返回(极廉价)。

### 4.2 `std::span`:连续内存的非拥有视图

- **本质是指针 + 长度**,不是容器。拷贝一个 span 就是拷两个标量,所以
  接口惯例是**按值传递**(比传引用还少一层间接)。仓库里 `Request` 的
  私有构造函数就用它接收字段表:
  `Request(std::string_view, RequestLineOffsets, std::span<const HeaderFieldOffsets>)`。
- **CTAD 与 const 正确性**:`std::span(fields_)` 不写模板参数,由推导
  指南从容器推。关键细节:这段代码在 **const 成员函数**里,`fields_`
  的类型是 `const std::vector<...>`,于是推导结果是
  `std::span<const HeaderFieldOffsets>`——**只读性自动成立**,调用方
  无法通过视图写字段。
- **只服务连续内存**:vector、`std::array`、C 数组都能直接构造 span;
  `std::list`/`std::deque` 这类非连续容器不行(名字里的 "span" 指的就是
  "一段连续内存的跨度")。这换来的是 `data()` + `size()` 的零开销访问和
  与 C API 的无缝衔接。
- **与 `string_view` 的分工**:`string_view` 专指字符序列且只读;
  `span<T>` 适用于任意元素类型,且 `span<T>`(非 const T)**允许写穿**
  到所指元素——所以只读接口应显式写 `span<const T>`,如本例。
- **extent(长度是编译期已知与否)**:`span<T>` 默认动态长度;
  长度编译期已知时可写 `span<T, N>`,对象里连长度字段都能省掉。
- 常用操作:`size()/empty()/data()/front()/back()`、下标,以及切片
  `first<N>()/last<N>()/subspan(off, n)`——都不拷贝数据,只调整窗口。
- **代价:不拥有**。源容器销毁或扩容(vector 重新分配使旧 `data()`
  失效)之后,span 就是悬垂指针 + 过期长度,**不会有任何运行期报警**,
  只能靠生命周期纪律兜底(见 4.4)。

### 4.3 ranges 视图与 `|` 管道:惰性在哪里

- C++20 的"范围(range)"统一了遍历语义:任何有 begin/end 的东西都是
  范围,算法库(`std::ranges::find` 等)直接吃范围而非迭代器对。
  **视图(view)** 是其中特殊的一类:O(1) 拷贝/移动、**不拥有元素**。
  `std::views` 是 `std::ranges::views` 的别名。
- `views::transform(f)` 返回的 `transform_view` 只是把"(底层范围, f)"
  打包;**遍历时才对每个元素现调 f**——全程没有任何中间容器。1000 个
  字段也只是 1000 次"查表翻译",零分配。
- `|` 管道语法是嵌套调用的语法糖:`r | views::transform(f) |
  views::filter(p)` 等价于 `filter_view(transform_view(r, f), p)`,
  从左往右读就是数据流向。常用家族:`transform`(映射)、`filter`
  (按谓词过滤)、`take/drop`(取前/去前 N 个)、`iota`(生成整数序列)、
  `reverse`(需要双向范围)。
- **范围类别会被保留**:`span` 是"连续 + 随机访问 + sized"的范围,
  `transform_view` 原样继承这些性质——所以 `headers()` 的返回值能
  `.size()`、能下标随机访问(这正是真实头文件注释 "sized/indexable"
  的含义),不是只能从头走到尾的单遍流。
- **性能**:transform 在优化器眼里就是"循环体里调一个可内联的小函数",
  内联后与手写循环无异;本例循环上界还被 8 KiB 头预算封顶。

### 4.4 生命周期:三层借用,两条规则

这段代码同时存在**三层借用**,悬垂风险也分三层:

1. `span` 借用成员 `fields_`——`Request` 活着才有效;
2. lambda 里的 `bytes` 借用成员 `head_` 的字符——同上;
3. `transform_view` 的**迭代器借用视图对象本身**(其内部持有指回视图
   的指针以便调用函数)——这层最反直觉:视图对象销毁后,**已取出的
   迭代器**悬垂。

由此得出两条使用规则:

- **规则一**:遍历期间 `Request` 必须存活(头文件注释 "The range and
  fields borrow from this Request" 即此意),与第 3 节 `string_view` 的
  纪律同源;
- **规则二**:迭代器存活期间,视图对象必须存活(注释 "iterators
  additionally require the returned range object to remain alive")。

好消息:范围 for 天然满足规则二——`for (auto f : req.headers())` 中,
临时视图被绑定到隐藏的 `auto&&`,生命周期延长到循环结束。坏消息的
典型写法:

```cpp
auto it = std::ranges::begin(req.headers());  // 临时视图在本行末销毁
++it;                                         // 悬垂迭代器:未定义行为
```

标准库提供了部分防线:`std::ranges` 算法收到**右值范围**时返回
`std::ranges::dangling` 而非悬垂迭代器,把一类错误从运行期 UB 提前到
编译期;但 range-for 与手存迭代器仍只能靠上面两条规则。

另一个值得学的细节是 lambda 的**初始化捕获**
`[bytes = std::string_view(head_)]`:它把"指针 + 长度"**按值拷进闭包**
(而不是捕获 `this` 或引用 `head_`),闭包因此可以自由拷贝移动;
当然字符数据本身仍属于 `Request`——第 2 层借用并没有消失。

### 4.5 返回 `auto` 与"物化"

- 管道组合出的类型形如 `transform_view<span<const HeaderFieldOffsets>,
  <lambda 类型>>`,而 **lambda 类型每个都独一无二、无法拼写**,
  所以返回类型只能写 `auto`。它是纯视图,按值返回只是搬几个指针。
- C++20 没有标准的"视图 → 容器"物化操作;若真要落成
  `std::vector<HeaderField>`,得手写循环或 `ranges::copy`。C++23 才有
  `req.headers() | std::ranges::to<std::vector>()`。本仓库的调用方全部
  直接遍历,从不物化——**惰性设计配合"只用一次"的访问模式才是零成本**。

### 4.6 对照:另外三种设计为什么没被选

| 备选设计 | 问题 |
| --- | --- |
| 返回 `const std::vector<HeaderField>&` | 必须提前物化并双份存储(字符 + 每字段独立 string),1000 字段即上千次分配;还把存储形态焊死在公共 API 上 |
| 提供 `header(size_t i)` 下标访问器 | 可用,但遍历场景啰嗦,表达不出"这是一段有序字段"的范围语义 |
| 公开 `detail::HeaderFieldOffsets` | 把 `detail::` 实现细节泄漏进公共契约,破坏第 9 节的可信构造边界 |

惰性视图同时拿到三件事:**零分配、零双份存储、实现细节封死在
detail 层**;代价只是 4.4 的生命周期纪律——这正是 service ADR-0004
"所有权存储 + 惰性物化"设计的落点。

## 5. `[[nodiscard]]`

解析函数全部标注 `[[nodiscard]]`(`parse_request`、`RequestDecoder::feed`)。
忽略返回值(忘了检查解析结果)直接变成**编译错误**。对于"返回值就是全部
意义"的查询/解析接口,这是零成本的强制防线。

## 6. 编译期查找表:`constexpr` 构造函数 + `inline constexpr` 实例

`service/src/http_syntax.hpp` 的 `ByteClassTable` 是性能优化的核心,也是
C++ constexpr 能力的好例子:

```cpp
struct ByteClassTable {
    enum Mask : std::uint8_t { digit = 1 << 0, hexdig = 1 << 1, /* ... */ };
    std::uint8_t masks[256] = {};
    constexpr ByteClassTable() { /* 256 次循环填表 */ }
};
inline constexpr ByteClassTable byte_classes{};
```

- 构造函数标 `constexpr`,配 `inline constexpr` 实例:整张 256 字节的表在
  **编译期生成**,放进 `.rodata`,运行期零初始化成本。
- 每个字节分类(`is_digit`/tchar/target 字符等)从多次 `string_view::find`
  集合查找折叠为**一次表加载 + 位与**。实测收益见
  [`performance-and-simd.md`](performance-and-simd.md)。

## 7. Pimpl 与 `std::unique_ptr`

`RequestDecoder` 公开头文件只有:

```cpp
class RequestDecoder {
  public:
    RequestDecoder() noexcept;
    ~RequestDecoder();
    RequestDecoder(const RequestDecoder &) = delete;
    // ...
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;   // 摘录,略去其余成员
```

(Pimpl idiom,`impl_` 指向前向声明的 `Impl`。)

- **编译防火墙**:`http_chunk_line.hpp` 等内部实现变化不触发使用方重编译。
- **头文件干净**:复杂的 `ChunkLine`/`HeadFields` 状态机不出现在公共接口。
- 细节: destructor 必须在 `.cpp` 里定义(此时 `Impl` 已完整),所以头文件
  里只声明 `~RequestDecoder()`。拷贝被 `= delete`——每连接的解码器持有
  独立状态,拷贝没有合理语义。

## 8. 移动语义、SSO 与"偏移代替视图"

service ADR-0004 的关键设计:`Request` 内部**不存 `string_view` 字段**,而存
`std::uint16_t` 偏移描述符(`HeaderFieldOffsets{name_begin, name_size, ...}`)。
原因写在类型注释里:

> Offsets, rather than saved views, survive both short-string and heap-backed
> moves.

`std::string` 有 SSO(短字符串优化):短字符串存在对象内部的栈缓冲区。
移动一个 SSO 字符串是**逐字节拷贝**,源对象的内部指针随即失效——如果
字段视图缓存了 `data()` 指针,移动后就是悬垂指针。偏移(下标)则永远随
`head_` 一起有效。`version()` 里 `head_.empty() ? 0U : 8U` 这种细节也在
防御空值/移动后状态。

配套的还有手工写的移动构造/赋值(`noexcept`,移动后源为安全空态)与
**强异常安全**的拷贝赋值:先构造临时再 `swap`,两段分配都成功才提交——
`Request &operator=(const Request &other)` 因此不能是 `= default`。

## 9. 私有构造 + `friend`:可信构造边界

`Request` 的构造函数是私有的,只对两处开放:

```cpp
friend class detail::HeadFields;
friend std::variant<ParsedRequest, ParseError> parse_request(std::string_view raw);
```

含义:**只有通过文法校验的代码路径才能构造 `Request`**。外部拿不到
"未验证的头部"对象——不变量(方法 token 合法、target 百分号编码合法、
偏移不越界)由类型系统兜底,而不是靠调用方自觉。这是"make illegal states
unrepresentable"在 C++ 的落地方式之一。

## 10. 有符号 `char` 陷阱

`http_syntax.hpp` 的分类函数注释点名了这个问题:平台 `char` 可以是
signed(macOS arm64 上是 signed)。字节值 `>= 0x80` 时,`char` 变负数,
直接当下标索引 256 表就是**越界**。所以分类器先把八位组转成无符号再查表:

```cpp
constexpr bool is_digit(char c);  // 内部按 unsigned 索引
```

obs-text(`0x80-0xFF` 的字段值字节,RFC 9110 5.5 允许)使这条路径真实可达。

## 11. 分配失败的传播策略

`Request` 的普通拷贝可能抛 `bad_alloc`。仓库的策略是**不吞**:解析器把
分配失败映射为编码错误 `ParseError::resource_exhausted`(HTTP 500),
`RequestDecoder` 文档注明 "Allocation failure returns resource_exhausted"。
错误处理原则(显式、结构化、不掩盖)来自
`docs/development/cpp-development-standard.md`。

## 延伸阅读

- cppreference:`std::variant`、`std::string_view` 生命周期 hazards、
  `std::span`、ranges 库、constexpr。
- ADR 证据:`docs/adr/ADR-0003-local-runtime-path.md`(Request-Head
  Optimization Evidence 节);
  `service/docs/adr/ADR-0004-owned-request-storage.md`。
