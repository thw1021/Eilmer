// tests/test_geom_grid_paver_stage.cpp
//
// Replay of probes/geom/grid/probe_paver_stage.d against the port of
// geom/grid/paver.d: the paver_demo.d boundary driven through the PavedGrid
// constructor's statement sequence, with a fingerprint of POINT_LIST after every
// stage that can move a node. Backlog T-011 / T-931.
//
// The corpus is baselines/e587b235/probes/geom_grid_paver_stage.txt, captured from
// ldc2 1.40.0 at gdtk e587b235. Like tests/test_geom_grid_paver.cpp, this file
// replays the constructor body rather than calling PavedGrid's, because the point of
// the gate is where in the sequence a node first moves away from the reference; the
// library's own console lines are therefore printed here exactly where paver.d prints
// them, and the interleaved text is compared too (it pins which guard branches ran and
// how many paving iterations ran, not just the numbers).
//
// Run: ./test_geom_grid_paver_stage <baseline.txt>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <fstream>
#include <memory>
#include <string>
#include <unistd.h>
#include <vector>

#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/math.hpp"
#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/arc.hpp"
#include "geom/gpath/line.hpp"
#include "geom/gpath/path.hpp"
#include "geom/grid/paver.hpp"

namespace {

using eil::geom::Arc3;
using eil::geom::Line;
using eil::geom::PathPtr;
using eil::geom::Point;
using eil::geom::PointPtr;
using eil::geom::UFace;
using eil::geom::Cell;
using eil::geom::Vector3;
using AA = eil::Array<double>;
using AAA = eil::Array<eil::Array<double>>;

const char* kCapture = "/tmp/eil_paver_stage_capture.txt";

unsigned long long bits(double x) {
    unsigned long long u;
    std::memcpy(&u, &x, sizeof u);
    return u;
}

std::string hexp(double x) {
    char buf[20];
    std::snprintf(buf, sizeof buf, "%016llx", bits(x));
    return buf;
}

std::string hexu(unsigned long long u) {
    char buf[20];
    std::snprintf(buf, sizeof buf, "%016llx", u);
    return buf;
}

void F(const std::string& tag, double v) { eil::writefln("%s|%.17g|%s", tag, v, hexp(v)); }
void I(const std::string& tag, long v) { eil::writefln("%s|%d", tag, v); }
void T(const std::string& tag, const std::string& v) { eil::writefln("%s|%s", tag, v); }

// One node, field by field — the same set probes/geom/grid/probe_paver_stage.d prints.
// Coordinates alone would have called the T-931 divergence stage clean: the node that
// started it had identical x and y and an interior_angle one bit off, and the previous
// x/y-only fingerprint reported the step as byte-identical.
void dumpNode(const std::string& tag, const PointPtr& p) {
    eil::writefln("%s|ID=%d|x=%.17g/%s|y=%.17g/%s|ang=%.17g/%s|cat=%s|VR=%.17g/%s,%.17g/%s"
                  "|VL=%.17g/%s,%.17g/%s|growth=%.17g/%s|fsf=%d|rowID=%d",
                  tag, p->ID, p->x, hexp(p->x), p->y, hexp(p->y), p->interior_angle,
                  hexp(p->interior_angle), p->node_cat, p->V_R[0], hexp(p->V_R[0]), p->V_R[1],
                  hexp(p->V_R[1]), p->V_L[0], hexp(p->V_L[0]), p->V_L[1], hexp(p->V_L[1]),
                  p->growth, hexp(p->growth), static_cast<int>(p->first_seam_flag), p->rowID);
}

// Fingerprints of the module-global list after a stage: wrapping sums, in list order, of
// the bit patterns of every numeric field the paving carries, plus the node_cat length
// sum and the rowID sum. The list lengths are in the row too — how many nodes a step
// produced is itself behaviour.
void stage(const std::string& tag, bool dump_points) {
    unsigned long long sx = 0, sy = 0, sa = 0, s0 = 0, s1 = 0, s2 = 0, s3 = 0, sg = 0;
    unsigned long long sc = 0, sr = 0;
    for (size_t t = 0; t < eil::geom::POINT_LIST.length(); ++t) {
        const PointPtr& p = eil::geom::POINT_LIST[t];
        sx += bits(p->x);
        sy += bits(p->y);
        sa += bits(p->interior_angle);
        s0 += bits(p->V_R[0]);
        s1 += bits(p->V_R[1]);
        s2 += bits(p->V_L[0]);
        s3 += bits(p->V_L[1]);
        sg += bits(p->growth);
        sc += p->node_cat.size();
        sr += p->rowID;
    }
    eil::writefln("st.%s|pts=%d|faces=%d|cells=%d|sx=%s|sy=%s|sa=%s|sVR=%s,%s|sVL=%s,%s|"
                  "sg=%s|sc=%d|sr=%d", tag, eil::geom::POINT_LIST.length(),
                  eil::geom::FACE_LIST.length(), eil::geom::CELL_LIST.length(), hexu(sx),
                  hexu(sy), hexu(sa), hexu(s0), hexu(s1), hexu(s2), hexu(s3), hexu(sg),
                  static_cast<long long>(sc), static_cast<long long>(sr));
    if (dump_points) {
        for (size_t t = 0; t < eil::geom::POINT_LIST.length(); ++t) {
            dumpNode("pt." + tag + "." + eil::to_string(t), eil::geom::POINT_LIST[t]);
        }
    }
}

// paver_demo.d:12-34
eil::Array<Vector3> construct_boundary(const eil::Array<PathPtr>& edges,
                                       const eil::Array<size_t>& n) {
    eil_assert(edges.length() == n.length());
    eil::Array<Vector3> boundary;
    size_t i = 0;
    eil::Array<double> t_list;
    double t;
    double t_incr;
    for (size_t e = 0; e < edges.length(); ++e) {
        const PathPtr& edge = edges[e];
        t = 0;
        t_incr = 1.0 / (n[i] - 1);
        t_list = eil::Array<double>();
        for (size_t j = 0; j < static_cast<size_t>(n[i] - 1); ++j) {
            t_list.append(t);
            t += t_incr;
        }
        for (size_t v = 0; v < t_list.length(); ++v) {
            boundary.append((*edge)(t_list[v]));
        }
        ++i;
    }
    return boundary;
}

void run() {
    Vector3 P0 = Vector3(-0.5, 0);
    Vector3 P1 = Vector3(0, 0);
    Vector3 P2 = Vector3(0.5, 1.0);
    Vector3 P3 = Vector3(3, 3);
    Vector3 P4 = Vector3(3, 5.5);
    Vector3 P5 = Vector3(0, 1.5);
    Vector3 M0 = Vector3(0.108, 0.54);
    Vector3 M1 = Vector3(-0.3, 1.04);
    auto P0P1 = std::make_shared<Line>(P0, P1);
    size_t n0 = 10;
    auto P1P2 = std::make_shared<Arc3>(P1, M0, P2);
    size_t n1 = 30;
    auto P2P3 = std::make_shared<Line>(P2, P3);
    size_t n2 = 50;
    auto P3P4 = std::make_shared<Line>(P3, P4);
    size_t n3 = 30;
    auto P4P5 = std::make_shared<Line>(P4, P5);
    size_t n4 = 70;
    auto P5P0 = std::make_shared<Arc3>(P5, M1, P0);
    size_t n5 = 30;
    eil::Array<Vector3> boundary_vectors =
        construct_boundary({P0P1, P1P2, P2P3, P3P4, P4P5, P5P0}, {n0, n1, n2, n3, n4, n5});

    // usgrid.d:516-519
    AAA boundary_points;
    for (size_t t = 0; t < boundary_vectors.length(); ++t) {
        const Vector3& p = boundary_vectors[t];
        AA row;
        row.append(p.x);
        row.append(p.y);
        row.append(p.z);
        boundary_points.append(row);
    }
    for (size_t t = 0; t < boundary_points.length(); ++t) {
        const AA& b = boundary_points[t];
        eil::writefln("bp.%d|%.17g/%s|%.17g/%s|%.17g/%s", t, b[0], hexp(b[0]), b[1],
                      hexp(b[1]), b[2], hexp(b[2]));
    }

    //------------- paver.d:1607-1730, inlined, with a stage per mutation -------------
    eil::writeln("importing external boundary...");                       // D:1611
    for (size_t t = 0; t < boundary_points.length(); ++t) {
        const AA& p = boundary_points[t];
        eil::geom::make2DPoint(p[0], p[1]);
    }
    for (size_t t = 0; t < eil::geom::POINT_LIST.length(); ++t) {
        PointPtr point = eil::geom::POINT_LIST[t];
        point->store_rowID(point->ID);
    }
    stage("0imported", true);

    eil::Array<PointPtr> external_boundary = eil::geom::POINT_LIST;        // D:1619
    size_t npoints_boundary = external_boundary.length();
    eil::writefln("there are %s points in the external boundary", npoints_boundary);

    eil::geom::assign_node_properties(external_boundary.all());            // D:1625
    stage("1nodeprops", false);
    eil::geom::adjust_row_ends(external_boundary.all(), eil::geom::initial_size, 1);
    stage("2rowends", true);

    for (size_t t = 0; t < external_boundary.length(); ++t) {              // D:1628-1630
        PointPtr point = external_boundary[t];
        eil::geom::makeFace(point->ID, point->pointR->ID);
    }
    stage("3faces", false);

    if (eil::geom::clockwise_check(external_boundary.all()) > 0) {          // D:1636
        throw eil::ErrorBase("imported external boundary must be anti-clockwise");
    } else {
        eil::writeln("boundary is anti-clockwise and closed - good");       // D:1640
    }
    stage("4cw", false);

    if (static_cast<int>(
            eil::geom::intersecting_boundary(external_boundary.all()).length()) > 0) {
        throw eil::ErrorBase("imported boundary intersects itself - not allowed");
    } else {
        eil::writeln("boundary does not self-intersect - good");            // D:1644
    }
    stage("5selfint", false);

    eil::geom::initial_size = eil::geom::average_face_length(external_boundary.all());
    eil::geom::avr_area = eil::pow(eil::geom::initial_size, 2.0);
    F("sz.initial_size", eil::geom::initial_size);
    F("sz.avr_area", eil::geom::avr_area);

    eil::writeln("generating first new row...");                            // D:1654
    eil::Array<PointPtr> New_Row = eil::geom::generate_new_row(external_boundary.all());
    size_t npoints_row = New_Row.length();
    eil::writefln("there are %s points in the first new paving row", npoints_row);
    stage("6row1", true);
    eil::geom::assign_node_properties(New_Row.all());                       // D:1658
    stage("7row1props", false);
    eil::Array<PointPtr> Prev_Row = New_Row;
    F("mv.8row1smk", eil::geom::smoothing(New_Row.all(), Prev_Row.all(), 1, 0));
    stage("8row1smk", false);
    eil::geom::even_out_size(New_Row.all());                                // D:1661
    stage("9row1even", false);

    eil::Array<std::array<PointPtr, 4>> intersections =
        eil::geom::intersecting_boundary(New_Row.all());                    // D:1664
    I("ix.firstrow", intersections.length());
    bool effective_intersection = 0;
    for (size_t t = 0; t < New_Row.length(); ++t) {
        PointPtr p = New_Row[t];
        if (p->interior_angle > 6.25 || p->interior_angle < 0.03) {
            effective_intersection = 1;
        }
    }
    I("ix.effective", static_cast<long>(effective_intersection));
    if (intersections.length() != 0 || effective_intersection == 1) {        // D:1669
        eil::writeln("first new row intersected itself ... ");               // D:1671
        eil::geom::smoothing(New_Row.all(), Prev_Row.all(), 1, 1);
        eil::geom::smoothing(New_Row.all(), Prev_Row.all(), 1, 0);
    }
    eil::geom::assign_node_properties(New_Row.all());                        // D:1675
    stage("10row1pre", false);
    eil::geom::adjust_row_ends(New_Row.all(), eil::geom::initial_size, 0);   // D:1676
    stage("11row1adj", false);

    bool seam_next = 1;                                                      // D:1682
    int loop_counter = eil::d_init<int>();
    int iteration = 0;
    while (true) {
        ++loop_counter;                                                      // D:1685
        eil::writeln("paving new rows until intersection...");               // D:1686
        eil::geom::pave_rows_until_intersection(New_Row, Prev_Row, npoints_boundary,
                                                seam_next);
        stage("12loop" + eil::to_string(iteration) + ".pave", false);
        I("lp" + eil::to_string(iteration) + ".seamnext",
          static_cast<long>(seam_next));
        if (seam_next == 0) {
            break;
        }
        eil::geom::smoothing(New_Row.all(), Prev_Row.all(), 1, 0);           // D:1691
        stage("12loop" + eil::to_string(iteration) + ".smk", false);
        eil::writeln("commencing seaming procedure...");                     // D:1692
        eil::geom::seaming_procedure(New_Row);                               // D:1693
        stage("12loop" + eil::to_string(iteration) + ".seam", false);
        Prev_Row = New_Row;                                                  // D:1694
        eil::writeln("...completed seam.");                                  // D:1695
        eil::geom::even_out_size(New_Row.all());                             // D:1696
        stage("12loop" + eil::to_string(iteration) + ".even", false);
        ++iteration;
    }
    I("loop.count", loop_counter);

    eil::writeln("smoothing before closing");                                // D:1701
    eil::Array<PointPtr> inside_nodes = eil::geom::POINT_LIST.from(npoints_boundary);
    double biggest_move =
        eil::geom::smoothing(inside_nodes.all(), Prev_Row.all(), 0, 1);       // D:1703
    F("mv.13preclose.first", biggest_move);
    stage("13preclose1", false);
    double move = biggest_move;
    for (int i = 0; i < 1; ++i) {                                             // D:1705-1707
        move = eil::geom::smoothing(inside_nodes.all(), Prev_Row.all(), 0, 0);
        F("mv.13preclose.second", move);
    }
    stage("13preclose", true);

    eil::writeln("commencing closing seam...");                               // D:1710
    eil::geom::closing_seam(New_Row.all());                                   // D:1711
    stage("14closing", false);

    eil::writeln("completing final smoothing...");                            // D:1713
    inside_nodes = eil::geom::POINT_LIST.from(npoints_boundary);               // D:1714
    biggest_move =
        eil::geom::smoothing(inside_nodes.all(), Prev_Row.all(), 0, 1);        // D:1715
    F("mv.15final.first", biggest_move);
    stage("15final1", false);
    move = biggest_move;
    for (int i = 0; i < 2; ++i) {                                             // D:1717-1719
        move = eil::geom::smoothing(inside_nodes.all(), Prev_Row.all(), 0, 0);
        F("mv.15final.loop" + eil::to_string(i), move);
    }
    eil::writeln("DONE.");                                                    // D:1720
    eil::write("total cell count: ");                                          // D:1721
    eil::writeln(eil::geom::CELL_LIST.length());
    stage("15final", true);

    eil::writefln("fields|dims=%d|nv=%d|nf=%d|nc=%d|nb=%d", eil::geom::ndimensions,
                  eil::geom::POINT_LIST.length(), eil::geom::FACE_LIST.length(),
                  eil::geom::CELL_LIST.length(), npoints_boundary);
    T("END", "done");
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

    run();

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
    size_t first_diff = 0;
    for (size_t i = 0; i < expected.size() || i < actual.size(); ++i) {
        const bool have_e = i < expected.size();
        const bool have_g = i < actual.size();
        const std::string e = have_e ? expected[i] : "<none>";
        const std::string g = have_g ? actual[i] : "<none>";
        if (have_e && have_g && e == g) { ++matched; continue; }
        if (!have_e || !have_g || diffs == 0) first_diff = i + 1;
        if (shown < 12) {
            std::printf("line %zu\n  D:   |%s|\n  C++: |%s|\n", i + 1, e.c_str(), g.c_str());
            ++shown;
        }
        ++diffs;
    }
    std::printf("%s: geom_grid_paver_stage, probe replay %zu D lines, %zu C++ lines, "
                "%d differ; %zu of %zu rows byte-identical; first difference at line %zu\n",
                diffs == 0 ? "PASS" : "FAIL", expected.size(), actual.size(), diffs, matched,
                expected.size(), first_diff);
    return diffs == 0 ? 0 : 1;
}
