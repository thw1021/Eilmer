// geom/surface/spherepatch.d -> src/geom/surface/spherepatch.cpp
// (module geom.surface.spherepatch)

#include "geom/surface/spherepatch.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <string>

#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/math.hpp"
#include "geom/elements/vector3.hpp"

namespace eil {
namespace geom {
namespace {

// D:39 `auto valid_names = ["north", "east", "south", "west", "bottom", "top"];`
// The order is D's literal order and is what std::find compares against; membership is
// exact element equality, not a substring test.
constexpr std::array<const char*, 6> valid_names = {"north", "east", "south", "west", "bottom",
                                                    "top"};

// D's canFind(haystack, needle) over two strings is a substring test.
bool part_has(const std::string& which_part, const char* needle) {
    return which_part.find(needle) != std::string::npos;
}

} // namespace

// D:35-45
SpherePatch::SpherePatch(number radius, const Vector3& centre, const std::string& face_name,
                         const std::string& which_part) {
    this->radius = radius;
    this->centre = Vector3(centre);
    if (std::find(valid_names.begin(), valid_names.end(), face_name) == valid_names.end()) {
        // D:40-42 — Exception (not Error), measured threw=Exception for all eight
        // sp.badname.* rows. The message concatenates with no separator, so an empty
        // face_name leaves the trailing space.
        throw eil::ExceptionBase("Invalid face name: " + face_name, __FILE__, __LINE__);
    }
    this->face_name = face_name; // Don't check for valid names; there are many.
    this->which_part = which_part;
}

// D:47-53 — the copy constructor does NOT re-validate face_name (see header note;
// measured by sp.invalid.copy versus sp.invalid.dup).
SpherePatch::SpherePatch(const SpherePatch& other) {
    radius = other.radius;
    centre = Vector3(other.centre);
    face_name = other.face_name;
    which_part = other.which_part;
}

// D:55-58 — re-enters the validating constructor.
std::shared_ptr<ParametricSurface> SpherePatch::dup() const {
    return std::make_shared<SpherePatch>(radius, centre, face_name, which_part);
}

// D:60-107. D switches on a string; the if/else-if chains below keep D's case order and
// D's default throws.
Vector3 SpherePatch::operator()(double r, double s) const {
    double x_cube, y_cube, z_cube;
    // First, map the full face of the cube.
    if (face_name == "east") { // D:65
        x_cube = 1.0;
        y_cube = -1.0 + 2.0 * r;
        z_cube = -1.0 + 2.0 * s;
    } else if (face_name == "west") { // D:66
        x_cube = -1.0;
        y_cube = -1.0 + 2.0 * r;
        z_cube = -1.0 + 2.0 * s;
    } else if (face_name == "south") { // D:67
        x_cube = -1.0 + 2.0 * r;
        y_cube = -1.0;
        z_cube = -1.0 + 2.0 * s;
    } else if (face_name == "north") { // D:68
        x_cube = -1.0 + 2.0 * r;
        y_cube = 1.0;
        z_cube = -1.0 + 2.0 * s;
    } else if (face_name == "bottom") { // D:69
        x_cube = -1.0 + 2.0 * r;
        y_cube = -1.0 + 2.0 * s;
        z_cube = -1.0;
    } else if (face_name == "top") { // D:70
        x_cube = -1.0 + 2.0 * r;
        y_cube = -1.0 + 2.0 * s;
        z_cube = 1.0;
    } else {
        throw eil::ExceptionBase("Invalid face: " + face_name, __FILE__, __LINE__); // D:71
    }
    if (which_part.length() > 0) {
        // Limit the mapping to half or quarter of a full face.
        // which_part may contain more than one face name.
        // It is the responsibility of the user to provide a
        // consistent selection.
        // e.g. "south-top" is ok but "south-north" is not consistent.
        if (face_name == "east" || face_name == "west") { // D:80-86
            if (part_has(which_part, "south")) y_cube = -1.0 + r;
            if (part_has(which_part, "north")) y_cube = r;
            if (part_has(which_part, "bottom")) z_cube = -1.0 + s;
            if (part_has(which_part, "top")) z_cube = s;
        } else if (face_name == "south" || face_name == "north") { // D:87-93
            if (part_has(which_part, "west")) x_cube = -1.0 + r;
            if (part_has(which_part, "east")) x_cube = r;
            if (part_has(which_part, "bottom")) z_cube = -1.0 + s;
            if (part_has(which_part, "top")) z_cube = s;
        } else if (face_name == "bottom" || face_name == "top") { // D:94-100
            if (part_has(which_part, "west")) x_cube = -1.0 + r;
            if (part_has(which_part, "east")) x_cube = r;
            if (part_has(which_part, "south")) y_cube = -1.0 + s;
            if (part_has(which_part, "north")) y_cube = s;
        } else {
            // D:101-102. Unreachable from a constructor-validated patch (see header
            // note): measured by sp.invalid.withpart, which reports the FIRST switch's
            // message, and sp.secondswitch.top, which reports threw=none.
            throw eil::ExceptionBase("Invalid face: " + face_name, __FILE__, __LINE__);
        }
    }
    std::array<double, 3> xyz = sphere_mapping(x_cube, y_cube, z_cube); // D:105
    // D:106 — per-component multiply-then-add against the stored radius.
    return Vector3(xyz[0] * radius + centre.x, xyz[1] * radius + centre.y,
                   xyz[2] * radius + centre.z);
} // end opCall()

// D:109-116
std::string SpherePatch::toString() const {
    // to!string(radius) is the %.6g rule; to!string(centre) is Vector3.toString().
    // face_name and which_part are interpolated raw, as D's ~ on strings does.
    return "SpherePatch(radius=" + eil::to_string(radius) + ", centre=" + centre.toString() +
           ", face=" + face_name + ", which_part=" + which_part + ")";
}

// Not a D symbol (see header note).
std::string SpherePatch::classString() const { return "SpherePatch"; }

// D:120-126. Map -1,1 cube to a sphere of radius 1.
std::array<double, 3> SpherePatch::sphere_mapping(double x, double y, double z) const {
    double x_dash = x * eil::sqrt(1.0 - 0.5 * z * z - 0.5 * y * y + y * y * z * z / 3.0);
    double y_dash = y * eil::sqrt(1.0 - 0.5 * z * z - 0.5 * x * x + x * x * z * z / 3.0);
    double z_dash = z * eil::sqrt(1.0 - 0.5 * y * y - 0.5 * x * x + x * x * y * y / 3.0);
    return {x_dash, y_dash, z_dash};
}
// Source: https://math.stackexchange.com/questions/118760/
// can-someone-please-explain-the-cube-to-sphere-mapping-formula-to-me
//
// For cube with edge length 2a, resulting in sphere with radius a.
// x' = x * a * math.sqrt( a*a - z*z/2 - y*y/2 + y*y*z*z/(3*a*a))
// y' = y * a * math.sqrt( a*a - z*z/2 - x*x/2 + x*x*z*z/(3*a*a))
// z' = z * a * math.sqrt( a*a - y*y/2 - x*x/2 + x*x*y*y/(3*a*a))

} // namespace geom
} // namespace eil
