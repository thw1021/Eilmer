// geom/misc/svg.d -> src/geom/misc/svg.cpp
//
// Definitions in D source order (D:45 through D:359). Every emitted fragment is a
// literal copy of D's writef/write argument, so the byte comparison against
// baselines/e587b235/probes/geom_misc_svg.txt is a comparison of the same text built
// the same way. See the header for the non-1:1 notes.
#include "geom/misc/svg.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>

#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/math.hpp"

namespace eil {
namespace geom {

// D:45-66
SVGContext::SVGContext(double width, double height, const std::string& unitLength,
                       const std::string& title, const std::string& description) {
    this->width = width;
    this->height = height;
    this->unitLength = unitLength;
    // Anonymous units in SVG 1.1 are pixels with 90px=1in or 1.25px=1pt.
    // D's switch over strings, in the same case order.
    if (this->unitLength == "in") {
        unitToPixels = 90.0;
    } else if (this->unitLength == "mm") {
        // svg.d:55 writes `90.0/25.4`, which ldc2 folds at compile time through
        // extended precision into 0x1.c58b162c58b16p+1 — one ULP below the value
        // g++ produces for the same expression, and one below what D itself
        // computes when the operands are not literals
        // (probes/substrate/probe_fold_div.d). The reference solver's actual
        // conversion factor is the folded one, so the port takes its bits; writing
        // the division here would shift every millimetre document coordinate by
        // 1 ULP (seen as x100=354.3307086614173 in D vs ...36 from g++).
        unitToPixels = 0x1.c58b162c58b16p+1;  // 90.0/25.4, as folded by ldc2
    } else if (this->unitLength == "cm") {
        // Measured: ldc2's fold of 90.0/2.54 equals the run-time quotient, so the
        // expression is safe to keep here.
        unitToPixels = 90.0 / 2.54;
    } else if (this->unitLength == "m") {
        // Measured: same for 90.0/0.0254.
        unitToPixels = 90.0 / 0.0254;
    } else {
        throw eil::ExceptionBase("SvgEnvironment: Unknown units " + unitLength, __FILE__,
                                 __LINE__);
    }
    setLineWidth(0.25);
    setDashArray();
    this->title = title;
    this->description = description;
    return;
} // end SVGContext()

SVGContext::~SVGContext() {
    // Phobos' std.stdioFile closes its handle in ~this, which is what happens to a
    // D SVGContext that is abandoned between open() and close() (svg.d's arc guard
    // can throw there). fclose(nullptr) is not legal, so guard.
    if (f != nullptr) {
        std::fclose(f);
        f = nullptr;
    }
}

void SVGContext::write(const std::string& s) { std::fwrite(s.data(), 1, s.size(), f); }

// D:68-82
//
// Transforms x-coordinate from user-space to SVG space.
double SVGContext::toPixelsX(double x) { return x * unitToPixels; }

// Transforms y-coordinate from user-space to SVG space, flipping the origin from the
// lower-left of user space to the upper-left of the page.
double SVGContext::toPixelsY(double y) { return (height - y) * unitToPixels; }

// D:84-89
// Sets line width (in mm).
void SVGContext::setLineWidth(double w) {
    lineWidth = w * 72.0 / 25.4;
    return;
}

// D:91-98
// 6 hexadecimal digits in form "#rrggbb", or a colour name.
void SVGContext::setLineColour(const std::string& colour) {
    lineColour = colour;
    return;
}

// D:100-104
void SVGContext::setFillColour(const std::string& colour) {
    fillColour = colour;
    return;
}

// D:106-110
void SVGContext::clearFillColour() {
    fillColour = "none";
    return;
}

// D:112-118
// Sets length of dashes and gaps (in mm).
void SVGContext::setDashArray(double dashLength, double gapLength) {
    this->dashLength = dashLength * 72.0 / 25.4;
    this->gapLength = gapLength * 72.0 / 25.4;
    return;
}

// D:120-143
// Assembles a suitable style specification string.
std::string SVGContext::styleStr(bool fill, bool stroke, bool dashed, double fillOpacity,
                                 double strokeOpacity) {
    std::string style = "";
    if (fill) {
        style += eil::format("fill:%s;fill-opacity:%g", fillColour, fillOpacity);
    } else {
        style += eil::format("fill:%s", "none");
    }
    if (stroke) {
        style += eil::format(";stroke:%s;stroke-width:%.2f;stroke-linecap:%s", lineColour,
                             lineWidth, lineCap);
        style += eil::format(";stroke-opacity:%g", strokeOpacity);
    }
    if (dashed) {
        style += eil::format(";stroke-dasharray: %.2f %.2f", dashLength, gapLength);
    }
    return style;
} // end styleStr()

// D:145-161
// Opens the SVG file and writes the preamble.
void SVGContext::open(const std::string& fileName) {
    f = std::fopen(fileName.c_str(), "w");
    if (f == nullptr) {
        // Measured Phobos text for `File(name, "w")` on an unreachable path:
        //   Cannot open file `/path' in mode `w' (No such file or directory)
        // raised as std.exception.ErrnoException. The substrate has no ErrnoException
        // (reported gap), so the class is ExceptionBase and the text is reproduced.
        std::string msg = "Cannot open file `" + fileName + "' in mode `w' (" +
                          std::strerror(errno) + ")";
        throw eil::ExceptionBase(msg, __FILE__, __LINE__);
    }
    write("<?xml version=\"1.0\"?>\n");
    write("<!DOCTYPE svg PUBLIC \"-//W3C//DTD SVG 1.0//EN\"\n");
    write("\"http://www.w3.org/TR/2001/REC-SVG-20010904/DTD/svg10.dtd\">\n");
    write("<svg xmlns:svg=\"http://www.w3.org/2000/svg\"\n");
    write("     xmlns:xlink=\"http://www.w3.org/1999/xlink\"\n");
    writef("     width=\"%.2f\" height=\"%.2f\"\n", width * unitToPixels, height * unitToPixels);
    write("     version=\"1.0\">\n");
    writef("<title>%s</title>\n", title);
    writef("<desc>%s</desc>\n", description);
    write("<!-- drawing begins here -->\n");
    return;
} // end open()

// D:163-170
// Inserts a comment into the SVG file.
void SVGContext::add_comment(const std::string& text) {
    write("<!-- ");
    write(text);
    write(" -->\n");
    return;
}

// D:172-179
// Finishes off the SVG file and closes it.
void SVGContext::close() {
    write("<!-- end of drawing -->\n");
    write("</svg>\n");
    std::fclose(f);
    f = nullptr;
    return;
}

// D:181-189
// id needs to be a unique identifier string, if supplied; opacity is 0.0 .. 1.0.
void SVGContext::begin_group(const std::string& id, double opacity) {
    write("<g ");
    if (id.length() > 0) { writef(" id=\"%s\"", id); }
    writef("opacity=\"%g\" >\n", opacity);
    return;
}

// D:191-195
void SVGContext::end_group() {
    write("</g>\n");
    return;
}

// D:197-207
// Render a line from point 1 to point 2.
void SVGContext::line(double x1, double y1, double x2, double y2, bool dashed, double opacity) {
    auto x1p = toPixelsX(x1); auto y1p = toPixelsY(y1);
    auto x2p = toPixelsX(x2); auto y2p = toPixelsY(y2);
    writef("<line x1=\"%.2f\" y1=\"%.2f\" x2=\"%.2f\" y2=\"%.2f\" ", x1p, y1p, x2p, y2p);
    writef("style=\"%s\" />\n", styleStr(false, true, dashed, 1.0, opacity));
    return;
}

// D:209-223
// Render a polyline from lists of x and y coordinates.
void SVGContext::polyline(eil::Slice<double> xlist, eil::Slice<double> ylist, bool dashed,
                          double opacity) {
    auto x0 = toPixelsX(xlist[0]); auto y0 = toPixelsY(ylist[0]);
    writef("<path d=\"M %.2f,%.2f", x0, y0);
    const size_t npts = std::min(xlist.length(), ylist.length());
    for (size_t i = 1; i < npts; ++i) {
        auto x = toPixelsX(xlist[i]); auto y = toPixelsY(ylist[i]);
        writef(" L %.2f,%.2f", x, y);
    }
    write("\""); // finish the coordinate string
    writef(" style=\"%s\"", styleStr(false, true, dashed, 1.0, opacity));
    write("/>\n");
    return;
} // end polyline()

// D:225-242
// Render a (closed) polygon from lists of x and y coordinates. The default opacity is
// 0.5 so that covered-over objects are still visible.
void SVGContext::polygon(eil::Slice<double> xlist, eil::Slice<double> ylist, bool fill,
                         bool stroke, bool dashed, const std::string& id, double opacity) {
    auto x0 = toPixelsX(xlist[0]); auto y0 = toPixelsY(ylist[0]);
    writef("<polygon points=\"%.2f,%.2f", x0, y0);
    const size_t npts = std::min(xlist.length(), ylist.length());
    for (size_t i = 1; i < npts; ++i) {
        auto x = toPixelsX(xlist[i]); auto y = toPixelsY(ylist[i]);
        writef(" %.2f,%.2f", x, y);
    }
    write("\""); // finish the coordinate string
    writef(" style=\"%s\"", styleStr(fill, stroke, dashed, opacity, opacity));
    write("/>\n");
    return;
} // end polygon()

// D:244-271
// Render a circular arc from point 0 to point 1 with centre at c, in user-space
// coordinates.
void SVGContext::arc(double x0, double y0, double x1, double y1, double xc, double yc,
                     bool dashed, double opacity) {
    double dx0 = x0 - xc; double dy0 = y0 - yc;
    double dx1 = x1 - xc; double dy1 = y1 - yc;
    double r0 = eil::sqrt(dx0 * dx0 + dy0 * dy0);
    double r1 = eil::sqrt(dx1 * dx1 + dy1 * dy1);
    if (std::fabs(r0 - r1) > 1.0e-6 * (r0 + 1.0)) {
        throw eil::ExceptionBase("Radii don't match", __FILE__, __LINE__);
    }
    double crossz = (dx0 * dy1 - dx1 * dy0); // z-component of vector product
    bool clockwise = (crossz < 0.0);
    // now, do the rendering in SVG space (in points)
    double x0p = toPixelsX(x0); double y0p = toPixelsY(y0);
    double x1p = toPixelsX(x1); double y1p = toPixelsY(y1);
    double rp = r0 * unitToPixels;
    double x_axis_rotation = 0.0;
    int large_arc_flag = 0;
    int sweep_flag = (clockwise) ? 1 : 0;
    writef("<path d=\"M %.2f %.2f A %.2f %.2f, %.2f, %d, %d, %.2f %.2f\" ", x0p, y0p, rp, rp,
           x_axis_rotation, large_arc_flag, sweep_flag, x1p, y1p);
    writef("style=\"%s\"", styleStr(false, true, dashed, 1.0, opacity));
    write("/>\n");
    return;
} // end arc()

// D:273-285
// Render a circle of radius r at centre (x,y).
void SVGContext::circle(double x, double y, double r, bool fill, bool stroke, bool dashed,
                        const std::string& id, double opacity) {
    double xp = toPixelsX(x); double yp = toPixelsY(y);
    double rp = r * unitToPixels;
    writef("<circle cx=\"%.2f\" cy=\"%.2f\" r=\"%.2f\" ", xp, yp, rp);
    writef("style=\"%s\" />\n", styleStr(fill, stroke, dashed, opacity, opacity));
    return;
}

// D:287-301
// Render a third-order Bezier curve.
void SVGContext::bezier3(double x0, double y0, double x1, double y1, double x2, double y2,
                         double x3, double y3, bool dashed, double opacity) {
    double x0p = toPixelsX(x0); double y0p = toPixelsY(y0);
    double x1p = toPixelsX(x1); double y1p = toPixelsY(y1);
    double x2p = toPixelsX(x2); double y2p = toPixelsY(y2);
    double x3p = toPixelsX(x3); double y3p = toPixelsY(y3);
    writef("<path d=\"M %.2f %.2f ", x0p, y0p);
    writef("C %.2f %.2f %.2f %.2f %.2f %.2f\" ", x1p, y1p, x2p, y2p, x3p, y3p);
    writef("style=\"%s\" />\n", styleStr(false, true, dashed, 1.0, opacity));
    return;
} // end bezier3()

// D:303-329
// Render the textString at point (x,y). `angle` is positive counterclockwise, so the
// SVG rotation is written as -angle. anchor is one of "start", "middle" or "end";
// fontSize is in points.
void SVGContext::text(double x, double y, const std::string& textString, double angle,
                      const std::string& anchor, int fontSize, const std::string& colour,
                      const std::string& fontFamily) {
    double xp = toPixelsX(x); double yp = toPixelsY(y);
    writef("<text x=\"%.2f\" y=\"%.2f\"", xp, yp);
    if (angle != 0.0) {
        // my angle is positive counterclockwise.
        writef(" transform=\"rotate(%.2f,%.2f,%.2f)\" ", -angle, xp, yp);
    }
    writef(" style=\"font-size:%dpt;text-anchor:%s;fill:%s;stroke:none", fontSize, anchor,
           colour);
    writef(";font-weight:normal;font-family:%s\" ", fontFamily);
    write(">"); // end of opening tag
    write(textString);
    write("</text>\n");
    return;
} // end text()

// D:331-359
// Render a dot with a text label. dotSize is the dot diameter in mm, and the label is
// drawn only when it is non-empty.
void SVGContext::dotlabel(double x, double y, const std::string& label,
                          const std::string& anchor, double dotSize, int fontSize,
                          const std::string& colour, const std::string& fontFamily) {
    double xp = toPixelsX(x); double yp = toPixelsY(y);
    double rp = dotSize / 2.0 * 72.0 / 25.4;
    writef("<circle cx=\"%.2f\" cy=\"%.2f\" r=\"%.2f\" ", xp, yp, rp);
    writef("style=\"fill:%s\" />\n", colour);
    if (label.length() > 0) {
        // put the label slightly above the dot.
        writef("<text x=\"%.2f\" y=\"%.2f\"", xp, yp - 1.5 * rp);
        writef(" style=\"font-size:%dpt;text-anchor:%s;fill:%s;stroke:none", fontSize, anchor,
               colour);
        writef(";font-weight:normal;font-family:%s\" ", fontFamily);
        write(">"); // end of opening tag
        write(label);
        write("</text>\n");
    }
    return;
} // end dotlabel()

} // namespace geom
} // namespace eil
