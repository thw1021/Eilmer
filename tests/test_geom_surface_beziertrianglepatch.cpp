// tests/test_geom_surface_beziertrianglepatch.cpp — C++ mirror of
// probes/geom/surface/probe_beziertrianglepatch.d, replayed line for line against
// baselines/e587b235/probes/geom_surface_beziertrianglepatch.txt (268 D rows), plus
// the D `unittest` block (D:667-723) ported assertion by assertion.
//
// Two sections, in the order the brief asks for:
//   (A) D's six unittest assertions, each message naming its D line.
//   (B) the probe replay, group by group:
//     btp.tsi.*     toSingleIndex over the n=1,2,3,5 lattice and its size_t wrap.
//     btp.max.*     std.algorithm.comparison.max, whose NaN/-0 behaviour feeds dp.
//     btp.size.*/btp.ctor.*  the (n+1)(n+2)/2 count and the D:67-70 Error text.
//     btp.opCall.*  de Casteljau on the Farin net, symmetric/asymmetric/collapsed/
//                   NaN nets, and the D:104 `if (w < 0.0) w = 0.0` clamp at u+v > 1.
//     btp.stale.*   opCall reads _b[0], so B without updateWorkingSpace() is ignored.
//     btp.project.* projectPoint's 11x11 scan + penalty + nelmin result, including
//                   the D:198 throw (measured n_fe = 10003, restarts = 0, f_min nan).
//     btp.file.*    the four writers as whole file bodies with <newline> marks.
//     btp.fpc.*/btp.fpcb.*  the two point-cloud fitters and their nelmin paths.
//
// Regenerate the corpus (ldc2 1.40.0, shadow copy of the read-only gdtk tree):
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/surface
//     probes/geom/surface/probe_beziertrianglepatch.d -of=/tmp/probe_btp
//     -od/tmp/probe_obj_btp; then /tmp/probe_btp is redirected into
//     baselines/e587b235/probes/geom_surface_beziertrianglepatch.txt
//
// Run: ./test_geom_surface_beziertrianglepatch <baseline.txt>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/bezier.hpp"
#include "geom/surface/beziertrianglepatch.hpp"
#include "nm/number.hpp"

namespace {

using eil::geom::Bezier;
using eil::geom::BezierTrianglePatch;
using eil::geom::Vector3;
using BtpPtr = std::shared_ptr<BezierTrianglePatch>;

std::vector<std::string> g_lines;
int g_asserts_run = 0;
int g_asserts_passed = 0;
std::vector<std::string> g_assert_failures;

void emit(const std::string& s) { g_lines.push_back(s); }

// A ported D `assert(...)`: it must be reported with its D line, and a failure must
// not stop the replay (the summary line is the number the report quotes).
void d_assert(bool cond, const char* what, int d_line) {
    ++g_asserts_run;
    if (cond) {
        ++g_asserts_passed;
        return;
    }
    char buf[256];
    std::snprintf(buf, sizeof buf, "ASSERTION FAILED (beziertrianglepatch.d:%d): %s", d_line, what);
    g_assert_failures.push_back(buf);
}

// The probe's helpers, one for one.

// D's join(split(s,"\n"), "<newline>"), on a string (an exception message) and on a
// file's whole body (the four writers).
std::string esc(const std::string& text) {
    std::string out;
    size_t pos = 0;
    while (true) {
        size_t found = text.find('\n', pos);
        if (found == std::string::npos) {
            out += text.substr(pos);
            break;
        }
        out += text.substr(pos, found - pos) + "<newline>";
        pos = found + 1;
    }
    return out;
}

std::string body(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return esc(text);
}

size_t file_len(const std::string& path) {
    std::ifstream in(path, std::ios::ate | std::ios::binary);
    return static_cast<size_t>(in.tellg());
}

void dump_v3(const std::string& tag, const Vector3& v) {
    emit(eil::format("%s x=%.17g y=%.17g z=%.17g", tag, eil::re(v.x), eil::re(v.y),
                     eil::re(v.z)));
}

void dump_uv(const std::string& tag, BezierTrianglePatch& btp, double u, double v) {
    auto p = btp(u, v);
    emit(eil::format("%s u=%.17g v=%.17g x=%.17g y=%.17g z=%.17g", tag, u, v, eil::re(p.x),
                     eil::re(p.y), eil::re(p.z)));
}

eil::Array<Vector3> net(int n, int kind) {
    eil::Array<Vector3> B;
    B.set_length(static_cast<size_t>((n + 1) * (n + 2) / 2));
    if (n == 0) {
        B[0] = (kind == 1) ? Vector3(1.25, -2.5, 3.75) : Vector3(2.0, 3.0, 4.0);
        return B;
    }
    for (int i : eil::iota(n, -1, -1)) {
        for (int j : eil::iota(n - i, -1, -1)) {
            auto k = n - (i + j);
            auto idx = eil::geom::toSingleIndex(static_cast<size_t>(i), static_cast<size_t>(j),
                                                static_cast<size_t>(k));
            if (kind == 0) { // symmetric: barycentric in three distinct corners
                double a = static_cast<double>(i) / n;
                double b = static_cast<double>(j) / n;
                double c = static_cast<double>(k) / n;
                B[idx] = Vector3(2.0 * a, 0.5 * b, -1.25 * c);
            } else if (kind == 1) { // asymmetric in i, j and k
                B[idx] = Vector3(1.7 * i - 0.3 * j + 0.11 * k, 0.25 * i + 1.1 * j - 0.7 * k,
                        -0.5 * i + 0.3 * j + 2.0 * k);
            } else if (kind == 2) { // fully collapsed net
                B[idx] = Vector3(1.5, -2.5, 0.125);
            } else { // one NaN component
                B[idx] = Vector3(0.5 * i, eil::d_init<double>(), 0.25 * k);
            }
        }
    }
    return B;
}

eil::Array<Vector3> pad(int m) {
    eil::Array<Vector3> B;
    B.set_length(static_cast<size_t>(m));
    for (int i = 0; i < m; ++i) B[static_cast<size_t>(i)] = Vector3(0.5 * i, 1.25 * i, -0.75 * i);
    return B;
}

void dump_net(const std::string& tag, const BtpPtr& btp) {
    for (size_t i = 0; i < btp->B.length(); ++i) {
        emit(eil::format("%s B[%d] x=%.17g y=%.17g z=%.17g", tag, i, eil::re(btp->B[i].x),
                         eil::re(btp->B[i].y), eil::re(btp->B[i].z)));
    }
}

BtpPtr farin() {
    eil::Array<Vector3> B = {Vector3(6, 0, 9), Vector3(3, 3, 6), Vector3(3, 0, 0),
        Vector3(0, 6, 0), Vector3(0, 3, 0), Vector3(0, 0, 0)};
    return std::make_shared<BezierTrianglePatch>(B.all(), 2);
}

// D's `catch (Throwable t)` with the class name the probe prints. ErrorBase and
// ExceptionBase are siblings (wave-02-brief lesson 1), so both clauses are needed.
template <typename F> void catch_as_d(F&& f, const std::string& tag_prefix) {
    try {
        f();
    }
    catch (const eil::ErrorBase& t) {
        emit(tag_prefix + " type=object.Error msg=" + esc(t.msg()));
    }
    catch (const eil::ExceptionBase& t) {
        emit(tag_prefix + " type=object.Exception msg=" + esc(t.msg()));
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }

    // ==================================================================
    // (B) the probe, in the probe's order
    // ==================================================================

    // --- 1. the lattice index helper (D:51-56) -------------------------
    for (int n : {1, 2, 3, 5}) {
        for (int i : eil::iota(n, -1, -1)) {
            for (int j : eil::iota(n - i, -1, -1)) {
                auto k = n - (i + j);
                auto T = [](int a, int b, int c) {
                    return eil::geom::toSingleIndex(static_cast<size_t>(a), static_cast<size_t>(b),
                                                    static_cast<size_t>(c));
                };
                emit(eil::format("btp.tsi n=%d i=%d j=%d k=%d idx=%d u=%d v=%d w=%d", n, i, j, k,
                                 T(i, j, k), T(i + 1, j, k), T(i, j + 1, k), T(i, j, k + 1)));
            }
        }
    }
    {
        auto T = [](long long a, long long b, long long c) {
            return eil::geom::toSingleIndex(static_cast<size_t>(a), static_cast<size_t>(b),
                                            static_cast<size_t>(c));
        };
        emit(eil::format("btp.tsi.neg idx=%d u=%d v=%d w=%d", T(-1, 0, 1), T(-1, -1, 0),
                         T(0, -1, 2), T(-2, 1, 3)));
    }

    // --- 1b. std.algorithm.comparison.max semantics (see the .cpp note) --
    {
        auto mx2 = [](double a, double b) { return (a < b) ? b : a; };
        auto mx3 = [&](double a, double b, double c) { return mx2(mx2(a, b), c); };
        const double nan = eil::d_init<double>();
        const double qnan = std::numeric_limits<double>::quiet_NaN();
        emit(eil::format("btp.max nanform=%.17g", nan));
        emit(eil::format("btp.max max(1,2)=%.17g", mx2(1.0, 2.0)));
        emit(eil::format("btp.max max(nan,1)=%.17g", mx2(qnan, 1.0)));
        emit(eil::format("btp.max max(1,nan)=%.17g", mx2(1.0, qnan)));
        emit(eil::format("btp.max max3(1,nan,2)=%.17g", mx3(1.0, qnan, 2.0)));
        emit(eil::format("btp.max max3(1,2,nan)=%.17g", mx3(1.0, 2.0, qnan)));
        emit(eil::format("btp.max max3(nan,1,2)=%.17g", mx3(qnan, 1.0, 2.0)));
        emit(eil::format("btp.max max(-0.0,0.0)=%.17g", mx2(-0.0, 0.0)));
        emit(eil::format("btp.max max(0.0,-0.0)=%.17g", mx2(0.0, -0.0)));
        emit(eil::format("btp.max max3(2,2,2)=%.17g", mx3(2.0, 2.0, 2.0)));
    }

    // --- 2. degree / lattice sizes and the count guard -----------------
    for (int n : {0, 1, 2, 3, 5}) {
        auto nn = net(n, 0);
        BezierTrianglePatch sized(nn.all(), n);
        emit(eil::format("btp.size n=%d nB=%d opCall(0.25,0.25)", sized.n, sized.B.length()));
        dump_uv("btp.size.eval", sized, 0.25, 0.25);
    }
    for (int n : {1, 2, 3}) {
        for (int supplied : {0, 2, 3, 4, 6, 10}) {
            auto p = pad(supplied);
            catch_as_d([&] {
                BezierTrianglePatch bad(p.all(), n);
                emit(eil::format("btp.ctor.noerror n=%d supplied=%d nB=%d", n, supplied,
                                 bad.B.length()));
            },
                       eil::format("btp.ctor.err n=%d supplied=%d", n, supplied));
        }
    }

    // --- 3. opCall -----------------------------------------------------
    auto btp = farin();
    const std::vector<std::vector<double>> uvlist = {{0.0, 0.0}, {1.0, 0.0}, {0.0, 1.0},
        {1.0 / 3, 1.0 / 3}, {0.2, 0.3}, {0.5, 0.5}, {0.25, 0.75}, {1.1, 0.0}, {0.0, 1.1},
        {0.6, 0.6}, {-0.1, -0.1}, {-0.1, 0.2}, {0.9999999999999999, 0.0}, {0.0, 0.0},
        {2.0, -1.0}, {std::numeric_limits<double>::quiet_NaN(), 0.5}};
    for (const auto& uv : uvlist) dump_uv("btp.opCall.farin", *btp, uv[0], uv[1]);
    dump_uv("btp.opCall.farin.nanv", *btp, 0.5, std::numeric_limits<double>::quiet_NaN());

    for (int n : {1, 2, 3, 5}) {
        auto sn = net(n, 0);
        auto an = net(n, 1);
        BezierTrianglePatch s(sn.all(), n);
        BezierTrianglePatch a(an.all(), n);
        const std::vector<std::vector<double>> uvs = {{0.0, 0.0}, {1.0 / 3, 1.0 / 3},
            {0.2, 0.3}, {0.7, 0.3}};
        for (const auto& uv : uvs) {
            dump_uv("btp.opCall.sym", s, uv[0], uv[1]);
            dump_uv("btp.opCall.asym", a, uv[0], uv[1]);
        }
        dump_uv("btp.opCall.sym.corner0", s, 1.0, 0.0);
        dump_uv("btp.opCall.asym.corner0", a, 1.0, 0.0);
        emit("btp.opCall.asym str=" + a(0.2, 0.3).toString());
    }
    auto cn = net(3, 2);
    auto nn2 = net(2, 3);
    BezierTrianglePatch coll(cn.all(), 3);
    BezierTrianglePatch nanp(nn2.all(), 2);
    for (const auto& uv : std::vector<std::vector<double>>{
             {0.0, 0.0}, {1.0 / 3, 1.0 / 3}, {1.1, 0.0}, {0.25, 0.25}}) {
        dump_uv("btp.opCall.collapsed", coll, uv[0], uv[1]);
    }
    for (const auto& uv : std::vector<std::vector<double>>{{0.0, 0.0}, {1.0 / 3, 1.0 / 3}, {1.1, 0.0}}) {
        dump_uv("btp.opCall.nanet", nanp, uv[0], uv[1]);
    }
    eil::Array<Vector3> half = {Vector3(6, 0, 9), Vector3(3, 3, 6), Vector3(3, 0, 0),
        Vector3(0, 0, 0), Vector3(0, 3, 0), Vector3(0, 0, 0)};
    BezierTrianglePatch edge(half.all(), 2);
    for (const auto& uv : std::vector<std::vector<double>>{
             {0.0, 0.0}, {0.5, 0.0}, {0.0, 0.5}, {1.0 / 3, 1.0 / 3}}) {
        dump_uv("btp.opCall.halvedge", edge, uv[0], uv[1]);
    }

    // --- 4. the working space is a snapshot ----------------------------
    {
        auto st = farin();
        dump_uv("btp.stale.before", *st, 0.2, 0.3);
        st->B[0].x = 7.0;
        st->B[3].y = -1.0;
        dump_uv("btp.stale.afterMutation", *st, 0.2, 0.3);
        st->updateWorkingSpace();
        dump_uv("btp.stale.afterUpdate", *st, 0.2, 0.3);
        dump_uv("btp.stale.recheck", *st, 0.2, 0.3);
    }

    // --- 5. projectPoint -----------------------------------------------
    {
        Vector3 q;
        double uu = 0.0, vv = 0.0;
        auto p = (*btp)(1.0 / 3, 1.0 / 3);
        auto dist = btp->projectPoint(p, q, uu, vv);
        emit(eil::format("btp.project.onsurf dist=%.17g u=%.17g v=%.17g", dist, uu, vv));
        dump_v3("btp.project.onsurf.q", q);
    }
    {
        Vector3 q;
        double uu = 0.0, vv = 0.0;
        auto dist = btp->projectPoint(Vector3(2.0, 2.0, 3.0), q, uu, vv);
        emit(eil::format("btp.project.near dist=%.17g u=%.17g v=%.17g", dist, uu, vv));
        dump_v3("btp.project.near.q", q);
    }
    {
        Vector3 q;
        double uu = 0.0, vv = 0.0;
        auto dist = btp->projectPoint(Vector3(-4.0, 9.0, 22.0), q, uu, vv);
        emit(eil::format("btp.project.far dist=%.17g u=%.17g v=%.17g", dist, uu, vv));
        dump_v3("btp.project.far.q", q);
    }
    {
        Vector3 q;
        double uu = 0.0, vv = 0.0;
        auto dist = btp->projectPoint((*btp)(1.0, 0.0), q, uu, vv);
        emit(eil::format("btp.project.corner dist=%.17g u=%.17g v=%.17g", dist, uu, vv));
    }
    {
        Vector3 q;
        double uu = 0.0, vv = 0.0;
        auto dist = coll.projectPoint(Vector3(0.0, 0.0, 0.0), q, uu, vv);
        emit(eil::format("btp.project.collapsed dist=%.17g u=%.17g v=%.17g", dist, uu, vv));
        dump_v3("btp.project.collapsed.q", q);
    }
    {
        Vector3 q;
        double uu = 0.0, vv = 0.0;
        const double qnan = std::numeric_limits<double>::quiet_NaN();
        catch_as_d([&] {
            auto dist = btp->projectPoint(Vector3(qnan, 1.0, 2.0), q, uu, vv);
            emit(eil::format("btp.project.nan.nothrow dist=%.17g", dist));
        },
                   "btp.project.nan.err");
        double uu2 = 0.0, vv2 = 0.0;
        auto d2 = btp->projectPoint(Vector3(qnan, 1.0, 2.0), q, uu2, vv2, true);
        emit(eil::format("btp.project.nan.returned dist=%.17g u=%.17g v=%.17g", d2, uu2, vv2));
        dump_v3("btp.project.nan.q", q);
    }

    // --- 6. the four writers, as whole file bodies ----------------------
    {
        const std::string p1 = "/tmp/probe_btp_ctrl.txt";
        const std::string p2 = "/tmp/probe_btp_cvtk.vtk";
        const std::string p3 = "/tmp/probe_btp_avtk.vtk";
        const std::string p4 = "/tmp/probe_btp.dat";
        eil::geom::writeBezierTriangleCtrlPtsAsText(*btp, p1);
        emit("btp.file.ctrlpts len=" + std::to_string(file_len(p1)));
        emit("btp.file.ctrlpts.body=" + body(p1));
        eil::geom::writeBezierTriangleCtrlPtsAsText(coll, p1);
        emit("btp.file.ctrlpts.collapsed.body=" + body(p1));

        for (int n : {1, 2, 3, 5}) {
            auto bn = net(n, 1);
            BezierTrianglePatch bp(bn.all(), n);
            eil::geom::writeBezierTriangleCtrlPtsAsVtkXml(bp, p2);
            emit(eil::format("btp.file.ctrlvtk n=%d len=%d", n, file_len(p2)));
            emit(eil::format("btp.file.ctrlvtk n=%d.body=%s", n, body(p2)));
        }

        for (int ne : {1, 2, 3, 5}) {
            eil::geom::writeBezierTriangleAsVtkXml(*btp, p3, ne);
            emit(eil::format("btp.file.trivtk ne=%d len=%d", ne, file_len(p3)));
            emit(eil::format("btp.file.trivtk ne=%d.body=%s", ne, body(p3)));
        }
        eil::geom::writeBezierTriangleAsVtkXml(nanp, p3, 3);
        emit("btp.file.trivtk.nanet.body=" + body(p3));

        for (int ne : {1, 3, 5}) {
            eil::geom::writeBezierTriangleAsDat(*btp, p4, ne);
            emit(eil::format("btp.file.dat ne=%d len=%d", ne, file_len(p4)));
            emit(eil::format("btp.file.dat ne=%d.body=%s", ne, body(p4)));
        }
        std::remove(p1.c_str());
        std::remove(p2.c_str());
        std::remove(p3.c_str());
        std::remove(p4.c_str());
    }

    // --- 7. bezierTriangleFromPointCloud, corner form -------------------
    {
        auto src = farin();
        eil::Array<Vector3> points;
        double du = 1.0 / 4;
        double dv = du;
        for (double uu : eil::iota(0.0, 1.0 + 0.5 * du, du)) {       // D:696
            for (double vv : eil::iota(0.0, (1.0 - uu) + 0.5 * dv, dv)) { // D:697
                if ((uu + vv) > 1.0) continue;
                points.append((*src)(uu, vv));
            }
        }
        emit("btp.fpc.cloud n=" + std::to_string(points.length()));
        for (size_t i = 0; i < points.length(); ++i) {
            emit(eil::format("btp.fpc.cloud[%d] x=%.17g y=%.17g z=%.17g", i,
                             eil::re(points[i].x), eil::re(points[i].y), eil::re(points[i].z)));
        }
        Vector3 p0(6, 0, 9);
        Vector3 p1(0, 6, 0);
        Vector3 p2(0, 0, 0);
        eil::Array<Vector3> B = {Vector3(6, 0, 9), Vector3(3.1, 2.9, 6.2),
            Vector3(2.8, 0.15, -0.05), Vector3(0, 6, 0), Vector3(-0.3, 2.85, -0.1),
            Vector3(0, 0, 0)};
        auto guess = std::make_shared<BezierTrianglePatch>(B.all(), 2);
        auto fitA = eil::geom::bezierTriangleFromPointCloud(points.all(), p0, p1, p2, 2, guess);
        emit(eil::format("btp.fpc.guess n=%d nB=%d", fitA->n, fitA->B.length()));
        dump_net("btp.fpc.guess", fitA);
        dump_uv("btp.fpc.guess.eval", *fitA, 0.2, 0.3);
        dump_uv("btp.fpc.guess.eval0", *fitA, 0.0, 0.0);
        dump_uv("btp.fpc.guess.eval1", *fitA, 1.0, 0.0);
        dump_uv("btp.fpc.guess.eval2", *fitA, 0.0, 1.0);

        auto fitB = eil::geom::bezierTriangleFromPointCloud(points.all(), p0, p1, p2, 2, nullptr);
        emit(eil::format("btp.fpc.noguess n=%d nB=%d", fitB->n, fitB->B.length()));
        dump_net("btp.fpc.noguess", fitB);
        dump_uv("btp.fpc.noguess.eval", *fitB, 0.2, 0.3);

        auto wn = net(3, 1);
        auto wrong = std::make_shared<BezierTrianglePatch>(wn.all(), 3);
        auto fitC = eil::geom::bezierTriangleFromPointCloud(points.all(), p0, p1, p2, 2, wrong);
        dump_net("btp.fpc.wrongsizenet", fitC);

        eil::Array<Vector3> none;
        auto fitD = eil::geom::bezierTriangleFromPointCloud(none.all(), p0, p1, p2, 2, guess);
        dump_net("btp.fpc.emptynet", fitD);

        catch_as_d([&] {
            auto fitE = eil::geom::bezierTriangleFromPointCloud(points.all(), p0, p1, p2, 1, nullptr);
            dump_net("btp.fpc.deg1.nothrow", fitE);
        },
                   "btp.fpc.deg1.err");
    }

    // --- 8. bezierTriangleFromPointCloud, Bezier-edge form --------------
    {
        Vector3 ca(0, 0, 0);
        Vector3 cb(3, 0, 1);
        Vector3 cc(0, 3, 0);
        eil::Array<Vector3> v0 = {cb, Vector3(2.0, 1.0, 0.0), Vector3(0.5, 2.0, 0.5), cc};
        eil::Array<Vector3> v1 = {ca, Vector3(1.0, 0.0, 0.4), Vector3(2.0, 0.0, 0.1), cc};
        eil::Array<Vector3> v2 = {ca, Vector3(1.0, 1.0, -0.2), Vector3(0.4, 2.0, 0.3), cb};
        Bezier b0(v0);
        Bezier b1(v1);
        Bezier b2(v2);

        eil::Array<Vector3> points;
        auto src = farin();
        for (double uu : eil::iota(0.0, 1.0 + 0.25, 0.25)) {
            for (double vv : eil::iota(0.0, (1.0 - uu) + 0.125, 0.25)) {
                if ((uu + vv) > 1.0) continue;
                points.append((*src)(uu, vv));
            }
        }
        bool ok = false;
        auto fit = eil::geom::bezierTriangleFromPointCloud(points.all(), b0, b1, b2, 3, nullptr, ok);
        emit(eil::format("btp.fpcb.n3 ok=%s nB=%d", ok ? "true" : "false", fit->B.length()));
        dump_net("btp.fpcb.n3", fit);
        dump_uv("btp.fpcb.n3.eval", *fit, 0.2, 0.3);

        auto gn = net(3, 1);
        auto g = std::make_shared<BezierTrianglePatch>(gn.all(), 3);
        bool ok2 = false;
        auto fit2 = eil::geom::bezierTriangleFromPointCloud(points.all(), b0, b1, b2, 3, g, ok2);
        emit(eil::format("btp.fpcb.n3guess ok=%s", ok2 ? "true" : "false"));
        dump_net("btp.fpcb.n3guess", fit2);

        bool ok3 = false;
        catch_as_d([&] {
            eil::Array<Vector3> w0 = {cb, Vector3(0.2, 1.5, 0.0), cc};
            eil::Array<Vector3> w1 = {ca, Vector3(1.5, 0.0, 0.2), cc};
            eil::Array<Vector3> w2 = {ca, Vector3(1.2, 1.2, 0.1), cb};
            Bezier b0d2(w0);
            Bezier b1d2(w1);
            Bezier b2d2(w2);
            auto fitE =
                eil::geom::bezierTriangleFromPointCloud(points.all(), b0d2, b1d2, b2d2, 2, nullptr, ok3);
            emit(eil::format("btp.fpcb.n2.nothrow ok=%s", ok3 ? "true" : "false"));
            dump_net("btp.fpcb.n2", fitE);
        },
                   "btp.fpcb.n2.err");

        eil::Array<Vector3> s0 = {Vector3(0.2, 0.0, 0.0), Vector3(2.0, 1.0, 0.0),
            Vector3(0.5, 2.0, 0.5), cc};
        eil::Array<Vector3> s1 = {ca, Vector3(1.0, 0.0, 0.4), Vector3(2.0, 0.0, 0.1),
            Vector3(3.5, 0.0, 1.0)};
        eil::Array<Vector3> s2 = {Vector3(0.0, 0.25, 0.0), Vector3(1.0, 1.0, -0.2),
            Vector3(0.4, 2.0, 0.3), cb};
        Bezier b0shift(s0);
        Bezier b1shift(s1);
        Bezier b2shift(s2);
        bool ok4 = false;
        catch_as_d([&] {
            auto x = eil::geom::bezierTriangleFromPointCloud(points.all(), b0shift, b1, b2, 3,
                                                             nullptr, ok4);
            emit("btp.fpcb.mismatch.b0b2.nothrow n=" + std::to_string(x->n));
        },
                   "btp.fpcb.mismatch.b0b2");
        catch_as_d([&] {
            auto x = eil::geom::bezierTriangleFromPointCloud(points.all(), b0, b1shift, b2, 3,
                                                             nullptr, ok4);
            emit("btp.fpcb.mismatch.b0b1.nothrow n=" + std::to_string(x->n));
        },
                   "btp.fpcb.mismatch.b0b1");
        catch_as_d([&] {
            auto x = eil::geom::bezierTriangleFromPointCloud(points.all(), b0, b1, b2shift, 3,
                                                             nullptr, ok4);
            emit("btp.fpcb.mismatch.b1b2.nothrow n=" + std::to_string(x->n));
        },
                   "btp.fpcb.mismatch.b1b2");
    }

    // ==================================================================
    // (A) the D `unittest` block, D:667-723, assertion by assertion
    // ==================================================================
    {
        // Example on 17.1 on p. 312 in Farin (2001)  — D:671-676
        eil::Array<Vector3> B = {Vector3(6, 0, 9), Vector3(3, 3, 6), Vector3(3, 0, 0),
            Vector3(0, 6, 0), Vector3(0, 3, 0), Vector3(0, 0, 0)};
        BezierTrianglePatch utp(B.all(), 2);
        double u = 1. / 3;
        double v = 1. / 3;
        auto p = utp(u, v);
        // Farin gives results as (2, 2, 7/3);  — D:681
        d_assert(eil::geom::approxEqualVectors(p, Vector3(2, 2, 7. / 3)),
                 "approxEqualVectors(p, Vector3(2, 2, 7./3))", 681);

        // Test projection of point that is ON surface  — D:684-689
        Vector3 q;
        auto dist = utp.projectPoint(p, q, u, v);
        d_assert(dist < 1.0e-6, "dist < 1.0e-6", 686);
        d_assert(eil::geom::approxEqualVectors(q, Vector3(2, 2, 7. / 3)),
                 "approxEqualVectors(q, Vector3(2, 2, 7./3))", 687);
        d_assert(eil::isClose(u, 1. / 3, 1.0e-6), "isClose(u, 1./3, 1.0e-6)", 688);
        d_assert(eil::isClose(v, 1. / 3, 1.0e-6), "isClose(v, 1./3, 1.0e-6)", 689);

        // Test optimizer with this surface. 1. Generate a bunch of points — D:692-702
        eil::Array<Vector3> points;
        double du = 1.0 / 4;
        double dv = du;
        for (double uu : eil::iota(0.0, 1.0 + 0.5 * du, du)) {       // D:696
            for (double vv : eil::iota(0.0, (1.0 - uu) + 0.5 * dv, dv)) { // D:697
                if ((uu + vv) > 1.0) continue;
                points.append(utp(uu, vv));
            }
        }
        // 2. Find Bezier triangle to fit this point cloud. — D:704-718
        auto p0 = Vector3(6, 0, 9);
        auto p1 = Vector3(0, 6, 0);
        auto p2 = Vector3(0, 0, 0);
        // Let's perturb the points a little as an initial guess. — D:707-717
        B[1].x = 3.1;
        B[1].y = 2.9;
        B[1].z = 6.2;
        B[2].x = 2.8;
        B[2].y = 0.15;
        B[2].z = -0.05;
        B[4].x = -0.3;
        B[4].y = 2.85;
        B[4].z = -0.1;
        auto guess = std::make_shared<BezierTrianglePatch>(B.all(), 2);
        auto testPatch = eil::geom::bezierTriangleFromPointCloud(points.all(), p0, p1, p2, 2, guess);

        p = (*testPatch)(0.2, 0.3);
        q = utp(0.2, 0.3);
        d_assert(eil::geom::distance_between(p, q) < 0.1, "distance_between(p, q) < 0.1", 722);
    }

    // ==================================================================
    // replay
    // ==================================================================
    std::ifstream in(argv[1]);
    if (!in) {
        std::fprintf(stderr, "cannot open baseline %s\n", argv[1]);
        return 2;
    }
    std::vector<std::string> expected;
    for (std::string l; std::getline(in, l);) expected.push_back(l);

    int diffs = 0, shown = 0;
    for (size_t i = 0; i < expected.size() || i < g_lines.size(); ++i) {
        const std::string e = i < expected.size() ? expected[i] : "<none>";
        const std::string g = i < g_lines.size() ? g_lines[i] : "<none>";
        if (e == g) continue;
        if (shown < 12) {
            size_t at = 0;
            while (at < e.size() && at < g.size() && e[at] == g[at]) ++at;
            std::printf("line %zu (first difference at byte %zu)\n  D:   |%s|\n  C++: |%s|\n"
                        "  D[...]:   |%s|\n  C[...]: |%s|\n",
                        i + 1, at, e.substr(0, 120).c_str(), g.substr(0, 120).c_str(),
                        e.substr(at, std::min<size_t>(80, e.size() - at)).c_str(),
                        g.substr(at, std::min<size_t>(80, g.size() - at)).c_str());
            ++shown;
        }
        ++diffs;
    }
    for (const auto& f : g_assert_failures) std::printf("%s\n", f.c_str());
    std::printf("%s: geom_surface_beziertrianglepatch, probe replay %zu D lines, %zu C++ lines, "
                "%d differ; %zu of %zu rows byte-identical. D unittest assertions "
                "%d of %d passed.\n",
                (diffs == 0 && g_assert_failures.empty()) ? "PASS" : "FAIL", expected.size(),
                g_lines.size(), diffs,
                std::min(expected.size(), g_lines.size()) - static_cast<size_t>(diffs),
                std::min(expected.size(), g_lines.size()), g_asserts_passed, g_asserts_run);
    return (diffs == 0 && g_assert_failures.empty()) ? 0 : 1;
}
