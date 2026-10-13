// probes/geom/gpath/probe_arc.d — what geom/gpath/arc.d produces.
//
// Arc is where the geometry goes off the axis: it builds a local frame from the
// three points (arc.d:71-90), rotates in that frame and transforms back, so the
// order of the cross products and the normalize() calls are all observable. Arc
// also overrides length() with the *geometric* length, while Arc3 inherits it —
// both are printed next to Path's 100-chord sum, because Polyline's breakpoint
// table divides by whichever one its segment reports.
//
// The three Error paths (arc.d:69, :78, :89) and Arc3's two (arc.d:112, :137) are
// dumped with their message text, since geom/grid/paver.d catches on it.
//
// See probe_path.d for the reference build recipe (explicit module file list,
// read-only gdtk tree, no @nogc edits). In short:
//   G=/home/tang/packages/gdtk/src
//   LIST=$(ls $G/geom/elements/*.d $G/geom/gpath/*.d $G/geom/grid/*.d \
//             $G/geom/misc/*.d $G/geom/surface/*.d $G/geom/volume/*.d $G/nm/*.d \
//             $G/ntypes/*.d $G/geom/package.d | grep -v '_demo\.d')
//   ROOTS=""; for f in $LIST; do
//     grep -qE '^[[:space:]]*(void|int)[[:space:]]+main[[:space:]]*\(' "$f" || \
//       ROOTS="$ROOTS $f"; done
//   ROOTS="$ROOTS $G/geom/geometry_exception.d $G/extern/gzip/gzip.d"
//   /home/tang/packages/dlang/bin/ldc2 -w -g -dip1008 -I$G -I$G/extern/gzip \
//     -od/tmp/gp/obj -of=/tmp/gp/probe_arc \
//     probes/geom/gpath/probe_arc.d probes/geom/gpath/pathdump.d $ROOTS
//   /tmp/gp/probe_arc > baselines/e587b235/probes/geom_gpath_arc.txt
import std.conv;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.gpath.path;
import geom.gpath.arc;
import nm.number;
import pathdump;

void main()
{
    // arc.d:166-174 — the quarter arc used by the unittest and by gpath_demo.
    auto a = Vector3([2.0, 2.0, 0.0]);
    auto b = Vector3([1.0, 2.0, 1.0]);
    auto c = Vector3([1.0, 2.0, 0.0]);
    auto abc = new Arc(a, b, c);
    dump_path("arc.qtr", abc);

    // evaluate_position_and_length directly: the out parameters are the raw
    // observables (arc.d:58-101), including L, which is the arc length at t = 1
    // and is scaled by t only after L has been computed.
    foreach (t; [0.0, 0.25, 0.5, 1.0, 1.5, -0.5]) {
        Vector3 loc;
        number L;
        abc.evaluate_position_and_length(t, loc, L);
        writefln("arc.eval t=%.17g L=%.17g x=%.17g y=%.17g z=%.17g", t, L, loc.x, loc.y, loc.z);
    }

    // The geometric length against Path's chord sum.
    writefln("arc.length geom=%.17g chords=%.17g", abc.length(), abc.Path.length());
    writefln("arc.pl half geom=%.17g chords=%.17g",
             abc.partial_length(0.0, 0.5), abc.Path.partial_length(0.0, 0.5));

    // Arc3 (three points, centre solved by least squares — arc.d:108-143).
    auto d = abc(0.5);
    auto adb = new Arc3(a, d, b);
    dump_path("arc3.thru", adb);
    writefln("arc3.thru.length geom=%.17g chords=%.17g", adb.length(), adb.Path.length());

    // The centre solve itself (arc.d:117-140): n = cross(m-a, m-b), the two
    // bisectors, the 3x2 least-squares solution s_am/s_mb and the two centre
    // estimates that must agree to 1e-9. c is the only part of it that survives
    // into the base object (Arc3's `m` is module-private in D, so the middle point
    // is echoed from the caller's own value), so each triple prints its solved
    // centre and the three radii it implies; a wrong bisector sign or a swapped
    // lsqsolve column changes these before it changes any point on the curve.
    writefln("arc3.centre tag=thru a=%s m=%s b=%s c=%s", a.toString(), d.toString(),
             b.toString(), adb.c.toString());
    writefln("arc3.centre tag=thru cx=%.17g cy=%.17g cz=%.17g", adb.c.x, adb.c.y, adb.c.z);
    {
        Vector3 ca = adb.a - adb.c;
        Vector3 cm = d - adb.c;
        Vector3 cb = adb.b - adb.c;
        writefln("arc3.centre tag=thru rad a=%.17g m=%.17g b=%.17g", geom.abs(ca),
                 geom.abs(cm), geom.abs(cb));
    }
    // The shallow arc of arc.d:176-183, and two triples that leave the xy plane so
    // the plane normal n and the two tangent vectors are not axis aligned.
    {
        auto sh = new Arc3(Vector3(0.0, 1.0), Vector3(0.5, 1.2), Vector3(1.0, 1.0));
        writefln("arc3.centre tag=shallow a=%s m=%s b=%s c=%s", Vector3(0.0, 1.0).toString(),
                 Vector3(0.5, 1.2).toString(), Vector3(1.0, 1.0).toString(), sh.c.toString());
        writefln("arc3.centre tag=shallow cx=%.17g cy=%.17g cz=%.17g", sh.c.x, sh.c.y, sh.c.z);
        Vector3 q1 = Vector3(0.0, 0.0, 0.0);
        // q2 is deliberately off the right angle at the midpoint of q1-q3: the
        // triple (0,0,0), (1,1,0), (2,0,1) lies on an *exact* semicircle, which
        // makes cross(ca, cb) zero and arc.d:75-79 rejects it (measured).
        Vector3 q2 = Vector3(0.8, 1.0, 0.25);
        Vector3 q3 = Vector3(2.0, 0.0, 1.0);
        auto sk = new Arc3(q1, q2, q3);
        writefln("arc3.centre tag=skew a=%s m=%s b=%s c=%s", q1.toString(), q2.toString(),
                 q3.toString(), sk.c.toString());
        writefln("arc3.centre tag=skew cx=%.17g cy=%.17g cz=%.17g", sk.c.x, sk.c.y, sk.c.z);
        {
            Vector3 ca = sk.a - sk.c;
            Vector3 cm = q2 - sk.c;
            Vector3 cb = sk.b - sk.c;
            writefln("arc3.centre tag=skew rad a=%.17g m=%.17g b=%.17g", geom.abs(ca),
                     geom.abs(cm), geom.abs(cb));
        }
        writefln("arc3.skew t=0.5 x=%.17g y=%.17g z=%.17g", sk(0.5).x, sk(0.5).y, sk(0.5).z);
        dump_path("arc3.skew", sk);
        // A nearly colinear triple: the two bisectors become parallel, so nm.bbla's
        // least-squares solve calls the matrix singular and throws an *Exception*
        // (bbla.d:789), not the Error arc.d:112 raises for exactly colinear points.
        // Measured, and kept: it shows which of the two failure texts a caller sees.
        dump_error("arc3.err.nearlycolinear", new Arc3(Vector3(0.0, 0.0, 0.0),
                                                       Vector3(1.0, 1.0e-7, 0.0),
                                                       Vector3(2.0, 0.0, 0.0)));
    }

    // The Arc3 of arc.d:176-183, the shallow curve used for the intersection test.
    auto pth = new Arc3(Vector3(0.0, 1.0), Vector3(0.5, 1.2), Vector3(1.0, 1.0));
    dump_path("arc3.shallow", pth);

    // A near-semicircle and an arc whose sweep is negative (b before a), to pin
    // atan2's branch and the sign of L (arc.d:92-94). An *exact* half circle is not
    // representable: ca == -cb makes cross(ca, cb) zero, so arc.d:75-79 throws
    // "cannot find plane of three points" (measured, and recorded in
    // baselines/e587b235/probes/geom_gpath_arc.txt as the arc.err.halfcircle row).
    // These two use |ca| == |cb| to within 1e-5 (the arc.d:68 tolerance) with a
    // sweep of +-2.9419 radians.
    auto semi = new Arc(Vector3(1.0, 0.0, 0.0), Vector3(-0.98, 0.199, 0.0), Vector3(0.0, 0.0, 0.0));
    dump_path("arc.semi", semi);
    auto back = new Arc(Vector3(1.0, 0.0, 0.0), Vector3(-0.98, -0.199, 0.0), Vector3(0.0, 0.0, 0.0));
    writefln("arc.back length=%.17g opCall(0.5) x=%.17g y=%.17g z=%.17g", back.length(),
             back(0.5).x, back(0.5).y, back(0.5).z);

    // Copy constructors.
    dump_path("arc.copy", new Arc(abc));
    dump_path("arc3.copy", new Arc3(adb));

    // The exact half circle, which D rejects (see the note above).
    auto half = new Arc(Vector3(1.0, 0.0, 0.0), Vector3(-1.0, 0.0, 0.0), Vector3(0.0, 0.0, 0.0));
    dump_error("arc.err.halfcircle", half(0.5));

    // Error paths. radii mismatch (arc.d:68-70):
    auto bad = new Arc(Vector3(2.0, 0.0, 0.0), Vector3(0.0, 3.0, 0.0), Vector3(0.0, 0.0, 0.0));
    dump_error("arc.err.radii", bad(0.5));
    // colinear points, no plane (arc.d:75-79). b == a is the reachable shape: it
    // passes the radii test (|ca| == |cb|) and then cross(ca, cb) == 0.
    auto col = new Arc(Vector3(1.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0), Vector3(0.0, 0.0, 0.0));
    dump_error("arc.err.colinear", col(0.5));
    // Arc3 with colinear points (arc.d:111-116):
    dump_error("arc3.err.colinear", new Arc3(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 1.0, 0.0),
                                            Vector3(2.0, 2.0, 0.0)));
    // Arc3 whose two centre estimates disagree is not constructible from
    // consistent data, so the second Error (arc.d:136-140) is unreachable through
    // the public constructor; it is not dumped.
    writefln("arc.class abc=%s adb=%s semi=%s", abc.classString(), adb.classString(),
             semi.classString());
}
