#pragma once
// geom/misc/svg.d -> src/geom/misc/svg.hpp (module geom.misc.svg).
//
// A 2D drawing context that writes Scalable Vector Graphics text. The coordinate
// transform is part of the contract: user space has (0,0) in the LOWER-LEFT corner and
// units of `unitLength`; SVG space has (0,0) in the UPPER-LEFT corner and anonymous
// units of pixels, with 90px = 1in / 1.25px = 1pt (svg.d:52). Lengths are emitted as
// %.2f and opacities as %g, so the whole module is judged on byte-identical text, not
// on a tolerance.
//
// Namespace: eil::svg would be a module namespace; it is not needed — SVGContext and
// its members collide with nothing else in eil::geom (grep over gdtk src/geom: svg.d
// is the only declaration of SVGContext, toPixelsX/Y, styleStr, dotlabel, ...), so the
// class sits flat in eil::geom with univariatefunctions/kdtree/nurbs_utils, as
// wave-02-brief section 2 asks.
//
// Non-1:1 notes (all registered in provenance/fidelity.yml):
//   * D's private `File f` becomes a `FILE*`. Phobos' File closes in its destructor,
//     so the ported destructor does the same when close() was never called (reachable:
//     svg.d's arc guard can throw between open() and close(), and D's GC then closes
//     the handle). Copying is deleted because D class semantics are reference
//     semantics — a ported handle is std::shared_ptr<SVGContext>, mirroring the
//     adapted-handles rule, and duplicating a FILE* would double-close it.
//   * `f = File(fileName, "w")` failure in D raises std.exception.ErrnoException with
//     the measured text
//       Cannot open file `/path' in mode `w' (No such file or directory)
//     The substrate has no ErrnoException class (reported as a gap), so the port throws
//     eil::ExceptionBase with that exact text; errno's string comes from strerror().
//   * add_comment's parameter is named `text` in D (svg.d:163), shadowing std.stdio's
//     text(). The shadowing name is kept, as the properties.hpp precedent keeps
//     shadowing names, and the body never needs the shadowed function.
//   * D's `switch (unitLength)` over strings becomes an if/else chain over
//     std::string, in the same case order, with the same default Exception.
//   * polyline and polygon loop over `min(xlist.length, ylist.length)` (svg.d:215,
//     :234): lists of unequal length are legal and silently truncated. Kept, and
//     probed — the fourth `polyline` of svgprobe_full.svg in
//     probes/geom/misc/probe_svg.d passes 4 x-values and 2 y-values, so the emitted
//     path has 3 vertices, not 5.
//   * `setLineWidth`/`setDashArray` convert mm to points as `w * 72.0/25.4`, evaluated
//     left to right in D; the port writes the same expression rather than a folded
//     constant, because a folded one can differ by 1 ULP (backlog note on constant
//     folding).

#include <cstddef>
#include <cstdio>
#include <string>

#include "eil/array.hpp"
#include "eil/format.hpp"

namespace eil {
namespace geom {

class SVGContext {
  public:
    // D:45-66
    explicit SVGContext(double width = 120.0, double height = 120.0,
                        const std::string& unitLength = "mm",
                        const std::string& title = "Untitled",
                        const std::string& description = "No description");
    ~SVGContext();
    SVGContext(const SVGContext&) = delete;
    SVGContext& operator=(const SVGContext&) = delete;

    // D:68-82
    double toPixelsX(double x);
    double toPixelsY(double y);

    // D:84-118
    void setLineWidth(double w);
    void setLineColour(const std::string& colour);
    void setFillColour(const std::string& colour);
    void clearFillColour();
    void setDashArray(double dashLength = 2.0, double gapLength = 2.0);

    // D:120-143
    std::string styleStr(bool fill, bool stroke, bool dashed, double fillOpacity = 1.0,
                         double strokeOpacity = 1.0);

    // D:145-179
    void open(const std::string& fileName = "drawing.svg");
    void add_comment(const std::string& text);
    void close();

    // D:181-195
    void begin_group(const std::string& id = "", double opacity = 1.0);
    void end_group();

    // D:197-359
    void line(double x1, double y1, double x2, double y2, bool dashed = false,
              double opacity = 1.0);
    void polyline(eil::Slice<double> xlist, eil::Slice<double> ylist, bool dashed = false,
                  double opacity = 1.0);
    void polygon(eil::Slice<double> xlist, eil::Slice<double> ylist, bool fill = true,
                 bool stroke = true, bool dashed = false, const std::string& id = "",
                 double opacity = 0.5);
    void arc(double x0, double y0, double x1, double y1, double xc, double yc,
             bool dashed = false, double opacity = 1.0);
    void circle(double x, double y, double r, bool fill = true, bool stroke = true,
                bool dashed = false, const std::string& id = "", double opacity = 0.5);
    void bezier3(double x0, double y0, double x1, double y1, double x2, double y2, double x3,
                 double y3, bool dashed = false, double opacity = 1.0);
    void text(double x, double y, const std::string& textString, double angle = 0.0,
              const std::string& anchor = "start", int fontSize = 10,
              const std::string& colour = "black", const std::string& fontFamily = "sanserif");
    void dotlabel(double x, double y, const std::string& label = "",
                  const std::string& anchor = "middle", double dotSize = 2.0, int fontSize = 10,
                  const std::string& colour = "black",
                  const std::string& fontFamily = "sanserif");

  private:
    // Somewhere to keep the rendering configuration. Field order follows D:29-42.
    double width;
    double height;
    std::string unitLength;
    double unitToPixels = 1.0;
    std::string lineColour = "black";
    std::string fillColour = "none";
    double lineWidth = 1.0; // points
    std::string lineCap = "round";
    std::string lineJoin = "round"; // D:37 declares and sets it; no emitter in svg.d reads it
    double dashLength = 2.0 * 72.0 / 25.4; // 2mm in points
    double gapLength = 2.0 * 72.0 / 25.4;
    std::string title;
    std::string description;
    FILE* f = nullptr;

    // D's f.write(s) and f.writef(fmt, args...): the formatted form goes through
    // eil::format, i.e. the same renderer the rest of the port measures against Phobos.
    void write(const std::string& s);
    template <typename... Args>
    void writef(const std::string& fmt, const Args&... args) {
        write(eil::format(fmt, args...));
    }
};

} // namespace geom
} // namespace eil
