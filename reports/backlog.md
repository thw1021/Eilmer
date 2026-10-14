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
- [x] **T-011** `geom/grid/`（`sgrid.d` 1865、`usgrid.d` 2524、`paver.d` 1737、`grid.d` 490）
  — 结构化/非结构网格核心。依赖 T-010、T-017
  **进度（2026-10-14，四配置全绿）**：`grid.d`(490) 与 `paver2d.d`(52) **已闭合**——
  `geom_grid_grid` **643/643** 字节相同（含 T-919 钉住的行 178，Debug/Release/
  slice-check/-flto 四档一致）、`geom_grid_nanadd` **638/638**；
  `geom_grid_paver2d` **480/480 行 + 396/396 坐标**（`fill_interior` 因 G5 未转而做成
  模板，两种容器写法 `Array<shared_ptr<USGFace>>` 与 `Array<USGFace*>` 结果逐行一致）。
  `paver.d`(1737) → **G2 完成 2026-10-14**（见 `task-log.md` 的 T-011 G2 / T-019 节）：
  从归档状态（当时回放只发出 **219/1961 行**、217 行字节相同，停在基线第 219 行
  `sec3.ipr.emptyrow`，`make.*` 那节生成代码根本没写）推到
  **1961 行全发出、1961 行字节相同、0 差异**，Release / Debug / slice-check 三档一致，
  注册为 `ctest:geom_grid_paver`。补的是**测试侧** 1742 行生成器
  （`grpMake/grpNode/grpRow/grpSmk/grpSeam2/grpPave` + square/lshape/wedge/circle 四种边界），
  与主线程归档时的判断一致。
  **参照系全程未动**：`probes/geom/grid/probe_paver.d` 与 1961 行基线的 sha256 仍是
  `d96d146f…`（自主线程归档那次提交 `a8027e8` 起未变）⇒ 排除"改基线迁就实现"。
  实现侧只改两处，各有行号作证：① 删本地 `d_sin/d_cos` volatile 包装、改调
  `eil::sin`/`eil::cos`（T-920），由 `sec2.lrm*`/`sec2.ulv*`（30 行）与
  `pave.circle6.*` 在 -O3 下钉住；② `point_dist`(:171)/`norm`(:202) 的外层 `^^0.5`
  改 `eil::sqrt` ——**这就是 T-925 的来源**，改之前 145–146 行（`sec2.ulv3.0/1`）
  是全树唯二红行。`wip/` 已清空删除：半成品是在门禁下干净落地的，没有靠降低标准。
  **参照侧已就绪（本轮完成）**：G4 语料补齐到 **2,097 行**（`de88552`：原基线比探针少 18 行
  `sg.frmt.*`，纯插入、旧行未变）；G5 从零建立 `probes/geom/grid/probe_usgrid.d`
  （2,964 行）→ **1,393 行语料**，`rc=0`、两跑逐字节相同、与代理交付的 1,360 行前缀
  **0 行差异**（后 33 行是纯扩展）。
  **一次值得记住的事故**：我给 usgrid 探针**先后派了两个代理**，因为第一个"看起来死了"
  （其实还在跑，65 分钟后才报失败），于是两个写手**交替覆写同一个探针文件**——
  交付时语料比探针少 33 行，且我最初用一个**半成品源码编出来的二进制**去验，
  看到的是一个并不存在的崩溃。教训：**重派之前先确认前一个已停**，
  以及"探针与其基线必须来自同一份源"要逐次核 sha。
  **G4/G5 实现进行中（本轮派出的代理在 150 轮上限处被截断）**，
  留下的状态**是量出来的，不是它自报的**：
  `sgrid.hpp/cpp`（13.4 KB / 91.6 KB）+ `usgrid.hpp/cpp`（16.9 KB / 129 KB）
  + `tests/test_geom_grid_usgrid.cpp`（50.8 KB）**全部能编**，
  且**两个门禁都按纪律没有注册**（`wave2_grid_g4/g5.cmake` 里 `add_test` 被注释掉，
  注册条件写死为"语料 0 差异才开"），所以全树仍是 **100/100 三配置**。
  我手工把它编出来跑了一次真实回放：
  **1393 行里只发出 315 行、301 行字节相同** ⇒ 约 **23%**，还差 1078 行；
  `sgrid` 的测试**根本没开始写**。
  **G6 的 sgrid/usgrid demo 基线本轮已抓**（原来标"尚未抓取"）：
  `geom_grid_sgrid_demo.txt` **26 行 stdout** + `…sgrid_demo.artefacts.txt` **16 个产物摘要**；
  `geom_grid_usgrid_demo.txt` **56 行** + **28 个产物摘要**。
  两跑（不同 scratch 根目录）stdout 逐字节相同、每个产物 sha256 相同。
  **新发现（必须记下来，否则端口测试一定写错）**：这两个 demo 用**相对路径**找输入，
  工作目录因此是契约的一部分——`sgrid_demo` 要 `../../examples/eilmer/3D/gridpro-import/blk.tmp`、
  `usgrid_demo` 要 `sample-data/square-mesh.su2`；布局不对时它们**直接死于
  `ErrnoException`**（我第一次跑就是 rc=1、20/8 行后中断，看着像 demo 坏了）。
  复现布局写在两个 `.artefacts.txt` 的头注释里。产物按 `phase-0.md` 既有规则**只存摘要清单
  不存字节**（sgrid_demo 单独就产 3.4 MB 的 .vtk），`.gz` 存**解压后 payload** 摘要。
  **第二个续做代理（同样在 150 轮处被截断，且没留下任何报告）实测推进到**：
  **发出 615 行、615 行全部字节相同**（`add_executable` 已开、`add_test` 仍注释着，
  注册纪律没破），约 **44%**；停在语料第 616 行
  `Cheat by making all triangle cells connected to the mid-point.`——
  也就是从"paver2d `fill_interior` 的两行 writeln"开始，测试侧后面还没生成。
  ⇒ **仍是"测试侧缺口"而不是"实现错"**（与 G2 完全同型）：前 615 行零不匹配说明
  被真正走到的路径都对，我先前定位的两类分歧（行 208 的
  `ConvOverflowException` 次序、行 267 起的 `index [18446744073709551615]`）
  都已落在 615 之内 ⇒ 已修。
  **我自己的两次测量错误也记在这里，别再犯**：①第一次我看 usgrid 回放"0/100 全红"，
  真相是我在新构建目录里**只编了一个 target**就去跑 ctest；②更早我把"源码改了但用的是
  旧二进制"当成探针崩溃。**"全绿/全红"之前必须先确认编了什么、用什么编的。**
  **已定位的两类真实分歧**（下一位不必重新发现）：
  1. `us.face.str.bigcount`（语料行 208）：D 抛
     `std.conv.ConvOverflowException "Overflow in integral conversion"`，
     端口走成了 `OutOfMemoryError "Memory allocation failed"` ⇒ 端口在越界转换之后
     先去分配，次序与 D 不同；
  2. `us.ctor.d{0,1,2,3,-1}.tostring`（行 267 起连续多行）：D 是
     `ArrayIndexError "index [0] is out of bounds for array of length 0"`，
     端口是 `index [18446744073709551615]` ⇒ 端口拿一个**下溢的 size_t** 去索引，
     D 在那个位置用的是 0（`usgrid.d:880` 的 `toString()` 里）。
  **剩余**：把 usgrid 回放到 1393 行 0 差异（当前 315 发出）、写 sgrid 的回放测试并到
  2097 行 0 差异、然后才允许注册两条门禁；四个 demo 里 `sgrid_demo`(101)/`usgrid_demo`(103)
  的 D 基线**尚未抓取**；
  四个 demo（`paver2d_demo` 51、`paver_demo` 115、`sgrid_demo` 101、`usgrid_demo` 103；
  前两者的 D 基线**已抓取并验证可复生成**，见 `cmake/wave2_grid_g3.cmake` 的头注释）、
  以及从 T-012 挪来的 `gmopatch.d` 171 + `meshpatch.d` 100。
  **范围追加（2026-10-14 实测）**：`surface/gmopatch.d`(171) 与 `surface/meshpatch.d`(100)
  **不能进 T-012**：前者 `import geom.grid.sgrid` 且在 :87 `new StructuredGrid(...)`，
  后者 :23 的构造入参就是 `ref StructuredGrid grid`。两者改挂 **T-011**，随 sgrid 一起做完
  （否则在 Wave 2 里必须凭空造一个 StructuredGrid 桩，那就是改行为，同 T-018 的理由）
  **G4 与 G5 均已闭合（2026-10-14，两条门禁都已注册）**：
  - `geom_grid_usgrid`：语料 **1393 行全发出、1393 行逐字节相同、0 差异**；
  - `geom_grid_sgrid`：语料 **2097 行全发出、2097 行逐字节相同、0 差异**；
  - **三配置实测**：default / `Release` / `Release+EIL_SLICE_CHECK` 各 **102/102**
    （注册后重新构建再跑，不是注册前的旧测试清单）。两个数字都是主线程**在代理没用过的
    构建目录里自己复跑**得到的。
  - 这一段里被语料逼出来的**真端口缺陷共 5 个**，都不是数值：
    ①`usgrid.cpp` `read_from_vtk_text_file` 为 D:1791 的**一行** `readln` 读了两行（行 1199/1201/1203）；
    ②`usgrid.cpp` `d_readln` 把**读失败**当 EOF——D 的缓冲 File 在 read 失败时抛
    `StdioException(strerror(errno))`，只有真 EOF 才返回空串且不抛（行 1267；glibc 对目录
    `fopen` 会成功、第一次 `fgetc` 才 `EISDIR`）；
    ③`sgrid.cpp` `toString()`(行 58) 与 `read_from_text_file`(行 1031)：**D 的函数实参从左到右求值，
    gcc 从右到左**，空格点上报了第二个下标的回绕索引；
    ④`sgrid.cpp` `measure_of_badness` 把 D:1336 的 else 步长写成 `j-1`（应是 `i-1`），
    `j==0` 时回绕并提前触发 D:1337（行 703）；
    ⑤`sgrid.cpp` 的 `formattedRead`/`rawRead`：**输入耗尽时 std.format 不做任何转换**（既不赋值也不抛，
    行 1104/1106），`File.rawRead` **短读不抛**且 D 的局部数组起点是 `.init`（行 1167）。
    ③④⑤ 都是**"空白网格/正常文件的行全绿，只有畸形输入才暴露"**那一类，
    与 T-923"没语料的路径能藏住编造文本"同源。
  - 顺带闭合 **T-927**（`to!size_t` 报 `ulong`、无符号不吃符号；`formattedRead` 同规则）与
    **T-929/T-926/T-928 的实测部分**（三条 substrate 缺口的证据都落了盘，实现仍待做）。
  **T-011 剩余（G6）**：四个 demo `paver2d_demo`(51) `paver_demo`(115) `sgrid_demo`(101)
  `usgrid_demo`(103) 的**基线都已抓**，剩 C++ demo 程序与门禁；
  以及 `surface/gmopatch.d`(171) + `surface/meshpatch.d`(100) —— 这两个的**探针与语料还没抓**，
  是 G6 里唯一"参照侧未就绪"的部分。demo 的**工作目录是契约**（相对路径，见上面"新发现"）。
  **G6 完成，T-011 整包闭合（2026-10-14）**：
  - **G6a 四个 demo 全部转完并进门禁**：stdout 四条全注册且逐字节相同
    （paver2d 2/2、paver **129/129**、sgrid **26/26**、usgrid **56/56**），
    产物门禁两条注册（paver2d、usgrid）；新增 `harness/compare_artefacts.sh`
    （逐行 sha256 + 字节数，`.gz` 比解压后 payload，缺文件计为失败）。
    demo 的**工作目录契约**按基线头注释实现（cmake fixture 建 `examples` 符号链接、
    从 `<root>/src/eilmer` 运行），否则 D:96 的 `blk.tmp` 直接 `ErrnoException`。
  - **两条产物门禁故意不注册**，各自原因已升格成任务：`T-931`（paver 内部铺面，
    1490 个顶点里 398 行末位不同、前 214 个全同 ⇒ 定位在内部而非边界）、
    `T-930`（**Phobos `to!double` 不是正确舍入**，主线程自己复测确认：
    15 个数值 token 里 2 个差 1 ULP、D 是错的一方）。
  - **G6b `gmopatch.d`/`meshpatch.d` 闭合**：先抓参照再写实现
    （gmopatch **497 行**、meshpatch **303 行**，两份语料各自**行 1 记了 D 文件 sha256**）；
    我用影子树**重跑 `probe_gmopatch.d`，输出与已提交语料逐字节相同**
    （这条检查就是上一次"语料比探针少 33 行"事故的对策）。
    `gmopatch.hpp` 落地后 `sgrid_demo` 的 `__has_include` 分支打开，
    `test_gmo_grid-2D.vtk`（17,420 字节，sha `e4cbfa71…223edddf`）与基线行一致，
    于是 sgrid 产物集从 10/16 升到 **11/16**，剩下 5 行**只**是 T-930。
  - 顺带闭合 **T-932**（`to!int(double)` 边界，端口一处 `>=` 把最大合法 int 拒掉）。
  **全波状态**：default **110/110**（新增 6 条 demo + 2 条 surface 门禁），
  Release 与 `EIL_SLICE_CHECK` 同数（见下条提交/`task-log.md`）；
  `check_staleness.sh` = `no drift`；`geom/grid/` 与它带进来的 `gmopatch`/`meshpatch`
  至此**没有剩余待办**，Wave 2 只剩 T-015（等 Wave 5 候选二进制）与 T-016 收尾。
- [x] **T-012** `geom/surface/` + `geom/volume/`（`beziertrianglepatch.d` 723、`nurbsvolume.d` 544 等）
  — 曲面/体单元，`grid_motion` 与 `sketch` 依赖。依赖 T-010
  **完成 2026-10-14**：`tfivolume.d` 最后落地（`geom_volume_tfivolume`
  **4464/4464 行字节相同 + 2/2 D unittest 断言 + 2/2 结构检查**，三配置一致），
  两个目录级 demo 也过（`demo_equiv_geom_surface_surface` 18 行、
  `demo_equiv_geom_volume_volume` 13 行）。核对方式：对 `gdtk/src/geom/{surface,volume}/*.d`
  逐个查同名 `.cpp`/`.hpp`，缺失只剩 `package.d`（模块聚合，无代码）、
  `surface_demo.d`/`volume_demo.d`（以 demo_equiv 门禁转写）与
  `gmopatch.d`/`meshpatch.d`（已划归 T-011）。
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
- [x] **T-019** `geom/misc/sketch.d`(964) + `sketch_demo.d`(83) — 从 T-013 拆出（原清单误记为已做）。
  **依赖更正（2026-10-14 实测，原条目把它排错了波次）**：本条原写作"依赖 geom.grid，
  故排在 T-012 **与 T-011** 之后"。实测 `grep -cE "sgrid|usgrid|StructuredGrid|
  UnstructuredGrid|geom\.grid" sketch.d` = **0** —— sketch.d 的**函数体完全不碰网格**，
  它 `import geom;`（:36）只是包聚合器。它真正用的是 surface/volume/gpath 的具体子类
  （demo 里 `new CoonsPatch(...)`、`new Arc(...)`），**全部已在 T-012 落地**。
  另：`import ntypes.complex;`（:29）是**无条件**的，但 `Complex` 在全文件出现次数 = 1
  （就是那行 import 自己）⇒ 无实际依赖，不需要等 T-904。
  ⇒ **T-019 现在就能做，不等 T-011**。参照侧已实测可跑：
  `ldc2` 编 `sketch_demo.d` 并链接 **rc=0**，stdout **恰好 2 行**、stderr 0 字节：
  `Begin demonstration of the geometric rendering with svg` / `Done.`；
  写出三个文件产物 `test.svg` 9,739 字节（md5 `510aea9d…`）、
  `test2.svg` 1,955（`60bc566d…`）、`test3.svg` 8,856（`ae87fcbc…`）。
  **D 侧连跑两次自比**：三个文件与 stdout 全部 SAME ⇒ 可当基线（同 T-015 的先自测再入库规矩）。
  — **完成 2026-10-14**：`sketch.d` → `src/geom/misc/sketch.{hpp,cpp}`（287/1155 行），
  `sketch_demo.d` → `tests/demo_geom_misc_sketch.cpp`。语料 **682 行、0 差异**
  （主线程用 `ldc2 -O0` **独立复抓**该探针，与原基线 0 行差异，不是采信代理报数）；
  251/251 个内嵌数值逐位相同、最坏残差 0 ULP、**无容差带**；`sketch.d` 没有任何 unittest 块，
  所以另有 13/13 条 smoke 断言。demo 侧 stdout 2 行/62 字节 + 三个 SVG **文件逐字节**
  （9739/1955/8856，与本条上面记录的 md5 一致）。注册 5 条测试
  （`geom_misc_sketch` + `demo_equiv_…` + 三条 `demo_artifact_…`）。
  **成员无缺失的正面对账**：D↔端口名字集合差为空；简报里"28 个方法"是我按正则数的，
  实测该正则只命中 **27**（漏 `toString`，它以 `override` 开头）——
  已按实测口径更正：**数是工具给的结论，不是事实**。
  **`rotating_a.d`(49) 与 `drifting_eye.d`(65) 从本条移出** → 新开 **T-921**：
  它们不是"排在后面"的问题，是**本机根本没有 GNU libplot**（`pkg-config --exists plplot` 失败、
  `ldconfig -p | grep -c plplot` = 0），`ldc2 -c` 能过但**链接失败**
  （`undefined reference to pl_parampl / pl_newpl / pl_selectpl / pl_openpl / pl_fspace`），
  且两者**都没有 `void main`** ⇒ 参照侧连二进制都产不出来，Wave 2 抓不到任何基线。
  `sketch.d` 自己的 libplot 支在 `version(with_libplot)` 内（:33-34、:62、:152、:178、:261），
  按既有约定原样保留不编译
- [x] **T-922** **端口系统性缺陷：D 的裸 `new Error(msg)` 不带抛出点，端口却给它编了一个。**
  **已全部完成 2026-10-14**，最终账用**带括号配对的逐调用计数**核对（不是行窗口近似）：
  `ErrorBase(...)` 调用中携带 `__FILE__/__LINE__` 的数量
  **84 → 1**（`a1800ca` 84 → 第一批后 `050ab29` 44 → T-914 合并后 `f9b633e` 39 → 本批 1）。
  **保留的那 1 处是正确写法**：`geom/grid/grid.cpp:76` 对应 D 的 `final switch` 越界，
  抛的是 `core.exception.SwitchError`，**它带 `switch` 所在文件的行号**
  （实测语料行 `gen.final_switch`），所以那里删掉才是错。
  同批另两处**该带而未动**的：`util/lua_service.hpp` 的 `EIL_THROW_LUA_INPUT`
  （D 的 `LuaInputException`，`lua_service.d:688-691` 默认实参就是 `__FILE__/__LINE__`）。
  核对口径也记在这里，免得下次又用近似数：`ExceptionBase(` 带 `__FILE__` 的数量
  扫描前后都是 **15**（一族都不该动）；本轮开始时我用行窗口数出的是"82/42"，
  逐调用数出来是 **84/39**——**差的那几处正是跨行调用**，
  行窗口看不见它们，`arc.cpp:130/158` 两处就是这么漏掉的（已在本批补上）。
  三配置 **100/100**、`check_staleness` → `no drift`。下面原记录保留作为判据出处。
  本轮为核 sketch 简报里的 throw 分类而实测出来的（`probes/substrate/probe_throw_identity.d`
  → `baselines/e587b235/probes/substrate_throw_identity.txt`，**15 行，重跑 0 差异、
  行内无地址**）。D 侧的真实规则（每行都是 ldc2 1.40.0 运行期打印的
  `classinfo.name / .msg / .file / .line / toString()` 首行）：

  | D 写法 | 运行期类名 | `.file` / `.line` | 端口现在怎么做 |
  |---|---|---|---|
  | `throw new Exception(m)` | `object.Exception` | **调用点**（且是**编译时传进来的相对路径**原文） | 传 `__FILE__/__LINE__` ✅ 对 |
  | `throw new Error(m)` | `object.Error` | **`""` / `0`** | 传 `__FILE__/__LINE__` ❌ **编出来的** |
  | `throw new Throwable(m)` | `object.Throwable` | `""` / `0` | — |
  | `enforce(c, new Error(m))` | `object.Error` | `""` / `0` | ❌（`bbla.d:341` 就是这种写法） |
  | `new GeometryException(m)` | `geom.geometry_exception.GeometryException` | 调用点（`geometry_exception.d:11` 的 `file=__FILE__, line=__LINE__` 默认实参） | ✅ 对 |
  | `assert(c)` | `core.exception.AssertError` | 调用点；`.msg=="Assertion failure"` | ✅ T-918 已定形 |
  | `assert(c, m)` | `core.exception.AssertError` | 调用点；`.msg` **只有 m** | ✅ T-918 已定形 |
  | 数组越界 `a[9]` | `core.exception.ArrayIndexError` | 调用点 | ✅ |
  | `final switch` 值越界 | `core.exception.SwitchError` | 调用点 | ✅（`grid.cpp:76` 注释里已自证） |
  | `arr.length = -1` | **`core.exception.OutOfMemoryError`** | **`rt/lifetime.d` / 1454** | ~~端口在 `bezierpatch.cpp:29,33,37`、`controlpointpatch.cpp:28,32` 传自己文件的 `__FILE__/__LINE__`~~ **已修**：规则收进 `eil::Array::set_length_signed`（只带消息），5 个本地重载删除 → 见 T-914 |
  | Phobos 内部 assert（`maxElement` 空区间、`sort` 的分区守卫） | `core.exception.AssertError` | **Phobos 源文件与行号**（`.../std/algorithm/searching.d` 1338、`sorting.d` 2195/2201） | ❌ 端口传自己文件的 `__FILE__/__LINE__`；且 `src/nm/smla.hpp:127` 的**消息文本本身也是编的**：端口写 `"range is empty"`，D 实际是 `"r is an empty range"`（searching.d:1338 原文）——与 T-918 同一类缺陷，因为**没有任何语料行覆盖它**所以一直没红 |
  | `to!double` 失败 | `std.conv.ConvException` | `.../std/conv.d` 3349（库文件，不是调用点） | 语料只比 `.msg` ✅ 文本已由 T-913 逐字钉住 |
  | `new ErrnoException(m, e)` | `std.exception.ErrnoException` | `""` / `0`（默认实参是 `null`/`0`，与 Exception 不同） | 需按站点核 |

  **爆炸半径（本轮实测清点）**：端口里 `ErrorBase(...)` 带 `__FILE__` 的站点共 **83 个**
  （注意：按行 grep 只能数到 43 个，因为调用跨行；多行感知统计才是 83）。
  其中**按消息文本能直接对上 D 侧 `new Error(`** 的有 **39 个**
  （`aopatch.cpp` ×4、`coonspatch.cpp` ×4、`polynomial.cpp` ×5、`arc.cpp` ×3、`bezier.cpp` ×3、
  `polyline.cpp`、`beziertrianglepatch.cpp:199`、`nomenclature.cpp:30`、
  `univariatefunctions.cpp` ×11、`nurbs_utils.cpp:126`、`bbla.hpp` ×5 等），
  另有 **32 个**用变量消息（形如 `eil::ErrorBase(eil::text(errMsg), __FILE__, __LINE__)  // D:28`），
  其 `// D:NN` 引用指向的行已抽查为裸 `throw new Error(text(errMsg))`
  （`nurbssurface.cpp` ×5 已核实）。剩下 12 个需逐点读。
  **为什么现在没红**：语料比对的是 `.msg`，全树只有
  `tests/test_geom_elements_projection.cpp` 一处比 `.file()`，而它比的是
  `GeometryException`（确实带调用点）⇒ 缺陷**完全潜伏**。
  **为什么必须修**：需求第 4 条要求日志输出逐字节一致，而 `toString()` 的第一行是
  `类名@文件(行号): 消息`；Wave 4/5 的求解器日志一接上就会把这 83 处全部暴露，
  那时改就是"改行为"了。趁现在只有端口侧、无历史日志可比，一次性扫掉。
  **已入库的部分**：`substrate/include/eil/math.hpp` 之外的
  `substrate/include/eil/error.hpp` 里把上面那张表**原样写成注释**（含"toString() 的
  类名前缀与回溯不可移植 → T-903"、"Phobos/rt 内部 raise 的位置不是调用点"两条），
  所以以后新转写站点有本地依据，不必再去别处找规则。
  **按文件清点好的待扫站点**（分类判据：消息字面量能在 D 树里对上 `new Error(`，
  或站点尾注释 `// D:NN` 指向的那行是 `new Error(`）——共 **40 个**，全是删两个实参：
  `geom/gpath/{arc:1, bezier:3, nurbs:2, polyline:1, polynomial:5}`、
  `geom/misc/{nurbs_utils:1, univariatefunctions:11}`、`geom/surface/nurbssurface:5`、
  `geom/volume/nurbsvolume:7`、`nm/bbla.hpp:4`。
  另有 **35 个**分类器返回 `None`（消息是变量拼接、且站点无 `// D:NN` 引用）、
  **6 个** `notfound`、**1 个** `unclear` —— 这 42 个**必须逐点读 D 侧**再决定，
  不能猜。**总数更正**：本轮首次报的 83 是**修 smla 之前**的数（smla.hpp:127 那条
  已随 T-923 改掉），现在的实际数是 **82**。
  **进度更新（同日，本波内已完成 40/82）**：上面那 40 个"消息字面量能对上 D 侧
  `new Error(`"的站点**已改完**——做法是**纯文本删除** `,\s*__FILE__\s*,\s*__LINE__`
  （先按行号定位 `ErrorBase(` 再在其后 300 字符内找这一段），
  **不动行结构**。核对方式三条，都过：
  ① 全局计数 `ErrorBase(` 带 `__FILE__` 的站点 **82 → 42**，正好 −40；
  ② 同一次统计里 `ExceptionBase(` 带 `__FILE__` 的 **15 → 15 未动**
  （这些是 D 侧确实记录抛出点的，动了就是新缺陷）；
  ③ `git diff` 逐文件看只有该删除项、没有多余 `}`，再编 Release/Debug/slice-check
  三配置 **100/100**、`check_staleness.sh` `no drift`。
  第一批涉及 10 个文件：`geom/gpath/{arc,bezier,nurbs,polyline,polynomial}.cpp`、
  `geom/misc/{nurbs_utils,univariatefunctions}.cpp`、`geom/surface/nurbssurface.cpp`、
  `geom/volume/nurbsvolume.cpp`、`nm/bbla.hpp`。
  **剩下的 42 个**才是难的部分：35 个消息是变量拼出来的且站点没有 `// D:NN` 引用、
  6 个文本在 D 树里找不到对应、1 个不明确——**必须逐点读 D 侧那条 throw 的类**再决定，
  不能批量处理。**Wave 3 之前做完**（判据同上：D 的类决定，不是端口的方便）。
  **踩过的坑（下一位别再踩）**：本轮试过用脚本一次性删实参，**两次都把多行调用改坏了**——
  第一次把 `throw ...("...",\n  __FILE__, __LINE__);` 的后半行吃掉、留下**多余的 `}`**
  （`bbla.hpp` 的 hstack/vstack 直接编不过），第二次正则漏了行尾 `;` 又产生 `))`。
  两次都靠 `cmake --build` 才抓到，已 `git checkout -- src` 全部回退，
  **没有**把坏改动留在树里。结论：这一类"看起来机械"的全树改写**必须逐点改并用编译验证**，
  或者用能正确配对括号的解析（本仓库里 `eil::text(` 之类嵌套调用不少）。
  **修法（机械，但有前置）**：①`ThrowableBase` 的默认已是 `file=""`/`line=0`，
  所以修复=删掉那两处实参，不动消息文本；②按上表逐站点决定"删/留"，**每个站点的判据是它
  D 侧那一条 throw 的类**，不是端口的方便；③Phobos/rt 内部 assert 那两类**不能**填端口的
  `__FILE__`，也不能硬写 `rt/lifetime.d(1454)`（那是 ldc2 版本相关的源码路径）——
  正确做法是 `.file` 留空 + 头注释写实测值，并把该行的可观测部分交给 `.msg`；
  ④扫完补一个**端口侧回放测试**（本条的前半）把 15 行 D 语料的
  `msg` + "有没有 file/line" 布尔值钉住，否则下次还会漂；
  ⑤顺带修 `smla.hpp:127` 的消息文本 `"range is empty"` → `"r is an empty range"`，
  并给它加一条覆盖语料行（今天没有行覆盖它，这就是它没红的原因）。
  → 已做 40 个，余 42 个待逐点核 → **Wave 3 之前做完**（Wave 3 的 `gas/` 是 `Exception` 与 `Error` 混用最密的包）
- [ ] **T-921** `geom/misc/libplot.d`(428) + `rotating_a.d`(49) + `drifting_eye.d`(65)：
  **外部库缺失**，非转写难度问题（判据见 T-019 本条内的实测）。装好 libplplot 之后再做，
  或按 `with_libplot` 变体推到 Wave 12（与 T-904 `complex_numbers` 同批）。
  留此条是为了**防止将来又把它当"T-011 做完就能做"** 而凭空造桩

- [x] **T-923** **假门禁**：`nm/stmatrix.d` 的读取失败路径**一条语料都没有**，
  于是端口里两条**凭空造的异常文本**一直没人管。
  **完成 2026-10-14**（同 T-922 的动因，扫出来的下一处）。
  做法是先**量 D 真正抛什么**（`probes/nm/probe_stmatrix_fails.d` →
  `baselines/e587b235/probes/nm_stmatrix_fails.txt`，**7 行，D 连跑两次自比相同**）：
  * 文件打不开：`std.exception.ErrnoException`
    `msg=[Cannot open file \`X' in mode \`r' (No such file or directory)]`
    ——端口原来抛的是 `ExceptionBase("Failed to open \"X\"")`，**文本与类都不是 D 的**；
  * 空文件 / 只有注释 / 声明 2 条只给 1 条 / 首个空行：D **一律**
    `core.exception.ArrayIndexError`
    `msg=[index [0] is out of bounds for array of length 0]`，
    因为 D 的 `readln()` 到 EOF **不抛异常、返回空串**，紧接着的 `line[0]`（或空 `split()`
    之后的 `tks[0]`）才是真正发作点 ⇒ 端口原来的
    `ExceptionBase("Unexpected EOF")` 是**这条函数里根本不存在的异常**。
  * 表头 token 不足：`ArrayIndexError index [2] is out of bounds for array of length 2`。
  修法按实测改三处（`src/nm/stmatrix.hpp`）：open 失败改为 strerror 组装的 D 文本
  （substrate 尚无 ErrnoException，类名限制是既有的 **T-903**）；两处 EOF 检查**删掉**，
  让 `getline` 失败留下的空串走 D 同一条越界路径——端口 `eil::Array::operator[]` 抛的
  `RangeError(i,n)` 文本与 D 逐字节相同，所以不需要新增任何机制。
  **新增门禁** `ctest:nm_stmatrix_fails`（7 行逐字节），并做过**判别力验证**：
  故意把 mode 改成 `w'`、把 `RangeError(0,0)` 改成 `(1,0)` → **3 行变红**；
  还原后 **7/7**。这条测试比原来那条"读得动就好"的测试强的地方，正是它现在会因
  异常文本改变而红。
  **推广出来的检查（后续波次照做）**：凡端口里 `throw ...Base("任意字面量")` 的站点，
  该字面量必须在 D 树里能 grep 到原文；grep 不到就是**编的**。本轮按这条全树扫了一遍
  `src/`：**100 个带字面量消息的 throw 站点，只有这 3 个对不上**（其余 97 个逐字命中 D），
  说明这不是普遍习惯问题，而是**没有语料覆盖的路径**才会漏——所以本条的价值在
  "补语料"而不只在"改文本"。
- [x] **T-016** Wave 2 收尾报告 `wave-02.md` + `wave-02` tag + 本清单更新
  **完成 2026-10-15**。`reports/wave-02.md` 从"进行中"更新为"已完成"并刷新全部实测数字
  （C++ 头 75→**81**、src+substrate 行 29,376→**35,386**、测试/demo 行 30,337→**37,179**、
  语料总行 84,656→**87,263**、CTest 100→**113** 且三配置全绿），新增 §1.5 收尾批表
  （T-929/T-928/T-926① 各自的实测事实、落点、门禁、判别力）；§3"待完成"去重并改为准确的分派清单
  （T-015 阻塞、T-921 环境阻塞、T-926②③、T-930、T-931、T-916 与横切债务）。`wave-02` git tag 随本报告打上。

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
- [x] **T-914** substrate 缺口：`eil::Array::set_length(size_t)` 表达不了 D 的
  **有符号** `arr.length = L`。D 里 `L<0` 抛 `Error "Memory allocation failed"`；
  C++ 侧若先转 `size_t` 就会变成试图分配 2^64 个元素。本波代理在
  `src/geom/surface/bezierpatch.cpp:26-39` 用本地 `set_length_checked` 三个重载绕开，
  同一需求在 volume/grid 还会再出现 → 应在 `substrate/include/eil/array.hpp` 定形。
  — **完成 2026-10-14**：`eil::Array::set_length_signed(long)` 已入 substrate，
  **5 个本地重载全部删除**（bezierpatch.cpp 三个 + controlpointpatch.cpp 两个），
  两个头注释里指向已消失的本地 helper 的说法也改了（注释指向不存在的符号，比没注释更坏）。
  语义按**实测**而不是按名字定：`neg.length = -1` 在 D 里是
  `core.exception.OutOfMemoryError`、`.msg == "Memory allocation failed"`、
  `.file == "rt/lifetime.d"`、`.line == 1454`
  （`probes/substrate/probe_throw_identity.d` 行 `gen.negative_length`）⇒ substrate 版
  **只带消息、不带 `__FILE__/__LINE__`**（T-922 的一条：D 的位置在运行时内部，
  端口填自己文件的位置就是凭空造站点）；`L == 0` 合法、**不抛**
  （语料行 `bzp.nm1.ctor threw=none`）。可达性有语料撑着：`bzp.nm2.ctor`、`cpp.gneg`、
  `cpp.gnegj`、`sg.frmt.nivbig` 四条都是这段文本。
  改后两模块门禁仍 Passed（`geom_surface_bezierpatch`、`geom_surface_controlpointpatch`）。
  **分发约定**：G4/G5（sgrid/usgrid）里把用户输入直接当数组长度的地方，转写时直接调
  `set_length_signed`，**不要再建第六个本地副本**
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
- [x] **T-919** `geom_grid_grid` 行 178 `cbc.nan2d x=`：D `-nan` / 未打桩的端口 `nan`。
  **完成 2026-10-14**，数字与复跑命令见 `task-log.md` 的 T-917/T-918/T-919 节。
  根因**不是** `addpd`/向量化（那条理论已由实测否证，见下），而是
  **IEEE 754-2008 6.3 对"两个 NaN 相加返回哪一个"不作规定，x86-64 SSE 按寄存器角色回答**
  （`addsd xmm_dst, xmm_src` 返回 dst 那个），所以**两个编译器的答案都随 -O 变**：
  同一份 probe 源在 ldc2 **-O1/-O2/-O3 比 -O0 基线分别少 90/103/103 行一致**
  （动的组是 `add.ref`/`add.ptr`/`opplus_eq`/`scale`/`opmul_eq`；
  `opplus`/`opminus`/`opminus_eq`/`opdiv`/`opdiv_eq`/`add.fac` 不动）。
  ⇒ 基线是 ldc2 **-O0** 抓的，所以 -O0 的取值是契约；端口把这一条写出来
  （`eil::add_rhs_nan` / `eil::mul_rhs_nan`，`substrate/include/eil/math.hpp`），
  应用于 `vector3.cpp` 的 7 个 `add` 重载、`scale`、`operator+=`、`operator*=`，
  其余算符保持原样（实测两侧在每个 -O 档取值相同）。
  常驻守卫：`probes/geom/grid/probe_grid_nanadd.d` → 638 行位型语料 → 
  `tests/test_geom_grid_nanadd.cpp`。**打桩后** 638/638 与 643/643 在
  Debug / Release / slice-check / 手工 `-flto -O3` 四种构建全字节相同；
  **去掉桩**则 nanadd 在 -O0/-O3/-flto 分别差 **36 / 67 / 90** 行。
  代价实测：四顶点重心 14.5 ns（钉住）vs 10.4 ns（未钉），**+37~40%**，按需求第 3 条保留。
  **已排除的假设（本条的历史错误记录，已改）**：`vector3.cpp` 的
  `x += other.x; y += other.y` 被 GCC 打成 `addpd` 因而取错 lane 的 NaN payload。
  实测：未打桩端口加 `-fno-tree-slp-vectorize` 后 `grep -c addpd` = **0**，
  行 178 照旧红，nanadd 反而恶化到 **103 行** differ。
  （`-fno-slp-vectorize` 在 g++ 里根本不存在——wave-01 教训 2 的再现，
  先 `g++ -O3 -fno-tree-slp-vectorize -c` 验过才用。）
- [x] **T-920** 同一个自变量的 `cos(x)`+`sin(x)` 会被**两个编译器都**从 -O1 起合并成一次
  glibc `sincos()`，而 `sincos` 与两次独立 libm 调用**不等价**。
  **完成 2026-10-14**，数字与复跑命令见 `task-log.md` 的 T-920 节。
  最初登记的说法（"g++ 合并、ldc2 不合并"）**是错的**：LDC 把 Phobos 的
  `std.math.cos/sin` 下发成 `llvm.cos.f64`/`llvm.sin.f64`，LLVM 自己的 sincos pass
  从 -O1 起又把它们合回去。按"哪一种 libm 形态能同时复现某行的两个位型"分类
  （`tests/test_data/sincos_angle.txt`，415 个角）：
  - ldc2 **-O0**：separate-only **401** / sincos-only **0** / 两者皆可 14
  - ldc2 **-O3**：separate-only **0** / sincos-only **401** / 两者皆可 14
  ⇒ **401/415 行在参照系自己内部随 -O 移动**。基线全是 ldc2 -O0 抓的，所以契约是
  "两次独立调用"——与 T-919 同构：不是去发现一条值层面的规则，而是钉住一个优化级别。
  分歧密度：6×10⁷ 个 0..2π 抽样（g++ -O3、`-ffp-contract=off`）中 400 个角两侧不同
  ≈ **每 1466 个分量一次**，最大绝对差 `1.1102230246251565e-16` ＝ 1 ULP。
  1 ULP 在角度上不是末位噪声：T-910 实测过 `cos(theta)` 过零附近 1 ULP 的角
  会变成 ~2.9e-09 的**相对**变化，再乘半径进入坐标。
  **入库的修法**：`substrate/include/eil/math.hpp` 加 `eil::sin`/`eil::cos`
  （自变量经一次 `volatile` 拷贝；实测该 TU 的 `nm -u` 只剩 `U sin`/`U cos`，无 `sincos`）。
  改道站点：`vector3.cpp`（`rotate_about_zaxis`、`rotate2d`）、`projection.cpp:78`、
  `grid.cpp:533-534`、`arc.cpp:119`、`helix.cpp:70`、`wedgevolume.cpp`
  （此问题最早由它发现：当时 -O3 下 **4594 行里红 20 行**，局部用 volatile 函数指针绕过；
  现删掉本地副本、统一走 substrate——同一规则留两份必漂移，T-910 的教训）、
  `idealgasflow.cpp` 全部成对站点，外加**自己算角度的 9 个测试文件**
  （Release 下原先有 **4 个测试 .o 被合并**，也就是门禁自身的算术走合并形态）。
  门禁：`probes/substrate/probe_sin_cos_pair.d` → **415 行位型语料** →
  `tests/test_substrate_sin_cos_pair.cpp`（注册在 `cmake/wave2_t920.cmake`）。
  判别力实测：**去掉** wrapper 后 Release -O3 **差 401 行、只剩 14 行相同**
  （那 14 行正是 ctrl 组）；装上后 **415/415** 在 -O0 / -O3 / slice-check / `-flto -O3`
  四种构建字节相同。改道后 Release 里引用 `sincos` 的 .o 从 **11 → 0**，-flto 构建也是 0。
  全树 **93 个测试三配置 100%**。
  代价实测：一次 cos+sin 对 **11.97 ns（合并）→ 14.94 ns（保持分开）**，约 +22%；
  与 T-919 同理，按需求第 3 条不为这点速度放宽一致性。
  **同类面仍需后续 Wave 复查**：其他"成对且可融合"的 libm 调用——本项目里目前只找到
  sin/cos 这一对（`exp`/`expm1`、`log`/`log10` 之类 glibc 没有融合形态），
  Wave 4 的 `fluxcalc` 类若出现 `sqrt`+`rsqrt` 或成对三角调用需照此法复查
- [x] **T-925** **更正本仓库自己的一条转写规则**：`x ^^ 0.5` 是 `eil::sqrt(x)`，
  **不是** `eil::pow(x, 0.5)`。`reports/wave-02-brief.md` 第 151 行原来写的是
  "`a ^^ b` **一律**写成 `eil::pow(a, b)`"——那句是错的，而错误来自一个合理的观察
  （`^^2`/`^^3` 确实等于 `pow`）被当成全称。**完成 2026-10-14**：
  - 实测范围 **765 个非负操作数**（`i/7` 400 个、`i*0.001` 200 个、`1/i` 100 个、
    `i*1e-8` 60 个，再加 `0/±0/1/2/1/3/1e±300/real.max`）：
    `x^^0.5` 与 `sqrt(x)` **765/765 位型相同**；`pow(x,0.5)` 与 `x^^0.5`
    **只在 1 个操作数上不同 —— `x = -0.0`**：
    `(-0.0)^^0.5 = 8000000000000000`（`sqrt(-0.0)` 同值，**保留零的符号**），
    `pow(-0.0,0.5) = 0000000000000000`（丢掉符号）。
  - 域错误侧也一致：`(-1)^^0.5 = (-2.5)^^0.5 = (-1e-300)^^0.5 = 7ff8000000000000`
    （**正 NaN**，与 `eil::sqrt`/`eil::pow` 的 T-917 规范化相同）；
    但 `pow(-real.max, 0.5)` 在 D 里给 `7ff0…`（**无穷**而非 NaN），
    所以"两者随便选一个"在负数域上也不成立。
  - 变量指数同样走 sqrt：`double e=0.5; x^^e` 与 `sqrt(x)` 与 `pow(x,e)` 三者位型相同
    （`x=2` → `3ff6a09e667f3bcd`）⇒ 规则看**指数的值**不看它是不是字面量。
  - **发现路径**：`geom_grid_paver` 1961 行里唯二红行 145–146（`sec2.ulv3.0/1`，
    D `-0.50000000000000033/3ffeb67ae8584caa` vs 端口 `…022/…ca9`），
    端口当时在 `point_dist`/`norm`（`paver.cpp:171/:202`）写 `eil::pow(…, 0.5)`。
    D 侧 `^^0.5` 在 Wave 2 只有这两处（`paver.d:202`、`:238`）。
  - **入库**：规则写进 `substrate/include/eil/math.hpp` 的 `eil::pow` 注释头上
    （离最容易用错的地方最近），`reports/wave-02-brief.md` 那条已更正；
    语料扩到 **35 行**（新增 9 行 `half` 组，**按位型而不是按 `==` 比较**——
    第一版用 `==` 时 `-0.0` 行报 `opIsPow=true`、NaN 行报 `opIsSqrt=false`，
    两个都是假信号，因为 `==` 看不见零的符号、也永远看不见 NaN 的相同）。
    顺带记一笔：D 的 `real.max` 放进 `double[]` 会**溢出成 +inf**
    （`7ff0000000000000`），端口表里当初写 `DBL_MAX` 就差一行。
  - **判别力实测**：把 `op=` 从 `eil::sqrt` 换回 `eil::pow(x,0.5)` →
    `-0.0` 那行立刻红（**1 行差异 / 35**）；换回来 **35/35**。
  - **Wave 12 会再撞上**：`shape_sensitivity_core.d:2315-2316`、`ntypes/complex.d:1820`
    （`z^^0.5` 是 csqrt 的写法）。转写这两处时按本条，不要用 `eil::pow`。
- [x] **T-927** **substrate 缺陷：整数 `to!T` 的失败文本按"辅助函数名字"报类型，而不是按 `T`**。
  **完成 2026-10-14**。G5 语料行 1170–1172（`us.su2rd.no-nelem/no-npoin/no-nmark`）是这三行
  把问题暴露出来的：D 写 `to!size_t(getHeaderContent("NELEM"))`，空串时报
  `…converting from type string to type ulong`，端口报的是 `int`——因为 `usgrid.cpp` 的
  11 处 `to!size_t` 全部借用了 `substrate` 里那个硬编码 `"int"` 的 `to_long`。
  - **实测**（`probes/substrate/probe_conv_int_typename.d` →
    `baselines/…/substrate_conv_int_typename.txt`，**105 行，重跑 0 差异、行内无地址**）三条规则：
    1. 报文里是**解析后的类型名**，别名报基元：`size_t`→`ulong`、`ptrdiff_t`→`long`；
    2. **无符号目标根本不吃前导符号**：`to!int("+")` 是 *end of input*，而
       `to!ulong("+")`/`to!size_t("-1")`/`to!size_t("-0")` 报 `Unexpected '+'`/`'-'`；
    3. 比目标范围大的量值抛 **`std.conv.ConvOverflowException`**
       `"Overflow in integral conversion"`（不是 `ConvException`）。
    窄类型还有第 4 条不对称，**目前无端口站点需要，只记录不实现**：`short/byte/ubyte` 的
    end-of-input 与前导字符错误报**基元类型**（`short`→`int`、`ubyte`→`uint`），
    而尾部垃圾错误报**目标类型**（`short`/`ubyte`）。
  - **`formattedRead` 同规则**（`probes/substrate/probe_format_int_target.d` →
    `substrate_format_int_target.txt`，**26 行可复现**）：std.format 的数字读取把 token
    交给 std.conv，所以 `%d` 读进 `size_t` 也说 `ulong`、读进 `long` 说 `long`。
    另有两个**只属于 std.format** 的形状：输入在字面量匹配前就耗尽 →
    `std.format.FormatException "parseToFormatSpec: Cannot find character 'n' in the input string."`
    （不是 conv 错误），`%d` 读 `"0x10"` 会吃掉 `0` 把 `"x10"` 留在流里、值为 0，
    **十六进制在这条路上到不了 conv 的语法**。G4 的 `sg.frmt.*`（语料行 1096–1113）必须按此实现。
  - **入库**：`substrate/include/eil/conv.hpp` 把扫描器改成带 `tname`/`allow_sign` 参数的
    `parse_int_strict`，新增 `eil::to_size_t`，`eil::to<integral>` 按 `d_int_type_name<T>()`
    选名字；`usgrid.cpp` 11 处、`sgrid.cpp` 1 处 `to!size_t` 站点改用 `to_size_t`。
    **改前先数过依赖**：`to_long` 只在 sgrid/usgrid 两个尚未注册的模块里用，全部语料里
    `converting from type string to type` 只出现在这两个包（`int`×19、`ulong`×6），
    所以改动不影响已注册门禁；改后全量 **100/100** 复跑确认。
  - **Wave 3+ 会再撞上**：任何把整数读进 `size_t`/`uint` 的文件解析代码（`gas/species.fld`
    读取、`fluidblock` 头、Lua 配置里的整数强转）。规则：**看 D 源码写的目标类型名，
    不看端口辅助函数的名字**。
- [ ] **T-926** substrate 缺口 **3 条**（T-019 的 sketch 作业报出；按纪律它不许改 substrate）：
  1. **✅ 已完成 2026-10-15**（见下方"完成"段）。**`eil::to_string` 不派发类的 `toString()`**。D 的 `to!string(x)` 对带 `toString()` 的
     类型会调它，而 `substrate/include/eil/format.hpp:139` 对非算术类型直接 `static_assert`。
     sketch 的站点只能手写。**这条最要紧**：`gas/`(Wave 3)、`fluidblock`(Wave 4) 与求解器
     日志（Wave 5）都在大量把对象塞进消息文本，逐点手写会繁殖成几十处。
     **规则已实测**（`probes/substrate/probe_tostring_dispatch.d` →
     `substrate_tostring_dispatch.txt`，**25 行、重跑 0 差异、行内无地址**）：
     带 `toString()` 的 struct **和** class 都用它（`Both` 那种"无参 + 带 fmt"双重载里
     **无参那个赢**）；**没有** `toString()` 时两者不同——struct 走字段dump
     `NoToString(1, 2.5)`，class 走**全限定类型名** `probe_tostring_dispatch.Bare`
     （**不是**字段 dump、**也不是**指针地址，所以这行可复现）；`null` 类引用 → `null`；
     enum → 成员名；数组 → `[e1, e2]` 且**元素照常派发**；AA → `[["a":1]]`；
     只有 `toString(string fmt, Args...)` 而没有无参版的 struct **不**被 `to!string` 使用
     （实测落回字段 dump）；`format("%d", obj)` 对带 `toString()` 的类型直接
     `FormatException "Expected '%s' format specifier for type 'Simple'"`。
     **待实现**（实现要重建 `eil_geom`，与在飞的 agent 撞，故先落证据）：
     substrate 用 ADL/`decltype` 探测 `t.toString()`，命中就调用；
     字段 dump 与"类名 dump"C++ 无反射，**只能逐类手写 `toString()`**——
     所以这条的实现价值在"让调用点写 `eil::to_string(x)` 就能派发"，
     而不是自动 dump。已手写 `toString()` 的端口类（`USGCell`、`Vector3` 等）正好可直接受益。
     **完成**（提交 `7a964f0`，详见 [task-log.md](task-log.md#t-926-1)）：`format.hpp` 新增
     `has_noarg_to_string`（void_t 探测**零参** `toString()`，与既有 range trait 同法），`to_string`
     在非算术分支的 `static_assert` **之前**加一条：命中就 `return v.toString()`。实测的四条规则都钉进
     门禁 `substrate_tostring_dispatch`（回放 T-019 的 25 行语料，**12 行逐字节核对、13 行需反射的跳过并点名、0 差**）：
     struct **和** class 的无参 `toString()` 都派发；**无参优先于** `(fmt, Args…)`（`struct.both`→"plain"）；
     只有 fmt 版的 struct **不**派发（用编译期 `static_assert(!has_noarg_to_string<FormatToString>)` 锁死）；
     容器仍走 `[a, b]` dump 并**逐元素派发**（`to_string(Simple[])`→"[Simple(1), Simple(2)]"）、`"%s"` 同文本。
     trait 已随 `to_string` 一起 `using` 重导出到 `eil::`。**113/113 三配置全绿**，探针重跑逐字节相同，
     `check_staleness` 无漂移。第 2 条（`Slice` 动态数组默认实参）与第 3 条（`readln`，与 T-923 同一
     EOF-返空语义）**未做**，留 Wave 3 前定形第 2 条、Wave 4/5 补第 3 条。
  2. **`eil::Slice<T>` 形参无法表达 D 的动态数组默认实参**（D 写
     `double[] canvas_mm = []` 这类默认值）。sketch 用 3 个委托重载局部绕开了；
     局部可用 ≠ 通用可用，`geom/` 之外还会有。
  3. **没有 `readln()`**。sketch 只在**未编译的 `with_libplot` 支**里用到，本波无需求；
     但要注意它与 T-923 是同一语义问题：D 的 `readln()` 到 EOF **返回空串、不抛异常**，
     已被 `nm_stmatrix_fails` 语料钉住。将来补 `readln()` 必须照这个实测行为，
     不要按 C++ `getline` 的失败流状态直觉写。
  （代理还报了第 4 条"没有 libplot 声明"，那与既有 **T-921** 同源，不另列。）
  → **Wave 3 之前**定形第 1 条（`gas/` 是第一个大量把对象写进消息的包）
- [x] **T-928** substrate 缺口：**没有 `std.conv.ConvOverflowException`，整数扫描也没有范围检查**。
  由 G5 的代理按纪律报出（它不许改 substrate），T-927 的实测表已把规则钉死。现状是**两份互不相同的
  本地替身**，都在 `src/`：
  1. `src/geom/grid/usgrid.hpp:96` 定义了真正的 `ConvOverflowException`（派生自
     `eil::ConversionException`），配 `usgrid.cpp:300` 的 `d_to_int`——**逐位累积时立刻查界**，
     超界报 `"Overflow in integral conversion"`。这条是被语料钉住的：
     `geom_grid_usgrid.txt` 行 208（`new USGFace("9999999999999999999 1")`）报的就是这个类，
     且**必须在 `vtx_id_list.length = …` 分配之前**报（否则变成 OutOfMemoryError，行 207）。
  2. `src/util/lua_service.hpp:166` 的 `to_int_from_double` 没有那个类，直接 `ExceptionBase`，
     消息也不同：`"Conversion positive overflow"` / `"Conversion negative overflow"` /
     `"Input was NaN"`。
  **关键：两者消息不同不是不一致，而是两条不同的 D 路径**——①是 **string→整数**，②是
  **数值→整数**（T-927 两份探针分别量到 `…type ulong` 与 `Overflow in integral conversion`）。
  所以实现时必须**按来源分派**，不许合并成一个通用 overflow 文本；把 ①的范围检查塞进
  `parse_int_strict`（`substrate/include/eil/conv.hpp:266`）即可，随后删掉 `d_to_int` 与
  `usgrid.hpp` 的本地类。②那三行文本**保持现状**，只在 substrate 有了类之后把
  `ExceptionBase` 换成 `ConvOverflowException`（**消息一字不改**）。
  → **实现窗口**：改 substrate 会重建 `eil_geom`，与在飞的 grid agent 撞，故与 T-926 第 1 条
  一起在无编译冲突的窗口做；做完必须三配置复跑并给数字（当前 100/100）。
  **2026-10-14 全树扫了一遍"端口自己写的 conv 文本"，第三条替身与一条门禁盲区**
  （扫法：`grep -rn 'unexpected_char\|unexpected_end\|converting from type\|Overflow in integral\|Input was NaN\|Conversion positive overflow' src/ substrate/`，
  排除 substrate 自己那两个文件）：
  3. **`src/util/time_utils.cpp:54` 的 `to_int_d` 是第三个严格整数扫描器**（另两个是 substrate 的
     `parse_int_strict` 和 `usgrid.cpp:300` 的 `d_to_int`），三处文本各写一遍。
     它按 D 的 `to!int`（已核：`time_utils.d:31/32/33/37` 全是 `to!int`）**类型名 `int` 是对的**，
     但**类是 `ExceptionBase`**，而 D 抛 `std.conv.ConvException` 与超范围的
     `ConvOverflowException`。
  4. **这条为什么没人发现——注册门禁本身看不见**：`tests/test_util_time_utils.cpp:31-32`
     只有两个种类标签 `kErrorPrefix="Error"` / `kConvPrefix="Exception"`，
     `expect_throw` 是 `catch (ErrorBase&) / catch (ExceptionBase&)` 二选一
     （`ConversionException` 派生自 `ExceptionBase`，所以**抛哪个都进同一支**），
     它比的是 `.msg` 逐字符，**类名从来没进过断言**。⇒ 与 T-922 完全同形：
     "门禁只比 msg"就会让类名错误长存。
     **做完 T-928 必须顺手把这条断言加粗**：让 `expect_throw` 记录端口自己那个
     "应当等于 D 类名"的字符串（对照 `substrate_throw_identity.txt` 的实测类名），
     否则下一次还是没人看得见。
  5. 全树扫描的其余命中都是**有语料钉住的**：`svgpath.cpp:76/:83`（`geom_gpath_svgpath` 注册通过）、
     `usgrid.cpp:304/311/315`（`geom_grid_usgrid` 行 208 钉住类与文本）、
     `sgrid.cpp:165/167` 只是注释。**没有第四处。**
  **完成 2026-10-15**（提交 `fef8d4c`，详见 [task-log.md](task-log.md#t-928)）。
  `ConvOverflowException` 进 substrate（`error.hpp`，`ConversionException` 的子类），
  `parse_int_strict` 改成**按目标类型**的模板、逐位查界、用 `make_unsigned_t<T>` 累积
  （于是 `ulong` 自己的最大值 `18446744073709551615` 能转、不再溢出有符号累加器），
  有符号两端不对称（`int` 收 `2147483647`/`-2147483648`、拒 `2147483648`/`-2147483649`）。
  **三个 string→int 扫描器收敛成一个**：`usgrid.hpp` 的本地类 + `usgrid.cpp` 的 `d_to_int`
  删除（17 处改走 `eil::to_int`），`time_utils.cpp` 的第三个扫描器 `to_int_d` 也删除。
  两条 D 路径**按来源保持分立**：string→int 报 `"Overflow in integral conversion"`；
  数值→int 保留它自己的三条文本（`"Conversion positive/negative overflow"`、`"Input was NaN"`），
  所以 `lua_service` 的 `to_int_from_double/long` 与 `meshpatch` 的 `d_to_int(double)`
  **只换类、消息一字不改**。④的断言已加粗：`test_util_time_utils` 的 `expect_throw`
  现在断言 **D 类名**（`object.Error`/`std.conv.ConvException`/`std.conv.ConvOverflowException`），
  `meshpatch` harness 改为**先于父类**捕获 `ConvOverflowException` 而非按消息反推。
  新门禁 `substrate_conv_int_typename` 回放 T-927 语料（**67 行核对、29 行窄类型跳过并点名、0 差**）——
  此前**无门禁**正是范围检查缺失能长存的原因。**112/112 三配置全绿**，探针重跑逐字节相同，
  `check_staleness` 无漂移；判别力已用"关掉范围检查看 5 条 overflow 行 + 2 条 time_utils 行转红
  （`2147483648` 回绕成 `-2147483648`）"验证。**遗留**：`substrate_format_int_target.txt`
  （`formattedRead "%d"` 那条路，含 `FormatException`/hex 特有形状）仍**未接门禁**——
  它属 T-011 G4 的格式读取器、不是本条的 string→int；本轮改动使该路的 overflow 也走
  `eil::to_int` 因而**更贴参照**，但专门的回放留待格式读取器那条线。
- [x] **T-929** substrate 缺陷 **2 条**，都在 `substrate/include/eil/array.hpp`，由 G4 的代理报出
  （它按纪律不许改 substrate），两条都有语料行做证据：
  1. **`set_length` 把新增尾部填成 0，D 填 `T.init`**（`array.hpp:162-170` 是
     `new (p_ + i) T()`；D 的 `arr.length = n` 对 `double` 填 **NaN**）。
     证据：`geom_grid_sgrid.txt` 行 **757–760**（`sg.rgrid.identity.s row[*] 0 nan nan 1`
     ——那两个 `nan` 就是 D 里**从未被写过**的槽位）。
     现在的处置是**测试侧**补：`tests/test_geom_grid_sgrid.cpp:816` 的 `d_init_doubles(n)`
     把 harness 自己构造的**输入**数组按 D 的 init 填满。这**不是**为了对上期望输出而改结果，
     而是复刻 D 探针源码 `double[16] id; id[2]=…` 那种"只写一部分"的输入——所以门禁仍然有效。
     **但端口侧的真实缺口还在**：任何 `Array<double>` 在 `set_length` 之后读未写槽位的地方，
     D 得 NaN、端口得 0.0。实现就是 `new (p_ + i) T(eil::d_init<T>())`
     （`substrate/include/eil/number.hpp:58` 已经有 `d_init`，注释里本来就要求"成员默认值写
     `eil::d_init<number>()` 而不是 0.0"——数组增长这条漏了同一条规则）。
     **做完必须**：删掉 `d_init_doubles`，让 harness 走生产路径，再复跑全量（当前 100/100
     里凡是 `.length =` 之后读尾部的语料都可能因此**从假绿变红**——那才是真的）。
  2. **`set_length_signed`（T-914）只拦负数**；一个**很大的正数**（D 里同样分配不出来）
     在端口里走的是 `Array` 自己的 `"Array allocation failed"` + 自己的 `__FILE__/__LINE__`，
     而 D 是 `core.exception.OutOfMemoryError "Memory allocation failed"` @
     **`rt/lifetime.d(1454)`**（实测见 `substrate_throw_identity.txt`）。
     ⇒ 端口现有**两份翻译替身**：`sgrid.cpp:238-245` 的 `d_set_length` 与
     `usgrid.cpp:292` 的 `set_length_d`（后者注释里已写明是给 `us.sgbad.zeroj` 那行用的）。
     实现应把这层收进 substrate（`set_length` 分配失败即报 D 的文本与 D 的**运行时位置**），
     然后删掉两份替身。
  → 与 T-926 第 1 条、T-928 同批做（都要重建 `eil_geom`）；**做完的判定不是"能编过"**，
  而是三配置复跑 + `d_init_doubles`/`d_set_length`/`set_length_d` 三个替身**都从树里消失**。
  **完成 2026-10-14**（提交 `dde23b3`，详见 [task-log.md](task-log.md#t-929)）。两条都收进
  substrate，三个替身全部从树里消失（`d_init_doubles`/`d_set_length`/`set_length_d`），
  51 处调用点直接走 `.set_length`。先量后写：`probes/substrate/probe_array_init.d`（30 行、
  两跑逐字节相同）实测 `new double[n]` 与 `.length = n` **都**给 `0x7ff8000000000000`(NaN)，
  struct 数组给逐字段 `T.init`、class 数组给 `null`；`set_length_signed(-1)` 与 `reserve` 失败
  都抛 `core.exception.OutOfMemoryError "Memory allocation failed" @ rt/lifetime.d(1454)`。
  usgrid 的 5 条 OOM 语料行此前显示端口的模块位置、现在由 harness 从异常读 `file()/line()`
  修正为 D 运行时位置。**111/111 三配置全绿**，`substrate_array_init` 逐行复刻 30 行基线 0 差，
  `check_staleness` 无漂移；判别力已用"把 `grow_init` 退回 `T()` 看两道门禁转红"验证。
- [ ] **T-930** **substrate 缺口（Wave 3 起就是硬依赖）：Phobos 的 `to!double` 十进制→二进制**不是正确舍入的**。
  由 G6a 的 `sgrid_demo` 产物门禁暴露（`cmake/wave2_grid_g6.cmake` 里那条"10 of 16 rows
  reproduce"的注释），主线程**自己复测确认**：
  `probes/substrate/probe_to_double_rounding.d` → `substrate_to_double_rounding.txt`
  （34 行、两跑逐字节相同、行内无地址），同一批 token 用 g++ -O0 打 `strtod` 的位型对比，
  **15 个数值 token 里 2 个差 1 ULP，且 D 是错的那一方**：
  `0.2929550221030571` → D `0x1.2bfc66bc86beep-2`（bits `3fd2bfc66bc86bee`）、
  strtod/`eil::to_double`/Python `float()` 都给 `…bed`；`2.929550221030571e-1` 同样差 1 ULP
  （说明指数写法走的是同一条路）。其余 13 个（含 `0.1`/`0.3`/`1e308`/denormal/
  `…5712345` 长位串）**位型全同**。
  - **为什么它现在就要记下来**：这些值不经过任何几何运算——`import_gridpro_grid`
    （sgrid.d:1603）只做 `uflowz(scale * to!double(tks[m]))`、`scale=1.0`，
    所以差异**完全来自解析**。而 Wave 3+ 的入口全是解析：Lua 配置里的浮点字面量、
    `.fld`/`.dat` 表、`fluidblock` 头、网格文件。**只要用户写下一个 D 会误舍的十进制，
    端口就给另一个 double**，之后每一步都跟着错。
  - **纪律**：忠实复刻优先于"更正确"。本波要求是"行为与 D 求解器一致、不得优化"，
    所以 `eil::to_double` 必须复刻 Phobos 的算法（`std/conv.d` 的 decimal→binary 快速路径），
    **不能**停在 strtod。T-913 只管了它的**错误文本**，没管**舍入**。
  - **实现前必须先扫范围**：写一个 `probes/substrate/probe_to_double_range.d`，
    把 Phobos 路径与 strtod 在**大量随机/边界十进制串**上比位型，量出分歧率与触发条件
    （位数？是否含指数？尾数超过 19 位？），再决定是复刻整条快速路径还是
    只在"会分歧的类"上走复刻。**先量后写**，这是本仓库反复验证过的顺序。
  - **门禁状态**：`demo_equiv_geom_grid_sgrid` 已注册且绿（26 行 stdout 在 `%.6g` 下看不出差）；
    `demo_artefacts_geom_grid_sgrid` **故意没注册**——16 行里 6 行不行，
    5 行是本条、1 行是 G6b 未落地（`test_gmo_grid-2D.vtk`）。
    注册条件写死在 `wave2_grid_g6.cmake` 的注释里。
- [ ] **T-931** `geom/grid/paver.cpp` 的**内部铺面算术**与参照差 1 ULP 级（不是格式化差异）。
  由 `paver_demo` 的产物暴露：`paved_grid.vtk` **字节长度相同（141,490）**，
  但 **398/1490 个顶点行的末位不同**，第一处差异在第 235 行（顶点 228）
  `6.036702247749276516e-03` vs 端口 `6.036702247749274781e-03`，全文件最大相对差 **4.3e-13**。
  **定位是量出来的**：前 **214** 个顶点逐字节相同，而 214 正是 D 基线第 2 行说的
  "there are 214 points in the external boundary" ⇒ `construct_boundary` 与 gpath 的弧/线采样
  与参照一致，分歧在 **paver 的内部铺面**里。
  **为什么现有门禁没抓到**：`geom_grid_paver` 语料 **1961/1961 逐字节相同**——它数的量
  覆盖不到这条路径（与 T-923"没有语料的路径能藏住东西"同一类，只是这里藏的是数值末位）。
  **待做**：给 paver 的内部点补语料（按 `%.17g` + 位型），先复现"214 之后才开始差"，
  再定位到具体表达式；候选是 T-920 的 `cos/sin` 融合、T-909 的常量折叠、以及
  迭代里 `a*b+c` 的融合次序。**不许**为了产物一致而调容差。
  `demo_artefacts_geom_grid_paver` 因此在 `wave2_grid_g6.cmake:112-119` 注释着，
  stdout 门禁（129/129）照常注册且绿。
- [x] **T-932** **`to!int(double)` 的上界是"严格大于 `INT_MAX`"，端口按 `>=` 写会把最大合法 int 拒掉。**
  **完成 2026-10-14**。G6b 代理先在自己 helper 里发现（它把 `v >= 2147483648.0` 改成
  D 的判据），我复测时确认 **`src/util/lua_service.hpp:168` 的 `to_int_from_double` 同样错**，
  而且**它上面那行注释本来就写着 "exclusive of the endpoints"** —— 代码与自己正确的注释相反。
  - 实测（`probes/substrate/probe_to_int_from_double.d` →
    `substrate_to_int_from_double.txt`，**16 行、重跑逐字节相同、无地址**）：
    `2147483647.0` **通过**；`2147483647.5` 与 `2147483648.0` → `ConvOverflowException
    "Conversion positive overflow"`；`-2147483648.0` **通过**；`-2147483648.5` → negative 变体；
    **NaN 走的是 `ConvException "Input was NaN"`，不是 overflow**（两类不同）；
    `±inf`/`1e30` 都归到 positive/negative overflow；截断方向是**向零**
    （`3.9→3`、`-3.9→-3`、`-2147483647.9999→-2147483647`）。
  - **为什么这条影响用户**：`lua_service` 是作业脚本参数进求解器的路。`>=` 写法的后果是
    一个把 `maxIterations = 2147483647` 写进 Lua 配置的用户**在这里死掉，而参照接受**。
    边界值不是理论点：它就是 `INT_MAX` 本身。
  - **修**：`src/util/lua_service.hpp` 一处 `>=` → `>`（下界本来就写对了）。
    同时确认**没有任何注册用例把错误行为钉住**
    （`test_util_lua_roundtrip.cpp:628` 的 `"2147483647"` 是**字符串**→int，走另一条路；
    `test_util_time_utils.cpp:62` 同理），所以这次改动不会把绿测改红；
    集成后 default **110/110**，Release 与 slice-check 另计。
  - **推广出的检查**（值得写进每个简报）：端口里所有手写范围判断，**逐个问"边界值本身
    D 收还是拒"**。`>=`/`>` 差一个字符，语料通常不采那个点。候选清单：
    `to_int_from_long`（已核：用的是 `>`，正确）、各 `enforce`/`clamp` 站点。
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
