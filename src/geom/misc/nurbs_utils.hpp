#pragma once
// geom/misc/nurbs_utils.d -> src/geom/misc/nurbs_utils.hpp
// (module geom.misc.nurbs_utils).
//
// The NURBS evaluation kernels shared by geom/gpath/nurbs.d,
// geom/surface/nurbssurface.d and geom/volume/nurbsvolume.d: Piegl & Tiller's
// A2.1 findSpan and A2.2 basisFuns, the workspace they need, the clamped/uniform
// normalised knot-vector generator, the control-point deep copy and the
// at-least-two-points guard.
//
// Namespace: eil::geom, flat. Nothing here collides: findSpan, basisFuns,
// NURBSWorkspace, PwTest, duplicatePw, autoKnotVector are declared by no other
// geom module (grep over gdtk src/geom), so no eil::geom::<module> nesting was
// needed (recorded in reports/wave-02-brief.md section 2).
//
// Non-1:1 notes (all registered in provenance/fidelity.yml):
//   * convNURBSVolDataToVTK (D:122-128) is NOT ported. It is the only thing in
//     geom/misc that names geom.volume.nurbsvolume (it constructs a NURBSVolume),
//     which would make misc <-> volume a header cycle, and it has no callers
//     anywhere in the tree: grep over all of src/ (including geom/luawrap and the
//     examples) finds only its own definition. D's import of geom.volume.nurbsvolume
//     (D:13) is therefore also absent. Deferred edge for the volume stage: when
//     nurbsvolume.d lands, that function belongs next to its consumer, not here.
//   * nurbs_utils.d has no curveEval, contrary to what the wave-02 brief
//     anticipated: the curve/surface/volume evaluation lives in the three
//     consumers, which call findSpan + basisFuns themselves. Ported surface is
//     exactly the six functions above.
//   * D's `const double[] U` parameters are ported as eil::Slice<double> rather than
//     Slice<const double>, because Slice<const T> is not constructible from
//     Array<T>/Slice<T> in the substrate (the same gap properties.hpp:20-21 reports;
//     substrate gap, see reports/). The read-only promise D makes is honoured by the
//     bodies, which never write through U.
//   * D's `ref double[] N` in basisFuns is a *reference to the array*, which would
//     let the callee rebind or resize it. The body only assigns N[0..p], so it is
//     ported as a view (eil::Slice<double>); the element writes still reach the
//     caller's array, as D's do.
//   * NURBSWorkspace's `.length = p+1` (D:52-55) leaves the new elements at
//     double.init, i.e. quiet NaN, not 0 — measured: the probe row
//     "ws p=2 left=[nan, nan, nan]" (probes/geom/misc/probe_nurbs_utils.d, baseline
//     geom_misc_nurbs_utils.txt). eil::Array<T>::set_length value-initialises, so the
//     constructor fills with eil::d_init<double>() to match D. Index 0 of left/right
//     is never written by basisFuns and never read either (its loops touch [1..p]),
//     so the NaN is only visible in a dump like the probe's — but a dump is exactly
//     what the gate compares, and wave-02-brief section 5 says not to hide it.
//   * PwTest has no callers in the tree either (grep over src/); it is ported because
//     it is part of the module and its Error message text is asserted by the probe.
//   * D's `throw new Error(...)` at nurbs_utils.d:79 (PwTest) and :103 (autoKnotVector)
//     are Errors, not Exceptions, so they map to eil::ErrorBase; D's
//     `throw new Exception(errMsg)` at :33 (findSpan) maps to eil::ExceptionBase. The
//     two are siblings (wave-01 lesson 1), so a ported `catch (ExceptionBase)` does not
//     swallow the Errors, exactly as in D. The probe prints which branch fired.

#include <array>
#include <cstddef>
#include <string>

#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/number.hpp"

namespace eil {
namespace geom {

// D:17-47. Piegl & Tiller A2.1. Returns the span index; throws ExceptionBase with
// D's exact multi-line message when u is outside the knot range by more than tol.
int findSpan(double u, int n, int p, eil::Slice<double> U);

// D:49-56.
struct NURBSWorkspace {
    eil::Array<double> left;
    eil::Array<double> right;
    explicit NURBSWorkspace(int p);
};

// D:58-73. Piegl & Tiller A2.2. Fills N[0..p] with the non-vanishing basis functions.
void basisFuns(int i, double u, int p, eil::Slice<double> U, eil::Slice<double> N,
               NURBSWorkspace& nws);

// D:75-81. Guard: fewer than two control points is an Error in D.
void PwTest(eil::Slice<std::array<double, 4>> Pw);

// A ported `number[4][][][]` (D:83): three levels of dynamic array of a 4-element
// static array of number.
using PwArray = eil::Array<eil::Array<eil::Array<std::array<number, 4>>>>;

// D:83-96. Element-by-element deep copy of the weighted control-point net.
PwArray duplicatePw(const PwArray& Pw);

// D:98-120. Clamped, uniform, normalised knot vector for N control points of degree
// p. Throws ErrorBase when p is incompatible with N.
eil::Array<double> autoKnotVector(int N, int p);

// D:122-128 convNURBSVolDataToVTK is deliberately absent — see the header note.

} // namespace geom
} // namespace eil
