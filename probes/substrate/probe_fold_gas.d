// probes/substrate/probe_fold_gas.d — which literal constant expressions in the
// Wave-3 `gas/` closure does ldc2 fold to something other than the run-time value?
//
// BACKGROUND (reports/backlog.md T-909). ldc2 1.40.0 folds a floating-point constant
// expression at compile time, and the folded result is NOT the correctly-rounded
// double: measured in probe_fold_div.d, `0.2 + 1.0e-10` folds to
// 0.20000000000999999... and computes at run time to 0.20000000001000002..., and
// `90.0/25.4` (geom/misc/svg.d:55) differs by 1 ULP. A C++ port that writes the same
// expression gets g++'s run-time value and therefore disagrees with the reference
// solver in the last bit — which under a cancellation or a square root is not a last
// bit at all.
//
// Wave 2's exposure turned out to be exactly one site (svg.d:55) only because the
// search that concluded "zero" looked at module-level const declarations and missed
// literals inside function bodies. This probe does the wider search: every
// literal<op>literal that appears anywhere in the gas/ closure, taken from a grep over
// the reference tree rather than from a guess about where constants live.
//
// For each expression the probe prints the folded value and the run-time value side by
// side, obtained by routing the same operands through a variable so the compiler
// cannot fold them. `equal=false` rows are the ones a C++ port must hard-code from D.
//
// Build and capture:
//   cd /home/tang/packages/Eilmer
//   /home/tang/packages/dlang/bin/ldc2 probes/substrate/probe_fold_gas.d \
//     -of=/tmp/probe_fold_gas && /tmp/probe_fold_gas \
//     > baselines/e587b235/probes/substrate_fold_gas.txt
module probe_fold_gas;

import std.stdio;
import std.format;
import std.math;

// ldc2 cannot constant-fold through this without interprocedural analysis at -O0,
// which is how the reference solver is built (see reports/phase-0.md).
double[] scratch;

string bits(double x) { return format("%016x", *cast(ulong*)&x); }

void cmp(const char[] expr, double folded, double runtime) {
    writefln("%-46s folded=%.17g/%s runtime=%.17g/%s equal=%s ulpDiff=%s",
             expr, folded, bits(folded), runtime, bits(runtime), folded == runtime,
             folded == runtime ? "0" :
             format("%d", cast(long)((*cast(long*)&folded) - (*cast(long*)&runtime))));
}

void main() {
    scratch.length = 4;

    // gas/two_temperature_dissociating_nitrogen.d:245,267,271 and the same factor in
    // three_temperature_trans_props.d (6 sites) and multi_temperature_trans_props.d:
    // the cal/(cm.s.K) -> J/(m.s.K) conversion. One of the most repeated literal
    // division in the kinetics/transport code.
    { double a = 4.184, b = 1.0e-2; cmp("4.184/1.0e-2", 4.184 / 1.0e-2, a / b); }

    // gas/thermo/cea_thermo_curves.d:198 -- R for oxygen, feeds every curve value.
    { double a = 8.31451, b = 0.0159994; cmp("8.31451/0.0159994", 8.31451 / 0.0159994, a / b); }

    // gas/two_temperature_argon_plus_ideal.d:674,683
    { double a = 1.0e5, b = 208.0, c = 300.0;
      cmp("1.0e5/(208.0*300.0)", 1.0e5 / (208.0 * 300.0), a / (b * c)); }
    { double g = 5.0, t = 300.0; cmp("5.0/3.0*208.0*300.0", 5.0 / 3.0 * 208.0 * 300.0,
                                     g / 3.0 * 208.0 * t); }

    // gas/two_temperature_dissociating_nitrogen.d:131 and its two mirrors in
    // diffusion/three_temperature_trans_props.d:111, multi_temperature_trans_props.d:149
    { double a = 0.45, b = 2.54; cmp("0.45 - 2.54*0.5", 0.45 - 2.54 * 0.5, a - b * 0.5); }

    // gas/two_temperature_nitrogen.d:156 -- reduced mass, one particle.
    { double a = 0.5, m = 28.0134e-3; cmp("0.5*28.0134e-3", 0.5 * 28.0134e-3, a * m); }

    // gas/thermo/three_temperature_gas.d:545,586,593 -- heat-capacity ratios.
    { double a = 5.0, b = 2.0; cmp("5.0/2.0", 5.0 / 2.0, a / b); }
    { double a = 3.0, b = 2.0; cmp("3.0/2.0", 3.0 / 2.0, a / b); }
    { double a = 9.0, b = 2.0; cmp("9.0/2.0", 9.0 / 2.0, a / b); }
    { double a = 7.0, b = 2.0; cmp("7.0/2.0", 7.0 / 2.0, a / b); }
    { double a = 1.0, b = 3.0; cmp("1.0/3.0", 1.0 / 3.0, a / b); }
    { double a = 2.0, b = 3.0; cmp("2.0/3.0", 2.0 / 3.0, a / b); }
    { double a = 1.0, b = 2.0; cmp("1.0/2.0", 1.0 / 2.0, a / b); }

    // gas/thermo/three_temperature_gas.d:54 -- a hand-written PI literal, raised to 1.5.
    { double a = 2.0, p = 3.14159265; cmp("pow(2.0*3.14159265, 1.5)",
                                          pow(2.0 * 3.14159265, 1.5), pow(a * p, 1.5)); }

    // The Wave-2 one, kept here so the corpus shows a known-unequal row beside the
    // new ones (geom/misc/svg.d:55).
    { double a = 90.0, b = 25.4; cmp("90.0/25.4 (svg.d:55, known bad)", 90.0 / 25.4, a / b); }

    // Divisions by a decimal that is a power of ten times a small integer: these are
    // the shape most likely to fold wrong, and the transport code uses them as scales.
    foreach (n; [1.0e-2, 1.0e-3, 1.0e-5, 1.0e-9, 1.0e3, 1.0e5]) {
        double a = 1.0, b = n;
        writefln("1.0/%-12g folded=%s runtime=%s equal=%s", n, bits(1.0 / n), bits(a / b),
                 1.0 / n == a / b);
    }
}
