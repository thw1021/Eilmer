// probes/substrate/probe_phobos_transcendentals.d — are Phobos' exp/sinh/cosh/tanh
// the same function glibc exposes? Reported as a substrate gap by the geom/misc
// stage (42 of 857 replayed lines land 1 ULP apart); this decides whether that is
// Phobos' own code or the documented <=1 ULP libm split, and whether a wrapper is
// needed the way eil::sqrt exists for the NaN sign bit.
//
// Build: ldc2 -O0 probes/substrate/probe_phobos_transcendentals.d -of=/tmp/tr_d
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
