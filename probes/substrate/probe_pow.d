// probes/substrate/probe_pow.d — pin how ldc2 lowers `a ^^ b` and std.math.pow.
//
// Why this probe exists: ~107 `^^` sites are in Wave 2 scope (89 of them `^^2`),
// concentrated in Bezier/NURBS/Paver polynomial evaluation. Repeated
// multiplication is NOT the same bits, so the translation rule for `^^` had to
// be measured rather than assumed.
//
// Build and capture:
//   ldc2 -O0 probes/substrate/probe_pow.d -of=/tmp/pow_d && /tmp/pow_d \
//     > baselines/e587b235/probes/substrate_pow.txt
// Replay check: ctest -R substrate_pow
import std.stdio, std.math, std.conv, std.format;


void main() {
    double[] xs = [1.2345, 0.1, 2.0/3.0, 1.0e154, 1e-8, -3.7, 0.5];
    foreach (x; xs) {
        writefln("sq %.17g op=%.17g mul=%.17g powd=%.17g powi=%.17g opIsPow=%s opIsMul=%s",
                 x, x ^^ 2, x*x, pow(x, 2.0), pow(x, 2), (x ^^ 2)==pow(x,2.0), (x ^^ 2)==(x*x));
    }
    foreach (x; xs) {
        writefln("cube %.17g op=%.17g mul=%.17g powd=%.17g powi=%.17g opIsPow=%s opIsMul=%s",
                 x, x ^^ 3, x*x*x, pow(x, 3.0), pow(x, 3), (x ^^ 3)==pow(x,3.0), (x ^^ 3)==(x*x*x));
    }
    foreach (x; xs) writefln("misc %.17g pow0=%.17g powNeg2=%.17g powHalf=%.17g",
                             x, x ^^ 0, x ^^ -2, (cast(double)1.0) ^^ 0.5);
    // x ^^ 0.5 is sqrt(x), and it is NOT pow(x, 0.5). Backlog T-925.
    // Found while closing geom_grid_paver, where rows 145-146 (`sec2.ulv3.0/1') were the
    // only red lines in a 1961-row replay: the port had eil::pow(e, 0.5) where D had
    // `(...)^^0.5', and the two disagree in the last bit. Measured over 765 non-negative
    // operands drawn from i/7, i*0.001, 1/i and i*1e-8: `x^^0.5' equals sqrt(x) on all
    // 765 bit patterns, while pow(x,0.5) differs on exactly one of them -- x = -0.0:
    //   (-0.0)^^0.5 = 8000000000000000 = sqrt(-0.0)   (the sign of zero survives)
    //   pow(-0.0,0.5) = 0000000000000000              (it does not)
    // So a negative zero -- reachable from any `0 - x` or a downward-facing normal -- is
    // where the two forms separate, and a signed zero that flips sign changes the result
    // of every later division by it. The rows below print the raw patterns for that reason.
    double[] hs = [-0.0, 0.0, 2.0, 1.0/3.0, 1.0e300, 1.0e-300, real.max, -1.0, -2.5];
    foreach (x; hs) {
        const double h = x ^^ 0.5;
        const double q = sqrt(x);
        const double w = pow(x, 0.5);
        const string hx = format("%016x", *cast(ulong*)&x);
        const string hh = format("%016x", *cast(ulong*)&h);
        const string hq = format("%016x", *cast(ulong*)&q);
        const string hw = format("%016x", *cast(ulong*)&w);
        // Compared as PATTERNS, not with `==`: `==` cannot see the difference this group
        // exists to record (-0.0 == 0.0 is true), and `==` is false for every NaN pair
        // however identical the bits. Both facts were hit by the first version of these
        // rows, which reported opIsPow=true for the -0.0 case and false for the NaN ones.
        writefln("half %s op=%s sqrt=%s pow=%s opIsSqrt=%s opIsPow=%s",
                 hx, hh, hq, hw, hh == hq, hh == hw);
    }
    writefln("frac %s %s", 8.0 ^^ 2.5, pow(8.0, 2.5));
    writefln("intbase %s %s", 2 ^^ 3, 2 ^^ 3);
    // Domain errors: the NaN sign bit is what the log comparison sees.
    writefln("nan_domain %s %s %s %s", (cast(double)-1.0) ^^ 0.5, pow(-1.0, 0.5),
             sqrt(-1.0), log(-1.0));
    writefln("tostring %s %s %s", 1.0/3.0, to!string(1.0/3.0), to!string(3.141592653589793));
    writefln("pi %s %s %s", cast(real)PI, cast(double)(PI/4.0), cast(double)(PI/4.0*4.0));

    // ---- sq2/cu3 sweep (backlog T-931) ----------------------------------
    // The 7 operands above all had pow(x,2.0) == x*x, and from that the rule
    // "`x ^^ 2` is eil::pow" was written down. It is wrong: over a wide sweep D's
    // `x ^^ 2` is x*x on EVERY operand, while glibc's pow(x,2.0) -- which is what a
    // port eil::pow(x,2) call becomes -- differs from it on about 1 operand in 650.
    // paver.d:202 hit exactly that and lost 398 vertices in the demo's paved_grid.vtk.
    // Rows print raw patterns for the same reason the `half` rows do.
    double[] xs2;
    xs2 ~= [0.021687283126135326, 0.03392281542249831];  // the operands that exposed it
    for (int i = 1; i <= 400; ++i) {
        xs2 ~= i / 7.0;
        xs2 ~= i * 0.001;
        xs2 ~= 1.0 / i;
        xs2 ~= i * 1e-8;
        xs2 ~= 0.05 + i * 1e-4;
    }
    for (int i = 0; i < 200; ++i) xs2 ~= 0.040262832185305138 + i * 1e-18;
    xs2 ~= [0.0, -0.0, 1.0, -1.0, 2.0, -2.0, 0.5, 1.0 / 3.0, 2.0 / 3.0,
            double.min_normal, -double.min_normal, 0x0.0000000000001p-1022,
            -0x0.0000000000001p-1022, double.max, -double.max,
            1.3407807929942596e154, 1.3407807929942597e154, double.infinity,
            -double.infinity, double.nan];
    // The flags compare PATTERNS, never `==`: the NaN operand in this list has identical
    // bits in every column yet fails every `==`, which is the same false signal T-925
    // records for the `half` rows.
    string pat(double v) { return format("%016x", *cast(ulong*)&v); }
    foreach (x; xs2) {
        const double o = x ^^ 2;
        const double m = x * x;
        const double w = pow(x, 2.0);
        writefln("sq2 %s d=%s mul=%s powd=%s dEqMul=%s dEqPowd=%s", pat(x), pat(o), pat(m),
                 pat(w), pat(o) == pat(m), pat(o) == pat(w));
    }
    foreach (x; xs2) {
        const double o = x ^^ 3;
        const double ma = (x * x) * x;
        const double w = pow(x, 3.0);
        writefln("cu3 %s d=%s mulA=%s powd=%s dEqMulA=%s dEqPowd=%s", pat(x), pat(o), pat(ma),
                 pat(w), pat(o) == pat(ma), pat(o) == pat(w));
    }
    writefln("sweep|operands=%d", xs2.length);
}
