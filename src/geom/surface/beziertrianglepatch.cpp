// geom/surface/beziertrianglepatch.d -> src/geom/surface/beziertrianglepatch.cpp
// (module geom.surface.beziertrianglepatch)
//
// The implementation side of the header note. Everything here is line-for-line with
// D: the loop nesting (i outer, j inner, k implied), the descending eil::iota walks,
// the left-to-right u*a + v*b + w*c sums, the `if (w < 0.0) w = 0.0` clamp at D:104,
// and the format strings of the four writers. Nothing was reordered and no tolerance
// was introduced.

#include "geom/surface/beziertrianglepatch.hpp"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/math.hpp"
#include "eil/number.hpp"
#include "eil/range.hpp"
#include "nm/nelmin.hpp"

namespace eil {
namespace geom {
namespace {

// D:51-56 takes size_t parameters and D's call sites pass int loop variables
// (D:114-117, :238-245, :297-304, :386, :406, :429, :562, :582-583, :606, :653).
// D's implicit int -> size_t conversion wraps mod 2^64 for a negative argument,
// measured at ldc2 1.40: toSingleIndex(-1, 0, 1) prints 2, and the probe's
// btp.tsi.neg row pins all four combinations. IDX is that conversion, spelled.
inline std::size_t IDX(int i, int j, int k) {
    return toSingleIndex(static_cast<std::size_t>(i), static_cast<std::size_t>(j),
                         static_cast<std::size_t>(k));
}

// std.algorithm.comparison.max (imported at D:42) is not std::max. For two doubles
// comparison.d:1681 is `a < b ? b : a`, and for three arguments comparison.d:1644
// splits as a = max(args[0..2]), b = args[2], so it is max(max(a,b),c). That matters
// because a NaN position changes the answer: measured (btp.max rows) max(nan,1) = nan,
// max(1,nan) = 1, max3(1,nan,2) = 2, max3(1,2,nan) = 2, max3(nan,1,2) = nan, and
// max(-0.0, 0.0) = -0. D:149 feeds this into maxDist, hence into the penalty at D:154
// and into dp at D:457/D:641, which sets nelmin's initial simplex.
inline number d_max(number a, number b) { return (a < b) ? b : a; }
inline number d_max(number a, number b, number c) { return d_max(d_max(a, b), c); }

// D:219 `auto nCells = n^^2;`, D:273/D:332 `int nCells = (nEdgePts-1)^^2;`.
// These are std.math.pow's integral arm (integerPow): measured typeof(3^^2) == int
// with value 9 and (5-1)^^2 == 16. The value still goes through eil::pow
// (wave-02-brief section 5 forbids expanding ^^ into a product) and is truncated to
// int, which is exact well past the int range D's own nCells can hold.
inline int pow2_as_int(int x) { return static_cast<int>(eil::pow(static_cast<number>(x), 2)); }

// std.stdio.File(fileName, "w") — the Phobos failure text is the same one
// src/geom/surface/parametricsurface.cpp:52-60 reproduces; std.exception
// .ErrnoException has no substrate class yet, so it stays an ExceptionBase.
std::FILE* open_for_write(const std::string& fileName) {
    std::FILE* f = std::fopen(fileName.c_str(), "w");
    if (f == nullptr) {
        std::string msg = "Cannot open file `" + fileName + "' in mode `w' (" +
                          std::strerror(errno) + ")";
        throw eil::ExceptionBase(msg, __FILE__, __LINE__);
    }
    return f;
}

// D's File.writeln(s) / File.writefln(fmt, args) are format-then-newline.
struct FileWriter {
    std::FILE* f;
    void wl(const char* s) const { std::fwrite(s, 1, std::strlen(s), f); std::fputc('\n', f); }
    template <typename... Args> void wfln(const char* fmt, const Args&... args) const {
        const std::string s = eil::format(fmt, args...);
        std::fwrite(s.data(), 1, s.size(), f);
        std::fputc('\n', f);
    }
};

// D:155-171 `number fMin(number[] x)` nested in projectPoint. It closes over the
// target p, the *out-parameter* q (which it overwrites on every feasible call),
// the penalty and `this` (opCall mutates _b, hence the non-const pointer).
struct ProjectPointObjective {
    BezierTrianglePatch* self;
    const Vector3* p;
    Vector3* q;
    number penalty;

    double operator()(eil::Slice<number> x) const {
        double u = eil::re(x[0]);
        double v = eil::re(x[1]);
        // Check that u and v are feasible.
        // Apply penalty if not.
        if (u < -0.1) return penalty;
        if (v < -0.1) return penalty;
        if (u + v > 1.1) return penalty;
        // Now proceed to evaluate distance.
        *q = (*self)(u, v);
        return distance_between(*p, *q); // D:170 to!number(...) is identity here
    }
};

// D:420-446, the corner form's cost function. `d` is the design vector nelmin passes
// in, which D names the same as the enclosing `number[] d` (the shadowing is D's,
// wave-02-brief section 0.1: keep it, do not "fix" it).
struct CornerFitObjective {
    eil::Slice<Vector3> points;
    BezierTrianglePatch* patch;
    int n;
    int* count; // unused in the corner form; D:596's counter belongs to the edges

    double operator()(eil::Slice<number> d) const {
        // Adjust control points based on supplied design values 'd'.
        std::size_t pos = 0;
        for (int i : eil::iota(n, -1, -1)) {         // D:424
            for (int j : eil::iota(n - i, -1, -1)) { // D:425
                auto k = n - (i + j);
                // Skip corner points
                if (i == n || j == n || k == n) continue;
                auto idx = IDX(i, j, k);
                patch->B[idx].x = d[pos]; ++pos;
                patch->B[idx].y = d[pos]; ++pos;
                patch->B[idx].z = d[pos]; ++pos;
            }
        }
        // Remember to update working space after changing control
        // point locations.
        patch->updateWorkingSpace();
        // Evaluate error between point cloud and triangle batch
        number err = 0.0;
        Vector3 q;
        double uFound = eil::d_init<double>();
        double vFound = eil::d_init<double>();
        for (std::size_t a = 0; a < points.length(); ++a) {
            err += patch->projectPoint(points[a], q, uFound, vFound, true);
        }
        return err;
    }
};

// D:597-630, the Bezier-edge form's cost function. Identical but for which points
// are pinned (i==0 || j==0 || k==0 instead of the corners) and the extra `count++`
// at D:624, whose only reader is the commented-out writeln at D:626.
struct EdgeFitObjective {
    eil::Slice<Vector3> points;
    BezierTrianglePatch* patch;
    int n;
    int* count;

    double operator()(eil::Slice<number> d) const {
        // Adjust control points based on supplied design values 'd'.
        std::size_t pos = 0;
        for (int i : eil::iota(n, -1, -1)) {         // D:601
            for (int j : eil::iota(n - i, -1, -1)) { // D:602
                auto k = n - (i + j);
                // Skip edge points
                if (i == 0 || j == 0 || k == 0) continue;
                auto idx = IDX(i, j, k);
                patch->B[idx].x = d[pos]; ++pos;
                patch->B[idx].y = d[pos]; ++pos;
                patch->B[idx].z = d[pos]; ++pos;
            }
        }
        // Remember to update working space after changing control
        // point locations.
        patch->updateWorkingSpace();
        // Evaluate error between point cloud and triangle batch
        number err = 0.0;
        Vector3 q;
        double uFound = eil::d_init<double>();
        double vFound = eil::d_init<double>();
        for (std::size_t a = 0; a < points.length(); ++a) {
            err += patch->projectPoint(points[a], q, uFound, vFound, true);
        }
        ++(*count);
        return err;
    }
};

// D has no helper here: the design-vector write-back loop and the "Optimizer stats:"
// message block are each written out twice in the D source (D:461-475 vs D:647-661,
// and D:193-198 vs D:479-484). Both pairs are reproduced inline below, in each
// function, so this file stays statement-for-statement with D instead of factoring
// the duplication away.
} // namespace

//-----------------------------------------------------------------------
// class BezierTrianglePatch

BezierTrianglePatch::BezierTrianglePatch(eil::Slice<Vector3> _B, int _n) {
    n = _n;
    auto nCtrlPts = (n + 1) * (n + 2) / 2;
    if (static_cast<std::size_t>(nCtrlPts) != _B.length()) {
        // D:68-70 `throw new Error(text(...))` — an Error, not an Exception, so a
        // ported `catch (ExceptionBase)` must not see it. The text() call has no
        // separators: the number runs into the following word, as the measured
        // btp.ctor.err rows show ("...supplied.3 expected, 0 supplied.").
        throw eil::ErrorBase(eil::text("BezierTrianglePatch: incorrect number of control points supplied.",
                                        nCtrlPts, " expected, ", _B.length(), " supplied."));
    }
    B.set_length(static_cast<std::size_t>(nCtrlPts));
    B.all().assign_from(_B);
    initialiseWorkingSpace();
}

void BezierTrianglePatch::initialiseWorkingSpace() {
    _b.set_length(static_cast<std::size_t>(n) + 1);
    for (int i = 0; i < n + 1; ++i) { // D:79 foreach (i; 0 .. n + 1)
        auto n_local = n - i;
        _b[static_cast<std::size_t>(i)].set_length(
            static_cast<std::size_t>((n_local + 1) * (n_local + 2) / 2));
    }
    // Copy B into _b[0].
    // Yes, we are duplicating the storage of those control points,
    // but it makes the implementation of de Casteljau's algorithm
    // much nicer. Typically, the number of control points will
    // be relatively small, so this shouldn't be a big storage penalty.
    _b[0].all().assign_from(B.all());
}

void BezierTrianglePatch::updateWorkingSpace() {
    // After changing control points, we need to reflect this
    // in the working space.
    _b[0].all().assign_from(B.all());
}

Vector3 BezierTrianglePatch::operator()(double u, double v) {
    auto w = 1.0 - u - v;
    // We may allow u or v to get slightly larger than 1.0
    // if we are perturbing to get sensitivity.
    // In which case, we won't allow w to go negative.
    if (w < 0.0) w = 0.0;

    // de Casteljau algorithm as described in:
    // Farin (2001), pp. 310--311
    for (int r = 1; r < n + 1; ++r) { // D:108 foreach (r; 1 .. n + 1)
        // Loop over points via triangle indices.
        auto n_local = n - r;
        for (int i : eil::iota(n_local, -1, -1)) {         // D:111
            for (int j : eil::iota(n_local - i, -1, -1)) { // D:112
                auto k = n_local - (i + j);
                auto idx = IDX(i, j, k);
                auto uIdx = IDX(i + 1, j, k);
                auto vIdx = IDX(i, j + 1, k);
                auto wIdx = IDX(i, j, k + 1);
                // Do Vector3 operation component-wise to
                // avoid allocation of temporaries.
                _b[static_cast<std::size_t>(r)][idx].x =
                    u * _b[static_cast<std::size_t>(r - 1)][uIdx].x +
                    v * _b[static_cast<std::size_t>(r - 1)][vIdx].x +
                    w * _b[static_cast<std::size_t>(r - 1)][wIdx].x;
                _b[static_cast<std::size_t>(r)][idx].y =
                    u * _b[static_cast<std::size_t>(r - 1)][uIdx].y +
                    v * _b[static_cast<std::size_t>(r - 1)][vIdx].y +
                    w * _b[static_cast<std::size_t>(r - 1)][wIdx].y;
                _b[static_cast<std::size_t>(r)][idx].z =
                    u * _b[static_cast<std::size_t>(r - 1)][uIdx].z +
                    v * _b[static_cast<std::size_t>(r - 1)][vIdx].z +
                    w * _b[static_cast<std::size_t>(r - 1)][wIdx].z;
            }
        }
    }
    return _b[static_cast<std::size_t>(n)][0];
}

number BezierTrianglePatch::projectPoint(const Vector3& p, Vector3& q, double& uFound,
                                         double& vFound, bool returnOnFailure) {
    // Establish an initial guess for u and v
    eil::Array<number> samples = {0.0, 0.1, 0.2, 0.3, 0.4, 0.5,
                                  0.6, 0.7, 0.8, 0.9, 1.0};
    q = (*this)(0.0, 0.0);
    number minDist = distance_between(p, q);
    number maxDist = minDist;
    double min_u = 0.0;
    double min_v = 0.0;
    for (std::size_t su = 0; su < samples.length(); ++su) { // D:139
        double u = samples[su];
        for (std::size_t sv = 0; sv < samples.length(); ++sv) { // D:140
            double v = samples[sv];
            if (u + v > 1.0) break;
            q = (*this)(u, v);
            number dist = distance_between(p, q);
            if (dist < minDist) {
                minDist = dist;
                min_u = u;
                min_v = v;
            }
            maxDist = d_max(maxDist, dist);
        }
    }

    // Define function to minimize
    number penalty = 1000 * maxDist;

    // Set up initial guess
    eil::Array<number> x = {min_u, min_v};

    // Call optimizer
    number f_min = eil::d_init<number>();
    int n_fe = eil::d_init<int>();
    int n_restart = eil::d_init<int>();
    // D:179 `number[] dx = [to!number(0.01), to!number(0.01)]`; with number == double
    // to!number is the identity, so the literals are written directly.
    eil::Array<number> dx = {number(0.01), number(0.01)};
    double tol = 1.0e-8;
    int max_steps = 10000;

    ProjectPointObjective fMin{this, &p, &q, penalty};
    bool success = eil::nm::nelmin::minimize<ProjectPointObjective, number>(
        fMin, x, f_min, n_fe, n_restart, dx.all(), tol, 1, max_steps);

    if (success || returnOnFailure) {
        uFound = eil::re(x[0]);
        vFound = eil::re(x[1]);
        q = (*this)(uFound, vFound);
        return distance_between(p, q);
    }

    // Otherwise, we failed in the minimize step.
    std::string errMsg = "Error in BezierTrianglePatch.projectPoint()\n";
    errMsg += "Optimizer stats:\n";
    errMsg += eil::format("  number of function evaluations: %d\n", n_fe);
    errMsg += eil::format("  number of restarts: %d\n", n_restart);
    errMsg += eil::format("  minimum of function found: %12.6e\n", f_min);
    throw eil::ExceptionBase(errMsg, __FILE__, __LINE__);
}

//-----------------------------------------------------------------------
// module-level writers

void writeBezierTriangleCtrlPtsAsText(BezierTrianglePatch& btp, const std::string& fileName) {
    std::FILE* f = open_for_write(fileName);
    for (std::size_t i = 0; i < btp.B.length(); ++i) { // D:209 foreach (ref p; btp.B)
        const Vector3& p = btp.B[i];
        std::string s = eil::format("%20.16e %20.16e %20.16e", eil::re(p.x), eil::re(p.y),
                                    eil::re(p.z));
        std::fwrite(s.data(), 1, s.size(), f);
        std::fputc('\n', f);
    }
    std::fclose(f);
}

void writeBezierTriangleCtrlPtsAsVtkXml(BezierTrianglePatch& btp, const std::string& fileName) {
    auto nPtsTotal = btp.B.length();
    auto n = btp.n;
    auto nCells = pow2_as_int(n); // D:219 `n^^2`, the integer arm of std.math.pow

    FileWriter out{open_for_write(fileName)};
    out.wl("<VTKFile type=\"UnstructuredGrid\" version=\"1.0\" header_type=\"UInt64\">");
    out.wl("  <UnstructuredGrid>");
    out.wfln("    <Piece NumberOfPoints=\"%d\" NumberOfCells=\"%d\">", nPtsTotal, nCells);
    out.wl("       <Points>");
    out.wl("         <DataArray type=\"Float32\" Name=\"Points\" NumberOfComponents=\"3\" "
           "format=\"ascii\">");
    for (std::size_t i = 0; i < btp.B.length(); ++i) { // D:227
        const Vector3& p = btp.B[i];
        out.wfln("       %20.16e %20.16e %20.16e", eil::re(p.x), eil::re(p.y), eil::re(p.z));
    }
    out.wl("        </DataArray>");
    out.wl("      </Points>");
    out.wl("      <Cells>");
    out.wl("        <DataArray type=\"Int32\" Name=\"connectivity\">");
    for (int i : eil::iota(n, 0, -1)) {        // D:234
        for (int j : eil::iota(n - i, -1, -1)) { // D:235
            auto k = n - (i + j);
            // Triangle pattern A
            auto idx0 = IDX(i, j, k);
            auto idx1 = IDX(i - 1, j + 1, k);
            auto idx2 = IDX(i - 1, j, k + 1);
            out.wfln("       %d %d %d", idx0, idx1, idx2);
            // Triangle pattern B
            if (j == 0) continue;
            idx1 = IDX(i - 1, j, k + 1);
            idx2 = IDX(i, j - 1, k + 1);
            out.wfln("       %d %d %d", idx0, idx1, idx2);
        }
    }
    out.wl("         </DataArray>");
    out.wl("         <DataArray type=\"Int32\" Name=\"offsets\">");
    int offset = 0;
    for (int i = 0; i < nCells; ++i) { // D:252 foreach (i; 0 .. nCells)
        offset += 3;
        out.wfln("        %d", offset);
    }
    out.wl("         </DataArray>");
    out.wl("         <DataArray type=\"UInt8\" Name=\"types\">");
    for (int i = 0; i < nCells; ++i) { // D:258
        out.wl("          5");
    }
    out.wl("         </DataArray>");
    out.wl("       </Cells>");
    out.wl("    </Piece>");
    out.wl("  </UnstructuredGrid>");
    out.wl("</VTKFile>");
    std::fclose(out.f);
}

void writeBezierTriangleAsVtkXml(BezierTrianglePatch& btp, const std::string& fileName,
                                 int nEdgePts) {
    int nPtsTotal = nEdgePts * (nEdgePts + 1) / 2;
    int n = nEdgePts - 1;
    int nCells = pow2_as_int(nEdgePts - 1); // D:273

    FileWriter out{open_for_write(fileName)};
    out.wl("<VTKFile type=\"UnstructuredGrid\" version=\"1.0\" header_type=\"UInt64\">");
    out.wl("  <UnstructuredGrid>");
    out.wfln("    <Piece NumberOfPoints=\"%d\" NumberOfCells=\"%d\">", nPtsTotal, nCells);
    out.wl("       <Points>");
    out.wl("         <DataArray type=\"Float32\" Name=\"Points\" NumberOfComponents=\"3\" "
           "format=\"ascii\">");
    for (int i : eil::iota(n, -1, -1)) {        // D:281
        for (int j : eil::iota(n - i, -1, -1)) { // D:282
            double u = static_cast<double>(i) / n;
            double v = static_cast<double>(j) / n;
            auto p = btp(u, v);
            out.wfln("       %20.16e %20.16e %20.16e", eil::re(p.x), eil::re(p.y), eil::re(p.z));
        }
    }
    out.wl("        </DataArray>");
    out.wl("      </Points>");
    out.wl("      <Cells>");
    out.wl("        <DataArray type=\"Int32\" Name=\"connectivity\">");
    for (int i : eil::iota(n, 0, -1)) {        // D:293
        for (int j : eil::iota(n - i, -1, -1)) { // D:294
            auto k = n - (i + j);
            // Triangle pattern A
            auto idx0 = IDX(i, j, k);
            auto idx1 = IDX(i - 1, j + 1, k);
            auto idx2 = IDX(i - 1, j, k + 1);
            out.wfln("       %d %d %d", idx0, idx1, idx2);
            // Triangle pattern B
            if (j == 0) continue;
            idx1 = IDX(i - 1, j, k + 1);
            idx2 = IDX(i, j - 1, k + 1);
            out.wfln("       %d %d %d", idx0, idx1, idx2);
        }
    }
    out.wl("         </DataArray>");
    out.wl("         <DataArray type=\"Int32\" Name=\"offsets\">");
    int offset = 0;
    for (int i = 0; i < nCells; ++i) { // D:311
        offset += 3;
        out.wfln("        %d", offset);
    }
    out.wl("         </DataArray>");
    out.wl("         <DataArray type=\"UInt8\" Name=\"types\">");
    for (int i = 0; i < nCells; ++i) { // D:317
        out.wl("          5");
    }
    out.wl("         </DataArray>");
    out.wl("       </Cells>");
    out.wl("    </Piece>");
    out.wl("  </UnstructuredGrid>");
    out.wl("</VTKFile>");
    std::fclose(out.f);
}

void writeBezierTriangleAsDat(BezierTrianglePatch& btp, const std::string& fileName,
                              int nEdgePts) {
    int nPtsTotal = nEdgePts * (nEdgePts + 1) / 2;
    int n = nEdgePts - 1;
    int nCells = pow2_as_int(nEdgePts - 1); // D:332, computed but never written

    std::FILE* f = open_for_write(fileName);
    std::string s = eil::format("%d", nPtsTotal);
    std::fwrite(s.data(), 1, s.size(), f);
    std::fputc('\n', f);
    for (int i : eil::iota(n, -1, -1)) {        // D:336
        for (int j : eil::iota(n - i, -1, -1)) { // D:337
            double u = static_cast<double>(i) / n;
            double v = static_cast<double>(j) / n;
            auto p = btp(u, v);
            std::string row =
                eil::format("       %20.16e %20.16e %20.16e", eil::re(p.x), eil::re(p.y),
                            eil::re(p.z));
            std::fwrite(row.data(), 1, row.size(), f);
            std::fputc('\n', f);
        }
    }
    std::fclose(f);
    (void)nCells;
}

//-----------------------------------------------------------------------
// the two point-cloud fitters

BezierTrianglePatch::Ptr bezierTriangleFromPointCloud(
    eil::Slice<Vector3> points, const Vector3& p0, const Vector3& p1, const Vector3& p2, int n,
    const BezierTrianglePatch::Ptr& initGuess) {
    // NOTE: This function has no constraint at the edges. It's quite possible to find a patch that fits the point
    //       cloud well, but the patch overlaps at the edges. It's not usually a big deal, just something to be
    //       aware of.
    double tol = 1.0e-3;
    int maxSteps = 10000;
    /* D:355-362, kept as a block comment because the lines end in backslashes.
     * Corners are located at
     *
     *            p1
     *             + b(0,n,0)
     *            / \
     *           /   \
     *       p2 +_____+ p0
     *   b(0,0,n)        b(n,0,0)
     */

    auto nCtrlPts = (n + 1) * (n + 2) / 2;
    eil::Array<Vector3> ctrlPts;
    ctrlPts.set_length(static_cast<std::size_t>(nCtrlPts));
    eil::Array<number> d;

    // -----------------------------------------------------------------
    // Establish an initial guess for the location of control points
    // -----------------------------------------------------------------
    // We know the corner points. Put them in the list.
    // These points are not to be touched by the optimiser.
    ctrlPts[IDX(n, 0, 0)] = p0;
    ctrlPts[IDX(0, n, 0)] = p1;
    ctrlPts[IDX(0, 0, n)] = p2;
    // The user might have supplied an initial guess.
    // We can use this if the degree is correct.
    if (initGuess != nullptr && initGuess->n == n) { // D:380 `initGuess !is null`
        for (int i : eil::iota(n, -1, -1)) {         // D:381
            for (int j : eil::iota(n - i, -1, -1)) { // D:382
                auto k = n - (i + j);
                // Skip corner points
                if (i == n || j == n || k == n) continue;
                auto idx = IDX(i, j, k);
                ctrlPts[idx] = initGuess->B[idx];
                d.append(ctrlPts[idx].x);
                d.append(ctrlPts[idx].y);
                d.append(ctrlPts[idx].z);
            }
        }
    } else {
        // Distribute the remaining control points roughly equally using barycentric coordinates
        double du = 1.0 / n;
        double dv = du;
        for (int i : eil::iota(n, -1, -1)) {         // D:398
            for (int j : eil::iota(n - i, -1, -1)) { // D:399
                auto k = n - (i + j);
                // Skip corner points
                if (i == n || j == n || k == n) continue;
                double u = i * du;
                double v = j * dv;
                double w = 1.0 - u - v;
                auto idx = IDX(i, j, k);
                ctrlPts[idx] = u * p0 + v * p1 + w * p2;
                d.append(ctrlPts[idx].x);
                d.append(ctrlPts[idx].y);
                d.append(ctrlPts[idx].z);
            }
        }
    }
    auto myBezTriPatch = std::make_shared<BezierTrianglePatch>(ctrlPts.all(), n);
    // -------------- Done: establishing starting guess ------------------------

    // --------------------------------------------------------------
    // Build cost function to be minimized.
    // --------------------------------------------------------------
    CornerFitObjective fMin{points, myBezTriPatch.get(), n, nullptr};
    // ----------- Done: building cost function ----------------------------

    // ---------------------------------------------------------------------
    // Optimize the placement of control points using Nelder-Mead minimiser
    // ---------------------------------------------------------------------
    number f_min = eil::d_init<number>();
    int n_fe = eil::d_init<int>();
    int n_restart = eil::d_init<int>();
    eil::Array<number> dx;
    dx.set_length(d.length());
    // Make the initial perturbations of control points 1/100th of the longest side length
    number dp =
        0.01 * d_max(distance_between(p0, p1), distance_between(p0, p2), distance_between(p1, p2));
    dx.fill(dp); // D:458 dx[] = dp;
    bool success = eil::nm::nelmin::minimize<CornerFitObjective, number>(
        fMin, d, f_min, n_fe, n_restart, dx.all(), tol, 1, maxSteps);
    if (success) {
        std::size_t pos = 0;
        for (int i : eil::iota(n, -1, -1)) {         // D:462
            for (int j : eil::iota(n - i, -1, -1)) { // D:463
                auto k = n - (i + j);
                // Skip corner points
                if (i == n || j == n || k == n) continue;
                auto idx = IDX(i, j, k);
                myBezTriPatch->B[idx].x = d[pos]; ++pos;
                myBezTriPatch->B[idx].y = d[pos]; ++pos;
                myBezTriPatch->B[idx].z = d[pos]; ++pos;
            }
        }
        // Remember to update working space after changing control
        // point locations.
        myBezTriPatch->updateWorkingSpace();
        return myBezTriPatch;
    }
    // Otherwise, we failed in the optimizer.
    std::string errMsg = "Error in bezierTriangleFromPointCloud().\n";
    errMsg += "Optimizer stats:\n";
    errMsg += eil::format("  number of function evaluations: %d\n", n_fe);
    errMsg += eil::format("  number of restarts: %d\n", n_restart);
    errMsg += eil::format("  minimum of function found: %12.6e\n", f_min);
    throw eil::ExceptionBase(errMsg, __FILE__, __LINE__);
}

BezierTrianglePatch::Ptr bezierTriangleFromPointCloud(
    eil::Slice<Vector3> points, const Bezier& b0, const Bezier& b1, const Bezier& b2, int n,
    const BezierTrianglePatch::Ptr& initGuess, bool& success) {
    double tol = 0.01;
    int maxSteps = 500;
    /* D:491-508. A block comment because the ASCII art ends its lines in backslashes.
     * Edges and their directions are defined as:
     * (_,_,_) = (u,v,w)
     * b_ = Bezier curve
     *
     *                     + (0,1,0)
     *              _0_   / \   *1
     *               /   /   \   \
     *          b0--/   /     \   \--b2
     *             /   /       \   \
     *            /   /         \   \
     *          1*   /           \  -0-
     *              +_____________+
     *           (0,0,1)         (1,0,0)
     *
     *              1*-----------|0
     *                     |
     *                     b1
     */
    // b0 : defines Bezier triangle curve u=0
    //    : runs from v=1 --> w=1
    //
    // b1 : defines Bezier triangle curve v=0
    //    : runs from u=1 --> w=1
    //
    // b2 : defines Bezier triangle curve w=0
    //    : runs from u=1 --> v=1
    //

    // Perform some checks that Bezier end points are (approximately) coincident
    // (D:520-528, `throw new Error(...)`: three distinct messages, and all three are
    // Errors, not Exceptions — measured classinfo.name is object.Error for each.)
    if (!approxEqualVectors(b0.B[0], b2.B[b2.B.length() - 1])) {
        throw eil::ErrorBase("Error: b0 and b2 end points are not coincident.\n");
    }
    if (!approxEqualVectors(b0.B[b0.B.length() - 1], b1.B[b1.B.length() - 1])) {
        throw eil::ErrorBase("Error: b0 and b1 end points are not coincident.\n");
    }
    if (!approxEqualVectors(b1.B[0], b2.B[0])) {
        throw eil::ErrorBase("Error: b1 and b2 end points are not coincident.\n");
    }

    auto nCtrlPts = (n + 1) * (n + 2) / 2;
    eil::Array<Vector3> ctrlPts;
    ctrlPts.set_length(static_cast<std::size_t>(nCtrlPts));
    eil::Array<number> d;

    // -----------------------------------------------------------------
    // Establish an initial guess for the location of control points
    // -----------------------------------------------------------------
    // We know the edge points. Put them in the list.
    // These points are not to be touched by the optimiser.
    // Work down b0 curve
    for (std::size_t i = 0; i < b0.B.length(); ++i) { // D:541 foreach (i, b; b0.B)
        ctrlPts[ctrlPts.length() - static_cast<std::size_t>(n) - 1 + i] = b0.B[i];
    }
    // Work down b1 curve
    for (std::size_t i = 0; i < b1.B.length(); ++i) { // D:545
        auto idx = (i + 1) * (i + 2) / 2 - 1;
        ctrlPts[idx] = b1.B[i];
    }
    // Work down b2 curve
    for (std::size_t i = 0; i < b2.B.length(); ++i) { // D:550
        auto idx = i * (i + 1) / 2;
        ctrlPts[idx] = b2.B[i];
    }
    // The user might have supplied an initial guess.
    // We can use this if the degree is correct.
    if (initGuess != nullptr && initGuess->n == n) { // D:556
        for (int i : eil::iota(n, -1, -1)) {         // D:557
            for (int j : eil::iota(n - i, -1, -1)) { // D:558
                auto k = n - (i + j);
                // Skip edge points
                if (i == 0 || j == 0 || k == 0) continue;
                auto idx = IDX(i, j, k);
                ctrlPts[idx] = initGuess->B[idx];
                d.append(ctrlPts[idx].x);
                d.append(ctrlPts[idx].y);
                d.append(ctrlPts[idx].z);
            }
        }
    } else {
        // Distribute the remaining control points roughly equally using barycentric coordinates
        double du = 1.0 / n;
        double dv = du;
        for (int i : eil::iota(n, -1, -1)) {         // D:574
            for (int j : eil::iota(n - i, -1, -1)) { // D:575
                auto k = n - (i + j);
                // Skip edge points
                if (i == 0 || j == 0 || k == 0) continue;
                double u = i * du;
                double v = j * dv;
                double w = 1.0 - u - v;
                auto idx = IDX(i, j, k);
                ctrlPts[idx] = u * ctrlPts[0] +
                               v * ctrlPts[ctrlPts.length() - static_cast<std::size_t>(n) - 1] +
                               w * ctrlPts[ctrlPts.length() - 1];
                d.append(ctrlPts[idx].x);
                d.append(ctrlPts[idx].y);
                d.append(ctrlPts[idx].z);
            }
        }
    }
    auto myBezTriPatch = std::make_shared<BezierTrianglePatch>(ctrlPts.all(), n);
    // -------------- Done: establishing starting guess ------------------------

    // --------------------------------------------------------------
    // Build cost function to be minimized.
    // --------------------------------------------------------------
    int count = 0; // D:596; only the commented-out writeln at D:626 reads it
    EdgeFitObjective fMin{points, myBezTriPatch.get(), n, &count};
    // ----------- Done: building cost function ----------------------------

    // ---------------------------------------------------------------------
    // Optimize the placement of control points using Nelder-Mead minimiser
    // ---------------------------------------------------------------------
    number f_min = eil::d_init<number>();
    int n_fe = eil::d_init<int>();
    int n_restart = eil::d_init<int>();
    eil::Array<number> dx;
    dx.set_length(d.length());
    // Make the initial perturbations of control points 1/100th of the longest side length
    number dp = 0.01 * d_max(distance_between(b0.B[0], b0.B[b0.B.length() - 1]),
                             distance_between(b1.B[0], b1.B[b1.B.length() - 1]),
                             distance_between(b2.B[0], b2.B[b2.B.length() - 1]));
    dx.fill(dp); // D:644
    success = eil::nm::nelmin::minimize<EdgeFitObjective, number>(
        fMin, d, f_min, n_fe, n_restart, dx.all(), tol, 1, maxSteps);

    // D:647-661 runs whether or not the optimiser converged.
    std::size_t pos = 0;
    for (int i : eil::iota(n, -1, -1)) {         // D:648
        for (int j : eil::iota(n - i, -1, -1)) { // D:649
            auto k = n - (i + j);
            // Skip edge points
            if (i == 0 || j == 0 || k == 0) continue;
            auto idx = IDX(i, j, k);
            myBezTriPatch->B[idx].x = d[pos]; ++pos;
            myBezTriPatch->B[idx].y = d[pos]; ++pos;
            myBezTriPatch->B[idx].z = d[pos]; ++pos;
        }
    }
    // Remember to update working space after changing control
    // point locations.
    myBezTriPatch->updateWorkingSpace();
    return myBezTriPatch;
}

} // namespace geom
} // namespace eil
