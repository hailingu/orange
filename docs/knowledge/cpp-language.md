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

- `std::span<const HeaderFieldOffsets>`:对连续描述符数组的**非拥有视图**,
  传参/遍历零拷贝。
- `std::views::transform`:**惰性**范围适配器,不物化中间 vector——
  头部字段按需逐个由"偏移描述符"翻译成 `HeaderField` 视图。1000 个字段
  也不会多一次分配。
- 注意 lambda 把 `head_` 先转成 `string_view` 捕获,保证管道各站借用同一份
  存活的数据。

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
