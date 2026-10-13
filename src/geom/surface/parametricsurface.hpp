#pragma once
// geom/surface/parametricsurface.d -> src/geom/surface/parametricsurface.hpp
// (module geom.surface.parametricsurface)
//
// The abstract base of the two-parameter surface hierarchy: 15 D subclasses
// (grep '^class .* : ParametricSurface' over geom/surface) and the type every
// structured/unstructured grid builder in geom/grid takes. wave-02-brief
// section 4b fixes the shape of this class; the notes below are only where the
// port had to depart from a literal reading of D.
//
// Non-1:1 notes (registered in provenance/fidelity.yml):
//   * adapted-handles: D:37 `abstract ParametricSurface dup() const` returns a class
//     handle, so the port returns std::shared_ptr<ParametricSurface>. Covariant
//     returns are impossible through shared_ptr, so a subclass's dup() returns the
//     base handle and loses the static type D gave it; call sites that need the
//     derived type use dynamic_pointer_cast, which is the same run-time type test
//     D's `cast(X)s.dup()` performs.
//   * D:36 `abstract Vector3 opCall(double r, double s) const` is the C++
//     operator()(r,s), matching geom/gpath/path.hpp.
//   * area() (D:40-66) is NOT const in D, because `opCall` is const but the method
//     carries no const qualifier; it is kept non-const here so a caller cannot use a
//     const surface, exactly as in D.
//   * D:45-46 declare `Vector3[][] p;` then `p.length = nr+1;`, which yields nr+1
//     *null* inner arrays, and D:51 `p[i] ~= opCall(r,s)` grows each from null.
//     eil::Array<Array<Vector3>> with set_length(nr+1) reproduces that: the inner
//     arrays start empty and append, so the final shape is (nr+1) x (ns+1) in the
//     same row-major visitation order D uses.
//   * D:42 `Vector3 vector_area; vector_area.set(0.0,0.0,0.0);` — the default
//     construction is Vector3.init (NaN, wave-02-brief section 5) and the set() makes
//     it zero. Both statements are kept, in that order; dropping the first would
//     change nothing observable but would stop matching D statement for statement.
//   * D:59 quad_properties(...) is geom.elements' free function; the port passes the
//     four corners positionally just as D does, and the two tolerance parameters
//     keep D's defaults (1.0e-12 and 1.0e-20).
//   * writeSurfaceAsVtkXml (D:70-94) is a module-level free function taking a
//     *class handle by value* (D reference semantics), so the port takes a
//     const reference to the object rather than a shared_ptr: it only calls the
//     virtual opCall. It writes through a FILE*, opened with "w"; D's std.stdio.File
//     raises std.exception.ErrnoException when the path is unusable and the
//     substrate has no ErrnoException (svg.cpp:139-147 reproduces the message as an
//     ExceptionBase), so this does the same and the text is measured, not invented.
//   * The `%20.16e` coordinate rows and the six literal header lines are the file's
//     contract with ParaView and with the D-side golden files; the loop order is
//     j-outer/i-inner (D:80-81), which is *not* the i-outer order area() uses.

#include <memory>
#include <string>

#include "eil/array.hpp"
#include "geom/elements/vector3.hpp"

namespace eil {
namespace geom {

class ParametricSurface {
  public:
    virtual ~ParametricSurface() = default;

    virtual Vector3 operator()(double r, double s) const = 0; // D:36 opCall
    virtual std::shared_ptr<ParametricSurface> dup() const = 0;
    virtual std::string toString() const = 0;

    // D:40-66
    Vector3 area(int nr = 10, int ns = 10);

    // D's subclasses also rely on the class being a reference type; the alias the
    // port uses everywhere for `ParametricSurface surf`.
    using Ptr = std::shared_ptr<ParametricSurface>;
};

// D:70-94
void write_surface_as_vtk_xml(const ParametricSurface& surf, const std::string& fileName,
                              int nrPts, int nsPts);

} // namespace geom
} // namespace eil
