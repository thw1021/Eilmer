// geom/gpath/xsplinelsq.d -> src/geom/gpath/xsplinelsq.cpp
// (module geom.gpath.xsplinelsq).

#include "geom/gpath/xsplinelsq.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "eil/conv.hpp"
#include "eil/error.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"
#include "nm/splinelsq.hpp"

namespace eil {
namespace geom {

// D:28-33
XSplineLsq::XSplineLsq(const eil::Array<double>& xd, const eil::Array<double>& yd,
                       const eil::Array<double>& wd, const eil::Array<double>& xs, int nseg) {
    spl = eil::nm::CubicSplineLsq(xd.all(), yd.all(), wd.all(), xs.all(), nseg);
}

// D:43-51, the file-reading part of the second constructor
XSplineLsq::Data XSplineLsq::data_from_file(const std::string& fileName) {
    // This function takes a filename and processes it assuming that each
    // line contains (x,y,w) tuples (space-delimited).
    // If any y-values are missing on a given line, they are assumed to be 0.0.
    // If any weights are missing, they are assumed to be 1.0.
    Data out;
    const std::vector<std::string> lines = detail::read_file_lines(fileName);
    for (size_t il = 0; il < lines.size(); ++il) { // D: foreach (line; f.byLine)
        const std::vector<std::string> tokens = detail::tokenize_data_line(lines[il]);
        if (tokens.empty()) continue; // blank or "#" comment line
        double x = eil::to_double(tokens[0]);
        double y = 0.0;
        if (tokens.size() > 1) { y = eil::to_double(tokens[1]); }
        double w = 1.0;
        if (tokens.size() > 2) { w = eil::to_double(tokens[2]); }
        out.xd.append(x);
        out.yd.append(y);
        out.wd.append(w);
    }
    return out;
} // end data_from_file()

// D:35-66
XSplineLsq::XSplineLsq(const std::string& fileName, const eil::Array<double>& xs_in, int nseg) {
    Data d = data_from_file(fileName);
    eil::Array<double> xd = d.xd, yd = d.yd, wd = d.wd;
    eil_assert(nseg > 2);          // D:53 "Too few segments specified."
    eil::Array<double> xs(xs_in.dup());
    if (xs.length() > 0) {
        // Already have xs locations.
        eil_assert(xs.length() == static_cast<size_t>(nseg) + 1); // D:56
    } else {
        // Distribute xs locations uniformly over data range.
        // D:59-63 resizes the *parameter* view; see the header note.
        xs.set_length(static_cast<size_t>(nseg) + 1);
        for (int i = 0; i <= nseg; ++i) { // D: foreach (i; 0 .. nseg+1)
            double t = static_cast<double>(i) / nseg;
            xs[i] = xd[0] * (1.0 - t) + xd[xd.length() - 1] * t;
        }
    }
    spl = eil::nm::CubicSplineLsq(xd.all(), yd.all(), wd.all(), xs.all(), nseg);
} // end constructor

// D:68-71
XSplineLsq::XSplineLsq(const XSplineLsq& other) : spl(other.spl) {}

// D:73-76
std::shared_ptr<Path> XSplineLsq::dup() const { return std::make_shared<XSplineLsq>(*this); }

// D:78-84
Vector3 XSplineLsq::operator()(double t) const {
    double x = spl.xmin() * (1.0 - t) + spl.xmax() * t;
    double y = spl(x);
    double z = 0.0;
    return Vector3(x, y, z);
} // end opCall()

// D:86-93
std::string XSplineLsq::toString() const {
    return "XSplineLsq(xd=" + d_tostr(spl.xd) +
        ", yd=" + d_tostr(spl.yd) +
        ", xs=" + d_tostr(spl.xs) +
        ", nseg=" + d_tostr(spl.nseg) +
        ")";
}

// D:95-98
std::string XSplineLsq::classString() const { return "XSplineLsq"; }

} // namespace geom
} // namespace eil
