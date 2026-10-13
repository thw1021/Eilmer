// tests/test_geom_volume_twosurfacevolume.cpp — C++ mirror of
// probes/geom/volume/probe_twosurfacevolume.d, replayed line for line against
// baselines/e587b235/probes/geom_volume_twosurfacevolume.txt (6608 D rows).
//
// TwoSurfaceVolume (D:44-134) is a ruled volume between two parametric surfaces.
// The rows cover: toString() for all six accepted spellings of ruled_direction and
// the eight derived corners for each (the constructor writes p[] in a different
// index order per branch, D:60-61 / :67-68 / :74-76 — that permutation is the
// module's geometric contract); the alias pairs, where the MEASURED answer is that
// the texts differ (tsv.alias.t_is_k=false) while the geometry does not
// (tsv.alias.pointsAllSame=true), because toString() prints the field raw (D:130);
// the `1.0-1` quirk of the "r"/"i" branch (D:110), kept verbatim and gated by three
// rows that prove face0 is discarded there and used in the "t"/"k" branch;
// dup() and the copy constructor, which differ in that dup() re-derives the corners;
// 216-point sweeps of opCall and all nine inherited finite differences for each of
// the three distinct branches (3 x 2160 rows); and both throw paths.
//
// (A) D unittest (D:136-153), ported assertion by assertion in source order:
//     1 assertion — `assert(approxEqualVectors(d, Vector3(0.1, 0.2, 1.5)))` at
//     twosurfacevolume.d:152, preceded by the eight corner assignments (D:137-146)
//     and the two CoonsPatch faces + volume + opCall (D:148-151). The corpus rows
//     tsv.ut.* carry the measured values, including d.y=0.19999999999999996 (the
//     residual row gives -5.5511151231257827e-17 against the ideal 0.2).
//
// (B) Throw paths, with WHICH type escaped (the brief's requirement): D:79
//     constructor default, D:121 opCall default (reached by reassigning the public
//     field, as D's own code allows), and dup() re-entering the constructor. ldc2
//     reports all three as `object.Exception`; the port throws
//     eil::ExceptionBase, and the name below is the ported code's answer to the
//     same question, so a port that threw the wrong sibling would print a different
//     word and fail the row (Exception and Error are siblings here: a
//     `catch (ExceptionBase)` never catches an ErrorBase).
//
// Regenerate (full command in the probe header):
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/volume
//     probes/geom/volume/probe_twosurfacevolume.d -of=/tmp/probe_tsv
//     -od=/tmp/w2_volA/obj_tsv
//   then: /tmp/probe_tsv > baselines/e587b235/probes/geom_volume_twosurfacevolume.txt
//
// Run: ./test_geom_volume_twosurfacevolume <baseline.txt>
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
#include "geom/surface/coonspatch.hpp"
#include "geom/surface/parametricsurface.hpp"
#include "geom/volume/parametricvolume.hpp"
#include "geom/volume/twosurfacevolume.hpp"

namespace {

using eil::geom::CoonsPatch;
using eil::geom::ParametricSurface;
using eil::geom::TwoSurfaceVolume;
using eil::geom::Vector3;

std::vector<std::string> g_lines;
int g_checks = 0, g_check_fails = 0;      // throw-path classification checks
int g_ut_checks = 0, g_ut_fails = 0;          // D `unittest` block assertions

void emit(const std::string& s) { g_lines.push_back(s); }
void emit_bool(const std::string& tag, bool v) { emit(tag + "=" + (v ? "true" : "false")); }

// The D-side spelling of `e.classinfo.name` for the two throwable siblings the port
// has (same table as tests/test_geom_surface_ruledsurface.cpp:81-83).
std::string d_type_name(const std::exception& e) {
    if (dynamic_cast<const eil::ExceptionBase*>(&e) != nullptr) return "object.Exception";
    if (dynamic_cast<const eil::ErrorBase*>(&e) != nullptr) return "object.Error";
    return "unknown";
}
bool caught_as_exception(const std::exception& e) {
    return dynamic_cast<const eil::ExceptionBase*>(&e) != nullptr;
}
bool caught_as_error(const std::exception& e) {
    return dynamic_cast<const eil::ErrorBase*>(&e) != nullptr;
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

std::string tostr(const ParametricSurface::Ptr& s) {
    return s ? s->toString() : std::string("null");
}

void dump_members(const std::string& pfx, const TwoSurfaceVolume& v) {
    emit(pfx + ".str=" + v.toString());
    emit(pfx + ".face0=" + tostr(v.face0));
    emit(pfx + ".face1=" + tostr(v.face1));
    emit(pfx + ".ruled_direction=" + v.ruled_direction);
    for (int i = 0; i < 8; ++i) dump3(pfx + ".p[" + std::to_string(i) + "]", v.p[i]);
}

void dump_sweep(const std::string& pfx, const TwoSurfaceVolume& v) {
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

// The D unittest assertion, counted apart from the throw-path checks so the
// summary can say how many of each there were.
void check_ut(bool cond, const char* what) {
    ++g_ut_checks;
    if (!cond) {
        ++g_ut_fails;
        std::printf("FAILED unittest assertion: %s\n", what);
    }
}

void check(bool cond, const char* what) {
    ++g_checks;
    if (!cond) {
        ++g_check_fails;
        std::printf("FAILED assertion: %s\n", what);
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }

    auto f0 = std::make_shared<ProbeSurface>(Vector3(0.5, -0.25, 0.1), Vector3(2.0, 0.75, -0.3),
                                             Vector3(-0.5, 1.5, 0.9), Vector3(1.25, 2.5, 0.05));
    auto f1 = std::make_shared<ProbeSurface>(Vector3(0.1, 0.2, 3.0), Vector3(2.5, -0.4, 2.6),
                                             Vector3(-0.3, 1.9, 3.9), Vector3(1.6, 2.1, 3.05));

    const std::vector<std::string> dirs = {"t", "k", "s", "j", "r", "i"};
    std::vector<std::shared_ptr<TwoSurfaceVolume>> vols;
    for (const std::string& d : dirs) {
        vols.push_back(std::make_shared<TwoSurfaceVolume>(f0, f1, d));
        dump_members("tsv." + d, *vols.back());
    }

    emit_bool("tsv.alias.t_is_k", vols[0]->toString() == vols[1]->toString());
    emit_bool("tsv.alias.s_is_j", vols[2]->toString() == vols[3]->toString());
    emit_bool("tsv.alias.r_is_i", vols[4]->toString() == vols[5]->toString());
    bool alias_pts_same = true;
    for (double rr : {0.0, 0.25, 0.5, 0.9995, 1.0}) {
        for (double ss : {0.0, 0.5, 1.0}) {
            for (double tt : {0.0, 0.3, 1.0}) {
                alias_pts_same = alias_pts_same && vec_eq((*vols[0])(rr, ss, tt), (*vols[1])(rr, ss, tt));
                alias_pts_same = alias_pts_same && vec_eq((*vols[2])(rr, ss, tt), (*vols[3])(rr, ss, tt));
                alias_pts_same = alias_pts_same && vec_eq((*vols[4])(rr, ss, tt), (*vols[5])(rr, ss, tt));
            }
        }
    }
    emit_bool("tsv.alias.pointsAllSame", alias_pts_same);

    // The "i" branch quirk, measured twice.
    auto vi = vols[5];
    auto q = (*vi)(0.375, 0.25, 0.625);
    const Vector3 expect = (*f1)(0.25, 0.625).scale(0.375); // face1(s,t).scale(r), D:110
    dump3("tsv.i.point", q);
    dump3("tsv.i.face1_scaled", expect);
    emit_bool("tsv.i.discards_face0", vec_eq(q, expect));
    {
        Vector3 i_at_zero = (*vi)(0.0, 0.25, 0.625);
        emit_bool("tsv.i.collapses_at_r0",
                  i_at_zero.x == 0.0 && i_at_zero.y == 0.0 && i_at_zero.z == 0.0);
    }
    auto other0 = std::make_shared<ProbeSurface>(Vector3(-7.0, -8.0, -9.0), Vector3(7.0, 8.0, 9.0),
                                                 Vector3(11.0, 12.0, 13.0),
                                                 Vector3(-11.0, -12.0, -13.0));
    auto vi2 = std::make_shared<TwoSurfaceVolume>(other0, f1, "i");
    bool same = true;
    for (double rr : {0.0, 0.375, 1.0}) {
        for (double ss : {0.0, 0.5, 1.0}) {
            same = same && vec_eq((*vi)(rr, ss, 0.625), (*vi2)(rr, ss, 0.625));
        }
    }
    emit_bool("tsv.i.face0_unused", same);
    auto vk = vols[0];
    auto k_with_other_face0 = std::make_shared<TwoSurfaceVolume>(other0, f1, "t");
    emit_bool("tsv.k.face0_used",
              !vec_eq((*vk)(0.4, 0.3, 0.6), (*k_with_other_face0)(0.4, 0.3, 0.6)));

    // dup() through the base handle, and the copy constructor.
    eil::geom::ParametricVolume::Ptr basev = vk;
    auto dk = std::dynamic_pointer_cast<TwoSurfaceVolume>(basev->dup());
    emit("tsv.k.dup.str=" + dk->toString());
    emit_bool("tsv.k.dup.isFresh", dk.get() != vk.get());
    emit_bool("tsv.k.dup.textSame", dk->toString() == vk->toString());
    for (int i = 0; i < 8; ++i)
        dump3("tsv.k.dup.p[" + std::to_string(i) + "]", dk->p[i]);

    auto ck = std::make_shared<TwoSurfaceVolume>(*vk); // D:83-89 copy constructor
    emit("tsv.k.copy.str=" + ck->toString());
    emit_bool("tsv.k.copy.textSame", ck->toString() == vk->toString());
    emit_bool("tsv.k.copy.isFresh", ck.get() != vk.get());
    for (int i = 0; i < 8; ++i)
        dump3("tsv.k.copy.p[" + std::to_string(i) + "]", ck->p[i]);

    // Full sweeps for the three distinct branches.
    dump_sweep("tsv.k", *vols[0]);
    dump_sweep("tsv.j", *vols[3]);
    dump_sweep("tsv.i", *vols[5]);

    // ---- throw paths ----------------------------------------------------------
    try {
        auto bad = std::make_shared<TwoSurfaceVolume>(f0, f1, "q");
        emit("tsv.ctorThrow.noThrow=" + bad->toString());
    } catch (const std::exception& e) {
        emit("tsv.ctorThrow.asException=" + d_type_name(e) + " msg=[" + e.what() + "]");
        check(caught_as_exception(e), "D:79 throws an Exception, catchable as ExceptionBase");
    }
    try {
        auto bad = std::make_shared<TwoSurfaceVolume>(f0, f1, "");
        emit("tsv.ctorThrowEmpty.noThrow=" + bad->toString());
    } catch (const std::exception& e) {
        // ThrowableBase derives from std::runtime_error, so what() is D's .msg for
        // either sibling — no down-cast, so a wrong-sibling throw still prints here
        // and fails the row instead of aborting the comparison.
        emit(eil::format("tsv.ctorThrowEmpty=%s isException=%s isError=%s msg=[%s]",
                         d_type_name(e), caught_as_exception(e) ? "true" : "false",
                         caught_as_error(e) ? "true" : "false", e.what()));
        check(!caught_as_error(e), "D:79's Exception is not an Error (siblings, not parent)");
    }
    vk->ruled_direction = "q";
    try {
        auto p = (*vk)(0.5, 0.5, 0.5);
        emit(eil::format("tsv.opcallThrow.noThrow x=%.17g", eil::re(p.x)));
    } catch (const std::exception& e) {
        emit("tsv.opcallThrow.asException=" + d_type_name(e) + " msg=[" + e.what() + "]");
        check(caught_as_exception(e), "D:121 throws an Exception, catchable as ExceptionBase");
    }
    try {
        auto d2 = vk->dup();
        emit("tsv.dupThrow.noThrow=" + d2->toString());
    } catch (const std::exception& e) {
        emit("tsv.dupThrow.asException=" + d_type_name(e) + " msg=[" + e.what() + "]");
        check(caught_as_exception(e), "dup() re-enters the validating constructor (D:93 -> D:79)");
    }
    vk->ruled_direction = "t";
    for (int i = 0; i < 8; ++i)
        dump3("tsv.k.p[" + std::to_string(i) + "].afterDirectionRepair", vk->p[i]);

    // ---- (A) the D unittest, D:136-153, in source order ----------------------
    std::array<Vector3, 8> pp;                       // D:137 Vector3[8] p;
    pp[0] = Vector3(0.0, 0.1, 0.0);                  // D:138
    pp[1] = Vector3(1.0, 0.1, 0.0);                  // D:139
    pp[2] = Vector3(1.0, 1.1, 0.0);                  // D:140
    pp[3] = Vector3(0.0, 1.1, 0.0);                  // D:141
    pp[4] = Vector3(0.0, 0.1, 3.0);                  // D:143
    pp[5] = Vector3(1.0, 0.1, 3.0);                  // D:144
    pp[6] = Vector3(1.0, 1.1, 3.0);                  // D:145
    pp[7] = Vector3(0.0, 1.1, 3.0);                  // D:146

    ParametricSurface::Ptr my_face_bottom = std::make_shared<CoonsPatch>(pp[0], pp[1], pp[2],
                                                                         pp[3]); // D:148
    ParametricSurface::Ptr my_face_top = std::make_shared<CoonsPatch>(pp[4], pp[5], pp[6],
                                                                      pp[7]);   // D:149
    auto my_box = std::make_shared<TwoSurfaceVolume>(my_face_bottom, my_face_top, "t"); // D:150
    Vector3 d = (*my_box)(0.1, 0.1, 0.5);                                               // D:151
    dump3("tsv.ut.d", d);
    emit_bool("tsv.ut.approxEqualVectors",
              eil::geom::approxEqualVectors(d, Vector3(0.1, 0.2, 1.5)));
    emit(eil::format("tsv.ut.residual dx=%.17g dy=%.17g dz=%.17g", d.x - 0.1, d.y - 0.2,
                     d.z - 1.5));
    emit("tsv.ut.str=" + my_box->toString());
    for (int i = 0; i < 8; ++i) dump3("tsv.ut.p[" + std::to_string(i) + "]", my_box->p[i]);
    eil_assert(eil::geom::approxEqualVectors(d, Vector3(0.1, 0.2, 1.5))); // D:152
    check_ut(eil::geom::approxEqualVectors(d, Vector3(0.1, 0.2, 1.5)),
             "twosurfacevolume.d:152 approxEqualVectors(d, Vector3(0.1, 0.2, 1.5))");

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

    std::printf("%s: geom_volume_twosurfacevolume, probe replay %zu D lines, %zu C++ lines, "
                "%d differ; %zu of %zu rows byte-identical; D unittest %d assertion, "
                "%d failed; throw-path checks %d, %d failed\n",
                (line_diffs == 0 && g_check_fails == 0 && g_ut_fails == 0) ? "PASS" : "FAIL",
                expected.size(), g_lines.size(), line_diffs,
                compared - static_cast<size_t>(line_diffs), compared, g_ut_checks, g_ut_fails,
                g_checks, g_check_fails);
    return (line_diffs == 0 && g_check_fails == 0 && g_ut_fails == 0) ? 0 : 1;
}
