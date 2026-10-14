// geom/gpath/nurbs.d -> src/geom/gpath/nurbs.cpp (module geom.gpath.nurbs).
//
// Definitions in D source order: the two constructors (D:20-59), the four overrides
// (D:60-75), then deBoor (D:87-97). The evaluation order inside deBoor — findSpan,
// basisFuns, zero the two buffers, accumulate, divide by the weight — is D's, and the
// accumulation writes mCw's four elements in index order 0,1,2,3 exactly as D's array
// operation `mCw[] += mN[i]*mPw[...]` does (an array op is element-wise with no
// reassociation, so there is nothing to reorder here).
//
// See the header for the non-1:1 notes (the static buffers and their cross-degree
// interference, the unique_ptr holding the shared workspace, to!int and the unsigned
// comparisons in the guards, ErrorBase vs ExceptionBase).
#include "geom/gpath/nurbs.hpp"

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
#include "geom/gpath/path.hpp"
#include "geom/misc/nurbs_utils.hpp"

namespace eil {
namespace geom {

// D:82-85. One set for the whole process, as in D.
eil::Array<double> NURBS::mN;
std::array<double, 4> NURBS::mCw = {eil::d_init<double>(), eil::d_init<double>(),
                                    eil::d_init<double>(), eil::d_init<double>()};
std::array<double, 3> NURBS::mC = {eil::d_init<double>(), eil::d_init<double>(),
                                   eil::d_init<double>()};
std::unique_ptr<NURBSWorkspace> NURBS::mNws;

// D:20-50.
NURBS::NURBS(eil::Slice<std::array<double, 4>> Pw, eil::Slice<double> U, int p) {
    // Test Pw is minimum viable
    if (Pw.length() < 2) {
        std::string errMsg = "NURBS() A NURBS path requires at least two control points.\n";
        errMsg += eil::format("Supplied number of control points: %s", Pw.length());
        // D:26 `throw new Error(text(errMsg))`. text() of a single string is that
        // string, so the message is errMsg unchanged.
        throw eil::ErrorBase(eil::text(errMsg));
    }
    // Test p is valid
    // D:29 compares int against the size_t Pw.length-1, so D converts p to size_t
    // first; the same conversion is written out here (a negative p becomes huge and
    // passes, as it does in D).
    if (static_cast<size_t>(p) > Pw.length() - 1) {
        std::string errMsg =
            "NURBS() Degree of NURBS path is not compatible with given number of control points.\n";
        errMsg += eil::format("Supplied path degree: %s\n", p);
        errMsg += eil::format("Maximum allowed path degree: %s", Pw.length() - 1);
        throw eil::ErrorBase(eil::text(errMsg)); // D:33
    }
    // Test knot vector is valid
    this->m_a = eil::to<int>(Pw.length() - 1); // D:36 to!int(Pw.length-1)
    // D:37 compares a size_t against int m_a+p+2, i.e. it converts the sum to size_t.
    if (U.length() != static_cast<size_t>(m_a + p + 2)) {
        std::string errMsg =
            "NURBS() Knot vector is not compatible with given number control points and curve degree.\n";
        errMsg += eil::format("Supplied number of knots: %s\n", U.length());
        errMsg += eil::format("Required number of knots: %s", m_a + p + 2);
        throw eil::ErrorBase(eil::text(errMsg)); // D:41
    }

    this->mPw = eil::Array<std::array<double, 4>>(Pw); // D:44 Pw.dup
    this->mU = eil::Array<double>(U);                  // D:45 U.dup
    this->m_p = p;                                     // D:46
    mN.set_length(p + 1);                              // D:47
    // D:47 grows the shared array, which leaves the new slots at double.init (NaN);
    // basisFuns (nurbs_utils.d:58-73) writes N[0..p] before deBoor reads any of them,
    // so the NaN never reaches a value.
    mNws = std::make_unique<NURBSWorkspace>(p);        // D:48
} // end NURBS()

// D:51-59. The copy constructor, which — as in D — runs none of the validation above
// and leaves the object exactly as copied.
NURBS::NURBS(const NURBS& other) {
    this->mPw = other.mPw.dup(); // D:53
    this->mU = other.mU.dup();   // D:54
    this->m_p = other.m_p;       // D:55
    this->m_a = other.m_a;       // D:56
    mN.set_length(other.m_p + 1);                                  // D:57
    mNws = std::make_unique<NURBSWorkspace>(other.m_p);             // D:58
}

// D:60-63. dup() rebuilds through the validating constructor rather than copying, so
// it can throw for a payload the copy constructor would accept — it is built from this
// object's own (already valid) fields, so it cannot.
std::shared_ptr<Path> NURBS::dup() const {
    return std::make_shared<NURBS>(mPw.all(), mU.all(), m_p);
}

// D:64-67.
Vector3 NURBS::operator()(double u) const { return deBoor(u); }

// D:68-71. to!string of a double[4][] and a double[] is D's bracketed array form,
// which is what d_tostr (geom/gpath/path.hpp) renders.
std::string NURBS::toString() const {
    return "NURBS(Pw=" + d_tostr(mPw) + ", U=" + d_tostr(mU) + ", p=" +
           eil::to_string(m_p) + ")";
}

// D:72-75.
std::string NURBS::classString() const { return "NURBS"; }

// D:87-97.
Vector3 NURBS::deBoor(double u) const {
    // Returns the Cartesian coordinates of a point on a NURBS curve at a given
    // parameter value
    // This is algorithm A4.1 from Piegl and Tiller (1997) - 'The NURBS Book'
    int span = findSpan(u, m_a, m_p, mU.all());
    basisFuns(span, u, m_p, mU.all(), mN.all(), *mNws);
    // D:92-93 `mCw[] = 0.0; mC[] = 0.0;` — whole-array fills of the static buffers.
    for (size_t k = 0; k < mCw.size(); ++k) mCw[k] = 0.0;
    for (size_t k = 0; k < mC.size(); ++k) mC[k] = 0.0;
    // D:94 foreach (i; 0 .. m_p+1) mCw[] += mN[i]*mPw[span-m_p+i][];
    for (int i = 0; i < m_p + 1; ++i) {
        const std::array<double, 4>& pw = mPw[static_cast<size_t>(span - m_p + i)];
        const double w = mN[static_cast<size_t>(i)];
        for (size_t k = 0; k < mCw.size(); ++k) mCw[k] += w * pw[k];
    }
    // D:95 foreach (i; 0 .. 3) mC[i] = mCw[i]/mCw[3];
    for (int i = 0; i < 3; ++i) mC[i] = mCw[i] / mCw[3];
    // D:96 Vector3(mC) reads mC[0..3] (geom/elements/vector3.d:82-93).
    return Vector3(eil::Slice<double>(mC.data(), mC.size(), nullptr, 0));
} // end deBoor()

} // namespace geom
} // namespace eil
