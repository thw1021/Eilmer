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
//   pave.*   the whole PavedGrid constructor on eight boundaries plus the five guard
//            boundaries, and the complete POINT_LIST / FACE_LIST / CELL_LIST.
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
