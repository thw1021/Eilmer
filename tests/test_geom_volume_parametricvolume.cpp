// tests/test_geom_volume_parametricvolume.cpp — C++ mirror of
// probes/geom/volume/probe_parametricvolume.d, replayed line for line against
// baselines/e587b235/probes/geom_volume_parametricvolume.txt (2165 D rows).
//
// ParametricVolume is the abstract base of the 8-subclass volume hierarchy: nine
// finite-difference accessors (D:68-268) and the VTK writer (D:272-304), all
// exercised through one concrete trilinear ProbeVolume so the differencing under
// test is the base class' and not a subclass' evaluation.
//
// The sweep is the cross product of six values per parameter, chosen by the branch
// analysis in the probe header (the boundary tests are `x+d > 1.0` and `x-d < 0.0`
// with d = 0.001, and 0.999+0.001 rounds to exactly 1.0, so `>` rather than `>=`
// decides the two rows at r = 0.001 and r = 0.999). 216 points x 9 accessors is
// deliberately redundant: the mixed partials call the first derivatives, which
// branch on their own, so one r value can put the outer function on its central
// branch while the inner dVds() takes a one-sided one. That is the only way the
// structural difference between D:144 (three-point, /(d*d)) and D:214 (two-point, /d)
// becomes observable instead of merely plausible.
//
// Tag formatting is part of the contract: the parameter appears through D's
// `to!string(double)`, which is %.6g, so the tag says "1" and not "1.0".
//
// Regenerate:
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/volume \
//     probes/geom/volume/probe_parametricvolume.d -of=/tmp/probe_pvl \
//     && /tmp/probe_pvl > baselines/e587b235/probes/geom_volume_parametricvolume.txt
//
// Run: ./test_geom_volume_parametricvolume <baseline.txt>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "eil/format.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/volume/parametricvolume.hpp"

namespace {

using eil::geom::ParametricVolume;
using eil::geom::Vector3;

std::vector<std::string> g_lines;

void emit(const std::string& s) { g_lines.push_back(s); }

class ProbeVolume : public ParametricVolume {
  public:
    Vector3 lo, dx, dy, dz;

    ProbeVolume(Vector3 lo, Vector3 dx, Vector3 dy, Vector3 dz)
        : lo(lo), dx(dx), dy(dy), dz(dz) {}

    // D's opCall: lo + r*dx + s*dy + t*dz, plus an r*s cross term (which makes
    // d2Vdrds non-zero and separates it from d2Vdr2/d2Vds2) and an s*t one.
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

std::string one_line(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::string out;
    size_t pos = 0;
    while (true) {
        size_t nl = body.find('\n', pos);
        if (nl == std::string::npos) { out += body.substr(pos); break; }
        out += body.substr(pos, nl - pos) + "<newline>";
        pos = nl + 1;
    }
    return out;
}

size_t file_len(const std::string& path) {
    std::ifstream in(path, std::ios::ate | std::ios::binary);
    return static_cast<size_t>(in.tellg());
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }

    auto vol = std::make_shared<ProbeVolume>(Vector3(0.25, -0.5, 0.0), Vector3(2.0, 0.5, 0.0),
                                             Vector3(-0.5, 1.5, 0.75),
                                             Vector3(0.5, 0.25, 3.0));
    emit("pvl.str=" + vol->toString());

    auto d = std::dynamic_pointer_cast<ProbeVolume>(vol->dup());
    emit("pvl.dup str=" + d->toString());

    const std::vector<double> pts = {0.0, 0.001, 0.5, 0.999, 0.9995, 1.0};

    for (double r : pts)
        for (double s : pts)
            for (double t : pts) {
                auto p = (*vol)(r, s, t);
                dump3("pvl.pt r=" + eil::to_string(r) + " s=" + eil::to_string(s) +
                          " t=" + eil::to_string(t),
                      p);
            }

    const char* names[9] = {"dVdr",   "dVds",   "dVdt",   "d2Vdr2",  "d2Vds2",
                            "d2Vdt2", "d2Vdrds", "d2Vdrdt", "d2Vdsdt"};
    for (double r : pts) {
        for (double s : pts) {
            for (double t : pts) {
                const std::string tag = " r=" + eil::to_string(r) + " s=" + eil::to_string(s) +
                                        " t=" + eil::to_string(t);
                dump3(std::string("pvl.") + names[0] + tag, vol->dVdr(r, s, t));
                dump3(std::string("pvl.") + names[1] + tag, vol->dVds(r, s, t));
                dump3(std::string("pvl.") + names[2] + tag, vol->dVdt(r, s, t));
                dump3(std::string("pvl.") + names[3] + tag, vol->d2Vdr2(r, s, t));
                dump3(std::string("pvl.") + names[4] + tag, vol->d2Vds2(r, s, t));
                dump3(std::string("pvl.") + names[5] + tag, vol->d2Vdt2(r, s, t));
                dump3(std::string("pvl.") + names[6] + tag, vol->d2Vdrds(r, s, t));
                dump3(std::string("pvl.") + names[7] + tag, vol->d2Vdrdt(r, s, t));
                dump3(std::string("pvl.") + names[8] + tag, vol->d2Vdsdt(r, s, t));
            }
        }
    }

    const std::string tmp = "/tmp/probe_pvl_full.vtk";
    const std::string tmp1 = "/tmp/probe_pvl_one.vtk";
    eil::geom::write_volume_as_vtk_xml(*vol, tmp, 3, 2, 3);
    emit("pvl.vtk.full len=" + std::to_string(file_len(tmp)));
    emit("pvl.vtk.full.body=" + one_line(tmp));

    eil::geom::write_volume_as_vtk_xml(*vol, tmp1, 1, 1, 1);
    emit("pvl.vtk.one.body=" + one_line(tmp1));

    std::remove(tmp.c_str());
    std::remove(tmp1.c_str());

    std::ifstream in(argv[1]);
    if (!in) {
        std::fprintf(stderr, "cannot open baseline %s\n", argv[1]);
        return 2;
    }
    std::vector<std::string> expected;
    for (std::string l; std::getline(in, l);) expected.push_back(l);

    // Count bit-identical numeric fields too, so the summary reports quantities and
    // not only whole lines (a %.17g field that differs by 1 ULP changes the line but
    // says nothing about the size of the gap).
    // Line-level replay only. The rows are whole formatted records (three %.17g
    // fields, or a whole VTK file on one line), so a per-field count would either
    // double-count the tag columns or need a parser that is not this test's job; the
    // honest number is "how many of the 2165 D rows came out byte-identical".
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

    std::printf("%s: geom_volume_parametricvolume, probe replay %zu D lines, %zu C++ lines, "
                "%d differ; %zu of %zu rows byte-identical\n",
                line_diffs == 0 ? "PASS" : "FAIL", expected.size(), g_lines.size(), line_diffs,
                compared - static_cast<size_t>(line_diffs), compared);
    return line_diffs == 0 ? 0 : 1;
}
