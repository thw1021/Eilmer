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
