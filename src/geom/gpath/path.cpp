// geom/gpath/path.d -> src/geom/gpath/path.cpp (module geom.gpath.path).
//
// Definitions in D source order. The header records the non-1:1 points; the two
// that show up in the bodies below are the namespace-qualified call to the free
// intersect2D (D:142) and the capturing lambda that replaces D:154's nested
// function.

#include "geom/gpath/path.hpp"

#include <cmath>
#include <fstream>
#include <string>
#include <vector>

#include "eil/error.hpp"
#include "geom/elements/projection.hpp"
#include "geom/gpath/line.hpp"
#include "nm/linesearch.hpp"

namespace eil {
namespace geom {

// D:34-55
Vector3 Path::dpdt(double t) const {
    // Obtain the derivative approximately, via a finite-difference.
    double dt = 0.001;
    Vector3 p0 = (*this)(t);
    Vector3 derivative;
    if (t + dt > 1.0) {
        // t is close to the t=1.0 boundary, use a one-sided difference.
        Vector3 pminus1 = (*this)(t - dt);
        derivative = (p0 - pminus1) / dt;
    } else if (t - dt < 0.0) {
        // s is close to the s=0 boundary, use a one-sided difference.
        Vector3 pplus1 = (*this)(t + dt);
        derivative = (pplus1 - p0) / dt;
    } else {
        // Not near a boundary, use central-difference.
        Vector3 pminus1 = (*this)(t - dt);
        Vector3 pplus1 = (*this)(t + dt);
        derivative = (pplus1 - pminus1) / (2.0 * dt);
    }
    return derivative;
}

// D:56-79
Vector3 Path::d2pdt2(double t) const {
    // Obtain the derivative approximately, via a finite-difference.
    double dt = 0.001;
    Vector3 p0 = (*this)(t);
    Vector3 derivative;
    if (t + dt > 1.0) {
        // t is close to the t=1.0 boundary, use a one-sided difference.
        Vector3 pminus1 = (*this)(t - dt);
        Vector3 pminus2 = (*this)(t - 2 * dt);
        derivative = (p0 - 2 * pminus1 + pminus2) / (dt * dt);
    } else if (t - dt < 0.0) {
        // s is close to the s=0 boundary, use a one-sided difference.
        Vector3 pplus1 = (*this)(t + dt);
        Vector3 pplus2 = (*this)(t + 2 * dt);
        derivative = (pplus2 - 2 * pplus1 + p0) / (dt * dt);
    } else {
        // Not near a boundary, use central-difference.
        Vector3 pminus1 = (*this)(t - dt);
        Vector3 pplus1 = (*this)(t + dt);
        derivative = (pplus1 - 2 * p0 + pminus1) / (dt * dt);
    }
    return derivative;
}

// D:80-97
number Path::partial_length(double ta, double tb) const {
    if (tb < ta) {
        double tmp = ta; ta = tb; tb = tmp;
    }
    number L = 0.0;
    int n = 100;
    double dt = (tb - ta) / n;
    Vector3 p0 = (*this)(ta);
    Vector3 p1, dp;
    for (int i = 1; i <= n; ++i) { // D: foreach (i; 1 .. n+1)
        p1 = (*this)(ta + dt * i);
        dp = p1 - p0;
        L += eil::geom::abs(dp);
        p0 = p1;
    }
    return L;
}

// D:98-101
number Path::length() const { return partial_length(0.0, 1.0); }

// D:102-121. D's `out double t` is written as a reference here; both exits
// assign it, so the D out-parameter reset to double.init is not observable.
Vector3 Path::point_from_length(number length, double& t) const {
    number L = 0.0;
    int n = 1000;
    double dt = 1.0 / n;
    Vector3 p0 = (*this)(0.0);
    Vector3 p1, dp;
    for (int i = 1; i <= n; ++i) { // D: foreach (i; 1 .. n+1)
        p1 = (*this)(dt * i);
        dp = p1 - p0;
        L += eil::geom::abs(dp);
        p0 = p1;
        if (L > length) {
            t = dt * i;
            return p1;
        }
    }
    t = dt * n;
    return p1;
} // end point_from_length()

// D:124-150
bool Path::intersect2D(const Vector3& ps, const Vector3& d, double& t, int nseg) const {
    // Determine the intersection of a projected line on the Path.
    // Input:
    //     ps starting point for projected line
    //     d direction of projected line
    // Output:
    //     t parametric position of intersection along the Path
    // Returns:
    //     true, if the intersection point was located;
    //     false, if the intersection was not found
    // See PJ's workbook page 34, 2017-06-24 for notation and derivation.
    if (dynamic_cast<const Line*>(this) != nullptr) { nseg = 1; } // straight Line
    double delt = 1.0 / nseg;
    double t0 = 0.0; Vector3 p0 = (*this)(0.0);
    for (int i = 0; i < nseg; ++i) { // D: foreach (i; 0 .. nseg)
        double t1 = delt * (i + 1); Vector3 p1 = (*this)(t1);
        double tOnSegment;
        // D:142 calls the FREE geom.elements.intersect2D (projection.d:16). The
        // member name hides it in this scope, so the qualified name is required;
        // without the ::eil::geom qualifier this line would recurse into itself.
        bool intersectionOK = ::eil::geom::intersect2D(p0, p1, ps, d, tOnSegment);
        if (intersectionOK && tOnSegment >= 0.0 && tOnSegment <= 1.0) {
            t = t0 + tOnSegment * delt;
            return true;
        }
        t0 = t1; p0 = p1; // for next segment
    }
    return false;
} // end intersect2D()

// D:151-175
double Path::closestDistance(const Vector3& p, double& t) const {
    // This is the function we'll try to minimise with a line-search method.
    // D:154-158 is a nested function closing over `this` and `p`; its parameter
    // `t` shadows this function's `ref double t` (D:151) and the lambda parameter
    // keeps that shadowing harmless.
    auto distanceToPoint = [this, &p](double tt) {
        Vector3 ptOnPath = (*this)(tt);
        return distance_between(p, ptOnPath);
    };
    double tL = 0.0;
    double tR = 1.0;
    double tol = 1.0e-6;
    eil::nm::linesearch::minimize(distanceToPoint, tL, tR, tol);

    Vector3 ptOnPath;
    // Handle the special cases of being right at the end points.
    if (tL == 0.0) {
        t = 0.0;
    } else if (tR == 1.0) {
        t = 1.0;
    } else {
        t = 0.5 * (tL + tR);
    }
    ptOnPath = (*this)(t);
    return distance_between(p, ptOnPath);
} // end closestDistance

} // namespace geom

namespace geom {
namespace detail {
// See the declaration in path.hpp: this is D's `line.strip().split()` from
// std.string, which splits on runs of whitespace and drops leading/trailing
// whitespace, followed by the two `continue` guards the four gpath file readers
// share.
std::vector<std::string> tokenize_data_line(const std::string& line) {
    std::vector<std::string> tokens;
    const std::string ws = " \t\r\n\v\f";
    size_t pos = 0;
    while (pos < line.size() && ws.find(line[pos]) != std::string::npos) ++pos;
    while (pos < line.size()) {
        size_t end = pos;
        while (end < line.size() && ws.find(line[end]) == std::string::npos) ++end;
        tokens.push_back(line.substr(pos, end - pos));
        pos = end;
        while (pos < line.size() && ws.find(line[pos]) != std::string::npos) ++pos;
    }
    if (tokens.empty()) return tokens;    // D: if (tokens.length == 0) continue;
    if (tokens[0] == "#") return {};      // D: if (tokens[0] == "#") continue;
    return tokens;
}

// D's `auto f = File(fileName, "r"); foreach (line; f.byLine)`. A missing file is
// the same failure D raises (File.exists/ENOTFOUND becomes a std::ios failure,
// which D reports as "errno=No such file or directory"; the port throws
// eil::ExceptionBase with that text so a caller's catch (Exception) sees it).
std::vector<std::string> read_file_lines(const std::string& fileName) {
    std::ifstream in(fileName);
    if (!in) {
        throw eil::ExceptionBase("Cannot open file " + fileName + " in mode r.",
                                 __FILE__, __LINE__);
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push_back(line);
    }
    return lines;
} // end read_file_lines()

} // namespace detail
} // namespace geom
} // namespace eil
