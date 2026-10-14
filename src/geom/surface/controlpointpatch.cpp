// geom/surface/controlpointpatch.d -> src/geom/surface/controlpointpatch.cpp
// (module geom.surface.controlpointpatch)

#include "geom/surface/controlpointpatch.hpp"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "eil/error.hpp"
#include "eil/format.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/surface/aopatch.hpp"
#include "geom/surface/channelpatch.hpp"
#include "geom/surface/coonspatch.hpp"

namespace eil {
namespace geom {
// D's signed `arr.length = L` goes through eil::Array::set_length_signed now
// (substrate/include/eil/array.hpp) rather than a second local copy of the rule.


// D:41-53.
ControlPointPatch::ControlPointPatch(const PathPtr& south, const PathPtr& north,
                                     const PathPtr& west, const PathPtr& east,
                                     const eil::Array<eil::Array<Vector3>>& C) {
    init_from_grid(south, north, west, east, C);
}

// The body of the 5-arg constructor, also reached from D:63's mid-body delegation.
void ControlPointPatch::init_from_grid(const PathPtr& south, const PathPtr& north,
                                       const PathPtr& west, const PathPtr& east,
                                       const eil::Array<eil::Array<Vector3>>& C) {
    mN = static_cast<int>(C.length()); // D:43 to!int(C.length)
    mM = static_cast<int>(C[0].length()); // D:44 — row 0 only, so a ragged grid is legal

    if (north) mNorth = north->dup(); // D:46
    if (east) mEast = east->dup();    // D:47
    if (south) mSouth = south->dup(); // D:48
    if (west) mWest = west->dup();    // D:49

    mC.set_length(static_cast<size_t>(mN)); // D:51
    for (size_t i = 0; i < static_cast<size_t>(mN); ++i) mC[i] = C[i].dup(); // D:52
}

// D:55-132.
ControlPointPatch::ControlPointPatch(const PathPtr& south, const PathPtr& north,
                                     const PathPtr& west, const PathPtr& east, int ncpi,
                                     int ncpj, const std::string& guidePatch) {
    // Let's construct, then alter
    eil::Array<eil::Array<Vector3>> C;
    C.set_length_signed(ncpi); // D:60 C.length = ncpi
    for (size_t i = 0; i < C.length(); ++i) C[i].set_length_signed(ncpj); // D:61

    init_from_grid(south, north, west, east, C); // D:63 this(south, north, west, east, C)

    double dr = 1.0 / (ncpi - 2);
    double ds = 1.0 / (ncpj - 2);
    eil::Array<double> rPos;
    rPos.set_length(static_cast<size_t>(ncpi)); // D:67
    eil::Array<double> sPos;
    sPos.set_length(static_cast<size_t>(ncpj)); // D:68

    rPos[0] = 0.0;
    for (int i = 1; i < ncpi - 1; ++i) rPos[i] = dr / 2.0 + (i - 1) * dr; // D:71
    rPos[static_cast<int>(rPos.length()) - 1] = 1.0;                      // D:72 rPos[$-1]

    sPos[0] = 0.0;
    for (int i = 1; i < ncpj - 1; ++i) sPos[i] = ds / 2.0 + (i - 1) * ds; // D:75
    sPos[static_cast<int>(sPos.length()) - 1] = 1.0;                       // D:76 sPos[$-1]

    ParametricSurface::Ptr gpatch;
    bool usingChannelAsGuide = false;
    bool channelBoundedOnEW = false;
    // D:81-103 switches on a string; the chain keeps D's case order and all 17 spellings.
    if (guidePatch == "coons" || guidePatch == "Coons" || guidePatch == "COONS" ||
        guidePatch == "tfi" || guidePatch == "TFI") {
        gpatch = std::make_shared<CoonsPatch>(south, north, west, east);
    } else if (guidePatch == "channel" || guidePatch == "Channel" ||
               guidePatch == "CHANNEL") {
        gpatch = std::make_shared<ChannelPatch>(south, north, false, true);
        usingChannelAsGuide = true;
    } else if (guidePatch == "channel-e2w" || guidePatch == "Channel-e2w" ||
               guidePatch == "CHANNEL-E2W" || guidePatch == "channel_e2w") {
        gpatch = std::make_shared<ChannelPatch>(west, east, false, true);
        usingChannelAsGuide = true;
        channelBoundedOnEW = true;
    } else if (guidePatch == "aopatch" || guidePatch == "AOPatch" ||
               guidePatch == "AOPATCH" || guidePatch == "area-orthogonal" ||
               guidePatch == "area_orthogonal") {
        gpatch = std::make_shared<AOPatch>(south, north, west, east);
    } else {
        // D:97-102 — Error, not Exception. The message is four concatenations of which the
        // second is std.format.format("... '%s' ...\n", guidePatch); eil::format renders
        // %s of a string the same way (no quotes), and the \n characters are part of the
        // text, which the replay marks as <newline>.
        std::string errMsg = "Error in constructor to ControlPointPatch.\n";
        errMsg += eil::format("The guide patch type '%s' is not supported.\n", guidePatch);
        errMsg += "Check the spelling or what you are requesting.";
        errMsg += "Supported guide patches are: 'coons', 'channel', 'channel-e2w' and 'aopatch'";
        throw eil::ErrorBase(errMsg);
    }

    if (!channelBoundedOnEW) {
        for (int i = 0; i < ncpi; ++i) {
            for (int j = 0; j < ncpj; ++j) {
                mC[i][j] = (*gpatch)(rPos[i], sPos[j]);
            }
        }
        if (usingChannelAsGuide) {
            // correct the boundaries: east and west
            for (int j = 0; j < ncpj; ++j) {
                mC[0][j] = (*west)(sPos[j]);
                mC[static_cast<int>(mC.length()) - 1][j] = (*east)(sPos[j]); // mC[$-1]
            }
        }
    } else {
        // This case must be with a channel patch bounded on east-west
        for (int i = 0; i < ncpi; ++i) {
            for (int j = 0; j < ncpj; ++j) {
                mC[i][j] = (*gpatch)(sPos[j], rPos[i]); // note the swapped arguments, D:123
            }
        }
        // Correct the boundaries: south and north
        for (int i = 0; i < ncpi; ++i) {
            mC[i][0] = (*south)(rPos[i]);
            mC[i][static_cast<int>(mC[i].length()) - 1] = (*north)(rPos[i]); // [j][$-1]
        }
    }
}

// D:134-144.
ControlPointPatch::ControlPointPatch(const ControlPointPatch& other) {
    mN = other.mN;
    mM = other.mM;
    if (other.mNorth) mNorth = other.mNorth->dup();
    if (other.mEast) mEast = other.mEast->dup();
    if (other.mSouth) mSouth = other.mSouth->dup();
    if (other.mWest) mWest = other.mWest->dup();
    mC.set_length(other.mC.length());
    for (size_t i = 0; i < mC.length(); ++i) mC[i] = other.mC[i].dup();
}

// D:146-149
std::shared_ptr<ParametricSurface> ControlPointPatch::dup() const {
    return std::make_shared<ControlPointPatch>(mSouth, mNorth, mWest, mEast, mC);
}

// D:161-174
Vector3 ControlPointPatch::operator()(double r, double s) const {
    // Convert r, s to r_hat, s_hat
    // 0 <= r, s <= 1
    double r_hat = r * (mN - 2) + 1.0;
    double s_hat = s * (mM - 2) + 1.0;

    Vector3 p = T(r_hat, s_hat);
    if (mWest) p += (1.0 - G(r_hat, 1)) * ((*mWest)(s) - F(s_hat, 1));
    if (mEast) p += G(r_hat, mN - 1) * ((*mEast)(s) - F(s_hat, mN));
    if (mSouth) p += (1.0 - H(s_hat, 1)) * ((*mSouth)(r) - E(r_hat, 1));
    if (mNorth) p += H(s_hat, mM - 1) * ((*mNorth)(r) - E(r_hat, mM));
    return p;
}

// D:176-179 — a constant, exactly as in D.
std::string ControlPointPatch::toString() const { return "ControlPointPatch()"; }

// Not a D symbol (see header note).
std::string ControlPointPatch::classString() const { return "ControlPointPatch"; }

// D:181-184
void ControlPointPatch::nCtrlPts(int& nci, int& ncj) {
    nci = static_cast<int>(mC.length());
    ncj = static_cast<int>(mC[0].length());
}

// D:186-189
Vector3 ControlPointPatch::getCtrlPt(int i, int j) { return mC[i][j]; }

// D:191-194
void ControlPointPatch::setCtrlPt(int i, int j, Vector3 p) { mC[i][j] = p; }

// D:196-222
void ControlPointPatch::writeCtrlPtsAsVtkXml(const std::string& baseFileName) {
    auto n = mC.length();
    auto m = mC[0].length();

    // We'll take care of the extension because the VTK-XML files
    // are fussy about extensions when using the XML
    std::string ext = ".vts";
    const std::string path = baseFileName + ext;
    std::FILE* f = std::fopen(path.c_str(), "w");
    if (f == nullptr) {
        // std.stdio.File raises std.exception.ErrnoException; the substrate has none, so
        // this reproduces its text the way write_surface_as_vtk_xml does
        // (src/geom/surface/parametricsurface.cpp:49-58). Measured against ldc2 at row
        // cpp.vtk.badpath of baselines/e587b235/probes/geom_surface_controlpointpatch.txt.
        const std::string msg = "Cannot open file `" + path + "' in mode `w' (" +
                                std::strerror(errno) + ")";
        throw eil::ExceptionBase(msg, __FILE__, __LINE__);
    }
    auto wl = [&f](const char* s) { std::fputs(s, f); std::fputc('\n', f); };
    wl("<VTKFile type=\"StructuredGrid\" version=\"1.0\" header_type=\"UInt64\">");
    std::fprintf(f, "%s\n",
                 eil::format("  <StructuredGrid WholeExtent=\"%d %d %d %d 0 0\">", 0,
                             static_cast<int>(n) - 1, 0, static_cast<int>(m) - 1)
                     .c_str());
    std::fprintf(f, "%s\n",
                 eil::format("    <Piece Extent=\"%d %d %d %d 0 0\">", 0, static_cast<int>(n) - 1,
                             0, static_cast<int>(m) - 1)
                     .c_str());
    wl("      <Points>");
    wl("        <DataArray type=\"Float32\" Name=\"Points\" NumberOfComponents=\"3\" "
       "format=\"ascii\">");
    for (size_t j = 0; j < m; ++j) {          // D:210 — j outer
        for (size_t i = 0; i < n; ++i) {      // D:211 — i inner
            auto p = mC[i][j];
            std::fprintf(f, "       %20.16e %20.16e %20.16e\n", eil::re(p.x), eil::re(p.y),
                         eil::re(p.z));
        }
    }
    wl("        </DataArray>");
    wl("      </Points>");
    wl("    </Piece>");
    wl("  </StructuredGrid>");
    wl("</VTKFile>");
    std::fclose(f); // D:221 f.close()
}

// D:224-232 — Eq (4) in Eiseman (1988).
double ControlPointPatch::Omega(double r) const {
    if (r < -1.0) return 0.0;
    if (-1.0 <= r && r < 0.0) return 0.5 * (r + 1.0) * (r + 1.0);
    if (0 <= r && r <= 1.0) return 1.0 - 0.5 * (r - 1.0) * (r - 1.0);
    // all other cases, 1 < r
    return 1.0;
}

// D:234-242 — Eq (6) in Eiseman (1988).
double ControlPointPatch::G(double r, int alpha) const {
    // Deal with special end cases
    if (alpha == 1) return 2.0 * Omega(r - 1.0) - 1.0;
    if (alpha == mN - 1) return 2.0 * Omega(r - mN + 1.0);
    // For other cases
    return Omega(r - alpha);
}

// D:244-250
double ControlPointPatch::H(double s, int beta) const {
    if (beta == 1) return 2.0 * Omega(s - 1.0) - 1.0;
    if (beta == mM - 1) return 2.0 * Omega(s - mM + 1.0);
    // For other cases
    return Omega(s - beta);
}

// D:253-270 — Eq (7) in Eiseman (1988).
Vector3 ControlPointPatch::E(double r, int j) const {
    // REVISIT A POTENTIAL OPTIMISATION
    // It looks like we only ever use the displacement Vectors (C_alpha,j - C_alpha-1,j)
    // so we could pre-compute and store these.
    // (Plus we'd need starting points.)
    // The alpha index is annoying here. We need to work with 1-based in the G function
    // call and 0-based in the access to mC, so C_{alpha+1,j} becomes mC[alpha][j].
    Vector3 p = mC[0][j - 1];
    for (int alpha = 1; alpha < mN; ++alpha) {
        p += G(r, alpha) * (mC[alpha][j - 1] - mC[alpha - 1][j - 1]);
    }
    return p;
}

// D:272-284 — Eq (8) in Eiseman (1988).
Vector3 ControlPointPatch::F(double s, int i) const {
    // As above for E, now the beta index is annoying: C_{i,beta+1} becomes mC[i][beta].
    Vector3 p = mC[i - 1][0];
    for (int beta = 1; beta < mM; ++beta) {
        p += H(s, beta) * (mC[i - 1][beta] - mC[i - 1][beta - 1]);
    }
    return p;
}

// D:286-294 — Eq (9) in Eiseman (1988).
Vector3 ControlPointPatch::T(double r, double s) const {
    Vector3 p = E(r, 1);
    for (int beta = 1; beta < mM; ++beta) {
        p += H(s, beta) * (E(r, beta + 1) - E(r, beta));
    }
    return p;
}

} // namespace geom
} // namespace eil
