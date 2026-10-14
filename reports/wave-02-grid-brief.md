# T-011 分派简报（geom/grid，每个子代理必读）

先读 `reports/wave-02-brief.md`（通用纪律、映射表、实测钉死的语义）和
`reports/wave-02-surfvol-brief.md`（§1 虚函数契约的**更正**：`classString()` 只在
`geom/gpath` 有，surface/volume/grid 没有；别照旧写法标 `override`）。
本文件只补 geom/grid 特有的一切。**强制顺序**：先编译、先抓 D 语料入库、再写实现。

## 0. 范围与依赖次序（实测依赖，不是猜的）

| ID | D 模块 | 行数 | 依赖 | 说明 |
|---|---|---|---|---|
| G1 | `grid/grid.d` | 490 | elements/gpath、`Quaternion`（已转）、gzip | **自洽**，不引用 surface/volume；`Grid` 是 sgrid/usgrid 的基类，必须先落地 |
| G2 | `grid/paver.d` | 1737 | G1（`PavedGrid : Grid`） | 用 `Arc3`/`Vector3`/`Grid`；**不**用 surface/volume（实测 `grep -oE "ParametricSurface\|ParametricVolume" paver.d` 为空） |
| G3 | `grid/paver2d.d` | 52 | G1 | `enum NodeType`、`fill_interior`；D 原文自己写着 "Cheat …temporary arrangement"，**照抄不优化** |
| G4 | `grid/sgrid.d` | 1865 | G1 + **T-012 全部 surface/volume** + `nm.nelmin` + gzip | 构造器签名是 `const ParametricSurface`/`const ParametricVolume`（`sgrid.d:82,104,116,595,620`）→ **必须等 T-012 完成** |
| G5 | `grid/usgrid.d` | 2524 | G1 + G2 + G3 + `geom.misc.kdtree` + gzip | `import geom.grid.paver: PavedGrid, POINT_LIST, FACE_LIST, CELL_LIST` 选择性 import；`paver2d = geom.grid.paver2d` 改名 import |
| G6 | 4 个 demo + `gmopatch` + `meshpatch` | 115+51+101+103+171+100 | G1–G5 | `gmopatch.d:87` `new StructuredGrid(...)`、`meshpatch.d:23` 入参 `ref StructuredGrid` → 这两个是**从 T-012 移过来的**，必须 sgrid 之后 |

**已定顺序**：G1 → (G2, G3) → G4 → G5 → G6。G2/G3 可并行。

## 1. 已经存在的可用件（别重复实现）

- `src/geom/elements/{vector3,properties,projection,nomenclature}.{hpp,cpp}` — 含 `Quaternion`
- `src/geom/gpath/*` — 13 个模块全绿（nurbs 195/195、svgpath 818/818、gpath_utils 564/564）
- `src/geom/misc/{univariatefunctions,svg,kdtree,nurbs_utils}` — kdtree 是 usgrid 的依赖
- `src/nm/{spline,splinelsq,nelmin}`、`src/extern/gzip/gzip` — sgrid 用 nelmin 与 gzip
- `src/geom/surface/parametricsurface`、`src/geom/volume/parametricvolume`（基类）
  + 15 个已转子类（见 backlog T-012 条目）

## 2. grid 特有的硬约束

1. **gzip 比对的是解压后的 payload，不是容器字节**（容器含 mtime，D 自己两次运行都不同，
   `reports/phase-0.md` 记过）。`sgrid.d:1623` 的 `pipeShell("gunzip -c " ~ fileName)`
   走 `popen`；`.gz` 写出后要比**解开的文本**。
2. `Grid` 的抽象方法表（`grid.d:95-122`）是 sgrid/usgrid 的实现契约，
   `read_from_gzip_file`/`write_to_gzip_file`/`write_to_vtk_file`/
   `get_vtx_id_list_for_cell`/`vtk_element_type_for_cell`/`get_cell_type`/
   `get_boundary_grid`/`get_list_of_boundary_cells`/`number_of_vertices_for_cell`
   一律纯虚，**签名照抄，不要"顺手"加 const 或改返回类型**。
3. `grid.d:37` `throw new Error("Unknown type of grid: " ~ name)` 是 **Error 不是 Exception**
   （本端口 `catch (ExceptionBase)` 抓不到 `ErrorBase`，两者是兄弟）。
   grid 里所有 `throw new Error(...)` 都按 `eil::ErrorBase` 转，消息文本逐字节照抄。
4. `sort_cells_into_bins`（`grid.d:277`）、`find_enclosing_cell_fast/slow`（`:320,`:355）、
   `get_bin_ix/iy/iz`（`:248-276`）是一组**分箱加速**逻辑，索引算术的取整方向、
   边界 `<`/`<=` 都是可观测行为 → 逐句转写，语料必须覆盖 bin 边界与越界点。
5. `point_is_inside_cell`（`:179`）依赖 `geom.elements` 的 `projection`/`properties`，
   已逐位验证过，直接调用，不要重写判定。
6. `rotate(Quaternion q, ...)`（`:443`）与 `rotate(double theta, Vector3 axis, ...)`（`:481`）
   两条旋转入口都要转；`Quaternion` 在端口是 `std::array<double,4>`，
   D 的 `alias Quaternion = double[4]` 默认值是四个 NaN（`d_init`），不是零。
7. **有符号数组长度**：D 的 `arr.length = L` 里 `L` 可能是负数，
   `eil::Array::set_length(size_t)` 表达不了（T-914）。grid 里若遇到，
   按 `src/geom/surface/bezierpatch.cpp:26-39` 的既有做法在**转换之前**抛
   D 的 `Error "Memory allocation failed"`，并在头注释指向 T-914，**不要**改 substrate。

## 3. 每个模块的交付件（缺一不可）

1. `src/geom/grid/<module>.hpp`（+ `.cpp`）——顶部注释 `<d 路径> -> src/<路径> (module <d.module>)` + 非 1:1 之处
2. `probes/geom/grid/probe_<module>.d` —— 头部写可复跑抓取命令
3. `baselines/e587b235/probes/geom_grid_<module>.txt` —— **先入库再写实现**
4. `tests/test_geom_grid_<module>.cpp` —— D `unittest` 逐条断言 + 语料逐行回放，
   汇总行给"N of M 行字节相同 / 多少量逐位相同 / 最坏残差与归因"
5. demo → `tests/demo_geom_grid_<module>.cpp`，stdout 与 D 字节相同
6. 追加**你自己那一节的** `cmake/wave2_grid_gN.cmake`（已建好空文件，`if(EXISTS)` 自注册）
7. `provenance/fidelity.yml` 追加条目 → `python3 tools/gen_manifest.py` → `check_staleness.sh` 须 `no drift`

## 4. 抓取 D 语料

```bash
cd /home/tang/packages/Eilmer
export PATH=/home/tang/packages/dlang/bin:$PATH
# /tmp/gdtk_shadow 是 gdtk/src 的副本（删了 @nogc，因 ldc2 1.40 拒 @nogc 里的 new）
ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/grid \
  probes/geom/grid/probe_<module>.d -of=/tmp/probe_gN -od/tmp/probe_obj_gN
/tmp/probe_gN > baselines/e587b235/probes/geom_grid_<module>.txt
```
`gdtk` 树**只读**。

## 5. 构建自检

```bash
cd /home/tang/packages/Eilmer && rm -rf /tmp/w2_gN && cmake -S . -B /tmp/w2_gN \
  -DCMAKE_BUILD_TYPE=Release && cmake --build /tmp/w2_gN --target <你的 targets> -j3
cd /tmp/w2_gN && ctest -R "<你的测试名正则>" --output-on-failure
```
再跑 Debug 与 `-DEIL_SLICE_CHECK=ON`。**机器只有 16 核且并行有别的代理，用 `-j3`；
只构建你自己的 target**，别人半成品文件里的编译错误不是你的任务。

## 6. 报告纪律

不允许只写"通过"。每个结论带产生它的数字。发现 substrate 缺口 → 写进最终报告
交主线程登记 T-9xx，**不要自己改 `substrate/`、`CMakeLists.txt`、`reports/backlog.md`、
`reports/task-log.md`、别人的模块文件或别人的 cmake 片段**。

## 7. G2（paver）恢复作业的**实测起点**（2026-10-14，主线程核对）

前一个代理在 150 轮上限处被打断，产物**不在构建图里**，全部在 `wip/geom-grid-paver/`：

| 文件 | 行数 | 状态 |
|---|---|---|
| `wip/geom-grid-paver/paver.hpp` | 268 | 声明齐（30 个 D 成员都在） |
| `wip/geom-grid-paver/paver.cpp` | 1,720 | 按 D 顺序写的实现（D:21→D:1731），**未经语料验证** |
| `wip/geom-grid-paver/test_geom_grid_paver.cpp` | 624 | 只写到 `sec3.ipr.*` 一节 |
| `wip/geom-grid-paver/wave2_grid_g2.cmake.register` | — | 现成的注册片段 |

**停在哪一行，是被量出来的**：`ctest:geom_grid_paver` 当前只发出 **219 行**，
其中 **217 行字节相同**，最后一行是基线第 219 行 `sec3.ipr.emptyrow|0`，
下一条应为 `make.npoints|4`。⇒ **缺的是测试侧各节的生成代码，不是 paver.cpp 的算术**。
基线语料共 **1,961 行**，分组统计（`awk -F. '{print $1}'`）：

```
pave 922 | node 169 | sec2 131 | seam 127 | row 122 | smk 90 | seam2 62
sec3 47  | cls  40  | make 31  | + D 侧 writeln 文本若干组（14/11/10/10/8/8/8/8 …）
```

**作业协议（必须照做，否则又是一次白干）**：

1. 先把四个文件移回 `src/geom/grid/`、`tests/`，**但先不要注册测试**。
   `cmake/wave2_grid_g2.cmake` 的注册是 `if(EXISTS tests/…)`，所以测试文件一落地就会红。
   约定：**只有当整条语料 0 差异时才写注册片段**；未完成的实现可以留在 `src/` 里编译
   （编译不产生红测试），但门禁必须始终 93/93 全绿。
2. 按语料**分组顺序**一节一节补测试生成代码：`make.*` → `cls.*` → `node.*` → `row.*` →
   `pave.*` → `seam.*`/`seam2.*` → `smk.*` → 剩余 `writeln` 文本组。每补一节就
   `diff` 一次，把"本节第一处不同的行号"记进最终报告——**不要**攒到最后一次性比对。
3. 语料是**全或无**的门禁（逐行字节相同），没有"先放过一半"的中间态。
   如果某一节确实做不到，**停下来报告**：给出该节行号、D 值、端口值、你已排除的假设，
   把现状留在 `wip/`，**绝不**放宽比较、改基线、或按容差放行。
4. `paver.cpp` 里那套**本地** `sin`/`cos` volatile 规避（头注释 8-18 行）**删掉**，
   改用 substrate 的 `eil::sin`/`eil::cos`（T-920 已入库）。同理成对 `^^` → `eil::pow`、
   `atan`/`atan2` → `eil::atan`/`eil::atan2`。这些都是已钉死的规则，不要再造副本。
5. `paver.d` **不**依赖 sgrid/usgrid（实测 `grep -n "StructuredGrid\|UnstructuredGrid" paver.d`
   只在 :145 的一条注释里提到 `USGCell_type`），所以 G2 与 G4/G5 无顺序关系。

## 8. G4/G5 的真实依赖（更正第 0 节表格的暗示，2026-10-14 实测）

`sgrid.d` 与 `usgrid.d` **互相引用**，不是单向：

- `usgrid.d:553` `this(const StructuredGrid sg, const string new_label="")` — usgrid 用 sgrid 类型
- `sgrid.d:1316` `auto usg = new UnstructuredGrid(this);`（`write_to_su2_file` 内）— sgrid 用 usgrid 类型
- 两者都通过 `import geom;`（`geom/grid/package.d` 同时 `public import` 二者）拿到对方的名字

⇒ 在 C++ 里必须**成对**做，头文件互相前向声明、`.cpp` 里再 include 对方；
单独先做 G4 会留下一个"要等 G5 才能验证"的函数体。安排时把 G4+G5 当一个工作包，
或先做 G4 但把 `sgrid.d:1316` 那条路径标成"待 G5 验证"并**不注册**相关语料行。
`usgrid.d` 还额外 `import geom.grid.paver`（选择性 import `PavedGrid, POINT_LIST, FACE_LIST, CELL_LIST`）
→ **G5 必须等 G2 落地**。

## 9. G4（`sgrid.d` 1865）与 G5（`usgrid.d` 2524）开工前必读（2026-10-14 主线程实测）

### 9.1 语料的**陈旧状态**——先修它，再写实现
`baselines/e587b235/probes/geom_grid_sgrid.txt` 现在是 **2,079 行**，但
`probes/geom/grid/probe_sgrid.d`（1,673 行，**能编、能跑，rc=0**）当场重跑产出
**2,097 行**。`diff` 只有**一个 hunk**：`1096,1113d1095`，即前一次作业在**抓完基线之后**
又给探针加了 18 行 `sg.frmt.*`（坏格式头、空版本串、双空格版本号一类的解析失败路径），
**没有来得及重抓基线**。该 hunk 是**纯插入**，原 2,079 行**一行未变**
（这很重要：说明重抓是安全的，不是"基线漂移"）。
⇒ **开工第一步**：按 §4 的命令重抓 `geom_grid_sgrid.txt`（2,097 行），
自己再跑第二遍确认逐字节相同，然后才动 C++。任何"先照着 2,079 行写、回头再补"的做法都会
把 18 行解析失败路径写丢——而那 18 行恰好是本模块最容易漏的异常文本。

### 9.2 G5 **还没有任何语料**
`baselines/e587b235/probes/` 里**没有** usgrid 相关文件，`probes/geom/grid/` 里也**没有**
`probe_usgrid.d`。⇒ G5 必须先写 D 探针并抓基线，才允许碰 `src/geom/grid/usgrid.*`。

### 9.3 实测构造密度（决定语料必须覆盖什么）
| | sgrid.d | usgrid.d |
|---|---|---|
| `^^` | **15** | 0 |
| `atan2` | 1（:1294） | 0 |
| `sin`/`cos` | 2/2（**:1299/:1304 同自变量成对**） | 0/0 |
| `to!double` | **13** | 6 |
| `to!string` | 3 | **29** |
| `assert` | 32 | 42 |
| `throw` | **42**：29 `new Error` + 12 `GeometryException` + 1 `Exception` | **28**：8 `new Error` + 19 `GeometryException` + 1 `Exception` |
| `unittest` 块 | 1 | 1 |
| `.dup` | 1 | **19** |
| `iota` | 1 | 0 |
| `File(`/`gzip`/`pipeShell` 站点 | 21 | 21 |
| `version(...)` | 0 | 0 |

由这张表直接得出的硬要求：
1. `sgrid.d:1299` 与 `:1304` 是 `r*cos(theta…)` 与 `r*sin(theta…)` **同一自变量成对**
   → 必须走 **`eil::cos`/`eil::sin`**（T-920：两个编译器都从 -O1 起融合成 glibc
   `sincos()`，1 ULP，约每 1466 个分量一次）。`:1294` 的 `atan2` → **`eil::atan2`**
   （T-910：Phobos 自带实现，与 glibc 在轴附近差 1 ULP，过零放大成 2.9e-09 相对）。
2. 15 处 `^^` → **`eil::pow`**，**不得展开成乘法**（`(1.0-r)^^3` 这类三次曲线插值权重
   是逐位可观测的）。
3. 29 + 8 = **37 处 `throw new Error`**：按 **T-922** 实测，D 的裸 `object.Error`
   `.file==""`、`.line==0`，所以端口必须写 `eil::ErrorBase(msg)`**不带** `__FILE__/__LINE__`；
   而 31 处 `GeometryException`（`geometry_exception.d:11` 的默认实参是
   `file=__FILE__, line=__LINE__`）**要**带抛出点。同一文件里两种规则并存，
   逐站点看 D 的类，不要按端口方便统一。
4. 74 处 `assert` → `eil_assert` / `eil_assert_msg`（T-918：D 的 `AssertError.msg`
   分别是 `"Assertion failure"` 与**只有消息本身**，没有条件文本、没有类名前缀）。
5. 19 处 `to!string` 与 13+6 处 `to!double`：前者是 `%.6g` 语义、后者由 **T-913**
   按 443 个操作数重测过 7 条规则（`+nan` 可转但 `+inf` 不可、`1e` 与 `1e+` 的文本不同、
   `_` 只在紧跟数字时才是分隔符、hex 尾数后 `p` 必需……）。**都是异常/文本可观测点**，
   语料必须覆盖到出错分支，不能只走 happy path。
6. `gzip`/`pipeShell` 各 21 处：比的是**解压后的 payload**，容器字节含 mtime，
   D 自己两次运行都不相同（§2 第 1 条；`sgrid.d` 用 `pipeShell("gunzip -c …")` 走 popen）。

### 9.4 与 G3 已落资产的**对齐点**（别把已绿的东西改红）
`src/geom/grid/paver2d.hpp` 现在是**模板 + 前向声明**的写法，因为当时 usgrid 不存在：
它的测试 fixture 手抄了 `usgrid.d:112`（`makeFaceTag`）、`:154`（`USGFace`）、
`:218`（`USGCell`）、`:40-56`（`USGCell_type`），并且**两种容器写法**
（`eil::Array<std::shared_ptr<USGFace>>` 与 `eil::Array<USGFace*>`）都实例化并验证过
结果一致（`geom_grid_paver2d` 480/480 行 + 396/396 坐标）。
⇒ G5 落地时必须：①`USGFace`/`USGCell`/`USGCell_type`/`makeFaceTag` 的**真实定义**要与
fixture 逐字段一致，或改 fixture 并**重跑 `geom_grid_paver2d`** 证明仍是 480/480；
②把 `fill_interior` 从"模板 + 前向声明"收敛回 D 的非模板签名
（`ref USGFace[] faces` 等），收敛时**不得**改变任何已验证数值；
③删掉 paver2d.hpp 里那条"G5 未转"的说明段，换成指向真实定义。
这三步是 `ctest:geom_grid_paver2d` 能不能继续算证据的前提。

### 9.5 顺序
`paver`(G2，进行中) → `usgrid`(G5，先写探针) → `sgrid`(G4) 或 G4 与 G5 同人成对做
（§8 的双向耦合）→ G6：`sgrid_demo`(101)、`usgrid_demo`(103)、`paver_demo`(115)、
`paver2d_demo`(51) 的 D 基线**已全部抓好在仓库里**（`geom_grid_paver2d_demo.txt` 98 字节、
`geom_grid_paver_demo.txt` 5,097 字节 + `paved_grid.vtk` 141,490 字节等），
再加 `gmopatch.d`(171)、`meshpatch.d`(100)。

## 10. G4+G5 工作包（`sgrid.d` 1865 + `usgrid.d` 2524）——必须同人同批做完

### 10.1 为什么不能拆成两个作业
双向依赖是**实测**的，不是猜的：
- `usgrid.d:553` `this(const StructuredGrid sg, const string new_label="")` —— usgrid 的构造器
  要读 sgrid 的真实成员；`:2492` 的 D `unittest` 里还 `new StructuredGrid(my_patch, 11, 21, cf)`。
- `sgrid.d:1313-1318` `write_to_su2_file` 的**函数体就是纯委托**：
  `auto usg = new UnstructuredGrid(this); usg.write_to_su2_file(fileName, scale, use_gmsh_order_for_wedges);`
  ⇒ sgrid 语料里的 `sg.su2.len / sg.su2.scaled.len / sg.su2.3d.len`（各带完整 body，
  最长一行 1,529 与 1,959 字节的 SU2 文本）**必须等 usgrid 才能复现**。
- `usgrid.d:26` 选择性 `import geom.grid.paver: PavedGrid, POINT_LIST, FACE_LIST, CELL_LIST`，
  并在 `:522/:531/:535/:540` 使用——**paver 已落地（G2，1961/1961）**，这三个列表在端口是
  `src/geom/grid/paver.hpp:195-197` 的 `inline eil::Array<...>`（C++17 inline 变量，
  跨 TU 唯一实例，对应 D 的模块级可变全局）。**不要**在 usgrid 里再声明一份。

打破循环的写法：两个 `.hpp` 互相**前向声明**，`.cpp` 里再 `#include` 对方
（sgrid.cpp 需要 `UnstructuredGrid` 的完整类型来 new 它；usgrid.cpp 需要 `StructuredGrid`
读 `niv/njv/vertices`）。头文件里只出现指针/引用形参。

### 10.2 语料现状（一个已就绪，一个完全没有）
| | 探针 | 基线 | 状态 |
|---|---|---|---|
| G4 sgrid | `probes/geom/grid/probe_sgrid.d`(1,673 行，`ldc2 -O0` 可编可跑) | **2,097 行** | **主线程已重抓并提交**（commit `de88552`：原基线 2,079 少 18 行 `sg.frmt.*`，纯插入、旧行一字未变，D 连跑两次自比相同） |
| G5 usgrid | **无** | **无** | **第一步必须是写探针 + 抓基线**，不许先写 C++ |

usgrid 探针要覆盖的最小集合（按 §9.3 的密度推）：42 个 assert 的两条形（有/无消息）、
28 个 throw（8 `new Error` / 19 `GeometryException` / 1 `Exception`，见 §9.3 表）、
29 个 `to!string`（**是 `%.6g` 语义**，见 wave-02-brief 第 5 节）、19 个 `.dup`
（D 的类 `dup()` 是深拷贝语义，逐个测它拷了什么）、`:2492` 那个 `unittest` 的全部内容、
以及 `write_to_su2_file` 的**双向**结果（usgrid 直接写 + 由 sgrid 委托来写，两者应逐字节一致）。

### 10.3 这一包里必须用的既有规则（都已入库，别再发明）
- **`x ^^ 0.5` → `eil::sqrt(x)`，不是 `eil::pow(x,0.5)`**（T-925，语料 `substrate_pow` 的
  `half` 组按位型钉住；判别证据是 `-0.0` 的符号）。sgrid/usgrid 里若有 `^^0.5` 站点照此写。
- **成对的 `cos(x)`+`sin(x)` → `eil::cos`/`eil::sin`**（T-920）。**sgrid 已实测命中**：
  `sgrid.d:1299` 与 `:1304` 就是 `r*cos(theta…)` 与 `r*sin(theta…)` 同自变量成对
  （`makeWedgeGrid`），另有 `:1294` 的 `atan2` → **`eil::atan2`**（T-910）。
- **带符号数组长度 → `eil::Array::set_length_signed(long)`**（T-914，已进 substrate）。
  sgrid 的用户输入维度直接进 `length =`，`sg.frmt.nivbig err=Memory allocation failed`
  这一行就是它；**不要再写第六个本地 `set_length_checked`**。
- **`throw new Error(...)` 的站点不带 `__FILE__/__LINE__`；`GeometryException` 带**
  （T-922 实测表，规则正文在 `substrate/include/eil/error.hpp` 头上）。
  这一包里的数量不小：sgrid 29 个裸 Error + 12 个 GeometryException，
  usgrid 8 个裸 Error + 19 个 GeometryException。**逐站点按 D 的类决定。**
- **`assert` 用 `eil_assert` / `eil_assert_msg`**（T-918：D 的 `.msg` 分别是
  `"Assertion failure"` 和**只有消息本身**）。
- **`to!double` 的错误文本**按 T-913 的 7 条规则（语料 `substrate_conv_failtext` 443 行）；
  sgrid 的 13 处 `to!double` 全在 gzip 文件解析里，出错分支必须被语料覆盖
  （`sg.frmt.*` 那 18 行就是干这个的）。
- **`.gz` 比解压后的 payload**（§2 第 1 条）；`sgrid.d:1623` 的 `pipeShell("gunzip -c …")`
  走 popen。
- **常量折叠**（T-909）：sgrid/usgrid 里凡是"字面量算式当常量用"的地方，
  语料要同时给出折叠值与运行值（本轮已见两次：`sketch.d:128` 折叠≠运行、
  `paver` 的 `node.tol*` 用折叠值），别假定两者相同。

### 10.4 不许把已经绿的改红
`geom_grid_paver2d` 现在是 **480/480 行 + 396/396 坐标**，它的测试**手抄**了
`usgrid.d:112/154/218/40-56` 的类型定义当 fixture（见 `paver2d.hpp` 的 MEASURED DEPENDENCY 段）。
G5 落地时 `USGFace`/`USGCell`/`USGCell_type`/`makeFaceTag` 的**真实定义**必须与那个 fixture
逐字段一致；若你不得不改 fixture，**改完必须重跑 `geom_grid_paver2d` 证明仍是 480/480**，
并在报告里给出这个数。同时把 `paver2d.hpp` 里"G5 未转"的说明段与 `fill_interior` 的模板化
收敛回 D 的非模板签名——收敛是**机械**的，任何数值变化都是缺陷。
`geom_grid_paver`（1961/1961）与 `geom_grid_grid`（643/643）、`geom_grid_nanadd`（638/638）
同样不许变红。

### 10.5 注册与收尾纪律（与 §7 相同，再强调一次）
- 门禁是全或无：**只有整条语料 0 差异才写 `add_test()`**。
  `cmake/wave2_grid_g4.cmake` / `wave2_grid_g5.cmake` 已建好并被 `include()`，
  里面用 `if(EXISTS)` 自注册——**先别放测试文件进去**，或放了就把注册留在 0 差异之后再开。
  当前基线：**100/100（Release / Debug / slice-check）**，`check_staleness.sh` = `no drift`。
- 语料/探针**不许为了通过而改**。如果你认为 D 侧某行是参照的 bug，保留原样、写进报告。
- 做完后跑**三配置**并给数字；`provenance/fidelity.yml` 与 `manifest.yml` 由主线程写，
  你只需在报告里给出该记的实测数字。
- **如果时间/轮次不够**：停下来报告"usgrid/sgrid 各自到哪一节、语料第几行、
  D 值 vs 端口值、已排除的假设"。主线程按 `paver` 的先例接着做——
  **明确的部分结果 + 诚实的停止点**，比一个假的绿有用得多。

## 11. G4/G5 分段进度表（2026-10-14 主线程实测，按语料行号）

**G5 现状**：`tests/test_geom_grid_usgrid.cpp` 已能发出语料 **1–1252 行，1252/1252 逐字节相同**
（`/tmp/w2_g5r/test_geom_grid_usgrid <corpus>`，摘要行
`probe replay 1393 D lines, 1252 C++ lines, 141 differ; 1252 of 1393 rows byte-identical`）。
1253 之后**全部是 `<none>`**——不是数值不对，是**回放组还没写**。
这一段红过一次：1170–1172 三行的类型名是 `int` 而 D 报 `ulong`，根因与规则见 **T-927**
（已修，`eil::to_size_t`）。1190–1252 落地时又抓到一个**真端口缺陷**：
`read_from_vtk_text_file` 为 D 的一行 `label = f.readln().strip()`（D:1791）读了**两行**，
于是后面的 `"ASCII"` 扫描整体晚一行——1199/1201/1203 三行全报
`Did not find ASCII while reading VTK grid file`。空白网格的行（304/305/310）当时是绿的，
**只有真文件才会暴露"读了几个换行"这类错误**。

**G4 现状**：`tests/test_geom_grid_sgrid.cpp` 从零建成，已发出 **1–830 行、830/830 逐字节相同**
（`probe replay 2097 D lines, 830 C++ lines, 1267 differ; 830 of 2097 rows byte-identical`）。
两段落地各抓到一个真缺陷（`sgrid.cpp`）：①`toString()` 的 D:228 两个下标**求值顺序**——D 从左到右，
gcc 的函数实参从右到左，空格点上报了 `vertices[$-1]` 的回绕下标
`18446744073709551615` 而 D 报 `index [0] … length 0`（行 58）⇒ 手工定序；
②`measure_of_badness` 把 D:1336 的 else 分支**写错了下标方向**（`this[i,j-1,k]` 而非
`this[i-1,j,k]`），`j==0` 时 size_t 回绕、把 D:1337 的步子提前触发（行 703）。
D:1337 自身那个 `b` 写成而不是 `c` 的笔误**照抄不改**（语料在数这个值）。
另报出 **T-929** 两条 substrate 缺口（数组增长填 0 而非 `T.init`；`set_length_signed` 只管负数）。
harness 的行文本用的是 sgrid 探针自己的 `tryit/tryit_cut`（`err=`/`exc=`，**语料里 0 个 `class=`/`dline=`**），
与 usgrid 的 `tw/twd` 不同——照抄 usgrid 的格式会全线红。

**关键事实（省掉下一位的一段弯路）**：`src/geom/grid/sgrid.cpp`（1875 行）与
`src/geom/grid/usgrid.cpp`（2883 行）**函数层面已经转完**——D 侧 31 个 sgrid 成员函数在
C++ 里全部有对应实现（`single_cell_index`/`oldSingleIndex` 是 lambda，
`determine_rs_grids` 的嵌套 `objective` 是 `Objective` 仿函数，见 `sgrid.cpp:1345/:1525`）。
所以 **G4/G5 剩下的工作是写回放 harness，不是转写求解器**；harness 暴露出来的才是真 bug。

### 11.1 G5 剩余分段（语料行号，必须按序接在 `grp_su2rd()` 之后）

| 行 | 组 | D 代码 | harness 里对应要mirror的 probe 段 |
|---|---|---|---|
| ✅ 1190–1205 | `grp_vtk()` | `write_to_vtk_file` D:1850 / `read_from_vtk_text_file` D:1785 | `probe_usgrid.d:2232-2296` |
| ✅ 1206–1216 | `grp_stl()` | 继承的 `Grid.write_to_stl_file` grid.d:389 | `:2297-2317` |
| ✅ 1217–1252 | `grp_foam()` | `write_openFoam_polyMesh` D:1930-2162 | `:2318-2379` |
| 1253–1267 | `grp_imp()` | import 构造器 D:830-846 + 其余够不到的 throw/assert | `:2380-2447` |
| 1268–1282 | `grp_idx()`+`grp_conn()` | 抽象表下标 D:988-1010 / `get_connected_cell_ids_for_cell` D:1045 | `:2448-2479` |
| 1283–1300 | `grp_sort/bnd/blc()` | D:888 / D:1057 / D:1106 | `:2480-2538` |
| 1301–1316 | `grp_vaf/split/link()` | D:1133/:1167 / D:1179 / D:1258 | `:2539-2576` |
| 1317–1337 | `grp_join/stats()` | `joinGrid` D:2164 六个 assert + D:2342 outsign / `writeStats` D:2416 | `:2577-2634` |
| 1338–1347 | `grp_vtxb/bin/edge()` | D:2450 / grid.d bin sort / D:881 下溢 | `:2635-2676` |
| 1348–1360 | `grp_unit()` | usgrid.d 自带 unittest D:2484-2524 | `:2677-2718` |
| 1361–1393 | `grp_gap()` | 其余够不到的站点 | `:2719-end` |

### 11.2 G4 分段（`geom_grid_sgrid.txt` 2097 行；harness 从零开始，第一段还要建骨架）

| 行 | 内容（D 段） | 备注 |
|---|---|---|
| ✅ 1–459 | `ctor`(71) `idx`(67) `opidx`(47) `cell`(106) `abst`(124) `str`(5) `tag`(13) `dup`(26) | **含 harness 骨架**：`tw/twd`、`classify`、`emit`、`dump_*`，照 `test_geom_grid_usgrid.cpp` 的同一套行格式，否则格式差会伪装成数值差 |
| ✅ 460–830 | `sub`(68) `bnd`(100) `blc`(6) `path`(29) `surf`(89) `rgrid`(79) | `sg.surf/vol` 要 `ParametricSurface`/`ParametricVolume`，G3 已注册可用 |
| 831–1173 | `vol`(114) `readtxt`(84) `wgz`(5) `readgz`(59) `frmt`(18) `wraw`(4) `readraw`(50) `vtk`(11) `su2`(3) | **`sg.frmt.*` 必读 T-927**：`%d` 读进 `size_t` 报 `ulong`、读进 `long` 报 `long`；输入提前耗尽是 `FormatException`（`parseToFormatSpec: Cannot find character …`）而不是 conv 错误。**顺一条实测**：sgrid.d 全部 10 处 `formattedRead`（:861/:867/:869/:871/:873/:875/:896/:910/:914/:933）的目标类型只有 `string`/`int`(dimensions, grid.d:43)/`size_t`(niv·njv·nkv·ntags)/`double`(x,y,z)——**没有 `long`**，所以端口里 `sgrid.cpp:248` 与 `usgrid.cpp:338` 那两个 `fmt_into<long>` 特化是**死代码**，下次碰到这两个文件时删掉即可，别照着它们加特化 |
| 1174–1594 | `join`(97) `slab`(62) `wedge`(74) `bad`(9) `rs`(57) `determ`(52) `rotq`(44) `rot`(101) | `determ` 走 `nelmin.minimize`（已注册）；`wedge` 有 T-920 的 `cos/sin` 成对与 T-910 的 `atan2` |
| 1595–2097 | `uflowz`(25) `vtkhdr`(9) `gridpro`(37) `rsgp`(31) `fswap`(41) `plot3d`(100) `ccp`(19) `pic`(32) `bin`(156) `fsc`(26) `unit`(8) | `plot3d` 的 `pipeShell("gunzip -c …")` 走 popen；`bin`(156) 是全语料最大的一段 |

### 11.3 派发纪律（本轮踩过的坑，写给下一个主线程）

- **一次只放一个 agent 碰 `eil_geom`**。sgrid.cpp 与 usgrid.cpp 编进同一个库，两个 agent
  并行必然互相看到对方半成品（G5 早期已经发生过一次文件被并写弄坏）。
- **数字自己复跑**。agent 报"1–946 全同"时真实情况是"发出的 1189 行里有 3 行不同"——
  只有 harness 的摘要行可信。
- **`add_executable` 常开、`add_test` 常关**，直到整条语料 0 差异；这样半成品继续编译、
  套件保持 100/100，进度可交接。

