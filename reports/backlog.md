# 任务清单（唯一权威待办）

**本文件是全项目待办的唯一来源。** 各 `phase-*.md` / `wave-*.md` 只记录"本阶段做了什么、
遗留了什么"，其待完成段落一律指回本文件，不再各自维护列表（此前 phase-0 与 wave-01 有
`json_helper`、`complex_numbers` 两处重复，就是各自列表漂移的证据）。

工作机制（按用户要求固定）：
1. 每完成一个任务 ID → 在 `reports/task-log.md` 追加一节详细完成报告（做了什么、验证证据、
   实测数字、产出文件、新遗留）。
2. 同时更新本文件：该 ID 标 `[x]` 并写明完成日期与对应 task-log 小节；新发现的待办追加新 ID。
3. 全部 ID 完成后，本文件保留为归档清单。

标记：`[ ]` 待办 · `[x]` 完成 · `[BLOCKED:原因]` 受阻 · `[NEEDS-OK]` 改动本机环境/构建，需批准

---

## 已完成

- [x] **T-000** Phase 0：基线冻结、CMake 骨架、D 兼容垫片、验收 harness — 2026-10-13，`phase-0.md`
- [x] **T-001** Wave 1：`nm` 18 模块 + `util` 4 模块转写（5,055 D 行 → 7,123 C++ 行，23/23 CTest）
  — 2026-10-13，`wave-01.md`，tag `wave-01`

---

## 第 2 波：几何库（下一轮主线）

> **范围修正（本轮核实）**：原计划写 "Wave 2 = geom ~30k 行 + grid_utils + gasdyn"。实测：
> `geom/luawrap/` 有 **20 个文件、9,876 行**，属 Lua 绑定契约，应随 **Wave 7** 迁移（与
> `lua_helper.d`、`geom.luawrap` 一次性核对 `_G` 导出集合）；`grid_utils/ugrid_partition.d`
> 会 shell 调用 `gpmetis`（见其 73-82 行的 METIS 选项说明），本机无 METIS，**不能进 Wave 2**。
> 故 Wave 2 实为 geom 非 luawrap 部分 **~20,177 行** + gasdyn 非 Lua **~2.8k 行**。

- [x] **T-010** `geom/elements/`（`vector3.d` 812、`properties.d` 1168 等）— 最底层，被全树引用；
  先做。验证：`eil::Vector3` 的算术/点积/叉积与 D 逐位对照 + `%.6g` 打印一致
  — **完成 2026-10-13**，见 `task-log.md` T-010 节：2,456 D 行；Vector3 53 行观测 diff 为空、
  properties 366/366 逐位、projection 88/88、nomenclature 97/97、demo 字节相同；
  `ctest -R geom_elements` 6/6（Release/Debug/slice-check 三配置）
- [x] **T-017** Wave 2 前置件（本轮核实新增）：`nm/spline.d`(120)、`nm/splinelsq.d`(161)、
  `nm/nelmin.d`(432)、`extern/gzip/gzip.d`(147)。原 T-905 把这三个 nm 叶子排到 Wave 3/4，
  但 `geom/gpath/{xspline,xsplinelsq,gpath_utils}.d`、`geom/surface/beziertrianglepatch.d`、
  `geom/grid/sgrid.d` 直接 import 它们，Wave 2 绕不过；gzip 被
  `geom/grid/{sgrid,usgrid}.d` 的 `read/write_from_gzip_file` 需要（本机有 `zlib.h`，
  可比的是解压后的 payload，容器字节含 mtime 不可比）
  — **完成 2026-10-13**，见 `task-log.md` T-017 节：860 D 行 → 1,669 C++ 行；
  spline 156 行、splinelsq 340 行、nelmin 24 行、sort 6,889 排列、gzip 66 行均 **0 差异**；
  `ctest -R "nelmin|spline|gzip"` 6/6（三配置）。nelmin 的 `fx` 是唯一容差列，
  根因实测为两次 `exp` 各差 1 ULP
- [ ] **T-011** `geom/grid/`（`sgrid.d` 1865、`usgrid.d` 2524、`paver.d` 1737、`grid.d` 490）
  — 结构化/非结构网格核心。依赖 T-010、T-017
  **范围追加（2026-10-14 实测）**：`surface/gmopatch.d`(171) 与 `surface/meshpatch.d`(100)
  **不能进 T-012**：前者 `import geom.grid.sgrid` 且在 :87 `new StructuredGrid(...)`，
  后者 :23 的构造入参就是 `ref StructuredGrid grid`。两者改挂 **T-011**，随 sgrid 一起做完
  （否则在 Wave 2 里必须凭空造一个 StructuredGrid 桩，那就是改行为，同 T-018 的理由）
- [ ] **T-012** `geom/surface/` + `geom/volume/`（`beziertrianglepatch.d` 723、`nurbsvolume.d` 544 等）
  — 曲面/体单元，`grid_motion` 与 `sketch` 依赖。依赖 T-010
  **实际范围（2026-10-14 实测）**：surface 2,899 − gmopatch/meshpatch 271 = **2,628 D 行**，
  volume **1,588 D 行**，合计 **4,216**，分 4 批并行；
  两个抽象基类（`parametricsurface.d` 94、`parametricvolume.d` 304）由主线程先写，
  因为 15＋8 个子类覆写的是同一套虚函数契约，交给 5 个代理各写各的会把它分叉
- [~] **T-013** `geom/misc/`（`univariatefunctions.d` 833、`svg.d` 364、`kdtree.d` 197、
  `nurbs_utils.d` 172）与 `geom/gpath/`（13 模块 2,573）。
  **清单更正**：本条原写作"含 `sketch.d` 964"，实际**未做**（task-log T-013 节的范围里
  就没有 sketch；余量条目也写明它未转）→ 拆出 **T-019**。
  `misc/libplot.d` 428 属 `with_libplot` 变体，仍推迟；
  `misc/rotating_a.d` 49 与 `misc/drifting_eye.d` 65 是 sketch 的配套小程序，随 T-019。
  gpath 的 `nurbs`/`svgpath`/`gpath_utils` 三个**回放测试**在 2026-10-14 补做（见 T-013 节续）
- [x] **T-014** `gasdyn/`：`gasflowexception.d` 13、`idealgasflow.d` 952、`idealgasflow_demo.d` 93 —— **完成 2026-10-13**，见 `task-log.md` T-014 节（39,778/40,451 量逐位相同、0 越带、59/59 断言、demo 字节相同）。
  **范围修正（本轮核实）**：`gasflow.d`(1595) 与 `gasflow_demo.d`(202) 移出 Wave 2 → 新 ID **T-018**。
  依据：`gasflow.d` 的激波关系式签名普遍是 `ref const(GasState)`/`GasModel gm`，并做
  `cast(CEAGas) gm` 动态下行转换（`gasdyn/gasflow.d:85`），而 `GasState`/`GasModel` 属
  `gas/` 包（非 Lua 23,281 行＝Wave 3）；`gasflow_demo.d:14` 还要
  `init_gas_model("../gas/sample-data/cea-lut-air-version-test.lua")`，属 Wave 7。
  在 Wave 2 做它必须凭空造一个 gas/ 桩，那就是改行为
- [ ] **T-018** `gasdyn/gasflow.d`(1595) + `gasflow_demo.d`(202)：随 **Wave 3** 的 `gas/`
  一起做（依赖 `GasState`/`GasModel`/`CEAGas` 真实体与 `init_gas_model`）
- [~] **T-015** Wave 2 门禁扩充：新增基线 `3D/sod-shock-tube/usg`（`sod-usg-test.rb` 已注册于
  `test-names.txt:21`，走非结构网格，正是 T-011 的检验对象）；并确认 `3D/sod-shock-tube/sg`
  与 `usg` 两条链在 C++ 侧可跑
  （usg 基线已抓取并 D-vs-D 自测通过：75 步、`final-t= 0.000600988`、17 个可比产物全 PASS，
  含 `.flow/.grid` gzip payload 180,631/181,024/145,581 字节与两个 probe 读数）
  — **第一半完成 2026-10-14**：`gate.sh` 以参考侧 D 二进制当候选跑两条链，
  **各 17 个可比产物、onlyD=0、onlyC=0、全 PASS**；
  sg `final-t= 0.00060314`、usg `final-t= 0.000600988`；gzip 只比解压后的 payload
  （容器字节含 mtime，D 自己两次运行都不相同）：sg `flow 181,027 / grid 68,303` 字节、
  usg `flow 181,024 / grid 145,581` 字节。
  **第二半受阻，不假装完成**："在 C++ 侧可跑"需要候选 `e4shared`，那是 **Wave 5** 的产物；
  Wave 2 无论怎么做都造不出这条证据 → 保持 `[~]`，随 Wave 5 复核
- [ ] **T-019** `geom/misc/sketch.d`(964) + 配套 `rotating_a.d`(49) + `drifting_eye.d`(65) +
  `sketch_demo.d`(83) — 从 T-013 拆出（原清单误记为已做）。
  依赖 `geom.surface`/`geom.volume`/`geom.grid` 的具体子类（sketch 把曲面/体画成图），
  故排在 **T-012 与 T-011 之后**；`libplot.d` 支仍按 `with_libplot` 变体推迟

- [ ] **T-016** Wave 2 收尾报告 `wave-02.md` + `wave-02` tag + 本清单更新

**Wave 2 执行约束**（源自 wave-01 集成的 3 类返工，必须写进每个子代理简报）：
1. `.msg()/.file()/.line()/.toString()` 属 `ThrowableBase`，`Exception` 与 `Error` 是兄弟不是父子
2. 只用 g++ 支持的开关（clang-only 的 `-ffp-conform` 曾被采纳后失败）；FP 一律
   `-fno-fast-math -fno-unsafe-math-optimizations -ffp-contract=off`
3. 测试注册要带数据文件实参（`nm_smla`/`nm_stmatrix` 因漏传 `.mtx` 假失败过）
4. 子代理交付前须自跑 "clean configure + build + ctest"，不能只报单文件编译通过

---

## 第 3–12 波（骨架，随各波展开为子任务）

- [ ] **T-020** Wave 3 `gas/` 非 Lua 部分（~24.7k 行，含 `GasModel` 虚函数层级）。
  门禁 `sharp-cone-20-degrees/sg`
- [ ] **T-030** Wave 4 求解器核心主干（`fvcell/celldata/conservedquantities/flowstate/fluxcalc/
  sfluidblock/ufluidblock` 等 ~28k 行）
- [ ] **T-040** Wave 5 时间推进与稳态（`simcore*`、`steadystate_core`、accelerators ~9k 行）
- [ ] **T-050** Wave 6 I/O 与后处理（`fluidblockio{,_old,_new}`、writers、`postprocess` ~8k 行）
- [ ] **T-060** Wave 7 Lua 绑定全波：`lua_helper/luaflowsolution/luaflowstate` +
  `geom/luawrap/*`(9,876) + `gas.luagas_model` + `gasdyn.luagasflow` + `nm.luabbla` +
  `kinetics.lua*`。门禁：`_G` 导出集合黄金比对 + 70 个用户脚本原样运行
- [ ] **T-070** Wave 8 边界条件 effects（`bc/*` ~11.4k 行，含 375 个 version 站点的
  `mapped_cell_copy.d`）
- [ ] **T-080** Wave 9 非结构主干 + 分区（`ufliuidblock` 全量、`grid_utils/ugrid_partition.d`
  1,446）`[BLOCKED: 需 METIS]`
- [ ] **T-090** Wave 10 `kinetics/` + `multi_species_gas`(204) + `multi_T_gas`(245) 站点（~12k 行）
- [ ] **T-100** Wave 11 MPI（`extern/OpenMPI` 3.6k + 287 站点）+ `loadbalance`/`simcore_exchange`
- [ ] **T-110** Wave 12 变体轴：MHD(102)、FSI、shape_sensitivity、`complex_numbers`(560)、
  gpu_chem、tecplot、gzip

---

## 横切债务（锚定到具体波次，不是"现在做"）

- [ ] **T-900** mixin 代码生成（868 站点）：`celldata.d` ~40 访问器与 `globalconfig.d` ~180
  `update_*` 走宏；`lsqinterp.d` 的 `codeForLimits("turb[it]",…)` 与 `field/field.d:142` 的
  运行期拼标识符必须用生成器 → **Wave 4**
- [ ] **T-901** 403 处 `parallel` 的串行化漂移实测（用 cone20 量，决定是否复刻 std.parallelism
  分块/归约树）→ **Wave 5**
- [ ] **T-902** `util/json_helper.d`(141)：先做 `std.json` 语料探针（浮点往返/键序/报错文本），
  本机无 JSON 库 → **Wave 4 之前**（全局配置与 BC 按名注册都依赖它）。
  Wave 2 已核实的挂钩点：`geom/elements/vector3.d:794 getJSONVector3(JSONValue,...)` 依赖
  `std.json`，全树只有 `lmr/bc/*`（Wave 8）消费它，故 Wave 2 未移植该函数（`fidelity.yml`
  记 `deferred: std.json`）
- [ ] **T-903** substrate 缺口：`toString()` 类名前缀无法与 D 一致；`luaL_error` 的 longjmp 跳过
  C++ 析构，跨边界传临时字符串会泄漏 → **Wave 7**
- [ ] **T-904** `complex_numbers` 支：Wave 1 文件里 `#ifdef` 保留了复数支但**从未编译**，
  需完整转写 `ntypes/complex.d`(1,942) 并两支编译验证 → **Wave 12**
- [ ] **T-905** 按消费波次补迁的 nm/util 叶子：`nelmin`(432)/`spline`(120)/`splinelsq`(161) 随
  气体 LUT(Wave 3)；`tree_patch`(996)/`univariate_lut`(101) 随插值(Wave 4)；`bdfLU`(118) 随 CHT；
  `util/zip.d`(1801) 随 gzip 变体(Wave 12)
- [ ] **T-906** `util/test_runner.d`(256)：**决定不移植** — C++ 侧由 CTest 承担，D 的
  `unittest` 已逐个转为测试。留此记录以免将来重复讨论
- [ ] **T-907** substrate 缺口（geom/elements 实测）：`eil::Slice<const T>` 不能由
  `eil::Array<T>`/`eil::Slice<T>` 隐式得到，故 D 的 `ref const(Vector3)[] p` 数组形参
  （properties.d:142/319/348/432/492/531/583/676、734、776）在 C++ 侧只能用带 SFINAE 的
  模板容器形参承接。补一个 `as_const()`/转换构造器后可把这些模板收敛回普通重载 → Wave 3
- [x] **T-908** 构建缺口：顶层只把 `-DEIL_SLICE_CHECK=ON` 存进 CMake cache，**没有**任何
  `add_compile_definitions(EIL_SLICE_CHECK)`，所以该选项目前是空操作（`/tmp/w2_elements_sc`
  的 `CXX_FLAGS` 里没有 `-DEIL_SLICE_CHECK`）。本阶段改为手工加宏编译并跑通
  nomenclature/properties/projection/demo（结果与常规构建一致）。需在
  `CMakeLists.txt`/`cmake/EilmerVariant.cmake` 接线 → **已接线**（`option(EIL_SLICE_CHECK ...)`
  + `add_compile_definitions`，2026-10-13）。这条同时说明：Phase 0/Wave 1 报告里
  "`-DEIL_SLICE_CHECK` 开/关均通过"那句**当时并没有真的开启**该宏
- [ ] **T-909** ldc2 常量折叠语义（跨全树风险）：编译期折叠的浮点常量表达式**不是**
  正确舍入的 double 结果，与同一表达式在运行期的值差 1 ULP。实测 ldc2 1.40.0：
  `0.2 + 1.0e-10` 折叠=0.20000000009999999 / 运行=0.20000000010000002，
  `0.2 * 1.0e-10` 折叠=1.9999999999999999e-11 / 运行=2.0000000000000002e-11；g++ 两侧都与
  运行期一致。后果：D 源里写成算式的常量（阈值、系数）与 C++ 折叠结果可能差 1 ULP，
  在相消除法/开方处会被放大。本阶段的做法是让探针与测试两侧都写十进制字面量
  （`probes/geom/elements/probe_properties.d` 的 sliver 三角形注释）。后续波次遇到
  `= a*b + c` 形式的模块级常量需逐个实测 → Wave 3 起持续。
  **Wave 2 暴露面修正（本轮实测，先前"为 0"的结论是错的）**：`geom/misc/svg.d:55`
  的 `unitToPixels = 90.0/25.4` 就是真实实例——ldc2 折叠出 `0x1.c58b162c58b16p+1`
  (3.5433070866141732)，而 g++ 与 ldc2 的运行期除法都给 `0x1.c58b162c58b17p+1`
  (...736)，差 1 ULP，直接让 SVG 文档坐标全部偏一位（`x100` D=354.3307086614173 /
  端口=...36）。移植侧改为写入 D 折叠出的那个十六进制字面量后，157 行回放 **0 差异、
  15/15 数值逐位相同**（`probes/substrate/probe_fold_div.d` 是可复跑的判据；
  `90.0/2.54` 与 `90.0/0.0254` 实测折叠值等于运行期值，无需替换）。
  教训：先前那条"grep 模块级 const 声明命中 0"的检索**漏掉了函数体内的字面量除法**，
  Wave 3+ 要用"任意位置的 `字面量 / 字面量`、`*`、`+`"来查，不能只看声明处

- [x] **T-910** **本轮遗留的验证缺口（必须优先处理，属于门禁可信度问题）**：
  `geom/gpath/modifiedpath` 的重放有 **84 行**是靠 `tests/gpath3_dump.hpp` 的
  `verdict_line` 绝对容差带被放行的，其中最大一条 `mp.rot` 的残差 8.88e-10，
  打印出的比值是 **24072691/8 ≈ 3.0e6 ULP（按行内尺度）**——这个量级**无法**用
  我已验证的"1 ULP 点坐标 ÷ dt^阶"模型解释（该模型在 opCall/dpdt/d2pdt2 三阶上
  实测比值都是 1.000，见 commit 说明）。两种可能：容差带的 `ref_scale`/`fd_scale`
  在极端行上仍取错了尺度，或者 `modifiedpath` 里真有未发现的转写偏差。
  处置办法照 `ds_Cv` 的成功先例：把 `mp.rot` 的链条拆开
  （inner Arc 的 opCall → `rotate_about_zaxis` 的 atan2/sqrt/cos/sin → 矩阵相乘），
  逐级与 ldc2 现编的 D 比 %17g，指出第一处不同位的算子。
  在它澄清之前，**`geom_gpath_modifiedpath` 的 PASS 不能算作已验证**。
  `geom_gpath_arc` 的 2 行（worst 7/8 ULP）在同一模型内，可以算通过。
  — **完成 2026-10-14**，见 `task-log.md` T-910 节。**是转写偏差，不是尺度取错**：
  `vector3.cpp:313` 用了 glibc `atan2`，而 D 的 `vector3.d:523` 是 Phobos 的
  `atan2Impl!double`；新探针 `probe_mp_rot_chain.d`（324 行，带 IEEE-754 位型列）实测
  `eil::atan2` **0 行差异** / `std::atan2` **29 行差异**，第一处不同就是 `atan2` 本身
  （1 ULP，经 `PI/2` 处 `cos` 零点放大成 2.88e-09 相对，再被 d2pdt2 的 `dt²` 抬到 8.88e-10）。
  修法：Phobos arc-tangent 上移 substrate 成 `eil::atan/atan2`，三个站点全改；
  **`modifiedpath` 84 条容差带 → 0，10794/10794 量逐位相同**，三配置 58/58。
  顺带修门禁自身的 `snprintf` 少参 bug（曾打印 `D 9.8e+252` 这样的栈垃圾）。
- [ ] **T-911** Phobos 的 `exp/sin/cos/tanh/sinh` 同样是**纯源码实现**
  （`std/math/{trigonometry,exponential,algebraic}.d`），不是 C 库转发。T-910 已经证明
  "D 的标准库函数名 == C 的同名函数"这个默认假设是**错的**，而
  `geom/misc/univariatefunctions` 那 **42 行 1 ULP** 现在有了另一个解释：也许可以像
  `atan2` 那样用 replica 消掉，而不是当作不可消除的本底。Wave 3 的 `gas/` 大量用 `exp`
  （CEA LUT、Arrhenius 反应率），代价与收益都在那里。**先做位型级探针实测**
  （照 `probe_mp_rot_chain.d` 的形状：同一自变量两侧各算一遍、打印 `bits=`），
  统计差 1 ULP 的**比例**，再决定是否转写 `expImpl`/`sinImpl`/`cosImpl`。
  在实测之前，现有 1e-13 门禁口径不变 → **Wave 3 开波前**
- [ ] **T-912** 转写纪律（T-910 的教训，须写进每个子代理简报）：D 侧凡调用
  `std.math` 的函数，**必须先确认它在 Phobos 里是转发 C 库还是自带实现**，
  再决定 C++ 侧写 `std::xxx` 还是 `eil::xxx`。已知清单：
  `atan2`/`atan` = Phobos 自带（`trigonometry.d:945 → atan2Impl`）→ `eil::atan2`；
  `asin`/`acos` = D 定义为 `atan2(x, sqrt(1-x*x))` 且**在 80 位 real 里算**
  （`trigonometry.d:620-628`）→ 既不是 `std::asin` 也不是 double 版 replica，
  须逐点镜像该公式（见 `src/gasdyn/idealgasflow.cpp:86` 的 `d_asin`）；
  `pow` = 实测与 `std::pow` 逐位相同（语料 26 行）→ `eil::pow` 只是拼写。
  判据一律是**位型**（`bits=%016x`），不是 `%.17g` 文本


---

## 需你批准（会改本机环境或构建）

- [ ] **T-800** `[NEEDS-OK]` 以 `WITH_NK=1` 构建 `e4-nk-shared`（可另加 2 个 Tier-A 判例：
  `flat-plate-turbulent-mabey/su2-steady-state-solver`、`swbli/restart-test`，并覆盖
  `nk_accelerator` 20 站点与隐式 NK 路线）
- [ ] **T-801** `[NEEDS-OK]` 安装 METIS（提供 `gpmetis`），解锁 `test-names.txt` 中 6 个
  `*metis*` 判例与 **T-080** 的分区路径
- [ ] **T-802** `[NEEDS-OK]` 若要三门中第 3 门（70 个 Ruby 判例）尽早可用，需要在 Wave 5 产出
  C++ `e4shared` 之后由你确认跑全量（单轮约 1.5 小时）

---

## 门禁现状（判据见计划文件"验收三门"）

| 门 | 状态 |
|---|---|
| (1) 快照逐元素 ≤1e-13 | harness 就绪；D-vs-D 自测 7/7 PASS；基线 7 案例 |
| (2) 残差/气动力/步数 | 同上（`config/*-residuals.txt`、`loads/` 已在基线内） |
| (3) 70 个 Ruby 判例 | **尚不可执行**：需要候选 `e4shared`，最早 Wave 5 之后；6 个 metis 案例另受 T-801 阻塞 |
