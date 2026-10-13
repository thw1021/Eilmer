// geom/surface/parametricsurface.d -> src/geom/surface/parametricsurface.cpp
// (module geom.surface.parametricsurface)

#include "geom/surface/parametricsurface.hpp"

#include <cstdio>
#include <cstring>
#include <string>

#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "geom/elements/properties.hpp"

namespace eil {
namespace geom {

// D:40-66
Vector3 ParametricSurface::area(int nr, int ns) {
    Vector3 vector_area;
    vector_area.set(0.0, 0.0, 0.0);
    double dr = 1.0 / nr;
    double ds = 1.0 / ns;
    eil::Array<eil::Array<Vector3>> p;
    p.set_length(static_cast<size_t>(nr) + 1);
    for (int i = 0; i < nr + 1; ++i) {
        double r = dr * i;
        for (int j = 0; j < ns + 1; ++j) {
            double s = ds * j;
            p[static_cast<size_t>(i)].append((*this)(r, s));
        }
    }
    number dA;
    Vector3 centroid;
    Vector3 n, t1, t2;
    for (int i = 0; i < nr; ++i) {
        for (int j = 0; j < ns; ++j) {
            quad_properties(p[i][j], p[i + 1][j], p[i + 1][j + 1], p[i][j + 1], centroid, n, t1,
                            t2, dA);
            n *= dA;
            vector_area += n;
        }
    }
    return vector_area;
} // end area()

// D:70-94
void write_surface_as_vtk_xml(const ParametricSurface& surf, const std::string& fileName,
                              int nrPts, int nsPts) {
    double dr = 1.0 / (nrPts - 1);
    double ds = 1.0 / (nsPts - 1);
    std::FILE* f = std::fopen(fileName.c_str(), "w");
    if (f == nullptr) {
        // Measured Phobos text of `File(name, "w")` on an unusable path, as reproduced
        // for SVGContext (src/geom/misc/svg.cpp:139-147); std.exception.ErrnoException
        // has no substrate equivalent yet, so the class is ExceptionBase.
        std::string msg = "Cannot open file `" + fileName + "' in mode `w' (" +
                          std::strerror(errno) + ")";
        throw eil::ExceptionBase(msg, __FILE__, __LINE__);
    }
    auto w = [&f](const std::string& s) { std::fwrite(s.data(), 1, s.size(), f); };
    // D's File.writeln(s) and File.writefln(fmt, args) are format-then-newline;
    // substrate has no writefln-to-stream, so eil::format + "\n" is the same bytes.
    auto wl = [&w](const char* s) { w(s); w("\n"); };

    wl("<VTKFile type=\"StructuredGrid\" version=\"1.0\" header_type=\"UInt64\">");
    w(eil::format("  <StructuredGrid WholeExtent=\"%d %d %d %d 0 0\">", 0, nrPts - 1, 0,
                  nsPts - 1) +
      "\n");
    w(eil::format("    <Piece Extent=\"%d %d %d %d 0 0\">", 0, nrPts - 1, 0, nsPts - 1) + "\n");
    wl("      <Points>");
    wl("        <DataArray type=\"Float32\" Name=\"Points\" NumberOfComponents=\"3\" "
       "format=\"ascii\">");
    for (int j = 0; j < nsPts; ++j) {
        for (int i = 0; i < nrPts; ++i) {
            auto r = i * dr;
            auto s = j * ds;
            auto p = surf(r, s);
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
