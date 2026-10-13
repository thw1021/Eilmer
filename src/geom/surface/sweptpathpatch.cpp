// geom/surface/sweptpathpatch.d -> src/geom/surface/sweptpathpatch.cpp
// (module geom.surface.sweptpathpatch)

#include "geom/surface/sweptpathpatch.hpp"

#include <memory>
#include <string>

#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"

namespace eil {
namespace geom {

// D:17-25. D:21-24 read the PARAMETERS (they shadow the members in D), which are the
// caller's objects, not the dups — the values are the same at this point, but the
// evaluation targets are kept exactly as D writes them.
SweptPathPatch::SweptPathPatch(const PathPtr& cA, const PathPtr& cB) {
    this->cA = cA->dup();
    this->cB = cB->dup();
    p00 = (*cB)(0.0);
    p10 = (*cB)(1.0);
    p01 = (*cB)(0.0) + (*cA)(1.0) - (*cA)(0.0);
    p11 = (*cB)(1.0) + (*cA)(1.0) - (*cA)(0.0);
}

// D:27-35 — the corners come over verbatim; they are not re-derived.
SweptPathPatch::SweptPathPatch(const SweptPathPatch& other) {
    cA = other.cA->dup();
    cB = other.cB->dup();
    p00 = other.p00;
    p10 = other.p10;
    p11 = other.p11;
    p01 = other.p01;
}

// D:37-40 — back through the constructor, so the corners are recomputed from the
// boundaries (measured against spp.corrupt.copy, which keeps them).
std::shared_ptr<ParametricSurface> SweptPathPatch::dup() const {
    return std::make_shared<SweptPathPatch>(cA, cB);
}

// D:42-46. Association is D's: (cB(r) + cA(s)) - cA(0.0).
Vector3 SweptPathPatch::operator()(double r, double s) const {
    Vector3 p = (*cB)(r) + (*cA)(s) - (*cA)(0.0);
    return p;
}

// D:48-51
std::string SweptPathPatch::toString() const {
    return "SweptPathPatch(cA=" + d_tostr(cA) + ", cB=" + d_tostr(cB) + ")";
}

// Not a D symbol (see header note).
std::string SweptPathPatch::classString() const { return "SweptPathPatch"; }

} // namespace geom
} // namespace eil

// D:54 — kept verbatim. The `sweptpathpatch` version identifier is not enabled anywhere
// in the gdtk tree (grep -r 'version(sweptpathpatch)' finds only this comment), so there
// is no branch to carry; the TODO is the module's own note about future work.
// [TODO] version(sweptpathpatch)
