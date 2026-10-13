#pragma once
// gasdyn/idealgasflow.d -> src/gasdyn/idealgasflow.hpp (module gasdyn.idealgasflow).
//
// Quasi-one-dimensional steady flow of an ideal gas: isentropic area/Mach and
// reservoir relations, normal shock, Rayleigh line (heat addition),
// Prandtl-Meyer turning, oblique shock, and the Taylor-Maccoll conical-flow
// integrator. Ported statement-for-statement; every `^^` is eil::pow (substrate
// rule, reports/wave-02-brief.md section 5), every sqrt/log/pow/asin/acos is the
// eil:: wrapper so a domain error prints `nan` the way Phobos does.
//
// Namespace: eil::gasdyn, flat (brief section 2). No symbol in this module
// collides with another eil::gasdyn declaration, nor with eil::nm: the D module
// imports nm.bbla, nm.bracketing, nm.ridder and nm.linesearch but names nothing
// those modules also export, so no per-module namespace is needed.
//
// Non-1:1 notes (registered in provenance/fidelity.yml):
//   * adapted-callback: D builds the root-finding objectives as in-line
//     `delegate` closures (`M_Rayleigh` idealgasflow.d:268, `beta_obl` :512,
//     `beta_cone` :900). nm/bracketing.hpp and nm/ridder.hpp take the callback as
//     a compile-time `auto` non-type template parameter, which cannot name a
//     local, so each objective is a file-scope function over file-scope state in
//     the .cpp -- the same adaptation bracketing.hpp:23-26 documents. Nothing in
//     this module re-enters an objective while another is live (theta_cone calls
//     theta_obl, never beta_obl/M_Rayleigh/beta_cone), so the state cannot be
//     clobbered; the counters exposed for verification are read back the same way.
//   * D's `T[]` return of `taylor_maccoll_odes`/`theta_cone` is an owned GC
//     array -> eil::Array<double>; the `double[5]` locals stay C arrays with
//     eil::Slice views, matching D's static-array element-wise `[]` arithmetic.
//   * D default-initialises a floating local to `double.init` (NaN). The one
//     place where that is reachable is `double[5] z_old` (idealgasflow.d:849),
//     which is only written inside the integration loop; it is NaN-initialised
//     here so an unreachable pre-loop read gives NaN rather than uninitialised
//     memory. See the note at the definition.
//   * Dead imports of the D module are not carried: std.stdio and std.string are
//     unused in idealgasflow.d (std.string only in its unittests), and
//     `import nm.linesearch` brings `minimize`, which this module never calls.
//   * `version(...)` branches: none. The module has no version identifier, so
//     nothing is kept under #ifdef.
//
// Not ported, on purpose (brief section 4, scope correction 1):
//   gasdyn/gasflow.d (1595) and gasdyn/gasflow_demo.d (202). gasflow.d's shock
//   relations take `ref const(GasState)`/`GasModel gm` from gas/ (23,281 D lines,
//   Wave 3) and downcast `cast(CEAGas) gm`; gasflow_demo.d needs
//   init_gas_model("../gas/sample-data/cea-lut-air-version-test.lua") from the Lua
//   wave. Stubbing gas/ here would change behaviour.

#include "eil/array.hpp"
#include "eil/eil.hpp"

namespace eil {
namespace gasdyn {

/// Isentropic flow

// idealgasflow.d:56 Area ratio A/Astar for an isentropic, quasi-1D flow.
double A_Astar(double M, double g = 1.4);

// idealgasflow.d:70 ratio of total (stagnation) temperature to static temperature
double T0_T(double M, double g = 1.4);

// idealgasflow.d:81 ratio of total pressure to static pressure
double p0_p(double M, double g = 1.4);

// idealgasflow.d:92 ratio of stagnation density to free-stream density
double r0_r(double M, double g = 1.4);

/// 1-D normal shock relations. State 1 is before the shock, state 2 after.

// idealgasflow.d:115 Mach number M2 after a normal shock
double m2_shock(double M1, double g = 1.4);

// idealgasflow.d:131 density ratio r2/r1 across a normal shock
double r2_r1(double M1, double g = 1.4);

// idealgasflow.d:147 velocity ratio u2/u1 across a normal shock
double u2_u1(double M1, double g = 1.4);

// idealgasflow.d:161 static pressure ratio p2/p1 across a normal shock
double p2_p1(double M1, double g = 1.4);

// idealgasflow.d:175 static temperature ratio T2/T1 across a normal shock
double T2_T1(double M1, double g = 1.4);

// idealgasflow.d:186 stagnation pressure ratio p02/p01 across a normal shock
double p02_p01(double M1, double g = 1.4);

// idealgasflow.d:200 nondimensional entropy change ds/Cv across a normal shock
double ds_Cv(double M1, double g = 1.4);

// idealgasflow.d:216 Pitot pressure (absolute) of a Mach M1 free stream at p1
double pitot_p(double p1, double M1, double g = 1.4);

/// 1-D flow with heat addition (Rayleigh-line). State star is sonic.

// idealgasflow.d:249 total temperature ratio T0/T0star
double T0_T0star(double M, double g = 1.4);

// idealgasflow.d:266 Mach number from a T0/T0star ratio (nm root finders).
// `T0T0star` is D's parameter name; it deliberately does not shadow T0_T0star().
double M_Rayleigh(double T0T0star, double g = 1.4);

// idealgasflow.d:285 static temperature ratio T/Tstar
double T_Tstar(double M, double g = 1.4);

// idealgasflow.d:299 static pressure ratio p/pstar
double p_pstar(double M, double g = 1.4);

// idealgasflow.d:313 density ratio r/rstar
double r_rstar(double M, double g = 1.4);

// idealgasflow.d:327 stagnation pressure ratio p0/p0star
double p0_p0star(double M, double g = 1.4);

/// Isentropic flow turning (Prandtl-Meyer). nu = 0 when M = 1.

// idealgasflow.d:358 Prandtl-Meyer function nu(M), Anderson eqn 4.40
double PM1(double M, double g = 1.4);

// idealgasflow.d:380 Mach number from nu: Fraser polynomial seed + secant loop
double PM2(double nu, double g = 1.4, double tol = 1.0e-6);

// idealgasflow.d:425 M(nu) by the Fraser (1975) polynomial, asymptotic above 65 deg
double MFromNu_approximate(double nu, double g = 1.4);

// idealgasflow.d:452 Mach angle in radians (M >= 1)
double MachAngle(double M);

/// Oblique shock relations. beta is the wave angle, theta the deflection,
/// both in radians, measured from the on-coming stream direction.

// idealgasflow.d:502 shock angle from a deflection angle (nm root finders)
double beta_obl(double M1, double theta, double g = 1.4, double tol = 1.0e-6);

// idealgasflow.d:536 shock angle from the static pressure ratio across the shock.
// D names the second parameter `p2_p1`, which shadows p2_p1() above inside this
// body; the name is kept, and the body does not call the shadowed function.
double beta_obl2(double M1, double p2_p1, double g = 1.4);

// idealgasflow.d:556 deflection angle from the shock angle
double theta_obl(double M1, double beta, double g = 1.4);

// idealgasflow.d:585 d(tan theta)/d(beta); negative means the shock is detached
double dtan_theta(double M1, double beta, double g = 1.4);

// idealgasflow.d:602 Mach number after an oblique shock
double M2_obl(double M1, double beta, double theta, double g = 1.4);

// idealgasflow.d:620 density ratio r2/r1 across an oblique shock
double r2_r1_obl(double M1, double beta, double g = 1.4);

// idealgasflow.d:636 normal velocity ratio Vn2/Vn1 across an oblique shock
double Vn2_Vn1_obl(double M1, double beta, double g = 1.4);

// idealgasflow.d:652 speed ratio V2/V1 across an oblique shock
double V2_V1_obl(double M1, double beta, double g = 1.4);

// idealgasflow.d:668 static pressure ratio p2/p1 across an oblique shock
double p2_p1_obl(double M1, double beta, double g = 1.4);

// idealgasflow.d:684 static temperature ratio T2/T1 across an oblique shock
double T2_T1_obl(double M1, double beta, double g = 1.4);

// idealgasflow.d:700 stagnation pressure ratio p02/p01 across an oblique shock
double p02_p01_obl(double M1, double beta, double g = 1.4);

/// Taylor-Maccoll cone flow.

// idealgasflow.d:742 the five d/d(theta) derivatives, solved as a 5x6 augmented
// linear system through nm.bbla's Gauss-Jordan elimination. The parameter is
// D's `double[] z` view of the caller's state vector [rho, V_r, V_theta, h, p].
eil::Array<double> taylor_maccoll_odes(eil::Slice<double> z, double theta,
                                       double g = 1.4);

// idealgasflow.d:765 cone-surface conditions given the shock angle:
// returns [theta_c, V_c, p_c, T_c].
eil::Array<double> theta_cone(double V1, double p1, double T1, double beta,
                              double R = 287.1, double g = 1.4);

// idealgasflow.d:873 conical shock angle given the cone half-angle (radians)
double beta_cone(double V1, double p1, double T1, double theta, double R = 287.1,
                 double g = 1.4);

// idealgasflow.d:913 same, from the free-stream Mach number (delegates to beta_cone)
double beta_cone2(double M1, double theta, double R = 287.1, double g = 1.4);

} // namespace gasdyn
} // namespace eil
