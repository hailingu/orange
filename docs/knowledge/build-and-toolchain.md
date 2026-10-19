# 构建、工具链与编译 — CMake、编译器与 CI 的工程实践

对应索引:[README.md](README.md) › 文件索引。素材来自
`service/CMakeLists.txt`、`.github/workflows/http-performance.yml` 与
`docs/development/cpp-development-standard.md` 的实际约定。

## 1. CMake 基础设施

### 1.1 out-of-source 构建

```sh
cmake -S service -B service/build      # -S 源目录 -B 构建目录
cmake --build service/build            # 编译(可用 --parallel N)
ctest --test-dir service/build --output-on-failure
```

构建产物全部落在独立目录,**永不提交**(`.gitignore` 的 `build/`;
仓库规范原文:"`service/build/` is disposable output and never
committed")。CI 里用 `$runner.temp` 下的独立目录,跑完 `cmake -E rm -rf`
删除——这就是 AGENTS.md"一次性验证执行"(Disposable Verification
Execution)的落地:不留可复用产物,就不需要 OCR。

### 1.2 目标设计:小而显式

`service/CMakeLists.txt` 只有两个层次:

```cmake
add_library(orange_http src/http_messages.cpp ...)   # 唯一生产库
target_include_directories(orange_http PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(http_message_tests tests/http_message_tests.cpp)
target_link_libraries(http_message_tests PRIVATE orange_http)
add_test(NAME http_message_tests COMMAND http_message_tests)
```

- **一个能力一个测试套件、一个可执行文件**:CTest 按进程退出码判生死,
  失败定位到套件粒度(`http_chunk_line_tests`、`http_head_fields_tests`、
  `http_request_storage_tests` 各自独立成靶);
- `PUBLIC` include:链接 `orange_http` 的目标自动获得其头文件路径
  (传播属性;`PRIVATE` 则只影响自身);
- `enable_testing()` + `add_test`:CTest 的注册动作,通常在顶层
  CMakeLists 调用一次。

### 1.3 条件目标与防呆

基准构建是**可选**的,且带防呆:

```cmake
option(ORANGE_BUILD_BENCHMARKS "..." OFF)
if(ORANGE_BUILD_BENCHMARKS)
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Release")
        message(FATAL_ERROR "Parser benchmarks require CMAKE_BUILD_TYPE=Release")
    endif()
```

为什么:Debug 模式的基准数字毫无意义(无优化 + 断言开销),与其发布
误导性数字,不如配置期直接失败。

### 1.4 把编译上下文烙进产物

```cmake
target_compile_definitions(http_parser_bench PRIVATE
    ORANGE_BENCH_COMPILER="${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}"
    ORANGE_BENCH_FLAGS="${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_RELEASE}"
    ORANGE_BENCH_ARCH="${CMAKE_SYSTEM_PROCESSOR}")
```

基准报告自我描述:哪个编译器、什么标志、什么架构——脱离上下文的纳秒数
不可比较(ADR-0008 的可复现性要求)。

### 1.5 生成器表达式与脚本模式

```cmake
add_test(NAME http_parser_bench_cli
    COMMAND ${CMAKE_COMMAND} -DBENCHMARK=$<TARGET_FILE:http_parser_bench> ...
            -P ${CMAKE_CURRENT_SOURCE_DIR}/bench/verify.cmake)
```

- `$<TARGET_FILE:target>`:生成器表达式,在生成期解析成可执行文件的真实
  路径(跨配置构建目录下位置不固定,手拼路径不可靠);
- `cmake -P script.cmake`:**脚本模式**,把 CMake 语言当跨平台脚本用
  (这里驱动基准并做阈值判定);`cmake -E rm -rf` 则是跨平台命令行
  (删除目录不依赖 `rm` 存在)。

## 2. 编译器与警告策略

- `add_compile_options(-Wall -Wextra)`:全体目标开基础警告集;仓库纪律
  是"**维护代码里出现新警告即缺陷**",CI 里的警告不是噪音;
- 默认 `CMAKE_BUILD_TYPE=Debug`(本地验证带调试信息);基准与性能验证
  用 `Release` + `-O2 -DNDEBUG`:
  - `-O2`:优化级别 2(内联、循环优化、向量化尝试);
  - `-DNDEBUG`:关闭 `assert`——断言在基准热路径上是数量级的失真;
- 工具链是 Apple clang(macOS arm64);同一套配置在 CI(macos-15)上
  用 `-DCMAKE_CXX_COMPILER=clang++` 显式固定。

## 3. Sanitizer:把隐性疾病变成当场崩溃

ADR-0009 的 CI 用两套构建:Release 验正确性+性能,**Debug + sanitizers**
验内存/未定义行为:

```sh
cmake -S service -B build/sanitizers -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' \
  -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined'
```

- **AddressSanitizer(ASan)**:编译期插桩,捕捉越界、use-after-free、
  栈上悬垂(orange 的 `parsed_of` 悬垂就是它抓的)、double-free;
- **UndefinedBehaviorSanitizer(UBSan)**:捕捉有符号溢出、错误对齐、
  除零等未定义行为;
- `-fno-omit-frame-pointer`:保留栈帧指针,报告里的调用栈可符号化;
- 平台差异要懂:**Apple 平台的 ASan 不含 LeakSanitizer**,所以 CI 显式
  `ASAN_OPTIONS=detect_leaks=0`(承认限制而不是让它误报);UBSan 用
  `halt_on_error=1` 让首个 UB 直接失败;
- sanitizer 与正常构建**分目录共存**,不互相污染缓存。

## 4. GitHub Actions 工作流(`http-performance.yml`)

ADR-0009 的产物,值得逐段学习:

```yaml
'on':
  pull_request: { branches: [dev], paths: ['service/**', '.github/workflows/http-performance.yml'] }
  push: { paths: ['service/**', '...'] }
  workflow_dispatch:
```

- **paths 过滤**:只有 `service/**` 变化才跑,纯文档提交不烧 CI 分钟;
- `workflow_dispatch`:允许手动触发。

```yaml
concurrency:
  group: http-parser-${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true
```

- 同一分支连续 push 时**取消进行中的旧运行**——省资源且避免旧结果
  覆盖新结果的竞态。

```yaml
- uses: actions/checkout@3d3c42e5aac5ba805825da76410c181273ba90b1 # v7.0.1
  with: { persist-credentials: false }
```

- **按完整 SHA 固定 action**(注释标注版本号):标签可被改写,SHA 不可
  ——供应链基本功;
- `persist-credentials: false`:此 job 无需推代码,不留凭据在运行器上
  (最小权限)。

```yaml
permissions: { contents: read }
timeout-minutes: 10
```

- 权限整体压到只读;超时兜底防悬挂 job。

其他两处好习惯:

- 基准 JSON 用 `tee` 落盘后写进 `$GITHUB_STEP_SUMMARY`(job 摘要页直接
  可读),并声明"magnitude guards, not server SLOs"——**宽量级护栏**,
  把共享 runner 的噪声挡在告警之外;
- 最后一步 `if: always()` 执行 `cmake -E rm -rf "$ORANGE_VERIFY_ROOT"`:
  无论成败都清理一次性编译输出,对齐 AGENTS.md 的一次性验证边界。

## 5. clang-format 与本地验证三件套

仓库惯例的"验证三件套"(各 ADR 证据反复出现):

```sh
cmake -S service -B service/build && cmake --build service/build   # 无警告
ctest --test-dir service/build --output-on-failure                  # 全绿
clang-format --dry-run -Werror <files>                              # 格式零漂移
```

根 `.clang-format` 提交在库,格式争议从代码评审中消失。

## 6. vcpkg(已批准待实现)

ADR-0003 计划用 `vcpkg.json` **manifest 模式**锁 OpenSSL:依赖声明进
版本控制、`vcpkg_installed/` 进 gitignore、版本精确锁定的哲学与前述
"action 按 SHA 固定"一脉相承。

## 延伸阅读

- CMake 官方教程(target/link/生成器表达式三节);
- GCC/Clang 手册 `-fsanitize` 章节;ASan/UBSan 官方文档;
- GitHub Actions 文档:workflow syntax、安全加固(固定 SHA、最小
  permissions)。
