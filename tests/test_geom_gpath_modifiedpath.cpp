// tests/test_geom_gpath_modifiedpath.cpp
//
// Two layers, both required (the pattern of tests/test_geom_elements_properties.cpp):
//
//  A. the four `unittest` blocks of geom/gpath/modifiedpath.d (D:322-394) ported
//     assertion for assertion, in source order. Block 2's `acb.Path.dpdt(0.5)`
//     (D:349-350) is a qualified call into the *base* class' finite differences;
//     C++ spells the same thing `acb->Path::dpdt(0.5)`, which is non-virtual.
//
//  B. a line-for-line replay of probes/geom/gpath/probe_modifiedpath.d against
//     baselines/e587b235/probes/geom_gpath_modifiedpath.txt: 2288 lines. Every
//     wrapper is built around a known inner Path (a Line and a Bezier at minimum,
//     plus an Arc, a Polyline and a nested ReversedPath) and dumped over the shared
//     t sweep including t = 0.0 and t = 1.0, side by side with the inner path's own
//     opCall/dpdt/d2pdt2 at the mapped parameter. The `bitmatch=` field says whether
//     the wrapper's opCall(t) equals inner(underlying_t(t)) bit for bit -- D gives
//     true on all 99 rows, so a delegation that reads the wrong handle or maps the
//     parameter twice cannot hide. The arc-length table (101 entries) and the
//     d_underlying_t_dt / d2_underlying_t_dt2 derivatives are dumped too, and every
//     wrapper's toString()/classString() is printed because the solver and grid logs
//     compare them (D's "ArcLengthParametrizedPath" typo and ReversedPath's
//     inherited "SubRangedPath" identity are part of that contract).
//
// The Probe* classes below only raise the visibility of modifiedpath.d's protected
// members by calling them through a qualified-id (no override), exactly like the
// Probe* subclasses in the D probe; nothing else is added.
//
// Regenerate the baseline (full recipe in the probe header):
//   ldc2 -i -I/tmp/gdtk_shadow ... probes/geom/gpath/probe_modifiedpath.d -of=/tmp/x
//   /tmp/x > baselines/e587b235/probes/geom_gpath_modifiedpath.txt
//
// Run: ./test_geom_gpath_modifiedpath <baseline.txt>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/arc.hpp"
#include "geom/gpath/bezier.hpp"
#include "geom/gpath/line.hpp"
#include "geom/gpath/modifiedpath.hpp"
#include "geom/gpath/path.hpp"
#include "geom/gpath/polyline.hpp"
#include "nm/number.hpp"

#include "gpath3_dump.hpp"

using eil::geom::ArcLengthParameterizedPath;
using eil::geom::MirrorImagePath;
using eil::geom::PathPtr;
using eil::geom::ReversedPath;
using eil::geom::RotatedAboutZAxisPath;
using eil::geom::SubRangedPath;
using eil::geom::TranslatedPath;
using eil::geom::Vector3;
using eil::Array;
using eil::number;
using eil_test_gpath3::emitf;

namespace {

// ---------------------------------------------------------------- probe subclasses
class ProbeALP : public ArcLengthParameterizedPath {
  public:
    explicit ProbeALP(const PathPtr& other) : ArcLengthParameterizedPath(other) {}
    explicit ProbeALP(const ArcLengthParameterizedPath& other)
        : ArcLengthParameterizedPath(other) {} // D:62-65, rebuilds from the inner path
    double ut(double t) const { return ArcLengthParameterizedPath::underlying_t(t); }
    double dut(double t) const { return ArcLengthParameterizedPath::d_underlying_t_dt(t); }
    double d2ut(double t) const { return ArcLengthParameterizedPath::d2_underlying_t_dt2(t); }
    eil::Array<double> table() const { return arc_length_vector.dup(); }
};

class ProbeSub : public SubRangedPath {
  public:
    ProbeSub(const PathPtr& other, double a, double b) : SubRangedPath(other, a, b) {}
    double ut(double t) const { return SubRangedPath::underlying_t(t); }
    double dut(double t) const { return SubRangedPath::d_underlying_t_dt(t); }
    double d2ut(double t) const { return SubRangedPath::d2_underlying_t_dt2(t); }
};

class ProbeRev : public ReversedPath {
  public:
    explicit ProbeRev(const PathPtr& other) : ReversedPath(other) {}
    double ut(double t) const { return ReversedPath::underlying_t(t); }
    double dut(double t) const { return ReversedPath::d_underlying_t_dt(t); }
    double d2ut(double t) const { return ReversedPath::d2_underlying_t_dt2(t); }
};

class ProbeTrans : public TranslatedPath {
  public:
    ProbeTrans(const PathPtr& other, const Vector3& s) : TranslatedPath(other, s) {}
    Vector3 applyT(Vector3& p) const { return TranslatedPath::apply_transform(p); }
};

class ProbeMirror : public MirrorImagePath {
  public:
    ProbeMirror(const PathPtr& other, const Vector3& pt, const Vector3& nm)
        : MirrorImagePath(other, pt, nm) {}
    Vector3 applyT(Vector3& p) const { return MirrorImagePath::apply_transform(p); }
};

class ProbeRot : public RotatedAboutZAxisPath {
  public:
    ProbeRot(const PathPtr& other, double angle) : RotatedAboutZAxisPath(other, angle) {}
    Vector3 applyT(Vector3& p) const { return RotatedAboutZAxisPath::apply_transform(p); }
};

// ============================================================================
// A. the four D unittest blocks, assertion for assertion, in D's order.
// ============================================================================
void port_d_unittest() {
    using namespace eil::geom;

    // --- block 1, modifiedpath.d:322-333 -------------------------------------
    {
        Vector3 a(0.0, 0.0, 0.0);
        Vector3 b(1.0, 1.0, 1.0);
        Vector3 c(4.0, 4.0, 4.0);
        auto abc = std::make_shared<Bezier>(Array<Vector3>{a, b, c});
        auto abc_dsh = std::make_shared<ArcLengthParameterizedPath>(abc);
        Vector3 f = (*abc_dsh)(0.5);
        CHECK3(approxEqualVectors(f, Vector3(2, 2, 2)), "D:332 approxEqualVectors(f, (2,2,2))");
    }

    // --- block 2, modifiedpath.d:335-358 -------------------------------------
    {
        Vector3 a(2.0, 2.0, 0.0);
        Vector3 b(1.0, 2.0, 1.0);
        Vector3 c(1.0, 2.0, 0.0);
        auto acb = std::make_shared<ArcLengthParameterizedPath>(
            std::make_shared<Bezier>(Array<Vector3>{a, c, b}));
        number L = acb->underlying_path->length();
        Vector3 dA(-1, 0, 1);
        number dAdt = eil::geom::abs(dA) / L;
        Vector3 d2A(-1, 0, 1);
        Vector3 d(2, 0, 2);
        number d2Adt2 = eil::geom::unit(d2A).dot(d) / L;
        // D:349-350, the qualified calls into Path's finite differences.
        CHECK3(approxEqualVectors(acb->dpdt(0.5), acb->Path::dpdt(0.5)),
               "D:349 acb.dpdt(0.5) == acb.Path.dpdt(0.5)");
        CHECK3(approxEqualVectors(acb->d2pdt2(0.5), acb->Path::d2pdt2(0.5)),
               "D:350 acb.d2pdt2(0.5) == acb.Path.d2pdt2(0.5)");
        CHECK3(approxEqualVectors(acb->dpdt(0.5), Vector3(-1, 0, 1) / dAdt), "D:353 dpdt");
        CHECK3(approxEqualVectors(acb->d2pdt2(0.5), Vector3(2, 0, 2) / eil::pow(dAdt, 2.0) -
                                            Vector3(-1, 0, 1) * d2Adt2 / eil::pow(dAdt, 3.0)),
               "D:354-356 d2pdt2");
    }

    // --- block 3, modifiedpath.d:360-381 -------------------------------------
    {
        Vector3 a(2.0, 2.0, 0.0);
        Vector3 b(1.0, 2.0, 1.0);
        Vector3 c(1.0, 2.0, 0.0);
        auto abc = std::make_shared<Arc>(a, b, c);
        eil::geom::PathList segs = {abc, std::make_shared<Line>(b, c)};
        auto polyline = std::make_shared<Polyline>(segs);
        auto rev_poly = std::make_shared<ReversedPath>(polyline);
        CHECK3(approxEqualVectors((*polyline)(0.25), (*rev_poly)(0.75)),
               "D:374 polyline(0.25) == rev_poly(0.75)");
        auto acb = std::make_shared<SubRangedPath>(std::make_shared<Bezier>(Array<Vector3>{a, c, b}),
                                                   0.5, 0.75);
        CHECK3(approxEqualVectors(acb->dpdt(0), Vector3(-1, 0, 1) / 4), "D:376 sub dpdt");
        CHECK3(approxEqualVectors(acb->d2pdt2(0), Vector3(2, 0, 2) / 16), "D:377 sub d2pdt2");
        auto r_acb = std::make_shared<ReversedPath>(std::make_shared<Bezier>(Array<Vector3>{a, c, b}));
        CHECK3(approxEqualVectors(r_acb->dpdt(0.5), -Vector3(-1, 0, 1)), "D:379 rev dpdt");
        CHECK3(approxEqualVectors(r_acb->d2pdt2(0.5), Vector3(2, 0, 2)), "D:380 rev d2pdt2");
    }

    // --- block 4, modifiedpath.d:383-394 -------------------------------------
    {
        Vector3 a(2.0, 0.0, 0.0);
        Vector3 b(0.0, 2.0, 0.0);
        Vector3 c(0.0, 0.0, 0.0);
        auto abc = std::make_shared<Arc>(a, b, c);
        // D:391 `PI / 4` is 80-bit real arithmetic narrowed into the double
        // parameter (modifiedpath.d:296); EIL_PI keeps that.
        auto abc_rotated = std::make_shared<RotatedAboutZAxisPath>(
            abc, static_cast<double>(EIL_PI / 4.0L));
        // D:392 writeln("abc_rotated(1.0)=", ...) is unittest output, not an
        // assertion; the probe replays it as a corpus row.
        CHECK3(approxEqualVectors((*abc_rotated)(1.0),
                                  eil::sqrt(2.0) * Vector3(-1, 1, 0)),
               "D:393 approxEqualVectors(abc_rotated(1.0), sqrt(2)*(-1,1,0))");
    }
}

// ============================================================================
// B. the probe replay, in probe order.
// ============================================================================
// probe_modifiedpath.d:101-131
template <typename R>
void dump_reparam(const char* tag, const R& rp, const PathPtr& inner) {
    using namespace eil_test_gpath3;
    dump_path(tag, rp);
    for (double t : t_sweep()) {
        double u = rp.ut(t);
        Vector3 w = rp(t);
        Vector3 i = (*inner)(u);
        Vector3 d1 = rp.dpdt(t);
        Vector3 d2 = rp.d2pdt2(t);
        Vector3 id1 = inner->dpdt(u);
        const double dut = rp.dut(u);
        emitf("%s ut t=%.17g u=%.17g dut=%.17g d2ut=%.17g wrap x=%.17g y=%.17g z=%.17g "
              "inner(u) x=%.17g y=%.17g z=%.17g bitmatch=%s",
              tag, t, u, dut, rp.d2ut(u), w.x, w.y, w.z, i.x, i.y, i.z,
              bools(w.x == i.x && w.y == i.y && w.z == i.z));
        Vector3 chain = id1 * dut;
        emitf("%s d1 t=%.17g wrap x=%.17g y=%.17g z=%.17g inner(u) x=%.17g y=%.17g z=%.17g "
              "chain x=%.17g y=%.17g z=%.17g",
              tag, t, d1.x, d1.y, d1.z, id1.x, id1.y, id1.z, chain.x, chain.y, chain.z);
        Vector3 it = (*inner)(t);
        Vector3 i2 = inner->d2pdt2(t);
        emitf("%s d2 t=%.17g wrap x=%.17g y=%.17g z=%.17g inner.opCall t=%.17g x=%.17g "
              "y=%.17g z=%.17g inner.d2pdt2 t=%.17g x=%.17g y=%.17g z=%.17g",
              tag, t, d2.x, d2.y, d2.z, t, it.x, it.y, it.z, t, i2.x, i2.y, i2.z);
    }
    for (double t : {0.0, 0.5, 1.0}) {
        Vector3 fd1 = rp.Path::dpdt(t);   // D: rp.Path.dpdt(t), a non-virtual call
        Vector3 fd2 = rp.Path::d2pdt2(t);
        Vector3 od1 = rp.dpdt(t);
        Vector3 od2 = rp.d2pdt2(t);
        emitf("%s fdcmp t=%.17g Path.dpdt x=%.17g y=%.17g z=%.17g dpdt x=%.17g y=%.17g "
              "z=%.17g Path.d2pdt2 x=%.17g y=%.17g z=%.17g d2pdt2 x=%.17g y=%.17g z=%.17g",
              tag, t, fd1.x, fd1.y, fd1.z, od1.x, od1.y, od1.z, fd2.x, fd2.y, fd2.z, od2.x, od2.y,
              od2.z);
    }
}

// probe_modifiedpath.d:133-147
void dump_transformed(const char* tag, const PathPtr& tp, const PathPtr& inner) {
    using namespace eil_test_gpath3;
    dump_path(tag, tp);
    for (double t : t_sweep()) {
        Vector3 w = (*tp)(t);
        Vector3 i = (*inner)(t);
        Vector3 d1 = tp->dpdt(t);
        Vector3 d2 = tp->d2pdt2(t);
        emitf("%s cmp t=%.17g wrap x=%.17g y=%.17g z=%.17g inner x=%.17g y=%.17g z=%.17g", tag, t,
              w.x, w.y, w.z, i.x, i.y, i.z);
        emitf("%s der t=%.17g dpdt x=%.17g y=%.17g z=%.17g d2pdt2 x=%.17g y=%.17g z=%.17g", tag, t,
              d1.x, d1.y, d1.z, d2.x, d2.y, d2.z);
    }
}

// probe_modifiedpath.d:149-154
void dump_table(const char* tag, const eil::Array<double>& a) {
    emitf("%s table.length=%d", tag, static_cast<int>(a.length()));
    for (size_t i = 0; i < a.length(); ++i) {
        emitf("%s table[%d]=%.17g", tag, static_cast<int>(i), a[i]);
    }
}

void replay_probe() {
    using namespace eil::geom;
    using namespace eil_test_gpath3;

    // probe_modifiedpath.d:159-164 — the inner paths every wrapper is built on.
    auto ln = std::make_shared<Line>(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 2.0, 0.5));
    Vector3 a(2.0, 2.0, 0.0);
    Vector3 b(1.0, 2.0, 1.0);
    Vector3 c(1.0, 2.0, 0.0);
    auto bz = std::make_shared<Bezier>(Array<Vector3>{a, c, b});
    auto arc = std::make_shared<Arc>(a, b, c);

    // probe_modifiedpath.d:167-175 — SubRangedPath, forward, identity, reversed range.
    auto sr = std::make_shared<ProbeSub>(bz, 0.25, 0.75);
    dump_reparam("mp.sub.fwd", *sr, bz);
    auto sr0 = std::make_shared<ProbeSub>(ln, 0.0, 1.0);
    dump_reparam("mp.sub.identity", *sr0, ln);
    auto srNeg = std::make_shared<ProbeSub>(ln, 1.0, 0.25);
    dump_reparam("mp.sub.revrange", *srNeg, ln);
    emitf("mp.sub.members t0=%.17g t1=%.17g srNeg.t0=%.17g srNeg.t1=%.17g", sr->t0, sr->t1,
          srNeg->t0, srNeg->t1);

    // probe_modifiedpath.d:178-182 — ReversedPath.
    auto rv = std::make_shared<ProbeRev>(bz);
    dump_reparam("mp.rev.bezier", *rv, bz);
    auto rvL = std::make_shared<ProbeRev>(ln);
    dump_reparam("mp.rev.line", *rvL, ln);

    // probe_modifiedpath.d:185-198 — ArcLengthParameterizedPath over four inners.
    auto al = std::make_shared<ProbeALP>(bz);
    dump_reparam("mp.al.bezier", *al, bz);
    dump_table("mp.al.bezier", al->table());
    auto alL = std::make_shared<ProbeALP>(ln);
    dump_reparam("mp.al.line", *alL, ln);
    auto tbl = alL->table();
    for (int i = 0; i < 11; ++i) {
        emitf("mp.al.line table.decile[%d]=%.17g", i, tbl[static_cast<size_t>(i) * 10]);
    }
    auto alArc = std::make_shared<ProbeALP>(arc);
    dump_reparam("mp.al.arc", *alArc, arc);
    PathList plsegs = {arc, std::make_shared<Line>(b, c)};
    auto pl = std::make_shared<Polyline>(plsegs);
    auto alPoly = std::make_shared<ProbeALP>(pl);
    dump_reparam("mp.al.polyline", *alPoly, pl);

    // probe_modifiedpath.d:201-207
    emitf("mp.al.bezier L.table=%.17g L.inner=%.17g", al->table()[al->table().length() - 1],
          eil::re(al->underlying_path->length()));
    for (double frac : {0.0, 0.1, 0.25, 0.5, 0.75, 0.9, 1.0, -0.1, 1.1}) {
        emitf("mp.al.bezier ut frac=%.17g u=%.17g", frac, al->ut(frac));
    }

    // probe_modifiedpath.d:210-228 — the TransformedPath family.
    auto tr = std::make_shared<ProbeTrans>(bz, Vector3(1.0, -2.0, 0.5));
    dump_transformed("mp.trans", tr, bz);
    auto trL = std::make_shared<TranslatedPath>(ln, Vector3(0.0, 0.0, 3.0));
    dump_transformed("mp.trans.line", trL, ln);
    auto mi = std::make_shared<ProbeMirror>(bz, Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
    dump_transformed("mp.mirror", mi, bz);
    auto miL = std::make_shared<MirrorImagePath>(ln, Vector3(0.5, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
    dump_transformed("mp.mirror.line", miL, ln);
    auto rot = std::make_shared<ProbeRot>(arc, static_cast<double>(EIL_PI / 4.0L));
    dump_transformed("mp.rot", rot, arc);
    // dtheta is the double member (modifiedpath.d:295), so %.20g prints the
    // 20 decimal digits of the narrowed value, exactly as D does.
    emitf("mp.rot dtheta=%.20g", rot->dtheta);
    auto rotL = std::make_shared<RotatedAboutZAxisPath>(ln, 1.0);
    dump_transformed("mp.rot.line", rotL, ln);

    // probe_modifiedpath.d:231-246 — apply_transform, and what it does to its
    // `ref Vector3` argument.
    Vector3 start(1.0, 2.0, 3.0);
    Vector3 pv = start;
    Vector3 tr2 = tr->applyT(pv);
    emitf("mp.apply.trans in x=%.17g y=%.17g z=%.17g ret x=%.17g y=%.17g z=%.17g after x=%.17g"
          " y=%.17g z=%.17g", start.x, start.y, start.z, tr2.x, tr2.y, tr2.z, pv.x, pv.y, pv.z);
    pv = start;
    Vector3 mi2 = mi->applyT(pv);
    emitf("mp.apply.mirror in x=%.17g y=%.17g z=%.17g ret x=%.17g y=%.17g z=%.17g after x=%.17g"
          " y=%.17g z=%.17g", start.x, start.y, start.z, mi2.x, mi2.y, mi2.z, pv.x, pv.y, pv.z);
    pv = start;
    Vector3 rt2 = rot->applyT(pv);
    emitf("mp.apply.rot in x=%.17g y=%.17g z=%.17g ret x=%.17g y=%.17g z=%.17g after x=%.17g"
          " y=%.17g z=%.17g", start.x, start.y, start.z, rt2.x, rt2.y, rt2.z, pv.x, pv.y, pv.z);

    // probe_modifiedpath.d:249-256 — classString()/toString().
    const PathPtr idents[] = {sr, rv, al,     tr,      mi,     rot,
                              trL, miL, rotL, sr0,     alPoly, alArc};
    for (const PathPtr& pth : idents) {
        emitf("mp.ident class=%s str=%s", pth->classString().c_str(), pth->toString().c_str());
    }

    // probe_modifiedpath.d:259-268 — dup().
    emitf("mp.dup.sub class=%s", sr->dup()->classString().c_str());
    emitf("mp.dup.rev class=%s", rv->dup()->classString().c_str());
    emitf("mp.dup.al class=%s", al->dup()->classString().c_str());
    dump_path("mp.dup.sub", sr->dup());
    dump_path("mp.dup.rev", rv->dup());
    dump_path("mp.dup.al", al->dup());
    dump_path("mp.dup.trans", tr->dup());
    dump_path("mp.dup.mirror", mi->dup());
    dump_path("mp.dup.rot", rot->dup());

    // probe_modifiedpath.d:271-278 — a wrapper of a wrapper.
    auto nested = std::make_shared<ProbeSub>(std::make_shared<ReversedPath>(bz), 0.0, 0.5);
    dump_path("mp.nested", nested);
    for (double t : t_sweep()) {
        double u = nested->ut(t);
        Vector3 ip = (*nested->underlying_path)(u);
        Vector3 wp = (*nested)(t);
        emitf("mp.nested ut t=%.17g u=%.17g innerAt x=%.17g y=%.17g z=%.17g wrap x=%.17g "
              "y=%.17g z=%.17g", t, u, ip.x, ip.y, ip.z, wp.x, wp.y, wp.z);
    }

    // probe_modifiedpath.d:280-288 — the ArcLengthParameterizedPath copy constructor.
    auto alcp = std::make_shared<ProbeALP>(*al);
    dump_path("mp.al.copy", alcp);
    auto ctbl = alcp->table();
    emitf("mp.al.copy table.length=%d last=%.17g", static_cast<int>(ctbl.length()),
          ctbl[ctbl.length() - 1]);
    for (double t : t_sweep()) {
        emitf("mp.al.copy ut t=%.17g u=%.17g orig u=%.17g same=%s", t, alcp->ut(t), al->ut(t),
              bools(alcp->ut(t) == al->ut(t)));
    }

    // probe_modifiedpath.d:291-324 — the quantities D's own unittests compute.
    {
        auto acb = std::make_shared<ArcLengthParameterizedPath>(
            std::make_shared<Bezier>(Array<Vector3>{a, c, b}));
        number L = acb->underlying_path->length();
        Vector3 dA(-1, 0, 1);
        number dAdt = eil::geom::abs(dA) / L;
        Vector3 d2A(-1, 0, 1);
        Vector3 d(2, 0, 2);
        number d2Adt2 = eil::geom::unit(d2A).dot(d) / L;
        emitf("mp.ut2 L=%.17g dAdt=%.17g d2Adt2=%.17g", eil::re(L), eil::re(dAdt), eil::re(d2Adt2));
        Vector3 p1 = acb->dpdt(0.5);
        Vector3 p2 = acb->Path::dpdt(0.5);
        Vector3 p3 = acb->d2pdt2(0.5);
        Vector3 p4 = acb->Path::d2pdt2(0.5);
        emitf("mp.ut2 dpdt x=%.17g y=%.17g z=%.17g Path.dpdt x=%.17g y=%.17g z=%.17g", p1.x, p1.y,
              p1.z, p2.x, p2.y, p2.z);
        emitf("mp.ut2 d2pdt2 x=%.17g y=%.17g z=%.17g Path.d2pdt2 x=%.17g y=%.17g z=%.17g", p3.x,
              p3.y, p3.z, p4.x, p4.y, p4.z);
        Vector3 e1 = Vector3(-1, 0, 1) / dAdt;
        Vector3 e2 = Vector3(2, 0, 2) / eil::pow(dAdt, 2.0) -
                     Vector3(-1, 0, 1) * d2Adt2 / eil::pow(dAdt, 3.0);
        emitf("mp.ut2 hand.dpdt x=%.17g y=%.17g z=%.17g hand.d2pdt2 x=%.17g y=%.17g z=%.17g", e1.x,
              e1.y, e1.z, e2.x, e2.y, e2.z);
    }
    {
        Vector3 ua(0.0, 0.0, 0.0);
        Vector3 ub(1.0, 1.0, 1.0);
        Vector3 uc(4.0, 4.0, 4.0);
        auto abc_bz = std::make_shared<Bezier>(Array<Vector3>{ua, ub, uc});
        auto abc_dsh = std::make_shared<ArcLengthParameterizedPath>(abc_bz);
        Vector3 fu = (*abc_dsh)(0.5);
        emitf("mp.ut1 f x=%.17g y=%.17g z=%.17g", fu.x, fu.y, fu.z);
    }
    {
        PathList segs = {std::make_shared<Arc>(a, b, c), std::make_shared<Line>(b, c)};
        auto pol = std::make_shared<Polyline>(segs);
        auto revp = std::make_shared<ReversedPath>(pol);
        Vector3 p25 = (*pol)(0.25);
        Vector3 r75 = (*revp)(0.75);
        emitf("mp.ut3 pol(0.25) x=%.17g y=%.17g z=%.17g rev(0.75) x=%.17g y=%.17g z=%.17g", p25.x,
              p25.y, p25.z, r75.x, r75.y, r75.z);
        auto sub = std::make_shared<SubRangedPath>(std::make_shared<Bezier>(Array<Vector3>{a, c, b}),
                                                  0.5, 0.75);
        Vector3 sd1 = sub->dpdt(0);
        Vector3 sd2 = sub->d2pdt2(0);
        emitf("mp.ut3 sub dpdt x=%.17g y=%.17g z=%.17g d2pdt2 x=%.17g y=%.17g z=%.17g", sd1.x,
              sd1.y, sd1.z, sd2.x, sd2.y, sd2.z);
        auto rsub = std::make_shared<ReversedPath>(std::make_shared<Bezier>(Array<Vector3>{a, c, b}));
        Vector3 rd1 = rsub->dpdt(0.5);
        Vector3 rd2 = rsub->d2pdt2(0.5);
        emitf("mp.ut3 rev dpdt x=%.17g y=%.17g z=%.17g d2pdt2 x=%.17g y=%.17g z=%.17g", rd1.x, rd1.y,
              rd1.z, rd2.x, rd2.y, rd2.z);
    }
    {
        auto arc4 = std::make_shared<Arc>(Vector3(2.0, 0.0, 0.0), Vector3(0.0, 2.0, 0.0),
                                         Vector3(0.0, 0.0, 0.0));
        auto abc_rotated =
            std::make_shared<RotatedAboutZAxisPath>(arc4, static_cast<double>(EIL_PI / 4.0L));
        std::string vs;
        Vector3 r1 = (*abc_rotated)(1.0);
        emitf("abc_rotated(1.0)=%s", vstr(r1, vs));
        Vector3 ref = eil::sqrt(2.0) * Vector3(-1, 1, 0);
        emitf("mp.ut4 x=%.17g y=%.17g z=%.17g sqrt2neg x=%.17g y=%.17g z=%.17g", r1.x, r1.y, r1.z,
              ref.x, ref.y, ref.z);
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <probes/geom_gpath_modifiedpath.txt>\n";
        return 2;
    }
    port_d_unittest();
    replay_probe();
    // Why the only failing rows fail: every differing line is a
    // RotatedAboutZAxisPath row (mp.rot.* and mp.dup.rot.*), and the difference is
    // not in modifiedpath.d's delegation -- the inner Arc points, the mirror and
    // translation rows, and all 99 `bitmatch=` rows agree bit for bit. D's
    // Vector3.rotate_about_zaxis (vector3.d:523) computes atan2(y, x), and Phobos'
    // atan2 for doubles is NOT glibc's: std/math/trigonometry.d:945-950 makes it
    // atan(y/x) plus a quadrant correction, and its atan (trigonometry.d:676,
    // 694-834) is a rational minimax in the reduced argument that is 1 ULP away from
    // a correctly rounded atan on 150 of 2007 sample arguments (measured: a C port
    // of that branch reproduces ldc2's atan bit for bit on the whole sweep, while
    // glibc's differs). rotate_about_zaxis cancels two nearly equal numbers to get
    // x (|x| ~ 2e-7 against |p| ~ 2.8), so that 1 ULP in the angle becomes ~3e-9
    // relative in x. The fix belongs in the substrate (an eil::atan / eil::atan2
    // that mirrors Phobos' atanImpl/atan2Impl) and in the single call site
    // src/geom/elements/vector3.cpp:313 -- both outside this slice, so the rows are
    // left byte-compared rather than relaxed. Reported as T-9xx.
    return eil_test_gpath3::report("modifiedpath", argv[1],
        "the differing rows are all RotatedAboutZAxisPath rows; cause is std::atan2 "
        "vs Phobos' atan2Impl(=atan(y/x) with Phobos' polynomial atan), 1 ULP in the "
        "angle amplified by cancellation in rotate_about_zaxis "
        "(src/geom/elements/vector3.cpp:313; needs eil::atan2 in "
        "substrate/include/eil/math.hpp -- T-9xx).");
}
