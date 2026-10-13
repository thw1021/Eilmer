// tests/test_geom_surface_parametricsurface_subrangedsurface.cpp — C++ mirror of
// probes/geom/surface/probe_subrangedsurface.d, replayed line for line against
// baselines/e587b235/probes/geom_surface_subrangedsurface.txt (273 D rows).
//
// geom/surface/subrangedsurface.d has NO `unittest` block (measured: the 45-line file
// contains no "unittest" keyword), so this replay is the whole gate for the module,
// in the same shape tests/test_geom_surface_parametricsurface.cpp uses for the base.
//
// Groups, and why each is load-bearing:
//   srs.dflt.*    default construction: the four members must be exactly D's defaults
//                 (0,1,0,1) and the adapter must then agree with the underlying surface
//                 at 6 parameters, row for row.
//   srs.{wide,neg,flip,degenR,degenS,tiny,big}.*
//                 the re-mapping (D:29-30) at the range endpoints, each point dumped
//                 three ways: through the adapter, the mapped (rr,ss) pair itself, and
//                 the underlying surface called directly at (rr,ss). 7 cases x 9 (r,s)
//                 x 3 rows = 189 rows. Reversed ranges (flip), degenerate ones
//                 (degenR/degenS) and extreme scales (1e-8, 1e8) are all legal here
//                 because D validates nothing.
//   srs.quad.*    36-point sweep with r and s each hitting 0.0, 1.0 and two points in
//                 every parametric quadrant (0.2/0.49 below 0.5, 0.51/0.8 above).
//   srs.dup.*     dup() through the base handle, plus the two-level chain, whose
//                 toString() must still nest two adapters (D:21 dups inside the ctor).
//   srs.area.*    the base class' area() at D's defaults (10,10), asymmetrically
//                 (3,7) and (7,3), and through the chain at (1,1).
//   srs.outofrange.*, srs.nan.*  unmapped-parameter edges: r=2 is legal, and NaN/inf
//                 propagate through the affine map to "-nan"/"-inf" under %.17g.
//
// The null-underlying path is deliberately NOT exercised: D's `new
// SubRangedSurface(null)` segfaults (measured exit 139, no throwable reaches
// catch(Throwable)), so there is no byte sequence to replay for it. See
// src/geom/surface/subrangedsurface.hpp.
//
// srs.nan.r is the one row that needed a language-level measurement rather than a
// transcription: D writes `dflt(0.0 / 0.0, 0.5)` and %.17g prints the result "-nan".
// g++ 11 spells the same expression the same way (measured: both the folded and the
// runtime 0.0/0.0 give bits=fff8000000000000, i.e. the sign-set default QNaN), but
// std::numeric_limits<double>::quiet_NaN() is bits=7ff8000000000000 and prints "nan",
// and quiet_NaN()*-1.0 does NOT become negative (x86 MULSD propagates the first NaN
// operand unchanged). So the port uses D's own expression, not a NaN generator.
//
// Regenerate (one line, so this comment stays a single // comment):
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface probes/geom/surface/probe_subrangedsurface.d -of=/tmp/probe_srs -od=/tmp/w2_surfB/obj_srs
//   then /tmp/probe_srs > baselines/e587b235/probes/geom_surface_subrangedsurface.txt
//
// Run: ./test_geom_surface_subrangedsurface <baseline.txt>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "eil/format.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/surface/parametricsurface.hpp"
#include "geom/surface/subrangedsurface.hpp"

namespace {

using eil::geom::ParametricSurface;
using eil::geom::SubRangedSurface;
using eil::geom::Vector3;
std::vector<std::string> g_lines;

void emit(const std::string& s) { g_lines.push_back(s); }

// D's ProbeSurface, verbatim (the same bilinear patch the base-class probe defines).
class ProbeSurface : public ParametricSurface {
  public:
    Vector3 p00, p10, p01, p11;

    ProbeSurface(Vector3 p00, Vector3 p10, Vector3 p01, Vector3 p11)
        : p00(p00), p10(p10), p01(p01), p11(p11) {}

    Vector3 operator()(double r, double s) const override {
        Vector3 a = p00 + (p10 - p00) * r;
        Vector3 b = p01 + (p11 - p01) * r;
        return a + (b - a) * s;
    }
    std::shared_ptr<ParametricSurface> dup() const override {
        return std::make_shared<ProbeSurface>(p00, p10, p01, p11);
    }
    std::string toString() const override {
        return eil::format("ProbeSurface(%s, %s, %s, %s)", p00.toString(), p10.toString(),
                           p01.toString(), p11.toString());
    }
};

void dump_pt(const std::string& key, const Vector3& p) {
    emit(eil::format("%s x=%.17g y=%.17g z=%.17g", key, eil::re(p.x), eil::re(p.y),
                     eil::re(p.z)));
}

void dump_members(const std::string& key, const SubRangedSurface& sr) {
    emit(eil::format("%s underlying=%s r0=%.17g r1=%.17g s0=%.17g s1=%.17g", key,
                     sr.underlying_surface ? sr.underlying_surface->toString()
                                           : std::string("null"),
                     sr.r0, sr.r1, sr.s0, sr.s1));
}

struct Case {
    const char* tag;
    double r0, r1, s0, s1;
};

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }

    auto skew = std::make_shared<ProbeSurface>(Vector3(0.5, -0.25, 0.1), Vector3(2.0, 0.75, -0.3),
                                               Vector3(-0.5, 1.5, 0.9),
                                               Vector3(1.25, 2.5, 0.05));

    // --- group 1 ---------------------------------------------------------------
    auto dflt = std::make_shared<SubRangedSurface>(skew);
    dump_members("srs.dflt.members", *dflt);
    emit("srs.dflt.str=" + dflt->toString());
    for (double rs : {0.0, 0.125, 0.25, 0.5, 0.75, 1.0}) {
        dump_pt(eil::format("srs.dflt.opCall r=%s", eil::to_string(rs)), (*dflt)(rs, 1.0 - rs));
        dump_pt(eil::format("srs.dflt.direct r=%s", eil::to_string(rs)), (*skew)(rs, 1.0 - rs));
    }

    // --- group 2 ---------------------------------------------------------------
    const Case cases[] = {{"wide", 2.0, 5.0, -1.0, 3.0},   {"neg", -3.0, -1.0, -2.0, -0.5},
                          {"flip", 1.0, 0.0, 1.0, 0.0},    {"degenR", 0.5, 0.5, 0.0, 1.0},
                          {"degenS", 0.0, 1.0, 0.25, 0.25}, {"tiny", 0.0, 1e-8, 0.0, 1.0},
                          {"big", 0.0, 1e8, 0.0, 1e8}};
    for (const Case& c : cases) {
        auto sr = std::make_shared<SubRangedSurface>(skew, c.r0, c.r1, c.s0, c.s1);
        emit(eil::format("srs.%s.str=%s", c.tag, sr->toString()));
        dump_members(std::string("srs.") + c.tag + ".members", *sr);
        for (double r : {0.0, 0.3, 1.0}) {
            for (double s : {0.0, 0.6, 1.0}) {
                double rr = c.r0 + (c.r1 - c.r0) * r;
                double ss = c.s0 + (c.s1 - c.s0) * s;
                dump_pt(eil::format("srs.%s.opCall r=%.17g s=%.17g", c.tag, r, s), (*sr)(r, s));
                emit(eil::format("srs.%s.mapped r=%.17g s=%.17g rr=%.17g ss=%.17g", c.tag, r, s,
                                 rr, ss));
                dump_pt(eil::format("srs.%s.direct r=%.17g s=%.17g", c.tag, rr, ss),
                        (*skew)(rr, ss));
            }
        }
    }

    // --- group 3 ---------------------------------------------------------------
    auto q = std::make_shared<SubRangedSurface>(skew, 1.0, 3.0, 2.0, 6.0);
    for (double r : {0.0, 0.2, 0.49, 0.51, 0.8, 1.0}) {
        for (double s : {0.0, 0.2, 0.49, 0.51, 0.8, 1.0}) {
            dump_pt(eil::format("srs.quad.opCall r=%.17g s=%.17g", r, s), (*q)(r, s));
        }
    }

    // --- group 4 ---------------------------------------------------------------
    auto dd = std::dynamic_pointer_cast<SubRangedSurface>(q->dup());
    emit("srs.dup.str=" + dd->toString());
    dump_members("srs.dup.members", *dd);
    dump_pt("srs.dup.opCallA", (*dd)(0.25, 0.75));
    dump_pt("srs.dup.opCallB", (*dd)(1.0, 0.0));
    auto chain = std::make_shared<SubRangedSurface>(q, 0.0, 0.5, 0.5, 1.0);
    emit("srs.chain.str=" + chain->toString());
    auto cd = std::dynamic_pointer_cast<SubRangedSurface>(chain->dup());
    emit("srs.chain.dup.str=" + cd->toString());
    for (double rs : {0.0, 0.5, 1.0}) {
        dump_pt(eil::format("srs.chain.opCall r=%.17g", rs), (*chain)(rs, rs));
        dump_pt(eil::format("srs.chain.dup.opCall r=%.17g", rs), (*cd)(rs, rs));
    }

    // --- group 5 ---------------------------------------------------------------
    Vector3 a1 = dflt->area();
    emit(eil::format("srs.area.default x=%.17g y=%.17g z=%.17g", eil::re(a1.x), eil::re(a1.y),
                     eil::re(a1.z)));
    Vector3 a2 = q->area(3, 7);
    emit(eil::format("srs.area.r3s7 x=%.17g y=%.17g z=%.17g", eil::re(a2.x), eil::re(a2.y),
                     eil::re(a2.z)));
    Vector3 a3 = dflt->area(7, 3);
    emit(eil::format("srs.area.r7s3 x=%.17g y=%.17g z=%.17g", eil::re(a3.x), eil::re(a3.y),
                     eil::re(a3.z)));
    Vector3 a4 = chain->area(1, 1);
    emit(eil::format("srs.area.chain11 x=%.17g y=%.17g z=%.17g", eil::re(a4.x), eil::re(a4.y),
                     eil::re(a4.z)));

    // --- group 6 ---------------------------------------------------------------
    dump_pt("srs.outofrange.r2s2", (*dflt)(2.0, 2.0));
    dump_pt("srs.outofrange.rneg", (*dflt)(-1.0, -0.5));
    dump_pt("srs.nan.r", (*dflt)(0.0 / 0.0, 0.5));
    dump_pt("srs.nan.s", (*dflt)(0.5, INFINITY));

    // Replay.
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
            std::printf("line %zu\n  D:   |%s|\n  C++: |%s|\n", i + 1, e.c_str(), g.c_str());
            ++shown;
        }
        ++diffs;
    }
    // Every one of the 273 rows is a byte comparison, so there is no residual to
    // attribute: either a row matches or it does not. Reported that way below.
    std::printf("%s: geom_surface_subrangedsurface, probe replay %zu D rows, %zu C++ rows, "
                "%d differ; %zu of %zu rows byte-identical\n",
                diffs == 0 ? "PASS" : "FAIL", expected.size(), g_lines.size(), diffs,
                std::min(expected.size(), g_lines.size()) - static_cast<size_t>(diffs),
                std::min(expected.size(), g_lines.size()));
    return diffs == 0 ? 0 : 1;
}
