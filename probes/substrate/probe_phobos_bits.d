// probes/substrate/probe_phobos_bits.d — Phobos-vs-glibc divergence, counted by
// IEEE-754 bit pattern instead of by eye.
//
// WHY. T-910 proved that D's std.math.atan2 is NOT glibc's atan2: Phobos keeps its
// own atanImpl!double (CEPHES/fdlibm) in source, and the 1 ULP difference was the
// whole explanation for 84 tolerance-band rows in geom/gpath/modifiedpath. That left
// an assumption standing on borrowed evidence: the project has been treating the
// residual 1 ULP differences in exp/sinh/cosh/tanh/sin/cos/log as an irreducible
// library floor (reports/phase-0.md, reports/wave-02-brief.md section 5). If Phobos
// implements those in pure source too, they are replicable exactly like atan2 was,
// and Wave 3's gas/ tranche (CEA look-up tables and Arrhenius reaction rates, both
// exp-heavy) would not need a tolerance floor at all. This probe decides that by
// counting, and the counts are the deliverable.
//
// WHAT IT PRINTS. For each function and each argument: the value at %.17g and its raw
// 64-bit pattern. A bit-level diff between this corpus and the C++ mirror names the
// functions that genuinely diverge and how often, so "how much of our 1e-13 floor is
// actually ours to remove" becomes a number.
//
// Arguments are written as decimal literals, never as literal arithmetic (T-909:
// ldc2's compile-time folding of constant expressions is not correctly rounded and
// differs from the same expression at run time by 1 ULP — measuring through a folded
// constant would attribute ldc2's folder to Phobos' library).
//
// Build and capture:
//   cd /home/tang/packages/Eilmer
//   /home/tang/packages/dlang/bin/ldc2 probes/substrate/probe_phobos_bits.d \
//     -of=/tmp/probe_bits && /tmp/probe_bits \
//     > baselines/e587b235/probes/substrate_phobos_bits.txt
import std.format;
import std.math;
import std.stdio;

string bits(double x)
{
    return format("%016x", *cast(ulong*)&x);
}

void emit(const char[] fn, double arg, double res)
{
    writefln("%s arg=%.17g v=%.17g bits=%s", fn, arg, res, bits(res));
}

double[] args()
{
    // Covers the ranges the solver actually evaluates: small positive (reaction-rate
    // exponents), moderate (Mach/area relations), large (exp overflow paths), the
    // negative mirror, and the special arguments. Not random: each class gets its own
    // rows so a divergence can be attributed to a range rather than "somewhere".
    return [-100.0, -20.0, -3.0, -1.5, -0.5, -0.25, -1.0e-5, -0.0, 0.0, 1.0e-9, 1.0e-5,
            0.1, 0.25, 0.5, 0.6931471805599453, 1.0, 1.5, 2.0, 2.718281828459045,
            3.141592653589793, 5.0, 10.0, 20.0, 40.0, 700.0, 710.0, 745.0, 746.0,
            1.0e10, 1.0e-300, 0.9999999999999999, 1.0000000000000002];
}

void main()
{
    foreach (x; args()) {
        emit("exp", x, exp(x));
        emit("expm1", x, expm1(x));
        emit("log", x, x > 0.0 ? log(x) : log(-x));
        emit("log10", x, x > 0.0 ? log10(x) : log10(-x));
        emit("log2", x, x > 0.0 ? log2(x) : log2(-x));
        emit("sinh", x, sinh(x));
        emit("cosh", x, cosh(x));
        emit("tanh", x, tanh(x));
        emit("sin", x, sin(x));
        emit("cos", x, cos(x));
        emit("tan", x, tan(x));
        emit("atan", x, atan(x));
        emit("sqrt", x, x >= 0.0 ? sqrt(x) : sqrt(-x));
        emit("cbrt", x, cbrt(x));
        // pow: the two arms D distinguishes (integer exponent and real exponent).
        emit("pow_int2", x, pow(x, 2));
        emit("pow_real2", x, pow(x, 2.0));
        emit("pow_int3", x, pow(x, 3));
        emit("pow_half", x, x >= 0.0 ? pow(x, 0.5) : pow(-x, 0.5));
    }

    // asin/acos only in-domain; the domain-error NaN sign is measured separately below.
    foreach (x; [-1.0, -0.9999999999999999, -0.7071067811865476, -0.5, -0.25, 0.0,
                 0.25, 0.5, 0.7071067811865476, 0.9999999999999999, 1.0]) {
        emit("asin", x, asin(x));
        emit("acos", x, acos(x));
    }

    // atan2 over a 2-D grid, including the near-axis arguments that exposed T-910.
    foreach (double y; [-1.0, -0.5, 0.0, 0.5, 1.0, 2.0, 1.0e-8]) {
        foreach (double x; [-1.6653345369377348e-16, -1.0, -0.5, 0.0, 0.25, 0.5,
                            1.0, 1.7071067811865475, 1.9999996915748783, 1.0e10]) {
            emit("atan2", y, atan2(y, x));
        }
    }

    // Domain errors: the sign bit of the returned NaN is user-visible in logs.
    emit("sqrt_neg", -1.0, sqrt(-1.0));
    emit("log_neg", -1.0, log(-1.0));
    emit("asin_gt", 1.24, asin(1.24));
    emit("acos_gt", 1.24, acos(1.24));
    emit("pow_neg_half", -1.0, pow(-1.0, 0.5));
}
