#pragma once
// geom/gpath/gpath_utils.d -> src/geom/gpath/gpath_utils.hpp (module geom.gpath.gpath_utils).
//
// Free helpers over the gpath curves: sample a Path at clustered parameter values
// (D:19-28), read an x y z point file (D:30-43), and fit a Bezier to a point list by
// minimising the sum of Path.closestDistance with nm/nelmin (D:45-174 and D:176-315).
//
// D's import list (gpath_utils.d:9-16) is std.conv, std.stdio, std.string,
// std.range:iota, geom, nm.nelmin, ntypes.complex and nm.number. Two notes:
//   * `import geom;` (D:13) is the package aggregator; per wave-02-brief section 4
//     item 5 it is replaced by the modules actually used: geom.gpath.path (Path),
//     geom.gpath.line (Line), geom.gpath.bezier (Bezier) and
//     geom.misc.univariatefunctions (UnivariateFunction), plus geom/elements/vector3.
//   * `import std.range : iota` (D:12) is UNUSED in this module (no iota call appears
//     in its 315 lines), so there is no eil::iota dependency.
//
// Non-1:1 notes (all registered in provenance/fidelity.yml):
//   * adapted-handles: D returns a Bezier *handle* and takes Path/UnivariateFunction
//     handles, so the port uses std::shared_ptr throughout (brief section 3's class
//     row and 4b item 1). `Bezier initGuess` is a nullable handle: D:73 tests it with
//     `if (initGuess)` and the demo passes a default-initialised (null) Bezier, so the
//     port tests the shared_ptr against nullptr, and a null guess takes D's
//     auto-distribution branch.
//   * D's `number funMin(number[] d)` is a NESTED function closing over myBez,
//     nCtrlPts, dim, points and the enclosing function's `t` (D:120-140, :250-275). Per
//     brief 4b item 5 it becomes a capturing functor handed to
//     eil::nm::nelmin::minimize; nothing else about the call changes.
//   * nm.nelmin lives in the namespace eil::nm::nelmin (it is there because D keeps
//     nelmin's minimize apart from nm/linesearch.d's by module), so the call reads
//     nelmin::minimize<FunMin, number>(...) where D writes minimize!(funMin, number).
//   * FitCounters below is NOT a D symbol. gpath_utils.d:146-153 keeps f_min, n_fe,
//     n_restart and the perturbation dp in local variables and discards them, so the
//     number of simplex steps a fit took is not observable through D's entry point at
//     all. The optional `counters` out-parameter publishes exactly those four plus the
//     design vector d that minimize returns. It is write-only from the port's point of
//     view: pass nullptr (the default, and what every ported caller does) and the
//     arithmetic, the call sequence and the return value are D's. The probe compares
//     the counts against a line-for-line D copy of the same function
//     (probes/geom/gpath/probe_gpath_utils.d), which is the only way to get them on the
//     D side; rows "cmp.*" of the baseline show the copy and the real function agree.
//   * The two functions' error paths are `throw new Exception` in D (D:65, :79, :187,
//     :201) and stay eil::ExceptionBase. Their messages use format("%d", ...) on a
//     size_t and on an int; eil::format renders both as integers, matching D.
//   * D:227-244 sizes the design vector of optimiseBezierPoints2 to
//     2*(nCtrlPts-2)-2 but fills it from i=2 with stride 2, leaving d[0] and d[1] at 0
//     and then OVERWRITING the last two entries with the end-control-point x values.
//     The same shape appears in the cost function, which reads d[$-2] and d[$-1] for
//     the slope constraints. It looks like a bug, and it is preserved exactly: the two
//     unused entries make the simplex two dimensions wider than the parameters the cost
//     function reads, which changes how many evaluations nelmin performs.
//   * Both cost functions share the enclosing function's single `double t` (D:58, :180)
//     and hand it to closestDistance, whose out-parameter form overwrites it on every
//     call (path.d:151). D's local starts at double.init (NaN) and the port gives it
//     eil::d_init, which is what the first call inside nelmin sees.

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "eil/array.hpp"
#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/bezier.hpp"
#include "geom/gpath/path.hpp"
#include "geom/misc/univariatefunctions.hpp"

namespace eil {
namespace geom {

// D class handles (brief section 3): Path/UnivariateFunction parameters and Bezier
// return values pass by shared_ptr, and `const(UnivariateFunction)` becomes a
// shared_ptr to const.
using BezierPtr = std::shared_ptr<Bezier>;
using ClusterPtr = std::shared_ptr<const UnivariateFunction>;

// The minimiser bookkeeping that D keeps in locals (see the FitCounters note above).
// Passing nullptr leaves the ported behaviour identical to D's.
struct FitCounters {
    number dp = eil::d_init<number>(); // D:151 myBez.length()/100.0
    number f_min = eil::d_init<number>();
    int n_fe = eil::d_init<int>();
    int n_restart = eil::d_init<int>();
    eil::Array<number> d;              // D:153 the design vector minimize returns
};

// D:19-28. Points of `pth` at the niv parameter values clusterf distributes.
eil::Array<Vector3> discretize_path(const PathPtr& pth, size_t niv,
                                    const ClusterPtr& clusterf);

// D:30-43. Append the x(,y(,z)) triples of a text file to `points`. Blank lines and
// lines whose first token is exactly "#" are skipped; a token that is not a number
// raises the same ConversionException std.conv does.
void readPointsFromFile(const std::string& fileName, eil::Array<Vector3>& points);

// D:45-53. The file form: read the points, then call the point form with its defaults.
BezierPtr optimiseBezierPoints(const std::string& fileName, int nCtrlPts,
                               const BezierPtr& initGuess, eil::Array<double>& ts,
                               bool& success, int dim = 2, FitCounters* counters = nullptr);

// D:55-174. The point form.
BezierPtr optimiseBezierPoints(const eil::Slice<Vector3>& points, int nCtrlPts,
                               const BezierPtr& initGuess, eil::Array<double>& ts,
                               bool& success, double tol = 1.0e-06, int max_steps = 10000,
                               int dim = 2, FitCounters* counters = nullptr);

// D:176-315. The same fit with the first and last interior control points pinned to
// the supplied end slopes.
BezierPtr optimiseBezierPoints2(const eil::Slice<Vector3>& points, int nCtrlPts,
                                const BezierPtr& initGuess, const Vector3& startSlope,
                                const Vector3& endSlope, eil::Array<double>& ts,
                                bool& success, double tol = 1.0e-06, int max_steps = 10000,
                                int dim = 2, FitCounters* counters = nullptr);

} // namespace geom
} // namespace eil
