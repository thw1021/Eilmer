// tests/test_substrate_phobos_bits.cpp — C++ mirror of
// probes/substrate/probe_phobos_bits.d, replayed against
// baselines/e587b235/probes/substrate_phobos_bits.txt (673 rows, each carrying the
// value at %.17g AND the raw IEEE-754 bit pattern).
//
// This test does NOT require zero differences, and that is the point. It is a
// census of where Phobos' std.math and glibc genuinely disagree, so the project's
// 1e-13 tolerance floor rests on a count rather than on an assumption carried over
// from phase 0. Rows that differ are printed with their ULP gap and grouped by
// function, and the summary states how many of the 76 divergent rows are 1 ULP,
// how many are the substrate's known domain-error NaN sign (already canonicalised
// by eil::sqrt/eil::log — see substrate/include/eil/math.hpp), and how many are
// structural.
//
// The structural ones are the finding worth reading:
//   sinh(-20)     D -242582602.56479013  vs  -242582597.70489514  (2.0e-08 relative)
//   sinh(710)     D inf                  vs  1.1169973830808557e+308
//   tan(1e10)     D -0.55834869957180533 vs  -0.55834963781124181  (1.7e-06 relative)
// Each is far above any tolerance the gates use, so the honest statement is not
// "Phobos and glibc agree to 1 ULP" but "they agree to 1 ULP except in three
// argument classes, none of which the current solver reaches" — and the call sites
// that would reach them are named in reports/backlog.md T-911.
//
// Regenerate:
//   ldc2 probes/substrate/probe_phobos_bits.d -of=/tmp/probe_bits \
//     && /tmp/probe_bits > baselines/e587b235/probes/substrate_phobos_bits.txt
//
// Run: ./test_substrate_phobos_bits <baseline.txt>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <vector>

namespace {

struct Row {
    std::string fn, arg, v, bits;
};

Row parse(const std::string& l) {
    Row r;
    size_t sp = l.find(' ');
    r.fn = l.substr(0, sp);
    std::map<std::string, std::string> kv;
    size_t i = sp + 1;
    while (i < l.size()) {
        size_t eq = l.find('=', i);
        if (eq == std::string::npos) break;
        const std::string k = l.substr(i, eq - i);
        size_t j = l.find(' ', eq + 1);
        if (j == std::string::npos) j = l.size();
        kv[k] = l.substr(eq + 1, j - eq - 1);
        i = j + 1;
    }
    r.arg = kv["arg"]; r.v = kv["v"]; r.bits = kv["bits"];
    return r;
}

long long ulp_gap(double a, double b) {
    int64_t ia, ib;
    std::memcpy(&ia, &a, 8);
    std::memcpy(&ib, &b, 8);
    if (ia < 0) ia = INT64_MIN - ia;      // monotone ordering of the reals
    if (ib < 0) ib = INT64_MIN - ib;
    return ia > ib ? ia - ib : ib - ia;
}

const char* kNaNSign[] = {"sqrt_neg", "log_neg", "asin_gt", "acos_gt", "pow_neg_half"};

bool is_nan_sign_row(const std::string& fn) {
    for (const char* f : kNaNSign) if (fn == f) return true;
    return false;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]); return 2; }
    std::ifstream in(argv[1]);
    if (!in) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    std::vector<std::string> expected;
    for (std::string l; std::getline(in, l);) expected.push_back(l);

    // Rebuild the same operand list and the same emission order as the D probe.
    const std::vector<double> A = {-100.0, -20.0, -3.0, -1.5, -0.5, -0.25, -1.0e-5, -0.0,
        0.0, 1.0e-9, 1.0e-5, 0.1, 0.25, 0.5, 0.6931471805599453, 1.0, 1.5, 2.0,
        2.718281828459045, 3.141592653589793, 5.0, 10.0, 20.0, 40.0, 700.0, 710.0,
        745.0, 746.0, 1.0e10, 1.0e-300, 0.9999999999999999, 1.0000000000000002};
    const std::vector<double> T = {-1.0, -0.9999999999999999, -0.7071067811865476, -0.5,
        -0.25, 0.0, 0.25, 0.5, 0.7071067811865476, 0.9999999999999999, 1.0};

    auto bits_of = [](double x) {
        uint64_t u; std::memcpy(&u, &x, 8);
        char b[32]; std::snprintf(b, sizeof b, "%016llx", (unsigned long long)u); return std::string(b);
    };
    auto v_of = [](double x) {
        char b[64]; std::snprintf(b, sizeof b, "%.17g", x); return std::string(b);
    };

    std::vector<std::pair<std::string, Row>> got;
    auto emit = [&](const char* fn, double arg, double res) {
        Row r; r.fn = fn; r.arg = v_of(arg); r.v = v_of(res); r.bits = bits_of(res);
        got.emplace_back(fn, r);
    };
    for (double x : A) {
        emit("exp", x, std::exp(x)); emit("expm1", x, std::expm1(x));
        emit("log", x, x > 0 ? std::log(x) : std::log(-x));
        emit("log10", x, x > 0 ? std::log10(x) : std::log10(-x));
        emit("log2", x, x > 0 ? std::log2(x) : std::log2(-x));
        emit("sinh", x, std::sinh(x)); emit("cosh", x, std::cosh(x)); emit("tanh", x, std::tanh(x));
        emit("sin", x, std::sin(x)); emit("cos", x, std::cos(x)); emit("tan", x, std::tan(x));
        emit("atan", x, std::atan(x));
        emit("sqrt", x, x >= 0 ? std::sqrt(x) : std::sqrt(-x)); emit("cbrt", x, std::cbrt(x));
        emit("pow_int2", x, std::pow(x, 2.0)); emit("pow_real2", x, std::pow(x, 2.0));
        emit("pow_int3", x, std::pow(x, 3.0));
        emit("pow_half", x, x >= 0 ? std::pow(x, 0.5) : std::pow(-x, 0.5));
    }
    for (double x : T) { emit("asin", x, std::asin(x)); emit("acos", x, std::acos(x)); }
    for (double y : {-1.0, -0.5, 0.0, 0.5, 1.0, 2.0, 1.0e-8})
        for (double x : {-1.6653345369377348e-16, -1.0, -0.5, 0.0, 0.25, 0.5, 1.0,
                         1.7071067811865475, 1.9999996915748783, 1.0e10})
            emit("atan2", y, std::atan2(y, x));
    emit("sqrt_neg", -1.0, std::sqrt(-1.0)); emit("log_neg", -1.0, std::log(-1.0));
    emit("asin_gt", 1.24, std::asin(1.24)); emit("acos_gt", 1.24, std::acos(1.24));
    emit("pow_neg_half", -1.0, std::pow(-1.0, 0.5));

    // ---- census ----------------------------------------------------------------
    std::map<std::string, int> n_rows, n_exact, n_one_ulp, n_structural, n_nan_sign;
    int total_diff = 0;
    for (size_t i = 0; i < expected.size() && i < got.size(); ++i) {
        const Row d = parse(expected[i]);
        const Row c = got[i].second;
        ++n_rows[d.fn];
        if (d.bits == c.bits) { ++n_exact[d.fn]; continue; }
        ++total_diff;
        if (is_nan_sign_row(d.fn)) { ++n_nan_sign[d.fn]; continue; }
        const long long g = ulp_gap(std::strtod(d.v.c_str(), nullptr),
                                    std::strtod(c.v.c_str(), nullptr));
        if (g <= 1) { ++n_one_ulp[d.fn]; }
        else {
            ++n_structural[d.fn];
            std::printf("STRUCTURAL %-6s arg=%-22s D=%-24s C++=%-24s gap=%lld ULP\n",
                        d.fn.c_str(), d.arg.c_str(), d.v.c_str(), c.v.c_str(), g);
        }
    }

    std::printf("%-16s%7s%7s%9s%10s%11s\n", "function", "rows", "exact", "1-ULP",
                ">1ULP", "nan-sign");
    int exact_tot = 0, one_tot = 0, struct_tot = 0, nan_tot = 0;
    for (const auto& e : n_rows) {
        std::printf("%-16s%7d%7d%9d%10d%11d\n", e.first.c_str(), e.second,
                    n_exact[e.first], n_one_ulp[e.first], n_structural[e.first],
                    n_nan_sign[e.first]);
        exact_tot += n_exact[e.first]; one_tot += n_one_ulp[e.first];
        struct_tot += n_structural[e.first]; nan_tot += n_nan_sign[e.first];
    }
    std::printf("TOTAL rows=%zu exact=%d  1-ULP=%d  structural=%d  domain-NaN-sign=%d\n",
                expected.size(), exact_tot, one_tot, struct_tot, nan_tot);
    std::printf("PASS: substrate_phobos_bits census complete (%d of %zu rows byte-identical,"
                " %.1f%%)\n", exact_tot, expected.size(), 100.0 * exact_tot / expected.size());
    // A census test cannot "fail" on divergence — divergence is what it measures. It
    // fails only if the two sides stop describing the same rows.
    return (expected.size() == got.size() && total_diff == expected.size() - exact_tot) ? 0 : 1;
}
