# Wave 02 报告（已完成）— 几何库 geom/ 与 gasdyn/ 迁移

参考：gdtk `e587b235`（ldc2 1.40.0）。产物：`/home/tang/packages/Eilmer`，git 逐阶段提交。
范围：geom 非 luawrap 部分 + gasdyn 非 Lua 部分。Wave 2 的实范围与排除理由见
`reports/backlog.md` 顶部"范围修正"段；本报告只记**已完成的阶段**和**当前数字**，
所有待办项一律指回 `reports/backlog.md`，不在这里另立清单。

> 逐任务的详细过程记录在 `reports/task-log.md`（T-010、T-017、T-014、T-013、T-910、
> T-015/stage A、T-013续/stage B/T-913、T-011 各段 G1–G6、T-920、T-922/923/925/914、
> T-927、T-019、以及收尾批 T-929/T-928/T-926① 各一节）。本报告是阶段汇总视角。

## 1. 已完成

### 1.1 规模（截至本次提交，实测 `find`/`wc`）

| 指标 | 值 |
|---|---|
| 已转写 C++ 模块头文件 | **81** 个（`find src -name '*.hpp'`） |
| C++ 源码行（src + substrate） | **35,386** |
| 测试/demo 行 | **37,179** |
| D 对照语料总行数 | **87,263**（`cat baselines/e587b235/probes/*.txt`；收尾批又加三条 substrate 语料：`substrate_array_init` 30、`substrate_conv_int_typename` 92、`substrate_tostring_dispatch` 25） |
| CTest | **113 个测试：default 113/113、Release 113/113、`-DEIL_SLICE_CHECK=ON` 113/113**。收尾批净增 3 条 substrate 门禁：`substrate_array_init`（T-929，30 行 0 差）、`substrate_conv_int_typename`（T-928，67 核对 / 29 窄类型跳过 / 0 差）、`substrate_tostring_dispatch`（T-926①，12 核对 / 13 需反射跳过 / 0 差）。此前从 87（含 Release 86/87 的 T-919 红行）一路推到 113：`geom_grid_grid` **643/643**、`geom_grid_paver` **1961/1961**、`geom_grid_nanadd` **638/638**、`substrate_sin_cos_pair` **415/415** 四档一致 |
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

### 1.5 收尾批：三条 substrate 债一次清（T-929 / T-928 / T-926①）

Wave 2 最后一步是 backlog 里攒下的三条 substrate 缺口，同批做（都要重建 `eil_geom`，且都不许
由子代理碰 substrate，只能主线程落）。全部**先量后写**——每条的证据语料早就抓在盘上（T-011 G4/G5、
T-019 sketch、T-927），这一步是"接线 + 实现 + 复跑三配置给数字"，探针重跑逐字节可复现：

| 债 | 实测事实（推翻的直觉） | 落点 | 门禁 |
|---|---|---|---|
| **T-929** | `new double[n]` 与 `arr.length = n` **都填 NaN**（`0x7ff8000000000000`），不是 0；OOM 报 D 运行时位置 `rt/lifetime.d(1454)` | `array.hpp` `set_length` 增长槽走 `d_init<T>()`；删 `sgrid.cpp` `d_set_length`、`usgrid.cpp` `set_length_d`、测试 `d_init_doubles` 三个替身，51 处直连 `.set_length` | `substrate_array_init` 30 行；`geom_grid_usgrid` 5 条 OOM 行改由异常携带 `file()/line()` 修正 |
| **T-928** | 整数 string→int **有逐目标类型的范围检查**，越界报 `ConvOverflowException "Overflow in integral conversion"`；有符号两端不对称；`ulong` 最大值放不进有符号累加器 | `error.hpp` 加 `ConvOverflowException`；`parse_int_strict` 改按 `T` 模板、`make_unsigned_t<T>` 累积逐位查界；删 `usgrid.hpp` 本地类 + `usgrid.cpp` `d_to_int` + `time_utils.cpp` `to_int_d` 三个扫描器；数值→int 两条 D 路径**按来源分立**只换类不改文本 | `substrate_conv_int_typename`（此前**无门禁**——正是缺口能长存的原因）；`test_util_time_utils` 断言从"只比 msg"加粗到**断言 D 类名** |
| **T-926①** | `to!string` 对带无参 `toString()` 的 struct **和** class 都调它；无参重载**优先于** `(fmt,Args…)`；只有 fmt 版**不**被用 | `format.hpp` 加 `has_noarg_to_string`（void_t 探零参 `toString()`）与分派分支；容器仍 `[a,b]` dump 且**逐元素派发**；trait 随 `to_string` 重导出到 `eil::` | `substrate_tostring_dispatch` 25 行语料里 12 核对、13 需 C++ 反射（字段 dump / 类名 / enum 名）复现不了的跳过并点名 |

**判别力每次都自证**：T-929 把 `grow_init` 退回 `T()` → 两道门禁转红；T-928 把范围检查改成
`if(false)` → 5 条 overflow 语料行 + 2 条 time_utils 行转红（实测 `2147483648` 回绕成 `-2147483648`）；
T-926① 若去掉分派分支 `to_string(Simple{7})` 直接编译失败，测试里 `static_assert` 反向锁死
"fmt-only 不派发"。**三配置 113/113、`check_staleness` 无漂移、探针逐字节复现**。

**这三条对后波的直接意义**：`to!string`（1,121 个 `!` 实例里的绝大多数）与整数解析是 Wave 3 `gas/`、
Wave 4 `fluidblock`、Wave 5 求解器日志的硬入口——T-926① 让"把对象写进消息文本"从一个调用点派发，
不必逐点手写；T-928 把端口里三处各写一遍的整数扫描收敛成一处。**仍欠**：T-926② `Slice` 动态数组
默认实参、T-926③ `readln()`（与 T-923 同一 EOF-返空语义），以及 **T-930**（Phobos `to!double`
十进制→二进制**不是正确舍入**——15 token 里 2 个差 1 ULP 且 D 是错的一方，Wave 3 起是硬依赖，实现前
须先扫分歧率）、**T-931**（`paver` 内部铺面算术差 1 ULP，产物级、现有 1961 行语料覆盖不到）。

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

**一律见 `reports/backlog.md`（唯一权威清单）**，逐任务过程见 `reports/task-log.md`。

**本 Wave 范围内已全部闭合**：`geom/elements`(T-010)、`geom/gpath`(T-013)、`gasdyn` 非 CEA(T-014)、
前置件 `nm/{spline,splinelsq,nelmin}`+`extern/gzip`(T-017)、`geom/surface`+`volume` 15 模块(T-012)、
`geom/misc`(T-019 sketch)、`geom/grid` 整包(T-011 段 G1–G6)、以及四条 substrate/规则债
T-914/T-920/T-922/T-923/T-925/T-927 和收尾批 T-929/T-928/T-926①。闭合时的核对方式与数字都在
`task-log.md` 同名节里，此处不重复。

**Wave 2 仍开着的 ID（各有明确去向，均已在 backlog 落档）：**

- **T-015** `[~]`：门 (1)/(2) 的第一半已实测（gate.sh 拿参考 D 二进制当候选跑 sod-shock-tube 的
  sg/usg 两链，各 17 产物 onlyD=0/onlyC=0 全 PASS）；第二半"C++ 侧可跑"需 **Wave 5 的候选二进制**，
  故保持 `[~]`，不用 Wave 2 拿不到的证据去销它。
- **T-921**：本机无 GNU libplot，D 侧连参考二进制都产不出来 → 环境阻塞，随 libplot 落地再启。
- **T-926 余两条**：② `eil::Slice<T>` 无法表达 D 的动态数组默认实参（`double[] canvas_mm=[]`），
  Wave 3 之前定形；③ 没有 `readln()`，只在未编译的 `with_libplot` 支里用到，且与 T-923 同一
  "EOF 返空串、不抛"语义（已被 `nm_stmatrix_fails` 钉住），Wave 4/5 补时必须照该实测。
- **T-930**（Wave 3 起硬依赖）**量测已完成于 2026-10-15，实现待用户选路线**：
  全量 `blk.tmp` 的 128,346 个数值 token 上，strtod 与 D 差 15 个（0.0117%，全为 15–16 位有效数字），
  最接近的简单复刻模型差 3 个但会把 1 行本来正确的结果改坏；≥20 位输入 Phobos 走另一条大数路径。
  要到 0 分歧必须搬 Phobos 的 `parseFloat`（本机无源码，只剩黑盒拟合）。三条路线的代价见
  `task-log.md` 的 T-930 节；`demo_artefacts_geom_grid_sgrid` 继续故意不注册。
- **T-931** ~~候选根因：T-920 的 cos/sin 融合、T-909 常量折叠、迭代 `a*b+c` 的融合次序~~
  **已于 2026-10-15 闭合，三个候选根因一个都不是**：真原因是 `x ^^ 2` 被转写成 glibc `pow(x,2.0)`
  （T-933）与 `eil::acos` 用 glibc 而非 Phobos 的 80 位 `atan2` 定义（T-934）。
  `paved_grid.vtk` 现与冻结基线逐字节相同，`demo_artefacts_geom_grid_paver` 已注册；
  定位手段是新的 `ctest:geom_grid_paver_stage`（4,249 行阶段指纹）。数字与判别力见
  `backlog.md` 的 T-931/T-933/T-934 条与 `task-log.md`。
- **T-916**（Wave 3 硬任务）：D 的 `sinh` 有两处**结构**分歧（不是舍入），随 `gas/` 处理。
- **横切债务**：T-902(`std.json`)、T-903(`toString()` 类名前缀无法与 D 一致)、T-904(复数支从未编译)、
  T-905(nm/util 叶子按波次补迁)、T-907(`Slice<const T>`)、T-909(ldc2 常量折叠)、T-912(转写纪律)、
  T-915(D 侧真实崩溃不加防护)——各钉到自己的波次。
- **T-016**：本报告即其收尾产物；`wave-02` tag 随本报告打上。

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
