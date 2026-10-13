// tests/test_geom_misc_nurbs_utils.cpp
//
// Two layers, both required (the pattern of tests/test_geom_elements_properties.cpp):
//
//  A. the `unittest` block of geom/misc/nurbs_utils.d (D:130-172) ported
//     assertion-for-assertion, in source order, with D's own tolerance arguments
//     (isClose defaults: 1.0e-8 relative, 0.0 absolute).
//
//  B. a line-for-line replay of probes/geom/misc/probe_nurbs_utils.d against
//     baselines/e587b235/probes/geom_misc_nurbs_utils.txt: 121 lines covering
//     findSpan over five knot vectors (the Piegl example, a single-span clamped
//     cubic, a repeated-knot vector, a clamped quintic and a degree-1 vector) at every
//     knot, every mid-span value and the tol-window near misses; the full basisFuns
//     rows plus the workspace state they leave; autoKnotVector for eight compatible
//     and five incompatible (N,p); PwTest's three sizes; and duplicatePw's copy.
//     Numeric fields are `key=%.17g` so they can be paired positionally with D's.
//
// Run: ./test_geom_misc_nurbs_utils ../baselines/e587b235/probes/geom_misc_nurbs_utils.txt
//
// Regenerate the baseline with ldc2 (see the probe header for why the module and the
// probe are compiled separately and linked):
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   cd /home/tang/packages/Eilmer
//   ldc2 -c -I/home/tang/packages/gdtk/src /home/tang/packages/gdtk/src/geom/misc/nurbs_utils.d -of=/tmp/nurbs_utils.o
//   ldc2 -c -I/home/tang/packages/gdtk/src probes/geom/misc/probe_nurbs_utils.d -of=/tmp/nru_probe.o
//   ldc2 /tmp/nru_probe.o /tmp/nurbs_utils.o -of=/tmp/nru_d
//   /tmp/nru_d > baselines/e587b235/probes/geom_misc_nurbs_utils.txt
#include <array>
#include <cctype>
#include <cstdarg>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <initializer_list>
#include <limits>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "geom/misc/nurbs_utils.hpp"
#include "nm/number.hpp"

using eil::geom::NURBSWorkspace;
using eil::geom::findSpan;
using eil::geom::basisFuns;
using eil::geom::autoKnotVector;
using eil::geom::PwTest;
using eil::geom::duplicatePw;
using eil::geom::PwArray;

namespace {

int failures = 0;
int checks = 0;

void check(bool cond, const char* what, int line) {
    ++checks;
    if (!cond) {
        ++failures;
        std::printf("FAIL line %d: %s\n", line, what);
    }
}

#define CHECK(c, msg) check((c), msg, __LINE__)

std::vector<std::string> produced;

void emitf(const char* fmt, ...) {
    char buf[8192];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    produced.emplace_back(buf);
}

/// Same rule as esc() in the probe: a message that contains a newline is printed with
/// `\n` so one observation stays on one line.
std::string esc(const std::string& s) {
    std::string res;
    for (char c : s) {
        if (c == '\n') res += "\\n";
        else res += c;
    }
    return res;
}

std::vector<double> numbers_in(const std::string& s) {
    std::vector<double> out;
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] != '=') { ++i; continue; }
        size_t b = i + 1;
        size_t e = s.find(' ', b);
        if (e == std::string::npos) e = s.size();
        const std::string tok = s.substr(b, e - b);
        if (!tok.empty()) {
            const char c = tok[0];
            if (c == '-' || c == '+' || std::isdigit(static_cast<unsigned char>(c)) || c == 'n' ||
                c == 'i') {
                char* endp = nullptr;
                const double v = std::strtod(tok.c_str(), &endp);
                if (endp == tok.c_str() + tok.size()) out.push_back(v);
            }
        }
        i = b;
    }
    return out;
}

// The key names of the numeric fields, in the order they appear on a line, so the
// summary can say *which* quantity drifted.
std::vector<std::string> keys_in(const std::string& s) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] != '=') { ++i; continue; }
        size_t b = s.rfind(' ', i);
        if (b == std::string::npos) b = 0; else ++b;
        out.push_back(s.substr(b, i - b));
        size_t e = s.find(' ', i + 1);
        i = (e == std::string::npos) ? s.size() : e;
    }
    return out;
}

long long ulp_gap(double a, double b) {
    int64_t ia, ib;
    std::memcpy(&ia, &a, 8);
    std::memcpy(&ib, &b, 8);
    if (ia < 0) ia = INT64_MIN - ia;
    if (ib < 0) ib = INT64_MIN - ib;
    return ia > ib ? ia - ib : ib - ia;
}

eil::Array<double> arr(std::initializer_list<double> v) {
    eil::Array<double> a;
    for (const double& x : v) a.append(x);
    return a;
}

std::string joined(const eil::Slice<double>& v) {
    std::string row;
    for (size_t i = 0; i < v.length(); ++i) {
        char buf[64];
        std::snprintf(buf, sizeof buf, i == 0 ? "%.17g" : ",%.17g", v[i]);
        row += buf;
    }
    return row;
}

double sum_row(eil::Slice<double> N) {
    double s = 0.0;
    for (size_t i = 0; i < N.length(); ++i) s += N[i];
    return s;
}

// ============================================================================
// A. the D unittest block (D:130-172), assertion for assertion, in D's order.
// ============================================================================
void port_d_unittest() {
    // example 2.3 from Piegl and Tiller (1997) - 'The NURBS Book'
    // findSpan test
    double u = 2.5;
    eil::Array<double> U = arr({0.0, 0.0, 0.0, 1.0, 2.0, 3.0, 4.0, 4.0, 5.0, 5.0, 5.0});
    int m = 11;
    int p = 2;
    int n = m - p - 1;
    int i = findSpan(u, n, p, U.all());
    int iExact = 4;
    CHECK(eil::isClose(i, iExact), "D:140 findSpan example 2.3");

    // basisFuns test
    NURBSWorkspace nws(p);
    eil::Array<double> N;
    N.set_length(p + 1);
    basisFuns(i, u, p, U.all(), N.all(), nws);
    std::array<double, 3> Nexact = {1.0 / 8.0, 3.0 / 4.0, 1.0 / 8.0};
    CHECK(N.length() == Nexact.size(), "D:148 basisFuns row length");
    for (size_t idx = 0; idx < N.length(); ++idx) {
        CHECK(eil::isClose(N[idx], Nexact[idx]), "D:150 basisFuns example 2.3 row");
    }

    // single-span autoKnotVector test
    int p_ss = 3;
    int N_ss = 4;
    eil::Array<double> U_ss = autoKnotVector(N_ss, p_ss);
    std::array<double, 8> UExact_ss = {0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0};
    CHECK(U_ss.length() == UExact_ss.size(), "D:158 autoKnotVector single span length");
    for (size_t idx = 0; idx < U_ss.length(); ++idx) {
        CHECK(eil::isClose(U_ss[idx], UExact_ss[idx]), "D:160 autoKnotVector single span");
    }

    // multi-span autoKnotVector test
    int p_ms = 2;
    int N_ms = 5;
    eil::Array<double> U_ms = autoKnotVector(N_ms, p_ms);
    std::array<double, 8> UExact_ms = {0.0, 0.0, 0.0, 1.0 / 3.0, 2.0 / 3.0, 1.0, 1.0, 1.0};
    CHECK(U_ms.length() == UExact_ms.size(), "D:168 autoKnotVector multi span length");
    for (size_t idx = 0; idx < U_ms.length(); ++idx) {
        CHECK(eil::isClose(U_ms[idx], UExact_ms[idx]), "D:170 autoKnotVector multi span");
    }
}

// ============================================================================
// B. the probe replay. Same vectors, same sweeps, same print order as
//    probes/geom/misc/probe_nurbs_utils.d.
// ============================================================================
void dump_span(const char* tag, const eil::Array<double>& U, int p,
               std::initializer_list<double> us) {
    int n = static_cast<int>(U.length()) - p - 1;
    emitf("vec %s p=%d n=%d m=%d U=%s", tag, p, n, static_cast<int>(U.length()) - 1,
          eil::to_string(U).c_str());
    for (double u : us) {
        try {
            int i = findSpan(u, n, p, U.all());
            emitf("span %s u=%.17g i=%d", tag, u, i);
        } catch (const eil::ExceptionBase& e) {
            emitf("span %s u=%.17g EXC isError=0 isException=1 msg=[%s]", tag, u,
                  esc(e.msg()).c_str());
        } catch (const eil::ErrorBase& e) {
            emitf("span %s u=%.17g EXC isError=1 isException=0 msg=[%s]", tag, u,
                  esc(e.msg()).c_str());
        }
    }
}

void dump_basis(const char* tag, const eil::Array<double>& U, int p,
                std::initializer_list<double> us) {
    int n = static_cast<int>(U.length()) - p - 1;
    for (double u : us) {
        int i = findSpan(u, n, p, U.all());
        NURBSWorkspace nws(p);
        eil::Array<double> N;
        N.set_length(p + 1);
        basisFuns(i, u, p, U.all(), N.all(), nws);
        const std::string row = joined(N.all());
        const std::string lefts = joined(nws.left.all());
        const std::string rights = joined(nws.right.all());
        emitf("basis %s u=%.17g i=%d N=[%s] sum=%.17g left=[%s] right=[%s]", tag, u, i,
              row.c_str(), sum_row(N.all()), lefts.c_str(), rights.c_str());
    }
}

void replay_probe() {
    using namespace eil::geom;

    // ---- findSpan --------------------------------------------------------
    eil::Array<double> Uex =
        arr({0.0, 0.0, 0.0, 1.0, 2.0, 3.0, 4.0, 4.0, 5.0, 5.0, 5.0});
    dump_span("ex", Uex, 2, {0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 5.0, 1.0e-13,
                             -1.0e-13, 5.0 + 1.0e-13, 5.0 - 1.0e-13, 1.0e-6, -1.0e-6,
                             5.0 + 1.0e-6, 4.9999999999999, 2.5000000000001});

    eil::Array<double> Ucub = arr({0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0});
    dump_span("cubic1span", Ucub, 3, {0.0, 0.25, 0.5, 0.75, 1.0, 1.0e-13, -1.0e-13});

    eil::Array<double> Urep = arr({0.0, 0.0, 0.0, 1.0, 1.0, 2.0, 2.0, 3.0, 3.0, 3.0});
    dump_span("repeated", Urep, 2, {0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0});

    eil::Array<double> Uq = arr({0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 5.0, 5.0,
                                 5.0, 5.0, 5.0});
    dump_span("quintic", Uq, 5, {0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 5.0});

    eil::Array<double> Ulin = arr({0.0, 0.0, 0.25, 0.5, 0.75, 1.0, 1.0});
    dump_span("linear", Ulin, 1, {0.0, 0.1, 0.25, 0.3, 0.5, 0.75, 1.0});

    // u exactly equal to U[n+1]: the early `return n` branch (nurbs_utils.d:37).
    emitf("early u=%.17g", Uex[9]);
    emitf("early u=%.17g", Ucub[4]);
    emitf("early u=%.17g", Urep[7]);

    // ---- basisFuns -------------------------------------------------------
    dump_basis("ex", Uex, 2, {0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 5.0});
    dump_basis("cubic1span", Ucub, 3, {0.0, 0.25, 0.5, 0.75, 1.0});
    dump_basis("repeated", Urep, 2, {0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0});
    dump_basis("linear", Ulin, 1, {0.0, 0.1, 0.25, 0.5, 0.75, 1.0});
    {
        NURBSWorkspace nws(2);
        eil::Array<double> N;
        N.set_length(3);
        basisFuns(4, 2.5, 2, Uex.all(), N.all(), nws);
        emitf("basis unittest row=%.17g,%.17g,%.17g len=%d", N[0], N[1], N[2],
              static_cast<int>(N.length()));
    }

    // ---- NURBSWorkspace --------------------------------------------------
    for (int p : {1, 2, 3, 5}) {
        NURBSWorkspace w(p);
        emitf("ws p=%d left=%s right=%s", p, eil::to_string(w.left).c_str(),
              eil::to_string(w.right).c_str());
    }

    // ---- autoKnotVector --------------------------------------------------
    const int combos[8][2] = {{4, 3}, {5, 2}, {4, 1}, {2, 1}, {7, 3}, {10, 5}, {3, 2}, {6, 4}};
    for (const auto& c : combos) {
        const int N = c[0];
        const int p = c[1];
        eil::Array<double> U = autoKnotVector(N, p);
        emitf("knots N=%d p=%d len=%d U=[%s]", N, p, static_cast<int>(U.length()),
              joined(U.all()).c_str());
    }
    const int bad[5][2] = {{4, 4}, {4, 0}, {2, 5}, {1, 1}, {4, -1}};
    for (const auto& c : bad) {
        try {
            eil::Array<double> U = autoKnotVector(c[0], c[1]);
            emitf("knots N=%d p=%d built=1 len=%d", c[0], c[1], static_cast<int>(U.length()));
        } catch (const eil::ErrorBase& e) {
            emitf("knots N=%d p=%d built=0 isError=1 msg=[%s]", c[0], c[1], esc(e.msg()).c_str());
        } catch (const eil::ExceptionBase& e) {
            emitf("knots N=%d p=%d built=0 isError=0 msg=[%s]", c[0], c[1], esc(e.msg()).c_str());
        }
    }

    // ---- PwTest ----------------------------------------------------------
    {
        eil::Array<std::array<double, 4>> pts;
        pts.append(std::array<double, 4>{{0.0, 0.0, 0.0, 1.0}});
        pts.append(std::array<double, 4>{{1.0, 0.0, 0.0, 1.0}});
        pts.append(std::array<double, 4>{{2.0, 1.0, 0.5, 1.0}});
        try {
            PwTest(pts.all());
            emitf("pwtest 3 built=1");
        } catch (const eil::ErrorBase& e) {
            emitf("pwtest 3 built=0 isError=1 msg=[%s]", esc(e.msg()).c_str());
        } catch (const eil::ExceptionBase& e) {
            emitf("pwtest 3 built=0 isError=0 msg=[%s]", esc(e.msg()).c_str());
        }

        eil::Array<std::array<double, 4>> one;
        one.append(std::array<double, 4>{{0.0, 0.0, 0.0, 1.0}});
        try {
            PwTest(one.all());
            emitf("pwtest 1 built=1");
        } catch (const eil::ErrorBase& e) {
            emitf("pwtest 1 built=0 isError=1 msg=[%s]", esc(e.msg()).c_str());
        } catch (const eil::ExceptionBase& e) {
            emitf("pwtest 1 built=0 isError=0 msg=[%s]", esc(e.msg()).c_str());
        }

        eil::Array<std::array<double, 4>> none;
        try {
            PwTest(none.all());
            emitf("pwtest 0 built=1");
        } catch (const eil::ErrorBase& e) {
            emitf("pwtest 0 built=0 isError=1 msg=[%s]", esc(e.msg()).c_str());
        } catch (const eil::ExceptionBase& e) {
            emitf("pwtest 0 built=0 isError=0 msg=[%s]", esc(e.msg()).c_str());
        }
    }

    // ---- duplicatePw -----------------------------------------------------
    {
        PwArray pw;
        pw.set_length(2);
        int c = 0;
        for (size_t i = 0; i < 2; ++i) {
            pw[i].set_length(2);
            for (size_t j = 0; j < 2; ++j) {
                pw[i][j].set_length(2);
                for (size_t k = 0; k < 2; ++k) {
                    pw[i][j][k] = std::array<double, 4>{{static_cast<eil::number>(c),
                                           static_cast<eil::number>(c) + 0.5,
                                           static_cast<eil::number>(c) - 0.25, 1.0}};
                    ++c;
                }
            }
        }
        PwArray cp = duplicatePw(pw);
        emitf("dup dims %d %d %d", static_cast<int>(cp.length()), static_cast<int>(cp[0].length()),
              static_cast<int>(cp[0][0].length()));
        for (size_t i = 0; i < 2; ++i)
            for (size_t j = 0; j < 2; ++j)
                for (size_t k = 0; k < 2; ++k) {
                    emitf("dup orig[%d][%d][%d]=%.17g,%.17g,%.17g,%.17g copy=%.17g,%.17g,%.17g,%.17g "
                          "same=%s",
                          static_cast<int>(i), static_cast<int>(j), static_cast<int>(k),
                          pw[i][j][k][0], pw[i][j][k][1], pw[i][j][k][2], pw[i][j][k][3],
                          cp[i][j][k][0], cp[i][j][k][1], cp[i][j][k][2], cp[i][j][k][3],
                          (cp[i][j][k] == pw[i][j][k]) ? "true" : "false");
                }
        // The copy is independent: mutate it and re-read the original.
        cp[0][0][0][0] = -99.0;
        emitf("dup afterMutate orig=%.17g copy=%.17g", pw[0][0][0][0], cp[0][0][0][0]);
        PwArray empty;
        PwArray ce = duplicatePw(empty);
        emitf("dup empty len=%d", static_cast<int>(ce.length()));
    }

    emitf("done=1");
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <probes/geom_misc_nurbs_utils.txt>\n";
        return 2;
    }

    port_d_unittest();
    replay_probe();

    std::ifstream in(argv[1]);
    if (!in) {
        std::cerr << "cannot open baseline " << argv[1] << "\n";
        return 2;
    }
    std::vector<std::string> expected;
    for (std::string line; std::getline(in, line);) expected.push_back(line);

    int line_diffs = 0;
    size_t quantities = 0, identical = 0;
    long long worst_ulp = 0;
    std::string worst_where;
    double worst_d = 0.0, worst_cpp = 0.0, worst_resid = 0.0;
    for (size_t i = 0; i < expected.size() || i < produced.size(); ++i) {
        if (i >= expected.size() || i >= produced.size() || expected[i] != produced[i]) {
            if (line_diffs < 20) {
                std::cout << "line " << (i + 1) << "\n  D:   |"
                          << (i < expected.size() ? expected[i] : std::string("<none>"))
                          << "|\n  C++: |"
                          << (i < produced.size() ? produced[i] : std::string("<none>")) << "|\n";
            }
            ++line_diffs;
        }
        if (i < expected.size() && i < produced.size()) {
            const std::vector<double> a = numbers_in(expected[i]);
            const std::vector<double> b = numbers_in(produced[i]);
            const std::vector<std::string> k = keys_in(expected[i]);
            const std::string tag = expected[i].substr(0, expected[i].find(' '));
            for (size_t j = 0; j < a.size() && j < b.size(); ++j) {
                ++quantities;
                const bool same_bits = std::memcmp(&a[j], &b[j], 8) == 0;
                if (same_bits) {
                    ++identical;
                } else {
                    const long long gap = ulp_gap(a[j], b[j]);
                    const double resid = std::fabs(a[j] - b[j]);
                    if (gap > worst_ulp) {
                        worst_ulp = gap;
                        worst_resid = resid;
                        worst_d = a[j];
                        worst_cpp = b[j];
                        worst_where = tag + " " + (j < k.size() ? k[j] : std::string("?"));
                    }
                }
            }
        }
    }
    std::cout << (line_diffs == 0 && failures == 0 ? "PASS" : "FAIL") << ": nurbs_utils, "
              << checks - failures << "/" << checks << " unittest assertions hold; probe replay "
              << expected.size() << " D lines, " << produced.size() << " C++ lines, " << line_diffs
              << " differ; " << identical << " of " << quantities << " quantities bit-identical";
    if (worst_ulp > 0) {
        std::cout << ", worst residual " << worst_resid << " (" << worst_ulp
                  << " ULP) on quantity " << worst_where << " (D " << worst_d << " vs C++ "
                  << worst_cpp << ")";
    } else {
        std::cout << ", worst residual 0 (0 ULP) on quantity none";
    }
    std::cout << "\n";
    return (line_diffs == 0 && failures == 0) ? 0 : 1;
}
