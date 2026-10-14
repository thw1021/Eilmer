# 任务完成日志（逐项追加）

规则：每完成 `backlog.md` 中的一个任务 ID，就在本文件**追加**一节，写清"范围、做了什么、
验证证据与实测数字、产出文件、新遗留"，然后回 `backlog.md` 把该 ID 标 `[x]` 并链到本节。
新发现的待办只写进 `backlog.md`（不在这里积累清单）。已完成任务不再重述过程，
`phase-*.md` / `wave-*.md` 提供阶段汇总视角，本文件提供逐任务视角。

条目模板：

```
## T-xxx — 标题  (YYYY-MM-DD)
- 范围：输入文件与行数
- 做了什么：关键决定与转写要点
- 验证：命令/测试/对照的实测数字（不允许只写"通过"）
- 产出：新增或修改的文件
- 新遗留：新增 backlog ID，或"无"
```

---

## T-000 — Phase 0：基线冻结、构建骨架、D 兼容垫片、验收 harness  (2026-10-13)

- **范围**：gdtk `e587b235` 全树勘察 + 移植基础设施；无 D 模块转写。
- **做了什么**：
  - 冻结参考：确认 `gdtk/build/` 是 HEAD 的完整自洽安装（`e4shared` 横幅
    `Revision-id: e587b235`、`Compiler-name: ldc2`），**未重编、gdtk 零写入**。
  - 摸清运行时契约：内嵌 Lua 被改造为读 `DGD_LUA_PATH`/`DGD_LUA_CPATH`
    （`extern/lua-5.4.3/src/loadlib.c:275`），标准 `LUA_PATH` 无效。
  - 修正主干变体集合：makefile 默认打开 `multi_species_gas/multi_T_gas/MHD/turbulence/
    enable_fp_exceptions/flavour_debug`，与参考横幅 `Capabilities:` 一致 → 主干**含**这三个
    改变结构体布局的变体。
  - CMake + 变体 INTERFACE target + `EILMER_ABI_HASH` 链接符号（主干哈希 `0357ff576ba1`）。
  - D 兼容垫片 `substrate/include/eil/`：`number/real/Complex`、`Array`+`Slice`（代际计数）、
    `AA`、`format`、`math`、`conv`、`error`（Throwable 拆 Exception/Error 两支）。
  - 建立"实测代替推断"方法：`tools/fmt_probe.d` → `tools/fmt_corpus.txt`（786 行观测），
    `tests/test_format_corpus.cpp` 多集合比对复现全部行。
  - 建 harness：`extract_cmds.py`/`run_case.sh`/`normalize_log.py`/`compare.py`/`gate.sh`，
    并抓 7 个 Tier-A 基线。
- **验证**：
  - 语料比对 **786/786 行零差异**。
  - harness 自检：把 **D 二进制当候选**跑 `gate.sh` → 7 案例全 PASS（证明 FAIL 可归因于移植）。
  - `sod-test.rb` 对参考安装 4 tests / 14 assertions / 0 failures；sod 75 步、`final-t= 0.00060314`。
  - 参考侧非确定性实测：同一 `e4shared` 两次运行 `config/cone20.fluidBlockArrays` md5
    `d8ce/7413/d8ce` 变化而 `cone20.list` 稳定 → 判为 D 侧 AA 顺序，改用多重集合比对并显式标注。
  - 编译开关核实：`build/CMakeFiles/eil_nm.dir/flags.make` 含 `-ffp-contract=off`。
- **产出**：`CMakeLists.txt`、`cmake/*`、`substrate/*`、`tools/{fmt_probe.d,fmt_corpus.txt}`、
  `harness/*`、`baselines/e587b235/{7 案例}+SHA256SUMS`、`reports/phase-0.md`。提交 `c4ccbec`、`00602f0`。
- **新遗留**：T-800（`e4-nk-shared` 缺失使 2 个 Tier-A 判例不可用，已写 `INVALID` 防护）、
  T-801（METIS 缺失阻塞 6 个 metis 判例）、T-902（std.json 无库可用）、T-903（substrate 缺口）。

## T-001 — Wave 1：nm 与 util 叶子层转写  (2026-10-13)

- **范围**：22 个 D 模块 / **5,055 行** — `nm/{number,nm_exception,package,limiters,bracketing,
  brent,ridder,secant,linesearch,newton,bbla,stmatrix,smla,rsla,schedule,newtoncotes,gaussquad,
  rungekutta}`、`util/{lua,lua_service,msg_service,time_utils}`。
- **做了什么**：逐模块镜像为 `src/<dir>/<module>.hpp/.cpp`；`T[]` 形参→`Slice`、
  `new T[n]`→`Array`；D 类引用语义→`shared_ptr`（标 `adapted-handles`）；extern-C 模板
  `toStringObj(T, string mt)`→函数体 + `EIL_LUA_TOSTRING_OBJ` 宏壳（标 `adapted-callback`）；
  所有 D `unittest` 逐个转 CTest，demo 转对照测试。
- **验证**（全部以 ldc2 现编的 D 程序为基准）：
  - 线代簇：**467 个量的 `%.17g` dump diff 为空**（含 GMRES/ILU/Crout/Jacobi/SGS 全链，
    17 位可唯一往返 ⇒ 逐位相同）；`demo_nm_bbla` stdout 41 行字节相同。
  - 求积/ODE：3 个 demo stdout 字节相同，**函数调用次数也相同**（315/55、231/21、1725/305、91
    ⇒ 自适应二分树形一致）；Runge-Kutta 所有步进输出逐位相同。
  - 求根簇：2 个 demo 字节相同；limiters 35/35 位相同；`approxEqualNumbers` 13/13 判定相同。
  - Lua 层：**226 行 transcript 与 D 探针字节相同**（24 条错误串、注册名、`luaL_error` 前缀文本、
    栈副作用、26 个 `timeStringToSeconds` 输入、`.init` 取值）；**ASan+UBSan+LSan 全清**。
  - 残余差：4 个解析真值 1–2 ULP（最大相对 2.99e-16）、`PI/4−estimate` 绝对差 3.06e-17
    （`%.6g` 打印后仍相同）→ 根因为 Phobos 与 glibc 超越函数本身差 ≤1 ULP（实测约 13% 的
    `exp` 采样点末位不同），非转写误差。
  - 构建：clean configure → 23/23 CTest 通过，`Debug(-O0)` 与 `Release(-O3)` 双模式均通过。
- **产出**：`src/nm/*`、`src/util/*`、`tests/*`（23 测试 + 6 demo）、`provenance/{fidelity.yml,
  manifest.yml}`（42 条含 `d_blob`）、`tools/{gen_manifest.py,check_staleness.sh}`（报 `no drift`）、
  `reports/wave-01.md`，tag `wave-01`。提交 `836d3b8`。
- **转写过程反向修正了垫片**（说明对照有效）：`Throwable` 两支分离（`.msg()` 上移到公共基类）、
  `ConversionException` 不再二次包裹消息、`to_double/to_long` 补全 Phobos 的 5 种失败文本、
  新增 `eil::real`+`EIL_PI`（实测 D 的 `real` 是 80 位）、`-ffp-contract=off` 取代 clang-only 开关。
  并修掉 2 个移植自身 bug：`SMatrix::set()` 覆盖已有条目、行尾追加分支返回 D 侧的越界引用。
- **新遗留**：T-010…T-016（Wave 2 范围修正：`geom/luawrap` 20 文件 9,876 行改归 Wave 7，
  `grid_utils/ugrid_partition.d` 调 `gpmetis` 故移出 Wave 2）、T-904（复数支未编译验证）、
  T-905（nm/util 剩余叶子）。

## T-010 — Wave 2：`geom/elements/` 全量转写  (2026-10-13)

- **范围**：`geom/elements/vector3.d`(812)、`properties.d`(1168)、`projection.d`(309)、
  `nomenclature.d`(68)、`geom/geometry_exception.d`(16)、`vector3_demo.d`(63)
  ＝ **2,456 D 行**；产出 C++ 约 3,270 行（库 1,600＋测试 1,843＋探针）。
- **做了什么**：
  - 逐模块镜像成 `src/geom/elements/<module>.hpp/.cpp`，路径与 gdtk 一一对应；
    命名空间 `eil::geom`。
  - `Vector3`：D 的 `opBinary/opBinaryRight/opOpAssign/opUnary` → 真实运算符重载；
    `x.re/x.im` → `eil::re()/im()`；`^^2` → `eil::pow`（见下方实测条目）。
  - 两条**必须由实测而非推理**定下的语义：
    1. **`a ^^ b` 就是 `std.math.pow`**，`x^^3` 与 `x*x*x` 差 1 ULP
       （`1.2345^^3=1.8813659636249997` vs 乘法 `...9995`；`2.0/3.0` 同样）。
       Wave 2 范围共 **107 个 `^^` 站点**（89 个 `^^2`），properties.d 自身 19 处活跃站点
       → 全部转成 23 个 `eil::pow` 调用，**一处都不允许禁展成乘法**。
       已钉成 `probes/substrate/probe_pow.d` → 26 行语料 → `ctest -R substrate_pow` 零差异。
    2. **`Vector3 v;` / `Vector3()` 是 NaN 不是 0**：D 的 `double.init` 是 quiet NaN，
       struct 逐字段继承 `.init`（探针 `str_init` 行打印 `Vector3(nan, nan, nan)`）。
       移植初版把成员默认成 `0.0`，会把参考求解器会传播进日志的 NaN 藏起来 →
       新增 `eil::d_init<T>()`（浮点 NaN / 整型 0 / bool false）并改用。
  - 保留的 D 怪癖（都带 file:line）：`vector3.d:137-143` `add(x,y,z,factor)` **参数遮蔽成员**，
    对对象无副作用；`properties.d:662-671` 坍缩钳位在 `moment /= volume` **之前**，
    极薄 hex 的形心返回未归一化矩（`h=-1e-13` → cx=-5e-14, vol=0）；
  - `projection.d:17` `out double t`：实测 D 进入即赋 `double.init`=NaN，
    故平行分支即使调用方传了 7.0/0.0 也返回 NaN → C++ 侧同样先置 NaN。
- **验证**（全部以 ldc2 1.40.0 现编的 D 二进制为基准，且**主线程独立复跑**过）：
  - Vector3：`probes/geom/elements/probe_vector3.d` 的 **53 行 `%17g`/`%.6g` 观测 diff 为空**
    （算术、dot/cross/distance、normalize 三条分支（>0 / 下溢 / 1e-30 冲刷）、
    两种旋转、四种坐标框变换、矩阵变换、toString 各形态）；D `unittest` 27/27 断言成立。
  - properties：**39/39** D `unittest` 断言、探针 **77 行 0 差异、366 of 366 个量逐位相同**
    （triangle/quad/tet/pyramid/wedge/hex，含歪斜、退化、坍缩阈值、负体积/NaN 路径）。
  - projection：**26/26** 断言、47/47 行、**88/88 逐位相同**；
    nomenclature：**41/41** 断言、60/60 行、**97/97 逐位相同**。
  - `vector3_demo` stdout 与 D **原始字节相同**（`ctest -R demo_equiv_geom_elements_vector3`）。
  - 复现性核验：重跑 `probe_properties.d` 与已入库基线 `diff` 为空 ⇒ 基线不是手改出来的。
  - 构建：Release / Debug / `-DEIL_SLICE_CHECK=ON` 三种配置，`ctest -R geom_elements` **6/6 通过**。
- **产出**：`src/geom/{geometry_exception,elements/*}.{hpp,cpp}`、
  `tests/test_geom_elements_{vector3,vector3_equiv,nomenclature,properties,projection}.cpp`、
  `tests/demo_geom_elements_vector3.cpp`、`probes/{substrate,geom/elements}/*.d`、
  `baselines/e587b235/probes/{substrate_pow,geom_elements_*}.txt`、
  `reports/wave-02-brief.md`、`cmake/wave2_geom{,_core}.cmake`。
  提交 `f3bb3cf 53e415b 54cc771`。
- **新遗留**：T-907（`eil::Slice<const T>` 不可由 `Array<T>` 构造，数组形参暂用 SFINAE 模板）、
  T-909（ldc2 常量折叠非正确舍入；**Wave 2 暴露面实测为 0**，Wave 3+ 须逐波重测）；
  `getJSONVector3` 依赖 `std.json` 未转（只有 `lmr/bc/*`＝Wave 8 消费），记入 `fidelity.yml`。

## T-017 — Wave 2 前置件：`nm/{spline,splinelsq,nelmin}` + `extern/gzip`  (2026-10-13)

- **范围**：`nm/spline.d`(120)、`nm/splinelsq.d`(161)、`nm/nelmin.d`(432)、
  `extern/gzip/gzip.d`(147) ＝ **860 D 行** → **1,669 C++ 行**。
  原 backlog 把这三个 nm 叶子排到 Wave 3/4（T-905），但
  `geom/gpath/{xspline,xsplinelsq,gpath_utils}.d`、`geom/surface/beziertrianglepatch.d`、
  `geom/grid/sgrid.d` 直接 import 它们，gzip 被 `sgrid/usgrid` 的
  `read/write_from_gzip_file` 需要 ⇒ Wave 2 绕不过，故本轮前置完成。
- **做了什么**：
  - `spline`（三次样条，走已转的 `nm.bbla` 的 `gaussJordanElimination`）、
    `splinelsq`（最小二乘样条，调 nelmin）、`nelmin`（Nelder-Mead，
    D 的 `alias F`＋模板参数 → C++ 函数指针形参；`Vertex!T`/`less`/`sort` 等
    与 `nm/linesearch.d` 的 `minimize` 撞名，故置于 `eil::nm::nelmin` 命名空间）。
  - `nelmin.d:342` 的 D `unittest` 断言 `nfe==503`（精确值）→ 断言改成范围＋
    **逐位打印**，因为迭代次数本身就是算法路径的证据。
  - gzip：用 zlib（`deflateInit2` 窗口 `15+16` 出 gzip 容器，`15` 出 zlib 容器，`-15` 出 raw），
    覆盖 D 的 `HeaderFormat{automatic,gzip,zlib,raw}` 与 `Compress/Uncompress` 两阶段流。
    **比对的是解压后的 payload，不是容器字节**——容器含 mtime，D 自己两次运行都不相同
    （phase-0 已记录）。
- **验证**（`ctest -R "nelmin|spline|gzip"` **6/6 通过**，Release/Debug/slice-check 三配置）：
  - `nm_spline`：**156 行**语料 replay **0 差异、0 断言失败**
    （节点/基函数/导数/插值值按 `%17g` 逐位；含 20 节点 × 100 采样点的同一循环）。
  - `nm_splinelsq`：**340 行 0 差异、0 断言失败**
    （含最大偏差 `0.014439180761296466` 与 D 打印的 `0.0144392` 对照）。
  - `nm_nelmin`：16 行探针＋8 行 unittest 打印，**0 差异**；
    `conv/nfe/nres/x 向量`全部逐位相同（nfe=503/547、nres=0）。
  - `nm_nelmin_sort`：**6,889 个 D 捕获排列**的排序结果与 tie 表一致。
  - `extern_gzip_gzip`：**66 行观测 diff 为空**（0 断言失败）。覆盖 11 类：D 写出的容器由本端口解压、
    本端口一次 `compress()` 写完、空载荷、无尾换行/有尾换行/CRLF/空行、
    跨 `0x4000` 分块边界的 60,000 字节 1,000 行、10 万个同一字符的极端流、
    raw 容器区间读、被截断的容器、损坏容器、zlib(非 gzip) 容器、不存在的路径；
    容器 CRC32 与 ISIZE trailer 都对得上载荷。
    D 写的 gzip/raw 容器按 SHA-256 登记后逐字节读取一致。
  - **本阶段唯一需要"容差例外"的地方，根因已实测**：nelmin 的 `fx` 列 2 行差
    相对 8.3e-15（`0.00090895282253090665` vs `...91413`）。搜索路径逐位相同
    （nfe/nres/x 全等），差异来自目标函数里两次 `exp`：
    `exp(-1.2050774716664778*0.5)` D=`0.54742011542684965` / C++=`...4954`，
    `exp(-0.46338762974692316*2.0)` D=`...64002` / C++=`...63997`——各差 1 ULP，
    经 `r = y - eta` 的相消放大到残差和。这就是 phase-0 记录的 Phobos/glibc
    超越函数分歧，**不是转写误差**；测试现在对其它字段仍要求逐字节相同，
    只对 `fx=` 允许 1e-13，并**把每一行被容差的记录打印出来**，例外可审计。
- **产出**：`src/nm/{spline,splinelsq,nelmin}.{hpp,cpp}`、`src/extern/gzip/gzip.{hpp,cpp}`、
  `tests/{test_nm_spline,test_nm_splinelsq,test_nm_nelmin,test_nm_nelmin_sort,test_extern_gzip_gzip,demo_nm_nelmin}.cpp`、
  `tests/test_data/gzip_payload.{txt,dgz.gz,zraw}`、`probes/nm/*.d`、`probes/extern/gzip/*.d`、
  `baselines/e587b235/probes/{nm_spline,nm_splinelsq,nm_nelmin,nm_nelmin_sort,nm_nelmin_unittest,nm_nelmin_demo,extern_gzip_gzip}.txt`、
  `cmake/wave2_nm.cmake`。提交 `505c549`。
- **新遗留**：Phobos `Random/uniform` 未移植（`geom/misc/kdtree.d:175` 用
  `Random(19920829)` 生成 100 个测试点）→ 决策见 T-013 简报：抓 D 的点集回放，不搬 MT19937；
  `demo_equiv_*` 只 glob `demo_nm_*`，本轮补的 `demo_nm_nelmin` 因此自动注册。

## T-014 — Wave 2：`gasdyn/` 非 CEA 部分  (2026-10-13)

- **范围**：`gasdyn/gasflowexception.d`(13)、`gasdyn/idealgasflow.d`(952)、
  `gasdyn/idealgasflow_demo.d`(93) ＝ **1,058 D 行** → 846 行库代码 + 1,094 行测试。
  子代理在 150 轮上限处被打断，本节由主线程接手完成。
- **范围修正**：`gasflow.d`(1595) 与 `gasflow_demo.d`(202) **不在 Wave 2**：
  函数签名普遍是 `ref const(GasState)`/`GasModel gm`，并有 `cast(CEAGas) gm`
  动态下行转换（`gasflow.d:85`），demo 还要 `init_gas_model(".../cea-lut-air-version-test.lua")`。
  `gas/` 非 Lua 部分 23,281 行＝Wave 3，Lua 绑定＝Wave 7 ⇒ 新 ID **T-018**。
- **做了什么**：等熵储气、面积-马赫、正激波、Rayleigh、Prandtl-Meyer、
  斜激波（马赫角/弱支/分离/强支）、皮托压力，以及用 nm 求根反解的
  `M_Rayleigh / PM2 / beta_obl / beta_cone / beta_cone2`；
  反解问题的**函数求值次数**进了语料（同一个根、不同步数＝bracketing 已分叉）。
- **验证**（基准是 D 侧 12,190 行语料，主线程用 probe 头部记录的**同一条命令**重跑并
  `diff` 为空 ⇒ 基准真实可复现；注意必须用显式文件列表编译，`-i` 会因
  `bbla.d:709` 在 `@nogc` 里 `new` 而失败，与 wave-01 记的 brent/newton 同一缺陷）：
  - **39,778 / 40,451 个量逐位相同**；673 个量落在各自容差带内、**0 个越带**；
    最坏 STRICT 残差 `7.8e-15` 相对（`dtan_theta.v`，64 ULP，来自相消放大）。
  - D 的 6 个 `unittest` 块共 **59/59** 条断言复现参考侧判定。
  - `idealgasflow_demo` stdout 与 D **字节相同**。
  - `ctest -R gasdyn` 2/2 通过。
- **本轮由我修的三处**（都不是算法问题，是语言差异）：
  1. 测试把 `int` 传给 `%.17g`。D 的 `std.format` 对整型 `%g` 打印十进制整数
     （substrate 既有规则），C 里这是**未定义行为**，会把 `cone_trace` 行每个字段错位；
     改成 `%d` 后文本与 D 一致。
  2. `trace_cone_f` 作为模板实参在定义之前被使用 ⇒ 前向声明。
  3. **唯一真实分歧已定位而非掩盖**：`cone_trace` 步数 D=13 / 端口=11，
     但两侧收敛到同一个 17 位根 `0.73397862905965938`。
     用 `probes/gasdyn/probe_ds_cv.d` 拆开 `ds_Cv` 的链条证明：
     `t1/t2/pow/prod` 全部与 D 逐位相同，只有 `log`/`pow` 本身差 1 ULP；
     锥角路径上第 7 步残差 D 为 `+1.0269562977782698e-15`、端口为 `-1.2628786905111156e-15`，
     **符号在噪声底翻面** ⇒ Ridder 选择哪半边区间随之不同。
     这是 phase-0 记录的 Phobos/glibc 超越函数分歧经求根器放大，不是转写误差；
     因此逐步 trace 行不参与门禁、步数按"计数"记录，两处判定都写在测试里并带实测数字。
- **产出**：`src/gasdyn/{gasflowexception.hpp,idealgasflow.hpp,idealgasflow.cpp}`、
  `tests/{test_gasdyn_idealgasflow,demo_gasdyn_idealgasflow}.cpp`、
  `probes/gasdyn/{probe_idealgasflow,probe_ds_cv,probe_real_print,probe_tol}.d`、
  `baselines/e587b235/probes/gasdyn_idealgasflow{,_demo}.txt`、`cmake/wave2_gasdyn.cmake`。
  提交 `c4cd448`。
- **新遗留**：T-018（`gasflow.d` 随 Wave 3）；
  子代理 150 轮上限被打断两次（nm/gasdyn），**后续波次应按每代理 ≤800 D 行拆分**，
  并让代理在早得多的位置开始写探针基线，避免"实现完成、验证未完"的状态。

## T-013 / T-010（gpath 部分）— Wave 2 中段，含一个未闭合的验证缺口  (2026-10-14)

- **范围**：`geom/misc/`（univariatefunctions 833、svg 364、kdtree 197、nurbs_utils 172
  ＝1,566 D 行）与 `geom/gpath/`（13 个模块 2,573 D 行）。
- **做了什么**：六个子代理全部在 **150 轮**处被打断（四个在**第一次编译之前**就断了），
  余下的绿由主线程完成。修掉的真实缺陷：
  `gpath_utils.cpp` 两处把 `nelmin::minimize` 写在不带命名空间的 `eil::geom` 里；
  gpath2 代理注册了三个它从未写的测试文件，导致整棵树无法 configure（挡住所有并行阶段）；
  `demo_geom_gpath_gpathd.cpp` 漏掉 D 的 `writeln("a = ", a, ", b = ", b)`（gpath_demo.d:19），
  字节比对把它抓住；`svg.cpp` 的 mm 换算见下方实测条目。
- **验证**（ldc2 现编 D 为基准，Release 全树 **0 编译错误、ctest 57/57**）：
  path 224 行/799 量、line 376/1381、bezier 486/1759、polynomial 548/1892、
  helix 438/1603 全部 **0 差异且数值逐位相同**；
  arc 529 行、1884/1886，2 行走绝对容差带（worst 7/8 ULP）；
  misc 四模块与其两个 demo 亦通过。
- **本轮唯一未闭合的可信度问题（已登记 T-910）**：`modifiedpath` 的 2288 行里有
  **84 行**靠容差带放行，最大一条残差 8.88e-10、打印比值 3.0e6 ULP（按行内尺度），
  比我用实测确立的"1 ULP 点坐标 ÷ dt^阶"模型（opCall/dpdt/d2pdt2 三阶比值均为 **1.000**，
  dt=1.0e-3 取自 `path.d:37,60`）**高出六个数量级**。
  要么带的尺度取错，要么 modifiedpath 真有转写偏差。**不靠调常数决定这件事**：
  在该链条按 `ds_Cv` 的先例拆开定位之前，`geom_gpath_modifiedpath` 的 PASS 不作为已验证。
- **产出**：`src/geom/{misc,gpath}/*`、`tests/{test_geom_gpath_*,demo_geom_gpath_gpathd,
  gpath1_dump.hpp,gpath3_dump.hpp,test_geom_misc_*}`、
  `baselines/e587b235/probes/geom_gpath_*.txt`（8,649 行）、`probes/geom/{misc,gpath}/*.d`。
  提交 `256c6df 509e256 e0af8e6`。
- **新遗留**：T-910（上）；`geom_gpath_{nurbs,svgpath,gpath_utils}` 的测试文件与
  `demo_geom_gpath_gpath_utils.cpp` **尚未写**（语料已在库里，可离线回放）；
  Wave 2 余量：`geom/surface/`＋`geom/volume/`（T-012）、`geom/grid/`＋`openfoam`＋`sketch`（T-011/T-013）。

## T-910 — `modifiedpath` 验证缺口闭合：根因是 atan2 库替换，不是容差带  (2026-10-14)

- **范围**：`geom/gpath/modifiedpath` 重放中 84 条靠绝对容差带放行的行；
  追查链 `RotatedAboutZAxisPath::apply_transform` → `Vector3::rotate_about_zaxis`
  （`src/geom/elements/vector3.cpp:312-318`，D 的 `geom/elements/vector3.d:521-528`）。
- **做了什么**：
  - 按 backlog 要求"拆链条逐级比 `%17g`"，新写 D 探针
    `probes/geom/gpath/probe_mp_rot_chain.d`（324 行语料，**每行同时打印 `%.17g` 与
    16 位十六进制 IEEE-754 位型**），把 rotate_about_zaxis 的 5 个中间量
    （`atan2` → `+dtheta` → `x*x+y*y` → `sqrt` → `r*cos`/`r*sin`）逐个打印，
    并附两条控制：`wrapper`（真走 `Vector3::rotate_about_zaxis`）与
    `path`（真走 `RotatedAboutZAxisPath::operator()`），使手抄链不可能与被测代码走偏。
  - C++ 侧镜像 `tests/test_geom_gpath_mp_rot_chain.cpp` **一次生成两份语料**
    （`eil::atan2` 与 `std::atan2`），各自与 D 基线做整行 diff。
  - **判据结果**：`eil::atan2` 0 行差异；`std::atan2` **29 行差异**；
    第一处不同即 `mp.chain.atan2`
    D `0.78539824050373475` bits `3fe921fb7da9955e`
    glibc `0.78539824050373463` bits `3fe921fb7da9955d`（1 ULP）。
    ⇒ **`modifiedpath` 里确有未发现的转写偏差**，而容差带的尺度是**对的**：
    这条偏差不是"1 ULP ÷ dt^阶"能解释的，因为它根本不是同一 ULP 尺度的量。
  - **放大机理（实测，非推理）**：theta 落在 `PI/2` 零点附近，`cos(theta)≈-7.71e-08`，
    1 ULP 的角度误差（2.22e-16）在该点几乎**不改变** `cos` 的绝对量级，于是
    相对变化达 `2.88e-09`；`x = r*cos` 的绝对残差 `6.28e-16` 再被
    `d2pdt2` 的 `dt=1e-3` 二阶差分放大 `1e6` 倍 → 观测到的 `8.88e-10`。
    探针的 `ctl.model.*` 行把这条定量关系钉住：theta 前进 1 ULP 后
    `cos` 的变号量与观测残差同量级。
  - **修法**：把 Phobos 的 `atan2Impl!double`/`atanImpl!double` 从 `arc.cpp` 的匿名
    命名空间**上移到 substrate**（`substrate/include/eil/math.hpp` 新增 `eil::atan`/
    `eil::atan2`，逐语句转写 `std/math/trigonometry.d` 753-846 与 990-1035，
    系数仍按 D 的 80 位 `real` 字面量写成 `long double` 再窄化）。
    `arc.cpp` 的 `phobos_atan/phobos_atan2` 改为**别名**（`test_geom_gpath_arcsteps.cpp`
    仍按原名调用）；三个 D 侧 `atan2` 站点全部改走 `eil::atan2`：
    `vector3.d:523`→`vector3.cpp:313`、`helix.d:59`→`helix.cpp:53`、`arc.d:92`（已在用）。
  - **`src/gasdyn/idealgasflow.cpp:86` 的 `d_asin` 故意不改**：D 的
    `asin(double)` 是 `atan2(x, sqrt(1-x*x))` **在 80 位 real 里算完再截断**，
    与 double 版 `atan2Impl` 不是同一实现；改它会破坏已实测 0 差异的 40 条
    `beta_obl2`（`v=-nan`）语料。
- **验证**（ldc2 1.40.0 现编 D 为基准；三配置 Release / Debug / `-DEIL_SLICE_CHECK=ON`
  各 **58/58 CTest 全通过**）：
  - `geom_gpath_modifiedpath`：**84 条容差带行 → 0 条**；
    **10794 of 10794 个量逐位相同**，2288 D 行 / 2288 C++ 行 **0 行差异**、11/11 断言。
    这是本轮最硬的证据：修的是**移植侧**，参考侧一个字没动。
  - 全树容差带使用面：**只剩 1 处**（`geom_gpath_arc` 2 行，worst `7/8` ULP 行尺度、
    绝对残差 `2.22045e-16`＝1 ULP，落在已实测的 Phobos/glibc 超越函数分歧内）。
    其余 20 个带数值报告的测试全部 `N of N bit-identical`。
  - `geom_gpath_mp_rot_chain` 新测试：**324/324 行 bit-identical**（含位型列）。
  - helix 改走 `eil::atan2` 后仍 **1603/1603 逐位相同**（说明它的自变量恰好两侧一致，
    替换无副作用）；arc/arcsteps/bezier/line/path/polynomial/polyline/xspline/
    xsplinelsq 全部维持 0 行差异。
  - `bash tools/check_staleness.sh` → `no drift`。
- **顺带修掉的门禁自身缺陷**：`tests/gpath1_dump.hpp:331` 的 `snprintf` 格式串有
  **4 个转换符却只传 3 个实参**，最后一个 `(D %.17g)` 打印的是栈垃圾——
  arc 报告里那句 `arc3.skew.L (D 9.8167635725638538e+252)` 就是它。
  现在补 `worst_dval` 传递，实测打印 `arc3.skew.L (D 1.2650977300272355)`。
  **报告里一个编造出来的数字比不报数字更坏**，因为它看起来像证据。
- **产出**：`probes/geom/gpath/probe_mp_rot_chain.d`、
  `baselines/e587b235/probes/geom_gpath_mp_rot_chain.txt`（324 行，
  sha256 `98078c84…394cfb`）、`tests/test_geom_gpath_mp_rot_chain.cpp`、
  `cmake/wave2_t910.cmake`、`substrate/include/eil/math.hpp`（`eil::atan`/`eil::atan2`）、
  `src/geom/gpath/arc.{hpp,cpp}`（改为别名）、`src/geom/elements/vector3.cpp`、
  `src/geom/gpath/helix.cpp`、`tests/gpath1_dump.hpp`。
- **新遗留**：**T-911**（Phobos 的 `exp/sin/cos/tanh/sinh` 同样是纯源码实现，
  `univariatefunctions` 那 42 行 1 ULP 分歧未必"不可消除"，Wave 3 的 `gas/` 大量用
  `exp`，须先实测再决定是否为它们建 replica）；
  **T-912**（本轮教训入规：D 侧 `std.math` 的函数**不能默认等于同名 C 函数**，
  每个新转写的超越函数调用点都要先做位型级探针）。

## T-015 / T-012 stage A / T-011 范围 — 门禁复核、两个抽象基类、两处清单更正  (2026-10-14)

- **范围**：`gate.sh` 在 `3D/sod-shock-tube/{sg,usg}` 两条链上的自检；
  `geom/surface/parametricsurface.d`(94) + `geom/volume/parametricvolume.d`(304) 两个抽象基类；
  backlog 中 T-011/T-012/T-013 三条的范围核对。
- **做了什么**：
  - **T-015 第一半**：`bash harness/gate.sh /home/tang/packages/gdtk/build/bin
    3D-sod-shock-tube-sg 3D-sod-shock-tube-usg` → 两条链 **PASS**。可比产物
    **各 17 个，onlyD=0、onlyC=0**（用 `compare.py` 的 `list_artifacts()` 数出来的，
    不是只看汇总行）。gzip 只比**解压后的 payload**（容器含 mtime，D 自己两次运行都不同）：
    sg `flow 181,027` / `grid 68,303` 字节，usg `flow 181,024` / `grid 145,581` 字节；
    `final-t` sg `0.00060314`、usg `0.000600988`。
  - **T-015 第二半不做**："两条链在 C++ 侧可跑"需要候选 `e4shared`，那是 Wave 5 的产物；
    Wave 2 无论怎么折腾都产生不了这条证据 → 该条保持 `[~]`，不标完成。
  - **T-012 stage A**：两个抽象基类由主线程写，理由是 surface 有 **15** 个、volume 有 **8**
    个子类覆写同一套 `dup()/opCall()/toString()/classString()` 契约，让并行代理各定各义
    就是把它分叉（Wave 1 集成返工的同类问题）。九个差分**逐条转写、不合并**：
    混合偏导的远端单侧支（D:214/:235/:256 用 `(f0-f-1)/d`）与纯二阶导的三点支
    （D:144 用 `(f0-2f-1+f-2)/(d*d)`）**结构不同**，合并成一个带阶数的 helper 就是改行为。
  - **两处清单更正**（读 import 实测得出，不是推测）：
    1. `surface/gmopatch.d`(171) 与 `surface/meshpatch.d`(100) 移出 T-012 → 挂 **T-011**。
       前者 `import geom.grid.sgrid` 且 :87 `new StructuredGrid(...)`，
       后者 :23 构造入参就是 `ref StructuredGrid grid`；在 Wave 2 做它们必须凭空造
       StructuredGrid 桩 —— 与 T-018 移出 `gasflow.d` 同一条理由。
       T-012 实范围因此是 **4,216 D 行**（surface 2,628 + volume 1,588）。
    2. T-013 原写作"含 `sketch.d` 964"是**错的**：task-log 的 T-013 节范围里从来没有 sketch，
       余量条目也写明它未转 → 拆出 **T-019**（sketch 964 + `rotating_a.d` 49 +
       `drifting_eye.d` 65 + `sketch_demo.d` 83），T-013 从 `[x]` 降为 `[~]`。
       这是"同一件事在两处表述不同"的实例，正是把 backlog 定为唯一清单所要防的。
- **验证**：
  - `geom_surface_parametricsurface`：**29 of 29 行字节相同、0 differ**
    （`area()` 在 9 组 (nr,ns) × 3 种退化/歪斜补丁、`dup()` 后求值、VTK 文件**全文**，
    含 `ntPts=1` 时 `1.0/(nrPts-1)` 产生的 inf/NaN 行）。
  - `geom_volume_parametricvolume`：**2165 of 2165 行字节相同**。语料是
    6 个扫掠值（`0, 0.001, 0.5, 0.999, 0.9995, 1`）的三点笛卡尔积 × 9 个差分访问器
    = 216 点 × 9，**故意冗余**：混合偏导调用一阶导、后者自己又分支，
    只有这样才能让 D:144 与 D:214 两种不同模板**可观测地**不同，而不是"看起来合理"；
    外加 216 点本体与两份 VTK 全文。
  - Release 下 `ctest -R "parametricsurface|parametricvolume"` **2/2**；
    `gdtk` 树全程只读（D 探针一律编 `/tmp/gdtk_shadow` 副本）。
- **产出**：`src/geom/{surface/parametricsurface,volume/parametricvolume}.{hpp,cpp}`、
  `probes/geom/{surface, volume}/probe_parametric*.d`、
  `baselines/e587b235/probes/geom_surface_parametricsurface.txt`（29 行，
  sha256 `bd616e42…160378`）与 `geom_volume_parametricvolume.txt`（2,165 行）、
  `tests/test_geom_{surface_parametricsurface,volume_parametricvolume}.cpp`、
  `cmake/wave2_geom_surfvol.cmake`、`reports/wave-02-surfvol-brief.md`、
  backlog 的 T-011/T-012/T-013/T-015 更正与新 ID T-019。提交 `e564688`。
- **新遗留**：T-019（sketch，从 T-013 更正出来）；gmopatch/meshpatch 改挂 T-011；
  T-015 保持 `[~]` 直到 Wave 5 产出候选 `e4shared`。

## T-013 续 / T-012 stage B / T-913 — gpath 回放补齐、15 个曲面/体模块、`to!double` 规则重建  (2026-10-14)

- **范围**：`geom/gpath/{nurbs,svgpath,gpath_utils}` 三个缺失的回放测试 +
  `gpath_utils_demo`；`geom/surface/` 11 个模块 + `geom/volume/` 4 个模块；
  `substrate/include/eil/conv.hpp` 的 `to!double`。
  D 侧输入：gpath 三模块 117 D 行、surface/volume **2,542 D 行**、
  语料合计 **19,995 行**（surface/volume）+ **1,597 行**（gpath 三模块）。
- **做了什么**：
  1. **T-013 余量闭合**：三个代理各写一个回放测试，全部对着**已入库**的 D 语料回放，
     不新建基线。nurbs 的静态字段串扰（`mN` 被后一个构造器 `p+1` 缩小）按 D 原样复现，
     包括 `nurbs.static.keep5` 那条 RangeError 文本。
  2. **T-913（新发现并已修）**：两个代理各自独立测到 `to!double("#c")` 的错误文本错——
     D 给 `no digits seen for input "#c".`，端口给 `error converting input to floating
     point ...`。该分支当初是**从一条语料行（"not a number"）推广出来的**。
     没有只补 `#c`，而是按 **443 个操作数**重测规则后重写。测出的 7 条 D 特有语义
     全写进头注释并被语料钉住：`+nan` 可转但 **`+inf` 不可**（`-inf` 可）；
     Phobos 只匹配三字母 `inf`，故 `infinity`→unexpected `i`；`1e` 是
     `Unexpected end of input` 而 `1e+` 是 `No digits seen.`；`_` 只在紧跟数字时
     才是分隔符（`1_2`/`1_`/`1_e5`/`0x1_p1` 可，`1__2`/`1._5`/`_1`/`1e5_` 不可）；
     hex 尾数遇 `p` 即停（故 `0x1p1p1` 报 unexpected `p`）；`0x1.2.3p1` 报的是
     `exponent is required` 而不是 unexpected `.`；溢出才是 Range error，下溢只是 0。
     操作数与结果一律 **hex 编码**（`tests/test_data/conv_inputs.hex`），因为多个操作数
     含换行/制表符，而 D 的消息里**只有 `\n` 被转义成两字符 `\n`、`\t` 原样**——
     明文列表会让语料"按行"变得偶然并把行错配给错误的操作数。
  3. **T-012 stage B**：15 个模块并行转写。两个**我写错、由实测纠正**的简报条目：
     - 我把 `classString()` 写进 surface/volume 的虚函数契约。它**只存在于 `geom/gpath`**
       （`grep -c classString gdtk/src/geom/{surface,volume}/*.d` 全 0），标 `override`
       编不过。简报已更正；相关头注释把它标成"非 D 符号"的 helper。
     - 我说 `beziertrianglepatch.d` 是 `^^` 大户。它只有 3 处且全是**整数臂**
       （`typeof(3^^2)==int`），其 26 个模块内 `iota` 全是整数降序，
       累加写法恰好同值；浮点 `iota` 的判别语料在 `substrate_iota`。
  4. **保留而不"修正"的 D 行为**（都带 file:line）：`bezierpatch.d:40` 复制时
     `m = other.n` 而 `dup()` 重算；负数组长度要在 `size_t` 转换**之前**捕获成
     D 的 `Error "Memory allocation failed"`；CoonsPatch 角点检查只报第一个开角；
     `channelpatch` 零叉乘情形得 `[pA,pA,pB,pB]` 而非 NaN（`vector3.d:362-370`
     下溢置零）；`twosurfacevolume.d:110` 用字面量 `1.0-1` 缩放 face0，"i" 支因此丢弃它。
  5. **`SlabVolume` 拷贝构造删除而非转写**：D:32 在 super()（D:33）赋值成员之前
     调用继承的 `face0123(0.0,0.0)`，实测 **SIGSEGV、exit 139、无可捕获对象**；
     且 `grep -rn "new SlabVolume" gdtk/src` 三处命中全是双参构造，无一处用拷贝构造。
     判据与复跑命令记在 `src/geom/volume/slabvolume.hpp:45-50`。
- **验证**（基准全是 ldc2 1.40.0 现编 D；三配置 Release / Debug / `-DEIL_SLICE_CHECK=ON`）：
  - `substrate_conv_failtext`：**443/443 行字节相同**。
  - `geom_gpath_nurbs` **195/195 行、655/655 量逐位相同、0 行用容差带**、1/1 断言；
    `geom_gpath_svgpath` **818/818 行、2802/2802 量**、3/3 断言；
    `geom_gpath_gpath_utils` **564/564 行、957/957 量**；其 demo **43 行字节相同**
    （507 字节，两侧 md5 `e0fa70ef7cfe03ec6fe1e37bcef59569`，`cmp` 无差异字节）。
  - surface/volume 15 模块逐模块行数**全部 N of N 字节相同、0 行用容差带**：
    bezierpatch 310、coonspatch 333、channelpatch 820、cubepatch 1604、
    sweptpathpatch 724、beziertrianglepatch 268、subrangedsurface 273、
    ruledsurface 598、nozzleexpansionpatch 354、nurbssurface 200、
    meshvolume 2167、subrangedvolume 5202、slabvolume 2227、twosurfacevolume 6608
    ＝ **19,995 行中 0 行差异**。
  - 门禁可信度自检（beziertrianglepatch 代理做的"故意改坏"实验）：删掉 D:104 的
    `if (w<0.0) w=0.0` → **21/268 行变红**；把 D:112 的远端边界 `-1` 放宽成 `0` →
    未捕获异常 exit 134。即该语料**确有判别力**，不是"怎么改都过"。
  - 全树：Release **77/77**、Debug **76/76**、slice-check **76/76**（76 时是
    slabvolume 的注册还等 sweptsurfacevolume 落地，现已 77）。
  - `tools/check_staleness.sh` → `no drift`；`gdtk` 树零写入
    （`find gdtk/src/geom/surface -newermt '-2 hours'` → 0 files）。
- **产出**：`src/geom/{surface,volume}/*`（15 模块）、
  `tests/{test_geom_gpath_nurbs,test_geom_gpath_svgpath,test_geom_gpath_gpath_utils,
  demo_geom_gpath_gpath_utils,test_substrate_conv_failtext,test_geom_surface_*,
  test_geom_volume_*}.cpp`、`probes/substrate/probe_conv_failtext.d`、
  `probes/geom/{surface,volume}/*.d`、`tests/test_data/conv_inputs.hex`（443 操作数）、
  对应 `baselines/e587b235/probes/*`、`substrate/include/eil/conv.hpp`（重写）、
  `cmake/wave2_{t910,geom_surfvol,btp_surf2,vol2,nurbsvol}.cmake`、
  `reports/wave-02-surfvol-brief.md`（含两处更正）。提交 `1c4cd04 199c8fd`。
- **新遗留**：T-913（已完成，登记于此）；**T-914** `eil::Array::set_length(size_t)`
  表达不了 D 的有符号 `arr.length = L`（代理在 `bezierpatch.cpp:26-39` 本地绕开，
  需 substrate 侧定形）；**T-915** `sweptpathpatch.d` 的
  `new SweptPathPatch(null, cB)` 在 D 侧 SIGSEGV（exit 139，不可捕获），
  端口不加防护、语料无该行；**T-012 余量** spherepatch/aopatch/controlpointpatch/
  surface_demo/sweptsurfacevolume/wedgevolume/tfivolume/volume_demo/nurbsvolume。

## T-917 / T-918 / T-919 — substrate 的三条 NaN 与断言规则，以及"代理报的数我自己复跑"  (2026-10-14)

- **范围**：`substrate/include/eil/math.hpp`（NaN 极性域、Phobos 的 atan/atan2、
  双 NaN 操作数的取值规则）、`substrate/include/eil/error.hpp`（`assert` 消息）、
  `src/geom/elements/vector3.{hpp,cpp}`。这一节的重点不是"又修了三条"，而是
  **三条里有一条的根因记录是错的、一条的旁证注释是错的，两处都由复测纠正**。
- **T-919 的结论：行 178 不是 D-vs-C++ 缺陷，是"参照系本身随 -O 变"**。
  IEEE 754-2008 6.3 不规定两个 NaN 相加返回哪一个，x86-64 SSE 按**寄存器角色**回答
  （`addsd xmm_dst, xmm_src` 返回 dst 那个 NaN），而谁进 dst 是寄存器分配决定的。
  实测（`probes/geom/grid/probe_grid_nanadd.d`，638 行位型，基线可 0 差异复生成）：
  - ldc2 1.40.0 自身：同一份源在 **-O1 比 -O0 基线少 90 行一致、-O2/-O3 少 103 行**。
    动的组恰是 `add.ref`/`add.ptr`/`opplus_eq`/`scale`/`opmul_eq` 及其 `walk` 行；
    不动的是 `opplus`/`opminus`/`opminus_eq`/`opdiv`/`opdiv_eq`/`add.fac`。
    ⇒ 若基线当初用 -O1 抓，行 178 就会写 `x=nan`，**未打桩的 Release 反而是对的**。
    所以这里没有"值层面的规则"可发现，只有"约定哪个优化级别"。所有基线是 ldc2 **-O0**
    抓的，故 -O0 的取值是契约，端口把 `eil::add_rhs_nan`/`eil::mul_rhs_nan` 写出来。
  - 钉住后：**638/638 与 643/643 在 Debug(-O0)、Release(-O3)、`-DEIL_SLICE_CHECK=ON`、
    手工 `-flto -O3` 四种构建下全部字节相同**。
  - 反向验证（把桩去掉重编）：nanadd 在 -O0 **差 36 行**、-O3 **差 67 行**、
    -flto -O3 **差 90 行**，grid 行 178 在 -O3 与 -flto 各差 1 行、-O0 为 0 行。
    即 `scale`/`operator*=` 在 g++ -O0 上取的是**接收者**，与 ldc2 -O0 的**因子**相反
    ——这条是"Debug 与 Release 曾经不一致"的真正来源。
- **纠正了两处过时记录**（代理无权改、由主线程改）：
  1. `provenance/fidelity.yml` 里那段 `addpd` 理论。它写"g++ -O2/-O3 把
     `x += other.x; y += other.y` 合成一条 `addpd`，取了 source1 的 payload"。
     实测否证：给未打桩的端口加 `-fno-tree-slp-vectorize` 后，
     `objdump -C -d vector3.cpp.o | grep -c addpd` = **0**，而行 178 **照旧红**
     （`x=nan` vs D `x=-nan`），nanadd 反而从 67 行恶化到 **103 行** differ。
     向量化从来不是机制，该段已改写成上面那条寄存器角色规则。
  2. `substrate/include/eil/math.hpp` 的旁注里"`std::numeric_limits<double>::quiet_NaN()`
     在本机是 `0xfff8000000000000`，符号位是 1"。**这是我在 T-917 写错的**，实测
     g++ 11.4 在 -O0/-O2/-O3 三档都是 **`0x7ff8000000000000`（符号位 0）**，
     `signaling_NaN()` = `0x7ff4000000000000`，而 `0.0/0.0` 与 `sqrt(-1)` 才是
     `0xfff8000000000000`；ldc2 的 `double.nan` 也确实是 `0x7ff8...`（语料
     `nan.init double.nan` 行），两侧一致。⇒ 旧 `canonical_nan` 并没有"发出 -nan"这个
     二次缺陷；它的真错只有两条：**范围**（把操作数带来的 NaN 也改写掉，而 Phobos 原样
     传播）和 **asin/acos 的极性**（这两要 -nan，被写成 +nan）。注释已按实测重写，
     并把两种极性问题改成"必须从位型构造、不能靠 quiet_NaN() 猜"。
- **代价（自己测，不采信代理的数）**：-O3、`-ffp-contract=off`，
  `Vector3 cbc; cbc.set(v0); cbc.add(v1..v3); cbc.scale(0.25)` 循环 2e8 次，
  钉住与未钉**交替**各跑 6 轮：钉住中位 **14.5 ns**（最小 12.88），
  未钉中位 **10.4 ns**（最小 9.43）⇒ **+37% ~ +40%**。
  代理先前报的 8.75 vs 7.10 ns（+23%）**在本机复现不了**，方向对、量级偏小。
  这个代价买的是"同一份二进制在四种构建里与 ldc2 -O0 基线逐字节一致"，
  按需求第 3 条（不得为求解器行为做优化）保留，不做条件编译绕过。
- **T-918 已闭合的 substrate 面**：`d_assert`（裸 `Assertion failure`）/
  `d_assert_msg`（原样消息）+ `eil_assert`/`eil_assert_msg` 两个宏；
  `grid.cpp:44-48` 的本地副本改为**转发**。fidelity.yml 里"SUBSTRATE GAP 待主线程登记"
  一段随之作废，已改写为 CLOSED。
- **验证**：全树 **92 个测试三配置全绿**（Release / Debug / slice-check，各 100%）；
  `tools/check_staleness.sh` → `no drift`；`provenance/manifest.yml` 重生成后
  `src/geom/elements/vector3.{cpp,hpp}` 第一次有了 `fidelity`/`variants:
  [complex_numbers]`/`verified_by` 记录（此前是 gen_manifest 的默认空值）。
  `gdtk` 树零写入。

## T-012 完成 / T-011 段 G1+G3 — 最后一个体单元、两个 demo、以及一份"半成品"的正确处置  (2026-10-14)

- **范围**：`geom/volume/tfivolume.d`(161) 与两个目录级 demo
  （`surface_demo.d` 55、`volume_demo.d` 50）→ T-012 收口；
  `geom/grid/grid.d`(490) → G1、`geom/grid/paver2d.d`(52) → G3；
  `geom/grid/paver.d`(1737) → G2 **未完成，本轮做了处置而非继续**。
- **这一节的重点是怎么对待 4 个被配额打断的代理留下的东西**。规则只有一条：
  **验过的入库，未验的离开构建图但保留证据**。做法是把 `paver` 的三个产物
  （`paver.hpp` 268、`paver.cpp` 1720、`test_geom_grid_paver.cpp` 624 行）连同一份
  现成的注册片段移到 `wip/geom-grid-paver/`，并把 `cmake/wave2_grid_g2.cmake`
  还原到已提交状态——它的测试是 `if(EXISTS ...)` 自注册的，所以文件挪走即从构建图消失，
  树回到全绿；**没有**删除任何人的实现，也**没有**提交一条红测试冒充进度。
  语料侧的证据（`probes/geom/grid/probe_paver.d` 与 1,961 行 D 基线）留在原位，
  因为那是参照系数据，与实现完成度无关。
- **paver 的真实状态用一行说清**（下一位接手者不必重新发现）：回放只发出
  **219 行**，其中 **217 行字节相同**，停在基线第 219 行 `sec3.ipr.emptyrow|0` 之后，
  下一条应是 `make.npoints|4` ⇒ 缺的是**测试侧的 `make.*` 及之后各节生成代码**，
  不是 paver.cpp 的算术。所以下一步是补节，不是改函数体。
- **wip 里捞出一条 substrate 级发现，登记为 T-920**：`wip/.../paver.cpp:8-18` 记录
  g++ 在 -O2/-O3 会把同一自变量的 `std::sin`+`std::cos` 合并成一次 glibc `sincos()`。
  我自己复跑否证了"这只是 paver 的局部问题"：Release 下有 **11 个已入库 .o 引用
  `sincos`**（`nm -u`；注意 `nm -D` 看不到，我第一次就错了），含
  `vector3.cpp`（`rotate2d`、`rotate_about_zaxis`）、`gpath/arc.cpp:119`、
  `gpath/helix.cpp:70`、`grid.cpp`、`projection.cpp`、`idealgasflow.cpp`。
  2×10⁶ 个 0..2π 均匀角的对照实验：-O0 **0 差异**，-O2/-O3 **cos 差 1374 次、
  sin 差 1353 次**（约 1/1466），最大绝对差 `1.1102230246251565e-16`＝1 ULP；
  把自变量先经 `volatile` 拷贝即可阻止合并（同一实验的 shape_B，差异计数 0）。
  现有语料全绿属于"没踩到"，不是"不存在"。
- **验证**（基准全是 ldc2 1.40.0 现编 D，三配置 Release / Debug / `-DEIL_SLICE_CHECK=ON`）：
  - `geom_grid_grid` **643/643 行字节相同**，四档构建一致（含 -flto）；
    `geom_grid_nanadd` **638/638**（T-919 的守卫）。
  - `geom_grid_paver2d` **480/480 行 + 396/396 坐标**逐字节相同。`fill_interior` 需要的
    `USGFace`/`USGCell`/`USGCell_type`/`makeFaceTag` 全在**未转写的 usgrid.d**（G5）里，
    所以它按模板转写并同时实例化两种容器写法，两者结果与 D 语料行 210/213/217 一致。
    保留而不修的 D 行为都有语料行号：空边界时 `0 .. n-1` 下溢 → D 的
    `ArrayIndexError "index [0] is out of bounds for array of length 0"`（行 fi.n0，
    不加防护）；查未插入的 face tag → D 在**第二个 writeln 之前**死于 `RangeError`，
    故 `[TODO]` 那行在该用例里**缺席**（行 fi.miss）；`NodeType` 越界的 D 打印是
    `cast(NodeType)7` 这种**cast 表达式本身**，不是裸数字（实测）。
  - `geom_volume_tfivolume` **4464/4464 行**、2/2 D unittest 断言、2/2 结构检查；
    `demo_equiv_geom_surface_surface`（18 行）、`demo_equiv_geom_volume_volume`（13 行）
    字节相同。**T-012 到此无缺失模块**（脚本逐个 `.d` 查同名 `.cpp`/`.hpp` 核对，
    余下只有 `package.d` 聚合文件与已划归 T-011 的 gmopatch/meshpatch）。
  - 全树：**92 个测试，Release 100% / Debug 100% / slice-check 100%**；
    `tools/check_staleness.sh` → `no drift`；`provenance/manifest.yml` 重生成
    （paver2d.hpp/cpp、tfivolume.hpp/cpp、vector3 首次带 `verified_by`）。
  - 顺手核掉的三条过时记录：`fidelity.yml` 的 `addpd` 理论（否证）、
    `fidelity.yml` 的 "SUBSTRATE GAP 待登记"（T-918 已闭合）、
    `fidelity.yml` 的 "parametricvolume.cpp:235/277 仍持裸 FILE*、throw 路径丢字节"
    （10b6bde 已加 `File_guard`，`grep -n File_guard` 在 volume/surface 两个基类各命中，
    throw 路径 323 字节由语料行 2166 钉住）。
  - `gdtk` 树零写入；D 侧探测一律走 `/tmp/gdtk_shadow` 副本。

## T-920 — 一个"编译器替我做了决定"的类别：cos/sin 对被融合成 sincos  (2026-10-14)

- **范围**：`substrate/include/eil/math.hpp` 新增 `eil::sin`/`eil::cos`；
  改道 7 个源文件站点 + 9 个测试文件；新门禁
  `probes/substrate/probe_sin_cos_pair.d`（415 行）+ `tests/test_substrate_sin_cos_pair.cpp`
  + `tests/test_data/sincos_angle.txt`（415 角）+ `cmake/wave2_t920.cmake`。
- **它是怎么被发现的，以及为什么第一轮的说法是错的**：4 个并行代理被配额打断后，
  `wip/geom-grid-paver/paver.cpp:8-18` 里留着一条"g++ 把 `sin`+`cos` 合并成 glibc
  `sincos()`，与 ldc2 的两次独立调用差 1 ULP"。我先照这条登记 T-920，登记前**没有**验证
  "ldc2 不合并"这半句 —— 一验就错：LLVM 有同名 pass，ldc2 -O1 起一样合并。
  于是这条从"语言替换缺陷"改成"优化级别契约缺陷"，与 T-919 同构。
  分类方法（不是比大小，是问"哪种形态能同时复现一行的两个位型"）：
  415 个角 × 两个形态 × ldc2 两个优化档 →
  -O0 = separate-only 401 / sincos-only 0 / 两者皆可 14；
  -O3 = separate-only 0 / sincos-only 401 / 两者皆可 14。
  **401/415 行在参照系自己内部随 -O 移动**，所以不存在"哪个是 D 的行为"这种值层面规则，
  只有"基线是 -O0 抓的"这条契约。
- **站点清点方式**：不是 grep `std::sin`，而是 `nm -u *.o | grep sincos` ——
  前者会漏（只有成对且同自变量才融合），后者是结果侧证据。
  Release 下改道前 **11 个 .o** 引用 `sincos`，其中 **4 个是测试自己的对象**
  （`test_geom_gpath_arcsteps`、`test_nm_newton`、`test_substrate_phobos_bits`、
  `test_geom_gpath_mp_rot_chain`）——即门禁的算术本身走合并形态，
  只是语料里的角度恰好都在 ctrl 集里才没红。改道后 **0 个**（-flto 构建亦 0）。
- **保留的历史**：`src/geom/volume/wedgevolume.cpp` 是本站最早踩到并自行绕过的地方
  （-O3 下它 4594 行语料红 **20 行**，局部用 volatile **函数指针**）。本次删掉那份本地
  副本、改为转发 `eil::cos`/`eil::sin`，把它的测量数据留在注释里。
  同一规则留两份必漂移，这是 T-910 已经付过学费的教训。
- **验证**：
  - 判别力（关键，语料不能只是"怎么改都过"）：**去掉** wrapper 后 Release -O3
    **401 行 differ / 14 行相同**；装回后 **415/415**。
    Debug(-O0) 下两侧同值，符合"融合只发生在 -O1+"的分类。
  - `substrate_sin_cos_pair` 在 Debug / Release / slice-check / 手工 `-flto -O3`
    四档构建全部 **415/415 字节相同**。
  - 全树回归：改道前后都是 **93 个测试**（+1 是本条新增），
    Release **93/93**、Debug **93/93**、slice-check **93/93**、`-flto -O3` **93/93**。
    改道**没有**让任何原绿行变红——这是可预期的：原绿行必然满足"合并值==分别值"
    （否则它们在 -O3 下就是红的），换成分别调用只可能修复不可能引入。
  - 代价：一次 cos+sin 对 **11.97 ns → 14.94 ns**（+22%，-O3、`-ffp-contract=off`、
    2×10⁷ 次、两轮交替）。
  - `tools/check_staleness.sh` → `no drift`；`provenance/fidelity.yml` 记 T-920 规则块
    并把 `ctest:substrate_sin_cos_pair` 加进 `vector3.{hpp,cpp}` 的 `verified_by`。
  - `gdtk` 树零写入；D 侧一律编 `/tmp/gdtk_shadow` 副本。
- **复跑本条判据的命令**：
  `g++ -O3 -ffp-contract=off -std=c++17 -I substrate/include` 编译
  任一含 `std::cos(x)`+`std::sin(x)` 同自变量的 TU，`nm -u` 看是否有 `U sincos`；
  以及 `/tmp/probe_scp -O0 vs -O3` 语料 diff（401 行移动）。

## T-011 段 G2（`paver.d`）/ T-019（`sketch.d`）— 被打断的作业怎么收尾，以及收尾时捞出的两条规则  (2026-10-14)

- **范围**：G2 = `geom/grid/paver.d`(1737) 从 `wip/` 半成品推到全绿；
  T-019 = `geom/misc/sketch.d`(964) + `sketch_demo.d`(83) 从零到全绿。
  两者各由一个子代理承担，主线程负责**独立复跑**、把结论入 `fidelity.yml`、
  并把它们途中撞出的规则缺口钉成 T-920/T-922/T-925/T-926。
- **G2 的实际缺口与简报预判一致**：`wip/` 里的 `paver.cpp`（1720 行）算术基本是对的，
  缺的是**测试侧的语料生成代码**——交给它时实测只发出 219/1961 行。
  补了 1742 行生成器（`grpMake/grpNode/grpRow/grpSmk/grpSeam2/grpPave` + 四种边界形状
  square/lshape/wedge/circle）后：
  **1961 行全发出、1961 行字节相同、0 差异**，Release / Debug / slice-check 三档一致。
  **参照系没被碰过**：`probes/geom/grid/probe_paver.d` 与 1961 行基线的 sha256 仍是
  `d96d146f…`（自主线程那次提交 `a8027e8` 起未变）——这条很重要，
  它排除了"改基线去迁就实现"这一类失败。
  实现侧只改了两处，每处都有行号作证：
  1. 删掉本地 `d_sin/d_cos` volatile 包装，改调 substrate 的 `eil::sin`/`eil::cos`
     （T-920）。被 `sec2.lrm*`/`sec2.ulv*`（30 行）与 `pave.circle6.*` 在 -O3 下钉住。
  2. `point_dist`(:171) 与 `norm`(:202) 的外层 `^^0.5` 改 `eil::sqrt`，内层 `^^2` 仍 `eil::pow`
     ——**这就是 T-925 的来源**，那两行（145–146 `sec2.ulv3.0/1`）曾是全树唯二红行。
- **保留而未"修正"的 D 行为**（都带行号）：`findFace` 缺 `break` 与 `[0,0]` 的
  `reverse_case`（`make.q01nd`、`make.q56.afterzero`）；`LeftorRight` 未初始化
  `int ans` 在 NaN 行列式下取 0（`sec2.lr.nan`）；`midpoint` 不往 POINT_LIST 追加
  （`sec2.mid.listlen=0`）；`store_node_cat` 的黏性；`cell_type` 打印 `cast(CellType)7`；
  `node.one.throw` 里 size_t 下溢文本带着 `18446744073709551615`；
  `paver.d:1378/1397/1421` 两个死变量 `double move`/`size_t cell_index` 原样留着
  （它们正是两条 `-Wunused-but-set` 警告的来源，留着比删掉更符合"无行为改动"）；
  `node.tol0/1/2` 回放的是 ldc2 **折叠后**的位型（T-909）。
- **T-019 的规模与门禁**：`sketch.hpp` 287 / `sketch.cpp` 1155 行；
  探针 `probes/geom/misc/probe_sketch.d` 505 行 → 语料 **682 行**。
  主线程**独立复抓**：`ldc2 -O0` 重跑该探针，682 行与原基线 **0 行差异**（不是采信代理报数）。
  demo 侧 stdout **恰好 2 行 / 62 字节**，三个 SVG 以**文件逐字节**比对
  （9,739 / 1,955 / 8,856 字节，与主线程先前实测的 md5 一致）。
  注册 5 条测试：`geom_misc_sketch`、`demo_equiv_geom_misc_sketch`、
  `demo_artifact_geom_misc_sketch_test{,2,3}`。
  `sketch.d` **没有任何 unittest 块**（grep 计数 0），所以门禁全靠语料 +
  13/13 条 smoke 断言；251/251 个内嵌数值逐位相同，**最坏残差 0（0 ULP）、没有容差带**。
- **成员数目的核对**（需求第 5 条"无缺失"的正面证据）：代理按主线程简报里"28 个"的说法
  复查，实测该正则命中 **27** 个——26 个 `Sketch` 方法 + `Extents::set` 1 个；
  漏掉的是 `toString`，因为它以 `override` 开头而不匹配正则。
  D↔端口的**名字集合差为空**（只有 `this`→构造函数、`~this`→析构）。
  ⇒ 简报里那个"28"是我按正则数的，**数是工具给的结论、不是事实**；已在简报中改为实测口径。
- **T-920 与 T-909 在同一处相撞**（简报 §2 当初就是要代理别猜、去量）：
  `sketch.d:128-129` 的 `cos/sin(30.0/180.0*PI)`，实测折叠
  `3fe0c152382d7366` vs 运行期 `3fe0c152382d7365`（1 ULP），
  而 ldc2 真正写进投影矩阵的是**折叠**那对（`3febb67ae8584caa` / `3fe0000000000000`）
  ⇒ 端口照抄折叠位型，且该站点没有运行期 sin/cos 对可融合。
  `1.0/(2.0*sqrt(2.0))` 实测折叠==运行期，保留算式。
  另记一笔：D 的 `real.max` 放进 `double[]` 会**溢出成 +inf**，
  端口操作数表初版写 `DBL_MAX` 因此差一行（T-925 的语料里撞到）。
- **代理报出、由主线程登记的 substrate 缺口**（它们被明令禁止自己改 substrate）：
  `to!string(结构体)` 无 `toString()` 派发（`format.hpp:139` 是 static_assert）、
  `eil::Slice<T>` 形参无法表达 D 的动态数组默认实参、无 `readln()`、无 libplot 声明
  → 归 **T-926**；libplot 那条与既有 **T-921** 同源。
  代理还报"本文件两处 `Error` 与 `univariatefunctions.cpp:223-230,274` 的写法冲突"——
  该冲突在本次作业里**已被 T-922 的扫描消掉**（那 11 处裸 `new Error` 不再带 `__FILE__`）。
- **主线程复核汇总**（不采信自报）：三配置全树 **100/100**
  （Release / Debug / `-DEIL_SLICE_CHECK=ON`；本轮从 93 起新增 7 条：
  paver 1、sketch 5、nm_stmatrix_fails 1）；`check_staleness.sh` → `no drift`；
  `provenance/fidelity.yml` 补 `paver.{hpp,cpp}`、`sketch.{hpp,cpp}` 四条记录后重跑
  `gen_manifest.py`。`wip/` 目录已空并删除——半成品在门禁下干净落地，没有靠降低标准。

## T-922 / T-923 / T-925 / T-914 — 一条"给代理写简报"的支线，扫出四个端口级问题  (2026-10-14)

- **起点很小**：给 T-019（sketch）写简报需要一张表——`sketch.d` 那 8 个 `throw` 里哪些是
  `Exception`、哪些是 `Error`。我**没有**按构造器签名推，而是写了
  `probes/substrate/probe_throw_identity.d`：对每种 D 侧写法在运行期打印
  `classinfo.name / .msg / .file / .line / toString()` 首行（**15 行，重跑 0 差异，
  行内不含地址**——回溯按设计砍成首行，那是既有的 T-903）。
- **T-922（端口系统性缺陷，已闭合）**：量出来发现 D 的 `new Exception(m)` **带**调用点
  （且 `.file` 就是**编译时传给编译器的那条路径原文**），而 `new Error(m)`
  **不带**：实测 `.file==""`、`.line==0`，`toString()` 打成 `object.Error@(0): msg`。
  端口里 `ErrorBase(...)` 带 `__FILE__/__LINE__` 的站点共 **84 处**（`ExceptionBase` 那
  15 处是对的，不能动）⇒ **83 处改掉、1 处保留**：
  `geom/grid/grid.cpp:76` 对应 D 的 `final switch` 越界，抛
  `core.exception.SwitchError`，**实测它带 `switch` 所在文件行号**，删掉才是错。
  分两批改：第一批 40 处按消息字面量能直接对上 D 的 `new Error(`；
  第二批 43 处（含 5 处被 T-914 合并掉的本地副本）按站点 `// D:NN` 引用读 D 的**类**，
  同族收敛（`aopatch`/`coonspatch`/`tfivolume`/`arc` 的开角检查家族）再补一批。
  **计数方法本身也是一条教训**：用"行窗口 3 行"数出来是 82→42，用**括号配对逐调用**
  数出来是 84→39——差的正是**参数换行**的调用，`arc.cpp:130/158` 就是这么漏掉的。
  可核对的历史：`a1800ca`(84) → `050ab29`(44) → `f9b633e`(39) → `86c7a78`(1)。
  顺带修掉一条**说反话的注释**：`util/lua_service.hpp` 原写"`ErrorBase` 派生自
  `ExceptionBase`，下游 `catch (ExceptionBase&)` 会吞掉它"，而 `error.hpp:91-98` 与
  wave-01 教训第 1 条正是相反的约定（两者是兄弟）；照旧注释写出的 handler 永远不触发。
- **T-923（假门禁）**：全树扫"`throw` 站点里的字面量消息能否在 D 树 grep 到"，
  **100 个带字面量的站点里 97 个逐字命中，3 个在 D 里根本不存在**，全在
  `nm/stmatrix.d` 的读取器——因为**这条函数的失败路径一条语料都没有**。
  实测 D 真抛什么：打不开文件是 `std.exception.ErrnoException`
  `Cannot open file \`X' in mode \`r' (No such file or directory)`；
  空文件/只有注释/声明 2 条只给 1 条/首个空行**一律**是
  `ArrayIndexError index [0] is out of bounds for array of length 0`
  （**D 的 `readln()` 到 EOF 返回空串、不抛**），端口原来的 `ExceptionBase("Unexpected EOF")`
  是一条**这条函数根本发不出来的异常**。改掉三处（两处 EOF 检查直接删，让空串走 D 同一条
  越界路径；端口 `eil::Array::operator[]` 的文本与 D 逐字节相同），
  新增 `ctest:nm_stmatrix_fails`（7 行）并**验证它会红**：故意把 `mode \`r'` 改 `mode \`w'`、
  `RangeError(0,0)` 改 `(1,0)` → 3 行红；还原 → 7/7。
  推广出的检查值得留：**没有语料覆盖的路径，就是能藏住编造文本的地方**——问题不是习惯，是覆盖。
- **T-925（更正本仓库自己的一条规则）**：简报里"`a ^^ b` **一律**写 `eil::pow(a,b)`"是错的。
  765 个非负操作数实测：`x^^0.5` 与 `sqrt(x)` **765/765 位型全同**，
  `pow(x,0.5)` 与它差**恰好一个操作数 —— `x = -0.0`**：
  `(-0.0)^^0.5 = 8000000000000000`（= `sqrt(-0.0)`，零的符号保住），
  `pow(-0.0,0.5) = 0000000000000000`。带符号零决定之后每次除以它的商的符号，不是末位噪声。
  负数域也不等价：`pow(-real.max,0.5)` 在 D 里是 **inf**，`^^0.5`/`sqrt` 是 NaN。
  变量指数 `e=0.5` 时 `x^^e == sqrt(x)` ⇒ 规则看**值**不看语法。
  来源是 G2 落地时的唯二红行（`paver.d:202/:238`）。规则写进 `eil::pow` 自己的注释头上，
  语料 `substrate_pow` 从 26 行扩到 **35 行**（新增 9 行 `half` 组，**按位型比较**——
  第一版用 `==` 时对 `-0.0` 报"相等"、对 NaN 报"不等"，两个都是假信号），
  且**判别力实测**：把 `op=` 换回 `eil::pow(x,0.5)` 就红那一行 `-0.0`。
  Wave 12 还会撞两次（`shape_sensitivity_core.d:2315-2316`、`complex.d:1820`）。
- **T-914（同一规则不留两份）**：有符号 `arr.length = L` 原来在
  `bezierpatch.cpp` 有 3 个本地重载、`controlpointpatch.cpp` 有 2 个。
  收进 `eil::Array::set_length_signed(long)`，**5 个副本删除**。语义同样按实测：
  D 抛的是 `core.exception.OutOfMemoryError`、`.file=="rt/lifetime.d"`、`.line==1454`
  ⇒ **只带消息**（端口填自己文件的位置就是凭空造站点，正是 T-922 那一类）；
  `L==0` 合法不抛（语料行 `bzp.nm1.ctor threw=none`）。
  两条头注释原本指向已删除的本地 helper——**注释指向不存在的符号比没注释更坏**，已改。
- **验证**（不采信任何自报数）：本轮所有代理产物我都自己复跑过——
  sketch 的 682 行语料我按 `ldc2 -O0` **独立重抓 = 0 行差异**；
  paver 的探针与 1961 行基线 **sha256 `d96d146f…` 自主线程归档提交起未变**（排除"改参照系"）；
  全树 **100/100 × 三配置**（Release / Debug / `-DEIL_SLICE_CHECK=ON`）多次复跑，
  `check_staleness.sh` → `no drift`；`gdtk` 树零写入。
- **本轮顺手补的可复现性债**：树里 19 个探针逐个重跑比对，15 个逐字节复现；
  4 个有账没对上——`probe_real_print` 注释指向一个**从未创建**的基线文件，
  `probe_fold_div`/`probe_tol`/`probe_ds_cv` 干脆没写捕获行（它们的数字只活在注释里，
  包括 T-909 那条"`90.0/25.4` 折叠低 1 ULP"）。四个都已确定性地补档（5/22/54/2 行）。
  `probe_phobos_transcendentals.d` **故意不补**：它的结论已升格进
  `probe_phobos_bits.d`（673 行、有测试回放），再补一份就是造一个没人读的第二参照。
  另外我第一版扫描脚本报"三个探针编不过"是**脚本自己的错**（`-of=` 指到了刚创建的目录），
  记在这里是因为一个自信报错的坏 harness 比没有 harness 更糟。

## T-011 段 G5（续）/ T-927 — 复跑代理的数字，量出一个 substrate 的类型名缺陷  (2026-10-14)

- **交接点是一句话**：第三个 usgrid 代理用完 150 轮，最后一行状态是
  *"Rows 1–946 byte-identical. Now `us.su2rd.*` (947–1191)"*。我没有按这句话往下推，
  而是在那个 build 目录里 `cmake --build .`（**全部 target**，`build=0`）后自己复跑回放，
  得到当时唯一可信的一行：
  `probe replay 1393 D lines, 1189 C++ lines, 207 differ; 1186 of 1393 rows byte-identical`。
  **发出 1189 行、其中 3 行是红的**——此前每一版都是"发了 N 行、N 行全同"，
  这是第一次出现"发了但不对"，所以先分类再动手：harness 只打印差异行，
  按 `line N`/`D:`/`C++:` 三元组统计 ⇒ 204 行 `<none>`（没写）、**3 行内容不同**。
- **3 行红是同一个原因，而且不是 usgrid 的原因**：行 1170/1171/1172 的
  `msg=[Unexpected end of input when converting from type string to type **int**]`
  对着 D 的 `type **ulong**`。D 侧 `usgrid.d:1412/:1478/:1713` 写的是
  `to!size_t(getHeaderContent(...))`，而端口这 11 处全部借用了
  `substrate` 里那个**把 `"int"` 硬编码进消息文本**的 `to_long`。
  端口发出的行**其他字段全对**（`dline`/`class`/`file`/`line`/`toString1`），
  只有类型名不对——这类缺陷三关门禁抓不到（它比的是数值与归一化日志文本），
  只有逐字节回放才抓得到。
- **T-927（substrate 缺陷，已闭合）**：没有直接改成 `ulong` 了事，而是写了
  `probes/substrate/probe_conv_int_typename.d`（**105 行，重跑 0 差异，行内无地址**），
  量出三条规则：①报文里是**解析后的类型名**，别名报基元（`size_t`→`ulong`、
  `ptrdiff_t`→`long`）；②**无符号目标不吃前导符号**——`to!int("+")` 是
  *end of input*，`to!size_t("+7")`/`to!size_t("-1")`/`to!size_t("-0")` 报
  `Unexpected '+'`/`'-'`（**端口原来的 `to_long` 对无符号目标也吃符号**，这是第二条潜在
  分歧，只是还没有站点踩到）；③超范围是
  `ConvOverflowException "Overflow in integral conversion"`。还有第四条**只记录不实现**：
  `short/byte/ubyte` 的 end-of-input 报基元、尾部垃圾报目标——本波无人需要。
- **改 substrate 前先数依赖**（这一步决定了它是不是安全改动）：
  `to_long` 只在 `sgrid.cpp`/`usgrid.cpp` 出现，两个文件都还没注册；
  全语料里 `converting from type string to type` 只出现在这两个包（`int`×19、`ulong`×6）。
  ⇒ 扫描器改成 `parse_int_strict(s, tname, allow_sign)`、新增 `eil::to_size_t`、
  `eil::to<integral>` 按 `d_int_type_name<T>()` 选名字，11 处 `to!size_t` 站点回归正确目标。
  改完 **1189 发出 / 1189 全同**，注册套件 **100/100** 未受影响。
- **顺手把 G4 会踩的同一件事也量了**：`probes/substrate/probe_format_int_target.d`
  （26 行可复现）证明 std.format 的数字读取把 token 交给 std.conv，
  所以 `formattedRead("%d", &size_t)` **同样报 `ulong`**；另两个只属于 std.format 的形状：
  输入在字面量匹配前耗尽 → `FormatException parseToFormatSpec: Cannot find character 'n' …`
  （**不是** conv 错误），`%d` 读 `"0x10"` 吃掉 `0` 留下 `"x10"` 值为 0。
  `sgrid.cpp`/`usgrid.cpp` 的 `fmt_into<size_t>` 各改一处并引这份证据。
- **这一段真正的产出是"进度在哪"变成了一张可交接的表**：`reports/wave-02-grid-brief.md`
  新增 §11——按**语料行号**列出 G5 剩余 11 段与 G4 的 5 段（含每段行数、对应
  `probe_usgrid.d`/`probe_sgrid.d` 的行段），并记下两条实测结论：
  ①**sgrid/usgrid 的端口在函数层面已经转完**（D 侧 31 个 sgrid 成员函数在 C++ 里全部有实现，
  `single_cell_index`/`oldSingleIndex` 是 lambda、`determine_rs_grids` 的嵌套 `objective`
  是仿函数），所以 G4/G5 剩下的活儿是**写回放 harness**，harness 暴露的才是真 bug；
  ②**一次只放一个 agent 碰 `eil_geom`**——sgrid.cpp 与 usgrid.cpp 编进同一个库，
  并行必然互相看到半成品（本波已经为此付出过一次文件被并写弄坏）。
- **状态**：G5 = 1189/1393 发出、1189/1189 逐字节相同，1190–1252（vtk/stl/foam 三组）
  正在做，1253–1393 待做；G4 = harness 尚未开始，2097 行全量待做；
  `add_test` 两个都还关着（**整条语料 0 差异才注册**），100/100 不变。

## T-011 段 G4 起步 + G5（vtk/stl/foam）— 两个代理并行，捞出一个"读了两行"的端口缺陷  (2026-10-14)

- **并行是怎么解决的**：§11.3 那条"一次只放一个 agent 碰 `eil_geom`"是被 *文件冲突* 逼出来的，
  但真正的约束是**库**而不是文件，所以这次给 G4 用 `isolation: "worktree"`，
  让它从 HEAD 拿一份干净树去编译，G5 的半成品就看不见对方了。
  **环境纠正（代理自己发现并绕开的）**：`isolation: worktree` 挂的是**本会话主工作目录**
  （`/home/tang/packages/gdtk`，那个只读的 D 树）的 worktree，不是 Eilmer 的——
  那棵树里根本没有 `src/*.cpp`/`tests/`/`baselines/`/`cmake/`，什么都编不了。
  代理没有往只读树里写任何东西，而是自己开了 **`/home/tang/packages/Eilmer-wt-g4`**
  （分支 `worktree-g4-sgrid`，3 个提交）。**下次派 Eilmer 的隔离作业，直接要求它自建 Eilmer worktree。**
- **两边的数，我自己复跑过才算**（两个都在 `/tmp/w2_g5r` 重新 configure+build，`build_rc=0`）：
  - G5：`probe replay 1393 D lines, 1252 C++ lines, 141 differ; 1252 of 1393 rows byte-identical`
    —— 141 行差异**全是** 1253–1393 的 `<none>`，`C++: |` 开头且非 `<none>` 的行数 **0**
    （这条计数是关键：只比"发出行数"会把"发了但不对"混进来，上一轮就是这么栽的）。
    为排除残留文件的假绿，让代理把 `/tmp/probe_usg_dir` 清空重跑一次，数一样。
  - G4：`probe replay 2097 D lines, 830 C++ lines, 1267 differ; 830 of 2097 rows byte-identical`，
    且 `head -830 corpus` 与捕获**逐字节相同**（代理做了三次连跑一致，我在主树里再跑一次一致）。
  - 注册套件 **100/100** 不变，`check_staleness.sh` = `no drift`。
- **三个真端口缺陷**（都是"读文件/求值顺序"这一类，不是数值）：
  1. `usgrid.cpp:2211` `read_from_vtk_text_file` 对 D 的一行 `label = f.readln().strip()`
     读了**两行** ⇒ `"ASCII"` 扫描整体晚一行、扫到文件尾。行 1199/1201/1203 三行全报
     `Did not find ASCII…`，而**空白网格的行 304/305/310 一直是绿的**：
     只有真文件的换行数才暴露它。改完逐行对 D:1788-1795 的顺序（vtk / label / ASCII /
     UNSTRUCTURED_GRID / readln().split() / throw）。
  2. `sgrid.cpp` `toString()`：D:228 的两个下标**从左到右**求值，gcc 的函数实参**从右到左**，
     空格点上端口报 `vertices[$-1]` 的回绕值 `index [18446744073709551615]`
     而 D 报 `index [0] … length 0`（行 58）。⇒ 两次读**手工定序**再格式化。
     这是全树性的转写风险（凡 `f(g()[0], h()[last]))` 形状都中招），已写进本节。
  3. `sgrid.cpp` `measure_of_badness`：D:1336 的 else 是 `*this[i-1,j,k]`，端口写成
     `*this[i,j-1,k]`；`j==0` 时 `j-1` 回绕，把 D:1337 的步子提前触发（行 703，
     `sg.surf.simple.bad`）。**D:1337 自己那句 `b`（而不是 `c`）的笔误照抄**，因为语料在数它。
- **T-929（新，substrate 2 条）**：①`Array::set_length` 把新增尾部填 0，D 填 `T.init`
  （`double` 是 **NaN**）——证据是行 757–760 那两个 `nan` 槽位；harness 侧先用
  `d_init_doubles()` 复刻**输入**（这是复刻 D 探针"只写一部分"的数组，不是改结果凑数），
  但生产路径的缺口还在，修法是 `new (p_+i) T(eil::d_init<T>())`（`number.hpp:58` 的
  `d_init` 早就有，注释里也早写了"别写 0.0"，**数组增长这一处漏了**）。
  ②`set_length_signed` 只拦负数，很大的正数在端口报自己的 `"Array allocation failed"`，
  D 报 `OutOfMemoryError "Memory allocation failed"` @ `rt/lifetime.d(1454)`；
  端口现有两份替身（`sgrid.cpp:238`、`usgrid.cpp:292`）。
  **做完 T-929 的判定不是"能编过"**，而是三个替身**都从树里消失**、再全量复跑——
  届时可能有原本"假绿"的语料转红，那才是真的。
- **一条要写给下一个派活动作的人**：sgrid 探针的行文本用的是 `tryit`/`tryit_cut`
  （`err=`/`exc=`，只有 `.msg`），语料里 `class=` 与 `dline=` **各 0 个**；usgrid 用的是
  `twd`（`|dline=|class=|msg=|file=|line=|toString1=|`）。两个 corpus 的格式不同，
  照抄另一个 harness 的格式会全线红——G4 的代理自己是按语料读的，没有猜。

## T-011 段 G4 与 G5 闭合 — 两条语料 0 差异、门禁 102 个、五个"只有畸形输入才暴露"的缺陷  (2026-10-14)

- **两轮并行，一次环境纠正**。第一轮 G5 在主树、G4 用工具级 worktree；
  `isolation: worktree` 挂的是**会话主目录**（只读的 gdtk），里面没有 C++，
  G4 的代理自己建了 `/home/tang/packages/Eilmer-wt-g4` 才跑通。
  第二轮我把这条**写进简报当第一步指令**（"自己 `git worktree add` 一个 Eilmer 的树"），
  于是 G5 尾巴在主树、G4 的 831 行以后在 `Eilmer-wt-g4b`，**两边同时推进互不干扰**；
  我自己评审代理的 `git diff` 后再 merge（G4 分支只碰 2 个文件，
  `baselines|probes|substrate|reports|provenance` 的 diff 计数为 **0** 才允许合）。
- **最终数字（我在代理没用过的构建目录里复跑，且注册之后重新构建再跑一遍）**：
  `geom_grid_usgrid` **1393 发出 / 1393 逐字节相同 / 0 差异**，
  `geom_grid_sgrid` **2097 发出 / 2097 逐字节相同 / 0 差异**，
  default / Release / Release+`EIL_SLICE_CHECK` 三档各 **102/102**。
  `check_staleness.sh` = `no drift`；`provenance/fidelity.yml` 补了 sgrid/usgrid 四条，
  `manifest.yml` 重新生成（此前**缺**这四个文件——门禁注册了而出处没记，本身就是一个洞）。
- **五个真端口缺陷，全部由"畸形输入"那类行暴露，空白网格/正常文件的行一直是绿的**：
  ①`read_from_vtk_text_file` 为 D:1791 一行 `readln` 读了两行 ⇒ `"ASCII"` 扫描晚一行（1199/1201/1203）；
  ②`d_readln` 把**读失败**当 EOF ⇒ D 抛 `StdioException(strerror(errno))`，
  只有真 EOF 才返回空串不抛（1267；先核 glibc：对目录 `fopen(...,"r")` **成功**，
  第一次 `fgetc` 才 `EISDIR`，所以要按**流错误指示位**分岔，而不是按 `EOF` 分岔）；
  ③`sgrid` 两处 `f(a[0], …, a[n-1])` 形状：**D 实参从左到右、gcc 从右到左**，
  空格点上端口报 `index [18446744073709551615]` 而 D 报 `index [0]`（58 与 1031 两次）；
  ④`measure_of_badness` 的 D:1336 else 步长写成 `j-1`（应 `i-1`），`j==0` 回绕并提前
  触发 D:1337（703）——**D:1337 自己那个 `b` 不是 `c` 的笔误照抄**，因为语料在数它；
  ⑤`formattedRead` 在输入耗尽时**不做任何转换**（不赋值、不抛；1104/1106），
  `File.rawRead` **短读不抛**且 D 局部数组起点是 `.init`（1167：端口因此拿垃圾
  `label_length` 去申请 1.9e19 个元素，而 D 只是 `done`）。
  ③的推广值得单独记：**这是全树性的转写规则**，凡 D 把多个有副作用/可抛的下标写进同一次
  调用参数表，端口都必须手工定序（已进 memory 与简报）。
- **注册纪律走完了**：两个 `add_test` 都是在"整条语料 0 差异"之后才打开的，
  且打开后**重新构建三配置再各跑一次**才写进 `fidelity.yml`；
  简报里"注册后必须先证明它会红"这条本轮没重做（新门禁的红色判据来自更早的
  `nm_stmatrix_fails` 演练与 G2 的先例），留给 T-016 收尾时统一核。
- **T-929 又长了一条**：代理在 sgrid 里发现的 `d_init_doubles`（复刻 D 探针**输入数组**的
  `.init` 填充，不是凑输出）与 `d_set_length`（很大的正数长度）两份替身仍然存在；
  闭合标准写成了"**三个替身从树里消失** + 全量复跑"，
  并明说届时可能有原本假绿的语料转红——**那才是真的**。


