// tests/test_substrate_tostring_dispatch.cpp — replays the reproducible rows of
// baselines/e587b235/probes/substrate_tostring_dispatch.txt against eil::to_string after
// backlog T-926 item 1: substrate/include/eil/format.hpp gained a no-arg-toString() detector
// (has_noarg_to_string) and to_string now dispatches to t.toString() instead of
// static_assert-ing on every non-arithmetic type.
//
// The corpus was measured by probes/substrate/probe_tostring_dispatch.d (25 rows, ldc2 1.40.0,
// regenerates with 0 diff). D's default text for a type WITHOUT a toString() is a field dump
// ("NoToString(1, 2.5)") or, for a class, the fully-qualified type name
// ("probe_tostring_dispatch.Bare") — C++ has no reflection to reproduce those, and the backlog
// states explicitly that this item's value is "a call site can write eil::to_string(x) and it
// dispatches", NOT "auto-dump". So the rows that need reflection are counted as skipped and
// named (a skip-list that moves shows up in the output), and the rows that DO route through a
// member toString() — which is every port class with a hand-written one (Vector3, USGCell,
// every geom patch, nm's Bbla/Smla/Schedule) — are checked byte for byte against D.
//
// The dispatch facts this gate actually proves:
//   * a struct AND a class with a no-arg toString() are rendered by it;
//   * when BOTH a no-arg toString() and a toString(fmt, ...) exist, the NO-ARG one wins
//     (struct.both -> "plain", not the fmt overload);
//   * a container with no toString() still dumps "[a, b]" and DISPATCHES each element, so an
//     array of toString()-bearing structs prints "[Simple(1), Simple(2)]";
//   * "%s" reaches the same text (it calls to_string);
//   * a type with ONLY a fmt toString() is NOT matched (compile-time static_assert below).
//
// Regenerate the baseline (needs only Phobos, no gdtk shadow tree):
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -O0 -of=/tmp/probe_tostring_dispatch probes/substrate/probe_tostring_dispatch.d
//   /tmp/probe_tostring_dispatch > baselines/e587b235/probes/substrate_tostring_dispatch.txt
//
// Run: ./test_substrate_tostring_dispatch <baseline.txt>
#include <cstdio>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include "eil/format.hpp"

namespace {

// ---- port mirrors of the probe's user types ----

struct Simple {
    int v;
    std::string toString() const { return eil::format("Simple(%d)", v); }  // D: format("Simple(%d)", v)
};

struct Both {
    int v;
    std::string toString() const { return "plain"; }                       // D: no-arg -> "plain"
    std::string toString(const std::string&) const { return "fmt:" + std::to_string(v); }
};

struct FormatToString {
    int v;
    // fmt-only: to!string must NOT dispatch this (D falls back to a field dump).
    std::string toString(const std::string& fmt) const { return fmt + std::to_string(v); }
};

// A class-shaped handle with a hand-written toString(), like Vector3 / USGCell on the port side.
struct Cls {
    int v;
    std::string toString() const { return eil::format("Cls<%d>", v); }      // D: format("Cls<%d>", v)
};

// The fmt-only struct must NOT be matched by the no-arg detector (it has no zero-arg overload).
static_assert(!eil::has_noarg_to_string<FormatToString>::value,
              "T-926: a type with only a fmt toString() must not be dispatched");
static_assert(eil::has_noarg_to_string<Simple>::value, "Simple has a no-arg toString()");
static_assert(eil::has_noarg_to_string<Both>::value, "Both has a no-arg toString()");

// ---- the rows this port can reproduce (no reflection needed) ----

std::string compute(const std::string& label) {
    if (label == "struct.with.toString") return eil::to_string(Simple{7});
    if (label == "struct.both") return eil::to_string(Both{9});
    if (label == "direct.both") return Both{9}.toString();
    if (label == "class.with.toString") return eil::to_string(Cls{6});
    if (label == "int") return eil::to_string(42);
    if (label == "bool") return eil::to_string(true);
    if (label == "dstring") return eil::to_string(std::string("x"));
    if (label == "array.int") return eil::to_string(std::vector<int>{1, 2, 3});
    if (label == "array.struct") return eil::to_string(std::vector<Simple>{{1}, {2}});
    if (label == "nested.array.of.array")
        return eil::to_string(std::vector<std::vector<int>>{{1, 2, 3}, {1, 2, 3}});
    if (label == "format.simple") return eil::format("%s", Simple{7});
    if (label == "format.array.struct") return eil::format("%s", std::vector<Simple>{{1}, {2}});
    return "";
}

const std::set<std::string> kReproducible = {
    "struct.with.toString", "struct.both",   "direct.both",
    "class.with.toString",  "int",           "bool",
    "dstring",              "array.int",     "array.struct",
    "nested.array.of.array", "format.simple", "format.array.struct",
};

// text=[...] is the last field on a reproducible row (nothing after it carries a ']').
std::string extract_text(const std::string& line) {
    const size_t b = line.find("text=[");
    if (b == std::string::npos) return "";
    const size_t start = b + 6;
    const size_t last = line.rfind(']');
    return line.substr(start, last == std::string::npos || last < start ? 0 : last - start);
}

int g_fail = 0, g_checked = 0, g_skipped = 0;

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }
    std::ifstream in(argv[1]);
    if (!in) {
        std::fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }
    std::vector<std::string> skipped;
    for (std::string line; std::getline(in, line);) {
        if (line.empty()) continue;
        const std::string label = line.substr(0, line.find('|'));
        if (!kReproducible.count(label)) {
            ++g_skipped;
            skipped.push_back(label);
            continue;
        }
        const std::string want = extract_text(line);
        const std::string got = compute(label);
        ++g_checked;
        if (got != want) {
            ++g_fail;
            std::printf("FAIL %s\n  D:   [%s]\n  C++: [%s]\n", label.c_str(), want.c_str(),
                        got.c_str());
        }
    }
    if (!skipped.empty()) {
        std::printf("skipped %d rows needing D reflection (no toString() to dispatch):\n  ",
                    g_skipped);
        for (const std::string& k : skipped) std::printf("%s ", k.c_str());
        std::printf("\n");
    }
    std::printf("%s: substrate_tostring_dispatch, %d rows checked, %d skipped, %d failures\n",
                g_fail == 0 ? "PASS" : "FAIL", g_checked, g_skipped, g_fail);
    return g_fail == 0 ? 0 : 1;
}
