// probes/substrate/probe_fold_div.d — does ldc2 fold 90.0/25.4 to the same double
// as a run-time division? Motivated by geom/misc/svg.d:55 `unitToPixels = 90.0/25.4`
// showing up as a 1-ULP difference in svg document coordinates.
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
