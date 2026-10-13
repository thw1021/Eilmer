# 任务完成日志（逐项追加）

规则：每完成 `backlog.md` 中的一个任务 ID，就在本文件**追加**一节，写清"范围、做了什么、
验证证据与实测数字、产出文件、新遗留"，然后回 `backlog.md` 把该 ID 标 `[x]` 并链到本节。
新发现的待办只写进 `backlog.md`（不在这里积累清单）。已完成任务不再重述过程，
`phase-*.md` / `wave-*.md` 提供阶段汇总视角，本文件提供逐任务视角。

条目模板：

```
## T-xxx — 标题  (YYYY-MM-DD)
- 范围：输入文件与行数
- 做了什么：关键决定与转写要点
- 验证：命令/测试/对照的实测数字（不允许只写"通过"）
- 产出：新增或修改的文件
- 新遗留：新增 backlog ID，或"无"
```

---

## T-000 — Phase 0：基线冻结、构建骨架、D 兼容垫片、验收 harness  (2026-10-13)

- **范围**：gdtk `e587b235` 全树勘察 + 移植基础设施；无 D 模块转写。
- **做了什么**：
  - 冻结参考：确认 `gdtk/build/` 是 HEAD 的完整自洽安装（`e4shared` 横幅
    `Revision-id: e587b235`、`Compiler-name: ldc2`），**未重编、gdtk 零写入**。
  - 摸清运行时契约：内嵌 Lua 被改造为读 `DGD_LUA_PATH`/`DGD_LUA_CPATH`
    （`extern/lua-5.4.3/src/loadlib.c:275`），标准 `LUA_PATH` 无效。
  - 修正主干变体集合：makefile 默认打开 `multi_species_gas/multi_T_gas/MHD/turbulence/
    enable_fp_exceptions/flavour_debug`，与参考横幅 `Capabilities:` 一致 → 主干**含**这三个
    改变结构体布局的变体。
  - CMake + 变体 INTERFACE target + `EILMER_ABI_HASH` 链接符号（主干哈希 `0357ff576ba1`）。
  - D 兼容垫片 `substrate/include/eil/`：`number/real/Complex`、`Array`+`Slice`（代际计数）、
    `AA`、`format`、`math`、`conv`、`error`（Throwable 拆 Exception/Error 两支）。
  - 建立"实测代替推断"方法：`tools/fmt_probe.d` → `tools/fmt_corpus.txt`（786 行观测），
    `tests/test_format_corpus.cpp` 多集合比对复现全部行。
  - 建 harness：`extract_cmds.py`/`run_case.sh`/`normalize_log.py`/`compare.py`/`gate.sh`，
    并抓 7 个 Tier-A 基线。
- **验证**：
  - 语料比对 **786/786 行零差异**。
  - harness 自检：把 **D 二进制当候选**跑 `gate.sh` → 7 案例全 PASS（证明 FAIL 可归因于移植）。
  - `sod-test.rb` 对参考安装 4 tests / 14 assertions / 0 failures；sod 75 步、`final-t= 0.00060314`。
  - 参考侧非确定性实测：同一 `e4shared` 两次运行 `config/cone20.fluidBlockArrays` md5
    `d8ce/7413/d8ce` 变化而 `cone20.list` 稳定 → 判为 D 侧 AA 顺序，改用多重集合比对并显式标注。
  - 编译开关核实：`build/CMakeFiles/eil_nm.dir/flags.make` 含 `-ffp-contract=off`。
- **产出**：`CMakeLists.txt`、`cmake/*`、`substrate/*`、`tools/{fmt_probe.d,fmt_corpus.txt}`、
  `harness/*`、`baselines/e587b235/{7 案例}+SHA256SUMS`、`reports/phase-0.md`。提交 `c4ccbec`、`00602f0`。
- **新遗留**：T-800（`e4-nk-shared` 缺失使 2 个 Tier-A 判例不可用，已写 `INVALID` 防护）、
  T-801（METIS 缺失阻塞 6 个 metis 判例）、T-902（std.json 无库可用）、T-903（substrate 缺口）。

## T-001 — Wave 1：nm 与 util 叶子层转写  (2026-10-13)

- **范围**：22 个 D 模块 / **5,055 行** — `nm/{number,nm_exception,package,limiters,bracketing,
  brent,ridder,secant,linesearch,newton,bbla,stmatrix,smla,rsla,schedule,newtoncotes,gaussquad,
  rungekutta}`、`util/{lua,lua_service,msg_service,time_utils}`。
- **做了什么**：逐模块镜像为 `src/<dir>/<module>.hpp/.cpp`；`T[]` 形参→`Slice`、
  `new T[n]`→`Array`；D 类引用语义→`shared_ptr`（标 `adapted-handles`）；extern-C 模板
  `toStringObj(T, string mt)`→函数体 + `EIL_LUA_TOSTRING_OBJ` 宏壳（标 `adapted-callback`）；
  所有 D `unittest` 逐个转 CTest，demo 转对照测试。
- **验证**（全部以 ldc2 现编的 D 程序为基准）：
  - 线代簇：**467 个量的 `%.17g` dump diff 为空**（含 GMRES/ILU/Crout/Jacobi/SGS 全链，
    17 位可唯一往返 ⇒ 逐位相同）；`demo_nm_bbla` stdout 41 行字节相同。
  - 求积/ODE：3 个 demo stdout 字节相同，**函数调用次数也相同**（315/55、231/21、1725/305、91
    ⇒ 自适应二分树形一致）；Runge-Kutta 所有步进输出逐位相同。
  - 求根簇：2 个 demo 字节相同；limiters 35/35 位相同；`approxEqualNumbers` 13/13 判定相同。
  - Lua 层：**226 行 transcript 与 D 探针字节相同**（24 条错误串、注册名、`luaL_error` 前缀文本、
    栈副作用、26 个 `timeStringToSeconds` 输入、`.init` 取值）；**ASan+UBSan+LSan 全清**。
  - 残余差：4 个解析真值 1–2 ULP（最大相对 2.99e-16）、`PI/4−estimate` 绝对差 3.06e-17
    （`%.6g` 打印后仍相同）→ 根因为 Phobos 与 glibc 超越函数本身差 ≤1 ULP（实测约 13% 的
    `exp` 采样点末位不同），非转写误差。
  - 构建：clean configure → 23/23 CTest 通过，`Debug(-O0)` 与 `Release(-O3)` 双模式均通过。
- **产出**：`src/nm/*`、`src/util/*`、`tests/*`（23 测试 + 6 demo）、`provenance/{fidelity.yml,
  manifest.yml}`（42 条含 `d_blob`）、`tools/{gen_manifest.py,check_staleness.sh}`（报 `no drift`）、
  `reports/wave-01.md`，tag `wave-01`。提交 `836d3b8`。
- **转写过程反向修正了垫片**（说明对照有效）：`Throwable` 两支分离（`.msg()` 上移到公共基类）、
  `ConversionException` 不再二次包裹消息、`to_double/to_long` 补全 Phobos 的 5 种失败文本、
  新增 `eil::real`+`EIL_PI`（实测 D 的 `real` 是 80 位）、`-ffp-contract=off` 取代 clang-only 开关。
  并修掉 2 个移植自身 bug：`SMatrix::set()` 覆盖已有条目、行尾追加分支返回 D 侧的越界引用。
- **新遗留**：T-010…T-016（Wave 2 范围修正：`geom/luawrap` 20 文件 9,876 行改归 Wave 7，
  `grid_utils/ugrid_partition.d` 调 `gpmetis` 故移出 Wave 2）、T-904（复数支未编译验证）、
  T-905（nm/util 剩余叶子）。
