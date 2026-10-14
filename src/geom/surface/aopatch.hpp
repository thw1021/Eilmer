#pragma once
// geom/surface/aopatch.d -> src/geom/surface/aopatch.hpp
// (module geom.surface.aopatch)
//
// A surface whose interior is interpolated through an area-orthogonal (Knupp,
// "A Robust Elliptic Grid Generator", JCP 100 (1992) 409-418) background mesh fitted
// over a Coons patch, with the Coons patch itself as the fall-back evaluator within
// 1.0e-4 of any parametric boundary (D:17-228).
//
// Non-1:1 notes (provenance/fidelity.yml is read-only for this stage, so they are
// recorded here and in the stage report):
//   * adapted-handles: D:27-28 hold the four boundaries as `Path` class handles, so the
//     port stores PathPtr and dups on construction (D:49-52) and on copy (D:92-95).
//     Measured by ao.bez.aftermut / ao.bez.callerstr: mutating the caller's Bezier after
//     construction leaves the patch's south at z=0.1 while the caller's reads z=5.
//   * D:36-44 the four-corner constructor builds four Lines and DELEGATES to the
//     four-Path constructor (D:43), so it inherits the corner checks; C++ delegating
//     construction is used, matching coonspatch.cpp:22-25.
//   * D:64-79 four `throw new Error(text(...))` corner-consistency guards in the order
//     p00, p10, p11, p01, so only the FIRST open corner can report — measured with
//     ao.open.all, which has all four open and still prints the p00 message. Error and
//     Exception are siblings in this port (wave-02-brief 1.1), so a ported
//     `catch (ExceptionBase)` does not catch it. The message is built with text(...) over
//     Vector3 values, i.e. the %.6g form ("Vector3(0, 0.1, 3)").
//   * D:90-102 the copy constructor assigns north/east/south/west, p00..p11 and
//     tfi_surface, but NOT _nx/_ny: they stay int.init == 0. That is a genuine `.init`
//     state left behind by a constructor and it is observable three ways (rows
//     ao.copy.*): toString() prints "nx=0, ny=0"; compute_background_mesh() then runs with
//     dXi = 1.0/0 == +inf and the single node becomes tfi_surface(inf*0, inf*0) =
//     tfi_surface(nan, nan); and an interior opCall indexes _bgmesh[0][1], one past a
//     length-1 array, which in D raises core.exception.ArrayIndexError with the text
//     "index [1] is out of bounds for array of length 1". The port's eil::Array raises
//     eil::RangeError, which carries that same text verbatim (substrate error.hpp:71-87)
//     and derives from ErrorBase, so the replayed row is byte-identical; the port test
//     maps RangeError back to D's class name for the type= column and says so.
//     Kept, not fixed: the constructor is D's and dup() (D:104-108) is the path that
//     preserves _nx/_ny.
//   * D:104-108 dup() re-enters the FULL checking constructor with this._nx/_ny, so dup
//     and copy are different objects — that is the pair ao.dup / ao.copy.
//   * D:100 `tfi_surface = other.tfi_surface.dup();` is D's *covariant* dup(), statically
//     a CoonsPatch. The port's dup() returns shared_ptr<ParametricSurface> (wave-02-brief
//     4b forbids a covariant shared_ptr), so dynamic_pointer_cast<CoonsPatch> recovers the
//     derived type — the same run-time type test D performs at compile time.
//   * D:124-125 `to!int(r / dr)` truncates toward zero; static_cast<int> is used, NOT
//     eil::to<int>, because eil::to<int> routes through to_string's %.6g formatter and
//     would re-round (0.9999999 -> "1" -> 1). D's own r/dr values are exactly representable
//     quotients here, and the node sweep ao.ut.node.* (121 rows, r = ix/10 built the same
//     way D:158 builds the mesh) measures which cell each r lands in and what local_r
//     becomes, so a mis-truncation is caught row by row.
//   * D:181-213 read and write `_bgmesh[ix][iy].x .re` and `.y .re`. `.re` on a double is
//     an identity in D (measured: ldc2 compiles `double.re` and returns the same value),
//     so eil::re() is the port's spelling. `_bgmesh[ix][iy].x = numer/denom` writes the
//     number field directly.
//   * D:212-213 `fabs(...).re`: fabs only clears the sign bit, so Phobos and glibc cannot
//     differ in bits for any finite or infinite argument and no wrapper is needed
//     (wave-02-brief T-912 asks for a check per std.math call; this is the check).
//     NaN moves compare false against largest_x_move/largest_y_move exactly as in D.
//   * D:223-225 `writeln("Warning: AO iteration did not converge.")` writes to STDOUT
//     during construction, so it is part of the captured program text and its position in
//     the row order is measured (baselines rows 700 and 727).
//   * D:151-152 `_bgmesh.length = _nx + 1;` then `_bgmesh[ix].length = _ny + 1;` gives
//     _nx+1 EMPTY inner arrays grown by assignment, mirrored by
//     Array<Array<Vector3>>::set_length, which value-initialises the grown tail.
//   * D:196-199, 203-210 write `dXi2 * dEta2` and `2.0 * (g22*dEta2 + g11*dXi2)`: the
//     multiplication grouping is D's and is preserved token for token, since
//     -ffp-contract=off keeps the products separate but the association still matters.
//   * No classString() exists in D's surface hierarchy (0 matches over
//     gdtk/src/geom/surface), so classString() is declared but not marked override; the
//     ao.<tag> classname= rows compare it with the last dotted component of D's
//     typeid(...).name.
//   * D:13 imports std.math and D:7 std.stdio but the module uses only fabs from the
//     former and writeln from the latter; geom.gpath supplies Path/Line (D:13-14), and
//     CoonsPatch the TFI fall-back.

#include <memory>
#include <string>

#include "eil/array.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"
#include "geom/surface/coonspatch.hpp"
#include "geom/surface/parametricsurface.hpp"

namespace eil {
namespace geom {

class AOPatch : public ParametricSurface {
    // Another surface defined as a blend of 4 bounding paths.
    //
    // The topology of the parametric surface is the same as that of the CoonsPatch.
    // The difference, however, is in the evaluation of points on the surface.
    // Here, points are interpolated within a background mesh that has been fitted
    // with the Area-Orthogonality (AO) elliptic grid generator
    // described by Patrick M. Knupp "A Robust Elliptic Grid Generator"
    // J. Computational Physics Vol.100 pp409-418 (1992)
  public:
    PathPtr north, east, south, west; // bounding paths (D:27)
    Vector3 p00, p10, p11, p01;      // corners (D:28)

    // D:36-44 — builds the four bounding Lines and delegates (see header note).
    AOPatch(const Vector3& p00, const Vector3& p10, const Vector3& p11, const Vector3& p01,
            int nx = 10, int ny = 10);

    // D:46-88 — the checking constructor. Parameter order is D's, not NESW.
    AOPatch(const PathPtr& south, const PathPtr& north, const PathPtr& west,
            const PathPtr& east, int nx = 10, int ny = 10);

    // D:90-102 (`this(ref const(AOPatch) other)`) — leaves _nx/_ny at int.init == 0.
    AOPatch(const AOPatch& other);

    std::shared_ptr<ParametricSurface> dup() const override;
    Vector3 operator()(double r, double s) const override; // D:110-135 opCall
    std::string toString() const override;
    std::string classString() const; // see header note: not a D symbol here

  private:
    std::shared_ptr<CoonsPatch> tfi_surface; // D:31
    // D:32 `int _nx, _ny;` — class fields default to int.init, which is 0.
    int _nx = 0, _ny = 0;
    eil::Array<eil::Array<Vector3>> _bgmesh; // D:33 Vector3[][]
    void compute_background_mesh();          // D:148-227
}; // end class AOPatch

} // namespace geom
} // namespace eil
