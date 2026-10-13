// probes/geom/misc/probe_nurbs_utils.d — dump the NURBS kernels of
// geom/misc/nurbs_utils.d: every findSpan answer over a knot-vector sweep, the full
// basisFuns rows (and the workspace contents they leave behind), autoKnotVector for
// compatible and incompatible (N, p), duplicatePw's deep copy, and the two throwing
// guards with their messages.
//
// Why this probe exists: nurbssurface.d, nurbsvolume.d and gpath/nurbs.d all call
// these three kernels, and findSpan's binary search is the kind of code where an
// off-by-one still returns a "reasonable" span. So the sweep visits every knot value,
// every mid-span value, both clamped ends, and the near-miss values that the tol
// = 1.0e-12 correction (nurbs_utils.d:23-27) is there to catch.
//
// Two scope notes, measured rather than assumed:
//   * nurbs_utils.d has no curveEval (the wave-02 brief anticipated one). Its
//     evaluation surface is exactly findSpan, basisFuns, NURBSWorkspace, PwTest,
//     duplicatePw and autoKnotVector; curve evaluation lives in the consumers.
//   * convNURBSVolDataToVTK (nurbs_utils.d:122-128) is the only thing in geom/misc
//     that touches geom.volume.nurbsvolume, it has no callers anywhere in the tree
//     (grep over src/, luawrap and examples finds only its own unittest reference),
//     and it is what would make misc <-> volume a header cycle. It is NOT ported and
//     NOT probed here; see src/geom/misc/nurbs_utils.hpp and reports/backlog.md.
//
// Build and capture:
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   cd /home/tang/packages/Eilmer
//   # The probe and the module are compiled separately and linked together, because
//   # `-i` would also code-generate geom.volume.nurbsvolume, which reaches
//   # geom.elements.projection.d:154 (`@nogc` + `new GeometryException`), and ldc2
//   # 1.40.0 rejects that. This keeps the measurement on the real gdtk source.
//   ldc2 -c -I/home/tang/packages/gdtk/src \
//     /home/tang/packages/gdtk/src/geom/misc/nurbs_utils.d -of=/tmp/nurbs_utils.o
//   ldc2 -c -I/home/tang/packages/gdtk/src probes/geom/misc/probe_nurbs_utils.d \
//     -of=/tmp/nru_probe.o
//   ldc2 /tmp/nru_probe.o /tmp/nurbs_utils.o -of=/tmp/nru_d
//   /tmp/nru_d > baselines/e587b235/probes/geom_misc_nurbs_utils.txt
import std.conv;
import std.format;
import std.math;
import std.stdio;

import nm.number;

import geom.misc.nurbs_utils;

/// One printed line per observation: the findSpan message embeds newlines.
string esc(string t) {
    string res;
    foreach (dchar c; t) {
        if (c == '\n') res ~= "\\n";
        else res ~= text(c);
    }
    return res;
}

void dump_span(string tag, double[] U, int p, double[] us) {
    int n = cast(int)U.length - p - 1;
    writefln("vec %s p=%d n=%d m=%d U=%s", tag, p, n, cast(int)U.length - 1,
             format("%s", U));
    foreach (u; us) {
        string line;
        try {
            int i = findSpan(u, n, p, U);
            line = format("span %s u=%.17g i=%d", tag, u, i);
        }
        catch (Exception e) {
            // D:33 throws new Exception (not Error); the message carries %.18e values.
            line = format("span %s u=%.17g EXC isError=0 isException=1 msg=[%s]", tag, u,
                          esc(e.msg));
        }
        catch (Error e) {
            line = format("span %s u=%.17g EXC isError=1 isException=0 msg=[%s]", tag, u,
                          esc(e.msg));
        }
        writeln(line);
    }
}

void dump_basis(string tag, double[] U, int p, double[] us) {
    int n = cast(int)U.length - p - 1;
    foreach (u; us) {
        int i = findSpan(u, n, p, U);
        auto nws = NURBSWorkspace(p);
        double[] N;
        N.length = p + 1;
        basisFuns(i, u, p, U, N, nws);
        string row;
        foreach (j; 0 .. N.length) row ~= format("%s%.17g", j == 0 ? "" : ",", N[j]);
        string lefts;
        foreach (j; 0 .. nws.left.length) lefts ~= format("%s%.17g", j == 0 ? "" : ",", nws.left[j]);
        string rights;
        foreach (j; 0 .. nws.right.length) rights ~= format("%s%.17g", j == 0 ? "" : ",", nws.right[j]);
        writefln("basis %s u=%.17g i=%d N=[%s] sum=%.17g left=[%s] right=[%s]", tag, u, i, row,
                 sum_row(N), lefts, rights);
    }
}

double sum_row(double[] N) {
    double s = 0.0;
    foreach (v; N) s += v;
    return s;
}

void main() {
    // ---- findSpan ---------------------------------------------------------
    // Piegl & Tiller example 2.3 (the module's own unittest vector).
    double[] Uex = [0.0, 0.0, 0.0, 1.0, 2.0, 3.0, 4.0, 4.0, 5.0, 5.0, 5.0];
    double[] us1 = [0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 5.0,
                    1.0e-13, -1.0e-13, 5.0 + 1.0e-13, 5.0 - 1.0e-13, 1.0e-6, -1.0e-6,
                    5.0 + 1.0e-6, 4.9999999999999, 2.5000000000001];
    dump_span("ex", Uex, 2, us1);

    // Clamped cubic with a single span (the autoKnotVector N=4,p=3 case).
    double[] Ucub = [0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0];
    dump_span("cubic1span", Ucub, 3, [0.0, 0.25, 0.5, 0.75, 1.0, 1.0e-13, -1.0e-13]);

    // Repeated interior knots (multiplicity p) and a uniform quintic.
    double[] Urep = [0.0, 0.0, 0.0, 1.0, 1.0, 2.0, 2.0, 3.0, 3.0, 3.0];
    dump_span("repeated", Urep, 2, [0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0]);
    double[] Uq = [0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 5.0, 5.0, 5.0, 5.0, 5.0];
    dump_span("quintic", Uq, 5, [0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 5.0]);
    // p = 1: the binary search runs with low == p == 1.
    double[] Ulin = [0.0, 0.0, 0.25, 0.5, 0.75, 1.0, 1.0];
    dump_span("linear", Ulin, 1, [0.0, 0.1, 0.25, 0.3, 0.5, 0.75, 1.0]);

    // u exactly equal to U[n+1]: the early `return n` branch (nurbs_utils.d:37).
    foreach (u; [Uex[9], Ucub[4], Urep[7]]) {
        writefln("early u=%.17g", u);
    }

    // ---- basisFuns --------------------------------------------------------
    dump_basis("ex", Uex, 2, [0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0, 3.5, 4.0, 4.5, 5.0]);
    dump_basis("cubic1span", Ucub, 3, [0.0, 0.25, 0.5, 0.75, 1.0]);
    dump_basis("repeated", Urep, 2, [0.0, 0.5, 1.0, 1.5, 2.0, 2.5, 3.0]);
    dump_basis("linear", Ulin, 1, [0.0, 0.1, 0.25, 0.5, 0.75, 1.0]);
    // p == 1 with the degree-1 workspace, and the Piegl hand-checked row.
    {
        auto nws = NURBSWorkspace(2);
        double[] N;
        N.length = 3;
        basisFuns(4, 2.5, 2, Uex, N, nws);
        writefln("basis unittest row=%.17g,%.17g,%.17g len=%d", N[0], N[1], N[2], N.length);
    }

    // ---- NURBSWorkspace ---------------------------------------------------
    foreach (p; [1, 2, 3, 5]) {
        auto w = NURBSWorkspace(p);
        writefln("ws p=%d left=%s right=%s", p, w.left, w.right);
    }

    // ---- autoKnotVector ---------------------------------------------------
    foreach (c; [[4.0, 3.0], [5.0, 2.0], [4.0, 1.0], [2.0, 1.0], [7.0, 3.0], [10.0, 5.0],
                 [3.0, 2.0], [6.0, 4.0]]) {
        int N = cast(int)c[0];
        int p = cast(int)c[1];
        auto U = autoKnotVector(N, p);
        string row;
        foreach (v; U) row ~= format("%s%.17g", row.length == 0 ? "" : ",", v);
        writefln("knots N=%d p=%d len=%d U=[%s]", N, p, U.length, row);
    }
    // The guard at nurbs_utils.d:102-103 throws a D Error (not Exception).
    foreach (c; [[4.0, 4.0], [4.0, 0.0], [2.0, 5.0], [1.0, 1.0], [4.0, -1.0]]) {
        try {
            auto U = autoKnotVector(cast(int)c[0], cast(int)c[1]);
            writefln("knots N=%d p=%d built=1 len=%d", cast(int)c[0], cast(int)c[1], U.length);
        }
        catch (Error e) {
            writefln("knots N=%d p=%d built=0 isError=1 msg=[%s]", cast(int)c[0], cast(int)c[1],
                     esc(e.msg));
        }
        catch (Exception e) {
            writefln("knots N=%d p=%d built=0 isError=0 msg=[%s]", cast(int)c[0], cast(int)c[1],
                     esc(e.msg));
        }
    }

    // ---- PwTest (nurbs_utils.d:75-81) ------------------------------------
    {
        double[4][3] pts = [[0.0, 0.0, 0.0, 1.0], [1.0, 0.0, 0.0, 1.0], [2.0, 1.0, 0.5, 1.0]];
        try {
            PwTest(pts[]);
            writeln("pwtest 3 built=1");
        }
        catch (Error e) { writeln(format("pwtest 3 built=0 isError=1 msg=[%s]", esc(e.msg))); }
        catch (Exception e) { writeln(format("pwtest 3 built=0 isError=0 msg=[%s]", esc(e.msg))); }

        double[4][1] one = [[0.0, 0.0, 0.0, 1.0]];
        try {
            PwTest(one[]);
            writeln("pwtest 1 built=1");
        }
        catch (Error e) { writeln(format("pwtest 1 built=0 isError=1 msg=[%s]", esc(e.msg))); }
        catch (Exception e) { writeln(format("pwtest 1 built=0 isError=0 msg=[%s]", esc(e.msg))); }

        double[4][] none;
        try {
            PwTest(none);
            writeln("pwtest 0 built=1");
        }
        catch (Error e) { writeln(format("pwtest 0 built=0 isError=1 msg=[%s]", esc(e.msg))); }
        catch (Exception e) { writeln(format("pwtest 0 built=0 isError=0 msg=[%s]", esc(e.msg))); }
    }

    // ---- duplicatePw (nurbs_utils.d:83-96) --------------------------------
    {
        number[4][][][] pw = new number[4][][][](2, 2, 2);
        int c = 0;
        foreach (i; 0 .. 2) foreach (j; 0 .. 2) foreach (k; 0 .. 2) {
            pw[i][j][k] = [cast(number)c, cast(number)c + 0.5, cast(number)c - 0.25, 1.0];
            ++c;
        }
        auto cp = duplicatePw(pw);
        writefln("dup dims %d %d %d", cp.length, cp[0].length, cp[0][0].length);
        foreach (i; 0 .. 2) foreach (j; 0 .. 2) foreach (k; 0 .. 2) {
            writefln("dup orig[%d][%d][%d]=%.17g,%.17g,%.17g,%.17g copy=%.17g,%.17g,%.17g,%.17g same=%s",
                     i, j, k, pw[i][j][k][0], pw[i][j][k][1], pw[i][j][k][2], pw[i][j][k][3],
                     cp[i][j][k][0], cp[i][j][k][1], cp[i][j][k][2], cp[i][j][k][3],
                     to!string(cp[i][j][k][] == pw[i][j][k][]));
        }
        // The copy is independent: mutate it and re-read the original.
        cp[0][0][0][0] = -99.0;
        writefln("dup afterMutate orig=%.17g copy=%.17g", pw[0][0][0][0], cp[0][0][0][0]);
        // An empty outer array is a legal input.
        number[4][][][] empty;
        auto ce = duplicatePw(empty);
        writefln("dup empty len=%d", ce.length);
    }

    writeln("done=1");
}
