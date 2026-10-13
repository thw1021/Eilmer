// tests/test_geom_gpath_mp_rot_chain.cpp — C++ mirror of
// probes/geom/gpath/probe_mp_rot_chain.d, replayed line for line against
// baselines/e587b235/probes/geom_gpath_mp_rot_chain.txt.
//
// Why this test exists (reports/backlog.md T-910): geom_gpath_modifiedpath accepted
// 84 of its 2288 lines through an absolute tolerance band, the worst residual being
// 8.88e-10, and the backlog refused to call that verified until the chain was broken
// apart. This test breaks it apart. Vector3::rotate_about_zaxis
// (src/geom/elements/vector3.cpp:312-318, D's vector3.d:521-528) is recomputed one
// operator at a time for exactly the arguments the mp.rot rows pass through it, and
// every intermediate is printed together with its IEEE-754 bit pattern, so the first
// line whose bits differ names the operator and the gap says what it costs.
//
// `bits=` is what makes this a proof rather than a reading. "%.17g" round-trips for
// normal finite values, but two NaNs, or a +0.0/-0.0 pair, can print alike while
// being different numbers.
//
// The corpus is generated twice, with the two candidate arc tangents, and both are
// diffed:
//
//   phobos  — eil::atan2 (substrate/include/eil/math.hpp), the replica of Phobos'
//       atan2Impl!double/atanImpl!double that geom/gpath/arc already required.
//   libm    — std::atan2, what vector3.cpp:313 called before T-910 was worked.
//
// PASS requires the phobos corpus to have zero differing lines. The libm count is
// printed because it is the measurement that identifies the defect; the test does not
// depend on its value.
//
// Regenerate:
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/gpath \
//     probes/geom/gpath/probe_mp_rot_chain.d -of=/tmp/probe_mpc \
//     && /tmp/probe_mpc > baselines/e587b235/probes/geom_gpath_mp_rot_chain.txt
//
// Run: ./test_geom_gpath_mp_rot_chain <baseline.txt>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/arc.hpp"
#include "geom/gpath/modifiedpath.hpp"
#include "geom/gpath/path.hpp"

namespace {

using eil::geom::Arc;
using eil::geom::PathPtr;
using eil::geom::RotatedAboutZAxisPath;
using eil::geom::Vector3;

// Which atan2 the hand-written chain uses. The `wrapper`/`path` rows always go
// through src/geom/elements/vector3.cpp itself, so they are unaffected; they are
// printed in both corpora as the control that the chain stands for the real code.
enum class Atan2_mode { phobos, libm };

Atan2_mode g_mode = Atan2_mode::phobos;

std::vector<std::string> g_lines;

std::string val(double x) {
    char buf[64];
    std::snprintf(buf, sizeof buf, "%.17g", x);
    return buf;
}

std::string bits(double x) {
    uint64_t u;
    std::memcpy(&u, &x, sizeof u);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(u));
    return buf;
}

void emit(const std::string& tag, double x) {
    g_lines.push_back(tag + " v=" + val(x) + " bits=" + bits(x));
}

double chain_atan2(double y, double x) {
    return g_mode == Atan2_mode::libm ? std::atan2(y, x) : eil::atan2(y, x);
}

std::vector<double> t_sweep() {
    return {0.0, 0.0005, 0.001, 0.05, 0.25, 0.5, 0.75, 0.95, 0.999, 0.9995, 1.0};
}

// One line for each observable of probe_mp_rot_chain.d's main().
std::vector<std::string> produce() {
    g_lines.clear();

    Vector3 a(2.0, 2.0, 0.0);
    Vector3 b(1.0, 2.0, 1.0);
    Vector3 c(1.0, 2.0, 0.0);
    PathPtr arc = std::make_shared<Arc>(a, b, c);
    double dtheta = static_cast<double>(EIL_PI / 4.0L);
    emit("ctl.dtheta", dtheta);

    for (double t : t_sweep()) {
        g_lines.push_back("mp.chain.row t=" + val(t));
        Vector3 p = (*arc)(t);
        emit("mp.chain.in.x", eil::re(p.x));
        emit("mp.chain.in.y", eil::re(p.y));
        emit("mp.chain.in.z", eil::re(p.z));

        double theta0 = chain_atan2(eil::re(p.y), eil::re(p.x));
        emit("mp.chain.atan2", theta0);
        double theta = theta0 + dtheta;
        emit("mp.chain.theta", theta);

        double xx = eil::re(p.x) * eil::re(p.x);
        emit("mp.chain.xx", xx);
        double yy = eil::re(p.y) * eil::re(p.y);
        emit("mp.chain.yy", yy);
        double sum = xx + yy;
        emit("mp.chain.sum", sum);
        double r = eil::sqrt(sum);
        emit("mp.chain.r", r);

        double cost = std::cos(theta);
        emit("mp.chain.cos", cost);
        double sint = std::sin(theta);
        emit("mp.chain.sin", sint);
        emit("mp.chain.rxcos", r * cost);
        emit("mp.chain.rysin", r * sint);

        Vector3 q = p;
        q.rotate_about_zaxis(dtheta);
        emit("mp.chain.wrapper.x", eil::re(q.x));
        emit("mp.chain.wrapper.y", eil::re(q.y));
        emit("mp.chain.wrapper.z", eil::re(q.z));

        PathPtr rot = std::make_shared<RotatedAboutZAxisPath>(arc, dtheta);
        Vector3 w = (*rot)(t);
        emit("mp.chain.path.x", eil::re(w.x));
        emit("mp.chain.path.y", eil::re(w.y));
        emit("mp.chain.path.z", eil::re(w.z));
    }

    for (double y : {2.0, 1.9999996915748783, 1.7071067811865475, 1.0015707956808308,
                     0.70710678118654746, 0.5, 0.1, 3.0, 7.7, 0.000123456789}) {
        for (double x : {1.7071067811865475, 2.0, 1.0, 0.25, 1e10, 1e-10,
                         3.141592653589793}) {
            emit("ctl.atan2 " + val(y) + " " + val(x), chain_atan2(y, x));
        }
    }
    for (double th : {0.0, 0.1, 0.7853981633974483, 1.5707963267948966, 1.6522047499950325,
                      2.356194490192345, 3.141592653589793, 4.0, 5.5, 6.283185307179586}) {
        emit("ctl.cos " + val(th), std::cos(th));
        emit("ctl.sin " + val(th), std::sin(th));
    }
    for (double th : {1.6522047499950325, 1.6522047499950327, 1.6522047499950329}) {
        emit("ctl.ulp.cos " + val(th), std::cos(th));
        emit("ctl.ulp.sin " + val(th), std::sin(th));
    }

    const double p05x = 1.7071067811865475, p05y = 2.0;
    const double th05 = chain_atan2(p05y, p05x) + dtheta;
    const double r05 = eil::sqrt(p05x * p05x + p05y * p05y);
    emit("ctl.model.theta", th05);
    emit("ctl.model.r", r05);
    emit("ctl.model.x", r05 * std::cos(th05));
    emit("ctl.model.x.plus1ulp", r05 * std::cos(std::nextafter(th05, 2.0)));
    emit("ctl.model.y", r05 * std::sin(th05));
    emit("ctl.model.y.plus1ulp", r05 * std::sin(std::nextafter(th05, 2.0)));
    g_lines.push_back("ctl.model.dtheta.ulp=" + val(std::nextafter(th05, 2.0) - th05));

    return g_lines;
}

// Lines differing from the baseline, counted per group. `first` receives the first
// differing line of the group for the report.
int diff_count(const std::vector<std::string>& expected, const std::vector<std::string>& got,
               std::string* first, std::string* first_d) {
    int n = 0;
    for (size_t i = 0; i < expected.size() || i < got.size(); ++i) {
        const std::string e = i < expected.size() ? expected[i] : "<none>";
        const std::string g = i < got.size() ? got[i] : "<none>";
        if (e == g) continue;
        if (n == 0 && first) {
            *first = g;
            *first_d = e;
        }
        ++n;
    }
    return n;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }
    std::ifstream in(argv[1]);
    if (!in) {
        std::fprintf(stderr, "cannot open baseline %s\n", argv[1]);
        return 2;
    }
    std::vector<std::string> expected;
    for (std::string l; std::getline(in, l);) expected.push_back(l);

    g_mode = Atan2_mode::phobos;
    const std::vector<std::string> phobos = produce();
    std::string f1, d1;
    const int n_phobos = diff_count(expected, phobos, &f1, &d1);

    g_mode = Atan2_mode::libm;
    const std::vector<std::string> libm = produce();
    std::string f2, d2;
    const int n_libm = diff_count(expected, libm, &f2, &d2);

    std::printf("rows: %zu in the baseline, %zu produced per mode\n", expected.size(),
                phobos.size());
    std::printf("eil::atan2 (Phobos replica): %d line(s) differ\n", n_phobos);
    if (n_phobos) std::printf("  first D:   |%s|\n  first C++: |%s|\n", d1.c_str(), f1.c_str());
    std::printf("std::atan2 (glibc):          %d line(s) differ\n", n_libm);
    if (n_libm) std::printf("  first D:   |%s|\n  first C++: |%s|\n", d2.c_str(), f2.c_str());

    // The wrapper rows are the control: whatever mode the hand-written chain used,
    // vector3.cpp itself is fixed, so its rows must be identical in both corpora.
    int wrapper_mismatch = 0;
    for (size_t i = 0; i < phobos.size() && i < libm.size(); ++i) {
        const std::string& a = phobos[i];
        if (a.rfind("mp.chain.wrapper.", 0) != 0 && a.rfind("mp.chain.path.", 0) != 0) continue;
        if (a != libm[i]) ++wrapper_mismatch;
    }
    std::printf("rotate_about_zaxis rows identical across both corpora: %s\n",
                wrapper_mismatch ? "NO" : "yes");

    const bool ok = (n_phobos == 0 && phobos.size() == expected.size());
    std::printf("%s: geom_gpath_mp_rot_chain, %d of %zu lines bit-identical with eil::atan2\n",
                ok ? "PASS" : "FAIL", ok ? static_cast<int>(phobos.size()) : n_phobos,
                phobos.size());
    return ok ? 0 : 1;
}
