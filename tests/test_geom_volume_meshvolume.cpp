// tests/test_geom_volume_meshvolume.cpp — C++ mirror of
// probes/geom/volume/probe_meshvolume.d, replayed line for line against
// baselines/e587b235/probes/geom_volume_meshvolume.txt (2167 D rows).
//
// geom/volume/meshvolume.d is 14 lines holding one EMPTY class (D:12-14), so this
// test has two jobs and no others:
//
//   (A) D unittest assertions: there are none. Measured —
//       `grep -c unittest geom/volume/meshvolume.d` = 0 and `grep -c 'assert(' ` = 0
//       and `grep -c throw` = 0, all three against the read-only reference tree at
//       e587b235. So part (A) below is the *compile-time* assertion that the module
//       is still a placeholder rather than a silently-usable class, which is the
//       only thing meshvolume.d can be wrong about:
//         g++ 11 on `new eil::geom::MeshVolume()`:
//           error: invalid new-expression of abstract class type
//                  'eil::geom::MeshVolume'
//           note: because the following virtual functions are pure within
//                 'eil::geom::MeshVolume':
//                   ParametricVolume::operator()(double,double,double) const
//                   ParametricVolume::dup() const
//                   ParametricVolume::toString() const
//         ldc2 1.40 on `new MeshVolume()`:
//           Error: cannot create instance of abstract class `MeshVolume`
//           /tmp/gdtk_shadow/geom/volume/meshvolume.d(12): class is declared here
//           parametricvolume.d(67): `string toString() const` is not implemented
//           parametricvolume.d(66): `ParametricVolume dup() const` is not implemented
//           parametricvolume.d(65): `Vector3 opCall(double r,double s,double t)
//                                   const` is not implemented
//       Both sides refuse, and both name the same three virtuals, so the port
//       inherits exactly the contract D inherits. (Reproduced with
//       `g++ -std=c++17 -I src -I substrate/include -c <scratch.cpp>`.)
//
//   (B) The 2167-row replay: toString()/dup() through the MeshVolume handle, then
//       the 216-point (r,s,t) sweep of opCall and of all nine inherited finite
//       differences, also through the MeshVolume handle. The sweep is the cross
//       product of 0.0, 0.001, 0.5, 0.999, 0.9995, 1.0 per parameter, chosen because
//       the base-class stencils branch on `x + 0.001 > 1.0` / `x - 0.001 < 0.0` and
//       0.999+0.001 rounds to exactly 1.0 (probe header, and
//       probes/geom/volume/probe_parametricvolume.d for the full argument). The last
//       three rows check that dup() is a fresh deep copy — mutating the source after
//       the dup must not touch the dup's text.
//
// Regenerate (the probe header carries the full command):
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/volume
//     probes/geom/volume/probe_meshvolume.d -of=/tmp/probe_mv
//     -od=/tmp/w2_volA/obj_mv
//   then: /tmp/probe_mv > baselines/e587b235/probes/geom_volume_meshvolume.txt
//
// Run: ./test_geom_volume_meshvolume <baseline.txt>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "eil/eil.hpp"
#include "eil/format.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/volume/meshvolume.hpp"
#include "geom/volume/parametricvolume.hpp"

namespace {

using eil::geom::MeshVolume;
using eil::geom::ParametricVolume;
using eil::geom::Vector3;

std::vector<std::string> g_lines;

void emit(const std::string& s) { g_lines.push_back(s); }
void emit_bool(const std::string& tag, bool v) {
    emit(tag + "=" + (v ? "true" : "false")); // D's %s on a bool, measured wording
}

// The same concrete subclass as the D probe, derived from MeshVolume.
class ProbeMeshVolume : public MeshVolume {
  public:
    Vector3 lo, dx, dy, dz;

    ProbeMeshVolume(Vector3 lo, Vector3 dx, Vector3 dy, Vector3 dz)
        : lo(lo), dx(dx), dy(dy), dz(dz) {}

    Vector3 operator()(double r, double s, double t) const override {
        Vector3 p = lo + dx * r + dy * s + dz * t;
        p += Vector3(0.11, -0.07, 0.05) * (r * s);
        p += Vector3(-0.03, 0.02, 0.09) * (s * t);
        return p;
    }
    std::shared_ptr<ParametricVolume> dup() const override {
        return std::make_shared<ProbeMeshVolume>(lo, dx, dy, dz);
    }
    std::string toString() const override {
        return eil::format("ProbeMeshVolume(lo=%s, dx=%s, dy=%s, dz=%s)", lo.toString(),
                           dx.toString(), dy.toString(), dz.toString());
    }
};

void dump3(const std::string& tag, const Vector3& v) {
    emit(eil::format("%s x=%.17g y=%.17g z=%.17g", tag, eil::re(v.x), eil::re(v.y),
                     eil::re(v.z)));
}

std::string para(double r, double s, double t) {
    return " r=" + eil::to_string(r) + " s=" + eil::to_string(s) + " t=" + eil::to_string(t);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }

    // (A) The placeholder really is a placeholder: MeshVolume declares nothing, so
    //     the three pure virtuals of ParametricVolume (parametricvolume.hpp:48-50)
    //     stay pure and `new MeshVolume()` is ill-formed, as ldc2 reports it.
    static_assert(std::is_base_of_v<ParametricVolume, MeshVolume>,
                  "meshvolume.hpp: D:12 derives MeshVolume from ParametricVolume");
    static_assert(std::is_abstract_v<MeshVolume>,
                  "meshvolume.hpp: MeshVolume overrides nothing, so it cannot be "
                  "instantiated — the same rejection ldc2 gives for `new MeshVolume()`");

    // (B) Replay.
    auto src = std::make_shared<ProbeMeshVolume>(Vector3(0.25, -0.5, 0.0),
                                                 Vector3(2.0, 0.5, 0.0),
                                                 Vector3(-0.5, 1.5, 0.75),
                                                 Vector3(0.5, 0.25, 3.0));
    MeshVolume* h = src.get(); // D: `MeshVolume h = new ProbeMeshVolume(...)`

    emit_bool("mv.derivesFromParametricVolume",
              std::is_base_of_v<ParametricVolume, MeshVolume>);
    emit("mv.str=" + h->toString());

    std::shared_ptr<ParametricVolume> b = h->dup(); // D: ParametricVolume b = h; b.dup()
    auto d = std::dynamic_pointer_cast<ProbeMeshVolume>(b);
    emit("mv.dup.str=" + d->toString());
    // D: `writefln("mv.dup.isFresh=%s", (cast(MeshVolume)d !is h))` — a handle
    // identity test, spelled as the pointer comparison it is.
    emit_bool("mv.dup.isFresh", d.get() != h);

    const std::vector<double> pts = {0.0, 0.001, 0.5, 0.999, 0.9995, 1.0};

    for (double r : pts)
        for (double s : pts)
            for (double t : pts)
                dump3("mv.pt" + para(r, s, t), (*h)(r, s, t));

    for (double r : pts) {
        for (double s : pts) {
            for (double t : pts) {
                const std::string tg = para(r, s, t);
                dump3("mv.dVdr" + tg, h->dVdr(r, s, t));
                dump3("mv.dVds" + tg, h->dVds(r, s, t));
                dump3("mv.dVdt" + tg, h->dVdt(r, s, t));
                dump3("mv.d2Vdr2" + tg, h->d2Vdr2(r, s, t));
                dump3("mv.d2Vds2" + tg, h->d2Vds2(r, s, t));
                dump3("mv.d2Vdt2" + tg, h->d2Vdt2(r, s, t));
                dump3("mv.d2Vdrds" + tg, h->d2Vdrds(r, s, t));
                dump3("mv.d2Vdrdt" + tg, h->d2Vdrdt(r, s, t));
                dump3("mv.d2Vdsdt" + tg, h->d2Vdsdt(r, s, t));
            }
        }
    }

    const std::string before = d->toString();
    src->lo = Vector3(9.0, 9.0, 9.0);
    emit("mv.dup.afterSourceMutation=" + d->toString());
    emit_bool("mv.dup.textUnchanged", d->toString() == before);
    emit("mv.str.afterSourceMutation=" + h->toString());

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

    std::printf("%s: geom_volume_meshvolume, probe replay %zu D lines, %zu C++ lines, "
                "%d differ; %zu of %zu rows byte-identical; D unittest 0 assertions "
                "(meshvolume.d has no unittest block)\n",
                line_diffs == 0 ? "PASS" : "FAIL", expected.size(), g_lines.size(), line_diffs,
                compared - static_cast<size_t>(line_diffs), compared);
    return line_diffs == 0 ? 0 : 1;
}
