// geom/volume/tfivolume.d -> src/geom/volume/tfivolume.cpp
// (module geom.volume.tfivolume) — see tfivolume.hpp for the import substitutions and
// the non-1:1 notes. This TU carries the six members declared there: the two validating
// constructors (D:24-86, D:91-102), the copy constructor (D:107-111), dup() (D:113-116),
// opCall (D:118-138), toString() (D:140-146) and the private checkVolume() (D:149-161).

#include "geom/volume/tfivolume.hpp"

#include <array>
#include <cstddef>
#include <memory>
#include <string>

#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/number.hpp"
#include "geom/elements/nomenclature.hpp"
#include "geom/elements/properties.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/geometry_exception.hpp"
#include "geom/gpath/path.hpp"
#include "geom/surface/coonspatch.hpp"
#include "geom/surface/parametricsurface.hpp"
#include "geom/volume/parametricvolume.hpp"

namespace {

using eil::geom::ParametricSurface;
using eil::geom::Vector3;

// D's `to!string(classHandle)` calls the object's virtual toString(), and a null class
// handle prints the literal "null" (measured: tvi.nullFace.str keeps
// "ProbeSurface(...), null, ..." in the faces list). Same spelling as
// src/geom/volume/wedgevolume.cpp:24-26 and twosurfacevolume.cpp:22-24.
std::string tostr(const ParametricSurface::Ptr& s) {
    return s ? s->toString() : std::string("null");
}

// D:33-34 etc. — `text("TFIVolume open at corner N p= ", <payload>, " p_alt1= ", ...,
// " p_alt2= ", ...)`. text() is plain concatenation of to!string of each argument, and
// a Vector3 renders as Vector3(1, 2.2, 3) through the %.6g form (wave-02-brief
// section 5). Corners 2 (D:47) and 3 (D:54) pass the WHOLE ARRAY `p`, not `p[k]`, and
// d_tostr renders a static array as "[e0, e1, ...]"; those two rows are where this
// module's Vector3.init (NaN per field) becomes observable.
std::string open_corner_text_1(const char* head, const Vector3& corner, const Vector3& alt1,
                               const Vector3& alt2) {
    return eil::text(head, " p= ", eil::geom::d_tostr(corner), " p_alt1= ",
                     eil::geom::d_tostr(alt1), " p_alt2= ", eil::geom::d_tostr(alt2));
}
std::string open_corner_text_arr(const char* head, const std::array<Vector3, 8>& pts,
                                 const Vector3& alt1, const Vector3& alt2) {
    return eil::text(head, " p= ", eil::geom::d_tostr(pts), " p_alt1= ",
                     eil::geom::d_tostr(alt1), " p_alt2= ", eil::geom::d_tostr(alt2));
}

} // namespace

namespace eil {
namespace geom {

// -----------------------------------------------------------------------------
// D:24-86 — the generic, 6-face constructor.
//
// Statement order is D's and every one of the eight blocks is written out separately,
// because the FIRST failing corner is the only one observable and the corner indices
// arrive in an order that is neither NESW nor the box constructor's assignment order
// (bottom, west, south / bottom, east, south / bottom, east, north / ...).
//
// D:27 `foreach(i; 0 .. 6) faces[i] = faceArray[i].dup();` — the loop bound is the
// literal 6, not `$`, so a LONGER slice is silently truncated while a shorter one dies
// in the dup loop with a D bounds Error before a single corner is computed (measured:
// tvi.shortArray raises "index [5] is out of bounds for array of length 5" and
// tvi.longArray is accepted). Slice::operator[] gives exactly that text and raises
// eil::RangeError, which is an ErrorBase — a D Error, so the same sibling escapes.
// -----------------------------------------------------------------------------
TFIVolume::TFIVolume(FaceSlice faceArray) {
    for (size_t i = 0; i < 6; ++i) { // D:27
        faces[i] = faceArray[i]->dup();
    }
    // Check that the corners of the faces coincide. (D:28)
    { // corner 0 (D:29-35)
        p[0] = (*faces[bottom])(0.0, 0.0);
        Vector3 p_alt1 = (*faces[west])(0.0, 0.0);
        Vector3 p_alt2 = (*faces[south])(0.0, 0.0);
        if (!approxEqualVectors(p[0], p_alt1) || !approxEqualVectors(p[0], p_alt2)) {
            throw eil::ErrorBase(open_corner_text_1("TFIVolume open at corner 0", p[0],
                                                    p_alt1, p_alt2),
                                 __FILE__, __LINE__);
        }
    }
    { // corner 1 (D:36-42)
        p[1] = (*faces[bottom])(1.0, 0.0);
        Vector3 p_alt1 = (*faces[east])(0.0, 0.0);
        Vector3 p_alt2 = (*faces[south])(1.0, 0.0);
        if (!approxEqualVectors(p[1], p_alt1) || !approxEqualVectors(p[1], p_alt2)) {
            throw eil::ErrorBase(open_corner_text_1("TFIVolume open at corner 1", p[1],
                                                    p_alt1, p_alt2),
                                 __FILE__, __LINE__);
        }
    }
    { // corner 2 (D:43-49) — prints the whole p array, D:47
        p[2] = (*faces[bottom])(1.0, 1.0);
        Vector3 p_alt1 = (*faces[east])(1.0, 0.0);
        Vector3 p_alt2 = (*faces[north])(1.0, 0.0);
        if (!approxEqualVectors(p[2], p_alt1) || !approxEqualVectors(p[2], p_alt2)) {
            throw eil::ErrorBase(open_corner_text_arr("TFIVolume open at corner 2", p, p_alt1,
                                                      p_alt2),
                                 __FILE__, __LINE__);
        }
    }
    { // corner 3 (D:50-56) — prints the whole p array, D:54
        p[3] = (*faces[bottom])(0.0, 1.0);
        Vector3 p_alt1 = (*faces[west])(1.0, 0.0);
        Vector3 p_alt2 = (*faces[north])(0.0, 0.0);
        if (!approxEqualVectors(p[3], p_alt1) || !approxEqualVectors(p[3], p_alt2)) {
            throw eil::ErrorBase(open_corner_text_arr("TFIVolume open at corner 3", p, p_alt1,
                                                      p_alt2),
                                 __FILE__, __LINE__);
        }
    }
    { // corner 4 (D:57-63)
        p[4] = (*faces[top])(0.0, 0.0);
        Vector3 p_alt1 = (*faces[west])(0.0, 1.0);
        Vector3 p_alt2 = (*faces[south])(0.0, 1.0);
        if (!approxEqualVectors(p[4], p_alt1) || !approxEqualVectors(p[4], p_alt2)) {
            throw eil::ErrorBase(open_corner_text_1("TFIVolume open at corner 4", p[4],
                                                    p_alt1, p_alt2),
                                 __FILE__, __LINE__);
        }
    }
    { // corner 5 (D:64-70)
        p[5] = (*faces[top])(1.0, 0.0);
        Vector3 p_alt1 = (*faces[east])(0.0, 1.0);
        Vector3 p_alt2 = (*faces[south])(1.0, 1.0);
        if (!approxEqualVectors(p[5], p_alt1) || !approxEqualVectors(p[5], p_alt2)) {
            throw eil::ErrorBase(open_corner_text_1("TFIVolume open at corner 5", p[5],
                                                    p_alt1, p_alt2),
                                 __FILE__, __LINE__);
        }
    }
    { // corner 6 (D:71-77)
        p[6] = (*faces[top])(1.0, 1.0);
        Vector3 p_alt1 = (*faces[east])(1.0, 1.0);
        Vector3 p_alt2 = (*faces[north])(1.0, 1.0);
        if (!approxEqualVectors(p[6], p_alt1) || !approxEqualVectors(p[6], p_alt2)) {
            throw eil::ErrorBase(open_corner_text_1("TFIVolume open at corner 6", p[6],
                                                    p_alt1, p_alt2),
                                 __FILE__, __LINE__);
        }
    }
    { // corner 7 (D:78-84)
        p[7] = (*faces[top])(0.0, 1.0);
        Vector3 p_alt1 = (*faces[west])(1.0, 1.0);
        Vector3 p_alt2 = (*faces[north])(0.0, 1.0);
        if (!approxEqualVectors(p[7], p_alt1) || !approxEqualVectors(p[7], p_alt2)) {
            throw eil::ErrorBase(open_corner_text_1("TFIVolume open at corner 7", p[7],
                                                    p_alt1, p_alt2),
                                 __FILE__, __LINE__);
        }
    }
    checkVolume(); // D:85
} // end generic constructor

// Wire-Frame constructor TODO (D:88) — absent from the reference, so absent here.

// -----------------------------------------------------------------------------
// D:91-102 — the simple-box constructor. The parameter is named `p` and SHADOWS the
// member `p` exactly as in D, so D:93 writes `this.p[i] = p[i].dup()` (member on the
// left, parameter on the right) while D:94-99 read the PARAMETER. Both spellings are
// kept. D's bounds check is in the same place: `foreach(i; 0 .. 8)` with the literal 8,
// so a 7-point slice dies at index [7] (tvi.shortPts) and a longer one would be
// truncated rather than rejected.
// -----------------------------------------------------------------------------
TFIVolume::TFIVolume(PointSlice p) {
    for (size_t i = 0; i < 8; ++i) { // D:93
        this->p[i] = p[i].dup();
    }
    // The patch corners are paired as D pairs them: CoonsPatch(p00, p10, p11, p01).
    faces[north] = std::make_shared<CoonsPatch>(p[3], p[2], p[6], p[7]); // D:94
    faces[south] = std::make_shared<CoonsPatch>(p[0], p[1], p[5], p[4]); // D:95
    faces[east] = std::make_shared<CoonsPatch>(p[1], p[2], p[6], p[5]);  // D:96
    faces[west] = std::make_shared<CoonsPatch>(p[0], p[3], p[7], p[4]);  // D:97
    faces[top] = std::make_shared<CoonsPatch>(p[4], p[5], p[6], p[7]);   // D:98
    faces[bottom] = std::make_shared<CoonsPatch>(p[0], p[1], p[2], p[3]); // D:99
    // Don't bother testing for open corners. (D:100)
    checkVolume(); // D:101
} // end constructor

// Surface-extrusion constructor TODO (D:104).

// -----------------------------------------------------------------------------
// D:107-111 — the copy constructor. It dups every face and every corner and does NOT
// re-run checkVolume(), which is what makes it differ from dup() (D:113-116), and the
// corpus measures the difference: tvi.faces.copy.* and tvi.faces.dup.* print the same
// text for the same source but take different code paths.
// -----------------------------------------------------------------------------
TFIVolume::TFIVolume(const TFIVolume& other) {
    for (size_t i = 0; i < 6; ++i) { // D:109
        this->faces[i] = other.faces[i]->dup();
    }
    for (size_t i = 0; i < 8; ++i) { // D:110 — Vector3.dup() of a struct is a copy
        this->p[i] = other.p[i].dup();
    }
} // end copy constructor

// D:113-116. `new TFIVolume(this.faces)` re-enters the VALIDATING 6-face constructor,
// so a volume accepted by the box constructor is corner-checked again on every dup —
// measured by tvi.swap.dup, which throws "TFIVolume open at corner 1" on an object the
// box constructor never checked. The static_cast to FaceSlice mirrors D's implicit
// static-array-to-slice conversion of the 6-element faces member.
std::shared_ptr<ParametricVolume> TFIVolume::dup() const {
    return std::make_shared<TFIVolume>(
        FaceSlice(const_cast<ParametricSurface::Ptr*>(faces.data()), faces.size(), nullptr, 0));
} // end dup

// -----------------------------------------------------------------------------
// D:118-138 — blend the six face evaluations against the trilinear corner box.
//
// Operand order matters: BigC (D:134-135) is one expression of eight Vector3 sums, and
// p_rst (D:136) is `0.5*(...) - 0.5*BigC`, so each term of the bracket is a
// scalar*Vector3 product computed in that order. `0.5*BigC` is a LEFT-hand scalar
// multiply, which is eil::geom's opBinaryRight (vector3.hpp:117), not BigC*0.5.
// -----------------------------------------------------------------------------
Vector3 TFIVolume::operator()(double r, double s, double t) const {
    Vector3 pW = (*faces[west])(s, t);    // D:127
    Vector3 pE = (*faces[east])(s, t);    // D:128
    Vector3 pS = (*faces[south])(r, t);   // D:129
    Vector3 pN = (*faces[north])(r, t);   // D:130
    Vector3 pB = (*faces[bottom])(r, s);  // D:131
    Vector3 pT = (*faces[top])(r, s);     // D:132
    double omr = 1.0 - r;                 // D:133
    double oms = 1.0 - s;
    double omt = 1.0 - t;
    Vector3 BigC = (omr * oms * omt) * p[0] + (omr * oms * t) * p[4] // D:134-135
                   + (omr * s * omt) * p[3] + (omr * s * t) * p[7] + (r * oms * omt) * p[1]
                   + (r * oms * t) * p[5] + (r * s * omt) * p[2] + (r * s * t) * p[6];
    Vector3 p_rst = 0.5 * (omr * pW + r * pE + oms * pS + s * pN + omt * pB + t * pT) // D:136
                    - 0.5 * BigC;
    return p_rst;
} // end opCall

// D:140-146. Three steps: an initial `~` concatenation, a `foreach(i; 1 .. 6)` of
// `repr ~= ", " ~ to!string(faces[i])`, then `repr ~= "])"`. It indexes faces
// positionally, so the print order is the FACE INDEX order west, east, south, north,
// bottom, top (geom/elements/nomenclature.d:12-19), which is NOT the order either
// constructor assigns them — the reason tvi.faces.str and tvi.textSame are observable.
std::string TFIVolume::toString() const {
    std::string repr = "TFIVolume(faces=[" + tostr(faces[0]); // D:142
    for (size_t i = 1; i < 6; ++i) {                          // D:143 foreach(i; 1 .. 6)
        repr += ", " + tostr(faces[i]);
    }
    repr += "])"; // D:144
    return repr;
} // end toString

// -----------------------------------------------------------------------------
// D:149-161 — private, non-const. The four locals are number.init, i.e. NaN (D:153),
// and `Vector3 centroid` is Vector3.init (D:154); hex_cell_properties assigns all five
// before they are read, but the port keeps the NaN initialisation so an unread path
// shows the same NaN instead of an indeterminate value.
//
// `format("...(%g)...", volume)` is D's std.format %g, which is 6 significant digits by
// default: the measured rows are "(0)" for an exactly-flat block and "(-3.1079)" for the
// inside-out one, i.e. the same digits the observable `%.17g` dump rounds to 6 — not the
// C library's %g, which would print the same bytes here but not for a NaN.
//
// The throwable is geom.geometry_exception.GeometryException, an **Exception** (D
// derives it from Exception), so a ported `catch (ExceptionBase)` takes it while the
// eight open-corner guards above (D's `new Error`) do not — measured by the
// tvi.catch.flat.asException / tvi.catch.corner.asError pair.
// -----------------------------------------------------------------------------
void TFIVolume::checkVolume() {
    // Compute the volume of the hexahedron, as defined by the corners of the block.
    // This should give a rough and ready test for an ill-defined block. (D:151-152)
    number volume = eil::d_init<number>(), iLength = eil::d_init<number>(),
           jLength = eil::d_init<number>(), kLength = eil::d_init<number>();
    Vector3 centroid;
    hex_cell_properties(p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7], true, centroid, volume,
                        iLength, jLength, kLength); // D:155-156, positional
    if (volume <= 0.0) {                             // D:157
        std::string msg = eil::format("Apparently-invalid volume (%g) for hex block.", volume);
        EIL_THROW_GEOM(msg); // D:159
    }
} // end checkVolume

} // namespace geom
} // namespace eil
