// geom/surface/nurbssurface.d -> src/geom/surface/nurbssurface.cpp
// (module geom.surface.nurbssurface).
//
// Definitions in D source order: the validating constructor (D:21-71), the copy
// constructor (D:72-86), the three overrides (D:87-98) and deBoor (D:119-135). Inside
// deBoor the sequence is exactly D's: findSpan(u), basisFuns(u), findSpan(v),
// basisFuns(v), the temp accumulation loop, THEN the whole-array zeroing of mSw and mS,
// then the mSw accumulation over l, then the division by mSw[3], then Vector3(mS).
// Swapping the zeroing above the accumulation loops would leave the previous call's
// values in mSw and is the reason the order is commented rather than "tidied".
//
// See the header for the non-1:1 notes (the static buffers and the construction-order
// interference, the m_b = other.m_p quirk in the copy constructor and its two measured
// consequences, ErrorBase for the five guards, the size_t comparisons in the guards).
#include "geom/surface/nurbssurface.hpp"

#include <array>
#include <cstddef>
#include <memory>
#include <string>

#include "eil/array.hpp"
#include "eil/conv.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/misc/nurbs_utils.hpp"
#include "geom/surface/parametricsurface.hpp"

namespace eil {
namespace geom {

// D:111-117. One set for the whole process, as in D. The two static arrays start at
// double.init (NaN) exactly as D's `static double[4] mSw` does; deBoor writes every lane
// before reading it.
std::array<double, 4> NURBSSurface::mSw = {eil::d_init<double>(), eil::d_init<double>(),
                                          eil::d_init<double>(), eil::d_init<double>()};
std::array<double, 3> NURBSSurface::mS = {eil::d_init<double>(), eil::d_init<double>(),
                                         eil::d_init<double>()};
eil::Array<double> NURBSSurface::mNu;
eil::Array<double> NURBSSurface::mNv;
eil::Array<std::array<double, 4>> NURBSSurface::temp;
std::unique_ptr<NURBSWorkspace> NURBSSurface::mNws_u;
std::unique_ptr<NURBSWorkspace> NURBSSurface::mNws_v;

// D:21-71.
NURBSSurface::NURBSSurface(const Pw2& Pw, eil::Slice<double> U, int p, eil::Slice<double> V,
                           int q) {
    // D:23-29. Test Pw is minimum viable. D's `||` short-circuits, so Pw[0] is only read
    // when Pw.length >= 2.
    if (Pw.length() < 2 || Pw[0].length() < 2) {
        std::string errMsg =
            "NURBSSurface() A NURBS surface requires at least two control points in each "
            "direction.\n";
        errMsg += eil::format("Supplied number of control points in direction u: %s\n",
                              Pw.length());
        errMsg += eil::format("Supplied number of control points in direction v: %s",
                              Pw[0].length());
        throw eil::ErrorBase(eil::text(errMsg)); // D:28
    }
    // D:30-36. Test that p is valid. D compares int against size_t, so -1 wraps.
    if (static_cast<size_t>(p) > Pw.length() - 1) {
        std::string errMsg =
            "NURBSSurface() Degree of NURBS surface in direction u is not compatible with "
            "given number of control points.\n";
        errMsg += eil::format("Supplied degree in u direction: %s\n", p);
        errMsg += eil::format("Maximum allowed degree in u direction: %s", Pw.length() - 1);
        throw eil::ErrorBase(eil::text(errMsg)); // D:35
    }
    // D:37-43. Test that q is valid. D's message says "direction u" twice here (D:40-41);
    // kept verbatim, because the corpus compares the text.
    if (static_cast<size_t>(q) > Pw[0].length() - 1) {
        std::string errMsg =
            "NURBSSurface() Degree of NURBS surface in direction v is not compatible with "
            "given number of control points.\n";
        errMsg += eil::format("Supplied degree in u direction: %s\n", q);
        errMsg += eil::format("Maximum allowed degree in u direction: %s", Pw[0].length() - 1);
        throw eil::ErrorBase(eil::text(errMsg)); // D:42
    }
    // D:44-51. Test U knot vector is valid.
    this->m_a = eil::to<int>(Pw.length() - 1); // D:45 to!int(Pw.length-1)
    if (U.length() != static_cast<size_t>(m_a + p + 2)) {
        std::string errMsg =
            "NURBS() Knot vector in direction u is not compatible with given number control "
            "points and surface degree.\n";
        errMsg += eil::format("Supplied number of knots in direction u: %s\n", U.length());
        errMsg += eil::format("Required number of knots in direction u: %s", m_a + p + 2);
        throw eil::ErrorBase(eil::text(errMsg)); // D:50
    }
    // D:52-59. Test V knot vector is valid. Again D's first line says "direction u"
    // (D:55) while the two detail lines say v; both kept as written.
    this->m_b = eil::to<int>(Pw[0].length() - 1); // D:53
    if (V.length() != static_cast<size_t>(m_b + q + 2)) {
        std::string errMsg =
            "NURBS() Knot vector in direction u is not compatible with given number control "
            "points and surface degree.\n";
        errMsg += eil::format("Supplied number of knots in direction v: %s\n", V.length());
        errMsg += eil::format("Required number of knots in direction v: %s", m_b + q + 2);
        throw eil::ErrorBase(eil::text(errMsg)); // D:58
    }

    this->mPw = Pw.dup();      // D:61
    this->mU = eil::Array<double>(U); // D:62 U.dup
    this->m_p = p;             // D:63
    this->mV = eil::Array<double>(V); // D:64 V.dup
    this->m_q = q;             // D:65
    mNu.set_length(p + 1);     // D:66 (static, shared: see the header)
    mNws_u = std::make_unique<NURBSWorkspace>(p); // D:67
    mNv.set_length(q + 1);     // D:68
    mNws_v = std::make_unique<NURBSWorkspace>(q); // D:69
    temp.set_length(m_q + 1);  // D:70
}

// D:72-86. The copy constructor: no validation, and D:80 assigns m_b from other.m_p.
// That line is the reference behaviour and is copied; see the header for the two
// measured consequences.
NURBSSurface::NURBSSurface(const NURBSSurface& other) {
    this->mPw = other.mPw.dup(); // D:74
    this->mU = other.mU.dup();   // D:75
    this->m_p = other.m_p;       // D:76
    this->m_a = other.m_a;       // D:77
    this->mV = other.mV.dup();   // D:78
    this->m_q = other.m_q;       // D:79
    this->m_b = other.m_p;       // D:80 — m_p, not m_b. Preserved.
    mNu.set_length(other.m_p + 1);                // D:81
    mNws_u = std::make_unique<NURBSWorkspace>(other.m_p); // D:82
    mNv.set_length(other.m_q + 1);                // D:83
    mNws_v = std::make_unique<NURBSWorkspace>(other.m_q); // D:84
    temp.set_length(m_q + 1);                     // D:85
}

// D:87-90. Through the validating constructor, which re-derives a correct m_b.
std::shared_ptr<ParametricSurface> NURBSSurface::dup() const {
    return std::make_shared<NURBSSurface>(mPw, mU.all(), m_p, mV.all(), m_q);
}

// D:91-94.
Vector3 NURBSSurface::operator()(double u, double v) const { return deBoor(u, v); }

// D:95-98. to!string of the nested net, of the two knot vectors and of the two degrees.
std::string NURBSSurface::toString() const {
    return "NURBSSurface(Pw=" + d_tostr(mPw) + ", U=" + d_tostr(mU) + ", p=" +
           eil::to_string(m_p) + ", V=" + d_tostr(mV) + ", q=" + eil::to_string(m_q) + ")";
}

// D:119-135.
Vector3 NURBSSurface::deBoor(double u, double v) const {
    // Returns the Cartesian coordinates of a point on a NURBS surface at a given set of
    // parameter values
    // This is algorithm A4.3 from Piegl and Tiller (1997) - 'The NURBS Book'
    int uspan = findSpan(u, m_a, m_p, mU.all());                     // D:122
    basisFuns(uspan, u, m_p, mU.all(), mNu.all(), *mNws_u);          // D:123
    int vspan = findSpan(v, m_b, m_q, mV.all());                     // D:124
    basisFuns(vspan, v, m_q, mV.all(), mNv.all(), *mNws_v);          // D:125
    for (int l = 0; l < m_q + 1; ++l) {                              // D:126
        for (size_t e = 0; e < 4; ++e) temp[l][e] = 0.0;             // D:127 temp[l][] = 0
        for (int k = 0; k < m_p + 1; ++k) {                          // D:128
            const std::array<double, 4>& pw =
                mPw[static_cast<size_t>(uspan - m_p + k)]
                   [static_cast<size_t>(vspan - m_q + l)];
            const double w = mNu[static_cast<size_t>(k)];
            for (size_t e = 0; e < 4; ++e) temp[l][e] += w * pw[e];
        }
    }
    for (size_t e = 0; e < mSw.size(); ++e) mSw[e] = 0.0;            // D:130
    for (size_t e = 0; e < mS.size(); ++e) mS[e] = 0.0;              // D:131
    for (int l = 0; l < m_q + 1; ++l) {                              // D:132
        const double w = mNv[static_cast<size_t>(l)];
        for (size_t e = 0; e < 4; ++e) mSw[e] += w * temp[l][e];
    }
    for (int i = 0; i < 3; ++i) mS[i] = mSw[i] / mSw[3];             // D:133
    // D:134 Vector3(mS) reads the three elements (geom/elements/vector3.d:82-93).
    return Vector3(eil::Slice<double>(mS.data(), mS.size(), nullptr, 0));
} // end deBoor()

} // namespace geom
} // namespace eil
