// probes/geom/surface/probe_cubepatch.d — what geom/surface/cubepatch.d produces.
//
// CubePatch (D:21-116) maps the unit square onto a face of the cube of edge 2a centred
// on `centre`, optionally onto a half or quarter of that face via `which_part`.
//
// What is measured rather than reasoned:
//  * the first switch (D:63-71) is the whole face mapping, and it is the axis assignment
//    that matters (which of x/y/z is the constant one, and whether the varying pair is
//    (r,s) or (r,s) in the other order). All six canonical faces are swept over
//    r,s ∈ {0.0, 0.25, 0.5, 0.75, 1.0}, which includes both parameter boundaries and
//    one point in each parametric quadrant.
//  * the return (D:104) is `x_cube*a + centre.x`, i.e. scale-then-add per component. A
//    port that factored (x_cube + centre.x)*a, or that scaled the cube coordinate before
//    the sign was applied, differs — so a non-unit a (2.5) and an off-origin centre are
//    swept as well.
//  * `which_part` is a SUBSTRING test (canFind(which_part, "south") etc., D:81-98) and
//    the four ifs are independent, so "south-top" gives a quarter face while the
//    self-confessed nonsense "south-north" silently lets the LAST if win (y_cube = r).
//    D's own comment (D:75-77) calls that the user's responsibility; it is dumped, not
//    corrected. Same for a which_part that matches nothing ("xyz") and for the mixed
//    orders ("top-south" vs "south-top").
//  * the face name is validated against the six names EXACTLY (canFind over the array at
//    D:38-41), so "east-west" and "East" are rejected while which_part accepts any string
//    at all — the asymmetry is dumped through both a rejected and an accepted case.
//  * the two throws are `new Exception` (not Error), and they are different texts:
//    "Invalid face name: " (constructor) vs "Invalid face: " (opCall). Which type
//    escapes matters here because Exception and ExceptionBase's sibling Error are not
//    interchangeable in the port.
//  * opCall's default branches (D:70 and D:100-101) cannot be reached through a
//    constructor-validated face_name, but face_name is a PUBLIC field (D:25), so the
//    first is reachable by assignment and is dumped. The second switch's default is
//    shown unreachable by construction: all six valid names are handled there, so no
//    which_part value can reach it — measured by sweeping all six names with which_part
//    set and seeing no throw.
//  * D:34 `this(number a, Vector3 centre, string face_name, string which_part="")` — the
//    default is the empty string, which is also the only state where the half/quarter
//    branch (D:72 `if (which_part.length > 0)`) is skipped. Dumped as a member row.
//  * area() (inherited, parametricsurface.d:40-66) is summed over a planar face, so its
//    magnitude is the face area; at the D defaults (10,10) and an asymmetric (3,7).
//  * dup() (D:54-57) goes back through the VALIDATING constructor, so a patch whose
//    face_name was corrupted cannot be duped — that throw is dumped. The copy
//    constructor (D:46-52) does not validate, and that difference is the observable.
//  * D:118-152's unittest writes two lines (D:127, D:134); they are reproduced as
//    cubp.unittest.* rows so the ported test replays the same bytes.
//
// Build and capture (see probes/geom/surface/probe_parametricsurface.d for why the
// reference tree is copied with @nogc stripped):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface \
//     probes/geom/surface/probe_cubepatch.d -of=/tmp/probe_cbp -od/tmp/probe_obj_w2_surfA
//   /tmp/probe_cbp > baselines/e587b235/probes/geom_surface_cubepatch.txt
import std.array;
import std.algorithm;
import std.conv;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.gpath;
import geom.surface.cubepatch;
import geom.surface.parametricsurface;
import nm.number;

void dump_patch(const char[] tag, CubePatch pp) {
    writefln("%s str=%s", tag, pp.toString());
    writefln("%s classname=%s", tag, split(to!string(typeid(pp).name), ".")[$-1]);
    writefln("%s a=%.17g centre x=%.17g y=%.17g z=%.17g face=%s which_part=%s which_part.len=%d",
             tag, pp.a, pp.centre.x, pp.centre.y, pp.centre.z, pp.face_name, pp.which_part,
             pp.which_part.length);
}

void dump_sweep(const char[] tag, ParametricSurface ps) {
    foreach (r; [0.0, 0.25, 0.5, 0.75, 1.0]) {
        foreach (s; [0.0, 0.25, 0.5, 0.75, 1.0]) {
            auto p = ps(r, s);
            writefln("%s opCall r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g", tag, r, s, p.x, p.y,
                     p.z);
        }
    }
}

void dump_area(const char[] tag, ParametricSurface ps, int nr, int ns) {
    auto a = ps.area(nr, ns);
    writefln("%s area nr=%d ns=%d x=%.17g y=%.17g z=%.17g", tag, nr, ns, a.x, a.y, a.z);
}

void dump_throw(const char[] tag, void delegate() dg) {
    try {
        dg();
        writefln("%s threw=none", tag);
    } catch (Error e) {
        writefln("%s threw=Error msg=%s", tag, e.msg);
    } catch (Exception e) {
        writefln("%s threw=Exception msg=%s", tag, e.msg);
    }
}

void main()
{
    //--- group A: the six canonical faces, a=1 at the origin ---------------------
    foreach (face; ["north", "east", "south", "west", "bottom", "top"]) {
        auto pp = new CubePatch(1.0, Vector3(0.0, 0.0, 0.0), face);
        string tag = format("cubp.f.%s", face);
        dump_patch(tag, pp);
        dump_sweep(tag, pp);
        dump_area(tag, pp, 10, 10);
        dump_area(tag, pp, 3, 7);
    }

    //--- group B: a non-unit cube, off-origin centre ----------------------------
    auto big = new CubePatch(2.5, Vector3(0.5, -1.25, 3.0), "east");
    dump_patch("cubp.big", big);
    dump_sweep("cubp.big", big);
    dump_area("cubp.big", big, 10, 10);
    dump_area("cubp.big", big, 3, 7);
    auto westbig = new CubePatch(2.5, Vector3(0.5, -1.25, 3.0), "west");
    dump_patch("cubp.big.west", westbig);
    dump_sweep("cubp.big.west", westbig);
    dump_area("cubp.big.west", big, 10, 10);

    // a fractional a, to show to!string(a)'s %.6g form in toString()
    auto frac = new CubePatch(1.0 / 3.0, Vector3(0.0, 0.0, 0.0), "top");
    dump_patch("cubp.frac", frac);
    dump_sweep("cubp.frac", frac);
    dump_area("cubp.frac", frac, 10, 10);

    //--- group C: dup() and the copy constructor -------------------------------
    auto d = big.dup();
    dump_patch("cubp.big.dup", d);
    dump_sweep("cubp.big.dup", d);
    dump_area("cubp.big.dup", d, 10, 10);
    auto c = new CubePatch(big);
    dump_patch("cubp.big.copy", c);
    dump_sweep("cubp.big.copy", c);
    dump_area("cubp.big.copy", d, 3, 7);

    //--- group D: which_part — halves, quarters, and the nonsense cases --------
    foreach (part; ["", "south", "north", "bottom", "top", "south-top", "north-bottom",
                    "west-east", "south-north", "bottom-top", "xyz", "east", "top-south"]) {
        foreach (face; ["east", "south", "bottom"]) {
            string tag = format("cubp.part.%s.%s", face, (part.length == 0) ? "empty" : part);
            auto pp = new CubePatch(1.0, Vector3(0.0, 0.0, 0.0), face, part);
            dump_patch(tag, pp);
            dump_sweep(tag, pp);
        }
    }
    // the remaining faces at the corners the parts are meant to select
    foreach (face; ["west", "north", "top"]) {
        foreach (part; ["south-top", "north-bottom", "west", "east"]) {
            string tag = format("cubp.part2.%s.%s", face, part);
            auto pp = new CubePatch(1.0, Vector3(0.0, 0.0, 0.0), face, part);
            dump_patch(tag, pp);
            writefln("%s corner r=0 s=0 x=%.17g y=%.17g z=%.17g", tag, pp(0.0, 0.0).x,
                     pp(0.0, 0.0).y, pp(0.0, 0.0).z);
            writefln("%s corner r=1 s=1 x=%.17g y=%.17g z=%.17g", tag, pp(1.0, 1.0).x,
                     pp(1.0, 1.0).y, pp(1.0, 1.0).z);
        }
    }

    //--- group E: the constructor's face-name guard ----------------------------
    foreach (bad; ["", " ", "East", "EAST", "east-west", "east ", " east", "n", "north-south",
                   "side"]) {
        dump_throw(format("cubp.badname.%s", (bad.length == 0) ? "empty" : bad), () {
            new CubePatch(1.0, Vector3(0.0, 0.0, 0.0), bad);
        });
    }
    // every valid name must construct (and construct with which_part, unchecked)
    foreach (good; ["north", "east", "south", "west", "bottom", "top"]) {
        dump_throw(format("cubp.goodname.%s", good), () {
            new CubePatch(1.0, Vector3(0.0, 0.0, 0.0), good, "top");
        });
    }

    //--- group F: opCall's default branch, reached through the public field -----
    auto mut = new CubePatch(1.0, Vector3(0.0, 0.0, 0.0), "east");
    mut.face_name = "sideways";
    dump_patch("cubp.mutated", mut);
    dump_throw("cubp.mutated.opCall", () { mut(0.5, 0.5); });
    dump_throw("cubp.mutated.area", () { mut.area(); });
    dump_throw("cubp.mutated.dup", () { mut.dup(); });
    // with which_part set, the FIRST switch still throws before the second is reached
    auto mut2 = new CubePatch(1.0, Vector3(0.0, 0.0, 0.0), "top", "south");
    mut2.face_name = "bogus";
    dump_throw("cubp.mutated2.opCall", () { mut2(0.5, 0.5); });
    // the copy constructor keeps an invalid name (it does not validate)
    auto mut3 = new CubePatch(mut2);
    dump_patch("cubp.copy.invalid", mut3);
    dump_throw("cubp.copy.invalid.opCall", () { mut3(0.5, 0.5); });

    //--- group G: the D unittest's own observable values (D:118-152) -----------
    {
        number a = 1.0;
        number zero = 0.0;
        auto east_patch = new CubePatch(a, Vector3(0.0, 0.0, 0.0), "east");
        auto mid_e = east_patch(0.5, 0.5);
        auto p5_east = east_patch(0.0, 1.0);
        auto p6_east = east_patch(1.0, 1.0);
        writefln("cubp.unittest mid_e_line=%s", "mid_e=" ~ to!string(mid_e));
        writefln("cubp.unittest mid_e x=%.17g y=%.17g z=%.17g", mid_e.x, mid_e.y, mid_e.z);
        writefln("cubp.unittest p5_east x=%.17g y=%.17g z=%.17g", p5_east.x, p5_east.y,
                 p5_east.z);
        writefln("cubp.unittest p6_east x=%.17g y=%.17g z=%.17g", p6_east.x, p6_east.y,
                 p6_east.z);

        auto top_patch = new CubePatch(a, Vector3(0.0, 0.0, 0.0), "top");
        auto mid_t = top_patch(0.5, 0.5);
        auto p5_top = top_patch(1.0, 0.0);
        auto p6_top = top_patch(1.0, 1.0);
        writefln("cubp.unittest mid_t_line=%s", "mid_t=" ~ to!string(mid_t));
        writefln("cubp.unittest mid_t x=%.17g y=%.17g z=%.17g", mid_t.x, mid_t.y, mid_t.z);
        writefln("cubp.unittest p5_top x=%.17g y=%.17g z=%.17g", p5_top.x, p5_top.y, p5_top.z);
        writefln("cubp.unittest p6_top x=%.17g y=%.17g z=%.17g", p6_top.x, p6_top.y, p6_top.z);

        // D:139 assert(isClose(abs(p6_top), sqrt(3 * a))); — the two operands and the
        // verdict, so the ported assertion is replayed against D's own numbers.
        number m = abs(p6_top);
        number s = sqrt(3 * a);
        writefln("cubp.unittest abs_p6_top=%.17g sqrt3a=%.17g isClose=%s", m, s, isClose(m, s));

        auto south_patch = new CubePatch(a, Vector3(0.0, 0.0, 0.0), "south", "top");
        auto mid_s = south_patch(0.5, 0.0);
        auto p5_south = south_patch(1.0, 1.0);
        writefln("cubp.unittest mid_s x=%.17g y=%.17g z=%.17g", mid_s.x, mid_s.y, mid_s.z);
        writefln("cubp.unittest p5_south x=%.17g y=%.17g z=%.17g", p5_south.x, p5_south.y,
                 p5_south.z);

        auto north_patch = new CubePatch(a, Vector3(0.0, 0.0, 0.0), "north", "top-east");
        auto mid_n = north_patch(0.0, 0.0);
        auto p6_north = north_patch(1.0, 1.0);
        writefln("cubp.unittest mid_n x=%.17g y=%.17g z=%.17g", mid_n.x, mid_n.y, mid_n.z);
        writefln("cubp.unittest p6_north x=%.17g y=%.17g z=%.17g", p6_north.x, p6_north.y,
                 p6_north.z);
    }

    //--- group H: NaN and extreme parameters -----------------------------------
    auto nanp = new CubePatch(1.0, Vector3(), "east");
    dump_patch("cubp.nan.centre", nanp);
    dump_sweep("cubp.nan.centre", nanp);
    dump_area("cubp.nan.centre", nanp, 10, 10);
    auto nanA = new CubePatch(number.init, Vector3(0.0, 0.0, 0.0), "top");
    dump_patch("cubp.nan.a", nanA);
    dump_sweep("cubp.nan.a", nanA);
    auto zeroA = new CubePatch(0.0, Vector3(0.0, 0.0, 0.0), "bottom");
    dump_patch("cubp.zero.a", zeroA);
    dump_sweep("cubp.zero.a", zeroA);
    dump_area("cubp.zero.a", zeroA, 10, 10);
}
