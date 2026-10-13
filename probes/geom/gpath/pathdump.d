// probes/geom/gpath/pathdump.d — shared dumper for the geom/gpath probes.
//
// Why this exists: geom/gpath/path.d is a virtual interface. Path itself is
// abstract, so the only way to measure Path's own code (the finite differences in
// dpdt/d2pdt2, the chord sums in partial_length/length/point_from_length, the
// `cast(Line)` test in intersect2D and the golden-section search in
// closestDistance) is to call it through a base-class handle. Every gpath probe
// includes the same dump of the same observables, and tests/gpath_dump_common.cpp
// emits the identical line sequence from the C++ side, so a difference in any of
// them shows up as a line diff rather than as a plausible-looking number.
//
// The t sweep is not arbitrary. Path.dpdt (path.d:40-53) and Path.d2pdt2
// (path.d:62-77) branch on `t+dt > 1.0` and `t-dt < 0.0` with dt = 0.001, so the
// sweep was chosen by measuring the boundary arithmetic with ldc2
// (writefln("%.20g", 0.999+0.001) prints exactly 1, so the test is `>` not `>=`):
//   t = 0.0, 0.0005   -> 0.0005-0.001 = -0.0005 < 0  -> forward one-sided
//   t = 0.001         -> 0.001-0.001 = 0, not < 0    -> central
//   t = 0.999         -> 0.999+0.001 = 1, not > 1    -> central
//   t = 0.9995, 1.0   -> 0.9995+0.001 = 1.0005 > 1   -> backward one-sided
//   t = 0.05 ... 0.95 -> central
// Those are the branches where a port silently goes wrong (a port that used `>=`,
// or that compared against 1.0 - dt, agrees everywhere except at t = 0.001 and
// t = 0.999), so both the boundary values and the two values that sit exactly on
// the FP edge are in the list.
//
// Build and capture (see the note in probes/geom/gpath/probe_line.d for why the
// reference tree is copied):
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/gpath \
//     probes/geom/gpath/probe_<module>.d -of=/tmp/x && /tmp/x
import std.format;
import std.stdio;

import geom.elements;
import geom.gpath.path;

double[] t_sweep()
{
    return [0.0, 0.0005, 0.001, 0.05, 0.25, 0.5, 0.75, 0.95, 0.999, 0.9995, 1.0];
}

Vector3[] query_points()
{
    return [Vector3(0.5, 0.5, 0.0),
            Vector3(0.0, 0.0, 0.0),
            Vector3(1.0, 1.0, 1.0),
            Vector3(2.0, 0.0, 1.0)];
}

void dump_path(const char[] name, const Path pth)
{
    writefln("%s class=%s", name, pth.classString());
    writefln("%s str=%s", name, pth.toString());
    writefln("%s length=%.17g", name, pth.length());

    foreach (t; t_sweep()) {
        auto p = pth(t);
        writefln("%s opCall t=%.17g x=%.17g y=%.17g z=%.17g", name, t, p.x, p.y, p.z);
        auto d1 = pth.dpdt(t);
        writefln("%s dpdt t=%.17g x=%.17g y=%.17g z=%.17g", name, t, d1.x, d1.y, d1.z);
        auto d2 = pth.d2pdt2(t);
        writefln("%s d2pdt2 t=%.17g x=%.17g y=%.17g z=%.17g", name, t, d2.x, d2.y, d2.z);
        writefln("%s plen ta=%.17g tb=%.17g L=%.17g", name,
                 0.5 * t, t, pth.partial_length(0.5 * t, t));
        // The swapped call exercises the tb < ta exchange at path.d:82-84.
        writefln("%s plenswap ta=%.17g tb=%.17g L=%.17g", name,
                 t, 0.5 * t, pth.partial_length(t, 0.5 * t));
    }

    double L = pth.length();
    foreach (frac; [0.0, 0.25, 0.5, 0.75, 0.999, 1.0, 1.5]) {
        double tt;
        auto p = pth.point_from_length(frac * L, tt);
        writefln("%s pfl target=%.17g t=%.17g x=%.17g y=%.17g z=%.17g",
                 name, frac * L, tt, p.x, p.y, p.z);
    }

    foreach (q; query_points()) {
        double tt;
        double dist = pth.closestDistance(q, tt);
        writefln("%s cd px=%.17g py=%.17g pz=%.17g t=%.17g dist=%.17g",
                 name, q.x, q.y, q.z, tt, dist);
    }

    // intersect2D with the two sweeps D's own gpath unittests use (line.d:76-83
    // and arc.d:176-183), once with the default nseg = 20 and once with 10.
    auto ps = Vector3(0.5, 0.5, 0.0);
    auto dir = Vector3(0.0, 1.0, 0.0);
    double tt;
    bool found = pth.intersect2D(ps, dir, tt);
    writefln("%s i2d psx=%.17g psy=%.17g dx=%.17g dy=%.17g found=%s t=%.17g",
             name, ps.x, ps.y, dir.x, dir.y, found, tt);
    double tt10;
    bool found10 = pth.intersect2D(ps, dir, tt10, 10);
    writefln("%s i2d10 found=%s t=%.17g", name, found10, tt10);
    auto ps2 = Vector3(0.25, 0.0, 0.0);
    double tt2;
    bool found2 = pth.intersect2D(ps2, dir, tt2);
    writefln("%s i2db found=%s t=%.17g", name, found2, tt2);
} // end dump_path()

// The two-sided catch D's error paths need: geom/gpath throws `new Error` for
// geometry violations and `new Exception` for user input violations, and wave 1
// established that in D (and in the port) the two are siblings. Dumping which
// branch caught tells the replay test that the taxonomy survived the translation.
void dump_error(const char[] tag, lazy void stmt)
{
    bool asError = false, asException = false;
    string caught = "";
    try {
        stmt;
    } catch (Error e) {
        asError = true;
        caught = e.msg;
    } catch (Exception e) {
        asException = true;
        caught = e.msg;
    }
    writefln("%s asError=%s asException=%s msg=%s", tag, asError, asException, caught);
}
