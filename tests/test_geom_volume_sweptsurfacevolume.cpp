// tests/test_geom_volume_sweptsurfacevolume.cpp — C++ mirror of
// probes/geom/volume/probe_sweptsurfacevolume.d, replayed line for line against
// baselines/e587b235/probes/geom_volume_sweptsurfacevolume.txt (6587 D rows).
//
// SweptSurfaceVolume (D:12-65) translates a bottom surface along a sweep path. The
// rows cover: toString() and both stored members through to!string; all eight derived
// corners (D:24-31) twice — once as stored, once as opCall at the matching parameter
// corner, plus the eight equality rows, because p[0] (D:24) has NO face subtraction
// while opCall (D:55) does; dup() and the copy constructor (which differ in whether
// p[] is re-derived or copied, gated by the ssv.mu.* rows after p[0] is overwritten);
// 216-point sweeps of opCall and all nine inherited finite differences for the main
// volume and for two degenerate ones (zero-length sweep path, collapsed face), 3 x
// 2160 rows; toString() with both handles nulled; and the D unittest.
//
// Throw paths: this module has none to exercise. grep over sweptsurfacevolume.d's 83
// lines finds no `throw`, no `enforce` and no `assert` outside the unittest, so there
// is no throwable type to classify here and no row claims there is. The one failure
// mode D:22 can reach is a null constructor argument, where ldc2 1.40 produces an
// uncatchable segfault (measured in a scratch D program: "Segmentation fault (core
// dumped)", exit status 139, `catch (Throwable)` never runs — the same measurement
// recorded for SubRangedVolume at src/geom/volume/subrangedvolume.hpp:51-56). It is
// not replayed here because it would kill this process; the port dereferences the
// shared_ptr the same way rather than adding an enforce D does not have.
//
// (A) D unittest (D:67-83), ported assertion by assertion in source order:
//     1 assertion — `assert(approxEqualVectors(d, Vector3(0.1, 0.2, 1.5)))` at
//     sweptsurfacevolume.d:82, preceded by the eight corner assignments (D:68-77), the
//     CoonsPatch face and Line edge (D:79-80) and the opCall (D:81). The corpus rows
//     ssv.ut.* carry the measured values, including d.y=0.19999999999999993 and the
//     residual row dy=-8.3266726846886741e-17 against the ideal 0.2.
//
// Regenerate (full command in the probe header):
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/volume
//     probes/geom/volume/probe_sweptsurfacevolume.d -of=/tmp/probe_ssv
//     -od/tmp/probe_obj_vol2
//   then: /tmp/probe_ssv > baselines/e587b235/probes/geom_volume_sweptsurfacevolume.txt
//
// Run: ./test_geom_volume_sweptsurfacevolume <baseline.txt>
#include <array>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/line.hpp"
#include "geom/gpath/path.hpp"
#include "geom/surface/coonspatch.hpp"
#include "geom/surface/parametricsurface.hpp"
#include "geom/volume/parametricvolume.hpp"
#include "geom/volume/sweptsurfacevolume.hpp"

namespace {

using eil::geom::CoonsPatch;
using eil::geom::Line;
using eil::geom::ParametricSurface;
using eil::geom::Path;
using eil::geom::PathPtr;
using eil::geom::SweptSurfaceVolume;
using eil::geom::Vector3;

std::vector<std::string> g_lines;
int g_ut_checks = 0, g_ut_fails = 0; // D `unittest` block assertions

void emit(const std::string& s) { g_lines.push_back(s); }
void emit_bool(const std::string& tag, bool v) { emit(tag + "=" + (v ? "true" : "false")); }

// The probe's own face and path, so the corpus does not depend on any surface or path
// subclass port landing first (same shape as
// tests/test_geom_volume_twosurfacevolume.cpp:85-104). The path is quadratic and
// written as `t*t`, never `^^`, so no pow-vs-expanded-product difference can enter
// (wave-02-brief section 5).
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

class ProbePath : public Path {
  public:
    Vector3 a, b, c;

    ProbePath(Vector3 a, Vector3 b, Vector3 c) : a(a), b(b), c(c) {}

    Vector3 operator()(double t) const override { return a + b * t + c * (t * t); }
    std::shared_ptr<Path> dup() const override {
        return std::make_shared<ProbePath>(a, b, c);
    }
    std::string toString() const override {
        return eil::format("ProbePath(%s, %s, %s)", a.toString(), b.toString(), c.toString());
    }
    std::string classString() const override { return "ProbePath"; }
};

void dump3(const std::string& tag, const Vector3& v) {
    emit(eil::format("%s x=%.17g y=%.17g z=%.17g", tag, eil::re(v.x), eil::re(v.y),
                     eil::re(v.z)));
}

bool vec_eq(const Vector3& a, const Vector3& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

std::string tostr(const ParametricSurface::Ptr& s) {
    return s ? s->toString() : std::string("null");
}
std::string tostr(const PathPtr& p) { return p ? p->toString() : std::string("null"); }

void dump_members(const std::string& pfx, const SweptSurfaceVolume& v) {
    emit(pfx + ".str=" + v.toString());
    emit(pfx + ".face0123=" + tostr(v.face0123));
    emit(pfx + ".edge04=" + tostr(v.edge04));
    for (int i = 0; i < 8; ++i) dump3(pfx + ".p[" + std::to_string(i) + "]", v.p[i]);
}

void dump_sweep(const std::string& pfx, const SweptSurfaceVolume& v) {
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

// The D unittest assertion, counted apart from the row comparison so the summary can
// say how many there were.
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

    auto f = std::make_shared<ProbeSurface>(Vector3(0.5, -0.25, 0.1), Vector3(2.0, 0.75, -0.3),
                                            Vector3(-0.5, 1.5, 0.9), Vector3(1.25, 2.5, 0.05));
    auto e = std::make_shared<ProbePath>(Vector3(-1.5, 0.75, 2.25), Vector3(2.5, -1.0, 0.5),
                                         Vector3(0.75, 1.25, -0.25));
    auto v = std::make_shared<SweptSurfaceVolume>(f, e);
    dump_members("ssv.main", *v);

    // p[i] (D:24-31) against opCall at the matching parameter corner (D:55).
    const std::vector<double> cr = {0.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0, 0.0};
    const std::vector<double> cs = {0.0, 0.0, 1.0, 1.0, 0.0, 0.0, 1.0, 1.0};
    const std::vector<double> ct = {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0};
    for (int i = 0; i < 8; ++i) {
        Vector3 q = (*v)(cr[i], cs[i], ct[i]);
        dump3("ssv.anchor.p[" + std::to_string(i) + "]", v->p[i]);
        dump3("ssv.anchor.opCall[" + std::to_string(i) + "]", q);
        emit_bool("ssv.anchor.equal[" + std::to_string(i) + "]", vec_eq(v->p[i], q));
    }

    // dup() through the base handle, and the copy constructor.
    eil::geom::ParametricVolume::Ptr basev = v;
    auto dv = std::dynamic_pointer_cast<SweptSurfaceVolume>(basev->dup());
    emit("ssv.dup.str=" + dv->toString());
    emit_bool("ssv.dup.isFresh", dv.get() != v.get());
    emit_bool("ssv.dup.textSame", dv->toString() == v->toString());
    emit_bool("ssv.dup.faceSameHandle", dv->face0123.get() != f.get());
    emit_bool("ssv.dup.edgeSameHandle", dv->edge04.get() != e.get());
    for (int i = 0; i < 8; ++i)
        dump3("ssv.dup.p[" + std::to_string(i) + "]", dv->p[i]);

    auto cv = std::make_shared<SweptSurfaceVolume>(*v); // D:34-39 copy constructor
    emit("ssv.copy.str=" + cv->toString());
    emit_bool("ssv.copy.textSame", cv->toString() == v->toString());
    emit_bool("ssv.copy.isFresh", cv.get() != v.get());
    for (int i = 0; i < 8; ++i)
        dump3("ssv.copy.p[" + std::to_string(i) + "]", cv->p[i]);

    // Full sweeps: main volume, then the two degenerate ones.
    dump_sweep("ssv.main", *v);

    auto eSame = std::make_shared<ProbePath>(Vector3(1.0, 2.0, 3.0), Vector3(0.0, 0.0, 0.0),
                                             Vector3(0.0, 0.0, 0.0));
    auto vFlat = std::make_shared<SweptSurfaceVolume>(f, eSame);
    dump_members("ssv.flat", *vFlat);
    dump_sweep("ssv.flat", *vFlat);

    Vector3 c0(0.3, -1.1, 2.4);
    auto fPoint = std::make_shared<ProbeSurface>(c0, c0, c0, c0);
    auto vPoint = std::make_shared<SweptSurfaceVolume>(fPoint, e);
    dump_members("ssv.pointface", *vPoint);
    dump_sweep("ssv.pointface", *vPoint);

    // After overwriting p[0], the copy constructor carries the mutation and dup()
    // re-derives the corner from face0123/edge04 (D:24), so the two diverge.
    v->p[0] = Vector3(99.0, -99.0, 0.5);
    auto cv2 = std::make_shared<SweptSurfaceVolume>(*v);
    auto dv2 = std::dynamic_pointer_cast<SweptSurfaceVolume>(v->dup());
    dump3("ssv.mu.copy.p[0]", cv2->p[0]);
    dump3("ssv.mu.dup.p[0]", dv2->p[0]);
    emit_bool("ssv.mu.copyKeepsMutation", vec_eq(cv2->p[0], Vector3(99.0, -99.0, 0.5)));
    emit_bool("ssv.mu.dupReDerived", vec_eq(dv2->p[0], (*e)(0.0)));

    // toString() with both handles nulled: D prints "null" for a null class handle.
    v->face0123 = nullptr;
    v->edge04 = nullptr;
    emit("ssv.nullface.str=" + v->toString());

    // ---- (A) the D unittest, D:67-83, in source order ----------------------
    std::array<Vector3, 8> pp;                            // D:68 Vector3[8] p;
    pp[0] = Vector3(0.0, 0.1, 0.0);                      // D:69
    pp[1] = Vector3(1.0, 0.1, 0.0);                      // D:70
    pp[2] = Vector3(1.0, 1.1, 0.0);                      // D:71
    pp[3] = Vector3(0.0, 1.1, 0.0);                      // D:72
    pp[4] = Vector3(0.0, 0.1, 3.0);                      // D:74
    pp[5] = Vector3(1.0, 0.1, 3.0);                      // D:75
    pp[6] = Vector3(1.0, 1.1, 3.0);                      // D:76
    pp[7] = Vector3(0.0, 1.1, 3.0);                      // D:77

    ParametricSurface::Ptr my_face = std::make_shared<CoonsPatch>(pp[0], pp[1], pp[2],
                                                                  pp[3]); // D:79
    PathPtr my_edge = std::make_shared<Line>(pp[0], pp[4]);                // D:80
    auto my_box = std::make_shared<SweptSurfaceVolume>(my_face, my_edge);  // D:80
    Vector3 d = (*my_box)(0.1, 0.1, 0.5);                                  // D:81
    dump3("ssv.ut.d", d);
    emit_bool("ssv.ut.approxEqualVectors",
              eil::geom::approxEqualVectors(d, Vector3(0.1, 0.2, 1.5)));
    emit(eil::format("ssv.ut.residual dx=%.17g dy=%.17g dz=%.17g", d.x - 0.1, d.y - 0.2,
                     d.z - 1.5));
    emit("ssv.ut.str=" + my_box->toString());
    for (int i = 0; i < 8; ++i) dump3("ssv.ut.p[" + std::to_string(i) + "]", my_box->p[i]);
    auto utDup = std::dynamic_pointer_cast<SweptSurfaceVolume>(my_box->dup()); // D:41-44
    emit_bool("ssv.ut.dup.textSame", utDup->toString() == my_box->toString());
    for (int i = 0; i < 8; ++i) dump3("ssv.ut.dup.p[" + std::to_string(i) + "]", utDup->p[i]);
    eil_assert(eil::geom::approxEqualVectors(d, Vector3(0.1, 0.2, 1.5))); // D:82
    check_ut(eil::geom::approxEqualVectors(d, Vector3(0.1, 0.2, 1.5)),
             "sweptsurfacevolume.d:82 approxEqualVectors(d, Vector3(0.1, 0.2, 1.5))");

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

    std::printf("%s: geom_volume_sweptsurfacevolume, probe replay %zu D lines, %zu C++ lines, "
                "%d differ; %zu of %zu rows byte-identical; D unittest %d assertion, "
                "%d failed\n",
                (line_diffs == 0 && g_ut_fails == 0) ? "PASS" : "FAIL",
                expected.size(), g_lines.size(), line_diffs,
                compared - static_cast<size_t>(line_diffs), compared, g_ut_checks, g_ut_fails);
    return (line_diffs == 0 && g_ut_fails == 0) ? 0 : 1;
}
