// probes/geom/volume/probe_meshvolume.d — what
// geom/volume/meshvolume.d (14 lines) produces.
//
// meshvolume.d is an EMPTY subclass: `class MeshVolume : ParametricVolume { // TODO
// some day... }` (D:12-14). It declares no fields, no ctor, no overrides and no
// unittest block. So the only two observables the module has are:
//
//   (1) it changes nothing about the volume contract — a concrete subclass of
//       MeshVolume behaves exactly like the same subclass of ParametricVolume, so
//       every inherited finite difference and opCall is dumped through a
//       MeshVolume-typed handle (216 sweep points x opCall + 9 accessors), and the
//       dup()/toString() pair is dumped through the base handle;
//   (2) it is not instantiable in the way D's contract forbids — measured, not
//       reasoned. `new MeshVolume()` with ldc2 1.40 fails to compile with:
//         /tmp/gdtk_shadow/geom/volume/meshvolume.d(12): Error: cannot create
//         instance of abstract class `MeshVolume` ... is declared here
//         /tmp/gdtk_shadow/geom/volume/parametricvolume.d(67): function `string
//         toString() const` is not implemented
//         /tmp/gdtk_shadow/geom/volume/parametricvolume.d(66): function
//         `ParametricVolume dup() const` is not implemented
//         /tmp/gdtk_shadow/geom/volume/parametricvolume.d(65): function `Vector3
//         opCall(double r, double s, double t) const` is not implemented
//       (reproduce: `printf 'import geom.volume.meshvolume;\nvoid main(){auto
//       m=new MeshVolume(); writeln(m);}\n' > /tmp/s2.d; ldc2 -i
//       -I/tmp/gdtk_shadow /tmp/s2.d` — captured verbatim 2026-10-14).
//       `is(MeshVolume : ParametricVolume)` is TRUE and is dumped as a row, because
//       C++ answers the identical question with std::is_base_of_v, so the row gates
//       the ported class really deriving from ParametricVolume. `is(Abstract!MeshVolume)`
//       is NOT dumped: ldc2 measures it `false` (it reads the declared keyword, and
//       meshvolume.d:12 writes no `abstract`), and C++ has no keyword to read in
//       reply — std::is_abstract_v would answer `true` to a different question. The
//       declaration-level fact is pinned by the compile-time check quoted above on
//       the D side and by g++ 11's rejection on the C++ side, measured on this
//       machine as:
//         error: invalid new-expression of abstract class type
//                'eil::geom::MeshVolume'
//         note: because the following virtual functions are pure within
//               'eil::geom::MeshVolume':
//                 ParametricVolume::operator()(double,double,double) const
//                 ParametricVolume::dup() const
//                 ParametricVolume::toString() const
//       (both sides refuse, both name the same three virtuals; the text is repeated
//       in tests/test_geom_volume_meshvolume.cpp).
//
// Why the sweep is the full cross product of six values per parameter (216 points):
// every inherited accessor branches on `x + d > 1.0` then `x - d < 0.0` with
// d = 0.001, and 0.999 + 0.001 rounds to exactly 1.0, so the sweep carries
//   0.0     -> lower one-sided branch
//   0.001   -> x - d == 0, NOT < 0 -> central branch (a port using >= fails here only)
//   0.5     -> central
//   0.999   -> x + d == 1, NOT > 1 -> central again
//   0.9995  -> upper one-sided branch
//   1.0     -> upper one-sided
// and the mixed partials call the first derivatives, which branch again on their own.
// Same six values and the same reason as probes/geom/volume/probe_parametricvolume.d.
//
// Not probed, with the reason: there is no stored member, no array of faces/edges/
// corners and no D `.init` left unset by a constructor, because MeshVolume declares
// no data member at all (D:12-14 body is a comment); and it declares no `throw` or
// `assert` path, so the guard rows are empty by construction rather than by omission.
//
// Build and capture (see probes/geom/surface/probe_parametricsurface.d for why the
// reference tree is copied and @nogc removed):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/volume \
//     probes/geom/volume/probe_meshvolume.d -of=/tmp/probe_mv \
//     -od=/tmp/w2_volA/obj_mv
//   /tmp/probe_mv > baselines/e587b235/probes/geom_volume_meshvolume.txt
import std.algorithm;
import std.array;
import std.conv;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.volume.meshvolume;
import geom.volume.parametricvolume;
import nm.number;

// The same trilinear ProbeVolume the base-class probe uses (plus the r*s and s*t
// cross terms that make d2Vdrds/d2Vdsdt non-zero), except derived from MeshVolume.
// Anything this dump prints must therefore match
// baselines/e587b235/probes/geom_volume_parametricvolume.txt's arithmetic, which is
// the actual claim meshvolume.d has to satisfy: it adds nothing.
class ProbeMeshVolume : MeshVolume {
    Vector3 lo, dx, dy, dz;

    this(Vector3 lo, Vector3 dx, Vector3 dy, Vector3 dz) {
        this.lo = lo; this.dx = dx; this.dy = dy; this.dz = dz;
    }
    override Vector3 opCall(double r, double s, double t) const {
        Vector3 p = lo + dx*r + dy*s + dz*t;
        p += Vector3(0.11, -0.07, 0.05) * (r*s);
        p += Vector3(-0.03, 0.02, 0.09) * (s*t);
        return p;
    }
    override ParametricVolume dup() const {
        return new ProbeMeshVolume(lo, dx, dy, dz);
    }
    override string toString() const {
        return format("ProbeMeshVolume(lo=%s, dx=%s, dy=%s, dz=%s)", to!string(lo),
                      to!string(dx), to!string(dy), to!string(dz));
    }
}

void dump3(const char[] tag, Vector3 v)
{
    writefln("%s x=%.17g y=%.17g z=%.17g", tag, v.x, v.y, v.z);
}

void main()
{
    // Only traits a C++ port can answer for the same question. `is(Abstract!MeshVolume)`
    // is deliberately NOT dumped: ldc2 1.40 measures it as `false` (the trait reports
    // the *declared* attribute, and meshvolume.d:12 writes no `abstract` keyword), while
    // the C++ equivalent question has no keyword to read — C++ `std::is_abstract_v`
    // would say `true`. Dumping it would compare two different questions. The
    // declaration-level fact is instead pinned by the compile-time check in the header.
    writefln("mv.derivesFromParametricVolume=%s", is(MeshVolume : ParametricVolume));

    auto src = new ProbeMeshVolume(Vector3(0.25, -0.5, 0.0), Vector3(2.0, 0.5, 0.0),
                                  Vector3(-0.5, 1.5, 0.75), Vector3(0.5, 0.25, 3.0));
    MeshVolume h = src;
    // toString() reached through the MeshVolume handle — D resolves it to the
    // subclass, since MeshVolume neither declares nor overrides it.
    writefln("mv.str=%s", h.toString());

    ParametricVolume b = h;
    auto d = cast(ProbeMeshVolume)(b.dup());
    writefln("mv.dup.str=%s", d.toString());
    writefln("mv.dup.isFresh=%s", (cast(MeshVolume)d !is h));

    // dup() must be a deep copy — checked at the END of main (below), after the
    // sweep, so that mutating the source cannot change the swept coordinates.

    double[6] pts = [0.0, 0.001, 0.5, 0.999, 0.9995, 1.0];

    foreach (r; pts) {
        foreach (s; pts) {
            foreach (t; pts) {
                auto p = h(r, s, t);
                dump3(format("mv.pt r=%s s=%s t=%s", to!string(r), to!string(s), to!string(t)),
                      p);
            }
        }
    }

    foreach (r; pts) {
        foreach (s; pts) {
            foreach (t; pts) {
                const string tg = format(" r=%s s=%s t=%s", to!string(r), to!string(s),
                                         to!string(t));
                dump3("mv.dVdr" ~ tg,    h.dVdr(r,s,t));
                dump3("mv.dVds" ~ tg,    h.dVds(r,s,t));
                dump3("mv.dVdt" ~ tg,    h.dVdt(r,s,t));
                dump3("mv.d2Vdr2" ~ tg,  h.d2Vdr2(r,s,t));
                dump3("mv.d2Vds2" ~ tg,  h.d2Vds2(r,s,t));
                dump3("mv.d2Vdt2" ~ tg,  h.d2Vdt2(r,s,t));
                dump3("mv.d2Vdrds" ~ tg, h.d2Vdrds(r,s,t));
                dump3("mv.d2Vdrdt" ~ tg, h.d2Vdrdt(r,s,t));
                dump3("mv.d2Vdsdt" ~ tg, h.d2Vdsdt(r,s,t));
            }
        }
    }

    // The deep-copy check, last so the mutation above cannot reach the sweep.
    // Mutated through `src`, because `MeshVolume` has no member to name — which is
    // itself the observable: meshvolume.d:12-14 declares no data at all.
    string before = d.toString();
    src.lo = Vector3(9.0, 9.0, 9.0);
    writefln("mv.dup.afterSourceMutation=%s", d.toString());
    writefln("mv.dup.textUnchanged=%s", (d.toString() == before));
    writefln("mv.str.afterSourceMutation=%s", h.toString());
}
