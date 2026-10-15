// tests/test_substrate_acos_one.cpp
//
// Replay of probes/substrate/probe_acos_one.d: D's own asin/acos answers, row for row,
// against eil::asin / eil::acos. Backlog T-011 / T-931.
//
// What this gate is for. Phobos defines acos(x) = atan2(sqrt(1-x*x), x) and asin(x) =
// atan2(x, sqrt(1-x*x)), both formed in 80-bit real and narrowed on the way out. glibc's
// std::acos is a different function. In the middle of the domain the two agree; at the
// ends they do not, because acos has a square-root singularity there: of the 8,492 rows
// this corpus covers, glibc misses 120 of them, and 118 of those 120 are arguments inside
// 2^-30 of +-1 or outside the domain. geom/grid/paver.d:579 computes acos(r) from the
// cosine rule, and on the paver_demo.d boundary two nodes land exactly in that band,
// which is how T-931 was found.
//
// The row also pins the NaN polarity: D's acos/asin give a NEGATIVE quiet NaN for an
// argument outside [-1,1] AND for +-inf (measured: acosspec rows for 7ff0.../fff0...), so
// the substrate must not let libm's "unspecified sign" through.
//
// The informational line at the end reports what std::acos/std::asin would have scored.
// It is printed, not asserted: if glibc one day becomes correctly rounded there, that is
// not a reason for this gate to fail — eil:: matching D is the contract either way.
//
// Regenerate the baseline:
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -O0 probes/substrate/probe_acos_one.d -of=/tmp/probe_acos && /tmp/probe_acos \
//     > baselines/e587b235/probes/substrate_acos_one.txt
//
// Run: ./test_substrate_acos_one <baseline.txt>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "eil/math.hpp"

namespace {

using ull = unsigned long long;

std::string hexof(double d) {
    ull u;
    std::memcpy(&u, &d, sizeof u);
    char b[20];
    std::snprintf(b, sizeof b, "%016llx", u);
    return b;
}

bool from_hex(const std::string& h, double* out) {
    ull u = std::strtoull(h.c_str(), nullptr, 16);
    std::memcpy(out, &u, sizeof u);
    return true;
}

bool is_acos(const std::string& kind) { return kind.rfind("acos", 0) == 0; }

// The asinatan2/acosatan2 rows are D writing Phobos' DEFINITION out longhand, not D
// calling acos/asin, and for an infinite operand the two disagree on the NaN sign:
// D's acos(+inf) is fff8... (rows acosspec/asspec) while the hand-written
// atan2(sqrt(1-x*x), x) at x=+inf is 7ff8... . The port's contract is the FUNCTION, so
// those rows are reported as excluded rather than graded. The exclusion is counted and
// printed, never silent.
bool is_formula_row(const std::string& kind) {
    return kind.rfind("asinatan2", 0) == 0 || kind.rfind("acosatan2", 0) == 0;
}

}  // namespace

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
    long total = 0, mism = 0, glibc_mism = 0, shown = 0, excluded = 0;
    for (std::string line; std::getline(in, line);) {
        std::istringstream ss(line);
        std::string kind, inbits, outfield;
        if (!std::getline(ss, kind, '|')) continue;
        if (!std::getline(ss, inbits, '|')) continue;
        if (!std::getline(ss, outfield, '|')) continue;  // out=<hex>
        if (kind == "count") continue;
        if (is_formula_row(kind)) ++excluded;
        const std::string want = outfield.substr(outfield.find('=') + 1);
        if (is_formula_row(kind)) continue;  // counted in `excluded' above
        double x = 0.0, d = 0.0;
        from_hex(inbits, &x);
        from_hex(want, &d);
        const bool ac = is_acos(kind);
        double got = ac ? eil::acos(x) : eil::asin(x);
        double glibc = ac ? std::acos(x) : std::asin(x);
        ++total;
        if (hexof(glibc) != want) ++glibc_mism;
        // A NaN operand propagates its own payload in D, and both sides must agree on
        // that too; comparing the printed bit pattern covers it.
        if (hexof(got) != want) {
            ++mism;
            if (shown < 12) {
                std::printf("line %ld %s: in=%s want=%s got=%s\n", total, kind.c_str(),
                            inbits.c_str(), want.c_str(), hexof(got).c_str());
                ++shown;
            }
        }
    }
    std::printf("%s: substrate_acos_one, %ld rows replayed against eil::acos/eil::asin, "
                "%ld differ, %ld formula-definition rows excluded (they differ from D's own "
                "acos/asin in the NaN sign at +-inf); std::acos/std::asin would differ on "
                "%ld of the replayed rows\n",
                mism == 0 ? "PASS" : "FAIL", total, mism, excluded, glibc_mism);
    return mism == 0 ? 0 : 1;
}
