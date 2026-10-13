// tests/test_geom_gpath_path.cpp
//
// Verification of geom/gpath/path.d (177 D lines) -> src/geom/gpath/path.{hpp,cpp}.
//
//  A. unittest replay: path.d has NO `unittest` block in D (measured: the file ends
//     at the closing brace of class Path, path.d:177). So there is nothing to port
//     assertion-for-assertion here, and section A asserts only the invariants that
//     path.d's own consumers rely on and that D states in code rather than in a test
//     — the three-branch finite-difference stencil at each boundary of t, the
//     tb<ta exchange in partial_length, and the `cast(Line)` special case in
//     intersect2D. They are eil-level restatements of the code under test, so they
//     are listed as live checks and counted in the "n/m unittest assertions hold"
//     total.
//
//  B. probe replay: line-for-line against
//     baselines/e587b235/probes/geom_gpath_path.txt (224 lines captured from
//     probes/geom/gpath/probe_path.d). Path is abstract, so everything is measured
//     through a base-class handle: a Stub that overrides only the four abstract
//     members (dup, opCall, toString, classString) so every other call lands in
//     path.d's own code, plus a Line to show which of those virtuals Line replaces.
//     The t sweep includes 0.0 and 1.0, where dpdt/d2pdt2 switch between the
//     one-sided and central differences (path.d:40-53, :62-77).
//
// Run: ./test_geom_gpath_path ../baselines/e587b235/probes/geom_gpath_path.txt

#include <cmath>
#include <memory>
#include <string>

#include "eil/eil.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/line.hpp"
#include "geom/gpath/path.hpp"
#include "nm/number.hpp"

#include "gpath1_dump.hpp"

using eil::geom::Line;
using eil::geom::Path;
using eil::geom::PathPtr;
using eil::geom::Vector3;
using eil::number;

using namespace eil_test_gpath1;

namespace {

// The mirror of the Stub class in probes/geom/gpath/probe_path.d: curved in all
// three components so the one-sided and central differences differ, with `t ^^ 2`
// as eil::pow (never a product) and sin() computed in extended precision and
// rounded only on store, as D's `real zr = sin(PI*t); to!number(zr)` does.
class Stub : public Path {
  public:
    std::shared_ptr<Path> dup() const override { return std::make_shared<Stub>(); }
    Vector3 operator()(double t) const override {
        const double y = eil::pow(t, 2);
        const eil::real zr = std::sin(EIL_PI * static_cast<eil::real>(t));
        return Vector3(t, y, static_cast<number>(zr));
    }
    std::string toString() const override { return "Stub()"; }
    std::string classString() const override { return "Stub"; }
};

// ============================================================================
// A. the live checks (D has no unittest block in this module)
// ============================================================================
void port_d_unittest() {
    Stub stub;

    // path.d:40-53 — the branch tests are `t+dt > 1.0` and `t-dt < 0.0` with
    // dt = 0.001. t = 0.001 takes the central branch (0.001-0.001 is exactly 0, so
    // `t-dt < 0.0` is false) while t = 0.0005 takes the forward one-sided one.
    const Vector3 d0 = stub.dpdt(0.0);
    const Vector3 d_edge = stub.dpdt(0.001);
    const Vector3 d_half = stub.dpdt(0.0005);
    CHECK1(std::fabs(d0.y - 0.001) < 1.0e-12, "dpdt(0.0) is the forward difference: (f(dt)-f(0))/dt");
    CHECK1(std::fabs(d_half.y - 0.002) < 1.0e-12, "dpdt(0.0005) still one-sided: (f(0.0015)-f(0.0005))/dt");
    CHECK1(std::fabs(d_edge.y - 0.002) < 1.0e-12, "dpdt(0.001) is central: (f(0.002)-f(0))/0.002");
    // At t = 0.999, 0.999+0.001 rounds to exactly 1.0, so the test is `>` not `>=`
    // and the central difference runs; at 0.9995 the backward one-sided runs.
    const Vector3 d999 = stub.dpdt(0.999);
    const Vector3 d9995 = stub.dpdt(0.9995);
    CHECK1(std::fabs(d999.y - 1.998) < 1.0e-12, "dpdt(0.999) is central: (f(1)-f(0.998))/0.002");
    CHECK1(std::fabs(d9995.y - 1.998) < 1.0e-12, "dpdt(0.9995) is the backward difference");

    // path.d:56-79 — same three branches for the second derivative.
    CHECK1(std::fabs(stub.d2pdt2(0.0).y - 2.0) < 1.0e-6, "d2pdt2(0.0) one-sided");
    CHECK1(std::fabs(stub.d2pdt2(0.5).y - 2.0) < 1.0e-6, "d2pdt2(0.5) central");
    CHECK1(std::fabs(stub.d2pdt2(1.0).y - 2.0) < 1.0e-6, "d2pdt2(1.0) one-sided");

    // path.d:82-84 — partial_length swaps its arguments when tb < ta, so the two
    // orders return the same number.
    CHECK1(stub.partial_length(0.2, 0.7) == stub.partial_length(0.7, 0.2),
           "partial_length(tb<ta) swaps (path.d:82-84)");
    // path.d:98-101 — length() is partial_length(0.0, 1.0), i.e. the 100-chord sum.
    CHECK1(stub.length() == stub.partial_length(0.0, 1.0), "length() is partial_length(0,1)");

    // path.d:136 — `if (cast(Line)this !is null) nseg = 1;`: the run-time type test,
    // kept as dynamic_cast (wave-02-brief 4b item 4). A Line must take it and a
    // non-Line must not.
    const Line seg = Line(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
    CHECK1(dynamic_cast<const Line*>(&stub) == nullptr, "a Stub is not a Line");
    CHECK1(dynamic_cast<const Line*>(&seg) != nullptr, "a Line is a Line");

    // path.d:102-121 — point_from_length past the end returns the last sample with
    // t = dt*n = 1.0.
    double tt = 7.5;
    const Vector3 pe = seg.point_from_length(1.0e3, tt);
    CHECK1(tt == 1000.0, "Line::point_from_length extrapolates (line.d:56-61)");
    CHECK1(pe.x == 1000.0, "Line::point_from_length lands on opCall(t)");
    double tp = 7.5;
    const Vector3 pb = stub.point_from_length(-1.0, tp);
    CHECK1(tp == 0.001, "Path::point_from_length exits on the first chord past the target");
    (void)pb;

    // path.d:124 — D's `out double t` resets the caller's variable to double.init
    // (NaN) before the body runs, which is observable on the one path that returns
    // false without assigning t (a ray parallel to every segment). Measured with
    // ldc2: an empty `void set(out double x){}` leaves a caller holding 3.25 as nan.
    double tre = 0.5;
    const bool fpar = seg.intersect2D(Vector3(0.5, 0.5, 0.0), Vector3(1.0, 0.0, 0.0), tre);
    CHECK1(!fpar, "intersect2D misses for a parallel ray");
    CHECK1(std::isnan(tre), "intersect2D resets its out parameter to NaN on entry (path.d:124)");
}

// ============================================================================
// B. the probe replay, in probe order
// ============================================================================
void replay_probe() {
    Stub stub;
    dump_path("path.stub", stub);

    dump_path("path.line", Line(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0)));

    const PathPtr s = std::make_shared<Stub>();
    const PathPtr sd = s->dup();
    emitf("path.dup.class=%s", sd->classString().c_str());
    dump_path("path.dup", *sd);

    const Line ab = Line(Vector3(0.0, 0.0, 0.0), Vector3(1.0, 0.0, 0.0));
    double t0 = eil::d_init<double>(), t1 = eil::d_init<double>(), tm = eil::d_init<double>();
    const double d0 = ab.closestDistance(Vector3(-1.0, 0.5, 0.0), t0);
    emitf("path.cd.before t=%.17g dist=%.17g", t0, d0);
    const double d1 = ab.closestDistance(Vector3(2.0, 0.5, 0.0), t1);
    emitf("path.cd.after  t=%.17g dist=%.17g", t1, d1); // two spaces: as printed in D
    const double dm = ab.closestDistance(Vector3(0.5, 0.5, 0.0), tm);
    emitf("path.cd.middle t=%.17g dist=%.17g", tm, dm);

    double tp = eil::d_init<double>();
    const Vector3 pe = ab.point_from_length(1.0e3, tp);
    emitf("path.pfl.over t=%.17g x=%.17g y=%.17g z=%.17g", tp, pe.x, pe.y, pe.z);
    double tz = eil::d_init<double>();
    const Vector3 pz = ab.point_from_length(0.0, tz);
    emitf("path.pfl.zero t=%.17g x=%.17g y=%.17g z=%.17g", tz, pz.x, pz.y, pz.z);

    // D's `out double t` reset, measured by a variable that still holds a valid t.
    double tre = tm;
    const bool fpar = ab.intersect2D(Vector3(0.5, 0.5, 0.0), Vector3(1.0, 0.0, 0.0), tre);
    emitf("path.i2d.parallel found=%s t=%.17g", fpar ? "true" : "false", tre);
    const bool fnom = ab.intersect2D(Vector3(0.5, -1.0, 0.0), Vector3(0.0, 1.0, 0.0), tre);
    emitf("path.i2d.afterpar found=%s t=%.17g", fnom ? "true" : "false", tre);
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <probes/geom_gpath_path.txt>\n";
        return 2;
    }
    port_d_unittest();
    replay_probe();
    return report("path", argv[1], argc > 2);
}
