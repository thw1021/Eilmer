// tests/test_aa.cpp — associative-array behaviour the solver leans on.
//
// 45 AA declarations in src/eilmer, including the nested
// `string[string][size_t]` in bc/ghost_cell_effect/mapped_cell_copy.d:63.
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "eil/aa.hpp"
#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    } else {
        std::printf("ok: %s\n", what);
    }
}
} // namespace

int main() {
    eil::AA<std::string, int> aa;
    aa.set("one", 1);
    aa.set("two", 2);
    check(aa.length() == 2, "length");
    check(aa.get("one") == 1, "get an existing key");
    check(aa.contains("two") && !aa.contains("three"), "contains");

    // `(k in aa)` is a pointer in D, null when absent.
    check(aa.find("one") != nullptr && aa.find("nine") == nullptr, "find returns pointer/null");

    // Reading an absent key throws with Phobos' exact text (corpus "aa_missing").
    try {
        aa.get("nope");
        check(false, "missing key should throw");
    } catch (const eil::RangeViolation& e) {
        check(std::strcmp(e.what(), "Range violation") == 0, "Range violation text matches D");
    }

    // Re-assignment replaces rather than appends.
    aa.set("one", 11);
    check(aa.length() == 2 && aa.get("one") == 11, "set overwrites");

    // keys()/values() come out sorted; D's own order is hash-seeded and varies
    // per process, which the probe normalized by sorting before printing.
    auto ks = aa.keys();
    check(ks.length() == 2 && std::string(ks[0]) == "one" && std::string(ks[1]) == "two",
          "keys are ordered deterministically");
    auto vs = aa.values();
    check(vs.length() == 2 && vs[0] == 11 && vs[1] == 2, "values follow the same order");

    aa.remove("nope");
    check(!aa.contains("nope"), "remove");

    // Nested AA, the mapped_cell_copy.d shape.
    eil::AA<std::size_t, eil::AA<std::string, std::string>> blocks;
    blocks.get_or_default(12).set("cell", "1:2:3");
    check(blocks.get(12).get("cell") == "1:2:3", "nested AA insert/read");

    // Iteration order is stable across processes, unlike D's hash-seeded order.
    eil::AA<int, double> byId;
    for (int i : {5, 1, 3, 2, 4}) byId.set(i, i * 0.5);
    std::vector<int> seen;
    for (const auto& kv : byId) seen.push_back(kv.first);
    bool ascending = true;
    for (size_t i = 1; i < seen.size(); ++i) ascending = ascending && seen[i] > seen[i - 1];
    check(ascending, "iteration is deterministic (sorted)");

    std::printf("%s: associative arrays (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
