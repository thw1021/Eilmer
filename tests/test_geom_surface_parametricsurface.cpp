// tests/test_geom_surface_parametricsurface.cpp — C++ mirror of
// probes/geom/surface/probe_parametricsurface.d, replayed line for line against
// baselines/e587b235/probes/geom_surface_parametricsurface.txt (29 D rows).
//
// ParametricSurface is the abstract base of the 15-subclass surface hierarchy, and
// its own behaviour is two things only: area() (D:40-66) and the VTK writer
// (D:70-94). Both are exercised through the same concrete ProbeSurface the D probe
// defines, so what is compared is the base class' arithmetic and formatting, not a
// subclass' evaluation.
//
// The rows and why each group is load-bearing:
//   psf.unit.*   a flat unit square. Its vector area must be (0,0,+1) for every
//                (nr,ns) — that is the control on quad winding and on the nr*ns cell
//                count. D's own values drift in the last bits (nr=100 gives
//                0.99999999999990619), which is exactly why they are replayed
//                byte-for-byte rather than checked to a tolerance.
//   psf.skew.*   a sheared, tilted patch: all three components non-zero, so a
//                swapped cross product or a dropped dA factor shows in every column.
//   psf.point.*  four coincident corners: zero-area quads, the NaN/zero branch of
//                geom/elements/properties.d.
//   psf.dup.*    dup() through the base handle.
//   psf.vtk.*    the whole file body, newlines marked <newline>, including the
//                nrPts=1 case where D's `1.0/(nrPts-1)` is inf and the coordinates
//                come out as "-nan" under %20.16e.
//
// Regenerate:
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface \
//     probes/geom/surface/probe_parametricsurface.d -of=/tmp/probe_psf \
//     && /tmp/probe_psf > baselines/e587b235/probes/geom_surface_parametricsurface.txt
//
// Run: ./test_geom_surface_parametricsurface <baseline.txt>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/surface/parametricsurface.hpp"

#include "eil/format.hpp"

namespace {

using eil::geom::ParametricSurface;
using eil::geom::Vector3;
std::vector<std::string> g_lines;

void emit(const std::string& s) { g_lines.push_back(s); }

// D's ProbeSurface, verbatim.
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

void dump_area(const std::string& tag, const ParametricSurface& ps, int nr, int ns) {
    auto a = const_cast<ParametricSurface&>(ps).area(nr, ns);
    emit(eil::format("%s nr=%d ns=%d area x=%.17g y=%.17g z=%.17g", tag, nr, ns, eil::re(a.x),
                     eil::re(a.y), eil::re(a.z)));
}

// D's join(split(text,"\n"), "<newline>"), on the file the writer just produced.
std::string one_line(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::string out;
    size_t pos = 0;
    while (true) {
        size_t nl = body.find('\n', pos);
        if (nl == std::string::npos) {
            out += body.substr(pos);
            break;
        }
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

    // A flat unit square in the z=0 plane: its vector area must be (0,0,1) for any
    // (nr,ns), the control that the winding and the sum are right.
    auto unit = std::make_shared<ProbeSurface>(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0),
                                               Vector3(0.0, 1.0, 0.0), Vector3(1.0, 1.0, 0.0));
    dump_area("psf.unit.default", *unit, 10, 10);
    for (int n : {1, 2, 3, 5, 10, 100}) dump_area("psf.unit.sqr", *unit, n, n);
    dump_area("psf.unit.r1s7", *unit, 1, 7);
    dump_area("psf.unit.r7s1", *unit, 7, 1);

    auto skew = std::make_shared<ProbeSurface>(Vector3(0.5, -0.25, 0.1), Vector3(2.0, 0.75, -0.3),
                                               Vector3(-0.5, 1.5, 0.9),
                                               Vector3(1.25, 2.5, 0.05));
    dump_area("psf.skew.default", *skew, 10, 10);
    for (int n : {1, 2, 3, 4, 8, 17, 64}) dump_area("psf.skew.sqr", *skew, n, n);
    dump_area("psf.skew.r3s11", *skew, 3, 11);
    dump_area("psf.skew.r11s3", *skew, 11, 3);

    auto flat = std::make_shared<ProbeSurface>(Vector3(1.0, 1.0, 1.0), Vector3(1.0, 1.0, 1.0),
                                               Vector3(1.0, 1.0, 1.0), Vector3(1.0, 1.0, 1.0));
    dump_area("psf.point.default", *flat, 10, 10);

    auto d = std::dynamic_pointer_cast<ProbeSurface>(skew->dup());
    emit("psf.dup str=" + d->toString());
    for (double rs : {0.0, 0.25, 0.5, 1.0}) {
        auto q = (*d)(rs, 1.0 - rs);
        emit(eil::format("psf.dup opCall r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g", rs, 1.0 - rs,
                         eil::re(q.x), eil::re(q.y), eil::re(q.z)));
    }

    const std::string tmp = "/tmp/probe_psf_unit.vtk";
    const std::string tmp2 = "/tmp/probe_psf_skew.vtk";
    const std::string tmp3 = "/tmp/probe_psf_one.vtk";
    eil::geom::write_surface_as_vtk_xml(*unit, tmp, 3, 4);
    emit("psf.vtk.unit len=" + std::to_string(file_len(tmp)));
    emit("psf.vtk.unit.body=" + one_line(tmp));

    eil::geom::write_surface_as_vtk_xml(*skew, tmp2, 2, 2);
    emit("psf.vtk.skew.body=" + one_line(tmp2));

    eil::geom::write_surface_as_vtk_xml(*skew, tmp3, 1, 1);
    emit("psf.vtk.one.body=" + one_line(tmp3));

    std::remove(tmp.c_str());
    std::remove(tmp2.c_str());
    std::remove(tmp3.c_str());

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
    // geom/surface/parametricsurface.d has no `unittest` block, so this replay is
    // the whole gate for the base class, exactly as in the D probe.
    std::printf("%s: geom_surface_parametricsurface, probe replay %zu D lines, %zu C++ lines, "
                "%d differ; %zu of %zu rows byte-identical\n",
                diffs == 0 ? "PASS" : "FAIL", expected.size(), g_lines.size(), diffs,
                std::min(expected.size(), g_lines.size()) - static_cast<size_t>(diffs),
                std::min(expected.size(), g_lines.size()));
    return diffs == 0 ? 0 : 1;
}
