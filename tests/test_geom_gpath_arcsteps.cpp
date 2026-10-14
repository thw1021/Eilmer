// tests/test_geom_gpath_arcsteps.cpp
//
// Corpus replay of probes/geom/gpath/probe_arcsteps.d against
// baselines/e587b235/probes/geom_gpath_arcsteps.txt (116 D lines at %.17g).
//
// Two layers, both needed by the arc port:
//
//  1. atan2/atan over 41 (y,x) pairs. D's std.math.atan2(double,double) is Phobos'
//     own atan2Impl, which quadrant-reduces and calls atanImpl (CEPHES/fdlibm);
//     glibc's atan2 is the IBM Accurate Mathematical Engine and disagrees by 1 ULP
//     near the y-axis (atan2(1.0, -1.6653345369377348e-16): D 1.5707963267948966,
//     glibc 1.5707963267948968). arc.d:92 calls it once per evaluated point, so a
//     port that writes std::atan2 moves every Arc number. eil::geom::phobos_atan2
//     (src/geom/gpath/arc.cpp) is the faithful translation and this corpus is its
//     gate. The promotion of that helper to eil::atan2 in the substrate is reported
//     as a T-9xx candidate: vector3.cpp:313, helix.cpp:44 and the asin replica in
//     idealgasflow.cpp all call std::atan2 and carry the same latent divergence.
//
//  2. The intermediates of Arc.evaluate_position_and_length (arc.d:58-101),
//     recomputed with the ported Vector3 operations for four arcs whose centres are
//     on-axis, off-axis and shallow. This is what localised the atan2 divergence in
//     the first place: every step agrees until theta.
//
// Run: ./test_geom_gpath_arcsteps ../baselines/e587b235/probes/geom_gpath_arcsteps.txt

#include <cmath>
#include "eil/math.hpp"
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "eil/eil.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/arc.hpp"
#include "nm/number.hpp"

#include "gpath1_dump.hpp"

using eil::geom::Arc;
using eil::geom::Arc3;
using eil::geom::Vector3;
using eil::number;

namespace {

using namespace eil_test_gpath1;

// The same table, in the same order, as atan_table() in the probe. Built at run
// time (rather than as a literal) so the sqrt(3.0) entry carries the same bits D
// put into it.
std::vector<std::pair<double, double>> pairs() {
    const double inf = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();
    return {
        {1.0, -1.6653345369377348e-16}, {1.0, 0.0},          {-1.0, 0.0},
        {0.0, 1.0},                     {-0.0, 1.0},         {0.0, -1.0},
        {-0.0, -1.0},                   {1.0, 1.0},          {1.0, -1.0},
        {-1.0, 1.0},                    {-1.0, -1.0},        {1.0, eil::sqrt(3.0)},
        {0.5, 0.25},                    {0.66, 1.0},         {0.67, 1.0},
        {0.4142135623730951, 1.0},      {0.4142135623730950, 1.0},
        {2.414213562373095, 1.0},       {2.4142135623730951, 1.0},
        {3.0, 1.0},                     {1.0, 3.0},          {1.0e-300, 1.0},
        {1.0, 1.0e-300},                {1.0e-300, 1.0e-300}, {1.0e300, 1.0e-300},
        {1.0e-300, 1.0e300},            {2.0, 1.7},          {-0.3, 1.2},
        {1.2, -0.3},                    {-1.2, -0.3},        {0.7071067811865476, 0.7071067811865475},
        {1.0, inf},                     {1.0, -inf},         {inf, 1.0},
        {-inf, 1.0},                    {inf, inf},          {-inf, -inf},
        {nan, 1.0},                     {1.0, nan},
        {0.99999815799335934, 1.8420066406622953e-06},
        {-5.551115123125783e-17, 1.0},
    };
}

void atan_table() {
    for (const std::pair<double, double>& p : pairs()) {
        const double v = eil::geom::phobos_atan2(p.first, p.second);
        emitf("atan2 y=%.17g x=%.17g v=%.17g ay=%.17g ax=%.17g", p.first, p.second, v,
              eil::geom::phobos_atan(p.first), eil::geom::phobos_atan(p.second));
    }
}

void v(const char* tag, const Vector3& p) {
    emitf("%s x=%.17g y=%.17g z=%.17g", tag, p.x, p.y, p.z);
}

void steps(const char* tag, Vector3 a, Vector3 b, Vector3 c) {
    const std::string t(tag);
    Vector3 ca = a - c;
    Vector3 cb = b - c;
    number ca_mag = eil::geom::abs(ca);
    number cb_mag = eil::geom::abs(cb);
    emitf("%s ca_mag=%.17g cb_mag=%.17g diff=%.17g", tag, ca_mag, cb_mag,
          std::fabs(ca_mag - cb_mag));
    v((t + ".ca").c_str(), ca);
    v((t + ".cb").c_str(), cb);

    Vector3 tangent1 = Vector3(ca);
    tangent1.normalize();
    v((t + ".t1").c_str(), tangent1);

    Vector3 n = eil::geom::cross(ca, cb);
    v((t + ".cr").c_str(), n);
    emitf("%s nabs=%.17g", tag, eil::geom::abs(n));
    n.normalize();
    v((t + ".n").c_str(), n);

    Vector3 tangent2 = eil::geom::cross(n, tangent1);
    v((t + ".t2").c_str(), tangent2);

    Vector3 cb_local = cb;
    Vector3 zerov = Vector3(0.0, 0.0, 0.0);
    cb_local.transform_to_local_frame(tangent1, tangent2, n, zerov);
    v((t + ".cbl").c_str(), cb_local);

    number theta = eil::geom::phobos_atan2(cb_local.y, cb_local.x);
    number L = theta * cb_mag;
    emitf("%s theta=%.17g L=%.17g", tag, theta, L);

    for (double tt : {0.0, 0.0005, 0.001, 0.5}) {
        number th = theta * tt;
        number cx = eil::cos(th) * cb_mag;
        number sy = eil::sin(th) * cb_mag;
        emitf("%s loc t=%.17g cx=%.17g sy=%.17g", tag, tt, cx, sy);
        Vector3 loc = Vector3(cx, sy, number(0.0));
        loc.transform_to_global_frame(tangent1, tangent2, n, c);
        v((t + ".loc").c_str(), loc);
    }
}

void replay_probe() {
    atan_table();

    Vector3 a = Vector3(eil::Array<number>{2.0, 2.0, 0.0}.all());
    Vector3 b = Vector3(eil::Array<number>{1.0, 2.0, 1.0}.all());
    Vector3 cex = Vector3(eil::Array<number>{1.0, 2.0, 0.0}.all());
    steps("qtr", a, b, cex);

    Arc abc = Arc(a, b, cex);
    Vector3 d = abc(0.5);
    Arc3 adb = Arc3(a, d, b);
    v("thru.a", adb.a);
    v("thru.b", adb.b);
    v("thru.c", adb.c);
    steps("thru", adb.a, adb.b, adb.c);

    steps("thru.c0", a, b, Vector3(adb.c.x, adb.c.y, 0.0));

    Arc3 sh = Arc3(Vector3(0.0, 1.0), Vector3(0.5, 1.2), Vector3(1.0, 1.0));
    steps("shallow", sh.a, sh.b, sh.c);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <probes/geom_gpath_arcsteps.txt>\n";
        return 2;
    }
    replay_probe();
    return report("arcsteps", argv[1], argc > 2);
}
