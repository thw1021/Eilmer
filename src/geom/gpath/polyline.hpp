#pragma once
// geom/gpath/polyline.d -> src/geom/gpath/polyline.hpp (module geom.gpath.polyline).
//
// A Path made of Path segments, with the parameter breakpoints set from the
// cumulative segment lengths (reset_breakpoints, D:281-290), plus the
// Gauss-Seidel cubic-Bezier spline-through-points constructor (D:53-108) and the
// Gmsh .geo writer (D:175-278).
//
// Non-1:1 notes:
//   * adapted-handles: D:27 `Path[] segments` holds class references, so it is an
//     eil::Array of std::shared_ptr<Path> here and dup() returns shared_ptr<Path>.
//     D's constructor dups every segment (D:37), so a Polyline owns copies and
//     mutating the caller's segment objects afterwards has no effect — the shared
//     pointers are to freshly dupped objects, matching that.
//   * D:107 `this(seg, isClosed)` is a delegating constructor call at the END of
//     the spline constructor's body; C++ can only delegate in the mem-init list, so
//     the statements that build `seg` (D:55-105) move verbatim into the static
//     helper spline_segments() and the delegation happens before the body runs.
//     Same order of arithmetic, same Gauss-Seidel iteration count and break test.
//   * D:97 `Vector3[4] p03` (a static array, elements = Vector3.init) ports to an
//     eil::Array<Vector3> of length 4, which value-initialises to the same NaN
//     triple per element; all four entries are assigned before Bezier reads them.
//   * D:204 `int tag1;` is *not* assigned on the path where the last segment's end
//     point is within tol of previousPoint, and D's int.init is 0, so the Gmsh line
//     is written with tag 0 (measured: see the "gmsh.*" rows of
//     baselines/e587b235/probes/geom_gpath_polyline.txt). The port initialises tag1
//     to 0 for that reason, not to silence a warning.
//   * D:226 `auto mySeg = cast(Bezier) seg;` -> dynamic_cast, per brief 4b item 4.
//     D's switch is on a string, which C++ cannot do; the cases become an if/else
//     chain in the same order ("Line", "Arc", "Bezier", default).
//   * D:111-130's file constructor uses std.stdio.File/byLine + std.string
//     strip/split. Tokenisation is the shared eil::geom::detail helper (path.hpp)
//     so the four gpath file readers agree; a missing file throws
//     eil::ExceptionBase instead of D's File exception, with the same text intent.
//   * toGmshString() is a non-const method in D (it takes `ref int` tags); kept
//     non-const, and the tag references model D's `ref int` (the caller sees the
//     incremented tags, which is how paver.d chains multiple polylines).

#include <cstddef>
#include <memory>
#include <string>

#include "eil/array.hpp"
#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"

namespace eil {
namespace geom {

class Polyline : public Path {
  public:
    PathList segments;              // D:27, collection of Path segments
    eil::Array<double> t_values;    // D:28, collection of segment break-points
    bool closed = false;            // D:29, assume an open path, unless told otherwise

    // D:32-50. Construct from the Path segments.
    Polyline(const PathList& segments, bool isClosed = false, double tolerance = 1.0e-10);
    // D:53-108. Construct as a spline through specified points.
    Polyline(const eil::Array<Vector3>& p_orig, bool isClosed = false,
             double tolerance = 1.0e-10);
    // D:111-130. Contructs a spline from a file containing x(,y(,z)) coordinates.
    Polyline(const std::string& fileName, bool isClosed = false);
    // D:132-135, `this(ref const(Polyline) other, bool isClosed=false)`
    Polyline(const Polyline& other, bool isClosed = false);

    std::shared_ptr<Path> dup() const override;
    Vector3 operator()(double t) const override;
    std::string toString() const override;
    std::string classString() const override;

    // D:175-278. We use int tags for the points, curves and loops written to the
    // string. Values passed in will be used as the starting values for these tags.
    // Subsequent calls should be careful to not double-up on tag values.
    std::string toGmshString(int& pointTag, int& curveTag, int& loopTag,
                             const std::string& label = "", double len = 1.0e-2);

  private:
    void reset_breakpoints(); // D:281-290
    // The body of D:55-105, which D runs before delegating to this(seg, isClosed).
    static PathList spline_segments(const eil::Array<Vector3>& p_orig, bool isClosed,
                                    double tolerance);
    // The body of D:117-129, which D runs before delegating to this(points, isClosed).
    static eil::Array<Vector3> points_from_file(const std::string& fileName);
}; // end class Polyline

} // namespace geom
} // namespace eil
