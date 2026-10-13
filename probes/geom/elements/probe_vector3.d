// probes/geom/elements/probe_vector3.d — D-side numbers for the Vector3 port.
//
// Why: geom/elements/vector3.d is the bottom of the whole dependency closure, so
// its arithmetic order is what the 1e-13 gate ultimately measures. This probe
// prints every operation the port claims to reproduce, at %.17g (enough digits to
// be a bit-exactness claim, not a tolerance claim) plus the %.6g string forms that
// reach solver logs.
//
// Build and capture:
//   ldc2 -i -I/home/tang/packages/gdtk/src probes/geom/elements/probe_vector3.d \
//     -of=/tmp/v3_d && /tmp/v3_d > baselines/e587b235/probes/geom_elements_vector3.txt
// Replay check: ctest -R geom_elements_vector3_equiv
module probe_vector3;

import std.stdio, std.conv, std.math;
import nm.number;
import geom.elements.vector3;

void main() {
    Vector3 a = Vector3(1.0, 2.2, 3.0);
    Vector3 b = Vector3(0.5, -1.25, 4.0);
    Vector3 c = Vector3([1.0, 2.2, 3.0]);

    writefln("ctor_array %.17g %.17g %.17g", c.x, c.y, c.z);
    writefln("ctor_one %.17g %.17g %.17g", Vector3(7.0).x, Vector3(7.0).y, Vector3(7.0).z);
    writefln("ctor_two %.17g %.17g %.17g", Vector3(7.0, 8.0).x, Vector3(7.0, 8.0).y,
             Vector3(7.0, 8.0).z);

    writefln("add %.17g %.17g %.17g", (a+b).x, (a+b).y, (a+b).z);
    writefln("sub %.17g %.17g %.17g", (a-b).x, (a-b).y, (a-b).z);
    writefln("mul %.17g %.17g %.17g", (a*2.0).x, (a*2.0).y, (a*2.0).z);
    writefln("rmul %.17g %.17g %.17g", (3.0*b).x, (3.0*b).y, (3.0*b).z);
    writefln("div %.17g %.17g %.17g", (a/3.0).x, (a/3.0).y, (a/3.0).z);
    writefln("neg %.17g %.17g %.17g", (-a).x, (-a).y, (-a).z);
    writefln("pos %.17g %.17g %.17g", (+a).x, (+a).y, (+a).z);

    Vector3 acc = Vector3(a); acc += b;
    writefln("iadd %.17g %.17g %.17g", acc.x, acc.y, acc.z);
    acc -= b;
    writefln("isub %.17g %.17g %.17g", acc.x, acc.y, acc.z);
    acc *= 2.5;
    writefln("imul %.17g %.17g %.17g", acc.x, acc.y, acc.z);
    acc /= 7.0;
    writefln("idiv %.17g %.17g %.17g", acc.x, acc.y, acc.z);

    writefln("dot %.17g", dot(a, b));
    writefln("dot_member %.17g", a.dot(b));
    writefln("dot_ptr %.17g", dot(&a, &b));
    writefln("dot_product %.17g", dot_product(a.x, a.y, a.z, b.x, b.y, b.z));

    Vector3 xw = cross(a, b);
    writefln("cross %.17g %.17g %.17g", xw.x, xw.y, xw.z);
    number cx, cy, cz;
    cross_product(a.x, a.y, a.z, b.x, b.y, b.z, cx, cy, cz);
    writefln("cross_product %.17g %.17g %.17g", cx, cy, cz);

    writefln("abs %.17g", abs(a));
    writefln("distance %.17g", distance_between(a, b));
    writefln("distance_self %.17g", distance_between(a, a));
    Vector3 u = unit(b);
    writefln("unit %.17g %.17g %.17g %.17g", u.x, u.y, u.z, abs(u));

    // normalize() branches: the >0 path, the underflow path, and the 1e-30 flush.
    Vector3 nz = Vector3(0.0, 0.0, 0.0); nz.normalize();
    writefln("norm_zero %.17g %.17g %.17g", nz.x, nz.y, nz.z);
    Vector3 nt = Vector3(1.0e-200, 0.0, 0.0); nt.normalize();
    writefln("norm_underflow %.17g %.17g %.17g", nt.x, nt.y, nt.z);
    Vector3 nf = Vector3(1.0e-31, 2.0e-31, 1.0); nf.normalize();
    writefln("norm_flush %.17g %.17g %.17g", nf.x, nf.y, nf.z);

    // The D add(x,y,z,factor) overload has parameters shadowing the members.
    Vector3 sh = Vector3(1.0, 2.0, 3.0);
    sh.add(10.0, 20.0, 30.0, 4.0);
    writefln("add_shadow %.17g %.17g %.17g", sh.x, sh.y, sh.z);
    sh.add(b);
    writefln("add_ref %.17g %.17g %.17g", sh.x, sh.y, sh.z);
    sh.add(b, 0.5);
    writefln("add_ref_factor %.17g %.17g %.17g", sh.x, sh.y, sh.z);
    sh.scale(-2.0);
    writefln("scale %.17g %.17g %.17g", sh.x, sh.y, sh.z);
    sh.clear();
    writefln("clear %.17g %.17g %.17g", sh.x, sh.y, sh.z);

    // Rotations. PI is D's 80-bit real, so these two spell round differently:
    // `PI/4` reaches the double parameter as wide-then-round, `to!number(PI)/3`
    // rounds first and divides in double.
    Vector3 r1 = Vector3(1.0, 0.0, 0.0); r1.rotate_about_zaxis(PI/4);
    writefln("rot_z %.17g %.17g %.17g", r1.x, r1.y, r1.z);
    Vector3 r2 = Vector3(1.0, 0.0, 0.0); r2.rotate_about_zaxis(PI/3);
    writefln("rot_z_third %.17g %.17g %.17g", r2.x, r2.y, r2.z);
    Vector3 q45 = Vector3(cos(to!number(PI)/4), sin(to!number(PI)/4));
    q45.rotate2d(15.0*PI/180.0);
    writefln("rot2d_45_60 %.17g %.17g %.17g", q45.x, q45.y, q45.z);
    Vector3 q60 = Vector3(cos(to!number(PI)/3), sin(to!number(PI)/3));
    writefln("rot2d_target %.17g %.17g %.17g", q60.x, q60.y, q60.z);
    Vector3 q30 = Vector3(cos(to!number(PI)/6), sin(to!number(PI)/6));
    q30.rotate2d(30.0*PI/180.0);
    writefln("rot2d_30_60 %.17g %.17g %.17g", q30.x, q30.y, q30.z);

    Vector3 mpt = Vector3(0.0, 1.0, 0.0);
    Vector3 mnm = Vector3(0.0, 1.0, 0.0);
    Vector3 mi = Vector3(1.0, 0.0, 0.0);
    mi.mirror_image(mpt, mnm);
    writefln("mirror %.17g %.17g %.17g", mi.x, mi.y, mi.z);

    // Frame transforms, 3D and 2D, with and without translation.
    Vector3 n = Vector3(1.0, 1.0, 0.0); n = unit(n);
    Vector3 t1 = Vector3(-1.0, 1.0, 0.0); t1 = unit(t1);
    Vector3 t2 = cross(n, t1);
    Vector3 h = Vector3(1.0, 0.0, 1.0);
    Vector3 h3 = Vector3(h);
    h3.transform_to_local_frame(n, t1, t2);
    writefln("frame_local %.17g %.17g %.17g", h3.x, h3.y, h3.z);
    h3.transform_to_global_frame(n, t1, t2);
    writefln("frame_global %.17g %.17g %.17g", h3.x, h3.y, h3.z);
    Vector3 h2 = Vector3(h);
    h2.transform_to_local_frame(n, t1);
    writefln("frame_local2d %.17g %.17g %.17g", h2.x, h2.y, h2.z);
    h2.transform_to_global_frame(n, t1);
    writefln("frame_global2d %.17g %.17g %.17g", h2.x, h2.y, h2.z);
    Vector3 ctr = Vector3(0.5, 0.5, 0.0);
    Vector3 hc = Vector3(h);
    hc.transform_to_local_frame(n, t1, t2, ctr);
    writefln("frame_local_c %.17g %.17g %.17g", hc.x, hc.y, hc.z);
    hc.transform_to_global_frame(n, t1, t2, ctr);
    writefln("frame_global_c %.17g %.17g %.17g", hc.x, hc.y, hc.z);

    number[9] R = [0.0, 1.0, 0.0, -1.0, 0.0, 0.0, 0.0, 0.0, 1.0];
    number[] Rs = R[];  // D's parameter is `ref const(number[])`, so it needs an lvalue
    Vector3 mt = Vector3(1.0, 2.0, 3.0);
    mt.apply_matrix_transform(Rs);
    writefln("matrix_transform %.17g %.17g %.17g", mt.x, mt.y, mt.z);

    number ax = 1.0, ay = 2.0, az = 3.0;
    transform_to_local_frame(ax, ay, az, n, t1, t2);
    writefln("free_local %.17g %.17g %.17g", ax, ay, az);
    transform_to_global_frame(ax, ay, az, n, t1, t2);
    writefln("free_global %.17g %.17g %.17g", ax, ay, az);

    // Strings: toString() is the %.6g form every solver log and restart file sees.
    writefln("str_a %s", a.toString());
    writefln("str_b %s", b.toString());
    writefln("str_third %s", Vector3(1.0/3.0).toString());
    writefln("str_big %s", Vector3(1.0e100, -2.5e-7, 0.0).toString());
    writefln("str_init %s", Vector3.init.toString());

    writefln("approx %s %s %s", approxEqualVectors(a, c), approxEqualVectors(a, b),
             approxEqualVectors(Vector3(1.0, 2.0, 3.0), Vector3(1.0, 2.0, 3.0+1.0e-9)));
}
