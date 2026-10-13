#pragma once
// geom/surface/spherepatch.d -> src/geom/surface/spherepatch.hpp
// (module geom.surface.spherepatch)
//
// Maps the unit square onto a face of the canonical cube and then radially onto a
// sphere of radius `radius` about `centre` (D:22-134). Optionally the mapping covers
// only a half or a quarter of the face, selected by `which_part` (D:73-104).
//
// Non-1:1 notes (provenance/fidelity.yml is read-only for this stage, so they are
// recorded here and in the stage report):
//   * D:35 `this(number radius, Vector3 centre, string face_name, string which_part="")`
//     — Vector3 and string are D value types, so the port takes const references and
//     copies in the body, including D:38 `this.centre = Vector3(centre);`.
//   * D:39-42 `canFind(valid_names, face_name)` searches an ARRAY of names, i.e. exact
//     element equality, so "North", "northish", "north " and "" are all rejected with
//     `new Exception` — Exception, not Error (measured: all eight sp.badname.* rows print
//     threw=Exception, and the two opCall guards print threw=Exception too). The message
//     is "Invalid face name: " ~ face_name with no separator, so an empty name gives the
//     trailing-space text "Invalid face name: " (row sp.badname.empty).
//   * D:43 does NOT validate which_part, and the per-axis tests at D:82-100 are
//     SUBSTRING searches, canFind(haystack, needle). The two forms of canFind are
//     therefore ported differently: std::find over the six names, std::string::find for
//     the part strings. "notsouth" on an east face clamps y_cube (row sp.substr.notsouth),
//     which only a substring test can do.
//   * D:82-100: the four ifs per face group are independent, so a part string naming two
//     opposite faces ("south-north") silently lets the LAST one win. Kept verbatim; rows
//     sp.incons.* measure which clamp wins for all six orderings of each opposite pair.
//   * D:64-72 and D:79-103 switch on a string; C++ cannot, so they are if/else-if chains
//     in D's case order with the same default throws. The two guards print "Invalid face: "
//     ~ face_name. The FIRST is reachable only because face_name is public (D:26) and can
//     be mutated after construction (row sp.invalid.firstswitch). The SECOND default is
//     NOT reachable at all: the second switch handles the same six names the constructor
//     admits, so a mutated name fails in the first switch before the second runs —
//     measured, sp.invalid.withpart prints the first switch's message even with
//     which_part set, and sp.secondswitch.top (a valid name plus a which_part) prints
//     threw=none. Both branches are ported anyway because D has them.
//   * D:47-53 the copy constructor copies face_name WITHOUT validating; D:55-58 dup()
//     re-enters the validating constructor. Rows sp.invalid.copy (constructs happily with
//     "bogus" and throws only when called) and sp.invalid.dup (throws at dup time,
//     "Invalid face name: bogus") are the measured pair that separates them.
//   * D:105-106 `double[3] xyz = sphere_mapping(...)` then
//     `Vector3(xyz[0]*radius+centre.x, ...)`: per-component multiply-then-add, in that
//     order, against the stored radius. Preserved (a non-unit radius and an off-origin
//     centre are swept, rows sp.offset.*).
//   * sphere_mapping (D:120-126) is the only substrate function this module reaches:
//     three `sqrt` calls. There is no atan2/atan/asin site here (measured: 0 matches for
//     atan|asin|acos| asin|sin|cos|tan over the 171 D lines; the std.math import at D:13
//     supplies only sqrt), and no division by the radius either (the only `/` is the
//     literal /3.0), so a zero radius collapses the patch onto the centre rather than
//     producing an infinity (rows sp.zeroradius.*, 15 of 15 opCall values exactly equal to
//     the centre). sqrt is eil::sqrt; over [0,1]x[0,1] the radicand is >= 1/3, and outside
//     it goes negative where D's sqrt prints positive nan (rows sp.nanparam.*).
//   * No classString() exists anywhere in D's surface hierarchy (0 matches over
//     gdtk/src/geom/surface), so classString() is declared but not marked override; the
//     sp.<tag> classname= rows compare it with the last dotted component of D's
//     typeid(...).name.
//   * toString() (D:109-116) mixes formatters: to!string(radius) is the %.6g rule,
//     to!string(centre) is Vector3's own toString(), and face_name/which_part are
//     concatenated RAW (no quotes).

#include <array>
#include <memory>
#include <string>

#include "geom/elements/vector3.hpp"
#include "geom/surface/parametricsurface.hpp"
#include "nm/number.hpp"

namespace eil {
namespace geom {

class SpherePatch : public ParametricSurface {
  public:
    number radius;
    Vector3 centre;
    std::string face_name; // face identifier as per the canonical cube
    // This is the cube of named faces described in the Eilmer guide.
    // If we want one half or the other of a full-face patch
    // or one quarter of the patch, we specify the adjoining face(s)
    // nearest the part that we want.
    // For example, we might specify "top" to get the top-half of an east face
    // or "top-south" to get the quarter the east face that includes vertex p5.
    std::string which_part;

    // D:35-45
    SpherePatch(number radius, const Vector3& centre, const std::string& face_name,
                const std::string& which_part = std::string());
    // D:47-53 (`this(ref const(SpherePatch) other)`) — no validation
    SpherePatch(const SpherePatch& other);

    std::shared_ptr<ParametricSurface> dup() const override;
    Vector3 operator()(double r, double s) const override; // D:60-107 opCall
    std::string toString() const override;
    std::string classString() const; // see header note: not a D symbol here

  private:
    // D:120-126 — map -1,1 cube to a sphere of radius 1. D returns a static
    // `double[3]`; the port returns std::array<double, 3>, the same fixed-size value.
    std::array<double, 3> sphere_mapping(double x, double y, double z) const;
}; // end class SpherePatch

} // namespace geom
} // namespace eil
