// geom/surface/gmopatch.d -> src/geom/surface/gmopatch.cpp
// (module geom.surface.gmopatch)

#include "geom/surface/gmopatch.hpp"

#include <memory>
#include <string>

#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/math.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/line.hpp"
#include "geom/gpath/path.hpp"
#include "geom/grid/sgrid.hpp"
#include "geom/misc/univariatefunctions.hpp"
#include "geom/surface/coonspatch.hpp"

namespace eil {
namespace geom {

// D:35-43. D builds the four bounding Lines and then delegates to the four-Path
// constructor with the SAME niv/njv it was handed (D:42), so the corner constructor
// inherits the checks and the redistribution. Pairing is D:38-41 verbatim:
// north = Line(p01,p11), east = Line(p10,p11), south = Line(p00,p10), west = Line(p00,p01).
GMOPatch::GMOPatch(const Vector3& p00, const Vector3& p10, const Vector3& p11,
                   const Vector3& p01, int niv, int njv)
    : GMOPatch(std::make_shared<Line>(p00, p10), std::make_shared<Line>(p01, p11),
               std::make_shared<Line>(p00, p01), std::make_shared<Line>(p10, p11), niv, njv) {}

// D:45-89. Parameter order is south, north, west, east (D:47-50 explains that it is the
// grid-generation paper's order, not NESW).
GMOPatch::GMOPatch(const PathPtr& south, const PathPtr& north, const PathPtr& west,
                   const PathPtr& east, int niv, int njv) {
    this->north = north->dup();
    this->east = east->dup();
    this->south = south->dup();
    this->west = west->dup();
    // D:56-59 evaluate the unqualified names, which in D resolve to the PARAMETERS (they
    // shadow the members declared at D:22). The members were just set to dups of those
    // same handles, so the corners are the same numbers either way; transcribed with the
    // parameters because that is what D resolves.
    p00 = (*south)(0.0);
    p10 = (*south)(1.0);
    p01 = (*north)(0.0);
    p11 = (*north)(1.0);
    // Check alternate evaluation of corners for consistency. (D:60-64)
    Vector3 p00_alt = (*west)(0.0);
    Vector3 p10_alt = (*east)(0.0);
    Vector3 p01_alt = (*west)(1.0);
    Vector3 p11_alt = (*east)(1.0);
    // D:65-80 — four `throw new Error(text(...))` guards, in the order p00, p10, p11,
    // p01, so only the first open corner is ever reported (corpus row gmo.open.all still
    // names p00). Bare object.Error carries no site (T-922), and text(...) over a Vector3
    // is its %.6g toString().
    if (!approxEqualVectors(p00, p00_alt)) {
        throw eil::ErrorBase(eil::text("GMOPatch open corner p00= ", d_tostr(p00),
                                       " p00_alt= ", d_tostr(p00_alt)));
    }
    if (!approxEqualVectors(p10, p10_alt)) {
        throw eil::ErrorBase(eil::text("GMOPatch open corner p10= ", d_tostr(p10),
                                       " p10_alt= ", d_tostr(p10_alt)));
    }
    if (!approxEqualVectors(p11, p11_alt)) {
        throw eil::ErrorBase(eil::text("GMOPatch open corner p11= ", d_tostr(p11),
                                       " p11_alt= ", d_tostr(p11_alt)));
    }
    if (!approxEqualVectors(p01, p01_alt)) {
        throw eil::ErrorBase(eil::text("GMOPatch open corner p01= ", d_tostr(p01),
                                       " p01_alt= ", d_tostr(p01_alt)));
    }
    // Now we need to set up a temporary grid and adjust the internal distribution of the
    // interpolation parameters to get a nice grid. We hang onto the redistributed
    // parameters. (D:81-88)
    //
    // D:84 hands CoonsPatch the caller's four paths (parameters again, shadowing the
    // members) and CoonsPatch dups them; D:87's StructuredGrid is a local that dies with
    // the constructor, and the only thing that survives is what determine_rs_grids writes
    // back into r_grid/s_grid through the `ref double[4][4]` parameters of sgrid.d:1396.
    // The grid sizes are D's implicit int -> size_t widening (sgrid.d:35 takes size_t);
    // a negative value wraps exactly as D's does, and no corpus row goes there because a
    // wrapped vertex count makes the runtime, not this module, decide the failure.
    auto my_patch = std::make_shared<CoonsPatch>(south, north, west, east);
    ClusterF cf = {std::make_shared<LinearFunction>(), std::make_shared<LinearFunction>(),
                   std::make_shared<LinearFunction>(), std::make_shared<LinearFunction>()};
    StructuredGrid my_grid(*my_patch, static_cast<size_t>(niv), static_cast<size_t>(njv), cf,
                           r_grid, s_grid);
    my_grid.determine_rs_grids(*my_patch, cf, r_grid, s_grid);
} // end constructor()

// D:91-103. No checks and no re-optimisation: the corners come over verbatim and both
// grids are copied BY VALUE (D:101-102 slice-assign over static arrays, which is a
// whole-array copy). Corpus rows gmo.copy.* / gmo.indep.* pin that a later mutation of
// the source's r_grid[1][2] leaves the copy where it was.
GMOPatch::GMOPatch(const GMOPatch& other) {
    this->north = other.north->dup();
    this->east = other.east->dup();
    this->south = other.south->dup();
    this->west = other.west->dup();
    p00 = other.p00;
    p10 = other.p10;
    p01 = other.p01;
    p11 = other.p11;
    r_grid = other.r_grid;
    s_grid = other.s_grid;
} // end constructor()

// D:105-108 — NOT a copy: dup() re-enters the checking constructor over the already
// duped boundaries with the DEFAULT 11x11 test grid, so the parameters are re-optimised
// (gmo.dup55 grids_eq_ut=true, grids_eq_src55=false).
std::shared_ptr<ParametricSurface> GMOPatch::dup() const {
    return std::make_shared<GMOPatch>(this->south, this->north, this->west, this->east);
}

// D:110-135
Vector3 GMOPatch::operator()(double r, double s) const {
    // Set up redistribution function to r,s. (D:112-125) The nested function with `out`
    // parameters becomes a lambda with reference out-parameters, as in
    // sgrid.cpp:904-918's identical remap. Every `^^` is eil::pow and none is expanded
    // into a multiplication (grid brief 9.3 item 2: these cubic weights are
    // bit-observable); no exponent here is 0.5, so T-925's sqrt exception does not apply.
    auto remap = [this](double r, double s, double& r_star, double& s_star) {
        // Tensor-product, cubic-Bezier interpolation of the gridded parameter values.
        std::array<double, 4> rr{}; std::array<double, 4> sr{}; // D:116
        for (size_t j = 0; j < 4; ++j) {
            rr[j] = eil::pow(1.0 - r, 3.0) * r_grid[0][j] +
                    3.0 * r * eil::pow(1.0 - r, 2.0) * r_grid[1][j] +
                    3.0 * (1.0 - r) * r * r * r_grid[2][j] + eil::pow(r, 3.0) * r_grid[3][j];
            sr[j] = eil::pow(1.0 - r, 3.0) * s_grid[0][j] +
                    3.0 * r * eil::pow(1.0 - r, 2.0) * s_grid[1][j] +
                    3.0 * (1.0 - r) * r * r * s_grid[2][j] + eil::pow(r, 3.0) * s_grid[3][j];
        }
        r_star = eil::pow(1.0 - s, 3.0) * rr[0] + 3.0 * s * eil::pow(1.0 - s, 2.0) * rr[1] +
                 3.0 * (1.0 - s) * s * s * rr[2] + eil::pow(s, 3.0) * rr[3];
        s_star = eil::pow(1.0 - s, 3.0) * sr[0] + 3.0 * s * eil::pow(1.0 - s, 2.0) * sr[1] +
                 3.0 * (1.0 - s) * s * s * sr[2] + eil::pow(s, 3.0) * sr[3];
    };
    double rstar = 0.0; double sstar = 0.0; // D:126 declares without initialising
    remap(r, s, rstar, sstar);
    Vector3 south_r = (*south)(rstar);
    Vector3 north_r = (*north)(rstar);
    Vector3 west_s = (*west)(sstar);
    Vector3 east_s = (*east)(sstar);
    Vector3 p = (1.0 - sstar) * south_r + sstar * north_r + (1.0 - rstar) * west_s +
                rstar * east_s -
                ((1.0 - rstar) * (1.0 - sstar) * p00 + (1.0 - rstar) * sstar * p01 +
                 rstar * (1.0 - sstar) * p10 + rstar * sstar * p11);
    return p;
} // end opCall()

// D:137-146. to!string over a Path handle is the path's own toString(); over a
// double[4][4] it is D's nested "[[...], [...]]" at %.6g, which d_tostr reproduces for
// std::array (geom/gpath/path.hpp:128-152).
std::string GMOPatch::toString() const {
    return "GMOPatch(south=" + d_tostr(south) + ", north=" + d_tostr(north) + ", west=" +
           d_tostr(west) + ", east=" + d_tostr(east) + ", r_grid=" + d_tostr(r_grid) +
           ", s_grid=" + d_tostr(s_grid) + ")";
}

// Not a D symbol (see header note).
std::string GMOPatch::classString() const { return "GMOPatch"; }

} // namespace geom
} // namespace eil
