// probes/geom/surface/probe_spherepatch.d — what geom/surface/spherepatch.d produces.
//
// SpherePatch (D:22-134) maps the unit cube's named faces onto a sphere of radius R
// about a centre, via the "cube-to-sphere" mapping of D:120-126. It is a
// ParametricSurface subclass, so the contract rows are toString()/dup()/opCall/area().
//
// What is measured rather than reasoned:
//  * the face switch (D:64-72) and the which_part switch (D:79-103) both run over the
//    SAME six names. The first switch pairs each face with the two cube axes that vary
//    (east/west vary y,z; south/north vary x,z; bottom/top vary x,y) — a swapped pairing
//    still gives a plausible sphere point, so every face is dumped at its four
//    parameter corners plus a mid point.
//  * the which_part rules (D:82-100) are the half/quarter-face clamps: "south" gives
//    -1+r and "north" gives r, i.e. the SECOND overwrite wins. "south-north" and
//    "top-bottom" are inconsistent-but-accepted inputs whose winner is therefore an
//    observable, so those pairs are dumped explicitly.
//  * D's two canFind forms differ: canFind over the ARRAY of valid names (D:40) is
//    exact element equality, canFind(which_part, "south") (D:82) is a SUBSTRING test.
//    "eastern" therefore contains "east", and "not-south" contains "south"; both are
//    probed so a port that confuses the two is caught.
//  * the constructor's only guard is D:40-42 `throw new Exception("Invalid face name: "
//    ~ face_name)`. Exception (not Error) — the row names which type escaped.
//  * D:43 deliberately does not re-check face_name, and the members are public, so
//    face_name can be mutated to a name neither switch knows. Then D:71 and D:102 are
//    reachable and both print "Invalid face: <name>". Both are dumped (with and without
//    a which_part, which is the only way to reach the second one).
//  * the copy constructor D:47-53 copies face_name WITHOUT validating, while dup()
//    D:55-58 re-enters the validating constructor. The pair sp.copy.invalid /
//    sp.dup.invalid distinguishes them: the copy survives with an invalid name and only
//    its opCall throws, the dup throws at construction.
//  * toString() (D:109-116) mixes formatters: to!string(radius) is the %.6g form,
//    to!string(centre) is Vector3.toString() ("Vector3(1, 2.2, 3)"), and face_name /
//    which_part are concatenated raw.
//  * sphere_mapping (D:122-124) takes sqrt of 1 - 0.5*a*a - 0.5*b*b + a*a*b*b/3.
//    Over [0,1]x[0,1] the argument is >= 1/3, but outside the unit square it goes
//    negative, and D's sqrt of a negative prints "nan" (positive NaN, wave-02-brief
//    section 5). Out-of-range (r,s) rows are therefore included; the module has no
//    atan2/asin site (measured: 0 matches for atan|asin|acos|sin|cos|tan over the
//    171 lines apart from the six sqrt rows at D:122-124,131-133), so the only
//    substrate function it touches is sqrt.
//  * radius = 0 and a NaN (default-constructed) centre are both dumped; the mapping
//    multiplies by radius, so a zero radius collapses the patch onto the centre and
//    there is no division anywhere in the module (checked: no `/` other than the
//    literal /3.0 at D:122-124).
//  * area() is the base sum (parametricsurface.d:40-66) at the D defaults (10,10), an
//    asymmetric (3,7) and the degenerate (1,1).
//
// Build and capture (see probes/geom/surface/probe_parametricsurface.d for why the
// reference tree is copied with @nogc stripped):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface \
//     probes/geom/surface/probe_spherepatch.d -of=/tmp/probe_sp -od/tmp/probe_obj_w2_surf2
//   /tmp/probe_sp > baselines/e587b235/probes/geom_surface_spherepatch.txt
import std.array;
import std.conv;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.surface.parametricsurface;
import geom.surface.spherepatch;
import nm.number;

void dump_state(const char[] tag, SpherePatch sp) {
    writefln("%s str=%s", tag, sp.toString());
    writefln("%s classname=%s", tag, split(to!string(typeid(sp).name), ".")[$-1]);
    writefln("%s radius=%.17g", tag, sp.radius);
    writefln("%s centre x=%.17g y=%.17g z=%.17g", tag, sp.centre.x, sp.centre.y, sp.centre.z);
    writefln("%s face=%s part=%s", tag, sp.face_name, sp.which_part);
}

/// The four parameter corners and the centre, %.17g: this is what distinguishes the
/// six faces of the first switch from each other.
void dump_corners(const char[] tag, SpherePatch sp) {
    foreach (t; [0.0, 0.5, 1.0]) {
        foreach (u; [0.0, 0.5, 1.0]) {
            auto p = sp(t, u);
            auto dist = p - sp.centre;
            writefln("%s corner r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g abs=%.17g",
                     tag, t, u, p.x, p.y, p.z, abs(dist));
        }
    }
}

void dump_sweep(const char[] tag, ParametricSurface ps) {
    foreach (r; [0.0, 0.2, 0.5, 0.75, 1.0]) {
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
    number R = 1.0;
    auto origin = Vector3(0.0, 0.0, 0.0);

    //--- group A: the east face, on the D unittest's data (D:140-171) ------------
    auto east = new SpherePatch(R, origin, "east");
    dump_state("sp.east", east);
    dump_sweep("sp.east", east);
    dump_area("sp.east", east, 10, 10);
    dump_area("sp.east", east, 3, 7);
    dump_area("sp.east", east, 1, 1);

    // The three points the D unittest names, at full precision.
    {
        void named(double t, double u) {
            auto p = east(t, u);
            writefln("sp.east.unittest r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g abs=%.17g",
                     t, u, p.x, p.y, p.z, abs(p));
        }
        named(0.5, 0.5);
        named(0.0, 1.0);
        named(1.0, 1.0);
    }
    // toString() through a base-class handle, which is what hierarchy callers use.
    writefln("sp.east str.as.base=%s", (cast(ParametricSurface)east).toString());

    auto dp = east.dup();
    dump_state("sp.dup", dp);
    dump_sweep("sp.dup", dp);
    dump_area("sp.dup", dp, 10, 10);

    auto cp = new SpherePatch(east);
    dump_state("sp.copy", cp);
    dump_sweep("sp.copy", cp);
    dump_area("sp.copy", cp, 10, 10);

    //--- group B: all six faces, full face ---------------------------------------
    auto west = new SpherePatch(R, origin, "west");
    dump_state("sp.west", west);
    dump_corners("sp.west", west);
    dump_sweep("sp.west", west);
    auto south = new SpherePatch(R, origin, "south");
    dump_state("sp.south", south);
    dump_corners("sp.south", south);
    dump_sweep("sp.south", south);
    auto north = new SpherePatch(R, origin, "north");
    dump_state("sp.north", north);
    dump_corners("sp.north", north);
    dump_sweep("sp.north", north);
    auto bottom = new SpherePatch(R, origin, "bottom");
    dump_state("sp.bottom", bottom);
    dump_corners("sp.bottom", bottom);
    auto top = new SpherePatch(R, origin, "top");
    dump_state("sp.top", top);
    dump_corners("sp.top", top);
    dump_area("sp.top", top, 10, 10);
    dump_area("sp.top", top, 3, 7);
    dump_area("sp.top", top, 1, 1);

    // The D unittest's top/south/north named points.
    {
        auto mid_t = top(0.5, 0.5);
        auto p5_top = top(1.0, 0.0);
        auto p6_top = top(1.0, 1.0);
        writefln("sp.top.unittest mid_t x=%.17g y=%.17g z=%.17g", mid_t.x, mid_t.y, mid_t.z);
        writefln("sp.top.unittest p5 x=%.17g y=%.17g z=%.17g", p5_top.x, p5_top.y, p5_top.z);
        writefln("sp.top.unittest p6 x=%.17g y=%.17g z=%.17g abs=%.17g",
                 p6_top.x, p6_top.y, p6_top.z, abs(p6_top));
        auto sp2 = new SpherePatch(R, origin, "south", "top");
        auto mid_s = sp2(0.5, 0.0);
        auto p5_south = sp2(1.0, 1.0);
        writefln("sp.southtop.unittest mid_s x=%.17g y=%.17g z=%.17g",
                 mid_s.x, mid_s.y, mid_s.z);
        writefln("sp.southtop.unittest p5 x=%.17g y=%.17g z=%.17g",
                 p5_south.x, p5_south.y, p5_south.z);
        auto np = new SpherePatch(R, origin, "north", "top-east");
        auto mid_n = np(0.0, 0.0);
        auto p6_north = np(1.0, 1.0);
        writefln("sp.northtopeast.unittest mid_n x=%.17g y=%.17g z=%.17g",
                 mid_n.x, mid_n.y, mid_n.z);
        writefln("sp.northtopeast.unittest p6 x=%.17g y=%.17g z=%.17g",
                 p6_north.x, p6_north.y, p6_north.z);
    }

    //--- group C: half- and quarter-face which_part, one clamp per branch ---------
    // east/west vary (y from r, z from s).
    foreach (part; ["south", "north", "bottom", "top"]) {
        auto h = new SpherePatch(R, origin, "east", part);
        dump_state("sp.eastpart." ~ part, h);
        dump_corners("sp.eastpart." ~ part, h);
    }
    // south/north vary (x from r, z from s).
    foreach (part; ["west", "east", "bottom", "top"]) {
        auto h = new SpherePatch(R, origin, "south", part);
        dump_state("sp.southpart." ~ part, h);
        dump_corners("sp.southpart." ~ part, h);
    }
    // bottom/top vary (x from r, y from s).
    foreach (part; ["west", "east", "south", "north"]) {
        auto h = new SpherePatch(R, origin, "top", part);
        dump_state("sp.toppardir." ~ part, h);
        dump_corners("sp.toppardir." ~ part, h);
    }
    // quarter faces: two clamps, on the two axes each branch touches.
    foreach (part; ["top-south", "top-north", "bottom-south", "bottom-north"]) {
        auto h = new SpherePatch(R, origin, "east", part);
        dump_state("sp.eastq." ~ part, h);
        dump_corners("sp.eastq." ~ part, h);
    }
    foreach (part; ["east-north", "west-north", "east-bottom", "west-south"]) {
        auto h = new SpherePatch(R, origin, "bottom", part);
        dump_state("sp.bottomq." ~ part, h);
        dump_corners("sp.bottomq." ~ part, h);
    }
    // Inconsistent pairs: the LAST matching if wins. Which one wins is observable.
    // The face is chosen so both needles in the pair are live for that face's branch.
    {
        void incons(string face, string part) {
            auto h = new SpherePatch(R, origin, face, part);
            dump_state("sp.incons." ~ face ~ "." ~ part, h);
            dump_corners("sp.incons." ~ face ~ "." ~ part, h);
        }
        incons("east", "south-north");
        incons("east", "north-south");
        incons("east", "bottom-top");
        incons("east", "top-bottom");
        incons("south", "west-east");
        incons("south", "east-west");
        incons("top", "west-east");
        incons("top", "east-west");
        incons("top", "south-north");
        incons("top", "north-south");
    }
    // canFind(which_part, needle) is a SUBSTRING test, unlike the array test.
    {
        auto h1 = new SpherePatch(R, origin, "east", "notsouth");
        dump_state("sp.substr.notsouth", h1);
        dump_corners("sp.substr.notsouth", h1);
        auto h2 = new SpherePatch(R, origin, "east", "top");
        dump_state("sp.substr.top", h2);
        dump_corners("sp.substr.top", h2);
        // an unrelated which_part changes nothing, but still selects a branch of the
        // second switch, so "north" on a "north" face is a no-op while "x" is too.
        auto h3 = new SpherePatch(R, origin, "east", "xyz");
        dump_state("sp.substr.xyz", h3);
        dump_corners("sp.substr.xyz", h3);
        auto h4 = new SpherePatch(R, origin, "north", "xyz");
        dump_state("sp.substr.north.xyz", h4);
        dump_corners("sp.substr.north.xyz", h4);
    }

    //--- group D: the throwing paths ---------------------------------------------
    // D:40-42 the constructor guard: exact array membership, Exception.
    foreach (bad; ["", "North", "NORTH", "nor", "northish", "east-west", "top2", "north "]) {
        dump_throw("sp.badname." ~ (bad.length == 0 ? "empty" : bad),
                   () { new SpherePatch(R, origin, bad, "top"); });
    }
    // The which_part is NOT validated at all: a nonsense part is stored.
    dump_throw("sp.badpart", () {
        auto h = new SpherePatch(R, origin, "east", "north-south-east-west-top-bottom");
        writefln("sp.badpart stored=%s", h.which_part);
    });

    // D:71 and D:102 — "Invalid face:" is reachable only by mutating the public
    // member, because the constructor already excluded every other name.
    {
        auto m = new SpherePatch(R, origin, "east");
        m.face_name = "bogus";
        writefln("sp.invalid.str=%s", m.toString());
        dump_throw("sp.invalid.firstswitch", () { auto p = m(0.5, 0.5); });
        dump_throw("sp.invalid.copy", () {
            auto c = new SpherePatch(m);
            writefln("sp.invalid.copy str=%s", c.toString());
            auto p = c(0.5, 0.5);
        });
        dump_throw("sp.invalid.dup", () { auto d = m.dup(); });
        // with which_part set, the FIRST switch still throws before the second runs,
        // so the only way to reach D:102 is a name the first switch knows ...
        auto m2 = new SpherePatch(R, origin, "east");
        m2.which_part = "top";
        m2.face_name = "bogus2";
        dump_throw("sp.invalid.withpart", () { auto p = m2(0.5, 0.5); });
        // ... and a name the second switch does not know but the first does is
        // impossible: both switches cover the same six names. Recorded as threw=none
        // only if that reasoning is wrong.
        auto m3 = new SpherePatch(R, origin, "top");
        m3.which_part = "top";
        dump_throw("sp.secondswitch.top", () { auto p = m3(0.5, 0.5); });
    }

    //--- group E: degenerate radii, centres and out-of-range parameters ----------
    {
        auto zr = new SpherePatch(0.0, Vector3(1.0, 2.0, 3.0), "east");
        dump_state("sp.zeroradius", zr);
        dump_sweep("sp.zeroradius", zr);
        dump_area("sp.zeroradius", zr, 10, 10);
        dump_area("sp.zeroradius", zr, 1, 1);

        auto nr = new SpherePatch(-2.5, origin, "north");
        dump_state("sp.negradius", nr);
        dump_sweep("sp.negradius", nr);
        dump_area("sp.negradius", nr, 3, 7);

        auto big = new SpherePatch(1.0e150, origin, "top", "east");
        dump_state("sp.bigradius", big);
        dump_corners("sp.bigradius", big);
        dump_area("sp.bigradius", big, 1, 1);

        auto nanc = new SpherePatch(R, Vector3(), "bottom");
        dump_state("sp.nancentre", nanc);
        dump_sweep("sp.nancentre", nanc);
        writefln("sp.nancentre str=%s", nanc.toString());

        // Out-of-range parameters: the sqrt argument of sphere_mapping turns negative
        // and D's sqrt prints nan (positive NaN).
        void oob(double t, double u) {
            auto p = east(t, u);
            writefln("sp.nanparam r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g",
                     t, u, p.x, p.y, p.z);
        }
        oob(-2.0, 0.5);
        oob(0.5, 2.0);
        oob(3.0, 3.0);
        oob(0.5, -0.5);
        oob(-0.5, -0.5);
    }

    //--- group F: offset centre, so the centre addend is observable --------------
    {
        auto oc = new SpherePatch(2.5, Vector3(-1.0, 3.0, 0.5), "west", "top-south");
        dump_state("sp.offset", oc);
        dump_corners("sp.offset", oc);
        dump_sweep("sp.offset", oc);
        dump_area("sp.offset", oc, 10, 10);
        dump_area("sp.offset", oc, 3, 7);
        dump_area("sp.offset", oc, 1, 1);
        auto ocd = oc.dup();
        dump_state("sp.offset.dup", ocd);
        auto occ = new SpherePatch(oc);
        dump_state("sp.offset.copy", occ);
        dump_corners("sp.offset.copy", occ);
    }

    //--- group G: the points geom/surface/spherepatch.d's unittest prints --------
    // (D:146 writeln("mid_e=", mid_e); D:153 writeln("mid_t=", mid_t);) — these are
    // Vector3 values, so writeln goes through Vector3.toString(), the %.6g form.
    {
        auto e2 = new SpherePatch(1.0, Vector3(0.0, 0.0, 0.0), "east");
        auto t2 = new SpherePatch(1.0, Vector3(0.0, 0.0, 0.0), "top");
        writefln("sp.utext mid_e=%s", (e2(0.5, 0.5)).toString());
        writefln("sp.utext mid_t=%s", (t2(0.5, 0.5)).toString());
        // and the two predicate rows the asserts use
        auto p6t = t2(1.0, 1.0);
        writefln("sp.utest abs_p6=%.17g", abs(p6t));
        writefln("sp.utest approx mid_e=%s", approxEqualVectors(e2(0.5, 0.5), Vector3(1.0, 0.0, 0.0)));
        writefln("sp.utest approx p5=%s",
                 approxEqualVectors(t2(1.0, 0.0), e2(0.0, 1.0)));
        writefln("sp.utest approx p6=%s",
                 approxEqualVectors(t2(1.0, 1.0), e2(1.0, 1.0)));
        writefln("sp.utest approx mid_t=%s", approxEqualVectors(t2(0.5, 0.5), Vector3(0.0, 0.0, 1.0)));
        auto s3 = new SpherePatch(1.0, Vector3(0.0, 0.0, 0.0), "south", "top");
        writefln("sp.utest approx mid_s=%s",
                 approxEqualVectors(s3(0.5, 0.0), Vector3(0.0, -1.0, 0.0)));
        writefln("sp.utest approx p5_s=%s", approxEqualVectors(s3(1.0, 1.0), e2(0.0, 1.0)));
        auto n4 = new SpherePatch(1.0, Vector3(0.0, 0.0, 0.0), "north", "top-east");
        writefln("sp.utest approx mid_n=%s",
                 approxEqualVectors(n4(0.0, 0.0), Vector3(0.0, 1.0, 0.0)));
        writefln("sp.utest approx p6_n=%s", approxEqualVectors(n4(1.0, 1.0), t2(1.0, 1.0)));
    }
}
