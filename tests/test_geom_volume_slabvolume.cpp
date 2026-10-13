// tests/test_geom_volume_slabvolume.cpp — C++ mirror of
// probes/geom/volume/probe_slabvolume.d, replayed line for line against
// baselines/e587b235/probes/geom_volume_slabvolume.txt (2227 D rows).
//
// SlabVolume (D:13-47) is a SweptSurfaceVolume whose sweep path is the Line through
// the bottom face's own first corner: super(face0123, new Line(p0, p0+dz)) with
// p0 = face0123(0.0,0.0) (D:25-26). The rows therefore dump toString() (D:41-46),
// every stored member including the three inherited from SweptSurfaceVolume
// (face0123, edge04 and the Vector3[8] corner array p[0..7], written by
// sweptsurfacevolume.d:24-31), the two anchor facts the sweep implies
// (edge04(0.0) == face(0,0) and edge04(1.0) == face(0,0)+dz), dup() with its own dz
// and corners plus a mutation-isolation check, then the 216-point (r,s,t) sweep of
// opCall and all nine inherited finite differences (2160 rows).
//
// (A) D unittest (D:49-65), ported assertion by assertion in source order:
//     1 assertion — `assert(approxEqualVectors(d, Vector3(0.1, 0.2, 1.5)))` at
//     slabvolume.d:64, after the eight corner assignments (D:50-59), the CoonsPatch
//     face (D:61) and the volume construction/evaluation (D:62-63). The corpus rows
//     slab.ut.coon.* carry D's measured values for that construction
//     (d.y=0.19999999999999993, residual -8.3266726846886741e-17).
//     The probe ALSO dumps the same volume over the bilinear four-corner face
//     (rows slab.ut.bilin.*), which is what a substitution would use. Measured from
//     the corpus itself: 10 of the 15 point rows agree and 5 do not (all the s=0.5
//     ones, in y: Coons 0.60000000000000009 vs bilinear 0.59999999999999998), and the
//     unittest point differs too (y 0.19999999999999993 vs 0.20000000000000004), so
//     this test replays the unittest rows with the real CoonsPatch (ported at
//     src/geom/surface/coonspatch.hpp) and not with the bilinear stand-in. The two
//     approxEqualVectors outcomes are nevertheless both true, so the D assertion's
//     outcome does not depend on that choice.
//
// (B) Guard paths. There are none that throw: slabvolume.d contains no `throw` and
//     no `enforce` (measured: grep -c throw = 0 over the module), and
//     ParametricVolume declares no classString. The one failure path is D's second
//     constructor (D:29-34), which calls a virtual method through the still-null
//     inherited face0123 handle: ldc2 1.40 dies with SIGSEGV, exit status 139, and
//     `catch (Throwable)` never runs (measured by
//     probes/geom/volume/probe_slabvolume_copyctor.d, whose stdout is only the
//     "before" line). A crash emits no bytes, so it cannot be a replay row; the port
//     keeps it unreachable by deleting the overload (see slabvolume.hpp), and the
//     static_assert below is the compile-time counterpart of that measured fact.
//
// Regenerate (full command in the probe header):
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/volume
//     probes/geom/volume/probe_slabvolume.d -of=/tmp/probe_slab
//     -od=/tmp/w2_volA/obj_slab
//   then: /tmp/probe_slab > baselines/e587b235/probes/geom_volume_slabvolume.txt
//
// Run: ./test_geom_volume_slabvolume <baseline.txt>
#include <array>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <type_traits>
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
#include "geom/volume/slabvolume.hpp"
#include "geom/volume/sweptsurfacevolume.hpp"

namespace {

using eil::geom::CoonsPatch;
using eil::geom::ParametricSurface;
using eil::geom::SlabVolume;
using eil::geom::Vector3;

std::vector<std::string> g_lines;
int g_checks = 0, g_check_fails = 0;      // guard / structural checks
int g_ut_checks = 0, g_ut_fails = 0;      // D `unittest` block assertions

void emit(const std::string& s) { g_lines.push_back(s); }
void emit_bool(const std::string& tag, bool v) { emit(tag + "=" + (v ? "true" : "false")); }

void check(bool cond, const char* what) {
    ++g_checks;
    if (!cond) {
        ++g_check_fails;
        std::printf("FAILED check: %s\n", what);
    }
}
void check_ut(bool cond, const char* what) {
    ++g_ut_checks;
    if (!cond) {
        ++g_ut_fails;
        std::printf("FAILED unittest assertion: %s\n", what);
    }
}

// D's to!string of a class handle: the virtual toString(), "null" for a null handle.
std::string tostr(const ParametricSurface::Ptr& s) {
    return s ? s->toString() : std::string("null");
}
std::string tostr(const eil::geom::PathPtr& p) {
    return p ? p->toString() : std::string("null");
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

void dump3(const std::string& tag, const Vector3& v) {
    emit(eil::format("%s x=%.17g y=%.17g z=%.17g", tag, eil::re(v.x), eil::re(v.y),
                     eil::re(v.z)));
}

bool vec_eq(const Vector3& a, const Vector3& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }

    // (B) The measured D crash, kept unreachable in the port: no copy constructor.
    static_assert(!std::is_copy_constructible_v<SlabVolume>,
                  "slabvolume.hpp deletes the copy constructor, because D's "
                  "this(ref const(SlabVolume)) (D:29-34) SIGSEGVs (exit 139, measured)");

    auto face = std::make_shared<ProbeSurface>(Vector3(0.5, -0.25, 0.1), Vector3(2.0, 0.75, -0.3),
                                               Vector3(-0.5, 1.5, 0.9), Vector3(1.25, 2.5, 0.05));
    Vector3 dz = Vector3(0.3, -0.2, 3.0);
    auto slab = std::make_shared<SlabVolume>(face, dz);

    emit("slab.str=" + slab->toString());
    emit("slab.face.str=" + tostr(slab->face0123));
    emit("slab.edge04.str=" + tostr(slab->edge04));
    emit("slab.edge04.t0.str=" + (*slab->edge04)(0.0).toString());
    emit("slab.edge04.t1.str=" + (*slab->edge04)(1.0).toString());
    emit("slab.face00.str=" + (*face)(0.0, 0.0).toString());
    emit_bool("slab.anchor.edge04_0_is_face00",
              vec_eq((*slab->edge04)(0.0), (*face)(0.0, 0.0)));
    emit_bool("slab.anchor.edge04_1_is_face00_plus_dz",
              vec_eq((*slab->edge04)(1.0), (*face)(0.0, 0.0) + dz));
    check(vec_eq((*slab->edge04)(0.0), (*face)(0.0, 0.0)),
          "edge04(0.0) anchors p0 at the face's own first corner (sweptsurfacevolume.d:16)");
    check(vec_eq((*slab->edge04)(1.0), (*face)(0.0, 0.0) + dz),
          "edge04(1.0) is face(0,0)+dz (slabvolume.d:26)");
    dump3("slab.dz", slab->dz);
    emit("slab.dz.text=" + slab->dz.toString());

    for (int i = 0; i < 8; ++i) dump3("slab.p[" + std::to_string(i) + "]", slab->p[i]);

    auto d = std::dynamic_pointer_cast<SlabVolume>(slab->dup());
    emit("slab.dup.str=" + d->toString());
    emit_bool("slab.dup.isFresh", d.get() != slab.get());
    dump3("slab.dup.dz", d->dz);
    for (int i = 0; i < 8; ++i) dump3("slab.dup.p[" + std::to_string(i) + "]", d->p[i]);
    const std::string before = d->toString();
    slab->dz = Vector3(7.0, 7.0, 7.0);
    emit_bool("slab.dup.textUnchanged", d->toString() == before);
    emit("slab.str.afterMutation=" + slab->toString());
    check(d->toString() == before, "dup() owns its own dz (D:38 -> D:24)");
    slab->dz = dz; // restore for the sweep

    const std::vector<double> pts = {0.0, 0.001, 0.5, 0.999, 0.9995, 1.0};
    for (double r : pts) {
        for (double s : pts) {
            for (double t : pts) {
                const std::string tg = " r=" + eil::to_string(r) + " s=" + eil::to_string(s) +
                                       " t=" + eil::to_string(t);
                dump3("slab.pt" + tg, (*slab)(r, s, t));
                dump3("slab.dVdr" + tg, slab->dVdr(r, s, t));
                dump3("slab.dVds" + tg, slab->dVds(r, s, t));
                dump3("slab.dVdt" + tg, slab->dVdt(r, s, t));
                dump3("slab.d2Vdr2" + tg, slab->d2Vdr2(r, s, t));
                dump3("slab.d2Vds2" + tg, slab->d2Vds2(r, s, t));
                dump3("slab.d2Vdt2" + tg, slab->d2Vdt2(r, s, t));
                dump3("slab.d2Vdrds" + tg, slab->d2Vdrds(r, s, t));
                dump3("slab.d2Vdrdt" + tg, slab->d2Vdrdt(r, s, t));
                dump3("slab.d2Vdsdt" + tg, slab->d2Vdsdt(r, s, t));
            }
        }
    }

    // ---- (A) the D unittest (D:49-65) ----------------------------------------
    std::array<Vector3, 8> q;                 // D:50 Vector3[8] p;
    q[0] = Vector3(0.0, 0.1, 0.0);            // D:51
    q[1] = Vector3(1.0, 0.1, 0.0);            // D:52
    q[2] = Vector3(1.0, 1.1, 0.0);            // D:53
    q[3] = Vector3(0.0, 1.1, 0.0);            // D:54
    q[4] = Vector3(0.0, 0.1, 3.0);            // D:56
    q[5] = Vector3(1.0, 0.1, 3.0);            // D:57
    q[6] = Vector3(1.0, 1.1, 3.0);            // D:58
    q[7] = Vector3(0.0, 1.1, 3.0);            // D:59

    ParametricSurface::Ptr my_face = std::make_shared<CoonsPatch>(q[0], q[1], q[2], q[3]); // D:61
    auto my_box = std::make_shared<SlabVolume>(my_face, q[4] - q[0]);                       // D:62
    Vector3 dvec = (*my_box)(0.1, 0.1, 0.5);                                                // D:63
    dump3("slab.ut.coon.d", dvec);
    emit_bool("slab.ut.coon.approxEqualVectors",
              eil::geom::approxEqualVectors(dvec, Vector3(0.1, 0.2, 1.5)));
    emit(eil::format("slab.ut.coon.residual dx=%.17g dy=%.17g dz=%.17g", dvec.x - 0.1,
                     dvec.y - 0.2, dvec.z - 1.5));
    emit("slab.ut.coon.str=" + my_box->toString());
    for (double rr : {0.0, 0.1, 0.5, 0.9995, 1.0}) {
        for (double ss : {0.0, 0.5, 1.0}) {
            dump3("slab.ut.coon.pt r=" + eil::to_string(rr) + " s=" + eil::to_string(ss) +
                      " t=0.5",
                  (*my_box)(rr, ss, 0.5));
        }
    }
    eil_assert(eil::geom::approxEqualVectors(dvec, Vector3(0.1, 0.2, 1.5))); // D:64
    check_ut(eil::geom::approxEqualVectors(dvec, Vector3(0.1, 0.2, 1.5)),
             "slabvolume.d:64 approxEqualVectors(d, Vector3(0.1, 0.2, 1.5))");

    // The bilinear stand-in the probe also dumped (rows slab.ut.bilin.*).
    ParametricSurface::Ptr bface = std::make_shared<ProbeSurface>(q[0], q[1], q[3], q[2]);
    auto bbox = std::make_shared<SlabVolume>(bface, q[4] - q[0]);
    Vector3 ub = (*bbox)(0.1, 0.1, 0.5);
    dump3("slab.ut.bilin.d", ub);
    emit_bool("slab.ut.bilin.approxEqualVectors",
              eil::geom::approxEqualVectors(ub, Vector3(0.1, 0.2, 1.5)));
    for (double rr : {0.0, 0.1, 0.5, 0.9995, 1.0}) {
        for (double ss : {0.0, 0.5, 1.0}) {
            dump3("slab.ut.bilin.pt r=" + eil::to_string(rr) + " s=" + eil::to_string(ss) +
                      " t=0.5",
                  (*bbox)(rr, ss, 0.5));
        }
    }

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

    std::printf("%s: geom_volume_slabvolume, probe replay %zu D lines, %zu C++ lines, "
                "%d differ; %zu of %zu rows byte-identical; D unittest %d assertion, "
                "%d failed; structural checks %d, %d failed\n",
                (line_diffs == 0 && g_check_fails == 0 && g_ut_fails == 0) ? "PASS" : "FAIL",
                expected.size(), g_lines.size(), line_diffs,
                compared - static_cast<size_t>(line_diffs), compared, g_ut_checks, g_ut_fails,
                g_checks, g_check_fails);
    return (line_diffs == 0 && g_check_fails == 0 && g_ut_fails == 0) ? 0 : 1;
}
