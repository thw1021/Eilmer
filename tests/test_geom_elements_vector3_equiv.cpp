// tests/test_geom_elements_vector3_equiv.cpp — replay probes/geom/elements/probe_vector3.d.
//
// Each line reproduces one D observation from
// baselines/e587b235/probes/geom_elements_vector3.txt at %.17g, so a match is a
// bit-exactness claim rather than a tolerance claim. Lines that differ are reported
// side by side.

#include <array>
#include <cstdarg>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "geom/elements/vector3.hpp"

using eil::geom::Vector3;
using eil::number;

namespace {

std::vector<std::string> produced;

void emitf(const char* fmt, ...) {
    char buf[8192];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    produced.emplace_back(buf);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <geom_elements_vector3.txt>\n";
        return 2;
    }

    Vector3 a = Vector3(1.0, 2.2, 3.0);
    Vector3 b = Vector3(0.5, -1.25, 4.0);
    Vector3 c = Vector3(eil::Array<number>{1.0, 2.2, 3.0}.all());

    emitf("ctor_array %.17g %.17g %.17g", c.x, c.y, c.z);
    emitf("ctor_one %.17g %.17g %.17g", Vector3(7.0).x, Vector3(7.0).y, Vector3(7.0).z);
    emitf("ctor_two %.17g %.17g %.17g", Vector3(7.0, 8.0).x, Vector3(7.0, 8.0).y,
          Vector3(7.0, 8.0).z);

    emitf("add %.17g %.17g %.17g", (a + b).x, (a + b).y, (a + b).z);
    emitf("sub %.17g %.17g %.17g", (a - b).x, (a - b).y, (a - b).z);
    emitf("mul %.17g %.17g %.17g", (a * 2.0).x, (a * 2.0).y, (a * 2.0).z);
    emitf("rmul %.17g %.17g %.17g", (3.0 * b).x, (3.0 * b).y, (3.0 * b).z);
    emitf("div %.17g %.17g %.17g", (a / 3.0).x, (a / 3.0).y, (a / 3.0).z);
    emitf("neg %.17g %.17g %.17g", (-a).x, (-a).y, (-a).z);
    emitf("pos %.17g %.17g %.17g", (+a).x, (+a).y, (+a).z);

    Vector3 acc = Vector3(a);
    acc += b;
    emitf("iadd %.17g %.17g %.17g", acc.x, acc.y, acc.z);
    acc -= b;
    emitf("isub %.17g %.17g %.17g", acc.x, acc.y, acc.z);
    acc *= 2.5;
    emitf("imul %.17g %.17g %.17g", acc.x, acc.y, acc.z);
    acc /= 7.0;
    emitf("idiv %.17g %.17g %.17g", acc.x, acc.y, acc.z);

    emitf("dot %.17g", eil::geom::dot(a, b));
    emitf("dot_member %.17g", a.dot(b));
    emitf("dot_ptr %.17g", eil::geom::dot(&a, &b));
    emitf("dot_product %.17g", eil::geom::dot_product(a.x, a.y, a.z, b.x, b.y, b.z));

    Vector3 xw = eil::geom::cross(a, b);
    emitf("cross %.17g %.17g %.17g", xw.x, xw.y, xw.z);
    number cx, cy, cz;
    eil::geom::cross_product(a.x, a.y, a.z, b.x, b.y, b.z, cx, cy, cz);
    emitf("cross_product %.17g %.17g %.17g", cx, cy, cz);

    emitf("abs %.17g", eil::geom::abs(a));
    emitf("distance %.17g", eil::geom::distance_between(a, b));
    emitf("distance_self %.17g", eil::geom::distance_between(a, a));
    Vector3 u = eil::geom::unit(b);
    emitf("unit %.17g %.17g %.17g %.17g", u.x, u.y, u.z, eil::geom::abs(u));

    Vector3 nz = Vector3(0.0, 0.0, 0.0);
    nz.normalize();
    emitf("norm_zero %.17g %.17g %.17g", nz.x, nz.y, nz.z);
    Vector3 nt = Vector3(1.0e-200, 0.0, 0.0);
    nt.normalize();
    emitf("norm_underflow %.17g %.17g %.17g", nt.x, nt.y, nt.z);
    Vector3 nf = Vector3(1.0e-31, 2.0e-31, 1.0);
    nf.normalize();
    emitf("norm_flush %.17g %.17g %.17g", nf.x, nf.y, nf.z);

    Vector3 sh = Vector3(1.0, 2.0, 3.0);
    sh.add(10.0, 20.0, 30.0, 4.0);
    emitf("add_shadow %.17g %.17g %.17g", sh.x, sh.y, sh.z);
    sh.add(b);
    emitf("add_ref %.17g %.17g %.17g", sh.x, sh.y, sh.z);
    sh.add(b, 0.5);
    emitf("add_ref_factor %.17g %.17g %.17g", sh.x, sh.y, sh.z);
    sh.scale(-2.0);
    emitf("scale %.17g %.17g %.17g", sh.x, sh.y, sh.z);
    sh.clear();
    emitf("clear %.17g %.17g %.17g", sh.x, sh.y, sh.z);

    const double pi4 = static_cast<double>(EIL_PI / 4.0L);
    const double pi3 = static_cast<double>(EIL_PI / 3.0L);
    Vector3 r1 = Vector3(1.0, 0.0, 0.0);
    r1.rotate_about_zaxis(pi4);
    emitf("rot_z %.17g %.17g %.17g", r1.x, r1.y, r1.z);
    Vector3 r2 = Vector3(1.0, 0.0, 0.0);
    r2.rotate_about_zaxis(pi3);
    emitf("rot_z_third %.17g %.17g %.17g", r2.x, r2.y, r2.z);
    const number PI_d = static_cast<number>(EIL_PI);
    Vector3 q45 = Vector3(std::cos(PI_d / 4), std::sin(PI_d / 4));
    q45.rotate2d(static_cast<double>(15.0L * EIL_PI / 180.0L));
    emitf("rot2d_45_60 %.17g %.17g %.17g", q45.x, q45.y, q45.z);
    Vector3 q60 = Vector3(std::cos(PI_d / 3), std::sin(PI_d / 3));
    emitf("rot2d_target %.17g %.17g %.17g", q60.x, q60.y, q60.z);
    Vector3 q30 = Vector3(std::cos(PI_d / 6), std::sin(PI_d / 6));
    q30.rotate2d(static_cast<double>(30.0L * EIL_PI / 180.0L));
    emitf("rot2d_30_60 %.17g %.17g %.17g", q30.x, q30.y, q30.z);

    Vector3 mpt = Vector3(0.0, 1.0, 0.0);
    Vector3 mnm = Vector3(0.0, 1.0, 0.0);
    Vector3 mi = Vector3(1.0, 0.0, 0.0);
    mi.mirror_image(mpt, mnm);
    emitf("mirror %.17g %.17g %.17g", mi.x, mi.y, mi.z);

    Vector3 n = Vector3(1.0, 1.0, 0.0);
    n = eil::geom::unit(n);
    Vector3 t1 = Vector3(-1.0, 1.0, 0.0);
    t1 = eil::geom::unit(t1);
    Vector3 t2 = eil::geom::cross(n, t1);
    Vector3 h = Vector3(1.0, 0.0, 1.0);
    Vector3 h3 = Vector3(h);
    h3.transform_to_local_frame(n, t1, t2);
    emitf("frame_local %.17g %.17g %.17g", h3.x, h3.y, h3.z);
    h3.transform_to_global_frame(n, t1, t2);
    emitf("frame_global %.17g %.17g %.17g", h3.x, h3.y, h3.z);
    Vector3 h2 = Vector3(h);
    h2.transform_to_local_frame(n, t1);
    emitf("frame_local2d %.17g %.17g %.17g", h2.x, h2.y, h2.z);
    h2.transform_to_global_frame(n, t1);
    emitf("frame_global2d %.17g %.17g %.17g", h2.x, h2.y, h2.z);
    Vector3 ctr = Vector3(0.5, 0.5, 0.0);
    Vector3 hc = Vector3(h);
    hc.transform_to_local_frame(n, t1, t2, ctr);
    emitf("frame_local_c %.17g %.17g %.17g", hc.x, hc.y, hc.z);
    hc.transform_to_global_frame(n, t1, t2, ctr);
    emitf("frame_global_c %.17g %.17g %.17g", hc.x, hc.y, hc.z);

    std::array<number, 9> R = {0.0, 1.0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 1.0};
    eil::Array<number> Ra(9);
    for (size_t i = 0; i < 9; ++i) Ra[i] = R[i];
    Vector3 mt = Vector3(1.0, 2.0, 3.0);
    mt.apply_matrix_transform(Ra.all());
    emitf("matrix_transform %.17g %.17g %.17g", mt.x, mt.y, mt.z);

    number ax = 1.0, ay = 2.0, az = 3.0;
    eil::geom::transform_to_local_frame(ax, ay, az, n, t1, t2);
    emitf("free_local %.17g %.17g %.17g", ax, ay, az);
    eil::geom::transform_to_global_frame(ax, ay, az, n, t1, t2);
    emitf("free_global %.17g %.17g %.17g", ax, ay, az);

    emitf("str_a %s", a.toString().c_str());
    emitf("str_b %s", b.toString().c_str());
    emitf("str_third %s", Vector3(1.0 / 3.0).toString().c_str());
    emitf("str_big %s", Vector3(1.0e100, -2.5e-7, 0.0).toString().c_str());
    emitf("str_init %s", Vector3().toString().c_str());

    emitf("approx %s %s %s", eil::geom::approxEqualVectors(a, c) ? "true" : "false",
          eil::geom::approxEqualVectors(a, b) ? "true" : "false",
          eil::geom::approxEqualVectors(Vector3(1.0, 2.0, 3.0), Vector3(1.0, 2.0, 3.0 + 1.0e-9))
              ? "true"
              : "false");

    std::ifstream in(argv[1]);
    if (!in) {
        std::cerr << "cannot open " << argv[1] << "\n";
        return 2;
    }
    std::vector<std::string> expected;
    for (std::string line; std::getline(in, line);) expected.push_back(line);

    int diffs = 0;
    for (size_t i = 0; i < expected.size() || i < produced.size(); ++i) {
        if (i >= expected.size() || i >= produced.size() || expected[i] != produced[i]) {
            if (diffs < 25) {
                std::cout << "line " << (i + 1) << "\n  D:  |"
                          << (i < expected.size() ? expected[i] : std::string("<none>")) << "|\n"
                          << "  C++:|" << (i < produced.size() ? produced[i] : std::string("<none>"))
                          << "|\n";
            }
            ++diffs;
        }
    }
    std::cout << (diffs == 0 ? "PASS" : "FAIL") << ": Vector3 vs D, " << expected.size()
              << " D lines, " << produced.size() << " C++ lines, " << diffs << " differ\n";
    return diffs == 0 ? 0 : 1;
}
