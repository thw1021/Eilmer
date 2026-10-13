// tests/demo_gasdyn_idealgasflow.cpp — port of gasdyn/idealgasflow_demo.d.
//
// file: idealgasflow_demo.d
// author: Momar Hughes and Peter J.
// date: 5 Apr 2015 initial code
//       2016-11-19 added Taylor-Maccoll demo
//
// stdout must be byte-identical to the D binary's. Baseline:
// baselines/e587b235/probes/gasdyn_idealgasflow_demo.txt, captured with (no
// backslash continuations, because a backslash at the end of a // comment line
// swallows the next line):
//   D=/home/tang/packages/dlang/bin; S=/home/tang/packages/gdtk/src
//   $D/ldc2 -w -g -dip1008 -I$S -od/tmp/gd_probe/obj -of=/tmp/gd_probe/demo_igf
//     $S/gasdyn/idealgasflow_demo.d $S/gasdyn/idealgasflow.d $S/gasdyn/gasflowexception.d
//   /tmp/gd_probe/demo_igf > baselines/e587b235/probes/gasdyn_idealgasflow_demo.txt
// (explicit file list, not `-i`; see probes/gasdyn/probe_idealgasflow.d for why).
// Checked by ctest -R demo_equiv_gasdyn_idealgasflow through harness/compare_text.sh.
//
// Two D spellings worth naming, because they are load-bearing for the text:
//   * idealgasflow_demo.d:75 computes the free-stream sound speed as
//     `sqrt(1.4*287*T1)` -- a hard-coded 1.4 and the rounded gas constant 287, not
//     the R=287.1 default the call below then uses. Kept verbatim; the cone numbers
//     in this demo are therefore not the same flow as the module's unittest 6.
//   * `theta_c*180.0/PI` etc. are `real` (80-bit) expressions in D, printed through
//     writeln's %.6g. Measured (probes/gasdyn/probe_real_print.d rows w6/d6): the
//     6-significant-digit text of the wide value equals that of its double
//     narrowing, so routing through eil::writeln's double renderer is exact here.

#include <cmath>
#include <cstdio>
#include <string>

#include "eil/eil.hpp"
#include "gasdyn/gasflowexception.hpp"
#include "gasdyn/idealgasflow.hpp"

namespace gd = eil::gasdyn;

int main() {
    eil::writeln("Begin gasdynamic demo...");
    double M = 2.4, g = 1.4;
    eil::writefln("Use M=%g and gamma=%g", M, g);

    eil::writeln("Test: T0_T...");
    eil::writeln("...returns T0/T = ", gd::T0_T(M,g));
    eil::writeln("Literature value = 2.152");

    eil::writeln("Test: p0_p...");
    eil::writeln("...returns p0/p = ", gd::p0_p(M,g));
    eil::writeln("Literature value = 14.620");

    eil::writeln("Test: NuFromM...");
    eil::writeln("...returns nu = ", gd::PM1(M,g));
    eil::writeln("Literature value = 0.6413");
    eil::writeln("...provided M=0.8:");
    try {
        gd::PM1(0.8,g);
    } catch (const eil::ExceptionBase& e) {   // D: catch (Exception e)
        (void)e;
        eil::writeln("Caught subsonic value.");
    }

    double nu = 0.6413479572;
    eil::writeln("Test: MFromNu...");
    eil::writeln("...returns M = ", gd::PM2(nu,g));
    eil::writeln("Literature value = 2.4");
    eil::writeln("...provided nu=-0.5:");
    try {
        gd::PM2(-0.5,g);
    } catch (const eil::ExceptionBase& e) {
        (void)e;
        eil::writeln("Caught subsonic value.");
    }

    eil::writeln("Test: MachAngle...");
    eil::writeln("...returns mu = ", gd::MachAngle(M));
    eil::writeln("Literature value = 0.430");
    eil::writeln("...provided M=0.8:");
    try {
        gd::MachAngle(0.8);
    } catch (const eil::ExceptionBase& e) {
        (void)e;
        eil::writeln("Caught subsonic value.");
    }

    M = 2.0;
    eil::writeln("Oblique shock relations may not quite match (data is from chart)...");
    double beta = 44.0*EIL_PI/180.0; double theta = 14.0*EIL_PI/180.0; // from chart, M=2
    eil::writefln("Computed: M1=%g, theta(beta=%g)=%g, beta(theta=%g)=%g",
                  M, beta, gd::theta_obl(M, beta), theta, gd::beta_obl(M, theta));
    eil::writeln("Conditions behind shock:");
    eil::writefln("M2=%g, expected 1.482 (from chart, 14 degree deflection)",
                  gd::M2_obl(M, beta, theta));
    eil::writefln("Computed: T2/T1=%g, p2/p1=%g, r2/r1=%g",
                  gd::T2_T1_obl(M, beta), gd::p2_p1_obl(M, beta), gd::r2_r1_obl(M, beta));
    eil::writeln("Expected: T2/T1=1.249, p2/p1=2.088, r2/r1=1.673",
                 " (approx. normal-shock table M=1.390)");
    eil::writefln("V2/V1=%g, p02/p01=%g", gd::V2_V1_obl(M, beta), gd::p02_p01_obl(M, beta));
    eil::writeln("Expected: V2/V1=0.8304=sin(B)/sin(B-d)*r1/r2");
    eil::writeln("");

    double M1 = 1.5; double p1 = 100.0e3; double T1 = 300.0;
    double R = 287.1; g = 1.4; double rho1 = p1/(R*T1);
    eil::writefln("Taylor-Maccoll cone flow demo with M1=%g", M1);
    eil::writeln("for M1=1.5, beta=49deg, expect theta=20deg from NACA1135.");
    double a1 = eil::sqrt(1.4*287*T1);
    double V1 = M1 * a1;
    beta = 49.0 * EIL_PI/180;
    eil::Array<double> results = gd::theta_cone(V1, p1, T1, beta);
    double theta_c=results[0];  double V_c=results[1];
    double p_c=results[2]; double T_c=results[3];
    eil::writeln("theta_c(deg)=", theta_c*180.0/EIL_PI, " expected 20deg, surface speed V_c=", V_c);
    eil::writeln("surface pressure coefficient=", (p_c - p1)/(0.5*rho1*V1*V1), " expected 0.385");
    eil::writefln("p_c: %g, T_c: %g", p_c, T_c);
    eil::writeln("");
    eil::writeln("Conical shock from cone with half-angle 20deg in M1=", M1);
    beta = gd::beta_cone(V1, p1, T1, 20.0*EIL_PI/180);
    eil::writeln("sigma(deg)=", beta*180/EIL_PI, " expected 49deg");
    eil::writeln("Repeat above test, but call beta_cone2()");
    beta = gd::beta_cone2(M1, 20.0*EIL_PI/180);
    eil::writeln("sigma(deg)=", beta*180/EIL_PI, " expected 49deg");

    eil::writeln("Finished gasdynamic demo.");
    return 0;
} // end main()
