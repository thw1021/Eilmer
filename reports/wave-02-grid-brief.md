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
