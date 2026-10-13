// probes/geom/volume/probe_slabvolume_copyctor.d — the one path of
// geom/volume/slabvolume.d that cannot be a corpus row: its second constructor.
//
// slabvolume.d:29-34 is `this(ref const(SlabVolume) other)`. Its body (D:31-33)
// assigns dz, then calls `face0123(0.0,0.0)` on the MEMBER — and the member is the
// one SweptSurfaceVolume declares (sweptsurfacevolume.d:14), which super() on D:33
// has not yet assigned. The handle is null and opCall is virtual, so the call
// cannot return.
//
// This program prints one line and then dies, so the crash is measured rather than
// reasoned:
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/volume
//     probes/geom/volume/probe_slabvolume_copyctor.d -of=/tmp/probe_slabc
//     -od=/tmp/w2_volA/obj_slabc
//   /tmp/probe_slabc ; echo "exit=$?"
// Measured on this machine (ldc2 1.40, 2026-10-14): stdout is only the "before"
// line, the `catch (Throwable e)` handler never runs, and the shell reports
//   Segmentation fault (core dumped)
//   exit=139
// The "after" line is never printed. That is the whole observable, and it is why
// baselines/e587b235/probes/geom_volume_slabvolume.txt has no copy-ctor rows.
import std.stdio;

import geom.elements;
import geom.surface.parametricsurface;
import geom.volume.slabvolume;
import nm.number;

class ProbeSurface : ParametricSurface {
    Vector3 p00, p10, p01, p11;
    this(Vector3 p00, Vector3 p10, Vector3 p01, Vector3 p11) {
        this.p00 = p00; this.p10 = p10; this.p01 = p01; this.p11 = p11;
    }
    override Vector3 opCall(double r, double s) const {
        Vector3 a = p00 + (p10 - p00) * r;
        Vector3 b = p01 + (p11 - p01) * r;
        return a + (b - a) * s;
    }
    override ParametricSurface dup() const { return new ProbeSurface(p00, p10, p01, p11); }
    override string toString() const { return "ProbeSurface()"; }
}

void main()
{
    ParametricSurface face = new ProbeSurface(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0),
                                             Vector3(0.0, 1.0, 0.0), Vector3(1.0, 1.0, 0.0));
    auto a = new SlabVolume(face, Vector3(0.0, 0.0, 3.0));
    writeln("before: constructing SlabVolume(ref const(SlabVolume))");
    stdout.flush();
    try {
        auto b = new SlabVolume(a);
        writeln("after: ", b.toString());
    } catch (Throwable e) {
        writefln("caught type=%s msg=[%s]", e.classinfo.name, e.msg);
    }
    writeln("end: unreachable if the constructor crashed");
}
