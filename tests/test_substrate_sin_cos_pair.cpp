// tests/test_substrate_sin_cos_pair.cpp — T-920's port side.
//
// Replays baselines/e587b235/probes/substrate_sin_cos_pair.txt byte for byte: for each
// angle in tests/test_data/sincos_angle.txt, print cos/sin as eil::cos/eil::sin give
// them, plus the same %.17g text the D probe prints.
//
// The corpus is the discriminating set, not a smoke test: 400 of its 415 angles are ones
// where glibc's fused `sincos()` and two separate libm calls disagree in the last bit.
// A port that lets the pair fuse therefore cannot pass this at -O1 or above, in either
// language -- ldc2 -O3 moves 401 of the same 415 rows, which is why the baselines here
// are -O0 captures and why the wrappers in eil/math.hpp exist.
//
// usage: test_substrate_sin_cos_pair <baseline.txt> <sincos_angle.txt>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "eil/math.hpp"
#include "eil/number.hpp"

namespace {

uint64_t to_bits(eil::number v) {
    uint64_t u = 0;
    std::memcpy(&u, &v, sizeof u);
    return u;
}

uint64_t from_hex(const std::string& h) {
    uint64_t v = 0;
    for (char c : h) {
        int n;
        if (c >= '0' && c <= '9') n = c - '0';
        else if (c >= 'a' && c <= 'f') n = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') n = c - 'A' + 10;
        else return 0;
        v = v * 16 + static_cast<uint64_t>(n);
    }
    return v;
}

// The row format is shared with the D probe by construction: both print the angle's hex,
// its tag, then cos/sin as bit patterns and as %.17g. Keeping the formatting in one place
// here means a mismatch is always a value mismatch, never a printf mismatch.
std::string row(const std::string& hexangle, const std::string& tag, eil::number c, eil::number s) {
    char buf[256];
    std::snprintf(buf, sizeof buf, "%s %s cos=%016llx sin=%016llx cosg=%.17g sing=%.17g",
                  hexangle.c_str(), tag.c_str(),
                  static_cast<unsigned long long>(to_bits(c)),
                  static_cast<unsigned long long>(to_bits(s)), c, s);
    return std::string(buf);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <baseline.txt> <sincos_angle.txt>\n", argv[0]);
        return 2;
    }
    std::ifstream in_base(argv[1]);
    if (!in_base) { std::fprintf(stderr, "cannot open baseline %s\n", argv[1]); return 2; }
    std::vector<std::string> expected;
    for (std::string l; std::getline(in_base, l);) expected.push_back(l);

    std::ifstream in_ang(argv[2]);
    if (!in_ang) { std::fprintf(stderr, "cannot open angles %s\n", argv[2]); return 2; }
    std::vector<std::string> actual;
    int n_div = 0, n_ctrl = 0;
    for (std::string l; std::getline(in_ang, l);) {
        if (l.empty() || l[0] == '#') continue;
        std::string hex, tag;
        {
            size_t sp = l.find(' ');
            hex = l.substr(0, sp);
            size_t t0 = l.find_first_not_of(' ', sp);
            tag = t0 == std::string::npos ? std::string() : l.substr(t0);
            size_t t1 = tag.find(' ');
            if (t1 != std::string::npos) tag = tag.substr(0, t1);
        }
        eil::number x;
        { uint64_t u = from_hex(hex); std::memcpy(&x, &u, sizeof x); }
        const eil::number c = eil::cos(x);
        const eil::number s = eil::sin(x);
        actual.push_back(row(hex, tag, c, s));
        if (tag == "div") ++n_div; else if (tag == "ctrl") ++n_ctrl;
    }

    int diffs = 0, shown = 0;
    size_t matched = 0;
    for (size_t i = 0; i < expected.size() || i < actual.size(); ++i) {
        const std::string e = i < expected.size() ? expected[i] : "<none>";
        const std::string g = i < actual.size() ? actual[i] : "<none>";
        if (e == g) { ++matched; continue; }
        ++diffs;
        if (shown < 12) {
            std::printf("line %zu\n  D:   |%s|\n  C++: |%s|\n", i + 1, e.c_str(), g.c_str());
            ++shown;
        }
    }
    if (actual.size() != expected.size()) {
        std::printf("FAIL: row count differs, D %zu vs C++ %zu\n", expected.size(), actual.size());
        return 1;
    }
    std::printf("%s: geom/substrate sin_cos_pair, probe replay %zu D lines, %zu C++ lines, "
                "%d differ; %zu of %zu rows byte-identical (%d div angles, %d ctrl)\n",
                diffs == 0 ? "PASS" : "FAIL", expected.size(), actual.size(), diffs, matched,
                expected.size(), n_div, n_ctrl);
    return diffs == 0 ? 0 : 1;
}
