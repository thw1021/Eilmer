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

- [ ] **T-010** `geom/elements/`（`vector3.d` 812、`properties.d` 1168 等）— 最底层，被全树引用；
  先做。验证：`eil::Vector3` 的算术/点积/叉积与 D 逐位对照 + `%.6g` 打印一致
- [ ] **T-011** `geom/grid/`（`sgrid.d` 1865、`usgrid.d` 2524、`paver.d` 1737、`grid.d` 490）
  — 结构化/非结构网格核心。依赖 T-010
- [ ] **T-012** `geom/surface/` + `geom/volume/`（`beziertrianglepatch.d` 723、`nurbsvolume.d` 544 等）
  — 曲面/体单元，`grid_motion` 与 `sketch` 依赖。依赖 T-010
- [ ] **T-013** `geom/misc/`（`univariatefunctions.d` 833、`sketch.d` 964、`svg.d` 364）与
  `geom/gpath/`（`modifiedpath.d` 394）。注意 `misc/libplot.d` 428 属 `with_libplot` 变体，推迟
- [ ] **T-014** `gasdyn/`：`gasflow.d` 1595、`idealgasflow.d` 952、`gasflowexception.d` 13、
  并把 4 个 `*_demo.d`（`gasflow_demo` 202 等）转为对照测试。依赖 T-010
- [ ] **T-015** Wave 2 门禁扩充：新增基线 `3D/sod-shock-tube/usg`（`sod-usg-test.rb` 已注册于
  `test-names.txt:21`，走非结构网格，正是 T-011 的检验对象）；并确认 `3D/sod-shock-tube/sg`
  与 `usg` 两条链在 C++ 侧可跑
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
  本机无 JSON 库 → **Wave 4 之前**（全局配置与 BC 按名注册都依赖它）
- [ ] **T-903** substrate 缺口：`toString()` 类名前缀无法与 D 一致；`luaL_error` 的 longjmp 跳过
  C++ 析构，跨边界传临时字符串会泄漏 → **Wave 7**
- [ ] **T-904** `complex_numbers` 支：Wave 1 文件里 `#ifdef` 保留了复数支但**从未编译**，
  需完整转写 `ntypes/complex.d`(1,942) 并两支编译验证 → **Wave 12**
- [ ] **T-905** 按消费波次补迁的 nm/util 叶子：`nelmin`(432)/`spline`(120)/`splinelsq`(161) 随
  气体 LUT(Wave 3)；`tree_patch`(996)/`univariate_lut`(101) 随插值(Wave 4)；`bdfLU`(118) 随 CHT；
  `util/zip.d`(1801) 随 gzip 变体(Wave 12)
- [ ] **T-906** `util/test_runner.d`(256)：**决定不移植** — C++ 侧由 CTest 承担，D 的
  `unittest` 已逐个转为测试。留此记录以免将来重复讨论

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
