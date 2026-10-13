// geom/surface/nozzleexpansionpatch.d -> src/geom/surface/nozzleexpansionpatch.cpp
// (module geom.surface.nozzleexpansionpatch).
//
// Definitions in D source order: the two constructors (D:38-46), dup (D:48-51), opCall
// (D:53-128), toString (D:130-133). Inside opCall the statement order is D's: the four
// branch tests, then pNW/pNE, north_length, the wall point, eps/wall_angle, the three
// finite-difference cases, the 2% then 98% tweaks, the mid point, the axis point, and
// finally the s == 0 test before the Bezier evaluation. Nothing is hoisted or cached:
// D re-evaluates cNorth(r) at D:76, :77, :83(x2), :84(x2), :87(x2), :88(x2), :91(x4),
// :92(x4), :101, :102, :108, :109, :113, and the port keeps every one of those calls
// (the Path operators are pure, so the count cannot change a value — but folding them
// would stop the port reading like the reference).
//
// The arithmetic order of the Bezier rows (D:123-124) is left-to-right as D spells it:
// (1-s)*(1-s)*axis_pt_x + 2*s*(1-s)*mid_pt_x + s*s*wall_pt_x is
// ((1-s)*(1-s))*axis_pt_x + ((2*s)*(1-s))*mid_pt_x + (s*s)*wall_pt_x in both languages;
// it is NOT rewritten as the Bernstein form (1-s)^2 + 2s(1-s) + s^2 with shared
// sub-expressions, which would round differently.
#include "geom/surface/nozzleexpansionpatch.hpp"

#include <cmath>
#include <limits>
#include <memory>
#include <string>

#include "eil/math.hpp"
#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"
#include "geom/surface/parametricsurface.hpp"

namespace eil {
namespace geom {

namespace {

// Phobos' poly() for a static coefficient array (std/math/algebraic.d:557-566): Horner
// from the top, `r = A[N-1]; repeat { r *= x; r += A[N-1-i]; }`.
template <size_t N> double d_poly(double x, const double (&A)[N]) {
    double r = A[N - 1];
    for (size_t i = 1; i < N; ++i) {
        r *= x;
        r += A[N - 1 - i];
    }
    return r;
}

} // namespace

// std.math.tan(double) is Phobos' own tanImpl (trigonometry.d:194 -> :326), not libm's,
// and the two disagree by 1 ULP at arguments the nozzle actually reaches (see the header
// note). Translated statement for statement from the ieeeDouble branch: the special
// cases (:402-406), the sign split (:408-413), the octant reduction (:416-436), the
// zero/singularity mapping (:438-442), the argument split and rational form (:444-456)
// and the quadrant flip (:458-461).
double d_tan(double x) {
    // Coefficients for tan(x) and PI/4 split into three parts
    // (trigonometry.d:366-378, ieeeExtended||ieeeDouble branch; D writes them as 80-bit
    // literals that convert to double exactly as the decimal literals below round).
    static const double P[3] = {-1.7956525197648487798769E7, 1.1535166483858741613983E6,
                                -1.3093693918138377764608E4};
    static const double Q[5] = {-5.3869575592945462988123E7, 2.5008380182335791583922E7,
                                -1.3208923444021096744731E6, 1.3681296347069295467845E4,
                                1.0000000000000000000000E0};
    static const double P1 = 7.853981554508209228515625E-1;
    static const double P2 = 7.946627356147928367136046290398E-9;
    static const double P3 = 3.061616997868382943065164830688E-17;

    // Special cases (D:402-406). NaN returns its argument unchanged, sign included;
    // infinity returns T.nan, which for double is the POSITIVE quiet NaN.
    if (x == 0.0 || std::isnan(x)) return x;
    if (std::isinf(x)) return std::numeric_limits<double>::quiet_NaN();

    bool sign = false;
    if (std::signbit(x)) { // D:408-413
        sign = true;
        x = -x;
    }

    // Compute x mod PI/4 (D:415-436, non-single branch). PI_4 is an 80-bit constant that
    // reaches a double through the cast, so the same "wide then narrow" rounding is kept.
    const double pi_4 = static_cast<double>(EIL_PI / 4.0L);
    double y = std::floor(x / pi_4);
    const double highBitsFactor = 0x1p3; // 8.0, D's ieeeDouble choice
    const double highBitsInv = 1.0 / highBitsFactor;
    double z = y * highBitsInv;
    z = y - highBitsFactor * std::floor(z);
    int j = static_cast<int>(z);

    // Map zeros and singularities to origin (D:438-442).
    if (j & 1) {
        j += 1;
        y += 1.0;
    }

    z = ((x - y * P1) - y * P2) - y * P3;
    const double zz = z * z;
    const double zzThreshold = 1.0e-14; // D:445, the ieeeDouble choice
    if (zz > zzThreshold) {
        y = z + z * (zz * d_poly(zz, P) / d_poly(zz, Q)); // D:452
    } else {
        y = z;
    }

    if (j & 2) y = (-1.0) / y; // D:458-460
    return sign ? -y : y;      // D:461
}

// D:38-41. The edge is dups'd, so the patch owns it.
NozzleExpansionPatch::NozzleExpansionPatch(const PathPtr& cNorth) : cNorth(cNorth->dup()) {}

// D:43-46.
NozzleExpansionPatch::NozzleExpansionPatch(const NozzleExpansionPatch& other) {
    cNorth = other.cNorth->dup();
}

// D:48-51.
std::shared_ptr<ParametricSurface> NozzleExpansionPatch::dup() const {
    return std::make_shared<NozzleExpansionPatch>(cNorth);
}

// D:53-128.
Vector3 NozzleExpansionPatch::operator()(double r, double s) const {
    Vector3 p; // D:55 — Vector3.init (NaN) for x and y until a branch writes them.
    p.z = 0.0; // D:56
    if (r == 0.0) {
        // D:57-60 East edge
        p.x = (*cNorth)(0.0).x;
        p.y = s * (*cNorth)(0.0).y;
    } else if (r == 1.0) {
        // D:61-64 West edge
        p.x = (*cNorth)(1.0).x;
        p.y = s * (*cNorth)(1.0).y;
    } else if (s == 1.0) {
        // D:65-68 North edge
        p.x = (*cNorth)(r).x;
        p.y = (*cNorth)(r).y;
    } else {
        // D:69-125 Any other point, including the south edge.
        auto pNW = (*cNorth)(0.0);
        auto pNE = (*cNorth)(1.0);
        auto north_length = pNE.x - pNW.x;
        // Wall point (Bezier control point at s=1)
        auto wall_pt_x = (*cNorth)(r).x;
        auto wall_pt_y = (*cNorth)(r).y;
        // Angle perpendicular to the wall at wall point
        double eps = 0.0001;
        number wall_angle = 0.0;
        if (r < eps) {
            // Near west boundary, use one-sided forward difference. (D:81-84)
            wall_angle = eil::atan(((*cNorth)(r + eps).y - (*cNorth)(r).y) /
                                   ((*cNorth)(r + eps).x - (*cNorth)(r).x));
        } else if (1.0 - r < eps) {
            // Near east bounary, use one-sided backward difference. (D:85-88)
            wall_angle = eil::atan(((*cNorth)(r).y - (*cNorth)(r - eps).y) /
                                   ((*cNorth)(r).x - (*cNorth)(r - eps).x));
        } else {
            // Away from either boundary, use central difference. (D:89-92)
            wall_angle = eil::atan(((*cNorth)(r + eps).y - (*cNorth)(r - eps).y) /
                                   ((*cNorth)(r + eps).x - (*cNorth)(r - eps).x));
        }
        // The 2% starting-region tweak (D:94-104).
        if (((*cNorth)(r).x - pNW.x) <= (0.02 * north_length)) {
            auto scale = ((*cNorth)(r).x - pNW.x) / (0.02 * north_length);
            wall_angle *= scale;
        }
        // The 98% exit-region tweak (D:105-111).
        if (((*cNorth)(r).x - pNW.x) >= (0.98 * north_length)) {
            auto scale = (pNE.x - (*cNorth)(r).x) / (0.02 * north_length);
            wall_angle *= scale;
        }
        // Mid point (Bezier control point at s=0.5). (D:112-114)
        auto mid_pt_y = (*cNorth)(r).y / 2.0;
        auto mid_pt_x = wall_pt_x + (mid_pt_y * d_tan(wall_angle));
        // Axis point (Bezier control point at s=0). (D:115-117)
        auto axis_pt_x = mid_pt_x;
        auto axis_pt_y = 0.0;
        if (s == 0.0) {
            p.x = axis_pt_x;
            p.y = axis_pt_y;
        } else {
            // Generate point on quadratic Bezier curve. (D:122-125)
            p.x = (1 - s) * (1 - s) * axis_pt_x + 2 * s * (1 - s) * mid_pt_x + s * s * wall_pt_x;
            p.y = (1 - s) * (1 - s) * axis_pt_y + 2 * s * (1 - s) * mid_pt_y + s * s * wall_pt_y;
        }
    }
    return p;
} // end opCall()

// D:130-133.
std::string NozzleExpansionPatch::toString() const {
    return "NozzleExpansionPatch(cNorth=" + d_tostr(cNorth) + ")";
}

} // namespace geom
} // namespace eil
