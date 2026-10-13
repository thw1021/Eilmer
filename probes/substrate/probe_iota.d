// probes/substrate/probe_iota.d — pin Phobos' std.range.iota element VALUES and counts.
//
// Why: 31 `iota` sites are in Wave 2 scope, 29 of them in
// geom/surface/beziertrianglepatch.d (descending index walks) plus two floating
// sweeps there and one in geom/grid/sgrid.d. A C++ port that walks the range by
// accumulating (`v += step`) does NOT produce the same bits: measured below, D's
// 6th element of iota(0.0, 1.0, 0.1) is 0.60000000000000009 whereas accumulation
// gives 0.59999999999999998 — D computes element n as `begin + n*step`.
// The bound is also strict: iota(0.0, 1.0, 0.25) stops at 0.75 and excludes the
// exact hit at 1.0.
//
// Build and capture:
//   ldc2 -O0 probes/substrate/probe_iota.d -of=/tmp/iota_d && /tmp/iota_d \
//     > baselines/e587b235/probes/substrate_iota.txt
// Replay check: ctest -R substrate_iota
module probe_iota;

import std.conv;
import std.range;
import std.stdio;

void dumpF(string name, double b, double e, double s) {
    int n = 0;
    foreach (v; iota(b, e, s)) {
        writefln("%s[%d]=%.17g", name, n, v);
        ++n;
    }
    writefln("%s.count=%d", name, n);
}

void dumpI(string name, int b, int e, int s) {
    string joined;
    int n = 0;
    foreach (v; iota(b, e, s)) {
        if (n) joined ~= ",";
        joined ~= to!string(v);
        ++n;
    }
    writefln("%s=%s count=%d", name, joined, n);
}

void main() {
    dumpF("one tenth", 0.0, 1.0, 0.1);          // accumulation diverges from here
    dumpF("quarter", 0.0, 1.0, 0.25);           // exact hit on the bound: excluded
    dumpF("sgrid", 0.0, 1.0 + 0.5 * 0.25, 0.25);
    dumpF("neg float", 1.0, 0.0, -0.25);
    dumpF("empty", 1.0, 1.0, 0.1);
    dumpI("desc4", 4, -1, -1);                  // the beziertrianglepatch walk
    dumpI("desc_n0", 5, 0, -1);                 // excludes 0
    dumpI("up", 0, 5, 1);
}
