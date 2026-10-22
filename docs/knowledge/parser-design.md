# 增量解析器设计 — 在任意分片的字节流上安全解析

对应索引:[README.md](README.md) › 文件索引。TCP 不保证"一次 read 拿到一个
完整请求":一个请求可能拆在任意两个字节之间到达,也可能和下一个请求粘在
同一次 read 里。orange 的解析器(`service/src/http_request_decoder.*` 及其
内部协作类)就是为这个现实设计的。本文讲清五个支柱:状态机、错误分类、
预算、记账、分片不变性。

## 1. 为什么不是"先攒齐再 parse"

最朴素的做法:循环 read 到缓冲区,直到 `parse_request` 不再返回
`incomplete`。问题:

- **恶意/慢客户端**:永不发 CRLF 的连接会让"找行结尾"在无界缓冲上反复
  重扫;
- **内存**:不设上限就得无限攒字节。

所以解析器从第一天就带着两个问题设计:**每一步消耗多少字节**、
**最多允许消耗多少字节**。

## 2. 显式状态机

### `ChunkLine`(http_chunk_line.hpp):chunk 尺寸行 + 扩展

内部是 12 相的枚举状态机(`Phase`):`size → before_separator →
before_extension → name → ... → quoted_value → escape → ... → lf`。
要点:

- **单字节入口** `feed(char)` 与**跨度入口** `feed(string_view)` 语义等价:
  跨度版只是单字节版的循环展开,停在第一个非 `pending` 结果并报告
  `consumed`(service ADR-0002 的"span advancement"——逐字节喂 8 KiB 的
  元数据会慢,按跨度推进一次移动到边界);
- 引号字符串值可**跨 fragment**:状态机记住自己在 `escape`/`quoted_value`
  等相,下一片字节续上,不保留任何扩展文本;
- 十六进制累加器带溢出与配额检查(`add_digit`),`oversized_` 标志让
  "超出剩余内容配额"稳定映射到 `too_large` 而不是回绕。

### `HeadFields`(http_head_fields.hpp):分片到达的头部块

`append_line(line, begin)` 逐行(已剥 CRLF)追加,内部只存
**偏移描述符**(`RequestLineOffsets`/`HeaderFieldOffsets`,`uint16_t`),
从不存借用视图——这是 service ADR-0003/0004 的联合设计:头部分片到达时
每行在**绝对偏移**处登记,`finish()` 时一次性由校验过的偏移构造所有权
`Request`,不再重复文法扫描。

偏移的**宽度**也是设计的一部分:选 `uint16_t` 而不是 `int`,因为所有
偏移都被 8 KiB 头预算(`header_block_cap`)卡死——最大 8191,`uint16_t`
上限 65535,还有 8 倍余量;`uint8_t`(上限 255)装不下,所以这是**能
用的最窄类型**。精确宽度让结构布局与每连接内存账目确定(`int` 只保证
≥16 位、通常 32 位,宽度随平台漂移);无符号也才是偏移/长度的天然
语义。内存账很直观:每个字段描述符 8 字节,而线上最小的字段行只要
4 字节(`a:\r\n`:1 字符名 + 冒号 + 空值 + CRLF),开销比 2:1,换成
`int` 就是 4:1。窄宽度成立的唯一前提,是解析器只允许**验证过、在
预算内**的偏移进入描述符(私有构造 + friend 的可信边界,第 7 节)——
"有界"这个上游不变量被编码进了类型本身。

### 层次划分

```
parse_request(raw)        一次到位的头部解析(整块输入)
parse_message(raw)        完整消息(头+体),管线字节不计入 consumed
RequestDecoder::feed()    连接级增量解码:持有跨 feed 的状态
  └─ ChunkLine / HeadFields / body_framing(detail 层,无独立所有权)
write_response(response)  响应序列化:与解析共享同一张字节文法表
```

公共契约只有最上面三层和 `write_response`;`detail::` 是实现细节。
序列化侧对字段名/值的校验**不另写一套规则**,直接调用
`detail::is_token`/`detail::is_field_octet`(`http_syntax.hpp` 的 256 项
`ByteClassTable`)——解析与序列化是同一文法的两个方向,共享分类器保证
双向永远对同一字符集达成一致(规则详见
[`http-protocol.md`](http-protocol.md) 第 6 节)。

## 3. 错误分类学与"粘性错误"

`ParseError` 把失败分成**可继续的**与**终局的**(完整表见
[`http-protocol.md`](http-protocol.md) 第 5 节):

- `incomplete`:唯一"再等等"的值;
- 其余都是终局错误:连接上的这条消息已死。

**粘性(sticky)错误**:`RequestDecoder::feed` 一旦返回终局错误,后续
`feed` 保持该错误、消耗 0 字节,直到 `reset()`。为什么:错误后输入流的
"边界"不可信,继续解析等于在垃圾上猜。测试用 8192/8193 边界与
"sticky 错误需 reset 才能服务独立流"钉死这一行为(CC-8)。

`reset()` 的语义是**新连接/新流的独立请求**,不是"跳过坏消息继续"。

## 4. 三重字节预算

| 预算 | 值 | 超限结果 | 防的是什么 |
| --- | --- | --- | --- |
| `header_block_cap` | 8192 | 431 | 无界头部/永不结束的行 |
| `request_body_cap` | 1048576 | 413 | 无界解码内容 |
| `chunk_metadata_cap` | 8192 | 413(关连接) | 用海量小 chunk 撑爆线格式 |

设计原则:

- **每次行搜索都以剩余预算为界**——永不结束的行最多扫 8 KiB,不会在
  无界缓冲上重扫(这同时是安全属性与性能属性,见
  [`performance-and-simd.md`](performance-and-simd.md) 第 7 节);
- 头预算**跨请求行与头部块共享**(`header_block_cap -
  ParsedRequest::consumed` 是头部块的可用额度);
- 解码内容与在线元数据**独立计量**:`consumed` 数的是线字节(含 chunk
  框架与丢弃的 trailer),body 预算数的是解码后字节——1 MiB 内容合法,
  8 KiB 元数据超限照样拒绝。

## 5. `consumed` 记账规则

增量解析最容易写错的就是"这次 feed 吃了几个字节"。orange 的规则
(`DecodeResult`/`ParsedMessage` 注释):

1. 成功:`consumed` 是**本片**被取走的字节数;管线字节(下一请求的开头)
   留给调用方,不吞;
2. 成功后继续 feed:**消耗 0**,直到 reset——防止调用方误把下一请求喂进
   已完成的解码器;
3. 错误:`consumed` 是**丢弃记账**(discard accounting),**不是**可复用的
   消息边界——正确动作是丢弃连接剩余输入并关闭,而不是 `raw.remove_prefix
   (consumed)` 接着解析。

规则 3 是安全关键:把错误后的 consumed 当边界继续解析,就是在攻击者
控制的偏移上重新对齐协议——request smuggling 的经典温床。

## 6. 分片不变性(fragmentation invariance)

同一线字节,无论怎么切片喂进来,结果必须一致。测试矩阵(ADR-0003
AC-1 / CC-8):

- 1 MiB 内容用 **512 / 1024 字节 HTTP chunk** 两种编码;
- 每种编码用 **17 字节 / 8192 字节 / 整块** 三种 feed 粒度喂入;
- 期望:512 字节 chunk 编码在元数据预算下失败、1024 成功,且**在每种
  feed 粒度下结论相同**。

这套"编码 × feed 粒度"的叉乘是增量解析器的标配回归:任何"喂法影响
结果"的缺陷都藏不住。

## 7. 所有权与构造边界(与 C++ 层的接缝)

- 解码层(`RequestDecoder`)**拥有**每连接的解析状态,不可拷贝;
- **对象粒度 = 连接,不是请求**:请求边界本身是解码的**输出**——
  在"还不知道这条请求在哪结束"之前,无法做到"每请求一个新对象",
  而一个请求可能分任意多片到达,状态必须跨 `feed()` 存活,所以对象
  锚定在连接上(accept 时构造,关闭时销毁)。请求之间的隔离靠
  `reset()`(语义上等价于换一个新对象,但零堆分配,且管线化的多条
  请求在 `drive_messages` 循环里连续"解出 → 处理 → reset");连接
  之间的隔离靠每连接独立对象("Instances share no mutable state")。
  `unique_ptr` 持有的原因是类型刻意不可移动、而连接对象要随 vector
  扩容/压缩搬迁(见 [`cpp-language.md`](cpp-language.md)
  第 7.3.1 节);
- 结果 `ParsedMessage` 拥有头部字符与解码体(`std::string body`),调用方
  的输入缓冲可以立刻回收;
- `Request` 的构造只开放给校验过的路径(friend 边界,见
  [`cpp-language.md`](cpp-language.md) 第 9 节)。

## 8. 已批准待实现的部分

错误码到**真实 socket 行为**的连接(431/413 → `Connection: close` 响应 →
EOF → 客户端重连)属于 ADR-0003 T-1 的网络部分与 CC-5,当前解析器层只
负责"给出编码错误与丢弃记账"。两连接进度(AC-7)同理是事件循环的职责,
见 [`network-programming.md`](network-programming.md)。

## 延伸阅读

- `service/src/http_request_decoder.hpp` 全文——接口注释本身就是设计文档;
- service ADR-0002(跨度推进)、ADR-0003(分片头偏移)、ADR-0004(所有权
  存储)的 Decision 节;
- 经典对照:llhttp、nghttp2 的分帧状态机;RFC 9112 2.2 对"消息边界"
  的论述。
