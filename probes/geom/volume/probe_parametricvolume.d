// probes/geom/volume/probe_parametricvolume.d — what
// geom/volume/parametricvolume.d produces.
//
// ParametricVolume is the abstract base of the 8-subclass volume hierarchy. Its own
// content is nine finite-difference accessors (D:68-268) and the VTK writer
// (D:272-304). Everything is exercised through one concrete trilinear ProbeVolume so
// what is measured is the base class' differencing, not a subclass' evaluation.
//
// Why the sweep is what it is: every accessor branches on `x + d > 1.0` then
// `x - d < 0.0` with d = 0.001, and 0.999 + 0.001 rounds to exactly 1.0 (measured in
// probes/geom/gpath/probe_path.d, and the same arithmetic as Path.dpdt at
// geom/gpath/path.d:40-53). So the sweep carries:
//   0.0     -> x - d < 0   -> the lower one-sided branch
//   0.001   -> x - d == 0  -> NOT < 0, so the central branch (a port using >= fails
//             here, and only here)
//   0.5     -> central
//   0.999   -> x + d == 1  -> NOT > 1, so the central branch again
//   0.9995  -> x + d > 1   -> the upper one-sided branch
//   1.0     -> upper one-sided
// and because the mixed partials call the FIRST derivatives, which branch themselves,
// a single r value can take the outer function's central branch while the inner
// dVds() takes its own one-sided branch. The sweep is therefore the cross product of
// these six values over (r,s,t), which is 216 points per accessor: that is what makes
// a mixed/pure difference (D:214 one-sided (f0-f-1)/d versus D:144 three-point
// (f0-2f-1+f-2)/(d*d)) observable instead of plausible.
//
// Also measured, and deliberately not cleaned up:
//   * the far one-sided branch of d2Vdr2 evaluates opCall(r-2*dr,...) at r=1.0, i.e.
//     0.998, while the near branch at r=0.0 reads opCall(0.002,...) — asymmetric
//     stencils, dumped at both ends.
//   * writeVolumeAsVtkXml's `1.0/(ntPts-1)` with ntPts=1 gives inf and the point
//     prints as "-nan" under %20.16e.
//   * D:42 of the surface base and D's `.init` default for `Vector3 derivative;`
//     mean an unassigned derivative would be NaN; every branch assigns, so the
//     sweep proves that by never printing nan where a number is expected.
//
// Build and capture (see probes/geom/surface/probe_parametricsurface.d for why the
// reference tree is copied):
//   cd /home/tang/packages/Eilmer
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   /home/tang/packages/dlang/bin/ldc2 -i -I/tmp/gdtk_shadow \
//     -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/volume \
//     probes/geom/volume/probe_parametricvolume.d -of=/tmp/probe_pvl
//   /tmp/probe_pvl > baselines/e587b235/probes/geom_volume_parametricvolume.txt
import std.algorithm;
import std.array;
import std.conv;
import std.file;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.volume.parametricvolume;
import nm.number;

// A trilinear box with unequal edge lengths and a shear, so no derivative component
// is zero and no second derivative vanishes by symmetry.
class ProbeVolume : ParametricVolume {
    Vector3 lo, dx, dy, dz;

    this(Vector3 lo, Vector3 dx, Vector3 dy, Vector3 dz) {
        this.lo = lo; this.dx = dx; this.dy = dy; this.dz = dz;
    }
    override Vector3 opCall(double r, double s, double t) const {
        // lo + r*dx + s*dy + t*dz + r*s*(a mixed term) — the r*s cross term is what
        // makes d2Vdrds non-zero and distinguishes it from d2Vdr2/d2Vds2.
        Vector3 p = lo + dx*r + dy*s + dz*t;
        p += Vector3(0.11, -0.07, 0.05) * (r*s);
        p += Vector3(-0.03, 0.02, 0.09) * (s*t);
        return p;
    }
    override ParametricVolume dup() const {
        return new ProbeVolume(lo, dx, dy, dz);
    }
    override string toString() const {
        return format("ProbeVolume(lo=%s, dx=%s, dy=%s, dz=%s)", to!string(lo), to!string(dx),
                      to!string(dy), to!string(dz));
    }
}

void dump3(const char[] tag, Vector3 v)
{
    writefln("%s x=%.17g y=%.17g z=%.17g", tag, v.x, v.y, v.z);
}

void main()
{
    auto vol = new ProbeVolume(Vector3(0.25, -0.5, 0.0), Vector3(2.0, 0.5, 0.0),
                               Vector3(-0.5, 1.5, 0.75), Vector3(0.5, 0.25, 3.0));
    writefln("pvl.str=%s", vol.toString());

    auto d = cast(ProbeVolume) vol.dup();
    writefln("pvl.dup str=%s", d.toString());

    // Six values, chosen by the branch analysis above. to!string of them (which is
    // what appears in the tag) is %.6g, so a port that re-formats the tag itself has
    // to reproduce "0.9995" and "1", not "1.0".
    double[6] pts = [0.0, 0.001, 0.5, 0.999, 0.9995, 1.0];

    // The point itself, at the corners of the sweep, as the reference the
    // differences are taken against.
    foreach (r; pts) {
        foreach (s; pts) {
            foreach (t; pts) {
                auto p = vol(r, s, t);
                dump3(format("pvl.pt r=%s s=%s t=%s", to!string(r), to!string(s), to!string(t)),
                      p);
            }
        }
    }

    string[9] names = ["dVdr","dVds","dVdt","d2Vdr2","d2Vds2","d2Vdt2","d2Vdrds","d2Vdrdt","d2Vdsdt"];
    foreach (r; pts) {
        foreach (s; pts) {
            foreach (t; pts) {
                dump3(format("pvl.%s r=%s s=%s t=%s", names[0], to!string(r), to!string(s),
                             to!string(t)), vol.dVdr(r,s,t));
                dump3(format("pvl.%s r=%s s=%s t=%s", names[1], to!string(r), to!string(s),
                             to!string(t)), vol.dVds(r,s,t));
                dump3(format("pvl.%s r=%s s=%s t=%s", names[2], to!string(r), to!string(s),
                             to!string(t)), vol.dVdt(r,s,t));
                dump3(format("pvl.%s r=%s s=%s t=%s", names[3], to!string(r), to!string(s),
                             to!string(t)), vol.d2Vdr2(r,s,t));
                dump3(format("pvl.%s r=%s s=%s t=%s", names[4], to!string(r), to!string(s),
                             to!string(t)), vol.d2Vds2(r,s,t));
                dump3(format("pvl.%s r=%s s=%s t=%s", names[5], to!string(r), to!string(s),
                             to!string(t)), vol.d2Vdt2(r,s,t));
                dump3(format("pvl.%s r=%s s=%s t=%s", names[6], to!string(r), to!string(s),
                             to!string(t)), vol.d2Vdrds(r,s,t));
                dump3(format("pvl.%s r=%s s=%s t=%s", names[7], to!string(r), to!string(s),
                             to!string(t)), vol.d2Vdrdt(r,s,t));
                dump3(format("pvl.%s r=%s s=%s t=%s", names[8], to!string(r), to!string(s),
                             to!string(t)), vol.d2Vdsdt(r,s,t));
            }
        }
    }

    // The VTK writer, dumped as file content including every space.
    string tmp = "/tmp/probe_pvl_full.vtk";
    writeVolumeAsVtkXml(vol, tmp, 3, 2, 3);
    writefln("pvl.vtk.full len=%d", (cast(string) read(tmp)).length);
    writefln("pvl.vtk.full.body=%s", join(split((cast(string) read(tmp)), "\n"), "<newline>"));

    string tmp1 = "/tmp/probe_pvl_one.vtk";
    writeVolumeAsVtkXml(vol, tmp1, 1, 1, 1);
    writefln("pvl.vtk.one.body=%s", join(split((cast(string) read(tmp1)), "\n"), "<newline>"));

    remove(tmp);
    remove(tmp1);
}
