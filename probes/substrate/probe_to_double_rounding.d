// probes/substrate/probe_to_double_rounding.d — is Phobos' `to!double` correctly rounded?
//
// Raised by the G6a sgrid_demo artefact gate (cmake/wave2_grid_g6.cmake): 5 of 16 artefact
// rows in geom_grid_sgrid_demo.artefacts.txt differ from the D reference, always in the last
// printed digit, for a grid whose values come straight out of `blk.tmp` through
// `to!double(token)` with scale=1.0 (sgrid.d:1603) -- so no geometry arithmetic is involved
// at all, only decimal->binary conversion. If Phobos and strtod disagree there, every job
// script literal in the solver inherits the difference, which is why this is measured before
// anything is blamed on geom/grid.
//
// Each row prints the input, then the IEEE-754 bit pattern of: (a) Phobos `to!double(s)`,
// (b) `std.strtol`-free reference -- the same string parsed by D's own `parse!double`, and
// (c) the decimal string written back with %a so a C++ reader can compare against strtod.
//
// Build and capture (reproducible; needs only Phobos):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -O0 -of=/tmp/probe_to_double_rounding probes/substrate/probe_to_double_rounding.d
//   /tmp/probe_to_double_rounding > baselines/e587b235/probes/substrate_to_double_rounding.txt
module probe.to_double_rounding;

import std.conv;
import std.format;
import std.stdio;

void main() {
    // The token that failed, plus neighbours chosen to cover: the same digits with the
    // rounding direction flipped, a short decimal, a power of ten, and digits long enough to
    // force Phobos off its fast path.
    enum texts = [
        "0.2929550221030571",
        "0.2929550221030572",
        "0.292955022103057",
        "2.929550221030571e-1",
        "0.1",
        "0.3",
        "1.0000000000000002",
        "9.007199254740993e15",
        "0.29295502210305712345",
        "1e308",
        "1e-308",
        "4.9406564584124654e-324",
        "0.5000000000000001",
        "2.2250738585072011e-308",
        "1.7976931348623157e308",
        "0.2929550221030571 ",
        " 0.2929550221030571",
    ];

    foreach (s; texts) {
        // Two tokens in the list carry surrounding whitespace, which is T-913 grammar
        // territory rather than rounding; catch so one rejection cannot abort the sweep.
        try {
            double a = to!double(s);
            // %a is the bit-exact hex float form: two implementations that print the same %a
            // hold the same double, which is the only comparison that means anything here.
            writefln("to!double|src=[%s]|bits=%016x|a=%a", s, (*cast(ulong*)&a), a);
        } catch (Exception e) {
            writefln("to!double|src=[%s]|class=%s|msg=[%s]", s, e.classinfo.name, e.msg);
        }
    }

    // And how the same values come back out as %.17g, which is what the corpora print.
    foreach (s; texts) {
        try {
            double a = to!double(s);
            writefln("roundtrip|src=[%s]|g17=%.17g", s, a);
        } catch (Exception e) {
            writefln("roundtrip|src=[%s]|class=%s|msg=[%s]", s, e.classinfo.name, e.msg);
        }
    }
}
