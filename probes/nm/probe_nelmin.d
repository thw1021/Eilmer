// probes/nm/probe_nelmin.d — pin what ldc2's nm/nelmin.d actually does.
//
// Why this probe exists: nelmin drives the grid-generation smoothing in
// geom/grid/sgrid.d (determine_rs_grids, tol=1.0e-3, P=3, max_fe=800), in
// geom/gpath/gpath_utils.d and in geom/surface/beziertrianglepatch.d. Those
// call sites only see the converged point, f_min, n_fe and n_restart, and a
// differing n_fe means the simplex steps (reflect/extend/contract/rescale) took
// a different path. So every case below prints the converged point at %.17g,
// f_min at %.17g, n_fe and n_restart, plus the convergence flag.
//
// Build and capture:
//   ldc2 -O0 -i -I/home/tang/packages/gdtk/src probes/nm/probe_nelmin.d \
//     -of=/tmp/nelmin_d && /tmp/nelmin_d \
//     > baselines/e587b235/probes/nm_nelmin.txt
// Replay check: ctest -R nm_nelmin
//
// The three objective functions are copied verbatim from nelmin.d's unittest
// blocks (lines 335-341, 366-378, 401-416) so the probe exercises the same
// arithmetic the ported test asserts on.
import std.stdio, std.conv, std.math;
import nm.number;
import nm.nelmin;

number testFunction1(number[] x) {
    number sum = 0.0;
    foreach (elem; x)
        sum += (elem - 1.0) * (elem - 1.0);
    return sum;
}

number testFunction2(number[] x) {
    number x1 = x[0];
    number x2 = x[1]; // rename to match the paper
    if ((x1 * x1 + x2 * x2) > 1.0) {
        return to!number(1.0e38);
    } else {
        number yp = 53.69 + 7.26 * x1 - 10.33 * x2 + 7.22 * x1 * x1
            + 6.43 * x2 * x2 + 11.36 * x1 * x2;
        number ys = 82.17 - 1.01 * x1 - 8.61 * x2 + 1.40 * x1 * x1
            - 8.76 * x2 * x2 - 7.20 * x1 * x2;
        return -yp + fabs(ys - 87.8);
    }
}

double testFunction3(double[] z) {
    double[] x = [0.25, 0.50, 1.00, 1.70, 2.00, 4.00];
    double[] y = [0.25, 0.40, 0.60, 0.58, 0.54, 0.27];
    double a1 = z[0];
    double a2 = z[1];
    double alpha1 = z[2];
    double alpha2 = z[3];
    double sum_residuals = 0.0;
    foreach (i; 0 .. 6) {
        double t = x[i];
        double eta = a1 * exp(alpha1 * t) + a2 * exp(alpha2 * t);
        double r = y[i] - eta;
        sum_residuals += r * r;
    }
    return sum_residuals;
}

// Rosenbrock: the reflection/extension branch fires repeatedly here because the
// valley is steep in one direction and flat in the other.
number rosenbrock(number[] x) {
    number sum = 0.0;
    foreach (i; 0 .. x.length - 1) {
        number a = x[i+1] - x[i]*x[i];
        number b = 1.0 - x[i];
        sum += 100.0*a*a + b*b;
    }
    return sum;
}

// |x| sum: a ridge, so test_for_minimum keeps failing and rescale() runs.
number ridged(number[] x) {
    number sum = 0.0;
    foreach (elem; x) sum += fabs(elem);
    return sum;
}

// All vertex values equal: sorts of ties, which is where an unstable sort can
// hand back a different permutation than another unstable sort.
number flat(number[] x) { return 0.0; }

void dump(string tag, bool conv, const number[] x, number fx, int nfe, int nres) {
    writef("%s conv=%s nfe=%d nres=%d fx=%.17g x=[", tag, conv, nfe, nres, fx);
    foreach (i, v; x) writef("%s%.17g", i == 0 ? "" : ",", v);
    writeln("]");
}

void dump3(string tag, bool conv, const double[] x, double fx, int nfe, int nres) {
    writef("%s conv=%s nfe=%d nres=%d fx=%.17g x=[", tag, conv, nfe, nres, fx);
    foreach (i, v; x) writef("%s%.17g", i == 0 ? "" : ",", v);
    writeln("]");
}

void main() {
    number fx;
    int nfe, nres;
    bool conv;

    // nelmin.d:342-346 -- unittest 1. Empty dx, so minimize() substitutes
    // 0.1*ones through the `dx.length != x.length` branch (nelmin.d:88-91).
    number[] x1 = [to!number(0.0), to!number(0.0), to!number(0.0), to!number(0.0)];
    number[] dx1;
    conv = minimize!(testFunction1, number)(x1, fx, nfe, nres, dx1);
    dump("unittest1", conv, x1, fx, nfe, nres);

    // nelmin.d:382-384 -- unittest 2, tol=1.0e-4.
    number[] x2 = [to!number(0.0), to!number(0.0)];
    number[] dx2 = [to!number(0.5), to!number(0.5)];
    conv = minimize!(testFunction2, number)(x2, fx, nfe, nres, dx2, 1.0e-4);
    dump("unittest2", conv, x2, fx, nfe, nres);

    // nelmin.d:419-422 -- unittest 3, tol=1.0e-9, P=2, max_fe=800.
    double[] x3 = [1.0, 1.0, -0.5, -2.5];
    double[] dx3 = [0.1, 0.1, 0.1, 0.1];
    double fx3;
    conv = minimize!(testFunction3, double)(x3, fx3, nfe, nres, dx3, 1.0e-9, 2, 800);
    dump3("unittest3", conv, x3, fx3, nfe, nres);

    // nelmin_demo.d:98 -- the same problem with P=1 (the demo's setting).
    number[] x3d = [to!number(1.0), to!number(1.0), to!number(-0.5), to!number(-2.5)];
    number[] dx3d = [to!number(0.1), to!number(0.1), to!number(0.1), to!number(0.1)];
    conv = minimize!(testFunction3n, number)(x3d, fx, nfe, nres, dx3d, 1.0e-9, 1, 800);
    dump("demo3_P1", conv, x3d, fx, nfe, nres);

    // unittest 2 with the default tol=1.0e-6 (nelmin.d:80 default argument).
    number[] x4 = [to!number(0.0), to!number(0.0)];
    conv = minimize!(testFunction2, number)(x4, fx, nfe, nres, dx2);
    dump("tf2_default_tol", conv, x4, fx, nfe, nres);

    // max_fe cut-off: the while loop at nelmin.d:94 exits on the evaluation
    // budget, so f_min/n_fe come from the last completed check.
    number[] x5 = [to!number(0.0), to!number(0.0), to!number(0.0), to!number(0.0)];
    conv = minimize!(testFunction1, number)(x5, fx, nfe, nres, dx1, 1.0e-6, 1, 200);
    dump("tf1_maxfe200", conv, x5, fx, nfe, nres);

    // n_check = 5 instead of the default 20 (nelmin.d:80).
    number[] x6 = [to!number(0.0), to!number(0.0)];
    conv = minimize!(testFunction2, number)(x6, fx, nfe, nres, dx2, 1.0e-4, 1, 400, 5);
    dump("tf2_ncheck5", conv, x6, fx, nfe, nres);

    // delta = 0.01: the test_for_minimum perturbation and the rescale ratio.
    number[] x7 = [to!number(0.0), to!number(0.0), to!number(0.0), to!number(0.0)];
    conv = minimize!(testFunction1, number)(x7, fx, nfe, nres, dx1, 1.0e-6, 1, 400, 20,
                                            0.01);
    dump("tf1_delta001", conv, x7, fx, nfe, nres);

    // sgrid.d:1411 uses P=3, so the parallel-replacement loop runs 3 times per
    // step and centroid(N-P) is the centroid of the other 13 vertices.
    number[] x8 = [to!number(0.0), to!number(0.0), to!number(0.0), to!number(0.0),
                   to!number(0.0), to!number(0.0)];
    number[] dx8;
    foreach (i; 0 .. 6) dx8 ~= 0.05;
    conv = minimize!(testFunction1, number)(x8, fx, nfe, nres, dx8, 1.0e-3, 3, 800);
    dump("tf1_P3", conv, x8, fx, nfe, nres);

    // Steep reflection: start far out so the reflected point beats the best
    // vertex and the extension branch (nelmin.d:270-284) is taken.
    number[] x9 = [to!number(-3.0), to!number(-3.0)];
    number[] dx9 = [to!number(1.0), to!number(1.0)];
    conv = minimize!(testFunction1, number)(x9, fx, nfe, nres, dx9, 1.0e-8, 1, 400);
    dump("tf1_far_start", conv, x9, fx, nfe, nres);

    // Rosenbrock, 3D.
    number[] x10 = [to!number(-1.2), to!number(1.0), to!number(0.5)];
    number[] dx10 = [to!number(0.5), to!number(0.5), to!number(0.5)];
    conv = minimize!(rosenbrock, number)(x10, fx, nfe, nres, dx10, 1.0e-6, 1, 400);
    dump("rosenbrock3d", conv, x10, fx, nfe, nres);

    // Ridge: repeated failed test_for_minimum => rescale() and nres > 0.
    number[] x11 = [to!number(1.0), to!number(-1.0)];
    number[] dx11 = [to!number(0.5), to!number(0.5)];
    conv = minimize!(ridged, number)(x11, fx, nfe, nres, dx11, 1.0e-6, 1, 400);
    dump("ridged", conv, x11, fx, nfe, nres);

    // Tied vertex values: every fx is 0.0, so all comparisons are ties.
    number[] x12 = [to!number(0.3), to!number(-0.4), to!number(0.0)];
    number[] dx12 = [to!number(0.1), to!number(0.1), to!number(0.1)];
    conv = minimize!(flat, number)(x12, fx, nfe, nres, dx12, 1.0e-6, 1, 120);
    dump("flat_ties", conv, x12, fx, nfe, nres);

    // Kcontract/Kextend/Kreflect other than the defaults (nelmin.d:81).
    number[] x13 = [to!number(-2.0), to!number(1.5), to!number(0.25)];
    number[] dx13 = [to!number(0.4), to!number(0.4), to!number(0.4)];
    conv = minimize!(rosenbrock, number)(x13, fx, nfe, nres, dx13, 1.0e-6, 1, 400, 20,
                                         0.001, 1.0, 2.0, 0.5);
    dump("rosenbrock_defaultK", conv, x13, fx, nfe, nres);

    number[] x14 = [to!number(-2.0), to!number(1.5), to!number(0.25)];
    conv = minimize!(rosenbrock, number)(x14, fx, nfe, nres, dx13, 1.0e-6, 1, 400, 20,
                                         0.001, 0.8, 1.5, 0.4);
    dump("rosenbrock_customK", conv, x14, fx, nfe, nres);

    // dx shorter than x: the substitute-dx branch again, with a 2-element dx
    // against a 3-element x (length mismatch is silently repaired).
    number[] x15 = [to!number(0.5), to!number(0.5), to!number(0.5)];
    number[] dx15 = [to!number(0.2), to!number(0.2)];
    conv = minimize!(testFunction1, number)(x15, fx, nfe, nres, dx15, 1.0e-8);
    dump("tf1_short_dx", conv, x15, fx, nfe, nres);
}

// nelmin.d:401 declares testFunction3 over double; the demo (nelmin_demo.d:50)
// uses a number[] version of the same body. Kept separate so the probe covers
// both spellings.
number testFunction3n(number[] z) {
    double[] x = [0.25, 0.50, 1.00, 1.70, 2.00, 4.00];
    double[] y = [0.25, 0.40, 0.60, 0.58, 0.54, 0.27];
    number a1 = z[0]; number a2 = z[1];
    number alpha1 = z[2]; number alpha2 = z[3];
    number sum_residuals = 0.0;
    foreach (i; 0 .. 6) {
        number t = x[i];
        number eta = a1 * exp(alpha1 * t) + a2 * exp(alpha2 * t);
        number r = y[i] - eta;
        sum_residuals += r * r;
    }
    return sum_residuals;
}
