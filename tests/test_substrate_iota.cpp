// tests/test_substrate_iota.cpp — replay probes/substrate/probe_iota.d.
//
// Pins the two iota rules eil/range.hpp implements: element n is `begin + n*step`
// (accumulation diverges from n=6 onward on a 0.1 step) and the far bound is
// strict (iota(0.0,1.0,0.25) drops the exact hit at 1.0). Both are used by
// geom/surface/beziertrianglepatch.d and geom/grid/sgrid.d.
//
// Run: ./test_substrate_iota baselines/e587b235/probes/substrate_iota.txt

#include <cstdarg>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "eil/eil.hpp"

namespace {

std::vector<std::string> produced;

void emitf(const char* fmt, ...) {
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    produced.emplace_back(buf);
}

void dump_f(const std::string& name, double b, double e, double s) {
    eil::Array<double> r = eil::iota(b, e, s);
    for (size_t n = 0; n < r.length(); ++n) emitf("%s[%zu]=%.17g", name.c_str(), n, r[n]);
    emitf("%s.count=%zu", name.c_str(), r.length());
}

void dump_i(const std::string& name, int b, int e, int s) {
    eil::Array<int> r = eil::iota(b, e, s);
    std::string joined;
    for (size_t n = 0; n < r.length(); ++n) {
        if (n) joined += ",";
        joined += std::to_string(r[n]);
    }
    emitf("%s=%s count=%zu", name.c_str(), joined.c_str(), r.length());
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <substrate_iota.txt>\n";
        return 2;
    }
    dump_f("one tenth", 0.0, 1.0, 0.1);
    dump_f("quarter", 0.0, 1.0, 0.25);
    dump_f("sgrid", 0.0, 1.0 + 0.5 * 0.25, 0.25);
    dump_f("neg float", 1.0, 0.0, -0.25);
    dump_f("empty", 1.0, 1.0, 0.1);
    dump_i("desc4", 4, -1, -1);
    dump_i("desc_n0", 5, 0, -1);
    dump_i("up", 0, 5, 1);

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
    std::cout << (diffs == 0 ? "PASS" : "FAIL") << ": iota corpus, " << expected.size()
              << " D lines, " << produced.size() << " C++ lines, " << diffs << " differ\n";
    return diffs == 0 ? 0 : 1;
}
