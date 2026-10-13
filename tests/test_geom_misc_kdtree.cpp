// tests/test_geom_misc_kdtree.cpp
//
// Two layers, both required (the pattern of tests/test_geom_elements_properties.cpp):
//
//  A. the three `unittest` blocks of geom/misc/kdtree.d (D:131-143 sorting,
//     D:146-159 construction, D:162-195 nearest) ported assertion-for-assertion, in
//     source order, with D's own tolerances (isClose default 1.0e-8, the 1e-12 the
//     nearest unittest passes).
//
//  B. a line-for-line replay of probes/geom/misc/probe_kdtree.d against
//     baselines/e587b235/probes/geom_misc_kdtree.txt: 403 lines. For the 25-node grid
//     of the third unittest it records the array before and after makeTree (makeTree
//     reorders the caller's array by quicksort), the tree shape as
//     `mk d=<depth> ax=<split axis> len=<region size> idx=<median index> ...` rows, and
//     per query: the returned node's coordinates AND its blkid/cellid, bestDist,
//     sqrt(bestDist), the visited-node count and the independent slow_nearest value —
//     so a traversal that lands on a different node, or visits a different number of
//     nodes, cannot pass by agreeing on the distance alone.
//
// The 100 query points come from tests/test_data/kdtree_queries.txt, NOT from an
// eil:: RNG. kdtree.d:176 seeds Phobos' MT19937 with Random(19920829) and draws
// `uniform(0.0, 4.0, rng)` triples (D:180-182); Phobos' MT19937 is deliberately not
// ported, so the points were captured from D at %.17g (17 digits round-trips a double
// exactly) and replayed here. Regenerate both files with (see the probe header):
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   cd /home/tang/packages/Eilmer
//   ldc2 -i -I/home/tang/packages/gdtk/src probes/geom/misc/probe_kdtree.d -of=/tmp/kd_d
//   /tmp/kd_d > baselines/e587b235/probes/geom_misc_kdtree.txt
//   sed -n 's/^query \([0-9]\+\) x=\([^ ]*\) y=\([^ ]*\) z=\(.*\)$/\1 \2 \3 \4/p'
//   (that sed applied to the baseline writes tests/test_data/kdtree_queries.txt)
//
// Run: ./test_geom_misc_kdtree <baseline.txt> <kdtree_queries.txt>
#include <array>
#include <cctype>
#include <cstdarg>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <limits>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "geom/misc/kdtree.hpp"
#include "nm/number.hpp"

using eil::geom::Node;
using eil::geom::quicksort;
using eil::geom::makeTree;
using eil::geom::distance_squared;
using eil::geom::fast_nearest;
using eil::geom::slow_nearest;

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

// --- test-side helpers mirroring the probe (product code does not print) ----------
size_t subtree_size(const Node* n) {
    if (n == nullptr) return 0;
    return 1 + subtree_size(n->left) + subtree_size(n->right);
}

void describe(const Node* n, size_t depth) {
    if (n == nullptr) return;
    const size_t len = subtree_size(n);
    emitf("mk d=%d ax=%d len=%d idx=%d x=%.17g y=%.17g z=%.17g blk=%d cell=%d",
          static_cast<int>(depth), static_cast<int>(depth % 3), static_cast<int>(len),
          static_cast<int>(len / 2), n->x[0], n->x[1], n->x[2], static_cast<int>(n->blkid),
          static_cast<int>(n->cellid));
    describe(n->left, depth + 1);
    describe(n->right, depth + 1);
}

std::string show3(const std::array<double, 3>& x) {
    return eil::format("%.17g %.17g %.17g", x[0], x[1], x[2]);
}

eil::Array<Node> grid25() {
    // kdtree.d:163-167, in declaration order, then the blkid/cellid loop of D:169-172.
    eil::Array<Node> nodes;
    const double zs[5] = {0.0, 2.0, 3.0, 4.0, 1.0};
    for (int row = 0; row < 5; ++row) {
        for (int x = 0; x < 5; ++x) {
            Node n{{{static_cast<double>(x), 4.0, zs[row]}}};
            nodes.append(n);
        }
    }
    for (size_t i = 0; i < nodes.length(); ++i) {
        nodes[i].blkid = 0;
        nodes[i].cellid = i;
    }
    return nodes;
}

// The 100 query points captured from D's Random(19920829), loaded from argv[2].
std::vector<std::array<double, 3>> queries;

// ============================================================================
// A. the three D unittest blocks, assertion for assertion, in D's order.
// ============================================================================
void port_d_unittest() {
    // --- unittest 1: test sorting (D:131-143) -----------------------------
    {
        eil::Array<Node> nodes;
        const double init[6][3] = {{2, 3, 0}, {5, 4, 0}, {9, 6, 0}, {4, 7, 0}, {8, 1, 0},
                                   {7, 2, 0}};
        for (const auto& v : init) nodes.append(Node{{v[0], v[1], v[2]}});

        eil::Array<Node> targetnodesx;
        const double tx[6][3] = {{2, 3, 0}, {4, 7, 0}, {5, 4, 0}, {7, 2, 0}, {8, 1, 0},
                                 {9, 6, 0}};
        for (const auto& v : tx) targetnodesx.append(Node{{v[0], v[1], v[2]}});

        eil::Array<Node> targetnodesy;
        const double ty[6][3] = {{8, 1, 0}, {7, 2, 0}, {2, 3, 0}, {5, 4, 0}, {9, 6, 0},
                                 {4, 7, 0}};
        for (const auto& v : ty) targetnodesy.append(Node{{v[0], v[1], v[2]}});

        quicksort<0>(nodes.all());
        CHECK(nodes.length() == targetnodesx.length(), "D:139 sorted x length");
        bool same_x = nodes.length() == targetnodesx.length();
        for (size_t i = 0; i < nodes.length() && same_x; ++i) same_x = (nodes[i] == targetnodesx[i]);
        CHECK(same_x, "D:140 nodes == targetnodesx");

        quicksort<1>(nodes.all());
        bool same_y = nodes.length() == targetnodesy.length();
        for (size_t i = 0; i < nodes.length() && same_y; ++i) same_y = (nodes[i] == targetnodesy[i]);
        CHECK(same_y, "D:142 nodes == targetnodesy");
    }

    // --- unittest 2: test kdtree construction (D:146-159) -----------------
    {
        eil::Array<Node> nodes;
        const double init[6][3] = {{2, 3, 1}, {5, 4, 2}, {9, 6, 3}, {4, 7, 6}, {8, 1, 8},
                                   {7, 2, 4}};
        for (const auto& v : init) nodes.append(Node{{v[0], v[1], v[2]}});

        Node* root = makeTree<0>(nodes.all());
        // D dereferences these pointers unconditionally; eil::d_init-style null checks
        // would change the test, so the same unconditional reads are kept (and an
        // unexpected shape would segfault exactly as D's RangeError would not).
        CHECK(root->left->x[0] <= root->x[0], "D:154 left.x[0] <= root.x[0]");
        CHECK(root->x[0] <= root->right->x[0], "D:155 root.x[0] <= right.x[0]");
        CHECK(root->left->left->x[1] <= root->left->x[1], "D:156 left.left.x[1] <= left.x[1]");
        CHECK(root->left->right->x[1] >= root->left->x[1], "D:157 left.right.x[1] >= left.x[1]");
        CHECK(root->right->left->x[1] <= root->right->x[1], "D:158 right.left.x[1] <= right.x[1]");
    }

    // --- unittest 3: test nearest computation (D:162-195) ----------------
    // The 100 query points are replayed from the data file (see the header), so this
    // block runs over exactly the points D's Random(19920829) produced.
    {
        eil::Array<Node> nodes = grid25();
        Node* root = makeTree<0>(nodes.all());
        const size_t ntests = 100;
        for (size_t n = 0; n < ntests && n < queries.size(); ++n) {
            Node thisPt{{queries[n]}};
            const double actual_distance = slow_nearest(nodes.all(), thisPt);

            const Node* found = nullptr;
            double bestDist = 0;
            size_t nVisited = 0;
            fast_nearest(root, thisPt, 0, found, bestDist, nVisited);

            CHECK(eil::isClose(eil::sqrt(bestDist), actual_distance, 1e-12),
                  "D:193 sqrt(bestDist) close to slow_nearest");
        }
        CHECK(queries.size() == ntests, "100 captured D query points loaded");
    }
}

// ============================================================================
// B. the probe replay.
// ============================================================================
void replay_probe() {
    using namespace eil::geom;

    eil::Array<Node> nodes = grid25();

    // Order of the array before makeTree mutates it.
    for (size_t i = 0; i < nodes.length(); ++i) {
        emitf("pre %d xyz=%s blk=%d cell=%d", static_cast<int>(i), show3(nodes[i].x).c_str(),
              static_cast<int>(nodes[i].blkid), static_cast<int>(nodes[i].cellid));
    }

    Node* root = makeTree<0>(nodes.all());

    // The array after makeTree: the quicksorts reorder it, and that order is the
    // observable shape of the tree's storage.
    for (size_t i = 0; i < nodes.length(); ++i) {
        emitf("post %d xyz=%s blk=%d cell=%d", static_cast<int>(i), show3(nodes[i].x).c_str(),
              static_cast<int>(nodes[i].blkid), static_cast<int>(nodes[i].cellid));
    }
    emitf("tree.len=%d", static_cast<int>(subtree_size(root)));
    describe(root, 0);

    // The 100 captured query points, printed exactly as the probe printed them.
    for (size_t n = 0; n < queries.size(); ++n) {
        emitf("query %d x=%.17g y=%.17g z=%.17g", static_cast<int>(n), queries[n][0],
              queries[n][1], queries[n][2]);
    }

    for (size_t n = 0; n < queries.size(); ++n) {
        Node thisPt{{queries[n]}};
        const double actual_distance = slow_nearest(nodes.all(), thisPt);
        const Node* found = nullptr;
        double bestDist = 0;
        size_t nVisited = 0;
        fast_nearest(root, thisPt, 0, found, bestDist, nVisited);
        emitf("ans %d fx=%.17g fy=%.17g fz=%.17g blk=%d cell=%d best=%.17g sqrt=%.17g visited=%d slow=%.17g",
              static_cast<int>(n), found->x[0], found->x[1], found->x[2],
              static_cast<int>(found->blkid), static_cast<int>(found->cellid), bestDist,
              eil::sqrt(bestDist), static_cast<int>(nVisited), actual_distance);
        // The unittest's own writefln (kdtree.d:191-192).
        emitf("point: %s, nearest: %s, dist = %g actual = %g id = %d",
              eil::to_string(thisPt.x).c_str(), eil::to_string(found->x).c_str(),
              eil::sqrt(bestDist), actual_distance, static_cast<int>(found->cellid));
    }

    // ---- the two sorting unittests' data (D:131-143, D:146-159) ----------
    eil::Array<Node> snodes;
    const double init[6][3] = {{2, 3, 0}, {5, 4, 0}, {9, 6, 0}, {4, 7, 0}, {8, 1, 0}, {7, 2, 0}};
    for (const auto& v : init) snodes.append(Node{{v[0], v[1], v[2]}});
    quicksort<0>(snodes.all());
    for (size_t i = 0; i < snodes.length(); ++i)
        emitf("sortx %d xyz=%s", static_cast<int>(i), show3(snodes[i].x).c_str());
    quicksort<1>(snodes.all());
    for (size_t i = 0; i < snodes.length(); ++i)
        emitf("sorty %d xyz=%s", static_cast<int>(i), show3(snodes[i].x).c_str());

    eil::Array<Node> tnodes;
    const double tinit[6][3] = {{2, 3, 1}, {5, 4, 2}, {9, 6, 3}, {4, 7, 6}, {8, 1, 8}, {7, 2, 4}};
    for (const auto& v : tinit) tnodes.append(Node{{v[0], v[1], v[2]}});
    Node* troot = makeTree<0>(tnodes.all());
    describe(troot, 0);

    // ---- ties, exact hits and the two degenerate trees -------------------
    std::array<std::array<double, 3>, 5> tps = {
        std::array<double, 3>{{2.0, 4.0, 2.0}}, std::array<double, 3>{{0.5, 4.0, 0.0}},
        std::array<double, 3>{{0.5, 4.0, 0.5}}, std::array<double, 3>{{1.0, 4.0, 2.5}},
        std::array<double, 3>{{4.0, 4.0, 4.0}}};
    for (size_t j = 0; j < tps.size(); ++j) {
        Node thisPt{tps[j]};
        const double actual_distance = slow_nearest(nodes.all(), thisPt);
        const Node* found = nullptr;
        double bestDist = 0;
        size_t nVisited = 0;
        fast_nearest(root, thisPt, 0, found, bestDist, nVisited);
        emitf("tie %d xyz=%s cell=%d best=%.17g slow=%.17g visited=%d", static_cast<int>(j),
              show3(tps[j]).c_str(), static_cast<int>(found->cellid), bestDist, actual_distance,
              static_cast<int>(nVisited));
    }

    eil::Array<Node> one;
    one.append(Node{{{1.0, 2.0, 3.0}}});
    Node* r1 = makeTree<0>(one.all());
    describe(r1, 0);

    eil::Array<Node> two;
    two.append(Node{{{1.0, 2.0, 3.0}}});
    two.append(Node{{{1.0, 2.0, 3.0}}});
    Node* r2 = makeTree<0>(two.all());
    describe(r2, 0);
    const Node* f2 = nullptr;
    double b2 = 0;
    size_t v2 = 0;
    Node dupPt{{{1.0, 2.0, 3.0}}};
    fast_nearest(r2, dupPt, 0, f2, b2, v2);
    emitf("dup.best=%.17g dup.visited=%d dup.cell=%d", b2, static_cast<int>(v2),
          static_cast<int>(f2->cellid));
}

bool load_queries(const char* path) {
    std::ifstream in(path);
    if (!in) {
        std::cerr << "cannot open query file " << path << "\n";
        return false;
    }
    std::string line;
    int expect = 0;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        int idx;
        double x, y, z;
        if (!(ss >> idx >> x >> y >> z)) {
            std::cerr << "bad query line: " << line << "\n";
            return false;
        }
        if (idx != expect++) {
            std::cerr << "query index out of order at line: " << line << "\n";
            return false;
        }
        queries.push_back(std::array<double, 3>{{x, y, z}});
    }
    return queries.size() == 100;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "usage: " << argv[0]
                  << " <probes/geom_misc_kdtree.txt> <test_data/kdtree_queries.txt>\n";
        return 2;
    }
    if (!load_queries(argv[2])) return 2;

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
                if (std::memcmp(&a[j], &b[j], 8) == 0) {
                    ++identical;
                } else {
                    const long long gap = ulp_gap(a[j], b[j]);
                    if (gap > worst_ulp) {
                        worst_ulp = gap;
                        worst_resid = std::fabs(a[j] - b[j]);
                        worst_d = a[j];
                        worst_cpp = b[j];
                        worst_where = tag + " " + (j < k.size() ? k[j] : std::string("?"));
                    }
                }
            }
        }
    }
    std::cout << (line_diffs == 0 && failures == 0 ? "PASS" : "FAIL") << ": kdtree, "
              << checks - failures << "/" << checks << " unittest assertions hold over "
              << queries.size() << " captured query points; probe replay " << expected.size()
              << " D lines, " << produced.size() << " C++ lines, " << line_diffs
              << " differ; " << identical << " of " << quantities
              << " quantities bit-identical";
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
