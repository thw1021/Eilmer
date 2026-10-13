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
- [ ] **T-012** `geom/surface/` + `geom/volume/`（`beziertrianglepatch.d` 723、`nurbsvolume.d` 544 等）
  — 曲面/体单元，`grid_motion` 与 `sketch` 依赖。依赖 T-010
- [ ] **T-013** `geom/misc/`（`univariatefunctions.d` 833、`sketch.d` 964、`svg.d` 364）与
  `geom/gpath/`（`modifiedpath.d` 394）。注意 `misc/libplot.d` 428 属 `with_libplot` 变体，推迟
- [x] **T-014** `gasdyn/`：`gasflowexception.d` 13、`idealgasflow.d` 952、`idealgasflow_demo.d` 93 —— **完成 2026-10-13**，见 `task-log.md` T-014 节（39,778/40,451 量逐位相同、0 越带、59/59 断言、demo 字节相同）。
  **范围修正（本轮核实）**：`gasflow.d`(1595) 与 `gasflow_demo.d`(202) 移出 Wave 2 → 新 ID **T-018**。
  依据：`gasflow.d` 的激波关系式签名普遍是 `ref const(GasState)`/`GasModel gm`，并做
  `cast(CEAGas) gm` 动态下行转换（`gasdyn/gasflow.d:85`），而 `GasState`/`GasModel` 属
  `gas/` 包（非 Lua 23,281 行＝Wave 3）；`gasflow_demo.d:14` 还要
  `init_gas_model("../gas/sample-data/cea-lut-air-version-test.lua")`，属 Wave 7。
  在 Wave 2 做它必须凭空造一个 gas/ 桩，那就是改行为
- [ ] **T-018** `gasdyn/gasflow.d`(1595) + `gasflow_demo.d`(202)：随 **Wave 3** 的 `gas/`
  一起做（依赖 `GasState`/`GasModel`/`CEAGas` 真实体与 `init_gas_model`）
- [ ] **T-015** Wave 2 门禁扩充：新增基线 `3D/sod-shock-tube/usg`（`sod-usg-test.rb` 已注册于
  `test-names.txt:21`，走非结构网格，正是 T-011 的检验对象）；并确认 `3D/sod-shock-tube/sg`
  与 `usg` 两条链在 C++ 侧可跑
  （usg 基线已抓取并 D-vs-D 自测通过：75 步、`final-t= 0.000600988`、17 个可比产物全 PASS，
  含 `.flow/.grid` gzip payload 180,631/181,024/145,581 字节与两个 probe 读数）
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
  **Wave 2 暴露面已实测为 0**：`grep -rE "(const|immutable) (double|real|number) \w+ = [0-9.]+ [-+*/] [0-9]"`
  在 geom(去 luawrap)+gasdyn 全树命中 **0 处**——本风险来自探针/测试自己写的算式字面量，
  不来自求解器源码；Wave 3+ 的 gas/ 与求解器主干要各自重测这条，不能假定仍为 0

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
