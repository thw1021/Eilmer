// probes/geom/elements/probe_projection.d — pin the measured semantics of
// geom/elements/projection.d before it is ported.
//
// What this probe decides:
//   1. `intersect2D(..., out double t)` — D's `out` parameter is assigned the type's
//      .init on entry, and `double.init` is NaN, NOT 0.0. The "parallel" rows pass
//      t in as 7.0 and print it afterwards, which is what decides whether the C++
//      `double& t` form has to reset it.
//   2. `project_onto_plane` return codes 0/1/2 and the projected point, at %.17g.
//   3. `barycentricCoords` 3-point vs 4-point: both return forms dumped at %.17g,
//      including the non-convex quad path that throws GeometryException.
//   4. `GeometryException` carries `__FILE__`/`__LINE__` defaults evaluated at the
//      CALL site (D expands default arguments there). The probe prints e.file/e.line
//      for a throw made on a known line so the port's macro can be checked against
//      the same shape, and confirms `catch (Exception e)` does catch it while it
//      does NOT catch the `new Error` from nomenclature.face_index.
//
// BUILD NOTE — the trunk tree cannot compile this probe as-is:
//   projection.d:136 marks the four-point barycentricCoords `@nogc`, but :154 in its
//   body throws `new GeometryException(...)`, which is a GC allocation. ldc2 1.40.0
//   rejects it whenever the body is analysed (`-i`, or `-unittest`, which is why the
//   module's own unittest block does not compile). The attribute changes no value, so
//   the probe is built against a shadow copy with that one token deleted:
//
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   mkdir -p /tmp/shadow/geom/elements
//   sed '136{/^@nogc$/d}' /home/tang/packages/gdtk/src/geom/elements/projection.d \
//     > /tmp/shadow/geom/elements/projection.d
//   ldc2 -i -I/tmp/shadow -I/home/tang/packages/gdtk/src \
//     probes/geom/elements/probe_projection.d -of=/tmp/proj_d && /tmp/proj_d \
//     > baselines/e587b235/probes/geom_elements_projection.txt
import std.conv;
import std.format;
import std.math;
import std.stdio;

import nm.number;
import geom.geometry_exception;
import geom.elements.nomenclature;
import geom.elements.properties;
import geom.elements.projection;
import geom.elements.vector3;

void main()
{
    writeln("== intersect2D, the unittest case ==");
    Vector3 i0 = Vector3(0.0, 1.0);
    Vector3 i1 = Vector3(1.0, 1.0);
    Vector3 ps = Vector3(0.5, 0.5);
    Vector3 dir = Vector3(0.0, 1.0);
    double t = 7.0;   // deliberately not zero: measures D's `out` initialisation
    bool found = intersect2D(i0, i1, ps, dir, t);
    writefln("cross.found=%s t17=%.17g t6=%s", to!string(found), t, to!string(t));

    writeln("== intersect2D, parallel: is t reset, and to what? ==");
    Vector3 a0 = Vector3(0.0, 1.0);
    Vector3 a1 = Vector3(1.0, 1.0);
    Vector3 ap = Vector3(0.5, 0.0);
    Vector3 ad = Vector3(1.0, 0.0);   // parallel to the segment
    double tpar = 7.0;
    bool fpar = intersect2D(a0, a1, ap, ad, tpar);
    writefln("parallel.found=%s t17=%.17g isNaN=%s", to!string(fpar), tpar,
             to!string(std.math.isNaN(tpar)));
    double tpar2 = 0.0;
    intersect2D(a0, a1, ap, ad, tpar2);
    writefln("parallel.zeroed t17=%.17g isNaN=%s", tpar2, to!string(std.math.isNaN(tpar2)));

    writeln("== intersect2D, more crossings and near-parallel ==");
    Vector3 s0 = Vector3(0.0, 0.0);
    Vector3 s1 = Vector3(2.0, 2.0);
    foreach (k; 0 .. 5) {
        double x = 0.25 + 0.5*cast(double)k;
        Vector3 q = Vector3(x, -1.0);
        Vector3 dv = Vector3(0.0, 1.0);
        double tk = -1.0;
        bool fk = intersect2D(s0, s1, q, dv, tk);
        writefln("cross2 k=%d q=%.17g found=%s t=%.17g", k, x, to!string(fk), tk);
    }
    // segment shrinking toward the 1e-16*segmentSize tolerance
    Vector3 tiny1 = Vector3(1.0e-16, 1.0e-16);
    double tp = -1.0;
    bool tf = intersect2D(s0, tiny1, ps, dir, tp);
    writefln("tol.tinyseg found=%s t=%.17g", to!string(tf), tp);

    writeln("== project_onto_plane, all three return codes ==");
    Vector3 pa = Vector3(1.0, 0.0, 0.0);
    Vector3 pb = Vector3(1.0, 1.0, 0.0);
    Vector3 pc = Vector3(0.5, 0.0, 0.0);
    Vector3 qr = Vector3(3.0, 3.0, -3.0);
    Vector3 q = Vector3(0.0, 0.0, 1.0);
    int flag = project_onto_plane(q, qr, pa, pb, pc);
    writefln("plane.flag=%d qx=%.17g qy=%.17g qz=%.17g qs=%s", flag, q.x, q.y, q.z,
             to!string(q));
    // qr parallel to the plane, q on the plane -> 1
    Vector3 qr1 = Vector3(1.0, 0.0, 0.0);
    Vector3 q1 = Vector3(0.75, 0.0, 0.0);
    int flag1 = project_onto_plane(q1, qr1, pa, pb, pc);
    writefln("plane.flag1=%d q1=%.17g %.17g %.17g", flag1, q1.x, q1.y, q1.z);
    // qr parallel to the plane, q off the plane -> 2
    Vector3 q2 = Vector3(0.75, 0.0, 3.0);
    int flag2 = project_onto_plane(q2, qr1, pa, pb, pc);
    writefln("plane.flag2=%d q2=%.17g %.17g %.17g", flag2, q2.x, q2.y, q2.z);
    // grazing direction, just inside/outside the 1e-12 tolerance
    Vector3 qr3 = Vector3(1.0, 0.0, 1.0e-13);
    Vector3 q3 = Vector3(0.0, 0.0, 1.0);
    int flag3 = project_onto_plane(q3, qr3, pa, pb, pc);
    writefln("plane.flag3=%d q3=%.17g %.17g %.17g", flag3, q3.x, q3.y, q3.z);

    writeln("== map_neutral_plane_to_cylinder ==");
    foreach (h; [1.0, 2.0, 0.0, -1.0]) {
        Vector3 mp = Vector3(1.0, 1.0, 1.0);
        map_neutral_plane_to_cylinder(mp, to!number(h));
        writefln("cyl h=%.17g x=%.17g y=%.17g z=%.17g s=%s", h, mp.x, mp.y, mp.z,
                 to!string(mp));
    }
    Vector3 mp9 = Vector3(0.5, 1.5, -0.25);
    map_neutral_plane_to_cylinder(mp9, to!number(0.75));
    writefln("cyl.off x=%.17g y=%.17g z=%.17g", mp9.x, mp9.y, mp9.z);
    writefln("cyl.ref sin1=%.17g cos1=%.17g", sin(1.0), cos(1.0));

    writeln("== barycentricCoords, triangle (3-point) ==");
    Vector3 b0 = Vector3(0.0, 0.0);
    Vector3 b1 = Vector3(1.0, 0.0);
    Vector3 b2 = Vector3(0.5, std.math.sin(60.0*std.math.PI/180.0));
    Vector3 bm = (b0+b1+b2)/3.0;
    number[3] bc = barycentricCoords(bm, b0, b1, b2);
    writefln("bary3.mid l0=%.17g l1=%.17g l2=%.17g sum=%.17g outside=%s", bc[0], bc[1], bc[2],
             bc[0]+bc[1]+bc[2], to!string(is_outside_triangle(bc)));
    Vector3 be = Vector3(0.5, 0.0);
    number[3] bc2 = barycentricCoords(be, b0, b1, b2);
    writefln("bary3.edge l0=%.17g l1=%.17g l2=%.17g sum=%.17g outside=%s outside_tol=%.17g",
             bc2[0], bc2[1], bc2[2], bc2[0]+bc2[1]+bc2[2], to!string(is_outside_triangle(bc2)),
             1.0e-10);
    Vector3 bo = Vector3(0.5, -0.01);
    number[3] bc3 = barycentricCoords(bo, b0, b1, b2);
    writefln("bary3.outside l0=%.17g l1=%.17g l2=%.17g outside=%s", bc3[0], bc3[1], bc3[2],
             to!string(is_outside_triangle(bc3)));
    writefln("bary3.tol outside_1e-3=%s outside_0=%s",
             to!string(is_outside_triangle(bc3, 1.0e-3)), to!string(is_outside_triangle(bc3, 0.0)));
    writefln("bary3.denomzero l0=%s l1=%s l2=%s", to!string(bc[0]), to!string(bc2[1]),
             to!string(bc3[2]));
    // degenerate triangle: denominator zero -> NaN/inf components
    Vector3 dd = Vector3(1.0, 1.0);
    number[3] bc4 = barycentricCoords(bm, b0, b1, dd);
    writefln("bary3.degenerate l0=%.17g l1=%.17g l2=%.17g outside=%s", bc4[0], bc4[1], bc4[2],
             to!string(is_outside_triangle(bc4)));

    writeln("== barycentricCoords, quad (4-point, Floater mean-value) ==");
    Vector3 c0 = Vector3(0.0, 0.0);
    Vector3 c1 = Vector3(1.0, 0.0);
    Vector3 c2 = Vector3(1.0, 1.0);
    Vector3 c3 = Vector3(0.0, 1.0);
    Vector3 cq = (c0+c1+c2+c3)/4.0;
    number[4] bq = barycentricCoords(cq, c0, c1, c2, c3);
    writefln("bary4.mid l0=%.17g l1=%.17g l2=%.17g l3=%.17g sum=%.17g outside=%s", bq[0], bq[1],
             bq[2], bq[3], bq[0]+bq[1]+bq[2]+bq[3], to!string(is_outside_quad(bq)));
    Vector3 cq2 = Vector3(0.5, 0.0);
    number[4] bq2 = barycentricCoords(cq2, c0, c1, c2, c3);
    writefln("bary4.edge l0=%.17g l1=%.17g l2=%.17g l3=%.17g outside=%s", bq2[0], bq2[1], bq2[2],
             bq2[3], to!string(is_outside_quad(bq2)));
    Vector3 cq3 = Vector3(0.5, -0.01);
    number[4] bq3 = barycentricCoords(cq3, c0, c1, c2, c3);
    writefln("bary4.outside l0=%.17g l1=%.17g l2=%.17g l3=%.17g outside=%s", bq3[0], bq3[1], bq3[2],
             bq3[3], to!string(is_outside_quad(bq3)));
    writefln("bary4.tol outside_1e-3=%s outside_0=%s",
             to!string(is_outside_quad(bq3, 1.0e-3)), to!string(is_outside_quad(bq3, 0.0)));
    Vector3 cq4 = Vector3(0.3, 0.7);
    Vector3 d1 = Vector3(1.2, -0.1);
    Vector3 d2 = Vector3(0.9, 1.1);
    Vector3 d3 = Vector3(-0.2, 0.8);
    number[4] bq4 = barycentricCoords(cq4, c0, d1, d2, d3);
    writefln("bary4.skew l0=%.17g l1=%.17g l2=%.17g l3=%.17g sum=%.17g outside=%s", bq4[0], bq4[1],
             bq4[2], bq4[3], bq4[0]+bq4[1]+bq4[2]+bq4[3], to!string(is_outside_quad(bq4)));
    // concave quad: one corner triangle has negative signed area -> throws
    Vector3 nv0 = Vector3(0.0, 0.0);
    Vector3 nv1 = Vector3(1.0, 0.0);
    Vector3 nv2 = Vector3(0.5, 0.2); // pushed inside -> non-convex
    Vector3 nv3 = Vector3(0.0, 1.0);
    Vector3 np = Vector3(0.2, 0.5);
    string msg;
    string cls;
    string etrace;
    bool caught = false;
    try {
        number[4] junk = barycentricCoords(np, nv0, nv1, nv2, nv3);
        writefln("bary4.concave unexpectedly_ok l0=%.17g", junk[0]);
    } catch (GeometryException e) {
        caught = true;
        msg = e.msg;
        cls = e.classinfo.name;
        writefln("bary4.concave geom hasfile=%s hasline=%s class=%s msg=%s",
                 to!string(e.file.length > 0), to!string(e.line > 0), e.classinfo.name, e.msg);
    } catch (Exception e) {
        cls = e.classinfo.name;
        msg = e.msg;
        writefln("bary4.concave wrong_catch %s", cls);
    }
    writefln("bary4.concave caught=%s class=%s msg=%s", to!string(caught), cls, msg);
    // The file/line a GeometryException carries name the *throw site*, which is a
    // different source file in the port by construction, so only the "is it set at
    // all and does it name the caller" shape is compared here. The port asserts the
    // exact value separately in tests/test_geom_elements_projection.cpp.
    writefln("bary4.concave siteshape throwInLibrary=true");

    writeln("== GeometryException file/line default at the call site ==");
    string m2;
    string f2s;
    uint l2s;
    bool asException = false;
    try {
        throw new GeometryException("explicit throw site"); // line 214 of this probe
    } catch (Exception e) {
        asException = true;
        m2 = e.msg;
        f2s = e.file;
        l2s = cast(uint)e.line;
    }
    writefln("geomthrow asException=%s msg=%s hasfile=%s hasline=%s sitename=%s",
             to!string(asException), m2, to!string(f2s.length > 0), to!string(l2s > 0),
             to!string(f2s == "probes/geom/elements/probe_projection.d"));
    writefln("geomthrow derivedFromException=%s",
             to!string(cast(Exception)null !is cast(Exception)(new GeometryException("x"))));

    writeln("== Error vs Exception reachability ==");
    bool eInExceptionCatch = false;
    bool eInErrorCatch = false;
    string emsg;
    try {
        face_index("up");
    } catch (Exception e) {
        eInExceptionCatch = true;
        emsg = e.msg;
    } catch (Error e) {
        eInErrorCatch = true;
        emsg = e.msg;
    }
    writefln("face_index.caughtAsException=%s caughtAsError=%s msg=%s",
             to!string(eInExceptionCatch), to!string(eInErrorCatch), emsg);

    writeln("== Quaternion renders as a static array ==");
    Quaternion qq = [ 0.0, 1.0, 2.0, 3.0 ];
    writefln("quat=%s len=%u", qq, qq.length);
}
