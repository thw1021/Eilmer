// probes/geom/gpath/probe_polynomial.d — what geom/gpath/polynomial.d produces.
//
// Polynomial has no unittest block in D, so everything here is measured from the
// class: the least-squares coefficients (polynomial.d:89-104), the curve, and the
// two constructors that must agree with each other.
//
// THE IMPORTANT ROW IS "poly.deriv.*". derivative_polynomial (polynomial.d:115-122)
// sums `C[i] * i * x^^(i-1)` from i = 0, and the loop index is a size_t, so the
// i == 0 term is C[0] * 0 * x^^18446744073709551615. Measured with ldc2: `double
// ^^ size_t` returns double and is the same value as pow(x, 1.8446744073709552e19),
// which is +inf for |x| > 1, 0 for |x| < 1 and 1 for x == 1. So dpdt() is
//   -inf * 0 = -nan  whenever the x range exceeds 1,
//   finite otherwise.
// That is D's behaviour and the port keeps it (see the note in
// src/geom/gpath/polynomial.hpp), which is why the probe dumps a range inside and
// a range outside |x| = 1 and prints the raw %.17g (nan signs included: D prints
// "-nan" for a negative quiet NaN, measured, so the sign is part of the text).
//
// See probe_path.d for the reference build recipe (explicit module file list
// against the read-only gdtk tree; no @nogc edits, no `-i`). In short:
//   G=/home/tang/packages/gdtk/src
//   LIST=$(ls $G/geom/elements/*.d $G/geom/gpath/*.d $G/geom/grid/*.d \
//             $G/geom/misc/*.d $G/geom/surface/*.d $G/geom/volume/*.d $G/nm/*.d \
//             $G/ntypes/*.d $G/geom/package.d | grep -v '_demo\.d')
//   ROOTS=""; for f in $LIST; do
//     grep -qE '^[[:space:]]*(void|int)[[:space:]]+main[[:space:]]*\(' "$f" || \
//       ROOTS="$ROOTS $f"; done
//   ROOTS="$ROOTS $G/geom/geometry_exception.d $G/extern/gzip/gzip.d"
//   /home/tang/packages/dlang/bin/ldc2 -w -g -dip1008 -I$G -I$G/extern/gzip \
//     -od/tmp/gp/obj -of=/tmp/gp/probe_polynomial \
//     probes/geom/gpath/probe_polynomial.d probes/geom/gpath/pathdump.d $ROOTS
//   /tmp/gp/probe_polynomial > baselines/e587b235/probes/geom_gpath_polynomial.txt
import std.conv;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.gpath.path;
import geom.gpath.polynomial;
import nm.number;
import pathdump;

void dump_coeffs(const char[] tag, const number[] C)
{
    foreach (i; 0 .. C.length) {
        writefln("%s C[%d]=%.17g", tag, i, C[i]);
    }
    writefln("%s ncoef=%d", tag, C.length);
}

void main()
{
    // A parabola through three points inside |x| <= 1.
    auto p1 = new Polynomial([Vector3(-1.0, 1.0), Vector3(0.0, 0.0), Vector3(1.0, 1.0)]);
    dump_path("poly.parab", p1);
    dump_coeffs("poly.parab", p1.C);
    writefln("poly.parab xrange a=%.17g b=%.17g", p1.P[0].x, p1.P[$ - 1].x);

    // The same shape scaled so that x runs from -2 to 2: the i == 0 wraparound term
    // then multiplies 0 by +inf.
    auto p2 = new Polynomial([Vector3(-2.0, 4.0), Vector3(0.0, 0.0), Vector3(2.0, 4.0)]);
    dump_path("poly.wide", p2);
    dump_coeffs("poly.wide", p2.C);
    foreach (t; [0.0, 0.25, 0.5, 1.0]) {
        Vector3 d = p2.dpdt(t);
        writefln("poly.deriv t=%.17g x=%.17g y=%.17g z=%.17g", t, d.x, d.y, d.z);
        Vector3 d0 = p1.dpdt(t);
        writefln("poly.deriv.inrange t=%.17g x=%.17g y=%.17g z=%.17g", t, d0.x, d0.y, d0.z);
    }

    // A quartic-ish fit through five points (least squares over-determined by one).
    auto p3 = new Polynomial([Vector3(0.0, 1.0), Vector3(0.25, 1.0954), Vector3(0.5, 1.1892),
                              Vector3(0.75, 1.2649), Vector3(1.0, 1.3229)]);
    dump_path("poly.five", p3);
    dump_coeffs("poly.five", p3.C);

    // The coefficients constructor (polynomial.d:34-43): P is rebuilt by
    // evaluating the polynomial at x0 and x1.
    number[] cs = [1.0, 2.0, -0.5, 0.125];
    auto p4 = new Polynomial(cs, 0.0, 1.0);
    dump_path("poly.fromC", p4);
    dump_coeffs("poly.fromC", p4.C);
    writefln("poly.fromC P0 x=%.17g y=%.17g P1 x=%.17g y=%.17g",
             p4.P[0].x, p4.P[0].y, p4.P[1].x, p4.P[1].y);

    // The (P, C) constructor, which checks the points against the coefficients
    // (polynomial.d:44-56). Matching data first, then the mismatching case.
    auto p5 = new Polynomial(p4.P, cs);
    dump_path("poly.fromPC", p5);
    number[] wrong = [1.0, 2.0, -0.5, 0.25];
    dump_error("poly.err.mismatch", new Polynomial(p4.P, wrong));
    dump_error("poly.err.nocoef", new Polynomial(cast(number[])[], 0.0, 1.0));
    dump_error("poly.err.noPoints", new Polynomial(cast(Vector3[])[]));
    dump_error("poly.err.onePoint", new Polynomial([Vector3(1.0, 2.0)]));

    // dup() is the interesting one: polynomial.d:62-65 implements it as
    // `new Polynomial(P, C)`, which re-runs the (P, C) constructor's
    // point-versus-coefficient check (polynomial.d:50-54). For an exactly
    // determined interpolation the check passes; for a least-squares fit with more
    // points than coefficients the residual makes `point.y != P(x).y` true, so
    // dup() THROWS for the fits below. That is D's behaviour, so each case is
    // recorded with the error catcher instead of being fixed.
    dump_error("poly.dup.exact", p1.dup());
    dump_error("poly.dup.wide", p2.dup());
    dump_error("poly.dup.five", p3.dup());
    dump_error("poly.dup.fromC", p4.dup());
    writefln("poly.dup.exactok t=%.17g x=%.17g y=%.17g z=%.17g", 0.5,
             p1.dup()(0.5).x, p1.dup()(0.5).y, p1.dup()(0.5).z);
    dump_path("poly.copy", new Polynomial(p3));

    // A flat polynomial (all points at the same y) and one whose x values repeat,
    // which makes the least-squares matrix rank deficient. The second case may or
    // may not be survivable for nm.bbla's solve, so it is guarded: whatever D does
    // (produce coefficients or throw) is what the row records.
    auto p6 = new Polynomial([Vector3(0.0, 1.0), Vector3(1.0, 1.0), Vector3(2.0, 1.0)]);
    dump_path("poly.flat", p6);
    dump_coeffs("poly.flat", p6.C);
    dump_error("poly.dup.flat", p6.dup());
    try {
        auto p7 = new Polynomial([Vector3(0.0, 1.0), Vector3(0.0, 2.0), Vector3(1.0, 3.0)]);
        dump_path("poly.rankdeficient", p7);
        dump_coeffs("poly.rankdeficient", p7.C);
    } catch (Throwable e) {
        writefln("poly.rankdeficient threw msg=%s", e.msg);
    }
}
