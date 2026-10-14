// tests/test_geom_grid_paver.cpp — C++ mirror of probes/geom/grid/probe_paver.d,
// replayed row for row against baselines/e587b235/probes/geom_grid_paver.txt
// (1961 rows captured from ldc2 1.40.0 at gdtk e587b235, sha256
// d96d146faee0799f27a54f69e461acac02b7ee51e7d875642a6d66df68df2a5c).
//
// geom/grid/paver.d has no `unittest` block — proved by
//   grep -c unittest /home/tang/packages/gdtk/src/geom/grid/paver.d  ->  0
// so this replay IS the gate. The module's five `assert(...)` sites (paver.d:125,
// :260, :576, :1032, :1271) and its six `throw new Error(...)` / two
// `throw new GeometryException(...)` sites are in-body guards, and each one is
// replayed here as a tagged row (cls.outsigns.bad, sec2.sq.tri.throw, node.one.throw,
// node.dup.throw, row.ss.orphan.throw, seam.cs.odd, seam.cs6, pave.sq1, pave.odd,
// pave.cw, pave.bowtie, node.cat.over.throw, smk.pidc.bad, ...), not as a separate
// section. Rows whose text is `|EXC|` are D Exceptions and `|ERR|` are D Errors —
// the two are siblings, so the catch order in tryExc() below is itself an assertion
// that the port keeps them apart.
//
// Row groups (the probe header documents why each one is load-bearing):
//   cls.*    the Point / UFace / Cell classes, .init of every field, the print()
//            texts, the ID counters, auto_cell_type over 0..8 faces, assign_outsigns.
//   sec2.*   Section 2 geometry: left_rot_matrix, the folded left90matrix global,
//            vector_rot, midpoint, point_dist, vector2D, LeftorRight (4 outcomes),
//            norm, unit_normal_vector, unit_leftV, squareness, winding_number.
//   sec3.*   Section 3: clockwise_check, closed_row_check, calc_area,
//            intersecting_boundary, intersecting_faces, average_face_length,
//            angle_between_vectors, inside_prev_row_check.
//   make.*   make2DPoint / makeFace / makeCell / findFace and the three global lists.
//   node.*   node_category at each tolerance and one ULP either side,
//            assign_node_properties, adjust_row_ends and its three writeln arms.
//   row.*    node_gen for every node category, generate_new_row, summed_squareness.
//   smk.*    smoothing, even_out_size, biggest_cell, fix_IDs, point_ID_check, seam,
//            closing_seam.
//   seam2.*  seaming_procedure.
//   pave.*   the whole PavedGrid constructor on the eight boundaries it completes
//            (rect8, rect16, wide, lshape, circle6, wide16, pwedge, rect32), the five
//            guard boundaries (odd, empty, single, cw, bowtie) and sq1, which paves two
//            rows and then trips the smoothing assert; plus the complete POINT_LIST /
//            FACE_LIST / CELL_LIST.
//
// Two routing rules the corpus proves, both already pinned in the substrate:
//   * a cos/sin pair of one argument stays two libm calls (eil::cos / eil::sin,
//     T-920). paver.d:174 builds that pair and the probe's circle() builds it again;
//     g++ would fuse them into glibc sincos(), which is 1 ULP off.
//   * `x ^^ 0.5` is ldc2's sqrt, not pow: measured over 62 operands, sqrt matches D on
//     62/62 while pow(x,0.5) differs on 4 (nextafter(1,0) -> 1.0, DBL_MAX -> inf,
//     -0.0 -> +0.0). Rows sec2.ulv3.0 / sec2.ulv3.1 are exactly that 1 ULP, so both
//     `^^0.5` sites (point_dist, norm) go through eil::sqrt while every other `^^`
//     site stays eil::pow.
//
// Output capture: paver.d itself writes 33 console lines from inside the paving
// functions (eprintln/writefln in D, eil::writeln here), so the rows cannot be
// accumulated in a vector the way test_geom_grid_grid.cpp does. Instead stdout is
// redirected to a scratch file with dup2()/freopen() for the whole run, which is
// then read back and compared line by line, and the real stdout is restored for the
// summary. Nothing is filtered and no row is skipped.
//
// Regenerate the baseline:
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/grid
//     probes/geom/grid/probe_paver.d -of=/tmp/probe_g2 -od/tmp/probe_obj_g2
//     and then /tmp/probe_g2 > baselines/e587b235/probes/geom_grid_paver.txt
//
// Run: ./test_geom_grid_paver <baseline.txt>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <limits>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/math.hpp"
#include "eil/number.hpp"
#include "geom/grid/paver.hpp"

namespace {

using eil::geom::Cell;
using eil::geom::CellPtr;
using eil::geom::Point;
using eil::geom::PointPtr;
using eil::geom::UFace;
using eil::geom::UFacePtr;
using AA = eil::Array<double>;
using AAA = eil::Array<eil::Array<double>>;

const char* kCapture = "/tmp/eil_paver_capture.txt";

unsigned long long bits(double x) {
    unsigned long long u;
    std::memcpy(&u, &x, sizeof u);
    return u;
}

// The corpus prints every double twice, as %.17g and as its raw bits. eil::format's
// %x spec renders through `unsigned`, which would truncate a 64-bit pattern, so the
// bits go out as a string instead.
std::string hx(double x) {
    char buf[20];
    std::snprintf(buf, sizeof buf, "%016llx", bits(x));
    return buf;
}

std::string esc(const std::string& s) {
    std::string out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\n') out += "<newline>";
        else out += s[i];
    }
    return out;
}

void F(const std::string& tag, double v) {
    eil::writefln("%s|%.17g|%s", tag, v, hx(v));
}
void I(const std::string& tag, long v) { eil::writefln("%s|%d", tag, v); }
void S(const std::string& tag, const std::string& v) {
    eil::writefln("%s|len=%d|[%s]", tag, static_cast<int>(v.size()), esc(v));
}
void T(const std::string& tag, const std::string& v) { eil::writefln("%s|%s", tag, v); }

void dumpP(const std::string& tag, const PointPtr& p) {
    if (p == nullptr) {
        T(tag + ".null", "null");
        return;
    }
    eil::writefln(
        "%s|ID=%d|x=%.17g/%s|y=%.17g/%s|z=%.17g/%s|ang=%.17g/%s|cat=%s|VR=%.17g/%s,"
        "%.17g/%s|VL=%.17g/%s,%.17g/%s|L=%d|R=%d|prev=%d|growth=%.17g/%s|fsf=%d|rowID=%d",
        tag, p->ID, p->x, hx(p->x), p->y, hx(p->y), p->z, hx(p->z), p->interior_angle,
        hx(p->interior_angle), p->node_cat, p->V_R[0], hx(p->V_R[0]), p->V_R[1],
        hx(p->V_R[1]), p->V_L[0], hx(p->V_L[0]), p->V_L[1], hx(p->V_L[1]),
        p->pointL ? static_cast<int>(p->pointL->ID) : -1,
        p->pointR ? static_cast<int>(p->pointR->ID) : -1,
        p->prev_node ? static_cast<int>(p->prev_node->ID) : -1, p->growth, hx(p->growth),
        static_cast<int>(p->first_seam_flag), p->rowID);
}

void dumpF(const std::string& tag, const UFacePtr& f) {
    if (f == nullptr) {
        T(tag + ".null", "null");
        return;
    }
    eil::writefln("%s|faceID=%d|pts=%d,%d", tag, f->face_ID, f->point_IDs[0],
                  f->point_IDs[1]);
}

std::string joinu(const eil::Array<size_t>& a) {
    std::string s;
    for (size_t i = 0; i < a.length(); ++i) {
        if (i) s += ",";
        s += eil::to_string(a[i]);
    }
    return s;
}
std::string joini(const eil::Array<int>& a) {
    std::string s;
    for (size_t i = 0; i < a.length(); ++i) {
        if (i) s += ",";
        s += eil::to_string(a[i]);
    }
    return s;
}

void dumpC(const std::string& tag, const CellPtr& c) {
    if (c == nullptr) {
        T(tag + ".null", "null");
        return;
    }
    eil::writefln("%s|cellID=%d|type=%s|pts=%s|faces=%s|out=%s", tag, c->cell_ID,
                  c->cell_type, joinu(c->point_IDs), joinu(c->face_IDs),
                  joini(c->outsigns));
}

void dumpLists(const std::string& tag) {
    eil::writefln("%s|counts|%d|%d|%d|%d", tag, eil::geom::POINT_LIST.length(),
                  eil::geom::FACE_LIST.length(), eil::geom::CELL_LIST.length(),
                  eil::geom::ndimensions);
    for (auto& t : eil::geom::POINT_LIST)
        dumpP(tag + ".P" + eil::to_string(t->ID), t);
    for (auto& t : eil::geom::FACE_LIST)
        dumpF(tag + ".F" + eil::to_string(t->face_ID), t);
    for (auto& t : eil::geom::CELL_LIST)
        dumpC(tag + ".C" + eil::to_string(t->cell_ID), t);
}

void resetAll() {
    eil::geom::POINT_LIST.clear();
    eil::geom::FACE_LIST.clear();
    eil::geom::CELL_LIST.clear();
    Point::ID_count = 0;
    UFace::face_ID_count = 0;
    Cell::cell_ID_count = 0;
    eil::geom::initial_size = 0;
    eil::geom::avr_area = 0;
}

void tryExc(const std::string& tag, const std::function<void()>& dg) {
    try {
        dg();
        T(tag, "no-throw");
    } catch (const eil::ExceptionBase& e) {
        eil::writefln("%s|EXC|%s", tag, esc(e.msg()));
    } catch (const eil::ErrorBase& e) {
        eil::writefln("%s|ERR|%s", tag, esc(e.msg()));
    }
}

PointPtr badPt();

PointPtr NP(double x, double y) { return std::make_shared<Point>(0, x, y); }

PointPtr mkp(size_t id, double x, double y) { return std::make_shared<Point>(id, x, y); }

eil::Array<PointPtr> mkrow(const std::vector<std::vector<double>>& xy) {
    eil::Array<PointPtr> r;
    for (const auto& p : xy) r.append(eil::geom::make2DPoint(p[0], p[1]));
    return r;
}

AAA aa(std::initializer_list<std::vector<double>> rows) {
    AAA out;
    for (const auto& r : rows) {
        AA e;
        for (double d : r) e.append(d);
        out.append(e);
    }
    return out;
}

std::array<double, 2> v2(double a, double b) { return std::array<double, 2>{a, b}; }
std::array<size_t, 2> s2(size_t a, size_t b) { return std::array<size_t, 2>{a, b}; }

const double kNaN = std::numeric_limits<double>::quiet_NaN();
// D's double.init is the POSITIVE quiet NaN (measured: bits 0x7ff8000000000000, and
// writeln prints "nan"), which is what eil::d_init<double>() gives.
double dinit() { return eil::d_init<double>(); }

// nextafter, as the probe uses std.math.nextafter(t, +-inf).
double up1(double t) { return std::nextafter(t, std::numeric_limits<double>::infinity()); }
double dn1(double t) {
    return std::nextafter(t, -std::numeric_limits<double>::infinity());
}

//---------------------------------------------------------------- group cls
void grpCls() {
    resetAll();
    PointPtr p = mkp(7, 1.25, -2.5);
    dumpP("cls.fresh", p);
    S("cls.fresh.node_cat", p->node_cat);
    S("cls.fresh.print", p->print());
    p->store_node_angle(0.75);
    p->store_node_cat("row end");
    p->store_vectors(v2(1.0, 2.0), v2(3.0, 4.0));
    p->store_rowID(11);
    dumpP("cls.stored", p);
    p->store_node_cat("row side");
    S("cls.cat.sticky", p->node_cat);
    PointPtr q = mkp(8, 3.5, 4.5);
    p->store_adjacent_points(q, q);
    I("cls.links.L", static_cast<long>(p->pointL->ID));
    I("cls.links.R", static_cast<long>(p->pointR->ID));
    T("cls.links.print", p->print());
    I("cls.IDcount", static_cast<long>(Point::ID_count));
    UFacePtr f = std::make_shared<UFace>(3, s2(10, 20));
    dumpF("cls.face", f);
    S("cls.face.print", f->print());
    I("cls.facecount", static_cast<long>(UFace::face_ID_count));
    CellPtr c = std::make_shared<Cell>(5);
    dumpC("cls.cell.fresh", c);
    S("cls.cell.fresh.print", c->print());
    c->point_IDs.append(1);
    c->point_IDs.append(2);
    c->point_IDs.append(3);
    S("cls.cell.partial.print", c->print());
    {
        CellPtr c2 = std::make_shared<Cell>(6);
        c2->face_IDs.set_length(4);
        c2->auto_cell_type();
        S("cls.celltype.4", c2->cell_type);
    }
    for (int n = 0; n < 9; ++n) {
        CellPtr cx = std::make_shared<Cell>(static_cast<size_t>(90 + n));
        cx->face_IDs.set_length(static_cast<size_t>(n));
        std::string tg = "cls.auto." + eil::to_string(n) + ".throw";
        tryExc(tg, [cx] { cx->auto_cell_type(); });
        S("cls.celltype." + eil::to_string(n), cx->cell_type);
    }
    CellPtr c3 = std::make_shared<Cell>(7);
    eil::Array<int> bad{1, 0};
    tryExc("cls.outsigns.bad", [&] { c3->assign_outsigns(bad.all()); });
    CellPtr c4 = std::make_shared<Cell>(8);
    eil::Array<int> okv{-1, 1, -1};
    c4->assign_outsigns(okv.all());
    dumpC("cls.outsigns.ok", c4);
    eil::Array<int> one{1};
    c4->assign_outsigns(one.all());
    dumpC("cls.outsigns.append", c4);
    I("cls.cellcount", static_cast<long>(Cell::cell_ID_count));
    PointPtr np;
    T("cls.nullpoint", np ? "null" : "null");
    dumpP("cls.nullpoint.dump", np);
}

//---------------------------------------------------------------- group sec2
void grpSec2() {
    resetAll();
    std::vector<double> angs = {0.0, 0.5, 1.0, static_cast<double>(EIL_PI / 2.0L),
                                static_cast<double>(EIL_PI),
                                static_cast<double>(3 * EIL_PI / 2.0L),
                                static_cast<double>(2 * EIL_PI), -0.25, dinit()};
    for (size_t i = 0; i < angs.size(); ++i) {
        double th = angs[i];
        auto m = eil::geom::left_rot_matrix(th);
        std::string tg = "sec2.lrm" + eil::to_string(i);
        F(tg + ".th", th);
        F(tg + ".00", m[0][0]);
        F(tg + ".01", m[0][1]);
        F(tg + ".10", m[1][0]);
        F(tg + ".11", m[1][1]);
    }
    F("sec2.gbl.00", eil::geom::left90matrix[0][0]);
    F("sec2.gbl.01", eil::geom::left90matrix[0][1]);
    F("sec2.gbl.10", eil::geom::left90matrix[1][0]);
    F("sec2.gbl.11", eil::geom::left90matrix[1][1]);
    std::array<std::array<double, 2>, 5> vs = {v2(1.0, 0.0), v2(0.0, 1.0), v2(1.0, 1.0),
                                              v2(0.5, -2.0), v2(3.0, 4.0)};
    for (size_t i = 0; i < vs.size(); ++i) {
        auto r = eil::geom::vector_rot(vs[i], eil::geom::left90matrix);
        F("sec2.vrot" + eil::to_string(i) + ".0", r[0]);
        F("sec2.vrot" + eil::to_string(i) + ".1", r[1]);
    }
    size_t nbefore = eil::geom::POINT_LIST.length();
    size_t cbefore = Point::ID_count;
    PointPtr a = eil::geom::make2DPoint(0.0, 0.0);
    PointPtr b = eil::geom::make2DPoint(2.0, 1.0);
    PointPtr m = eil::geom::midpoint(a, b);
    dumpP("sec2.midpoint", m);
    I("sec2.mid.listlen", static_cast<long>(eil::geom::POINT_LIST.length() - nbefore));
    I("sec2.mid.idcount", static_cast<long>(Point::ID_count - cbefore));
    PointPtr bad = mkp(99, dinit(), 1.0);
    bad->z = 0.0;
    PointPtr nanp = mkp(98, 1.0, dinit());
    nanp->z = 0.0;
    F("sec2.dist.ab", eil::geom::point_dist(a, b));
    F("sec2.dist.aa", eil::geom::point_dist(a, a));
    F("sec2.dist.ba", eil::geom::point_dist(b, a));
    F("sec2.dist.nan", eil::geom::point_dist(a, bad));
    F("sec2.dist.nan2", eil::geom::point_dist(nanp, b));
    PointPtr big = mkp(97, 1.0e200, 1.0e200);
    F("sec2.dist.big", eil::geom::point_dist(a, big));
    PointPtr tiny = mkp(96, 1.0e-200, 0.0);
    F("sec2.dist.tiny", eil::geom::point_dist(a, tiny));
    auto vab = eil::geom::vector2D(a, b);
    F("sec2.v2d.0", vab[0]);
    F("sec2.v2d.1", vab[1]);
    auto vba = eil::geom::vector2D(b, a);
    F("sec2.v2d.r0", vba[0]);
    F("sec2.v2d.r1", vba[1]);
    PointPtr c1 = eil::geom::make2DPoint(1.0, 0.0);
    PointPtr lft = eil::geom::make2DPoint(0.5, 1.0);
    PointPtr rgt = eil::geom::make2DPoint(0.5, -1.0);
    PointPtr onl = eil::geom::make2DPoint(2.0, 0.0);
    I("sec2.lr.left", eil::geom::LeftorRight(a, b, lft));
    I("sec2.lr.right", eil::geom::LeftorRight(a, b, rgt));
    I("sec2.lr.online", eil::geom::LeftorRight(a, c1, onl));
    I("sec2.lr.degen", eil::geom::LeftorRight(a, a, b));
    I("sec2.lr.nan", eil::geom::LeftorRight(a, bad, b));
    I("sec2.lr.nan2", eil::geom::LeftorRight(bad, a, b));
    std::array<std::array<double, 2>, 6> ns = {v2(0.0, 0.0), v2(3.0, 4.0), v2(-3.0, -4.0),
                                              v2(1.0, 1.0e-8), v2(1.0e200, 1.0e200),
                                              v2(dinit(), 1.0)};
    for (size_t i = 0; i < ns.size(); ++i)
        F("sec2.norm" + eil::to_string(i), eil::geom::norm(ns[i]));
    PointPtr p0 = eil::geom::make2DPoint(0.0, 0.0);
    PointPtr pR = eil::geom::make2DPoint(1.0, 0.0);
    auto un = eil::geom::unit_normal_vector(p0, pR);
    F("sec2.unv.0", un[0]);
    F("sec2.unv.1", un[1]);
    PointPtr pS = eil::geom::make2DPoint(1.5, 2.0);
    auto un2 = eil::geom::unit_normal_vector(p0, pS);
    F("sec2.unv2.0", un2[0]);
    F("sec2.unv2.1", un2[1]);
    PointPtr pZ = eil::geom::make2DPoint(0.0, 0.0);
    auto un3 = eil::geom::unit_normal_vector(p0, pZ);
    F("sec2.unv3.0", un3[0]);
    F("sec2.unv3.1", un3[1]);
    std::vector<double> ths = {0.0, static_cast<double>(EIL_PI / 2.0L),
                               static_cast<double>(EIL_PI / 3.0L),
                               static_cast<double>(2 * EIL_PI / 3.0L),
                               static_cast<double>(EIL_PI)};
    for (size_t i = 0; i < ths.size(); ++i) {
        double th = ths[i];
        auto uv = eil::geom::unit_leftV(v2(1.0, 0.0), th);
        F("sec2.ulv" + eil::to_string(i) + ".0", uv[0]);
        F("sec2.ulv" + eil::to_string(i) + ".1", uv[1]);
        auto uv2 = eil::geom::unit_leftV(v2(-0.5, 0.75), th);
        F("sec2.ulv2_" + eil::to_string(i) + ".0", uv2[0]);
        F("sec2.ulv2_" + eil::to_string(i) + ".1", uv2[1]);
    }
    auto uvz = eil::geom::unit_leftV(v2(0.0, 0.0), 1.0);
    F("sec2.ulvz.0", uvz[0]);
    F("sec2.ulvz.1", uvz[1]);
    std::vector<std::string> names = {"sq", "rect", "skew", "degen", "nan"};
    std::vector<std::vector<std::vector<double>>> quads = {
        {{0, 0}, {1, 0}, {1, 1}, {0, 1}}, {{0, 0}, {2, 0}, {2, 1}, {0, 1}},
        {{0, 0}, {2, 0}, {3, 1}, {0, 1}}, {{0, 0}, {1, 0}, {1, 0}, {0, 0}},
        {{0.0, 0.0}, {1.0, 0.0}, {1.0, dinit()}, {0.0, 1.0}}};
    for (size_t i = 0; i < quads.size(); ++i) {
        resetAll();
        eil::Array<PointPtr> ps = mkrow(quads[i]);
        CellPtr cc = eil::geom::makeCell(ps.all());
        F("sec2.sq." + names[i], eil::geom::squareness(cc));
        dumpC("sec2.sq." + names[i] + ".cell", cc);
    }
    resetAll();
    eil::Array<PointPtr> tri =
        mkrow({{0, 0}, {1, 0}, {0, 1}});
    CellPtr tc = eil::geom::makeCell(tri.all());
    tryExc("sec2.sq.tri.throw", [&] { F("sec2.sq.tri", eil::geom::squareness(tc)); });
    resetAll();
    eil::Array<PointPtr> ccw = mkrow({{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}});
    eil::geom::assign_node_properties(ccw.all());
    PointPtr pin = mkp(500, 0.25, 0.25);
    PointPtr pout = mkp(501, 2.0, 0.5);
    PointPtr pedge = mkp(502, 0.5, 0.0);
    PointPtr pvert = mkp(503, 1.0, 1.0);
    I("sec2.wn.inside", eil::geom::winding_number(pin, ccw.all()));
    I("sec2.wn.outside", eil::geom::winding_number(pout, ccw.all()));
    I("sec2.wn.edge", eil::geom::winding_number(pedge, ccw.all()));
    I("sec2.wn.vertex", eil::geom::winding_number(pvert, ccw.all()));
    eil::Array<PointPtr> empty_row;
    I("sec2.wn.empty", eil::geom::winding_number(pin, empty_row.all()));
    resetAll();
    eil::Array<PointPtr> cw = mkrow({{0.0, 0.0}, {0.0, 1.0}, {1.0, 1.0}, {1.0, 0.0}});
    eil::geom::assign_node_properties(cw.all());
    I("sec2.wn.cw.inside", eil::geom::winding_number(pin, cw.all()));
    I("sec2.wn.cw.outside", eil::geom::winding_number(pout, cw.all()));
}

//---------------------------------------------------------------- group sec3
void grpSec3() {
    resetAll();
    eil::Array<PointPtr> ccw = mkrow({{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}});
    eil::geom::assign_node_properties(ccw.all());
    F("sec3.cw.ccw", eil::geom::clockwise_check(ccw.all()));
    eil::Array<PointPtr> cw = mkrow({{0.0, 0.0}, {0.0, 1.0}, {1.0, 1.0}, {1.0, 0.0}});
    eil::geom::assign_node_properties(cw.all());
    F("sec3.cw.cw", eil::geom::clockwise_check(cw.all()));
    eil::Array<PointPtr> tri = mkrow({{0.0, 0.0}, {2.0, 0.0}, {1.0, 1.5}});
    eil::geom::assign_node_properties(tri.all());
    F("sec3.cw.tri", eil::geom::clockwise_check(tri.all()));
    eil::Array<PointPtr> empty_row;
    F("sec3.cw.empty", eil::geom::clockwise_check(empty_row.all()));
    resetAll();
    eil::Array<PointPtr> broken = mkrow({{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}});
    eil::geom::assign_node_properties(broken.all());
    broken[1]->pointR = broken[3];
    tryExc("sec3.crc.broken", [&] { eil::geom::closed_row_check(broken.all()); });
    resetAll();
    eil::Array<PointPtr> ok2 = mkrow({{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}});
    eil::geom::assign_node_properties(ok2.all());
    tryExc("sec3.crc.ok", [&] { eil::geom::closed_row_check(ok2.all()); });
    tryExc("sec3.cw.broken",
           [&] { F("sec3.cw.broken.v", eil::geom::clockwise_check(broken.all())); });
    F("sec3.area.ccw", eil::geom::calc_area(ok2.all()));
    F("sec3.area.cw", eil::geom::calc_area(cw.all()));
    F("sec3.area.tri", eil::geom::calc_area(tri.all()));
    {
        eil::Array<PointPtr> one;
        one.append(ok2[0]);
        F("sec3.area.one", eil::geom::calc_area(one.all()));
        eil::Array<PointPtr> two;
        two.append(ok2[0]);
        two.append(ok2[2]);
        F("sec3.area.two", eil::geom::calc_area(two.all()));
        eil::Array<PointPtr> nanq;
        nanq.append(ok2[0]);
        nanq.append(ok2[1]);
        nanq.append(badPt());
        nanq.append(ok2[3]);
        F("sec3.area.nan", eil::geom::calc_area(nanq.all()));
        tryExc("sec3.area.empty",
               [&] { F("sec3.area.empty.v", eil::geom::calc_area(empty_row.all())); });
    }
    resetAll();
    eil::Array<PointPtr> cross = mkrow({{0.0, 0.0}, {1.0, 1.0}, {1.0, 0.0}, {0.0, 1.0}});
    eil::geom::assign_node_properties(cross.all());
    auto ib = eil::geom::intersecting_boundary(cross.all());
    I("sec3.ib.cross.n", static_cast<long>(ib.length()));
    for (size_t i = 0; i < ib.length(); ++i) {
        eil::writefln("sec3.ib.cross.%d|%d,%d,%d,%d", i, ib[i][0]->ID, ib[i][1]->ID,
                      ib[i][2]->ID, ib[i][3]->ID);
    }
    resetAll();
    eil::Array<PointPtr> sq = mkrow({{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}});
    eil::geom::assign_node_properties(sq.all());
    auto ib2 = eil::geom::intersecting_boundary(sq.all());
    I("sec3.ib.square.n", static_cast<long>(ib2.length()));
    resetAll();
    eil::Array<PointPtr> hex = mkrow({{0.0, 0.0}, {2.0, 0.0}, {3.0, 1.0}, {2.0, 2.0},
                                      {0.0, 2.0}, {-1.0, 1.0}});
    eil::geom::assign_node_properties(hex.all());
    auto ib3 = eil::geom::intersecting_boundary(hex.all());
    I("sec3.ib.hex.n", static_cast<long>(ib3.length()));
    I("sec3.ib.empty.n",
      static_cast<long>(eil::geom::intersecting_boundary(empty_row.all()).length()));
    resetAll();
    eil::Array<PointPtr> f4 = mkrow({{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}});
    eil::geom::assign_node_properties(f4.all());
    for (auto& p : f4) eil::geom::makeFace(p->ID, p->pointR->ID);
    auto ifc = eil::geom::intersecting_faces(f4.all());
    I("sec3.ifc.square.n", static_cast<long>(ifc.length()));
    resetAll();
    eil::Array<PointPtr> g8 =
        mkrow({{0.0, 0.0}, {2.0, 0.0}, {2.0, 1.0}, {1.0, 1.0}, {1.0, 2.0}, {0.0, 2.0},
               {0.0, 1.0}, {2.0, 2.0}});
    eil::geom::assign_node_properties(g8.all());
    for (auto& p : g8) eil::geom::makeFace(p->ID, p->pointR->ID);
    auto ifc2 = eil::geom::intersecting_faces(g8.all());
    I("sec3.ifc.cross.n", static_cast<long>(ifc2.length()));
    for (size_t i = 0; i < ifc2.length(); ++i) {
        eil::writefln("sec3.ifc.cross.%d|%d,%d,%d,%d", i, ifc2[i][0]->ID, ifc2[i][1]->ID,
                      ifc2[i][2]->ID, ifc2[i][3]->ID);
    }
    I("sec3.ifc.empty.n",
      static_cast<long>(eil::geom::intersecting_faces(empty_row.all()).length()));
    F("sec3.afl.square", eil::geom::average_face_length(sq.all()));
    F("sec3.afl.hex", eil::geom::average_face_length(hex.all()));
    {
        eil::Array<PointPtr> one;
        one.append(sq[0]);
        F("sec3.afl.one", eil::geom::average_face_length(one.all()));
        F("sec3.afl.empty", eil::geom::average_face_length(empty_row.all()));
        eil::Array<PointPtr> dup;
        dup.append(sq[0]);
        dup.append(sq[0]);
        F("sec3.afl.dup", eil::geom::average_face_length(dup.all()));
    }
    F("sec3.abv.90", eil::geom::angle_between_vectors(v2(1.0, 0.0), v2(0.0, 1.0)));
    F("sec3.abv.m90", eil::geom::angle_between_vectors(v2(1.0, 0.0), v2(0.0, -1.0)));
    F("sec3.abv.0", eil::geom::angle_between_vectors(v2(1.0, 0.0), v2(2.0, 0.0)));
    F("sec3.abv.180", eil::geom::angle_between_vectors(v2(1.0, 0.0), v2(-1.0, 0.0)));
    F("sec3.abv.45", eil::geom::angle_between_vectors(v2(1.0, 0.0), v2(1.0, 1.0)));
    F("sec3.abv.m45", eil::geom::angle_between_vectors(v2(1.0, 0.0), v2(1.0, -1.0)));
    F("sec3.abv.60",
      eil::geom::angle_between_vectors(v2(1.0, 0.0), v2(0.5, 0.8660254037844386)));
    F("sec3.abv.zero", eil::geom::angle_between_vectors(v2(1.0, 0.0), v2(0.0, 0.0)));
    F("sec3.abv.zerobo", eil::geom::angle_between_vectors(v2(0.0, 0.0), v2(0.0, 0.0)));
    F("sec3.abv.nan", eil::geom::angle_between_vectors(v2(1.0, 0.0), v2(dinit(), 1.0)));
    F("sec3.abv.big",
      eil::geom::angle_between_vectors(v2(1.0e200, 1.0e200), v2(1.0e200, -1.0e200)));
    F("sec3.abv.tiny",
      eil::geom::angle_between_vectors(v2(1.0e-200, 0.0), v2(0.0, 1.0e-200)));
    PointPtr pin = mkp(500, 0.25, 0.25);
    PointPtr pout = mkp(501, 2.0, 0.5);
    {
        eil::Array<PointPtr> a1;
        a1.append(pin);
        eil::Array<PointPtr> a2;
        a2.append(pout);
        eil::Array<PointPtr> a3;
        a3.append(pin);
        a3.append(pout);
        I("sec3.ipr.inside", static_cast<int>(eil::geom::inside_prev_row_check(a1.all(), sq.all())));
        I("sec3.ipr.outside", static_cast<int>(eil::geom::inside_prev_row_check(a2.all(), sq.all())));
        I("sec3.ipr.mixed", static_cast<int>(eil::geom::inside_prev_row_check(a3.all(), sq.all())));
        I("sec3.ipr.emptynew",
          static_cast<int>(eil::geom::inside_prev_row_check(empty_row.all(), sq.all())));
        eil::Array<PointPtr> none;
        I("sec3.ipr.emptyrow",
          static_cast<int>(eil::geom::inside_prev_row_check(a1.all(), none.all())));
    }
}

PointPtr badPt() { return mkp(700, 1.0, dinit()); }

//---------------------------------------------------------------- group make
void grpMake() {
    resetAll();
    PointPtr a = eil::geom::make2DPoint(0.0, 0.0);
    PointPtr b = eil::geom::make2DPoint(1.0, 0.0);
    PointPtr c = eil::geom::make2DPoint(1.0, 1.0);
    PointPtr d = eil::geom::make2DPoint(0.0, 1.0);
    I("make.npoints", static_cast<long>(eil::geom::POINT_LIST.length()));
    I("make.idcount", static_cast<long>(Point::ID_count));
    UFacePtr f0 = eil::geom::makeFace(0, 1);
    dumpF("make.f0", f0);
    UFacePtr f1 = eil::geom::makeFace(1, 0);
    dumpF("make.f1", f1);
    I("make.nfaces", static_cast<long>(eil::geom::FACE_LIST.length()));
    I("make.facecount", static_cast<long>(UFace::face_ID_count));
    auto q1 = eil::geom::findFace(s2(0, 1), true);
    dumpF("make.q01", q1);
    auto q2 = eil::geom::findFace(s2(1, 0), true);
    dumpF("make.q10", q2);
    auto q3 = eil::geom::findFace(s2(0, 1), false);
    dumpF("make.q01nd", q3);
    auto q4 = eil::geom::findFace(s2(5, 6), true);
    T("make.q56", q4 == nullptr ? "null" : "found");
    // the [0,0] default of `reverse_case` (paver.d:516) matches any directional query
    UFacePtr fz = eil::geom::makeFace(0, 0);
    (void)fz;
    auto q5 = eil::geom::findFace(s2(5, 6), true);
    dumpF("make.q56.afterzero", q5);
    auto q6 = eil::geom::findFace(s2(5, 6), false);
    dumpF("make.q56.nd", q6);
    // makeCell: fresh faces, a reused face with each sign, and both present
    resetAll();
    eil::Array<PointPtr> ps = mkrow({{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}});
    CellPtr c1 = eil::geom::makeCell(ps.all());
    dumpC("make.cell.fresh", c1);
    dumpLists("make.lists.aftercell");
    {
        eil::Array<PointPtr> sw;
        sw.append(ps[2]);
        sw.append(ps[3]);
        sw.append(ps[0]);
        sw.append(ps[1]);
        CellPtr c2 = eil::geom::makeCell(sw.all());
        dumpC("make.cell.second", c2);
    }
    I("make.cells.n", static_cast<long>(eil::geom::CELL_LIST.length()));
    UFacePtr pre = eil::geom::makeFace(0, 3);
    dumpF("make.pre", pre);
    CellPtr c3 = eil::geom::makeCell(ps.all());
    dumpC("make.cell.withface", c3);
    resetAll();
    eil::Array<PointPtr> trio = mkrow({{0.0, 0.0}, {1.0, 0.0}, {0.5, 1.0}});
    CellPtr c4 = eil::geom::makeCell(trio.all());
    dumpC("make.cell.tri", c4);
    S("make.cell.tri.print", c4->print());
    resetAll();
    eil::Array<PointPtr> nothing;
    CellPtr c5 = eil::geom::makeCell(nothing.all());
    dumpC("make.cell.empty", c5);
    resetAll();
    PointPtr rep = eil::geom::make2DPoint(0.0, 0.0);
    eil::Array<PointPtr> reps;
    reps.append(rep);
    reps.append(rep);
    reps.append(rep);
    reps.append(rep);
    CellPtr c6 = eil::geom::makeCell(reps.all());
    dumpC("make.cell.repeated", c6);
}

//---------------------------------------------------------------- group node
// ldc2 constant-folds paver.d:774's `[(110./180.)*PI, (240./180.)*PI, (315./180.)*PI]`
// to these three bit patterns; rows node.tol0/1/2 are the folded values and the first
// two are 1 ULP off any run-time evaluation of the same expression (the same constants
// paver.cpp uses inside node_category, and the reason rows node.cat0.exactcat /
// node.cat1.exactcat land on the category they do).
static double fold_bits(unsigned long long u) {
    double d;
    std::memcpy(&d, &u, sizeof d);
    return d;
}

void grpNode() {
    std::vector<double> tols = {fold_bits(0x3ffeb7c166fdfe3aull),
                                fold_bits(0x4010c152382d7366ull),
                                fold_bits(0x4015fdbbe9bba775ull)};
    for (size_t i = 0; i < tols.size(); ++i) F("node.tol" + eil::to_string(i), tols[i]);
    for (size_t i = 0; i < tols.size(); ++i) {
        double t = tols[i];
        std::string tg = "node.cat" + eil::to_string(i);
        // exactly one ULP either side of the threshold, through std.math.nextafter
        double dl = dn1(t);
        double du = up1(t);
        F(tg + ".below", dl);
        S(tg + ".belowcat", eil::geom::node_category(dl));
        S(tg + ".exactcat", eil::geom::node_category(t));
        F(tg + ".above", du);
        S(tg + ".abovecat", eil::geom::node_category(du));
        S(tg + ".twoup", eil::geom::node_category(up1(du)));
        S(tg + ".twodn", eil::geom::node_category(dn1(dl)));
    }
    S("node.cat.zero", eil::geom::node_category(0.0));
    S("node.cat.pi", eil::geom::node_category(static_cast<double>(EIL_PI)));
    S("node.cat.2pi", eil::geom::node_category(static_cast<double>(2 * EIL_PI)));
    tryExc("node.cat.2piup.throw",
           [&] { S("node.cat.2piup", eil::geom::node_category(6.283185307179587)); });
    S("node.cat.nan", eil::geom::node_category(dinit()));
    tryExc("node.cat.over.throw", [&] {
        S("node.cat.over", eil::geom::node_category(static_cast<double>(2 * EIL_PI + 0.1)));
    });
    tryExc("node.cat.over2.throw",
           [&] { S("node.cat.over2", eil::geom::node_category(1.0e300)); });
    // assign_node_properties over the fixtures
    resetAll();
    eil::Array<PointPtr> sq = mkrow({{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}});
    eil::geom::assign_node_properties(sq.all());
    for (size_t i = 0; i < sq.length(); ++i) {
        PointPtr p = sq[i];
        std::string t = "node.sq" + eil::to_string(i);
        F(t + ".ang", p->interior_angle);
        S(t + ".cat", p->node_cat);
        F(t + ".VR0", p->V_R[0]);
        F(t + ".VR1", p->V_R[1]);
        F(t + ".VL0", p->V_L[0]);
        F(t + ".VL1", p->V_L[1]);
        I(t + ".L", static_cast<long>(p->pointL->ID));
        I(t + ".R", static_cast<long>(p->pointR->ID));
    }
    resetAll();
    eil::Array<PointPtr> hex = mkrow({{0.0, 0.0}, {2.0, 0.0}, {3.0, 1.0}, {2.0, 2.0},
                                      {0.0, 2.0}, {-1.0, 1.0}});
    eil::geom::assign_node_properties(hex.all());
    for (size_t i = 0; i < hex.length(); ++i) {
        F("node.hex" + eil::to_string(i) + ".ang", hex[i]->interior_angle);
        S("node.hex" + eil::to_string(i) + ".cat", hex[i]->node_cat);
    }
    resetAll();
    eil::Array<PointPtr> cwq = mkrow({{0.0, 0.0}, {0.0, 1.0}, {1.0, 1.0}, {1.0, 0.0}});
    eil::geom::assign_node_properties(cwq.all());
    for (size_t i = 0; i < cwq.length(); ++i)
        F("node.cw" + eil::to_string(i) + ".ang", cwq[i]->interior_angle);
    // a straight row: the middle node sits exactly on pi
    resetAll();
    eil::Array<PointPtr> line = mkrow({{0.0, 0.0}, {1.0, 0.0}, {2.0, 0.0}});
    eil::geom::assign_node_properties(line.all());
    for (size_t i = 0; i < line.length(); ++i) {
        F("node.line" + eil::to_string(i) + ".ang", line[i]->interior_angle);
        S("node.line" + eil::to_string(i) + ".cat", line[i]->node_cat);
    }
    // the 3.141..3.142 exemption: an angle inside that band is left alone
    resetAll();
    eil::Array<PointPtr> band =
        mkrow({{0.0, 0.0}, {1.0, 0.0001}, {2.0, 0.0}, {1.0, -1.0}});
    eil::geom::assign_node_properties(band.all());
    for (size_t i = 0; i < band.length(); ++i) {
        F("node.band" + eil::to_string(i) + ".ang", band[i]->interior_angle);
        S("node.band" + eil::to_string(i) + ".cat", band[i]->node_cat);
    }
    // two points: pointL and pointR are the same node, r clamps to 1
    resetAll();
    eil::Array<PointPtr> two = mkrow({{0.0, 0.0}, {1.0, 0.0}});
    tryExc("node.two.throw", [&] { eil::geom::assign_node_properties(two.all()); });
    for (size_t i = 0; i < two.length(); ++i) {
        F("node.two" + eil::to_string(i) + ".ang", two[i]->interior_angle);
        S("node.two" + eil::to_string(i) + ".cat", two[i]->node_cat);
    }
    // one point: every distance is 0, r is 0/0, the assert fires
    resetAll();
    eil::Array<PointPtr> one = mkrow({{0.0, 0.0}});
    tryExc("node.one.throw", [&] { eil::geom::assign_node_properties(one.all()); });
    dumpP("node.one.after", one[0]);
    // a duplicate point in a longer row: a==0 -> r NaN -> assert
    resetAll();
    eil::Array<PointPtr> dup = mkrow({{0.0, 0.0}, {0.0, 0.0}, {1.0, 1.0}});
    tryExc("node.dup.throw", [&] { eil::geom::assign_node_properties(dup.all()); });
    resetAll();
    eil::Array<PointPtr> nanrow = mkrow({{0.0, 0.0}, {1.0, dinit()}, {1.0, 1.0}});
    tryExc("node.nanrow.throw", [&] { eil::geom::assign_node_properties(nanrow.all()); });
    resetAll();
    eil::Array<PointPtr> nothing;
    tryExc("node.emptyrow.throw", [&] { eil::geom::assign_node_properties(nothing.all()); });
    // adjust_row_ends
    resetAll();
    eil::Array<PointPtr> oct = mkrow({{0.0, 0.0}, {1.0, 0.0}, {2.0, 0.0}, {2.0, 1.0},
                                      {2.0, 2.0}, {1.0, 2.0}, {0.0, 2.0}, {0.0, 1.0}});
    eil::geom::assign_node_properties(oct.all());
    std::vector<std::string> before;
    for (auto& p : oct) before.push_back(p->node_cat);
    eil::geom::adjust_row_ends(oct.all(), eil::geom::initial_size, true);
    for (size_t i = 0; i < oct.length(); ++i) {
        S("node.are.b1." + eil::to_string(i), before[i]);
        S("node.are.a1." + eil::to_string(i), oct[i]->node_cat);
    }
    resetAll();
    eil::Array<PointPtr> wedge =
        mkrow({{0.0, 0.0}, {4.0, 0.0}, {4.0, 0.4}, {0.5, 2.0}, {0.0, 1.0}, {0.0, 0.5}});
    eil::geom::assign_node_properties(wedge.all());
    eil::geom::adjust_row_ends(wedge.all(), eil::geom::initial_size, true);
    for (size_t i = 0; i < wedge.length(); ++i)
        S("node.are.wedge." + eil::to_string(i), wedge[i]->node_cat);
    eil::geom::adjust_row_ends(wedge.all(), eil::geom::initial_size, false);
    for (size_t i = 0; i < wedge.length(); ++i)
        S("node.are.wedge.b0." + eil::to_string(i), wedge[i]->node_cat);
    // growth / reduction arms, driven by hand-set prev_node and vectors
    resetAll();
    eil::Array<PointPtr> ch = mkrow({{0.0, 0.0}, {1.0, 0.0}, {2.0, 0.0}, {2.0, 1.0},
                                     {1.0, 1.0}, {0.0, 1.0}});
    eil::geom::assign_node_properties(ch.all());
    eil::geom::adjust_row_ends(ch.all(), eil::geom::initial_size, true);
    ch[3]->prev_node = ch[0];
    ch[3]->V_R = v2(4.0, 0.0);
    ch[3]->V_L = v2(0.0, 4.0);
    ch[0]->V_R = v2(1.0, 0.0);
    ch[0]->V_L = v2(0.0, 1.0);
    eil::geom::adjust_row_ends(ch.all(), eil::geom::initial_size, false);
    for (size_t i = 0; i < ch.length(); ++i)
        S("node.are.growth." + eil::to_string(i), ch[i]->node_cat);
    F("node.are.growth.g", ch[3]->growth);
    resetAll();
    eil::Array<PointPtr> ch2 = mkrow({{0.0, 0.0}, {1.0, 0.0}, {2.0, 0.0}, {2.0, 1.0},
                                      {1.0, 1.0}, {0.0, 1.0}});
    eil::geom::assign_node_properties(ch2.all());
    eil::geom::adjust_row_ends(ch2.all(), 1.0, true);
    ch2[3]->prev_node = ch2[0];
    ch2[3]->V_R = v2(0.01, 0.0);
    ch2[3]->V_L = v2(0.0, 0.01);
    ch2[0]->V_R = v2(1.0, 0.0);
    ch2[0]->V_L = v2(0.0, 1.0);
    eil::geom::adjust_row_ends(ch2.all(), 1.0, false);
    for (size_t i = 0; i < ch2.length(); ++i)
        S("node.are.shrink." + eil::to_string(i), ch2[i]->node_cat);
    F("node.are.shrink.g", ch2[3]->growth);
    // no row end at all in the row: the smallest-angled node is promoted
    resetAll();
    eil::Array<PointPtr> nr =
        mkrow({{0.0, 0.0}, {2.0, 0.0}, {2.0, 1.0}, {1.0, 1.6}, {0.0, 1.0}});
    eil::geom::assign_node_properties(nr.all());
    for (auto& p : nr) p->node_cat = "row side";
    eil::geom::adjust_row_ends(nr.all(), eil::geom::initial_size, false);
    for (size_t i = 0; i < nr.length(); ++i)
        S("node.are.norend." + eil::to_string(i), nr[i]->node_cat);
    // neighbouring row ends: the larger-angle one loses (paver.d:745-746)
    resetAll();
    eil::Array<PointPtr> ends = mkrow({{0.0, 0.0}, {1.0, 0.0}, {2.0, 0.0}, {2.0, 1.0},
                                       {2.0, 2.0}, {1.0, 2.0}, {0.0, 2.0}, {0.0, 1.0}});
    eil::geom::assign_node_properties(ends.all());
    for (auto& p : ends) p->node_cat = "row side";
    ends[0]->node_cat = "row end";
    ends[1]->node_cat = "row end";
    ends[4]->node_cat = "row end";
    ends[5]->node_cat = "row end";
    eil::geom::adjust_row_ends(ends.all(), eil::geom::initial_size, true);
    for (size_t i = 0; i < ends.length(); ++i)
        S("node.are.ends." + eil::to_string(i), ends[i]->node_cat);
    // the cell-size-reduction arm (paver.d:737-740)
    resetAll();
    eil::Array<PointPtr> red = mkrow({{0.0, 0.0}, {1.0, 0.0}, {2.0, 0.0}, {3.0, 0.0},
                                      {3.0, 1.0}, {3.0, 2.0}, {2.0, 2.0}, {1.0, 2.0},
                                      {0.0, 2.0}, {0.0, 1.0}});
    eil::geom::assign_node_properties(red.all());
    for (auto& p : red) p->node_cat = "row side";
    red[0]->node_cat = "row end";
    red[1]->node_cat = "row end";
    red[7]->prev_node = red[2];
    red[7]->V_R = v2(0.01, 0.0);
    red[7]->V_L = v2(0.0, 0.01);
    red[2]->V_R = v2(1.0, 0.0);
    red[2]->V_L = v2(0.0, 1.0);
    eil::geom::adjust_row_ends(red.all(), 1.0, false);
    for (size_t i = 0; i < red.length(); ++i)
        S("node.are.reduce." + eil::to_string(i), red[i]->node_cat);
    F("node.are.reduce.g", red[7]->growth);
}

//---------------------------------------------------------------- group row
void grpRow() {
    resetAll();
    eil::Array<PointPtr> oct = mkrow({{0.0, 0.0}, {1.0, 0.0}, {2.0, 0.0}, {2.0, 1.0},
                                      {2.0, 2.0}, {1.0, 2.0}, {0.0, 2.0}, {0.0, 1.0}});
    eil::geom::assign_node_properties(oct.all());
    eil::geom::adjust_row_ends(oct.all(), eil::geom::initial_size, true);
    for (size_t i = 0; i < oct.length(); ++i) oct[i]->store_rowID(i);
    for (auto& p : oct) eil::geom::makeFace(p->ID, p->pointR->ID);
    dumpLists("row.before");
    eil::Array<PointPtr> New_Nodes;
    for (size_t i = 0; i < oct.length(); ++i) {
        S("row.gen." + eil::to_string(i) + ".cat", oct[i]->node_cat);
        eil::geom::node_gen(oct.all(), i, New_Nodes);
        eil::writefln("row.gen.%d.after|new=%d|cells=%d|faces=%d|pts=%d", i,
                      New_Nodes.length(), eil::geom::CELL_LIST.length(),
                      eil::geom::FACE_LIST.length(), eil::geom::POINT_LIST.length());
        for (size_t j = 0; j < New_Nodes.length(); ++j)
            dumpP("row.gen." + eil::to_string(i) + ".N" + eil::to_string(j), New_Nodes[j]);
    }
    dumpLists("row.after");
    // generate_new_row: needs two consecutive row-side nodes
    resetAll();
    eil::Array<PointPtr> sq = mkrow({{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}});
    eil::geom::assign_node_properties(sq.all());
    eil::geom::adjust_row_ends(sq.all(), eil::geom::initial_size, true);
    for (size_t i = 0; i < sq.length(); ++i) sq[i]->store_rowID(i);
    for (auto& p : sq) eil::geom::makeFace(p->ID, p->pointR->ID);
    tryExc("row.gnr.sq.throw",
           [&] {
               eil::Array<PointPtr> r = eil::geom::generate_new_row(sq.all());
               eil::writefln("row.gnr.sq.n|%d", r.length());
               for (size_t j = 0; j < r.length(); ++j)
                   dumpP("row.gnr.sq." + eil::to_string(j), r[j]);
           });
    dumpLists("row.gnr.sq.lists");
    resetAll();
    eil::Array<PointPtr> hex = mkrow({{0.0, 0.0}, {2.0, 0.0}, {3.0, 1.0}, {2.0, 2.0},
                                      {0.0, 2.0}, {-1.0, 1.0}});
    eil::geom::assign_node_properties(hex.all());
    eil::geom::adjust_row_ends(hex.all(), eil::geom::initial_size, true);
    for (size_t i = 0; i < hex.length(); ++i) hex[i]->store_rowID(i);
    tryExc("row.gnr.hex.throw",
           [&] {
               eil::Array<PointPtr> r2 = eil::geom::generate_new_row(hex.all());
               eil::writefln("row.gnr.hex.n|%d", r2.length());
               for (size_t j = 0; j < r2.length(); ++j)
                   dumpP("row.gnr.hex." + eil::to_string(j), r2[j]);
           });
    dumpLists("row.gnr.hex.lists");
    // a row with no row-side node at all
    resetAll();
    eil::Array<PointPtr> noside =
        mkrow({{0.0, 0.0}, {1.0, 0.0}, {2.0, 0.0}, {2.0, 1.0}, {2.0, 2.0}, {1.0, 2.0},
               {0.0, 2.0}, {0.0, 1.0}});
    eil::geom::assign_node_properties(noside.all());
    for (auto& p : noside) p->node_cat = "row end";
    tryExc("row.gnr.none.throw", [&] { eil::geom::generate_new_row(noside.all()); });
    eil::Array<PointPtr> nothing;
    tryExc("row.gnr.empty.throw", [&] { eil::geom::generate_new_row(nothing.all()); });
    // summed_squareness
    resetAll();
    eil::Array<PointPtr> ps =
        mkrow({{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}, {2.0, 0.5}});
    CellPtr cc = eil::geom::makeCell(
        eil::Array<PointPtr>{ps[0], ps[1], ps[2], ps[3]}.all());
    F("row.ss.in", eil::geom::summed_squareness(ps[0]));
    F("row.ss.in2", eil::geom::summed_squareness(ps[2]));
    F("row.ss.cell", eil::geom::squareness(cc));
    tryExc("row.ss.orphan.throw",
           [&] { F("row.ss.orphan", eil::geom::summed_squareness(ps[4])); });
}

//---------------------------------------------------------------- group smk
void grpSmk() {
    using eil::geom::CELL_LIST;
    using eil::geom::FACE_LIST;
    using eil::geom::POINT_LIST;
    resetAll();
    eil::Array<PointPtr> sq = mkrow({{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}});
    eil::geom::assign_node_properties(sq.all());
    for (auto& p : sq) eil::geom::makeFace(p->ID, p->pointR->ID);
    eil::geom::makeCell(eil::Array<PointPtr>{sq[0], sq[1], sq[2], sq[3]}.all());
    for (size_t i = 0; i < sq.length(); ++i)
        dumpP("smk.before." + eil::to_string(i), sq[i]);
    F("smk.move.b1f0", eil::geom::smoothing(sq.all(), sq.all(), true, false));
    for (size_t i = 0; i < sq.length(); ++i)
        dumpP("smk.after.b1f0." + eil::to_string(i), sq[i]);
    F("smk.move.b0f1", eil::geom::smoothing(sq.all(), sq.all(), false, true));
    for (size_t i = 0; i < sq.length(); ++i)
        dumpP("smk.after.b0f1." + eil::to_string(i), sq[i]);
    F("smk.move.b1f1", eil::geom::smoothing(sq.all(), sq.all(), true, true));
    F("smk.move.b0f0", eil::geom::smoothing(sq.all(), sq.all(), false, false));
    for (size_t i = 0; i < sq.length(); ++i)
        dumpP("smk.after.b0f0." + eil::to_string(i), sq[i]);
    // a node that no face mentions
    resetAll();
    eil::Array<PointPtr> sq2 = mkrow({{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}});
    eil::geom::assign_node_properties(sq2.all());
    for (auto& p : sq2) eil::geom::makeFace(p->ID, p->pointR->ID);
    PointPtr orphan = eil::geom::make2DPoint(5.0, 5.0);
    orphan->node_cat = "row side";
    eil::Array<PointPtr> orph;
    orph.append(orphan);
    eil::Array<PointPtr> nothing;
    F("smk.orphan", eil::geom::smoothing(orph.all(), sq2.all(), false, false));
    F("smk.orphan.b1", eil::geom::smoothing(orph.all(), sq2.all(), true, false));
    // empty surface
    F("smk.empty", eil::geom::smoothing(nothing.all(), sq2.all(), false, false));
    // biggest_cell / even_out_size / fix_IDs / point_ID_check
    resetAll();
    eil::Array<PointPtr> grid = mkrow({{0.0, 0.0}, {1.0, 0.0}, {2.0, 0.0}, {2.0, 1.0},
                                       {2.0, 2.0}, {1.0, 2.0}, {0.0, 2.0}, {0.0, 1.0}});
    eil::geom::assign_node_properties(grid.all());
    eil::geom::adjust_row_ends(grid.all(), 1.0, true);
    for (size_t i = 0; i < grid.length(); ++i) grid[i]->store_rowID(i);
    for (auto& p : grid) eil::geom::makeFace(p->ID, p->pointR->ID);
    CellPtr ca = eil::geom::makeCell(
        eil::Array<PointPtr>{grid[0], grid[1], grid[5], grid[6]}.all());
    CellPtr cb = eil::geom::makeCell(
        eil::Array<PointPtr>{grid[1], grid[2], grid[3], grid[4]}.all());
    eil::Array<CellPtr> both;
    both.append(ca);
    both.append(cb);
    F("smk.biggest.1", eil::geom::biggest_cell(both.all(), 1.0));
    F("smk.biggest.0", eil::geom::biggest_cell(both.all(), 0.0));
    eil::Array<CellPtr> nocells;
    F("smk.biggest.empty", eil::geom::biggest_cell(nocells.all(), 2.0));
    eil::Array<CellPtr> onecell;
    onecell.append(ca);
    F("smk.biggest.nan", eil::geom::biggest_cell(onecell.all(), dinit()));
    F("smk.sq.a", eil::geom::squareness(ca));
    F("smk.sq.b", eil::geom::squareness(cb));
    dumpLists("smk.before.fix");
    FACE_LIST[0]->face_ID = 77;
    CELL_LIST[0]->cell_ID = 88;
    eil::geom::fix_IDs(FACE_LIST.all(), CELL_LIST.all());
    dumpLists("smk.after.fix");
    tryExc("smk.pidc.ok", [&] { eil::geom::point_ID_check(); });
    POINT_LIST[2]->ID = 4242;
    tryExc("smk.pidc.bad", [&] { eil::geom::point_ID_check(); });
    POINT_LIST[2]->ID = 2;
    // even_out_size moves nodes in place
    resetAll();
    eil::Array<PointPtr> row = mkrow({{0.0, 0.0}, {1.0, 0.0}, {2.5, 0.2}, {4.0, 0.0},
                                      {4.0, 1.0}, {2.0, 1.4}, {0.0, 1.0}});
    eil::geom::assign_node_properties(row.all());
    eil::geom::adjust_row_ends(row.all(), 1.0, true);
    for (size_t i = 0; i < row.length(); ++i)
        dumpP("smk.eos.before." + eil::to_string(i), row[i]);
    eil::geom::even_out_size(row.all());
    for (size_t i = 0; i < row.length(); ++i)
        dumpP("smk.eos.after." + eil::to_string(i), row[i]);
    resetAll();
    eil::Array<PointPtr> row2 = mkrow({{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}});
    eil::geom::assign_node_properties(row2.all());
    tryExc("smk.eos.tri", [&] { eil::geom::even_out_size(row2.all()); });
    // seam: non-closing on a hand-built row with an acute corner
    resetAll();
    eil::Array<PointPtr> wedge = mkrow({{0.0, 0.0}, {3.0, 0.0}, {3.0, 0.6}, {2.0, 0.6},
                                        {2.0, 1.2}, {1.0, 1.2}, {0.0, 1.2}, {0.0, 0.6}});
    eil::geom::assign_node_properties(wedge.all());
    eil::geom::adjust_row_ends(wedge.all(), 1.0, true);
    for (size_t i = 0; i < wedge.length(); ++i) wedge[i]->store_rowID(i);
    for (auto& p : wedge) eil::geom::makeFace(p->ID, p->pointR->ID);
    dumpLists("seam.b0.before");
    eil::Array<size_t> adjust_IDs;
    size_t left = wedge.length();
    eil::Array<PointPtr> inner =
        eil::geom::seam(wedge[2], wedge, adjust_IDs, left, false);
    eil::writefln("seam.b0.done|inner=%d|adjust=%d|left=%d|cells=%d|faces=%d|pts=%d",
                  inner.length(), adjust_IDs.length(), left, CELL_LIST.length(),
                  FACE_LIST.length(), POINT_LIST.length());
    for (size_t i = 0; i < inner.length(); ++i)
        dumpP("seam.b0.inner." + eil::to_string(i), inner[i]);
    for (size_t i = 0; i < adjust_IDs.length(); ++i)
        I("seam.b0.adj." + eil::to_string(i), static_cast<long>(adjust_IDs[i]));
    dumpLists("seam.b0.lists");
    // the special four-node closing case
    resetAll();
    eil::Array<PointPtr> four = mkrow({{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}});
    eil::geom::assign_node_properties(four.all());
    for (size_t i = 0; i < four.length(); ++i) four[i]->store_rowID(i);
    for (auto& p : four) eil::geom::makeFace(p->ID, p->pointR->ID);
    eil::Array<size_t> ad2;
    size_t left2 = four.length();
    eil::Array<PointPtr> inner2 = eil::geom::seam(four[0], four, ad2, left2, true);
    eil::writefln("seam.sp.done|inner=%d|adjust=%d|left=%d|cells=%d", inner2.length(),
                  ad2.length(), left2, CELL_LIST.length());
    // closing_seam over an odd row trips the assert with its D message
    resetAll();
    eil::Array<PointPtr> three = mkrow({{0.0, 0.0}, {1.0, 0.0}, {0.5, 1.0}});
    eil::geom::assign_node_properties(three.all());
    tryExc("seam.cs.odd", [&] { eil::geom::closing_seam(three.all()); });
    tryExc("seam.cs.empty", [&] { eil::geom::closing_seam(nothing.all()); });
    resetAll();
    eil::Array<PointPtr> six = mkrow({{0.0, 0.0}, {2.0, 0.0}, {3.0, 1.0}, {2.0, 2.0},
                                      {0.0, 2.0}, {-1.0, 1.0}});
    eil::geom::assign_node_properties(six.all());
    for (size_t i = 0; i < six.length(); ++i) six[i]->store_rowID(i);
    for (auto& p : six) eil::geom::makeFace(p->ID, p->pointR->ID);
    dumpLists("seam.cs6.before");
    tryExc("seam.cs6", [&] { eil::geom::closing_seam(six.all()); });
    dumpLists("seam.cs6.lists");
    // the >5-cell-spaces arm of seam (paver.d:1227-1230)
    resetAll();
    PointPtr st = eil::geom::make2DPoint(0.0, 0.0);
    PointPtr L1 = eil::geom::make2DPoint(-1.0, 0.0);
    PointPtr L2 = eil::geom::make2DPoint(-1.0, -0.05);
    PointPtr L3 = eil::geom::make2DPoint(-1.0, -0.2);
    PointPtr R1 = eil::geom::make2DPoint(1.0, 0.0);
    PointPtr R2 = eil::geom::make2DPoint(0.98, -0.05);
    PointPtr R3 = eil::geom::make2DPoint(0.96, -0.2);
    st->pointL = L1; st->pointR = R1;
    L1->pointL = L2; L1->pointR = st; L1->pointL->pointR = L1;
    R1->pointR = R2; R1->pointL = st; R1->pointR->pointL = R1;
    L2->pointL = L3; R2->pointR = R3;
    L3->pointR = L2; R3->pointL = R2;
    PointPtr L4 = eil::geom::make2DPoint(-1.0, -0.5);
    PointPtr R4 = eil::geom::make2DPoint(0.9, -0.5);
    L3->pointL = L4; R3->pointR = R4;
    eil::Array<PointPtr> wide;
    for (const PointPtr& p : {st, R1, R2, R3, R4, L4, L3, L2, L1}) wide.append(p);
    for (size_t i = 0; i < wide.length(); ++i) wide[i]->store_rowID(i);
    for (const PointPtr& p : {st, R1, R2, L1, L2})
        eil::geom::makeFace(p->ID, p->pointR->ID);
    dumpLists("seam.wide.before");
    eil::Array<size_t> ad3;
    size_t left3 = wide.length();
    eil::Array<PointPtr> inner3 = eil::geom::seam(st, wide, ad3, left3, false);
    eil::writefln("seam.wide.done|inner=%d|adjust=%d|left=%d|cells=%d", inner3.length(),
                  ad3.length(), left3, CELL_LIST.length());
    for (size_t i = 0; i < inner3.length(); ++i)
        dumpP("seam.wide.inner." + eil::to_string(i), inner3[i]);
    for (size_t i = 0; i < ad3.length(); ++i)
        I("seam.wide.adj." + eil::to_string(i), static_cast<long>(ad3[i]));
    dumpLists("seam.wide.lists");
}

//---------------------------------------------------------------- group seam2
void grpSeam2() {
    // seaming_procedure: the acute-node arm and the "smallest angle node" arm
    resetAll();
    eil::Array<PointPtr> wedge = mkrow({{0.0, 0.0}, {3.0, 0.0}, {3.0, 0.6}, {2.0, 0.6},
                                        {2.0, 1.2}, {1.0, 1.2}, {0.0, 1.2}, {0.0, 0.6}});
    eil::geom::assign_node_properties(wedge.all());
    eil::geom::adjust_row_ends(wedge.all(), 1.0, true);
    for (size_t i = 0; i < wedge.length(); ++i) wedge[i]->store_rowID(i);
    for (auto& p : wedge) eil::geom::makeFace(p->ID, p->pointR->ID);
    dumpLists("seam2.a.before");
    eil::Array<PointPtr> wcopy = wedge;
    tryExc("seam2.a", [&] { eil::geom::seaming_procedure(wcopy); });
    eil::writefln("seam2.a.done|row=%d|cells=%d|faces=%d|pts=%d", wcopy.length(),
                  eil::geom::CELL_LIST.length(), eil::geom::FACE_LIST.length(),
                  eil::geom::POINT_LIST.length());
    for (size_t i = 0; i < wcopy.length(); ++i)
        dumpP("seam2.a.row." + eil::to_string(i), wcopy[i]);
    dumpLists("seam2.a.lists");
    resetAll();
    eil::Array<PointPtr> obt =
        mkrow({{0.0, 0.0}, {4.0, 0.0}, {4.0, 2.0}, {2.0, 2.4}, {0.0, 2.0}});
    eil::geom::assign_node_properties(obt.all());
    eil::geom::adjust_row_ends(obt.all(), 1.0, true);
    for (size_t i = 0; i < obt.length(); ++i) obt[i]->store_rowID(i);
    for (auto& p : obt) eil::geom::makeFace(p->ID, p->pointR->ID);
    eil::Array<PointPtr> ocopy = obt;
    tryExc("seam2.b", [&] { eil::geom::seaming_procedure(ocopy); });
    eil::writefln("seam2.b.done|row=%d|cells=%d", ocopy.length(),
                  eil::geom::CELL_LIST.length());
    for (size_t i = 0; i < ocopy.length(); ++i)
        dumpP("seam2.b.row." + eil::to_string(i), ocopy[i]);
    resetAll();
    eil::Array<PointPtr> none;
    tryExc("seam2.empty", [&] { eil::geom::seaming_procedure(none); });
}

//---------------------------------------------------------------- group pave
// The probe's pave() helper: one PavedGrid construction, its six exported fields, the
// two module globals the constructor leaves behind, and the complete POINT_LIST /
// FACE_LIST / CELL_LIST in D's order. The catch order is the port's stand-in for D's
// Exception / Error split (wave-02-brief §1.1): an ErrorBase must NOT land in the
// EXC arm, and rows pave.sq1 / pave.odd / pave.cw / pave.bowtie are what pins that.
void paveOne(const std::string& tag, const AAA& b) {
    resetAll();
    try {
        eil::geom::PavedGrid g(b);
        eil::writefln("pave.%s.fields|dims=%d|nv=%d|nf=%d|nc=%d|nb=%d", tag,
                      g.dimensions, g.nvertices, g.nfaces, g.ncells, g.nboundaries);
        S("pave." + tag + ".label", g.label);
        F("pave." + tag + ".initial_size", eil::geom::initial_size);
        F("pave." + tag + ".avr_area", eil::geom::avr_area);
        dumpLists("pave." + tag);
    } catch (const eil::ExceptionBase& e) {
        eil::writefln("pave.%s|EXC|%s", tag, esc(e.msg()));
        eil::writefln("pave.%s.after|pts=%d|faces=%d|cells=%d", tag,
                      eil::geom::POINT_LIST.length(), eil::geom::FACE_LIST.length(),
                      eil::geom::CELL_LIST.length());
    } catch (const eil::ErrorBase& e) {
        eil::writefln("pave.%s|ERR|%s", tag, esc(e.msg()));
        eil::writefln("pave.%s.after|pts=%d|faces=%d|cells=%d", tag,
                      eil::geom::POINT_LIST.length(), eil::geom::FACE_LIST.length(),
                      eil::geom::CELL_LIST.length());
    }
    std::fflush(stdout);
}

// D's square(n, w, h): n points along each of the four sides, starting at each corner
// and walking anticlockwise, so the boundary has 4n points.
AAA squareB(size_t n, double w, double h) {
    AAA b;
    for (size_t i = 0; i < n; ++i) { AA e; e.append(w * i / static_cast<double>(n));
        e.append(0.0); b.append(e); }
    for (size_t i = 0; i < n; ++i) { AA e; e.append(w);
        e.append(h * i / static_cast<double>(n)); b.append(e); }
    for (size_t i = 0; i < n; ++i) { AA e; e.append(w - w * i / static_cast<double>(n));
        e.append(h); b.append(e); }
    for (size_t i = 0; i < n; ++i) { AA e; e.append(0.0);
        e.append(h - h * i / static_cast<double>(n)); b.append(e); }
    return b;
}

static void sideB(AAA& b, double p0, double p1, double q0, double q1, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        AA e;
        e.append(p0 + (q0 - p0) * i / static_cast<double>(n));
        e.append(p1 + (q1 - p1) * i / static_cast<double>(n));
        b.append(e);
    }
}

AAA lshapeB(size_t n) {
    AAA b;
    sideB(b, 0.0, 0.0, 2.0, 0.0, n);
    sideB(b, 2.0, 0.0, 2.0, 1.0, n);
    sideB(b, 2.0, 1.0, 1.0, 1.0, n);
    sideB(b, 1.0, 1.0, 1.0, 2.0, n);
    sideB(b, 1.0, 2.0, 0.0, 2.0, n);
    sideB(b, 0.0, 2.0, 0.0, 0.0, n);
    return b;
}

AAA wedgeB(size_t n) {
    AAA b;
    sideB(b, 0.0, 0.0, 4.0, 0.0, n);
    sideB(b, 4.0, 0.0, 4.0, 0.4, n);
    sideB(b, 4.0, 0.4, 0.5, 2.0, n);
    sideB(b, 0.5, 2.0, 0.0, 1.0, n);
    return b;
}

// circle(m, r): 2m points on a circle. `2*PI*i/cnt` is computed in Phobos' 80-bit real
// and narrowed only on the store, and cos/sin of one argument must stay two libm calls
// (eil::cos / eil::sin), or glibc's fused sincos shifts them by 1 ULP.
AAA circleB(size_t m, double r) {
    AAA b;
    size_t cnt = 2 * m;
    for (size_t i = 0; i < cnt; ++i) {
        double t = static_cast<double>(
            2 * EIL_PI * static_cast<eil::real>(i) / static_cast<eil::real>(cnt));
        AA e;
        e.append(r * eil::cos(t));
        e.append(r * eil::sin(t));
        b.append(e);
    }
    return b;
}

void grpPave() {
    paveOne("sq1", squareB(1, 1, 1));
    paveOne("odd", aa({{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}}));
    { AAA empty; paveOne("empty", empty); }
    paveOne("single", aa({{0.0, 0.0}}));
    paveOne("cw", aa({{0.0, 0.0}, {0.0, 1.0}, {1.0, 1.0}, {1.0, 0.0}}));
    paveOne("bowtie", aa({{0.0, 0.0}, {1.0, 1.0}, {1.0, 0.0}, {0.0, 1.0}}));
    paveOne("rect8", squareB(2, 1, 1));
    paveOne("rect16", squareB(4, 1, 1));
    paveOne("wide", squareB(2, 3, 0.6));
    paveOne("lshape", lshapeB(3));
    paveOne("circle6", circleB(6, 1));
    paveOne("wide16", squareB(4, 3, 0.6));
    paveOne("pwedge", wedgeB(3));
    paveOne("rect32", squareB(8, 1, 1));
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }
    int saved = ::dup(1);
    std::fflush(stdout);
    if (std::freopen(kCapture, "w", stdout) == nullptr) {
        std::fprintf(stderr, "cannot open capture file %s\n", kCapture);
        return 2;
    }

    grpCls();
    grpSec2();
    grpSec3();
    grpMake();
    grpNode();
    grpRow();
    grpSmk();
    grpSeam2();
    grpPave();
    T("END", "done");

    std::fflush(stdout);
    ::dup2(saved, 1);
    ::close(saved);

    std::ifstream in(argv[1]);
    if (!in) {
        std::fprintf(stderr, "cannot open baseline %s\n", argv[1]);
        return 2;
    }
    std::vector<std::string> expected;
    for (std::string l; std::getline(in, l);) expected.push_back(l);
    std::ifstream got(kCapture);
    std::vector<std::string> actual;
    for (std::string l; std::getline(got, l);) actual.push_back(l);

    int diffs = 0, shown = 0;
    size_t matched = 0;
    for (size_t i = 0; i < expected.size() || i < actual.size(); ++i) {
        const bool have_e = i < expected.size();
        const bool have_g = i < actual.size();
        const std::string e = have_e ? expected[i] : "<none>";
        const std::string g = have_g ? actual[i] : "<none>";
        if (have_e && have_g && e == g) { ++matched; continue; }
        if (shown < 12) {
            std::printf("line %zu\n  D:   |%s|\n  C++: |%s|\n", i + 1, e.c_str(), g.c_str());
            ++shown;
        }
        ++diffs;
    }
    std::printf("%s: geom_grid_paver, probe replay %zu D lines, %zu C++ lines, %d differ; "
                "%zu of %zu rows byte-identical\n",
                diffs == 0 ? "PASS" : "FAIL", expected.size(), actual.size(), diffs, matched,
                expected.size());
    return diffs == 0 ? 0 : 1;
}
