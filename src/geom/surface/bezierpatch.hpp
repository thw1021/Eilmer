#pragma once
// geom/surface/bezierpatch.d -> src/geom/surface/bezierpatch.hpp
// (module geom.surface.bezierpatch)
//
// Tensor-product Bezier surface: n+1 Bezier curves along s whose values at a given s
// are the control net of one Bezier curve along r (D:53-63).
//
// Non-1:1 notes (registered in provenance/fidelity.yml):
//   * adapted-handles: D:48 `override BezierPatch dup() const` returns the concrete
//     class handle; the virtual contract of this port (wave-02-surfvol-brief 1) is
//     std::shared_ptr<ParametricSurface>, which is what the base class declares.
//   * D:18-80 declares NO classString() — measured: `grep -rn classString
//     gdtk/src/geom/surface/` returns 0 hits, and the abstract base
//     (gdtk/src/geom/surface/parametricsurface.d:34-68) has none either, so this
//     classString() is NOT marked override and has no D-side caller. The corpus row
//     `bzp.<tag> classname=` compares it against the last dotted component of D's
//     `typeid(bp).name`, which is the only class-name text D produces.
//   * D:26 `this(const Vector3[][] _Q, int _n, int _m)` takes a D dynamic array of
//     dynamic arrays; the port takes a const reference to
//     eil::Array<eil::Array<Vector3>>. D's body dups each row (D:32) so the patch
//     never aliases the caller's net, and the port does the same (`bzp.aliaschk`
//     corpus row measures that mutating Q afterwards leaves the patch unchanged).
//   * D:30 `Q.length = _n + 1;` assigns a SIGNED length. Measured: for _n = -2 D
//     raises `Error` with message "Memory allocation failed" (row bzp.nm2.ctor),
//     while _n = -1 yields a zero-length Q and constructs fine (row bzp.nm1.ctor).
//     A size_t conversion would silently wrap instead, so the sign test is explicit
//     (d_set_length_checked, defined in bezierpatch.cpp).
//   * D:37-46 the copy constructor assigns `m = other.n;`, NOT other.m. That is a
//     real difference in behaviour, measured, and deliberately preserved: for the
//     n=3/m=4 net of the D unittest the copy reports m=3 while dup() reports m=4
//     (rows bzp.copy vs bzp.dup). opCall does not read m, so the surfaces still
//     agree; only the member value distinguishes them. Do not "fix" it here
//     (wave-02-brief 0.1).
//   * D:79 `private Bezier[] _b1s;` stays private; the port holds
//     eil::Array<std::shared_ptr<Bezier>> so the exact-derivative Bezier behaviour
//     (dpdt/d2pdt2 of the already-ported geom/gpath/bezier.hpp) is what runs.
//   * D:82 `writeCtrlPtsAsVtkXml(BezierPatch bPatch, string fileName)` is snake_cased
//     to write_ctrl_pts_as_vtk_xml, matching the base-class writer's ported name
//     (src/geom/surface/parametricsurface.hpp:72). It takes the object by const
//     reference rather than by class handle, because it only reads Q.
//   * D:85 `auto m = bPatch.Q[0].length;` uses Q[0]'s inner length as the j bound for
//     EVERY row, so a jagged net reads out of bounds: measured `Error`
//     "index [2] is out of bounds for array of length 2" (row bzp.vtk.jagged). Kept
//     as written.
//   * D:87 `File(fileName, "w")` raises std.exception.ErrnoException on an unusable
//     path; the substrate has no ErrnoException, so the writer throws ExceptionBase
//     with the measured Phobos text (row bzp.vtk.badpath), exactly as
//     parametricsurface.cpp:52-60 does.

#include <memory>
#include <string>

#include "eil/array.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/bezier.hpp"
#include "geom/surface/parametricsurface.hpp"

namespace eil {
namespace geom {

class BezierPatch : public ParametricSurface {
  public:
    // Control net is defined by points Q[i][j] with
    // 0 <= i <= n, 0 <= j <= m
    eil::Array<eil::Array<Vector3>> Q;
    int n; // order of polynomial in r-parameter direction
    int m; // order of polynomial in s-parameter direction

    // D:26-35
    BezierPatch(const eil::Array<eil::Array<Vector3>>& _Q, int _n, int _m);
    // D:37-46 (`this(ref const(BezierPatch) other)`), including `m = other.n`
    BezierPatch(const BezierPatch& other);

    std::shared_ptr<ParametricSurface> dup() const override;
    Vector3 operator()(double r, double s) const override; // D:53-63 opCall
    std::string toString() const override;
    std::string classString() const; // see header note: not a D symbol here

    // D:70-76
    void initialiseWorkingSpace();

  private:
    eil::Array<std::shared_ptr<Bezier>> _b1s; // D:79
}; // end class BezierPatch

// D:82-105
void write_ctrl_pts_as_vtk_xml(const BezierPatch& bPatch, const std::string& fileName);

} // namespace geom
} // namespace eil
