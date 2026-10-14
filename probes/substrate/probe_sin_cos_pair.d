// probes/substrate/probe_sin_cos_pair.d — T-920's reference side.
//
// What D gives for cos(x) and sin(x) over the 415 angles of
// tests/test_data/sincos_angle.txt, printed as raw IEEE-754 bit patterns plus %.17g.
//
// Why this probe exists: g++ at -O1 and above recognises a cos(x)/sin(x) pair of the
// same argument and rewrites it into a single call to glibc's `sincos`. That is not the
// same function as two libm calls -- but, and this is the part an earlier note in this
// repository got wrong, **ldc2 does the same thing**. Phobos' std.math.cos/std.math.sin
// forward to core.math, LDC lowers them to llvm.cos.f64 / llvm.sin.f64, and LLVM's own
// sincos pass fuses them again from -O1 up.
//
// Measured on this box over these 415 angles, classifying each corpus row by which libm
// form reproduces BOTH of its bit patterns exactly:
//   ldc2 -O0    separate-calls-only 401   sincos-only 0     agree-with-both 14
//   ldc2 -O3    separate-calls-only   0   sincos-only 401   agree-with-both 14
//   g++  -O0    separate (like ldc2 -O0)     g++ -O1..-O3   fused (like ldc2 -O3)
// 401 of 415 rows therefore MOVE between the reference's own -O0 and -O3. The scan that
// built the angle list (6e7 draws over 0..2pi at g++ -O3, -ffp-contract=off) found the
// two forms disagreeing on about 1 component in 1466.
//
// So this is not a language substitution to be alarmed about; it is the same shape of
// problem as T-919: the baselines are all ldc2 -O0 captures, -O0's form is the contract,
// and a port compiled at -O3 must not be left to the optimiser's discretion. The port
// keeps the pair two calls (eil::sin / eil::cos), and this corpus pins that. The 400
// `div` angles are the ones that can tell the two forms apart; the 15 `ctrl` angles are
// there so the replay also proves the ordinary cases still match.
//
// Build and capture (reproducible; the gdtk tree itself is read-only):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip \
//     probes/substrate/probe_sin_cos_pair.d -of=/tmp/probe_scp -od/tmp/probe_obj_scp
//   /tmp/probe_scp > baselines/e587b235/probes/substrate_sin_cos_pair.txt
// NO -O flag, deliberately: at ldc2 -O1/-O3 the same source prints a different corpus
// (the 401 rows above), which is exactly what this file is set up to demonstrate.
//
// The angle is parsed by hand from its hex encoding rather than with to!double, so no
// library conversion sits between the input and the function under measurement.

import std.array;
import std.file;
import std.format;
import std.math;
import std.stdio;
import std.string;

string hexify(ulong u)
{
    return format("%016x", u);
}

int nib(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

ulong unhex64(const char[] h)
{
    ulong res = 0;
    foreach (c; h) res = res * 16 + nib(c);
    return res;
}

void one(const string line)
{
    auto parts = line.split();
    if (parts.length < 2) return;
    const ulong au = unhex64(parts[0]);
    const double x = *cast(double*)&au;
    const double c = cos(x);
    const double s = sin(x);
    const ulong cu = *cast(ulong*)&c;
    const ulong su = *cast(ulong*)&s;
    writefln("%s %s cos=%s sin=%s cosg=%.17g sing=%.17g",
             parts[0], parts[1], hexify(cu), hexify(su), c, s);
}

void main(string[] args)
{
    const string path = args.length > 1 ? args[1] : "tests/test_data/sincos_angle.txt";
    foreach (raw; (cast(string) read(path)).split("\n")) {
        if (raw.length == 0 || raw[0] == '#') continue;
        one(raw);
    }
}
