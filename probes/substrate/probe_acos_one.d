// probes/substrate/probe_acos_one.d — what Phobos' asin/acos return for arguments near
// the ±1 endpoint, measured against ldc2 1.40.0. Backlog T-011 / T-931.
//
// Why this probe exists: paver.d:577 computes `acos(r)` with the cosine rule, and on the
// paver_demo.d boundary two of its 422 nodes land at r = -0.99999999999… There, acos has a
// square-root singularity, so a difference in *how* acos is evaluated (80-bit vs double,
// Phobos vs glibc) is not last-digit in the argument — it moves the result by 1 ULP, which
// then moves a paving node by 1 ULP and the whole interior by 398 vertices in the last
// digit. The existing 11-sample check (backlog T-911) could not see this because none of
// its arguments were near ±1.
//
// Build and capture (reproducible):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -O0 probes/substrate/probe_acos_one.d -of=/tmp/probe_acos -od/tmp/probe_obj_acos
//   /tmp/probe_acos > baselines/e587b235/probes/substrate_acos_one.txt
// Replay: ctest -R substrate_acos_one (tests/test_substrate_acos_one.cpp).
//
// Rows are emitted as hex bit patterns on both sides so the comparison is on the pattern,
// not on a decimal rendering: an argument that is the same number to 17 significant digits
// can still be a different double.
import std.stdio;
import std.format;
import std.math;

void R(string kind, double x, double v)
{
    pragma(once);
    writefln("%s|%016x|out=%016x", kind, *cast(ulong*) &x, *cast(ulong*) &v);
}

void main()
{
    // Sweep 1: distance from -1 and +1, in a geometric ladder that walks right up to the
    // endpoint, including the exact endpoint and one past it (domain error).
    for (int e = 0; e <= 60; ++e)
    {
        double d = 0x1p-53 * (e + 1);
        R("acosm1", -1.0 + d, acos(-1.0 + d));
        R("acosp1", 1.0 - d, acos(1.0 - d));
        R("asinm1", -1.0 + d, asin(-1.0 + d));
        R("asinp1", 1.0 - d, asin(1.0 - d));
        R("acosm1x", -1.0 - d, acos(-1.0 - d));   // outside the domain
        R("acosp1x", 1.0 + d, acos(1.0 + d));
    }
    // Sweep 2: the exact endpoint and one ULP either side.
    double m1 = -1.0, p1 = 1.0;
    double m1u = nextafter(-1.0, double.infinity);
    double p1u = nextafter(1.0, double.infinity);
    R("acosedge", m1, acos(m1));
    R("acosedge", p1, acos(p1));
    R("acosedge", m1u, acos(m1u));
    R("acosedge", p1u, acos(p1u));
    R("asinedge", m1, asin(m1));
    R("asinedge", p1, asin(p1));
    R("asinedge", m1u, asin(m1u));
    R("asinedge", p1u, asin(p1u));
    // Sweep 3: dense uniform sampling over the whole domain, to bound the general rate.
    for (int i = 0; i <= 4000; ++i)
    {
        double x = -1.0 + 2.0 * i / 4000.0;
        R("acosuni", x, acos(x));
        R("asinuni", x, asin(x));
    }
    // Sweep 4: the cosine-rule arguments this actually came from.
    double[] rs = [-0.99999999999999978, -0.99999999999999889, -0.99999999837404541,
                   0.99999999999999989, 0.99999999999998, 0.5, -0.5, 0.0, -0.0];
    foreach (x; rs)
    {
        R("acoscdr", x, acos(x));
        R("asincdr", x, asin(x));
    }
    // Sweep 5: the specials, because the sign of a NaN and of a zero is observable in
    // the printed text and in every later division by them.
    double[] sp = [0.0, -0.0, 1.0, -1.0, double.infinity, -double.infinity, double.nan,
                   -double.nan, 0x1.0p-1074, -0x1.0p-1074];
    foreach (x; sp)
    {
        R("acosspec", x, acos(x));
        R("asinspec", x, asin(x));
        double up = x + 0x1p-30;      // the column must be the ARGUMENT, not the base
        double dn = x - 0x1p-30;
        R("acosup", up, acos(up));
        R("asinup", up, asin(up));
        R("acosdn", dn, acos(dn));
        R("asindn", dn, asin(dn));
    }
    // The formula Phobos defines, evaluated in the compiler's own real, as a row source.
    foreach (x; rs ~ sp)
    {
        real r = cast(real) x;
        real s = cast(real) 1.0 - r * r;
        double v = cast(double) atan2(r, sqrt(s));
        R("asinatan2", x, v);
        double w = cast(double) atan2(sqrt(s), r);
        R("acosatan2", x, w);
    }
    writefln("count|%d", 1);
}
