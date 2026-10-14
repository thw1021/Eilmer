# Wave 02 报告（进行中）— 几何库 geom/ 与 gasdyn/ 迁移

参考：gdtk `e587b235`（ldc2 1.40.0）。产物：`/home/tang/packages/Eilmer`，git 逐阶段提交。
范围：geom 非 luawrap 部分 + gasdyn 非 Lua 部分。Wave 2 的实范围与排除理由见
`reports/backlog.md` 顶部"范围修正"段；本报告只记**已完成的阶段**和**当前数字**，
所有待办项一律指回 `reports/backlog.md`，不在这里另立清单。

> 逐任务的详细过程记录在 `reports/task-log.md`（T-010、T-017、T-014、T-013、T-910、
> T-015/stage A、T-013续/stage B/T-913 各一节）。本报告是阶段汇总视角。

## 1. 已完成

### 1.1 规模（截至本次提交，实测 `find`/`wc`）

| 指标 | 值 |
|---|---|
| 已转写 C++ 模块头文件 | **75** 个（`find src -name '*.hpp'`） |
| C++ 源码行（src + substrate） | **29,376** |
| 测试/demo 行 | **30,337** |
| D 对照语料总行数 | **84,656**（`cat baselines/e587b235/probes/*.txt`；含本轮新增的
  sin/cos 415、stmatrix 失败路径 7、throw 身份 15、sketch 682 与四处补档的孤儿测量） |
| CTest | **100 个测试：Debug 100/100、`-DEIL_SLICE_CHECK=ON` 100/100、Release 100/100**（本轮新增 7 条：`geom_grid_paver` 1、sketch 5、`nm_stmatrix_fails` 1；另有 `-flto -O3` 档单跑 grid/sin_cos 全绿）。（上一版此处记的是"87 个、Release 86/87、故意留红"——红行即 T-919，现已闭合；`geom_grid_grid` **643/643**、`geom_grid_nanadd` **638/638**、`substrate_sin_cos_pair` **415/415** 在四档构建一致） |
| 溯源 | `provenance/manifest.yml` 每条带 `d_blob`；`tools/check_staleness.sh` → `no drift` |
| gdtk 树写入 | **0**（探针一律编 `/tmp/gdtk_shadow` 副本） |
| 不在构建图内的半成品 | **无**（`wip/` 已清空删除：G2 在 1961/1961 的门禁下落地，见 `task-log.md`） |

转写清单（本 Wave）：
- `geom/elements/`：vector3、properties、projection、nomenclature + geometry_exception
- `geom/gpath/`：path、line、arc、bezier、polynomial、polyline、helix、modifiedpath、
  xspline、xsplinelsq、gpath_utils、nurbs、svgpath（13/13）
- `geom/misc/`：univariatefunctions、svg、kdtree、nurbs_utils（`libplot` 按变体推迟；
  `sketch` 另立 T-019）
- `geom/surface/`：parametricsurface、bezierpatch、coonspatch、channelpatch、cubepatch、
  sweptpathpatch、beziertrianglepatch、subrangedsurface、ruledsurface、
  nozzleexpansionpatch、nurbssurface（11 个已绿；spherepatch/aopatch/controlpointpatch
  与 `surface_demo` 在转；`gmopatch`/`meshpatch` 改挂 T-011）
- `geom/volume/`：parametricvolume、meshvolume、subrangedvolume、slabvolume、
  twosurfacevolume（5 个已绿；sweptsurfacevolume/wedgevolume/tfivolume/nurbsvolume
  在转）
- `gasdyn/`：gasflowexception、idealgasflow、idealgasflow_demo
- 前置件：`nm/{spline,splinelsq,nelmin}`、`extern/gzip`

### 1.2 逐模块对照结果

**全部基准都是 ldc2 1.40.0 现场编译的 D 二进制**，逐行字节比对；下表数字直接取自测试
自己的 stdout，不是转述。

| 分组 | 结果 |
|---|---|
| geom/elements | Vector3 53 行 diff 空；properties **366/366** 逐位；projection **88/88**；nomenclature **97/97**；demo 字节相同 |
| geom/gpath | path 799/799、line 1381/1381、bezier 1759/1759、polynomial 1892/1892、helix **1603/1603**、polyline 1987/1987、xspline 2256/2256、xsplinelsq 1979/1979、arcsteps 418/418 全逐位相同；arc 1884/1886（2 行走绝对带，worst 7/8 ULP）；**modifiedpath 10794/10794 逐位、0 行用带**；nurbs **655/655**；svgpath **2802/2802**；gpath_utils **957/957** + demo 43/43 字节相同 |
| geom/misc | svg 15/15 数值逐位；nurbs_utils 267/267；kdtree 1697/1697；univariatefunctions 1501/1543（42 行差 ≤1 ULP，归因到 Phobos/glibc 的 exp/tanh/sinh） |
| geom/surface + volume（stage B） | 15 个模块 **19,995 行语料 0 行差异、0 行用容差带**（逐模块行数见 task-log） |
| gasdyn | **39,778/40,451** 量逐位、673 个在带内、**0 个越带**、59/59 断言、demo 字节相同 |
| 前置件 | spline 156、splinelsq 340、nelmin 24、sort 6,889 排列、gzip 66 均 0 差异 |
| substrate | fmt 语料 786/786；pow 26/26；iota 31/31；**`to!double` 新语料 443/443** |

### 1.3 门禁可信度——两处主动自证，不是"信自己的绿"

1. **T-910 闭合**：`modifiedpath` 曾有 **84 行靠绝对容差带放行**、最大残差 8.88e-10、
   打印比值 3.0e6 ULP。没有调常数，而是把 `rotate_about_zaxis` 拆成 5 个算子、
   每个同时打 `%.17g` 与 **IEEE-754 位型**（`probe_mp_rot_chain.d`，324 行），
   同一份 C++ 语料跑两遍分别用 `eil::atan2` 与 `std::atan2`：
   **Phobos 复刻 0 行差异 / glibc 29 行差异**，第一处不同就是 `atan2` 本身。
   根因：`vector3.cpp:313` 用了 glibc `atan2`，而 D 的 `vector3.d:523` 是 Phobos 的
   `atan2Impl!double`；1 ULP 角度误差落在 `PI/2` 的 cos 零点 → `cos` 相对变化
   2.88e-09 → `d2pdt2` 再除以 `dt²` → 8.88e-10。修法是把 arc-tangent 上移 substrate
   成 `eil::atan/atan2`、三个站点全改；**84 条带 → 0，10794/10794 逐位**。
2. **语料确有判别力**（beziertrianglepatch 代理做的改坏实验）：删掉 D:104 的
   `if (w<0.0) w=0.0` → **21/268 行变红**；把 D:112 远端边界 `-1` 放宽成 `0` →
   未捕获异常 exit 134。
3. **门禁自身的假证据也被修掉**：`tests/gpath1_dump.hpp:331` 有 4 个转换符只传 3 个实参，
   每行汇总的 `(D …)` 打的是栈垃圾（曾打 `D 9.8e+252`，真值 1.2650977300272355）。
   报告里一个编造的数字比不报数字更坏。

### 1.4 反向修正：实测纠正了 4 条"从语言语义推出来"的规则

| 条目 | 错在哪 | 现在的口径 |
|---|---|---|
| `to!double` 错误文本分支（T-913） | 从**一条**语料行推广，`#c` 就错了 | 按 **443 个操作数**重测后重写；测出 7 条 D 特有语义（`+nan` 可转而 `+inf` 不可、只匹配三字母 `inf`、`1e` 与 `1e+` 文本不同、`_` 分隔符的精确位置、hex 尾数遇 `p` 即停、溢出才是 Range error…）；操作数与结果全 hex 编码，因为 D 只转义 `\n` 不转义 `\t` |
| surface/volume 虚函数契约 | 我把 `classString()` 写了进去 | 它**只存在于 geom/gpath**（`grep -c` 在 surface/volume 全 0）；简报已更正 |
| `beziertrianglepatch` 是 `^^` 大户 | 我说它重度用 `^^` | 只有 3 处且全是**整数臂**；26 个 `iota` 全整数降序，累加写法恰好同值 |
| ldc2 常量折叠暴露面为 0（T-909） | 先前"grep 声明处命中 0"的检索漏了函数体内字面量除法 | `svg.d:55` 的 `90.0/25.4` 就是实例，折叠值与运行值差 1 ULP；改用折叠出的十六进制字面量后 157 行 0 差异 |

## 2. 与 D 非 1:1 之处

全部登记在 `provenance/fidelity.yml` 并在对应 task-log 小节解释。本 Wave 新增的类别：
`adapted-handles`（D 类引用→`shared_ptr`，含 `dup()` 返回基类句柄）、
`adapted-arrays`（`T[]`→`Array`/`Slice`；有符号 `arr.length = L` 走 substrate 的
  `eil::Array::set_length_signed`，T-914 已定形，先前散在两个 surface 模块里的 5 个本地副本已删）、
`deferred`（`getJSONVector3` 依赖 `std.json` → T-902）。

保留而未"修正"的 D 怪癖（每条带 file:line）：`vector3.d:137-143` 参数遮蔽成员导致
`add()` 无副作用；`properties.d:662-671` 坍缩钳位在 `/= volume` 之前；
`projection.d:17` 出参进入即 NaN；`bezierpatch.d:40` 复制留 `m=other.n` 而 `dup()` 重算；
负数组长度必须在转 `size_t` **之前**变成 D 的 `Error "Memory allocation failed"`；
`twosurfacevolume.d:110` 用字面量 `1.0-1` 缩放 face0 因而"i"支丢弃它；
CoonsPatch 角点检查只报第一个开角；`channelpatch` 零叉乘得 `[pA,pA,pB,pB]` 而非 NaN。

**D 侧真实崩溃按原样保留**（T-915）：`slabvolume.d:32` 在 `super()` 赋值成员之前调
继承的 `face0123(0.0,0.0)`，实测 SIGSEGV / exit 139 / 无可捕获对象，且全树无调用者
→ 该拷贝构造在端口 `= delete` 并写明判据；`sweptpathpatch.d` 的 null 面同理不加防护。

## 3. 待完成

**一律见 `reports/backlog.md`（唯一权威清单）**。本 Wave 尚未闭合的 ID：
T-011（geom/grid：G1 `grid.d` 490、G3 `paver2d.d` 52、**G2 `paver.d` 1737** 三条已闭合
（G2 于本轮从 219/1961 的归档状态推到 **1961/1961、0 差异**，且**探针与基线全程未改**
`sha256 d96d146f…`）；余 **G4 `sgrid.d` 1865 + G5 `usgrid.d` 2524**——这两条是一个工作包，
因为 `usgrid.d:553` 的构造入参是 `const StructuredGrid`、而 `sgrid.d:1313-1318` 的
`write_to_su2_file` 是**纯委托**给 `new UnstructuredGrid(this)`，
sgrid 语料里的 `sg.su2.*` 行必须等 usgrid 才能复现；usgrid 还需要已落地的 paver
（选择性 import `PavedGrid, POINT_LIST, FACE_LIST, CELL_LIST`）；
另余 G6 四个 demo（`sgrid_demo` 101、`usgrid_demo` 103 未抓；
`paver_demo` 115、`paver2d_demo` 51 的 D 基线已在库中）+ `gmopatch.d` 171 / `meshpatch.d` 100
（两者都要 `StructuredGrid` 类型，随 sgrid 之后））、
**T-019 已闭合**（`sketch.d` 964 + `sketch_demo.d` 83：语料 682 行 0 差异、
251/251 数值逐位、demo stdout 2 行 + 三个 SVG 文件逐字节；主线程独立复抓确认）、
**本轮新立并已闭合：T-920**（成对 `sin`/`cos` 被**两个编译器**都融合成 `sincos()`，
415 角语料里 **401 行随参照系自己的 -O 移动**，规则上移 substrate）、
**T-922**（端口给 **84 处** D 侧本不带抛出点的裸 `new Error` 编了 `__FILE__/__LINE__`；
**83 处已改，剩 1 处是正确的**——`final switch` 越界抛 `SwitchError`，它带位置。
核对用的是**括号配对逐调用计数**，先前用行窗口数出的 82/42 是错的，真数是 84/39，
差额正是跨行调用）、**T-923**（`stmatrix` 读取失败路径**一条语料都没有**，因而藏着两条
凭空造的异常文本；补 7 行语料并做过判别力验证）、**T-925**（更正本仓库自己的规则：
`x^^0.5` 是 `eil::sqrt` 不是 `eil::pow`，判别证据是 `-0.0` 的符号）、
**T-914**（有符号数组长度的 5 个本地副本收进 substrate）。
**仍开着：T-926**（sketch 报出的 3 条 substrate 缺口：`to!string` 不派发 `toString()`、
`Slice` 形参表达不了 D 的动态数组默认实参、没有 `readln()`）、
T-921（本机无 GNU libplot，D 侧连二进制都产不出来）
**T-920 已闭合**（本轮实测新立、本轮内修完：`cos(x)`+`sin(x)` 对被 **两个编译器**都从 -O1
起融合成一次 glibc `sincos()`，与两次独立 libm 调用差 1 ULP；415 角语料里
**401 行随参照系自己的 -O 移动**，故契约仍是 ldc2 -O0 的"两次独立调用"。
修法上移 substrate 成 `eil::sin`/`eil::cos`，改道 7 个源文件 + 9 个测试文件
（改道前 Release 有 **11 个 .o**、其中 **4 个是测试自身**引用 `sincos`，改道后 **0**），
门禁 `substrate_sin_cos_pair` 415/415 四档一致，代价 +22% 每对——
见 `task-log.md` T-920 节）。
仍待启：**T-016**（本报告收尾 + `wave-02` tag）、**T-019**（`geom/misc/sketch.d` 964
及其 3 个配套程序，依赖 T-011/T-012 的具体子类，故只能在两者之后）、
以及横切债务 T-902/903/904/905/907/909/911/912/914/915（各钉到自己的波次）。
**T-012 与 T-910/911/913/917/918/919 已闭合**（T-012 的核对方式与余留说明见
`task-log.md` 同名节：`gdtk/src/geom/{surface,volume}/*.d` 逐个查同名 `.cpp`/`.hpp`，
缺失只剩 `package.d` 聚合文件、已转成 demo_equiv 的两个 demo、以及划归 T-011 的两个 patch）。

## 4. 门禁状态

| 门 | 状态 |
|---|---|
| (1) 快照逐元素 ≤1e-13 | harness 就绪；基线 **8** 案例（新增 `3D/sod-shock-tube/usg`）；**今日复跑** `gate.sh` 以 D 二进制当候选 → **8/8 PASS**，可比产物共 **351** 个（cone20-new-flow-format 29、cone20-sg-implicit 51、cone20-sg 32、piston-in-tube-1-block 38、piston-simple 34、steepening-wave-ldfss2 133、sod-sg 17、sod-usg 17），onlyD=0、onlyC=0 |
| (2) 残差/气动力/步数 | 同上，`config/*-residuals.txt`、`loads/` 在基线内 |
| (3) 70 个 Ruby 判例 | **仍不可执行**：需候选 `e4shared`，最早 Wave 5 之后 |

T-015 第一半实测：`gate.sh` 以参考侧 D 二进制当候选跑 `3D/sod-shock-tube` 的 sg/usg 两条链，
**各 17 个可比产物、onlyD=0、onlyC=0、全 PASS**（sg `final-t= 0.00060314`，
usg `0.000600988`；gzip 只比解压后 payload：sg `flow 181,027 / grid 68,303`，
usg `flow 181,024 / grid 145,581` 字节）。第二半"C++ 侧可跑"需 Wave 5 的候选二进制，
故 T-015 保持 `[~]`，不用 Wave 2 拿不到的证据去销它。
