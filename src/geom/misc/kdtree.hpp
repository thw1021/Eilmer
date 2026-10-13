#pragma once
// geom/misc/kdtree.d -> src/geom/misc/kdtree.hpp (module geom.misc.kdtree).
//
// 3D kd-tree for nearest-neighbour finding (D header: "Based on examples from
// https://rosettacode.org/wiki/K-d_tree#D"). Used by geom/grid/usgrid.d in the grid
// stage and by the lmr/eilmer solvers' point lookups, so traversal order and tie
// breaking are gate-relevant, not just the returned distance.
//
// Namespace: eil::geom, flat. `Node` collides with nothing in geom (grep: kdtree.d is
// the only `struct Node` in the tree) and quicksort/makeTree/fast_nearest/slow_nearest/
// distance_squared are unique too, so no eil::geom::<module> nesting was needed.
//
// Non-1:1 notes (all registered in provenance/fidelity.yml):
//   * The template parameters of quicksort and makeTree are D's compile-time `size_t
//     idx` / `size_t i = 0` axes; they stay non-type template parameters here, so the
//     axis cycling (0 -> 1 -> 2 -> 0, `enum i2 = (i+1)%3`) is resolved at compile time
//     exactly as in D. Their bodies therefore live in this header, while the three
//     non-template functions (distance_squared, fast_nearest, slow_nearest) are
//     declared here and defined in kdtree.cpp.
//   * D declares quicksort/makeTree/distance_squared/fast_nearest `pure nothrow @nogc`.
//     C++ has no equivalent spelling; the ported bodies allocate nothing and raise
//     nothing themselves. A Slice/Slice-style index out of bounds does throw
//     (ErrorBase via eil::Slice), where D would raise core.exception.RangeError — the
//     same "range violation is not undefined behaviour" rule as the mapping table.
//   * D's `in Node a, in Node b` / `in Node nd` become `const Node&`, and
//     `ref const(Node)* best` becomes `const Node*&`. The tree stores raw `Node*`
//     pointing INTO the caller's array, exactly as D's Node* point into the caller's
//     GC array: the tree does not own its nodes and dies with them (adapted-arrays).
//   * kdtree.d:59 computes `immutable size_t nPos = n - nodes.ptr;` and never reads
//     it. Kept as a comment at the site rather than deleted.
//   * quicksort swaps x, blkid and cellid but NOT left/right (D:39-41). That is what
//     makes makeTree safe: a node's children are only assigned after the region that
//     contains it has been sorted, and the two recursive regions exclude the median
//     itself, so no node is ever moved once it has children. The port swaps the same
//     three fields and nothing else.
//   * distance_squared squares with `^^` (D:68-70), which is std.math.pow and is
//     ported as eil::pow, while slow_nearest multiplies (D:121-124: cx*cx + cy*cy +
//     cz*cz). D really does use two different expressions for the same quantity; both
//     are kept as written.
//   * kdtree.d:176 seeds Phobos' Mersenne Twister with 19920829 to draw the 100 test
//     queries. Phobos' MT19937 is NOT ported (wave-02 brief); the queries are captured
//     from D and replayed from tests/test_data/kdtree_queries.txt, so the C++ test
//     walks the tree with byte-identical input points.

#include <array>
#include <cstddef>
#include <utility>

#include "eil/array.hpp"
#include "eil/math.hpp"
#include "eil/number.hpp"

namespace eil {
namespace geom {

// D:11-15. A value-semantics node; the three coordinates default to double.init
// (quiet NaN), the block/cell ids to size_t.init (0) and the child pointers to null.
struct Node {
    std::array<double, 3> x = {{eil::d_init<double>(), eil::d_init<double>(),
                                 eil::d_init<double>()}};
    Node* left = nullptr;
    Node* right = nullptr;
    size_t blkid = eil::d_init<size_t>();
    size_t cellid = eil::d_init<size_t>();
};

// D compares structs with `==` implicitly, field by field — including the two child
// pointers, which is what kdtree.d:139-142's `assert(nodes == targetnodes)` uses. The
// port spells that comparison out, so the unittest compares the same five fields.
inline bool operator==(const Node& a, const Node& b) {
    return a.x == b.x && a.left == b.left && a.right == b.right && a.blkid == b.blkid &&
           a.cellid == b.cellid;
}

// D:17-46. Sort `nodes` by coordinate idx. Hoare partition on the middle element,
// recursing on [0 .. i] and [i .. $].
template <size_t idx>
void quicksort(eil::Slice<Node> nodes) {
    size_t len = nodes.length();
    if (len < 2) return;

    const double pivot = nodes[len / 2].x[idx];

    size_t i, j;
    for (i = 0, j = len - 1;; i++, j--) {
        while (nodes[i].x[idx] < pivot) i++;
        while (nodes[j].x[idx] > pivot) j--;

        if (i >= j) break;

        std::swap(nodes[i].x, nodes[j].x);
        std::swap(nodes[i].blkid, nodes[j].blkid);
        std::swap(nodes[i].cellid, nodes[j].cellid);
        // D swaps exactly these three fields: left/right stay where they are. See the
        // header note for why that is safe here.
    }

    quicksort<idx>(nodes.slice(0, i));
    quicksort<idx>(nodes.slice(i, nodes.length()));
} // end quicksort()

// D:48-65. Build the tree in place and return the root. `nodes` is reordered by the
// sorts, and the returned pointers refer into it.
template <size_t i = 0>
Node* makeTree(eil::Slice<Node> nodes) {
    if (nodes.length() == 0) return nullptr;

    quicksort<i>(nodes);
    size_t medianIdx = nodes.length() / 2;
    Node* n = &nodes[medianIdx];

    // medianIdx will be zero if there is only element in nodes
    // In that case we can just skip this bit and return n
    if (medianIdx > 0) {
        constexpr size_t i2 = (i + 1) % 3;
        // kdtree.d:59 also computes `immutable size_t nPos = n - nodes.ptr;` here and
        // never reads it.
        n->left = makeTree<i2>(nodes.slice(0, medianIdx));
        n->right = makeTree<i2>(nodes.slice(medianIdx + 1, nodes.length()));
    }

    return n;
} // end makeTree()

// D:67-72. Squared distance; D writes the squares with `^^`, so these are eil::pow.
double distance_squared(const Node& a, const Node& b);

// D:74-111. Nearest-neighbour descent. Returns through `best`, `bestDist` (the
// SQUARED distance) and `nVisited`, the number of nodes examined.
void fast_nearest(const Node* root, const Node& nd, size_t i, const Node*& best,
                  double& bestDist, size_t& nVisited);

// D:113-128. Brute-force reference: the plain sum of squared differences with
// multiplication, and sqrt at the end.
double slow_nearest(eil::Slice<Node> nodes, const Node& point);

} // namespace geom
} // namespace eil
