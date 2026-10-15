// geom/misc/kdtree.d -> src/geom/misc/kdtree.cpp
//
// The three non-template functions of the module (D:67-128). quicksort and makeTree are
// templates (their axis index is a compile-time parameter in D) and stay in the header.
//
// Expression order and operator choice are copied from D, including the two oddities
// recorded in the header: distance_squared squares with `^^` (std.math.pow, so
// eil::pow) while slow_nearest multiplies, and the `!bestDist` early return that makes
// an exact hit stop the descent.
#include "geom/misc/kdtree.hpp"

#include <cmath>
#include <cstddef>

#include "eil/array.hpp"
#include "eil/math.hpp"
#include "eil/number.hpp"

namespace eil {
namespace geom {

// D:67-72
double distance_squared(const Node& a, const Node& b) {
    double result = eil::sqr(a.x[0] - b.x[0]) + eil::sqr(a.x[1] - b.x[1]) +
                    eil::sqr(a.x[2] - b.x[2]);
    return result;
} // end distance_squared()

// D:74-111
//
// Fast searching for nearest neighbour in 3D, modified from
//    https://rosettacode.org/wiki/Sorting_algorithms/Quicksort#D
// Note that this routine returns "bestDist" the closest distance, SQUARED
void fast_nearest(const Node* root, const Node& nd, size_t i, const Node*& best,
                  double& bestDist, size_t& nVisited) {
    if (root == nullptr) return;

    const double d = distance_squared(*root, nd);
    const double dx = root->x[i] - nd.x[i];
    const double dx2 = eil::sqr(dx);
    nVisited++;

    if (!best || d < bestDist) {
        bestDist = d;
        best = root;
    }

    // If chance of exact match is high.
    if (!bestDist) return;

    const size_t i2 = (i + 1 >= 3) ? 0 : i + 1;

    fast_nearest(dx > 0 ? root->left : root->right, nd, i2, best, bestDist, nVisited);
    if (dx2 >= bestDist) return;
    fast_nearest(dx > 0 ? root->right : root->left, nd, i2, best, bestDist, nVisited);
} // end fast_nearest()

// D:113-128
//
// Check the distance to the nearest point by literally checking all of them
double slow_nearest(eil::Slice<Node> nodes, const Node& point) {
    // D declares these four bare (`double cx,cy,cz,dsq;`), which gives each of them
    // double.init == NaN before the loop writes them; eil::d_init keeps that.
    double cx = eil::d_init<double>(), cy = eil::d_init<double>(), cz = eil::d_init<double>(),
           dsq = eil::d_init<double>();
    double dsqmin = 1e32;
    for (size_t n = 0; n < nodes.length(); ++n) {
        cx = (nodes[n].x[0] - point.x[0]);
        cy = (nodes[n].x[1] - point.x[1]);
        cz = (nodes[n].x[2] - point.x[2]);
        // D multiplies here (and uses `^^` in distance_squared); kept as written.
        dsq = cx * cx + cy * cy + cz * cz;
        dsqmin = std::fmin(dsq, dsqmin);
    }
    return eil::sqrt(dsqmin);
} // end slow_nearest()

} // namespace geom
} // namespace eil
