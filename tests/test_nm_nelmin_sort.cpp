// tests/test_nm_nelmin_sort.cpp — replay of probes/nm/probe_nelmin_sort.d against
// baselines/e587b235/probes/nm_nelmin_sort.txt.
//
// What it pins: the permutation D's `vertices.sort!(less)` leaves behind it when
// several simplex vertices have exactly the same fx. nelmin.d's testFunction1 is
// symmetric in the coordinates, so its initial simplex has four vertices with
// fx == 3.01, and which vertex the sort calls "worst" changes the whole
// trajectory: with std::sort instead of the ported path, three of the probe's
// sixteen minimise cases come out with different coordinates, or with a different
// evaluation count (nfe 120 where D reports 119).
//
// The patterns are generated identically in both languages: exhaustive over
// 3-level fx vectors for n = 2..6, then 100 LCG patterns per n from 7 to 64. That
// spans both of Phobos' regimes -- sorting.d's quickSortImpl goes straight to
// shortSort at or below shortSortGetsBetter = max(32, 1024/Elem.sizeof) = 42 for
// Vertex!double, and partitions above it -- so the heapsort/introsort arms and the
// short-range networks are all covered.
//
// Run: ./test_nm_nelmin_sort ../baselines/e587b235/probes/nm_nelmin_sort.txt

#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "eil/array.hpp"
#include "eil/format.hpp"
#include "nm/nelmin.hpp"

namespace {

std::vector<std::string> produced;

std::string fmt(const char* f, ...) {
    char buf[65536];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return std::string(buf);
}

using Vertex = eil::nm::nelmin::Vertex<double>;

// The probe's permutation(): the payload of each vertex is its original index.
std::string permutation(const std::vector<double>& fx) {
    eil::Array<Vertex> v;
    for (size_t i = 0; i < fx.size(); ++i) {
        eil::Array<double> x;
        x.append(static_cast<double>(i));   // the index travels in x[0], as in the probe
        v.append(Vertex{x, fx[i]});
    }
    eil::nm::nelmin::sort_detail::sort_vertices(v);
    std::string s;
    for (size_t k = 0; k < v.length(); ++k) {
        if (k) s += ",";
        s += eil::to_string(static_cast<int>(v[k].x[0]));
    }
    return s;
}

void emit_row(int n, const char* src, const std::vector<double>& fx) {
    eil::Array<double> a;
    for (double v : fx) a.append(v);
    produced.push_back(fmt("perm n=%d src=%s fx=%s %s", n, src,
                           eil::to_string(a).c_str(), permutation(fx).c_str()));
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::printf("usage: %s <probes/nm_nelmin_sort.txt>\n", argv[0]);
        return 2;
    }

    // Exhaustive: every fx pattern over {0,1,2} for n = 2 .. 6.
    for (int n = 2; n <= 6; ++n) {
        int total = 1;
        for (int i = 0; i < n; ++i) total *= 3;
        for (int code = 0; code < total; ++code) {
            std::vector<double> fx;
            int c = code;
            for (int i = 0; i < n; ++i) { fx.push_back(static_cast<double>(c % 3)); c /= 3; }
            emit_row(n, "exhaustive", fx);
        }
    }

    // Sampled: 100 LCG patterns per n from 7 to 64.
    for (int n = 7; n <= 64; ++n) {
        uint32_t seed = 20220725u + static_cast<uint32_t>(n);
        for (int t = 0; t < 100; ++t) {
            std::vector<double> fx;
            for (int i = 0; i < n; ++i) {
                seed = seed * 1103515245u + 12345u;
                fx.push_back(static_cast<double>((seed >> 16) % static_cast<uint32_t>(n < 5 ? n : 5)));
            }
            emit_row(n, "lcg", fx);
        }
    }

    std::ifstream in(argv[1]);
    if (!in) {
        std::printf("cannot open corpus %s\n", argv[1]);
        return 2;
    }
    std::vector<std::string> expected;
    for (std::string line; std::getline(in, line);) expected.push_back(line);

    long diffs = 0;
    const size_t rows = std::max(expected.size(), produced.size());
    for (size_t i = 0; i < rows; ++i) {
        if (i >= expected.size() || i >= produced.size() || expected[i] != produced[i]) {
            if (diffs < 10) {
                std::printf("line %zu\n  D:  |%s|\n  C++:|%s|\n", i + 1,
                            i < expected.size() ? expected[i].c_str() : "<none>",
                            i < produced.size() ? produced[i].c_str() : "<none>");
            }
            ++diffs;
        }
    }
    const bool ok = (diffs == 0);
    std::printf("%s: nm.nelmin sort permutation table, %zu tie patterns, %ld differ\n",
                ok ? "PASS" : "FAIL", expected.size(), static_cast<long>(diffs));
    return ok ? 0 : 1;
}
