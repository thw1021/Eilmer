// tests/test_geom_volume_tfivolume.cpp — C++ mirror of probes/geom/volume/
// probe_tfivolume.d, replayed line for line against
// baselines/e587b235/probes/geom_volume_tfivolume.txt (4,464 D rows).
//
// TFIVolume (D:17-162) is the transfinite-interpolation hexahedron: six boundary
// surfaces plus eight corners, blended by opCall (D:118-138). The corpus is replayed in
// the probe's own order, group by group:
//
//   rows    1-   15  tvi.faces.*   dump_members of the 6-face constructor
//   rows   16-   30  tvi.box.*     dump_members of the 8-point constructor
//   rows   31-   33  cornersSame / textSame / faceIsDup
//   rows   34- 2193  tvi.faces.*   the 216-point (r,s,t) sweep: opCall + 9 differences
//   rows 2194- 4353  tvi.box.*     the same sweep on the box-built volume
//   rows 4354- 4374  dup() for both constructions
//   rows 4375- 4384  the copy constructor (D:107-111), which skips checkVolume
//   row    4385      tvi.skip.str  a face swapped in after construction
//   rows   4386-4387 tvi.swap.*    dup() re-runs the corner checks and throws
//   row    4388      tvi.nullFace.str
//   rows   4389-4396 tvi.corner[0..7] — one case per D guard site
//   rows   4397-4408 tvi.tol[*]    the approxEqualVectors bracket at |a| = 0.1
//   rows   4409-4416 tvi.ztol[*]   the same bracket anchored at 0
//   rows   4417-4428 tvi.*         checkVolume, degenerate geometry, catch as Exception/Error
//   rows   4429-4432 tvi.*         the two array-length edges
//   rows   4433-4464 tvi.ut.*      the D unittest (D:164-190)
//
// (A) The D `unittest` (D:164-190) has exactly 2 assertions —
//   D:178 assert(approxEqualVectors(c, Vector3(0.1, 0.2, 1.5)));
//   D:189 assert(approxEqualVectors(d, Vector3(0.1, 0.2, 1.5)));
// (measured: grep -c 'assert(' over the module is 2, both inside the unittest block, no
// assert elsewhere). Both are ported below in source order as eil_assert + a counted
// check_ut, and the values they are taken from are the corpus rows tvi.ut.c / tvi.ut.d.
//
// (B) Everything else the module can throw is replayed, not asserted away: eight
// open-corner Errors (D:32-84), the two whole-array messages at corners 2 and 3 (D:47,
// D:54) whose tail is Vector3.init NaN, the GeometryException from checkVolume (D:159),
// which is an Exception while the corner guards are Errors (siblings in this port), and
// the two D bounds Errors from the two constructors' different loop bounds (6 vs 8).
//
// Regenerate (full command in the probe header):
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/volume
//     probes/geom/volume/probe_tfivolume.d -of=/tmp/probe_tfi -od/tmp/w2_vol2c/obj_tfiv
//   then: /tmp/probe_tfi > baselines/e587b235/probes/geom_volume_tfivolume.txt
//
// Run: ./test_geom_volume_tfivolume <baseline.txt>
#include <array>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/number.hpp"
#include "geom/elements/nomenclature.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/geometry_exception.hpp"
#include "geom/gpath/path.hpp"
#include "geom/surface/coonspatch.hpp"
#include "geom/surface/parametricsurface.hpp"
#include "geom/volume/parametricvolume.hpp"
#include "geom/volume/tfivolume.hpp"

namespace {

using eil::geom::CoonsPatch;
using eil::geom::Face;
using eil::geom::ParametricSurface;
using eil::geom::ParametricVolume;
using eil::geom::TFIVolume;
using eil::geom::Vector3;

std::vector<std::string> g_lines;
int g_checks = 0, g_check_fails = 0; // structural checks
int g_ut_checks = 0, g_ut_fails = 0; // D `unittest` block assertions

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

// D's to!string of a class handle: the virtual toString(), "null" for a null handle
// (measured by tvi.nullFace.str).
std::string tostr(const ParametricSurface::Ptr& s) {
    return s ? s->toString() : std::string("null");
}

// The probe's ProbeSurface (probes/geom/volume/probe_tfivolume.d:77-95). Field order is
// the D one, p00, p10, p11, p01, and toString prints them in that same order, so the
// faces[N] rows line up with D's.
class ProbeSurface : public ParametricSurface {
  public:
    Vector3 p00, p10, p11, p01;

    ProbeSurface(Vector3 p00, Vector3 p10, Vector3 p11, Vector3 p01)
        : p00(p00), p10(p10), p11(p11), p01(p01) {}

    Vector3 operator()(double r, double s) const override {
        Vector3 a = p00 + (p10 - p00) * r;
        Vector3 b = p01 + (p11 - p01) * r;
        return a + (b - a) * s;
    }
    std::shared_ptr<ParametricSurface> dup() const override {
        return std::make_shared<ProbeSurface>(p00, p10, p11, p01);
    }
    std::string toString() const override {
        return eil::format("ProbeSurface(%s, %s, %s, %s)", p00.toString(), p10.toString(),
                           p11.toString(), p01.toString());
    }
};

using PSArray = std::array<ParametricSurface::Ptr, 6>;
using PPoint = std::array<Vector3, 8>;

// probe_tfivolume.d:138-148 — the six faces of a hexahedron, assembled the way
// tfivolume.d:94-99 assembles them.
PSArray faces_of(const PPoint& q) {
    PSArray f;
    f[Face::north] = std::make_shared<ProbeSurface>(q[3], q[2], q[6], q[7]);
    f[Face::south] = std::make_shared<ProbeSurface>(q[0], q[1], q[5], q[4]);
    f[Face::east] = std::make_shared<ProbeSurface>(q[1], q[2], q[6], q[5]);
    f[Face::west] = std::make_shared<ProbeSurface>(q[0], q[3], q[7], q[4]);
    f[Face::top] = std::make_shared<ProbeSurface>(q[4], q[5], q[6], q[7]);
    f[Face::bottom] = std::make_shared<ProbeSurface>(q[0], q[1], q[2], q[3]);
    return f;
}

// A Slice over a fixed array — what D's implicit static-array-to-slice conversion does
// at the constructor call sites.
TFIVolume::FaceSlice slice_of(PSArray& f) {
    return TFIVolume::FaceSlice(f.data(), f.size(), nullptr, 0);
}
TFIVolume::PointSlice slice_of(PPoint& q) {
    return TFIVolume::PointSlice(q.data(), q.size(), nullptr, 0);
}

void dump3(const std::string& tag, const Vector3& v) {
    emit(eil::format("%s x=%.17g y=%.17g z=%.17g", tag, eil::re(v.x), eil::re(v.y),
                     eil::re(v.z)));
}

// probe_tfivolume.d:102-111
void dump_members(const std::string& pfx, const TFIVolume& v) {
    emit(pfx + ".str=" + v.toString());
    for (int i = 0; i < 6; ++i) {
        emit(eil::format("%s.faces[%d]=%s", pfx, i, tostr(v.faces[size_t(i)])));
    }
    for (int i = 0; i < 8; ++i) {
        dump3(eil::format("%s.p[%d]", pfx, i), v.p[size_t(i)]);
    }
}

// probe_tfivolume.d:113-134 — opCall plus the nine inherited finite differences over
// the 216-point sweep. The six abscissae straddle the base stencils' `x + 0.001 > 1.0`
// branch (probes/geom/volume/probe_parametricvolume.d).
void dump_sweep(const std::string& pfx, const TFIVolume& v) {
    const std::vector<double> pts = {0.0, 0.001, 0.5, 0.999, 0.9995, 1.0};
    for (double r : pts) {
        for (double s : pts) {
            for (double t : pts) {
                const std::string tg = eil::format(" r=%s s=%s t=%s", eil::to_string(r),
                                                   eil::to_string(s), eil::to_string(t));
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

// D's e.classinfo.name for the throwable shapes this module produces: tfivolume.d:34 etc.
// throw `new Error` (object.Error), checkVolume throws GeometryException (an Exception),
// and the two constructors' bounds checks raise core.exception.ArrayIndexError. The name
// is taken from the DYNAMIC type, because the catch arms below bind the base: in this
// port RangeError derives from ErrorBase and GeometryException from ExceptionBase, and
// ErrorBase/ExceptionBase are siblings (wave-02-brief section 1 item 1).
std::string d_classinfo(const eil::ThrowableBase& e) {
    if (dynamic_cast<const eil::RangeError*>(&e) != nullptr) {
        return "core.exception.ArrayIndexError";
    }
    if (dynamic_cast<const eil::geom::GeometryException*>(&e) != nullptr) {
        return "geom.geometry_exception.GeometryException";
    }
    if (dynamic_cast<const eil::ErrorBase*>(&e) != nullptr) return "object.Error";
    return "object.Exception";
}

// probe_tfivolume.d:150-154 report(). D's `cast(Exception) e !is null` and
// `cast(Error) e !is null` are the two sibling tests the port keeps apart, so the two
// booleans come from dynamic_cast against ErrorBase / ExceptionBase.
template <typename E> void report(const std::string& tag, const E& e) {
    const bool is_err = dynamic_cast<const eil::ErrorBase*>(&e) != nullptr;
    const bool is_exc = dynamic_cast<const eil::ExceptionBase*>(&e) != nullptr;
    emit(eil::format("%s=%s isException=%s isError=%s msg=[%s]", tag, d_classinfo(e),
                     is_exc ? "true" : "false", is_err ? "true" : "false", e.msg()));
}

// Run one probe `try { emit-nothrow } catch (Throwable) { report }` block. The noThrow
// lambda emits its own row when the construction succeeds.
template <typename F, typename G>
void attempt(const std::string& tag, F&& build, G&& on_ok) {
    try {
        on_ok(build());
    } catch (const eil::RangeError& e) {
        report(tag, e);
    } catch (const eil::geom::GeometryException& e) {
        report(tag, e);
    } catch (const eil::ErrorBase& e) {
        report(tag, e);
    } catch (const eil::ExceptionBase& e) {
        report(tag, e);
    }
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

    // ---- rows 1-33: the two constructors on the same sheared block -------------
    // probe_tfivolume.d:160-168 — no two corners share a coordinate, so an index
    // permutation in faces[] or p[] shows.
    PPoint q = {Vector3(0.0, 0.1, 0.0),  Vector3(1.0, 0.1, 0.0),   Vector3(1.0, 1.1, 0.0),
                Vector3(0.0, 1.1, 0.0),  Vector3(0.2, 0.05, 3.0),  Vector3(1.3, 0.2, 2.75),
                Vector3(0.95, 1.25, 3.2), Vector3(-0.1, 0.95, 2.9)};

    auto face_set = faces_of(q);
    auto byFaces = std::make_shared<TFIVolume>(slice_of(face_set)); // D:24 constructor
    dump_members("tvi.faces", *byFaces);
    auto byBox = std::make_shared<TFIVolume>(slice_of(q)); // D:91 constructor
    dump_members("tvi.box", *byBox);

    bool corners_same = true; // probe:175-177
    for (int i = 0; i < 8; ++i) corners_same = corners_same && vec_eq(byFaces->p[i], byBox->p[i]);
    emit_bool("tvi.cornersSame", corners_same);
    emit_bool("tvi.textSame", byFaces->toString() == byBox->toString());

    auto fset = faces_of(q); // probe:180-182 — the stored faces are dups, not the caller's
    auto byFaces2 = std::make_shared<TFIVolume>(slice_of(fset));
    emit_bool("tvi.faceIsDup", byFaces2->faces[0].get() != fset[0].get());

    // ---- rows 34-4353: the two 2160-row sweeps --------------------------------
    dump_sweep("tvi.faces", *byFaces);
    dump_sweep("tvi.box", *byBox);

    // ---- rows 4354-4374: dup() of both constructions -------------------------
    // probe:188-197. D calls dup() through a ParametricVolume handle; the port goes
    // through the base pointer and casts back with dynamic_pointer_cast, the same
    // run-time type test D's `cast(TFIVolume)` performs.
    std::shared_ptr<ParametricVolume> basev = byFaces;
    auto df = std::dynamic_pointer_cast<TFIVolume>(basev->dup());
    emit("tvi.faces.dup.str=" + df->toString());
    emit_bool("tvi.faces.dup.textSame", df->toString() == byFaces->toString());
    emit_bool("tvi.faces.dup.isFresh", df.get() != byFaces.get());
    for (int i = 0; i < 8; ++i) {
        dump3(eil::format("tvi.faces.dup.p[%d]", i), df->p[size_t(i)]);
    }
    auto db = std::dynamic_pointer_cast<TFIVolume>(
        std::static_pointer_cast<ParametricVolume>(byBox)->dup());
    emit("tvi.box.dup.str=" + db->toString());
    emit_bool("tvi.box.dup.textSame", db->toString() == byBox->toString());
    for (int i = 0; i < 8; ++i) {
        dump3(eil::format("tvi.box.dup.p[%d]", i), db->p[size_t(i)]);
    }

    // ---- rows 4375-4384: the copy constructor (D:107-111) --------------------
    auto cf = std::make_shared<TFIVolume>(*byFaces);
    emit("tvi.faces.copy.str=" + cf->toString());
    emit_bool("tvi.faces.copy.textSame", cf->toString() == byFaces->toString());
    for (int i = 0; i < 8; ++i) {
        dump3(eil::format("tvi.faces.copy.p[%d]", i), cf->p[size_t(i)]);
    }

    // ---- row 4385: a face replaced after construction (probe:207-213) --------
    // The box constructor never corner-checked, so a mismatched face can be installed
    // through the public faces[] member and still print.
    PSArray brokenFaces = faces_of(q);
    auto keeper = std::dynamic_pointer_cast<ProbeSurface>(brokenFaces[Face::west]);
    keeper->p00 = keeper->p00 + Vector3(0.05, 0.0, 0.0);
    auto byBoxSkip = std::make_shared<TFIVolume>(slice_of(q));
    byBoxSkip->faces[Face::west] = keeper;
    emit("tvi.skip.str=" + byBoxSkip->toString());

    // ---- rows 4386-4387: dup() re-runs the corner checks (probe:220-230) -----
    auto swapped = std::make_shared<TFIVolume>(slice_of(q));
    ParametricSurface::Ptr tmpFace = swapped->faces[Face::west];
    swapped->faces[Face::west] = swapped->faces[Face::south];
    swapped->faces[Face::south] = tmpFace;
    emit("tvi.swap.str=" + swapped->toString());
    attempt("tvi.swap.dup", [&] { return swapped->dup(); },
            [](const std::shared_ptr<ParametricVolume>& dd) {
                emit("tvi.swap.dup.noThrow=" + dd->toString());
            });

    // ---- row 4388: null face handles are printable (probe:234-237) -----------
    auto nulled = std::make_shared<TFIVolume>(slice_of(q));
    nulled->faces[Face::east] = nullptr;
    nulled->faces[Face::top] = nullptr;
    emit("tvi.nullFace.str=" + nulled->toString());

    // ---- rows 4389-4396: one case per D corner guard (D:32-84) ---------------
    // Each case moves only the one ProbeSurface field that supplies p_alt1 for that
    // site, so the guard that fires is the one under test (probe:239-270).
    const Face alt_face[8] = {Face::west, Face::east, Face::east, Face::west,
                              Face::west, Face::east, Face::east, Face::west};
    for (int c = 0; c < 8; ++c) {
        PSArray fs = faces_of(q);
        auto surf = std::dynamic_pointer_cast<ProbeSurface>(fs[alt_face[c]]);
        if (c == 0 || c == 1) surf->p00 = surf->p00 + Vector3(0.05, 0.0, 0.0);
        else if (c == 2 || c == 3) surf->p10 = surf->p10 + Vector3(0.05, 0.0, 0.0);
        else if (c == 4 || c == 5) surf->p01 = surf->p01 + Vector3(0.05, 0.0, 0.0);
        else surf->p11 = surf->p11 + Vector3(0.05, 0.0, 0.0);
        const std::string tag = eil::format("tvi.corner[%d]", c);
        try {
            auto bad = std::make_shared<TFIVolume>(slice_of(fs));
            emit(tag + ".noThrow=" + bad->toString());
        } catch (const eil::RangeError& e) {
            report(tag, e);
        } catch (const eil::ErrorBase& e) {
            report(tag, e);
        }
    }

    // ---- rows 4397-4408: the approxEqualVectors bracket at |a| = 0.1 ---------
    // D's defaults maxRelDiff=1.0e-2 / maxAbsDiff=1.0e-5 with isClose's `<=` (see the
    // header of substrate/include/eil/math.hpp). delta 1.0000001e-5 is printed "1e-05"
    // because to!string is %.6g, but it is a different double.
    const std::vector<double> deltas = {0.0, 1.0e-6, 1.0e-5, 1.0000001e-5, 0.009, 0.01,
                                        0.011, 0.05};
    for (size_t i = 0; i < deltas.size(); ++i) {
        const double dl = deltas[i];
        PSArray fs = faces_of(q);
        auto w = std::dynamic_pointer_cast<ProbeSurface>(fs[Face::west]);
        w->p00 = w->p00 + Vector3(0.0, dl, 0.0);
        const std::string tag = eil::format("tvi.tol[%d]", int(i));
        const std::string dls = eil::to_string(dl);
        try {
            auto ok = std::make_shared<TFIVolume>(slice_of(fs));
            emit(eil::format("%s.dl=%s accepted=%s p0.y=%.17g", tag, dls, true, ok->p[0].y));
        } catch (const eil::ErrorBase& e) {
            emit(eil::format("%s.dl=%s rejected=%s", tag, dls, true));
            report(tag, e);
        }
    }
    // ---- rows 4409-4416: the same bracket anchored at a zero coordinate ------
    PPoint zq = q;
    zq[0] = Vector3(0.0, 0.0, 0.0);
    for (size_t i = 0; i < deltas.size(); ++i) {
        const double dl = deltas[i];
        PSArray fs = faces_of(zq);
        auto w = std::dynamic_pointer_cast<ProbeSurface>(fs[Face::west]);
        w->p00 = w->p00 + Vector3(dl, 0.0, 0.0);
        const std::string tag = eil::format("tvi.ztol[%d]", int(i));
        const std::string dls = eil::to_string(dl);
        try {
            auto ok = std::make_shared<TFIVolume>(slice_of(fs));
            emit(eil::format("%s.dl=%s accepted=true p0.x=%.17g", tag, dls, ok->p[0].x));
        } catch (const eil::ErrorBase&) {
            emit(eil::format("%s.dl=%s rejected=true", tag, dls));
        }
    }

    // ---- rows 4417-4428: checkVolume and the degenerate geometries -----------
    PPoint flat; // probe:309-317 — every corner in the z=0 plane, volume exactly 0
    for (int i = 0; i < 4; ++i) flat[i] = q[i];
    flat[4] = q[0];
    flat[5] = q[1];
    flat[6] = q[2];
    flat[7] = q[3];
    attempt("tvi.flat", [&flat] { return std::make_shared<TFIVolume>(slice_of(flat)); },
            [](const std::shared_ptr<TFIVolume>& v) { emit("tvi.flat.noThrow=" + v->toString()); });
    attempt("tvi.flatFaces",
            [&flat] {
                PSArray fs = faces_of(flat);
                return std::make_shared<TFIVolume>(slice_of(fs));
            },
            [](const std::shared_ptr<TFIVolume>& v) {
                emit("tvi.flatFaces.noThrow=" + v->toString());
            });
    PPoint thin = q; // probe:327-337 — 1.0e-13 tall
    thin[4] = Vector3(q[4].x, q[4].y, 1.0e-13);
    thin[5] = Vector3(q[5].x, q[5].y, 1.0e-13);
    thin[6] = Vector3(q[6].x, q[6].y, 1.0e-13);
    thin[7] = Vector3(q[7].x, q[7].y, 1.0e-13);
    attempt("tvi.thin", [&thin] { return std::make_shared<TFIVolume>(slice_of(thin)); },
            [](const std::shared_ptr<TFIVolume>& v) {
                emit(eil::format("tvi.thin.noThrow=%.17g", (*v)(0.5, 0.5, 0.5).z));
            });
    PPoint inv = q; // probe:339-346 — bottom winding reversed
    inv[0] = q[1];
    inv[1] = q[0];
    inv[2] = q[3];
    inv[3] = q[2];
    attempt("tvi.inverted", [&inv] { return std::make_shared<TFIVolume>(slice_of(inv)); },
            [](const std::shared_ptr<TFIVolume>& v) {
                emit("tvi.inverted.noThrow=" + v->toString());
            });
    PPoint inside_out; // probe:351-361 — the bottom and top slabs exchanged
    for (int i = 0; i < 4; ++i) {
        inside_out[i] = q[i + 4];
        inside_out[i + 4] = q[i];
    }
    attempt("tvi.insideOut",
            [&inside_out] { return std::make_shared<TFIVolume>(slice_of(inside_out)); },
            [](const std::shared_ptr<TFIVolume>& v) {
                emit("tvi.insideOut.noThrow=" + v->toString());
            });
    attempt("tvi.insideOutFaces",
            [&inside_out] {
                PSArray fs = faces_of(inside_out);
                return std::make_shared<TFIVolume>(slice_of(fs));
            },
            [](const std::shared_ptr<TFIVolume>& v) {
                emit("tvi.insideOutFaces.noThrow=" + v->toString());
            });
    PPoint nanq = q; // probe:370-377 — a NaN corner: the CoonsPatch guard fires first
    nanq[0] = Vector3(eil::quiet_nan_positive(), eil::quiet_nan_positive(),
                      eil::quiet_nan_positive());
    attempt("tvi.nan", [&nanq] { return std::make_shared<TFIVolume>(slice_of(nanq)); },
            [](const std::shared_ptr<TFIVolume>& v) { emit("tvi.nan.noThrow=" + v->toString()); });
    PPoint nanTop = q; // probe:379-386 — NaN only in a top corner
    nanTop[4] = Vector3(eil::quiet_nan_positive(), 0.05, 3.0);
    attempt("tvi.nanTop", [&nanTop] { return std::make_shared<TFIVolume>(slice_of(nanTop)); },
            [](const std::shared_ptr<TFIVolume>& v) {
                emit("tvi.nanTop.noThrow=" + v->toString());
            });
    PPoint one; // probe:389-396 — all six surfaces collapsed onto one point
    for (int i = 0; i < 8; ++i) one[i] = Vector3(0.25, -0.5, 1.0);
    attempt("tvi.collapsed",
            [&one] {
                PSArray fs = faces_of(one);
                return std::make_shared<TFIVolume>(slice_of(fs));
            },
            [](const std::shared_ptr<TFIVolume>& v) {
                emit("tvi.collapsed.noThrow=" + v->toString());
            });
    PSArray bothFaces = faces_of(flat); // probe:399-407 — open corner AND zero volume
    auto bw = std::dynamic_pointer_cast<ProbeSurface>(bothFaces[Face::west]);
    bw->p00 = bw->p00 + Vector3(0.5, 0.0, 0.0);
    attempt("tvi.bothBad", [&bothFaces] { return std::make_shared<TFIVolume>(slice_of(bothFaces)); },
            [](const std::shared_ptr<TFIVolume>& v) {
                emit("tvi.bothBad.noThrow=" + v->toString());
            });

    // probe:409-428 — the Exception/Error split. D's `catch (Exception e)` cannot take
    // the open-corner Error and `catch (Error e)` cannot take the GeometryException,
    // which is what the two arms below record.
    try {
        PPoint f2 = flat;
        auto v = std::make_shared<TFIVolume>(slice_of(f2));
        emit("tvi.catch.flat.noThrow=true");
        (void)v;
    } catch (const eil::ExceptionBase& e) {
        emit("tvi.catch.flat.asException=" + d_classinfo(e) + " msg=[" + e.msg() + "]");
    } catch (const eil::ErrorBase& e) {
        emit("tvi.catch.flat.asError=" + d_classinfo(e) + " msg=[" + e.msg() + "]");
    }
    try {
        PSArray fs = faces_of(q);
        auto w = std::dynamic_pointer_cast<ProbeSurface>(fs[Face::south]);
        w->p00 = w->p00 + Vector3(0.0, 0.0, 0.5);
        auto v = std::make_shared<TFIVolume>(slice_of(fs));
        emit("tvi.catch.corner.noThrow=true");
        (void)v;
    } catch (const eil::ExceptionBase& e) {
        emit("tvi.catch.corner.asException=" + d_classinfo(e) + " msg=[" + e.msg() + "]");
    } catch (const eil::ErrorBase& e) {
        emit("tvi.catch.corner.asError=" + d_classinfo(e) + " msg=[" + e.msg() + "]");
    }

    // ---- rows 4429-4432: the two array-length edges --------------------------
    // D's two constructors loop to different literal bounds (6 at D:27, 8 at D:93), so
    // a short face set and a short point list fail at different indices; a long face set
    // is truncated rather than rejected.
    PSArray six = faces_of(q);
    std::array<ParametricSurface::Ptr, 5> shortSet = {six[0], six[1], six[2], six[3], six[4]};
    attempt("tvi.shortArray",
            [&shortSet] {
                return std::make_shared<TFIVolume>(TFIVolume::FaceSlice(
                    shortSet.data(), shortSet.size(), nullptr, 0));
            },
            [](const std::shared_ptr<TFIVolume>& v) {
                emit("tvi.shortArray.noThrow=" + v->toString());
            });
    std::array<ParametricSurface::Ptr, 8> longSet;
    for (int i = 0; i < 6; ++i) longSet[i] = six[i];
    longSet[6] = std::make_shared<ProbeSurface>(Vector3(9, 9, 9), Vector3(8, 8, 8),
                                                Vector3(7, 7, 7), Vector3(6, 6, 6));
    longSet[7] = std::make_shared<ProbeSurface>(Vector3(-9, -9, -9), Vector3(-8, -8, -8),
                                                Vector3(-7, -7, -7), Vector3(-6, -6, -6));
    attempt("tvi.longArray",
            [&longSet] {
                return std::make_shared<TFIVolume>(TFIVolume::FaceSlice(
                    longSet.data(), longSet.size(), nullptr, 0));
            },
            [&](const std::shared_ptr<TFIVolume>& v) {
                emit_bool("tvi.longArray.accepted", v->faces[0] != nullptr);
                emit("tvi.longArray.str=" + v->toString());
            });
    std::array<Vector3, 7> shortPts = {q[0], q[1], q[2], q[3], q[4], q[5], q[6]};
    attempt("tvi.shortPts",
            [&shortPts] {
                return std::make_shared<TFIVolume>(
                    TFIVolume::PointSlice(shortPts.data(), shortPts.size(), nullptr, 0));
            },
            [](const std::shared_ptr<TFIVolume>& v) {
                emit("tvi.shortPts.noThrow=" + v->toString());
            });

    // ---- rows 4433-4464: the D unittest (D:164-190) --------------------------
    PPoint u = {Vector3(0.0, 0.1, 0.0),  Vector3(1.0, 0.1, 0.0),  Vector3(1.0, 1.1, 0.0),
                Vector3(0.0, 1.1, 0.0),  Vector3(0.0, 0.1, 3.0),  Vector3(1.0, 0.1, 3.0),
                Vector3(1.0, 1.1, 3.0),  Vector3(0.0, 1.1, 3.0)};

    auto simple_box = std::make_shared<TFIVolume>(slice_of(u)); // D:176
    auto c = (*simple_box)(0.1, 0.1, 0.5);                      // D:177
    dump3("tvi.ut.c", c);
    emit_bool("tvi.ut.simple.approxEqualVectors",
              eil::geom::approxEqualVectors(c, Vector3(0.1, 0.2, 1.5)));
    emit(eil::format("tvi.ut.simple.residual dx=%.17g dy=%.17g dz=%.17g", c.x - 0.1, c.y - 0.2,
                     c.z - 1.5));
    emit("tvi.ut.simple.str=" + simple_box->toString());
    for (int i = 0; i < 8; ++i) {
        dump3(eil::format("tvi.ut.simple.p[%d]", i), simple_box->p[size_t(i)]);
    }
    for (int i = 0; i < 6; ++i) {
        emit(eil::format("tvi.ut.simple.faces[%d]=%s", i, tostr(simple_box->faces[size_t(i)])));
    }
    // D:178 — the first of the module's two unittest assertions.
    eil_assert(eil::geom::approxEqualVectors(c, Vector3(0.1, 0.2, 1.5)));
    check_ut(eil::geom::approxEqualVectors(c, Vector3(0.1, 0.2, 1.5)),
             "tfivolume.d:178 approxEqualVectors(c, Vector3(0.1, 0.2, 1.5))");

    PSArray my_faces; // D:180-187, the six CoonsPatch faces of the same box
    my_faces[Face::north] = std::make_shared<CoonsPatch>(u[3], u[2], u[6], u[7]); // D:181
    my_faces[Face::south] = std::make_shared<CoonsPatch>(u[0], u[1], u[5], u[4]); // D:182
    my_faces[Face::east] = std::make_shared<CoonsPatch>(u[1], u[2], u[6], u[5]);  // D:183
    my_faces[Face::west] = std::make_shared<CoonsPatch>(u[0], u[3], u[7], u[4]);  // D:184
    my_faces[Face::top] = std::make_shared<CoonsPatch>(u[4], u[5], u[6], u[7]);   // D:185
    my_faces[Face::bottom] = std::make_shared<CoonsPatch>(u[0], u[1], u[2], u[3]); // D:186
    auto generic_box = std::make_shared<TFIVolume>(slice_of(my_faces));            // D:187
    auto d = (*generic_box)(0.1, 0.1, 0.5);                                         // D:188
    dump3("tvi.ut.d", d);
    emit_bool("tvi.ut.generic.approxEqualVectors",
              eil::geom::approxEqualVectors(d, Vector3(0.1, 0.2, 1.5)));
    emit(eil::format("tvi.ut.generic.residual dx=%.17g dy=%.17g dz=%.17g", d.x - 0.1, d.y - 0.2,
                     d.z - 1.5));
    emit("tvi.ut.generic.str=" + generic_box->toString());
    emit_bool("tvi.ut.generic.sameTextAsSimple", generic_box->toString() == simple_box->toString());
    for (int i = 0; i < 8; ++i) {
        dump3(eil::format("tvi.ut.generic.p[%d]", i), generic_box->p[size_t(i)]);
    }
    auto utDup = std::dynamic_pointer_cast<TFIVolume>(generic_box->dup()); // D:197
    emit_bool("tvi.ut.generic.dup.textSame", utDup->toString() == generic_box->toString());
    // D:189 — the second unittest assertion.
    eil_assert(eil::geom::approxEqualVectors(d, Vector3(0.1, 0.2, 1.5)));
    check_ut(eil::geom::approxEqualVectors(d, Vector3(0.1, 0.2, 1.5)),
             "tfivolume.d:189 approxEqualVectors(d, Vector3(0.1, 0.2, 1.5))");

    // Structural checks for the two facts the probe states rather than prints.
    check(corners_same == false,
          "the 6-face and 8-point constructors do not agree bit for bit (tvi.cornersSame)");
    check(byFaces2->faces[0].get() != fset[0].get(),
          "the constructor stores dups of the caller's faces (tfivolume.d:27)");

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

    std::printf("%s: geom_volume_tfivolume, probe replay %zu D lines, %zu C++ lines, "
                "%d differ; %zu of %zu rows byte-identical; D unittest %d assertion, "
                "%d failed; structural checks %d, %d failed\n",
                (line_diffs == 0 && g_check_fails == 0 && g_ut_fails == 0) ? "PASS" : "FAIL",
                expected.size(), g_lines.size(), line_diffs,
                compared - static_cast<size_t>(line_diffs), compared, g_ut_checks, g_ut_fails,
                g_checks, g_check_fails);
    return (line_diffs == 0 && g_check_fails == 0 && g_ut_fails == 0) ? 0 : 1;
}
