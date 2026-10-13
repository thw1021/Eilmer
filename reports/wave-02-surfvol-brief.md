# T-012 分派简报（geom/surface + geom/volume，每个子代理必读）

先读 `reports/wave-02-brief.md`（通用纪律、映射表、已钉死的实测语义），本文件只补
T-012 特有的一切。**两段强制工作顺序**：先让文件进编译器，再逐组抓 D 语料比对；
不允许"写完整个模块再第一次编译"。

## 0. 已经由主线程完成的部分（不要重写，直接用）

- `src/geom/surface/parametricsurface.{hpp,cpp}` — 抽象基类 + `write_surface_as_vtk_xml`
- `src/geom/volume/parametricvolume.{hpp,cpp}` — 抽象基类 + 九个差分 + `write_volume_as_vtk_xml`
- 两者的 D 探针与语料、以及逐行回放测试：
  `probes/geom/surface/probe_parametricsurface.d` → **29/29 行字节相同**；
  `probes/geom/volume/probe_parametricvolume.d` → **2165/2165 行字节相同**
  （`ctest -R "parametricsurface|parametricvolume"`）。
- CMake 片段 `cmake/wave2_geom_surfvol.cmake`：每个新模块**追加**
  `target_sources(eil_geom PRIVATE ...)` 与自己的测试块，并且测试块一律用
  `if(EXISTS ...)` 自注册 —— 这样中途别人还没写完文件时整棵树仍可 configure。
  **不要改顶层 `CMakeLists.txt`、`reports/backlog.md`、`substrate/`。**

## 1. 虚函数契约（所有子类必须照这个写，别自创）

```cpp
class FooPatch : public ParametricSurface {
  public:
    Vector3 operator()(double r, double s) const override;            // D: opCall
    std::shared_ptr<ParametricSurface> dup() const override;           // adapted-handles
    std::string toString() const override;
};
```
`ParametricVolume` 同构，`operator()(r,s,t)` 三参。**本波更正（代理实测，我写错了）**：
`classString()` **只存在于 `geom/gpath`**（`grep -c classString gdtk/src/geom/surface/*.d
gdtk/src/geom/volume/*.d` → 全 0）。把它写成 `override` 编不过；surface/volume 的子类
若要类名行，就声明成**非虚** helper 并在头注释写明"不是 D 符号"，
`ParametricSurface`/`ParametricVolume` 基类也**不加**这个纯虚。
D 的 `cast(X)v !is null` → `dynamic_cast`（要 RTTI，不要换成虚函数标志位）。

另一条同类更正：`geom/volume/parametricvolume.d` **没有** `area()` 之外的日志虚函数，
volume 侧的九差分是**具体实现**不是虚函数，子类不 override 它们。

## 2. 本波已知的 D 怪癖（照抄，注释里写 file:line，不许顺手修）

| 位置 | 行为 |
|---|---|
| `surface/beziertrianglepatch.d` | 29 处 `iota` 做降指标遍历 → 必须 `eil::iota`（`begin+n*step`，远端严格） |
| `surface/gmopatch.d:16,87` | `import geom.grid.sgrid` 并 `new StructuredGrid(...)` → **依赖 T-011，本波不做** |
| `surface/meshpatch.d:23` | 构造入参 `ref StructuredGrid grid` → 同上，**本波不做** |
| `volume/*` 全体 | `import geom.surface` 是聚合头 → C++ 侧改成实际用到的头（基类即可），头注释写替代关系 |

## 3. 每个模块的交付件（缺一不可）

1. `src/geom/<dir>/<module>.hpp`（+ `.cpp`，仅当需要独立 TU）——顶部注释写
   `<d 路径> -> src/<路径> (module <d.module>)` 与非 1:1 之处。
2. `probes/geom/<dir>/probe_<module>.d` —— D 探针，头部写**可复跑的抓取命令**。
3. `baselines/e587b235/probes/geom_<dir>_<module>.txt` —— ldc2 现编 D 的 stdout。
4. `tests/test_geom_<dir>_<module>.cpp` —— (A) D `unittest` 逐条断言 + (B) 语料逐行回放，
   汇总行必须给"N of M 行字节相同 / 多少量逐位相同 / 最坏残差多少 ULP 且归因到哪个函数"。
5. D 的 `*_demo.d` → `tests/demo_geom_<dir>_<module>.cpp`，stdout 与 D **字节相同**。
6. `cmake/wave2_geom_surfvol.cmake` 里追加自己的 target 与 `add_test`。
7. `provenance/fidelity.yml` 追加条目后跑 `python3 tools/gen_manifest.py`。

## 4. 抓取 D 语料的命令（照抄，勿改路径）

```bash
cd /home/tang/packages/Eilmer
export PATH=/home/tang/packages/dlang/bin:$PATH
# /tmp/gdtk_shadow 已存在（gdtk/src 的副本，删了 @nogc）。若不存在：
#   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
#   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface \
  probes/geom/surface/probe_<module>.d -of=/tmp/probe_x -od/tmp/probe_obj_<你的批次>
/tmp/probe_x > baselines/e587b235/probes/geom_surface_<module>.txt
```
`gdtk` 树**只读**，任何情况下不得写入。

## 5. 构建自检（并行期间必须用批次专属目录，不要共享）

```bash
cd /home/tang/packages/Eilmer && rm -rf /tmp/w2_<批次> && cmake -S . -B /tmp/w2_<批次> \
  -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/w2_<批次> -j4                       # 只有 16 核，别用 -j8 抢
(cd /tmp/w2_<批次> && ctest -R "<你的测试名正则>" --output-on-failure)
```
最后再跑一次 Debug 与 `-DEIL_SLICE_CHECK=ON`。**并行阶段兄弟代理会往同一棵树里写
半成品文件**，如果你看到 `tests/test_geom_gpath_svgpath.cpp` 之类的编译错误，那不是你
的：只构建你自己的 target（`cmake --build <dir> --target <你的 target>`），不要试图
修别人的文件，也不要因此关掉注册。

## 6. 报告纪律

结论必须是产生它的那个数字。"area() 与 D 一致" 不接受；
"area() 在 9 组 (nr,ns) 上 27 个分量 `%.17g` 全部字节相同" 才接受。
发现 substrate 缺口 → 写到你的最终报告里交给主线程登记 T-9xx，**不要自己改 substrate**。
