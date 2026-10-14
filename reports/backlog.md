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
  gpath 的 `nurbs`/`svgpath`/`gpath_utils` 三个**回放测试已补齐 2026-10-14**：
  nurbs **195/195 行字节相同、655/655 量逐位相同、0 行走容差带**；
  svgpath **818/818 行、2802/2802 量**；gpath_utils **564/564 行、957/957 量**，
  其 demo **43 行字节相同（507 字节，两侧 md5 `e0fa70ef…59569`）**。
  gpath_utils 曾差 1 行，根因是 substrate 的 `to!double` 错误文本分支 → 见 **T-913**
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

  **Wave 3 暴露面已实测完毕（2026-10-14，不再留"逐波重测"的欠账）**：
  `probes/substrate/probe_fold_gas.d` → `baselines/…/substrate_fold_gas.txt`，
  对 `gas/` 里真实出现的字面量算式逐个比"折叠值 vs 运行值"。
  **两处结构会折错**：
  * `4.184/1.0e-2` 折叠 `418.39999999999998` / 运行 `418.40000000000003`（差 1 ULP），
    站点 **26 处**（`gas/diffusion/{two,three,multi}_temperature_trans_props.d` 等，
    cal/(cm.s.K)→J/(m.s.K) 换算因子）；
  * `0.45 - 2.54*0.5` 折叠 `-0.81999999999999995` / 运行 `-0.82000000000000006`，
    站点 **7 处**。
  而 `5.0/2.0`、`3.0/2.0`、`9.0/2.0`、`7.0/2.0`、`1.0/3.0`、`2.0/3.0`、
  `pow(2.0*3.14159265,1.5)`、`8.31451/0.0159994`、`0.5*28.0134e-3`、`1.0e5/(208.0*300.0)`
  **全部折叠正确**。⇒ Wave 3 只需把那**两个形状**写成 D 折叠出的十六进制字面量，
  不必给所有常量建保护；此结论可复跑（`ldc2 probes/substrate/probe_fold_gas.d`）。
  这条同时把本条开头"Wave 3 起持续"的欠账结掉。

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
- [ ] **T-914** substrate 缺口：`eil::Array::set_length(size_t)` 表达不了 D 的
  **有符号** `arr.length = L`。D 里 `L<0` 抛 `Error "Memory allocation failed"`；
  C++ 侧若先转 `size_t` 就会变成试图分配 2^64 个元素。本波代理在
  `src/geom/surface/bezierpatch.cpp:26-39` 用本地 `set_length_checked` 三个重载绕开，
  同一需求在 volume/grid 还会再出现 → 应在 `substrate/include/eil/array.hpp` 定形
  （加 `set_length_signed(long)`），然后删掉本地绕行 → **Wave 3**
- [ ] **T-915** D 侧真实崩溃，端口不加防护：`sweptpathpatch.d` 的
  `new SweptPathPatch(null, cB)` 实测 **SIGSEGV、exit 139、无可捕获对象**
  （`probes/geom/surface/probe_sweptpathpatch.d` 组 H，语料里没有该行）。
  同样 `slabvolume.d:32` 在 `super()` 之前调继承的 `face0123(0.0,0.0)` 也是 SIGSEGV。
  记录以免将来把它误当移植缺陷；若 Wave 7 的 `geom/luawrap` 允许 Lua 传 null 进来，
  那时"崩溃"就是用户可见行为，须按 D 原样保留（不做空指针检查）→ **Wave 7 复核**
- [x] **T-911** Phobos 超越函数与 glibc 的分歧——**已实测成分类清单，不再当本底假设**
  — **完成 2026-10-14**：`probes/substrate/probe_phobos_bits.d`（**673 行，每行带
  IEEE-754 位型**）+ `tests/test_substrate_phobos_bits.cpp` 普查。
  结果 **597/673 逐位相同（88.7%）**，其余分四类：
  * **已经逐位相同、不需要 replica 的**：`sin` 32/32、`cos` 32/32、`sqrt` 32/32、
    `log2` 32/32、`asin` 11/11、`acos` 11/11、`pow` 四种指数臂各 32/32。
    ⇒ "Phobos 这些都是纯源码实现所以必然不同"这个前提是**错的**。
  * **1 ULP 类（可用 replica 消除，代价/收益待定）**：`atan2` 12、`cbrt` 12、`sinh` 5、
    `tanh` 5、`exp` 4、`cosh` 4、`atan` 2、`log10` 2、`log` 1、`expm1` 6。
  * **结构类（>1 ULP，不是舍入差异）**：
    `sinh(-20)` D=`-242582602.56479013` vs glibc `-242582597.70489514`（相对 **2.0e-08**，
    且 **D 侧 `sinh` 破坏奇对称**：`sinh(20)` 与 `-sinh(-20)` 不等，而两者都不等于
    `(exp(x)-exp(-x))/2`＝glibc 值）；
    `sinh(710)` D=`inf` vs glibc `1.1169973830808557e+308`（**溢出阈值不同**）；
    `tan(1e10)` 相对 **1.7e-06**（辐角约化品质不同）；`cbrt` 4 行 2 ULP；`expm1` 1 行 2 ULP。
  * **域错误 NaN 符号** 5 行（`sqrt_neg`/`log_neg`/`asin_gt`/`acos_gt`/`pow_neg_half`），
    substrate 的 `canonical_nan` 已在管，不是新分歧。
  **对求解器的实际影响（逐个调用点查过）**：
  `sinh` 在主干只有 `gas/two_temperature_nitrogen.d:111`（`sinh(θ_N2/(2Tv))`，**Tv 很小时
  自变量可越过 710 → D 出 inf 而 C++ 出 1.1e308，这是真会走到的路径**）、
  `geom/misc/univariatefunctions.d:600,704`（`sinh(beta)/beta`，beta 正常量级）、
  `ntypes/complex.d:978-979`（`complex_numbers` 变体，Wave 12）。
  `tan` 的调用点自变量都是小角度（`nozzleexpansionpatch.d:114`、
  `properties.d:1031/1047/1054` 的 `radians(30)`/`radians(60)`），落在 1 ULP 类。
  `tanh` 有负自变量（`quodas/source/block.d:443` `tanh(-d/scale)`）但只 1 ULP。
  `cbrt`/`expm1` 主干调用点 **0**。
  ⇒ 决定：**不为 1 ULP 类建 replica**（改了也只是把 88.7% 提到 97.9%，
  而门禁是 1e-13，1 ULP 是 1e-16 量级）；
  但 **Wave 3 必须专门处理 `sinh` 的两处结构性分歧**（奇对称破坏 + 溢出阈值），
  因为 `two_temperature_nitrogen.d` 会走到。已开 **T-916** 钉住它。
  普查本身作为常驻测试留在树里：它**不要求差异为零**（差异是它测的东西），
  只在两侧不再描述同一批行时失败。
- [x] **T-918** substrate 缺陷（grid 代理报的第三条）：`eil_assert` 渲染的是
  `"Assertion failure: <字符串化的条件>"`，而 **D 的 `e.msg` 根本不是这个**。实测
  （ldc2 1.40.0，`catch (Error e)` 取 `e.msg`）：
  `assert(i==3, "index i is invalid")` → `msg == "index i is invalid"`（**没有**
  "Assertion failure" 前缀）；`assert(i==3)` → `msg == "Assertion failure"`（**不带**
  条件文本）。旧实现对两种情况都追加 `#cond`，是参考侧从不产生的文本；而
  `harness/normalize_log.py` 是按字节比异常文本的，geom 有 **297** 个 assert 站点、
  src/eilmer 有 **249** 个。
  **完成 2026-10-14**：`substrate/include/eil/error.hpp` 拆成
  `d_assert`（裸文本）与 `d_assert_msg`（原样消息）+ 两个宏，
  `grid.cpp` 的本地副本改为**转发**（两份同一规则必漂移，见 T-910 的教训）。
  D 的 `toString()` 里 `core.exception.AssertError@file(line):` 这个类名前缀仍无法一致，
  那是既有的 T-903，不在本条范围。
- [ ] **T-919** `geom_grid_grid` 行 178 `cbc.nan2d x=`：**仅 Release 红**，
  D `-nan` / 端口 `nan`，且 Debug 与 `-DEIL_SLICE_CHECK=ON` 两侧一致为 `-nan`。
  643 行里唯一一条，量级是 y/z 完全相同、只有 x 的 NaN 符号位不同。
  **已排除的假设（不要再走一遍）**：grid 代理怀疑 `vector3.cpp:99-102` 的
  `x += other.x; y += other.y` 被 GCC 打成 `addpd` 因而取错 lane 的 NaN payload。
  实测 `objdump -d CMakeFiles/eil_geom.dir/src/geom/elements/vector3.cpp.o | grep -c addpd`
  = **0**；给该文件加 `-fno-tree-slp-vectorize` 后 Release **仍然 642/643**（该行不变），
  所以这个 flag 不是解，已回滚不留痕。**注意** `-fno-slp-vectorize` 在 g++ 里根本不存在
  （wave-01 教训 2 的再现，先 `g++ -O3 -fno-tree-slp-vectorize -c` 验过才用）。
  下一步该做的：在 `-O0` 与 `-O2` 下分别打印 `cell_barycentre` 的
  `cbc.set` 之后、每次 `add`、`scale(1.0/nvtx)` 之前的 x 位型，
  指认符号位是在哪一步翻的；怀疑方向是 `scale` 里 `x *= factor` 与
  `1.0/nvtx`（`nvtx==0` 时是 `inf`，`0*inf` 在两处求值顺序不同）。
  **这条测试保持红色不销**：它是唯一一条已知未解释的门禁差异，
  放宽比较或改基线都不允许 → **Wave 2 内解决**
- [ ] **T-916** Wave 3 硬任务：D 的 `sinh` 有两处**结构**分歧（不是舍入）——
  (a) 奇对称破坏：`sinh(-20)`=-242582602.56479013 而 `sinh(20)`=+242582597.70489514，
  二者绝对值不等，且都不等于 `(exp(x)-exp(-x))/2`（＝glibc 值）；
  (b) 溢出阈值不同：`sinh(710)` D 给 `inf`，glibc 给 `1.1169973830808557e+308`。
  主干调用点 `gas/two_temperature_nitrogen.d:111` 的自变量是 `θ_N2/(2*Tv)`，
  **Tv 很小（冷启动/高激波）时可以越过 710**，届时 D 出 inf、C++ 出有限值，
  振动能量方程随之分叉——不是末位差异。
  转写时须：先用 `probes/gas/probe_sinh_range.d`（待写）扫 `Tv` 全域把两条分歧的
  实际命中范围量出来，再决定是复刻 Phobos 的 `sinhImpl` 还是在调用点显式保留 D 行为。
  **禁止**用"反正 1e-13 容差够宽"糊过去：inf 与 1e308 的相对差是 1e-308 量级的**类别**差 → **Wave 3**
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
