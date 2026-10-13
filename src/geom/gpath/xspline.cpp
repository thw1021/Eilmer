// geom/gpath/xspline.d -> src/geom/gpath/xspline.cpp (module geom.gpath.xspline).

#include "geom/gpath/xspline.hpp"

#include <memory>
#include <string>
#include <vector>

#include "eil/conv.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"

namespace eil {
namespace geom {

// D:28-32
XSpline::XSpline(const eil::Array<double>& xs, const eil::Array<double>& ys) {
    spl = eil::nm::CubicSpline(xs.all(), ys.all());
}

// D:40-49, the file-reading part of the second constructor
XSpline::Sample XSpline::samples_from_file(const std::string& fileName) {
    // This function takes a filename and processes it assuming that each
    // line contains (x,y) tuples (space-delimited).  If any y-values are
    // missing on a given line, they are assumed to be 0.0.
    Sample s;
    const std::vector<std::string> lines = detail::read_file_lines(fileName);
    for (size_t il = 0; il < lines.size(); ++il) { // D: foreach (line; f.byLine)
        const std::vector<std::string> tokens = detail::tokenize_data_line(lines[il]);
        if (tokens.empty()) continue; // blank or "#" comment line
        double x = eil::to_double(tokens[0]);
        double y = 0.0;
        if (tokens.size() > 1) { y = eil::to_double(tokens[1]); }
        s.xs.append(x);
        s.ys.append(y);
    }
    return s;
} // end samples_from_file()

// D:34-51
XSpline::XSpline(const std::string& fileName) {
    Sample s = samples_from_file(fileName);
    spl = eil::nm::CubicSpline(s.xs.all(), s.ys.all());
}

// D:53-56
XSpline::XSpline(const XSpline& other) : spl(other.spl) {}

// D:58-61
std::shared_ptr<Path> XSpline::dup() const { return std::make_shared<XSpline>(*this); }

// D:63-69
Vector3 XSpline::operator()(double t) const {
    double x = spl.xmin() * (1.0 - t) + spl.xmax() * t;
    double y = spl(x);
    double z = 0.0;
    return Vector3(x, y, z);
} // end opCall()

// D:71-74
std::string XSpline::toString() const {
    return "XSpline(xs=" + d_tostr(spl.xs) + ", ys=" + d_tostr(spl.ys) + ")";
}

// D:76-79
std::string XSpline::classString() const { return "XSpline"; }

} // namespace geom
} // namespace eil
