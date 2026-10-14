#pragma once
// geom/surface/controlpointpatch.d -> src/geom/surface/controlpointpatch.hpp
// (module geom.surface.controlpointpatch)
//
// Eiseman (1988)'s control-point form of algebraic grid generation (D:30-296): a grid of
// control points C[i][j] interpolated by the nested G/H/E/F/T scheme of Eiseman's eqs
// (4)-(9), with up to four OPTIONAL bounding Paths whose conformity is added selectively.
//
// Non-1:1 notes (provenance/fidelity.yml is read-only for this stage, so they are
// recorded here and in the stage report):
//   * adapted-handles: D:38 holds `Path mNorth, mEast, mSouth, mWest;` as class handles,
//     so the port stores PathPtr. D:46-49 dups each one ONLY when it is non-null
//     (`if (north !is null)`), in the order north, east, south, west; opCall (D:169-172)
//     tests each one again before adding its conformity term. Missing boundaries are
//     therefore a real configuration, not an error — measured by cpp.nonull.* (all four
//     absent, so only T() contributes) and cpp.no.west/east/south/north.* (one absent).
//   * D:41 `this(in Path south, in Path north, in Path west, in Path east, in Vector3[][] C)`
//     — D's `in` on a dynamic array is a view; the port takes
//     const eil::Array<eil::Array<Vector3>>& because D:52 `mC[i] = C[i].dup()` only reads
//     C. The dup is a genuine deep copy in both directions: mutating the caller's grid
//     after construction does not move the patch (rows cpp.aftermut.*) and setCtrlPt does
//     not move the caller's grid (rows cpp.afterset.*).
//   * D:43-44 `mN = to!int(C.length); mM = to!int(C[0].length);` — the two counts come from
//     DIFFERENT things, so a ragged grid is accepted with mM read off row 0 only. Kept
//     (rows cpp.ragged.*), and C[0] on an empty grid raises D's ArrayIndexError, message
//     "index [0] is out of bounds for array of length 0" (rows cpp.g0, cpp.empty.grid).
//   * D:55-132 the guide-patch constructor DELEGATES to the 5-arg one in the MIDDLE of its
//     body (D:63), after building an empty C but before sampling gpatch. C++ cannot
//     delegate mid-body, so the delegated work is the shared private init_from_grid() that
//     the 5-arg constructor's body calls; the 7-arg constructor builds C, calls it, then
//     continues. The order of observable effects — C allocated, mN/mM/paths/mC set, only
//     then the guide switch — is unchanged.
//   * the guide-patch switch (D:81-103) matches on a string, so it is an if/else-if chain
//     in D's case order with D's exact 17 accepted spellings. Its default throws
//     `new Error` with a message assembled from four concatenations and one
//     std.format.format("%s") — Error, not Exception; rows cpp.bad.* carry the text with
//     its embedded newlines written as the marker <newline>, the same convention the VTK
//     body rows use.
//   * D:65-66 `double dr = 1.0/(ncpi - 2)` is a division by zero at ncpi == 2 and a
//     NEGATIVE step at ncpi == 1, and D:70-72 then writes rPos[0] = 0.0 before
//     rPos[$-1] = 1.0 overwrites the SAME slot when ncpi == 1. Both are measured
//     (cpp.g1.*, cpp.g2.*) rather than reasoned about, and ncpi == 1 is legal: the patch
//     samples everything at (1.0, 1.0).
//   * D:60-61 `C.length = ncpi; foreach (ref CC; C) CC.length = ncpj;` — a negative length
//     raises D's "Memory allocation failed" Error before any allocation, exactly as in
//     bezierpatch.d (rows bzp.nm2.ctor there). eil::Array::set_length takes size_t, so the
//     wrap is caught on the way in (set_length_checked below); rows cpp.gneg / cpp.gnegj
//     measure that it is the OUTER allocation that fails first.
//   * D:181-194 nCtrlPts/getCtrlPt/setCtrlPt are NOT const in D (no const qualifier) and
//     carry no bounds test: getCtrlPt(0,-1) indexes with the int converted to size_t, so
//     D reports index [18446744073709551615], which is what eil::Array::operator[] gives
//     too (rows cpp.get.*). nCtrlPts reads mC[0].length, not mM.
//   * D:196-222 writeCtrlPtsAsVtkXml appends ".vts" itself and writes j-outer/i-inner with
//     "%20.16e" — the loop order is the file's contract with ParaView and is NOT the
//     i-outer order area() uses. std.stdio.File raises std.exception.ErrnoException on an
//     unusable path (measured: type=std.exception.ErrnoException, msg="Cannot open file
//     `/no_such_dir_xyz/out.vts' in mode `w' (No such file or directory)"); the substrate
//     has no ErrnoException, so like write_surface_as_vtk_xml
//     (src/geom/surface/parametricsurface.cpp:49-58) this throws ExceptionBase with that
//     text, which the cpp.vtk.badpath row compares.
//   * D:176-179 toString() is the CONSTANT "ControlPointPatch()" and carries no state, so
//     the mirror dumps every stored member separately instead.
//   * No classString() exists in D's surface hierarchy (0 matches over
//     gdtk/src/geom/surface), so classString() is declared but not marked override.
//   * D:19-28's imports collapse to what is used: geom.elements (Vector3), geom.gpath
//     (Path and, in the probe, Line/Bezier/CoonsPatch), the three guide-patch classes and
//     the base. std.format.format is eil::format.
//   * The method NAMES are kept in D's camelCase (nCtrlPts, getCtrlPt, setCtrlPt,
//     writeCtrlPtsAsVtkXml) rather than snake-cased, following the precedent of
//     src/geom/gpath/path.hpp (closestDistance, intersect2D). D's free function
//     writeSurfaceAsVtkXml is snake-cased in the base; a class method is not.

#include <memory>
#include <string>

#include "eil/array.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"
#include "geom/surface/parametricsurface.hpp"

namespace eil {
namespace geom {

class ControlPointPatch : public ParametricSurface {
  public:
    // Control points are defined in 2D array C[i][j] with
    // 0 <= i <= N-1, and 0 <= j <= M-1
    // Note: we will index from 0 but Eiseman uses 1-based indices

    // D:41-53 — the caller supplies the control-point grid.
    ControlPointPatch(const PathPtr& south, const PathPtr& north, const PathPtr& west,
                      const PathPtr& east, const eil::Array<eil::Array<Vector3>>& C);

    // D:55-132 — the grid is sampled off a guide patch.
    ControlPointPatch(const PathPtr& south, const PathPtr& north, const PathPtr& west,
                      const PathPtr& east, int ncpi, int ncpj,
                      const std::string& guidePatch);

    // D:134-144 (`this(ref const(ControlPointPatch) other)`)
    ControlPointPatch(const ControlPointPatch& other);

    std::shared_ptr<ParametricSurface> dup() const override;
    Vector3 operator()(double r, double s) const override; // D:161-174 opCall
    std::string toString() const override;
    std::string classString() const; // see header note: not a D symbol here

    void nCtrlPts(int& nci, int& ncj);       // D:181-184
    Vector3 getCtrlPt(int i, int j);         // D:186-189
    void setCtrlPt(int i, int j, Vector3 p); // D:191-194
    void writeCtrlPtsAsVtkXml(const std::string& baseFileName); // D:196-222

  private:
    eil::Array<eil::Array<Vector3>> mC;
    int mN = 0;
    int mM = 0;
    PathPtr mNorth, mEast, mSouth, mWest;

    // The body of D:41-53, shared by the 5-arg constructor and by the mid-body
    // delegation at D:63 (see the header note).
    void init_from_grid(const PathPtr& south, const PathPtr& north, const PathPtr& west,
                        const PathPtr& east, const eil::Array<eil::Array<Vector3>>& C);

    // D:224-294, the Eiseman interpolation scheme. Private in D and unreachable except
    // through opCall, so the port keeps them private.
    double Omega(double r) const;
    double G(double r, int alpha) const;
    double H(double s, int beta) const;
    Vector3 E(double r, int j) const;
    Vector3 F(double s, int i) const;
    Vector3 T(double r, double s) const;
}; // end class ControlPointPatch

} // namespace geom
} // namespace eil
