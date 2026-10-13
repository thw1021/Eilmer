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
import std.stdio, std.math, std.conv;


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
    writefln("frac %s %s", 8.0 ^^ 2.5, pow(8.0, 2.5));
    writefln("intbase %s %s", 2 ^^ 3, 2 ^^ 3);
    // Domain errors: the NaN sign bit is what the log comparison sees.
    writefln("nan_domain %s %s %s %s", (cast(double)-1.0) ^^ 0.5, pow(-1.0, 0.5),
             sqrt(-1.0), log(-1.0));
    writefln("tostring %s %s %s", 1.0/3.0, to!string(1.0/3.0), to!string(3.141592653589793));
    writefln("pi %s %s %s", cast(real)PI, cast(double)(PI/4.0), cast(double)(PI/4.0*4.0));
}
