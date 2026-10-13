// probes/geom/volume/probe_subrangedvolume.d — what
// geom/volume/subrangedvolume.d (56 lines) produces.
//
// SubRangedVolume is a thin adapter: it stores a *dup* of an underlying volume
// (D:27) plus six re-mapping constants, and opCall (D:35-41) linearly maps
// (r,s,t) into [r0,r1]x[s0,s1]x[t0,t1] before forwarding. Everything observable is
// therefore (a) the mapping arithmetic, (b) the fact the ctor dups, so a later
// mutation of the caller's volume cannot reach the adapter, and (c) the exact
// toString() text (D:47-55), which interpolates `to!string(underlying_volume)` for a
// *class handle* — measured here to be that volume's own toString() ("PV()" style),
// not a type name and not an address.
//
// Why measuring rather than reasoning:
//  * the ctor defaults are r0=0, r1=1, s0=0, s1=1, t0=0, t1=1 (D:23-25). An
//    identity-mapped adapter must therefore agree with the underlying volume at
//    every sweep point, and its six constants print as "0"/"1" under to!string
//    (%.6g) — dumped, so a port that swapped the (r0,r1)/(s0,s1)/(t0,t1) pairs or
//    dropped a default fails on a row rather than plausibly.
//  * opCall reassigns its OWN PARAMETERS (D:37-39), so what is forwarded is
//    r0+(r1-r0)*r, s0+(s1-s0)*s, t0+(t1-t0)*t. Every sweep point is therefore dumped
//    THREE times: the mapped coordinates themselves (%.17g), the underlying volume
//    called directly at those coordinates, and the adapter's own opCall. Equality
//    between the second and third is then a measured fact rather than an assumption,
//    and the first pins the exact floating-point value of the mapping.
//  * dup() (D:42-46) goes back through the constructor, which dups again — so a dup
//    of an adapter is an equal-depth adapter, not a nested one (measured: "srf.dup"
//    text has ONE SubRangedVolume(...) wrapper). Nesting only appears when an
//    adapter wraps an adapter, which is dumped too.
//  * the nine inherited finite differences are swept over the cross product of
//    0.0, 0.001, 0.5, 0.999, 0.9995, 1.0 per parameter — the boundary tests are
//    `x + 0.001 > 1.0` and `x - 0.001 < 0.0`, and 0.999+0.001 rounds to exactly 1.0,
//    for the reasons given in probes/geom/volume/probe_parametricvolume.d. The
//    differencing happens in the ADAPTER's parameter space, so a subrange that is
//    not the identity scales every derivative: srf.B.dVdr at r=0.5 is the base
//    stencil applied to mapped coordinates 0.5 -> 0.5 -> (0.2+0.6*0.5)=0.5. That is
//    what makes these 1944 rows a gate on the mapping instead of a re-run of the
//    base corpus.
//  * there is NO throw/assert/classString path in this module — measured:
//    `grep -c 'throw' = 0`, `grep -c 'assert(' = 0`, `grep -c unittest = 0` against
//    the read-only reference tree, and ParametricVolume (parametricvolume.d:63-270)
//    declares no classString, unlike Path (geom/gpath/path.d:123).
//  * the one failure path that does exist is a null argument. It is NOT catchable
//    and therefore NOT a corpus row: `new SubRangedVolume(null)` reaches
//    `pvolume.dup()` (D:27), a virtual call through a null handle, and ldc2 1.40
//    inserts no null check. Measured on this machine: the process dies with
//    "Segmentation fault (core dumped)", exit status 139, and prints nothing —
//    `catch (Throwable e)` never runs. The port therefore adds no guard either:
//    an eil_assert/enforce there would turn a crash into a recoverable error and
//    change behaviour (recorded in provenance/fidelity.yml as adapted-handles).
//
// Build and capture (see probes/geom/surface/probe_parametricsurface.d for why the
// reference tree is copied and @nogc removed):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/volume
//     probes/geom/volume/probe_subrangedvolume.d -of=/tmp/probe_srf
//     -od=/tmp/w2_volA/obj_srf
//   /tmp/probe_srf > baselines/e587b235/probes/geom_volume_subrangedvolume.txt
import std.algorithm;
import std.array;
import std.conv;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.volume.parametricvolume;
import geom.volume.subrangedvolume;
import nm.number;

// The same trilinear volume the base-class probe uses (plus its r*s and s*t cross
// terms), so the underlying arithmetic is already pinned by
// baselines/e587b235/probes/geom_volume_parametricvolume.txt and only the adapter
// is under test here.
class ProbeVolume : ParametricVolume {
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

// Dump one adapter: its text, its six constants twice (%.17g through format, %.6g
// through to!string, because toString() uses the latter), then the full sweep.
void sweep(const char[] pfx, SubRangedVolume srv, ProbeVolume pv)
{
    writefln("%s.str=%s", pfx, srv.toString());
    writefln("%s.dup.str=%s", pfx, srv.dup().toString());
    writefln("%s.members r0=%.17g r1=%.17g s0=%.17g s1=%.17g t0=%.17g t1=%.17g",
             pfx, srv.r0, srv.r1, srv.s0, srv.s1, srv.t0, srv.t1);
    writefln("%s.members.conv=%s,%s,%s,%s,%s,%s", pfx, to!string(srv.r0), to!string(srv.r1),
             to!string(srv.s0), to!string(srv.s1), to!string(srv.t0), to!string(srv.t1));

    double[6] pts = [0.0, 0.001, 0.5, 0.999, 0.9995, 1.0];
    foreach (r; pts) {
        foreach (s; pts) {
            foreach (t; pts) {
                const string tg = format(" r=%s s=%s t=%s", to!string(r), to!string(s),
                                         to!string(t));
                // The mapped coordinates, in the same order D computes them.
                double rm = srv.r0 + (srv.r1-srv.r0)*r;
                double sm = srv.s0 + (srv.s1-srv.s0)*s;
                double tm = srv.t0 + (srv.t1-srv.t0)*t;
                writefln("%s.map%s rm=%.17g sm=%.17g tm=%.17g", pfx, tg, rm, sm, tm);
                dump3(pfx ~ ".under" ~ tg, pv(rm, sm, tm));
                dump3(pfx ~ ".pt" ~ tg, srv(r, s, t));
                dump3(pfx ~ ".dVdr" ~ tg,    srv.dVdr(r,s,t));
                dump3(pfx ~ ".dVds" ~ tg,    srv.dVds(r,s,t));
                dump3(pfx ~ ".dVdt" ~ tg,    srv.dVdt(r,s,t));
                dump3(pfx ~ ".d2Vdr2" ~ tg,  srv.d2Vdr2(r,s,t));
                dump3(pfx ~ ".d2Vds2" ~ tg,  srv.d2Vds2(r,s,t));
                dump3(pfx ~ ".d2Vdt2" ~ tg,  srv.d2Vdt2(r,s,t));
                dump3(pfx ~ ".d2Vdrds" ~ tg, srv.d2Vdrds(r,s,t));
                dump3(pfx ~ ".d2Vdrdt" ~ tg, srv.d2Vdrdt(r,s,t));
                dump3(pfx ~ ".d2Vdsdt" ~ tg, srv.d2Vdsdt(r,s,t));
            }
        }
    }
}

void main()
{
    auto pv = new ProbeVolume(Vector3(0.25, -0.5, 0.0), Vector3(2.0, 0.5, 0.0),
                              Vector3(-0.5, 1.5, 0.75), Vector3(0.5, 0.25, 3.0));
    writefln("srf.pv.str=%s", pv.toString());

    // (A) default subrange: the identity map.
    auto a = new SubRangedVolume(pv);
    sweep("srf.A", a, pv);

    // (B) a non-identity subrange on all three axes, one of them shifted off zero.
    auto b = new SubRangedVolume(pv, 0.2, 0.8, -0.1, 0.6, 0.3, 1.0);
    sweep("srf.B", b, pv);

    // The ctor dups (D:27): mutating the caller's volume afterwards changes neither
    // the adapter's stored text nor a point it evaluates.
    string beforeA = a.toString();
    Vector3 pBefore = a(0.3, 0.4, 0.5);
    pv.lo = Vector3(9.0, 9.0, 9.0);
    dump3("srf.A.pt.afterSourceMutation", a(0.3, 0.4, 0.5));
    writefln("srf.A.ptUnchanged=%s", (a(0.3,0.4,0.5) == pBefore));
    writefln("srf.A.textUnchanged=%s", (a.toString() == beforeA));
    writefln("srf.pv.str.afterMutation=%s", pv.toString());

    // Nesting: an adapter around an adapter. toString() renders the inner one in
    // full, so the exact bracket/comma structure of two nested adapters is a row.
    auto nest = new SubRangedVolume(b, 0.0, 0.5, 0.5, 1.0, 0.25, 0.75);
    writefln("srf.nest.str=%s", nest.toString());
    auto nd = cast(SubRangedVolume) nest.dup();
    writefln("srf.nest.dup.str=%s", nd.toString());
    writefln("srf.nest.dup.underIsSubRanged=%s", (cast(SubRangedVolume) nd.underlying_volume !is null));
    dump3("srf.nest.pt r=0.5 s=0.5 t=0.5", nest(0.5, 0.5, 0.5));
    dump3("srf.nest.dVdr r=0.5 s=0.5 t=0.5", nest.dVdr(0.5, 0.5, 0.5));
}
