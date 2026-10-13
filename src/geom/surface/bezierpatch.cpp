// geom/surface/bezierpatch.d -> src/geom/surface/bezierpatch.cpp
// (module geom.surface.bezierpatch)

#include "geom/surface/bezierpatch.hpp"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>

#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "geom/gpath/bezier.hpp"
#include "geom/surface/parametricsurface.hpp"

namespace eil {
namespace geom {
namespace {

// D's `arr.length = L` with a signed L. Measured against ldc2 1.40 (row bzp.nm2.ctor of
// baselines/e587b235/probes/geom_surface_bezierpatch.txt): a negative length raises an
// Error whose message is exactly "Memory allocation failed", while 0 is accepted
// (bzp.nm1.ctor threw=none). eil::Array::set_length takes size_t, so the wrap has to be
// caught before the conversion or the port would try to allocate 2^64 elements instead
// of failing the way D does.
void set_length_checked(eil::Array<eil::Array<Vector3>>& a, long len) {
    if (len < 0) throw eil::ErrorBase("Memory allocation failed", __FILE__, __LINE__);
    a.set_length(static_cast<size_t>(len));
}
void set_length_checked(eil::Array<std::shared_ptr<Bezier>>& a, long len) {
    if (len < 0) throw eil::ErrorBase("Memory allocation failed", __FILE__, __LINE__);
    a.set_length(static_cast<size_t>(len));
}
void set_length_checked(eil::Array<Vector3>& a, long len) {
    if (len < 0) throw eil::ErrorBase("Memory allocation failed", __FILE__, __LINE__);
    a.set_length(static_cast<size_t>(len));
}

} // namespace

// D:26-35
BezierPatch::BezierPatch(const eil::Array<eil::Array<Vector3>>& _Q, int _n, int _m) {
    n = _n;
    m = _m;
    set_length_checked(Q, static_cast<long>(_n) + 1); // D:30
    for (size_t i = 0; i < Q.length(); ++i) {         // D:31 foreach (i; 0 .. Q.length)
        Q[i] = _Q[i].dup();                           // D:32
    }
    initialiseWorkingSpace();
}

// D:37-46. `m = other.n` is D, verbatim (see header): the copied patch's m is the
// other one's n, which is observable through the member and is NOT corrected here.
BezierPatch::BezierPatch(const BezierPatch& other) {
    n = other.n;
    m = other.n; // D:40 -- deliberately not other.m
    set_length_checked(Q, static_cast<long>(other.Q.length()));
    for (size_t i = 0; i < Q.length(); ++i) { // D:42 foreach (i; 0 .. Q.length)
        Q[i] = other.Q[i].dup();
    }
    initialiseWorkingSpace();
}

// D:48-51
std::shared_ptr<ParametricSurface> BezierPatch::dup() const {
    return std::make_shared<BezierPatch>(Q, n, m);
}

// D:53-63
Vector3 BezierPatch::operator()(double r, double s) const {
    eil::Array<Vector3> B;
    set_length_checked(B, static_cast<long>(n) + 1); // D:56 B.length = n + 1
    for (int i = 0; i < n + 1; ++i) {                // D:57 foreach (i; 0 .. n + 1)
        auto p = (*_b1s[static_cast<size_t>(i)])(s);
        B[static_cast<size_t>(i)].set(p);
    }
    Bezier b2(B); // D:61 `Bezier b2 = new Bezier(B);`
    return b2(r);
}

// D:65-68
std::string BezierPatch::toString() const { return "BezierPatch()"; }

// Not a D symbol (see header note): D's ParametricSurface hierarchy has no
// classString(), so this is the port-side class name the logging contract asks for.
std::string BezierPatch::classString() const { return "BezierPatch"; }

// D:70-76
void BezierPatch::initialiseWorkingSpace() {
    set_length_checked(_b1s, static_cast<long>(n) + 1); // D:72
    for (int i = 0; i < n + 1; ++i) {                   // D:73 foreach (i; 0 .. n + 1)
        _b1s[static_cast<size_t>(i)] = std::make_shared<Bezier>(Q[static_cast<size_t>(i)]);
    }
}

// D:82-105
void write_ctrl_pts_as_vtk_xml(const BezierPatch& bPatch, const std::string& fileName) {
    auto n = bPatch.Q.length();
    auto m = bPatch.Q[0].length(); // D:85 -- Q[0]'s length is the j bound for every row

    std::FILE* f = std::fopen(fileName.c_str(), "w");
    if (f == nullptr) {
        // Measured Phobos text of `File(fileName, "w")` on an unusable path
        // (row bzp.vtk.badpath), reproduced the way parametricsurface.cpp:52-60 does;
        // std.exception.ErrnoException has no substrate equivalent yet, so the escaped
        // type is ExceptionBase.
        std::string msg = "Cannot open file `" + fileName + "' in mode `w' (" +
                          std::strerror(errno) + ")";
        throw eil::ExceptionBase(msg, __FILE__, __LINE__);
    }
    auto w = [&f](const std::string& s) { std::fwrite(s.data(), 1, s.size(), f); };
    auto wl = [&w](const char* s) { w(s); w("\n"); };

    wl("<VTKFile type=\"StructuredGrid\" version=\"1.0\" header_type=\"UInt64\">");
    w(eil::format("  <StructuredGrid WholeExtent=\"%d %d %d %d 0 0\">", 0, static_cast<int>(n) - 1,
                  0, static_cast<int>(m) - 1) +
      "\n");
    w(eil::format("    <Piece Extent=\"%d %d %d %d 0 0\">", 0, static_cast<int>(n) - 1, 0,
                  static_cast<int>(m) - 1) +
      "\n");
    wl("      <Points>");
    wl("        <DataArray type=\"Float32\" Name=\"Points\" NumberOfComponents=\"3\" "
       "format=\"ascii\">");
    for (size_t j = 0; j < m; ++j) { // D:93 foreach (j; 0 .. m)
        for (size_t i = 0; i < n; ++i) {
            auto p = bPatch.Q[i][j];
            w(eil::format("       %20.16e %20.16e %20.16e", eil::re(p.x), eil::re(p.y),
                          eil::re(p.z)) +
              "\n");
        }
    }
    wl("        </DataArray>");
    wl("      </Points>");
    wl("    </Piece>");
    wl("  </StructuredGrid>");
    wl("</VTKFile>");
    std::fclose(f);
}

} // namespace geom
} // namespace eil
