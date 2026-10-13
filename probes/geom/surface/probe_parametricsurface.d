// probes/geom/surface/probe_parametricsurface.d — what
// geom/surface/parametricsurface.d produces.
//
// ParametricSurface is the abstract base of the 15-subclass surface hierarchy, and
// it has only two things of its own: area() (D:40-66, a quad_properties sum over an
// (nr+1)x(ns+1) sample grid) and writeSurfaceAsVtkXml (D:70-94, the file a ParaView
// user opens). Both are measured here through the same concrete subclass, so what is
// compared is the base class' arithmetic, not a subclass' evaluation.
//
// Why this needs measuring rather than reasoning:
//  * area()'s inner arrays grow by `~=` from null (D:45-46, :51). The number of
//    quads summed is nr*ns, and nr/ns are the D defaults (10/10) unless a row says
//    otherwise; a port that used (nr-1)*(ns-1) cells, or that walked i/j in the
//    other order, would still print four plausible numbers.
//  * the corner order D:59 passes to quad_properties is
//    p[i][j], p[i+1][j], p[i+1][j+1], p[i][j+1] — the winding that sets the sign of
//    the summed area vector, so it is dumped per (nr,ns) rather than trusted.
//  * writeSurfaceAsVtkXml loops j OUTSIDE i (D:80-81), which is the opposite of
//    area()'s loop nesting, and it writes `%20.16e`, not %g. The whole file body is
//    dumped, because a single changed space changes a file users diff.
//  * the whole-integer nr/ns edge (1, 2) and the degenerate (nrPts-1)==0 divisor in
//    the writer (D:72-73, dr = 1.0/(nrPts-1)) are included: nrPts=1 divides by zero
//    and D's float division by zero is inf, which then prints a specific way.
//
// Build and capture (the reference tree is copied because geom/gpath/path.d:24
// imports geom.surface and the closure uses `new` inside @nogc functions, which ldc2
// 1.40 rejects; deleting @nogc is a compile-time attribute change only — see
// probes/geom/gpath/probe_path.d):
//   cd /home/tang/packages/Eilmer
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   /home/tang/packages/dlang/bin/ldc2 -i -I/tmp/gdtk_shadow \
//     -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface \
//     probes/geom/surface/probe_parametricsurface.d -of=/tmp/probe_psf
//   /tmp/probe_psf > baselines/e587b235/probes/geom_surface_parametricsurface.txt
import std.algorithm;
import std.conv;
import std.file;
import std.array;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.surface.parametricsurface;
import nm.number;

// A bilinear patch through the four corners, so the surface is analytic and any
// deviation in the base class' sampling shows up rather than being absorbed by a
// complicated subclass.
class ProbeSurface : ParametricSurface {
    Vector3 p00, p10, p01, p11;

    this(Vector3 p00, Vector3 p10, Vector3 p01, Vector3 p11) {
        this.p00 = p00;
        this.p10 = p10;
        this.p01 = p01;
        this.p11 = p11;
    }
    override Vector3 opCall(double r, double s) const {
        Vector3 a = p00 + (p10 - p00) * r;
        Vector3 b = p01 + (p11 - p01) * r;
        return a + (b - a) * s;
    }
    override ParametricSurface dup() const {
        return new ProbeSurface(p00, p10, p01, p11);
    }
    override string toString() const {
        return format("ProbeSurface(%s, %s, %s, %s)", to!string(p00), to!string(p10),
                      to!string(p01), to!string(p11));
    }
}

void dump_area(const char[] tag, ParametricSurface ps, int nr, int ns) {
    auto a = ps.area(nr, ns);
    writefln("%s nr=%d ns=%d area x=%.17g y=%.17g z=%.17g", tag, nr, ns, a.x, a.y, a.z);
}

void main()
{
    // A flat unit square in the z=0 plane first: its vector area must be (0,0,1)
    // for any (nr,ns), which is the control that the winding and the sum are right.
    auto unit = new ProbeSurface(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0),
                                 Vector3(0.0, 1.0, 0.0), Vector3(1.0, 1.0, 0.0));
    dump_area("psf.unit.default", unit, 10, 10);
    foreach (int n; [1, 2, 3, 5, 10, 100]) {
        dump_area("psf.unit.sqr", unit, n, n);
    }
    dump_area("psf.unit.r1s7", unit, 1, 7);
    dump_area("psf.unit.r7s1", unit, 7, 1);

    // A sheared, tilted patch: all three components are non-zero, so a swapped
    // cross product or a dropped factor is visible in every column.
    auto skew = new ProbeSurface(Vector3(0.5, -0.25, 0.1), Vector3(2.0, 0.75, -0.3),
                                 Vector3(-0.5, 1.5, 0.9), Vector3(1.25, 2.5, 0.05));
    dump_area("psf.skew.default", skew, 10, 10);
    foreach (int n; [1, 2, 3, 4, 8, 17, 64]) {
        dump_area("psf.skew.sqr", skew, n, n);
    }
    dump_area("psf.skew.r3s11", skew, 3, 11);
    dump_area("psf.skew.r11s3", skew, 11, 3);

    // A degenerate patch (four coincident corners): quad_properties gets zero-area
    // quads, whose documented behaviour is the NaN/zero branch in
    // geom/elements/properties.d. Dumped, not avoided.
    auto flat = new ProbeSurface(Vector3(1.0, 1.0, 1.0), Vector3(1.0, 1.0, 1.0),
                                 Vector3(1.0, 1.0, 1.0), Vector3(1.0, 1.0, 1.0));
    dump_area("psf.point.default", flat, 10, 10);

    // dup() through the base handle, because the hierarchy's contract is that a dup
    // evaluates identically.
    auto d = cast(ProbeSurface) skew.dup();
    writefln("psf.dup str=%s", d.toString());
    foreach (rs; [0.0, 0.25, 0.5, 1.0]) {
        auto q = d(rs, 1.0 - rs);
        writefln("psf.dup opCall r=%.17g s=%.17g x=%.17g y=%.17g z=%.17g", rs, 1.0 - rs, q.x,
                 q.y, q.z);
    }

    // The VTK writer. Dumped as file content, including every space.
    string tmp = "/tmp/probe_psf_unit.vtk";
    writeSurfaceAsVtkXml(unit, tmp, 3, 4);
    writefln("psf.vtk.unit len=%d", (cast(string) read(tmp)).length);
    writefln("psf.vtk.unit.body=%s", join(split((cast(string) read(tmp)), "\n"), "<newline>"));

    string tmp2 = "/tmp/probe_psf_skew.vtk";
    writeSurfaceAsVtkXml(skew, tmp2, 2, 2);
    writefln("psf.vtk.skew.body=%s", join(split((cast(string) read(tmp2)), "\n"), "<newline>"));

    // The divisor edge: nrPts = 1 makes dr = 1.0/0 = inf, so i*dr is 0*inf = nan at
    // i=0. Printed as the file body because that is what a user sees.
    string tmp3 = "/tmp/probe_psf_one.vtk";
    writeSurfaceAsVtkXml(skew, tmp3, 1, 1);
    writefln("psf.vtk.one.body=%s", join(split((cast(string) read(tmp3)), "\n"), "<newline>"));

    remove(tmp);
    remove(tmp2);
    remove(tmp3);
}
