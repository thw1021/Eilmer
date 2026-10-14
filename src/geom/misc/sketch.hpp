#pragma once
// geom/misc/sketch.d -> src/geom/misc/sketch.hpp (module geom.misc.sketch).
//
// A renderer-independent sketching front end: user-space geometry in metres goes
// through a view matrix, a projection matrix and a viewport transform onto a virtual
// canvas in millimetres, and the canvas coordinates are handed to a renderer (SVG here).
//
// `import geom;` (sketch.d:36) is the package aggregator, used only for Vector3, Arc,
// CoonsPatch, ParametricSurface and ParametricVolume; wave-02-brief section 4 item 5
// forbids a generated aggregate header, so the concrete module headers are included
// instead: geom/elements/vector3.hpp, geom/gpath/path.hpp (Path, the base Arc renders
// through), geom/surface/parametricsurface.hpp, geom/volume/parametricvolume.hpp and
// geom/misc/svg.hpp (geom.misc.svg, D:32). `import ntypes.complex;` (D:29) and
// `import nm.number;` (D:30) become eil/number.hpp and nm/number.hpp through those.
//
// Non-1:1 notes (all registered in provenance/fidelity.yml):
//   * adapted-handles: `SVGContext svg` (D:61) is a class reference, so it is a
//     std::shared_ptr<SVGContext> and each render() of a Path / ParametricSurface /
//     ParametricVolume takes a shared_ptr to the base, the same shape
//     geom/gpath/path.hpp and the surface/volume headers use. Copying a Sketch is
//     deleted because D class values are GC references, not copies.
//   * D's default arguments (`this(string renderer_name="svg", string
//     projection_name="xyortho", double[] canvas_mm = [0.0,0.0,120.0,120.0])`, D:99)
//     cannot be spelled for a Slice parameter, because a Slice needs a live backing
//     array and a header-scope temporary would dangle. They become three delegating
//     overloads (0-arg, 2-arg, 3-arg) whose default canvas comes from a file-scope
//     static in the .cpp. An explicitly-passed short array still indexes out of range
//     through Slice::operator[], as D's does.
//   * `const double dpi = 90.0;` (D:38) is a module-level constant: literal, no
//     arithmetic, so no folding risk. It is `inline constexpr` in eil::geom.
//   * `final switch` (D:123, :146, :173, :255, :286, :302, :317, :332, :348, :364, :392,
//     :404, :423, :456, :491, :533, :560, :605, :636, :674) has no C++ equivalent: each
//     becomes a switch over every enumerator with NO `default:` label, which keeps
//     D's "forget an enumerator and it does not compile" constraint alive through
//     -Wswitch-enum. Where a D case declares a variable (D:128-129, :136) the case body
//     is braced; that is C++ scoping only, not a change of order.
//   * `switch (string)` (D:103, :112, :220) becomes an if/else chain in the same case
//     order, matching the precedent at src/geom/misc/svg.cpp:28-52.
//   * The two literal expressions of D:128-129 and D:136 are MEASURED, not reasoned
//     about (probes/geom/misc/probe_sketch.d rows s1_*/s4 proj isometric/oblique,
//     ldc2 -O0, raw %016x):
//       30.0/180.0*PI        folded 0x3fe0c152382d7366 vs run-time 0x3fe0c152382d7365
//       cos(30.0/180.0*PI)   folded 0x3febb67ae8584caa vs run-time 0x3febb67ae8584cab
//       sin(30.0/180.0*PI)   folded 0x3fe0000000000000 vs run-time 0x3fdfffffffffffff
//     The projection matrix that the reference build actually stores is the FOLDED
//     pair, so T-909 bites here twice (once for the argument, once for each function)
//     and the port writes ldc2's own bits as hex-float literals, exactly as
//     src/geom/misc/svg.cpp:41 does for the folded 90.0/25.4. Writing
//     eil::cos()/eil::sin() here would hand the gate the run-time pair, which is
//     1 ULP off; there is deliberately no sin/cos pair evaluated at run time in this
//     module, so T-920's sincos fusion cannot bite either.
//     1.0/(2.0*sqrt(2.0)) is different: folded == run-time == 0x3fd6a09e667f3bcc, so
//     D:136 keeps its expression.
//   * `string title; string description;` (D:71-72) are null in D unless the caller
//     assigns them, and D renders a null string with %s as NOTHING (measured:
//     "s3 default title=[] desc=[]", and the demo documents carry
//     `<title></title>`). std::string's default state is the observable equivalent, so
//     no nullable-string wrapper is needed here.
//   * `p.x.re` (D:219, :421, ...) is D's built-in real-part property on a floating
//     point type; with nm.number's trunk `number = double` it is the identity, and the
//     port writes eil::re(), which is the identity too (substrate/include/eil/number.hpp:81).
//   * `to!string(eye)` (D:191-193) is std.conv's struct hook, i.e. Vector3's own
//     toString(); eil::to_string only dispatches floats, integers, strings and
//     range-likes, so the port calls .toString() directly. Same bytes.
//   * the 19 `version(with_libplot)` sites (D:33-34, :62-69, :152-163, :178-182,
//     :261-280, :291-295, :307-310, :322-325, :337-341, :353-356, :369-371, :396, :408,
//     :428-439, :461-474, :496-521, :538-549, :565-584, :610-621, :641-660, :679-703)
//     are kept under EIL_WITH_LIBPLOT and are NOT compiled: this machine has no GNU
//     libplot, so no D binary with the variant enabled exists to gate against (T-921).
//     They are transcribed for fidelity only; their compilability is unverified and the
//     libplot C API has no declaration in this tree. D:165's xplot Exception, which
//     lives in the version's `else` branch, IS compiled and IS gated
//     (baseline row "s3 build [xplot]/[xyortho] ok=0 isError=0").
//   * the two `new Error(...)` sites (D:162, :180) are inside those uncompiled blocks;
//     they port to eil::ErrorBase with the message only. Measured rule (fidelity.yml,
//     grid section): a bare object.Error carries file == "" and line == 0, so the port
//     does NOT pass __FILE__/__LINE__ there. The four `new Exception` sites (D:109, :119,
//     :165, :754) do carry the call site, as src/geom/misc/svg.cpp:50 does.
//   * Exception and Error are SIBLINGS here too (wave-01 integration lesson 1);
//     baseline rows "s3 sibling exception caughtByException=1" /
//     "s3 sibling error caughtByError=1" are what prove it on both sides.
//   * D's `File` flushes and closes in its destructor. The document handle lives in
//     SVGContext (which already ports that), and every Sketch::finish() reaches
//     svg.close(), so Sketch itself owns nothing to flush.

#include <array>
#include <cstddef>
#include <string>

#include "eil/array.hpp"
#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"
#include "geom/misc/svg.hpp"
#include "geom/surface/parametricsurface.hpp"
#include "geom/volume/parametricvolume.hpp"
#include "nm/number.hpp"

#ifdef EIL_WITH_LIBPLOT
// D:33-34 `import geom.misc.libplot;` — the variant's C API header. Not available on
// this machine (T-921); the blocks guarded by EIL_WITH_LIBPLOT below are transcription
// only and have never been compiled.
#include <plot.h>
#endif

namespace eil {
namespace geom {

// D:38
inline constexpr double dpi = 90.0; // Expected screen resolution.

// D:40-50
struct Extents {
    // Unassigned fields are D's .init, i.e. quiet NaN, not 0 (measured: probe rows
    // "s2 init x0" .. "s2 init height" print nan/7ff8000000000000).
    double x0 = d_init<double>(), x1 = d_init<double>(), y0 = d_init<double>(),
           y1 = d_init<double>();
    double width = d_init<double>(), height = d_init<double>();

    // D:43-49. The parameters shadow the members, as they do in D; `this->` is how the
    // port keeps D's meaning (the members, not the parameters, are written).
    void set(double x0, double y0, double x1, double y1) {
        this->x0 = x0;
        this->y0 = y0;
        this->x1 = x1;
        this->y1 = y1;
        width = x1 - x0;
        height = y1 - y0;
    }
};

// D:52-53
enum class Renderer { svg, xplot };
enum class Projection { xyortho, isometric, oblique };

// D:54-55 (module-level arrays, read-only in every reachable path).
inline const std::array<std::string, 2> rendererNames{{"svg", "xplot"}};
inline const std::array<std::string, 3> projectionNames{{"xyortho", "isometric",
                                                        "oblique"}};

// D:57-964
class Sketch {
  public:
    // We have some state and configuration data. (D:60-96)
    Renderer myRenderer = Renderer::svg;
    std::shared_ptr<SVGContext> svg;
#ifdef EIL_WITH_LIBPLOT
    plPlotter* myXplotter = nullptr;
    double xplot_line_width = 0.25; // Line thickness in mm.
    std::string xplot_bgcolourname = "white";
    std::string xplot_pencolourname = "black";
    std::string xplot_fillcolourname = "yellow";
    int xplot_greylevel = 0x8000; // 50% of 0xffff
#endif
    Projection myProjection = Projection::xyortho;
    std::string title;
    std::string description;
    // Rendering canvas and viewport into user-space
    //
    //     (x0,y1)---------(x1,y1)
    //        |               |
    //        |               |
    //        |               |
    //     (x0,y0)---------(x1,y0)
    //
    Extents canvas;   // Virtual-canvas or device-space in mm.
    Extents viewport; // view coordinates in metres (following projection)
    double proj_mat[4][4]; // projection matrix, as per OpenGL description
    double view_mat[4][4]; // for when we want to view the model differently
    // We keep in mind the usual 4-component homogeneous coordinate representation
    // (x,y,z,w) so that we can accommodate perspective projection eventually.
    // Graphics pipeline processes are:
    // (1) 3D vertex data for model --> multiply by view_matrix --> 3D world coordinates
    // (2) 3D world coordinates --> multiply by projection_matrix --> 2D view coordinates
    // (3) 2D view coordinates --> viewport transform --> 2D window/canvas coordinates
    //
    // Default view is having the z-axis coming towards your eye, x-axis to the right
    // and y-axis in the up direction.
    Vector3 eye = Vector3(0.0, 0.0, 0.0);
    Vector3 centre = Vector3(0.0, 0.0, -1.0);
    Vector3 up = Vector3(0.0, 1.0, 0.0);

    // D:99-168. The three overloads are D:99's three default arguments (see the header
    // note about a Slice default argument).
    Sketch();
    Sketch(const std::string& renderer_name, const std::string& projection_name);
    Sketch(const std::string& renderer_name, const std::string& projection_name,
           eil::Slice<double> canvas_mm);
    ~Sketch(); // D:170-184
    Sketch(const Sketch&) = delete;
    Sketch& operator=(const Sketch&) = delete;

    // D:186-196. `override` in D is against Object, which has no C++ counterpart here
    // (and sketch.d declares no classString(), grep-count 0), so it is not an override.
    std::string toString() const;

    // D:198-214
    void set_to_identity(double (&mat)[4][4]);
    void matrix_vector_multiply(double (&mat)[4][4], double (&vec)[4]);

    // D:216-231
    void apply_transform(const std::string& transform_name, Vector3& p, double& w);

    // D:233-251. The parameter names shadow the members exactly as in D; the members
    // are assigned from them first, then the shadowing names are what the arithmetic
    // reads.
    void look_at(const Vector3 eye, const Vector3 centre, const Vector3 up);

    // D:253-282
    void start(const std::string& file_name = "sketch.svg");
    // D:284-297
    void finish();

    // D:299-374. Sets line width in mm on our virtual canvas.
    void setLineWidth(double width);
    void setLineColour(const std::string& colour);
    void setFillColour(const std::string& colour);
    void clearFillColour();
    // Sets length of dashes and gaps, in units appropriate to the renderer.
    void setDashArray(double dashLength = 2.0, double gapLength = 2.0);

    // D:376-386. Map from user-space to canvas-space.
    double toCanvasX(double x);
    double toCanvasY(double y);

    // D:388-412. id needs to be a unique identifier string, if supplied; opacity is in
    // the range 0.0, 1.0
    void begin_group(const std::string& id = "", double opacity = 1.0);
    void end_group();

    // D:414-442. Render a line from point 1 to point 2.
    void line(const Vector3 p0, const Vector3 p1, bool dashed = false);
    // D:444-477
    void polyline(eil::Slice<Vector3> pa, bool dashed = false);
    // D:479-524
    void polygon(eil::Slice<Vector3> pa, bool fill = true, bool stroke = true,
                 bool dashed = false);
    // D:526-552
    void arc(const Vector3 p0, const Vector3 p1, const Vector3 pc, bool dashed = false);
    // D:554-587. r is in mm on the canvas.
    void circle(const Vector3 pc, double r, bool fill = true, bool stroke = true,
                bool dashed = false);
    // D:589-624
    void bezier3(const Vector3 p0, const Vector3 p1, const Vector3 p2, const Vector3 p3,
                 bool dashed = false);
    // D:626-663. angle (in degrees) of the text line wrt the x-axis in canvas system
    // (counterclockwise is positive).
    void text(const Vector3 p, const std::string& textString, const double angle = 0.0,
              const std::string& anchor = "start", int fontSize = 10,
              const std::string& colour = "black",
              const std::string& fontFamily = "sanserif");
    // D:665-706. dotSize is already in virtual-canvas mm; fontSize is in points.
    void dotlabel(const Vector3 p, const std::string& label = "",
                  const std::string& anchor = "middle", double dotSize = 2.0,
                  int fontSize = 10, const std::string& colour = "black",
                  const std::string& fontFamily = "sanserif");

    // ----------------------------------------------------------------------
    // The following methods should be independent of the particular renderer.
    // ----------------------------------------------------------------------

    // D:712-772
    // direction: one of "x", "y", "z"
    // vmin, vmax, vtic: range and increments for our rule
    // anchorPoint: rule will go through this point (if extended, maybe)
    // ticMarkSize: in world units
    // numberFormat: print specifier string, "%.1f" for example
    // textOffset: world units to anchor point for text labels
    // textAngle: angle (in degrees) of text line wrt x-axis in canvas system
    //            (counterclockwise is positive)
    // fontSize: in points
    void rule(const std::string& direction, double vmin, double vmax, double vtic,
              Vector3 anchorPoint, double ticMarkSize, const std::string& numberFormat,
              double textOffset, double textAngle, int fontSize);

    // D:775-781. Render functions for our geometric entities used in the flow simulation.
    void render(const PathPtr& pth, bool dashed = false, size_t n = 30);
    // D:783-818
    void render(const ParametricSurface::Ptr& surf, bool fill = true, bool stroke = true,
                bool dashed = false, size_t n = 30, bool facets = false);
    // D:820-962
    void render(const ParametricVolume::Ptr& pvol, bool fill = true, bool stroke = true,
                bool dashed = false, size_t n = 30, bool facets = false);
};

} // namespace geom
} // namespace eil
