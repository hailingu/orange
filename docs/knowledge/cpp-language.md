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
- 响应序列化器复用同一模式:`std::variant<std::string, ResponseError>`
  (`http_response.hpp` 的 `write_response`)——序列化的失败同样分多类
  (状态码范围、禁正文状态、字段文法、成帧冲突、分配失败),且**捕获
  `std::bad_alloc` 后翻译成 `resource_exhausted` 返回**,不跨模块边界抛
  异常(错误策略见第 11 节)。
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
3. **同一陷阱在另一层复现**(事件循环套件,2026-10-19):处理器把指向
   **函数局部 `std::array`** 的字段 span 塞进按值返回的 `Response`——
   return 一到,数组死亡,序列化读到的是尸体。修复:字段存储提升到
   处理器存活期。教训:借用契约("views borrow caller storage")对
   **返回值**尤其危险——函数返回的那一刻,它栈上的一切视图都过期;
   返回带视图的聚合时,视图必须指向比函数活得久的东西。

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

(Pimpl idiom,"pointer to implementation",别名 handle-body / Cheshire Cat:
公共类只是"把手",真正的实现藏在指针对面。)

### 7.1 `Impl` 里到底装了什么

看一眼 `.cpp` 里的真实定义就能理解为什么值得藏起来——`Impl` 持有的是
**一条连接解码过程中的全部可变状态**:

```cpp
// service/src/http_request_decoder.cpp(摘录,略去 trailer 相与辅助函数)
struct RequestDecoder::Impl {
    enum class Phase { head, fixed, chunk_line, chunk_data, chunk_cr,
                       chunk_lf, trailers, complete, failed };
    Phase phase = Phase::head;
    std::string head;                // 未完成的头部缓冲(受 8 KiB 预算)
    detail::HeadFields head_fields;  // 分片头部的偏移登记
    detail::ChunkLine chunk_line;    // chunk 尺寸行状态机
    ParsedMessage message;           // 半成品消息
    std::size_t remaining = 0;       // 定长体剩余字节
    std::size_t metadata_bytes = 0;  // 元数据预算记账
    ParseError error = ParseError::malformed;
};
```

这些成员的类型(`HeadFields`、`ChunkLine`)和它们的演化被完全挡在
`http_request_decoder.hpp` 之外:公共头里连 `#include` 都不需要带它们。

### 7.2 编译防火墙为什么有效

- C++ 的编译模型是**文本包含**:`#include "x.hpp"` 把头文件原样抄进来。
  即使成员是 `private`,它仍然出现在类定义里、参与对象布局——给私有
  部分加一个字段,类的**定义**就变了,所有 include 过这个头的翻译单元
  都得重编译。
- Pimpl 之后,公共类的布局恒定为**一个 `unique_ptr`(加一个 bool,见
  7.4)**,与 `Impl` 内部怎么增删成员完全无关。改 `Impl` 只重编译
  `http_request_decoder.cpp` 一个文件。
- 前提是**不完整类型(incomplete type)**能用:`struct Impl;` 只前向
  声明,头文件里 `Impl` 是不完整的。标准设施里,`unique_ptr` 是为不完整
  类型量身定制的通道——`sizeof(std::unique_ptr<Impl>)` 与 `Impl` 无关,
  恒等于一个指针的大小;而按值持有 `Impl` 根本不可能(类布局需要
  `sizeof(Impl)`)。
- 附赠收益:**ABI 稳定**。库以二进制发布时,Pimpl 类的对象布局跨版本
  不变,使用方不必因实现演进而重新编译(解释了为什么 Qt、OpenSSL 这类
  C++/C 库的公共句柄全是这种形状)。

### 7.3 `unique_ptr` 与不完整类型:三条特殊成员规则

本仓库的写法就是教科书答案——头文件只**声明**特殊成员,定义放到
`.cpp`(那里 `Impl` 已完整):

```cpp
// .hpp:  RequestDecoder() noexcept;   ~RequestDecoder();
// .cpp:  RequestDecoder::RequestDecoder() noexcept = default;
//        RequestDecoder::~RequestDecoder() = default;
```

- **为什么析构不能留在头文件**:默认删除器 `default_delete<Impl>` 执行
  `delete p` 时需要 `sizeof(Impl)`(先析构再释放)。若析构函数在头文件
  内隐式生成(或就地 `= default`),编译器就会在 `Impl` 不完整处实例化
  它——直接编译错误("invalid application of 'sizeof' to an incomplete
  type")。放到 `.cpp` 定义即解决,**`= default` 也可以,只要定义出现在
  类型完整处**。
- **构造函数同理**:即便什么都不做,也要在 `.cpp` 里出定义。
- **移动语义的连锁反应**:显式声明了析构函数,隐式移动构造/移动赋值就
  不再生成;加上 `RequestDecoder(const RequestDecoder &) = delete;`
  ——这个类**既不可拷贝也不可移动**,只能原地使用。对"每连接恰好一个
  解码器"的语义,这不是损失而是约束的正确形状(状态含在途请求,搬走
  或复制都没有合理含义)。
- **对照 `shared_ptr`**:它对不完整类型更宽容(删除器在**构造点**被
  类型擦除捕获,只要构造时完整,销毁点可以不完整)——但代价是原子
  引用计数、控制块分配、两倍指针体积。`unique_ptr` 在默认删除器下
  **零开销**(大小就是一个裸指针),这才是 Pimpl 的标准搭配。

### 7.3.1 同一手法解决"成员不可移动":事件循环里的解码器

`unique_ptr` 持有成员还有第二个用途,与编译防火墙无关:`RequestDecoder`
按其契约**既不可拷贝也不可移动**(显式删除拷贝 + 声明析构抑制了隐式
移动,见 7.3),而事件循环的 `Connection` 结构体住在 `std::vector`
里——vector 扩容(`emplace_back`)和压缩(`remove_if`)都要求元素可
移动。按值持有一个不可移动成员会让整个 `Connection` 成为 vector 放不
下的类型(编译器直接报 Cpp17MoveInsertable 静态断言)。解法一行:

```cpp
struct Connection {
    OwnedFd fd;
    std::unique_ptr<RequestDecoder> decoder;  // 指针持有:状态原地不动
    ...
};
```

移动 `Connection` 只是搬一个指针,解码器对象连同它的在途解析状态留在
原堆内存里——语义正确(状态不丢)且搬移廉价。**"不可移动的成员 +
必须可移动的容器" → 指针持有**,与 Pimpl 是同一个模式服务于两个理由
(编译隔离 vs 可移动性)。

### 7.4 懒分配与 `noexcept` 构造:真实代码的两个决策

公共类其实有**两个**数据成员,而不是一个:

```cpp
std::unique_ptr<Impl> impl_;
bool allocation_failed_ = false;
```

- **构造零分配**:`RequestDecoder() noexcept = default;` 让 `impl_`
  初始为空——头文件注释 "Creates an idle decoder with no borrowed input
  or body allocation" 是诚实的:构造函数真正 `noexcept`,不碰堆。
- **分配推迟到第一次 `feed`**:`if (!impl_) impl_ = std::make_unique<Impl>();`,
  并用 `try/catch` 把 `std::bad_alloc` 翻译成编码错误:

  ```cpp
  } catch (const std::bad_alloc &) {
      allocation_failed_ = true;
      impl_.reset();
      return {0, ParseError::resource_exhausted};   // HTTP 500
  }
  ```

  为什么标志放在 `Impl` **外面**:`bad_alloc` 发生后 `impl_` 已被清空,
  失败状态必须存在**不需要分配就能表示**的地方——外层一个 `bool`。
- 源码注释接着写道:"No usable input boundary survives a resource
  failure. The caller must discard this connection's input rather than
  retry the fragment."——分配失败与文法失败同样终局(与
  [`parser-design.md`](parser-design.md) 第 3、5 节的粘性错误/丢弃记账
  规则同一条纪律),`feed` 开头对 `allocation_failed_` 的检查保证它是
  粘性的。
- **`reset()` 复用分配**:`*impl_ = Impl{};` 就地用一个全新 `Impl`
  覆盖状态,`Impl` 对象本身的堆内存不释放重建;若从未分配过,只清标志。
- **`make_unique` 而不是裸 `new`**:C++14 起的标准做法,少写一次类型名,
  且构造参数求值期间的异常不留泄漏窗口。

### 7.5 `std::unique_ptr` 要点速览

- **独占所有权**:同一时刻只有一个 `unique_ptr` 拥有对象;`move` 转移
  所有权,`copy` 被删除;离开作用域自动 `delete`。
- **零开销抽象**:默认删除器下 `sizeof(unique_ptr<T>) == sizeof(T*)`;
  自定义**有状态**删除器会扩大对象(空函数对象走 EBO 抵消)。
- 单对象 `unique_ptr<T>` 与数组 `unique_ptr<T[]>` 是两个特化,不要
  混用;要动态数组时优先 `std::vector`。
- **按值传参 = 转移所有权**(调用方必须 `std::move`);工厂函数按值返回
  (配合 RVO/移动,零拷贝)。
- 与 `shared_ptr`(共享所有权、原子计数)/`weak_ptr`(非拥有观察)构成
  三档所有权模型;默认选 `unique_ptr`,确有多方持有需求才升级。

### 7.6 代价与适用边界

Pimpl 不是免费的:

- **每次访问多一次指针间接**:`impl_->feed(...)` 先解一次指针才能摸到
  状态——缓存上多一次潜在 miss,优化器也更难看穿。
- **每实例一次堆分配**(本例靠懒分配把"空解码器"成本精确归零)。
- 适用判断:公共头被**广泛 include**、接口窄而实现厚、追求编译隔离或
  ABI 稳定的组件——`RequestDecoder` 全中(每连接一个对象、调用频率
  等于网络 read 频率,间接成本无关紧要)。
- 不适用:热内循环里的紧凑数据(例如第 6 节按值存放的查找表——它就是
  要让编译器看穿一切)。

一句话:**Pimpl 买的是"改实现不惊动使用方",付的是一次间接与一次分配**;
对解析器这种"接口三个函数、实现一整个状态机"的组件,是最划算的交易。

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

### 8.1 `OwnedFd`:最小心智负担的移动专属 RAII

事件循环给每个 socket 描述符配了一个 15 行的移动专属包装
(`http_server.cpp` 的 `OwnedFd`),是上述纪律的最小完整样本:

```cpp
class OwnedFd {
  public:
    OwnedFd() noexcept = default;
    explicit OwnedFd(int fd) noexcept : fd_{fd} {}
    ~OwnedFd() { reset(); }                       // 析构即 close
    OwnedFd(OwnedFd &&other) noexcept
        : fd_{std::exchange(other.fd_, -1)} {}    // 移动 = 掏空源
    OwnedFd &operator=(OwnedFd &&other) noexcept {
        if (this != &other) { reset(); fd_ = std::exchange(other.fd_, -1); }
        return *this;
    }
    ...
};
```

- **`std::exchange` 是"移动并置空"的惯用一字函数**:`exchange(other.fd_,
  -1)` 返回旧值同时把源写成 -1——比"先保存、再赋值"两行不易错,且让
  "无效值 = -1"的约定只出现一次;
- **自移动防护**:`if (this != &other)` 先 `reset()` 再接管,自赋值变
  no-op 而不是把 fd 关掉再使用;
- **拷贝删除**、`get()/valid()/release()/reset()` 对齐智能指针词汇,
  唯一所有权一目了然。标准没有 `unique_fd`(C++23 也只有提案),这类
  包装是每个 C++ 网络代码的必经之路——cpp 标准的"描述符由拥有型类型
  包装、错误路径泄漏即缺陷"规则落在它身上。

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

## 12. 无分配数字格式化:`std::to_chars`

`write_response` 要把状态码、`Content-Length`(十进制)和 chunk 大小行
(十六进制)写进字节串,用的是 `<charconv>` 的 `std::to_chars`:

```cpp
// service/src/http_response.cpp(摘录)
void append_number(std::string &out, std::size_t value, int base) {
    char buffer[24];
    const auto result = std::to_chars(buffer, buffer + sizeof(buffer), value, base);
    out.append(buffer, static_cast<std::size_t>(result.ptr - buffer));
}
```

为什么是它而不是别的:

- **无分配、无 locale、无虚发**:输出写入调用方给的缓冲区,不碰堆,不查
  本地化设置(对照 `std::to_string` 会构造临时 string,`printf` 家族要过
  varargs 与 locale)。线格式是纯 ASCII,`to_chars` 的"性能优先、绝不失败
  于格式化本身"的契约正对口;
- **进制即参数**:base 16 直接得到**小写**十六进制(chunk 大小行的惯例),
  base 10 是十进制——同一函数服务 `Content-Length` 与 chunk 两种数字;
- **缓冲区上界要自己算**:64 位 `size_t` 十进制最长 **20** 位、十六进制
  16 位——**十进制更长**!24 字节缓冲同时覆盖两者;`to_chars` 返回
  `to_chars_result{ptr, ec}`,`ptr - buffer` 就是写入长度;
- `value == 0` 时写入 `"0"`,无需特判(对照手写循环容易漏掉零)。

`write_response` 还配了一个 `digit_count` 循环:序列化前**精确算出整条
响应的线长度**(状态行 + 字段 + 成帧字段 + 体 + chunk 开销),`reserve`
一次后纯追加——单次堆分配完成整条响应的组装。

## 13. 两个字面量/初始化陷阱(测试代码的真实修复)

- **十六进制转义的 maximal munch**:C/C++ 的 `\x` 转义会贪婪吞掉**尽可能
  多**的十六进制字符。`"a\x7Fb"` 不是 `a, 0x7F, b`——`\x7Fb` 是一个
  越界的转义,直接编译错误或得到错误值。写法是拆开两个相邻字面量:
  `"a\x7F" "b"`(编译期拼接,转义在第一个字面量内终止)。响应套件的
  DEL 夹具就是这么写的(`http_response_tests.cpp`);同理,`std::string_view("he\r\nllo")`
  是 **7** 字节不是 6——把 CR/LF 夹具数错长度是本次对着绿灯实现修掉的
  测试缺陷。
- **聚合部分初始化触发 `-Wmissing-field-initializers`**:
  `Response{200, BodyFraming::chunked, "hello"}` 合法(余下成员取默认
  值),但本仓库把警告当缺陷(`-Wall -Wextra` 零警告纪律)。解法不是
  到处补 `{}`,而是给测试写两个**显式命名全部成员**的构造辅助:
  `plain(status, framing, body, close)` 与 `with_fields(...)`,套件里
  再无部分初始化,可读性反而更好。

## 14. 成员函数尾置 `const`:三个容易混淆的位置

`service/src/http_head_fields.hpp` 里的一对声明:

```cpp
[[nodiscard]] HeadLineResult append_line(std::string_view line, std::size_t begin);      // 写状态
[[nodiscard]] ParsedRequest finish(std::string_view head, std::size_t consumed) const;   // 只读
```

`finish` 末尾的 `const` 是**常量成员函数**限定,与另外两个"const"位置
是三回事:

```cpp
const ParsedRequest f() const;   // 前置 const:返回值是 const(按值返回时通常不这么写)
ParsedRequest f() const;         // 尾置 const:函数不修改 *this(finish 用的)
ParsedRequest f() final;         // final:禁止虚函数重写(与 const 无关)
```

尾置 `const` 产生两个编译器强制的效果:

1. **函数体内不许写成员变量**:隐式 `this` 从 `HeadFields*` 变成
   `const HeadFields*`,对成员赋值直接编译错误(除非成员声明了
   `mutable`)。`append_line`(积累 `request_line_`/`fields_` 解析
   状态)因此不带 const,`finish`(只读已有偏移、构造新的
   `ParsedRequest`)带——限定符把"写阶段/读阶段"的分工写进了类型,
   编译器替你监督;
2. **`const` 对象上也能调用**:拿到 `const HeadFields&` 时只能调用带
   限定的成员函数,漏写 const 的"只读"函数在 const 对象上直接编译
   不过。

配套细节:尾置 const 是**函数签名的一部分**——`f()` 与 `f() const`
可以构成重载(典型如迭代器的 `begin()`/`begin() const`);基类虚函数
带 const 时,重写版本必须同样带 const。`HeadFields` 没有虚函数,重写
本就无从谈起;与"不可重写"相关的关键字是 `override`(标记重写)与
`final`(禁止重写)。

## 延伸阅读

- cppreference:`std::variant`、`std::string_view` 生命周期 hazards、
  `std::span`、ranges 库、constexpr。
- ADR 证据:`docs/adr/ADR-0003-local-runtime-path.md`(Request-Head
  Optimization Evidence 节);
  `service/docs/adr/ADR-0004-owned-request-storage.md`。
