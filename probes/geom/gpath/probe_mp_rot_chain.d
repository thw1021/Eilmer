// probes/geom/gpath/probe_mp_rot_chain.d — decompose the RotatedAboutZAxisPath
// chain that reports/backlog.md T-910 could not explain.
//
// T-910 asks a narrow question: geom_gpath_modifiedpath.txt has 84 lines that the
// replay accepted only through an absolute tolerance band, and the ratio printed
// with them reached 3.0e6 "ULP of row scale". Either the band is measured against
// the wrong scale, or modifiedpath has a real transcription deviation. This probe
// answers it by construction rather than by argument: it recomputes
// Vector3.rotate_about_zaxis (geom/elements/vector3.d:521-528) one operator at a
// time, for exactly the arguments the mp.rot rows pass through it, and prints every
// intermediate both as %.17g and as its 64-bit pattern. The first line whose bit
// pattern differs identifies the operator; the magnitude of the gap says whether a
// 1-ULP transcendental divergence is enough to produce the observed residual.
//
// Two controls come along:
//
//  * `mp.chain.wrapper` re-derives the same number through the real RotatedAboutZAxisPath
//    object, so the hand-written chain cannot drift away from the code under test.
//
//  * `ctl.*` rows measure Phobos-vs-glibc divergence directly, on the argument sets
//    this chain actually uses (theta0, theta, and the (y,x) pairs feeding atan2), so
//    the "1 ULP of theta amplified by r into x" claim is a counted number and not a
//    story.
//
// `bits` uses the raw IEEE-754 pattern so equality is bit equality, independent of
// how the two standard libraries choose to print -0.0 or a subnormal.
import std.format;
import std.math;
import std.conv;
import std.stdio;

import geom.elements;
import geom.gpath.path;
import geom.gpath.arc;
import geom.gpath.modifiedpath;

// The exact observable form of D's rotate_about_zaxis, split into the individual
// statements of vector3.d:523-526 so each can be printed.
string bits(double x)
{
    return format("%016x", *cast(ulong*)&x);
}

string val(double x)
{
    return format("%.17g", x);
}

void emit(const char[] tag, double x)
{
    writefln("%s v=%s bits=%s", tag, val(x), bits(x));
}

double[] t_sweep()
{
    return [0.0, 0.0005, 0.001, 0.05, 0.25, 0.5, 0.75, 0.95, 0.999, 0.9995, 1.0];
}

void main()
{
    // The same arc as probe_modifiedpath.d:163 (a = (2,2,0), b = (1,2,1), c = (1,2,0)),
    // and the same angle: PI computed as D's 80-bit real, divided by 4, rounded on the
    // way into the double parameter of RotatedAboutZAxisPath.
    Vector3 a = Vector3([2.0, 2.0, 0.0]);
    Vector3 b = Vector3([1.0, 2.0, 1.0]);
    Vector3 c = Vector3([1.0, 2.0, 0.0]);
    Path arc = new Arc(a, b, c);
    double dtheta = PI / 4;
    emit("ctl.dtheta", dtheta);

    foreach (t; t_sweep()) {
        // The tags inside the loop repeat once per sweep point, so each group opens
        // with the t that produced it; a diff then names a row instead of a tag.
        writefln("mp.chain.row t=%s", val(t));
        // Step 0: the inner path value, which is the input of the chain. The mp.rot
        // cmp rows already show these three numbers matching the inner arc bit for
        // bit; re-printing them here localises the divergence below, not above.
        Vector3 p = arc(t);
        emit("mp.chain.in.x", p.x);
        emit("mp.chain.in.y", p.y);
        emit("mp.chain.in.z", p.z);

        // Step 1: theta = atan2(y, x) + dtheta        (vector3.d:523)
        double theta0 = atan2(p.y, p.x);
        emit("mp.chain.atan2", theta0);
        double theta = theta0 + dtheta;
        emit("mp.chain.theta", theta);

        // Step 2: r = sqrt(x*x + y*y)                  (vector3.d:524)
        double xx = p.x * p.x;
        emit("mp.chain.xx", xx);
        double yy = p.y * p.y;
        emit("mp.chain.yy", yy);
        double sum = xx + yy;
        emit("mp.chain.sum", sum);
        double r = sqrt(sum);
        emit("mp.chain.r", r);

        // Step 3: x = r*cos(theta), y = r*sin(theta)   (vector3.d:525-526)
        double cost = cos(theta);
        emit("mp.chain.cos", cost);
        double sint = sin(theta);
        emit("mp.chain.sin", sint);
        double xn = r * cost;
        emit("mp.chain.rxcos", xn);
        double yn = r * sint;
        emit("mp.chain.rysin", yn);

        // Step 4: the same through the real code path, so steps 1-3 are tied to it.
        Vector3 q = Vector3(p);
        q.rotate_about_zaxis(dtheta);
        emit("mp.chain.wrapper.x", q.x);
        emit("mp.chain.wrapper.y", q.y);
        emit("mp.chain.wrapper.z", q.z);

        // Step 5: and through the Path wrapper, which is what dump_transformed prints.
        Path rot = new RotatedAboutZAxisPath(arc, dtheta);
        Vector3 w = rot(t);
        emit("mp.chain.path.x", w.x);
        emit("mp.chain.path.y", w.y);
        emit("mp.chain.path.z", w.z);
    }

    // Control: Phobos vs the C library on the arguments this chain uses. The C++
    // mirror of these loops calls std::atan2/std::cos/std::sin with the same decimal
    // literals, so the counts are directly comparable line by line.
    foreach (double y; [2.0, 1.9999996915748783, 1.7071067811865475, 1.0015707956808308,
             0.70710678118654746, 0.5, 0.1, 3.0, 7.7, 0.000123456789]) {
        foreach (double x; [1.7071067811865475, 2.0, 1.0, 0.25, 1e10, 1e-10, 3.141592653589793]) {
            emit(format("ctl.atan2 %s %s", val(y), val(x)), atan2(y, x));
        }
    }
    foreach (double th; [0.0, 0.1, 0.7853981633974483, 1.5707963267948966,
             1.6522047499950325, 2.356194490192345, 3.141592653589793, 4.0,
             5.5, 6.283185307179586]) {
        emit(format("ctl.cos %s", val(th)), cos(th));
        emit(format("ctl.sin %s", val(th)), sin(th));
    }
    // Same angle grid with the 1-ULP neighbours, which is the shape of the divergence
    // being claimed: one ULP in, a bounded number out.
    foreach (double th; [1.6522047499950325, 1.6522047499950327, 1.6522047499950329]) {
        emit(format("ctl.ulp.cos %s", val(th)), cos(th));
        emit(format("ctl.ulp.sin %s", val(th)), sin(th));
    }

    // The arithmetic the band model assumes: propagate a 1-ULP change in theta and in
    // r through x = r*cos(theta), for the t = 0.5 arguments, and print the products so
    // the predicted absolute residual in x is directly readable.
    double p05x = 1.7071067811865475, p05y = 2.0;
    double th05 = atan2(p05y, p05x) + dtheta;
    double r05 = sqrt(p05x * p05x + p05y * p05y);
    emit("ctl.model.theta", th05);
    emit("ctl.model.r", r05);
    emit("ctl.model.x", r05 * cos(th05));
    emit("ctl.model.x.plus1ulp", r05 * cos(nextafter(th05, 2.0)));
    emit("ctl.model.y", r05 * sin(th05));
    emit("ctl.model.y.plus1ulp", r05 * sin(nextafter(th05, 2.0)));
    writefln("ctl.model.dtheta.ulp=%s", val(nextafter(th05, 2.0) - th05));
}
