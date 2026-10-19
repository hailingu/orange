# HTTP/1.1 报文语法 — 解析器遵循的 RFC 规则

对应索引:[README.md](README.md) › 文件索引。orange 的解析器是**按 RFC
逐节实现**的,`service/src/http_messages.hpp` 的 `ParseError` 注释里每条
错误都标注了 RFC 节号。本文按"一个请求从哪字节到哪字节"的顺序整理这些
规则。

## 0. 权威文本分层

| RFC | 管什么 | orange 用到的节 |
| --- | --- | --- |
| RFC 9112 | HTTP/1.1 **报文在线格式**(怎么从字节切出消息) | 2.2(消息与行结尾)、3(请求行)、4(状态行)、5(字段行)、5.2(obs-fold)、6.1(TE 与 CL)、7.1.1(chunked) |
| RFC 9110 | HTTP **语义**(字段是什么意思、字符集) | 5.5(field-content/obs-text)、5.6.2(token/tchar)、5.2(重复字段策略)、7.6.1(Connection)、15(状态码与 reason phrase) |
| RFC 3986 | URI 语法 | 2.1–2.3(percent-encoding、保留/未保留字符) |
| RFC 6585 | 扩展状态码 | 5(431 Request Header Fields Too Large) |

## 1. 行结尾:CRLF,且拒绝裸 CR/LF

RFC 9112 2.2:HTTP/1.1 的行以 CRLF(`\r\n`)结束。orange 的处理:

- 行定位用 `find('\n')` 找到后**回看一个字节**确认前面是 `\r`;单独的
  `\r`(裸 CR)出现在行中间是语法错误 → `ParseError::malformed`(400)。
  这不是迂腐——历史代理拆分过不含 CR 的 LF 行,现代解析器必须把
  " smuggling"输入挡在文法层。
- 消息开头的**空行**(CRLF)按 RFC 9112 2.2 "SHOULD ignore" 被跳过;
  但只有空行耗尽整个 8 KiB 头预算时视为 malformed。

## 2. 请求行:`method SP request-target SP HTTP-version CRLF`

RFC 9112 3。三个 token 都不能为空,由 `parse_request` 校验:

- **method**:必须是 tchar 序列(RFC 9110 5.6.2 —— 字母数字加
  `!#$%&'*+-.^_`|~`),大小写敏感(`HTTP` 名亦然)。
- **request-target**:URI 字符集,且**百分号编码必须良构**(RFC 3986
  2.1:`%` 后必须跟两个 HEXDIG)。注意语义(这是不是合法路由)不是
  解析器的事——解析器只管在线语法。
- **HTTP-version**:精确形如 `HTTP/DIGIT.DIGIT`(如 `HTTP/1.1`)。

**超长分类**(RFC 9112 3):请求行放不进 8 KiB 头预算时,不是笼统报错,
而是按"谁更长" blamed:
- method 更长 → `method_too_long`,映射 **501**(RFC 9112 3 SHOULD);
- target 不短于 method → `target_too_long`,映射 **414**(MUST)。

这是"错误也是协议行为"的例子:状态码的选择本身被 RFC 规定。

## 3. 字段行:`field-name: OWS field-value OWS`

RFC 9112 5 + RFC 9110 5.5。逐条规则与实现:

| 规则 | RFC | orange 的实现 |
| --- | --- | --- |
| 字段名是 token | 9110 5.6.2 | 名字按 tchar 表校验;于是"冒号前不许有空白"(9112 5.1)自动成立——空白不在 token 集里 |
| 值两侧可选空白(OWS = SP/HTAB)被裁剪 | 9110 5.5 | 解析时 trim,结果不含前后 OWS |
| 值可含 obs-text(0x80–0xFF) | 9110 5.5 | 允许——这也是 signed char 陷阱的真实来源(见 [`cpp-language.md`](cpp-language.md) 第 10 节) |
| 旧行折叠 obs-fold(值以 SP/HTAB 开头续行)被**拒绝** | 9112 5.2 | → `malformed`(400)。RFC 9110 5.2 要求服务器拒绝或展开,orange 选拒绝 |
| 重复字段名**保留**为多条目 | 9110 5.2 | `Request::headers()` 保留顺序、大小写与重复;合并(`Cookie` 拼接之类)是**字段级策略**,不属于线格式解析器 |
| 字段名大小写保留,比较由调用方做 | 9110 | 原样保存 |

头部块以**一个空行**结束;头部块共享"请求行剩下的"预算,放不下时 →
`headers_too_long`,映射 **431**(RFC 6585 5)。

## 4. 消息体:Content-Length 与 chunked

RFC 9112 6。体的存在与长度由**成帧字段**(framing)决定,orange 在
`http_body_framing.cpp` 里"resolve once":CL/TE 只解析判定一次,
冲突/歧义 → `malformed`(这是 smuggling 防线的另一半)。

- **Content-Length**:十进制字节数,读满 N 字节即体结束。
- **Transfer-Encoding: chunked**(RFC 9112 7.1.1):体被切成块,每块
  `1*HEXDIG [ chunk-ext ] CRLF chunk-data CRLF`,以 `0` 大小行(+ 可选
  trailer)+ 空行收尾。解析要点:
  - 尺寸行是十六进制,`ChunkLine::feed` 逐字节(或逐跨度)状态机解码;
  - **chunk-ext**(扩展名=值)语法校验但**不保留**——引号串可跨分片,
    状态机照常推进(service ADR-0002 的测试覆盖 7,000 字节的扩展与
    trailer 值);
  - trailer 字段读完即丢弃。
- **唯一不支持的合法情形**:chunked 之前还有别的 transfer coding →
  `unsupported_transfer_coding`,映射 **501**。
- **上限**:`request_body_cap` = 1 MiB(解码后内容)→ 超限 `body_too_long`
  (**413**);chunk 尺寸行/扩展/trailer 的**在线元数据**另有独立预算
  `chunk_metadata_cap` = 8 KiB → 超限 `chunk_metadata_too_long`(**413**)。
  两个预算独立的原因:1 MiB 体可以用极少的元数据编码,也可以用海量小
  chunk 把线格式撑爆——解码内容和线开销是两种不同的资源。

## 5. 错误 → 状态码映射总表

`ParseError` 是事件循环决定响应的依据(`http_messages.hpp`):

| ParseError | HTTP | RFC 依据 |
| --- | --- | --- |
| `malformed` | 400 | RFC 9112 2.2(token/CR/文法/成帧歧义) |
| `method_too_long` | 501 | RFC 9112 3(SHOULD) |
| `target_too_long` | 414 | RFC 9112 3(MUST) |
| `headers_too_long` | 431 | RFC 6585 5 |
| `body_too_long` | 413 | ADR-0003 的 1 MiB 预算 |
| `chunk_metadata_too_long` | 413 | ADR-0003 的独立元数据预算(CC-8) |
| `unsupported_transfer_coding` | 501 | 只支持 chunked |
| `resource_exhausted` | 500 | 分配失败 |
| `incomplete` | —(继续读) | 不是错误:字节还没到齐 |

尺寸类拒绝(431/413)在 ADR-0003 的 Scope 里被进一步规定:响应带
`Connection: close`,**发完即关连接**,客户端重连——因为被拒绝的消息
边界不可信,不能假设"下一个请求从哪开始"。见
[`network-programming.md`](network-programming.md) 第 7 节。

## 6. 响应序列化:状态行、成帧与字段防线(`write_response`)

解析侧回答"怎么读",序列化侧回答"怎么写"——`service/src/http_response.*`
的 `orange::http::write_response()`(ADR-0003 T-1,红 `87548eb` / 绿
`743b5ed`)把一个 `Response` 值一次序列化为所有权线字节。规则同样是按
RFC 逐节来的:

- **状态行**(RFC 9112 4):`HTTP-version SP status-code SP [ reason-phrase ]
  CRLF`。注意**第二个 SP 是语法必选**,reason-phrase 才可空——未知码 599
  序列化为 `HTTP/1.1 599 \r\n`(空 reason 前仍留空格),不是删掉空格。
  版本恒为 `HTTP/1.1`:对非 1.x 请求版本答 505 是派发层(T-2)的策略。
- **状态码范围**(RFC 9110 15.1):"All valid status codes are within the
  range of 100 to 599, inclusive."——99 与 600 都是编码器拒绝的输入
  (`invalid_status`),不是"随便三位数"。
- **reason phrase 表**:只收录本切片会应答的注册码。413 在 RFC 9110 已
  更名为 **Content Too Large**(旧名 Payload Too Large);431 仍来自
  RFC 6585 5。未注册码用空 reason,合法且明确。
- **禁正文状态**(RFC 9110 15.2 / 15.3.5 / 15.4.5):"A 1xx/204/304
  response is terminated by the end of the header section; it cannot
  contain content or trailers."——这三类状态带体或带成帧字段直接是编码
  错误(`body_forbidden_status`),不是"体会被忽略"。
- **成帧二选一**:`Content-Length: N`(十进制)或
  `Transfer-Encoding: chunked`(RFC 9112 6.1/7.1 禁止二者共存)。chunked
  编码:小写十六进制大小行 + 数据 + CRLF,末块 `0\r\n\r\n`;**chunk 数据
  二进制安全**——数据区可以原样携带 CR/LF(测试夹具 `7\r\nhe\r\nllo\r\n0
  \r\n\r\n`),因为长度前缀已经定界,不再依赖行结尾;空体的 chunked
  响应就是单个 last-chunk。
- **编码器独占成帧字段**:`content-length`/`transfer-encoding`/
  `connection` 三个名字(大小写不敏感)由编码器持有,调用方字段带上即
  拒绝(`conflicting_field`)。这是请求侧 smuggling 防线的**响应侧对偶**:
  两条成帧意见永远不可能同时上线。
- **响应字段校验防 splitting**:字段名必须 token(RFC 9110 5.6.2)、值只
  能是 field-content(无 CTL,obs-text 放行)且**不得带首尾 OWS**——首尾
  空白上线后会变成 `Name:  value`,对端解析出的值与本意不同;CR/LF/NUL/
  DEL 出现在值里直接拒绝,响应拆分(response splitting)在线格式层被挡死。
- **`Connection: close`(RFC 9110 7.6.1)**:作为编码器字段跟在成帧字段
  之后、空行之前。431/413 拒绝响应的形状即
  `HTTP/1.1 431 Request Header Fields Too Large\r\nConnection: close\r\n\r\n`
  ——发完即关连接(见 [`network-programming.md`](network-programming.md)
  第 7 节)。
- **确定性顺序**:状态行 → 调用方字段(保序、保留重复)→ 成帧字段 →
  可选 `Connection: close` → 空行 → 成帧的体。同一输入永远同一字节。

字段名/值的校验直接复用解析侧的 `http_syntax.hpp` 分类器
(`detail::is_token`/`detail::is_field_octet`)——**解析与序列化共享同一
张字节文法表**,双向保证同一套 RFC 字符集(见
[`parser-design.md`](parser-design.md) 第 2 节)。

## 7. keep-alive 与连接生命周期

HTTP/1.1 默认持久连接(keep-alive):一条连接上多个"请求-响应"顺序
往返。orange 的模型:有效请求在一条稳定连接上多轮循环;被 431/413 拒绝
后连接关闭、客户端重连。空闲连接由服务端的 5 秒超时关闭以回收描述符。

## 8. 已批准待实现(T-2/T-3,ADR-0003)

以下协议行为还没有实现代码,引用的是 ADR-0003 的 Scope 与验收检查
(响应**成帧**本身已在 T-1 落地,见第 6 节;这里缺的是路由与监听):

- 路由与诊断端点:`GET /api/health` 返回 200 与精确体
  `{"status":"ok"}`;`POST /api/echo` 用第 6 节的 chunked 编码回显
  请求体;
- 静态文件服务:小 MIME 表、单区间 Range 请求(206)与不可满足区间(416)、
  405(方法不支持)/404(未知路径);
- TLS:`https://127.0.0.1:8443` 由锁版 OpenSSL 终止,首启生成自签名证书;
  明文监听 `http://127.0.0.1:8080`。

## 延伸阅读

- RFC 9112、RFC 9110、RFC 3986、RFC 6585 原文(工具:ietf.org 的 RFC
  页面内搜索节号最快)。
- "HTTP Smuggling" 系列文章——理解为什么 CL/TE 歧义、裸 CR/LF、obs-fold
  都是安全边界而不只是格式洁癖。
- 代码入口:`parse_request`(`http_messages.cpp`)、
  `parse_message`/`RequestDecoder`(`http_request_decoder.cpp`)、
  `body_framing`/`ChunkLine`(`http_body_framing.cpp`/`http_chunk_line.cpp`)、
  `write_response`(`http_response.cpp`)。
