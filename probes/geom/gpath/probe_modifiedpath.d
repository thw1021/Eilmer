// probes/geom/gpath/probe_modifiedpath.d — what geom/gpath/modifiedpath.d produces.
//
// modifiedpath.d is the deepest wrapper hierarchy in the tree:
//   Path -> ReParameterizedPath -> ArcLengthParameterizedPath
//                                -> SubRangedPath -> ReversedPath
//   Path -> TransformedPath -> TranslatedPath / MirrorImagePath / RotatedAboutZAxisPath
// Every wrapper delegates to a *dup* of the path it is given, so a wrapper that
// forwards to the wrong handle, or that forgets to carry the parameter derivative
// through the chain, still returns something plausible at t = 0.5. The rows below
// therefore print, for each wrapper and each t of the shared sweep, the wrapper's
// opCall/dpdt/d2pdt2, its underlying_t / d_underlying_t_dt / d2_underlying_t_dt2,
// and the *inner* path's own values at the mapped parameter -- so a broken
// delegation shows as a line diff instead of a number that merely looks
// reasonable. toString() and classString() come along inside dump_path() because
// the solver and grid logs print them (wave-02-brief 4b item 3), including D's
// "ArcLengthParametrizedPath" spelling (modifiedpath.d:76, no 'e') and
// ReversedPath's inherited "SubRangedPath" identity (:194-204).
//
// underlying_t/d_underlying_t_dt/d2_underlying_t_dt2/apply_transform are protected
// and arc_length_vector likewise, so the Probe* subclasses below add *forwarders*
// that qualify the call with `super.` -- they override nothing, so every virtual
// dispatch inside modifiedpath.d still runs the inherited implementation. The same
// shape is used on the C++ side in tests/test_geom_gpath_modifiedpath.cpp.
//
// Build and capture. The reference tree is copied because geom/gpath/path.d:24
// imports geom.surface, whose closure uses `new` inside `@nogc` functions, which
// ldc2 1.40 rejects (nm/bbla.d(709) is the same defect recorded in
// probes/gasdyn/probe_idealgasflow.d; the explicit-file-list recipe there cannot
// be used here because the import closure of geom.elements is dozens of modules
// deep). Deleting @nogc changes a compile-time attribute only -- no statement,
// validated in probes/geom/gpath/probe_path.d's header against dmd.
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/gpath \
//     probes/geom/gpath/probe_modifiedpath.d -of=/tmp/probe_mp -od/tmp/probe_mp_obj \
//     && /tmp/probe_mp > baselines/e587b235/probes/geom_gpath_modifiedpath.txt
import std.conv;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.gpath.path;
import geom.gpath.line;
import geom.gpath.arc;
import geom.gpath.bezier;
import geom.gpath.polyline;
import geom.gpath.modifiedpath;
import nm.number;
import pathdump;

class ProbeALP : ArcLengthParameterizedPath {
    this(const Path other) { super(other); }
    this(const ArcLengthParameterizedPath other) { super(other.underlying_path); }
    double ut(double t) const { return super.underlying_t(t); }
    double dut(double t) const { return super.d_underlying_t_dt(t); }
    double d2ut(double t) const { return super.d2_underlying_t_dt2(t); }
    double[] table() const { return arc_length_vector.dup(); }
}

class ProbeSub : SubRangedPath {
    this(const Path other, double a, double b) { super(other, a, b); }
    double ut(double t) const { return super.underlying_t(t); }
    double dut(double t) const { return super.d_underlying_t_dt(t); }
    double d2ut(double t) const { return super.d2_underlying_t_dt2(t); }
}

class ProbeRev : ReversedPath {
    this(const Path other) { super(other); }
    double ut(double t) const { return super.underlying_t(t); }
    double dut(double t) const { return super.d_underlying_t_dt(t); }
    double d2ut(double t) const { return super.d2_underlying_t_dt2(t); }
}

class ProbeTrans : TranslatedPath {
    this(const Path other, const Vector3 s) { super(other, s); }
    Vector3 applyT(ref Vector3 p) const { return super.apply_transform(p); }
}

class ProbeMirror : MirrorImagePath {
    this(const Path other, const Vector3 pt, const Vector3 nm) { super(other, pt, nm); }
    Vector3 applyT(ref Vector3 p) const { return super.apply_transform(p); }
}

class ProbeRot : RotatedAboutZAxisPath {
    this(const Path other, double angle) { super(other, angle); }
    Vector3 applyT(ref Vector3 p) const { return super.apply_transform(p); }
}

/// The wrapper, the inner path at the mapped parameter, and the parameter
/// derivatives, over the shared sweep. R is one of the Probe* subclasses.
void dump_reparam(R)(const char[] tag, R rp, const Path inner)
{
    dump_path(tag, rp);
    foreach (t; t_sweep()) {
        double u = rp.ut(t);
        Vector3 w = rp(t);
        Vector3 i = inner(u);
        auto d1 = rp.dpdt(t);
        auto d2 = rp.d2pdt2(t);
        Vector3 id1 = inner.dpdt(u);
        writefln("%s ut t=%.17g u=%.17g dut=%.17g d2ut=%.17g wrap x=%.17g y=%.17g z=%.17g "
                 ~ "inner(u) x=%.17g y=%.17g z=%.17g bitmatch=%s",
                 tag, t, u, rp.dut(u), rp.d2ut(u), w.x, w.y, w.z, i.x, i.y, i.z,
                 (w.x == i.x && w.y == i.y && w.z == i.z));
        // chain rule check: wrapper.dpdt should be inner.dpdt(u) * dut
        writefln("%s d1 t=%.17g wrap x=%.17g y=%.17g z=%.17g inner(u) x=%.17g y=%.17g z=%.17g "
                 ~ "chain x=%.17g y=%.17g z=%.17g",
                 tag, t, d1.x, d1.y, d1.z, id1.x, id1.y, id1.z,
                 (id1 * rp.dut(u)).x, (id1 * rp.dut(u)).y, (id1 * rp.dut(u)).z);
        writefln("%s d2 t=%.17g wrap x=%.17g y=%.17g z=%.17g inner.opCall t=%.17g x=%.17g "
                 ~ "y=%.17g z=%.17g inner.d2pdt2 t=%.17g x=%.17g y=%.17g z=%.17g",
                 tag, t, d2.x, d2.y, d2.z, t, inner(t).x, inner(t).y, inner(t).z,
                 t, inner.d2pdt2(t).x, inner.d2pdt2(t).y, inner.d2pdt2(t).z);
    }
    // Path's own finite differences vs. the wrapper's overrides (the branch of
    // modifiedpath.d:349-350 that D's unittest takes).
    foreach (t; [0.0, 0.5, 1.0]) {
        auto fd1 = rp.Path.dpdt(t);
        auto fd2 = rp.Path.d2pdt2(t);
        auto od1 = rp.dpdt(t);
        auto od2 = rp.d2pdt2(t);
        writefln("%s fdcmp t=%.17g Path.dpdt x=%.17g y=%.17g z=%.17g dpdt x=%.17g y=%.17g "
                 ~ "z=%.17g Path.d2pdt2 x=%.17g y=%.17g z=%.17g d2pdt2 x=%.17g y=%.17g z=%.17g",
                 tag, t, fd1.x, fd1.y, fd1.z, od1.x, od1.y, od1.z,
                 fd2.x, fd2.y, fd2.z, od2.x, od2.y, od2.z);
    }
}

void dump_transformed(const char[] tag, TransformedPath tp, const Path inner)
{
    dump_path(tag, tp);
    foreach (t; t_sweep()) {
        auto w = tp(t);
        auto i = inner(t);
        auto d1 = tp.dpdt(t);
        auto d2 = tp.d2pdt2(t);
        writefln("%s cmp t=%.17g wrap x=%.17g y=%.17g z=%.17g inner x=%.17g y=%.17g z=%.17g",
                 tag, t, w.x, w.y, w.z, i.x, i.y, i.z);
        writefln("%s der t=%.17g dpdt x=%.17g y=%.17g z=%.17g d2pdt2 x=%.17g y=%.17g z=%.17g",
                 tag, t, d1.x, d1.y, d1.z, d2.x, d2.y, d2.z);
    }
}

void dump_table(const char[] tag, const double[] a)
{
    writefln("%s table.length=%d", tag, a.length);
    foreach (i, v; a) {
        writefln("%s table[%d]=%.17g", tag, i, v);
    }
}

void main()
{
    // Inner paths used by the wrappers: a Line (exact dpdt/d2pdt2 overrides,
    // line.d:29-38) and a Bezier (its own derivative curves, bezier.d:43-50), so
    // a wrapper that drops a derivative override is measurable against both.
    auto ln = new Line(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 2.0, 0.5));
    auto a = Vector3([2.0, 2.0, 0.0]);
    auto b = Vector3([1.0, 2.0, 1.0]);
    auto c = Vector3([1.0, 2.0, 0.0]);
    auto bz = new Bezier([a, c, b]);
    auto arc = new Arc(a, b, c);

    // --- SubRangedPath: forward, identity, and a reversed range ---------------
    auto sr = new ProbeSub(bz, 0.25, 0.75);
    dump_reparam("mp.sub.fwd", sr, bz);
    auto sr0 = new ProbeSub(ln, 0.0, 1.0);
    dump_reparam("mp.sub.identity", sr0, ln);
    auto srNeg = new ProbeSub(ln, 1.0, 0.25);
    dump_reparam("mp.sub.revrange", srNeg, ln);
    writefln("mp.sub.members t0=%.17g t1=%.17g srNeg.t0=%.17g srNeg.t1=%.17g",
             sr.t0, sr.t1, srNeg.t0, srNeg.t1);

    // --- ReversedPath (a SubRangedPath with t0 = 1, t1 = 0) ------------------
    auto rv = new ProbeRev(bz);
    dump_reparam("mp.rev.bezier", rv, bz);
    auto rvL = new ProbeRev(ln);
    dump_reparam("mp.rev.line", rvL, ln);

    // --- ArcLengthParameterizedPath -----------------------------------------
    auto al = new ProbeALP(bz);
    dump_reparam("mp.al.bezier", al, bz);
    dump_table("mp.al.bezier", al.table());
    auto alL = new ProbeALP(ln);
    dump_reparam("mp.al.line", alL, ln);
    // Only the deciles of the second table; the first is complete and the loop
    // that builds it (modifiedpath.d:88-99) is the same 100-chord sum.
    auto tbl = alL.table();
    foreach (i; 0 .. 11) {
        writefln("mp.al.line table.decile[%d]=%.17g", i, tbl[i * 10]);
    }
    auto alArc = new ProbeALP(arc);
    dump_reparam("mp.al.arc", alArc, arc);
    auto pl = new Polyline([cast(Path) arc, cast(Path) new Line(b, c)]);
    auto alPoly = new ProbeALP(pl);
    dump_reparam("mp.al.polyline", alPoly, pl);

    // The table's last entry is the chord sum the parameterisation divides by.
    writefln("mp.al.bezier L.table=%.17g L.inner=%.17g", al.table()[$ - 1],
             al.underlying_path.length());
    foreach (frac; [0.0, 0.1, 0.25, 0.5, 0.75, 0.9, 1.0, -0.1, 1.1]) {
        writefln("mp.al.bezier ut frac=%.17g u=%.17g", frac, al.ut(frac));
    }

    // --- TransformedPath family ----------------------------------------------
    auto tr = new ProbeTrans(bz, Vector3(1.0, -2.0, 0.5));
    dump_transformed("mp.trans", tr, bz);
    auto trL = new TranslatedPath(ln, Vector3(0.0, 0.0, 3.0));
    dump_transformed("mp.trans.line", trL, ln);
    auto mi = new ProbeMirror(bz, Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
    dump_transformed("mp.mirror", mi, bz);
    auto miL = new MirrorImagePath(ln, Vector3(0.5, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
    dump_transformed("mp.mirror.line", miL, ln);
    // modifiedpath.d:391-393: an Arc rotated about the z-axis by PI/4, with PI
    // computed in 80-bit real and rounded on the way into the double member.
    auto rot = new ProbeRot(arc, PI / 4);
    dump_transformed("mp.rot", rot, arc);
    writefln("mp.rot dtheta=%.20g", rot.dtheta);
    auto rotL = new RotatedAboutZAxisPath(ln, 1.0);
    dump_transformed("mp.rot.line", rotL, ln);

    // apply_transform directly, showing that D's `ref Vector3 p` argument is not
    // changed by the translate case but is by the mirror/rotate mutators
    // (vector3.d:503, :521 return `ref Vector3` and modify *this).
    Vector3 start = Vector3(1.0, 2.0, 3.0);
    Vector3 pv = start;
    auto tr2 = tr.applyT(pv);
    writefln("mp.apply.trans in x=%.17g y=%.17g z=%.17g ret x=%.17g y=%.17g z=%.17g after x=%.17g"
             ~ " y=%.17g z=%.17g", start.x, start.y, start.z, tr2.x, tr2.y, tr2.z,
             pv.x, pv.y, pv.z);
    pv = start;
    auto mi2 = mi.applyT(pv);
    writefln("mp.apply.mirror in x=%.17g y=%.17g z=%.17g ret x=%.17g y=%.17g z=%.17g after x=%.17g"
             ~ " y=%.17g z=%.17g", start.x, start.y, start.z, mi2.x, mi2.y, mi2.z,
             pv.x, pv.y, pv.z);
    pv = start;
    auto rt2 = rot.applyT(pv);
    writefln("mp.apply.rot in x=%.17g y=%.17g z=%.17g ret x=%.17g y=%.17g z=%.17g after x=%.17g"
             ~ " y=%.17g z=%.17g", start.x, start.y, start.z, rt2.x, rt2.y, rt2.z,
             pv.x, pv.y, pv.z);

    // --- classString / toString, the load-bearing log text --------------------
    foreach (pth; [cast(Path) sr, cast(Path) rv, cast(Path) al, cast(Path) tr,
                   cast(Path) mi, cast(Path) rot, cast(Path) trL, cast(Path) miL,
                   cast(Path) rotL, cast(Path) sr0, cast(Path) alPoly, cast(Path) alArc]) {
        writefln("mp.ident class=%s str=%s", pth.classString(), pth.toString());
    }

    // --- dup() and the copy constructors --------------------------------------
    writefln("mp.dup.sub class=%s", sr.dup().classString());
    writefln("mp.dup.rev class=%s", rv.dup().classString());
    writefln("mp.dup.al class=%s", al.dup().classString());
    dump_path("mp.dup.sub", sr.dup());
    dump_path("mp.dup.rev", rv.dup());
    dump_path("mp.dup.al", al.dup());
    dump_path("mp.dup.trans", tr.dup());
    dump_path("mp.dup.mirror", mi.dup());
    dump_path("mp.dup.rot", rot.dup());

    // A wrapper built from a wrapper: the inner dup() chain must not alias, and
    // the composed parameter map must be the two maps applied in D's order.
    auto nested = new ProbeSub(new ReversedPath(bz), 0.0, 0.5);
    dump_path("mp.nested", nested);
    foreach (t; t_sweep()) {
        double u = nested.ut(t);
        auto ip = nested.underlying_path(u);
        auto wp = nested(t);
        writefln("mp.nested ut t=%.17g u=%.17g innerAt x=%.17g y=%.17g z=%.17g wrap x=%.17g "
                 ~ "y=%.17g z=%.17g", t, u, ip.x, ip.y, ip.z, wp.x, wp.y, wp.z);
    }
    // modifiedpath.d:62-65, the ArcLengthParameterizedPath copy constructor.
    auto alcp = new ProbeALP(al);
    dump_path("mp.al.copy", alcp);
    writefln("mp.al.copy table.length=%d last=%.17g", alcp.table().length,
             alcp.table()[$ - 1]);
    foreach (t; t_sweep()) {
        writefln("mp.al.copy ut t=%.17g u=%.17g orig u=%.17g same=%s", t, alcp.ut(t), al.ut(t),
                 alcp.ut(t) == al.ut(t));
    }

    // --- the quantities D's own unittests compute (modifiedpath.d:335-358) ----
    auto acb = new ArcLengthParameterizedPath(new Bezier([a, c, b]));
    auto L = acb.underlying_path.length();
    auto dA = Vector3(-1, 0, 1);
    auto dAdt = abs(dA) / L;
    Vector3 d2A = Vector3(-1, 0, 1);
    auto d = Vector3(2, 0, 2);
    auto d2Adt2 = unit(d2A).dot(d) / L;
    writefln("mp.ut2 L=%.17g dAdt=%.17g d2Adt2=%.17g", L, dAdt, d2Adt2);
    auto p1 = acb.dpdt(0.5);
    auto p2 = acb.Path.dpdt(0.5);
    auto p3 = acb.d2pdt2(0.5);
    auto p4 = acb.Path.d2pdt2(0.5);
    writefln("mp.ut2 dpdt x=%.17g y=%.17g z=%.17g Path.dpdt x=%.17g y=%.17g z=%.17g",
             p1.x, p1.y, p1.z, p2.x, p2.y, p2.z);
    writefln("mp.ut2 d2pdt2 x=%.17g y=%.17g z=%.17g Path.d2pdt2 x=%.17g y=%.17g z=%.17g",
             p3.x, p3.y, p3.z, p4.x, p4.y, p4.z);
    auto e1 = Vector3(-1, 0, 1) / dAdt;
    auto e2 = Vector3(2, 0, 2) / pow(dAdt, 2) - Vector3(-1, 0, 1) * d2Adt2 / pow(dAdt, 3);
    writefln("mp.ut2 hand.dpdt x=%.17g y=%.17g z=%.17g hand.d2pdt2 x=%.17g y=%.17g z=%.17g",
             e1.x, e1.y, e1.z, e2.x, e2.y, e2.z);
    // modifiedpath.d:326-332
    auto ua = Vector3([0.0, 0.0, 0.0]);
    auto ub = Vector3([1.0, 1.0, 1.0]);
    auto uc = Vector3([4.0, 4.0, 4.0]);
    auto abc_bz = new Bezier([ua, ub, uc]);
    auto abc_dsh = new ArcLengthParameterizedPath(abc_bz);
    auto fu = abc_dsh(0.5);
    writefln("mp.ut1 f x=%.17g y=%.17g z=%.17g", fu.x, fu.y, fu.z);
    // modifiedpath.d:360-381
    auto pol = new Polyline([cast(Path) new Arc(a, b, c), cast(Path) new Line(b, c)]);
    auto revp = new ReversedPath(pol);
    writefln("mp.ut3 pol(0.25) x=%.17g y=%.17g z=%.17g rev(0.75) x=%.17g y=%.17g z=%.17g",
             pol(0.25).x, pol(0.25).y, pol(0.25).z, revp(0.75).x, revp(0.75).y, revp(0.75).z);
    auto sub = new SubRangedPath(new Bezier([a, c, b]), 0.5, 0.75);
    auto sd1 = sub.dpdt(0);
    auto sd2 = sub.d2pdt2(0);
    writefln("mp.ut3 sub dpdt x=%.17g y=%.17g z=%.17g d2pdt2 x=%.17g y=%.17g z=%.17g",
             sd1.x, sd1.y, sd1.z, sd2.x, sd2.y, sd2.z);
    auto rsub = new ReversedPath(new Bezier([a, c, b]));
    auto rd1 = rsub.dpdt(0.5);
    auto rd2 = rsub.d2pdt2(0.5);
    writefln("mp.ut3 rev dpdt x=%.17g y=%.17g z=%.17g d2pdt2 x=%.17g y=%.17g z=%.17g",
             rd1.x, rd1.y, rd1.z, rd2.x, rd2.y, rd2.z);
    // modifiedpath.d:383-394, the rotated Arc whose writeln is part of the unittest
    auto arc4 = new Arc(Vector3(2.0, 0.0, 0.0), Vector3(0.0, 2.0, 0.0), Vector3(0.0, 0.0, 0.0));
    auto abc_rotated = new RotatedAboutZAxisPath(arc4, PI / 4);
    writefln("abc_rotated(1.0)=%s", abc_rotated(1.0));
    writefln("mp.ut4 x=%.17g y=%.17g z=%.17g sqrt2neg x=%.17g y=%.17g z=%.17g",
             abc_rotated(1.0).x, abc_rotated(1.0).y, abc_rotated(1.0).z,
             (sqrt(2.0) * Vector3(-1, 1, 0)).x, (sqrt(2.0) * Vector3(-1, 1, 0)).y,
             (sqrt(2.0) * Vector3(-1, 1, 0)).z);
}
