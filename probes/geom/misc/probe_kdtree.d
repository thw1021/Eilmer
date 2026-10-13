// probes/geom/misc/probe_kdtree.d — dump the whole observable surface of
// geom/misc/kdtree.d: the deterministic query set, the tree shape it builds, and
// what fast_nearest returns for every query.
//
// Why this probe exists: kdtree.d is the nearest-point kernel that geom/grid/usgrid.d
// uses to map a point to a cell, so a *wrong but plausible* traversal could still
// return a nearly-correct distance. The dump therefore records, per query, the
// identity of the returned node (its 3 coordinates AND its blkid/cellid), the raw
// bestDist, its square root, the number of nodes visited, and the independent
// slow_nearest reference value. Tree construction is dumped separately (split axis,
// region length, median index, median coordinates) so a different median choice or
// axis cycle cannot hide behind an agreeing distance.
//
// About the query points (READ THIS): kdtree.d:176 seeds Phobos'
// `Random(19920829)` (MT19937) and draws 100 points with `uniform(0.0, 4.0, rng)`.
// Phobos' MT19937 is deliberately NOT ported to C++; instead section "query" below
// is the byte-for-byte capture of those 100 triples at %.17g (a 17-significant-digit
// decimal round-trips an IEEE double exactly), and
// tests/test_data/kdtree_queries.txt holds the same 100 triples as "n x y z" so the
// C++ test replays D's identical query set.
//
// Build, capture the baseline and regenerate the query data file:
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   cd /home/tang/packages/Eilmer
//   ldc2 -i -I/home/tang/packages/gdtk/src probes/geom/misc/probe_kdtree.d -of=/tmp/kd_d
//   /tmp/kd_d > baselines/e587b235/probes/geom_misc_kdtree.txt
//   sed -n 's/^query \([0-9]\+\) x=\([^ ]*\) y=\([^ ]*\) z=\(.*\)$/\1 \2 \3 \4/p' \
//     baselines/e587b235/probes/geom_misc_kdtree.txt > tests/test_data/kdtree_queries.txt
import std.stdio, std.format, std.math, std.random;

import geom.misc.kdtree;

/// Number of nodes in the subtree rooted at n (pure pointer walk, no module changes).
size_t subtree_size(const(Node)* n) pure nothrow @nogc {
    if (n is null) return 0;
    return 1 + subtree_size(n.left) + subtree_size(n.right);
}

/// Dump the shape D's makeTree built. Every node at depth d was chosen by
/// quicksort!(d%3) on a region of `len` nodes, with median index len/2.
void describe(const(Node)* n, size_t depth) {
    if (n is null) return;
    size_t len = subtree_size(n);
    writefln("mk d=%d ax=%d len=%d idx=%d x=%.17g y=%.17g z=%.17g blk=%d cell=%d",
             depth, depth % 3, len, len / 2, n.x[0], n.x[1], n.x[2], n.blkid, n.cellid);
    describe(n.left, depth + 1);
    describe(n.right, depth + 1);
}

string show3(double[3] x) {
    return format("%.17g %.17g %.17g", x[0], x[1], x[2]);
}

void main() {
    // ====================================================================
    // 1. the 25-node grid of kdtree.d's third unittest (D:163-167), in the
    //    declaration order D gives it, with blkid=0 and cellid=index (D:169-172).
    // ====================================================================
    Node[] nodes = [{[0.0, 4.0, 0.0]}, {[1.0, 4.0, 0.0]}, {[2.0, 4.0, 0.0]},
                    {[3.0, 4.0, 0.0]}, {[4.0, 4.0, 0.0]},
                    {[0.0, 4.0, 2.0]}, {[1.0, 4.0, 2.0]}, {[2.0, 4.0, 2.0]},
                    {[3.0, 4.0, 2.0]}, {[4.0, 4.0, 2.0]},
                    {[0.0, 4.0, 3.0]}, {[1.0, 4.0, 3.0]}, {[2.0, 4.0, 3.0]},
                    {[3.0, 4.0, 3.0]}, {[4.0, 4.0, 3.0]},
                    {[0.0, 4.0, 4.0]}, {[1.0, 4.0, 4.0]}, {[2.0, 4.0, 4.0]},
                    {[3.0, 4.0, 4.0]}, {[4.0, 4.0, 4.0]},
                    {[0.0, 4.0, 1.0]}, {[1.0, 4.0, 1.0]}, {[2.0, 4.0, 1.0]},
                    {[3.0, 4.0, 1.0]}, {[4.0, 4.0, 1.0]}];

    foreach (i, ref node; nodes) {
        node.blkid = 0;
        node.cellid = i;
    }

    // Order of the array *before* makeTree mutates it (makeTree sorts in place).
    foreach (i, ref node; nodes) {
        writefln("pre %d xyz=%s blk=%d cell=%d", i, show3(node.x), node.blkid, node.cellid);
    }

    auto root = makeTree(nodes);

    // The array after makeTree: makeTree reorders `nodes` by quicksort, so the
    // physical order is part of the observable behaviour.
    foreach (i, ref node; nodes) {
        writefln("post %d xyz=%s blk=%d cell=%d", i, show3(node.x), node.blkid, node.cellid);
    }
    writefln("tree.len=%d", subtree_size(root));
    describe(root, 0);

    // ====================================================================
    // 2. the 100 query points from the same seed and draw order as D:176-182.
    //    These are the values copied into tests/test_data/kdtree_queries.txt.
    // ====================================================================
    size_t ntests = 100;
    auto rng = Random(19920829);
    double[] qx, qy, qz;
    qx.length = ntests;
    qy.length = ntests;
    qz.length = ntests;
    foreach (n; 0 .. ntests) {
        qx[n] = uniform(0.0, 4.0, rng);
        qy[n] = uniform(0.0, 4.0, rng);
        qz[n] = uniform(0.0, 4.0, rng);
        writefln("query %d x=%.17g y=%.17g z=%.17g", n, qx[n], qy[n], qz[n]);
    }

    // ====================================================================
    // 3. per-query answers. `nodes` is the makeTree-reordered array, exactly as
    //    the unittest passes it to slow_nearest (kdtree.d:184).
    // ====================================================================
    foreach (n; 0 .. ntests) {
        Node thisPt = {[qx[n], qy[n], qz[n]]};
        double actual_distance = slow_nearest(nodes, thisPt);

        const(Node)* found = null;
        double bestDist = 0;
        size_t nVisited = 0;
        root.fast_nearest(thisPt, 0, found, bestDist, nVisited);

        writefln("ans %d fx=%.17g fy=%.17g fz=%.17g blk=%d cell=%d best=%.17g sqrt=%.17g visited=%d slow=%.17g",
                 n, found.x[0], found.x[1], found.x[2], found.blkid, found.cellid,
                 bestDist, sqrt(bestDist), nVisited, actual_distance);
        // The unittest's own writefln line (kdtree.d:191-192), reproduced so the
        // port's %s-on-array and %g rendering is byte-checked too.
        writefln("point: %s, nearest: %s, dist = %g actual = %g id = %d",
                 thisPt.x, found.x, sqrt(bestDist), actual_distance, found.cellid);
    }

    // ====================================================================
    // 4. the two sorting unittests (kdtree.d:131-143 and :146-159).
    // ====================================================================
    Node[] snodes = [{[2, 3, 0]}, {[5, 4, 0]}, {[9, 6, 0]},
                     {[4, 7, 0]}, {[8, 1, 0]}, {[7, 2, 0]}];
    quicksort!(0)(snodes);
    foreach (i, ref node; snodes) writefln("sortx %d xyz=%s", i, show3(node.x));
    quicksort!(1)(snodes);
    foreach (i, ref node; snodes) writefln("sorty %d xyz=%s", i, show3(node.x));

    Node[] tnodes = [{[2, 3, 1]}, {[5, 4, 2]}, {[9, 6, 3]},
                     {[4, 7, 6]}, {[8, 1, 8]}, {[7, 2, 4]}];
    auto troot = makeTree(tnodes);
    describe(troot, 0);

    // Distance-squared ordering: fast_nearest compares pow(d,2) sums while
    // slow_nearest multiplies (kdtree.d:68-71 vs :120-126). A tie between two
    // nodes is decided by traversal order, so probe a point equidistant from
    // several grid nodes and an exact hit.
    double[3][5] tps = [[2.0, 4.0, 2.0], [0.5, 4.0, 0.0], [0.5, 4.0, 0.5],
                        [1.0, 4.0, 2.5], [4.0, 4.0, 4.0]];
    foreach (j, ref tp; tps) {
        Node thisPt = {[tp[0], tp[1], tp[2]]};
        double actual_distance = slow_nearest(nodes, thisPt);
        const(Node)* found = null;
        double bestDist = 0;
        size_t nVisited = 0;
        root.fast_nearest(thisPt, 0, found, bestDist, nVisited);
        writefln("tie %d xyz=%s cell=%d best=%.17g slow=%.17g visited=%d",
                 j, show3(tp), found.cellid, bestDist, actual_distance, nVisited);
    }

    // A single-node and a two-node tree: makeTree skips the child assignment when
    // medianIdx == 0 (kdtree.d:55-62), so the 1-element case is a real branch.
    Node[] one = [{[1.0, 2.0, 3.0]}];
    auto r1 = makeTree(one);
    describe(r1, 0);
    Node[] two = [{[1.0, 2.0, 3.0]}, {[1.0, 2.0, 3.0]}];
    auto r2 = makeTree(two);
    describe(r2, 0);
    const(Node)* f2 = null;
    double b2 = 0;
    size_t v2 = 0;
    Node dupPt = {[1.0, 2.0, 3.0]};
    r2.fast_nearest(dupPt, 0, f2, b2, v2);
    writefln("dup.best=%.17g dup.visited=%d dup.cell=%d", b2, v2, f2.cellid);
}
