# Wave 01 报告 — 叶子库层（nm + util）迁移

参考：gdtk `e587b235`（ldc2 1.40.0）。产物：`/home/tang/packages/Eilmer`。
范围：求解器依赖闭包里的叶子模块——它们只依赖 `ntypes/complex.d`、`nm/number.d` 与
Lua C API，因此可以独立编译、独立对照，是验证"逐函数语法等价转写"这套方法的最便宜样本。

## 1. 已完成

### 1.1 规模
| 指标 | 值 |
|---|---|
| 转写 D 模块 | 22 个（nm 18，util 4），共 **5,055** D 行 |
| 产出 C++ | **7,123** 行（含 Phase 0 的 substrate） |
| CMake/CTest | 23 个测试 + 6 个 demo，**23/23 通过** |
| D 对照 | 4 组聚类各自与 ldc2 构建的 D 二进制比对，见 1.3 |
| 溯源 | `provenance/manifest.yml` 42 条记录，含每文件 `d_blob`；`tools/check_staleness.sh` 报 `no drift` |

转写清单：`nm/{number,nm_exception,package,limiters,bracketing,brent,ridder,secant,linesearch,newton,bbla,stmatrix,smla,rsla,schedule,newtoncotes,gaussquad,rungekutta}`
与 `util/{lua,lua_service,msg_service,time_utils}`。

### 1.2 结构约定
- 一个 D 模块 = 一对 `src/<dir>/<module>.hpp/.cpp`，路径与 gdtk 一一镜像，不合并、不拆分。
- 模板代码进头文件；`.cpp` 仅保留需要独立 TU 的部分。
- 名称冲突处理：D 靠模块边界区分 `solve`/`minimize`（brent/ridder/secant/newton/linesearch/
  bbla/smla 都有），C++ 平铺 `eil::nm` 会重定义 → 冲突模块置于 `eil::nm::<module>`，
  全局唯一者（bracketing/limiters/number）保持平铺。
- `number` = `eil::number`（主干 `double`）；D 的 `T[]` 形参 → `eil::Slice<T>`（视图），
  `new T[n]` → `eil::Array<T>`（持有）；`arr.length=` → `set_length()`，`~=` → `append()`，
  `[a..b]`/`[a..$]` → `slice()`/`from()`，`$` → `length()`。

### 1.3 逐聚类对照结果（全部用 ldc2 现编的 D 程序做基准）

**A. 求根簇**（bracketing/brent/ridder/secant/linesearch/newton + number/limiters/异常）
- `demo_nm_ridder`、`demo_nm_linesearch` 的 stdout 与 D **字节相同**。
- `limiters` 35 个取值逐位相同；`approxEqualNumbers` 13/13 判定相同。
- 全部 6 个 D `unittest` 移植后通过（-O0 与 -O2 均通过，`-DEIL_SLICE_CHECK` 开/关均通过）。
- 最大偏差：`newton_3` 1.11e-16（2 ULP）、`brent_3` 5.55e-17（1 ULP）。
  根因是 Phobos 与 glibc 的 `exp/sin/cos/pow` 本身差 ≤1 ULP（实测抽样 10 万个 exp 自变量
  有 13,466 个末位不同），C++ 侧是正确舍入的一边——**不是转写误差**。

**B. 线代簇**（bbla/stmatrix/smla/rsla/schedule）
- 473 行 `%.17g` 全精度 dump、覆盖 467 个量（Gauss-Jordan 解与增广阵、LU 分解/solve 残量、
  逆阵与 `A·A⁻¹`、lsqsolve、Crout 解与主元、ILU(0)/ILU(p=2) 的 aa/ja/ia、
  gmres/gmres2/rpcGMRES/fGMRES/10 次重启、块乘、Jacobi 8 次与 SGS 4 次扫描）：**diff 为空**，
  含依赖 `sqrt` 的 GMRES 路径也逐位相同。17 位可唯一往返，故这确实是位相同而非容差掩盖。
- `demo_nm_bbla` stdout（41 行）字节相同；三条 D `unittest` 未覆盖的打印路径
  （`computeInverseDebug`、`SMatrix.toString`、`Schedule.toString`）也单独比到相同。
- 对照过程中**发现并修掉两个移植自身的 bug**：`SMatrix::set()` 覆盖了已有条目
  （D 的 `a[i,j] = a[i,j] - …` 在 C++ 未定序求值下被破坏），以及"行尾追加"分支返回
  `aa[ia[row+1]]`（D 里这个越界读被丢弃、无副作用，移植侧把它当赋值目标就成了错）。
  这正说明"与 D 逐位比"作为门禁的价值：两处都不是算法错误，而是语言差异。
- D 的类引用语义（复制即别名、经 `const` 句柄改内容，如 `LUDecomp` 的 "Reset A"）
  用 `shared_ptr` 承载 → manifest 标 `adapted-handles`。

**C. 求积/ODE 簇**（newtoncotes/gaussquad/rungekutta）
- 三个 demo 的 stdout 与 D **字节相同**；函数调用次数也相同（315/55、231/21、1725/305、91），
  说明自适应二分树形一致，而不只是末位巧合。
- newtoncotes 37/39、gaussquad 49/51 个量逐位相同；4 个例外是 `PI/4 − estimate` 行，
  绝对差 3.06e-17（D 的 `std.math.PI` 是 80 位量），**`%.6g` 打印后仍相同**。
- rungekutta 139/143 相同：所有步进输出（t1、x1、err、10 步推进状态与误差）**逐位相同**；
  4 个解析真值差 1–2 ULP（最大相对 2.99e-16），根因同上（Phobos `exp` 在 9/90 采样点上
  差 1 ULP）。
- newtoncotes 的发散案例（`sqrt` 核、区间 [0.75,1]、容差 1e-10）在两侧以相同方式终止。

**D. util/Lua 宿主层**（lua/lua_service/msg_service/time_utils）
- 与 ldc2 现编的 D 探针相比，**226 行 transcript 字节相同**，覆盖：全部 24 条错误字符串、
  `LuaInputException` 与 `object.Error` 的分派、注册名 `__index __name __tostring badNil
  doubled new typeName`、`luaL_error` 被 Lua 前缀化后的文本、`doLuaFile` 消息、
  `getInteger/getBooleanFromTable` 的 +1 栈副作用、`tableEmpty` 留下的 nil、
  26 个 `timeStringToSeconds` 输入（含 Phobos 的 `Unexpected 'x' … to type int`）、
  `double.init`→NaN / `int.init`→0 / `bool.init`→false、稀疏矩阵重读怪癖。
- 机制差异处理：`extern(C) int toStringObj(T, string mt)(L)` 是 extern-C 模板，C++ 禁止
  （"template with C linkage"）→ 拆成 C++ 链接的函数体 `toStringObj<T>(L, const char*)`
  加宏 `EIL_LUA_TOSTRING_OBJ(fname, T, "Name")` 生成真正 `extern "C"` 壳；`nm` 查表确认
  导出符号未修饰、Lua 侧能调用。
- 顺带修掉 D 侧一个潜在悬垂读：D 在 `lua_pop` 之后仍别名 Lua interned 缓冲，移植侧改为拷贝。
- **ASan + UBSan + LSan 全清**（这一层把指针交给 C 代码， sanitizer 是硬门槛）。

## 2. 与 D 非 1:1 之处（全部登记在 `provenance/fidelity.yml`）

1. `adapted-handles`：D 类的引用语义 → `shared_ptr`；`new X!T(literal)` → 工厂函数。
2. `adapted-callback`：D 的 `alias f` 编译期别名 → C++17 `auto` NTTP；回调一律具名函数。
3. `Tuple!(...)` → `std::pair`、`size_t[2][]` → `Array<std::array<size_t,2>>`（打印形态已核对相同）。
4. D 的 `GC.minimize()` 无对应物；`destroy(lev)` → 显式释放。
5. 默认句柄指向合法 0×0 对象，越界访问抛 `RangeError`（D 侧是指针错误）。
6. `sizeof(lua_KContext)`：D 把它别名成 int（4），C++ 取 Lua 头定义（8）；该字段恒为 0，无行为影响。
7. `util/lua_service.d` 的 `unittest` 块在上游已被注释且引用不存在的 `LuaState/getValues`
   → 无可移植内容，改为契约测试覆盖。
8. `brent.d`、`newton.d` 的 D `unittest` 在参考树里**编不过**（`@nogc` 中 `new Exception`）
   → 移植测试用等价体取基准值。
9. `stmatrix.d`/`smla.d` 里 `exit` 被 import 但两侧都未使用。
10. `#ifdef complex_numbers` 分支已按 D 原文两支保留，但复数支**尚未编译验证**（Wave 12 债务）。

## 3. 待完成

> **注意**：自 2026-10-13 起，待办统一由 `reports/backlog.md` 维护（唯一权威清单）。
> 以下列表保留作为当时的原始记录，其中的项已在 backlog 里获得任务 ID；
> 两处若有出入，以 `backlog.md` 为准。


1. `nm` 剩余：`nelmin`(432)、`spline`(120)、`splinelsq`(161)、`bdfLU`(118)、
   `tree_patch`(996)、`univariate_lut`(101)、`power_benchmark`(89) —— 各按消费波次移入
   （nelmin/spline 跟气体 LUT、tree_patch/univariate_lut 跟插值/场求解、bdfLU 跟 CHT）。
2. `nm/luabbla.d`(580) 与 `util/json_helper.d`(141)：前者并入 Wave 7 的 Lua 绑定波，
   后者需先做 std.json 语料探针（本机无 JSON 库）。
3. `util/zip.d`(1801)：随 gzip I/O 变体波。
4. `util/test_runner.d`(256)：不移植——C++ 侧用 CTest 承担同一职责，D 的 `unittest` 已逐个转为测试。
5. Wave 2：`geom`（~30k 行）+ `grid_utils` + `gasdyn` 非 Lua 部分。门禁案例
   `3D/sod-shock-tube/{sg,usg}`，另带 geom 的 39 个 `unittest`。
6. 方法学待办：本 wave 用"4 个子代理并行 + 主线程集成"的方式转写，集成时出现 3 类返工
   （`.msg()` 应在 `Throwable` 而非仅 `Exception`；clang-only 编译开关；测试注册漏传数据文件路径）。
   Wave 2 起把这三条写进转写简报，并让子代理**自带 ctest 注册片段以外的构建冒烟**。

## 4. 门禁状态

| 门禁 | 本轮可执行性 |
|---|---|
| (1) 快照逐元素 ≤1e-13 | harness 就绪并已用 D-vs-D 自验（7 案例 PASS） |
| (2) 残差/气动力/步数 | 同上，`config/*-residuals.txt`、`loads/` 已在基线内 |
| (3) 70 个 Ruby 判例 | 未跑：需要候选 `e4shared`（Wave 5+ 才存在）；`gpmetis` 缺失会跳过 6 个 metis 案例 |
| 单元/库级 | 23/23 通过，demo stdout 与 D 字节相同 |
