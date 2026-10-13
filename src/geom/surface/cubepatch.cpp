// geom/surface/cubepatch.d -> src/geom/surface/cubepatch.cpp
// (module geom.surface.cubepatch)

#include "geom/surface/cubepatch.hpp"

#include <algorithm>
#include <array>
#include <memory>
#include <string>

#include "eil/error.hpp"
#include "eil/format.hpp"
#include "geom/elements/vector3.hpp"

namespace eil {
namespace geom {
namespace {

// D:38 `auto valid_names = ["north", "east", "south", "west", "bottom", "top"];` in the
// constructor, checked with canFind over the ARRAY (exact element equality). D:63-71 then
// switches on the same six names.
constexpr std::array<const char*, 6> valid_names = {"north", "east", "south", "west", "bottom",
                                                    "top"};

// D's canFind(haystack, needle) over two strings is a substring test.
bool part_has(const std::string& which_part, const char* needle) {
    return which_part.find(needle) != std::string::npos;
}

} // namespace

// D:34-44
CubePatch::CubePatch(number a, const Vector3& centre, const std::string& face_name,
                     const std::string& which_part) {
    this->a = a;
    this->centre = Vector3(centre);
    if (std::find(valid_names.begin(), valid_names.end(), face_name) == valid_names.end()) {
        // D:40 `throw new Exception("Invalid face name: " ~ face_name)` — Exception, so
        // ExceptionBase in the port (measured: all ten cubp.badname.* rows print
        // threw=Exception).
        throw eil::ExceptionBase("Invalid face name: " + face_name, __FILE__, __LINE__);
    }
    this->face_name = face_name; // Don't check for valid names; there are many.
    this->which_part = which_part;
}

// D:46-52
CubePatch::CubePatch(const CubePatch& other) {
    a = other.a;
    centre = Vector3(other.centre);
    face_name = other.face_name;
    which_part = other.which_part;
}

// D:54-57
std::shared_ptr<ParametricSurface> CubePatch::dup() const {
    return std::make_shared<CubePatch>(a, centre, face_name, which_part);
}

// D:59-105. D switches on a string; the if/else-if chain below keeps D's case order and
// D's default throws.
Vector3 CubePatch::operator()(double r, double s) const {
    double x_cube, y_cube, z_cube;
    // First, map the full face of the cube.
    if (face_name == "east") {
        x_cube = 1.0;
        y_cube = -1.0 + 2.0 * r;
        z_cube = -1.0 + 2.0 * s;
    } else if (face_name == "west") {
        x_cube = -1.0;
        y_cube = -1.0 + 2.0 * r;
        z_cube = -1.0 + 2.0 * s;
    } else if (face_name == "south") {
        x_cube = -1.0 + 2.0 * r;
        y_cube = -1.0;
        z_cube = -1.0 + 2.0 * s;
    } else if (face_name == "north") {
        x_cube = -1.0 + 2.0 * r;
        y_cube = 1.0;
        z_cube = -1.0 + 2.0 * s;
    } else if (face_name == "bottom") {
        x_cube = -1.0 + 2.0 * r;
        y_cube = -1.0 + 2.0 * s;
        z_cube = -1.0;
    } else if (face_name == "top") {
        x_cube = -1.0 + 2.0 * r;
        y_cube = -1.0 + 2.0 * s;
        z_cube = 1.0;
    } else {
        throw eil::ExceptionBase("Invalid face: " + face_name, __FILE__, __LINE__); // D:70
    }
    if (which_part.length() > 0) {
        // Limit the mapping to half or quarter of a full face.
        // which_part may contain more than one face name.
        // It is the responsibility of the user to provide a
        // consistent selection.
        // e.g. "south-top" is ok but "south-north" is not consistent.
        if (face_name == "east" || face_name == "west") {
            if (part_has(which_part, "south")) y_cube = -1.0 + r;
            if (part_has(which_part, "north")) y_cube = r;
            if (part_has(which_part, "bottom")) z_cube = -1.0 + s;
            if (part_has(which_part, "top")) z_cube = s;
        } else if (face_name == "south" || face_name == "north") {
            if (part_has(which_part, "west")) x_cube = -1.0 + r;
            if (part_has(which_part, "east")) x_cube = r;
            if (part_has(which_part, "bottom")) z_cube = -1.0 + s;
            if (part_has(which_part, "top")) z_cube = s;
        } else if (face_name == "bottom" || face_name == "top") {
            if (part_has(which_part, "west")) x_cube = -1.0 + r;
            if (part_has(which_part, "east")) x_cube = r;
            if (part_has(which_part, "south")) y_cube = -1.0 + s;
            if (part_has(which_part, "north")) y_cube = s;
        } else {
            throw eil::ExceptionBase("Invalid face: " + face_name, __FILE__, __LINE__); // D:101
        }
    }
    return Vector3(x_cube * a + centre.x, y_cube * a + centre.y, z_cube * a + centre.z);
} // end opCall()

// D:107-114
std::string CubePatch::toString() const {
    // D's to!string(centre) is Vector3.toString() (the %.6g "Vector3(x, y, z)" form);
    // face_name and which_part are interpolated raw, as D's ~ on strings does.
    return "CubePatch(a=" + eil::to_string(a) + ", centre=" + centre.toString() + ", face=" +
           face_name + ", which_part=" + which_part + ")";
}

// Not a D symbol (see header note).
std::string CubePatch::classString() const { return "CubePatch"; }

} // namespace geom
} // namespace eil
