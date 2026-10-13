// probes/gasdyn/probe_idealgasflow.d — measured D behaviour for gasdyn/idealgasflow.d.
//
// What it pins, in the order the lines are printed:
//   1. std.math.isClose's *defaults* for (double,double), which the six unittest
//      blocks of idealgasflow.d rely on (18 bare isClose calls).
//   2. Every D unittest assertion, evaluated one at a time (a failing assert aborts
//      the block, so the probe prints the verdict of each instead of asserting).
//   3. The ideal-gas relations swept over a dense grid at %.17g: isentropic
//      reservoir relations, area-Mach, normal shock, Rayleigh line, Prandtl-Meyer,
//      oblique shock across the Mach angle / weak branch / detachment / strong
//      branch, pitot.
//   4. The inverse problems that use nm's root finders (M_Rayleigh, PM2, beta_obl,
//      beta_cone/beta_cone2), with the number of f() evaluations bracket()+solve()
//      actually performed, counted by a hand-rolled replica of the same call
//      sequence. The same root reached in a different number of steps means the
//      bracketing diverged, so the counts are part of the corpus.
//   5. The exception paths, printed as e.msg. That text is deterministic: it is
//      std.conv.text of the call's own arguments (%.6g per the substrate rules),
//      and the message alone is what the ported catch clauses look at.
//
// Build and capture (exact commands; the D tree is read-only, all output goes to
// /tmp, and nothing under /home/tang/packages/gdtk is written):
//   /home/tang/packages/dlang/bin/ldc2 -w -g -dip1008 -I/home/tang/packages/gdtk/src \
//     -od/tmp/gd_probe/obj -of=/tmp/gd_probe/probe_igf \
//     probes/gasdyn/probe_idealgasflow.d \
//     /home/tang/packages/gdtk/src/gasdyn/idealgasflow.d \
//     /home/tang/packages/gdtk/src/gasdyn/gasflowexception.d
//   /tmp/gd_probe/probe_igf > baselines/e587b235/probes/gasdyn_idealgasflow.txt
//
// The reference must be compiled from an explicit file list, NOT with `-i`:
// `ldc2 -i -I<gdtk>/src ...` fails with
//   nm/bbla.d(709): Error: cannot use `new` in `@nogc` function
//       gaussJordanElimination!double
// because idealgasflow.d:760 instantiates that @nogc template, whose two
// `throw new Exception` lines (bbla.d:709,718) are illegal in @nogc. Same
// reference-tree defect wave-01 recorded for brent.d/newton.d (reports/wave-01.md:89).
// Nothing in the D tree was modified; the flag set is gasdyn/makefile's DFLAGS
// minus `-debug` (ldc2 spells that `--d-debug`, and it changes nothing here).
//
// Replay check: ctest -R gasdyn_idealgasflow
import std.stdio, std.math, std.conv, std.format;
import std.string : indexOf;
import gasdyn.idealgasflow;
import gasdyn.gasflowexception : GasFlowException;
import nm.bracketing : bracket;
import nm.ridder : solve;

// ---------------------------------------------------------------- instrumentation
int g_evals = 0;   // counts f() evaluations of the replicated root finds
double g_g = 1.4;  // gamma for the replicated objectives
double g_target;   // target of the replicated objective
double g_M1;       // upstream Mach for the replicated objectives
double g_V1 = 0.0, g_p1 = 0.0, g_T1 = 0.0, g_R = 287.1;

double rayleigh_f(double m) {
    ++g_evals;
    return T0_T0star(m, g_g) - g_target;
}

double theta_f(double beta) {
    ++g_evals;
    return theta_obl(g_M1, beta, g_g) - g_target;
}

double cone_f(double beta) {
    ++g_evals;
    double[] r = theta_cone(g_V1, g_p1, g_T1, beta, g_R, g_g);
    return r[0] - g_target;
}

/// one scalar quantity of a sweep
void q(string name, double a, double b, double v) {
    writefln("%s a=%.17g b=%.17g v=%.17g", name, a, b, v);
}

/// one quantity that may hit a domain guard
void qc(string name, double a, double b, double delegate() f) {
    try {
        writefln("%s a=%.17g b=%.17g v=%.17g", name, a, b, f());
    } catch (Exception e) {
        writefln("%s a=%.17g b=%.17g THROW=%s", name, a, b, e.msg);
    }
}

/// one group (several values) returned together
void qv(string name, double a, double b, double[] v) {
    string s = format("%s a=%.17g b=%.17g", name, a, b);
    foreach (x; v) s ~= format(" v=%.17g", x);
    writeln(s);
}

void qvc(string name, double a, double b, double[] delegate() f) {
    try {
        qv(name, a, b, f());
    } catch (Exception e) {
        writefln("%s a=%.17g b=%.17g THROW=%s", name, a, b, e.msg);
    }
}

/// nominal row + the same call with V1 one ULP higher, as two corpus lines
void qvc_pair(string name, double V1, double beta, double p1, double T1, double R, double g) {
    double v1p = nextafter(V1, double.infinity);
    try {
        qv(name, V1, beta, theta_cone(V1, p1, T1, beta, R, g));
    } catch (Exception e) {
        writefln("%s a=%.17g b=%.17g THROW=%s", name, V1, beta, e.msg);
    }
    // Separate try/catch: the two rows can fail differently, and one shared block
    // would print the nominal row as THROW when only the perturbed one failed.
    try {
        qv(name ~ "_1ulp", v1p, beta, theta_cone(v1p, p1, T1, beta, R, g));
    } catch (Exception e) {
        writefln("%s_1ulp a=%.17g b=%.17g THROW=%s", name, v1p, beta, e.msg);
    }
}

void main() {
    // ---------------------------------------------------------------- 1. defaults
    // A ladder of relative gaps: the widest gap still called "close" is the default
    // maxRelDiff, and the abs rows show there is no absolute slack.
    foreach (d; [1.0e-10, 5.0e-10, 9.0e-10, 1.0e-9, 1.1e-9, 2.0e-9, 1.0e-8, 1.0e-7]) {
        writefln("tol_rel d=%.17g holds=%s", d, isClose(1.0, 1.0 + d));
    }
    foreach (d; [1.0e-9, 1.0e-8, 1.0e-7, 1.0e-6, 1.0e-5, 1.2e-4, 2.0e-4]) {
        writefln("tol_rel14 d=%.17g holds=%s", d, isClose(14.62, 14.62 * (1.0 + d)));
    }
    foreach (a; [1.0e-9, 1.0e-8, 1.0e-6, 1.0e-5, 1.0e-4]) {
        writefln("tol_abs d=%.17g holds=%s", a, isClose(0.0001, 0.0001 + a));
    }

    // ------------------------------------------------------- 2. unittest verdicts
    // The six unittest blocks, one assertion per line, in source order. Each line
    // carries the computed value at %.17g, the literature literal and the verdict
    // of the bare isClose(a,b) the D unittest uses.
    {
        double M = 2.4;
        double g = 1.4;
        writefln("u1_T0_T v=%.17g lit=2.152 holds=%s", T0_T(M, g), isClose(T0_T(M,g), 2.152));
        writefln("u1_p0_p v=%.17g lit=14.620 holds=%s", p0_p(M, g), isClose(p0_p(M,g), 14.620));
        writefln("u1_r0_r v=%.17g lit=6.7937 holds=%s", r0_r(M, g), isClose(r0_r(M,g), 6.7937));
    }
    {
        double M = 2.0;
        double g = 1.4;
        writefln("u2_m2_shock v=%.17g lit=0.5774 holds=%s", m2_shock(M,g), isClose(m2_shock(M,g), 0.5774));
        writefln("u2_p2_p1 v=%.17g lit=4.50 holds=%s", p2_p1(M,g), isClose(p2_p1(M,g), 4.50));
        writefln("u2_T2_T1 v=%.17g lit=1.687 holds=%s", T2_T1(M,g), isClose(T2_T1(M,g), 1.687));
        writefln("u2_r2_r1 v=%.17g lit=2.667 holds=%s", r2_r1(M,g), isClose(r2_r1(M,g), 2.667));
    }
    {
        double M = 2.0;
        double g = 1.4;
        writefln("u3_T0_T0star v=%.17g lit=0.7934 holds=%s", T0_T0star(M,g), isClose(T0_T0star(M,g), 0.7934));
        writefln("u3_T_Tstar v=%.17g lit=0.5289 holds=%s", T_Tstar(M,g), isClose(T_Tstar(M,g), 0.5289));
        writefln("u3_p_pstar v=%.17g lit=0.3636 holds=%s", p_pstar(M,g), isClose(p_pstar(M,g), 0.3636));
        writefln("u3_r_rstar v=%.17g lit=0.6875 holds=%s", r_rstar(M,g), isClose(r_rstar(M,g), 0.6875));
        writefln("u3_M_Rayleigh v=%.17g lit=2 holds=%s", M_Rayleigh(T0_T0star(M,g),g),
                 isClose(M_Rayleigh(T0_T0star(M,g),g), M));
    }
    {
        double M = 2.4;
        double g = 1.4;
        double nu = 0.6413479572;
        writefln("u4_PM1 v=%.17g lit=0.6413 holds=%s", PM1(M,g), isClose(PM1(M,g), 0.6413));
        writefln("u4_PM2 v=%.17g lit=2.4 holds=%s", PM2(nu,g), isClose(PM2(nu,g), 2.4));
        writefln("u4_MachAngle v=%.17g lit=0.430 holds=%s", MachAngle(M), isClose(MachAngle(M), 0.430));
        // The three catch clauses of the u4 block, on the message text.
        try { PM1(0.8,g); } catch (GasFlowException e) {
            writefln("u4_PM1_subsonic holds=%s msg=%s", indexOf(e.toString(), "subsonic") != -1, e.msg);
        }
        try { PM2(-0.5,g); } catch (GasFlowException e) {
            writefln("u4_PM2_negative holds=%s msg=%s", indexOf(e.toString(), "negative") != -1, e.msg);
        }
        try { MachAngle(0.8); } catch (GasFlowException e) {
            writefln("u4_MachAngle_subsonic holds=%s msg=%s", indexOf(e.toString(), "subsonic") != -1, e.msg);
        }
    }
    {
        double M = 2.0;
        double g = 1.4;
        double beta = 44.0 * PI / 180.0;
        double theta = 14.0 * PI / 180.0;
        writefln("u5_angles beta=%.17g theta=%.17g", beta, theta);
        writefln("u5_beta_obl v=%.17g lit=%.17g holds=%s", beta_obl(M,theta,g), beta, isClose(beta_obl(M,theta,g), beta));
        writefln("u5_beta_obl2 v=%.17g lit=%.17g holds=%s", beta_obl2(M,2.088,g), beta, isClose(beta_obl2(M,2.088,g), beta));
        writefln("u5_theta_obl v=%.17g lit=%.17g holds=%s", theta_obl(M,beta,g), theta, isClose(theta_obl(M,beta,g), theta));
        writefln("u5_M2_obl v=%.17g lit=1.482 holds=%s", M2_obl(M,beta,theta,g), isClose(M2_obl(M,beta,theta,g), 1.482));
        writefln("u5_T2_T1_obl v=%.17g lit=1.249 holds=%s", T2_T1_obl(M,beta,g), isClose(T2_T1_obl(M,beta,g), 1.249));
        writefln("u5_p2_p1_obl v=%.17g lit=2.088 holds=%s", p2_p1_obl(M,beta,g), isClose(p2_p1_obl(M,beta,g), 2.088));
        writefln("u5_r2_r1_obl v=%.17g lit=1.673 holds=%s", r2_r1_obl(M,beta,g), isClose(r2_r1_obl(M,beta,g), 1.673));
        writefln("u5_p02_p01_obl v=%.17g lit=0.9608 holds=%s", p02_p01_obl(M,beta,g), isClose(p02_p01_obl(M,beta,g), 0.9608));
        writefln("u5_Vn2_Vn1_obl v=%.17g lit=0.598 holds=%s", Vn2_Vn1_obl(M,beta,g), isClose(Vn2_Vn1_obl(M,beta,g), 0.598));
        writefln("u5_V2_V1_obl v=%.17g lit=0.828 holds=%s", V2_V1_obl(M,beta,g), isClose(V2_V1_obl(M,beta,g), 0.828));
        // The two catch clauses of the u5 block (D asserts true inside them).
        try { auto bb = beta_obl(M, 40.0*PI/180.0, g);
              writefln("u5_detached threw=%s v=%.17g", false, bb); }
        catch (Exception e) { writefln("u5_detached threw=%s msg=%s", true, e.msg); }
        try { auto tt = T2_T1_obl(0.8, beta, g);
              writefln("u5_subsonic_obl threw=%s v=%.17g", false, tt); }
        catch (Exception e) { writefln("u5_subsonic_obl threw=%s msg=%s", true, e.msg); }
    }
    {
        double M1 = 1.5; double p1 = 100.0e3; double T1 = 300.0;
        double R = 287.1; double g = 1.4; double rho1 = p1/(R*T1);
        double a1 = sqrt(g*R*T1);
        double V1 = M1 * a1;
        double beta = 49.0 * PI/180;
        double[] results = theta_cone(V1, p1, T1, beta);
        writefln("u6_theta_cone v=%.17g v=%.17g v=%.17g v=%.17g",
                 results[0], results[1], results[2], results[3]);
        writefln("u6_cone_angle v=%.17g lit=19.96 holds=%s", results[0]*180.0/PI,
                 isClose(results[0]*180.0/PI, 19.96));
        writefln("u6_cone_cp v=%.17g lit=0.386 holds=%s", (results[2]-p1)/(0.5*rho1*V1*V1),
                 isClose((results[2]-p1)/(0.5*rho1*V1*V1), 0.386));
        writefln("u6_beta_cone v=%.17g lit=49 holds=%s", beta_cone(V1,p1,T1,20.0*PI/180)*180/PI,
                 isClose(beta_cone(V1,p1,T1,20.0*PI/180)*180/PI, 49.0));
        writefln("u6_beta_cone2 v=%.17g lit=49 holds=%s", beta_cone2(M1,20.0*PI/180)*180/PI,
                 isClose(beta_cone2(M1,20.0*PI/180)*180/PI, 49.0));
    }

    // ------------------------------------------------- 3. dense relation sweeps
    double[] gammas = [1.2, 1.4, 1.666];
    double[] msup = [1.0000001, 1.001, 1.01, 1.05, 1.1, 1.2, 1.3, 1.4, 1.5, 1.75,
                     2.0, 2.5, 3.0, 4.0, 5.0, 7.0, 10.0, 20.0];
    double[] msub = [0.2, 0.5, 0.8, 0.99, 0.9999999];
    foreach (g; gammas) {
        foreach (M; msub ~ msup) {
            q("A_Astar", g, M, A_Astar(M, g));
            q("T0_T", g, M, T0_T(M, g));
            q("p0_p", g, M, p0_p(M, g));
            q("r0_r", g, M, r0_r(M, g));
            q("T0_T0star", g, M, T0_T0star(M, g));
            q("T_Tstar", g, M, T_Tstar(M, g));
            q("p_pstar", g, M, p_pstar(M, g));
            q("r_rstar", g, M, r_rstar(M, g));
            q("p0_p0star", g, M, p0_p0star(M, g));
            q("pitot_p", g, M, pitot_p(101325.0, M, g));
        }
        // Subsonic guard rows: the shock and PM relations throw below M=1.
        foreach (M; [0.0, 0.3, 0.8, 0.9999]) {
            qc("m2_shock", g, M, () => m2_shock(M, g));
            qc("r2_r1", g, M, () => r2_r1(M, g));
            qc("u2_u1", g, M, () => u2_u1(M, g));
            qc("p2_p1", g, M, () => p2_p1(M, g));
            qc("T2_T1", g, M, () => T2_T1(M, g));
            qc("p02_p01", g, M, () => p02_p01(M, g));
            qc("ds_Cv", g, M, () => ds_Cv(M, g));
            qc("PM1", g, M, () => PM1(M, g));
            qc("MachAngle", g, M, () => MachAngle(M));
        }
        // Supersonic normal-shock block and entropy.
        foreach (M; msup) {
            q("m2_shock", g, M, m2_shock(M, g));
            q("r2_r1", g, M, r2_r1(M, g));
            q("u2_u1", g, M, u2_u1(M, g));
            q("p2_p1", g, M, p2_p1(M, g));
            q("T2_T1", g, M, T2_T1(M, g));
            q("p02_p01", g, M, p02_p01(M, g));
            q("ds_Cv", g, M, ds_Cv(M, g));
            q("PM1", g, M, PM1(M, g));
            q("MachAngle", g, M, MachAngle(M));
        }
    }

    // ------------------------------------------------------ 4. oblique shocks
    // beta steps across the Mach angle, the weak branch, the theta maximum
    // (detachment) and the strong branch, for each gamma and each M1.
    double[] mobs = [1.2, 1.5, 2.0, 3.0, 5.0, 10.0, 20.0];
    foreach (g; gammas) {
        foreach (M; mobs) {
            for (int deg = 1; deg <= 89; deg += 2) {
                double beta = deg * PI / 180.0;
                qc("theta_obl", M, beta, () => theta_obl(M, beta, g));
                q("dtan_theta", M, beta, dtan_theta(M, beta, g));
                qc("r2_r1_obl", M, beta, () => r2_r1_obl(M, beta, g));
                qc("p2_p1_obl", M, beta, () => p2_p1_obl(M, beta, g));
                qc("T2_T1_obl", M, beta, () => T2_T1_obl(M, beta, g));
                qc("p02_p01_obl", M, beta, () => p02_p01_obl(M, beta, g));
                qc("Vn2_Vn1_obl", M, beta, () => Vn2_Vn1_obl(M, beta, g));
                qc("V2_V1_obl", M, beta, () => V2_V1_obl(M, beta, g));
                // M2_obl needs the deflection angle of this shock.
                qc("M2_obl", M, beta, () => M2_obl(M, beta, theta_obl(M, beta, g), g));
            }
            // The theta==0 and sign-beta branches of beta_obl/theta_obl.
            qc("beta_obl_zero", M, 0.0, () => beta_obl(M, 0.0, g));
            qc("theta_obl_neg", M, 40.0*PI/180.0, () => theta_obl(M, -40.0*PI/180.0, g));
            qvc("taylor_maccoll_odes", M, 40.0*PI/180.0,
                () => taylor_maccoll_odes([1.2e0, 300.0, -100.0, 1.0e6, 1.0e5], 40.0*PI/180.0, g));
            qvc("taylor_maccoll_odes", M, 70.0*PI/180.0,
                () => taylor_maccoll_odes([1.2e0, 300.0, -100.0, 1.0e6, 1.0e5], 70.0*PI/180.0, g));
            qvc("taylor_maccoll_odes", M, 89.0*PI/180.0,
                () => taylor_maccoll_odes([1.2e0, 300.0, -100.0, 1.0e6, 1.0e5], 89.0*PI/180.0, g));
        }
    }

    // ------------------------------- 5. inverse problems with step counts
    // M_Rayleigh: f(m) = T0_T0star(m,g) - target, bracket(1,2) then solve(1,2),
    // exactly idealgasflow.d:266-274. The replica is instrumented.
    foreach (g; gammas) {
        foreach (Msrc; [1.05, 1.2, 1.5, 2.0, 3.0, 5.0, 8.0, 15.0]) {
            double target = T0_T0star(Msrc, g);
            double lib = M_Rayleigh(target, g);
            g_evals = 0;
            g_g = g;
            g_target = target;
            double b1 = 1.0, b2 = 2.0;
            int flag = bracket!rayleigh_f(b1, b2);
            int fe_bracket = g_evals;
            double root = solve!rayleigh_f(b1, b2);
            writefln("M_Rayleigh g=%.17g in=%.17g lib=%.17g repl=%.17g flag=%d fe_bracket=%d fe_total=%d b1=%.17g b2=%.17g",
                     g, target, lib, root, flag, fe_bracket, g_evals, b1, b2);
        }
    }
    // PM2: secant loop seeded by MFromNu_approximate (idealgasflow.d:380-413).
    // The loop state is pinned by the returned M at %.17g plus the residual, since
    // any different iteration count lands on a different M.
    foreach (g; gammas) {
        foreach (nu; [0.0, 1.0e-3, 0.01, 0.05, 0.1, 0.2, 0.37, 0.6413479572, 0.9,
                     1.2, 1.5, 1.8, 2.0, 2.2, 2.4]) {
            q("MFromNu_approximate", g, nu, MFromNu_approximate(nu, g));
            qc("PM2", g, nu, () => PM2(nu, g));
            qc("PM2_resid", g, nu, () => nu - PM1(PM2(nu, g), g));
        }
        // Past the maximum turning angle of each gamma, and negative nu.
        foreach (nu; [-1.0e-3, -0.5, -1.0, 3.0, 3.5, 4.0]) {
            q("MFromNu_approximate", g, nu, MFromNu_approximate(nu, g));
            qc("PM2", g, nu, () => PM2(nu, g));
        }
    }
    // beta_obl: theta swept across the weak/strong split and the detachment limit.
    foreach (g; gammas) {
        foreach (M; mobs) {
            for (int deg = 1; deg <= 45; deg += 2) {
                double theta = deg * PI / 180.0;
                qc("beta_obl", M, theta, () => beta_obl(M, theta, g));
                qc("beta_obl_neg", M, theta, () => beta_obl(M, -theta, g));
                // replica of idealgasflow.d:502-527, instrumented
                g_evals = 0;
                g_g = g;
                g_M1 = M;
                g_target = theta;
                int flag = 0;
                double root = 0.0;
                double b1 = asin(1.0/M);
                double b2 = b1 * 1.05;
                bool took_branch = false;
                try {
                    if (theta < 1.0e-6) {
                        root = b1; took_branch = true;
                    } else {
                        double f1 = theta_f(b1);
                        if (fabs(f1) < 1.0e-6) {
                            root = b1; took_branch = true;
                        } else {
                            double f2 = theta_f(b2);
                            if (fabs(f2) < 1.0e-6) {
                                root = b2; took_branch = true;
                            } else {
                                flag = bracket!theta_f(b1, b2);
                                if (flag < 0) {
                                    throw new Exception(text(
                                        "beta_obl: could not bracket beta for M1=", M,
                                        " theta=", theta));
                                }
                                root = solve!theta_f(b1, b2);
                            }
                        }
                    }
                } catch (Exception e) {
                    writefln("beta_obl_repl a=%.17g b=%.17g THROW=%s fe_total=%d flag=%d",
                             M, theta, e.msg, g_evals, flag);
                    continue;
                }
                writefln("beta_obl_repl a=%.17g b=%.17g v=%.17g fe_total=%d flag=%d branch=%d b1=%.17g b2=%.17g",
                         M, theta, root, g_evals, flag, took_branch ? 1 : 0, b1, b2);
            }
            // beta_obl2 (explicit p2/p1 form, no root finder) and its guards
            foreach (pp; [0.5, 1.0, 1.5, 2.088, 5.0, 20.0, 60.0]) {
                qc("beta_obl2", M, pp, () => beta_obl2(M, pp, g));
            }
        }
    }
    // theta_cone: the shock-angle-side problem, including the beta <= mu and
    // beta < 1.01*mu (linear interpolation) branches.
    {
        double p1 = 100.0e3;
        double T1 = 300.0;
        double R = 287.1;
        foreach (g; gammas) {
            foreach (Msrc; [1.2, 1.5, 2.0, 3.0]) {
                double a1 = sqrt(g*R*T1);
                double V1 = Msrc * a1;
                double mu = asin(1.0/Msrc);
                // beta is swept both as a multiple of the Mach angle and in degrees.
                // Rows at or beyond beta = PI/2 are dropped: a conical shock that wide
                // is not a physical solution, tan(beta) is singular there, and the
                // Euler march amplifies a 1 ULP input change into an O(1) answer, so
                // such a row cannot gate a translation. The theta_cone_1ulp rows right
                // after each nominal row MEASURE that amplification: they are the same
                // call with V1 moved up by exactly one ULP (std.nextafter), which is the
                // smallest possible perturbation the port can be out by.
                foreach (frac; [0.9, 1.0, 1.005, 1.02, 1.1, 1.5, 2.0, 3.0]) {
                    double beta = mu * frac;
                    if (beta >= PI/2) continue;
                    qvc_pair("theta_cone", V1, beta, p1, T1, R, g);
                }
                foreach (deg; [5, 15, 25, 35, 45, 55, 65, 75, 85]) {
                    double beta = deg * PI/180.0;
                    qvc_pair("theta_cone", V1, beta, p1, T1, R, g);
                }
            }
        }
    }
    // beta_cone inverts theta_cone with bracket(b1,b2, mu, PI/2) + solve(b1,b2,1e-4)
    // (idealgasflow.d:873-911), replicated so the step count is visible.
    {
        double p1 = 100.0e3;
        double T1 = 300.0;
        double R = 287.1;
        foreach (g; gammas) {
            foreach (Msrc; [1.5, 3.0]) {
                double a1 = sqrt(g*R*T1);
                double V1 = Msrc * a1;
                foreach (deg; [5.0, 15.0, 20.0, 30.0]) {
                    double theta = deg * PI/180.0;
                    qc("beta_cone_lib", V1, theta, () => beta_cone(V1, p1, T1, theta, R, g));
                    g_evals = 0;
                    g_g = g; g_V1 = V1; g_p1 = p1; g_T1 = T1; g_R = R; g_target = theta;
                    double mu = asin(1.0/Msrc);
                    double b1 = mu * 1.01;
                    double b2 = 1.05 * b1;
                    int flag = 0;
                    double root = 0.0;
                    try {
                        flag = bracket!cone_f(b1, b2, mu, PI/2);
                        if (flag < 0) throw new Exception("beta_cone: could not bracket");
                        root = solve!cone_f(b1, b2, 1.0e-4);
                    } catch (Exception e) {
                        writefln("beta_cone_repl a=%.17g b=%.17g THROW=%s fe_total=%d flag=%d",
                                 V1, theta, e.msg, g_evals, flag);
                        continue;
                    }
                    writefln("beta_cone_repl a=%.17g b=%.17g v=%.17g fe_total=%d flag=%d",
                             V1, theta, root, g_evals, flag);
                }
                qc("beta_cone2", Msrc, 20.0*PI/180.0, () => beta_cone2(Msrc, 20.0*PI/180.0, R, g));
                qc("beta_cone2_defaultR", Msrc, 20.0*PI/180.0, () => beta_cone2(Msrc, 20.0*PI/180.0));
            }
        }
    }
    // One instrumented beta_cone iteration, row by row: it shows WHERE the two
    // implementations of the same bracket+solve sequence part company. At
    // g=1.2, M1=1.5, theta=5 deg the ninth evaluation lands on a residual of about
    // 1e-15, which is 11 orders below the 1e-4 stopping tolerance the solve() call
    // uses, and its SIGN is what nm/bracketing.d:53 and nm/ridder.d:57 test. A
    // 1 ULP difference in theta_cone therefore reorders the rest of the iteration
    // while still converging on the same beta.
    {
        double p1 = 100.0e3;
        double T1 = 300.0;
        double R = 287.1;
        double g = 1.2;
        double Msrc = 1.5;
        double a1 = sqrt(g*R*T1);
        g_V1 = Msrc * a1;
        g_p1 = p1; g_T1 = T1; g_R = R; g_g = g;
        g_target = 5.0 * PI/180.0;
        double mu = asin(1.0/Msrc);
        double b1 = mu * 1.01;
        double b2 = 1.05 * b1;
        g_evals = 0;   // local numbering, so the C++ replay matches step for step
        writeln("cone_trace bracket:");
        int flag = bracket!trace_cone_f(b1, b2, mu, PI/2);
        writefln("cone_trace flag=%d b1=%.17g b2=%.17g", flag, b1, b2);
        writeln("cone_trace solve:");
        double root = solve!trace_cone_f(b1, b2, 1.0e-4);
        writefln("cone_trace root=%.17g evals=%d", root, g_evals);
    }
    writeln("done");
}

/// the same objective as cone_f, but each evaluation is printed
double trace_cone_f(double beta) {
    ++g_evals;
    double[] r = theta_cone(g_V1, g_p1, g_T1, beta, g_R, g_g);
    double e = r[0] - g_target;
    writefln("cone_trace e=%.17g beta=%.17g f=%.17g", g_evals, beta, e);
    return e;
}
