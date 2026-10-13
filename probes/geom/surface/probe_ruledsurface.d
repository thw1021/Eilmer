// probes/geom/surface/probe_ruledsurface.d — what geom/surface/ruledsurface.d
// (88 lines) produces.
//
// RuledSurface interpolates linearly between two Paths along one parametric
// direction (D:64-81). The observable surface is small but exact: two class handles,
// a direction string, a pure2D flag, four cached corner points, one string switch in
// the constructor (D:27-44) and the same switch again in opCall (D:67-78).
//
// Why measuring rather than reasoning:
//  * the corner assignment PATTERN differs between the two direction families and is
//    the only thing the constructor computes: "r"/"i" makes edge0 the WEST edge
//    (p00=edge0(0), p01=edge0(1)) and edge1 the EAST one, "s"/"j" makes edge0 the
//    SOUTH edge (p00=edge0(0), p10=edge0(1)). Both are dumped for all four accepted
//    strings, because a port that mixed up p10/p01 would still evaluate opCall
//    correctly and only the members would be wrong.
//  * opCall's blend (D:70, :74) is `(1.0-r)*edge0(s) + r*edge1(s)`, i.e. the scalar
//    1.0-r multiplies a Vector3 — the order of the two products and of the add is
//    D's and is dumped on a 6x6 (r,s) sweep hitting 0.0 and 1.0 exactly and two
//    points in each parametric quadrant.
//  * pure2D (D:79) rewrites p.z AFTER the blend, so a patch whose edges have z != 0
//    shows it; dumped for both flag values.
//  * to!string(pure2D) is "true"/"false" (D's bool), and to!string(edge0) is the
//    Path's own toString(), so the text depends on two foreign classes' formatting —
//    measured here rather than assumed.
//  * BOTH switches have a `default:` throwing `new Exception("Invalid string for
//    ruled_direction: " ~ ruled_direction)` (D:43, D:77). The constructor's is easy;
//    opCall's is reachable from outside only because every field is public:
//    `ruled_direction = "q"` after construction. Both are captured, printing WHICH
//    throwable type fired (Exception, not Error — the two are siblings here).
//  * the copy constructor (D:47-57) re-dups the two edges and copies the four corners
//    verbatim, without re-running the switch — so a tampered object copies its invalid
//    string into a new object whose corners are still the old ones. dup() (D:59-62)
//    does NOT use it: it goes back through the validating constructor. Both are
//    dumped, and the difference is observable.
//
// Build and capture (one line, so the header comment stays a single // comment):
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface probes/geom/surface/probe_ruledsurface.d -of=/tmp/probe_rs -od=/tmp/w2_surfB/obj_rs
//   then /tmp/probe_rs > baselines/e587b235/probes/geom_surface_ruledsurface.txt
import std.algorithm;
import std.array;
import std.conv;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.gpath;
import geom.surface.parametricsurface;
import geom.surface.ruledsurface;
import nm.number;

string esc(string s) { return replace(replace(s, "\n", "<newline>"), "\r", "<cr>"); }

void dump_v3(string key, const Vector3 v) {
    writefln("%s x=%.17g y=%.17g z=%.17g", key, v.x, v.y, v.z);
}

void dump_all(string key, RuledSurface rs) {
    writefln("%s str=%s", key, rs.toString());
    writefln("%s dir=%s pure2D=%s", key, rs.ruled_direction, to!string(rs.pure2D));
    writefln("%s e0=%s e1=%s", key, to!string(rs.edge0), to!string(rs.edge1));
    dump_v3(key ~ ".p00", rs.p00);
    dump_v3(key ~ ".p10", rs.p10);
    dump_v3(key ~ ".p01", rs.p01);
    dump_v3(key ~ ".p11", rs.p11);
}

void sweep(string key, RuledSurface rs) {
    foreach (r; [0.0, 0.125, 0.3, 0.7, 0.875, 1.0]) {
        foreach (s; [0.0, 0.125, 0.3, 0.7, 0.875, 1.0]) {
            dump_v3(format("%s.opCall r=%.17g s=%.17g", key, r, s), rs(r, s));
        }
    }
}

void dump_area(string key, ParametricSurface ps, int nr, int ns) {
    auto a = ps.area(nr, ns);
    writefln("%s nr=%d ns=%d area x=%.17g y=%.17g z=%.17g", key, nr, ns, a.x, a.y, a.z);
}

void main()
{
    // Two straight edges that differ in all three components, plus a cubic Bezier so a
    // non-linear edge is in the mix: to!string of the members must then carry the
    // path's own text.
    auto west = new Line(Vector3(0.0, 0.0, 0.0), Vector3(0.0, 1.0, 0.25));
    auto east = new Line(Vector3(2.0, 0.5, -0.5), Vector3(2.0, 1.5, 0.75));
    auto bez = new Bezier([Vector3(0.5, -1.0, 0.125), Vector3(1.0, 0.5, -0.25),
                           Vector3(1.5, 1.0, 0.5), Vector3(2.5, 2.0, -0.125)]);

    // --- group 1: the four accepted direction strings, both pure2D values ---------
    foreach (dir; ["r", "i", "s", "j"]) {
        auto rs = new RuledSurface(west, east, dir, false);
        dump_all("rs.dir" ~ dir, rs);
        sweep("rs.dir" ~ dir, rs);
        auto rs2d = new RuledSurface(west, east, dir, true);
        dump_all("rs2d.dir" ~ dir, rs2d);
        sweep("rs2d.dir" ~ dir, rs2d);
    }

    // --- group 2: a curved edge, "s" and "r", and the coincident/degenerate edges -
    auto cb = new RuledSurface(west, bez, "s", false);
    dump_all("rs.bezS", cb);
    sweep("rs.bezS", cb);
    auto cr = new RuledSurface(bez, east, "r", false);
    dump_all("rs.bezR", cr);
    sweep("rs.bezR", cr);
    auto same = new RuledSurface(west, west, "s", false);
    dump_all("rs.sameEdge", same);
    sweep("rs.sameEdge", same);
    auto zero = new RuledSurface(new Line(Vector3(1.0, 2.0, 3.0), Vector3(1.0, 2.0, 3.0)), east,
                                 "r", false);
    dump_all("rs.zeroLen", zero);
    sweep("rs.zeroLen", zero);
    // pure2D with a curved edge whose z is non-zero everywhere.
    auto bez2d = new RuledSurface(bez, bez, "j", true);
    dump_all("rs.bez2d", bez2d);
    sweep("rs.bez2d", bez2d);

    // --- group 3: area() from the base, default and asymmetric -------------------
    dump_area("rs.area.default", cb, 10, 10);
    dump_area("rs.area.r3s7", cb, 3, 7);
    dump_area("rs.area.r7s3", cb, 7, 3);
    dump_area("rs.area.r1s1", cb, 1, 1);
    dump_area("rs.area.same.default", same, 10, 10);
    dump_area("rs.area.zero.default", zero, 10, 10);
    dump_area("rs.area.bez2d.r5s2", bez2d, 5, 2);

    // --- group 4: dup() (validating ctor) vs the copy constructor -----------------
    auto d = cast(RuledSurface) cb.dup();
    dump_all("rs.dup", d);
    dump_v3("rs.dup.opCall", d(0.3, 0.7));
    auto cp = new RuledSurface(cb);
    dump_all("rs.copy", cp);
    dump_v3("rs.copy.opCall", cp(0.3, 0.7));
    // The copy's edges are dups, not the same handles: mutate the original's cached
    // corners and show the copy does not follow.
    cb.p11 = Vector3(9.0, 9.0, 9.0);
    dump_v3("rs.copy.p11.afterMutate", cp.p11);
    dump_v3("rs.orig.p11.afterMutate", cb.p11);

    // --- group 5: both guard paths ------------------------------------------------
    foreach (bad; ["q", "", "R", "S", " i", "ii"]) {
        try {
            auto rs = new RuledSurface(west, east, bad, false);
            writefln("rs.ctorGuard.%s.noThrow str=%s", bad, rs.toString());
        } catch (Exception e) {
            writefln("rs.ctorGuard.%s.thrown type=%s msg=%s", bad, e.classinfo.name, esc(e.msg));
        }
    }
    // opCall's default: only reachable by writing the public field afterwards.
    auto tam = new RuledSurface(west, east, "s", false);
    tam.ruled_direction = "q";
    writefln("rs.tampered.str=%s", tam.toString());
    try {
        dump_v3("rs.tampered.opCall", tam(0.5, 0.5));
    } catch (Exception e) {
        writefln("rs.tampered.thrown type=%s msg=%s", e.classinfo.name, esc(e.msg));
    }
    // area() through the base calls opCall, so the same guard fires there too.
    try {
        dump_area("rs.tampered.area", tam, 10, 10);
    } catch (Exception e) {
        writefln("rs.tampered.area.thrown type=%s msg=%s", e.classinfo.name, esc(e.msg));
    }
    // dup() of a tampered object re-runs the switch, so it throws at construction.
    try {
        auto dd = cast(RuledSurface) tam.dup();
        writefln("rs.tampered.dup.noThrow str=%s", dd.toString());
    } catch (Exception e) {
        writefln("rs.tampered.dup.thrown type=%s msg=%s", e.classinfo.name, esc(e.msg));
    }
    // The copy constructor does NOT validate: it copies "q" and the old corners.
    auto tc = new RuledSurface(tam);
    writefln("rs.tampered.copy.str=%s", tc.toString());
    dump_v3("rs.tampered.copy.p00", tc.p00);
    try {
        dump_v3("rs.tampered.copy.opCall", tc(0.25, 0.75));
    } catch (Exception e) {
        writefln("rs.tampered.copy.thrown type=%s msg=%s", e.classinfo.name, esc(e.msg));
    }
    // Nothing was caught by `catch (Error)`: these are Exceptions. Prove the two are
    // siblings by attempting the Error handler on the same throw.
    try {
        auto bad2 = new RuledSurface(west, east, "q", false);
    } catch (Error e) {
        writefln("rs.errorHandler.caught type=%s", e.classinfo.name);
    } catch (Exception e) {
        writefln("rs.errorHandler.fellThroughToException type=%s msg=%s", e.classinfo.name,
                 esc(e.msg));
    }
}
