// probes/geom/misc/probe_univariatefunctions.d — dump everything a grid generator
// can observe from geom/misc/univariatefunctions.d: f(t) sweeps for all 8 derived
// classes plus the base, their toString() text, the parameter-distribution arrays
// they generate (sgrid.d's grid-spacing path), every documented endpoint value,
// each Error/Exception path with its message and taxonomy, and the iteration counts
// of the three internal Newton/Halley loops.
//
// READ THIS — why this probe imports univariatefunctions_copy, not the module: ldc2
// 1.40.0 (the compiler the frozen reference names in its own banner) cannot compile
// geom/misc/univariatefunctions.d at all, because VinokurFunction and
// VinokurGeomHybridFunction call nm.brent.solve from their constructors, and solve is
// declared @nogc while it does `throw new Exception` (nm/brent.d:51, :123):
//
//   nm/brent.d(51): Error: cannot use `new` in `@nogc` function
//       `geom.misc.univariatefunctions.VinokurFunction.this.solve!(zeroFun,double).solve`
//   geom/misc/univariatefunctions.d(603): Error: template instance ... error instantiating
//
// Identical with -release, and identical under ldmd2. Wave 1 met the same wall on
// nm.brent's own unittest (src/nm/brent.hpp:16-20). univariatefunctions_copy.d is a
// mechanical copy (module name, one import swapped, 3 counter statements) and
// brent_copy.d is nm/brent.d minus that one attribute — nothing that can change an
// arithmetic result or a message string. `python3 probes/geom/misc/gen_d_copies.py`
// prints the complete diff; `--check` re-verifies the committed copies are current.
// gdtk itself is never modified.
//
// --d-debug=1 is REQUIRED, not cosmetic: D's `debug { ... }` blocks are compiled OUT
// by a plain ldc2 invocation (measured — a `debug{writeln("ON");}` main prints nothing
// without the flag, prints with `--d-debug=1`). The two Vinokur catch blocks
// (univariatefunctions.d:607, :711) build their message inside `debug`, and the frozen
// reference is "Build-flavour: debug", whose message includes those lines. The port
// keeps them unconditionally, as src/nm/brent.hpp:84-86 already does, so the probe has
// to be built with --d-debug=1 for the texts to agree.
//
// Two measured facts this probe exists to pin down:
//   * univariatefunctions.d has NO fprime() and NO inverse() (0 hits over the whole
//     geom tree). The grid-spacing contract is opCall + the base class
//     distribute_parameter_values, so those two are swept instead; the converging
//     iterations are the three constructor-time loops (GeometricFunction.lambertW,
//     GaussianFunction centre Newton, GaussGeomHybridFunction.solve_for_xswitch).
//     brent's own iteration count belongs to nm/brent.d and is gated by ctest:nm_brent.
//   * 4 of the 8 copy constructors (which is what dup() calls) omit fields, so dup()
//     returns an object whose missing doubles are double.init == quiet NaN
//     (wave-02-brief section 5). Rows tagged "dup" measure that propagation.
//
// Build and capture:
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   cd /home/tang/packages/Eilmer
//   ldc2 --d-debug=1 -i -I/home/tang/packages/gdtk/src -Iprobes/geom/misc \
//     probes/geom/misc/probe_univariatefunctions.d -of=/tmp/uf_d
//   /tmp/uf_d > baselines/e587b235/probes/geom_misc_univariatefunctions.txt
import std.conv;
import std.format;
import std.math;
import std.stdio;

import univariatefunctions_copy;

// The sweep: documented endpoints t=0 and t=1, the unittest's sample points, and
// values either side of the geometric/gaussian switch point.
enum double[] TS = [0.0, 0.05, 0.1, 0.25, 0.5, 0.75, 0.9, 0.95, 1.0];

void sweep(string tag, UnivariateFunction f) {
    foreach (t; TS) writefln("f %s t=%.17g y=%.17g", tag, t, f(t));
}

void dist(string tag, UnivariateFunction f, size_t n, double t0, double t1) {
    auto tv = f.distribute_parameter_values(n, t0, t1);
    writefln("dist %s n=%d t0=%.17g t1=%.17g len=%d", tag, n, t0, t1, tv.length);
    foreach (i, v; tv) writefln("dist %s v[%d]=%.17g", tag, i, v);
}

/// Build inside a try so the Error/Exception taxonomy and the message text are
/// observable. `isError` distinguishes D's two sibling root branches, which the port
/// must keep apart (ExceptionBase and ErrorBase are siblings, not parent/child).
void try_build(string tag, lazy UnivariateFunction f) {
    try {
        auto g = f;
        writefln("err %s built=1 isError=0 isException=0 y0=%.17g y1=%.17g", tag, g(0.0), g(1.0));
    }
    catch (Error e) {
        writefln("err %s built=0 isError=1 isException=0 msg=[%s]", tag, esc(e.msg));
    }
    catch (Exception e) {
        writefln("err %s built=0 isError=0 isException=1 msg=[%s]", tag, esc(e.msg));
    }
}

/// Keep one printed line per observation: D messages from the multi-line guards
/// (VinokurGeomHybridFunction, VinokurFunction) embed `\n`, which would otherwise
/// shift the replay comparison. The port escapes with the same rule.
string esc(string s) {
    string res;
    foreach (dchar c; s) {
        if (c == '\n') res ~= "\\n";
        else res ~= text(c);
    }
    return res;
}

/// Ported from the module's own unittest (univariatefunctions.d:771-791), where it is
/// the de-facto *inverse*: it solves f(x) = m by bisection. D's version counts its
/// iterations in a local that is never read (`int count = 0; ... count += 1;`), so the
/// copy exposes the count through this `out` parameter instead. Same loop, same
/// tolerances (1e-6 relative interval, 1e-6 residual), same `&&` condition.
double bisect_iterate(alias f)(double p0, double p1, out int count) {
    count = 0;
    double f0 = f(p0);
    double f1 = f(p1);
    if (f0*f1 > 0.0) {
        throw new Exception("Does not bracket an odd number of roots.");
    }
    double pmid = 0.5*(p0+p1);
    double fmid = f(pmid);
    do {
        if (f0*fmid < 0.0) {
            p1 = pmid; f1 = fmid;
        } else {
            p0 = pmid; f0 = fmid;
        }
        pmid = 0.5*(p0+p1);
        fmid = f(pmid);
        count += 1;
    } while ((fabs(p1-p0)/pmid > 1e-6) && (fabs(fmid) > 1e-6));
    return pmid;
}

void main() {
    // ---- base class (D:17) ------------------------------------------------
    // opCall is the identity ("expect this to be overridden") and toString is fixed.
    auto base = new UnivariateFunction();
    writefln("base.toString=%s", base.toString());
    sweep("base", base);
    dist("base", base, 5, -1.0, 3.0);

    // ---- LinearFunction (D:41) -------------------------------------------
    auto lin = new LinearFunction();
    writefln("str LinearFunction.default=%s", lin.toString());
    sweep("lin.default", lin);
    auto lin2 = new LinearFunction(1.0, 0.0);
    writefln("str LinearFunction(1,0)=%s", lin2.toString());
    sweep("lin.1_0", lin2);
    writefln("close lin.1_0 f0.1=%.17g f0.9=%.17g", lin2(0.1), lin2(0.9));
    auto lin3 = new LinearFunction(2.5, -7.25);
    writefln("str LinearFunction(2.5,-7.25)=%s", lin3.toString());
    sweep("lin.2.5_-7.25", lin3);
    auto lind = lin3.dup();
    writefln("dup LinearFunction y0=%.17g y1=%.17g same=%s", lind(0.0), lind(1.0),
             to!string(lind(0.5) == lin3(0.5)));
    dist("lin.default", lin, 11, 0.0, 1.0);
    dist("lin.1_0", lin2, 5, -1.0, 3.0);

    // ---- QuadraticFunction (D:69) ----------------------------------------
    foreach (ratio; [2.0, 5.0, 1.5, 10.0]) {
        foreach (rev; [false, true]) {
            auto q = new QuadraticFunction(ratio, rev);
            string tag = "quad.r" ~ to!string(ratio) ~ (rev ? ".rev" : ".fwd");
            writefln("str %s=%s", tag, q.toString());
            sweep(tag, q);
        }
    }
    auto q1 = new QuadraticFunction(2.0, false);
    // The copy constructor (D:85-89) copies ratio and reverse but NOT a and b, so
    // dup() leaves a = b = double.init = NaN and every opCall is NaN. Preserved.
    auto qd = q1.dup();
    writefln("dup QuadraticFunction original=%.17g dup=%.17g dupIsNaN=%s dup0=%.17g",
             q1(0.25), qd(0.25), to!string(qd(0.25) != qd(0.25)), qd(0.0));
    writefln("dup QuadraticFunction str=%s", qd.toString());
    dist("quad.r2", q1, 9, 0.0, 1.0);

    // ---- RobertsFunction (D:110) and roberts_original (D:168) -------------
    foreach (combo; [[false, true], [true, false], [true, true], [false, false]]) {
        foreach (beta; [1.1, 2.0, 0.9, 1.0]) {
            auto rf = new RobertsFunction(combo[0], combo[1], beta);
            string tag = format("roberts.e%de%d.b%g", cast(int)combo[0], cast(int)combo[1], beta);
            writefln("str %s=%s", tag, rf.toString());
            sweep(tag, rf);
        }
    }
    auto runittest = new RobertsFunction(false, true, 1.1);
    writefln("close roberts.unittest f0.1=%.17g f0.9=%.17g", runittest(0.1), runittest(0.9));
    auto rd = runittest.dup();
    writefln("dup RobertsFunction f0.1=%.17g same=%s", rd(0.1), to!string(rd(0.1) == runittest(0.1)));
    dist("roberts.e0e1.b1.1", new RobertsFunction(true, true, 1.1), 11, 0.0, 1.0);

    // roberts_original directly, including the singular beta=1 (division by zero in
    // (beta+1)/(beta-1)) and beta<1 (negative base to a fractional power -> NaN).
    foreach (beta; [2.0, 1.1, 1.5]) {
        foreach (alpha; [0.5, 0.0]) {
            string tag = format("roberts_orig.a%g.b%g", alpha, beta);
            foreach (t; TS) writefln("f %s t=%.17g y=%.17g", tag, t, roberts_original(t, alpha, beta));
        }
    }
    foreach (t; TS) {
        writefln("f roberts_orig.beta1 t=%.17g y=%.17g", t, roberts_original(t, 0.5, 1.0));
        writefln("f roberts_orig.beta0.5 t=%.17g y=%.17g", t, roberts_original(t, 0.5, 0.5));
        writefln("f roberts_orig.beta0 t=%.17g y=%.17g", t, roberts_original(t, 0.5, 0.0));
    }

    // ---- GeometricFunction (D:183) ---------------------------------------
    // Only a few (a, r, N) sets get through lambertW at the code's own tol=1e-15; the
    // ones that do not are recorded under "err" below rather than silently dropped.
    auto geo = new GeometricFunction(0.005, 1.1, 40, false);
    writefln("close geometric.unittest f0.1=%.17g f0.9=%.17g", geo(0.1), geo(0.9));
    sweep("geo.0.005_1.1_40", geo);
    auto geo2 = new GeometricFunction(0.005, 1.1, 40, true);
    sweep("geo.0.005_1.1_40.rev", geo2);
    auto geo3 = new GeometricFunction(0.005, 1.2, 40, false);
    sweep("geo.0.005_1.2_40", geo3);
    auto geo4 = new GeometricFunction(0.01, 1.1, 40, false);
    sweep("geo.0.01_1.1_40", geo4);
    auto geo5 = new GeometricFunction(0.005, 1.1, 61, false);
    sweep("geo.0.005_1.1_61", geo5);
    auto geod = geo3.dup();
    writefln("dup GeometricFunction f0.5=%.17g same=%s", geod(0.5), to!string(geod(0.5) == geo3(0.5)));
    dist("geo.0.005_1.1_40", geo, 40, 0.0, 1.0);

    // ---- GaussianFunction (D:286) ----------------------------------------
    foreach (m; [0.0, 0.1, 0.5, 0.9, 1.0]) {
        auto gf = new GaussianFunction(m, 0.1, 0.2);
        string tag = format("gauss.m%g", m);
        sweep(tag, gf);
        writefln("str %s=%s", tag, gf.toString());
    }
    auto g4 = new GaussianFunction(0.5, 0.1, 0.2);
    writefln("close gaussian.unittest f0.1=%.17g f0.9=%.17g", g4(0.1), g4(0.9));
    sweep("gauss.s0.3.r0.9", new GaussianFunction(0.5, 0.3, 0.9));
    sweep("gauss.s0.01.r0.01", new GaussianFunction(0.5, 0.01, 0.01));
    auto gd = g4.dup();
    // GaussianFunction's copy ctor (D:324-331) omits b -> dup() is all NaN.
    writefln("dup GaussianFunction original=%.17g dup=%.17g dupIsNaN=%s", g4(0.25), gd(0.25),
             to!string(gd(0.25) != gd(0.25)));
    dist("gauss.m0.5", g4, 11, 0.0, 1.0);

    // ---- GaussGeomHybridFunction (D:370) ---------------------------------
    auto gh = new GaussGeomHybridFunction(0.01, 1.2, 40, 0.8, 0.1, 0.2, false);
    writefln("close gaussgeom.unittest f0.1=%.17g f0.9=%.17g", gh(0.1), gh(0.9));
    sweep("gaussgeom.0.01_1.2_40.m0.8", gh);
    auto gh2 = new GaussGeomHybridFunction(0.01, 1.2, 40, 0.5, 0.1, 0.2, false);
    sweep("gaussgeom.0.01_1.2_40.m0.5", gh2);
    auto gh3 = new GaussGeomHybridFunction(0.005, 1.1, 40, 0.3, 0.05, 0.3, false);
    sweep("gaussgeom.0.005_1.1_40.m0.3", gh3);
    auto gh4 = new GaussGeomHybridFunction(0.01, 1.2, 40, 0.8, 0.1, 0.2, true);
    sweep("gaussgeom.0.01_1.2_40.m0.8.rev", gh4);
    auto ghd = gh.dup();
    // GaussGeomHybridFunction's copy ctor (D:409-419) omits a and c -> NaN again.
    writefln("dup GaussGeomHybrid f0.5=%.17g dup0.5=%.17g dupIsNaN=%s", gh(0.5), ghd(0.5),
             to!string(ghd(0.5) != ghd(0.5)));
    dist("gaussgeom.0.01_1.2_40", gh, 21, 0.0, 1.0);

    // ---- VinokurFunction (D:557) -----------------------------------------
    auto vk = new VinokurFunction(40, 0.01, 0.02);
    sweep("vinokur.n40.0.01.0.02", vk);
    auto vk2 = new VinokurFunction(100, 0.001, 0.01);
    sweep("vinokur.n100.0.001.0.01", vk2);
    auto vk3 = new VinokurFunction(50, 0.1, 0.001);
    sweep("vinokur.n50.0.1.0.001", vk3);
    auto vkd = vk.dup();
    writefln("dup Vinokur f0.5=%.17g same=%s", vkd(0.5), to!string(vkd(0.5) == vk(0.5)));
    dist("vinokur.n40", vk, 41, 0.0, 1.0);

    // ---- VinokurGeomHybridFunction (D:643) -------------------------------
    auto vg = new VinokurGeomHybridFunction(40, 0.01, 3, 1.2, 0.02, 4, 1.1);
    sweep("vinokurgeo.n40", vg);
    auto vg2 = new VinokurGeomHybridFunction(40, 0.01, 0, 1.0, 0.01, 0, 1.0);
    sweep("vinokurgeo.nolayers", vg2);
    auto vg3 = new VinokurGeomHybridFunction(60, 0.005, 5, 1.05, 0.005, 5, 1.05);
    sweep("vinokurgeo.sym", vg3);
    auto vg4 = new VinokurGeomHybridFunction(20, 0.02, 2, 1.5, 0.02, 3, 0.8);
    sweep("vinokurgeo.r1lt1", vg4);
    auto vgd = vg.dup();
    writefln("dup VinokurGeomHybrid f0.5=%.17g same=%s", vgd(0.5), to!string(vgd(0.5) == vg(0.5)));
    dist("vinokurgeo.n40", vg, 41, 0.0, 1.0);

    // ---- the four guard branches, plus the two that are not guards -------
    try_build("geometric.a0", new GeometricFunction(0.0, 1.1, 40, false));
    try_build("geometric.aneg", new GeometricFunction(-0.5, 1.1, 40, false));
    try_build("geometric.r1", new GeometricFunction(0.005, 1.0, 40, false));
    try_build("geometric.r0.9", new GeometricFunction(0.005, 0.9, 40, false));
    // ns outside [1, N]: lambertW converges but the switch point is negative.
    try_build("geometric.ns_neg", new GeometricFunction(0.05, 1.1, 40, false));
    // lambertW itself timing out at the code's tol=1e-15 (D:253 Error).
    try_build("geometric.lambertW_timeout.21", new GeometricFunction(0.005, 1.1, 21, false));
    try_build("geometric.lambertW_timeout.a0.002", new GeometricFunction(0.002, 1.1, 40, false));
    try_build("geometric.lambertW_timeout.r1.01", new GeometricFunction(0.005, 1.01, 40, false));
    try_build("gaussian.mneg", new GaussianFunction(-0.1, 0.1, 0.2));
    try_build("gaussian.m2", new GaussianFunction(2.0, 0.1, 0.2));
    try_build("gaussian.ratio0", new GaussianFunction(0.5, 0.1, 0.0));
    try_build("gaussian.ratio2", new GaussianFunction(0.5, 0.1, 2.0));
    try_build("gaussgeom.A0", new GaussGeomHybridFunction(0.0, 1.2, 40, 0.8, 0.1, 0.2, false));
    try_build("gaussgeom.R1", new GaussGeomHybridFunction(0.01, 1.0, 40, 0.8, 0.1, 0.2, false));
    try_build("gaussgeom.m0", new GaussGeomHybridFunction(0.01, 1.2, 40, 0.0, 0.1, 0.2, false));
    try_build("gaussgeom.m1", new GaussGeomHybridFunction(0.01, 1.2, 40, 1.0, 0.1, 0.2, false));
    try_build("gaussgeom.ratio0", new GaussGeomHybridFunction(0.01, 1.2, 40, 0.8, 0.1, 0.0, false));
    try_build("gaussgeom.ratio1", new GaussGeomHybridFunction(0.01, 1.2, 40, 0.8, 0.1, 1.0, false));
    // solve_for_xswitch's Newton landing outside (0,1): Error at D:402.
    try_build("gaussgeom.xs_out", new GaussGeomHybridFunction(0.02, 1.1, 21, 0.4, 0.15, 0.5, false));
    try_build("vinokur.Blt1", new VinokurFunction(10, 1.0, 1.0));
    // B == 1 exactly is NOT guarded by `if (B<1.0)` (D:583): sinh(beta)/beta - 1 > 0
    // over the whole bracket, so brent cannot bracket a root and the catch at D:605
    // rethrows an Exception carrying brent's message.
    try_build("vinokur.Beq1", new VinokurFunction(20, 0.05, 0.05));
    try_build("vinokur.noroot", new VinokurFunction(1, 1.0e-100, 1.0e-100));
    // A multi-line Error message (D:678-682 and :688-692 append three more lines).
    try_build("vinokurgeo.vnlt1", new VinokurGeomHybridFunction(4, 0.01, 2, 1.2, 0.02, 2, 1.1));
    try_build("vinokurgeo.Blt1", new VinokurGeomHybridFunction(21, 0.02, 0, 1.0, 0.03, 5, 1.3));

    // ---- distribute_parameter_values edge cases (D:27-38) ----------------
    // dt = 1.0/(n-1) with n a size_t: n=1 divides by zero (inf), n=0 wraps.
    dist("linear.n1", new LinearFunction(0.0, 1.0), 1, 0.0, 1.0);
    dist("linear.n2", new LinearFunction(0.0, 1.0), 2, 0.0, 1.0);
    dist("linear.n0", new LinearFunction(0.0, 1.0), 0, 0.0, 1.0);
    dist("linear.reversed.n6", new LinearFunction(1.0, 0.0), 6, 10.0, 20.0);

    // ---- iteration counts (added by univariatefunctions_copy.d) ----------
    // D exposes no counter for its three internal fixed-point loops, so the copy
    // writes the loop index into a module global. The globals are at module scope, so
    // each row resets them before constructing.
    lam_iters = 0;
    auto gi = new GeometricFunction(0.005, 1.1, 40, false);
    writefln("iter GeometricFunction lam=%d f0.1=%.17g f0.9=%.17g", lam_iters, gi(0.1), gi(0.9));
    lam_iters = 0;
    auto gi2 = new GeometricFunction(0.005, 1.2, 40, false);
    writefln("iter GeometricFunction.r1.2 lam=%d f0.5=%.17g", lam_iters, gi2(0.5));
    lam_iters = 0;
    auto gi3 = new GeometricFunction(0.005, 1.1, 61, false);
    writefln("iter GeometricFunction.n61 lam=%d f0.5=%.17g", lam_iters, gi3(0.5));

    foreach (m; [0.0, 0.1, 0.5, 0.9, 1.0]) {
        gauss_iters = 0;
        auto gfi = new GaussianFunction(m, 0.1, 0.2);
        writefln("iter GaussianFunction m=%g gauss=%d f0.5=%.17g errNotConverged=%s", m,
                 gauss_iters, gfi(0.5), to!string(gauss_iters == 100));
    }
    gauss_iters = 0;
    auto g5 = new GaussianFunction(0.5, 0.3, 0.9);
    writefln("iter GaussianFunction s0.3.r0.9 gauss=%d f0.5=%.17g", gauss_iters, g5(0.5));
    gauss_iters = 0;
    auto g6 = new GaussianFunction(0.5, 0.01, 0.01);
    writefln("iter GaussianFunction s0.01.r0.01 gauss=%d f0.5=%.17g", gauss_iters, g6(0.5));

    foreach (c; [[0.01, 1.2, 40, 0.8, 0.1, 0.2, 0.0], [0.01, 1.2, 40, 0.5, 0.1, 0.2, 0.0],
                 [0.005, 1.1, 40, 0.3, 0.05, 0.3, 0.0], [0.01, 1.2, 40, 0.8, 0.1, 0.2, 1.0]]) {
        xsw_iters = 0;
        bool rev = (c[6] != 0.0);
        auto ghi = new GaussGeomHybridFunction(c[0], c[1], cast(int)c[2], c[3], c[4], c[5], rev);
        writefln("iter GaussGeomHybrid A=%g R=%g n=%d m=%g s=%g ratio=%g rev=%s xsw=%d f0.5=%.17g",
                 c[0], c[1], cast(int)c[2], c[3], c[4], c[5], rev, xsw_iters, ghi(0.5));
    }

    // The two Vinokur classes iterate inside brent.solve (whose own iteration count is
    // gated by ctest:nm_brent), so only the converged root shows up here: f(0.5) for
    // the symmetric pair is the bit-exact signature of that root.
    auto vki = new VinokurFunction(40, 0.01, 0.02);
    writefln("vinokur.copy n40 f0=%.17g f0.5=%.17g f1=%.17g", vki(0.0), vki(0.5), vki(1.0));
    auto vgi = new VinokurGeomHybridFunction(40, 0.01, 3, 1.2, 0.02, 4, 1.1);
    writefln("vinokurgeo.copy n40 f0=%.17g f0.5=%.17g f1=%.17g", vgi(0.0), vgi(0.5), vgi(1.0));

    // ---- the unittest's bisection inverse, with its iteration count -------
    // Mirrors test_gaussian_function_curvature (D:793-812): for each m, invert the
    // Gaussian cluster function by bisection, then measure slope and curvature there
    // with the finite differences D uses (h = 1.0e-3).
    foreach (m; [0.0, 0.1, 0.5, 0.9, 1.0]) {
        auto cf = new GaussianFunction(m, 0.1, 0.2);
        double test_point, slope, curvature;
        double h = 1.0e-3;
        int iters = 0;
        if ((m == 0.0) || (m == 1.0)) {
            test_point = m;
            writefln("inverse m=%g path=endpoint test=%.17g iters=-1", m, test_point);
        } else {
            test_point = bisect_iterate!((x) => cf(x) - m)(0.0, 1.0, iters);
            writefln("inverse m=%g path=bisect test=%.17g iters=%d resid=%.17g", m, test_point,
                     iters, fabs(cf(test_point) - m));
        }
        slope = (cf(test_point+h) - cf(test_point-h)) / (2*h);
        curvature = (cf(test_point+h) - 2*cf(test_point) + cf(test_point-h)) / (h*h);
        writefln("inverse m=%g slope=%.17g curvature=%.17g", m, slope, curvature);
    }
    // The bracket failure path of the same helper (D:776).
    {
        int iters = 0;
        try {
            auto cf = new GaussianFunction(0.5, 0.1, 0.2);
            // f(x) - 0.5 on [0.4, 0.6] has both endpoints on the same side of zero.
            double x = bisect_iterate!((t) => cf(t) + 1.0)(0.0, 1.0, iters);
            writefln("inverse bracket built=1 x=%.17g", x);
        }
        catch (Exception e) {
            writefln("inverse bracket built=0 isError=0 isException=1 msg=[%s] iters=%d",
                     esc(e.msg), iters);
        }
    }

    writefln("done=1");
}
