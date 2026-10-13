// tests/test_format_corpus.cpp
//
// Substrate conformance test: reimplements tools/fmt_probe.d's output line for
// line and diffs it against the D-captured corpus. Any divergence is a place
// where a ported `writefln`, `to!string` or `isClose` would print or decide
// differently from the reference solver.
//
// Run: ./test_format_corpus ../tools/fmt_corpus.txt

#include <cmath>
#include <cstdio>
#include <array>
#include <cstdarg>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

#include "eil/aa.hpp"
#include "eil/array.hpp"
#include "eil/conv.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/math.hpp"
#include "eil/number.hpp"

namespace {

std::vector<std::string> produced;

void emit(const std::string& line) { produced.push_back(line); }
// The exact same tables as the D probe. Duplicating them is deliberate: the
// corpus is only meaningful if both sides feed identical inputs.
void section_round_trip() {
    const std::vector<double> rt = {
        0.0, -0.0, 1.0, -1.0, 0.5, 3.14, 3.141592653589793,
        1.0e-5, 1.0e-4, 1.0e15, 1.0e16, 1.0e17, 1.2345678901234567e-8,
        1.0 / 3.0, 2.0 / 3.0, 1e-300, std::nextafter(0.0, 1.0), 1.7976931348623157e308,
        std::numeric_limits<double>::max(), std::numeric_limits<double>::min(),
    };
    for (double v : rt) emit("rt " + eil::to_string(v));

    const std::vector<long> ints = {0, 1, -1, 2147483647L, -2147483648L, 4294967295L};
    for (long v : ints) emit("int " + eil::to_string(v));
    emit("bool true");
    emit("bool false");
}

void section_specs() {
    const std::vector<std::string> specs = {"%g", "%.6f", "%.3f", "%e", "%14.6e", "%8.3f",
                                            "%.0e", "%.10e", "%20.12f", "%x", "%d", "%s", "%%"};
    const std::vector<double> vals = {
        0.0, -0.0, 1.0, 3.141592653589793, 1.0e-7, 1.23456789e12,
        9.9999999999, 0.0001, 0.00001, 1.0 / 3.0, -2.5, 1e300,
        std::numeric_limits<double>::infinity(), -std::numeric_limits<double>::infinity(),
        std::numeric_limits<double>::quiet_NaN(), 12345.0, -12345.0,
        1e-13, 4.0,
    };
    for (const std::string& sp : specs) {
        for (double v : vals) {
            char hex[64];
            std::snprintf(hex, sizeof hex, "%a", v);
            // The probe emits `|value|` on success and bare `ERR msg` on a
            // format exception; mirroring it keeps the diff meaningful.
            std::string tail;
            try {
                tail = "|" + eil::format(sp, v) + "|";
            } catch (const std::exception& e) {
                tail = "ERR " + std::string(e.what());
            }
            emit(std::string("spec ") + sp + " " + hex + " => " + tail);
        }
    }
    const std::vector<std::string> ispecs = {"%d", "%5d", "%s", "%x", "%g"};
    const std::vector<int> ivals = {0, 7, -7, 1000000};
    for (const std::string& sp : ispecs) {
        for (int v : ivals) {
            std::string tail;
            try {
                tail = "|" + eil::format(sp, v) + "|";
            } catch (const std::exception& e) {
                tail = "ERR " + std::string(e.what());
            }
            emit(std::string("ispec ") + sp + " " + eil::to_string(v) + " => " + tail);
        }
    }
}

void section_arrays() {
    // D printed the same line twice: once through to!string, once through
    // writeln's own array rendering. Both must agree.
    eil::Array<double> a({1.0, 2.5, -3.0});
    emit("arr " + eil::to_string(a));
    emit("arr " + eil::to_string(a));
    eil::Array<double> e;
    emit("arr " + eil::to_string(e));
    eil::Array<double> one({0.0});
    emit("arr " + eil::to_string(one));
    eil::Array<int> ia({1, 2, 3});
    emit("arr " + eil::to_string(ia));
}

void section_is_close() {
    struct Case { double x, y, rel, absd; };
    const std::vector<std::array<double, 2>> pairs = {
        {0.0, 0.0}, {0.0, -0.0}, {1.0, 1.0}, {1.0, 1.0 + 1e-16},
        {1.0, 1.0 + 1e-14}, {1.0, 1.001}, {1.0, 1.01}, {1.0, 1.02},
        {1.0, 1.1}, {-1.0, 1.0}, {0.0, 1e-6}, {0.0, 1e-4},
        {1e-300, 1.00001e-300}, {1e100, 1.001e100},
        {100.0, 100.9}, {100.0, 101.0}, {100.0, 100.99999},
        {std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity()},
        {std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::quiet_NaN()},
        {std::numeric_limits<double>::quiet_NaN(), 1.0}, {1.0, std::numeric_limits<double>::quiet_NaN()},
        {std::numeric_limits<double>::max(), std::numeric_limits<double>::max()},
        {0.0, std::numeric_limits<double>::min()},
        {1.0e-5, 0.0}, {1.0e-3, 0.0},
    };
    const std::vector<double> rels = {1.0e-2, 1.0e-8, 0.0, 1.0};
    const std::vector<double> abss = {1.0e-5, 0.0, 1.0e-9, 1.0};
    char buf[256];
    for (const auto& p : pairs) {
        for (double r : rels) {
            for (double ad : abss) {
                std::snprintf(buf, sizeof buf, "isclose %.17g %.17g %.17g %.17g => %s", p[0], p[1],
                              r, ad, eil::isClose(p[0], p[1], r, ad) ? "true" : "false");
                emit(buf);
            }
        }
    }
    auto tf = [](bool b) { return b ? "true" : "false"; };
    emit(std::string("bound_abs_y_nonzero ") + tf(eil::isClose(1.0, 1.00001, 0.0, 1e-5 / 1.0)));
    emit(std::string("bound_rel_y_nonzero ") + tf(eil::isClose(2.0, 1.0, 1.0, 0.0)));
    emit(std::string("bound_rel_y_nonzero_lt ") + tf(eil::isClose(2.0, 1.0, 0.9999999999, 0.0)));
    emit(std::string("bound_abs_eq ") + tf(eil::isClose(3.0, 2.0, 0.0, 1.0)));
    emit(std::string("bound_abs_lt ") + tf(eil::isClose(3.0, 2.0, 0.0, 0.9999999999)));
    emit(std::string("bound_y0_abs_eq ") + tf(eil::isClose(1e-5, 0.0, 0.0, 1e-5)));
    emit(std::string("bound_y0_rel_eq ") + tf(eil::isClose(1e-5, 0.0, 1.0, 0.0)));
    emit(std::string("bound_y0_rel_big ") + tf(eil::isClose(1e-5, 0.0, 2.0, 0.0)));
    emit(std::string("bound_y0_rel_small ") + tf(eil::isClose(1e-5, 0.0, 0.5, 0.0)));
    emit(std::string("bound_y0_zero_x ") + tf(eil::isClose(0.0, 0.0, 0.0, 0.0)));
    emit(std::string("bound_symmetric ") + tf(eil::isClose(1.0, 2.0, 1.0, 0.0)) + " " +
         tf(eil::isClose(2.0, 1.0, 1.0, 0.0)));

    for (const auto& p : pairs) {
        std::snprintf(buf, sizeof buf, "isclose_dflt %.17g %.17g => %s", p[0], p[1],
                      eil::isClose(p[0], p[1]) ? "true" : "false");
        emit(buf);
    }
}

void section_errors() {
    try {
        eil::enforce(false, "enforce with %s and %d", "a string", 42);
        emit("enforce <no throw>");
    } catch (const std::exception& e) {
        emit(std::string("enforce ") + e.what());
    }
    try {
        eil::to_double("not a number");
        emit("conv_err <no throw>");
    } catch (const std::exception& e) {
        emit(std::string("conv_err ") + e.what());
    }
    try {
        eil::Array<int> s({1, 2});
        const int v = s[5];
        emit("range_err <no throw> " + eil::to_string(v));
    } catch (const std::exception& e) {
        emit(std::string("range_err ") + e.what());
    }
    try {
        eil::Array<int> m(3);
        m[10] = 1;
        emit("bounds <no throw>");
    } catch (const std::exception& e) {
        emit(std::string("bounds ") + e.what());
    }
    // Arguments must not be literals: folding sqrt(-1.0) at compile time yields a
    // differently signed NaN than the sqrtsd instruction D actually executes.
    volatile double neg1 = -1.0;
    volatile double zero = 0.0;
    const double s1 = eil::sqrt(neg1);
    const double l1 = eil::log(neg1);
    emit("nan_math " + eil::to_string(s1) + " " + eil::to_string(l1) + " " +
         eil::to_string(zero / zero));
    emit("div_zero " + eil::to_string(1.0 / 0.0) + " " + eil::to_string(-1.0 / 0.0));
}

void section_aa() {
    eil::AA<std::string, int> aa;
    aa.set("alpha", 1);
    aa.set("beta", 2);
    emit("aa_len " + eil::to_string(aa.length()));
    emit("aa_get " + eil::to_string(aa.get("alpha")));
    emit(std::string("aa_has ") + (aa.contains("alpha") ? "true" : "false") + " " +
         (aa.contains("gamma") ? "true" : "false"));
    // std::map order is sorted, matching the probe's explicit sort.
    std::vector<std::string> ks;
    for (const std::string& k : aa.keys()) ks.push_back(k);
    emit("aa_keys " + eil::to_string(ks));
    std::vector<int> vs;
    for (int v : aa.values()) vs.push_back(v);
    emit("aa_values " + eil::to_string(vs));
    try {
        int missing = aa.get("gamma");
        emit("aa_missing <no throw> " + eil::to_string(missing));
    } catch (const std::exception& e) {
        emit(std::string("aa_missing ") + e.what());
    }
    eil::AA<int, std::string> rev;
    rev.set(7, "seven");
    emit("aa_int_key " + rev.get(7));
    eil::AA<std::size_t, eil::AA<std::string, std::string>> nested;
    nested.get_or_default(3).set("tag", "value");
    emit("aa_nested " + nested.get(3).get("tag"));
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <path to fmt_corpus.txt>\n";
        return 2;
    }
    section_round_trip();
    section_specs();
    section_arrays();
    section_is_close();
    section_errors();
    section_aa();

    std::ifstream in(argv[1]);
    if (!in) {
        std::cerr << "cannot open corpus " << argv[1] << "\n";
        return 2;
    }
    std::vector<std::string> expected;
    for (std::string line; std::getline(in, line);) expected.push_back(line);

    if (expected.size() != produced.size()) {
        std::cerr << "line count differs: D corpus " << expected.size() << ", C++ produced "
                  << produced.size() << "\n";
    }
    int mismatches = 0;
    const size_t n = std::min(expected.size(), produced.size());
    for (size_t i = 0; i < n; ++i) {
        if (expected[i] != produced[i]) {
            if (mismatches < 25) {
                std::cout << "corpus:" << (i + 1) << " D  |" << expected[i] << "|\n"
                          << "             C++ |" << produced[i] << "|\n";
            }
            ++mismatches;
        }
    }
    const size_t tail = expected.size() > n ? expected.size() - n : 0;
    std::cout << (mismatches + tail == 0 ? "PASS" : "FAIL")
              << ": format corpus, " << n << " lines compared, " << mismatches
              << " mismatched, " << tail << " unmatched\n";
    return (mismatches + tail) == 0 ? 0 : 1;
}
