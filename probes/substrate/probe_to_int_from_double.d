// probes/substrate/probe_to_int_from_double.d — the exact range boundary of `to!int(double)`.
//
// Raised while integrating G6b: the agent found its own d_to_int had to!int's overflow test
// as `v >= 2147483648.0` instead of D's `v > INT_MAX`, which differs at one value — and the
// same shape was still live in src/util/lua_service.hpp's to_int_from_double, whose own
// comment claimed "exclusive of the endpoints" while the code used `>=`. A boundary like this
// cannot be argued from the language; it has to be read off the runtime.
//
// Recorded here: which doubles convert, which raise, the raising class, and the direction of
// truncation. Two facts are easy to get wrong and both matter for a Lua-set parameter:
//   * the TOP bound is strictly exclusive (2147483647.0 converts; 2147483647.5 overflows),
//     and the BOTTOM bound is likewise strictly exclusive (-2147483648.0 converts);
//   * NaN raises std.conv.ConvException "Input was NaN", not a ConvOverflowException.
//
// Build and capture (reproducible; needs only Phobos):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -O0 -of=/tmp/probe_to_int_from_double probes/substrate/probe_to_int_from_double.d
//   /tmp/probe_to_int_from_double > baselines/e587b235/probes/substrate_to_int_from_double.txt
module probe.to_int_from_double;

import std.conv;
import std.stdio;

void one(string label, double v) {
    try {
        int r = to!int(v);
        writefln("%s|v=%.17g|no-throw|value=%d", label, v, r);
    } catch (Exception e) {
        writefln("%s|v=%.17g|class=%s|msg=[%s]", label, v, e.classinfo.name, e.msg);
    } catch (Error e) {
        writefln("%s|v=%.17g|class=%s|msg=[%s]", label, v, e.classinfo.name, e.msg);
    }
}

void main() {
    one("int.max", 2147483647.0);
    one("int.max+0.5", 2147483647.5);
    one("int.max+1", 2147483648.0);
    one("int.min", -2147483647.0 - 1.0);
    one("int.min-0.5", -2147483648.5);
    one("int.min+0.5", -2147483647.5);
    one("trunc.toward.zero.pos", 3.9);
    one("trunc.toward.zero.neg", -3.9);
    one("nan", double.nan);
    one("inf", double.infinity);
    one("minus.inf", -double.infinity);
    one("1e30", 1e30);
    one("minus.1e30", -1e30);
    one("zero", 0.0);
    one("minus.zero", -0.0);
    one("just.below.min", -2147483647.9999);
}
