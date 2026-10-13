// tests/demo_geom_misc_univariatefunctions.cpp
//
// Port of geom/misc/univariatefunctions_demo.d (15 lines). stdout is byte-compared
// against baselines/e587b235/probes/geom_misc_univariatefunctions_demo.txt through
// harness/compare_text.sh (CTest name demo_equiv_geom_misc_univariatefunctions).
//
// READ THIS — where that corpus comes from. ldc2 1.40.0 cannot compile
// geom/misc/univariatefunctions.d at all (its two Vinokur constructors call
// nm/brent.d's `solve`, which is `@nogc` and does `new Exception` at brent.d:51 and
// :123), so the baseline was captured from probes/geom/misc/univariatefunctions_demo_copy.d
// — the demo with its single import line swapped to the mechanical module copy
// (probes/geom/misc/univariatefunctions_copy.d + brent_copy.d). The complete diff is
// printed by `python3 probes/geom/misc/gen_d_copies.py`; nothing else about the demo
// differs, and `--check` confirms the committed copies are current. Verified
// 2026-10-14: rebuilding the copy and running it reproduces the 12 committed lines
// byte for byte.
//
// D source order is preserved line for line. Spelling differences only:
//   * `new X(...)` -> std::make_shared (adapted-handles).
//   * `foreach (i; 0 .. 11)` -> an explicit index loop over the same range.
//   * `writeln("x= ", x, ...)` -> eil::writeln, whose double rendering is the
//     measured `to!string(double)` rule (%.6g, shortest round trip is NOT what Phobos
//     does): the printed "roberts_y= 0.922907" is %.6g of 0.9229068..., and both the
//     x column and the linear column come out the same way.
//   * The demo constructs RobertsFunction and LinearFunction only, so it does not
//     touch brent — but it imports the module, and the module cannot be built here,
//     which is why the baseline needed the copy.
//
// Run: ./demo_geom_misc_univariatefunctions
//
// Regenerate the baseline:
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   cd /home/tang/packages/Eilmer
//   ldc2 --d-debug=1 -i -I/home/tang/packages/gdtk/src -Iprobes/geom/misc probes/geom/misc/univariatefunctions_demo_copy.d -of=/tmp/uf_demo_d
//   /tmp/uf_demo_d > baselines/e587b235/probes/geom_misc_univariatefunctions_demo.txt
#include <memory>

#include "eil/eil.hpp"
#include "geom/misc/univariatefunctions.hpp"

using eil::geom::LinearFunction;
using eil::geom::RobertsFunction;

int main() {
    eil::writeln("Begin demonstration of the clustering functions.");
    auto cf = std::make_shared<RobertsFunction>(false, true, 1.1);
    auto cf2 = std::make_shared<LinearFunction>(1.0, 0.0);
    for (int i = 0; i < 11; ++i) {
        double x = 0.1 * i;
        eil::writeln("x= ", x, " roberts_y= ", (*cf)(x), " linear_y= ", (*cf2)(x));
    }
    return 0;
}
