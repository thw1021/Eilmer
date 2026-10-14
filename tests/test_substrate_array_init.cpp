// tests/test_substrate_array_init.cpp — C++ mirror of
// probes/substrate/probe_array_init.d, replayed against
// baselines/e587b235/probes/substrate_array_init.txt (30 rows).
//
// What this gates (substrate/include/eil/array.hpp, detail::grow_init): D fills a
// newly-allocated or grown array slot with T.init, and for `double` that is a quiet NaN
// (0x7ff8000000000000), NOT zero. Before this was measured the port value-initialized the
// tail with T() (0.0), which silently hid every never-written slot the reference solver
// propagates as NaN -- the sgrid corpus rows 757-760 (`sg.rgrid.identity.s ... nan nan`)
// are exactly such slots, and the test-side d_init_doubles helper used to paper over the
// gap (backlog T-929). int.init is 0, bool.init is false, a struct inherits .init field by
// field (so its double member is NaN), and a class handle is null -- all of which C++'s T()
// already matches, so `double` is the only type whose fill this file actually corrects.
//
// Regenerate:
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i probes/substrate/probe_array_init.d -of=/tmp/probe_ai -od/tmp/probe_obj_ai \
//     && /tmp/probe_ai > baselines/e587b235/probes/substrate_array_init.txt
//
// Run: ./test_substrate_array_init <baseline.txt>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "eil/array.hpp"
#include "eil/number.hpp"

namespace {

std::vector<std::string> g_lines;

std::string bits(double x) {
    uint64_t u;
    std::memcpy(&u, &x, sizeof u);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(u));
    return buf;
}

const char* tf(bool b) { return b ? "true" : "false"; }

// Mirrors probe_array_init.d's drow(): tag is "v=<i>", printed after "<group>[<i>]|".
std::string drow(const std::string& group, size_t i, double x) {
    uint64_t u;
    std::memcpy(&u, &x, sizeof u);
    char buf[256];
    std::snprintf(buf, sizeof buf,
                  "%s[%zu]|v=%zu|bits=%s|isnan=%s|isinf=%s|iszero=%s|signbit=%d",
                  group.c_str(), i, i, bits(x).c_str(), tf(x != x),
                  tf(x == INFINITY || x == -INFINITY), tf(x == 0.0),
                  static_cast<int>((u >> 63) != 0));
    return buf;
}

void dump_doubles(const std::string& group, eil::Array<double>& a) {
    for (size_t i = 0; i < a.length(); ++i) g_lines.push_back(drow(group, i, a[i]));
}

// D's `struct S { double v; int i; }`: S.init sets v to NaN and i to 0, field by field.
struct S {
    double v = eil::d_init<double>();
    int i = eil::d_init<int>();
};

void dump_structs(const std::string& group, eil::Array<S>& a) {
    for (size_t i = 0; i < a.length(); ++i) {
        char buf[128];
        std::snprintf(buf, sizeof buf, "%s[%zu]|v.bits=%s|i=%d", group.c_str(), i,
                      bits(a[i].v).c_str(), a[i].i);
        g_lines.push_back(buf);
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]); return 2; }

    // 1. new double[n]
    eil::Array<double> nd(4);
    dump_doubles("new_double", nd);

    // 2. double[].length = n
    eil::Array<double> bl;
    bl.set_length(4);
    dump_doubles("len_double", bl);

    // 3. growing an existing array: kept element survives, new tail is T.init
    eil::Array<double> gr{1.5};
    gr.set_length(4);
    dump_doubles("grow_double", gr);

    // 4. int: int.init == 0
    eil::Array<int> ni(3);
    for (size_t i = 0; i < ni.length(); ++i)
        g_lines.push_back("new_int[" + std::to_string(i) + "]|v=" + std::to_string(ni[i]));
    eil::Array<int> il;
    il.set_length(3);
    for (size_t i = 0; i < il.length(); ++i)
        g_lines.push_back("len_int[" + std::to_string(i) + "]|v=" + std::to_string(il[i]));

    // 5. bool: bool.init == false
    eil::Array<bool> nb(2);
    for (size_t i = 0; i < nb.length(); ++i)
        g_lines.push_back("new_bool[" + std::to_string(i) + "]|v=" + tf(nb[i]));

    // 6. struct with a double field: v is NaN, i is 0
    eil::Array<S> ns(2);
    dump_structs("new_struct", ns);
    eil::Array<S> sl;
    sl.set_length(2);
    dump_structs("len_struct", sl);

    // 7. class array: every handle is null (modelled with a smart pointer)
    eil::Array<std::shared_ptr<int>> nc(2);
    for (size_t i = 0; i < nc.length(); ++i)
        g_lines.push_back("new_class[" + std::to_string(i) + "]|isnull=" +
                          std::to_string(nc[i] == nullptr ? 1 : 0));

    // 8. shrink then regrow: the returned slots are re-initialized, not stale
    eil::Array<double> sr{10.0, 20.0, 30.0, 40.0};
    sr.set_length(1);
    sr.set_length(4);
    dump_doubles("regrow_double", sr);

    std::ifstream in(argv[1]);
    if (!in) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    std::vector<std::string> expected;
    for (std::string l; std::getline(in, l);) expected.push_back(l);

    int diffs = 0;
    for (size_t i = 0; i < expected.size() || i < g_lines.size(); ++i) {
        const std::string e = i < expected.size() ? expected[i] : "<none>";
        const std::string g = i < g_lines.size() ? g_lines[i] : "<none>";
        if (e == g) continue;
        ++diffs;
        std::printf("row %zu\n  D:   |%s|\n  C++: |%s|\n", i + 1, e.c_str(), g.c_str());
    }
    const bool ok = (diffs == 0) && (expected.size() == g_lines.size());
    std::printf("%s: substrate_array_init, %zu D rows, %zu C++ rows, %d differ\n",
                ok ? "PASS" : "FAIL", expected.size(), g_lines.size(), diffs);
    return ok ? 0 : 1;
}
