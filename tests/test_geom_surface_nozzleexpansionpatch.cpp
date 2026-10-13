// tests/test_geom_surface_nozzleexpansionpatch.cpp — C++ mirror of
// probes/geom/surface/probe_nozzleexpansionpatch.d, replayed line for line against
// baselines/e587b235/probes/geom_surface_nozzleexpansionpatch.txt (354 D rows).
//
// geom/surface/nozzleexpansionpatch.d has NO `unittest` block (measured: 0 occurrences
// in the 135 lines), so this replay is the whole gate.
//
// Groups:
//   nep.kern (26 rows), nep.kern2 (15 rows)
//                  the two transcendentals with their BIT patterns. This is the part of
//                  the gate that can fail for a language reason rather than an
//                  algorithmic one: D's atan is Phobos' atanImpl (mirrored by eil::atan)
//                  and D's tan is Phobos' tanImpl too (mirrored by the local d_tan in
//                  nozzleexpansionpatch.cpp, because the substrate has no eil::tan).
//                  Measured on the 21-angle sweep before writing the port: atan matched
//                  libm on all 21, tan did NOT — at x = 0.90000000000000002 Phobos gives
//                  bits 3ff4299ba9c2a137 and libm gives ...138. Row 32 of the baseline
//                  is exactly that argument, so a port that wrote std::tan fails there.
//                  tan(+/-inf) and tan(-nan) are also covered, because tanImpl returns
//                  the POSITIVE quiet NaN for an infinite argument (row 40) while
//                  propagating a NaN argument unchanged (row 41).
//   {bez,line,steep,conv}.*
//                  4 north edges x (toString + 36-point (r,s) sweep). The sweep's r list
//                  hits 0.0 and 1.0 exactly (the two end-edge branches), 1e-9 and
//                  1-1e-9 (both one-sided difference branches), 2e-4 and 0.9998 (just
//                  outside them, central difference), and the s list hits 0.0 (the axis
//                  point inside the else branch) and 1.0 (the north-edge branch). "steep"
//                  drives wall_angle into the 0.9 rad region; "conv" makes it negative.
//   nep.dup*, nep.copy*
//                  dup() (through the dup-ing constructor) and the copy constructor.
//   nep.*.area.*   the base class' area() at (10,10) and asymmetrically at (3,7), (7,3),
//                  (5,5), (2,9), (5,5).
//   {vert,flat,nulledge,tiny}.*
//                  the singular divisions: a vertical edge makes north_length 0, so the
//                  2%/98% tweaks are 0/0 and x comes out "-nan" (rows 207-250); a
//                  zero-length edge makes the slope 0/0 too; a 1e-12-long edge makes the
//                  tweaks blow up to huge finite values. All printed, none avoided.
//   nep.bez.r=-0.5, r=2, s=2, nan, inf
//                  no parameter validation exists in the module.
//
// Regenerate (one line, so this comment stays a single // comment):
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface probes/geom/surface/probe_nozzleexpansionpatch.d -of=/tmp/probe_nep -od=/tmp/w2_surfB/obj_nep
//   then /tmp/probe_nep > baselines/e587b235/probes/geom_surface_nozzleexpansionpatch.txt
//
// Run: ./test_geom_surface_nozzleexpansionpatch <baseline.txt>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "eil/format.hpp"
#include "eil/math.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/bezier.hpp"
#include "geom/gpath/line.hpp"
#include "geom/gpath/path.hpp"
#include "geom/surface/nozzleexpansionpatch.hpp"
#include "geom/surface/parametricsurface.hpp"

namespace {

using eil::geom::NozzleExpansionPatch;
using eil::geom::ParametricSurface;
using eil::geom::Vector3;
std::vector<std::string> g_lines;

void emit(const std::string& s) { g_lines.push_back(s); }

std::string g17(double v) { return eil::format("%.17g", v); }

std::string hexbits(double v) {
    uint64_t u;
    std::memcpy(&u, &v, 8);
    char b[24];
    std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(u));
    return std::string(b);
}

void dump_v3(const std::string& key, const Vector3& v) {
    emit(eil::format("%s x=%.17g y=%.17g z=%.17g", key, eil::re(v.x), eil::re(v.y),
                     eil::re(v.z)));
}

void dump_area(const std::string& key, ParametricSurface& ps, int nr, int ns) {
    auto a = ps.area(nr, ns);
    emit(eil::format("%s nr=%d ns=%d area x=%.17g y=%.17g z=%.17g", key, nr, ns, eil::re(a.x),
                     eil::re(a.y), eil::re(a.z)));
}

void sweep(const std::string& key, const NozzleExpansionPatch& p) {
    for (double r : {0.0, 1e-9, 2e-4, 0.25, 0.5, 0.75, 0.9998, 1.0 - 1e-9, 1.0}) {
        for (double s : {0.0, 0.25, 0.5, 1.0}) {
            dump_v3(eil::format("%s.opCall r=%.17g s=%.17g", key, r, s), p(r, s));
        }
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }

    // --- group 0: the kernels, with bit patterns ---------------------------------
    for (double m : {0.0, 0.001, 0.01, 0.05, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0,
                    1.1, 1.26, 1.5, 2.0, 3.0, 5.0, 10.0, 100.0, -0.1, -0.5, -1.26, -3.0}) {
        double a = eil::atan(m);
        double t = eil::geom::d_tan(a);
        emit("nep.kern atan(m=" + g17(m) + ")=" + g17(a) + " abits=" + hexbits(a) + " tan(that)="
             + g17(t) + " tbits=" + hexbits(t));
    }
    for (double a : {0.0, 0.05, 0.25, 0.5, 0.75, 0.9, 1.0, 1.2, 1.4, 1.5707963267948966, -0.9, 3.0,
                     6.0, 1.0 / 0.0, 0.0 / 0.0}) {
        double t = eil::geom::d_tan(a);
        emit("nep.kern2 tan(" + g17(a) + ")=" + g17(t) + " bits=" + hexbits(t));
    }

    // --- group 1: four north edges ----------------------------------------------
    auto bez = std::make_shared<eil::geom::Bezier>(eil::Array<Vector3>{
        Vector3(1.0, 0.2, 0.0), Vector3(2.0, 0.25, 0.0), Vector3(3.0, 0.9, 0.0),
        Vector3(4.5, 2.2, 0.0)});
    NozzleExpansionPatch np(bez);
    emit("nep.bez str=" + np.toString());
    emit("nep.bez cNorth=" + eil::geom::d_tostr(np.cNorth));
    sweep("nep.bez", np);

    NozzleExpansionPatch ln(std::make_shared<eil::geom::Line>(Vector3(0.5, 0.1, 0.0),
                                                              Vector3(3.0, 1.6, 0.0)));
    emit("nep.line str=" + ln.toString());
    sweep("nep.line", ln);

    auto steepB = std::make_shared<eil::geom::Bezier>(eil::Array<Vector3>{
        Vector3(0.0, 0.0, 0.0), Vector3(0.2, 0.9, 0.0), Vector3(0.5, 1.8, 0.0),
        Vector3(0.8, 2.6, 0.0)});
    NozzleExpansionPatch steep(steepB);
    emit("nep.steep str=" + steep.toString());
    sweep("nep.steep", steep);

    NozzleExpansionPatch conv(std::make_shared<eil::geom::Line>(Vector3(1.0, 2.0, 0.0),
                                                                Vector3(4.0, 0.5, 0.0)));
    emit("nep.conv str=" + conv.toString());
    sweep("nep.conv", conv);

    // --- group 2: dup() and the copy constructor ----------------------------------
    auto dp = std::dynamic_pointer_cast<NozzleExpansionPatch>(
        std::make_shared<NozzleExpansionPatch>(np)->dup());
    emit("nep.dup str=" + dp->toString());
    dump_v3("nep.dup.opCall", (*dp)(0.37, 0.63));
    NozzleExpansionPatch cp(np);
    emit("nep.copy str=" + cp.toString());
    dump_v3("nep.copy.opCall", cp(0.37, 0.63));
    emit("nep.dup.dup str=" + dp->dup()->toString());

    // --- group 3: area() ---------------------------------------------------------
    dump_area("nep.bez.area.default", np, 10, 10);
    dump_area("nep.bez.area.r3s7", np, 3, 7);
    dump_area("nep.bez.area.r7s3", np, 7, 3);
    dump_area("nep.line.area.default", ln, 10, 10);
    dump_area("nep.steep.area.r5s5", steep, 5, 5);
    dump_area("nep.conv.area.r2s9", conv, 2, 9);

    // --- group 4: singular divisions ---------------------------------------------
    NozzleExpansionPatch vert(std::make_shared<eil::geom::Line>(Vector3(1.0, 0.0, 0.0),
                                                                Vector3(1.0, 2.0, 0.0)));
    emit("nep.vert str=" + vert.toString());
    sweep("nep.vert", vert);

    NozzleExpansionPatch flat(std::make_shared<eil::geom::Line>(Vector3(2.0, 0.0, 0.0),
                                                                Vector3(5.0, 0.0, 0.0)));
    emit("nep.flat str=" + flat.toString());
    sweep("nep.flat", flat);

    NozzleExpansionPatch nul(std::make_shared<eil::geom::Line>(Vector3(1.0, 1.0, 0.0),
                                                               Vector3(1.0, 1.0, 0.0)));
    emit("nep.null-edge str=" + nul.toString());
    sweep("nep.nulledge", nul);

    NozzleExpansionPatch tiny(std::make_shared<eil::geom::Line>(Vector3(0.0, 0.0, 0.0),
                                                                Vector3(1e-12, 1e-12, 0.0)));
    emit("nep.tiny str=" + tiny.toString());
    sweep("nep.tiny", tiny);

    dump_v3("nep.bez.r=-0.5", np(-0.5, 0.5));
    dump_v3("nep.bez.r=2", np(2.0, 0.5));
    dump_v3("nep.bez.s=2", np(0.5, 2.0));
    dump_v3("nep.bez.nan", np(0.0 / 0.0, 0.5));
    dump_v3("nep.bez.inf", np(1.0 / 0.0, 0.5));

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
    std::printf("%s: geom_surface_nozzleexpansionpatch, probe replay %zu D rows, %zu C++ rows, "
                "%d differ; %zu of %zu rows byte-identical\n",
                diffs == 0 ? "PASS" : "FAIL", expected.size(), g_lines.size(), diffs,
                std::min(expected.size(), g_lines.size()) - static_cast<size_t>(diffs),
                std::min(expected.size(), g_lines.size()));
    return diffs == 0 ? 0 : 1;
}
