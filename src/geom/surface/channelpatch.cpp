// geom/surface/channelpatch.d -> src/geom/surface/channelpatch.cpp
// (module geom.surface.channelpatch)

#include "geom/surface/channelpatch.hpp"

#include <memory>
#include <string>

#include "eil/array.hpp"
#include "eil/format.hpp"
#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/bezier.hpp"
#include "geom/gpath/line.hpp"
#include "geom/gpath/path.hpp"

namespace eil {
namespace geom {

// D:21-31
ChannelPatch::ChannelPatch(const PathPtr& cSouth, const PathPtr& cNorth, bool ruled, bool pure2D) {
    this->cSouth = cSouth->dup();
    this->cNorth = cNorth->dup();
    this->ruled = ruled;
    this->pure2D = pure2D;
    p00 = (*(this->cSouth))(0.0);
    p10 = (*(this->cSouth))(1.0);
    p01 = (*(this->cNorth))(0.0);
    p11 = (*(this->cNorth))(1.0);
}

// D:33-43
ChannelPatch::ChannelPatch(const ChannelPatch& other) {
    cSouth = other.cSouth->dup();
    cNorth = other.cNorth->dup();
    ruled = other.ruled;
    pure2D = other.pure2D;
    p00 = other.p00;
    p10 = other.p10;
    p01 = other.p01;
    p11 = other.p11;
}

// D:45-48
std::shared_ptr<ParametricSurface> ChannelPatch::dup() const {
    return std::make_shared<ChannelPatch>(cSouth, cNorth, ruled, pure2D);
}

// D:50-56
Vector3 ChannelPatch::operator()(double r, double s) const {
    PathPtr bridge_path = make_bridging_path(r);
    Vector3 p = (*bridge_path)(s);
    if (pure2D) {
        p.z = 0.0;
    }
    return p;
}

// D:58-62
std::string ChannelPatch::toString() const {
    return "ChannelPatch(cSouth=" + d_tostr(cSouth) + ", cNorth=" + d_tostr(cNorth) + ", ruled=" +
           eil::to_string(ruled) + ", pure2D=" + eil::to_string(pure2D) + ")";
}

// Not a D symbol (see header note).
std::string ChannelPatch::classString() const { return "ChannelPatch"; }

// D:64-104
// For any value 0.0 <= r <= 1.0, this function returns the Path
// that bridges the patch from a point A on the south curve to a
// corresponding point B on the north curve.
//
// When using this surface patch in a simulation with adjacent
// patches, it may be useful to call this function with values
// of r=0.0 and r=1.0 to get the west and east edges, respectively,
// of this ChannelPatch.
// These edges can be used to construct conforming patches.
//
// Note that the Bezier, with all 4 points colinear and equally spaced,
// will be equivalent to a straight line between its end points.
// You can make your life easy by carefully setting the end points
// and slopes of your defining south and north edge paths.
PathPtr ChannelPatch::make_bridging_path(double r) const {
    Vector3 pA = (*cSouth)(r);
    Vector3 pB = (*cNorth)(r);
    if (pure2D) {
        pA.z = 0.0;
        pB.z = 0.0;
    }
    if (ruled) {
        // Bridge with a straight line for a ruled surface.
        return std::make_shared<Line>(pA, pB);
    } else {
        // Bridge with a Bezier3 path that is normal to both defining curves.
        Vector3 pBminuspA = pB - pA;
        double L = eil::re(abs(pBminuspA)); // D:89 abs(pBminuspA).re
        Vector3 tangentA = cSouth->dpdt(r);
        Vector3 tangentB = cNorth->dpdt(r);
        // Out-of-plane vectors
        Vector3 oopvA = cross(tangentA, pBminuspA);
        Vector3 oopvB = cross(tangentB, pBminuspA);
        // Inward-facing normal vectors on the surface.
        // D:96-97 use different operand orders for the two ends; that is what makes both
        // normals face into the channel, and it is preserved exactly.
        Vector3 nA = cross(oopvA, tangentA);
        nA.normalize();
        Vector3 nB = cross(tangentB, oopvB);
        nB.normalize();
        // Intermediate control points for the cubic Bezier.
        Vector3 p1 = pA + L / 3.0 * nA;
        Vector3 p2 = pB + L / 3.0 * nB;
        if (pure2D) {
            p1.z = 0.0;
            p2.z = 0.0;
        }
        return std::make_shared<Bezier>(eil::Array<Vector3>{pA, p1, p2, pB});
    }
} // end make_bridging_path()

} // namespace geom
} // namespace eil
