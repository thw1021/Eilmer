// geom/surface/coonspatch.d -> src/geom/surface/coonspatch.cpp
// (module geom.surface.coonspatch)

#include "geom/surface/coonspatch.hpp"

#include <memory>
#include <string>

#include "eil/error.hpp"
#include "eil/format.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/line.hpp"
#include "geom/gpath/path.hpp"

namespace eil {
namespace geom {

// D:31-38. D builds the four Lines and then delegates; C++ delegates directly, so the
// same four Lines reach the checking constructor (D:40-74) and its corner cross-checks.
// The pairing is D's, verbatim: north = Line(p01,p11), east = Line(p10,p11),
// south = Line(p00,p10), west = Line(p00,p01).
CoonsPatch::CoonsPatch(const Vector3& p00, const Vector3& p10, const Vector3& p11,
                       const Vector3& p01)
    : CoonsPatch(std::make_shared<Line>(p00, p10), std::make_shared<Line>(p01, p11),
                 std::make_shared<Line>(p00, p01), std::make_shared<Line>(p10, p11)) {}

// D:40-74. Parameter order is south, north, west, east (D's comment at D:41-43 explains
// why it is not NESW).
CoonsPatch::CoonsPatch(const PathPtr& south, const PathPtr& north, const PathPtr& west,
                       const PathPtr& east) {
    this->north = north->dup();
    this->east = east->dup();
    this->south = south->dup();
    this->west = west->dup();
    p00 = (*(this->south))(0.0);
    p10 = (*(this->south))(1.0);
    p01 = (*(this->north))(0.0);
    p11 = (*(this->north))(1.0);
    // Check alternate evaluation of corners for consistency.
    Vector3 p00_alt = (*(this->west))(0.0);
    Vector3 p10_alt = (*(this->east))(0.0);
    Vector3 p01_alt = (*(this->west))(1.0);
    Vector3 p11_alt = (*(this->east))(1.0);
    // D:58-73 — four `throw new Error(text(...))` guards, in this order, so only the
    // first open corner reports. text(...) over a Vector3 is its toString(), i.e. the
    // %.6g form; measured in rows cnp.open.* / cnp.tol.* of
    // baselines/e587b235/probes/geom_surface_coonspatch.txt.
    if (!approxEqualVectors(p00, p00_alt)) {
        throw eil::ErrorBase(eil::text("CoonsPatch open corner p00= ", d_tostr(p00),
                                        " p00_alt= ", d_tostr(p00_alt)),
                             __FILE__, __LINE__);
    }
    if (!approxEqualVectors(p10, p10_alt)) {
        throw eil::ErrorBase(eil::text("CoonsPatch open corner p10= ", d_tostr(p10),
                                        " p10_alt= ", d_tostr(p10_alt)),
                             __FILE__, __LINE__);
    }
    if (!approxEqualVectors(p11, p11_alt)) {
        throw eil::ErrorBase(eil::text("CoonsPatch open corner p11= ", d_tostr(p11),
                                        " p11_alt= ", d_tostr(p11_alt)),
                             __FILE__, __LINE__);
    }
    if (!approxEqualVectors(p01, p01_alt)) {
        throw eil::ErrorBase(eil::text("CoonsPatch open corner p01= ", d_tostr(p01),
                                        " p01_alt= ", d_tostr(p01_alt)),
                             __FILE__, __LINE__);
    }
} // end constructor()

// D:76-86 — no corner checks; the corners come over verbatim.
CoonsPatch::CoonsPatch(const CoonsPatch& other) {
    this->north = other.north->dup();
    this->east = other.east->dup();
    this->south = other.south->dup();
    this->west = other.west->dup();
    p00 = other.p00;
    p10 = other.p10;
    p01 = other.p01;
    p11 = other.p11;
}

// D:88-91 — back through the checking constructor, from the already-duped boundaries.
std::shared_ptr<ParametricSurface> CoonsPatch::dup() const {
    return std::make_shared<CoonsPatch>(south, north, west, east);
}

// D:93-102
Vector3 CoonsPatch::operator()(double r, double s) const {
    Vector3 south_r = (*south)(r);
    Vector3 north_r = (*north)(r);
    Vector3 west_s = (*west)(s);
    Vector3 east_s = (*east)(s);
    Vector3 p = (1.0 - s) * south_r + s * north_r + (1.0 - r) * west_s + r * east_s -
                ((1.0 - r) * (1.0 - s) * p00 + (1.0 - r) * s * p01 + r * (1.0 - s) * p10 +
                 r * s * p11);
    return p;
}

// D:104-111
std::string CoonsPatch::toString() const {
    return "CoonsPatch(south=" + d_tostr(south) + ", north=" + d_tostr(north) + ", west=" +
           d_tostr(west) + ", east=" + d_tostr(east) + ")";
}

// Not a D symbol (see header note).
std::string CoonsPatch::classString() const { return "CoonsPatch"; }

} // namespace geom
} // namespace eil
