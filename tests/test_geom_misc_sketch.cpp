// tests/test_geom_misc_sketch.cpp
//
// Two layers, both required (the pattern of tests/test_geom_misc_svg.cpp):
//
//  A. D's unittest blocks. sketch.d has NONE (measured: `grep -c unittest
//     /home/tang/packages/gdtk/src/geom/misc/sketch.d` = 0, and reports/wave-02-sketch-
//     brief.md section 2 records the same count), so there is no assertion to port. This
//     layer asserts only what the D reference was measured to do at the module boundary
//     (a default Sketch prints the D toString(), a document opens with the D header and
//     the D 425.20 pixel width, the three Exception texts, and the Error/Exception
//     sibling split), and the real gate is layer B plus the demo byte comparison.
//     Recorded in provenance/fidelity.yml as "no D unittest".
//
//  B. a line-for-line replay of probes/geom/misc/probe_sketch.d against
//     baselines/e587b235/probes/geom_misc_sketch.txt: 682 rows, captured with ldc2 at
//     -O0 (no -O flag). 199 rows are bit patterns of quantities sketch.d computes
//     (projection and view matrices, transformed points, canvas mappings, the .init
//     NaNs, the to!int tick counts) and 483 are the emitted text of six SVG documents,
//     dumped after all drawing finished. sketch.d is text generation plus a projection
//     matrix, so the criterion is BYTE equality of every row; no tolerance is applied
//     anywhere in this file.
//
// About the S1 rows, so nobody mistakes them for a tautology: "…folded…" rows print the
// constants src/geom/misc/sketch.cpp actually stores (ldc2's own folded cos/sin of
// 30.0/180.0*PI — see the header note in that file), while "…runtime…" rows are
// genuinely recomputed here with eil::cos/eil::sin. Both sides therefore agree on the
// 1 ULP gap between the two forms, and row 66-69 ("s4 proj isometric") is the check that
// the stored constants are what the reference build puts in the matrix.
//
// This test writes skprobe_*.svg into its working directory and reads them back, exactly
// as the D probe does; nothing is compared from the filesystem except through those
// dumps, and the byte count in each "--- doc …" marker row is computed from the file the
// port itself wrote.
//
// Run: ./test_geom_misc_sketch ../baselines/e587b235/probes/geom_misc_sketch.txt
//
// Regenerate the baseline:
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   cd /home/tang/packages/Eilmer
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip \
//     probes/geom/misc/probe_sketch.d -of=/tmp/probe_sk -od/tmp/probe_obj_sk
//   mkdir -p /tmp/skwork && cd /tmp/skwork
//   /tmp/probe_sk > /home/tang/packages/Eilmer/baselines/e587b235/probes/geom_misc_sketch.txt
#include <cctype>
#include <cstdarg>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <unistd.h>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "eil/array.hpp"
#include "eil/eil.hpp"
#include "eil/error.hpp"
#include "eil/math.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/arc.hpp"
#include "geom/misc/sketch.hpp"
#include "geom/surface/coonspatch.hpp"
#include "geom/volume/slabvolume.hpp"
#include "nm/number.hpp"

using eil::geom::Arc;
using eil::geom::CoonsPatch;
using eil::geom::Extents;
using eil::geom::PathPtr;
using eil::geom::Sketch;
using eil::geom::SlabVolume;
using eil::geom::Vector3;

namespace {

int failures = 0;
int checks = 0;

void check(bool cond, const char* what, int line) {
    ++checks;
    if (!cond) {
        ++failures;
        std::printf("FAIL line %d: %s\n", line, what);
    }
}

#define CHECK(c, msg) check((c), msg, __LINE__)

std::vector<std::string> produced;

void emitf(const char* fmt, ...) {
    char buf[65536];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    produced.emplace_back(buf);
}

/// D: probe's bits() — format("%016x", *cast(ulong*)&x).
std::string bits(double x) {
    uint64_t u;
    std::memcpy(&u, &x, 8);
    char buf[24];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(u));
    return buf;
}

void pv(const std::string& tag, double x) {
    emitf("%s v=%.17g bits=%s", tag.c_str(), x, bits(x).c_str());
}

void pvm(const std::string& tag, const double (&m)[4][4]) {
    for (size_t i = 0; i < 4; ++i) {
        emitf("%s r%zu %.17g/%s %.17g/%s %.17g/%s %.17g/%s", tag.c_str(), i, m[i][0],
              bits(m[i][0]).c_str(), m[i][1], bits(m[i][1]).c_str(), m[i][2],
              bits(m[i][2]).c_str(), m[i][3], bits(m[i][3]).c_str());
    }
}

void pvec(const std::string& tag, const double (&v)[4]) {
    emitf("%s %.17g/%s %.17g/%s %.17g/%s %.17g/%s", tag.c_str(), v[0], bits(v[0]).c_str(),
          v[1], bits(v[1]).c_str(), v[2], bits(v[2]).c_str(), v[3], bits(v[3]).c_str());
}

template <typename... A> std::string fmt(const char* f, A&&... a) {
    char buf[128];
    std::snprintf(buf, sizeof buf, f, std::forward<A>(a)...);
    return buf;
}

// Sketch::rule() prints "You have asked for a lot of tic marks: n" itself, through
// eil::writeln straight to stdout (D:730). The corpus row therefore has to be picked up
// from the stream rather than composed here: fd 1 is temporarily redirected into a
// scratch file around the drawing that emits it, and the captured lines are appended to
// the produced list at the position D's own stream put them.
int saved_stdout_fd = -1;

void capture_begin() {
    std::fflush(stdout);
    saved_stdout_fd = ::dup(1);
    if (std::freopen("skprobe_stdout_capture.txt", "w", stdout) == nullptr) {
        std::perror("freopen");
        std::exit(2);
    }
}

void capture_end() {
    std::fflush(stdout);
    ::dup2(saved_stdout_fd, 1);
    ::close(saved_stdout_fd);
    saved_stdout_fd = -1;
    std::ifstream in("skprobe_stdout_capture.txt");
    for (std::string line; std::getline(in, line);) produced.push_back(line);
    std::remove("skprobe_stdout_capture.txt");
}

std::vector<std::string> read_lines(const std::string& name, size_t& bytes) {
    std::ifstream in(name, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string text = ss.str();
    bytes = text.size();
    std::vector<std::string> lines;
    size_t start = 0;
    while (true) {
        size_t nl = text.find('\n', start);
        if (nl == std::string::npos) {
            if (start < text.size()) lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, nl - start));
        start = nl + 1;
    }
    return lines;
}

/// The probe's dump_doc: one stdout line per document line, no trailing empty line.
void dump_doc(const std::string& name) {
    size_t bytes = 0;
    std::vector<std::string> lines = read_lines(name, bytes);
    emitf("--- doc %s bytes=%zu ---", name.c_str(), bytes);
    for (const std::string& l : lines) produced.push_back(l);
}

void build_row(const std::string& rname, const std::string& pname) {
    try {
        auto s = std::make_shared<Sketch>(rname, pname);
        emitf("s3 build [%s]/[%s] ok=1 str=[%s]", rname.c_str(), pname.c_str(),
              s->toString().c_str());
    } catch (const eil::ExceptionBase& e) {
        emitf("s3 build [%s]/[%s] ok=0 isError=0 msg=[%s]", rname.c_str(), pname.c_str(),
              e.msg().c_str());
    } catch (const eil::ErrorBase& e) {
        emitf("s3 build [%s]/[%s] ok=0 isError=1 msg=[%s]", rname.c_str(), pname.c_str(),
              e.msg().c_str());
    }
}

eil::Array<double> canvas_arr(std::initializer_list<double> vals) {
    eil::Array<double> a;
    for (double v : vals) a.append(v);
    return a;
}

eil::Array<Vector3> pts(std::initializer_list<Vector3> vals) {
    eil::Array<Vector3> a;
    for (const Vector3& v : vals) a.append(v);
    return a;
}

Vector3 V3(double x, double y, double z = 0.0) { return Vector3(x, y, z); }

// The two constants src/geom/misc/sketch.cpp stores for D:128-129. Declared here so the
// S1 rows name the same quantities the module uses, and so layer A can assert they are
// what a real isometric Sketch ends up holding.
constexpr double kCos30 = 0x1.bb67ae8584caap-1;
constexpr double kSin30 = 0.5;

// ============================================================================
// A. sketch.d declares no unittest block at all. Assert the module is alive and
//    that its boundaries are what the D reference was measured to produce.
// ============================================================================
void port_d_unittest() {
    auto s = std::make_shared<Sketch>();
    CHECK(s->toString() ==
              "Sketch(renderer=\"svg\", projection=\"xyortho\", eye=Vector3(0, 0, 0), "
              "centre=Vector3(0, 0, -1), up=Vector3(0, 1, 0))",
          "default Sketch prints D's toString() shape (corpus row 30)");
    CHECK(s->canvas.width == 120.0 && s->canvas.height == 120.0,
          "default canvas is 120x120 mm (D:100 default argument)");
    CHECK(s->viewport.x1 == 1.0 && s->viewport.width == 1.0,
          "constructor sets the viewport to unit space (D:145)");
    auto si = std::make_shared<Sketch>("svg", "isometric");
    // The stored matrix, not a restated constant, is the gate (corpus rows 66-67).
    const double neg_sin30 = -kSin30; // D:131 stores -sin30 at proj_mat[1][0]
    CHECK(std::memcmp(&si->proj_mat[0][0], &kCos30, 8) == 0,
          "isometric proj_mat[0][0] is ldc2's folded cos(30.0/180.0*PI)");
    CHECK(std::memcmp(&si->proj_mat[1][0], &neg_sin30, 8) == 0,
          "isometric proj_mat[1][0] is the negation of ldc2's folded sin");
    bool threw = false;
    try {
        auto bad = std::make_shared<Sketch>("tgif", "xyortho");
    } catch (const eil::ExceptionBase& e) {
        threw = e.msg() == "Unknown renderer: tgif";
    } catch (const eil::ErrorBase&) {
        threw = false;
    }
    CHECK(threw, "unknown renderer throws an Exception carrying D's message");
    bool xplot_threw = false;
    try {
        auto xp = std::make_shared<Sketch>("xplot", "xyortho");
    } catch (const eil::ExceptionBase& e) {
        xplot_threw = e.msg() == "You asked for xplot renderer but libplot is not included.";
    } catch (const eil::ErrorBase&) {
        xplot_threw = false;
    }
    CHECK(xplot_threw, "xplot without libplot throws an Exception, not an Error (D:165)");
    // Exception and Error stay siblings: an ErrorBase is not caught as an ExceptionBase.
    bool sibling_ok = false;
    try {
        try {
            throw eil::ErrorBase("layer-a error");
        } catch (const eil::ExceptionBase&) {
            sibling_ok = false;
        } catch (const eil::ErrorBase&) {
            sibling_ok = true;
        }
    } catch (...) {
        sibling_ok = false;
    }
    CHECK(sibling_ok, "catch (ExceptionBase) does not catch an ErrorBase");
    s->start("skprobe_unittest.svg");
    s->line(V3(0.0, 0.0), V3(1.0, 1.0));
    s->finish();
    size_t bytes = 0;
    std::vector<std::string> lines = read_lines("skprobe_unittest.svg", bytes);
    CHECK(lines.size() == 13, "a one-line Sketch document is 10 header + 1 + 2 trailer");
    CHECK(!lines.empty() && lines[0] == "<?xml version=\"1.0\"?>", "XML prolog");
    CHECK(lines.size() > 5 && lines[5] == "     width=\"425.20\" height=\"425.20\"",
          "120 mm canvas at the folded 90.0/25.4 is 425.20 px (corpus row 201)");
    CHECK(lines.size() > 11 && lines[11] == "<!-- end of drawing -->", "close() comment");
    CHECK(lines.size() > 12 && lines[12] == "</svg>", "closing svg element");
}

// ============================================================================
// B. the probe replay. Same objects, same calls, same order, same print statements.
// ============================================================================
void replay_probe() {
    // ---- S1 ---------------------------------------------------------------
    // ldc2's fold, as stored by the module (see the note above the class comment).
    pv("s1 arg_30_180_PI folded", 0x1.0c152382d7366p-1);
    const double l30 = 30.0, l180 = 180.0;
    const double lpi = static_cast<double>(EIL_PI);
    const double argRun = l30 / l180 * lpi;
    pv("s1 arg_30_180_PI runtime", argRun);
    pv("s1 cos folded", kCos30);
    pv("s1 sin folded", kSin30);
    const double cosPair = kCos30;
    const double sinPair = kSin30;
    pv("s1 cos folded stmt", cosPair);
    pv("s1 sin folded stmt", sinPair);
    // Recomputed at run time with two separate libm calls (eil::cos/eil::sin, never a
    // std::cos/std::sin pair — T-920).
    pv("s1 cos of runtime arg", eil::cos(argRun));
    pv("s1 sin of runtime arg", eil::sin(argRun));
    pv("s1 zfactor folded", 1.0 / (2.0 * eil::sqrt(2.0)));
    const double two = 2.0;
    pv("s1 zfactor runtime", 1.0 / (two * eil::sqrt(two)));
    pv("s1 dpi", eil::geom::dpi);

    // ---- S2 ---------------------------------------------------------------
    Extents e;
    pv("s2 init x0", e.x0);
    pv("s2 init x1", e.x1);
    pv("s2 init y0", e.y0);
    pv("s2 init y1", e.y1);
    pv("s2 init width", e.width);
    pv("s2 init height", e.height);
    e.set(0.0, 0.0, 120.0, 120.0);
    pv("s2 a x0", e.x0);
    pv("s2 a x1", e.x1);
    pv("s2 a y0", e.y0);
    pv("s2 a y1", e.y1);
    pv("s2 a width", e.width);
    pv("s2 a height", e.height);
    e.set(-1.5, 2.25, 3.0, 4.0);
    pv("s2 b x0", e.x0);
    pv("s2 b x1", e.x1);
    pv("s2 b y0", e.y0);
    pv("s2 b y1", e.y1);
    pv("s2 b width", e.width);
    pv("s2 b height", e.height);

    // ---- S3 ---------------------------------------------------------------
    auto sd = std::make_shared<Sketch>();
    emitf("s3 default str=[%s]", sd->toString().c_str());
    pv("s3 default canvas x0", sd->canvas.x0);
    pv("s3 default canvas x1", sd->canvas.x1);
    pv("s3 default canvas y0", sd->canvas.y0);
    pv("s3 default canvas y1", sd->canvas.y1);
    pv("s3 default canvas width", sd->canvas.width);
    pv("s3 default canvas height", sd->canvas.height);
    pv("s3 default viewport x0", sd->viewport.x0);
    pv("s3 default viewport x1", sd->viewport.x1);
    pv("s3 default viewport y0", sd->viewport.y0);
    pv("s3 default viewport y1", sd->viewport.y1);
    pv("s3 default viewport width", sd->viewport.width);
    pv("s3 default viewport height", sd->viewport.height);
    emitf("s3 default title=[%s] desc=[%s]", sd->title.c_str(), sd->description.c_str());
    auto s210 = std::make_shared<Sketch>("svg", "xyortho", canvas_arr({0.0, 0.0, 210.0, 297.0}).all());
    pv("s3 210x297 canvas x0", s210->canvas.x0);
    pv("s3 210x297 canvas y0", s210->canvas.y0);
    pv("s3 210x297 canvas x1", s210->canvas.x1);
    pv("s3 210x297 canvas y1", s210->canvas.y1);
    pv("s3 210x297 width", s210->canvas.width);
    pv("s3 210x297 height", s210->canvas.height);
    auto si = std::make_shared<Sketch>("svg", "isometric");
    emitf("s3 iso str=[%s]", si->toString().c_str());
    auto so = std::make_shared<Sketch>("svg", "oblique");
    emitf("s3 obl str=[%s]", so->toString().c_str());
    build_row("svg", "xyortho");
    build_row("svg", "isometric");
    build_row("svg", "oblique");
    build_row("xplot", "xyortho");
    build_row("SVG", "xyortho");
    build_row("tgif", "xyortho");
    build_row("svg", "iso");
    build_row("svg", "Orthographic");
    try {
        throw eil::ExceptionBase("s3 probe exception", __FILE__, __LINE__);
    } catch (const eil::ExceptionBase& ex) {
        emitf("s3 sibling exception caughtByException=1 msg=[%s]", ex.msg().c_str());
    } catch (const eil::ErrorBase& ex) {
        emitf("s3 sibling exception caughtByError=1 msg=[%s]", ex.msg().c_str());
    }
    try {
        throw eil::ErrorBase("s3 probe error");
    } catch (const eil::ExceptionBase& ex) {
        emitf("s3 sibling error caughtByException=1 msg=[%s]", ex.msg().c_str());
    } catch (const eil::ErrorBase& ex) {
        emitf("s3 sibling error caughtByError=1 msg=[%s]", ex.msg().c_str());
    }

    // ---- S4 ---------------------------------------------------------------
    pvm("s4 proj xyortho", sd->proj_mat);
    pvm("s4 proj isometric", si->proj_mat);
    pvm("s4 proj oblique", so->proj_mat);
    pvm("s4 view default", sd->view_mat);
    si->look_at(Vector3(0.0, 0.0, -1.0), Vector3(0.0, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
    pvm("s4 view lookat1", si->view_mat);
    pv("s4 lookat1 eye x", eil::re(si->eye.x));
    pv("s4 lookat1 eye y", eil::re(si->eye.y));
    pv("s4 lookat1 eye z", eil::re(si->eye.z));
    pv("s4 lookat1 centre x", eil::re(si->centre.x));
    pv("s4 lookat1 centre y", eil::re(si->centre.y));
    pv("s4 lookat1 centre z", eil::re(si->centre.z));
    pv("s4 lookat1 up x", eil::re(si->up.x));
    pv("s4 lookat1 up y", eil::re(si->up.y));
    pv("s4 lookat1 up z", eil::re(si->up.z));
    sd->look_at(Vector3(1.0, 2.0, 3.0), Vector3(0.5, -0.5, 0.0), Vector3(0.0, 0.0, 1.0));
    pvm("s4 view lookat2", sd->view_mat);
    pv("s4 lookat2 eye x", eil::re(sd->eye.x));
    pv("s4 lookat2 eye y", eil::re(sd->eye.y));
    pv("s4 lookat2 eye z", eil::re(sd->eye.z));
    pv("s4 lookat2 centre x", eil::re(sd->centre.x));
    pv("s4 lookat2 centre y", eil::re(sd->centre.y));
    pv("s4 lookat2 centre z", eil::re(sd->centre.z));
    pv("s4 lookat2 up x", eil::re(sd->up.x));
    pv("s4 lookat2 up y", eil::re(sd->up.y));
    pv("s4 lookat2 up z", eil::re(sd->up.z));

    // ---- S5 ---------------------------------------------------------------
    double vm1[4] = {1.0, 2.0, 3.0, 1.0};
    si->matrix_vector_multiply(si->proj_mat, vm1);
    pvec("s5 mvm isometric", vm1);
    double vm2[4] = {1.0, 2.0, 3.0, 1.0};
    so->matrix_vector_multiply(so->proj_mat, vm2);
    pvec("s5 mvm oblique", vm2);
    double vm3[4] = {1.0, 2.0, 3.0, 1.0};
    sd->matrix_vector_multiply(sd->proj_mat, vm3);
    pvec("s5 mvm xyortho", vm3);
    const double pts3[3][3] = {{0.0, 0.0, 1.0}, {1.0, 2.0, 3.0}, {-2.0, 0.5, 4.0}};
    for (size_t k = 0; k < 3; ++k) {
        auto sk = std::make_shared<Sketch>("svg", "xyortho");
        Vector3 p = V3(pts3[k][0], pts3[k][1], pts3[k][2]);
        double w = 1.0;
        sk->apply_transform("view", p, w);
        sk->apply_transform("projection", p, w);
        pv(fmt("s5 xy p%zu x", k), eil::re(p.x));
        pv(fmt("s5 xy p%zu y", k), eil::re(p.y));
        pv(fmt("s5 xy p%zu z", k), eil::re(p.z));
        pv(fmt("s5 xy p%zu w", k), w);
    }
    for (size_t k = 0; k < 3; ++k) {
        auto sk = std::make_shared<Sketch>("svg", "isometric");
        Vector3 p = V3(pts3[k][0], pts3[k][1], pts3[k][2]);
        double w = 1.0;
        sk->apply_transform("view", p, w);
        sk->apply_transform("projection", p, w);
        pv(fmt("s5 iso p%zu x", k), eil::re(p.x));
        pv(fmt("s5 iso p%zu y", k), eil::re(p.y));
        pv(fmt("s5 iso p%zu z", k), eil::re(p.z));
        pv(fmt("s5 iso p%zu w", k), w);
    }
    for (size_t k = 0; k < 3; ++k) {
        auto sk = std::make_shared<Sketch>("svg", "oblique");
        Vector3 p = V3(pts3[k][0], pts3[k][1], pts3[k][2]);
        double w = 1.0;
        sk->apply_transform("view", p, w);
        sk->apply_transform("projection", p, w);
        pv(fmt("s5 obl p%zu x", k), eil::re(p.x));
        pv(fmt("s5 obl p%zu y", k), eil::re(p.y));
        pv(fmt("s5 obl p%zu z", k), eil::re(p.z));
        pv(fmt("s5 obl p%zu w", k), w);
    }
    for (size_t k = 0; k < 3; ++k) {
        auto sk = std::make_shared<Sketch>("svg", "isometric");
        sk->look_at(Vector3(0.0, 0.0, -1.0), Vector3(0.0, 0.0, 0.0),
                    Vector3(0.0, 1.0, 0.0));
        Vector3 p = V3(pts3[k][0], pts3[k][1], pts3[k][2]);
        double w = 1.0;
        sk->apply_transform("view", p, w);
        sk->apply_transform("projection", p, w);
        pv(fmt("s5 isoview p%zu x", k), eil::re(p.x));
        pv(fmt("s5 isoview p%zu y", k), eil::re(p.y));
        pv(fmt("s5 isoview p%zu z", k), eil::re(p.z));
        pv(fmt("s5 isoview p%zu w", k), w);
    }
    {
        auto sk = std::make_shared<Sketch>("svg", "isometric");
        Vector3 p = V3(1.0, 2.0, 3.0);
        double w = 1.0;
        sk->apply_transform("shear", p, w);
        pv("s5 bogus x", eil::re(p.x));
        pv("s5 bogus y", eil::re(p.y));
        pv("s5 bogus z", eil::re(p.z));
        pv("s5 bogus w", w);
    }

    // ---- S6 ---------------------------------------------------------------
    {
        auto sk = std::make_shared<Sketch>();
        sk->viewport.set(-2.0, -2.0, 2.0, 2.0);
        for (double u : {-2.0, -1.0, 0.0, 1.0, 2.0, 3.0}) {
            pv(fmt("s6 cx %.17g", u), sk->toCanvasX(u));
            pv(fmt("s6 cy %.17g", u), sk->toCanvasY(u));
        }
    }
    {
        auto sk = std::make_shared<Sketch>();
        for (double u : {0.0, 0.25, 0.5, 1.0}) {
            pv(fmt("s6 dflt cx %.17g", u), sk->toCanvasX(u));
            pv(fmt("s6 dflt cy %.17g", u), sk->toCanvasY(u));
        }
    }
    {
        auto sk = std::make_shared<Sketch>("svg", "xyortho",
                                           canvas_arr({10.0, 20.0, 210.0, 297.0}).all());
        sk->viewport.set(-1.0, -1.0, 1.0, 1.0);
        for (double u : {-1.0, 0.0, 0.5, 1.0}) {
            pv(fmt("s6 off cx %.17g", u), sk->toCanvasX(u));
            pv(fmt("s6 off cy %.17g", u), sk->toCanvasY(u));
        }
    }

    // ---- S7 ---------------------------------------------------------------
    // D1: sketch_demo.d's first document, call for call.
    auto s = std::make_shared<Sketch>("svg", "xyortho", canvas_arr({0.0, 0.0, 120.0, 120.0}).all());
    s->viewport.set(-2.0, -2.0, 2.0, 2.0);
    auto a = Vector3(2.0, 0.0);
    auto b = Vector3(1.0, 1.0);
    auto c = Vector3(1.0, 0.0);
    auto abc = std::make_shared<Arc>(a, b, c);
    s->start("skprobe_test.svg");
    s->line(Vector3(-1.0, 1.0, 0.0), Vector3(1.0, -1.0, 0.0));
    s->render(abc);
    s->rule("x", -1.2, 1.2, 0.4, Vector3(0.0, -1.3), 0.03, "%.1f", 0.12, 0.0, 8);
    s->rule("y", -1.2, 1.2, 0.4, Vector3(-1.3, 0.0), 0.03, "%.1f", 0.06, 0.0, 8);
    s->setFillColour("green");
    auto p00 = Vector3(0.0, 0.1);
    auto p10 = Vector3(1.0, 0.1);
    auto p11 = Vector3(1.0, 1.1);
    auto p01 = Vector3(0.0, 1.1);
    auto my_patch = std::make_shared<CoonsPatch>(p00, p10, p11, p01);
    s->dotlabel(p00, "p00");
    s->dotlabel(p10, "p10");
    s->dotlabel(p11, "p11");
    s->dotlabel(p01, "p01");
    s->render(my_patch);
    s->finish();

    // D2: the same Sketch object, second document (state carries).
    s->start("skprobe_test2.svg");
    s->viewport.set(0.0, 0.0, 120.0, 120.0);
    s->line(Vector3(0.0, 0.0), Vector3(90.0, 120.0));
    s->setLineWidth(0.5);
    s->setFillColour("yellow");
    s->circle(Vector3(25.0, 85.0), 12.3);
    s->setLineWidth(0.25);
    {
        eil::Array<Vector3> pl =
            pts({Vector3(0, 50), Vector3(10, 60), Vector3(20, 50), Vector3(30, 60)});
        s->polyline(pl.all(), true);
    }
    s->text(Vector3(25.0, 85.0), "Circle", -30.0, "middle", 10);
    s->arc(Vector3(90.0, 0.0), Vector3(60.0, 30.0), Vector3(60.0, 0.0));
    s->setLineWidth(0.75);
    s->arc(Vector3(30.0, 0.0), Vector3(60.0, 30.0), Vector3(60.0, 0.0), true);
    s->bezier3(Vector3(80.0, 80.0), Vector3(40.0, 80.0), Vector3(80.0, 100.0),
               Vector3(40.0, 100.0));
    s->setLineWidth(0.01);
    s->dotlabel(Vector3(70.0, 20.0), "a");
    s->setLineWidth(0.5);
    {
        eil::Array<Vector3> pg =
            pts({Vector3(60, 10), Vector3(95, 10), Vector3(95, 50), Vector3(60, 50)});
        s->polygon(pg.all(), true, true, true);
    }
    s->finish();

    // D3: isometric, the demo's third document.
    auto s3 = std::make_shared<Sketch>("svg", "isometric",
                                       canvas_arr({0.0, 0.0, 120.0, 120.0}).all());
    s3->viewport.set(-2.0, -2.0, 2.0, 2.0);
    s3->look_at(Vector3(0.0, 0.0, -1.0), Vector3(0.0, 0.0, 0.0), Vector3(0.0, 1.0, 0.0));
    auto a3 = Vector3(2.0, 0.0);
    auto b3 = Vector3(1.0, 1.0);
    auto c3 = Vector3(1.0, 0.0);
    auto abc3 = std::make_shared<Arc>(a3, b3, c3);
    s3->start("skprobe_test3.svg");
    s3->line(Vector3(-1.0, 1.0, 0.0), Vector3(1.0, -1.0, 0.0));
    s3->render(abc3);
    s3->rule("x", -1.2, 1.2, 0.4, Vector3(0.0, 0.0, 0.0), 0.03, "%.1f", 0.12, -30.0, 8);
    s3->text(Vector3(1.0, 0.1), "x", 0.0, "middle", 10);
    s3->rule("y", -1.2, 1.2, 0.4, Vector3(0.0, 0.0, 0.0), 0.03, "%.1f", 0.20, -30.0, 8);
    s3->text(Vector3(0.1, 1.0), "y", 0.0, "middle", 10);
    s3->rule("z", -1.2, 1.2, 0.4, Vector3(0.0, 0.0, 0.0), 0.03, "%.1f", 0.16, 30.0, 8);
    s3->text(Vector3(0.0, 0.1, 1.0), "z", 0.0, "middle", 10);
    s3->finish();

    // D4: one document that reaches every remaining branch.
    auto f = std::make_shared<Sketch>();
    f->viewport.set(0.0, 0.0, 120.0, 120.0);
    f->title = "a title";
    f->description = "a description";
    f->start("skprobe_full.svg");
    f->setLineWidth(0.125);
    f->setLineColour("#ff0000");
    f->setFillColour("blue");
    f->setDashArray();
    f->line(V3(10.0, 10.0), V3(30.0, 40.0));
    f->line(V3(10.0, 15.0), V3(30.0, 45.0), true);
    f->setDashArray(4.0, 1.0);
    f->line(V3(10.0, 20.0), V3(40.0, 20.0), true);
    f->clearFillColour();
    f->line(V3(10.0, 25.0), V3(40.0, 25.0));
    f->setLineColour("black");
    f->setFillColour("none");
    f->begin_group();
    f->line(V3(10.0, 30.0), V3(40.0, 30.0));
    f->end_group();
    f->begin_group("grp", 0.25);
    f->line(V3(10.0, 35.0), V3(40.0, 35.0));
    f->end_group();
    {
        eil::Array<Vector3> v =
            pts({V3(10.0, 40.0), V3(20.0, 45.0), V3(30.0, 40.0), V3(40.0, 48.0)});
        f->polyline(v.all());
    }
    {
        eil::Array<Vector3> v =
            pts({V3(10.0, 50.0), V3(20.0, 55.0), V3(30.0, 50.0), V3(40.0, 58.0)});
        f->polyline(v.all(), true);
    }
    {
        eil::Array<Vector3> v = pts({V3(10.0, 60.0)});
        f->polyline(v.all());
    }
    {
        eil::Array<Vector3> v =
            pts({V3(50.0, 10.0), V3(70.0, 10.0), V3(70.0, 30.0), V3(50.0, 30.0)});
        f->polygon(v.all());
    }
    {
        eil::Array<Vector3> v =
            pts({V3(50.0, 35.0), V3(70.0, 35.0), V3(70.0, 55.0), V3(50.0, 55.0)});
        f->polygon(v.all(), false, true, false);
    }
    {
        eil::Array<Vector3> v =
            pts({V3(50.0, 60.0), V3(70.0, 60.0), V3(70.0, 80.0), V3(50.0, 80.0)});
        f->polygon(v.all(), true, false, false);
    }
    {
        eil::Array<Vector3> v =
            pts({V3(50.0, 85.0), V3(70.0, 85.0), V3(70.0, 105.0), V3(50.0, 105.0)});
        f->polygon(v.all(), false, false, true);
    }
    f->arc(V3(80.0, 10.0), V3(100.0, 10.0), V3(90.0, 20.0));
    f->arc(V3(80.0, 20.0), V3(100.0, 20.0), V3(90.0, 30.0), true);
    f->setFillColour("yellow");
    f->circle(V3(20.0, 70.0), 6.0);
    f->circle(V3(20.0, 85.0), 6.0, false, true, false);
    f->circle(V3(20.0, 100.0), 6.0, true, false, true);
    f->circle(V3(20.0, 115.0), 6.0, false, false, false);
    f->bezier3(V3(80.0, 40.0), V3(85.0, 60.0), V3(95.0, 60.0), V3(100.0, 40.0));
    f->bezier3(V3(80.0, 45.0), V3(85.0, 65.0), V3(95.0, 65.0), V3(100.0, 45.0), true);
    f->text(V3(30.0, 115.0), "plain");
    f->text(V3(60.0, 115.0), "rot", 30.0, "middle", 14, "red", "serif");
    f->text(V3(90.0, 115.0), "end", -45.0, "end", 8);
    f->text(V3(105.0, 115.0), "weird-anchor", 0.0, "bogus", 10);
    f->dotlabel(V3(35.0, 105.0));
    f->dotlabel(V3(45.0, 105.0), "L", "start", 3.0, 12, "blue", "monospace");
    f->dotlabel(V3(55.0, 105.0), "E", "end", 1.5, 6);
    f->render(abc);
    f->render(abc, true, 6);
    f->render(my_patch);
    f->render(my_patch, true, true, true, 4);
    f->render(my_patch, true, false, false, 4, true);
    f->render(my_patch, false, true, false, 3);
    auto slab = std::make_shared<SlabVolume>(my_patch, Vector3(0.0, 0.0, 15.0));
    f->render(slab);
    f->render(slab, true, true, true, 3);
    f->render(slab, true, false, false, 2, true);
    f->render(slab, false, true, false, 3);
    f->finish();

    // D5: oblique projection through the transform-carrying methods.
    auto ob = std::make_shared<Sketch>("svg", "oblique",
                                       canvas_arr({0.0, 0.0, 120.0, 120.0}).all());
    ob->viewport.set(-2.0, -2.0, 2.0, 2.0);
    ob->start("skprobe_oblique.svg");
    ob->line(V3(-1.0, 1.0, 0.0), V3(1.0, -1.0, 0.5));
    {
        eil::Array<Vector3> v =
            pts({V3(-1.0, -1.0, 0.0), V3(0.0, -1.0, 1.0), V3(1.0, 0.0, 0.5)});
        ob->polyline(v.all(), true);
    }
    {
        eil::Array<Vector3> v = pts({V3(-1.0, 0.0, 0.0), V3(0.0, 1.0, 1.0),
                                     V3(1.0, 0.0, 0.0), V3(0.0, -1.0, 0.5)});
        ob->polygon(v.all());
    }
    ob->bezier3(V3(-1.0, -1.5, 0.0), V3(0.0, -1.5, 1.0), V3(1.0, -1.5, 0.0),
                V3(1.5, -1.0, 0.5), true);
    ob->text(V3(0.5, 0.5, 0.5), "oblique text", 15.0, "middle", 9);
    ob->dotlabel(V3(-0.5, -0.5, 0.25), "q", "middle", 2.5, 7);
    ob->render(abc3);
    ob->render(my_patch, true, true, false, 5);
    ob->rule("z", -1.0, 1.0, 0.5, Vector3(0.0, 0.0, 0.0), 0.05, "%.2f", 0.1, 20.0, 7);
    ob->finish();

    // D6: rule coverage.
    const double rules[6][3] = {{0.0, 6.0, 0.1},   {-1.2, 1.2, 0.4}, {0.0, 1.0, 3.0},
                                {1.0, -1.0, 0.25}, {0.0, 2.0, 0.3333333},
                                {-5.0, 5.0, 0.4}};
    for (const auto& r : rules) {
        double vmin = r[0], vmax = r[1], vtic = r[2];
        int n = static_cast<int>(1.000001 * (vmax - vmin) / vtic);
        emitf("s6 nrule %.17g %.17g %.17g n=%d gt50=%s", vmin, vmax, vtic, n,
              n > 50 ? "true" : "false");
    }
    auto rg = std::make_shared<Sketch>();
    rg->viewport.set(0.0, 0.0, 120.0, 120.0);
    capture_begin(); // D's writeln at sketch.d:730 fires inside the first rule()
    rg->start("skprobe_rules.svg");
    rg->rule("x", 0.0, 6.0, 0.1, Vector3(10.0, 10.0), 1.0, "%.1f", 2.0, 0.0, 6);
    rg->rule("y", 1.0, -1.0, 0.25, Vector3(20.0, 20.0), 1.0, "%.2f", 2.0, 45.0, 6);
    rg->rule("z", 0.0, 40.0, 20.0, Vector3(30.0, 30.0, 0.0), 2.0, "%3.1f", 3.0, 0.0, 5);
    rg->finish();
    capture_end();
    try {
        rg->rule("q", 0.0, 1.0, 0.5, Vector3(0.0, 0.0), 0.1, "%.1f", 0.1, 0.0, 8);
        emitf("s7 invalid rule direction threw=0");
    } catch (const eil::ExceptionBase& ex) {
        emitf("s7 invalid rule direction threw=1 isError=0 msg=[%s]", ex.msg().c_str());
    } catch (const eil::ErrorBase& ex) {
        emitf("s7 invalid rule direction threw=1 isError=1 msg=[%s]", ex.msg().c_str());
    }

    // ---- now dump every document, in the order they were drawn -----------
    dump_doc("skprobe_test.svg");
    dump_doc("skprobe_test2.svg");
    dump_doc("skprobe_test3.svg");
    dump_doc("skprobe_full.svg");
    dump_doc("skprobe_oblique.svg");
    dump_doc("skprobe_rules.svg");
    emitf("s7 done=1");
}

long long ulp_gap(double a, double b) {
    int64_t ia, ib;
    std::memcpy(&ia, &a, 8);
    std::memcpy(&ib, &b, 8);
    if (ia < 0) ia = INT64_MIN - ia;
    if (ib < 0) ib = INT64_MIN - ib;
    return ia > ib ? ia - ib : ib - ia;
}

/// Numbers embedded in a row, for the numeric summary only. Bit patterns are taken
/// literally as the 16 hex digits after "bits=" or after a "/" in a matrix row.
std::vector<double> numbers_in(const std::string& s) {
    std::vector<double> out;
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] != '=') {
            ++i;
            continue;
        }
        size_t b = i + 1;
        size_t e = s.find(' ', b);
        if (e == std::string::npos) e = s.size();
        const std::string tok = s.substr(b, e - b);
        if (!tok.empty()) {
            const char c = tok[0];
            if (c == '-' || c == '+' || std::isdigit(static_cast<unsigned char>(c)) ||
                c == 'n' || c == 'i') {
                char* endp = nullptr;
                const double v = std::strtod(tok.c_str(), &endp);
                if (endp == tok.c_str() + tok.size()) out.push_back(v);
            }
        }
        i = b;
    }
    return out;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <probes/geom_misc_sketch.txt>\n";
        return 2;
    }

    port_d_unittest();
    replay_probe();

    std::ifstream in(argv[1]);
    if (!in) {
        std::cerr << "cannot open baseline " << argv[1] << "\n";
        return 2;
    }
    std::vector<std::string> expected;
    for (std::string line; std::getline(in, line);) expected.push_back(line);

    int line_diffs = 0;
    size_t quantities = 0, identical = 0;
    long long worst_ulp = 0;
    std::string worst_where;
    double worst_d = 0.0, worst_cpp = 0.0, worst_resid = 0.0;
    for (size_t i = 0; i < expected.size() || i < produced.size(); ++i) {
        if (i >= expected.size() || i >= produced.size() || expected[i] != produced[i]) {
            if (line_diffs < 20) {
                std::cout << "line " << (i + 1) << "\n  D:   |"
                          << (i < expected.size() ? expected[i] : std::string("<none>"))
                          << "|\n  C++: |"
                          << (i < produced.size() ? produced[i] : std::string("<none>"))
                          << "|\n";
            }
            ++line_diffs;
        }
        if (i < expected.size() && i < produced.size()) {
            const std::vector<double> a = numbers_in(expected[i]);
            const std::vector<double> b = numbers_in(produced[i]);
            const std::string tag = expected[i].substr(0, expected[i].find(' '));
            for (size_t j = 0; j < a.size() && j < b.size(); ++j) {
                ++quantities;
                if (std::memcmp(&a[j], &b[j], 8) == 0) {
                    ++identical;
                } else {
                    const long long gap = ulp_gap(a[j], b[j]);
                    if (gap > worst_ulp) {
                        worst_ulp = gap;
                        worst_resid = std::fabs(a[j] - b[j]);
                        worst_d = a[j];
                        worst_cpp = b[j];
                        worst_where = tag + " #" + std::to_string(j);
                    }
                }
            }
        }
    }
    std::cout << (line_diffs == 0 && failures == 0 ? "PASS" : "FAIL") << ": sketch, "
              << checks - failures << "/" << checks
              << " smoke assertions hold (sketch.d has no unittest block at all, grep "
                 "count 0); emitted-text replay "
              << expected.size() << " D lines vs " << produced.size()
              << " C++ lines, " << line_diffs
              << " differ (byte comparison, no tolerance); " << identical << " of "
              << quantities << " embedded numeric quantities bit-identical";
    if (worst_ulp > 0) {
        std::cout << ", worst residual " << worst_resid << " (" << worst_ulp
                  << " ULP) on quantity " << worst_where << " (D " << worst_d << " vs C++ "
                  << worst_cpp << ")";
    } else {
        std::cout << ", worst residual 0 (0 ULP) on quantity none";
    }
    std::cout << "\n";
    return (line_diffs == 0 && failures == 0) ? 0 : 1;
}
