// probes/geom/surface/probe_subrangedsurface.d — what
// geom/surface/subrangedsurface.d (45 lines) produces.
//
// SubRangedSurface is a thin adapter: it stores a *dup* of an underlying surface and
// four re-mapping constants, and opCall (D:27-32) linearly maps (r,s) into
// [r0,r1]x[s0,s1] before forwarding. Everything observable is therefore (a) the
// mapping arithmetic, (b) the fact the ctor dups (D:21), so later mutation of the
// caller's surface cannot reach it, and (c) the exact toString() text (D:37-44),
// which interpolates to!string of a *class handle* for the member — so the member
// text is the underlying surface's own toString().
//
// Why measuring rather than reasoning:
//  * the ctor takes defaults r0=0, r1=1, s0=0, s1=1 (D:19); a default-built object
//    must be the identity map, and that is dumped so a swapped pair (s0/r0) shows.
//  * opCall reassigns its OWN PARAMETERS (D:29-30), so the forwarded coordinates are
//    r0+(r1-r0)*r and s0+(s1-s0)*s. Every endpoint is dumped twice: once through the
//    adapter and once as a direct call on the underlying surface at the mapped
//    coordinates, so the mapping is checked, not assumed.
//  * dup() (D:33-36) goes back through the constructor, which dups again — so a dup
//    of a chain of N adapters nests N deep. Dumped for a one-level and a two-level
//    chain.
//  * D:40 `to!string(underlying_surface)` on a null handle: the only way to get one
//    is to hand the constructor a null, and that dies at D:21 with SIGSEGV (exit 139,
//    no throwable caught by `catch (Throwable)`), so no row can be written for it —
//    measured, and recorded in group 6's comment instead.
//  * there is no enforce/throw of any kind in this module and no classString override,
//    because ParametricSurface declares no classString (the D base is
//    geom/surface/parametricsurface.d:34-68: opCall, dup, toString, area only).
//
// Build and capture:
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface \
//     probes/geom/surface/probe_subrangedsurface.d -of=/tmp/probe_srs \
//     -od=/tmp/w2_surfB/obj_srs
//   /tmp/probe_srs > baselines/e587b235/probes/geom_surface_subrangedsurface.txt
import std.algorithm;
import std.array;
import std.conv;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.surface.parametricsurface;
import geom.surface.subrangedsurface;
import nm.number;

// The same bilinear patch the base-class probe uses, so the underlying arithmetic is
// already pinned by baselines/e587b235/probes/geom_surface_parametricsurface.txt and
// only the adapter is under test here.
class ProbeSurface : ParametricSurface {
    Vector3 p00, p10, p01, p11;

    this(Vector3 p00, Vector3 p10, Vector3 p01, Vector3 p11) {
        this.p00 = p00;
        this.p10 = p10;
        this.p01 = p01;
        this.p11 = p11;
    }
    override Vector3 opCall(double r, double s) const {
        Vector3 a = p00 + (p10 - p00) * r;
        Vector3 b = p01 + (p11 - p01) * r;
        return a + (b - a) * s;
    }
    override ParametricSurface dup() const {
        return new ProbeSurface(p00, p10, p01, p11);
    }
    override string toString() const {
        return format("ProbeSurface(%s, %s, %s, %s)", to!string(p00), to!string(p10),
                      to!string(p01), to!string(p11));
    }
}

void dump_pt(string key, const Vector3 p) {
    writefln("%s x=%.17g y=%.17g z=%.17g", key, p.x, p.y, p.z);
}

void dump_members(string key, SubRangedSurface sr) {
    writefln("%s underlying=%s r0=%.17g r1=%.17g s0=%.17g s1=%.17g", key,
             to!string(sr.underlying_surface), sr.r0, sr.r1, sr.s0, sr.s1);
}

void main()
{
    // A tilted, sheared patch: all three components non-zero in the interior, so a
    // mapping that swaps r and s is visible in every column.
    auto skew = new ProbeSurface(Vector3(0.5, -0.25, 0.1), Vector3(2.0, 0.75, -0.3),
                                 Vector3(-0.5, 1.5, 0.9), Vector3(1.25, 2.5, 0.05));

    // --- group 1: default construction (D:18-26) and its four data members ---------
    auto dflt = new SubRangedSurface(skew);
    dump_members("srs.dflt.members", dflt);
    writefln("srs.dflt.str=%s", dflt.toString());
    // defaults must be the identity map: compare adapter vs underlying.
    foreach (rs; [0.0, 0.125, 0.25, 0.5, 0.75, 1.0]) {
        dump_pt(format("srs.dflt.opCall r=%s", to!string(rs)), dflt(rs, 1.0 - rs));
        dump_pt(format("srs.dflt.direct r=%s", to!string(rs)), skew(rs, 1.0 - rs));
    }

    // --- group 2: the parameter remap, dumped at the range endpoints -------------
    struct Case { string tag; double r0, r1, s0, s1; }
    auto cases = [Case("wide", 2.0, 5.0, -1.0, 3.0), Case("neg", -3.0, -1.0, -2.0, -0.5),
                  Case("flip", 1.0, 0.0, 1.0, 0.0), Case("degenR", 0.5, 0.5, 0.0, 1.0),
                  Case("degenS", 0.0, 1.0, 0.25, 0.25), Case("tiny", 0.0, 1e-8, 0.0, 1.0),
                  Case("big", 0.0, 1e8, 0.0, 1e8)];
    foreach (c; cases) {
        auto sr = new SubRangedSurface(skew, c.r0, c.r1, c.s0, c.s1);
        writefln("srs.%s.str=%s", c.tag, sr.toString());
        dump_members("srs." ~ c.tag ~ ".members", sr);
        foreach (r; [0.0, 0.3, 1.0]) {
            foreach (s; [0.0, 0.6, 1.0]) {
                // adapter output, and the underlying surface at the mapped point the
                // ctor's formula (D:29-30) predicts — the two must agree.
                double rr = c.r0 + (c.r1 - c.r0) * r;
                double ss = c.s0 + (c.s1 - c.s0) * s;
                dump_pt(format("srs.%s.opCall r=%.17g s=%.17g", c.tag, r, s), sr(r, s));
                writefln("srs.%s.mapped r=%.17g s=%.17g rr=%.17g ss=%.17g", c.tag, r, s, rr, ss);
                dump_pt(format("srs.%s.direct r=%.17g s=%.17g", c.tag, rr, ss), skew(rr, ss));
            }
        }
    }

    // --- group 3: quadrant sweep through one adapter ------------------------------
    auto q = new SubRangedSurface(skew, 1.0, 3.0, 2.0, 6.0);
    foreach (r; [0.0, 0.2, 0.49, 0.51, 0.8, 1.0]) {
        foreach (s; [0.0, 0.2, 0.49, 0.51, 0.8, 1.0]) {
            dump_pt(format("srs.quad.opCall r=%.17g s=%.17g", r, s), q(r, s));
        }
    }

    // --- group 4: dup() (D:33-36) — goes through the ctor, which dups again ------
    auto dd = cast(SubRangedSurface) q.dup();
    writefln("srs.dup.str=%s", dd.toString());
    dump_members("srs.dup.members", dd);
    dump_pt("srs.dup.opCallA", dd(0.25, 0.75));
    dump_pt("srs.dup.opCallB", dd(1.0, 0.0));
    // a two-level chain: dup must nest, and the inner range must be applied first.
    auto chain = new SubRangedSurface(q, 0.0, 0.5, 0.5, 1.0);
    writefln("srs.chain.str=%s", chain.toString());
    auto cd = cast(SubRangedSurface) chain.dup();
    writefln("srs.chain.dup.str=%s", cd.toString());
    foreach (rs; [0.0, 0.5, 1.0]) {
        dump_pt(format("srs.chain.opCall r=%.17g", rs), chain(rs, rs));
        dump_pt(format("srs.chain.dup.opCall r=%.17g", rs), cd(rs, rs));
    }

    // --- group 5: the adapter is still a ParametricSurface: area() from the base --
    Vector3 a1 = dflt.area();
    writefln("srs.area.default x=%.17g y=%.17g z=%.17g", a1.x, a1.y, a1.z);
    Vector3 a2 = q.area(3, 7);
    writefln("srs.area.r3s7 x=%.17g y=%.17g z=%.17g", a2.x, a2.y, a2.z);
    Vector3 a3 = dflt.area(7, 3);
    writefln("srs.area.r7s3 x=%.17g y=%.17g z=%.17g", a3.x, a3.y, a3.z);
    Vector3 a4 = chain.area(1, 1);
    writefln("srs.area.chain11 x=%.17g y=%.17g z=%.17g", a4.x, a4.y, a4.z);

    // --- group 6: out-of-range and non-finite parameters --------------------------
    // This module has no enforce and no throw at all (measured by grep over the 45
    // lines: subrangedsurface.d contains neither "enforce" nor "throw"), so the only
    // failure mode is a null member — and that is not catchable: `new
    // SubRangedSurface(null)` dies inside psurf.dup() (D:21) with SIGSEGV and NO
    // throwable, measured as exit status 139 with the last stderr marker before the
    // call ("i-caught" never printed, see the note in
    // src/geom/surface/subrangedsurface.hpp). The port therefore cannot print a
    // message for it, and must not invent an enforce D does not have.
    // Out-of-range (r,s) is *not* validated by D: the map is affine, so r=2 is legal.
    dump_pt("srs.outofrange.r2s2", dflt(2.0, 2.0));
    dump_pt("srs.outofrange.rneg", dflt(-1.0, -0.5));
    // NaN/infinite parameters: the affine map propagates them, printed by %.17g.
    dump_pt("srs.nan.r", dflt(0.0 / 0.0, 0.5));
    dump_pt("srs.nan.s", dflt(0.5, 1.0 / 0.0));
}
