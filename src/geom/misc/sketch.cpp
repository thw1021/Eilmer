// geom/misc/sketch.d -> src/geom/misc/sketch.cpp (module geom.misc.sketch).
//
// Definitions in D source order: D:99-168 (constructor), D:170 (~this), D:186
// (toString), and then every method in the order the module declares them, so a reviewer
// can walk the two files side by side. The non-1:1 points are listed in the header; two
// more belong here:
//   * D's default-argument canvas is a fresh `[0.0, 0.0, 120.0, 120.0]` per call; the
//     port points a Slice at a read-only file-scope array of the same four values. No
//     reachable path writes through canvas_mm (the constructor only reads it), so the
//     sharing is unobservable.
//   * D dereferences `svg` (a class handle) in every renderer method without checking
//     that start() ran; that is a NullPointerError in D and a null shared_ptr
//     dereference in C++. Every gated call sequence calls start() first, so no guard was
//     added — adding one would invent a branch D does not have.

#include "geom/misc/sketch.hpp"

#include <array>
#include <cstddef>
#include <memory>
#include <string>

#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/math.hpp"
#include "eil/number.hpp"

namespace eil {
namespace geom {

namespace {

// D:102 / D:111 `!find(names, name).empty` — std.algorithm.find over a string[] with a
// string needle is a linear equality search, so the empty result means "not present".
template <size_t N> bool is_known_name(const std::array<std::string, N>& names,
                                       const std::string& name) {
    for (const std::string& n : names) {
        if (n == name) return true;
    }
    return false;
}

} // namespace

// D:99-168. D:99's `double[] canvas_mm = [0.0, 0.0, 120.0, 120.0]` default argument is
// the const-Array parameter declared in the header; the body only reads canvas_mm, exactly
// as D's does (backlog T-926 item 2 — three delegating overloads used to stand in for it).
Sketch::Sketch(const std::string& renderer_name, const std::string& projection_name,
               const eil::Array<double>& canvas_mm) {
    if (!is_known_name(rendererNames, renderer_name)) {
        throw eil::ExceptionBase("Unknown renderer: " + renderer_name, __FILE__, __LINE__);
    }
    // D:103-107 switch (string) -> if/else chain in the same case order.
    if (renderer_name == "svg") {
        myRenderer = Renderer::svg;
    } else if (renderer_name == "xplot") {
        myRenderer = Renderer::xplot;
    } else {
        myRenderer = Renderer::svg;
    }
    if (!is_known_name(projectionNames, projection_name)) {
        throw eil::ExceptionBase("Unknown projection: " + projection_name, __FILE__,
                                 __LINE__);
    }
    // D:112-117, same case order.
    if (projection_name == "xyortho") {
        myProjection = Projection::xyortho;
    } else if (projection_name == "isometric") {
        myProjection = Projection::isometric;
    } else if (projection_name == "oblique") {
        myProjection = Projection::oblique;
    } else {
        myProjection = Projection::xyortho;
    }
    set_to_identity(proj_mat);
    set_to_identity(view_mat); // consistent with default view
    // D:123 `final switch` — every enumerator, deliberately no default: label.
    switch (myProjection) {
    case Projection::xyortho:
        proj_mat[2][2] = 0.0; // x,y unchanged, z eliminated
        break;
    case Projection::isometric: {
        // D:128-129. Measured at ldc2 -O0 (probe rows "s1 cos folded"/"s1 sin folded"
        // and the stored matrix, "s4 proj isometric"): the literal argument
        // 30.0/180.0*PI folds to 0x3fe0c152382d7366 while the same expression at run
        // time gives 0x3fe0c152382d7365, and cos/sin of the folded argument are
        // 0x3febb67ae8584caa and 0x3fe0000000000000 (0.5), one ULP away from what
        // libm returns for the run-time argument (0x3febb67ae8584cab, 0x3fdfffffffffffff).
        // What D stores in proj_mat is the FOLDED pair (T-909), so those are the bits
        // written here; the same treatment svg.cpp:41 gives the folded 90.0/25.4.
        // Nothing here evaluates a sin/cos pair, so T-920's sincos fusion has no pair to
        // fuse and the values are -O-independent.
        const double cos30 = 0x1.bb67ae8584caap-1; // cos(30.0/180.0*PI), ldc2's fold
        const double sin30 = 0.5;                  // sin(30.0/180.0*PI), ldc2's fold
        proj_mat[0][0] = cos30;
        proj_mat[0][1] = 0.0;
        proj_mat[0][2] = -cos30;
        proj_mat[0][3] = 0.0;
        proj_mat[1][0] = -sin30;
        proj_mat[1][1] = 1.0;
        proj_mat[1][2] = -sin30;
        proj_mat[1][3] = 0.0;
        proj_mat[2][0] = 0.0;
        proj_mat[2][1] = 0.0;
        proj_mat[2][2] = 0.0;
        proj_mat[2][3] = 0.0;
        proj_mat[3][0] = 0.0;
        proj_mat[3][1] = 0.0;
        proj_mat[3][2] = 0.0;
        proj_mat[3][3] = 1.0;
        break;
    }
    case Projection::oblique: {
        // D:136. Measured: ldc2's fold of 1.0/(2.0*sqrt(2.0)) equals the run-time value
        // bit for bit (0x3fd6a09e667f3bcc, probe rows "s1 zfactor folded"/"runtime"), so
        // D's expression is kept rather than its bits.
        const double zfactor = 1.0 / (2.0 * eil::sqrt(2.0));
        proj_mat[0][0] = 1.0;
        proj_mat[0][1] = 0.0;
        proj_mat[0][2] = -zfactor;
        proj_mat[0][3] = 0.0;
        proj_mat[1][0] = 0.0;
        proj_mat[1][1] = 1.0;
        proj_mat[1][2] = -zfactor;
        proj_mat[1][3] = 0.0;
        proj_mat[2][0] = 0.0;
        proj_mat[2][1] = 0.0;
        proj_mat[2][2] = 0.0;
        proj_mat[2][3] = 0.0;
        proj_mat[3][0] = 0.0;
        proj_mat[3][1] = 0.0;
        proj_mat[3][2] = 0.0;
        proj_mat[3][3] = 1.0;
    }
    }
    // Set corner positions (in mm) for the canvas.
    canvas.set(canvas_mm[0], canvas_mm[1], canvas_mm[2], canvas_mm[3]);
    // For the model coordinates, assume unit space because we don't yet know better.
    viewport.set(0.0, 0.0, 1.0, 1.0);
    // D:146 final switch, no default: label.
    switch (myRenderer) {
    case Renderer::svg:
        // We're good for now, since we'll just be writing to a file
        // for each SVG plot.
        break;
    case Renderer::xplot:
#ifdef EIL_WITH_LIBPLOT
        // D:152-163 — the with_libplot variant, never compiled on this machine (T-921).
        // We need to get the Xplotter started up.
        {
            plPlotterParams* params = pl_newplparams();
            std::string bitmapsize = eil::format(
                "%dx%d", static_cast<int>(canvas.width * dpi / 25.4),
                       static_cast<int>(canvas.height * dpi / 25.4));
            pl_setplparam(params, "BITMAPSIZE", (void*)bitmapsize.c_str());
            pl_setplparam(params, "VANISH_ON_DELETE", (void*)"yes");
            pl_setplparam(params, "USE_DOUBLE_BUFFERING", (void*)"yes");
            // Create an X Plotter with the specified parameters.
            if ((myXplotter = pl_newpl_r("X", stdin, stdout, stderr, params)) == nullptr) {
                // D's `new Error`: message only, no file/line (measured rule).
                throw eil::ErrorBase("Couldn't create X Plotter.");
            }
        }
#else
        // D:164-166, the else branch of the version — this one IS compiled and IS gated
        // (baseline row "s3 build [xplot]/[xyortho] ok=0 isError=0").
        throw eil::ExceptionBase(
            "You asked for xplot renderer but libplot is not included.", __FILE__,
            __LINE__);
#endif
        break;
    }
} // end this

// D:170-184
Sketch::~Sketch() {
    // Cleanup
    switch (myRenderer) {
    case Renderer::svg:
        // Do nothing
        break;
    case Renderer::xplot:
#ifdef EIL_WITH_LIBPLOT
        if (pl_deletepl_r(myXplotter) < 0) {
            // The trailing newline is part of D's message bytes.
            throw eil::ErrorBase("Couldn't delete X Plotter\n");
        }
#endif
        break;
    }
}

// D:186-196
std::string Sketch::toString() const {
    std::string str = "Sketch(";
    str += "renderer=\"" + rendererNames[static_cast<size_t>(myRenderer)] + "\"";
    str += ", projection=\"" + projectionNames[static_cast<size_t>(myProjection)] + "\"";
    str += ", eye=" + eye.toString();
    str += ", centre=" + centre.toString();
    str += ", up=" + up.toString();
    str += ")";
    return str;
}

// D:198-204
void Sketch::set_to_identity(double (&mat)[4][4]) {
    for (size_t i = 0; i < 4; ++i) {
        for (size_t j = 0; j < 4; ++j) {
            mat[i][j] = 0.0;
        }
        mat[i][i] = 1.0;
    }
}

// D:206-214
void Sketch::matrix_vector_multiply(double (&mat)[4][4], double (&vec)[4]) {
    double result[4];
    for (size_t i = 0; i < 4; ++i) {
        result[i] = 0.0;
        for (size_t j = 0; j < 4; ++j) {
            result[i] += mat[i][j] * vec[j];
        }
    }
    for (size_t i = 0; i < 4; ++i) {
        vec[i] = result[i];
    }
}

// D:216-231
void Sketch::apply_transform(const std::string& transform_name, Vector3& p, double& w) {
    double ph[4]; // homogeneous coordinate representation
    ph[0] = re(p.x);
    ph[1] = re(p.y);
    ph[2] = re(p.z);
    ph[3] = w;
    if (transform_name == "view") {
        matrix_vector_multiply(view_mat, ph);
    } else if (transform_name == "projection") {
        matrix_vector_multiply(proj_mat, ph);
    }
    // D:227-229 default: do nothing.
    p.set(ph[0], ph[1], ph[2]);
    w = ph[3];
} // end apply_transform

// D:233-251. Set the view matrix so that we have a new view of the 3D model.
// In the new coordinates, we are looking back along the znew-axis,
// with ynew up and xnew toward the right.
// The three parameters shadow the members of the same name, as in D: the members are
// assigned from the parameters first, and the arithmetic below then reads the
// parameters — which are the same values, so the shadowing is faithful, not a bug.
void Sketch::look_at(const Vector3 eye, const Vector3 centre, const Vector3 up) {
    this->eye = eye;
    this->centre = centre;
    this->up = up;
    Vector3 znew = (eye - centre);
    znew.normalize();
    Vector3 xnew = (cross(up, znew));
    xnew.normalize();
    Vector3 ynew = (cross(znew, xnew));
    ynew.normalize();
    view_mat[0][0] = re(xnew.x);
    view_mat[0][1] = re(xnew.y);
    view_mat[0][2] = re(xnew.z);
    view_mat[1][0] = re(ynew.x);
    view_mat[1][1] = re(ynew.y);
    view_mat[1][2] = re(ynew.z);
    view_mat[2][0] = re(znew.x);
    view_mat[2][1] = re(znew.y);
    view_mat[2][2] = re(znew.z);
    view_mat[0][3] = -(re(dot(centre, xnew)));
    view_mat[1][3] = -(re(dot(centre, ynew)));
    view_mat[2][3] = -(re(dot(centre, znew)));
    view_mat[3][0] = 0.0;
    view_mat[3][1] = 0.0;
    view_mat[3][2] = 0.0;
    view_mat[3][3] = 1.0;
} // end look_at()

// D:253-282
void Sketch::start(const std::string& file_name) {
    switch (myRenderer) {
    case Renderer::svg:
        svg = std::make_shared<SVGContext>(canvas.width, canvas.height, "mm", title,
                                           description);
        svg->open(file_name);
        break;
    case Renderer::xplot:
#ifdef EIL_WITH_LIBPLOT
        // D:261-280 — with_libplot variant, never compiled (T-921).
        if (pl_openpl_r(myXplotter) < 0) {
            throw eil::ErrorBase("Couldn't open X Plotter.");
        }
        // Specify the virtual-canvas coordinate system,
        // starting at (0,0) bottom-left.
        pl_fspace_r(myXplotter, 0.0, 0.0, canvas.width, canvas.height);
        xplot_line_width = 0.5; // Line thickness in mm.
        xplot_bgcolourname = "white";
        xplot_pencolourname = "black";
        xplot_fillcolourname = "yellow";
        pl_flinewidth_r(myXplotter, xplot_line_width);
        pl_joinmod_r(myXplotter, "round");
        pl_capmod_r(myXplotter, "round");
        pl_pentype_r(myXplotter, 1);
        pl_linemod_r(myXplotter, "solid");
        pl_filltype_r(myXplotter, xplot_greylevel);
        pl_bgcolorname_r(myXplotter, xplot_bgcolourname.c_str());
        pl_pencolorname_r(myXplotter, xplot_pencolourname.c_str());
        pl_fillcolorname_r(myXplotter, xplot_fillcolourname.c_str());
        pl_erase_r(myXplotter);
#endif
        break;
    }
} // end start()

// D:284-297
void Sketch::finish() {
    switch (myRenderer) {
    case Renderer::svg:
        svg->close();
        break;
    case Renderer::xplot:
#ifdef EIL_WITH_LIBPLOT
        // D:291-295 — with_libplot variant, never compiled (T-921).
        if (pl_closepl_r(myXplotter) < 0) {
            throw eil::ErrorBase("Couldn't close X Plotter.");
        }
        eil::writeln("Rendering finished; press ENTER to continue.");
        std::string junk_text;
        std::getline(std::cin, junk_text); // D's readln()
#endif
        break;
    }
} // end finish()

// D:299-313. Sets line width in mm on our virtual canvas.
void Sketch::setLineWidth(double width) {
    switch (myRenderer) {
    case Renderer::svg:
        svg->setLineWidth(width); // SVG canvas was set up in mm.
        break;
    case Renderer::xplot:
#ifdef EIL_WITH_LIBPLOT
        xplot_line_width = width;
        pl_flinewidth_r(myXplotter, width);
#endif
        break;
    }
    return;
} // end setLineWidth()

// D:315-328
void Sketch::setLineColour(const std::string& colour) {
    switch (myRenderer) {
    case Renderer::svg:
        svg->setLineColour(colour);
        break;
    case Renderer::xplot:
#ifdef EIL_WITH_LIBPLOT
        xplot_pencolourname = colour;
        pl_pencolorname_r(myXplotter, colour.c_str());
#endif
        break;
    }
    return;
} // end setLineColour()

// D:330-344
void Sketch::setFillColour(const std::string& colour) {
    switch (myRenderer) {
    case Renderer::svg:
        svg->setFillColour(colour);
        break;
    case Renderer::xplot:
#ifdef EIL_WITH_LIBPLOT
        xplot_fillcolourname = colour;
        pl_filltype_r(myXplotter, xplot_greylevel);
        pl_fillcolorname_r(myXplotter, colour.c_str());
#endif
        break;
    }
    return;
} // end setFillColour()

// D:346-359
void Sketch::clearFillColour() {
    switch (myRenderer) {
    case Renderer::svg:
        svg->clearFillColour();
        break;
    case Renderer::xplot:
#ifdef EIL_WITH_LIBPLOT
        xplot_fillcolourname = "none";
        pl_fillcolorname_r(myXplotter, "none");
#endif
        break;
    }
    return;
} // end clearFillColour()

// D:361-374. Sets length of dashes and gaps, in units appropriate to the renderer.
void Sketch::setDashArray(double dashLength, double gapLength) {
    switch (myRenderer) {
    case Renderer::svg:
        svg->setDashArray(dashLength, gapLength); // in mm
        break;
    case Renderer::xplot:
#ifdef EIL_WITH_LIBPLOT
        // D:369-371: [TODO] — nothing in the variant body.
#endif
        break;
    }
    return;
} // end setDashArray()

// D:376-380. Map from user-space to canvas-space.
double Sketch::toCanvasX(double x) {
    return canvas.x0 + (x - viewport.x0) / viewport.width * canvas.width;
}

// D:382-386. Map from user-space to canvas-space.
double Sketch::toCanvasY(double y) {
    return canvas.y0 + (y - viewport.y0) / viewport.height * canvas.height;
}

// D:388-400
void Sketch::begin_group(const std::string& id, double opacity) {
    switch (myRenderer) {
    case Renderer::svg:
        svg->begin_group(id, opacity);
        break;
    case Renderer::xplot:
        // not applicable
        break;
    }
    return;
}

// D:402-412
void Sketch::end_group() {
    switch (myRenderer) {
    case Renderer::svg:
        svg->end_group();
        break;
    case Renderer::xplot:
        // not applicable
        break;
    }
    return;
}

// D:414-442. Render a line from point 1 to point 2.
void Sketch::line(const Vector3 p0, const Vector3 p1, bool dashed) {
    Vector3 p0tmp = Vector3(p0);
    Vector3 p1tmp = Vector3(p1);
    double w0 = 1.0;
    double w1 = 1.0;
    apply_transform("view", p0tmp, w0);
    apply_transform("view", p1tmp, w1);
    apply_transform("projection", p0tmp, w0);
    apply_transform("projection", p1tmp, w1);
    double x0 = toCanvasX(re(p0tmp.x));
    double y0 = toCanvasY(re(p0tmp.y));
    double x1 = toCanvasX(re(p1tmp.x));
    double y1 = toCanvasY(re(p1tmp.y));
    switch (myRenderer) {
    case Renderer::svg:
        svg->line(x0, y0, x1, y1, dashed);
        break;
    case Renderer::xplot:
#ifdef EIL_WITH_LIBPLOT
        pl_filltype_r(myXplotter, 0);
        pl_pentype_r(myXplotter, 1);
        pl_flinewidth_r(myXplotter, xplot_line_width);
        if (dashed) {
            pl_linemod_r(myXplotter, "longdashed");
        } else {
            pl_linemod_r(myXplotter, "solid");
        }
        pl_fline_r(myXplotter, x0, y0, x1, y1);
        pl_flushpl_r(myXplotter);
#endif
        break;
    }
    return;
} // end line()

// D:444-477
void Sketch::polyline(eil::Slice<Vector3> pa, bool dashed) {
    eil::Array<Vector3> ptmp;
    eil::Array<double> wtmp;
    for (size_t i = 0; i < pa.length(); ++i) {
        ptmp.append(Vector3(pa[i]));
        wtmp.append(1.0);
    }
    for (size_t i = 0; i < ptmp.length(); ++i) {
        apply_transform("view", ptmp[i], wtmp[i]);
        apply_transform("projection", ptmp[i], wtmp[i]);
    }
    eil::Array<double> xlist, ylist;
    for (size_t i = 0; i < ptmp.length(); ++i) {
        xlist.append(toCanvasX(re(ptmp[i].x)));
        ylist.append(toCanvasY(re(ptmp[i].y)));
    }
    switch (myRenderer) {
    case Renderer::svg:
        svg->polyline(xlist.all(), ylist.all(), dashed);
        break;
    case Renderer::xplot:
#ifdef EIL_WITH_LIBPLOT
        pl_filltype_r(myXplotter, 0);
        pl_pentype_r(myXplotter, 1);
        pl_flinewidth_r(myXplotter, xplot_line_width);
        if (dashed) {
            pl_linemod_r(myXplotter, "longdashed");
        } else {
            pl_linemod_r(myXplotter, "solid");
        }
        for (size_t i = 1; i < xlist.length(); ++i) {
            pl_fline_r(myXplotter, xlist[i - 1], ylist[i - 1], xlist[i], ylist[i]);
        }
        pl_flushpl_r(myXplotter);
#endif
        break;
    }
    return;
} // end polyline()

// D:479-524
void Sketch::polygon(eil::Slice<Vector3> pa, bool fill, bool stroke, bool dashed) {
    eil::Array<Vector3> ptmp;
    eil::Array<double> wtmp;
    for (size_t i = 0; i < pa.length(); ++i) {
        ptmp.append(Vector3(pa[i]));
        wtmp.append(1.0);
    }
    for (size_t i = 0; i < ptmp.length(); ++i) {
        apply_transform("view", ptmp[i], wtmp[i]);
        apply_transform("projection", ptmp[i], wtmp[i]);
    }
    eil::Array<double> xlist, ylist;
    for (size_t i = 0; i < ptmp.length(); ++i) {
        xlist.append(toCanvasX(re(ptmp[i].x)));
        ylist.append(toCanvasY(re(ptmp[i].y)));
    }
    switch (myRenderer) {
    case Renderer::svg:
        svg->polygon(xlist.all(), ylist.all(), fill, stroke, dashed);
        break;
    case Renderer::xplot:
#ifdef EIL_WITH_LIBPLOT
        if (fill) {
            pl_filltype_r(myXplotter, xplot_greylevel);
        } else {
            pl_filltype_r(myXplotter, 0);
        }
        if (stroke) {
            pl_pentype_r(myXplotter, 1);
            pl_flinewidth_r(myXplotter, xplot_line_width);
        } else {
            pl_pentype_r(myXplotter, 0);
        }
        if (dashed) {
            pl_linemod_r(myXplotter, "longdashed");
        } else {
            pl_linemod_r(myXplotter, "solid");
        }
        pl_fmove_r(myXplotter, xlist[0], ylist[0]);
        for (size_t i = 1; i < xlist.length(); ++i) {
            pl_fline_r(myXplotter, xlist[i - 1], ylist[i - 1], xlist[i], ylist[i]);
        }
        // D:517 `xlist[$-1]` is the last element.
        pl_fline_r(myXplotter, xlist[0], ylist[0], xlist[xlist.length() - 1],
                   ylist[ylist.length() - 1]);
        pl_closepath_r(myXplotter);
        pl_endpath_r(myXplotter);
        pl_flushpl_r(myXplotter);
#endif
        break;
    }
    return;
} // end polygon()

// D:526-552
void Sketch::arc(const Vector3 p0, const Vector3 p1, const Vector3 pc, bool dashed) {
    // This is a purely 2D xy-plane function.
    double x0 = toCanvasX(re(p0.x));
    double y0 = toCanvasY(re(p0.y));
    double x1 = toCanvasX(re(p1.x));
    double y1 = toCanvasY(re(p1.y));
    double xc = toCanvasX(re(pc.x));
    double yc = toCanvasY(re(pc.y));
    switch (myRenderer) {
    case Renderer::svg:
        svg->arc(x0, y0, x1, y1, xc, yc, dashed);
        break;
    case Renderer::xplot:
#ifdef EIL_WITH_LIBPLOT
        pl_filltype_r(myXplotter, 0);
        pl_pentype_r(myXplotter, 1);
        pl_flinewidth_r(myXplotter, xplot_line_width);
        if (dashed) {
            pl_linemod_r(myXplotter, "longdashed");
        } else {
            pl_linemod_r(myXplotter, "solid");
        }
        // D:547 passes (xc, yc, x0, y0, x1, y1) to pl_farc_r in that order.
        pl_farc_r(myXplotter, xc, yc, x0, y0, x1, y1);
        pl_flushpl_r(myXplotter);
#endif
        break;
    }
    return;
} // end arc()

// D:554-587
void Sketch::circle(const Vector3 pc, double r, bool fill, bool stroke, bool dashed) {
    // This is a purely 2D xy-plane function.
    // r is in mm on the canvas.
    double xc = toCanvasX(re(pc.x));
    double yc = toCanvasY(re(pc.y));
    switch (myRenderer) {
    case Renderer::svg:
        svg->circle(xc, yc, r, fill, stroke, dashed);
        break;
    case Renderer::xplot:
#ifdef EIL_WITH_LIBPLOT
        if (fill) {
            pl_filltype_r(myXplotter, xplot_greylevel);
        } else {
            pl_filltype_r(myXplotter, 0);
        }
        if (stroke) {
            pl_pentype_r(myXplotter, 1);
            pl_flinewidth_r(myXplotter, xplot_line_width);
        } else {
            pl_pentype_r(myXplotter, 0);
        }
        if (dashed) {
            pl_linemod_r(myXplotter, "longdashed");
        } else {
            pl_linemod_r(myXplotter, "solid");
        }
        pl_fcircle_r(myXplotter, xc, yc, r);
        pl_flushpl_r(myXplotter);
#endif
        break;
    }
    return;
} // end circle()

// D:589-624
void Sketch::bezier3(const Vector3 p0, const Vector3 p1, const Vector3 p2,
                     const Vector3 p3, bool dashed) {
    Vector3 p0tmp = Vector3(p0);
    Vector3 p1tmp = Vector3(p1);
    // D:593-594 really does build p2tmp/p3tmp from p0/p1, not from p2/p3. Kept as
    // written: this is the module's own behaviour, and "fixing" it would change every
    // transformed bezier3 output (reports/backlog.md is where a D-side fix belongs).
    Vector3 p2tmp = Vector3(p0);
    Vector3 p3tmp = Vector3(p1);
    double w0 = 1.0;
    double w1 = 1.0;
    double w2 = 1.0;
    double w3 = 1.0;
    apply_transform("view", p0tmp, w0);
    apply_transform("view", p1tmp, w1);
    apply_transform("view", p2tmp, w2);
    apply_transform("view", p3tmp, w3);
    apply_transform("projection", p0tmp, w0);
    apply_transform("projection", p1tmp, w1);
    apply_transform("projection", p2tmp, w2);
    apply_transform("projection", p3tmp, w3);
    double x0 = toCanvasX(re(p0tmp.x));
    double y0 = toCanvasY(re(p0tmp.y));
    double x1 = toCanvasX(re(p1tmp.x));
    double y1 = toCanvasY(re(p1tmp.y));
    double x2 = toCanvasX(re(p2tmp.x));
    double y2 = toCanvasY(re(p2tmp.y));
    double x3 = toCanvasX(re(p3tmp.x));
    double y3 = toCanvasY(re(p3tmp.y));
    switch (myRenderer) {
    case Renderer::svg:
        svg->bezier3(x0, y0, x1, y1, x2, y2, x3, y3, dashed);
        break;
    case Renderer::xplot:
#ifdef EIL_WITH_LIBPLOT
        pl_filltype_r(myXplotter, 0);
        pl_pentype_r(myXplotter, 1);
        pl_flinewidth_r(myXplotter, xplot_line_width);
        if (dashed) {
            pl_linemod_r(myXplotter, "longdashed");
        } else {
            pl_linemod_r(myXplotter, "solid");
        }
        pl_fbezier3_r(myXplotter, x0, y0, x1, y1, x2, y2, x3, y3);
        pl_flushpl_r(myXplotter);
#endif
        break;
    }
    return;
} // end bezier3()

// D:626-663
void Sketch::text(const Vector3 p, const std::string& textString, const double angle,
                  const std::string& anchor, int fontSize, const std::string& colour,
                  const std::string& fontFamily) {
    Vector3 p_tmp = Vector3(p);
    double w_p = 1.0;
    double w_dir = 1.0;
    double w_n = 1.0;
    apply_transform("view", p_tmp, w_p);
    apply_transform("projection", p_tmp, w_p);
    double xp = toCanvasX(re(p_tmp.x));
    double yp = toCanvasY(re(p_tmp.y));
    // D:633 declares w_dir and w_n and never uses them; kept, because deleting them
    // would be an edit to the algorithm rather than to the spelling.
    (void)w_dir;
    (void)w_n;
    switch (myRenderer) {
    case Renderer::svg:
        svg->text(xp, yp, textString, angle, anchor, fontSize, colour, fontFamily);
        break;
    case Renderer::xplot:
#ifdef EIL_WITH_LIBPLOT
        pl_pencolorname_r(myXplotter, colour.c_str());
        // Convert font size from points to virtual-canvas millimetres.
        double fontHeight = fontSize / 72.0 * 25.4;
        pl_ftextangle_r(myXplotter, angle);
        pl_fmove_r(myXplotter, xp, yp);
        pl_fontname_r(myXplotter, fontFamily.c_str());
        pl_ffontsize_r(myXplotter, fontHeight);
        int horizJust, vertJust;
        if (anchor == "start") {
            horizJust = 'l';
            vertJust = 'x';
        } else if (anchor == "middle") {
            horizJust = 'c';
            vertJust = 'x';
        } else if (anchor == "end") {
            horizJust = 'r';
            vertJust = 'x';
        } else {
            horizJust = 'l';
            vertJust = 'x';
        }
        pl_alabel_r(myXplotter, horizJust, vertJust, textString.c_str());
        pl_flushpl_r(myXplotter);
        pl_pencolorname_r(myXplotter, xplot_pencolourname.c_str());
#endif
        break;
    }
    return;
} // end text()

// D:665-706
void Sketch::dotlabel(const Vector3 p, const std::string& label,
                      const std::string& anchor, double dotSize, int fontSize,
                      const std::string& colour, const std::string& fontFamily) {
    Vector3 p_tmp = Vector3(p);
    double w_p = 1.0;
    apply_transform("view", p_tmp, w_p);
    apply_transform("projection", p_tmp, w_p);
    double xp = toCanvasX(re(p_tmp.x));
    double yp = toCanvasY(re(p_tmp.y));
    switch (myRenderer) {
    case Renderer::svg:
        svg->dotlabel(xp, yp, label, anchor, dotSize, fontSize, colour, fontFamily);
        break;
    case Renderer::xplot:
#ifdef EIL_WITH_LIBPLOT
        pl_pencolorname_r(myXplotter, colour.c_str());
        pl_fillcolorname_r(myXplotter, colour.c_str());
        pl_fcircle_r(myXplotter, xp, yp, dotSize / 2);
        if (label.length() > 0) {
            // Convert font size from points to virtual-canvas millimetres.
            double fontHeight = fontSize / 72.0 * 25.4;
            pl_ftextangle_r(myXplotter, 0.0);
            pl_fmove_r(myXplotter, xp, yp + 0.75 * dotSize);
            pl_fontname_r(myXplotter, fontFamily.c_str());
            pl_ffontsize_r(myXplotter, fontHeight);
            int horizJust, vertJust;
            if (anchor == "start") {
                horizJust = 'l';
                vertJust = 'x';
            } else if (anchor == "middle") {
                horizJust = 'c';
                vertJust = 'x';
            } else if (anchor == "end") {
                horizJust = 'r';
                vertJust = 'x';
            } else {
                horizJust = 'l';
                vertJust = 'x';
            }
            pl_alabel_r(myXplotter, horizJust, vertJust, label.c_str());
        }
        pl_flushpl_r(myXplotter);
        pl_pencolorname_r(myXplotter, xplot_pencolourname.c_str());
        pl_fillcolorname_r(myXplotter, xplot_fillcolourname.c_str());
#endif
        break;
    }
    return;
} // end dotlabel()

// ----------------------------------------------------------------------
// The following methods should be independent of the particular renderer.
// ----------------------------------------------------------------------

// D:712-772
void Sketch::rule(const std::string& direction, double vmin, double vmax, double vtic,
                  Vector3 anchorPoint, double ticMarkSize,
                  const std::string& numberFormat, double textOffset, double textAngle,
                  int fontSize) {
    Vector3 p0 = Vector3(anchorPoint);
    Vector3 p1 = Vector3(anchorPoint);
    Vector3 dp = Vector3(0.0, 0.0, 0.0);
    Vector3 dpTic = Vector3(0.0, 0.0, 0.0);
    int n = static_cast<int>(1.000001 * (vmax - vmin) / vtic); // be sure to get final tic
                                                                // mark
    if (n > 50) {
        eil::writeln("You have asked for a lot of tic marks: ", n);
    }
    Vector3 dpText = Vector3(0.0, 0.0, 0.0);
    std::string textAnchor;
    // D:734-755 switch (string) -> if/else chain in the same case order.
    if (direction == "x") {
        p0.x = vmin;
        p1.x = vmax;
        dp.x = vtic;
        dpTic.y = -ticMarkSize; // tic marks go in -y direction
        dpText.y = -textOffset;
        textAnchor = "middle";
    } else if (direction == "y") {
        p0.y = vmin;
        p1.y = vmax;
        dp.y = vtic;
        dpTic.x = -ticMarkSize; // tic marks go in -x direction
        dpText.x = -textOffset;
        textAnchor = "end";
    } else if (direction == "z") {
        p0.z = vmin;
        p1.z = vmax;
        dp.z = vtic;
        dpTic.x = -ticMarkSize; // tic marks go in -x direction
        dpText.x = -textOffset;
        textAnchor = "end";
    } else {
        throw eil::ExceptionBase("invalid direction for rule: " + direction, __FILE__,
                                 __LINE__);
    }
    begin_group(); // Keep tic marks together
    line(p0, p1);
    // D:758 `foreach (i; 0 .. n+1)` over an int bound: a negative n yields an empty
    // range, so the loops must run zero times, not wrap.
    for (int i = 0; i < n + 1; ++i) {
        Vector3 p = p0 + i * dp;
        Vector3 p2 = p + dpTic;
        line(p, p2);
    }
    end_group();
    // Label the tic marks
    for (int i = 0; i < n + 1; ++i) {
        Vector3 p = p0 + i * dp;
        Vector3 p3 = p + dpText;
        double value = vmin + i * vtic;
        std::string myText = eil::format(numberFormat, value);
        text(p3, myText, textAngle, textAnchor, fontSize);
    }
} // end rule()

// D:775-781. Render functions for our geometric entities used in the flow simulation.
void Sketch::render(const PathPtr& pth, bool dashed, size_t n) {
    eil::Array<Vector3> psample;
    double dt = 1.0 / n;
    for (size_t i = 0; i < n + 1; ++i) {
        psample.append((*pth)(i * dt));
    }
    polyline(psample.all(), dashed);
}

// D:783-818
void Sketch::render(const ParametricSurface::Ptr& surf, bool fill, bool stroke,
                   bool dashed, size_t n, bool facets) {
    double dt = 1.0 / n;
    // Sample boundaries, progressing counter-clockwise.
    eil::Array<Vector3> p_boundary;
    for (size_t i = 0; i < n + 1; ++i) {
        p_boundary.append((*surf)(i * dt, 0.0));
    } // south boundary
    for (size_t i = 1; i < n + 1; ++i) {
        p_boundary.append((*surf)(1.0, i * dt));
    } // east
    for (size_t i = 1; i < n + 1; ++i) {
        p_boundary.append((*surf)(1.0 - i * dt, 1.0));
    } // north
    for (size_t i = 1; i < n + 1; ++i) {
        p_boundary.append((*surf)(0.0, 1.0 - i * dt));
    } // west
    if (fill) {
        if (facets) {
            // Fill in the surface as (many) quadrilateral patches.
            begin_group();
            for (size_t i = 0; i < n; ++i) {
                double r0 = i * dt;
                double r1 = r0 + dt;
                for (size_t j = 0; j < n; ++j) {
                    double s0 = j * dt;
                    double s1 = s0 + dt;
                    eil::Array<Vector3> psample;
                    psample.append((*surf)(r0, s0));
                    psample.append((*surf)(r1, s0));
                    psample.append((*surf)(r1, s1));
                    psample.append((*surf)(r0, s1));
                    polygon(psample.all(), fill, false, false);
                }
            }
            end_group();
        } else {
            // Fill a single polygon boundary.
            // This will allow a much smaller SVG file for 2D renderings.
            polygon(p_boundary.all(), fill, false, false);
        }
    } // end if fill
    if (stroke) {
        polygon(p_boundary.all(), false, stroke, dashed);
    }
} // end render (ParametricSurface)

// D:820-962
void Sketch::render(const ParametricVolume::Ptr& pvol, bool fill, bool stroke,
                    bool dashed, size_t n, bool facets) {
    // Render the bounding surface and edges of the ParametricVolume.
    double dt = 1.0 / n;
    // Sample boundaries, progressing counter-clockwise.
    eil::Array<Vector3> p_bottom;
    for (size_t i = 0; i < n + 1; ++i) {
        p_bottom.append((*pvol)(i * dt, 0.0, 0.0));
    } // south edge
    for (size_t i = 1; i < n + 1; ++i) {
        p_bottom.append((*pvol)(1.0, i * dt, 0.0));
    } // east
    for (size_t i = 1; i < n + 1; ++i) {
        p_bottom.append((*pvol)(1.0 - i * dt, 1.0, 0.0));
    } // north
    for (size_t i = 1; i < n + 1; ++i) {
        p_bottom.append((*pvol)(0.0, 1.0 - i * dt, 0.0));
    } // west
    eil::Array<Vector3> p_top;
    for (size_t i = 0; i < n + 1; ++i) {
        p_top.append((*pvol)(i * dt, 0.0, 1.0));
    } // south edge
    for (size_t i = 1; i < n + 1; ++i) {
        p_top.append((*pvol)(1.0, i * dt, 1.0));
    } // east
    for (size_t i = 1; i < n + 1; ++i) {
        p_top.append((*pvol)(1.0 - i * dt, 1.0, 1.0));
    } // north
    for (size_t i = 1; i < n + 1; ++i) {
        p_top.append((*pvol)(0.0, 1.0 - i * dt, 1.0));
    } // west
    eil::Array<Vector3> p_west;
    for (size_t i = 0; i < n + 1; ++i) {
        p_west.append((*pvol)(0.0, 0.0, i * dt));
    }
    for (size_t i = 1; i < n + 1; ++i) {
        p_west.append((*pvol)(0.0, i * dt, 1.0));
    }
    for (size_t i = 1; i < n + 1; ++i) {
        p_west.append((*pvol)(0.0, 1.0, 1.0 - i * dt));
    }
    for (size_t i = 1; i < n + 1; ++i) {
        p_west.append((*pvol)(0.0, 1.0 - i * dt, 0.0));
    }
    eil::Array<Vector3> p_east;
    for (size_t i = 0; i < n + 1; ++i) {
        p_east.append((*pvol)(1.0, 0.0, i * dt));
    }
    for (size_t i = 1; i < n + 1; ++i) {
        p_east.append((*pvol)(1.0, i * dt, 1.0));
    }
    for (size_t i = 1; i < n + 1; ++i) {
        p_east.append((*pvol)(1.0, 1.0, 1.0 - i * dt));
    }
    for (size_t i = 1; i < n + 1; ++i) {
        p_east.append((*pvol)(1.0, 1.0 - i * dt, 0.0));
    }
    eil::Array<Vector3> p_south;
    for (size_t i = 0; i < n + 1; ++i) {
        p_south.append((*pvol)(i * dt, 0.0, 0.0));
    }
    for (size_t i = 1; i < n + 1; ++i) {
        p_south.append((*pvol)(1.0, 0.0, i * dt));
    }
    for (size_t i = 1; i < n + 1; ++i) {
        p_south.append((*pvol)(1.0 - i * dt, 0.0, 1.0));
    }
    for (size_t i = 1; i < n + 1; ++i) {
        p_south.append((*pvol)(0.0, 0.0, 1.0 - i * dt));
    }
    eil::Array<Vector3> p_north;
    for (size_t i = 0; i < n + 1; ++i) {
        p_north.append((*pvol)(i * dt, 1.0, 0.0));
    }
    for (size_t i = 1; i < n + 1; ++i) {
        p_north.append((*pvol)(1.0, 1.0, i * dt));
    }
    for (size_t i = 1; i < n + 1; ++i) {
        p_north.append((*pvol)(1.0 - i * dt, 1.0, 1.0));
    }
    for (size_t i = 1; i < n + 1; ++i) {
        p_north.append((*pvol)(0.0, 1.0, 1.0 - i * dt));
    }
    if (fill) {
        if (facets) {
            begin_group(); // hold all surfaces together
            // Fill in the bottom surface as (many) quadrilateral patches.
            begin_group();
            for (size_t i = 0; i < n; ++i) {
                double r0 = i * dt;
                double r1 = r0 + dt;
                for (size_t j = 0; j < n; ++j) {
                    double s0 = j * dt;
                    double s1 = s0 + dt;
                    eil::Array<Vector3> psample;
                    psample.append((*pvol)(r0, s0, 0.0));
                    psample.append((*pvol)(r1, s0, 0.0));
                    psample.append((*pvol)(r1, s1, 0.0));
                    psample.append((*pvol)(r0, s1, 0.0));
                    polygon(psample.all(), fill, false, false);
                }
            }
            end_group();
            // Fill in the top surface as (many) quadrilateral patches.
            begin_group();
            for (size_t i = 0; i < n; ++i) {
                double r0 = i * dt;
                double r1 = r0 + dt;
                for (size_t j = 0; j < n; ++j) {
                    double s0 = j * dt;
                    double s1 = s0 + dt;
                    eil::Array<Vector3> psample;
                    psample.append((*pvol)(r0, s0, 1.0));
                    psample.append((*pvol)(r1, s0, 1.0));
                    psample.append((*pvol)(r1, s1, 1.0));
                    psample.append((*pvol)(r0, s1, 1.0));
                    polygon(psample.all(), fill, false, false);
                }
            }
            end_group();
            // Fill in the west surface as (many) quadrilateral patches.
            begin_group();
            for (size_t i = 0; i < n; ++i) {
                double t0 = i * dt;
                double t1 = t0 + dt;
                for (size_t j = 0; j < n; ++j) {
                    double s0 = j * dt;
                    double s1 = s0 + dt;
                    eil::Array<Vector3> psample;
                    psample.append((*pvol)(0.0, s0, t0));
                    psample.append((*pvol)(0.0, s0, t1));
                    psample.append((*pvol)(0.0, s1, t1));
                    psample.append((*pvol)(0.0, s1, t0));
                    polygon(psample.all(), fill, false, false);
                }
            }
            end_group();
            // Fill in the east surface as (many) quadrilateral patches.
            begin_group();
            for (size_t i = 0; i < n; ++i) {
                double t0 = i * dt;
                double t1 = t0 + dt;
                for (size_t j = 0; j < n; ++j) {
                    double s0 = j * dt;
                    double s1 = s0 + dt;
                    eil::Array<Vector3> psample;
                    psample.append((*pvol)(1.0, s0, t0));
                    psample.append((*pvol)(1.0, s0, t1));
                    psample.append((*pvol)(1.0, s1, t1));
                    psample.append((*pvol)(1.0, s1, t0));
                    polygon(psample.all(), fill, false, false);
                }
            }
            end_group();
            // Fill in the south surface as (many) quadrilateral patches.
            begin_group();
            for (size_t i = 0; i < n; ++i) {
                double r0 = i * dt;
                double r1 = r0 + dt;
                for (size_t j = 0; j < n; ++j) {
                    double t0 = j * dt;
                    double t1 = t0 + dt;
                    eil::Array<Vector3> psample;
                    psample.append((*pvol)(r0, 0.0, t0));
                    psample.append((*pvol)(r1, 0.0, t0));
                    psample.append((*pvol)(r1, 0.0, t1));
                    psample.append((*pvol)(r0, 0.0, t1));
                    polygon(psample.all(), fill, false, false);
                }
            }
            end_group();
            // Fill in the north surface as (many) quadrilateral patches.
            begin_group();
            for (size_t i = 0; i < n; ++i) {
                double r0 = i * dt;
                double r1 = r0 + dt;
                for (size_t j = 0; j < n; ++j) {
                    double t0 = j * dt;
                    double t1 = t0 + dt;
                    eil::Array<Vector3> psample;
                    psample.append((*pvol)(r0, 1.0, t0));
                    psample.append((*pvol)(r1, 1.0, t0));
                    psample.append((*pvol)(r1, 1.0, t1));
                    psample.append((*pvol)(r0, 1.0, t1));
                    polygon(psample.all(), fill, false, false);
                }
            }
            end_group();
            end_group(); // for all surfaces, together
        } else {
            // Fill a single polygon boundary for each bounding surface.
            // This will allow a much smaller SVG file for 2D renderings.
            begin_group();
            polygon(p_bottom.all(), fill, false, false);
            polygon(p_top.all(), fill, false, false);
            polygon(p_west.all(), fill, false, false);
            polygon(p_east.all(), fill, false, false);
            polygon(p_south.all(), fill, false, false);
            polygon(p_north.all(), fill, false, false);
            end_group();
        }
    } // end if fill
    if (stroke) {
        begin_group();
        polygon(p_bottom.all(), false, stroke, dashed);
        polygon(p_top.all(), false, stroke, dashed);
        polygon(p_west.all(), false, stroke, dashed);
        polygon(p_east.all(), false, stroke, dashed);
        polygon(p_south.all(), false, stroke, dashed);
        polygon(p_north.all(), false, stroke, dashed);
        end_group();
    }
} // end render (ParametricVolume)

} // namespace geom
} // namespace eil
