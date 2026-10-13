// tests/test_gasdyn_idealgasflow.cpp
//
// Three things in one binary, because they measure the same translation.
//
//  A. The six `unittest` blocks of gasdyn/idealgasflow.d (lines 97-103, 227-234,
//     334-342, 461-486, 709-737, 937-952), ported assertion-for-assertion and in
//     order. Their tolerance predicate is the substrate's eil::isClose with the
//     default D's std.math.isClose actually uses for (double,double), measured by
//     this wave as rel=1.0e-9 / abs=0.0 (corpus lines tol_*). Each ported assert
//     requires the verdict the *reference* gives, which the corpus records: 25 of
//     the 29 numeric assertions do NOT hold in D either, because the blocks compare
//     a full-precision relation against a 3-4 digit literature value through a
//     1e-9 gate, and `ldc2 -unittest` on the reference aborts at
//     idealgasflow.d:101 ("Total pressure fail"). A port that quietly re-tuned the
//     literals to make its own asserts green would hide that, so every assertion
//     here is paired: the reference verdict at isClose's default, plus the coarse
//     literature agreement (1e-2..1e-4 relative) the block was written to show.
//
//  B. A replay of probes/gasdyn/probe_idealgasflow.d, line for line, diffed
//     against baselines/e587b235/probes/gasdyn_idealgasflow.txt, tallying how many
//     printed quantities are bit-identical and where the worst residual (in ULP)
//     is. The corpus also carries the nm root finders' evaluation counts
//     (fe_bracket/fe_total/flag) for M_Rayleigh, beta_obl and beta_cone, so a
//     bracketing sequence that diverges while landing on the same root still shows.
//
//  C. The consequence of a substrate gap: eil::isClose's own default maxRelDiff is
//     1.0e-8 (substrate/include/eil/math.hpp:72), an order of magnitude looser than
//     Phobos' 1.0e-9 for two doubles, so a bare `eil::isClose(a,b)` port of a bare
//     `isClose(a,b)` silently loosens the gate. Counted and printed below.
//
// Run: ./test_gasdyn_idealgasflow <probes/gasdyn_idealgasflow.txt>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "gasdyn/gasflowexception.hpp"
#include "gasdyn/idealgasflow.hpp"
#include "nm/bracketing.hpp"
#include "nm/number.hpp"
#include "nm/ridder.hpp"

using eil::gasdyn::GasFlowException;

namespace {

// ---------------------------------------------------------------- corpus plumbing
std::vector<std::string> produced;

void emitf(const char* fmt, ...) {
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    produced.emplace_back(buf);
}

// The measured std.math.isClose default for two doubles (corpus tol_* lines;
// std/math/operations.d:1250 + :1470-1477 give 10 ^^ -((double.dig+1)/2 + 1)).
constexpr double D_ISCLOSE_DEF_REL = 1.0e-9;
constexpr double D_ISCLOSE_DEF_ABS = 0.0;

bool d_isClose(double a, double b) {
    return eil::isClose(a, b, static_cast<eil::real>(D_ISCLOSE_DEF_REL),
                        static_cast<eil::real>(D_ISCLOSE_DEF_ABS));
}

const char* bools(bool v) { return v ? "true" : "false"; }

// The probe's q()/qc()/qv()/qvc() writers, spelled the same way.
void q(const char* name, double a, double b, double v) {
    emitf("%s a=%.17g b=%.17g v=%.17g", name, a, b, v);
}

void qc(const char* name, double a, double b, const std::function<double()>& f) {
    try {
        q(name, a, b, f());
    } catch (const eil::ExceptionBase& e) {
        // The probe's clause is `catch (Exception e)`; naming ExceptionBase keeps a
        // ported Error uncatchable, exactly as in D.
        emitf("%s a=%.17g b=%.17g THROW=%s", name, a, b, e.msg().c_str());
    }
}

void qv(const char* name, double a, double b, const std::vector<double>& v) {
    std::string s = eil::format("%s a=%.17g b=%.17g", name, a, b);
    for (double x : v) s += eil::format(" v=%.17g", x);
    produced.push_back(s);
}

void qvc(const char* name, double a, double b,
         const std::function<eil::Array<double>()>& f) {
    try {
        eil::Array<double> r = f();
        std::vector<double> v;
        for (size_t i = 0; i < r.length(); ++i) v.push_back(r[i]);
        qv(name, a, b, v);
    } catch (const eil::ExceptionBase& e) {
        emitf("%s a=%.17g b=%.17g THROW=%s", name, a, b, e.msg().c_str());
    }
}

// The probe's qvc_pair: a nominal row plus the same call with the first argument
// moved up exactly one ULP. That pair MEASURES the reference's own sensitivity to
// the smallest perturbation the port can be out by, and the harness below uses it
// as the acceptance envelope for the conical-flow rows.
void qvc_pair(const char* name, double V1, double beta,
              const std::function<eil::Array<double>(double)>& f) {
    double v1p = std::nextafter(V1, std::numeric_limits<double>::infinity());
    try {
        eil::Array<double> r = f(V1);
        std::vector<double> v;
        for (size_t i = 0; i < r.length(); ++i) v.push_back(r[i]);
        qv(name, V1, beta, v);
    } catch (const eil::ExceptionBase& e) {
        emitf("%s a=%.17g b=%.17g THROW=%s", name, V1, beta, e.msg().c_str());
    }
    try {
        eil::Array<double> r = f(v1p);
        std::vector<double> v;
        for (size_t i = 0; i < r.length(); ++i) v.push_back(r[i]);
        qv((std::string(name) + "_1ulp").c_str(), v1p, beta, v);
    } catch (const eil::ExceptionBase& e) {
        emitf("%s_1ulp a=%.17g b=%.17g THROW=%s", name, v1p, beta, e.msg().c_str());
    }
}

// D prints a `real` (80-bit) argument with %.17g *wide*, not rounded to double
// first: measured, `49.030391992388263` (real) vs `49.03039199238826` (the same
// value stored in a double), and `84.999999999999998` vs `85`. The three u6 lines
// below take an expression whose D type is real (`x*180.0/PI`), so they are
// rendered from a long double to match. See reports/wave-02-brief.md section 5.
void emitf_wide(const char* key, eil::real wide, const char* tail_fmt, ...) {
    char tail[4096];
    va_list ap;
    va_start(ap, tail_fmt);
    std::vsnprintf(tail, sizeof tail, tail_fmt, ap);
    va_end(ap);
    char head[4096];
    std::snprintf(head, sizeof head, "%s", key);
    char body[4200];
    std::snprintf(body, sizeof body, "%.17Lg", wide);
    std::strcat(head, body);
    produced.push_back(std::string(head) + " " + tail);
}

namespace gd = eil::gasdyn;

// Same helper the port uses: D's `asin(double)` is computed in Phobos' 80-bit real
// as atan2(x, sqrt(1-x*x)), so the bracket ends below are that, not std::asin.
double d_asin(double x) {
    const eil::real r = static_cast<eil::real>(x);
    const eil::real s = std::sqrt(static_cast<eil::real>(1.0) - r * r);
    return static_cast<double>(std::atan2(r, s));
}

// The probe's gammas and Mach ladders, in the probe's order.
const std::vector<double> kGammas = {1.2, 1.4, 1.666};
const std::vector<double> kMsup = {1.0000001, 1.001, 1.01, 1.05, 1.1, 1.2, 1.3, 1.4,
                                   1.5, 1.75, 2.0, 2.5, 3.0, 4.0, 5.0, 7.0, 10.0, 20.0};
const std::vector<double> kMsub = {0.2, 0.5, 0.8, 0.99, 0.9999999};
const std::vector<double> kMobs = {1.2, 1.5, 2.0, 3.0, 5.0, 10.0, 20.0};

// `deg * PI / 180.0` as the probe writes it: Phobos' PI is 80-bit, so the quotient
// is formed wide and rounded once, on the store into the double.
double deg2rad(int deg) { return static_cast<double>(static_cast<double>(deg) * EIL_PI / 180.0); }
double deg2rad_d(double deg) { return static_cast<double>(deg * EIL_PI / 180.0); }

// ------------------------------------------------- replica objectives (section B)
// The probe instruments the call sequences the library's own delegates make; these
// are the C++ side of that, with their own counters.
int g_evals = 0;
double g_g = 1.4, g_target = 0.0, g_M1 = 0.0;
double g_V1 = 0.0, g_p1 = 0.0, g_T1 = 0.0, g_R = 287.1;

double rayleigh_f(double m) {
    ++g_evals;
    return gd::T0_T0star(m, g_g) - g_target;
}

double theta_f(double beta) {
    ++g_evals;
    return gd::theta_obl(g_M1, beta, g_g) - g_target;
}

double cone_f(double beta) {
    ++g_evals;
    return gd::theta_cone(g_V1, g_p1, g_T1, beta, g_R, g_g)[0] - g_target;
}

// Used as a non-type template argument by the bracketed cone solve below, so it
// has to be named before that point; the body is with the other objectives.
double trace_cone_f(double beta);

// ------------------------------------------------------------------ section B body
void section_tol_defaults() {
    const double rel_ladder[8] = {1.0e-10, 5.0e-10, 9.0e-10, 1.0e-9, 1.1e-9, 2.0e-9,
                                  1.0e-8, 1.0e-7};
    for (double d : rel_ladder) emitf("tol_rel d=%.17g holds=%s", d, bools(d_isClose(1.0, 1.0 + d)));
    const double rel14[7] = {1.0e-9, 1.0e-8, 1.0e-7, 1.0e-6, 1.0e-5, 1.2e-4, 2.0e-4};
    for (double d : rel14)
        emitf("tol_rel14 d=%.17g holds=%s", d, bools(d_isClose(14.62, 14.62 * (1.0 + d))));
    const double absl[5] = {1.0e-9, 1.0e-8, 1.0e-6, 1.0e-5, 1.0e-4};
    for (double a : absl)
        emitf("tol_abs d=%.17g holds=%s", a, bools(d_isClose(0.0001, 0.0001 + a)));
}

// The six unittest blocks, printed exactly as the probe prints them, so section A's
// assertions and section B's corpus rows are computed from the same calls.
void section_unittest_values() {
    {   // idealgasflow.d:97-103
        double M = 2.4;
        double g = 1.4;
        emitf("u1_T0_T v=%.17g lit=2.152 holds=%s", gd::T0_T(M, g), bools(d_isClose(gd::T0_T(M, g), 2.152)));
        emitf("u1_p0_p v=%.17g lit=14.620 holds=%s", gd::p0_p(M, g), bools(d_isClose(gd::p0_p(M, g), 14.620)));
        emitf("u1_r0_r v=%.17g lit=6.7937 holds=%s", gd::r0_r(M, g), bools(d_isClose(gd::r0_r(M, g), 6.7937)));
    }
    {   // idealgasflow.d:227-234
        double M = 2.0;
        double g = 1.4;
        emitf("u2_m2_shock v=%.17g lit=0.5774 holds=%s", gd::m2_shock(M,g), bools(d_isClose(gd::m2_shock(M,g), 0.5774)));
        emitf("u2_p2_p1 v=%.17g lit=4.50 holds=%s", gd::p2_p1(M,g), bools(d_isClose(gd::p2_p1(M,g), 4.50)));
        emitf("u2_T2_T1 v=%.17g lit=1.687 holds=%s", gd::T2_T1(M,g), bools(d_isClose(gd::T2_T1(M,g), 1.687)));
        emitf("u2_r2_r1 v=%.17g lit=2.667 holds=%s", gd::r2_r1(M,g), bools(d_isClose(gd::r2_r1(M,g), 2.667)));
    }
    {   // idealgasflow.d:334-342
        double M = 2.0;
        double g = 1.4;
        emitf("u3_T0_T0star v=%.17g lit=0.7934 holds=%s", gd::T0_T0star(M,g), bools(d_isClose(gd::T0_T0star(M,g), 0.7934)));
        emitf("u3_T_Tstar v=%.17g lit=0.5289 holds=%s", gd::T_Tstar(M,g), bools(d_isClose(gd::T_Tstar(M,g), 0.5289)));
        emitf("u3_p_pstar v=%.17g lit=0.3636 holds=%s", gd::p_pstar(M,g), bools(d_isClose(gd::p_pstar(M,g), 0.3636)));
        emitf("u3_r_rstar v=%.17g lit=0.6875 holds=%s", gd::r_rstar(M,g), bools(d_isClose(gd::r_rstar(M,g), 0.6875)));
        emitf("u3_M_Rayleigh v=%.17g lit=2 holds=%s", gd::M_Rayleigh(gd::T0_T0star(M,g),g),
              bools(d_isClose(gd::M_Rayleigh(gd::T0_T0star(M,g),g), M)));
    }
    {   // idealgasflow.d:461-486
        double M = 2.4;
        double g = 1.4;
        double nu = 0.6413479572;
        emitf("u4_PM1 v=%.17g lit=0.6413 holds=%s", gd::PM1(M,g), bools(d_isClose(gd::PM1(M,g), 0.6413)));
        emitf("u4_PM2 v=%.17g lit=2.4 holds=%s", gd::PM2(nu,g), bools(d_isClose(gd::PM2(nu,g), 2.4)));
        emitf("u4_MachAngle v=%.17g lit=0.430 holds=%s", gd::MachAngle(M), bools(d_isClose(gd::MachAngle(M), 0.430)));
        try { gd::PM1(0.8,g); } catch (const GasFlowException& e) {
            emitf("u4_PM1_subsonic holds=%s msg=%s", bools(e.toString().find("subsonic") != std::string::npos),
                  e.msg().c_str());
        }
        try { gd::PM2(-0.5,g); } catch (const GasFlowException& e) {
            emitf("u4_PM2_negative holds=%s msg=%s", bools(e.toString().find("negative") != std::string::npos),
                  e.msg().c_str());
        }
        try { gd::MachAngle(0.8); } catch (const GasFlowException& e) {
            emitf("u4_MachAngle_subsonic holds=%s msg=%s", bools(e.toString().find("subsonic") != std::string::npos),
                  e.msg().c_str());
        }
    }
    {   // idealgasflow.d:709-737
        double M = 2.0;
        double g = 1.4;
        double beta = deg2rad(44);
        double theta = deg2rad(14);
        emitf("u5_angles beta=%.17g theta=%.17g", beta, theta);
        emitf("u5_beta_obl v=%.17g lit=%.17g holds=%s", gd::beta_obl(M,theta,g), beta, bools(d_isClose(gd::beta_obl(M,theta,g), beta)));
        emitf("u5_beta_obl2 v=%.17g lit=%.17g holds=%s", gd::beta_obl2(M,2.088,g), beta, bools(d_isClose(gd::beta_obl2(M,2.088,g), beta)));
        emitf("u5_theta_obl v=%.17g lit=%.17g holds=%s", gd::theta_obl(M,beta,g), theta, bools(d_isClose(gd::theta_obl(M,beta,g), theta)));
        emitf("u5_M2_obl v=%.17g lit=1.482 holds=%s", gd::M2_obl(M,beta,theta,g), bools(d_isClose(gd::M2_obl(M,beta,theta,g), 1.482)));
        emitf("u5_T2_T1_obl v=%.17g lit=1.249 holds=%s", gd::T2_T1_obl(M,beta,g), bools(d_isClose(gd::T2_T1_obl(M,beta,g), 1.249)));
        emitf("u5_p2_p1_obl v=%.17g lit=2.088 holds=%s", gd::p2_p1_obl(M,beta,g), bools(d_isClose(gd::p2_p1_obl(M,beta,g), 2.088)));
        emitf("u5_r2_r1_obl v=%.17g lit=1.673 holds=%s", gd::r2_r1_obl(M,beta,g), bools(d_isClose(gd::r2_r1_obl(M,beta,g), 1.673)));
        emitf("u5_p02_p01_obl v=%.17g lit=0.9608 holds=%s", gd::p02_p01_obl(M,beta,g), bools(d_isClose(gd::p02_p01_obl(M,beta,g), 0.9608)));
        emitf("u5_Vn2_Vn1_obl v=%.17g lit=0.598 holds=%s", gd::Vn2_Vn1_obl(M,beta,g), bools(d_isClose(gd::Vn2_Vn1_obl(M,beta,g), 0.598)));
        emitf("u5_V2_V1_obl v=%.17g lit=0.828 holds=%s", gd::V2_V1_obl(M,beta,g), bools(d_isClose(gd::V2_V1_obl(M,beta,g), 0.828)));
        try {
            double bb = gd::beta_obl(M, deg2rad(40), g);
            emitf("u5_detached threw=%s v=%.17g", bools(false), bb);
        } catch (const eil::ExceptionBase& e) {
            emitf("u5_detached threw=%s msg=%s", bools(true), e.msg().c_str());
        }
        try {
            double tt = gd::T2_T1_obl(0.8, beta, g);
            emitf("u5_subsonic_obl threw=%s v=%.17g", bools(false), tt);
        } catch (const eil::ExceptionBase& e) {
            emitf("u5_subsonic_obl threw=%s msg=%s", bools(true), e.msg().c_str());
        }
    }
    {   // idealgasflow.d:937-952
        double M1 = 1.5; double p1 = 100.0e3; double T1 = 300.0;
        double R = 287.1; double g = 1.4; double rho1 = p1/(R*T1);
        double a1 = eil::sqrt(g*R*T1);
        double V1 = M1 * a1;
        double beta = deg2rad(49);
        eil::Array<double> results = gd::theta_cone(V1, p1, T1, beta);
        emitf("u6_theta_cone v=%.17g v=%.17g v=%.17g v=%.17g",
              results[0], results[1], results[2], results[3]);
        // D's expressions here are `real` (they divide by PI), hence the wide print.
        {
            // D: results[0]*180.0 is a double product (ldc emits mulsd), widened
            // only for the division by the 80-bit PI.
            eil::real wide = static_cast<eil::real>(results[0] * 180.0) / EIL_PI;
            emitf_wide("u6_cone_angle v=", wide, "lit=19.96 holds=%s",
                       bools(d_isClose(static_cast<double>(wide), 19.96)));
        }
        emitf("u6_cone_cp v=%.17g lit=0.386 holds=%s", (results[2]-p1)/(0.5*rho1*V1*V1),
              bools(d_isClose((results[2]-p1)/(0.5*rho1*V1*V1), 0.386)));
        {
            eil::real wide = static_cast<eil::real>(
                gd::beta_cone(V1,p1,T1,deg2rad_d(20.0)) * 180.0) / EIL_PI;
            emitf_wide("u6_beta_cone v=", wide, "lit=49 holds=%s",
                       bools(d_isClose(static_cast<double>(wide), 49.0)));
        }
        {
            eil::real wide = static_cast<eil::real>(
                gd::beta_cone2(M1,deg2rad_d(20.0)) * 180.0) / EIL_PI;
            emitf_wide("u6_beta_cone2 v=", wide, "lit=49 holds=%s",
                       bools(d_isClose(static_cast<double>(wide), 49.0)));
        }
    }
}

void section_sweeps() {
    for (double g : kGammas) {
        std::vector<double> mall = kMsub;
        for (double M : kMsup) mall.push_back(M);
        for (double M : mall) {
            q("A_Astar", g, M, gd::A_Astar(M, g));
            q("T0_T", g, M, gd::T0_T(M, g));
            q("p0_p", g, M, gd::p0_p(M, g));
            q("r0_r", g, M, gd::r0_r(M, g));
            q("T0_T0star", g, M, gd::T0_T0star(M, g));
            q("T_Tstar", g, M, gd::T_Tstar(M, g));
            q("p_pstar", g, M, gd::p_pstar(M, g));
            q("r_rstar", g, M, gd::r_rstar(M, g));
            q("p0_p0star", g, M, gd::p0_p0star(M, g));
            q("pitot_p", g, M, gd::pitot_p(101325.0, M, g));
        }
        const double mguard[4] = {0.0, 0.3, 0.8, 0.9999};
        for (double M : mguard) {
            qc("m2_shock", g, M, [&]{ return gd::m2_shock(M, g); });
            qc("r2_r1", g, M, [&]{ return gd::r2_r1(M, g); });
            qc("u2_u1", g, M, [&]{ return gd::u2_u1(M, g); });
            qc("p2_p1", g, M, [&]{ return gd::p2_p1(M, g); });
            qc("T2_T1", g, M, [&]{ return gd::T2_T1(M, g); });
            qc("p02_p01", g, M, [&]{ return gd::p02_p01(M, g); });
            qc("ds_Cv", g, M, [&]{ return gd::ds_Cv(M, g); });
            qc("PM1", g, M, [&]{ return gd::PM1(M, g); });
            qc("MachAngle", g, M, [&]{ return gd::MachAngle(M); });
        }
        for (double M : kMsup) {
            q("m2_shock", g, M, gd::m2_shock(M, g));
            q("r2_r1", g, M, gd::r2_r1(M, g));
            q("u2_u1", g, M, gd::u2_u1(M, g));
            q("p2_p1", g, M, gd::p2_p1(M, g));
            q("T2_T1", g, M, gd::T2_T1(M, g));
            q("p02_p01", g, M, gd::p02_p01(M, g));
            q("ds_Cv", g, M, gd::ds_Cv(M, g));
            q("PM1", g, M, gd::PM1(M, g));
            q("MachAngle", g, M, gd::MachAngle(M));
        }
    }
}

void section_oblique() {
    for (double g : kGammas) {
        for (double M : kMobs) {
            for (int deg = 1; deg <= 89; deg += 2) {
                double beta = deg2rad(deg);
                qc("theta_obl", M, beta, [&]{ return gd::theta_obl(M, beta, g); });
                q("dtan_theta", M, beta, gd::dtan_theta(M, beta, g));
                qc("r2_r1_obl", M, beta, [&]{ return gd::r2_r1_obl(M, beta, g); });
                qc("p2_p1_obl", M, beta, [&]{ return gd::p2_p1_obl(M, beta, g); });
                qc("T2_T1_obl", M, beta, [&]{ return gd::T2_T1_obl(M, beta, g); });
                qc("p02_p01_obl", M, beta, [&]{ return gd::p02_p01_obl(M, beta, g); });
                qc("Vn2_Vn1_obl", M, beta, [&]{ return gd::Vn2_Vn1_obl(M, beta, g); });
                qc("V2_V1_obl", M, beta, [&]{ return gd::V2_V1_obl(M, beta, g); });
                qc("M2_obl", M, beta, [&]{ return gd::M2_obl(M, beta, gd::theta_obl(M, beta, g), g); });
            }
            qc("beta_obl_zero", M, 0.0, [&]{ return gd::beta_obl(M, 0.0, g); });
            qc("theta_obl_neg", M, deg2rad(40), [&]{ return gd::theta_obl(M, -deg2rad(40), g); });
            const double tm_beta[3] = {deg2rad(40), deg2rad(70), deg2rad(89)};
            for (double beta : tm_beta) {
                // D passes a `double[]` literal state vector [rho, V_r, V_theta, h, p].
                // The array must outlive the call, so it is a named local, not a
                // temporary inside the lambda the probe writes.
                double z[5] = {1.2e0, 300.0, -100.0, 1.0e6, 1.0e5};
                qvc("taylor_maccoll_odes", M, beta,
                    [&]{ return gd::taylor_maccoll_odes(
                             eil::Slice<double>(z, 5, nullptr, 0), beta, g); });
            }
        }
    }
}

void section_inverses() {
    // M_Rayleigh (bracket(1,2) + solve(1,2)) and its instrumented replica.
    for (double g : kGammas) {
        const double msrc[8] = {1.05, 1.2, 1.5, 2.0, 3.0, 5.0, 8.0, 15.0};
        for (double Msrc : msrc) {
            double target = gd::T0_T0star(Msrc, g);
            double lib = gd::M_Rayleigh(target, g);
            g_evals = 0;
            g_g = g;
            g_target = target;
            double b1 = 1.0, b2 = 2.0;
            int flag = eil::nm::bracket<rayleigh_f, double>(b1, b2);
            int fe_bracket = g_evals;
            double root = eil::nm::ridder::solve<rayleigh_f, double>(b1, b2);
            emitf("M_Rayleigh g=%.17g in=%.17g lib=%.17g repl=%.17g flag=%d fe_bracket=%d fe_total=%d b1=%.17g b2=%.17g",
                  g, target, lib, root, flag, fe_bracket, g_evals, b1, b2);
        }
    }
    // PM2: the secant loop seeded by MFromNu_approximate.
    for (double g : kGammas) {
        const double nuok[15] = {0.0, 1.0e-3, 0.01, 0.05, 0.1, 0.2, 0.37, 0.6413479572,
                                 0.9, 1.2, 1.5, 1.8, 2.0, 2.2, 2.4};
        for (double nu : nuok) {
            q("MFromNu_approximate", g, nu, gd::MFromNu_approximate(nu, g));
            qc("PM2", g, nu, [&]{ return gd::PM2(nu, g); });
            qc("PM2_resid", g, nu, [&]{ return nu - gd::PM1(gd::PM2(nu, g), g); });
        }
        const double nubad[6] = {-1.0e-3, -0.5, -1.0, 3.0, 3.5, 4.0};
        for (double nu : nubad) {
            q("MFromNu_approximate", g, nu, gd::MFromNu_approximate(nu, g));
            qc("PM2", g, nu, [&]{ return gd::PM2(nu, g); });
        }
    }
    // beta_obl, with the instrumented replica of its bracket/solve sequence.
    for (double g : kGammas) {
        for (double M : kMobs) {
            for (int deg = 1; deg <= 45; deg += 2) {
                double theta = deg2rad(deg);
                qc("beta_obl", M, theta, [&]{ return gd::beta_obl(M, theta, g); });
                qc("beta_obl_neg", M, theta, [&]{ return gd::beta_obl(M, -theta, g); });
                g_evals = 0;
                g_g = g;
                g_M1 = M;
                g_target = theta;
                int flag = 0;
                double root = 0.0;
                double b1 = d_asin(1.0/M);
                double b2 = b1 * 1.05;
                int took_branch = 0;
                std::string throw_msg;
                const double tol = 1.0e-6;        // beta_obl's own default
                try {
                    if (theta < tol) {
                        root = b1; took_branch = 1;
                    } else {
                        double f1 = theta_f(b1);
                        if (std::fabs(f1) < tol) {
                            root = b1; took_branch = 1;
                        } else {
                            double f2 = theta_f(b2);
                            if (std::fabs(f2) < tol) {
                                root = b2; took_branch = 1;
                            } else {
                                flag = eil::nm::bracket<theta_f, double>(b1, b2);
                                if (flag < 0) {
                                    throw eil::ExceptionBase(eil::text(
                                        "beta_obl: could not bracket beta for M1=", M,
                                        " theta=", theta), __FILE__, __LINE__);
                                }
                                root = eil::nm::ridder::solve<theta_f, double>(b1, b2);
                            }
                        }
                    }
                } catch (const eil::ExceptionBase& e) {
                    throw_msg = e.msg();
                }
                if (!throw_msg.empty()) {
                    emitf("beta_obl_repl a=%.17g b=%.17g THROW=%s fe_total=%d flag=%d",
                          M, theta, throw_msg.c_str(), g_evals, flag);
                } else {
                    emitf("beta_obl_repl a=%.17g b=%.17g v=%.17g fe_total=%d flag=%d branch=%d b1=%.17g b2=%.17g",
                          M, theta, root, g_evals, flag, took_branch, b1, b2);
                }
            }
            const double pp[7] = {0.5, 1.0, 1.5, 2.088, 5.0, 20.0, 60.0};
            for (double r : pp) qc("beta_obl2", M, r, [&]{ return gd::beta_obl2(M, r, g); });
        }
    }
    // theta_cone over the shock angle, then beta_cone/beta_cone2 inverting it.
    {
        double p1 = 100.0e3;
        double T1 = 300.0;
        double R = 287.1;
        for (double g : kGammas) {
            const double msrc[4] = {1.2, 1.5, 2.0, 3.0};
            for (double Msrc : msrc) {
                double a1 = eil::sqrt(g*R*T1);
                double V1 = Msrc * a1;
                double mu = d_asin(1.0/Msrc);
                const double frac[8] = {0.9, 1.0, 1.005, 1.02, 1.1, 1.5, 2.0, 3.0};
                for (double f : frac) {
                    double beta = mu * f;
                    // D's guard, compared in extended precision exactly as Phobos
                    // does it: the double nearest PI/2 is *below* the 80-bit PI/2,
                    // so the M1=2, frac=3 row is kept rather than skipped.
                    if (static_cast<eil::real>(beta) >= EIL_PI / 2.0) continue;
                    qvc_pair("theta_cone", V1, beta,
                             [&](double v){ return gd::theta_cone(v, p1, T1, beta, R, g); });
                }
                const int degs[9] = {5, 15, 25, 35, 45, 55, 65, 75, 85};
                for (int d : degs) {
                    double beta = deg2rad(d);
                    qvc_pair("theta_cone", V1, beta,
                             [&](double v){ return gd::theta_cone(v, p1, T1, beta, R, g); });
                }
            }
        }
        for (double g : kGammas) {
            const double msrc[2] = {1.5, 3.0};
            for (double Msrc : msrc) {
                double a1 = eil::sqrt(g*R*T1);
                double V1 = Msrc * a1;
                const double thetas[4] = {5.0, 15.0, 20.0, 30.0};
                for (double td : thetas) {
                    double theta = deg2rad_d(td);
                    qc("beta_cone_lib", V1, theta, [&]{ return gd::beta_cone(V1, p1, T1, theta, R, g); });
                    g_evals = 0;
                    g_g = g; g_V1 = V1; g_p1 = p1; g_T1 = T1; g_R = R; g_target = theta;
                    double mu = d_asin(1.0/Msrc);
                    double b1 = mu * 1.01;
                    double b2 = 1.05 * b1;
                    int flag = 0;
                    double root = 0.0;
                    std::string throw_msg;
                    try {
                        flag = eil::nm::bracket<cone_f, double>(b1, b2, mu,
                                                                static_cast<double>(EIL_PI / 2.0));
                        if (flag < 0) throw eil::ExceptionBase("beta_cone: could not bracket",
                                                               __FILE__, __LINE__);
                        root = eil::nm::ridder::solve<cone_f, double>(b1, b2, 1.0e-4);
                    } catch (const eil::ExceptionBase& e) {
                        throw_msg = e.msg();
                    }
                    if (!throw_msg.empty()) {
                        emitf("beta_cone_repl a=%.17g b=%.17g THROW=%s fe_total=%d flag=%d",
                              V1, theta, throw_msg.c_str(), g_evals, flag);
                    } else {
                        emitf("beta_cone_repl a=%.17g b=%.17g v=%.17g fe_total=%d flag=%d",
                              V1, theta, root, g_evals, flag);
                    }
                }
                qc("beta_cone2", Msrc, deg2rad_d(20.0),
                   [&]{ return gd::beta_cone2(Msrc, deg2rad_d(20.0), R, g); });
                qc("beta_cone2_defaultR", Msrc, deg2rad_d(20.0),
                   [&]{ return gd::beta_cone2(Msrc, deg2rad_d(20.0)); });
            }
        }
    }
    // The same instrumented beta_cone iteration the probe traces: one line per
    // objective evaluation, so the corpus shows exactly where the two bracket/solve
    // sequences part company while still converging on the same beta.
    {
        double p1 = 100.0e3;
        double T1 = 300.0;
        double R = 287.1;
        double g = 1.2;
        double Msrc = 1.5;
        double a1 = eil::sqrt(g*R*T1);
        g_V1 = Msrc * a1;
        g_p1 = p1; g_T1 = T1; g_R = R; g_g = g;
        g_target = deg2rad_d(5.0);
        double mu = d_asin(1.0/Msrc);
        double b1 = mu * 1.01;
        double b2 = 1.05 * b1;
        g_evals = 0;      // local numbering, as in the probe
        produced.push_back("cone_trace bracket:");
        int flag = eil::nm::bracket<trace_cone_f, double>(b1, b2, mu,
                                                         static_cast<double>(EIL_PI / 2.0));
        emitf("cone_trace flag=%d b1=%.17g b2=%.17g", flag, b1, b2);
        produced.push_back("cone_trace solve:");
        double root = eil::nm::ridder::solve<trace_cone_f, double>(b1, b2, 1.0e-4);
        emitf("cone_trace root=%.17g evals=%d", root, g_evals);
    }
    produced.push_back("done");
}

/// the same objective as cone_f, but each evaluation is printed (probe mirror)
double trace_cone_f(double beta) {
    ++g_evals;
    double e = gd::theta_cone(g_V1, g_p1, g_T1, beta, g_R, g_g)[0] - g_target;
    // D writes `%.17g` here and std.format renders an integer argument as a plain
    // integer (substrate rule, measured in tools/fmt_corpus.txt). In C that same
    // conversion specifier applied to an int is undefined behaviour, so the port
    // spells the field `%d` to produce the identical text.
    emitf("cone_trace e=%d beta=%.17g f=%.17g", g_evals, beta, e);
    return e;
}

// ------------------------------------------------------------- the diff + tally
//
// Acceptance is per quantity, and every threshold is a D-side number rather than a
// choice made here:
//
//   STRICT      everything computable in closed form (the isentropic, normal-shock,
//               Rayleigh, Prandtl-Meyer, oblique relations and the ODE system).
//               Bit-identical, else relative <= 1.0e-13, which is the numeric
//               criterion in reports/wave-02-brief.md section 6. Phobos' and
//               glibc's sin/cos/atan/pow differ by 1 ULP, so a few hundred of the
//               41k printed quantities land there.
//   SOLVE_TOL   quantities produced by nm/ridder::solve. Its loop stops when
//               |x2-x1|/(1+|x1|+|x2|) <= tol, with tol = 1.0e-9 for M_Rayleigh and
//               beta_obl (nm/ridder.hpp:58, the default the D call leaves in place)
//               and tol = 1.0e-4 for beta_cone (idealgasflow.d:910). A port whose
//               objective is off by 1 ULP stops a step earlier or later and is then
//               free to differ by that same tol, so the band here is 2*tol in the
//               interval metric. Wider than that is a real divergence.
//   SECANT      PM2 iterates while |nu - PM1(M)| > tol with tol = 1.0e-6
//               (idealgasflow.d:380, 407): the residual rows are gated at 1.0e-6
//               and the returned Mach number at 1.0e-5.
//   ODE         theta_cone rows. The band is 4x the row's own measured sensitivity:
//               the corpus pairs every nominal row with the same call at V1 + 1 ULP,
//               so |nominal - perturbed| says what one ULP of input is worth in that
//               thousand-step Euler march (measured max: 8.6e-14 relative). A port
//               inside that band is indistinguishable from the reference itself
//               being one bit off.
struct Tally {
    long long quantities = 0;
    long long identical = 0;
    long long within_gate = 0;
    long long outside_gate = 0;
    long long text_tokens = 0;
    long long text_differ = 0;
    long long lines_compared = 0;
    long long lines_differ = 0;
    long long counts_compared = 0;
    long long counts_equal = 0;
    long long worst_ulp = 0;              // worst strict residual, not the global max
    double worst_rel = 0.0;
    std::string worst_key;
    std::string worst_vals;
    double worst_gate_ratio = 0.0;        // max(|d| / band) over every quantity
    std::string worst_gate_key;
    std::string worst_gate_vals;
    std::vector<std::string> bad;
};

bool parse_number(const std::string& tok, std::string& key, double& val) {
    std::string body = tok;
    key.clear();
    size_t eq = tok.find('=');
    if (eq != std::string::npos) {
        key = tok.substr(0, eq);
        body = tok.substr(eq + 1);
    }
    if (body.empty() || !(std::isdigit(static_cast<unsigned char>(body[0])) ||
                         body[0] == '-' || body[0] == '+' || body[0] == '.')) {
        return false;
    }
    char* end = nullptr;
    val = std::strtod(body.c_str(), &end);
    if (end == body.c_str() || *end != '\0') return false;
    if (key.empty()) key = tok;
    return true;
}

// ULP distance between two finite doubles, sign-aware (monotone bit ordering).
long long ulp_distance(double a, double b) {
    if (a == b) return 0;
    if (!std::isfinite(a) || !std::isfinite(b)) return -1;
    long long ia, ib;
    std::memcpy(&ia, &a, sizeof ia);
    std::memcpy(&ib, &b, sizeof ib);
    if (ia < 0) ia = (long long)(0x8000000000000000LL) - ia;
    if (ib < 0) ib = (long long)(0x8000000000000000LL) - ib;
    return std::llabs(ia - ib);
}

// The iteration counters, reported separately from the numbers they produced.
bool is_count_key(const std::string& k) {
    // `evals` is the cone_trace step count. It is a count, not a value: D and the
    // port reach the identical 17-digit root but in 13 and 11 evaluations, because
    // the residual at the seventh step sits at theta_cone's 1e-15 noise floor and a
    // 1-ULP libm difference flips its sign (see the note in main()). Counts are
    // reported, not gated.
    return k == "fe_total" || k == "fe_bracket" || k == "flag" || k == "branch" ||
           k == "evals";
}

enum class Gate { STRICT, SOLVE_1E9, SOLVE_1E4, SECANT_TOL, SECANT_RESID, ODE };

Gate gate_of(const std::string& name, const std::string& key) {
    // Only the roots themselves get an iteration band: the inputs (`g`, `in`,
    // `a`, `b`, `theta`), the bracket ends (`b1`, `b2`) and the literature
    // literals are values D hands over unchanged, so they must be bit-identical.
    const bool root = (key == "v" || key == "lib" || key == "repl");
    if (name == "theta_cone" || name == "theta_cone_1ulp" || name == "u6_theta_cone") {
        return root ? Gate::ODE : Gate::STRICT;
    }
    if (root && (name == "beta_obl" || name == "beta_obl_neg" || name == "beta_obl_repl" ||
                 name == "M_Rayleigh" || name == "u3_M_Rayleigh" || name == "u5_beta_obl")) {
        return Gate::SOLVE_1E9;
    }
    if (root && (name == "beta_cone_lib" || name == "beta_cone_repl" || name == "beta_cone2" ||
                 name == "beta_cone2_defaultR" || name == "u6_beta_cone" ||
                 name == "u6_beta_cone2")) {
        return Gate::SOLVE_1E4;
    }
    if (root && (name == "PM2" || name == "u4_PM2")) return Gate::SECANT_TOL;
    if (root && name == "PM2_resid") return Gate::SECANT_RESID;
    return Gate::STRICT;
}

// Absolute acceptance band for one quantity, in that quantity's own units.
double tolerance(Gate g, double ref, double envelope) {
    const double scale = std::fmax(1.0, std::fabs(ref));
    switch (g) {
      case Gate::SOLVE_1E9:    return 2.0e-9 * scale;
      case Gate::SOLVE_1E4:    return 2.0e-4 * scale;
      case Gate::SECANT_TOL:   return 1.0e-5 * scale;
      case Gate::SECANT_RESID: return 1.0e-6;
      case Gate::ODE:          return std::fmax(4.0 * envelope, 1.0e-13 * scale);
      case Gate::STRICT:
      default:                 return 1.0e-13 * scale;
    }
}

// One-ULP envelopes read back out of the D capture: theta_cone rows come in
// (nominal, perturbed) pairs, so the reference itself says what a ULP costs. Both
// lines of a pair get the same per-field vector.
std::map<std::string, std::vector<double>> build_envelopes(
    const std::vector<std::string>& expected) {
    std::map<std::string, std::vector<double>> env;
    auto values = [](const std::string& line, std::vector<double>& out) {
        std::istringstream ls(line);
        std::string tok;
        while (ls >> tok) {
            if (tok.rfind("v=", 0) != 0) continue;
            char* end = nullptr;
            double d = std::strtod(tok.c_str() + 2, &end);
            if (end != tok.c_str() + 2 && *end == '\0') out.push_back(d);
        }
    };
    // Key: the " b=<beta>" field, which the nominal and the perturbed row share.
    for (size_t i = 0; i + 1 < expected.size(); ++i) {
        if (expected[i].rfind("theta_cone a=", 0) != 0) continue;
        if (expected[i + 1].rfind("theta_cone_1ulp a=", 0) != 0) continue;
        std::vector<double> nom, pert;
        values(expected[i], nom);
        values(expected[i + 1], pert);
        if (nom.size() != pert.size()) continue;
        std::vector<double> e;
        for (size_t k = 0; k < nom.size(); ++k) e.push_back(std::fabs(nom[k] - pert[k]));
        // Both lines of the pair share the envelope, keyed by their common b= value.
        size_t a = expected[i].find(" b=");
        size_t sp = expected[i].find(' ', a + 3);
        std::string key = expected[i].substr(a, sp == std::string::npos ? 0 : sp - a);
        env[key] = e;
    }
    return env;
}

void compare(const std::vector<std::string>& expected, Tally& t) {
    std::map<std::string, std::vector<double>> env = build_envelopes(expected);
    const size_t n = std::max(expected.size(), produced.size());
    for (size_t i = 0; i < n; ++i) {
        if (i >= expected.size() || i >= produced.size()) {
            ++t.lines_differ;
            if (t.bad.size() < 12) t.bad.push_back("line has no counterpart on both sides");
            continue;
        }
        ++t.lines_compared;
        const std::string name = expected[i].substr(0, expected[i].find(' '));
        // This row's one-ULP envelope, if it is a theta_cone row.
        std::vector<double> row_env;
        if (name == "theta_cone") {
            size_t a = expected[i].find(" b=");
            size_t sp = expected[i].find(' ', a + 3);
            auto it = env.find(expected[i].substr(a, sp == std::string::npos ? 0 : sp - a));
            if (it != env.end()) row_env = it->second;
        }
        std::istringstream le(expected[i]), lp(produced[i]);
        std::string te, tp;
        size_t vfield = 0;
        bool extra = false;
        while (le >> te) {
            if (!(lp >> tp)) { extra = true; break; }
            double ve = 0.0, vp = 0.0;
            std::string ke, kp;
            bool ne = parse_number(te, ke, ve);
            bool np = parse_number(tp, kp, vp);
            if (ne && np && ke == kp && !is_count_key(ke)) {
                ++t.quantities;
                bool both_nan = std::isnan(ve) && std::isnan(vp);
                if (te == tp || both_nan) {
                    ++t.identical;
                } else {
                    double e = (name == "theta_cone" && vfield < row_env.size())
                                   ? row_env[vfield] : 0.0;
                    const Gate g = gate_of(name, ke);
                    const double band = tolerance(g, ve, e);
                    const double diff = std::isfinite(ve) && std::isfinite(vp)
                                            ? std::fabs(vp - ve)
                                            : std::numeric_limits<double>::infinity();
                    const double ratio = diff / band;
                    if (ratio > t.worst_gate_ratio) {
                        t.worst_gate_ratio = ratio;
                        t.worst_gate_key = eil::text(name, ".", ke);
                        t.worst_gate_vals = eil::text("D=", ve, " C++=", vp, " band=", band);
                    }
                    if (ratio <= 1.0) {
                        ++t.within_gate;
                    } else {
                        ++t.outside_gate;
                        ++t.lines_differ;
                        if (t.bad.size() < 12) {
                            t.bad.push_back(eil::text("line ", i + 1, " [", te, " vs ", tp,
                                                      "] band=", band));
                        }
                    }
                    const long long u = ulp_distance(ve, vp);
                    if (g == Gate::STRICT && u > t.worst_ulp) {
                        t.worst_ulp = u;
                        t.worst_rel = ve != 0 ? diff / std::fabs(ve) : diff;
                        t.worst_key = eil::text(name, ".", ke);
                        t.worst_vals = eil::text("D=", ve, " C++=", vp);
                    }
                }
                if (ke == "v") ++vfield;
            } else if (ne && np && ke == kp) {
                ++t.counts_compared;
                if (ve == vp) ++t.counts_equal;
                else if (t.bad.size() < 12) {
                    // Recorded, not fatal: see the note above the report below.
                }
            } else {
                ++t.text_tokens;
                if (te != tp) {
                    ++t.text_differ;
                    ++t.lines_differ;
                    if (t.bad.size() < 12) {
                        t.bad.push_back(eil::text("line ", i + 1, " [", te, " vs ", tp, "]"));
                    }
                }
            }
        }
        if (extra || (lp >> tp)) ++t.lines_differ;
    }
}

// --------------------------------------------------------- section A, the ported
// asserts. `check` counts, `CHECK` is the D assert one-for-one.
int checks = 0;
int failures = 0;

void check(bool cond, const char* what, int line) {
    ++checks;
    if (!cond) {
        ++failures;
        std::printf("FAIL line %d: %s\n", line, what);
    }
}

#define CHECK(c, msg) check((c), msg, __LINE__)

void ported_unittests() {
    // ---- unittest 1, idealgasflow.d:97-103
    {
        double M = 2.4;
        double g = 1.4;
        CHECK(d_isClose(gd::T0_T(M,g), 2.152), "Total temperature fail");
        CHECK(!d_isClose(gd::p0_p(M,g), 14.620), "Total pressure fail: reference does not hold it");
        CHECK(eil::nm::approxEqualNumbers(gd::p0_p(M,g), 14.620), "p0_p: agrees with the literature value inside nm/number.d's own gate");
        CHECK(!d_isClose(gd::r0_r(M,g), 6.7937), "Total density fail: reference does not hold it");
        CHECK(eil::nm::approxEqualNumbers(gd::r0_r(M,g), 6.7937), "r0_r: agrees with the literature value inside nm/number.d's own gate");
    }
    // ---- unittest 2, idealgasflow.d:227-234
    {
        double M = 2.0;
        double g = 1.4;
        CHECK(!d_isClose(gd::m2_shock(M,g), 0.5774), "Mach number after shock: reference does not hold it");
        CHECK(eil::nm::approxEqualNumbers(gd::m2_shock(M,g), 0.5774), "m2_shock: agrees with the literature value inside nm/number.d's own gate");
        CHECK(d_isClose(gd::p2_p1(M,g), 4.50), "Pressure ratio across shock fail");
        CHECK(!d_isClose(gd::T2_T1(M,g), 1.687), "Temperature ratio across shock: reference does not hold it");
        CHECK(eil::nm::approxEqualNumbers(gd::T2_T1(M,g), 1.687), "T2_T1: agrees with the literature value inside nm/number.d's own gate");
        CHECK(!d_isClose(gd::r2_r1(M,g), 2.667), "Density ratio across shock: reference does not hold it");
        CHECK(eil::nm::approxEqualNumbers(gd::r2_r1(M,g), 2.667), "r2_r1: agrees with the literature value inside nm/number.d's own gate");
    }
    // ---- unittest 3, idealgasflow.d:334-342
    {
        double M = 2.0;
        double g = 1.4;
        CHECK(!d_isClose(gd::T0_T0star(M,g), 0.7934), "Rayleigh T0_T0star: reference does not hold it");
        CHECK(eil::nm::approxEqualNumbers(gd::T0_T0star(M,g), 0.7934), "T0_T0star: agrees with the literature value inside nm/number.d's own gate");
        CHECK(!d_isClose(gd::T_Tstar(M,g), 0.5289), "Rayleigh T_Tstar: reference does not hold it");
        CHECK(eil::nm::approxEqualNumbers(gd::T_Tstar(M,g), 0.5289), "T_Tstar: agrees with the literature value inside nm/number.d's own gate");
        CHECK(!d_isClose(gd::p_pstar(M,g), 0.3636), "Rayleigh p_pstar: reference does not hold it");
        CHECK(eil::nm::approxEqualNumbers(gd::p_pstar(M,g), 0.3636), "p_pstar: agrees with the literature value inside nm/number.d's own gate");
        CHECK(d_isClose(gd::r_rstar(M,g), 0.6875), "Rayleigh-line static p_pstar fail");
        // The inverse through nm bracket()+solve() holds at the reference's default.
        CHECK(d_isClose(gd::M_Rayleigh(gd::T0_T0star(M,g),g), M), "Rayleigh-line inverse fail");
    }
    // ---- unittest 4, idealgasflow.d:461-486
    {
        double M = 2.4;
        double g = 1.4;
        CHECK(!d_isClose(gd::PM1(M,g), 0.6413), "Prandtl-Meyer: reference does not hold it");
        CHECK(eil::nm::approxEqualNumbers(gd::PM1(M,g), 0.6413), "PM1: agrees with the literature value inside nm/number.d's own gate");
        bool found_subsonic = false;
        try {
            gd::PM1(0.8,g);
        } catch (const GasFlowException& e) {          // D: catch (GasFlowException e)
            found_subsonic = (e.toString().find("subsonic") != std::string::npos);
        }
        CHECK(found_subsonic, "Prandtl-Meyer failed to catch subsonic M");
        double nu = 0.6413479572;
        CHECK(!d_isClose(gd::PM2(nu,g), 2.4), "Inverse Prandtl-Meyer: reference does not hold it");
        CHECK(eil::nm::approxEqualNumbers(gd::PM2(nu,g), 2.4), "PM2 lands within its own tol of 2.4: agrees with the literature value inside nm/number.d's own gate");
        bool found_negative = false;
        try {
            gd::PM2(-0.5,g);
        } catch (const GasFlowException& e) {
            found_negative = (e.toString().find("negative") != std::string::npos);
        }
        CHECK(found_negative, "Prandtl-Meyer failed to catch negative nu");
        CHECK(!d_isClose(gd::MachAngle(M), 0.430), "Mach angle: reference does not hold it");
        CHECK(eil::nm::approxEqualNumbers(gd::MachAngle(M), 0.430), "MachAngle: agrees with the literature value inside nm/number.d's own gate");
        bool found_subsonic2 = false;
        try {
            gd::MachAngle(0.8);
        } catch (const GasFlowException& e) {
            found_subsonic2 = (e.toString().find("subsonic") != std::string::npos);
        }
        CHECK(found_subsonic2, "Mach angle failed to catch subsonic M");
    }
    // ---- unittest 5, idealgasflow.d:709-737
    {
        double M = 2.0;
        double g = 1.4;
        double beta = deg2rad(44);
        double theta = deg2rad(14);
        CHECK(!d_isClose(gd::beta_obl(M, theta, g), beta), "beta from theta: reference does not hold it");
        CHECK(eil::nm::approxEqualNumbers(gd::beta_obl(M, theta, g), beta), "beta_obl: agrees with the chart value inside nm/number.d's own gate");
        CHECK(!d_isClose(gd::beta_obl2(M, 2.088, g), beta), "beta from p2_p1: reference does not hold it");
        CHECK(eil::nm::approxEqualNumbers(gd::beta_obl2(M, 2.088, g), beta), "beta_obl2: agrees with the chart value inside nm/number.d's own gate");
        CHECK(!d_isClose(gd::theta_obl(M, beta, g), theta), "theta from beta: reference does not hold it");
        CHECK(eil::nm::approxEqualNumbers(gd::theta_obl(M, beta, g), theta), "theta_obl: agrees with the chart value inside nm/number.d's own gate");
        CHECK(!d_isClose(gd::M2_obl(M, beta, theta, g), 1.482), "M2 after shock: reference does not hold it");
        CHECK(eil::nm::approxEqualNumbers(gd::M2_obl(M, beta, theta, g), 1.482), "M2_obl: agrees with the literature value inside nm/number.d's own gate");
        CHECK(!d_isClose(gd::T2_T1_obl(M, beta, g), 1.249), "temperature ratio: reference does not hold it");
        CHECK(eil::nm::approxEqualNumbers(gd::T2_T1_obl(M, beta, g), 1.249), "T2_T1_obl: agrees with the literature value inside nm/number.d's own gate");
        CHECK(!d_isClose(gd::p2_p1_obl(M, beta, g), 2.088), "pressure ratio: reference does not hold it");
        CHECK(eil::nm::approxEqualNumbers(gd::p2_p1_obl(M, beta, g), 2.088), "p2_p1_obl: agrees with the literature value inside nm/number.d's own gate");
        CHECK(!d_isClose(gd::r2_r1_obl(M, beta, g), 1.673), "density ratio: reference does not hold it");
        CHECK(eil::nm::approxEqualNumbers(gd::r2_r1_obl(M, beta, g), 1.673), "r2_r1_obl: agrees with the literature value inside nm/number.d's own gate");
        CHECK(!d_isClose(gd::p02_p01_obl(M, beta, g), 0.9608), "total-pressure: reference does not hold it");
        CHECK(eil::nm::approxEqualNumbers(gd::p02_p01_obl(M, beta, g), 0.9608), "p02_p01_obl: agrees with the literature value inside nm/number.d's own gate");
        CHECK(!d_isClose(gd::Vn2_Vn1_obl(M, beta, g), 0.598), "normal velocity ratio: reference does not hold it");
        CHECK(eil::nm::approxEqualNumbers(gd::Vn2_Vn1_obl(M, beta, g), 0.598), "Vn2_Vn1_obl: agrees with the literature value inside nm/number.d's own gate");
        CHECK(!d_isClose(gd::V2_V1_obl(M, beta, g), 0.828), "absolute velocity ratio: reference does not hold it");
        CHECK(eil::nm::approxEqualNumbers(gd::V2_V1_obl(M, beta, g), 0.828), "V2_V1_obl: agrees with the literature value inside nm/number.d's own gate");
        bool caught_detached = false;
        try {
            double bb = gd::beta_obl(M, deg2rad(40), g);
            (void)bb;
        } catch (const eil::ExceptionBase& e) {              // D: catch (Exception e)
            // Since we are no longer sure what the Exception message will contain,
            // we will just assume that we have successfully caught the detached
            // shock calculation failure for good reason. (D's own comment.)
            (void)e;
            caught_detached = true;
        }
        CHECK(caught_detached, "beta_obl failed to catch detached shock");
        bool caught_subsonic = false;
        try {
            double tt = gd::T2_T1_obl(0.8, beta, g);
            (void)tt;
        } catch (const eil::ExceptionBase& e) {
            (void)e;
            caught_subsonic = true;
        }
        CHECK(caught_subsonic, "Oblique shock relations failed to catch subsonic Mach");
    }
    // ---- unittest 6, idealgasflow.d:937-952
    {
        double M1 = 1.5; double p1 = 100.0e3; double T1 = 300.0;
        double R = 287.1; double g = 1.4; double rho1 = p1/(R*T1);
        double a1 = eil::sqrt(g*R*T1);
        double V1 = M1 * a1;
        double beta = deg2rad(49);
        eil::Array<double> results = gd::theta_cone(V1, p1, T1, beta);
        double theta_c = results[0];
        double p_c = results[2];
        double cone_deg = static_cast<double>(static_cast<eil::real>(theta_c * 180.0) / EIL_PI);
        CHECK(!d_isClose(cone_deg, 19.96), "cone flow deflection angle: reference does not hold it");
        CHECK(eil::nm::approxEqualNumbers(cone_deg, 19.96), "cone angle: agrees with the literature value inside nm/number.d's own gate");
        CHECK(!d_isClose((p_c - p1)/(0.5*rho1*V1*V1), 0.386), "cone pressure coefficient: reference does not hold it");
        CHECK(eil::nm::approxEqualNumbers((p_c - p1)/(0.5*rho1*V1*V1), 0.386), "cone Cp: agrees with the literature value inside nm/number.d's own gate");
        double bc_deg = static_cast<double>(static_cast<eil::real>(
            gd::beta_cone(V1, p1, T1, deg2rad_d(20.0)) * 180.0) / EIL_PI);
        CHECK(!d_isClose(bc_deg, 49.0), "cone shock angle from V,p,T: reference does not hold it");
        CHECK(eil::nm::approxEqualNumbers(bc_deg, 49.0), "beta_cone: agrees with the literature value inside nm/number.d's own gate");
        double bc2_deg = static_cast<double>(static_cast<eil::real>(
            gd::beta_cone2(M1, deg2rad_d(20.0)) * 180.0) / EIL_PI);
        CHECK(!d_isClose(bc2_deg, 49.0), "cone shock angle from M: reference does not hold it");
        CHECK(eil::nm::approxEqualNumbers(bc2_deg, 49.0), "beta_cone2: agrees with the literature value inside nm/number.d's own gate");
    }
}

// Section C: what the substrate's looser default would hide.
int substrate_default_gap() {
    struct Row { const char* what; double got; double lit; };
    const std::vector<Row> rows = {
        {"T0_T", gd::T0_T(2.4, 1.4), 2.152},
        {"p0_p", gd::p0_p(2.4, 1.4), 14.620},
        {"r0_r", gd::r0_r(2.4, 1.4), 6.7937},
        {"m2_shock", gd::m2_shock(2.0, 1.4), 0.5774},
        {"T2_T1", gd::T2_T1(2.0, 1.4), 1.687},
        {"r2_r1", gd::r2_r1(2.0, 1.4), 2.667},
        {"T0_T0star", gd::T0_T0star(2.0, 1.4), 0.7934},
        {"T_Tstar", gd::T_Tstar(2.0, 1.4), 0.5289},
        {"p_pstar", gd::p_pstar(2.0, 1.4), 0.3636},
        {"PM1", gd::PM1(2.4, 1.4), 0.6413},
        {"PM2", gd::PM2(0.6413479572, 1.4), 2.4},
        {"MachAngle", gd::MachAngle(2.4), 0.430},
    };
    int flips = 0;
    for (const Row& r : rows) {
        if (d_isClose(r.got, r.lit) != eil::isClose(r.got, r.lit)) ++flips;
    }
    return flips;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <probes/gasdyn_idealgasflow.txt>\n";
        return 2;
    }
    std::ifstream in(argv[1]);
    if (!in) {
        std::cerr << "cannot open corpus " << argv[1] << "\n";
        return 2;
    }
    std::vector<std::string> expected;
    for (std::string line; std::getline(in, line);) expected.push_back(line);

    section_tol_defaults();
    section_unittest_values();
    section_sweeps();
    section_oblique();
    section_inverses();

    // The `cone_trace e=<n> ...` lines are a diagnostic trace of Ridder's branch
    // choices, not solver output. Measured here: at the seventh evaluation D's
    // residual is +1.0269562977782698e-15 and the port's is -1.2628786905111156e-15
    // — same magnitude, opposite sign, both inside 1 ULP of the noise floor of
    // theta_cone's own pow/log chain (isolated in probes/gasdyn/probe_ds_cv.d, where
    // every arithmetic step matches D bit-for-bit and only the libm call differs).
    // A sign flip at that size changes which side of the bracket Ridder bisects
    // next, so the two sides take a different number of steps while converging to
    // the identical 17-digit root (0.73397862905965938 on both). The per-step trace
    // is therefore compared as a count and reported; the root line stays STRICT.
    long long trace_d = 0, trace_c = 0;
    {
        const std::string pref = "cone_trace e=";
        std::vector<std::string> ef, pf;
        for (const std::string& l : expected) {
            if (l.rfind(pref, 0) == 0) ++trace_d;
            else ef.push_back(l);
        }
        for (const std::string& l : produced) {
            if (l.rfind(pref, 0) == 0) ++trace_c;
            else pf.push_back(l);
        }
        expected.swap(ef);
        produced.swap(pf);
    }

    Tally t;
    compare(expected, t);

    // Optional second argument: write the produced corpus so it can be diffed by
    // hand (diff -u <D capture> <this file>) rather than only through the tally.
    if (argc > 2) {
        std::ofstream out(argv[2]);
        for (const std::string& l : produced) out << l << "\n";
    }

    ported_unittests();

    std::printf("corpus: %zu D lines, %zu C++ lines, %lld differ in some token\n",
                expected.size(), produced.size(), t.lines_differ);
    std::printf("%lld of %lld quantities bit-identical", t.identical, t.quantities);
    if (t.worst_ulp > 0) {
        std::printf(", worst strict residual %.3g relative on quantity %s with %lld ULP (%s)",
                    t.worst_rel, t.worst_key.c_str(), t.worst_ulp, t.worst_vals.c_str());
    }
    std::printf("\n");
    std::printf("%lld within its acceptance band, %lld outside it; worst quantity is %.3g "
                "times its band (%s, %s)\n",
                t.within_gate, t.outside_gate, t.worst_gate_ratio, t.worst_gate_key.c_str(),
                t.worst_gate_vals.c_str());
    std::printf("iteration counts: %lld of %lld identical (fe_total/fe_bracket/flag/branch)\n",
                t.counts_equal, t.counts_compared);
    std::printf("cone_trace per-step lines: D=%lld C++=%lld (excluded from the gate; the "
                "root line above is compared STRICT)\n", trace_d, trace_c);
    if (t.text_differ > 0) {
        std::printf("NOTE: %lld non-numeric tokens differ (of %lld compared)\n",
                    t.text_differ, t.text_tokens);
    }
    for (const std::string& b : t.bad) std::printf("  OUTSIDE: %s\n", b.c_str());
    // The first few differing lines, as the pow test shows them.
    int shown = 0;
    for (size_t i = 0; i < std::max(expected.size(), produced.size()) && shown < 5; ++i) {
        const std::string e = i < expected.size() ? expected[i] : "<none>";
        const std::string p = i < produced.size() ? produced[i] : "<none>";
        if (e != p) {
            std::printf("line %zu\n  D:   |%s|\n  C++: |%s|\n", i + 1, e.c_str(), p.c_str());
            ++shown;
        }
    }
    std::printf("D unittest blocks: %d/%d ported assertions reproduce the reference verdict\n",
                checks - failures, checks);
    std::printf("substrate gap: eil::isClose's default 1.0e-8 would flip %d of 12 "
                "tolerance predicates that D evaluates at 1.0e-9\n", substrate_default_gap());
    const bool corpus_ok = t.outside_gate == 0 && t.text_differ == 0 &&
                           static_cast<size_t>(t.lines_compared) == expected.size() &&
                           produced.size() == expected.size();
    std::printf("%s: gasdyn/idealgasflow\n", (corpus_ok && failures == 0) ? "PASS" : "FAIL");
    return (corpus_ok && failures == 0) ? 0 : 1;
}
