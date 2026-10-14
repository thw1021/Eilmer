// probes/substrate/probe_fold_div.d — does ldc2 fold 90.0/25.4 to the same double
// as a run-time division? Motivated by geom/misc/svg.d:55 `unitToPixels = 90.0/25.4`
// showing up as a 1-ULP difference in svg document coordinates.
//
// Build and capture (reproducible; the gdtk tree itself is read-only):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip \
//     probes/substrate/probe_fold_div.d -of=/tmp/probe_fd -od/tmp/probe_obj_fd
//   /tmp/probe_fd > baselines/e587b235/probes/substrate_fold_div.txt
// No -O flag: every baseline in this repository is an ldc2 -O0 capture.
// The 5 rows were quoted in comments only until 2026-10-14; they are versioned now so
// the claim in reports/backlog.md T-909 ("90.0/25.4 folds 1 ULP low, 90.0/2.54 does not")
// can be re-derived rather than trusted.
module probe_fold_div;

import std.stdio, std.conv;

void main() {
    double folded = 90.0 / 25.4;          // both operands are literals
    double a = 90.0, b = 25.4;
    double runtime = a / b;               // cannot be folded
    writefln("folded=%.17g runtime=%.17g equal=%s", folded, runtime, folded == runtime);
    writefln("folded=%a runtime=%a", folded, runtime);
    writefln("x100 folded=%.17g runtime=%.17g", 100.0 * folded, 100.0 * runtime);

    double folded2 = 90.0 / 0.0254;
    double c = 0.0254;
    double runtime2 = 90.0 / c;
    writefln("m folded=%.17g runtime=%.17g equal=%s", folded2, runtime2, folded2 == runtime2);

    double folded3 = 90.0 / 2.54;
    double d = 2.54;
    writefln("cm folded=%.17g runtime=%.17g equal=%s", folded3, 90.0 / d, folded3 == 90.0 / d);
}
