// gasdyn/idealgasflow.d -> src/gasdyn/idealgasflow.cpp (module gasdyn.idealgasflow).
//
// Definitions in the order they appear in the D source. See the header for the
// non-1:1 points (delegate objectives -> file-scope functions over file-scope
// state, D arrays -> eil::Array/eil::Slice, `double.init` NaN seeding of z_old).
//
// The D module's own imports map as: std.conv -> eil::text, std.math -> eil::sqrt
// / eil::pow / eil::asin (substrate wrappers, so a domain error prints `nan`),
// std.string + std.stdio are unused outside the unittests, nm.bbla ->
// eil::nm::bbla, nm.bracketing -> eil::nm::bracket, nm.ridder ->
// eil::nm::ridder::solve, and nm.linesearch is imported by D but never called
// here, so it is not included.

#include "gasdyn/idealgasflow.hpp"

#include <cmath>
#include <limits>
#include <string>

#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/math.hpp"
#include "gasdyn/gasflowexception.hpp"
#include "nm/bbla.hpp"
#include "nm/bracketing.hpp"
#include "nm/ridder.hpp"

namespace eil {
namespace gasdyn {

namespace {

// D default-initialises a floating local to `double.init`, which is NaN. Only
// used where that default is actually reachable (theta_cone's z_old).
constexpr double D_DOUBLE_INIT = std::numeric_limits<double>::quiet_NaN();

// --- root-finding objectives. idealgasflow.d builds these as in-line delegates
// capturing the enclosing arguments; nm/bracketing.hpp:23-26 explains why the C++
// port uses a named function over file-scope state instead. Nothing here nests:
// theta_cone -> theta_obl only, never beta_obl/M_Rayleigh/beta_cone, so one set
// of slots per objective is enough and the state cannot be clobbered mid-solve.

double g_rayleigh_target = 0.0;   // idealgasflow.d:268 f_to_solve
double g_rayleigh_g = 1.4;

double rayleigh_error(double m) { return T0_T0star(m, g_rayleigh_g) - g_rayleigh_target; }

double g_beta_M1 = 0.0;           // idealgasflow.d:512 f_to_solve
double g_beta_theta = 0.0;
double g_beta_g = 1.4;

double beta_error(double beta) {
    return theta_obl(g_beta_M1, beta, g_beta_g) - g_beta_theta;
}

double g_cone_V1 = 0.0;           // idealgasflow.d:900 error_in_theta
double g_cone_p1 = 0.0;
double g_cone_T1 = 0.0;
double g_cone_R = 287.1;
double g_cone_g = 1.4;
double g_cone_theta = 0.0;

double cone_error(double beta_guess) {
    eil::Array<double> results = theta_cone(g_cone_V1, g_cone_p1, g_cone_T1, beta_guess,
                                            g_cone_R, g_cone_g);
    double theta_guess = results[0]; // here, we only care about this value
    return theta_guess - g_cone_theta;
}

// asin goes through the substrate: eil::asin IS Phobos' definition
// (atan2(x, sqrt(1-x*x)) formed in 80-bit, domain error printed as -nan), measured over
// 8,492 arguments in probes/substrate/probe_acos_one.d. The 40 beta_obl2 rows of
// baselines/e587b235/probes/gasdyn_idealgasflow.txt (all "v=-nan") are what first
// forced this routing; a local copy of the rule used to live here and is gone.

} // namespace

//------------------------------------------------------------------ isentropic flow

double A_Astar(double M, double g) {
    double t1 = (g + 1.0) / (g - 1.0);
    double m2 = eil::sqr(M);
    double t2 = 1.0 / m2 * eil::pow(2.0 / (g + 1.0) * (1.0 + (g - 1.0) * 0.5 * m2), t1);
    return eil::sqrt(t2);
}

double T0_T(double M, double g) {
    return 1.0 + 0.5 * (g - 1.0) * M * M;
}

double p0_p(double M, double g) {
    return eil::pow(T0_T(M, g), g/(g-1.0));
}

double r0_r(double M, double g) {
    return eil::pow(T0_T(M, g), 1.0 / (g - 1.0));
}

//---------------------------------------------------------------- normal shock

double m2_shock(double M1, double g) {
    if (M1 < 1.0) {
        EIL_THROW_GASFLOW(eil::text("r2_r1: subsonic Mach number: ", M1));
    }
    double numer = 1.0 + (g - 1.0) * 0.5 * eil::sqr(M1);
    double denom = g * eil::sqr(M1) - (g - 1.0) * 0.5;
    return eil::sqrt(numer / denom);
}

double r2_r1(double M1, double g) {
    if (M1 < 1.0) {
        EIL_THROW_GASFLOW(eil::text("r2_r1: subsonic Mach number: ", M1));
    }
    double numer = (g + 1.0) * eil::sqr(M1);
    double denom = 2.0 + (g - 1.0) * eil::sqr(M1);
    return numer / denom;
}

double u2_u1(double M1, double g) {
    if (M1 < 1.0) {
        EIL_THROW_GASFLOW(eil::text("u2_u1: subsonic Mach number: ", M1));
    }
    return 1.0 / r2_r1(M1, g);
}

double p2_p1(double M1, double g) {
    if (M1 < 1.0) {
        EIL_THROW_GASFLOW(eil::text("p2_p1: subsonic Mach number: ", M1));
    }
    return 1.0 + 2.0 * g / (g + 1.0) * (eil::sqr(M1) - 1.0);
}

double T2_T1(double M1, double g) {
    return p2_p1(M1, g) / r2_r1(M1, g);
}

double p02_p01(double M1, double g) {
    double t1 = (g + 1.0) / (2.0 * g * eil::sqr(M1) - (g - 1.0));
    double t2 = (g + 1.0) * eil::sqr(M1) / (2.0 + (g - 1.0) * eil::sqr(M1));
    return eil::pow(t1, 1.0/(g-1.0)) * eil::pow(t2, g/(g-1.0));
}

double ds_Cv(double M1, double g) {
    double t1 = p2_p1(M1, g);
    double t2 = r2_r1(M1, g);
    return eil::log(t1 * eil::pow(t2, g));
}

double pitot_p(double p1, double M1, double g) {
    if (M1 > 1.0) {
        double p2 = p2_p1(M1,g)*p1;
        double M2 = m2_shock(M1, g);
        return p0_p(M2, g)*p2;
    } else {
        return p0_p(M1, g)*p1;
    } // end if
} // end pitot_p()

//--------------------------------------------------- Rayleigh line (heat addition)

double T0_T0star(double M, double g) {
    double term1 = (g + 1.0) * eil::sqr(M);
    double term2 = eil::sqr(1.0 + g * eil::sqr(M));
    double term3 = 2.0 + (g - 1.0) * eil::sqr(M);
    return term1 / term2 * term3;
}

double M_Rayleigh(double T0T0star, double g) {
    // D: `auto f_to_solve = delegate(double m){return T0_T0star(m, g) - T0T0star;};`
    // The two captured values go to the file-scope slots the objective reads.
    g_rayleigh_target = T0T0star;
    g_rayleigh_g = g;
    double M1 = 1.0;
    double M2 = 2.0;
    int result_flag = eil::nm::bracket<rayleigh_error, double>(M1, M2);
    // [TODO] should test result_flag. (kept verbatim from idealgasflow.d:272; the
    // value is deliberately unused, so a non-bracketing input silently solves the
    // wrong interval rather than reporting it.)
    (void)result_flag;
    return eil::nm::ridder::solve<rayleigh_error, double>(M1, M2);
}

double T_Tstar(double M, double g) {
    return eil::sqr(M) * eil::sqr((1.0 + g) / (1.0 + g * eil::sqr(M)));
}

double p_pstar(double M, double g) {
    return (1.0 + g) / (1.0 + g * eil::sqr(M));
}

double r_rstar(double M, double g) {
    return 1.0 / eil::sqr(M) / (1.0 + g) * (1.0 + g * eil::sqr(M));
}

double p0_p0star(double M, double g) {
    double term1 = (2.0 + (g - 1.0) * eil::sqr(M)) / (g + 1.0);
    double term2 = g / (g - 1.0);
    return (1.0 + g) / (1.0 + g * eil::sqr(M)) * eil::pow(term1, term2);
}

//------------------------------------------------------- Prandtl-Meyer turning

double PM1(double M, double g) {
    double temp1 = eil::sqrt((g + 1.0) / (g - 1.0));
    double temp2 = M * M - 1.0;    // D writes M * M here, not M^^2; kept as a product
    double nu = 0.0;
    if (temp2 < 0.0) {
        EIL_THROW_GASFLOW(eil::text("PM1 received a subsonic Mach number: ", M));
    } else {
        // Not eil::sqrt here, deliberately: the guard above makes a domain error
        // unreachable on this path (temp2 is >= 0 or already NaN), and Phobos
        // propagates a NaN *payload* through sqrt, keeping the sign bit, while
        // eil::sqrt canonicalises every NaN to "+nan". D prints "-nan" for the
        // diverged PM2 rows (corpus gasdyn_idealgasflow.txt lines 10075-10133),
        // so the plain sqrt is what matches. The wrapper stays everywhere a real
        // domain error is reachable (temp1 above, and the sqrt()s in A_Astar,
        // m2_shock, M2_obl, V2_V1_obl, beta_obl2, theta_cone).
        temp2 = std::sqrt(temp2);
        nu = temp1 * std::atan(temp2 / temp1) - std::atan(temp2);
    } // end if
    return nu;
} // end PM1()

double PM2(double nu, double g, double tol) {
    if (nu < 0.0) {
        EIL_THROW_GASFLOW("Given negative value for Prandtl-Meyer function.");
    } // end if
    //
    // Generate an initial guess and, it it is good, return it. ("it it" is D's text)
    double M_0  = MFromNu_approximate(nu, g);
    double nu_0 = PM1(M_0, g);
    double f_0  = nu - nu_0;
    if (std::fabs(f_0) < tol) { return M_0; }
    //
    // Make some improvements using the secant method.
    double M_1  = M_0 * 1.001;
    double nu_1 = PM1(M_1, g);
    double f_1  = nu - nu_1;
    int count = 0;
    do {
        ++count;
        // Improve the guess to Mach number.
        double slope = (f_1 - f_0) / (M_1 - M_0);
        double M_2 = M_1 - f_1 / slope;
        double nu_2 = PM1(M_2, g);
        double f_2 = nu - nu_2;
        // Prepare for next iteration.
        M_0 = M_1; nu_0 = nu_1; f_0 = f_1;
        M_1 = M_2; nu_1 = nu_2; f_1 = f_2;
    } while (std::fabs(f_1) > tol && count < 30);
    //
    if (std::fabs(f_1) > tol) {
        EIL_THROW_GASFLOW(eil::text("PM2: iteration did not converge."));
    }
    return M_1;
} // end PM2()

double MFromNu_approximate(double nu, double g) {
    double M = 0.0;
    // D divides by its 80-bit PI, so the quotient is computed wide and rounded on
    // the store into the double nu_d (reports/wave-02-brief.md section 5).
    double nu_d = nu * 180.0 / EIL_PI;
    if (nu_d < 0.0) {
        M = 0.0;
    } else if (nu_d < 5.0) {
        M = 1.0 + 7.932e-2 * eil::pow(nu_d, 2.0/3.0) *
            (1.0 + nu_d * (3.681e-2 + nu_d * (-5.99e-3 + nu_d * 5.719e-4)) );
    } else if ( nu_d < 65.0 ) {
        M = 1.071 + nu_d * (3.968e-2 + nu_d * (-4.615e-4 + nu_d *
                                               (1.513e-5 + nu_d * (-1.840e-7 + nu_d * 1.186e-9))));
    } else {
        // Use an asymptotic expansion for large M.
        double bigG = eil::sqrt((g + 1.0) / (g - 1.0));
        // D: `nu_max = PI_2 * (bigG - 1.0)` with Phobos' PI_2 an 80-bit enum, so
        // the product is formed in extended precision and rounded once, on the
        // store into the double nu_max. PI/2 is exact in binary, so EIL_PI/2 is
        // the same value as the enum.
        double nu_max = static_cast<double>(EIL_PI / 2.0 * (bigG - 1.0));
        M = (1.0 - bigG * bigG) / (nu - nu_max);
    } // end if
    return M;
} // end MFromNu_approx()

double MachAngle(double M) {
    if (M < 1.0) {
        EIL_THROW_GASFLOW(eil::text("MachAngle: subsonic Mach number: ", M));
    } else {
        return eil::asin(1.0/M);
    } // end if
} // end MachAngle()

//----------------------------------------------------------- oblique shock relations

double beta_obl(double M1, double theta, double g, double tol) {
    if (M1 < 1.0) {
        EIL_THROW_GASFLOW(eil::text("beta_obl: subsonic Mach number: ", M1));
    }
    if (theta == 0.0) {
        EIL_THROW_GASFLOW("beta_obl: zero deflection angle, so there can be no shock");
    }
    int sign_beta = (theta < 0.0) ? -1 : 1;
    theta = std::fabs(theta);
    g_beta_M1 = M1;
    g_beta_theta = theta;
    g_beta_g = g;
    //
    double b1 = eil::asin(1.0/M1); // the weakest shock is at the Mach angle
    if (theta < tol) return sign_beta*b1;
    double f1 = beta_error(b1);
    if (std::fabs(f1) < tol) return sign_beta*b1;
    double b2 = b1 * 1.05; // second guess slightly stronger
    double f2 = beta_error(b2);
    if (std::fabs(f2) < tol) return sign_beta*b2;
    int result_flag = eil::nm::bracket<beta_error, double>(b1, b2);
    if (result_flag < 0) {
        std::string msg = eil::text("beta_obl: could not bracket beta for M1=", M1,
                                    " theta=", theta);
        EIL_THROW_GASFLOW(msg);
    }
    return sign_beta*(eil::nm::ridder::solve<beta_error, double>(b1, b2));
} // end beta_obl()

double beta_obl2(double M1, double p2_p1, double g) {
    if (M1 < 1.0) {
        EIL_THROW_GASFLOW(eil::text("beta_obl2: subsonic Mach number: ", M1));
    } // end if
    if (p2_p1 < 1.0) {
        EIL_THROW_GASFLOW(eil::text("beta_obl2: invalid p2_p1: ", p2_p1));
    } // end if
    // The parameter is D's `p2_p1` (idealgasflow.d:536), which shadows the p2_p1()
    // function above for the extent of this body. Nothing here calls that function,
    // so the shadow is inert; the name is kept to show which value is which.
    double dum1 = eil::sqrt(((g+1.)*p2_p1+g-1.)/2./g);
    return eil::asin(dum1/M1);
} // end beta_obl2()

double theta_obl(double M1, double beta, double g) {
    double M1n = M1 * std::fabs(eil::sin(beta));
    if (std::fabs(M1n - 1) < 1.0e-9) return 0.0;
    if (M1n < 1.0) {
        std::string msg = eil::text("theta_obl: subsonic normal Mach number: ", M1n,
                                   " for M1=", M1, " beta=", beta);
        EIL_THROW_GASFLOW(msg);
    }
    int sign_beta = (beta < 0.0) ? -1 : 1;
    beta = std::fabs(beta);
    if (dtan_theta(M1,beta,g) < 0.0){
        std::string msg = eil::text("theta_obl: assume shock is detached",
                                   " for M1=", M1, " beta=", beta);
        EIL_THROW_GASFLOW(msg);
    } // end if
    double t1 = 2.0 / std::tan(beta) * (eil::sqr(M1n) - 1.0);
    double t2 = eil::sqr(M1) * (g + eil::cos(2.0 * beta)) + 2.0;
    return sign_beta*std::atan(t1/t2);
} // end theta_obl()

double dtan_theta(double M1, double beta, double g) {
    beta = std::fabs(beta);
    double dum1 = eil::sqr(M1)*(eil::cos(2.0*beta)+g)+2.0;
    // D: `M1^^2*sin(beta)^^2` -- `^^` binds tighter than `*`, so it is
    // (M1^^2) * (sin(beta)^^2), two separate squares, not the square of the product.
    // Both `^^2` arms are eil::sqr (T-931).
    double dum2 = eil::sqr(M1)*eil::sqr(eil::sin(beta))-1.0;
    return 4.0*eil::sqr(M1)*eil::sqr(eil::cos(beta))/dum1
        + 4.0*eil::sqr(M1)*eil::sin(2.0*beta)*dum2/(eil::sqr(dum1))/std::tan(beta)
        - 2.0*dum2/dum1/(eil::sqr(eil::sin(beta)));
} // end dtan_theta()

double M2_obl(double M1, double beta, double theta, double g) {
    double M1n = M1 * std::fabs(eil::sin(beta));
    if (M1n < 1.0) {
        EIL_THROW_GASFLOW(eil::text("M2_obl: subsonic normal Mach number: ", M1n));
    }
    double numer = 1.0 + (g - 1.0) * 0.5 * eil::sqr(M1n);
    double denom = g * eil::sqr(M1n) - (g - 1.0) * 0.5;
    return eil::sqrt(numer / denom / eil::sqr(eil::sin(beta - theta)) );
} // end M2_obl()

double r2_r1_obl(double M1, double beta, double g) {
    double M1n = M1 * std::fabs(eil::sin(beta));
    if (M1n < 1.0) {
        EIL_THROW_GASFLOW(eil::text("r2_r1_obl: subsonic normal Mach number: ", M1n));
    }
    return r2_r1(M1n,g);
} // end r2_r1_obl()

double Vn2_Vn1_obl(double M1, double beta, double g) {
    double M1n = M1 * std::fabs(eil::sin(beta));
    if (M1n < 1.0) {
        EIL_THROW_GASFLOW(eil::text("Vn2_Vn1_obl: subsonic normal Mach number: ", M1n));
    }
    return u2_u1(M1n,g);
}

double V2_V1_obl(double M1, double beta, double g) {
    double M1n = M1 * std::fabs(eil::sin(beta));
    if (M1n < 1.0) {
        EIL_THROW_GASFLOW(eil::text("V2_V1_obl: subsonic normal Mach number: ", M1n));
    }
    return eil::sqrt(eil::sqr(eil::sin(beta) / r2_r1_obl(M1, beta, g))
                     + eil::sqr(eil::cos(beta)));
}

double p2_p1_obl(double M1, double beta, double g) {
    double M1n = M1 * std::fabs(eil::sin(beta));
    if (M1n < 1.0) {
        EIL_THROW_GASFLOW(eil::text("p2_p1_obl: subsonic normal Mach number: ", M1n));
    }
    return p2_p1(M1n,g);
}

double T2_T1_obl(double M1, double beta, double g) {
    double M1n = M1 * std::fabs(eil::sin(beta));
    if (M1n < 1.0) {
        EIL_THROW_GASFLOW(eil::text("T2_T1_obl: subsonic normal Mach number: ", M1n));
    }
    return T2_T1(M1n,g);
}

double p02_p01_obl(double M1, double beta, double g) {
    double M1n = M1 * std::fabs(eil::sin(beta));
    if (M1n < 1.0) {
        EIL_THROW_GASFLOW(eil::text("p02_p01_obl: subsonic normal Mach number: ", M1n));
    }
    return p02_p01(M1n,g);
}

//-------------------------------------------------------- Taylor-Maccoll cone flow

eil::Array<double> taylor_maccoll_odes(eil::Slice<double> z, double theta, double g) {
    /**
    The ODEs from the Taylor-Maccoll formulation.

    See PJ's workbook for Feb 2012 for details.
    We've packaged them formally so that we might one day use
    a more sophisticated ODE integrator requiring fewer steps.
    **/
    double rho=z[0]; double V_r=z[1]; double V_theta=z[2];
    double h=z[3]; double p=z[4];
    (void)p;    // read out of the state vector and not used, exactly as in D
    // Assemble linear system for determining the derivatives wrt theta.
    auto A = eil::nm::bbla::zeros<double>(5,6); // Augmented matrix with rhs in last column.
    A(0,0) = V_theta; A(0,2) = rho; A(0,5) = -2.0*rho*V_r - rho*V_theta/std::tan(theta);
    A(1,1) = 1.0; A(1,5) = V_theta;
    A(2,1) = rho*V_r; A(2,2) = rho*V_theta; A(2,4) = 1.0;
    A(3,1) = V_r; A(3,2) = V_theta; A(3,3) = 1.0;
    A(4,0) = h*(g-1)/g; A(4,3) = rho*(g-1)/g; A(4,4) = -1.0;
    eil::nm::bbla::gaussJordanElimination<double>(A);
    return A.getColumn(5);
}

eil::Array<double> theta_cone(double V1, double p1, double T1, double beta,
                              double R, double g) {
    /**
    Compute the cone-surface angle and conditions given the shock wave angle.

    Input:
    V1: speed of gas into shock
    p1: free-stream pressure
    T1: free-stream static temperature
    beta: shock wave angle wrt stream direction (in radians)
    R: gas constant
    g: ratio of specific heats

    Returns:
    an array of [theta_c, V_c, p_c, T_c]
    where
    theta_c is stream deflection angle in radians
    V_c is the cone-surface speed of gas in m/s
    p_c is the cone-surface pressure
    T_c is the cone-surface static temperature

    The computation starts with the oblique-shock jump and then integrates
    across theta until V_theta goes through zero.
    The cone surface corresponds to V_theta == 0.

       08-Mar-2012 : PJ ideal-gas version adapted from the cea2_gas_flow.py.
       24-Jun-2012 : RJG added checks to catch the limiting case when beta < mu
                   : and a linear interpolation when beta is only slightly larger
                   : than mu (1% larger)
       2016-11-19 ported to D by PJ
    **/
    // When beta is only this fraction larger than mu,
    // we'll apply a linear interpolation
    const double LINEAR_INTERP_SWITCH = 1.01;   // D: enum LINEAR_INTERP_SWITCH = 1.01
    // Free-stream properties and gas model.
    double a1 = eil::sqrt(g*R*T1);
    double M1 = V1 / a1;
    double C_p = R * g / (g-1);
    double h1 = C_p * T1;
    double rho1 = p1 / (R * T1);
    (void)h1;   // D computes the free-stream enthalpy here and never uses it
    // Test beta in relation to the Mach angle, mu
    double mu = eil::asin(1.0/M1);
    double beta2 = LINEAR_INTERP_SWITCH*mu;
    // Test for an infinitely-weak shock angle
    if (beta <= mu) { return eil::Array<double>{0.0, V1, p1, T1}; }
    //
    if (beta < beta2) {
        // It is difficult to integrate between the shock and cone body
        // when the shock angle is only slightly larger than the Mach
        // angle. In this instance, find the value at LINEAR_INTER_SWITCH*mu
        // and linearly interpolate to find the value at beta
        auto results = theta_cone(V1, p1, T1, beta2, R, g);
        double theta2=results[0]; double V2=results[1];
        double p2=results[2]; double T2=results[3];
        double frac = (beta - mu)/(beta2 - mu);
        double theta_c = frac*theta2;
        double V = (1.0 - frac)*V1 + frac*V2;
        double p = (1.0 - frac)*p1 + frac*p2;
        double T = (1.0 - frac)*T1 + frac*T2;
        return eil::Array<double>{theta_c, V, p, T};
    }
    //
    // Start at the point just downstream the oblique shock.
    double theta_s = theta_obl(M1, beta, g);
    double M2 = M2_obl(M1, beta, theta_s, g);
    if (M2 < 1.0) {
        EIL_THROW_GASFLOW("gone subsonic at shock");
    }
    double rho2 = rho1 * r2_r1_obl(M1, beta, g);
    double V2 = V1 * V2_V1_obl(M1, beta, g);
    double p2 = p1 * p2_p1_obl(M1, beta, g);
    double T2 = T1 * T2_T1_obl(M1, beta, g);
    double h2 = T2 * C_p;
    //
    // Initial conditions for Taylor-Maccoll integration.
    double dtheta = -0.05 * EIL_PI / 180.0;  // fraction-of-a-degree steps
    double theta = beta;
    double V_r = V2 * eil::cos(beta - theta_s);
    double V_theta = -V2 * eil::sin(beta - theta_s);
    double rho = rho2; double p = p2; double h = h2;
    // For integrating across the shock layer, the state vector is:
    double z[5] = {rho2, V_r, V_theta, h2, p2};
    // `double[5] z_old;` is D-default-initialised to double.init (NaN) and only
    // written inside the loop below. If the loop never executes (V_theta >= 0 on
    // entry) D interpolates from NaN and reports "oops, did not correctly find
    // cone surface" or a NaN answer; C++ would read uninitialised storage, so the
    // NaN seeding is kept to preserve that behaviour rather than invent one.
    double z_old[5] = {D_DOUBLE_INIT, D_DOUBLE_INIT, D_DOUBLE_INIT, D_DOUBLE_INIT,
                       D_DOUBLE_INIT};
    double theta_old= theta;
    while (V_theta < 0.0) {
        // Keep a copy for linear interpolation at the end.
        eil::Slice<double>(z_old, 5, nullptr, 0).assign_from(eil::Slice<double>(z, 5, nullptr, 0));
        theta_old = theta;
        // Do the update using a low-order method (Euler) for the moment.
        eil::Array<double> dz =
            taylor_maccoll_odes(eil::Slice<double>(z, 5, nullptr, 0), theta, g);
        // D assigns that `double[]` to a `double[5]`, which is a checked length
        // conversion (a mismatch raises on the D side); getColumn(5) of a 5x6
        // matrix always has 5 entries, so the check cannot fire here.
        if (dz.length() != 5) throw eil::RangeError(dz.length(), 5, __FILE__, __LINE__);
        double dzdtheta[5];
        for (size_t i = 0; i < 5; ++i) dzdtheta[i] = dz[i];
        for (size_t i = 0; i < 5; ++i) z[i] += dtheta * dzdtheta[i];
        theta += dtheta;
        rho=z[0]; V_r=z[1]; V_theta=z[2]; h=z[3]; p=z[4];
    }
    // At this point, V_theta should have crossed zero so
    // we can linearly-interpolate the cone-surface conditions.
    double V_theta_old = z_old[2];
    double frac = (0.0 - V_theta_old)/(V_theta - V_theta_old);
    double z_c[5];
    for (size_t i = 0; i < 5; ++i) {
        z_c[i] = z_old[i]*(1.0-frac) + z[i]*frac;
    }
    double theta_c = theta_old*(1.0-frac) + theta*frac;
    // At the cone surface...
    rho=z_c[0]; V_r=z_c[1]; V_theta=z_c[2]; h=z_c[3]; p=z_c[4];
    (void)rho;  // D unpacks the whole state vector; only V_r, p and h are read
    double T = h / C_p;
    if (std::fabs(V_theta) > 1.0e-6) {
        EIL_THROW_GASFLOW("oops, did not correctly find cone surface");
    }
    return eil::Array<double>{theta_c, V_r, p, T};
} // end theta_cone()

double beta_cone(double V1, double p1, double T1, double theta, double R, double g) {
    /**
    Compute the conical shock wave angle given the cone-surface deflection angle.

    Input:
    V1: speed of gas into shock
    p1: free-stream pressure
    T1: free-stream static temperature
    theta: stream deflection angle (in radians)
    R: gas constant
    g: ratio of specific heats

    Returns:
    shock wave angle wrt incoming stream direction (in radians)

    This ideal-gas version adapted from the cea2_gas_flow version, 08-Mar-2012.
    and then ported to D, November 2016.
    **/
    // Free-stream properties and gas model.
    double a1 = eil::sqrt(g*R*T1);
    double M1 = V1 / a1;
    double C_p = R * g / (g-1);
    double h1 = C_p * T1;
    double rho1 = p1 / (R * T1);
    (void)C_p; (void)h1; (void)rho1;  // D sets these up and uses none of them
    // We guess values of beta until this error measure is (close to) zero.
    g_cone_V1 = V1;
    g_cone_p1 = p1;
    g_cone_T1 = T1;
    g_cone_R = R;
    g_cone_g = g;
    g_cone_theta = theta;
    // Initial guess at bracket for shock wave angle.
    double b1 = eil::asin(1.0/M1) * 1.01; // to be stronger than a Mach wave
    double b2 = 1.05 * b1;
    // D's x2_max argument is `PI/2`, Phobos' 80-bit enum, rounded to the double
    // parameter on the way in; the x1_min argument is the Mach angle itself.
    eil::nm::bracket<cone_error, double>(b1, b2, eil::asin(1.0/M1),
                                        static_cast<double>(EIL_PI / 2.0));
    return eil::nm::ridder::solve<cone_error, double>(b1, b2, 1.0e-4);
} // end beta_cone()

double beta_cone2(double M1, double theta, double R, double g) {
    /**
    Compute the conical shock wave angle,
    given the cone-surface deflection angle and free stream Mach number.

    :param M1: free stream Mach number
    :param theta: stream deflection angle (in radians)
    :param R: gas constant
    :param g: ratio of specific heats
    :returns: shock wave angle wrt incoming stream direction (in radians)

    .. This version basically delegates work to beta_cone().
    **/
    // Compute free stream velocity assuming unit value temperature
    double T1 = 1.0;
    double a1 = eil::sqrt(g*R*T1);
    double V1 = M1*a1;
    // Set free stream pressure to unit value
    double p1 = 1.0;
    // Now ready to call beta_cone()
    return beta_cone(V1, p1, T1, theta, R, g);
} // end beta_cone2()

} // namespace gasdyn
} // namespace eil
