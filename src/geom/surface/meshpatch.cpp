// geom/surface/meshpatch.d -> src/geom/surface/meshpatch.cpp
// (module geom.surface.meshpatch)

#include "geom/surface/meshpatch.hpp"

#include <cmath>
#include <cstddef>
#include <memory>
#include <string>

#include "eil/array.hpp"
#include "eil/error.hpp"
#include "geom/grid/sgrid.hpp"

namespace eil {
namespace geom {

namespace {

// D:81, D:82 `to!int(r / dr)`. Phobos' floating -> integer conversion RAISES outside the
// target range rather than wrapping, and it range-checks the FLOATING value, not the
// truncated one. Measured with ldc2 1.40.0 against std/conv.d:1555-1574 and recorded in
// baselines/e587b235/probes/geom_surface_meshpatch.txt:
//
//   NaN                 std.conv.ConvException          msg=[Input was NaN]       row mp.oc.r.nan / mp.oc.s.nan
//   v >  2147483647.0   std.conv.ConvOverflowException  msg=[Conversion positive overflow]  mp.oc.r.pinf / mp.oc.r.big
//   v < -2147483648.0   std.conv.ConvOverflowException  msg=[Conversion negative overflow]  mp.oc.r.ninf / mp.oc.s.big
//   otherwise           cast(int), i.e. truncated toward zero
//
// The two boundaries are the extremes themselves and they are INCLUSIVE on the good side:
// measured separately (off-corpus, same compiler) because the difference is not cosmetic —
//   to!int(2147483647.0)   -> 2147483647          to!int(-2147483648.0) -> -2147483648
//   to!int(2147483647.5)   -> THROWS positive     to!int(-2147483648.5) -> THROWS negative
//   to!int(2147483646.5)   -> 2147483646          to!int(-0.9999)       -> 0
// so a test of `v >= 2147483648.0` would silently accept 2147483647.5 where D raises. (For
// the record: src/util/lua_service.hpp:169 has the mirror-image mistake, `v >= INT_MAX`,
// which raises at 2147483647.0 where D converts. Reported, not fixed: that file is not this
// stage's, its path is a Lua number rather than this one, and T-928 already notes the two
// are different D routes that must not be merged.)
//
// SUBSTRATE GAP, reported rather than fixed here (backlog T-928: "no
// std.conv.ConvOverflowException"): substrate declares the parent
// eil::ConversionException (= std.conv.ConvException, error.hpp:142) only, so both D
// classes arrive as the parent carrying D's exact message, and the replay distinguishes
// them by that message. src/geom/grid/usgrid.hpp:96 declares a second local copy of the
// overflow child for the string -> int case; this file does not include it, because
// pulling a grid header into a surface module to reuse a shim would widen the dependency
// for no evidence gain.
int d_to_int(double v) {
    if (std::isnan(v)) throw eil::ConversionException("Input was NaN");
    if (v > 2147483647.0) throw eil::ConversionException("Conversion positive overflow");
    if (v < -2147483648.0) throw eil::ConversionException("Conversion negative overflow");
    return static_cast<int>(v); // D's to!int truncates toward zero
}

// D:81-82, D:82's mirror. The clamp order is D's: max against 0, then min against the
// last cell index. The second clamp is UNSIGNED because niv/njv are size_t (D:18-19):
// measured with niv == 1, `min(0, niv-2)` leaves the index at 0 because niv-2 wraps to
// 18446744073709551614, and `min(7, 1-2)` leaves it at 7 (probe_meshpatch.d's note on
// rows mp.niv1.opcall / mp.grid1d.opcall, which blame D:89 and D:88 — one past a mesh
// of length 1 — and not the mesh[i][j] term at D:87 a signed clamp would have reached
// with i = -1).
size_t clamp_index(int i, size_t n) {
    const int non_neg = i < 0 ? 0 : i; // D:81 `i = max(0, i)`, both operands int
    const size_t last = n - 2;         // D:81 `niv-2`, size_t arithmetic
    // D's min is `a < b ? cast(CT)a : cast(CT)b` with CT = ulong, so the compare is
    // unsigned and the tie goes to the limit (same value either way).
    return (static_cast<size_t>(non_neg) < last) ? static_cast<size_t>(non_neg)
                                                : static_cast<size_t>(last);
}

} // namespace

// D:23-39. A grid with more than one cell through the thickness is not a mesh this
// surface can interpolate, and D says so with a bare Error (no site, T-922).
MeshPatch::MeshPatch(StructuredGrid& grid) {
    if (grid.nkv != 1) {
        throw eil::ErrorBase("MeshPatch expected a grid with nkv == 1");
    }
    // Keep our own copy of the underlying grid.
    niv = grid.niv;
    njv = grid.njv;
    mesh.set_length(niv); // D:31 — niv EMPTY rows, each grown by D:33's `~=`
    for (size_t i = 0; i < niv; ++i) {
        for (size_t j = 0; j < njv; ++j) { mesh[i].append(*grid.opIndex(i, j)); }
    }
    p00 = mesh[0][0];          // D:35
    p10 = mesh[niv - 1][0];    // D:36
    p01 = mesh[0][njv - 1];    // D:37
    p11 = mesh[niv - 1][njv - 1]; // D:38
} // end constructor()

// D:41-53. njv comes from row 0 alone (D:44), which is what makes a ragged lattice
// either truncate (mp.v.raggedlong) or raise inside the copy loop (mp.v.raggedshort).
MeshPatch::MeshPatch(const eil::Array<eil::Array<Vector3>>& grid) {
    niv = grid.length();
    njv = grid[0].length(); // D:44 — an empty lattice dies right here (mp.v.empty)
    mesh.set_length(niv);
    for (size_t i = 0; i < niv; ++i) {
        for (size_t j = 0; j < njv; ++j) { mesh[i].append(grid[i][j]); }
    }
    p00 = grid[0][0];          // D:49
    p10 = grid[niv - 1][0];    // D:50
    p01 = grid[0][njv - 1];    // D:51
    p11 = grid[niv - 1][njv - 1]; // D:52
} // end constructor()

// D:55-67. The corners are copied rather than re-derived (D:63-66), so a patch whose
// corners were hand-edited copies the edits.
MeshPatch::MeshPatch(const MeshPatch& other) {
    niv = other.niv;
    njv = other.njv;
    mesh.set_length(niv);
    for (size_t i = 0; i < niv; ++i) {
        for (size_t j = 0; j < njv; ++j) { mesh[i].append(other.mesh[i][j]); }
    }
    p00 = other.p00;
    p10 = other.p10;
    p01 = other.p01;
    p11 = other.p11;
} // end constructor()

// D:69-72 — back through the Vector3[][] constructor, from this object's own mesh.
std::shared_ptr<ParametricSurface> MeshPatch::dup() const {
    return std::make_shared<MeshPatch>(mesh);
}

// D:74-92. Interpolate within the background mesh: find the cell, then bilinear inside
// it. dr/ds are 1/(n-1) and D does not protect that division — a one-vertex-wide
// lattice gets an Infinity, which the index clamp then swallows (see the note on
// clamp_index) and the mesh read afterwards does not.
Vector3 MeshPatch::operator()(double r, double s) const {
    const double dr = 1.0 / (static_cast<double>(niv) - 1.0); // D:79
    const double ds = 1.0 / (static_cast<double>(njv) - 1.0); // D:80
    const size_t i = clamp_index(d_to_int(r / dr), niv);      // D:81
    const size_t j = clamp_index(d_to_int(s / ds), njv);      // D:82
    // Parametric coordinate within the coarse cell (D:84-85).
    const double local_r = (r - dr * static_cast<double>(i)) / dr;
    const double local_s = (s - ds * static_cast<double>(j)) / ds;
    // BiLinear interpolation (D:87-90). The four terms are hand-sequenced because each
    // one indexes the mesh and D's failure depends on WHICH term reaches first: with
    // njv == 1 the second term (D:88) raises, with niv == 1 the third (D:89) does, and
    // rows mp.grid1d.opcall / mp.niv1.opcall say so by line.
    Vector3 t0 = (1.0 - local_r) * (1.0 - local_s) * mesh[i][j];           // D:87
    Vector3 t1 = (1.0 - local_r) * local_s * mesh[i][j + 1];               // D:88
    Vector3 t2 = local_r * (1.0 - local_s) * mesh[i + 1][j];               // D:89
    Vector3 t3 = local_r * local_s * mesh[i + 1][j + 1];                   // D:90
    return t0 + t1 + t2 + t3;
} // end opCall()

// D:94-99 — the mesh is not rendered; the result is the constant "MeshPatch()".
std::string MeshPatch::toString() const { return "MeshPatch()"; }

// Not a D symbol (see header note).
std::string MeshPatch::classString() const { return "MeshPatch"; }

} // namespace geom
} // namespace eil
