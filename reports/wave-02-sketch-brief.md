# T-019 分派简报（`geom/misc/sketch.d` + `sketch_demo.d`，子代理必读）

先读 `reports/wave-02-brief.md`（通用纪律、映射表、已钉死的语义）。
本文件只补 sketch 特有的一切，且**全部是主线程实测**，不是推测。
**强制顺序**：先编译、先抓 D 语料入库、再写实现。

## 0. 范围（2 个模块，别再扩）

| D 模块 | 行数 | 产物 |
|---|---|---|
| `geom/misc/sketch.d` | 964 | `src/geom/misc/sketch.hpp/.cpp` + `probes/geom/misc/probe_sketch.d` + `baselines/e587b235/probes/geom_misc_sketch.txt` + `tests/test_geom_misc_sketch.cpp` |
| `geom/misc/sketch_demo.d` | 83 | `tests/demo_geom_misc_sketch.cpp` + `baselines/e587b235/probes/geom_misc_sketch_demo.txt`（stdout）与三个 `.svg` 产物基线 |

**不含** `rotating_a.d`/`drifting_eye.d`/`libplot.d` —— 它们因本机无 GNU libplot 而
**连 D 侧二进制都产不出来**，已单独挂 **T-921**，不要试图为它们造桩。

## 1. 依赖已经是齐的（实测，别再去等 T-011）

- `grep -cE "sgrid|usgrid|StructuredGrid|UnstructuredGrid|geom\.grid" sketch.d` = **0**。
  sketch.d 的函数体不碰网格。它 `import geom;`（:36）只是包聚合器，为了拿
  `Vector3`/`Arc`/`CoonsPatch` 这些名字——**这些在端口全部已转并全绿**。
- `import ntypes.complex;`（:29）**无条件**存在，但 `Complex` 在全文件只出现 1 次，
  就是那行 import 本身 ⇒ 无实际依赖，**不要**因此等 T-904。
- `import nm.number;`（:30）→ `src/nm/number.hpp` 已有。
- `import geom.misc.svg;`（:32）→ `src/geom/misc/svg.{hpp,cpp}` 已有且 157 行语料全绿。
- `version(with_libplot)` 共 **19 个 version() 站点**（import 在 :33-34，函数体在 :62, :152,
  :178, :261, :292 …）。按约定**原样保留但不编译**：用 `#ifdef EIL_WITH_LIBPLOT` 包住，
  并在头注释写明变体名。**不要删**，也不要把 `Renderer.xplot` 分支"顺手"改成报别的错。

## 2. 这个模块的两条已钉死规则会**同时**命中同一处 —— 必须先量

`sketch.d:128-129`（`Projection.isometric` 分支）：

```d
double cos30 = cos(30.0/180.0*PI);
double sin30 = sin(30.0/180.0*PI);
```

一个自变量的 `cos` 与 `sin` **成对出现**，而参数是**字面量算式** ⇒ 两条已实测的规则同时生效：

1. **T-920（成对融合）**：g++ 与 ldc2 都从 -O1 起把 `cos(x)+sin(x)` 融合成一次 glibc
   `sincos()`，与两次独立 libm 调用差 1 ULP。**必须写 `eil::cos(...)` / `eil::sin(...)`**，
   不要写 `std::cos/std::sin`。判据与复跑命令在 `substrate/include/eil/math.hpp` 的
   sin/cos 块和 `reports/task-log.md` 的 T-920 节。
2. **T-909（常量折叠）**：ldc2 折叠出的浮点常量**不是**正确舍入的 double，
   与同一表达式运行期差 1 ULP（已实测形状：`90.0/25.4`、`4.184/1.0e-2`、`0.45-2.54*0.5`）。
   ⇒ **必须实测**：`30.0/180.0*PI` 在 ldc2 里是折叠成常量还是运行期算？
   折出的位型与运行期位型是否相同？`sketch.d:136` 的 `1.0/(2.0*sqrt(2.0))` 同问。
   把两侧位型（`%016x`）写进探针语料，**不要靠推理选一个**。
   若发现折叠≠运行期，照 `geom/misc/svg.d` 的先例处理：端口里写入 **D 实际取到的那个**
   十六进制字面量，并把这条补进最终报告交主线程登记（**不要自己改 `substrate/`**）。

`sketch.d` 其余相关构造实测计数：`^^` **0**、`atan/atan2` **0**、`to!string` **3**、
`to!double` **0**、`assert` **0**、`iota` **0**、`.dup` **0**、`foreach(ref` **0**、
`format/writef` **2**、`throw` **8**、**`unittest` 0**。
⇒ **本模块没有 D 单元测试可转**，门禁完全靠探针语料 + demo 字节比对。
`to!string(double)` 的语义是已测过的 `%.6g`（见 wave-02-brief 第 5 节），3 处站点直接用
`eil::to_string`，别自己拼 `std::to_string`（那是 `%.17g` 风格，文本会错）。

## 3. `throw` 的 8 个站点分两类，类型不能混

| D 行 | 抛出类型 | 端口写法 |
|---|---|---|
| :109 `Unknown renderer: ` ~ name | `Exception` | `throw eil::ExceptionBase(msg, __FILE__, __LINE__)`（既有写法见 `src/geom/misc/svg.cpp:50`） |
| :119 `Unknown projection: ` ~ name | `Exception` | 同上 |
| :754 `invalid direction for rule: ` ~ direction | `Exception` | 同上 |
| :162 `Couldn't create X Plotter.` | `Error` | `eil::ErrorBase`，**仅消息、无 file/line**（已实测：裸 `object.Error` 的 `.file==""`、`.line==0`，见 `provenance/fidelity.yml` 的 grid 一节） |
| :180 `Couldn't delete X Plotter\n` | `Error` | 同上；**注意消息末尾那个 `\n` 是要照抄的字节** |
| :165 `You asked for xplot renderer but libplot is not included.` | `Exception` | 这条在 `version(with_libplot)` 的 **else** 分支，**是会编译到的**，不是变体死码 |
| :262 / :292 `Couldn't open/close X Plotter.` | `Error` | 在 `with_libplot` 内，不编译，但原样保留 |

**没有 `EIL_THROW_*` 宏给 misc 用**（substrate 里只有 `EIL_THROW_FLOW`，那是
`FlowSolverException` 的）。misc 的既有写法是直接 `throw eil::ExceptionBase(...)`。
另注：`src/geom/misc/univariatefunctions.cpp:223-230,274` 那批 `ErrorBase` **带了**
`__FILE__/__LINE__`，与上面"裸 `object.Error` 无 file/line"的实测不一致——
**不要照抄那批**，按实测写；如果你要动的站点与它们相邻，把这条发现写进最终报告，
由主线程决定是否统一（这是既有一致性问题，不是你要顺手修的）。

`Exception` 与 `Error` 在本端口是**兄弟不是父子**（`catch (ExceptionBase)` 抓不到
`ErrorBase`）—— 这是 wave-01 集成教训第 1 条，语料里要有一条"抛/抓"配对来证明没搞混。

## 4. 其它必须照抄的细节

- `struct Extents`（D:41-50）含 `set(double x0,double y0,double x1,double y1)`（:43）。
- `class Sketch` 跨 **D:57-964**（`964` 是文件末 `} // end class Sketch`），
  成员函数按 `^\s{4}(void|string|double|int|bool|Extents|Vector3) name(` 数到 **28** 个；
  自己再逐个数一遍并核对，**一个都不能漏**；漏了就是"功能缺失"，违反需求第 5 条。
- 默认实参是用户可见接口，逐个照抄：`this(string renderer_name="svg",
  string projection_name="xyortho", ...)`（:99）、`start(string file_name="sketch.svg")`（:253）、
  `setDashArray(double dashLength=2.0, double gapLength=2.0)`（:361）、
  `line(const Vector3 p0, const Vector3 p1, bool dashed=false)`（:414）、
  `polyline(..., bool dashed=false)`（:444）、`polygon(..., bool fill=true, bool stroke=true,
  bool dashed=false)`（:479）等。
- `const double dpi = 90.0;`（D:38）是**模块级常量**，字面量、无算式 ⇒ 无折叠风险，直接搬。
- `final switch (myProjection)`（:124）在 D 里要求穷尽且无 default。C++ 没有 `final switch`：
  照抄成覆盖全部枚举员的 switch，**不要**加 `default:` 分支（加了就丢掉"漏一个枚举员是编译错误"
  这一层可观测约束），并在头注释写明这一点。
- `override string toString()`（:186）：D 的 `toString` 是协变 override，端口照既有约定
  写 `std::string toString() const override`；**注意 `geom/gpath` 才有 `classString()`，
  misc/surface/volume 没有**（`grep -c classString` 在 sketch.d = 0），别标 `override` 到不存在的东西上。
- 渲染输出的**文本格式**是门禁本体：SVG 属性的小数位、顺序、`%.*f` 宽度、
  `start`/`finish` 何时打开/关闭文件（D 的 `File` 析构会 flush，端口已有的
  `File_guard` RAII 写法可参考 `src/geom/volume/parametricvolume.cpp:240-249`）。
  若 sketch 走 `geom/misc/svg` 的现成接口，就**复用**它，不要自己写 SVG 字符串。

## 5. demo 的基线（主线程已实测，可直接用来自check）

`sketch_demo.d` 用 ldc2 编译+链接 **rc=0**，运行 rc=0，stdout **恰好 2 行**、stderr 0 字节：

```
Begin demonstration of the geometric rendering with svg
Done.
```

写出 3 个文件：`test.svg` 9,739 字节（md5 `510aea9d14a0bdefcadbf5e6b10b30a4`）、
`test2.svg` 1,955（`60bc566d544722cce7f42680937263bd`）、`test3.svg` 8,856（`ae87fcbcd1e47c2eb8ec9f31670751e1`）。
**D 侧连跑两次全部 SAME** ⇒ 可当基线。demo 必须在 argv[1] 给的构建树目录里写文件，
**不许污染源仓库**（照 `tests/demo_geom_gpath_gpath_utils.cpp` 的做法）。

## 6. 抓取 D 语料

```bash
cd /home/tang/packages/Eilmer
export PATH=/home/tang/packages/dlang/bin:$PATH
# /tmp/gdtk_shadow 是 gdtk/src 的副本（去掉 @nogc）；gdtk 本体**只读**
rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip \
  probes/geom/misc/probe_sketch.d -of=/tmp/probe_sk -od/tmp/probe_obj_sk
/tmp/probe_sk > baselines/e587b235/probes/geom_misc_sketch.txt
```
**不加 -O**：本仓库所有基线都是 ldc2 **-O0** 抓取，-O0 就是契约（T-919/T-920 两次都是
栽在把高优化档的行为当成语言的行为）。

## 7. 构建自检（只构建你自己的 target）

```bash
cd /home/tang/packages/Eilmer && rm -rf /tmp/w2_sk && cmake -S . -B /tmp/w2_sk \
  -DCMAKE_BUILD_TYPE=Release && cmake --build /tmp/w2_sk --target <你的 targets> -j3
cd /tmp/w2_sk && ctest -R "geom_misc_sketch" --output-on-failure
```
再跑 Debug 与 `-DEIL_SLICE_CHECK=ON`。**机器 16 核但并行有别的代理，用 `-j3`；
只构建你自己的 target**，别人半成品里的编译错误不是你的任务。

## 8. 注册纪律（上次四个代理被打断，教训就在这条）

- 测试文件**没写完就不要注册**。`cmake/wave2_t019_sketch.cmake` 已建好，
  在里面用 `if(EXISTS ...)` 自注册；**门禁必须始终全绿**（当前基线 93/93 三配置）。
- 语料是**逐行字节相同**的全或无门禁。做不到就**停下来报告**，给出行号、D 值、端口值、
  已排除的假设；**绝不**放宽比较、改基线、加容差带。
- 允许写：`src/geom/misc/sketch.*`、`probes/geom/misc/probe_sketch.d`、
  `baselines/e587b235/probes/geom_misc_sketch*.txt`、`tests/{test,demo}_geom_misc_sketch*.cpp`、
  `cmake/wave2_t019_sketch.cmake`、`CMakeLists.txt` 里**只加一行** `include(...)`。
- **禁止改**：`substrate/**`、`reports/backlog.md`、`reports/task-log.md`、
  别人的模块文件/别人的 cmake 片段。发现 substrate 缺口 → 写进最终报告交主线程登记 T-9xx。
