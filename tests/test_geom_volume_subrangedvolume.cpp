// tests/test_geom_volume_subrangedvolume.cpp — C++ mirror of
// probes/geom/volume/probe_subrangedvolume.d, replayed line for line against
// baselines/e587b235/probes/geom_volume_subrangedvolume.txt (5202 D rows).
//
// SubRangedVolume (D:12-56) is a thin adapter: it stores a *dup* of an underlying
// volume (D:27) plus six re-mapping constants, and opCall (D:35-41) maps
// (r,s,t) into [r0,r1]x[s0,s1]x[t0,t1] before forwarding. The rows therefore check
// the exact toString() text (D:47-55, which interpolates to!string of a *class
// handle* — the underlying volume's own toString(), "null" if absent), the six
// constants twice (%.17g and D's %.6g to!string), and the mapping.
//
// (A) D unittest assertions: none. Measured against the read-only reference tree at
//     e587b235 — `grep -c unittest geom/volume/subrangedvolume.d` = 0,
//     `grep -c 'assert(' ` = 0, `grep -c throw` = 0. There is also no throw path to
//     exercise: the only failure route is `new SubRangedVolume(null)`, which reaches
//     `pvolume.dup()` (D:27) and SIGSEGVs rather than throwing (measured: exit
//     status 139, "Segmentation fault (core dumped)", `catch (Throwable)` never
//     runs). The port adds no guard, so it has no message to print either — the
//     honest coverage row is "0 throw paths, 1 crash path, not representable as a
//     log line in either language".
//
// (B) The replay. Every sweep point is dumped three times — the mapped coordinates,
//     the underlying volume called directly at them, and the adapter's own opCall —
//     so the second and third agreeing is a measured equality rather than an
//     assumption, and the first pins the floating-point value of
//     r0+(r1-r0)*r. The sweep is the cross product of 0.0, 0.001, 0.5, 0.999,
//     0.9995, 1.0 per parameter (216 points), because the nine inherited finite
//     differences branch on `x + 0.001 > 1.0` / `x - 0.001 < 0.0` and 0.999+0.001
//     rounds to exactly 1.0 — see probes/geom/volume/probe_parametricvolume.d.
//     Adapter A is the identity map (the constructor defaults, D:23-25), adapter B
//     is not, and the nesting rows check that dup() keeps one wrapper while an
//     adapter around an adapter renders nested.
//
// Regenerate (full command in the probe header):
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/volume
//     probes/geom/volume/probe_subrangedvolume.d -of=/tmp/probe_srf
//     -od=/tmp/w2_volA/obj_srf
//   then: /tmp/probe_srf > baselines/e587b235/probes/geom_volume_subrangedvolume.txt
//
// Run: ./test_geom_volume_subrangedvolume <baseline.txt>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "eil/format.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/volume/parametricvolume.hpp"
#include "geom/volume/subrangedvolume.hpp"

namespace {

using eil::geom::ParametricVolume;
using eil::geom::SubRangedVolume;
using eil::geom::Vector3;

std::vector<std::string> g_lines;

void emit(const std::string& s) { g_lines.push_back(s); }
void emit_bool(const std::string& tag, bool v) { emit(tag + "=" + (v ? "true" : "false")); }

class ProbeVolume : public ParametricVolume {
  public:
    Vector3 lo, dx, dy, dz;

    ProbeVolume(Vector3 lo, Vector3 dx, Vector3 dy, Vector3 dz)
        : lo(lo), dx(dx), dy(dy), dz(dz) {}

    Vector3 operator()(double r, double s, double t) const override {
        Vector3 p = lo + dx * r + dy * s + dz * t;
        p += Vector3(0.11, -0.07, 0.05) * (r * s);
        p += Vector3(-0.03, 0.02, 0.09) * (s * t);
        return p;
    }
    std::shared_ptr<ParametricVolume> dup() const override {
        return std::make_shared<ProbeVolume>(lo, dx, dy, dz);
    }
    std::string toString() const override {
        return eil::format("ProbeVolume(lo=%s, dx=%s, dy=%s, dz=%s)", lo.toString(),
                           dx.toString(), dy.toString(), dz.toString());
    }
};

void dump3(const std::string& tag, const Vector3& v) {
    emit(eil::format("%s x=%.17g y=%.17g z=%.17g", tag, eil::re(v.x), eil::re(v.y),
                     eil::re(v.z)));
}

// D has no opEq on Vector3 (measured: `grep -n opEq geom/elements/vector3.d` is
// empty), so `a == b` compares the fields, which is what ldc2 prints as true here.
bool vec_eq(const Vector3& a, const Vector3& b) {
    return a.x == b.x && a.y == b.y && a.z == b.z;
}

void sweep(const std::string& pfx, const SubRangedVolume& srv, const ProbeVolume& pv) {
    emit(pfx + ".str=" + srv.toString());
    emit(pfx + ".dup.str=" + srv.dup()->toString());
    emit(eil::format("%s.members r0=%.17g r1=%.17g s0=%.17g s1=%.17g t0=%.17g t1=%.17g",
                     pfx, srv.r0, srv.r1, srv.s0, srv.s1, srv.t0, srv.t1));
    emit(pfx + ".members.conv=" + eil::to_string(srv.r0) + "," + eil::to_string(srv.r1) +
         "," + eil::to_string(srv.s0) + "," + eil::to_string(srv.s1) + "," +
         eil::to_string(srv.t0) + "," + eil::to_string(srv.t1));

    const std::vector<double> pts = {0.0, 0.001, 0.5, 0.999, 0.9995, 1.0};
    for (double r : pts) {
        for (double s : pts) {
            for (double t : pts) {
                const std::string tg = " r=" + eil::to_string(r) + " s=" + eil::to_string(s) +
                                       " t=" + eil::to_string(t);
                const double rm = srv.r0 + (srv.r1 - srv.r0) * r;
                const double sm = srv.s0 + (srv.s1 - srv.s0) * s;
                const double tm = srv.t0 + (srv.t1 - srv.t0) * t;
                emit(eil::format("%s.map%s rm=%.17g sm=%.17g tm=%.17g", pfx, tg, rm, sm, tm));
                dump3(pfx + ".under" + tg, pv(rm, sm, tm));
                dump3(pfx + ".pt" + tg, srv(r, s, t));
                dump3(pfx + ".dVdr" + tg, srv.dVdr(r, s, t));
                dump3(pfx + ".dVds" + tg, srv.dVds(r, s, t));
                dump3(pfx + ".dVdt" + tg, srv.dVdt(r, s, t));
                dump3(pfx + ".d2Vdr2" + tg, srv.d2Vdr2(r, s, t));
                dump3(pfx + ".d2Vds2" + tg, srv.d2Vds2(r, s, t));
                dump3(pfx + ".d2Vdt2" + tg, srv.d2Vdt2(r, s, t));
                dump3(pfx + ".d2Vdrds" + tg, srv.d2Vdrds(r, s, t));
                dump3(pfx + ".d2Vdrdt" + tg, srv.d2Vdrdt(r, s, t));
                dump3(pfx + ".d2Vdsdt" + tg, srv.d2Vdsdt(r, s, t));
            }
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }

    auto pv = std::make_shared<ProbeVolume>(Vector3(0.25, -0.5, 0.0), Vector3(2.0, 0.5, 0.0),
                                            Vector3(-0.5, 1.5, 0.75),
                                            Vector3(0.5, 0.25, 3.0));
    emit("srf.pv.str=" + pv->toString());

    // (A) default subrange: the identity map.
    auto a = std::make_shared<SubRangedVolume>(pv);
    sweep("srf.A", *a, *pv);

    // (B) a non-identity subrange on all three axes, one of them shifted off zero.
    auto b = std::make_shared<SubRangedVolume>(pv, 0.2, 0.8, -0.1, 0.6, 0.3, 1.0);
    sweep("srf.B", *b, *pv);

    // The ctor dups (D:27): mutating the caller's volume afterwards changes neither
    // the adapter's stored text nor a point it evaluates.
    const std::string beforeA = a->toString();
    const Vector3 pBefore = (*a)(0.3, 0.4, 0.5);
    pv->lo = Vector3(9.0, 9.0, 9.0);
    dump3("srf.A.pt.afterSourceMutation", (*a)(0.3, 0.4, 0.5));
    emit_bool("srf.A.ptUnchanged", vec_eq((*a)(0.3, 0.4, 0.5), pBefore));
    emit_bool("srf.A.textUnchanged", a->toString() == beforeA);
    emit("srf.pv.str.afterMutation=" + pv->toString());

    // Nesting: an adapter around an adapter.
    auto nest = std::make_shared<SubRangedVolume>(b, 0.0, 0.5, 0.5, 1.0, 0.25, 0.75);
    emit("srf.nest.str=" + nest->toString());
    auto nd = std::dynamic_pointer_cast<SubRangedVolume>(nest->dup());
    emit("srf.nest.dup.str=" + nd->toString());
    emit_bool("srf.nest.dup.underIsSubRanged",
              std::dynamic_pointer_cast<SubRangedVolume>(nd->underlying_volume) != nullptr);
    dump3("srf.nest.pt r=0.5 s=0.5 t=0.5", (*nest)(0.5, 0.5, 0.5));
    dump3("srf.nest.dVdr r=0.5 s=0.5 t=0.5", nest->dVdr(0.5, 0.5, 0.5));

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

    std::printf("%s: geom_volume_subrangedvolume, probe replay %zu D lines, %zu C++ lines, "
                "%d differ; %zu of %zu rows byte-identical; D unittest 0 assertions "
                "(subrangedvolume.d has no unittest block), 0 throw paths\n",
                line_diffs == 0 ? "PASS" : "FAIL", expected.size(), g_lines.size(), line_diffs,
                compared - static_cast<size_t>(line_diffs), compared);
    return line_diffs == 0 ? 0 : 1;
}
