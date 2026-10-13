#pragma once
// geom/elements/nomenclature.d -> src/geom/elements/nomenclature.hpp
// (module geom.elements.nomenclature).
//
// Names used across the geometry code: cell-face indices, VTK cell types, the
// degree-to-radian helper and the quaternion alias.
//
// Non-1:1 notes (registered in provenance/fidelity.yml):
//   * `enum Face` (D:12-19) has deliberately aliased enumerators — west == iminus == 0,
//     east == iplus == 1, and so on. That is legal C++ and both spellings are kept,
//     in D's declaration order, with an explicit `unsigned` underlying type so
//     `return Face.west;` from a `uint` function stays implicit (D:26).
//   * `string[] face_name` (D:20) is a *mutable module-level D array*, not a
//     compile-time constant. It is ported as `eil::Array<std::string>` so that
//     `face_name[i]` bounds-checks like D (out-of-range throws eil::RangeError, an
//     Error), `face_name.length()` reads like D's `.length`, and `eil::to_string`
//     renders it the way Phobos renders a string array — measured:
//     `["west", "east", "south", "north", "bottom", "top"]` with the quotes
//     (probes/geom/elements/probe_nomenclature.d, "array=" row). Declared `extern`
//     here and defined once in nomenclature.cpp, because D's global is writable.
//   * `face_index` (D:23-35) throws `new Error(text("Invalid face name: ", name))` —
//     a D **Error**, so the port throws eil::ErrorBase. Measured in
//     probes/geom/elements/probe_nomenclature.d and again in probe_projection.d:
//     `catch (Exception)` in D does NOT catch it, so ported callers must not catch it
//     with ExceptionBase either (wave-1 lesson 1). Message text is byte-identical:
//     "Invalid face name: northwest".
//   * `radians` (D:56) is `PI*degrees/180.0` where PI is D's 80-bit real: the product
//     and quotient are computed in extended precision and rounded once on return.
//     Measured against ldc2 (probe rows "rad ..."), that differs from the pure-double
//     `(M_PI*degrees/180)` in 4 of 14 sampled angles (30, 60, 1/3, 1e8 degrees) by
//     1 ULP. The port therefore writes EIL_PI in eil::real arithmetic and casts on
//     return; it must not be replaced by M_PI.
//   * `alias Quaternion = double[4]` (D:68) is a D *static* array of length 4, so it
//     maps to std::array<double,4>. Note the .init difference: D's `double[4].init`
//     is four copies of `double.init`, which is NaN, so D's `Quaternion q;` starts as
//     [nan, nan, nan, nan] (measured: "quaternion_init"/"default_local" rows).
//     `std::array` leaves an un-initialised object indeterminate, so D's form is
//     spelled quaternion_init() here and `Quaternion q{};` is the zeroed one.

#include <array>
#include <cstddef>
#include <string>

#include "eil/array.hpp"
#include "eil/number.hpp"

namespace eil {
namespace geom {

// D:12-19. Aliased enumerators, kept in declaration order.
enum Face : unsigned {
    west = 0, iminus = 0,
    east = 1, iplus = 1,
    south = 2, jminus = 2,
    north = 3, jplus = 3,
    bottom = 4, kminus = 4,
    top = 5, kplus = 5
};

// D:20 — mutable global, see the header note.
extern eil::Array<std::string> face_name;
// Chicken names:       iminus, iplus,  jminus,  jplus,   kminus,   kplus

// D:23-35. Throws eil::ErrorBase (a D Error), not ExceptionBase.
unsigned face_index(const std::string& name);

// D:39-54, VTK cell types for reading and writing VTK files.
enum VTKElement : unsigned {
    vertex = 1,
    polyvertex = 2,
    line = 3,
    polyline = 4,
    triangle = 5,
    triangle_strip = 6,
    polygon = 7,
    pixel = 8,
    quad = 9,
    tetra = 10,
    voxel = 11,
    hexahedron = 12,
    wedge = 13,
    pyramid = 14
};

// D:56 `double radians(double degrees) { return PI*degrees/180.0; }` — wide
// arithmetic, single rounding on return.
double radians(double degrees);

// D:58-68. Quaternion represented as four scalars:
//
//   q = q0 + q1 i + q2 j + q3 k,  Quaternion q = (q0, q1, q2, q3);
//   double rot_scalar = q[0];
using Quaternion = std::array<double, 4>;

// D's `Quaternion.init` — four times `double.init`, which is NaN.
Quaternion quaternion_init();

} // namespace geom
} // namespace eil
