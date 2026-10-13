#pragma once
// geom/surface/cubepatch.d -> src/geom/surface/cubepatch.hpp
// (module geom.surface.cubepatch)
//
// Maps the unit square onto a face of the cube of edge length 2a centred on `centre`,
// optionally onto a half or a quarter of that face via `which_part` (D:21-116).
//
// Non-1:1 notes (registered in provenance/fidelity.yml):
//   * D:34 `this(number a, Vector3 centre, string face_name, string which_part="")` —
//     Vector3 and string are D value types, so the port takes const references and copies
//     in the body, including D:37 `this.centre = Vector3(centre);` which is the struct
//     copy constructor.
//   * D:38-41 `canFind(valid_names, face_name)` searches an ARRAY of names, i.e. exact
//     element equality, so "east-west", "East" and "east " are all rejected with
//     `new Exception` (Exception, not Error — measured, rows cubp.badname.*, all of them
//     ExceptionBase in the port). The message is "Invalid face name: " ~ face_name, with
//     no separator, so an empty name produces the trailing-space text
//     "Invalid face name: ".
//   * D:42 does NOT validate which_part, so the half/quarter test at D:72
//     (`if (which_part.length > 0)`) accepts any non-empty string and the per-axis tests
//     (D:81-98) are SUBSTRING searches (canFind(haystack, needle)). Both forms of
//     canFind are therefore different in D and are ported differently: std::find over the
//     six names, std::string::find for the part strings.
//   * D:81-98: the four ifs per face group are independent, so a part string naming two
//     opposite faces ("south-north") silently lets the LAST one win — D:75-77 says the
//     user is responsible for consistency. Preserved verbatim (rows cubp.part.*.*).
//   * D:63-71 and D:78-102 switch on a string; C++ cannot, so they are if/else-if chains
//     in D's case order with the same default throws. D:70 throws Exception("Invalid face:
//     " ~ face_name) from opCall and D:100-101 the same text from the part switch. The
//     first is reachable because face_name is public (D:25): assigning an invalid name and
//     calling the patch throws it (rows cubp.mutated.opCall). The SECOND default is not
//     reachable from a constructor-validated patch, because all six valid names are
//     handled by that switch; rows cubp.goodname.* measure exactly that (threw=none for
//     all six with which_part set), and cubp.mutated.opCall shows the first throw winning
//     whenever the name is bad.
//   * D:54-57 dup() re-enters the VALIDATING constructor, so a corrupted face_name makes
//     dup() throw "Invalid face name: " (row cubp.mutated.dup), while the copy
//     constructor (D:46-52) does not validate and copies the bad name (row
//     cubp.copy.invalid). That asymmetry is a real behavioural difference and is kept.
//   * D:104 returns Vector3(x_cube*a + centre.x, ...) — per-component scale-then-add. The
//     arithmetic order is preserved (a non-unit a and an off-origin centre are swept).
//   * No classString() exists in D's surface hierarchy (0 matches over
//     gdtk/src/geom/surface), so classString() is not marked override; the
//     cubp.<tag> classname= rows compare it with the last dotted component of D's
//     typeid(...).name.
//   * toString() uses to!string(a), i.e. the %.6g rule (a = 1/3 prints "0.333333", row
//     cubp.frac), and to!string(centre) is Vector3's own toString(). face_name and
//     which_part are interpolated RAW (no to!string), so no quotes appear.

#include <memory>
#include <string>

#include "geom/elements/vector3.hpp"
#include "geom/surface/parametricsurface.hpp"
#include "nm/number.hpp"

namespace eil {
namespace geom {

class CubePatch : public ParametricSurface {
  public:
    number a;
    Vector3 centre;
    std::string face_name; // face identifier as per the canonical cube
    // This is the cube of named faces described in the Eilmer guide.
    // If we want one half or the other of a full-face patch
    // or one quarter of the patch, we specify the adjoining face(s)
    // nearest the part that we want.
    // For example, we might specify "top" to get the top-half of an east face
    // or "top-south" to get the quarter the east face that includes vertex p5.
    std::string which_part;

    // D:34-44
    CubePatch(number a, const Vector3& centre, const std::string& face_name,
              const std::string& which_part = std::string());
    // D:46-52 (`this(ref const(CubePatch) other)`) — no validation
    CubePatch(const CubePatch& other);

    std::shared_ptr<ParametricSurface> dup() const override;
    Vector3 operator()(double r, double s) const override; // D:59-105 opCall
    std::string toString() const override;
    std::string classString() const; // see header note: not a D symbol here
}; // end class CubePatch

} // namespace geom
} // namespace eil
