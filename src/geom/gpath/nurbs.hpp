#pragma once
// geom/gpath/nurbs.d -> src/geom/gpath/nurbs.hpp (module geom.gpath.nurbs).
//
// A Path that is a NURBS curve: Piegl & Tiller's A4.1 de Boor evaluation (D:87-97)
// over the weighted control-point net Pw, the knot vector U and the degree p, using
// the shared kernels from geom/misc/nurbs_utils.{hpp,cpp} (findSpan, basisFuns,
// NURBSWorkspace) rather than a second copy of them here.
//
// Non-1:1 notes (all registered in provenance/fidelity.yml):
//   * adapted-handles: D:60 `override NURBS dup() const` returns a class handle, so
//     the port returns std::shared_ptr<Path> (wave-02-brief section 4b item 1). dup()
//     rebuilds through the validating constructor, exactly as D:62 does, so a dup of a
//     valid curve cannot fail and the copy constructor (D:51-59) — which does NOT
//     validate — remains the only unchecked way to make one.
//   * D:82-85 declare mN, mCw, mC and mNws as *static* class fields: one buffer per
//     process, shared by every NURBS object, which each constructor resizes to its own
//     degree (D:47-48, D:57-58). The port keeps them static, including the sharing, so
//     the cross-degree interference is preserved instead of accidentally fixed: a
//     degree-5 curve evaluated after a degree-2 one has been constructed reads mN[3],
//     which is out of bounds, and D's bounds check turns it into a RangeError
//     ("index [3] is out of bounds for array of length 3"; measured rows
//     nurbs.static.keep5 / nurbs.static.retry of
//     baselines/e587b235/probes/geom_gpath_nurbs.txt). Constructing the degree-5 curve
//     again restores it (nurbs.static.rebuild5).
//   * mNws is `static NURBSWorkspace` (D:85) and nurbs_utils.hpp's NURBSWorkspace has
//     only the explicit `NURBSWorkspace(int p)` constructor, so C++ cannot default
//     construct the static slot the way D's module init does. It is held as a
//     std::unique_ptr that every constructor reassigns, which is the same "replace the
//     shared workspace" behaviour; it is never null when deBoor can run, because every
//     path to an instance goes through one of the two constructors, both of which
//     assign it (D:48, D:58).
//   * mCw/mC are D static arrays whose elements start at double.init == NaN
//     (wave-02-brief section 5). deBoor zeroes both before reading them (D:92-93), so
//     the NaN is not observable; they are still initialised with eil::d_init so the
//     object's initial state is D's, not C++'s zero.
//   * The three constructor guards are `throw new Error` (D:26, :33, :41) and map to
//     eil::ErrorBase; the out-of-range parameter Exception that findSpan raises
//     (nurbs_utils.d:33) stays an eil::ExceptionBase. Sibling types, so a caller's
//     catch (ExceptionBase) does not swallow the constructor Errors, as in D.
//   * D:36 `to!int(Pw.length-1)` is a narrowing conversion of a size_t; the cast is
//     kept, and the `p > Pw.length-1` / `U.length != m_a+p+2` comparisons at D:29, :37
//     are the unsigned ones D performs (a negative int converts to a huge size_t, and
//     that is preserved rather than normalised).
//   * D's `const double[4][] Pw` / `const double[] U` parameters arrive as
//     eil::Slice, matching nurbs_utils.hpp's convention for D's const array
//     parameters (T-907: Slice<const T> is not constructible from Array<T>); the bodies
//     only read them and then dup into owned Arrays, as D:44-45 does.

#include <array>
#include <cstddef>
#include <memory>
#include <string>

#include "eil/array.hpp"
#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"
#include "geom/misc/nurbs_utils.hpp"

namespace eil {
namespace geom {

class NURBS : public Path {
  public:
    // D:20-50. Pw is the weighted control points, U the knot vector, p the degree.
    NURBS(eil::Slice<std::array<double, 4>> Pw, eil::Slice<double> U, int p);
    // D:51-59, `this(ref const NURBS other)`. No validation, as in D.
    NURBS(const NURBS& other);

    std::shared_ptr<Path> dup() const override;      // D:60-63
    Vector3 operator()(double u) const override;     // D:64-67 (opCall -> deBoor)
    std::string toString() const override;           // D:68-71
    std::string classString() const override;        // D:72-75

  private:
    eil::Array<std::array<double, 4>> mPw; // D:78 collection of weighted control points
    eil::Array<double> mU;                 // D:79 knot vector
    int m_p = eil::d_init<int>();          // D:80 degree of curve
    int m_a = eil::d_init<int>();          // D:81 a+1 = number of control points

    // D:82-85. Static in D, so shared by every NURBS in the process.
    static eil::Array<double> mN;
    static std::array<double, 4> mCw;
    static std::array<double, 3> mC;
    static std::unique_ptr<NURBSWorkspace> mNws;

    // D:87-97, algorithm A4.1 from Piegl and Tiller (1997) - 'The NURBS Book'. A const
    // method in D that writes the static buffers above; in C++ writing a static member
    // from a const method is likewise allowed, which is what keeps the two the same.
    Vector3 deBoor(double u) const;
}; // end class NURBS

} // namespace geom
} // namespace eil
