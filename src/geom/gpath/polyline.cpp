// geom/gpath/polyline.d -> src/geom/gpath/polyline.cpp (module geom.gpath.polyline).
//
// Build Paths out of segments. This includes the building of splines with
// Bezier3 segments.

#include "geom/gpath/polyline.hpp"

#include <cmath>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "eil/error.hpp"
#include "eil/conv.hpp"
#include "eil/format.hpp"
#include "eil/math.hpp"
#include "geom/elements/projection.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/arc.hpp"
#include "geom/gpath/bezier.hpp"
#include "geom/gpath/line.hpp"
#include "geom/gpath/path.hpp"

namespace eil {
namespace geom {

// D:32-50. Construct from the Path segments.
Polyline::Polyline(const PathList& segs, bool isClosed, double tolerance) {
    if (segs.length() == 0) {
        throw eil::ErrorBase("Polyline() No segments present.", __FILE__, __LINE__);
    }
    for (size_t i = 0; i < segs.length(); ++i) { // D: foreach (myseg; segments)
        segments.append(segs[i]->dup());
    }
    if (isClosed) {
        // Test that the end-points of the path are close enough and,
        // if not, add a line segment to close the path.
        Vector3 p0 = (*segs[0])(0.0);
        Vector3 p1 = (*segs[segs.length() - 1])(1.0); // D: segments[$-1]
        if (distance_between(p1, p0) > tolerance) {
            segments.append(std::make_shared<Line>(p1, p0));
        }
    }
    closed = isClosed;
    t_values.set_length(segments.length());
    reset_breakpoints();
}

// D:55-105, the statements of the spline constructor that run before the
// delegating call D:107.
PathList Polyline::spline_segments(const eil::Array<Vector3>& p_orig, bool isClosed,
                                            double tolerance) {
    eil::Array<Vector3> p;
    for (size_t i = 0; i < p_orig.length(); ++i) p.append(p_orig[i]); // D: p ~= pnt
    if (isClosed && distance_between(p[0], p[p.length() - 1]) > tolerance) {
        // Add one more point to close the path.
        p.append(p[0]);
    }
    size_t m = p.length() - 1;
    // Given m+1 interpolation points p, determine the m-segment
    // Bezier polyline that interpolates these points as a spline.
    // This is done by first determining the array of weight points
    // which define the spline and then evaluating the cubic
    // Bezier segments.
    // Reference:
    //     G. Engelin & F. Uhlig (1996)
    //     Numerical Algorithms with C
    //     Springer, Berlin
    //     Section 12.3.1

    eil::Array<Vector3> d;
    d.set_length(m + 1); // weight points
    // For a natural spline, the first and last weight points
    // are also the first and last interpolation points.
    d[0] = p[0];
    d[m] = p[m];

    // For the initial guess at the remaining weight points,
    // just use the supplied data points.
    for (size_t i = 1; i < m; ++i) { // D: foreach (i; 1 .. m)
        d[i] = p[i];
    }
    // Apply Gauss-Seidel iteration until
    // the internal weight points converge.
    Vector3 old_p;
    number max_diff;
    for (int j = 1; j < 50; ++j) { // D: foreach (j; 1 .. 50)
        max_diff = 0.0;
        for (size_t i = 1; i < m; ++i) { // D: foreach (i; 1 .. m)
            old_p = d[i];
            d[i] = 0.25 * (6.0 * p[i] - d[i - 1] - d[i + 1]);
            Vector3 diff = d[i] - old_p;
            max_diff = std::fmax(max_diff, eil::geom::abs(diff));
        } // end foreach i
        if (max_diff < tolerance) break;
    } // end foreach j

    // Final stage; calculate the Bezier segments
    eil::Array<Vector3> p03(4);
    PathList seg;
    for (size_t i = 0; i < m; ++i) { // D: foreach (i; 0 .. m)
        p03[0] = p[i];
        p03[1] = (2.0 * d[i] + d[i + 1]) / 3.0;
        p03[2] = (d[i] + 2.0 * d[i + 1]) / 3.0;
        p03[3] = p[i + 1];
        seg.append(std::make_shared<Bezier>(p03));
    }
    return seg;
} // end spline_segments()

// D:53-108. Construct as a spline through specified points.
Polyline::Polyline(const eil::Array<Vector3>& p_orig, bool isClosed, double tolerance)
    // D:107 `this(seg, isClosed)` — the third argument is left at its default,
    // exactly as D's delegating call does.
    : Polyline(spline_segments(p_orig, isClosed, tolerance), isClosed) {}

// D:111-130. Contructs a spline from a file containing x(,y(,z)) coordinates.
eil::Array<Vector3> Polyline::points_from_file(const std::string& fileName) {
    // This function takes a filename and processes it assuming that each
    // line contains (x,y,z) triples (space-delimited).  If any y- or z-values are
    // missing on a given line, they are assumed to be 0.0.  The x,y,z-triples
    // are gathered and used to create the Spline.
    // Ported Python code Spline2 from libgeom2.i 2015-10-05 by PJ
    eil::Array<Vector3> points;
    const std::vector<std::string> lines = detail::read_file_lines(fileName);
    for (size_t il = 0; il < lines.size(); ++il) { // D: foreach (line; f.byLine)
        const std::vector<std::string> tokens = detail::tokenize_data_line(lines[il]);
        if (tokens.empty()) continue; // blank or "#" comment line
        number x = eil::to_double(tokens[0]);
        number y = 0.0;
        if (tokens.size() > 1) { y = eil::to_double(tokens[1]); }
        number z = 0.0;
        if (tokens.size() > 2) { z = eil::to_double(tokens[2]); }
        points.append(Vector3(x, y, z));
    }
    return points;
} // end points_from_file()

Polyline::Polyline(const std::string& fileName, bool isClosed)
    : Polyline(points_from_file(fileName), isClosed) {} // end spline constructor

// D:132-135
Polyline::Polyline(const Polyline& other, bool isClosed)
    : Polyline(other.segments, isClosed) {}

// D:137-140
std::shared_ptr<Path> Polyline::dup() const { return std::make_shared<Polyline>(segments, closed); }

// D:142-163
Vector3 Polyline::operator()(double t) const {
    // Evaluate B(t) without considering arc_length parameterization flag
    // or subrange.
    size_t n = segments.length();
    if (n == 1) return (*segments[0])(t);
    size_t i = 0; // D: `size_t i;` is int.init == 0
    for (i = 0; i < n; ++i) {
        if (t <= t_values[i]) break;
    }
    if (i >= n) i = n - 1; // last segment
    // At this point, t_values[i-1] < t <= t_values[i] (we hope)
    // Have assumed that the t breakpoints are well behaved.
    // i.e. There are no zero-length segments.
    double t_local;
    if (i == 0) {
        t_local = t / t_values[i];
    } else {
        t_local = (t - t_values[i - 1]) / (t_values[i] - t_values[i - 1]);
    }
    return (*segments[i])(t_local);
} // end opCall()

// D:165-169
std::string Polyline::toString() const {
    return "Polyline(segments=" + d_tostr(segments) + ", closed=" + d_tostr(closed) + ")";
}

// D:170-173
std::string Polyline::classString() const { return "Polyline"; }

// D:175-278
std::string Polyline::toGmshString(int& pointTag, int& curveTag, int& loopTag,
                                   const std::string& label, double len) {
    std::string str = "// " + label + "\n";
    str += eil::format("len = %g;\n", len);
    eil::Array<int> segmentTags;
    Vector3 startPoint = (*segments[0])(0.0);
    Vector3 previousPoint = startPoint;
    pointTag += 1;
    str += eil::format("Point(%d) = {%g, %g, %g, len};\n", pointTag,
                       startPoint.x, startPoint.y, startPoint.z);
    int startPointTag = pointTag;
    double tol = 1.0e-6;

    for (size_t i = 0; i < segments.length(); ++i) { // D: foreach (i, seg; segments)
        const PathPtr& seg = segments[i];
        const std::string kind = seg->classString();
        if (kind == "Line") {
            // Put down the first point of the segment
            // only if it significantly different to the previous point.
            Vector3 p0 = (*seg)(0.0);
            if (distance_between(p0, previousPoint) > tol) {
                pointTag += 1;
                str += eil::format("Point(%d) = {%g, %g, %g, len};\n", pointTag,
                                   p0.x, p0.y, p0.z);
                previousPoint.set(p0);
            }
            int tag0 = pointTag;
            int tag1 = 0; // D:204 leaves this at int.init == 0 on one path (header)
            if ((i < segments.length() - 1) || (!closed)) {
                Vector3 p1 = (*seg)(1.0);
                if (distance_between(p1, previousPoint) > tol) {
                    pointTag += 1;
                    str += eil::format("Point(%d) = {%g, %g, %g, len};\n", pointTag,
                                       p1.x, p1.y, p1.z);
                    tag1 = pointTag;
                    previousPoint.set(p1);
                }
            } else {
                tag1 = startPointTag;
            }
            curveTag += 1;
            str += eil::format("Line(%d) = {%d, %d};\n", curveTag, tag0, tag1);
            segmentTags.append(curveTag);
        } else if (kind == "Arc") {
            str += "Arc not yet implemented\n";
        } else if (kind == "Bezier") {
            // Put down the first point of the segment
            // only if it significantly different to the previous point.
            const Bezier* mySeg = dynamic_cast<const Bezier*>(seg.get());
            eil::Array<Vector3> p;
            p.append((*mySeg).B[0]);
            if (distance_between(p[0], previousPoint) > tol) {
                pointTag += 1;
                str += eil::format("Point(%d) = {%g, %g, %g, len};\n", pointTag,
                                   p[0].x, p[0].y, p[0].z);
                previousPoint.set(p[0]);
            }
            eil::Array<int> tags;
            tags.append(pointTag);
            // Put down intermediate control points.
            for (size_t j = 1; j < mySeg->B.length() - 1; ++j) { // D: 1 .. mySeg.B.length-1
                pointTag += 1;
                str += eil::format("Point(%d) = {%g, %g, %g, len};\n", pointTag,
                                   mySeg->B[j].x, mySeg->B[j].y, mySeg->B[j].z);
                tags.append(pointTag);
            }
            // Conditionally put down final point of Bezier.
            if ((i < segments.length() - 1) || (!closed)) {
                p.append(mySeg->B[mySeg->B.length() - 1]); // D: mySeg.B[$-1]
                if (distance_between(p[p.length() - 1], previousPoint) > tol) {
                    pointTag += 1;
                    str += eil::format("Point(%d) = {%g, %g, %g, len};\n", pointTag,
                                       p[p.length() - 1].x, p[p.length() - 1].y,
                                       p[p.length() - 1].z);
                    tags.append(pointTag);
                    previousPoint.set(p[p.length() - 1]);
                }
            } else {
                tags.append(startPointTag);
            }
            curveTag += 1;
            str += eil::format("Bezier(%d) = {%d", curveTag, tags[0]);
            for (size_t j = 1; j < tags.length() - 1; ++j) { // D: foreach (j; 1 .. tags.length-1)
                str += eil::format(", %d", tags[j]);
            }
            str += eil::format(", %d};\n", tags[tags.length() - 1]);
            segmentTags.append(curveTag);
        } else {
            str += "// Segment type " + kind + " not handled.\n";
        } // end switch
    } // end foreach
    //
    loopTag += 1;
    str += eil::format("Curve Loop(%d) = {", loopTag);
    for (size_t i = 0; i < segmentTags.length(); ++i) { // D: foreach (i, segTag; segmentTags)
        str += eil::format("%d", segmentTags[i]);
        str += (i + 1 == segmentTags.length()) ? "};\n" : ", ";
    }
    curveTag += 1;
    str += eil::format("Physical Curve(\"%s\", %d) = {", label, curveTag);
    for (size_t i = 0; i < segmentTags.length(); ++i) {
        str += eil::format("%d", segmentTags[i]);
        str += (i + 1 == segmentTags.length()) ? "};\n" : ", ";
    }
    return str;
} // end toGmshString()

// D:281-290
void Polyline::reset_breakpoints() {
    // Set up the parameter breakpoints based on cumulative length.
    t_values[0] = eil::re(segments[0]->length());
    for (size_t i = 1; i < segments.length(); ++i) { // D: foreach (i; 1 .. segments.length)
        t_values[i] = t_values[i - 1] + eil::re(segments[i]->length());
    }
    double L_total = t_values[t_values.length() - 1];
    for (size_t i = 0; i < segments.length(); ++i) { // D: foreach (i; 0 .. segments.length)
        t_values[i] /= L_total;
    }
} // end reset_breakpoints()

} // namespace geom
} // namespace eil
