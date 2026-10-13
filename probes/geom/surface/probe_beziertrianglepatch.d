// probes/geom/surface/probe_beziertrianglepatch.d — what
// geom/surface/beziertrianglepatch.d (723 lines) produces.
//
// BezierTrianglePatch is NOT a ParametricSurface (D:5-9 says so explicitly): it is a
// standalone class holding a triangular control net B of (n+1)(n+2)/2 points and a
// private jagged working space _b, evaluated by de Casteljau over barycentric (u,v,w)
// with w = 1-u-v. Around it the module adds one lattice index helper, four file
// writers, two point-cloud fitters that drive nm/nelmin, and one unittest.
//
// What each group measures, and why reasoning would be wrong without it:
//  * btp.tsi.*    toSingleIndex (D:51-56) cancels its own first parameter:
//                 n=i+j+k then m=n-i => m=j+k, so the index is f(j,k) only. Every
//                 de Casteljau read (D:115-117) depends on that, and the arithmetic
//                 is size_t, so a negative int argument wraps mod 2^64 (measured:
//                 D:239-240 pass i-1, which is >= 0 only because the loops start at
//                 i=1). Dumped for n=1,2,3,5 including the u/v/w neighbours.
//  * btp.size.*   (n+1)(n+2)/2 control points for n=0,1,2,3,5, and the count guard.
//  * btp.opCall.* de Casteljau itself: u*_b[r-1][uIdx] + v*[vIdx] + w*[wIdx] summed
//                 left-to-right (D:120-122), plus D:104's `if (w < 0.0) w = 0.0`
//                 clamp, which is observable at u+v>1 (the result stops being a
//                 convex combination) and at NaN (a NaN w fails `< 0.0` and is kept).
//  * btp.stale.*  opCall reads _b[0] on every call, so mutating B without
//                 updateWorkingSpace() (D:91-96) returns the OLD point. Both rows
//                 are dumped: a port that re-copies B in opCall cannot fail.
//  * btp.project.* projectPoint (D:129-199): the 11x11 sample scan with its `break`
//                 at u+v>1, the penalty function, and nelmin's returned x. A NaN
//                 target never converges (all fx NaN => std_dev NaN < tol is false),
//                 so the 10000-evaluation budget runs out and D:198 throws. That is
//                 the throw path, and its message carries n_fe/n_restart/f_min.
//  * btp.file.*   the four writers (D:206-345) as whole file bodies with newlines
//                 marked, because their contract with ParaView is byte-exact text:
//                 `%20.16e` rows, `%d` counts from `nCells = n^^2` (integerPow:
//                 measured typeof(3^^2) == int, value 9), the triangle-pattern A/B
//                 connectivity, offsets, types, and the nEdgePts=1 case where
//                 `double(i)/n` is 0.0/0.0 and the point row prints NaN.
//  * btp.fpc.* / *btp.fpcb.* the two bezierTriangleFromPointCloud overloads
//                 (D:348-485 by corners, D:487-665 by Bezier edge curves): the two
//                 initial-guess branches (user guess vs barycentric), the design
//                 vector order they append, dp = 0.01*max(...) of the side lengths
//                 (D's max is `a<b ? b : a` nested left-to-right, so NaN position
//                 matters), the fMin write-back of d[] into B, and the final
//                 updateWorkingSpace. A degree whose free points are all pinned
//                 (n=1 corners, n=2 edges) leaves d empty, so nelmin's
//                 replace_vertex(0) trips its own `i=%d seems not to be in the high
//                 points` Exception - dumped as the third throw family.
//  * btp.err.*    every `throw` site with WHICH type was thrown. Error and Exception
//                 are siblings in D, so these are printed as classinfo.name.
//
// This module provides no toString()/classString() and no derivative accessors
// (dpdu/dpdv), so nothing is probed for them; it is not in the ParametricSurface
// family and has no dup().
//
// Build and capture (ldc2 1.40.0; the gdtk tree is read-only, hence the shadow copy
// with @nogc stripped - a compile-time attribute change only, see
// probes/geom/surface/probe_parametricsurface.d):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface \
//     probes/geom/surface/probe_beziertrianglepatch.d -of=/tmp/probe_btp \
//     -od/tmp/probe_obj_btp
//   /tmp/probe_btp > baselines/e587b235/probes/geom_surface_beziertrianglepatch.txt
import std.algorithm.comparison : max;
import std.array : join, split;
import std.conv : text, to;
import std.file : read, remove;
import std.range : iota;
import std.stdio;

import geom.elements;
import geom.gpath.bezier;
import geom.surface.beziertrianglepatch;
import nm.number;

string body(string path) {
    return join(split(cast(string) read(path), "\n"), "<newline>");
}

void dump_v3(string tag, Vector3 v) {
    writefln("%s x=%.17g y=%.17g z=%.17g", tag, v.x, v.y, v.z);
}

void dump_uv(string tag, BezierTrianglePatch btp, double u, double v) {
    auto p = btp(u, v);
    writefln("%s u=%.17g v=%.17g x=%.17g y=%.17g z=%.17g", tag, u, v, p.x, p.y, p.z);
}

// The control net of a degree-n patch, laid out through the module's own index
// function so the probe writes the same slots de Casteljau reads.
Vector3[] net(int n, int kind) {
    Vector3[] B;
    B.length = (n + 1) * (n + 2) / 2;
    if (n == 0) {
        B[0] = (kind == 1) ? Vector3(1.25, -2.5, 3.75) : Vector3(2.0, 3.0, 4.0);
        return B;
    }
    foreach (i; iota(n, -1, -1)) {
        foreach (j; iota(n - i, -1, -1)) {
            auto k = n - (i + j);
            auto idx = toSingleIndex!()(i, j, k);
            if (kind == 0) {   // symmetric: barycentric in three distinct corners
                double a = double(i) / n;
                double b = double(j) / n;
                double c = double(k) / n;
                B[idx] = Vector3(2.0 * a, 0.5 * b, -1.25 * c);
            } else if (kind == 1) {   // asymmetric in i, j and k
                B[idx] = Vector3(1.7 * i - 0.3 * j + 0.11 * k, 0.25 * i + 1.1 * j - 0.7 * k,
                        -0.5 * i + 0.3 * j + 2.0 * k);
            } else if (kind == 2) {   // fully collapsed net
                B[idx] = Vector3(1.5, -2.5, 0.125);
            } else {   // one NaN component, everything downstream is NaN
                B[idx] = Vector3(0.5 * i, double.nan, 0.25 * k);
            }
        }
    }
    return B;
}

void dump_net(string tag, BezierTrianglePatch btp) {
    foreach (i, p; btp.B) {
        writefln("%s B[%d] x=%.17g y=%.17g z=%.17g", tag, i, p.x, p.y, p.z);
    }
}

// A net of exactly m points, whatever m is, so only the constructor's own count
// guard (D:67-70) decides whether the call succeeds.
Vector3[] pad(int m) {
    Vector3[] B;
    B.length = m;
    foreach (i; 0 .. m) {
        B[i] = Vector3(0.5 * i, 1.25 * i, -0.75 * i);
    }
    return B;
}

// Farin (2001) p.312 example, the one the D unittest uses.
BezierTrianglePatch farin() {
    Vector3[] B = [
        Vector3(6, 0, 9),
        Vector3(3, 3, 6), Vector3(3, 0, 0),
        Vector3(0, 6, 0), Vector3(0, 3, 0), Vector3(0, 0, 0)
    ];
    return new BezierTrianglePatch(B, 2);
}

void main()
{
    // ------------------------------------------------------------------
    // 1. the lattice index helper (D:51-56)
    // ------------------------------------------------------------------
    foreach (int n; [1, 2, 3, 5]) {
        foreach (i; iota(n, -1, -1)) {
            foreach (j; iota(n - i, -1, -1)) {
                auto k = n - (i + j);
                writefln("btp.tsi n=%d i=%d j=%d k=%d idx=%d u=%d v=%d w=%d", n, i, j, k,
                        toSingleIndex!()(i, j, k), toSingleIndex!()(i + 1, j, k),
                        toSingleIndex!()(i, j + 1, k), toSingleIndex!()(i, j, k + 1));
            }
        }
    }
    // size_t wraparound of a negative index argument (D:239-240 pass i-1)
    writefln("btp.tsi.neg idx=%d u=%d v=%d w=%d", toSingleIndex!()(-1, 0, 1),
            toSingleIndex!()(-1, -1, 0), toSingleIndex!()(0, -1, 2), toSingleIndex!()(-2, 1, 3));

    // std.algorithm.comparison.max (imported at D:42) is not the C library's fmax:
    // the 2-argument specialisation at comparison.d:1681 is `a < b ? b : a` and the
    // 3-argument general form at comparison.d:1644 evaluates max(max(a,b),c). Pinned
    // because D:149 feeds it into maxDist, hence into the penalty at D:154 and into
    // dp at D:457/D:641, which sets nelmin's initial simplex. A NaN's POSITION
    // changes the answer, and -0.0 wins a tie.
    {
        double nan = double.nan;
        writefln("btp.max nanform=%.17g", nan);
        writefln("btp.max max(1,2)=%.17g", max(1.0, 2.0));
        writefln("btp.max max(nan,1)=%.17g", max(nan, 1.0));
        writefln("btp.max max(1,nan)=%.17g", max(1.0, nan));
        writefln("btp.max max3(1,nan,2)=%.17g", max(1.0, nan, 2.0));
        writefln("btp.max max3(1,2,nan)=%.17g", max(1.0, 2.0, nan));
        writefln("btp.max max3(nan,1,2)=%.17g", max(nan, 1.0, 2.0));
        writefln("btp.max max(-0.0,0.0)=%.17g", max(-0.0, 0.0));
        writefln("btp.max max(0.0,-0.0)=%.17g", max(0.0, -0.0));
        writefln("btp.max max3(2,2,2)=%.17g", max(2.0, 2.0, 2.0));
    }

    // ------------------------------------------------------------------
    // 2. degree / lattice sizes and the control-point-count guard
    // ------------------------------------------------------------------
    foreach (int n; [0, 1, 2, 3, 5]) {
        auto btp = new BezierTrianglePatch(net(n, 0), n);
        writefln("btp.size n=%d nB=%d opCall(0.25,0.25)", btp.n, btp.B.length);
        dump_uv("btp.size.eval", btp, 0.25, 0.25);
    }
    foreach (int n; [1, 2, 3]) {
        foreach (int supplied; [0, 2, 3, 4, 6, 10]) {
            try {
                auto bad = new BezierTrianglePatch(pad(supplied), n);
                writefln("btp.ctor.noerror n=%d supplied=%d nB=%d", n, supplied, bad.B.length);
            }
            catch (Throwable t) {
                writefln("btp.ctor.err n=%d supplied=%d type=%s msg=%s", n, supplied,
                        t.classinfo.name, body_of(t.msg));
            }
        }
    }

    // ------------------------------------------------------------------
    // 3. opCall: the Farin patch on, at and just off the parameter triangle
    // ------------------------------------------------------------------
    auto btp = farin();
    foreach (uv; [[0.0, 0.0], [1.0, 0.0], [0.0, 1.0], [1.0 / 3, 1.0 / 3], [0.2, 0.3],
                  [0.5, 0.5], [0.25, 0.75], [1.1, 0.0], [0.0, 1.1], [0.6, 0.6],
                  [-0.1, -0.1], [-0.1, 0.2], [0.9999999999999999, 0.0],
                  [0.0, 0.0], [2.0, -1.0], [double.nan, 0.5]]) {
        dump_uv("btp.opCall.farin", btp, uv[0], uv[1]);
    }
    dump_uv("btp.opCall.farin.nanv", btp, 0.5, double.nan);

    // symmetric / asymmetric / collapsed / NaN nets, degrees 1..5
    foreach (int n; [1, 2, 3, 5]) {
        auto s = new BezierTrianglePatch(net(n, 0), n);
        auto a = new BezierTrianglePatch(net(n, 1), n);
        foreach (uv; [[0.0, 0.0], [1.0 / 3, 1.0 / 3], [0.2, 0.3], [0.7, 0.3]]) {
            dump_uv("btp.opCall.sym", s, uv[0], uv[1]);
            dump_uv("btp.opCall.asym", a, uv[0], uv[1]);
        }
        dump_uv("btp.opCall.sym.corner0", s, 1.0, 0.0);
        dump_uv("btp.opCall.asym.corner0", a, 1.0, 0.0);
        writefln("btp.opCall.asym str=%s", to!string(a(0.2, 0.3)));
    }
    auto coll = new BezierTrianglePatch(net(3, 2), 3);
    auto nanp = new BezierTrianglePatch(net(2, 3), 2);
    foreach (uv; [[0.0, 0.0], [1.0 / 3, 1.0 / 3], [1.1, 0.0], [0.25, 0.25]]) {
        dump_uv("btp.opCall.collapsed", coll, uv[0], uv[1]);
    }
    foreach (uv; [[0.0, 0.0], [1.0 / 3, 1.0 / 3], [1.1, 0.0]]) {
        dump_uv("btp.opCall.nanet", nanp, uv[0], uv[1]);
    }
    // edge collapse: two corners of a degree-2 patch coincide (B[3]==B[5])
    Vector3[] half = [Vector3(6, 0, 9), Vector3(3, 3, 6), Vector3(3, 0, 0), Vector3(0, 0, 0),
        Vector3(0, 3, 0), Vector3(0, 0, 0)];
    auto edge = new BezierTrianglePatch(half, 2);
    foreach (uv; [[0.0, 0.0], [0.5, 0.0], [0.0, 0.5], [1.0 / 3, 1.0 / 3]]) {
        dump_uv("btp.opCall.halvedge", edge, uv[0], uv[1]);
    }

    // ------------------------------------------------------------------
    // 4. the working space is a snapshot: stale vs updateWorkingSpace()
    // ------------------------------------------------------------------
    auto st = farin();
    dump_uv("btp.stale.before", st, 0.2, 0.3);
    st.B[0].x = 7.0;
    st.B[3].y = -1.0;
    dump_uv("btp.stale.afterMutation", st, 0.2, 0.3);
    st.updateWorkingSpace();
    dump_uv("btp.stale.afterUpdate", st, 0.2, 0.3);
    dump_uv("btp.stale.recheck", st, 0.2, 0.3);

    // ------------------------------------------------------------------
    // 5. projectPoint
    // ------------------------------------------------------------------
    {
        Vector3 q;
        double uu = 0.0, vv = 0.0;
        auto p = btp(1.0 / 3, 1.0 / 3);
        auto dist = btp.projectPoint(p, q, uu, vv);
        writefln("btp.project.onsurf dist=%.17g u=%.17g v=%.17g", dist, uu, vv);
        dump_v3("btp.project.onsurf.q", q);
    }
    {
        Vector3 q;
        double uu = 0.0, vv = 0.0;
        auto dist = btp.projectPoint(Vector3(2.0, 2.0, 3.0), q, uu, vv);
        writefln("btp.project.near dist=%.17g u=%.17g v=%.17g", dist, uu, vv);
        dump_v3("btp.project.near.q", q);
    }
    {
        Vector3 q;
        double uu = 0.0, vv = 0.0;
        auto dist = btp.projectPoint(Vector3(-4.0, 9.0, 22.0), q, uu, vv);
        writefln("btp.project.far dist=%.17g u=%.17g v=%.17g", dist, uu, vv);
        dump_v3("btp.project.far.q", q);
    }
    {
        // corner: the sample scan starts at (0,0), which is a corner of the patch
        Vector3 q;
        double uu = 0.0, vv = 0.0;
        auto dist = btp.projectPoint(btp(1.0, 0.0), q, uu, vv);
        writefln("btp.project.corner dist=%.17g u=%.17g v=%.17g", dist, uu, vv);
    }
    {
        // the collapsed patch: the objective is constant, so nelmin converges at once
        Vector3 q;
        double uu = 0.0, vv = 0.0;
        auto dist = coll.projectPoint(Vector3(0.0, 0.0, 0.0), q, uu, vv);
        writefln("btp.project.collapsed dist=%.17g u=%.17g v=%.17g", dist, uu, vv);
        dump_v3("btp.project.collapsed.q", q);
    }
    {
        // NaN target: every fx is NaN, so the budget is exhausted and D:198 throws
        Vector3 q;
        double uu = 0.0, vv = 0.0;
        try {
            auto dist = btp.projectPoint(Vector3(double.nan, 1.0, 2.0), q, uu, vv);
            writefln("btp.project.nan.nothrow dist=%.17g", dist);
        }
        catch (Throwable t) {
            writefln("btp.project.nan.err type=%s msg=%s", t.classinfo.name,
                    join(split(t.msg, "\n"), "<newline>"));
        }
        // same target, but told to return on failure instead of throwing
        double uu2 = 0.0, vv2 = 0.0;
        auto d2 = btp.projectPoint(Vector3(double.nan, 1.0, 2.0), q, uu2, vv2, true);
        writefln("btp.project.nan.returned dist=%.17g u=%.17g v=%.17g", d2, uu2, vv2);
        dump_v3("btp.project.nan.q", q);
    }

    // ------------------------------------------------------------------
    // 6. the four writers, as whole file bodies
    // ------------------------------------------------------------------
    {
        string p1 = "/tmp/probe_btp_ctrl.txt";
        writeBezierTriangleCtrlPtsAsText(btp, p1);
        writefln("btp.file.ctrlpts len=%d", (cast(string) read(p1)).length);
        writefln("btp.file.ctrlpts.body=%s", body(p1));
        writeBezierTriangleCtrlPtsAsText(coll, p1);
        writefln("btp.file.ctrlpts.collapsed.body=%s", body(p1));

        string p2 = "/tmp/probe_btp_cvtk.vtk";
        foreach (int n; [1, 2, 3, 5]) {
            auto bp = new BezierTrianglePatch(net(n, 1), n);
            writeBezierTriangleCtrlPtsAsVtkXml(bp, p2);
            writefln("btp.file.ctrlvtk n=%d len=%d", n, (cast(string) read(p2)).length);
            writefln("btp.file.ctrlvtk n=%d.body=%s", n, body(p2));
        }

        string p3 = "/tmp/probe_btp_avtk.vtk";
        foreach (int ne; [1, 2, 3, 5]) {
            writeBezierTriangleAsVtkXml(btp, p3, ne);
            writefln("btp.file.trivtk ne=%d len=%d", ne, (cast(string) read(p3)).length);
            writefln("btp.file.trivtk ne=%d.body=%s", ne, body(p3));
        }
        // same writer on the NaN net: %20.16e of a NaN is what the file says
        writeBezierTriangleAsVtkXml(nanp, p3, 3);
        writefln("btp.file.trivtk.nanet.body=%s", body(p3));

        string p4 = "/tmp/probe_btp.dat";
        foreach (int ne; [1, 3, 5]) {
            writeBezierTriangleAsDat(btp, p4, ne);
            writefln("btp.file.dat ne=%d len=%d", ne, (cast(string) read(p4)).length);
            writefln("btp.file.dat ne=%d.body=%s", ne, body(p4));
        }
        remove(p1); remove(p2); remove(p3); remove(p4);
    }

    // ------------------------------------------------------------------
    // 7. bezierTriangleFromPointCloud, corner form (D:348-485)
    // ------------------------------------------------------------------
    {
        // the D unittest's point cloud: a 4x4-ish barycentric sampling
        auto src = farin();
        Vector3[] points;
        double du = 1.0 / 4;
        double dv = du;
        foreach (uu; iota(0.0, 1.0 + 0.5 * du, du)) {
            foreach (vv; iota(0.0, (1.0 - uu) + 0.5 * dv, dv)) {
                if ((uu + vv) > 1.0) continue;
                points ~= src(uu, vv);
            }
        }
        writefln("btp.fpc.cloud n=%d", points.length);
        foreach (i, p; points) {
            writefln("btp.fpc.cloud[%d] x=%.17g y=%.17g z=%.17g", i, p.x, p.y, p.z);
        }
        auto p0 = Vector3(6, 0, 9);
        auto p1 = Vector3(0, 6, 0);
        auto p2 = Vector3(0, 0, 0);
        Vector3[] B = [
            Vector3(6, 0, 9),
            Vector3(3.1, 2.9, 6.2), Vector3(2.8, 0.15, -0.05),
            Vector3(0, 6, 0), Vector3(-0.3, 2.85, -0.1), Vector3(0, 0, 0)
        ];
        auto guess = new BezierTrianglePatch(B, 2);
        auto fitA = bezierTriangleFromPointCloud(points, p0, p1, p2, 2, guess);
        writefln("btp.fpc.guess n=%d nB=%d", fitA.n, fitA.B.length);
        dump_net("btp.fpc.guess", fitA);
        dump_uv("btp.fpc.guess.eval", fitA, 0.2, 0.3);
        dump_uv("btp.fpc.guess.eval0", fitA, 0.0, 0.0);
        dump_uv("btp.fpc.guess.eval1", fitA, 1.0, 0.0);
        dump_uv("btp.fpc.guess.eval2", fitA, 0.0, 1.0);

        // no guess supplied -> barycentric distribution branch
        auto fitB = bezierTriangleFromPointCloud(points, p0, p1, p2, 2, null);
        writefln("btp.fpc.noguess n=%d nB=%d", fitB.n, fitB.B.length);
        dump_net("btp.fpc.noguess", fitB);
        dump_uv("btp.fpc.noguess.eval", fitB, 0.2, 0.3);

        // a guess of the wrong degree is ignored (the `initGuess.n == n` test)
        auto wrong = new BezierTrianglePatch(net(3, 1), 3);
        auto fitC = bezierTriangleFromPointCloud(points, p0, p1, p2, 2, wrong);
        dump_net("btp.fpc.wrongsizenet", fitC);

        // empty cloud: the objective is identically 0, so it converges immediately
        Vector3[] none;
        auto fitD = bezierTriangleFromPointCloud(none, p0, p1, p2, 2, guess);
        dump_net("btp.fpc.emptynet", fitD);

        // degree 1 pins every control point, so d is empty and nelmin's
        // replace_vertex(0) rejects its own index
        try {
            auto fitE = bezierTriangleFromPointCloud(points, p0, p1, p2, 1, null);
            dump_net("btp.fpc.deg1.nothrow", fitE);
        }
        catch (Throwable t) {
            writefln("btp.fpc.deg1.err type=%s msg=%s", t.classinfo.name, join(split(t.msg, "\n"), "<newline>"));
        }
    }

    // ------------------------------------------------------------------
    // 8. bezierTriangleFromPointCloud, Bezier-edge form (D:487-665)
    // ------------------------------------------------------------------
    {
        // The three shared lattice corners are idx 0, idx n(n+1)/2 and idx $-1, and
        // D:541-553 writes b0 over $-n-1 .. $-1, b1 over 0,2,5,.. and b2 over 0,1,3,..
        // so the guards at D:520-528 require b0.B[0]==b2.B[$-1] (corner Y),
        // b0.B[$-1]==b1.B[$-1] (corner Z) and b1.B[0]==b2.B[0] (corner X).
        Vector3 ca = Vector3(0, 0, 0);
        Vector3 cb = Vector3(3, 0, 1);
        Vector3 cc = Vector3(0, 3, 0);
        auto b0 = new Bezier([cb, Vector3(2.0, 1.0, 0.0), Vector3(0.5, 2.0, 0.5), cc]);
        auto b1 = new Bezier([ca, Vector3(1.0, 0.0, 0.4), Vector3(2.0, 0.0, 0.1), cc]);
        auto b2 = new Bezier([ca, Vector3(1.0, 1.0, -0.2), Vector3(0.4, 2.0, 0.3), cb]);

        Vector3[] points;
        auto src = farin();
        foreach (uu; iota(0.0, 1.0 + 0.25, 0.25)) {
            foreach (vv; iota(0.0, (1.0 - uu) + 0.125, 0.25)) {
                if ((uu + vv) > 1.0) continue;
                points ~= src(uu, vv);
            }
        }
        bool ok = false;
        auto fit = bezierTriangleFromPointCloud(points, b0, b1, b2, 3, null, ok);
        writefln("btp.fpcb.n3 ok=%s nB=%d", to!string(ok), fit.B.length);
        dump_net("btp.fpcb.n3", fit);
        dump_uv("btp.fpcb.n3.eval", fit, 0.2, 0.3);

        auto g = new BezierTrianglePatch(net(3, 1), 3);
        bool ok2 = false;
        auto fit2 = bezierTriangleFromPointCloud(points, b0, b1, b2, 3, g, ok2);
        writefln("btp.fpcb.n3guess ok=%s", to!string(ok2));
        dump_net("btp.fpcb.n3guess", fit2);

        // degree 2 has no interior control point, so d is empty again
        bool ok3 = false;
        try {
            auto b0d2 = new Bezier([cb, Vector3(0.2, 1.5, 0.0), cc]);
            auto b1d2 = new Bezier([ca, Vector3(1.5, 0.0, 0.2), cc]);
            auto b2d2 = new Bezier([ca, Vector3(1.2, 1.2, 0.1), cb]);
            auto fitE = bezierTriangleFromPointCloud(points, b0d2, b1d2, b2d2, 2, null, ok3);
            writefln("btp.fpcb.n2.nothrow ok=%s", to!string(ok3));
            dump_net("btp.fpcb.n2", fitE);
        }
        catch (Throwable t) {
            writefln("btp.fpcb.n2.err type=%s msg=%s", t.classinfo.name, join(split(t.msg, "\n"), "<newline>"));
        }

        // the three end-point coincidence guards (D:520-528)
        auto b0shift = new Bezier([Vector3(0.2, 0.0, 0.0), Vector3(2.0, 1.0, 0.0),
            Vector3(0.5, 2.0, 0.5), cc]);
        auto b1shift = new Bezier([ca, Vector3(1.0, 0.0, 0.4), Vector3(2.0, 0.0, 0.1),
            Vector3(3.5, 0.0, 1.0)]);
        auto b2shift = new Bezier([Vector3(0.0, 0.25, 0.0), Vector3(1.0, 1.0, -0.2),
            Vector3(0.4, 2.0, 0.3), cb]);
        bool ok4 = false;
        try {
            auto x = bezierTriangleFromPointCloud(points, b0shift, b1, b2, 3, null, ok4);
            writefln("btp.fpcb.mismatch.b0b2.nothrow n=%d", x.n);
        }
        catch (Throwable t) {
            writefln("btp.fpcb.mismatch.b0b2 type=%s msg=%s", t.classinfo.name,
                    join(split(t.msg, "\n"), "<newline>"));
        }
        try {
            auto x = bezierTriangleFromPointCloud(points, b0, b1shift, b2, 3, null, ok4);
            writefln("btp.fpcb.mismatch.b0b1.nothrow n=%d", x.n);
        }
        catch (Throwable t) {
            writefln("btp.fpcb.mismatch.b0b1 type=%s msg=%s", t.classinfo.name,
                    join(split(t.msg, "\n"), "<newline>"));
        }
        try {
            auto x = bezierTriangleFromPointCloud(points, b0, b1, b2shift, 3, null, ok4);
            writefln("btp.fpcb.mismatch.b1b2.nothrow n=%d", x.n);
        }
        catch (Throwable t) {
            writefln("btp.fpcb.mismatch.b1b2 type=%s msg=%s", t.classinfo.name,
                    join(split(t.msg, "\n"), "<newline>"));
        }
    }
}

// newlines inside a thrown message are marked so one message stays one row
string body_of(string s) {
    return join(split(s, "\n"), "<newline>");
}
