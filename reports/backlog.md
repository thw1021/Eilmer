- [ ] **T-936** 端口的文本读取器在 shim 里剥掉行尾终止符，D 是在**每个调用点** `.strip()`。
  今天等价（实测：`eil::readln_line` 合流后 `geom_grid_sgrid` 2097 行、`geom_grid_usgrid`
  1393 行仍逐字节相同），但那是端口自己加的一层归一化：新格式读取器只要按长度/下标取行
  （`if (line.empty())`、`line[0]`、`formattedRead` 的列位置）就会与参照分叉——D 的空行是
  `\n` 不是空串，且 CRLF 的 `\r` 是被保留的。`substrate_readln.txt` 的 29 行已把 D 的原始
  形状钉住，`eil::readln` 就是原始形状、`eil::readln_line` 才是归一化形状 ⇒ 迁移时逐站点
  换成"原始 readln + 显式 strip"，每改一个站点三配置复跑。→ **Wave 4/5 的 I/O 波次做**
- [ ] **T-916** Wave 3 硬任务：D 的 `sinh` 有两处**结构**分歧（不是舍入）——
  (a) 奇对称破坏：`sinh(-20)`=-242582602.56479013 而 `sinh(20)`=+242582597.70489514，
  二者绝对值不等，且都不等于 `(exp(x)-exp(-x))/2`（＝glibc 值）；
  (b) 溢出阈值不同：`sinh(710)` D 给 `inf`，glibc 给 `1.1169973830808557e+308`。
  主干调用点 `gas/two_temperature_nitrogen.d:111` 的自变量是 `θ_N2/(2*Tv)`，
  **Tv 很小（冷启动/高激波）时可以越过 710**，届时 D 出 inf、C++ 出有限值，
  振动能量方程随之分叉——不是末位差异。
  转写时须：先用 `probes/gas/probe_sinh_range.d`（待写）扫 `Tv` 全域把两条分歧的
  实际命中范围量出来，再决定是复刻 Phobos 的 `sinhImpl` 还是在调用点显式保留 D 行为。
  **禁止**用"反正 1e-13 容差够宽"糊过去：inf 与 1e308 的相对差是 1e-308 量级的**类别**差 → **Wave 3**
- [ ] **T-912** 转写纪律（T-910 的教训，须写进每个子代理简报）：D 侧凡调用
  `std.math` 的函数，**必须先确认它在 Phobos 里是转发 C 库还是自带实现**，
  再决定 C++ 侧写 `std::xxx` 还是 `eil::xxx`。已知清单：
  `atan2`/`atan` = Phobos 自带（`trigonometry.d:945 → atan2Impl`）→ `eil::atan2`；
  `asin`/`acos` = D 定义为 `atan2(x, sqrt(1-x*x))` 且**在 80 位 real 里算**
  （`trigonometry.d:620-628`）→ 既不是 `std::asin` 也不是 double 版 replica。**已落实**
  （2026-10-15，T-934）：规则上移进 `substrate/include/eil/math.hpp` 的 `eil::asin`/`eil::acos`，
  `src/gasdyn/idealgasflow.cpp` 的本地 `d_asin` 副本删除；当年的"逐点镜像"判断是对的，
  但"acos 与 glibc 逐位相同"（T-911 的 11 样本）是错的——8,454 行里 glibc 差 208 行；
  `pow` = 实测与 `std::pow` 逐位相同（语料 26 行）→ `eil::pow` 只是拼写。
  判据一律是**位型**（`bits=%016x`），不是 `%.17g` 文本


---

## 需你批准（会改本机环境或构建）

- [ ] **T-800** `[NEEDS-OK]` 以 `WITH_NK=1` 构建 `e4-nk-shared`（可另加 2 个 Tier-A 判例：
  `flat-plate-turbulent-mabey/su2-steady-state-solver`、`swbli/restart-test`，并覆盖
  `nk_accelerator` 20 站点与隐式 NK 路线）
- [ ] **T-801** `[NEEDS-OK]` 安装 METIS（提供 `gpmetis`），解锁 `test-names.txt` 中 6 个
  `*metis*` 判例与 **T-080** 的分区路径
- [ ] **T-802** `[NEEDS-OK]` 若要三门中第 3 门（70 个 Ruby 判例）尽早可用，需要在 Wave 5 产出
  C++ `e4shared` 之后由你确认跑全量（单轮约 1.5 小时）

---

## 门禁现状（判据见计划文件"验收三门"）

| 门 | 状态 |
|---|---|
| (1) 快照逐元素 ≤1e-13 | harness 就绪；D-vs-D 自测 7/7 PASS；基线 7 案例 |
| (2) 残差/气动力/步数 | 同上（`config/*-residuals.txt`、`loads/` 已在基线内） |
| (3) 70 个 Ruby 判例 | **尚不可执行**：需要候选 `e4shared`，最早 Wave 5 之后；6 个 metis 案例另受 T-801 阻塞 |
