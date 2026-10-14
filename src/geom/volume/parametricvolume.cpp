// geom/volume/parametricvolume.d -> src/geom/volume/parametricvolume.cpp
// (module geom.volume.parametricvolume)

#include "geom/volume/parametricvolume.hpp"

#include <cstdio>
#include <cstring>
#include <string>

#include "eil/error.hpp"
#include "eil/format.hpp"

namespace eil {
namespace geom {

// D:68-89
Vector3 ParametricVolume::dVdr(double r, double s, double t) const {
    // Obtain the derivative approximately, via a finite-difference.
    double dr = 0.001;
    Vector3 V0 = this->operator()(r, s, t);
    Vector3 derivative;
    if (r + dr > 1.0) {
        // r is close to the r=1.0 boundary, use a one-sided difference.
        Vector3 Vminus1 = this->operator()(r - dr, s, t);
        derivative = (V0 - Vminus1) / dr;
    } else if (r - dr < 0.0) {
        // r is close to the r=0.0 boundary, use a one-sided difference.
        Vector3 Vplus1 = this->operator()(r + dr, s, t);
        derivative = (Vplus1 - V0) / dr;
    } else {
        // Not near a boundary, use central-difference.
        Vector3 Vminus1 = this->operator()(r - dr, s, t);
        Vector3 Vplus1 = this->operator()(r + dr, s, t);
        derivative = (Vplus1 - Vminus1) / (2.0 * dr);
    }
    return derivative;
}

// D:90-111
Vector3 ParametricVolume::dVds(double r, double s, double t) const {
    // Obtain the derivative approximately, via a finite-difference.
    double ds = 0.001;
    Vector3 V0 = this->operator()(r, s, t);
    Vector3 derivative;
    if (s + ds > 1.0) {
        // s is close to the s=1.0 boundary, use a one-sided difference.
        Vector3 Vminus1 = this->operator()(r, s - ds, t);
        derivative = (V0 - Vminus1) / ds;
    } else if (s - ds < 0.0) {
        // s is close to the s=0.0 boundary, use a one-sided difference.
        Vector3 Vplus1 = this->operator()(r, s + ds, t);
        derivative = (Vplus1 - V0) / ds;
    } else {
        // Not near a boundary, use central-difference.
        Vector3 Vminus1 = this->operator()(r, s - ds, t);
        Vector3 Vplus1 = this->operator()(r, s + ds, t);
        derivative = (Vplus1 - Vminus1) / (2.0 * ds);
    }
    return derivative;
}

// D:112-133
Vector3 ParametricVolume::dVdt(double r, double s, double t) const {
    // Obtain the derivative approximately, via a finite-difference.
    double dt = 0.001;
    Vector3 V0 = this->operator()(r, s, t);
    Vector3 derivative;
    if (t + dt > 1.0) {
        // t is close to the t=1.0 boundary, use a one-sided difference.
        Vector3 Vminus1 = this->operator()(r, s, t - dt);
        derivative = (V0 - Vminus1) / dt;
    } else if (t - dt < 0.0) {
        // t is close to the t=0.0 boundary, use a one-sided difference.
        Vector3 Vplus1 = this->operator()(r, s, t + dt);
        derivative = (Vplus1 - V0) / dt;
    } else {
        // Not near a boundary, use central-difference.
        Vector3 Vminus1 = this->operator()(r, s, t - dt);
        Vector3 Vplus1 = this->operator()(r, s, t + dt);
        derivative = (Vplus1 - Vminus1) / (2.0 * dt);
    }
    return derivative;
}

// D:134-157
Vector3 ParametricVolume::d2Vdr2(double r, double s, double t) const {
    // Obtain the derivative approximately, via a finite-difference.
    double dr = 0.001;
    Vector3 V0 = this->operator()(r, s, t);
    Vector3 derivative;
    if (r + dr > 1.0) {
        // r is close to the r=1.0 boundary, use a one-sided difference.
        Vector3 Vminus1 = this->operator()(r - dr, s, t);
        Vector3 Vminus2 = this->operator()(r - 2 * dr, s, t);
        derivative = (V0 - 2 * Vminus1 + Vminus2) / (dr * dr);
    } else if (r - dr < 0.0) {
        // r is close to the r=0 boundary, use a one-sided difference.
        Vector3 Vplus1 = this->operator()(r + dr, s, t);
        Vector3 Vplus2 = this->operator()(r + 2 * dr, s, t);
        derivative = (Vplus2 - 2 * Vplus1 + V0) / (dr * dr);
    } else {
        // Not near a boundary, use central-difference.
        Vector3 Vminus1 = this->operator()(r - dr, s, t);
        Vector3 Vplus1 = this->operator()(r + dr, s, t);
        derivative = (Vplus1 - 2 * V0 + Vminus1) / (dr * dr);
    }
    return derivative;
}

// D:158-181
Vector3 ParametricVolume::d2Vds2(double r, double s, double t) const {
    // Obtain the derivative approximately, via a finite-difference.
    double ds = 0.001;
    Vector3 V0 = this->operator()(r, s, t);
    Vector3 derivative;
    if (s + ds > 1.0) {
        // s is close to the s=1.0 boundary, use a one-sided difference.
        Vector3 Vminus1 = this->operator()(r, s - ds, t);
        Vector3 Vminus2 = this->operator()(r, s - 2 * ds, t);
        derivative = (V0 - 2 * Vminus1 + Vminus2) / (ds * ds);
    } else if (s - ds < 0.0) {
        // s is close to the s=0 boundary, use a one-sided difference.
        Vector3 Vplus1 = this->operator()(r, s + ds, t);
        Vector3 Vplus2 = this->operator()(r, s + 2 * ds, t);
        derivative = (Vplus2 - 2 * Vplus1 + V0) / (ds * ds);
    } else {
        // Not near a boundary, use central-difference.
        Vector3 Vminus1 = this->operator()(r, s - ds, t);
        Vector3 Vplus1 = this->operator()(r, s + ds, t);
        derivative = (Vplus1 - 2 * V0 + Vminus1) / (ds * ds);
    }
    return derivative;
}

// D:182-205
Vector3 ParametricVolume::d2Vdt2(double r, double s, double t) const {
    // Obtain the derivative approximately, via a finite-difference.
    double dt = 0.001;
    Vector3 V0 = this->operator()(r, s, t);
    Vector3 derivative;
    if (t + dt > 1.0) {
        // t is close to the t=1.0 boundary, use a one-sided difference.
        Vector3 Vminus1 = this->operator()(r, s, t - dt);
        Vector3 Vminus2 = this->operator()(r, s, t - 2 * dt);
        derivative = (V0 - 2 * Vminus1 + Vminus2) / (dt * dt);
    } else if (t - dt < 0.0) {
        // t is close to the t=0 boundary, use a one-sided difference.
        Vector3 Vplus1 = this->operator()(r, s, t + dt);
        Vector3 Vplus2 = this->operator()(r, s, t + 2 * dt);
        derivative = (Vplus2 - 2 * Vplus1 + V0) / (dt * dt);
    } else {
        // Not near a boundary, use central-difference.
        Vector3 Vminus1 = this->operator()(r, s, t - dt);
        Vector3 Vplus1 = this->operator()(r, s, t + dt);
        derivative = (Vplus1 - 2 * V0 + Vminus1) / (dt * dt);
    }
    return derivative;
}

// D:206-226
Vector3 ParametricVolume::d2Vdrds(double r, double s, double t) const {
    // Obtain the derivative approximately, via a finite-difference.
    double dr = 0.001;
    Vector3 dVds0 = this->dVds(r, s, t);
    Vector3 derivative;
    if (r + dr > 1.0) {
        // r is close to the r=1.0 boundary, use a one-sided difference.
        Vector3 dVdsminus1 = this->dVds(r - dr, s, t);
        derivative = (dVds0 - dVdsminus1) / dr;
    } else if (r - dr < 0.0) {
        // r is close to the r=0.0 boundary, use a one-sided difference.
        Vector3 dVdsplus1 = this->dVds(r + dr, s, t);
        derivative = (dVdsplus1 - dVds0) / dr;
    } else {
        // Not near a boundary, use central-difference.
        Vector3 dVdsminus1 = this->dVds(r - dr, s, t);
        Vector3 dVdsplus1 = this->dVds(r + dr, s, t);
        derivative = (dVdsplus1 - dVdsminus1) / (2.0 * dr);
    }
    return derivative;
}

// D:227-247
Vector3 ParametricVolume::d2Vdrdt(double r, double s, double t) const {
    // Obtain the derivative approximately, via a finite-difference.
    double dr = 0.001;
    Vector3 dVdt0 = this->dVdt(r, s, t);
    Vector3 derivative;
    if (r + dr > 1.0) {
        // r is close to the r=1.0 boundary, use a one-sided difference.
        Vector3 dVdtminus1 = this->dVdt(r - dr, s, t);
        derivative = (dVdt0 - dVdtminus1) / dr;
    } else if (r - dr < 0.0) {
        // r is close to the r=0.0 boundary, use a one-sided difference.
        Vector3 dVdtplus1 = this->dVdt(r + dr, s, t);
        derivative = (dVdtplus1 - dVdt0) / dr;
    } else {
        // Not near a boundary, use central-difference.
        Vector3 dVdtminus1 = this->dVdt(r - dr, s, t);
        Vector3 dVdtplus1 = this->dVdt(r + dr, s, t);
        derivative = (dVdtplus1 - dVdtminus1) / (2.0 * dr);
    }
    return derivative;
}

// D:248-268
Vector3 ParametricVolume::d2Vdsdt(double r, double s, double t) const {
    // Obtain the derivative approximately, via a finite-difference.
    double ds = 0.001;
    Vector3 dVdt0 = this->dVdt(r, s, t);
    Vector3 derivative;
    if (s + ds > 1.0) {
        // s is close to the s=1.0 boundary, use a one-sided difference.
        Vector3 dVdtminus1 = this->dVdt(r, s - ds, t);
        derivative = (dVdt0 - dVdtminus1) / ds;
    } else if (s - ds < 0.0) {
        // s is close to the s=0.0 boundary, use a one-sided difference.
        Vector3 dVdtplus1 = this->dVdt(r, s + ds, t);
        derivative = (dVdtplus1 - dVdt0) / ds;
    } else {
        // Not near a boundary, use central-difference.
        Vector3 dVdtminus1 = this->dVdt(r, s - ds, t);
        Vector3 dVdtplus1 = this->dVdt(r, s + ds, t);
        derivative = (dVdtplus1 - dVdtminus1) / (2.0 * ds);
    }
    return derivative;
}

// D:272-304
void write_volume_as_vtk_xml(const ParametricVolume& vol, const std::string& fileName, int nrPts,
                             int nsPts, int ntPts) {
    double dr = 1.0 / (nrPts - 1);
    double ds = 1.0 / (nsPts - 1);
    double dt = 1.0 / (ntPts - 1);
    // D's std.stdio.File closes (and so flushes) in its destructor, which runs even
    // when the body below throws. A raw FILE* here would leak the handle and, worse,
    // leave a 0-byte file where D leaves the bytes written up to the throw (measured:
    // 247 bytes after a mid-write failure, group 7 of
    // probes/geom/volume/probe_nurbsvolume.d). The guard restores the destructor scope.
    std::FILE* raw = std::fopen(fileName.c_str(), "w");
    if (raw == nullptr) {
        // Same measured Phobos File-open failure text as parametricsurface.cpp.
        std::string msg = "Cannot open file `" + fileName + "' in mode `w' (" +
                          std::strerror(errno) + ")";
        throw eil::ExceptionBase(msg, __FILE__, __LINE__);
    }
    struct File_guard {
        std::FILE* h;
        ~File_guard() { if (h) std::fclose(h); }
    } guard{raw};
    std::FILE* f = guard.h;
    auto w = [&f](const std::string& s) { std::fwrite(s.data(), 1, s.size(), f); };
    auto wl = [&w](const char* s) { w(s); w("\n"); };

    wl("<VTKFile type=\"StructuredGrid\" version=\"1.0\" header_type=\"UInt64\">");
    w(eil::format("  <StructuredGrid WholeExtent=\"%d %d %d %d %d %d\">", 0, nrPts - 1, 0,
                  nsPts - 1, 0, ntPts - 1) +
      "\n");
    w(eil::format("    <Piece Extent=\"%d %d %d %d %d %d\">", 0, nrPts - 1, 0, nsPts - 1, 0,
                  ntPts - 1) +
      "\n");
    wl("      <Points>");
    wl("        <DataArray type=\"Float32\" Name=\"Points\" NumberOfComponents=\"3\" "
       "format=\"ascii\">");
    for (int k = 0; k < ntPts; ++k) {
        for (int j = 0; j < nsPts; ++j) {
            for (int i = 0; i < nrPts; ++i) {
                auto r = i * dr;
                auto s = j * ds;
                auto t = k * dt;
                auto p = vol(r, s, t);
                // D:290-294 version(complex_numbers) prints p.x.re/p.y.re/p.z.re and
                // the else-branch prints p.x/p.y/p.z. eil::re() is that else-branch
                // for a real number and the .re of the complex one, so one spelling
                // covers both variant states.
                w(eil::format("       %20.16e %20.16e %20.16e", eil::re(p.x), eil::re(p.y),
                              eil::re(p.z)) +
                  "\n");
            }
        }
    }
    wl("        </DataArray>");
    wl("      </Points>");
    wl("    </Piece>");
    wl("  </StructuredGrid>");
    wl("</VTKFile>");   // guard closes and flushes, as D's File destructor does
}

} // namespace geom
} // namespace eil
