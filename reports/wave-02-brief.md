# Wave 2 转写简报（每个子代理必读）

适用范围：`geom/`（非 luawrap）、`gasdyn/`（非 Lua）、以及 Wave 2 的 nm/gzip 前置件。
参考树：gdtk `e587b235`，编译器 ldc2 1.40.0。产物树：`/home/tang/packages/Eilmer`。

## 0. 不可违反的三条纪律

1. **语法层面等价，不改行为**。算法步骤、离散格式、收敛判据、循环边界、打印文本、
   参数默认值一律照抄 D 原文。发现 D 里的可疑写法（含 shadow、越界读、无意义自运算）
   时**保留原语义并在注释里写明为什么这样保留**，要修就写进 `reports/backlog.md`，
   绝不在转写中"顺手修正"。
2. **实测代替推断**。凡子任务涉及未在本文件钉死的 D 语义（运算符降级、格式化、
   `.init`、容器打印），先写 D 探针量一遍，再把结论补进本文件第 5 节。项目已经多次
   证明"从语言语义推理"会错。
3. **交付前自跑** `clean configure → build → ctest`（命令见第 6 节），报告里给数字，
   不允许只写"通过"。只报"单文件编译通过"不接受。

## 1. 来自 wave-01 集成的返工教训（backlog 第 48-53 行）

1. `.msg()/.file()/.line()/.toString()` 在 `eil::ThrowableBase` 上；`Exception` 与 `Error`
   是**兄弟**：`catch (ExceptionBase e)` 不能捕获 `ErrorBase`。D 原文 `catch (Exception e)`
   → 只 catch `ExceptionBase`；D 的 `throw new Error(...)` → `throw eil::ErrorBase(...)`。
2. 只用 **g++ 11** 支持的开关。FP 一律沿用顶层 `CMakeLists.txt` 的
   `-fno-fast-math -fno-unsafe-math-optimizations -ffp-contract=off`，
   **不要**添加 `-ffp-conform`（clang-only，曾采纳后失败）。不要新增任何 `-Ofast/-ffast-math`。
3. CTest 注册若测试要读数据文件，**必须把路径作为实参传进去**
   （`nm_smla`/`nm_stmatrix` 因漏传 `.mtx` 假失败过）。geom 的网格样例文件同此。
4. 头文件自包含：`#pragma once` + 显式 include，不依赖传递包含。

## 2. 结构约定

- 一个 D 模块 = 一对 `src/<dir>/<module>.hpp/.cpp`，路径与 gdtk 一一镜像，不合并不拆分。
  `geom/elements/vector3.d` → `src/geom/elements/vector3.hpp`。
- 命名空间：`eil::geom`（与 `eil::nm`/`eil::util` 同构）。**默认平铺**；只有当两个 D 模块
  导出同名符号、平铺会重定义时，才把冲突模块放进 `eil::geom::<module>`，并在头注释里写明
  是哪个符号冲突。D 靠 `import geom.grid.paver: PavedGrid, POINT_LIST` 这类选择性 import
  避冲突的地方要特别检查。
- 模板代码进头文件；只有需要独立 TU 的（非模板函数、静态数据、demo 入口）进 `.cpp`。
- 每个转写文件顶部注释写：`// geom/grid/sgrid.d -> src/geom/grid/sgrid.hpp (module geom.grid.sgrid)`，
  后接本文件的非 1:1 之处（若有）。
- D 的 `unittest` 块 → 逐个转成 `tests/test_<dir>_<module>.cpp` 里的可执行断言 + CTest 注册；
  D 的 `*_demo.d` → `tests/demo_<dir>_<module>.cpp`，stdout 必须与 D 二进制字节相同。
- 变体：`version(complex_numbers)` 支按 D 原文用 `#ifdef EIL_COMPLEX_NUMBERS` 保留但**不编译**
  （Wave 12 债务 T-904）；`with_libplot`、`FlowSolverVersion.tecplot_io` 之类未开的 version
  用 `#if 0` 或 `#ifdef` 原样保留并注明变体名，不要删。

## 2b. geom 剩余范围的构造分布（实测计数，据此排优先级）

`grep -rF` 统计，geom 去掉 luawrap：`foreach (` **491**、`~=` **478**、`assert(` **297**、
`override` **202**、`this(` **164**、`.dup` **120**、`version(` 32、`immutable ` 21、
`enum ` 12、`opIndex` 6、`template ` 5、`map(` 4。
**没有** `mixin`、`static if`、`__traits`、`alias this`、`scope(exit)`、`__gshared`、
`extern(C++)` —— 也就是说 geom 这一层没有元编程障碍，工作量全在
循环/追加/断言/虚函数重写这四类上，映射表第 3 节已覆盖。
两个提醒：`assert(` 297 处一律 `eil_assert`（D 的 release 求解器保留 assert，不是 NDEBUG 语义）；
`opIndex` 6 处 → `operator[]`，注意 D 的越界是 RangeError 而不是未定义行为。

## 3. 语言构造映射（已在前置件里定稿的写法，照抄）

| D | C++ |
|---|---|
| `struct Vector3 { number x,y,z; }` | `struct Vector3 { eil::number x, y, z; };`（值语义，与 D 一致） |
| `class Grid` / `this()` / `override` | `class` + 虚函数 + `override`；D 类引用语义（复制即别名）→ `std::shared_ptr`，头注释标 `adapted-handles` |
| `T[] p`（形参） | `eil::Slice<T>`；`new T[n]` → `eil::Array<T>(n)`；`arr.length=m` → `set_length(m)`；`~=` → `append()`/`append_slice()`；`.dup` → `dup()` |
| `a[lo..hi]` / `a[lo..$]` / `a[]` | `a.slice(lo,hi)` / `a.from(lo)` / `a.all()`；`$` → `a.length()` |
| `foreach (i, v; arr)` | 显式索引循环 `for (size_t i=0; i<arr.length(); ++i)`，保持求值顺序 |
| `foreach (k, v; aa)` | `for (auto& kv : aa)`（`std::map`，键序=字典序，见 `eil/aa.hpp` 说明） |
| `aa[k] = v` / `v = aa[k]` / `k in aa` | `aa.set(k,v)` / `aa.get(k)`（缺失抛 RangeViolation）/ `aa.contains(k)` |
| `writefln(fmt, a, b)` / `writeln(a,b)` / `to!string(v)` / `text(...)` | `eil::writefln` / `eil::writeln` / `eil::to_string` / `eil::text`（`substrate/include/eil/format.hpp`） |
| `std.format.format` | `eil::format`；`%s` 上数组给 `[1, 2.5, -3]`，字符串元素带引号 |
| `sqrt/log/log10/asin/acos` | `eil::sqrt` 等（`eil/math.hpp`，域错误返回**正** NaN，Phobos 与 glibc 的 NaN 符号位不同） |
| `PI`、`E` | `EIL_PI`、`EIL_E`（D 的是 80 位 `real`；`eil::real = long double`） |
| `approxEqualNumbers(a,b,rel,abs)` | `eil::nm::approxEqualNumbers`（`src/nm/number.hpp`） |
| `isClose` | `eil::isClose`（`<=` 语义，已按语料钉死） |
| `x.re` / `x.im`（`number` 可能是 Complex） | `eil::re(x)` / `eil::im(x)` |
| `assert(cond)` | `eil_assert(cond)`（抛 `ErrorBase`，release 仍在） |
| `enforce(c, "msg")` | `eil::enforce(c, "msg")` |
| `throw new XException(...)` | `EIL_THROW_FLOW(msg)` 或 `throw eil::XException(msg, __FILE__, __LINE__)`（必须带调用点 file/line） |
| `init`（struct/class 默认值） | 默认成员初始化 / `T{}` |
| `Tuple!(A,B)` | `std::pair`（wave-01 已定） |
| 默认参数 `= to!number(0.0)` | `= eil::number(0.0)` |
| `@property` | 保留为 `name()` 取值 / `name(v)` 赋值的函数对，**不要**改成公有成员（调用点写法不变） |

## 4. 本轮已知的前置与阻塞（别自己绕过，按此执行）

**前置件（stage 0 已转）**：`nm/spline.d`、`nm/splinelsq.d`、`nm/nelmin.d`、`extern/gzip/gzip.d`。
- `geom/gpath/{xspline,xsplinelsq,gpath_utils}.d`、`geom/surface/beziertrianglepatch.d`、
  `geom/grid/sgrid.d` 依赖 `nm.nelmin/nm.spline/nm.splinelsq` —— 已解锁。

**阻塞与范围修正（登记进 backlog，不许在代码里 hack 过去）**：
1. `gasdyn/gasflow.d`(1595) + `gasflow_demo.d`(202)：函数签名普遍带 `ref GasState`、
   `GasModel gm`，并做 `cast(CEAGas) gm` 动态下行转换；`gasflow_demo` 还要
   `init_gas_model(".../cea-lut-air-version-test.lua")`。`gas/` 非 Lua 部分 23,281 行属 Wave 3。
   → Wave 2 只做 `gasflowexception.d` + `idealgasflow.d` + `idealgasflow_demo.d`。
2. `geom/elements/vector3.d:794` `getJSONVector3(JSONValue,...)` 依赖 `std.json`（本机无库，
   T-902）。全树只有 `lmr/bc/*`（Wave 8）消费它 → Wave 2 不移植该函数，`fidelity.yml` 记
   `deferred: std.json`。
3. `geom/grid/sgrid.d:19` `import gzip`、`:18 import std.process : pipeShell`（1623 行
   `pipeShell("gunzip -c " ~ fileName)`）；`usgrid.d:14 import std.file`（`exists`/`mkdirRecurse`）。
   → gzip 走 zlib（`/usr/include/zlib.h` 存在）；`pipeShell` 用 `popen`；`std.file` 用
   `std::filesystem`。**`.gz` 解压后的文本可比，容器字节不可比**（entry mtime），
   门禁只比解压后的内容。
4. `geom/misc/libplot.d`(428)：`with_libplot` 变体，本轮不转（原 backlog 已记）。
5. `geom/{surface,volume,grid,misc,elements,gpath}/package.d`、`geom/package.d`：C++ 无对应物，
   不生成聚合头；`import geom;` 的调用点改为**实际用到的**模块头，并在头注释列出替代关系。
   若因此暴露 D 侧的循环依赖，写进 `fidelity.yml` 而不是造前向声明泥球。
6. `geom/luawrap/`(9,876 行)：Wave 7，本轮不碰。

## 4b. geom 的三条多态层级（转写前定了这些，别各写一套）

实测层级（`grep '^class' geom`）：

| D 基类 | 站点 | 子类数（实测 `grep -c 'class X : Base'`） | 关键契约 |
|---|---|---|---|
| `Path` | `geom/gpath/path.d:30` | 直接 12，gpath 内类共 20（含 4 层深链 `ReParameterizedPath→SubRangedPath→ReversedPath`） | `abstract Path dup() const`、`abstract Vector3 opCall(double t) const`、`abstract override string toString()`、`abstract string classString()` |
| `ParametricSurface` | `geom/surface/parametricsurface.d:34` | 15 | 同上形状（`opCall(u,v)` 二参） |
| `ParametricVolume` | `geom/volume/parametricvolume.d:63` | 8 | 同上形状（`opCall(u,v,w)` 三参） |
| `UnivariateFunction` | `geom/misc/univariatefunctions.d:17` | 9 | `f(t)`、`fprime(t)`、`inverse(y)` |
| `Grid` | `geom/grid/grid.d` | 2（`StructuredGrid`、`UnstructuredGrid`） | stage 3 用 |

统一写法（`Path` 为例，另两条同构）：

1. 基类 = 抽象类 + `virtual ~Path() = default`；
   `virtual std::shared_ptr<Path> dup() const = 0`（D 的 `Path dup()` 返回基类句柄，
   C++ 协变返回类型只能到裸指针，所以统一返回 `shared_ptr<Base>`，头注释标
   `adapted-handles`）。
2. `opCall` → `virtual Vector3 operator()(double t) const`。
3. `abstract override string toString()` → `virtual std::string toString() const = 0`；
   `classString()` 同样纯虚——D 用它打印类名，日志比对依赖它，**不要**省。
4. `cast(Line)this !is null`（`path.d:136`）→ `dynamic_cast<const Line*>(this) != nullptr`，
   需要 RTTI，别改成虚函数标志位（那会把 D 的"运行期类型判断"变成另一套逻辑）。
5. D 的**嵌套函数**（如 `path.d:154 double distanceToPoint(double t)` 闭包 `this` 与 `p`）
   → C++ 捕获型 lambda。为此 `eil::nm::linesearch::minimize` 加了可调用对象重载
   （`minimize(F f, T& a, T& b, double tol)`，与原 NTTP 版共用同一个 `minimize_fn` 体，
   已实测两条入口给出**逐位相同**的区间 `[0.300000173, 0.300000838]`）。
   凡 D 用嵌套函数当被最小化/被求根目标，一律走这个重载，不要手写第二份黄金分割。

## 5. 实测钉死的语义（逐条来自本仓探针，新结论继续追加）

- `to!string(double)` 是 `%.6g`，不是最短往返：`3.141592653589793` → `3.14159`。
- `Vector3.toString()` 产出 `Vector3(1, 2.2, 3)` 形态（`format("Vector3(%s, %s, %s)", to!string(x)...)`），
  `writeln(v)` 会走它。
- D 的 `Vector3.add(number x, number y, number z, number factor)`（vector3.d:137-143）**参数遮蔽成员**：
  体内 `x += factor*x` 作用于形参，对对象**无副作用**。移植必须保持"无副作用"，
  并加注释指向该行。同理 `scale`/`normalize` 里没有遮蔽，别混淆。
- LDC 的 `real` 是 80 位（`sizeof=16`，`mant_dig=64`），`PI/4` 这类表达式先在扩展精度算、
  存进 `double` 形参时才舍入 → 用 `EIL_PI` 并保留同样的"宽算窄存"。

- **`a ^^ b` 一律写成 `eil::pow(a, b)`，绝不禁展成乘法**。实测（`probes/substrate/probe_pow.d`
  → `baselines/e587b235/probes/substrate_pow.txt`，`ctest -R substrate_pow` 26 行零差异）：
  D 的 `^^` 就是 `std.math.pow`，`pow(x,2)` 与 `pow(x,2.0)` 同值，但 **`x^^3` 与 `x*x*x`
  不同值** —— `1.2345^^3=1.8813659636249997` 而 `1.2345*1.2345*1.2345=1.8813659636249995`
  （`2.0/3.0` 同样差 1 ULP）。Wave 2 范围有 **107 个 `^^` 站点**（89 个 `^^2`），
  集中在 Bezier/NURBS/Paver 的多项式求值里；把 `^^3` 当乘法展开就是 1 ULP 的行为改动。
  `eil::pow` 同时做 NaN 符号规范化（D 的 `(-1.0)^^0.5` 打印 `nan`，glibc `pow` 给 `-nan`）。
- `(-1.0)^^0.5`、`pow(-1,0.5)`、`sqrt(-1)`、`log(-1)` 四个域错误在 D 侧**全部打印 `nan`**
  （正 NaN），与 `eil/math.hpp` 的包装一致。

- **`Vector3 v;` / `Vector3()` 是 NaN 而不是 0**。实测 `probes/geom/elements/probe_vector3.d`
  的 `str_init` 行：D 打印 `Vector3(nan, nan, nan)`。D 的 `double.init` 是 quiet NaN
  （与 wave-1 语料一致），struct 逐字段继承 `.init`。C++ 侧用 `eil::d_init<number>()`
  给成员做默认值（`substrate/include/eil/number.hpp`），**不要**写 `= 0.0`：
  写成 0 会把参考求解器会传播下去的 NaN 藏起来，日志与数值都会分叉。
  注意 `Vector3(1.0)`、`set(x,y)`、`clear()` 这些显式路径仍是 0，与 D 一致。
- `eil::geom::Vector3` 与 D 的逐位对照结果：**53 行 `%17g`/`%.6g` 观测 diff 为空**
  （`ctest -R geom_elements_vector3_equiv`，基线 `baselines/e587b235/probes/geom_elements_vector3.txt`）。

- **`iota` 用 `eil::iota`（`substrate/include/eil/range.hpp`），不要写 `for (v += step)`**。
  实测 31 行语料（`probes/substrate/probe_iota.d` → `ctest -R substrate_iota` 0 差异）：
  D 的第 n 个元素是 **`begin + n*step`**，不是累加——`iota(0.0,1.0,0.1)` 第 6 个元素
  D 给 `0.60000000000000009`，累加给 `0.59999999999999998`（第 7/8/9 个也不同）；
  远端边界是**严格**的：`iota(0.0,1.0,0.25)` 到 0.75 就停，丢掉正好等于 1.0 的那一个，
  写成 `v <= end` 会多出一个点。负步长镜像同一条（`iota(5,0,-1)` → 5,4,3,2,1，不含 0）。
  Wave 2 共 **31 处 iota**，其中 29 处在 `geom/surface/beziertrianglepatch.d`
  （降指标遍历），另有浮点扫描在 sgrid 与该文件里。

（待补：数组切片越界文本）

## 6. D 探针与对照流程（必须可复现）

```bash
# 1) 写探针：Eilmer/probes/<dir>/<module>/probe_<topic>.d，import 被测 D 模块
export DLANG=/home/tang/packages/dlang/bin
$DLANG/ldc2 -i -I/home/tang/packages/gdtk/src probes/geom/elements/probe_vector3.d -of=/tmp/probe_d
# 注意：gdtk 树只读。任何情况下不得修改 /home/tang/packages/gdtk 下文件。

# 2) 抓基准 stdout 入库（版本化，供 ctest 复现）
mkdir -p baselines/e587b235/probes && /tmp/probe_d > baselines/e587b235/probes/geom_elements_vector3.txt
sha256sum baselines/e587b235/probes/geom_elements_vector3.txt

# 3) C++ 侧同名 test/demo 输出用 compare_text.sh 逐字节比
ctest -R <name> --output-on-failure
bash harness/compare_text.sh baselines/e587b235/probes/<file>.txt /path/to/cpp_demo
```

数值等价判据：能打印的走 stdout 字节相同或 `%.17g` dump diff 为空；
几何量另加相对容差 1e-13 的逐元素比对，并在报告里**同时给出位相同个数 / 总量**
（例如 "467 个量里 463 个逐位相同，4 个差 ≤2 ULP"）。

构建自检（交付前必跑，两个模式）：

```bash
rm -rf /tmp/w2_<yourstage> && cmake -S . -B /tmp/w2_<yourstage> -DCMAKE_BUILD_TYPE=Release \
  && cmake --build /tmp/w2_<yourstage> -j8 && (cd /tmp/w2_<yourstage> && ctest)
# 至少再跑一次 -DCMAKE_BUILD_TYPE=Debug，并跑一次 -DEIL_SLICE_CHECK=ON
```

## 7. 溯源登记

- 每新增一对 `.hpp/.cpp`，跑 `python3 tools/gen_manifest.py` 重新生成
  `provenance/manifest.yml`（记录 `d_blob`、行数、`fidelity`、`verified_by`）。
- 非 1:1 之处追加到 `provenance/fidelity.yml`，并在 `reports/task-log.md` 对应 T-ID 小节说明。
- `bash tools/check_staleness.sh` 必须报 `no drift`。
