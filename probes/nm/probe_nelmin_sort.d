// probes/nm/probe_nelmin_sort.d — what D's `vertices.sort!(less)` does to ties.
//
// nelmin.d sorts the simplex with std.algorithm's sort and the comparator
// `less` (a.fx < b.fx) at :96, :160, :170, :186, :195 and :230. Sorts of equal
// keys are the whole question here: nm/nelmin.d's own testFunction1 is symmetric
// in the coordinates, so the initial simplex has three or four vertices with
// EXACTLY equal fx (x=[0,0,0,0], dx=0.1 gives fx=3.01 four times), and which
// vertex the sort hands back as "worst" decides the whole trajectory -- the
// evaluation count included. geom/gpath and geom/sgrid objectives can hit the same
// situation, so the port's sort must produce D's permutation, not merely a sorted
// one.
//
// The table is the permutation of the original indices that D's sort leaves behind
// it: exhaustive over 3-level fx patterns (values 0/1/2) for n=2..6, then 100
// LCG-generated patterns per n from 7 to 18. n=17 matters because
// geom/grid/sgrid.d:1405 sizes that problem's vector at 16, so its simplex has 17
// vertices.
//
// Build and capture:
//   ldc2 -O0 -i -I/home/tang/packages/gdtk/src probes/nm/probe_nelmin_sort.d \
//     -of=/tmp/nelmin_sort_d && /tmp/nelmin_sort_d \
//     > baselines/e587b235/probes/nm_nelmin_sort.txt
// Replay check: ctest -R nm_nelmin_sort
import std.stdio, std.conv, std.algorithm;
import nm.nelmin;

string permutation(double[] fx) {
    Vertex!double[] v;
    foreach (i; 0 .. fx.length) {
        double[] x;
        x ~= cast(double)i;          // payload: the original index
        v ~= Vertex!double(x, fx[i]);
    }
    v.sort!(less);
    string s;
    foreach (k; 0 .. v.length) {
        if (k) s ~= ",";
        s ~= to!string(cast(int)v[k].x[0]);
    }
    return s;
}

void main() {
    // Exhaustive: every fx pattern over {0,1,2} for n = 2 .. 6.
    for (int n = 2; n <= 6; ++n) {
        int total = 1;
        for (int i = 0; i < n; ++i) total *= 3;
        for (int code = 0; code < total; ++code) {
            double[] fx;
            int c = code;
            for (int i = 0; i < n; ++i) { fx ~= cast(double)(c % 3); c /= 3; }
            writefln("perm n=%d src=exhaustive fx=%s %s", n, fx, permutation(fx));
        }
    }

    // Sampled: 100 LCG patterns per n up to 64. Phobos' quickSortImpl partitions
    // only above shortSortGetsBetter = max(32, 1024/Elem.sizeof), which is 42 for
    // Vertex!double (a 16-byte slice plus a double), so n = 43 .. 64 exercise the
    // partition + heapsort path and n = 7 .. 42 the shortSort path.
    for (int n = 7; n <= 64; ++n) {
        uint seed = 20220725u + cast(uint)n;
        for (int t = 0; t < 100; ++t) {
            double[] fx;
            for (int i = 0; i < n; ++i) {
                seed = seed * 1103515245u + 12345u;
                fx ~= cast(double)((seed >> 16) % cast(uint)(n < 5 ? n : 5));
            }
            writefln("perm n=%d src=lcg fx=%s %s", n, fx, permutation(fx));
        }
    }
}
