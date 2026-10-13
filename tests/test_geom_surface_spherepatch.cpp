// tests/test_geom_surface_spherepatch.cpp — C++ mirror of
// probes/geom/surface/probe_spherepatch.d, replayed line for line against
// baselines/e587b235/probes/geom_surface_spherepatch.txt (933 D rows), plus the
// `unittest` block of geom/surface/spherepatch.d (D:137-171) ported assertion for
// assertion, in source order.
//
// Row groups and what each one can catch:
//   sp.east.* / sp.west.* / sp.south.* / sp.north.* / sp.bottom.* / sp.top.*
//                        every face of the first switch (D:64-72). Each face is dumped
//                        as toString()/classString()/radius/centre/face/part, its four
//                        parameter corners plus the mid of each axis (which is what
//                        distinguishes "east varies y,z" from "east varies x,z"), and a
//                        5x5 (r,s) sweep containing 0.0 and 1.0 exactly and a point in
//                        each parametric quadrant.
//   sp.east.unittest.*   the three named points of D:143-145 at %.17g.
//   sp.dup.* / sp.copy.* D:55-58 re-enters the validating constructor, D:47-53 does not.
//   sp.eastpart.* / sp.southpart.* / sp.toppardir.*
//                        one which_part clamp at a time for each of the three face
//                        groups of D:79-100 — the axis a clamp acts on differs per group,
//                        so "top" moves z on an east face and y on a bottom face.
//   sp.eastq.* / sp.bottomq.*
//                        quarter faces: two clamps applied in D's if order.
//   sp.incons.<face>.<pair>
//                        inconsistent pairs ("south-north", "north-south", ...). D:75-77
//                        leaves consistency to the user, so the winner is the LAST
//                        matching if; ten orderings measure which one that is.
//   sp.substr.*          canFind(haystack, needle) is a SUBSTRING test, unlike the
//                        constructor's array test: "notsouth" clamps y, and a part string
//                        with no face name ("xyz") leaves the full-face values alone but
//                        still selects a branch of the second switch.
//   sp.badname.*         D:40-42, eight rejected names, each row naming WHICH type
//                        escaped (all eight are Exception, the sibling of Error in this
//                        port) and the message text. "north " (trailing space) and
//                        "northish" show the array test is exact.
//   sp.badpart           the which_part is never validated: it is stored verbatim and
//                        construction does not throw.
//   sp.invalid.*         the two "Invalid face:" guards. Both are reachable only by
//                        mutating the public face_name (D:26): firstswitch throws from
//                        D:71, copy shows the copy constructor keeping the bad name,
//                        dup shows D:55-58 throwing "Invalid face name: bogus" instead,
//                        withpart shows the FIRST switch winning when which_part is set,
//                        and secondswitch.top measures D:101-102 as unreachable
//                        (threw=none) for every name the constructor admits.
//   sp.zeroradius.*      radius 0: every one of the 25 sweep points is exactly the
//                        centre, and area() sums to 0. The module has no division by the
//                        radius (the only `/` in spherepatch.d is the literal /3.0), so
//                        there is no zero-division path to reproduce.
//   sp.negradius.* / sp.bigradius.*  a negative radius flips the sphere and 1e150 keeps
//                        the products finite.
//   sp.nancentre.*       Vector3() is Vector3.init, i.e. NaN per field, so all 25 sweep
//                        rows and toString() print nan.
//   sp.nanparam.*        out-of-range (r,s): sphere_mapping's radicand goes negative and
//                        D's sqrt prints positive nan. Three of the ten components are
//                        still finite, which is what pins which axis is which.
//   sp.offset.*          a non-unit radius and an off-origin centre, so D:106's
//                        per-component multiply-then-add is observable in all three axes.
//   sp.utext.* / sp.utest.*  the two writeln rows of the D unittest (D:146, D:153, the
//                        %.6g Vector3 form) and the nine predicate values the asserts use.
//
// Byte-for-byte comparison, no tolerance band.
//
// Regenerate (block comment so the shell line-splices do not trip -Wcomment):
/*
  ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface \
    probes/geom/surface/probe_spherepatch.d -of=/tmp/probe_sp -od/tmp/probe_obj_w2_surf2
  /tmp/probe_sp > baselines/e587b235/probes/geom_surface_spherepatch.txt
*/
//
// Run: ./test_geom_surface_spherepatch <baseline.txt>
#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/surface/parametricsurface.hpp"
#include "geom/surface/spherepatch.hpp"

namespace {

using eil::geom::ParametricSurface;
using eil::geom::SpherePatch;
using eil::geom::Vector3;
using eil::number;
using Ptr = std::shared_ptr<SpherePatch>;

std::vector<std::string> g_lines;
int g_checks = 0;
int g_failures = 0;

void emit(const std::string& s) { g_lines.push_back(s); }
template <typename... Args> void emitf(const std::string& fmt, const Args&... args) {
    emit(eil::format(fmt, args...));
}
void check(bool cond, const std::string& what) {
    ++g_checks;
    if (!cond) {
        ++g_failures;
        std::cout << "ASSERT FAILED: " << what << "\n";
    }
}

void dump_state(const std::string& tag, const SpherePatch& sp) {
    emitf("%s str=%s", tag, sp.toString());
    emitf("%s classname=%s", tag, sp.classString());
    emitf("%s radius=%.17g", tag, eil::re(sp.radius));
    emitf("%s centre x=%.17g y=%.17g z=%.17g", tag, eil::re(sp.centre.x), eil::re(sp.centre.y),
          eil::re(sp.centre.z));
    emitf("%s face=%s part=%s", tag, sp.face_name, sp.which_part);
}

void dump_corners(const std::string& tag, const SpherePatch& sp) {
    for (double t : {0.0, 0.5, 1.0}) {
        for (double u : {0.0, 0.5, 1.0}) {
            auto p = sp(t, u);
            auto dist = p - sp.centre;
            emitf("%s corner r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g abs=%.17g", tag, t, u,
                  eil::re(p.x), eil::re(p.y), eil::re(p.z), eil::re(eil::geom::abs(dist)));
        }
    }
}

void dump_sweep(const std::string& tag, const ParametricSurface& ps) {
    for (double r : {0.0, 0.2, 0.5, 0.75, 1.0}) {
        for (double s : {0.0, 0.25, 0.5, 0.75, 1.0}) {
            auto p = ps(r, s);
            emitf("%s opCall r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g", tag, r, s, eil::re(p.x),
                  eil::re(p.y), eil::re(p.z));
        }
    }
}

void dump_area(const std::string& tag, ParametricSurface& ps, int nr, int ns) {
    auto a = ps.area(nr, ns);
    emitf("%s area nr=%d ns=%d x=%.17g y=%.17g z=%.17g", tag, nr, ns, eil::re(a.x), eil::re(a.y),
          eil::re(a.z));
}

void dump_throw(const std::string& tag, const std::function<void()>& dg) {
    try {
        dg();
        emitf("%s threw=none", tag);
    } catch (const eil::ErrorBase& e) {
        emitf("%s threw=Error msg=%s", tag, e.msg());
    } catch (const eil::ExceptionBase& e) {
        emitf("%s threw=Exception msg=%s", tag, e.msg());
    }
}

Ptr make_patch(number radius, const Vector3& centre, const std::string& face,
               const std::string& part = std::string()) {
    return std::make_shared<SpherePatch>(radius, centre, face, part);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }

    const number R = 1.0;
    const Vector3 origin(0.0, 0.0, 0.0);

    //--- group A: the east face, on the D unittest's data ----------------------
    auto east = make_patch(R, origin, "east");
    dump_state("sp.east", *east);
    dump_sweep("sp.east", *east);
    dump_area("sp.east", *east, 10, 10);
    dump_area("sp.east", *east, 3, 7);
    dump_area("sp.east", *east, 1, 1);
    for (const std::pair<double, double>& pr :
         {std::make_pair(0.5, 0.5), std::make_pair(0.0, 1.0), std::make_pair(1.0, 1.0)}) {
        auto p = (*east)(pr.first, pr.second);
        emitf("sp.east.unittest r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g abs=%.17g", pr.first,
              pr.second, eil::re(p.x), eil::re(p.y), eil::re(p.z), eil::re(eil::geom::abs(p)));
    }
    emitf("sp.east str.as.base=%s",
          std::static_pointer_cast<ParametricSurface>(east)->toString());

    {
        auto dp = std::dynamic_pointer_cast<SpherePatch>(east->dup());
        dump_state("sp.dup", *dp);
        dump_sweep("sp.dup", *dp);
        dump_area("sp.dup", *dp, 10, 10);
    }
    {
        auto cp = std::make_shared<SpherePatch>(*east);
        dump_state("sp.copy", *cp);
        dump_sweep("sp.copy", *cp);
        dump_area("sp.copy", *cp, 10, 10);
    }

    //--- group B: all six faces, full face -------------------------------------
    {
        auto west = make_patch(R, origin, "west");
        dump_state("sp.west", *west);
        dump_corners("sp.west", *west);
        dump_sweep("sp.west", *west);
        auto south = make_patch(R, origin, "south");
        dump_state("sp.south", *south);
        dump_corners("sp.south", *south);
        dump_sweep("sp.south", *south);
        auto north = make_patch(R, origin, "north");
        dump_state("sp.north", *north);
        dump_corners("sp.north", *north);
        dump_sweep("sp.north", *north);
        auto bottom = make_patch(R, origin, "bottom");
        dump_state("sp.bottom", *bottom);
        dump_corners("sp.bottom", *bottom);
        auto top = make_patch(R, origin, "top");
        dump_state("sp.top", *top);
        dump_corners("sp.top", *top);
        dump_area("sp.top", *top, 10, 10);
        dump_area("sp.top", *top, 3, 7);
        dump_area("sp.top", *top, 1, 1);
    }
    {
        auto top = make_patch(R, origin, "top");
        auto mid_t = (*top)(0.5, 0.5);
        auto p5_top = (*top)(1.0, 0.0);
        auto p6_top = (*top)(1.0, 1.0);
        emitf("sp.top.unittest mid_t x=%.17g y=%.17g z=%.17g", eil::re(mid_t.x), eil::re(mid_t.y),
              eil::re(mid_t.z));
        emitf("sp.top.unittest p5 x=%.17g y=%.17g z=%.17g", eil::re(p5_top.x), eil::re(p5_top.y),
              eil::re(p5_top.z));
        emitf("sp.top.unittest p6 x=%.17g y=%.17g z=%.17g abs=%.17g", eil::re(p6_top.x),
              eil::re(p6_top.y), eil::re(p6_top.z), eil::re(eil::geom::abs(p6_top)));
        auto sp2 = make_patch(R, origin, "south", "top");
        auto mid_s = (*sp2)(0.5, 0.0);
        auto p5_south = (*sp2)(1.0, 1.0);
        emitf("sp.southtop.unittest mid_s x=%.17g y=%.17g z=%.17g", eil::re(mid_s.x),
              eil::re(mid_s.y), eil::re(mid_s.z));
        emitf("sp.southtop.unittest p5 x=%.17g y=%.17g z=%.17g", eil::re(p5_south.x),
              eil::re(p5_south.y), eil::re(p5_south.z));
        auto np = make_patch(R, origin, "north", "top-east");
        auto mid_n = (*np)(0.0, 0.0);
        auto p6_north = (*np)(1.0, 1.0);
        emitf("sp.northtopeast.unittest mid_n x=%.17g y=%.17g z=%.17g", eil::re(mid_n.x),
              eil::re(mid_n.y), eil::re(mid_n.z));
        emitf("sp.northtopeast.unittest p6 x=%.17g y=%.17g z=%.17g", eil::re(p6_north.x),
              eil::re(p6_north.y), eil::re(p6_north.z));
    }

    //--- group C: half- and quarter-face which_part ----------------------------
    for (const std::string& part : {"south", "north", "bottom", "top"}) {
        auto h = make_patch(R, origin, "east", part);
        dump_state("sp.eastpart." + part, *h);
        dump_corners("sp.eastpart." + part, *h);
    }
    for (const std::string& part : {"west", "east", "bottom", "top"}) {
        auto h = make_patch(R, origin, "south", part);
        dump_state("sp.southpart." + part, *h);
        dump_corners("sp.southpart." + part, *h);
    }
    for (const std::string& part : {"west", "east", "south", "north"}) {
        auto h = make_patch(R, origin, "top", part);
        dump_state("sp.toppardir." + part, *h);
        dump_corners("sp.toppardir." + part, *h);
    }
    for (const std::string& part : {"top-south", "top-north", "bottom-south", "bottom-north"}) {
        auto h = make_patch(R, origin, "east", part);
        dump_state("sp.eastq." + part, *h);
        dump_corners("sp.eastq." + part, *h);
    }
    for (const std::string& part : {"east-north", "west-north", "east-bottom", "west-south"}) {
        auto h = make_patch(R, origin, "bottom", part);
        dump_state("sp.bottomq." + part, *h);
        dump_corners("sp.bottomq." + part, *h);
    }
    for (const std::pair<const char*, const char*>& pr :
         {std::make_pair("east", "south-north"), std::make_pair("east", "north-south"),
          std::make_pair("east", "bottom-top"), std::make_pair("east", "top-bottom"),
          std::make_pair("south", "west-east"), std::make_pair("south", "east-west"),
          std::make_pair("top", "west-east"), std::make_pair("top", "east-west"),
          std::make_pair("top", "south-north"), std::make_pair("top", "north-south")}) {
        auto h = make_patch(R, origin, pr.first, pr.second);
        std::string tag = std::string("sp.incons.") + pr.first + "." + pr.second;
        dump_state(tag, *h);
        dump_corners(tag, *h);
    }
    {
        auto h1 = make_patch(R, origin, "east", "notsouth");
        dump_state("sp.substr.notsouth", *h1);
        dump_corners("sp.substr.notsouth", *h1);
        auto h2 = make_patch(R, origin, "east", "top");
        dump_state("sp.substr.top", *h2);
        dump_corners("sp.substr.top", *h2);
        auto h3 = make_patch(R, origin, "east", "xyz");
        dump_state("sp.substr.xyz", *h3);
        dump_corners("sp.substr.xyz", *h3);
        auto h4 = make_patch(R, origin, "north", "xyz");
        dump_state("sp.substr.north.xyz", *h4);
        dump_corners("sp.substr.north.xyz", *h4);
    }

    //--- group D: the throwing paths -------------------------------------------
    for (const std::string& bad :
         {"", "North", "NORTH", "nor", "northish", "east-west", "top2", "north "}) {
        std::string tag = "sp.badname." + (bad.empty() ? std::string("empty") : bad);
        dump_throw(tag, [&]() { make_patch(R, origin, bad, "top"); });
    }
    dump_throw("sp.badpart", [&]() {
        auto h = make_patch(R, origin, "east", "north-south-east-west-top-bottom");
        emitf("sp.badpart stored=%s", h->which_part);
    });
    {
        auto m = make_patch(R, origin, "east");
        m->face_name = "bogus";
        emitf("sp.invalid.str=%s", m->toString());
        dump_throw("sp.invalid.firstswitch", [&]() { auto p = (*m)(0.5, 0.5); (void)p; });
        dump_throw("sp.invalid.copy", [&]() {
            auto c = std::make_shared<SpherePatch>(*m);
            emitf("sp.invalid.copy str=%s", c->toString());
            auto p = (*c)(0.5, 0.5);
            (void)p;
        });
        dump_throw("sp.invalid.dup", [&]() { auto d = m->dup(); (void)d; });
        auto m2 = make_patch(R, origin, "east");
        m2->which_part = "top";
        m2->face_name = "bogus2";
        dump_throw("sp.invalid.withpart", [&]() { auto p = (*m2)(0.5, 0.5); (void)p; });
        auto m3 = make_patch(R, origin, "top");
        m3->which_part = "top";
        dump_throw("sp.secondswitch.top", [&]() { auto p = (*m3)(0.5, 0.5); (void)p; });
    }

    //--- group E: degenerate radii, centres, out-of-range parameters ----------
    {
        auto zr = make_patch(0.0, Vector3(1.0, 2.0, 3.0), "east");
        dump_state("sp.zeroradius", *zr);
        dump_sweep("sp.zeroradius", *zr);
        dump_area("sp.zeroradius", *zr, 10, 10);
        dump_area("sp.zeroradius", *zr, 1, 1);

        auto nr = make_patch(-2.5, origin, "north");
        dump_state("sp.negradius", *nr);
        dump_sweep("sp.negradius", *nr);
        dump_area("sp.negradius", *nr, 3, 7);

        auto big = make_patch(1.0e150, origin, "top", "east");
        dump_state("sp.bigradius", *big);
        dump_corners("sp.bigradius", *big);
        dump_area("sp.bigradius", *big, 1, 1);

        auto nanc = make_patch(R, Vector3(), "bottom");
        dump_state("sp.nancentre", *nanc);
        dump_sweep("sp.nancentre", *nanc);
        emitf("sp.nancentre str=%s", nanc->toString());

        for (const std::pair<double, double>& pr :
             {std::make_pair(-2.0, 0.5), std::make_pair(0.5, 2.0), std::make_pair(3.0, 3.0),
              std::make_pair(0.5, -0.5), std::make_pair(-0.5, -0.5)}) {
            auto p = (*east)(pr.first, pr.second);
            emitf("sp.nanparam r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g", pr.first, pr.second,
                  eil::re(p.x), eil::re(p.y), eil::re(p.z));
        }
    }

    //--- group F: offset centre -----------------------------------------------
    {
        auto oc = make_patch(2.5, Vector3(-1.0, 3.0, 0.5), "west", "top-south");
        dump_state("sp.offset", *oc);
        dump_corners("sp.offset", *oc);
        dump_sweep("sp.offset", *oc);
        dump_area("sp.offset", *oc, 10, 10);
        dump_area("sp.offset", *oc, 3, 7);
        dump_area("sp.offset", *oc, 1, 1);
        auto ocd = std::dynamic_pointer_cast<SpherePatch>(oc->dup());
        dump_state("sp.offset.dup", *ocd);
        auto occ = std::make_shared<SpherePatch>(*oc);
        dump_state("sp.offset.copy", *occ);
        dump_corners("sp.offset.copy", *occ);
    }

    //--- group G: the D unittest's printed values and predicates --------------
    {
        auto e2 = make_patch(1.0, Vector3(0.0, 0.0, 0.0), "east");
        auto t2 = make_patch(1.0, Vector3(0.0, 0.0, 0.0), "top");
        emitf("sp.utext mid_e=%s", (*e2)(0.5, 0.5).toString());
        emitf("sp.utext mid_t=%s", (*t2)(0.5, 0.5).toString());
        auto p6t = (*t2)(1.0, 1.0);
        emitf("sp.utest abs_p6=%.17g", eil::re(eil::geom::abs(p6t)));
        emitf("sp.utest approx mid_e=%s",
              eil::geom::approxEqualVectors((*e2)(0.5, 0.5), Vector3(1.0, 0.0, 0.0)));
        emitf("sp.utest approx p5=%s",
              eil::geom::approxEqualVectors((*t2)(1.0, 0.0), (*e2)(0.0, 1.0)));
        emitf("sp.utest approx p6=%s",
              eil::geom::approxEqualVectors((*t2)(1.0, 1.0), (*e2)(1.0, 1.0)));
        emitf("sp.utest approx mid_t=%s",
              eil::geom::approxEqualVectors((*t2)(0.5, 0.5), Vector3(0.0, 0.0, 1.0)));
        auto s3 = make_patch(1.0, Vector3(0.0, 0.0, 0.0), "south", "top");
        emitf("sp.utest approx mid_s=%s",
              eil::geom::approxEqualVectors((*s3)(0.5, 0.0), Vector3(0.0, -1.0, 0.0)));
        emitf("sp.utest approx p5_s=%s",
              eil::geom::approxEqualVectors((*s3)(1.0, 1.0), (*e2)(0.0, 1.0)));
        auto n4 = make_patch(1.0, Vector3(0.0, 0.0, 0.0), "north", "top-east");
        emitf("sp.utest approx mid_n=%s",
              eil::geom::approxEqualVectors((*n4)(0.0, 0.0), Vector3(0.0, 1.0, 0.0)));
        emitf("sp.utest approx p6_n=%s",
              eil::geom::approxEqualVectors((*n4)(1.0, 1.0), (*t2)(1.0, 1.0)));
    }

    //--- part A: the `unittest` block of geom/surface/spherepatch.d ------------
    // D:137-171, nine asserts, ported in source order.
    {
        number Rn = 1.0;
        number zero = 0.0;
        auto ep = make_patch(Rn, Vector3(0.0, 0.0, 0.0), "east");
        auto mid_e = (*ep)(0.5, 0.5);
        auto p5_east = (*ep)(0.0, 1.0);
        auto p6_east = (*ep)(1.0, 1.0);
        // D:146 writeln("mid_e=", mid_e)
        check(mid_e.toString() == "Vector3(1, 0, 0)", "spherepatch.d:146 mid_e text");
        check(eil::geom::approxEqualVectors(mid_e, Vector3(Rn, zero, zero)),
              "spherepatch.d:148 approxEqualVectors(mid_e, Vector3(R, zero, zero))");
        auto tp = make_patch(Rn, Vector3(0.0, 0.0, 0.0), "top");
        auto mid_t = (*tp)(0.5, 0.5);
        auto p5_top = (*tp)(1.0, 0.0);
        auto p6_top = (*tp)(1.0, 1.0);
        // D:153 writeln("mid_t=", mid_t)
        check(mid_t.toString() == "Vector3(0, 0, 1)", "spherepatch.d:153 mid_t text");
        check(eil::geom::approxEqualVectors(mid_t, Vector3(zero, zero, Rn)),
              "spherepatch.d:155 approxEqualVectors(mid_t, Vector3(zero, zero, R))");
        check(eil::geom::approxEqualVectors(p5_top, p5_east),
              "spherepatch.d:156 approxEqualVectors(p5_top, p5_east)");
        check(eil::geom::approxEqualVectors(p6_top, p6_east),
              "spherepatch.d:157 approxEqualVectors(p6_top, p6_east)");
        check(eil::isClose(eil::re(eil::geom::abs(p6_top)), Rn),
              "spherepatch.d:158 isClose(abs(p6_top), R)");
        auto sp = make_patch(Rn, Vector3(0.0, 0.0, 0.0), "south", "top");
        auto mid_s = (*sp)(0.5, 0.0);
        auto p5_south = (*sp)(1.0, 1.0);
        check(eil::geom::approxEqualVectors(mid_s, Vector3(zero, -Rn, zero)),
              "spherepatch.d:163 approxEqualVectors(mid_s, Vector3(zero, -R, zero))");
        check(eil::geom::approxEqualVectors(p5_south, p5_east),
              "spherepatch.d:164 approxEqualVectors(p5_south, p5_east)");
        auto np = make_patch(Rn, Vector3(0.0, 0.0, 0.0), "north", "top-east");
        auto mid_n = (*np)(0.0, 0.0);
        auto p6_north = (*np)(1.0, 1.0);
        check(eil::geom::approxEqualVectors(mid_n, Vector3(zero, Rn, zero)),
              "spherepatch.d:169 approxEqualVectors(mid_n, Vector3(zero, R, zero))");
        check(eil::geom::approxEqualVectors(p6_north, p6_top),
              "spherepatch.d:170 approxEqualVectors(p6_north, p6_top)");
    }

    //--- replay ---------------------------------------------------------------
    std::ifstream in(argv[1]);
    if (!in) {
        std::fprintf(stderr, "cannot open baseline %s\n", argv[1]);
        return 2;
    }
    std::vector<std::string> expected;
    for (std::string l; std::getline(in, l);) expected.push_back(l);

    int diffs = 0, shown = 0;
    for (size_t i = 0; i < expected.size() || i < g_lines.size(); ++i) {
        const std::string e = i < expected.size() ? expected[i] : "<none>";
        const std::string g = i < g_lines.size() ? g_lines[i] : "<none>";
        if (e == g) continue;
        if (shown < 12) {
            std::printf("line %zu\n  D:   |%.400s|\n  C++: |%.400s|\n", i + 1, e.c_str(),
                        g.c_str());
            ++shown;
        }
        ++diffs;
    }
    const size_t common = std::min(expected.size(), g_lines.size());
    std::printf(
        "%s: geom_surface_spherepatch, %d/%d unittest checks hold (the 9 asserts at "
        "spherepatch.d:148,155,156,157,158,163,164,169,170 plus the 2 writeln texts at "
        "D:146,153); "
        "probe replay %zu D lines, %zu C++ lines, %d differ; %zu of %zu rows byte-identical, "
        "0 rows accepted through a tolerance band\n",
        (diffs == 0 && g_failures == 0) ? "PASS" : "FAIL", g_checks - g_failures, g_checks,
        expected.size(), g_lines.size(), diffs, common - static_cast<size_t>(diffs), common);
    return (diffs == 0 && g_failures == 0) ? 0 : 1;
}
