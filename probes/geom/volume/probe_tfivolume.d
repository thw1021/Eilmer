// probes/geom/volume/probe_tfivolume.d — what geom/volume/tfivolume.d (190 lines)
// produces.
//
// TFIVolume (D:17-162) is a hexahedral volume built from six boundary surfaces and the
// blended-interpolation opCall (D:118-138). Three things make it worth a probe of its
// own rather than a happy-path check:
//
//  * TWO constructors with DIFFERENT amounts of validation. The generic one (D:24-86)
//    checks that the faces meet at each of the eight corners and throws a D **Error**
//    when they do not; the simple-box one (D:91-102) skips the corner checks ("Don't
//    bother testing for open corners", D:100) and only calls checkVolume(). Both then
//    run checkVolume (D:149-161), which throws geom.geometry_exception.GeometryException
//    (an **Exception**) when hex_cell_properties returns volume <= 0.0. So one module
//    has two throwables from the two sides of the Exception/Error split, and dup()
//    (D:113-116) goes back through the VALIDATING constructor, so a volume that was
//    accepted by the box constructor is re-tested on every dup.
//  * the corner test is approxEqualVectors (D:32 etc.), whose defaults are
//    maxRelDiff=1.0e-2 and maxAbsDiff=1.0e-5 with isClose's `<=` semantics
//    (geom/elements/vector3.d:698-704, nm/number.d:40-44). The tolerance bracket at the
//    bottom of this probe therefore measures WHEN a corner is declared open, and pins
//    `<=` against `<` at both thresholds.
//  * two of the eight error messages print the WHOLE p ARRAY rather than the corner
//    (D:47 and D:54 write `p` where D:33, :40, :61 ... write `p[k]`), and at that point
//    in the constructor only p[0..2] (corner 2) or p[0..3] (corner 3) have been
//    assigned, so the rest of the array is Vector3.init — NaN. The messages are
//    transcribed as D writes them (wave-02-brief rule 1), and the corpus rows carry the
//    NaN text, which is also the only place in this module where D's `.init` shows up.
//
// What is dumped: toString() (D:140-146) for both constructions; all six stored faces
// in FACE INDEX ORDER (west, east, south, north, bottom, top — geom/elements/
// nomenclature.d:12-19) and all eight corners; dup() and the copy constructor
// (D:107-111); opCall and the nine inherited finite differences over the 216-point
// sweep (six values per parameter, 0.0, 0.001, 0.5, 0.999, 0.9995, 1.0, chosen because
// the base stencils branch on `x + 0.001 > 1.0` / `x - 0.001 < 0.0` and 0.999 + 0.001
// rounds to exactly 1.0 — probes/geom/volume/probe_parametricvolume.d) for the
// faces-built and the box-built volume; the degenerate geometries (zero-height/flat
// block, a block thin enough for hex_cell_properties to zero its volume, a
// negatively-oriented block, a NaN-cornered block, boundary surfaces collapsed to a
// point); which throwable escapes and its escaped message text for all eight corner
// paths and every checkVolume path; the array-length edges (a faceArray shorter than 6,
// which is a D bounds Error, and one LONGER than 6, which D silently ignores because
// the loop is `foreach (i; 0 .. 6)`); and the D unittest (D:164-190).
//
// Not dumped on purpose: `e.file`/`e.line` of the GeometryException. D's defaults
// (geom/geometry_exception.d:11-15) expand to the *D* throw site, which no C++ binary
// can print; src/geom/geometry_exception.hpp:23-27 records the same decision for
// projection.d, and the port passes its own __FILE__/__LINE__ through EIL_THROW_GEOM.
// The message text and the throwable's class are compared instead.
//
// Build and capture:
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/volume \
//     probes/geom/volume/probe_tfivolume.d -of=/tmp/probe_tfi -od/tmp/probe_obj_vol2
//   /tmp/probe_tfi > baselines/e587b235/probes/geom_volume_tfivolume.txt
import std.algorithm;
import std.array;
import std.conv;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.geometry_exception;
import geom.gpath.line;
import geom.gpath.path;
import geom.surface.coonspatch;
import geom.surface.parametricsurface;
import geom.volume.parametricvolume;
import geom.volume.tfivolume;
import nm.number;

// Corners are declared in CoonsPatch's order (p00, p10, p11, p01 — see
// geom/surface/coonspatch.d:31) so faces_of() below reads like tfivolume.d:94-99, but
// opCall keeps the same bilinear arithmetic as the ProbeSurface in
// probes/geom/volume/probe_sweptsurfacevolume.d, which replayed 6587 of 6587 rows.
class ProbeSurface : ParametricSurface {
    Vector3 p00, p10, p11, p01;

    this(Vector3 p00, Vector3 p10, Vector3 p11, Vector3 p01) {
        this.p00 = p00; this.p10 = p10; this.p11 = p11; this.p01 = p01;
    }
    override Vector3 opCall(double r, double s) const {
        Vector3 a = p00 + (p10 - p00) * r;
        Vector3 b = p01 + (p11 - p01) * r;
        return a + (b - a) * s;
    }
    override ParametricSurface dup() const {
        return new ProbeSurface(p00, p10, p11, p01);
    }
    override string toString() const {
        return format("ProbeSurface(%s, %s, %s, %s)", to!string(p00), to!string(p10),
                      to!string(p11), to!string(p01));
    }
}

void dump3(const char[] tag, Vector3 v)
{
    writefln("%s x=%.17g y=%.17g z=%.17g", tag, v.x, v.y, v.z);
}

void dump_members(const char[] pfx, TFIVolume v)
{
    writefln("%s.str=%s", pfx, v.toString());
    foreach (i; 0 .. 6) {
        writefln("%s.faces[%d]=%s", pfx, i, to!string(v.faces[i]));
    }
    foreach (i; 0 .. 8) {
        dump3(format("%s.p[%d]", pfx, i), v.p[i]);
    }
}

void dump_sweep(const char[] pfx, TFIVolume v)
{
    double[6] pts = [0.0, 0.001, 0.5, 0.999, 0.9995, 1.0];
    foreach (r; pts) {
        foreach (s; pts) {
            foreach (t; pts) {
                const string tg = format(" r=%s s=%s t=%s", to!string(r), to!string(s),
                                         to!string(t));
                dump3(pfx ~ ".pt" ~ tg, v(r,s,t));
                dump3(pfx ~ ".dVdr" ~ tg,    v.dVdr(r,s,t));
                dump3(pfx ~ ".dVds" ~ tg,    v.dVds(r,s,t));
                dump3(pfx ~ ".dVdt" ~ tg,    v.dVdt(r,s,t));
                dump3(pfx ~ ".d2Vdr2" ~ tg,  v.d2Vdr2(r,s,t));
                dump3(pfx ~ ".d2Vds2" ~ tg,  v.d2Vds2(r,s,t));
                dump3(pfx ~ ".d2Vdt2" ~ tg,  v.d2Vdt2(r,s,t));
                dump3(pfx ~ ".d2Vdrds" ~ tg, v.d2Vdrds(r,s,t));
                dump3(pfx ~ ".d2Vdrdt" ~ tg, v.d2Vdrdt(r,s,t));
                dump3(pfx ~ ".d2Vdsdt" ~ tg, v.d2Vdsdt(r,s,t));
            }
        }
    }
}

// The six faces of a hexahedron given its 8 corner points, assembled exactly the way
// tfivolume.d:94-99 assembles them (ProbeSurface takes p00, p10, p11, p01).
ParametricSurface[6] faces_of(Vector3[8] q)
{
    ParametricSurface[6] f;
    f[Face.north]  = new ProbeSurface(q[3], q[2], q[6], q[7]);
    f[Face.south]  = new ProbeSurface(q[0], q[1], q[5], q[4]);
    f[Face.east]   = new ProbeSurface(q[1], q[2], q[6], q[5]);
    f[Face.west]   = new ProbeSurface(q[0], q[3], q[7], q[4]);
    f[Face.top]    = new ProbeSurface(q[4], q[5], q[6], q[7]);
    f[Face.bottom] = new ProbeSurface(q[0], q[1], q[2], q[3]);
    return f;
}

void report(const char[] tag, Throwable e)
{
    writefln("%s=%s isException=%s isError=%s msg=[%s]", tag, e.classinfo.name,
             (cast(Exception) e !is null), (cast(Error) e !is null), e.msg);
}

void main()
{
    // A sheared, unsymmetric block: no face is planar-symmetric with another and the
    // six faces are six different surfaces, so an index permutation in faces[] shows.
    Vector3[8] q;
    q[0] = Vector3(0.0, 0.1, 0.0);
    q[1] = Vector3(1.0, 0.1, 0.0);
    q[2] = Vector3(1.0, 1.1, 0.0);
    q[3] = Vector3(0.0, 1.1, 0.0);
    q[4] = Vector3(0.2, 0.05, 3.0);
    q[5] = Vector3(1.3, 0.2, 2.75);
    q[6] = Vector3(0.95, 1.25, 3.2);
    q[7] = Vector3(-0.1, 0.95, 2.9);

    // ---- the two constructors on the same block ----------------------------
    auto byFaces = new TFIVolume(faces_of(q));
    dump_members("tvi.faces", byFaces);
    auto byBox = new TFIVolume(q);
    dump_members("tvi.box", byBox);
    bool corners_same = true;
    foreach (i; 0 .. 8) corners_same = corners_same && (byFaces.p[i] == byBox.p[i]);
    writefln("tvi.cornersSame=%s", corners_same);
    writefln("tvi.textSame=%s", (byFaces.toString() == byBox.toString()));
    // The face handles are dups, not the caller's (D:27).
    auto fset = faces_of(q);
    auto byFaces2 = new TFIVolume(fset);
    writefln("tvi.faceIsDup=%s", (cast(ProbeSurface) byFaces2.faces[0] !is fset[0]));

    dump_sweep("tvi.faces", byFaces);
    dump_sweep("tvi.box", byBox);

    // ---- dup() and the copy constructor ------------------------------------
    ParametricVolume basev = byFaces;
    auto df = cast(TFIVolume) basev.dup();
    writefln("tvi.faces.dup.str=%s", df.toString());
    writefln("tvi.faces.dup.textSame=%s", (df.toString() == byFaces.toString()));
    writefln("tvi.faces.dup.isFresh=%s", (cast(TFIVolume) df !is byFaces));
    foreach (i; 0 .. 8) dump3(format("tvi.faces.dup.p[%d]", i), df.p[i]);
    auto db = cast(TFIVolume) cast(ParametricVolume) byBox.dup();
    writefln("tvi.box.dup.str=%s", db.toString());
    writefln("tvi.box.dup.textSame=%s", (db.toString() == byBox.toString()));
    foreach (i; 0 .. 8) dump3(format("tvi.box.dup.p[%d]", i), db.p[i]);

    auto cf = new TFIVolume(byFaces);
    writefln("tvi.faces.copy.str=%s", cf.toString());
    writefln("tvi.faces.copy.textSame=%s", (cf.toString() == byFaces.toString()));
    foreach (i; 0 .. 8) dump3(format("tvi.faces.copy.p[%d]", i), cf.p[i]);

    // dup() re-runs the corner checks. Prove it: build a volume whose faces agree only
    // to within tolerance, then break one face and show dup() throws where the field
    // assignment itself did not.
    auto brokenFaces = faces_of(q);
    auto keeper = cast(ProbeSurface) brokenFaces[Face.west];
    keeper.p00 = keeper.p00 + Vector3(0.05, 0.0, 0.0);
    // ... constructed through the box constructor, which skips the checks:
    auto byBoxSkip = new TFIVolume(q);
    byBoxSkip.faces[Face.west] = keeper;
    writefln("tvi.skip.str=%s", byBoxSkip.toString());

    // dup() re-enters the VALIDATING constructor (D:115). Prove it: a volume built by
    // the 8-point constructor (which skips the checks, D:100) has valid corners, but the
    // same object with two of its faces exchanged still prints and still evaluates —
    // until dup() re-checks it. faces[] is public (D:19), so this is reachable without
    // any cast tricks.
    auto swapped = new TFIVolume(q);
    auto tmpFace = swapped.faces[Face.west];
    swapped.faces[Face.west] = swapped.faces[Face.south];
    swapped.faces[Face.south] = tmpFace;
    writefln("tvi.swap.str=%s", swapped.toString());
    try {
        auto dd = swapped.dup();
        writefln("tvi.swap.dup.noThrow=%s", dd.toString());
    } catch (Throwable e) {
        report("tvi.swap.dup", e);
    }
    // A null face handle is printable (to!string of a null class handle is "null") but
    // opCall through it is a virtual call on null, which ldc2 does not guard, so only
    // toString() is called here.
    auto nulled = new TFIVolume(q);
    nulled.faces[Face.east] = null;
    nulled.faces[Face.top] = null;
    writefln("tvi.nullFace.str=%s", nulled.toString());

    // ---- the eight corner checks -------------------------------------------
    // One case per check site. Shifting a whole face moves four corners at once and the
    // FIRST check then wins, so each case moves only the one corner of the one face that
    // supplies p_alt1 for that site (D:30, :37, :44, :51, :58, :65, :72, :79):
    //   corner  alt1 face  (u,v)   ProbeSurface field
    //   0       west       (0,0)   p00
    //   1       east       (0,0)   p00
    //   2       east       (1,0)   p10
    //   3       west       (1,0)   p10
    //   4       west       (0,1)   p01
    //   5       east       (0,1)   p01
    //   6       east       (1,1)   p11
    //   7       west       (1,1)   p11
    // The shift is 0.05 in x, outside approxEqualVectors' 1.0e-2 relative band for these
    // coordinates. Corners 2 and 3 print the whole p ARRAY (D:47, D:54) with the tail
    // still Vector3.init.
    int[8] alt_face = [Face.west, Face.east, Face.east, Face.west,
                       Face.west, Face.east, Face.east, Face.west];
    foreach (c; 0 .. 8) {
        auto fs = faces_of(q);
        auto surf = cast(ProbeSurface) fs[alt_face[c]];
        if (c == 0 || c == 1) surf.p00 = surf.p00 + Vector3(0.05, 0.0, 0.0);
        else if (c == 2 || c == 3) surf.p10 = surf.p10 + Vector3(0.05, 0.0, 0.0);
        else if (c == 4 || c == 5) surf.p01 = surf.p01 + Vector3(0.05, 0.0, 0.0);
        else surf.p11 = surf.p11 + Vector3(0.05, 0.0, 0.0);
        try {
            auto bad = new TFIVolume(fs);
            writefln("tvi.corner[%d].noThrow=%s", c, bad.toString());
        } catch (Throwable e) {
            report(format("tvi.corner[%d]", c), e);
        }
    }

    // ---- the tolerance bracket on corner 0 ----------------------------------
    // approxEqualVectors defaults: maxRelDiff 1.0e-2, maxAbsDiff 1.0e-5, `<=`.
    double[8] deltas = [0.0, 1.0e-6, 1.0e-5, 1.0000001e-5, 0.009, 0.01, 0.011, 0.05];
    foreach (i, dl; deltas) {
        auto fs = faces_of(q);
        auto w = cast(ProbeSurface) fs[Face.west];
        // corner 0 compares faces[bottom](0,0) against faces[west](0,0) and
        // faces[south](0,0); q[0] is (0.0, 0.1, 0.0) so a shift in y is a shift with
        // |a| = 0.1 and a relative distance of delta/0.1.
        w.p00 = w.p00 + Vector3(0.0, dl, 0.0);
        try {
            auto ok = new TFIVolume(fs);
            writefln("tvi.tol[%d].dl=%s accepted=%s p0.y=%.17g", i, to!string(dl), true,
                     ok.p[0].y);
        } catch (Throwable e) {
            writefln("tvi.tol[%d].dl=%s rejected=%s", i, to!string(dl), true);
            report(format("tvi.tol[%d]", i), e);
        }
    }
    // The same bracket anchored at a zero coordinate, where only maxAbsDiff applies.
    Vector3[8] zq = q;
    zq[0] = Vector3(0.0, 0.0, 0.0);
    foreach (i, dl; deltas) {
        auto fs = faces_of(zq);
        auto w = cast(ProbeSurface) fs[Face.west];
        w.p00 = w.p00 + Vector3(dl, 0.0, 0.0);
        try {
            auto ok = new TFIVolume(fs);
            writefln("tvi.ztol[%d].dl=%s accepted=true p0.x=%.17g", i, to!string(dl),
                     ok.p[0].x);
        } catch (Throwable e) {
            writefln("tvi.ztol[%d].dl=%s rejected=true", i, to!string(dl));
        }
    }

    // ---- checkVolume paths --------------------------------------------------
    // Flat block: every corner in the z=0 plane, faces consistent, volume exactly 0.
    Vector3[8] flat;
    foreach (i; 0 .. 4) flat[i] = q[i];
    flat[4] = q[0]; flat[5] = q[1]; flat[6] = q[2]; flat[7] = q[3];
    try {
        auto v = new TFIVolume(flat);
        writefln("tvi.flat.noThrow=%s", v.toString());
    } catch (Throwable e) {
        report("tvi.flat", e);
    }
    // Same flat block through the faces constructor.
    try {
        auto v = new TFIVolume(faces_of(flat));
        writefln("tvi.flatFaces.noThrow=%s", v.toString());
    } catch (Throwable e) {
        report("tvi.flatFaces", e);
    }
    // Thin in z but not thin enough for hex_cell_properties to zero the volume:
    // 1.0e-13 height, which the 1.0e-20/1.0e-12 bands turn over whether it reports 0.
    Vector3[8] thin = q;
    thin[4] = Vector3(q[4].x, q[4].y, 1.0e-13);
    thin[5] = Vector3(q[5].x, q[5].y, 1.0e-13);
    thin[6] = Vector3(q[6].x, q[6].y, 1.0e-13);
    thin[7] = Vector3(q[7].x, q[7].y, 1.0e-13);
    try {
        auto v = new TFIVolume(thin);
        writefln("tvi.thin.noThrow=%.17g", v(0.5,0.5,0.5).z);
    } catch (Throwable e) {
        report("tvi.thin", e);
    }
    // Negatively-oriented block (p0..p3 wound the other way) — volume < 0.
    Vector3[8] inv = q;
    inv[0] = q[1]; inv[1] = q[0]; inv[2] = q[3]; inv[3] = q[2];
    try {
        auto v = new TFIVolume(inv);
        writefln("tvi.inverted.noThrow=%s", v.toString());
    } catch (Throwable e) {
        report("tvi.inverted", e);
    }
    // Inside-out block: the bottom and top slabs exchanged, which reverses the
    // dipyramid orientations hex_cell_properties sums (D:646-657 of properties.d) and is
    // large enough (|volume| >> 1.0e-12) to survive the collapse-to-zero band at
    // properties.d:662-666, so the %g in the message prints a negative number.
    Vector3[8] inside_out;
    foreach (i; 0 .. 4) {
        inside_out[i] = q[i+4];
        inside_out[i+4] = q[i];
    }
    try {
        auto v = new TFIVolume(inside_out);
        writefln("tvi.insideOut.noThrow=%s", v.toString());
    } catch (Throwable e) {
        report("tvi.insideOut", e);
    }
    try {
        auto v = new TFIVolume(faces_of(inside_out));
        writefln("tvi.insideOutFaces.noThrow=%s", v.toString());
    } catch (Throwable e) {
        report("tvi.insideOutFaces", e);
    }
    // NaN corners: the corner test fires first (approxEqualNumbers on NaN), so the
    // GeometryException path is not reached — measured, not assumed.
    Vector3[8] nanq = q;
    nanq[0] = Vector3(double.nan, double.nan, double.nan);
    try {
        auto v = new TFIVolume(nanq);
        writefln("tvi.nan.noThrow=%s", v.toString());
    } catch (Throwable e) {
        report("tvi.nan", e);
    }
    // NaN in a corner the checks do not touch first: the top face only.
    Vector3[8] nanTop = q;
    nanTop[4] = Vector3(double.nan, 0.05, 3.0);
    try {
        auto v = new TFIVolume(nanTop);
        writefln("tvi.nanTop.noThrow=%s", v.toString());
    } catch (Throwable e) {
        report("tvi.nanTop", e);
    }
    // All six boundary surfaces collapsed to a single point: corners coincide (so no
    // Error), volume is 0 -> GeometryException. The degenerate "surfaces collapse" case.
    Vector3[8] one;
    foreach (i; 0 .. 8) one[i] = Vector3(0.25, -0.5, 1.0);
    try {
        auto v = new TFIVolume(faces_of(one));
        writefln("tvi.collapsed.noThrow=%s", v.toString());
    } catch (Throwable e) {
        report("tvi.collapsed", e);
    }
    // An open corner AND zero volume: which throwable escapes? (The corner check is
    // D:32-84, checkVolume is D:85, so the Error must win.)
    auto both = faces_of(flat);
    auto bw = cast(ProbeSurface) both[Face.west];
    bw.p00 = bw.p00 + Vector3(0.5, 0.0, 0.0);
    try {
        auto v = new TFIVolume(both);
        writefln("tvi.bothBad.noThrow=%s", v.toString());
    } catch (Throwable e) {
        report("tvi.bothBad", e);
    }
    // GeometryException caught as Exception (it is one) and NOT as Error.
    try {
        auto v = new TFIVolume(flat);
        writefln("tvi.catch.flat.noThrow=true");
    } catch (Exception e) {
        writefln("tvi.catch.flat.asException=%s msg=[%s]", e.classinfo.name, e.msg);
    } catch (Error e) {
        writefln("tvi.catch.flat.asError=%s msg=[%s]", e.classinfo.name, e.msg);
    }
    // An open corner caught as Exception must fall through to the Error handler.
    try {
        auto fs = faces_of(q);
        auto w = cast(ProbeSurface) fs[Face.south];
        w.p00 = w.p00 + Vector3(0.0, 0.0, 0.5);
        auto v = new TFIVolume(fs);
        writefln("tvi.catch.corner.noThrow=true");
    } catch (Exception e) {
        writefln("tvi.catch.corner.asException=%s msg=[%s]", e.classinfo.name, e.msg);
    } catch (Error e) {
        writefln("tvi.catch.corner.asError=%s msg=[%s]", e.classinfo.name, e.msg);
    }

    // ---- array-length edges -------------------------------------------------
    ParametricSurface[5] shortSet = faces_of(q)[0 .. 5];
    try {
        auto v = new TFIVolume(shortSet);
        writefln("tvi.shortArray.noThrow=%s", v.toString());
    } catch (Throwable e) {
        report("tvi.shortArray", e);
    }
    ParametricSurface[8] longSet;
    auto fs6 = faces_of(q);
    foreach (i; 0 .. 6) longSet[i] = fs6[i];
    longSet[6] = new ProbeSurface(Vector3(9,9,9), Vector3(8,8,8), Vector3(7,7,7), Vector3(6,6,6));
    longSet[7] = new ProbeSurface(Vector3(-9,-9,-9), Vector3(-8,-8,-8), Vector3(-7,-7,-7),
                                  Vector3(-6,-6,-6));
    try {
        auto v = new TFIVolume(longSet);
        writefln("tvi.longArray.accepted=%s", (v.faces[0] !is null));
        writefln("tvi.longArray.str=%s", v.toString());
    } catch (Throwable e) {
        report("tvi.longArray", e);
    }
    Vector3[7] shortPts = q[0 .. 7];
    try {
        auto v = new TFIVolume(shortPts);
        writefln("tvi.shortPts.noThrow=%s", v.toString());
    } catch (Throwable e) {
        report("tvi.shortPts", e);
    }

    // ---- the D unittest (D:164-190) -----------------------------------------
    Vector3[8] p;
    p[0] = Vector3(0.0, 0.1, 0.0);
    p[1] = Vector3(1.0, 0.1, 0.0);
    p[2] = Vector3(1.0, 1.1, 0.0);
    p[3] = Vector3(0.0, 1.1, 0.0);
    p[4] = Vector3(0.0, 0.1, 3.0);
    p[5] = Vector3(1.0, 0.1, 3.0);
    p[6] = Vector3(1.0, 1.1, 3.0);
    p[7] = Vector3(0.0, 1.1, 3.0);

    auto simple_box = new TFIVolume(p);
    auto c = simple_box(0.1, 0.1, 0.5);
    dump3("tvi.ut.c", c);
    writefln("tvi.ut.simple.approxEqualVectors=%s", approxEqualVectors(c, Vector3(0.1, 0.2, 1.5)));
    writefln("tvi.ut.simple.residual dx=%.17g dy=%.17g dz=%.17g", c.x - 0.1, c.y - 0.2,
             c.z - 1.5);
    writefln("tvi.ut.simple.str=%s", simple_box.toString());
    foreach (i; 0 .. 8) dump3(format("tvi.ut.simple.p[%d]", i), simple_box.p[i]);
    foreach (i; 0 .. 6) writefln("tvi.ut.simple.faces[%d]=%s", i, to!string(simple_box.faces[i]));

    ParametricSurface[6] my_faces;
    my_faces[Face.north]  = new CoonsPatch(p[3], p[2], p[6], p[7]);
    my_faces[Face.south]  = new CoonsPatch(p[0], p[1], p[5], p[4]);
    my_faces[Face.east]   = new CoonsPatch(p[1], p[2], p[6], p[5]);
    my_faces[Face.west]   = new CoonsPatch(p[0], p[3], p[7], p[4]);
    my_faces[Face.top]    = new CoonsPatch(p[4], p[5], p[6], p[7]);
    my_faces[Face.bottom] = new CoonsPatch(p[0], p[1], p[2], p[3]);
    auto generic_box = new TFIVolume(my_faces);
    auto d = generic_box(0.1, 0.1, 0.5);
    dump3("tvi.ut.d", d);
    writefln("tvi.ut.generic.approxEqualVectors=%s", approxEqualVectors(d, Vector3(0.1, 0.2, 1.5)));
    writefln("tvi.ut.generic.residual dx=%.17g dy=%.17g dz=%.17g", d.x - 0.1, d.y - 0.2,
             d.z - 1.5);
    writefln("tvi.ut.generic.str=%s", generic_box.toString());
    writefln("tvi.ut.generic.sameTextAsSimple=%s",
             (generic_box.toString() == simple_box.toString()));
    foreach (i; 0 .. 8) dump3(format("tvi.ut.generic.p[%d]", i), generic_box.p[i]);
    auto utDup = cast(TFIVolume) generic_box.dup();
    writefln("tvi.ut.generic.dup.textSame=%s",
             (utDup.toString() == generic_box.toString()));
}
