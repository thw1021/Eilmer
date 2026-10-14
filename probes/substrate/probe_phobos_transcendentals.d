// probes/substrate/probe_phobos_transcendentals.d — are Phobos' exp/sinh/cosh/tanh
// the same function glibc exposes? Reported as a substrate gap by the geom/misc
// stage (42 of 857 replayed lines land 1 ULP apart); this decides whether that is
// Phobos' own code or the documented <=1 ULP libm split, and whether a wrapper is
// needed the way eil::sqrt exists for the NaN sign bit.
//
// Build: ldc2 -O0 probes/substrate/probe_phobos_transcendentals.d -of=/tmp/tr_d
//
// This is the one-shot survey that decided backlog T-911. It has NO baseline file of its
// own, deliberately: its findings were promoted into
// probes/substrate/probe_phobos_bits.d, which is the permanent, row-for-row version
// (baselines/e587b235/probes/substrate_phobos_bits.txt, 673 rows with IEEE-754 bit
// patterns, replayed by ctest:substrate_phobos_bits). Re-running this probe is still
// useful when a new transcendental is suspected -- it prints a per-function
// same/differ summary rather than a corpus -- but the gated artifact is the _bits one,
// so do not "fix" the missing baseline by capturing this output into a file nothing
// reads.
module probe_phobos_transcendentals;

import std.stdio, std.math;

void main() {
    // Arguments drawn from the Vinokur/Roberts iteration in
    // geom/misc/univariatefunctions.d, where the divergence showed up.
    double[6] xs = [0.02, 0.4, 1.0, 2.7314110761533215, 5.0, 20.0];
    foreach (x; xs) {
        writefln("x=%.17g exp=%.17g sinh=%.17g cosh=%.17g tanh=%.17g",
                 x, exp(x), sinh(x), cosh(x), tanh(x));
    }
}
