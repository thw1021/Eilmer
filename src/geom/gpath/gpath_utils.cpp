// geom/gpath/gpath_utils.d -> src/geom/gpath/gpath_utils.cpp (module geom.gpath.gpath_utils).
//
// Definitions in D source order: discretize_path (D:19-28), readPointsFromFile
// (D:30-43), the file form of optimiseBezierPoints (D:45-53), its point form
// (D:55-174) and optimiseBezierPoints2 (D:176-315).
//
// Every loop bound, index expression and accumulation order is D's, including the two
// shapes that look wrong and are not fixed here (the optimiseBezierPoints2 design
// vector with its two unused entries, and the single shared `t` handed to
// closestDistance) — see the header notes and
// baselines/e587b235/probes/geom_gpath_gpath_utils.txt, whose "counted.*" rows carry
// the simplex step counts of both spellings.
#include "geom/gpath/gpath_utils.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

#include "eil/array.hpp"
#include "eil/conv.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/bezier.hpp"
#include "geom/gpath/line.hpp"
#include "geom/gpath/path.hpp"
#include "geom/misc/univariatefunctions.hpp"
#include "nm/nelmin.hpp"

namespace eil {
namespace geom {

// D:19-28.
eil::Array<Vector3> discretize_path(const PathPtr& pth, size_t niv,
                                    const ClusterPtr& clusterf) {
    // First, set up clustered parameter values.
    eil::Array<double> r = clusterf->distribute_parameter_values(niv);
    // Now, accumulate points, one at a time.
    eil::Array<Vector3> points;
    for (size_t i = 0; i < niv; ++i) { // D: foreach (i; 0 .. niv)
        points.append((*pth)(r[i]));
    }
    return points;
} // end discretize_path()

// D:30-43.
void readPointsFromFile(const std::string& fileName, eil::Array<Vector3>& points) {
    // D's File(fileName,"r") + byLine; the shared gpath reader lives in
    // geom/gpath/path.cpp (detail::read_file_lines), and detail::tokenize_data_line
    // is D's line.strip().split() with the two `continue` guards folded in.
    for (const std::string& line : detail::read_file_lines(fileName)) {
        const std::vector<std::string> tokens = detail::tokenize_data_line(line);
        if (tokens.empty()) continue; // D:35, :36 the blank-line and "#" skips
        number x = eil::to_double(tokens[0]);
        number y = (tokens.size() > 1) ? eil::to_double(tokens[1]) : 0.0;
        number z = (tokens.size() > 2) ? eil::to_double(tokens[2]) : 0.0;
        points.append(Vector3(x, y, z));
    }
    // D:42 f.close(); read_file_lines closes when it returns.
} // end readPointsFromFile()

// D:45-53. The file form: read in the points and call the point form, whose tol,
// max_steps and the nelmin arguments beyond dim it leaves at their defaults.
BezierPtr optimiseBezierPoints(const std::string& fileName, int nCtrlPts,
                               const BezierPtr& initGuess, eil::Array<double>& ts,
                               bool& success, int dim, FitCounters* counters) {
    // Read in points.
    eil::Array<Vector3> points;
    readPointsFromFile(fileName, points);
    // call bezier optimiser
    BezierPtr myBez = optimiseBezierPoints(points.all(), nCtrlPts, initGuess, ts, success,
                                          1.0e-06, 10000, dim, counters);
    return myBez;
} // end optimiseBezierPoints()

namespace {

// D:120-140. The cost function of the point form, as a capturing functor (brief 4b
// item 5) over the same state D's nested function closes over.
struct FunMin1 {
    eil::Slice<Vector3> points;
    BezierPtr myBez;
    int nCtrlPts;
    int dim;
    double* t; // D:58 the enclosing function's single t, shared with every call

    number operator()(eil::Slice<number> d) const {
        // Adjust Bezier points based on supplied design values 'd'
        for (int i = 1; i < nCtrlPts - 1; ++i) { // D: foreach (i; 1 .. nCtrlPts-1)
            if (dim == 2) {
                myBez->B[static_cast<size_t>(i)].x = d[static_cast<size_t>(2 * i - 2)];
                myBez->B[static_cast<size_t>(i)].y = d[static_cast<size_t>(2 * i - 1)];
            } else {
                myBez->B[static_cast<size_t>(i)].x = d[static_cast<size_t>(3 * i - 3)];
                myBez->B[static_cast<size_t>(i)].y = d[static_cast<size_t>(3 * i - 2)];
                myBez->B[static_cast<size_t>(i)].z = d[static_cast<size_t>(3 * i - 1)];
            }
        }
        // Evaluate error between data points and Bezier curve
        number err = 0.0;
        for (size_t k = 0; k < points.length(); ++k) { // D: foreach (p; points)
            err += myBez->closestDistance(points[k], *t);
        }
        return err;
    }
};

// D:250-275. The cost function of the slope-constrained form.
struct FunMin2 {
    eil::Slice<Vector3> points;
    BezierPtr myBez;
    int nCtrlPts;
    int dim;
    Vector3 startSlope;
    Vector3 endSlope;
    double* t;

    number operator()(eil::Slice<number> d) const {
        // Adjust Bezier points based on supplied design values 'd'
        for (int i = 2; i < nCtrlPts - 2; ++i) { // D: foreach (i; 2 .. nCtrlPts-2)
            if (dim == 2) {
                myBez->B[static_cast<size_t>(i)].x = d[static_cast<size_t>(2 * i - 2)];
                myBez->B[static_cast<size_t>(i)].y = d[static_cast<size_t>(2 * i - 1)];
            } else {
                myBez->B[static_cast<size_t>(i)].x = d[static_cast<size_t>(3 * i - 3)];
                myBez->B[static_cast<size_t>(i)].y = d[static_cast<size_t>(3 * i - 2)];
                myBez->B[static_cast<size_t>(i)].z = d[static_cast<size_t>(3 * i - 1)];
            }
        }
        // Then set y and z coordinates of first points in from ends based on slope.
        const size_t last = static_cast<size_t>(nCtrlPts - 1);
        const size_t inboard = static_cast<size_t>(nCtrlPts - 2);
        auto scale0 = (d[d.length() - 2] - myBez->B[0].x) / startSlope.x; // D:265 d[$-2]
        myBez->B[1] = myBez->B[0] + scale0 * startSlope;
        auto scale1 = (myBez->B[last].x - d[d.length() - 1]) / endSlope.x; // D:267 d[$-1]
        myBez->B[inboard] = myBez->B[last] - scale1 * endSlope;
        // Evaluate error between data points and Bezier curve
        number err = 0.0;
        for (size_t k = 0; k < points.length(); ++k) { // D: foreach (p; points)
            err += myBez->closestDistance(points[k], *t);
        }
        return err;
    }
};

} // namespace

// D:55-174.
BezierPtr optimiseBezierPoints(const eil::Slice<Vector3>& points, int nCtrlPts,
                               const BezierPtr& initGuess, eil::Array<double>& ts,
                               bool& success, double tol, int max_steps, int dim,
                               FitCounters* counters) {
    double t = eil::d_init<double>(); // D:58 (a D local starts at double.init)
    // Check that there are more data points than
    // desired control points
    // D:61 compares size_t against int, so D converts nCtrlPts; the cast is written
    // out (a negative nCtrlPts becomes huge and passes, as it does in D).
    if (points.length() < static_cast<size_t>(nCtrlPts)) {
        std::string errMsg =
            "Error in optimiseBezierPoints: there are fewer data points than the desired number of control points.\n";
        errMsg += eil::format("No. of data points: %d\n", points.length());
        errMsg += eil::format("No. of desired control points: %d", nCtrlPts);
        throw eil::ExceptionBase(errMsg, __FILE__, __LINE__); // D:65
    }
    // D:67 grows the caller's array; the new slots are double.init (NaN) in D, and
    // every one of them is assigned below before any read.
    ts.set_length(points.length());
    // ------------------------------------------------------------------
    // Establish an initial guess for the location of the control points.
    // ------------------------------------------------------------------

    BezierPtr myBez;
    if (initGuess != nullptr) { // D:73 `if (initGuess)` tests the handle for null
        if (initGuess->B.length() != static_cast<size_t>(nCtrlPts)) {
            std::string errMsg =
                "Error in optimiseBezierPoints: the supplied initial guess Bezier does not have the same number of controls points\n";
            errMsg +=
                "as the desired number of control points for the optimised Bezier.\n";
            errMsg += eil::format("No. of control points desired: %d\n", nCtrlPts);
            errMsg += eil::format("No. of control points in supplied initial guess: %d",
                                  initGuess->B.length());
            throw eil::ExceptionBase(errMsg, __FILE__, __LINE__); // D:79
        }
        myBez = std::make_shared<Bezier>(*initGuess); // D:81 new Bezier(initGuess)
    } else {
        // Distribute Bezier control points at roughly equal
        // spacing using the given data points.
        eil::Array<Vector3> bezPts;
        // Add starting point as first control point.
        bezPts.append(points[0]);
        // Distribute points in line joining between start and end points
        auto line = std::make_shared<Line>(points[0], points[points.length() - 1]);
        double dt = 1.0 / (nCtrlPts - 1);
        for (int i = 1; i < nCtrlPts - 1; ++i) { // D:92 foreach (i; 1 .. nCtrlPts-1)
            bezPts.append((*line)(i * dt));
        }
        // Add end point as final contrl point.
        bezPts.append(points[points.length() - 1]);
        myBez = std::make_shared<Bezier>(bezPts);
    }
    eil::Array<number> d;
    // D:99-114. The array starts life at double.init in D (the length assignment), and
    // the loops below cover every entry of this form; the fill keeps the initial state
    // D's rather than C++'s zero in case a future spelling leaves one out.
    if (dim == 2)
        d.set_length(2 * (nCtrlPts - 2));
    else
        d.set_length(3 * (nCtrlPts - 2));
    d.fill(eil::d_init<number>());
    for (int i = 1; i < nCtrlPts - 1; ++i) { // D:104 foreach (i; 1 .. nCtrlPts-1)
        if (dim == 2) {
            d[static_cast<size_t>(2 * i - 2)] = myBez->B[static_cast<size_t>(i)].x;
            d[static_cast<size_t>(2 * i - 1)] = myBez->B[static_cast<size_t>(i)].y;
        } else {
            d[static_cast<size_t>(3 * i - 3)] = myBez->B[static_cast<size_t>(i)].x;
            d[static_cast<size_t>(3 * i - 2)] = myBez->B[static_cast<size_t>(i)].y;
            d[static_cast<size_t>(3 * i - 1)] = myBez->B[static_cast<size_t>(i)].z;
        }
    }
    // --------------- Done establishing guess ---------------------------

    // --------------------------------------------------------------------
    // Build cost function to be minimised.
    // --------------------------------------------------------------------
    FunMin1 funMin{points, myBez, nCtrlPts, dim, &t};
    // ------------- Done building cost function -----------------------

    // -----------------------------------------------------------------
    // Optimise placement of control points using Nelder-Mead minimiser
    // -----------------------------------------------------------------
    number f_min = eil::d_init<number>(); // D:146 `number f_min;`
    int n_fe = 0, n_restart = 0;          // D:147 int.init is 0
    eil::Array<number> dx;
    dx.set_length(d.length());
    // Make the initial perturbations 1/100th of the arc length.
    number dp = myBez->length() / 100.0; // D:151
    dx.fill(dp);                         // D:152 dx[] = dp;
    // D:153 `nm.nelmin.minimize!(funMin,number)(...)`; the ported nelmin lives in the
    // eil::nm::nelmin namespace and takes the callable as its first argument.
    success = eil::nm::nelmin::minimize<FunMin1, number>(funMin, d, f_min, n_fe, n_restart,
                                                dx.all(), tol, 1, max_steps);
    if (counters != nullptr) {
        counters->dp = dp;
        counters->f_min = f_min;
        counters->n_fe = n_fe;
        counters->n_restart = n_restart;
        counters->d = d;
    }
    for (int i = 1; i < nCtrlPts - 1; ++i) { // D:154 foreach (i; 1 .. nCtrlPts-1)
        if (dim == 2) {
            myBez->B[static_cast<size_t>(i)].x = d[static_cast<size_t>(2 * i - 2)];
            myBez->B[static_cast<size_t>(i)].y = d[static_cast<size_t>(2 * i - 1)];
        } else {
            myBez->B[static_cast<size_t>(i)].x = d[static_cast<size_t>(3 * i - 3)];
            myBez->B[static_cast<size_t>(i)].y = d[static_cast<size_t>(3 * i - 2)];
            myBez->B[static_cast<size_t>(i)].z = d[static_cast<size_t>(3 * i - 1)];
        }
    }
    // Populate ts vector with all the t-values associated with the data points.
    ts[0] = 0.0;
    for (size_t i = 1; i < points.length() - 1; ++i) { // D:167 foreach (i; 1 .. points.length-1)
        myBez->closestDistance(points[i], t);
        ts[i] = t;
    }
    ts[ts.length() - 1] = 1.0; // D:171 ts[$-1]
    return myBez;
    // ------------- Done calling optimiser -----------------------
} // end optimiseBezierPoints()

// D:176-315.
BezierPtr optimiseBezierPoints2(const eil::Slice<Vector3>& points, int nCtrlPts,
                                const BezierPtr& initGuess, const Vector3& startSlope,
                                const Vector3& endSlope, eil::Array<double>& ts,
                                bool& success, double tol, int max_steps, int dim,
                                FitCounters* counters) {
    double t = eil::d_init<double>(); // D:180
    // Check that there are more data points than
    // desired control points
    if (points.length() < static_cast<size_t>(nCtrlPts)) {
        std::string errMsg =
            "Error in optimiseBezierPoints: there are fewer data points than the desired number of control points.\n";
        errMsg += eil::format("No. of data points: %d\n", points.length());
        errMsg += eil::format("No. of desired control points: %d", nCtrlPts);
        // D:187. The message names optimiseBezierPoints, not ...2 — a copy-paste in D
        // (also at D:200) that is part of the observable text.
        throw eil::ExceptionBase(errMsg, __FILE__, __LINE__);
    }
    ts.set_length(points.length());
    // ------------------------------------------------------------------
    // Establish an initial guess for the location of the control points.
    // ------------------------------------------------------------------

    BezierPtr myBez;
    if (initGuess != nullptr) { // D:195
        if (initGuess->B.length() != static_cast<size_t>(nCtrlPts)) {
            std::string errMsg =
                "Error in optimiseBezierPoints: the supplied initial guess Bezier does not have the same number of controls points\n";
            errMsg +=
                "as the desired number of control points for the optimised Bezier.\n";
            errMsg += eil::format("No. of control points desired: %d\n", nCtrlPts);
            errMsg += eil::format("No. of control points in supplied initial guess: %d",
                                  initGuess->B.length());
            throw eil::ExceptionBase(errMsg, __FILE__, __LINE__); // D:201
        }
        myBez = std::make_shared<Bezier>(*initGuess); // D:203 new Bezier(initGuess)
    } else {
        // Distribute Bezier control points at roughly equal
        // spacing using the given data points.
        eil::Array<Vector3> bezPts;
        // Add starting point as first control point.
        bezPts.append(points[0]);
        // Distribute points in line joining between start and end points
        auto line = std::make_shared<Line>(points[0], points[points.length() - 1]);
        double dt = 1.0 / (nCtrlPts - 1);
        for (int i = 1; i < nCtrlPts - 1; ++i) { // D:214 foreach (i; 1 .. nCtrlPts-1)
            bezPts.append((*line)(i * dt));
        }
        // Add end point as final contrl point.
        bezPts.append(points[points.length() - 1]);
        // Fix first interior points
        const size_t last = static_cast<size_t>(nCtrlPts - 1);
        const size_t inboard = static_cast<size_t>(nCtrlPts - 2);
        auto scale0 = (bezPts[1].x - bezPts[0].x) / startSlope.x;
        bezPts[1] = bezPts[0] + scale0 * startSlope;
        auto scale1 = (bezPts[last].x - bezPts[inboard].x) / endSlope.x;
        bezPts[inboard] = bezPts[last] - scale1 * endSlope;

        myBez = std::make_shared<Bezier>(bezPts);
    }
    eil::Array<number> d;
    // D:227-231. Two entries narrower than the stride-2 loop below writes: d[0] and
    // d[1] are left at the array's initial state (double.init, which is why the fill
    // is NaN here), and the last two entries are overwritten after the loop by the
    // slope constraints. Preserved exactly; the wider simplex changes the evaluation
    // count.
    if (dim == 2)
        d.set_length(2 * (nCtrlPts - 2) - 2);
    else
        d.set_length(3 * (nCtrlPts - 2) - 4);
    d.fill(eil::d_init<number>());
    for (int i = 2; i < nCtrlPts - 2; ++i) { // D:232 foreach (i; 2 .. nCtrlPts-2)
        if (dim == 2) {
            d[static_cast<size_t>(2 * i - 2)] = myBez->B[static_cast<size_t>(i)].x;
            d[static_cast<size_t>(2 * i - 1)] = myBez->B[static_cast<size_t>(i)].y;
        } else {
            d[static_cast<size_t>(3 * i - 3)] = myBez->B[static_cast<size_t>(i)].x;
            d[static_cast<size_t>(3 * i - 2)] = myBez->B[static_cast<size_t>(i)].y;
            d[static_cast<size_t>(3 * i - 1)] = myBez->B[static_cast<size_t>(i)].z;
        }
    }
    const size_t last = static_cast<size_t>(nCtrlPts - 1);
    const size_t inboard = static_cast<size_t>(nCtrlPts - 2);
    d[d.length() - 2] = myBez->B[1].x;         // D:243 d[$-2]
    d[d.length() - 1] = myBez->B[inboard].x;   // D:244 d[$-1]
    // --------------- Done establishing guess ---------------------------

    // --------------------------------------------------------------------
    // Build cost function to be minimised.
    // --------------------------------------------------------------------
    FunMin2 funMin{points, myBez, nCtrlPts, dim, startSlope, endSlope, &t};
    // ------------- Done building cost function -----------------------

    // -----------------------------------------------------------------
    // Optimise placement of control points using Nelder-Mead minimiser
    // -----------------------------------------------------------------
    number f_min = eil::d_init<number>(); // D:281
    int n_fe = 0, n_restart = 0;          // D:282
    eil::Array<number> dx;
    dx.set_length(d.length());
    // Make the initial perturbations 1/100th of the arc length.
    number dp = myBez->length() / 100.0; // D:286
    dx.fill(dp);                         // D:287 dx[] = dp;
    success = eil::nm::nelmin::minimize<FunMin2, number>(funMin, d, f_min, n_fe, n_restart,
                                                dx.all(), tol, 1, max_steps);
    if (counters != nullptr) {
        counters->dp = dp;
        counters->f_min = f_min;
        counters->n_fe = n_fe;
        counters->n_restart = n_restart;
        counters->d = d;
    }
    for (int i = 2; i < nCtrlPts - 2; ++i) { // D:289 foreach (i; 2 .. nCtrlPts-2)
        if (dim == 2) {
            myBez->B[static_cast<size_t>(i)].x = d[static_cast<size_t>(2 * i - 2)];
            myBez->B[static_cast<size_t>(i)].y = d[static_cast<size_t>(2 * i - 1)];
        } else {
            myBez->B[static_cast<size_t>(i)].x = d[static_cast<size_t>(3 * i - 3)];
            myBez->B[static_cast<size_t>(i)].y = d[static_cast<size_t>(3 * i - 2)];
            myBez->B[static_cast<size_t>(i)].z = d[static_cast<size_t>(3 * i - 1)];
        }
    }
    // Then set y and z coordinates of first points in from ends based on slope.
    auto scale0 = (d[d.length() - 2] - myBez->B[0].x) / startSlope.x; // D:301
    myBez->B[1] = myBez->B[0] + scale0 * startSlope;
    auto scale1 = (myBez->B[last].x - d[d.length() - 1]) / endSlope.x; // D:303
    myBez->B[inboard] = myBez->B[last] - scale1 * endSlope;
    // Evaluate error between data points and Bezier curve
    // Populate ts vector with all the t-values associated with the data points.
    ts[0] = 0.0;
    for (size_t i = 1; i < points.length() - 1; ++i) { // D:308 foreach (i; 1 .. points.length-1)
        myBez->closestDistance(points[i], t);
        ts[i] = t;
    }
    ts[ts.length() - 1] = 1.0; // D:312 ts[$-1]
    return myBez;
    // ------------- Done calling optimiser -----------------------
} // end optimiseBezierPoints2()

} // namespace geom
} // namespace eil
