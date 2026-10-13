# Phase 0 报告 — 基线冻结、构建骨架、D 兼容垫片、验收 harness

gdtk 参考版本：`e587b2351e075f309f4877fe180b2cb5a55c7e5b`（ldc2 1.40.0 / clang-15 后端，FLAVOUR=debug）
产物目录：`/home/tang/packages/Eilmer`（git 分支 `main`，本阶段提交 `c4ccbec` 起）

## 1. 已完成

### 1.1 D 参考基线冻结
- 确认 `gdtk/build/`（bin/lib/data/share）是 HEAD 的完整自洽安装：`build/bin/e4shared` 横幅
  `Revision-id: e587b235`、`Compiler-name: ldc2`、`Build-flavour: debug`、
  `Capabilities: multi-species-gas multi-temperature-gas MHD turbulence.`
  **未重新编译**，因此 `/home/tang/packages/gdtk` 全程零写入（只读参考）。
- 修正本项目先前笔记：树内 `src/eilmer/e4shared` 报 `Revision-id: e587b235`，即当前 HEAD，
  并非陈旧基线（旧笔记记录的是 `1f9910ad`，已被之后的构建覆盖）。
- 求解器可运行性验证：`3D/sod-shock-tube/sg` 完整跑通（75 步，`final-t= 0.00060314`），
  其自带判例 `sod-test.rb` 对参考安装 **4 tests / 14 assertions / 0 failures**。

### 1.2 运行时环境契约（勘察得出，文档未载）
- 内嵌 Lua 5.4.3 被打过补丁：模块搜索路径读 **`DGD_LUA_PATH` / `DGD_LUA_CPATH`**
  （`gdtk/extern/lua-5.4.3/src/loadlib.c:275` 将 `LUA_PATH_VAR` 重定义为 `DGD_LUA_PATH`），
  标准 `LUA_PATH` 被忽略。不设这组变量时 `prep.lua` 里 `require 'lua_helper'` 直接失败。
- 完整可用环境：`DGD=<install>`、`PATH=<install>/bin`、`DGD_LUA_PATH=<install>/lib/?.lua`、
  `DGD_LUA_CPATH=<install>/lib/?.so`、`LD_LIBRARY_PATH=<install>/lib`。
  注意 `export A=1 B="$A/x"` 同行展开会取到 A 的旧值，必须分行导出。

### 1.3 主干变体集合（纠正计划初稿）
makefile 默认值 `MULTI_SPECIES_GAS?=1 MULTI_T_GAS?=1 MHD?=1 TURBULENCE?=1 WITH_FPE?=1
FLAVOUR?=debug`，与参考二进制横幅一致。因此**主干不是最小串行理想气体构建**，
而是包含 multi_species_gas / multi_T_gas / MHD / turbulence / enable_fp_exceptions /
flavour_debug；这三者会改变 `FlowState`/`CellData` 的结构体字段集合。
`complex_numbers / mpi_parallel / FSI / shape_sensitivity / nk_accelerator / gpu_chem /
with_tecplot_binary` 默认关闭，留待后续波次。

### 1.4 CMake 骨架（`CMakeLists.txt`、`cmake/EilmerVariant.cmake`、`cmake/eil_abi.hpp.in`）
- 变体宏沿用 D 拼写（`-Dturbulence` 而非 `-DEIL_TURBULENCE`），使 `version(turbulence)`
  与 `#ifdef turbulence` 一一对应；定义集中在一个 INTERFACE target 上，杜绝逐 TU 漂移。
- ABI 防串味：活动变体集合的 SHA1 短哈希生成符号 `eilmer_abi_<hash>`，由
  `substrate/src/abi.cpp` 定义、`tests/test_abi.cpp` 引用；混用标志集会**链接失败**而非算错。
  当前主干哈希 `0357ff576ba1`。横幅能力串由同一集合生成，与 D 逐字一致（有测试断言）。
- 浮点编译开关：`-fno-fast-math -fno-unsafe-math-optimizations -ffp-contract=off`。
  `-ffp-contract=off` 是必需的：g++ 默认 `fast`，会在 -O2 把 `a*b+c` 合成 FMA，
  而参考的 ldc2 debug 构建不发 FMA，末位就会不一致。
  （计划初稿写的 `-ffp-conform=off` 是 clang 专有，g++ 11.4 直接拒绝，已替换。）

### 1.5 D 兼容垫片 `substrate/include/eil/`（本轮核心交付）
Phobos 在本机只有预编译产物（无 `import/std/math.d`），因此**所有规则由实测决定**：
`tools/fmt_probe.d` 用 ldc2 编译、输出固化为 `tools/fmt_corpus.txt`（**786 行**观测），
`tests/test_format_corpus.cpp` 以多集合方式复现全部 786 行，**0 差异**。

实测推翻的"想当然"（每一条都会破坏逐字节日志比对）：

| 观测 | 若按直觉实现会怎样 |
|---|---|
| `to!string(double)` 是 `%.6g`，非最短往返：`3.141592653589793 → 3.14159` | 用 `std::to_chars` 会打印 17 位，日志全表不匹配 |
| `isClose` 相对项为 `diff <= rel*max(|x|,|y|)`，且两项都是 `<=` | 常见的 `diff/|y| <= rel` 读法与 9 条边界行矛盾，且 y=0 时除零 |
| D 的 `real` 在本工具链是 **80 位**（`sizeof=16, mant_dig=64`，`PI=0x1.921fb54442d1846ap+1`） | 常量算术在 Phobos 侧走扩展精度；垫片提供 `eil::real=long double`+`EIL_PI`，实测**赋值到 double 后与 D 逐位一致**（ct_/rt_ 行） |
| Phobos 的 `sqrt(-1)`、`log(-1)` 返回**正号 NaN**，glibc 返回负号；但 `0.0/0.0` 两边都是 `-nan` | 发散算例会把 `nan` 打成 `-nan`；`eil::sqrt/log/log10/asin/acos` 做规范化 |
| `%d`/`%x` 施于浮点参数是**异常**：`incompatible format character for floating point argument: %d`；多余参数报 `Orphan format arguments: args[0..1]` | printf 会静默按位解释参数，错格式符就悄悄过闸 |
| `%g`/`%d`/`%x` 施于 bool 打印 1/0，`%s` 打印 `true/false` | 与 C 的隐式整型提升不一致 |
| 关联数组读缺失键抛 `Range violation`；`%s` 对字符串数组加引号 `["alpha", "beta"]`，裸字符串不加引号 | 报错文本与数组文本双双漂移 |
| `to!double` 有 5 种失败文本（`Floating point conversion error`、`no digits seen`、`error converting input to floating point`、`Unexpected 'c' …`、`hex … exponent is required`）；`to!int` 严格，**不接受任何前后空格**（`" 42 "` 失败） | 选项解析的报错文本全部漂移 |
| `enforce(cond, "fmt", args...)` 的 `.msg` 是**未替换的原串** | 套用格式化会产生 D 不会出现的消息 |

组件：`number.hpp`（`number`/`real`/常量/`eil::re`）、`array.hpp`（`Array`/`Slice` 视图 vs 持有、
代际计数失效检测）、`aa.hpp`、`format.hpp`、`math.hpp`、`conv.hpp`、`error.hpp`
（`Throwable` 拆成 Exception/Error 两支——D 的 `catch (Exception)` **不**捕获 Error，
若让 ErrorBase 派生自 ExceptionBase，参考程序里的崩溃会在移植版变成被捕获继续跑）。

### 1.6 验收 harness（`harness/`）
- `extract_cmds.py`：从判例 `.rb` 里抽取 `cmd = "…"` 字面量，按序重放——
  70 个判例因此无需手工维护命令清单。
- `run_case.sh`：拷案例到 scratch、设定 1.2 的环境、`PATH` 前置候选 bin 目录（Lua 模块树
  固定取参考安装，故 Lua 契约是输入而非变量）、重放命令、归档**整棵产物树**
  （求解器写进 `config/ grid/ flow/ hist/ loads/` 子目录，只收顶层文件会漏掉重启文件）。
- `normalize_log.py`：占位化易变字段（Revision-id/date、Compiler-name、Build-date、
  Build-flavour、Heap memory、`WC=/WCtFT=/WCtMS=`、`wall-clock= sec`、`<GDTK_PATH>`、
  `prep-gas` 生成时间戳行）。
- `compare.py`：数值文件按 token 解析逐元素比（相对容差默认 1e-13，**不按文本比**），
  `.gz/.zip` 解压后比载荷，`.log/.lua` 归一化后逐字节比；集合化回退见 1.7。
- `gate.sh`：三门一次性执行并出 `gate.md`。
- **自检验证**：把 D 二进制当作候选跑 `gate.sh`，7 个 Tier-A 案例 **全部 PASS**
  ——即"D 对 D"等价，任何后续 FAIL 才能归因于移植本身。

### 1.7 本阶段发现的参考侧非确定性（重要）
1. `config/<job>.fluidBlockArrays` 在**同一 D 二进制两次运行间会变**（md5 d8ce…/7413…/d8ce…），
   而 `config/<job>.list` 稳定。原因是该文件由遍历关联数组写出，druntime 每进程重播种哈希。
   → 结论：移植侧用有序 `std::map` 无数值风险；但该文件**不能**要求序列相等，
   `compare.py` 因此在 token 序列不等时回退做**多重集合比较**，并显式标注
   `order-insensitive match`（宁可标注，绝不静默放宽）。
2. `.zip` 载荷相同但容器字节不同（偏移 10 的 mtime）→ 比载荷。
3. `__pycache__/*.pyc`、`tmp.*` 目录属运行环境残留，非求解器输出 → 排除。

### 1.8 Tier-A 基线（`baselines/e587b235/`，14 MB，7 案例）
`2D-cone20-sg`、`2D-cone20-sg-implicit`、`2D-cone20-new-flow-format`、`2D-piston-simple`、
`2D-piston-in-tube-1-block`、`2D-steepening-wave-ldfss2`、`3D-sod-shock-tube-sg`。
覆盖：显式/隐式、新流场格式、动网格（含激波拟合前置）、多波陡化（ldfss2 格式）、3D。
原先选的 `2D-flat-plate-turbulent-mabey/su2-steady-state-solver` 与 `2D/swbli/restart-test`
**被剔除**：两者调用 `e4-nk-shared`，参考安装里没有该二进制（rc=127）。
`run_case.sh` 现在遇到 rc=127 会写 `INVALID` 并退出 3——静默的假基线比没有基线更糟。

## 2. 待完成（后续阶段）

1. **Wave 2–12 主体迁移**（见计划文件波次表）。下一波是 `geom`（~30k 行）+
   `grid_utils` + `gasdyn` 非 Lua 部分。
2. **补齐 Tier-A**：需要 `WITH_NK=1` 构建 `e4-nk-shared`（覆盖 nk_accelerator 20 站点、
   隐式 NK 路线、swbli/湍流平板判例），以及安装 METIS 以启用 6 个 `*metis*` 判例。
   这两项都会改动本机环境/构建，**需你确认后再做**。
3. **`complex_numbers` 轴**：560 个 version 站点、`number → Complex<double>`、
   `ntypes/complex.d` 完整转写（当前只取 `:1926-1941` 的 12 行 struct）。Wave 1 文件里
   已按 `#ifdef complex_numbers` 保留两支，但复数支尚未编译验证——这是**已知债务**。
4. **`util/json_helper.d`**：需先做 std.json 的语料探针（浮点往返/键序/报错文本），
   本机无 JSON 库；它牵动 45 处 `std.json` 用法与 BC 按名注册。
5. **mixin 代码生成（868 站点）**：`celldata.d` 的 ~40 个访问器与 `globalconfig.d` 的
   ~180 个 `update_*` 走宏（第二参数已核实是合法 C++ 成员路径），`lsqinterp.d` 的
   `codeForLimits("turb[it]", …)` 必须用生成器——落地在 Wave 4。
6. **403 处 `parallel` 的串行化实测**：Wave 5 用 cone20 量漂移，决定是否复刻
   std.parallelism 的分块/归约树。
7. **substrate 已知缺口**：`ExceptionBase::toString()` 的类名部分无法与 D 一致
   （D 打印 `module.Class`，C++ 无运行期类型名对应物）；`luaL_error` 的 longjmp 会跳过
   C++ 析构，跨该边界传临时字符串的调用点会泄漏——已由 util 侧测试标注，Wave 7 处理。

## 3. 度量

| 项 | 值 |
|---|---|
| 语料观测行 | 786（D/C++ 全等） |
| CMake 测试 | 23 个，100% 通过 |
| Tier-A 案例 | 7，D-vs-D 自测 7 PASS |
| 主干 ABI 哈希 | `0357ff576ba1` |
| gdtk 写入 | 0 文件（只读参考） |
