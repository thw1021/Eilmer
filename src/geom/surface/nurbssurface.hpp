#pragma once
// geom/surface/nurbssurface.d -> src/geom/surface/nurbssurface.hpp
// (module geom.surface.nurbssurface).
//
// A ParametricSurface that is a NURBS surface: Piegl & Tiller's A4.3 de Boor evaluation
// (D:119-135) over a weighted control net, two knot vectors and two degrees, using the
// shared kernels from geom/misc/nurbs_utils.{hpp,cpp} (findSpan, basisFuns,
// NURBSWorkspace) rather than a second copy of them.
//
// Namespace: eil::geom, flat. NURBSSurface collides with nothing in geom (grep over
// gdtk src/geom); the free functions it needs (findSpan/basisFuns/NURBSWorkspace) come
// from geom/misc/nurbs_utils.hpp, which is flat too.
//
// Non-1:1 notes (all registered in provenance/fidelity.yml):
//   * adapted-handles: dup() (D:87-90) returns std::shared_ptr<ParametricSurface>
//     (wave-02-brief 4b item 1). It rebuilds through the VALIDATING constructor, as D
//     does, so a dup of a valid surface re-derives a correct m_b.
//   * D:21 `const double[4][][] Pw` is a two-level D view. Slice<Slice<T>> is not
//     constructible from Array<Array<T>> in the substrate (same gap nurbs_utils.hpp:28-32
//     records for Slice<const T>, T-907), so the parameter is a const reference to the
//     owning eil::Array<eil::Array<std::array<double,4>>>; the body still dups it into
//     mPw exactly as D:61 does. U and V are single-level views and stay eil::Slice<double>,
//     matching gpath/nurbs.hpp:66.
//   * D:111-117 declare mSw, mS, mNu, mNv, temp and the two workspaces as *static*
//     class fields: one set per process, shared by every NURBSSurface, and every
//     constructor resizes them to its own degree (D:66-70, D:81-85). The port keeps them
//     static, interference and all — measured rows ns.ord.* of
//     baselines/e587b235/probes/geom_surface_nurbssurface.txt show that evaluating a
//     quadratic surface after a surface with a smaller q is an immediate
//     "index [2] is out of bounds for array of length 2" (ArrayIndexError in D), and that
//     re-constructing a compatible one fixes it again. mNws_u/mNws_v are D `static
//     NURBSWorkspace`, which has only an explicit NURBSWorkspace(int p) constructor, so
//     they are held as std::unique_ptr and reassigned by both constructors, as
//     src/geom/gpath/nurbs.cpp:38 does.
//   * D:80 of the copy constructor writes `this.m_b = other.m_p;`, not other.m_b. That is
//     the reference behaviour and is copied, not fixed: m_b is the control-point count in
//     v minus one and feeds findSpan's search range, so a copy of a surface whose p and q
//     differ searches with the wrong n. Measured on the D binary: the copy of the cubic x
//     linear test surface raises "index [4] is out of bounds for array of length 4" at
//     BOTH v = 0.0 and v = 1.0 (rows ns.cub.copy.*) where the original evaluates fine;
//     and for a 4x4 quadratic net the wrong n makes findSpan's binary search fail to
//     converge — cp(0.0, 1.0) did not return within 6 s (timeout exit 124) while
//     cp(0.0, 0.5) returned immediately. The port reproduces the wrong m_b and therefore
//     both consequences; the probe simply cannot carry a row for a call that never
//     returns, so the v = 1.0 case of the quadratic copy is documented here instead of
//     dumped.
//   * the five constructor checks are `throw new Error(text(errMsg))` (D:28, :35, :42,
//     :50, :58) -> eil::ErrorBase, and their message texts contain the D copy-paste
//     wording ("direction u" inside the q check at D:40-41 and inside the V-length check at
//     D:55) — kept verbatim. text() of a single string is that string (nurbs.cpp:46-48
//     records the same), so eil::text(errMsg) is applied unchanged.
//   * D:31, :38, :46, :54 compare an int against a size_t expression (`p > Pw.length-1`,
//     `U.length != m_a+p+2`): D converts the int side to size_t, so a negative degree
//     wraps to a huge value. The casts are written out (measured: negP/negQ rows fall into
//     the *degree* guard, so -1 does NOT pass it — D:31 compares `static_cast<size_t>(-1) >
//     3`, which is true, hence "Supplied degree in u direction: -1").
//   * D:128 `temp[l][] += mNu[k]*mPw[uspan-m_p+k][vspan-m_q+l][]` is an element-wise array
//     op over 4 lanes in index order 0,1,2,3; the port writes the four lanes in the same
//     order (nothing reassociates).
//   * mSw/mS are static double[4]/double[3] whose elements start at double.init (NaN);
//     deBoor zeroes both before reading (D:130-131), in the order D does — AFTER the temp
//     accumulation loop, not before.

#include <array>
#include <cstddef>
#include <memory>
#include <string>

#include "eil/array.hpp"
#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/misc/nurbs_utils.hpp"
#include "geom/surface/parametricsurface.hpp"

namespace eil {
namespace geom {

class NURBSSurface : public ParametricSurface {
  public:
    // D's `const double[4][][]` — a two-level net of weighted control points.
    using Pw2 = eil::Array<eil::Array<std::array<double, 4>>>;

    // D:21-71
    NURBSSurface(const Pw2& Pw, eil::Slice<double> U, int p, eil::Slice<double> V, int q);
    // D:72-86, `this(ref const NURBSSurface other)`. Validates nothing, as in D.
    NURBSSurface(const NURBSSurface& other);

    std::shared_ptr<ParametricSurface> dup() const override; // D:87-90
    Vector3 operator()(double u, double v) const override;   // D:91-94 opCall
    std::string toString() const override;                   // D:95-98

  private:
    Pw2 mPw;                  // D:101 weighted control points
    eil::Array<double> mU;    // D:103 knot vector in direction u
    int m_p = eil::d_init<int>(); // D:104
    int m_a = eil::d_init<int>(); // D:105 a+1 = number of control points in u
    eil::Array<double> mV;    // D:107 knot vector in direction v
    int m_q = eil::d_init<int>(); // D:108
    int m_b = eil::d_init<int>(); // D:109 b+1 = number of control points in v

    // D:111-117. Static in D, so one set for the whole process.
    static std::array<double, 4> mSw;
    static std::array<double, 3> mS;
    static eil::Array<double> mNu;
    static eil::Array<double> mNv;
    static eil::Array<std::array<double, 4>> temp;
    static std::unique_ptr<NURBSWorkspace> mNws_u;
    static std::unique_ptr<NURBSWorkspace> mNws_v;

    // D:119-135, algorithm A4.3 from Piegl and Tiller (1997) - 'The NURBS Book'. Const
    // in D while it writes the static buffers above, which C++ also permits.
    Vector3 deBoor(double u, double v) const;
};

} // namespace geom
} // namespace eil
