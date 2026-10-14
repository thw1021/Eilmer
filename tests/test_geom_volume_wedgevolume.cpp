// tests/test_geom_volume_wedgevolume.cpp — C++ mirror of
// probes/geom/volume/probe_wedgevolume.d, replayed line for line against
// baselines/e587b235/probes/geom_volume_wedgevolume.txt (4594 D rows).
//
// WedgeVolume (D:17-81) sweeps a bottom surface about the x-axis. Every row of a
// Vector3 here carries its %.17g value AND its %016x bit pattern, because signed zeros
// and the sign of a NaN are part of this module's contract (atan2 quadrants feed
// straight into r*cos/r*sin, so 0*cos(theta1) can be -0.0).
//
// Groups, in probe order:
//  * wv.main — toString, the stored face, dtheta at %.17g + bits, the eight corners,
//    and the four face corners p[0..3] are copied from (D:27-30).
//  * 2160 rows of opCall + the nine inherited finite differences over the six
//    parameter values that pin the base's `>` / `<` boundary branches.
//  * dup() (re-derives p[]) and the copy constructor (copies p[]).
//  * wv.d0 — the zero-dtheta wedge. MEASURED: it is not an identity. 3 of the 4 swept
//    corners differ from their unswept partners (wv.d0.p[1+4].equals.p[1]=false, with
//    p[1].y=-0.75 against p[5].y=-0.74999999999999989, 1 ULP), and
//    wv.d0.identityAtT0=false over the 16-point (r,s) grid; then a second full 2160-row
//    sweep. A port that short-circuits theta==0 would print prettier corners and fail
//    every one of those rows.
//  * wv.th[0..7] — dtheta battery: -0.6, 0.1, PI/4 (computed in eil::real and rounded
//    once on store, wave-02-brief section 5), PI, 2*PI, 1e-20, number.init (NaN, bits
//    7ff8000000000000) and 0.0-0.0.
//  * wv.negZero / wv.nanTheta — dtheta reassigned through the public field after
//    construction. Measured: the corners do NOT follow (wv.nanTheta.p[4..7] still carry
//    the 0.6 sweep), which is only observable because D computes p[] in the
//    constructor.
//  * wv.axis[0..6] — the signed-zero / on-axis battery through a constant surface:
//    (y,z) = (0,0), (-0,0), (0,-0), (-0,-0), (0,-1), (-1,0), (-1,-0). These are the
//    atan2 quadrant cases (T-910), and the outputs are signed zeros again.
//  * wv.nullface — toString() with the face handle nulled prints "null" while the
//    corners keep their values.
//  * the D unittest (D:83-118), including D:94's writeln text.
//
// Throw paths: this module has none — grep over wedgevolume.d's 118 lines finds no
// `throw`, no `enforce`, no guard, so no throwable type is claimed here. A null
// constructor argument reaches `face0123.dup()` (D:25), a virtual call through a null
// handle, for which ldc2 1.40 gives an uncatchable segfault (measured for
// SubRangedVolume at src/geom/volume/subrangedvolume.hpp:51-56); it is not replayed
// because it would kill this process.
//
// (A) D unittest (D:83-118), assertion by assertion in source order: 1 compiled
//     assertion — `assert(approxEqualVectors(d, Vector3(0.1, 0.2*cos(0.05),
//     0.2*sin(0.05))))` at wedgevolume.d:96, preceded by the four corner assignments
//     (D:86-90), the CoonsPatch face + volume + opCall (D:91-93) and the writeln
//     (D:94). Measured: d = (0.10000000000000001, 0.19975005207899321,
//     0.0099958338541356641) against the assert's right-hand side
//     (…, 0.19975005207899327, 0.0099958338541356676), i.e. the swept value is 1 ULP
//     BELOW the closed form in y and z and the assertion still holds because
//     approxEqualVectors' default maxRelDiff is 1.0e-2 — the residual row prints
//     dy=-5.5511151231257827e-17, dz=-3.4694469519536142e-18. The complex-step block
//     (D:99-117, assert at D:116) is under `version (complex_numbers)`, which is not
//     enabled (backlog T-904); it is kept below under EIL_COMPLEX_NUMBERS and is not
//     compiled, and the corpus row wv.ut.cmplx.notCompiled=true records which side of
//     the variant the reference run was on.
//
// Regenerate (full command in the probe header):
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/volume
//     probes/geom/volume/probe_wedgevolume.d -of=/tmp/probe_wv -od/tmp/probe_obj_vol2
//   then: /tmp/probe_wv > baselines/e587b235/probes/geom_volume_wedgevolume.txt
//
// Run: ./test_geom_volume_wedgevolume <baseline.txt>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/math.hpp"
#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/surface/coonspatch.hpp"
#include "geom/surface/parametricsurface.hpp"
#include "geom/volume/parametricvolume.hpp"
#include "geom/volume/wedgevolume.hpp"

namespace {

using eil::geom::CoonsPatch;
using eil::geom::ParametricSurface;
using eil::geom::Vector3;
using eil::geom::WedgeVolume;

std::vector<std::string> g_lines;
int g_ut_checks = 0, g_ut_fails = 0; // D `unittest` block assertions

void emit(const std::string& s) { g_lines.push_back(s); }
void emit_bool(const std::string& tag, bool v) { emit(tag + "=" + (v ? "true" : "false")); }

// The raw IEEE-754 pattern, so equality is bit equality and +0.0 / -0.0 and the two
// NaN signs cannot be confused (wave-02-brief section 5, T-912). Same idiom as
// probes/geom/gpath/probe_mp_rot_chain.d:39.
std::string bits(eil::number x) {
    const double v = eil::re(x);
    uint64_t u = 0;
    std::memcpy(&u, &v, sizeof(u));
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(u));
    return std::string(buf);
}

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

// Ignores (r,s) so a signed zero reaches sweep_through_arc without being erased by the
// bilinear interpolation (probe's ConstSurface).
class ConstSurface : public ParametricSurface {
  public:
    Vector3 q;
    explicit ConstSurface(Vector3 q) : q(q) {}
    Vector3 operator()(double, double) const override { return q; }
    std::shared_ptr<ParametricSurface> dup() const override {
        return std::make_shared<ConstSurface>(q);
    }
    std::string toString() const override {
        return eil::format("ConstSurface(%s)", q.toString());
    }
};

void dump3(const std::string& tag, const Vector3& v) {
    emit(eil::format("%s x=%.17g %s y=%.17g %s z=%.17g %s", tag, eil::re(v.x), bits(v.x),
                     eil::re(v.y), bits(v.y), eil::re(v.z), bits(v.z)));
}

bool vec_eq(const Vector3& a, const Vector3& b) {
    // D's Vector3.opEquals compares fields with ==, and +0.0 == -0.0 is true, which is
    // why the signed-zero rows print their bit patterns separately rather than relying
    // on these equality rows.
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

std::string tostr(const ParametricSurface::Ptr& s) {
    return s ? s->toString() : std::string("null");
}

void dump_members(const std::string& pfx, const WedgeVolume& v) {
    emit(pfx + ".str=" + v.toString());
    emit(pfx + ".face0123=" + tostr(v.face0123));
    emit(eil::format("%s.dtheta=%.17g %s", pfx, eil::re(v.dtheta), bits(v.dtheta)));
    for (int i = 0; i < 8; ++i) dump3(pfx + ".p[" + std::to_string(i) + "]", v.p[i]);
}

void dump_sweep(const std::string& pfx, const WedgeVolume& v) {
    const std::vector<double> pts = {0.0, 0.001, 0.5, 0.999, 0.9995, 1.0};
    for (double r : pts) {
        for (double s : pts) {
            for (double t : pts) {
                const std::string tg = " r=" + eil::to_string(r) + " s=" + eil::to_string(s) +
                                       " t=" + eil::to_string(t);
                dump3(pfx + ".pt" + tg, v(r, s, t));
                dump3(pfx + ".dVdr" + tg, v.dVdr(r, s, t));
                dump3(pfx + ".dVds" + tg, v.dVds(r, s, t));
                dump3(pfx + ".dVdt" + tg, v.dVdt(r, s, t));
                dump3(pfx + ".d2Vdr2" + tg, v.d2Vdr2(r, s, t));
                dump3(pfx + ".d2Vds2" + tg, v.d2Vds2(r, s, t));
                dump3(pfx + ".d2Vdt2" + tg, v.d2Vdt2(r, s, t));
                dump3(pfx + ".d2Vdrds" + tg, v.d2Vdrds(r, s, t));
                dump3(pfx + ".d2Vdrdt" + tg, v.d2Vdrdt(r, s, t));
                dump3(pfx + ".d2Vdsdt" + tg, v.d2Vdsdt(r, s, t));
            }
        }
    }
}

void check_ut(bool cond, const char* what) {
    ++g_ut_checks;
    if (!cond) {
        ++g_ut_fails;
        std::printf("FAILED unittest assertion: %s\n", what);
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }

    auto f = std::make_shared<ProbeSurface>(Vector3(0.25, 1.0, -0.5), Vector3(1.5, -0.75, 0.4),
                                            Vector3(-0.5, 0.3, 1.2), Vector3(0.9, -1.1, -0.9));

    auto v = std::make_shared<WedgeVolume>(f, eil::number(0.6));
    dump_members("wv.main", *v);
    const std::array<Vector3, 4> fc = {(*f)(0.0, 0.0), (*f)(1.0, 0.0), (*f)(1.0, 1.0),
                                       (*f)(0.0, 1.0)};
    for (int i = 0; i < 4; ++i) dump3("wv.main.faceCorner[" + std::to_string(i) + "]", fc[i]);

    dump_sweep("wv.main", *v);

    eil::geom::ParametricVolume::Ptr basev = v;
    auto dv = std::dynamic_pointer_cast<WedgeVolume>(basev->dup());
    emit("wv.dup.str=" + dv->toString());
    emit_bool("wv.dup.isFresh", dv.get() != v.get());
    emit_bool("wv.dup.textSame", dv->toString() == v->toString());
    emit_bool("wv.dup.faceSameHandle", dv->face0123.get() != f.get());
    for (int i = 0; i < 8; ++i) dump3("wv.dup.p[" + std::to_string(i) + "]", dv->p[i]);

    auto cv = std::make_shared<WedgeVolume>(*v); // D:37-42 copy constructor
    emit("wv.copy.str=" + cv->toString());
    emit_bool("wv.copy.textSame", cv->toString() == v->toString());
    emit_bool("wv.copy.isFresh", cv.get() != v.get());
    for (int i = 0; i < 8; ++i) dump3("wv.copy.p[" + std::to_string(i) + "]", cv->p[i]);

    // ---- the zero-dtheta wedge ---------------------------------------------
    auto v0 = std::make_shared<WedgeVolume>(f, eil::number(0.0));
    dump_members("wv.d0", *v0);
    for (int i = 0; i < 4; ++i) {
        emit_bool("wv.d0.p[" + std::to_string(i) + "+4].equals.p[" + std::to_string(i) + "]",
                  vec_eq(v0->p[i + 4], v0->p[i]));
    }
    bool same_at_t0 = true;
    for (double rr : {0.0, 0.125, 0.5, 1.0}) {
        for (double ss : {0.0, 0.375, 0.5, 1.0}) {
            Vector3 q = (*v0)(rr, ss, 0.0);
            Vector3 w = (*f)(rr, ss);
            same_at_t0 = same_at_t0 && vec_eq(q, w);
            if (rr == 0.125 && ss == 0.375) {
                dump3("wv.d0.probe.q", q);
                dump3("wv.d0.probe.w", w);
            }
        }
    }
    emit_bool("wv.d0.identityAtT0", same_at_t0);
    dump_sweep("wv.d0", *v0);

    // ---- dtheta battery -----------------------------------------------------
    // PI/4, PI and 2*PI are computed in eil::real (D's 80-bit PI) and rounded once on
    // the store into number, wave-02-brief section 5.
    eil::number neg_zero = -eil::number(0.0);
    const std::array<eil::number, 8> thetas = {eil::number(-0.6),
                                              eil::number(0.1),
                                              static_cast<eil::number>(EIL_PI / 4.0),
                                              static_cast<eil::number>(EIL_PI),
                                              static_cast<eil::number>(2.0 * EIL_PI),
                                              eil::number(1.0e-20),
                                              eil::d_init<eil::number>(),
                                              eil::number(0.0) - eil::number(0.0)};
    for (size_t i = 0; i < thetas.size(); ++i) {
        auto vw = std::make_shared<WedgeVolume>(f, thetas[i]);
        dump_members("wv.th[" + std::to_string(i) + "]", *vw);
    }
    v->dtheta = neg_zero;
    dump_members("wv.negZero", *v);
    // D's to!number(double.nan) is a quiet NaN with the sign bit clear;
    // std::numeric_limits<double>::quiet_NaN() is the same pattern (the bits column of
    // wv.nanTheta.dtheta checks it).
    v->dtheta = std::numeric_limits<double>::quiet_NaN();
    dump_members("wv.nanTheta", *v);
    v->dtheta = eil::number(0.6);

    // ---- signed-zero / axis battery through ConstSurface ---------------------
    const std::array<Vector3, 7> axis = {Vector3(0.5, 0.0, 0.0),
                                         Vector3(0.5, neg_zero, 0.0),
                                         Vector3(0.5, 0.0, neg_zero),
                                         Vector3(0.5, neg_zero, neg_zero),
                                         Vector3(0.5, 0.0, -1.0),
                                         Vector3(0.5, -1.0, 0.0),
                                         Vector3(0.5, -1.0, neg_zero)};
    for (size_t i = 0; i < axis.size(); ++i) {
        auto fa = std::make_shared<ConstSurface>(axis[i]);
        auto va = std::make_shared<WedgeVolume>(fa, eil::number(0.6));
        const std::string p = "wv.axis[" + std::to_string(i) + "]";
        dump_members(p, *va);
        dump3(p + ".ptHalf", (*va)(0.25, 0.75, 0.5));
    }

    v->face0123 = nullptr;
    emit("wv.nullface.str=" + v->toString());
    for (int i = 0; i < 8; ++i) dump3("wv.nullface.p[" + std::to_string(i) + "]", v->p[i]);

    // ---- (A) the D unittest, D:83-118, in source order ----------------------
    std::array<Vector3, 8> pp;                              // D:86 Vector3[8] p;
    pp[0] = Vector3(0.0, 0.1, 0.0);                         // D:87
    pp[1] = Vector3(1.0, 0.1, 0.0);                         // D:88
    pp[2] = Vector3(1.0, 1.1, 0.0);                         // D:89
    pp[3] = Vector3(0.0, 1.1, 0.0);                         // D:90
    ParametricSurface::Ptr my_face = std::make_shared<CoonsPatch>(pp[0], pp[1], pp[2],
                                                                  pp[3]); // D:91
    auto my_box = std::make_shared<WedgeVolume>(my_face, eil::number(0.1)); // D:92
    Vector3 d = (*my_box)(0.1, 0.1, 0.5);                                   // D:93
    // D:94 writeln("my_box=", my_box, " d=", d)
    emit("my_box=" + my_box->toString() + " d=" + d.toString());
    dump3("wv.ut.d", d);
    emit(eil::format("wv.ut.expected y=%.17g z=%.17g", eil::re(0.2 * std::cos(0.05)),
                     eil::re(0.2 * std::sin(0.05))));
    const Vector3 expect(0.1, eil::number(0.2) * std::cos(0.05),
                         eil::number(0.2) * std::sin(0.05)); // D:96 right-hand side
    emit_bool("wv.ut.approxEqualVectors", eil::geom::approxEqualVectors(d, expect));
    emit(eil::format("wv.ut.residual dx=%.17g dy=%.17g dz=%.17g", d.x - 0.1,
                     d.y - (0.2 * std::cos(0.05)), d.z - (0.2 * std::sin(0.05))));
    emit("wv.ut.str=" + my_box->toString());
    for (int i = 0; i < 8; ++i) dump3("wv.ut.p[" + std::to_string(i) + "]", my_box->p[i]);
#ifdef EIL_COMPLEX_NUMBERS
    // D:99-117, `version (complex_numbers)` — kept per wave-02-brief section 2 and NOT
    // compiled in the trunk build (backlog T-904). It is transcribed as D writes it:
    // perturb p[1].y by the complex step hIm = complex(0.0, 1.0e-20), rebuild the face
    // and the wedge, read derivCmplx = p[5].y.im / hIm.im; then restore p[1], perturb
    // by the real step hRe = 1.0e-04 and take
    // derivReal = (p[5].y.re - my_box.p[5].y.re)/hRe; assert isClose(derivCmplx,
    // derivReal) (D:116). The complex-step half needs the complex `number` of that
    // variant, which the substrate does not build in the trunk configuration, so this
    // block is the variant's referent rather than a running test.
    {
        std::array<Vector3, 8> cp = pp;
        const eil::number hIm = eil::number(0.0, 1.0e-20); // D:101 complex(0.0, 1.0e-20)
        cp[1] = Vector3(cp[1].x, cp[1].y + hIm, cp[1].z);  // D:102
        ParametricSurface::Ptr my_perturbed_face =
            std::make_shared<CoonsPatch>(cp[0], cp[1], cp[2], cp[3]); // D:103
        auto my_perturbed_box =
            std::make_shared<WedgeVolume>(my_perturbed_face, eil::number(0.1)); // D:104
        const double derivCmplx = eil::im(my_perturbed_box->p[5].y) / eil::im(hIm); // D:105
        cp[1] = Vector3(1.0, 0.1, 0.0);                     // D:108
        const double hRe = 1.0e-04;                         // D:111
        cp[1] = Vector3(cp[1].x, cp[1].y + hRe, cp[1].z);   // D:112
        my_perturbed_face = std::make_shared<CoonsPatch>(cp[0], cp[1], cp[2], cp[3]); // D:113
        my_perturbed_box = std::make_shared<WedgeVolume>(my_perturbed_face,
                                                         eil::number(0.1)); // D:114
        const double derivReal = (eil::re(my_perturbed_box->p[5].y)
                                  - eil::re(my_box->p[5].y)) / hRe;         // D:115
        eil_assert(eil::isClose(derivCmplx, derivReal)); // D:116
        check_ut(eil::isClose(derivCmplx, derivReal),
                 "wedgevolume.d:116 isClose(derivCmplx, derivReal) [complex_numbers]");
    }
#else
    emit("wv.ut.cmplx.notCompiled=true");
#endif
    eil_assert(eil::geom::approxEqualVectors(d, expect)); // D:96
    check_ut(eil::geom::approxEqualVectors(d, expect),
             "wedgevolume.d:96 approxEqualVectors(d, Vector3(0.1, 0.2*cos(0.05), "
             "0.2*sin(0.05)))");

    std::ifstream in(argv[1]);
    if (!in) {
        std::fprintf(stderr, "cannot open baseline %s\n", argv[1]);
        return 2;
    }
    std::vector<std::string> expected;
    for (std::string l; std::getline(in, l);) expected.push_back(l);

    int line_diffs = 0, shown = 0;
    size_t compared = 0;
    for (size_t i = 0; i < expected.size() || i < g_lines.size(); ++i) {
        const std::string e = i < expected.size() ? expected[i] : "<none>";
        const std::string g = i < g_lines.size() ? g_lines[i] : "<none>";
        if (i < expected.size() && i < g_lines.size()) ++compared;
        if (e == g) continue;
        if (shown < 10) {
            std::printf("line %zu\n  D:   |%s|\n  C++: |%s|\n", i + 1, e.c_str(), g.c_str());
            ++shown;
        }
        ++line_diffs;
    }

    std::printf("%s: geom_volume_wedgevolume, probe replay %zu D lines, %zu C++ lines, "
                "%d differ; %zu of %zu rows byte-identical; D unittest %d assertion, "
                "%d failed\n",
                (line_diffs == 0 && g_ut_fails == 0) ? "PASS" : "FAIL",
                expected.size(), g_lines.size(), line_diffs,
                compared - static_cast<size_t>(line_diffs), compared, g_ut_checks, g_ut_fails);
    return (line_diffs == 0 && g_ut_fails == 0) ? 0 : 1;
}
