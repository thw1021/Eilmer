// tests/test_geom_grid_grid.cpp — C++ mirror of probes/geom/grid/probe_grid.d,
// replayed row for row against baselines/e587b235/probes/geom_grid_grid.txt
// (643 rows captured from ldc2 1.40.0 at gdtk e587b235).
//
// geom/grid/grid.d has no `unittest` block — proved by
//   grep -c unittest /home/tang/packages/gdtk/src/geom/grid/grid.d  ->  0
// so this replay IS the gate for the abstract Grid base, exactly as the probe is.
// The D module's four `assert(...)` sites (grid.d:73-75, :83) are in-contract checks
// that only fire on out-of-range indices, and they are replayed here as rows
// idx.1d si(...) / idx.3d si(...) / idx.ijk 3d(27), not as a separate section.
//
// Row groups (the probe header documents why each one is load-bearing):
//   gt.*   gridTypeName / gridTypeFromName, the unknown-name Error text, and proof
//          that a D Error is not caught as an Exception.
//   idx.*  single_index / ijk_indices over 1D/2D/3D lattices, k defaulting, the four
//          contract asserts, and the vertices.length-vs-nvertices distinction.
//   ab.*   all 17 abstract members through a Grid handle (the pure-virtual table).
//   wr.*   Grid.write(fileName, fmt) dispatch, including the unknown and empty fmts
//          that D's `default:` arm sends to the gzip writer.
//   ccp.*  compute_cell_properties for both true_centroid branches over every cell
//          type, plus the two "Unhandled number of vertices." Error arms.
//   pic.*  point_is_inside_cell at inside/outside/on-face/on-edge/on-vertex points
//          and all four GeometryException wordings.
//   cbc.*  cell_barycentre.
//   bin.*  get_bin_ix/iy/iz and sort_cells_into_bins, with the bin-occupancy dumps,
//          the 1e-6 delta floor, the 2D nbinz override, the zero-count exception, the
//          double-sort append behaviour and the D clamp's signed comparison.
//   fsc.*  find_enclosing_cell_fast / _slow / dispatcher, sorted and unsorted.
//   fncc.* find_nearest_cell_centre, including the strict-`<` tie rows.
//   stl.*  write_to_stl_file whole file bodies, newlines marked, including the NaN
//          vertex artefact.
//   vtk.*  write_to_vtk_file dispatch through the base handle and its artefact body.
//   rot.*  rotate(Quaternion, ctr) and rotate(theta, axis, ctr) with identity, 90 and
//          180 degree cases, centres, invalid norms and NaN propagation.
//
// Regenerate the baseline:
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/grid
//     probes/geom/grid/probe_grid.d -of=/tmp/probe_g1 -od/tmp/probe_obj_g1
//     and then /tmp/probe_g1 > baselines/e587b235/probes/geom_grid_grid.txt
//
// Run: ./test_geom_grid_grid <baseline.txt>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/number.hpp"
#include "geom/elements/nomenclature.hpp"
#include "geom/elements/properties.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/grid/grid.hpp"

namespace {

using eil::geom::Grid;
using eil::geom::Grid_t;
using eil::geom::Quaternion;
using eil::geom::Vector3;

std::vector<std::string> g_lines;
eil::Array<std::string> writer_log; // mirrors the probe's module-level log

void emit(const std::string& s) { g_lines.push_back(s); }

// D's fill_copy(): writes all 8 slots so the tail of the static array is observable.
void fill_copy(std::array<size_t, 8>& dest, const eil::Array<size_t>& src, size_t& nvtx) {
    for (size_t t = 0; t < 8; ++t) {
        dest[t] = (t < src.length()) ? src[t] : 999;
    }
    nvtx = src.length();
}

// The probe's write_marker(): one text line describing what the abstract writer was
// asked to do. A plain file here, not a gzip container — grid.d itself has no gzip
// code (the .gz artefact comparison belongs to sgrid/usgrid, G4/G5).
void write_marker(const std::string& kind, const std::string& fileName, const Grid* g,
                  double scale) {
    std::FILE* raw = std::fopen(fileName.c_str(), "w");
    if (raw == nullptr) throw eil::ExceptionBase("cannot open " + fileName, __FILE__, __LINE__);
    std::string line = eil::format(
        "marker kind=%s scale=%.17g ncells=%d nvertices=%d dims=%d label=%s", kind, scale,
        g->ncells, g->nvertices, g->dimensions, g->label);
    std::fwrite(line.data(), 1, line.size(), raw);
    std::fwrite("\n", 1, 1, raw);
    std::fclose(raw);
}

// D's `void delegate()` helpers land in tryit(), which tags which side of D's
// Throwable split a call left by: `err=` is an Error, `exc=` an Exception.
void tryit(const std::string& tag, const std::function<void()>& dg) {
    try {
        dg();
        emit(tag + " done");
    }
    catch (const eil::ExceptionBase& e) {
        emit(tag + " exc=" + e.msg());
    }
    catch (const eil::ErrorBase& e) {
        emit(tag + " err=" + e.msg());
    }
}

void dump3(const std::string& tag, const Vector3& v) {
    emit(eil::format("%s x=%.17g y=%.17g z=%.17g", tag, eil::re(v.x), eil::re(v.y),
                     eil::re(v.z)));
}

std::string one_line(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::string out;
    size_t pos = 0;
    while (true) {
        size_t nl = body.find('\n', pos);
        if (nl == std::string::npos) {
            out += body.substr(pos);
            break;
        }
        out += body.substr(pos, nl - pos) + "<newline>";
        pos = nl + 1;
    }
    return out;
}

size_t len_of(const std::string& path) {
    std::ifstream in(path, std::ios::ate | std::ios::binary);
    return static_cast<size_t>(in.tellg());
}

// The probe's ProbeGrid: the concrete subclass that makes the 17 pure virtuals of
// grid.d:93-122 observable through a base-class handle. Same body, same id lists,
// same marker files.
class ProbeGrid : public Grid {
  public:
    using Bins2 = eil::Array<eil::Array<size_t>>;

    Bins2 ijk_cell_ids; // D: the same lists, addressed by single_index(i,j,k)

    ProbeGrid(Grid_t gt, int dims, const std::string& lbl) : Grid(gt, dims, lbl) {}

    void load(const eil::Array<Vector3>& vs, size_t ni, size_t nj, size_t nk,
              const Bins2& cids) {
        vertices = vs;
        niv = ni; njv = nj; nkv = nk;
        nvertices = vs.length();
        cell_ids = cids;
        ncells = cids.length();
        ijk_cell_ids.clear();
        for (size_t t = 0; t < ni * nj * nk; ++t) {
            ijk_cell_ids.append(cids[t % cids.length()]);
        }
    }

    Vector3* opIndex(size_t i, size_t j, size_t k) override {
        return &vertices[single_index(i, j, k)];
    }
    Vector3* opIndex(size_t indx) override { return &vertices[indx]; }

    eil::Array<size_t> get_vtx_id_list_for_cell(size_t i, size_t j, size_t k) const override {
        return ijk_cell_ids[single_index(i, j, k)];
    }
    void copy_vtx_id_list_for_cell(std::array<size_t, 8>& vtx_list_copy, size_t& nvtx, size_t i,
                                   size_t j, size_t k) const override {
        fill_copy(vtx_list_copy, ijk_cell_ids[single_index(i, j, k)], nvtx);
    }
    eil::Array<size_t> get_vtx_id_list_for_cell(size_t indx) const override {
        return cell_ids[indx];
    }
    void copy_vtx_id_list_for_cell(std::array<size_t, 8>& vtx_list_copy, size_t& nvtx,
                                   size_t indx) const override {
        fill_copy(vtx_list_copy, cell_ids[indx], nvtx);
    }

    void read_from_gzip_file(const std::string& fileName, double scale) override {
        writer_log.append(eil::text("read_from_gzip_file ", fileName, " ", scale));
    }
    void read_from_raw_binary_file(const std::string& fileName, double scale) override {
        writer_log.append(eil::text("read_from_raw_binary_file ", fileName, " ", scale));
    }
    void write_to_gzip_file(const std::string& fileName) override {
        writer_log.append(eil::text("write_to_gzip_file ", fileName));
        write_marker("gziptext", fileName, this, 1.0);
    }
    void write_to_raw_binary_file(const std::string& fileName) override {
        writer_log.append(eil::text("write_to_raw_binary_file ", fileName));
        write_marker("rawbinary", fileName, this, 1.0);
    }
    void write_to_vtk_file(const std::string& fileName) override;
    void write_to_su2_file(const std::string& fileName, double scale,
                           bool use_gmsh_order_for_wedges) override {
        writer_log.append(eil::text("write_to_su2_file ", fileName, " ", scale, " ",
                                    use_gmsh_order_for_wedges));
        write_marker("su2", fileName, this, scale);
    }

    size_t number_of_vertices_for_cell(size_t i) override { return cell_ids[i].length(); }

    int vtk_element_type_for_cell(size_t i) override {
        switch (cell_ids[i].length()) {
          case 3: return static_cast<int>(eil::geom::triangle);
          case 4: return static_cast<int>(eil::geom::quad);
          case 5: return static_cast<int>(eil::geom::pyramid);
          case 6: return static_cast<int>(eil::geom::wedge);
          case 8: return static_cast<int>(eil::geom::hexahedron);
          default: return -1;
        }
    }
    int get_cell_type(size_t i) override { return static_cast<int>(cell_ids[i].length()); }

    std::shared_ptr<Grid> get_boundary_grid(size_t boundary_indx) override {
        if (boundary_indx > 2) return nullptr;
        auto b = std::make_shared<ProbeGrid>(grid_type, dimensions,
                                             eil::text("boundary_", boundary_indx));
        eil::Array<Vector3> vs;
        vs.append(vertices[0]);
        vs.append(vertices[1]);
        Bins2 cid;
        cid.append(eil::Array<size_t>{0, 1});
        b->load(vs, 2, 1, 1, cid);
        return b;
    }

    eil::Array<size_t> get_list_of_boundary_cells(size_t boundary_indx) override {
        eil::Array<size_t> res;
        for (size_t i = 0; i < ncells; ++i) {
            if (i % (1 + boundary_indx) == 0) res.append(i);
        }
        return res;
    }

    Bins2 cell_ids;
};

void ProbeGrid::write_to_vtk_file(const std::string& fileName) {
    writer_log.append(eil::text("write_to_vtk_file ", fileName));
    std::FILE* raw = std::fopen(fileName.c_str(), "w");
    if (raw == nullptr) throw eil::ExceptionBase("cannot open " + fileName, __FILE__, __LINE__);
    struct File_guard {
        std::FILE* h;
        ~File_guard() { if (h) std::fclose(h); }
    } guard{raw};
    std::FILE* f = guard.h;
    auto wl = [&f](const std::string& s) {
        std::fwrite(s.data(), 1, s.size(), f);
        std::fwrite("\n", 1, 1, f);
    };
    wl("# vtk DataFile Version 3.0");
    wl(eil::format("%s grid", label));
    wl("ASCII");
    wl("DATASET UNSTRUCTURED_GRID");
    wl(eil::format("POINTS %d float", nvertices));
    for (size_t i = 0; i < vertices.length(); ++i) {
        const Vector3& v = vertices[i];
        wl(eil::format("%.6e %.6e %.6e", eil::re(v.x), eil::re(v.y), eil::re(v.z)));
    }
    size_t tot = 0;
    for (size_t i = 0; i < cell_ids.length(); ++i) tot += 1 + cell_ids[i].length();
    wl(eil::format("CELLS %d %d", ncells, tot));
    for (size_t i = 0; i < cell_ids.length(); ++i) {
        std::string line = eil::to_string(cell_ids[i].length());
        for (size_t q = 0; q < cell_ids[i].length(); ++q) {
            line += eil::text(" ", cell_ids[i][q]);
        }
        wl(line);
    }
    wl(eil::format("CELL_TYPES %d", ncells));
    for (size_t i = 0; i < ncells; ++i) wl(eil::to_string(vtk_element_type_for_cell(i)));
}

eil::Array<size_t> ids(std::initializer_list<int> vals) {
    eil::Array<size_t> res;
    for (int v : vals) res.append(static_cast<size_t>(v));
    return res;
}

Vector3 V(double x, double y, double z) { return Vector3(x, y, z); }

using ArrV = eil::Array<Vector3>;
using Cids = ProbeGrid::Bins2;

Cids cids_of(const std::vector<std::vector<int>>& rows) {
    Cids out;
    for (const auto& r : rows) {
        eil::Array<size_t> a;
        for (int v : r) a.append(static_cast<size_t>(v));
        out.append(a);
    }
    return out;
}

std::shared_ptr<ProbeGrid> mk_1d() {
    auto g = std::make_shared<ProbeGrid>(Grid_t::structured_grid, 1, "g1d");
    Cids cid;
    cid.append(ids({0, 1}));
    cid.append(ids({1, 2}));
    g->load(ArrV{V(0.0, 0.0, 0.0), V(0.5, 0.0, 0.0), V(1.5, 0.0, 0.0)}, 3, 1, 1, cid);
    return g;
}

std::shared_ptr<ProbeGrid> mk_2d_tri() {
    auto g = std::make_shared<ProbeGrid>(Grid_t::structured_grid, 2, "tri2d");
    g->load(ArrV{V(0.0, 0.0, 0.0), V(1.0, 0.0, 0.0), V(0.0, 1.0, 0.0)}, 3, 1, 1,
            cids_of({{0, 1, 2}}));
    return g;
}

std::shared_ptr<ProbeGrid> mk_2d_quad() {
    auto g = std::make_shared<ProbeGrid>(Grid_t::unstructured_grid, 2, "quad2d");
    g->load(ArrV{V(0.125, 0.25, 0.0), V(0.875, 0.25, 0.0), V(0.875, 0.75, 0.0),
                 V(0.125, 0.75, 0.0)},
            4, 1, 1, cids_of({{0, 1, 2, 3}}));
    return g;
}

std::shared_ptr<ProbeGrid> mk_2d_grid32() {
    auto g = std::make_shared<ProbeGrid>(Grid_t::structured_grid, 2, "grid32");
    ArrV vs;
    for (size_t j = 0; j < 3; ++j) {
        for (size_t i = 0; i < 4; ++i) vs.append(V(static_cast<double>(i), static_cast<double>(j), 0.0));
    }
    Cids cid;
    for (size_t j = 0; j < 2; ++j) {
        for (size_t i = 0; i < 3; ++i) {
            eil::Array<size_t> row;
            row.append(i + 4 * j);
            row.append(i + 1 + 4 * j);
            row.append(i + 1 + 4 * (j + 1));
            row.append(i + 4 * (j + 1));
            cid.append(row);
        }
    }
    g->load(vs, 4, 3, 1, cid);
    return g;
}

std::shared_ptr<ProbeGrid> mk_3d_tetra() {
    auto g = std::make_shared<ProbeGrid>(Grid_t::unstructured_grid, 3, "tet3d");
    g->load(ArrV{V(0.0, 0.0, 0.0), V(1.0, 0.0, 0.0), V(0.0, 1.0, 0.0), V(0.0, 0.0, 1.0)}, 4, 1,
            1, cids_of({{0, 1, 2, 3}}));
    return g;
}

std::shared_ptr<ProbeGrid> mk_3d_hex_skew() {
    auto g = std::make_shared<ProbeGrid>(Grid_t::unstructured_grid, 3, "hexskew");
    g->load(ArrV{V(0.0, 0.0, 0.0), V(1.0, 0.125, 0.0), V(1.125, 1.0, 0.25),
                 V(0.0, 0.875, 0.125), V(0.25, 0.25, 1.0), V(1.0, 0.375, 1.0),
                 V(1.25, 1.0, 1.0), V(0.0, 1.0, 1.0)},
            2, 2, 2, cids_of({{0, 1, 2, 3, 4, 5, 6, 7}}));
    return g;
}

std::shared_ptr<ProbeGrid> mk_3d_hex_unit() {
    auto g = std::make_shared<ProbeGrid>(Grid_t::unstructured_grid, 3, "hexunit");
    g->load(ArrV{V(0.0, 0.0, 0.0), V(1.0, 0.0, 0.0), V(1.0, 1.0, 0.0), V(0.0, 1.0, 0.0),
                 V(0.0, 0.0, 1.0), V(1.0, 0.0, 1.0), V(1.0, 1.0, 1.0), V(0.0, 1.0, 1.0)},
            2, 2, 2, cids_of({{0, 1, 2, 3, 4, 5, 6, 7}}));
    return g;
}

std::shared_ptr<ProbeGrid> mk_3d_pyramid() {
    auto g = std::make_shared<ProbeGrid>(Grid_t::unstructured_grid, 3, "pyr3d");
    g->load(ArrV{V(0.0, 0.0, 0.0), V(1.0, 0.0, 0.0), V(1.0, 1.0, 0.0), V(0.0, 1.0, 0.0),
                 V(0.125, 0.25, 1.0)},
            2, 2, 2, cids_of({{0, 1, 2, 3, 4}}));
    return g;
}

std::shared_ptr<ProbeGrid> mk_3d_wedge() {
    auto g = std::make_shared<ProbeGrid>(Grid_t::unstructured_grid, 3, "wedge3d");
    g->load(ArrV{V(0.0, 0.0, 0.0), V(0.5, 1.0, 0.0), V(1.0, 0.0, 0.0), V(0.0, 0.0, 1.0),
                 V(0.5, 1.0, 1.0), V(1.0, 0.0, 1.0)},
            2, 2, 2, cids_of({{0, 1, 2, 3, 4, 5}}));
    return g;
}

std::shared_ptr<ProbeGrid> mk_3d_grid222() {
    auto g = std::make_shared<ProbeGrid>(Grid_t::structured_grid, 3, "grid222");
    ArrV vs;
    for (size_t k = 0; k < 3; ++k) {
        for (size_t j = 0; j < 3; ++j) {
            for (size_t i = 0; i < 3; ++i) {
                vs.append(V(static_cast<double>(i) * 0.5, static_cast<double>(j) * 0.5,
                            static_cast<double>(k) * 0.5));
            }
        }
    }
    Cids cid;
    for (size_t k = 0; k < 2; ++k) {
        for (size_t j = 0; j < 2; ++j) {
            for (size_t i = 0; i < 2; ++i) {
                size_t a = i + 3 * j + 9 * k;
                size_t b = i + 1 + 3 * j + 9 * k;
                size_t c = i + 1 + 3 * (j + 1) + 9 * k;
                size_t d = i + 3 * (j + 1) + 9 * k;
                size_t e = i + 3 * j + 9 * (k + 1);
                size_t f = i + 1 + 3 * j + 9 * (k + 1);
                size_t uu = i + 1 + 3 * (j + 1) + 9 * (k + 1);
                size_t w = i + 3 * (j + 1) + 9 * (k + 1);
                eil::Array<size_t> row;
                row.append(a); row.append(b); row.append(c); row.append(d);
                row.append(e); row.append(f); row.append(uu); row.append(w);
                cid.append(row);
            }
        }
    }
    g->load(vs, 3, 3, 3, cid);
    return g;
}

std::shared_ptr<ProbeGrid> mk_2d_pent() {
    auto g = std::make_shared<ProbeGrid>(Grid_t::unstructured_grid, 2, "pent2d");
    g->load(ArrV{V(0.0, 0.0, 0.0), V(1.0, 0.0, 0.0), V(1.0, 1.0, 0.0), V(0.0, 1.0, 0.0),
                 V(0.5, 0.5, 0.0)},
            5, 1, 1, cids_of({{0, 1, 2, 3, 4}}));
    return g;
}

std::shared_ptr<ProbeGrid> mk_3d_hept() {
    auto g = std::make_shared<ProbeGrid>(Grid_t::unstructured_grid, 3, "hept3d");
    g->load(ArrV{V(0.0, 0.0, 0.0), V(1.0, 0.0, 0.0), V(1.0, 1.0, 0.0), V(0.0, 1.0, 0.0),
                 V(0.0, 0.0, 1.0), V(1.0, 0.0, 1.0), V(1.0, 1.0, 1.0)},
            7, 1, 1, cids_of({{0, 1, 2, 3, 4, 5, 6}}));
    return g;
}

std::shared_ptr<ProbeGrid> mk_4d() {
    auto g = std::make_shared<ProbeGrid>(Grid_t::unstructured_grid, 4, "fourd");
    g->load(ArrV{V(0.0, 0.0, 0.0), V(1.0, 0.0, 0.0), V(0.0, 1.0, 0.0)}, 3, 1, 1,
            cids_of({{0, 1, 2}}));
    return g;
}

std::shared_ptr<ProbeGrid> mk_2d_flat() {
    auto g = std::make_shared<ProbeGrid>(Grid_t::unstructured_grid, 2, "flat2d");
    g->load(ArrV{V(0.0, 0.5, 0.0), V(1.0, 0.5, 0.0)}, 2, 1, 1, cids_of({{0, 1}}));
    return g;
}

std::shared_ptr<ProbeGrid> mk_1pt() {
    auto g = std::make_shared<ProbeGrid>(Grid_t::unstructured_grid, 3, "onept");
    g->load(ArrV{V(0.25, 0.25, 0.25)}, 1, 1, 1, cids_of({{0, 0, 0, 0}}));
    return g;
}

std::shared_ptr<ProbeGrid> mk_2d_nan() {
    auto g = std::make_shared<ProbeGrid>(Grid_t::unstructured_grid, 2, "nan2d");
    const double nanv = std::numeric_limits<double>::quiet_NaN();
    const double negnan = -nanv;
    g->load(ArrV{V(nanv, 0.0, 0.0), V(negnan, 1.0, 0.0), V(1.0, 0.0, 0.0)}, 3, 1, 1,
            cids_of({{0, 1, 2}}));
    return g;
}

void dump_props(const std::shared_ptr<Grid>& g, size_t indx, bool true_centroid,
                const std::string& tag) {
    Vector3 c;
    eil::number vol = eil::d_init<eil::number>();
    g->compute_cell_properties(indx, true_centroid, c, vol);
    emit(eil::format("%s tc=%s vol=%.17g cx=%.17g cy=%.17g cz=%.17g", tag, true_centroid,
                     eil::re(vol), eil::re(c.x), eil::re(c.y), eil::re(c.z)));
}

void dump_inside(const std::shared_ptr<Grid>& g, Vector3 p, size_t indx,
                 const std::string& tag) {
    bool inside = g->point_is_inside_cell(p, indx);
    emit(eil::format("%s inside=%s", tag, inside));
}

void dump_bins(const std::shared_ptr<Grid>& g, const std::string& tag) {
    emit(eil::format("%s nbx=%d nby=%d nbz=%d deltax=%.17g deltay=%.17g deltaz=%.17g "
                     "sorted=%s",
                     tag, g->nbx, g->nby, g->nbz, g->deltax, g->deltay, g->deltaz,
                     g->cells_are_sorted_into_bins));
    dump3(tag + ".bb0", g->bb0);
    dump3(tag + ".bb1", g->bb1);
    size_t nempty = 0;
    size_t placed = 0;
    for (size_t ix = 0; ix < g->nbx; ++ix) {
        for (size_t iy = 0; iy < g->nby; ++iy) {
            for (size_t iz = 0; iz < g->nbz; ++iz) {
                const eil::Array<size_t>& lst = g->bins[ix][iy][iz];
                placed += lst.length();
                if (lst.length() == 0) {
                    ++nempty;
                } else {
                    emit(eil::format("%s[%d][%d][%d]=%s", tag, ix, iy, iz, lst));
                }
            }
        }
    }
    emit(eil::format("%s empty=%d placed=%d depth=%d", tag, nempty, placed, g->bins.length()));
}

void dump_bin_indices(const std::shared_ptr<Grid>& g, Vector3 p, const std::string& tag) {
    emit(eil::format("%s ix=%d iy=%d iz=%d", tag, g->get_bin_ix(p), g->get_bin_iy(p),
                     g->get_bin_iz(p)));
}

void dump_search(const std::shared_ptr<Grid>& g, Vector3 p, const std::string& tag) {
    size_t indx = 0;
    bool found = false;
    tryit(eil::text(tag, " fast"), [&] {
        g->find_enclosing_cell_fast(p, indx, found);
        emit(eil::format("  found=%s indx=%d", found, indx));
    });
    tryit(eil::text(tag, " slow"), [&] {
        g->find_enclosing_cell_slow(p, indx, found);
        emit(eil::format("  found=%s indx=%d", found, indx));
    });
    tryit(eil::text(tag, " disp"), [&] {
        g->find_enclosing_cell(p, indx, found);
        emit(eil::format("  found=%s indx=%d", found, indx));
    });
}

void nearest(const std::shared_ptr<Grid>& g, Vector3 p, const std::string& tag) {
    size_t nc = 0;
    double md = 0.0;
    g->find_nearest_cell_centre(p, nc, md);
    emit(eil::format("%s nearest=%d dist=%.17g", tag, nc, md));
}

void dump_verts(const std::shared_ptr<Grid>& g, const std::string& tag) {
    emit(eil::format("%s nv=%d", tag, g->nvertices));
    for (size_t i = 0; i < g->nvertices; ++i) {
        dump3(eil::text(tag, " v", i), *g->opIndex(i));
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }

    //============================= gt: grid type names
    emit(eil::format("gt.name structured=%s unstructured=%s",
                     eil::geom::gridTypeName(Grid_t::structured_grid),
                     eil::geom::gridTypeName(Grid_t::unstructured_grid)));
    emit(eil::format("gt.from structured=%d unstructured=%d",
                     static_cast<int>(eil::geom::gridTypeFromName("structured_grid")),
                     static_cast<int>(eil::geom::gridTypeFromName("unstructured_grid"))));
    tryit("gt.from.empty", [] { emit(eil::format("  %d", static_cast<int>(eil::geom::gridTypeFromName("")))); });
    tryit("gt.from.unknown", [] { emit(eil::format("  %d", static_cast<int>(eil::geom::gridTypeFromName("tetrahedral")))); });
    tryit("gt.from.nearmiss", [] { emit(eil::format("  %d", static_cast<int>(eil::geom::gridTypeFromName("Structured_grid")))); });
    tryit("gt.from.dash", [] { emit(eil::format("  %d", static_cast<int>(eil::geom::gridTypeFromName("structured-grid")))); });
    tryit("gt.from.spaces", [] { emit(eil::format("  %d", static_cast<int>(eil::geom::gridTypeFromName(" structured_grid ")))); });
    try {
        auto t = eil::geom::gridTypeFromName("unstructured");
        emit(eil::format("gt.nocatch %d", static_cast<int>(t)));
    }
    catch (const eil::ExceptionBase& e) {
        emit("gt.nocatch EXC " + e.msg());
    }
    catch (const eil::ErrorBase& e) {
        emit("gt.nocatch ERR " + e.msg());
    }

    //============================= idx: unified indexing
    auto g1 = mk_1d();
    auto g2 = mk_2d_quad();
    auto g3 = mk_3d_grid222();
    emit(eil::format("idx.fields niv=%d njv=%d nkv=%d nvertices=%d vlength=%d ncells=%d "
                     "label=%s dims=%d",
                     g1->niv, g1->njv, g1->nkv, g1->nvertices, g1->vertices.length(),
                     g1->ncells, g1->label, g1->dimensions));
    emit(eil::format("idx.1d si(0,0)=%d si(1,0)=%d si(2,0)=%d si(2,0,0)=%d",
                     g1->single_index(0, 0), g1->single_index(1, 0), g1->single_index(2, 0),
                     g1->single_index(2, 0, 0)));
    tryit("idx.1d si(0,1)", [&] { emit(eil::format("  %d", g1->single_index(0, 1))); });
    tryit("idx.1d si(3,0)", [&] { emit(eil::format("  %d", g1->single_index(3, 0))); });
    tryit("idx.1d si(0,0,1)", [&] { emit(eil::format("  %d", g1->single_index(0, 0, 1))); });
    emit(eil::format("idx.2d si(0,0)=%d si(3,0)=%d si(3,0,0)=%d", g2->single_index(0, 0),
                     g2->single_index(3, 0), g2->single_index(3, 0, 0)));
    tryit("idx.2d si(4,0)", [&] { emit(eil::format("  %d", g2->single_index(4, 0))); });
    emit(eil::format("idx.3d si(0,0,0)=%d si(2,0,0)=%d si(0,2,0)=%d si(0,0,2)=%d si(2,2,2)=%d",
                     g3->single_index(0, 0, 0), g3->single_index(2, 0, 0),
                     g3->single_index(0, 2, 0), g3->single_index(0, 0, 2),
                     g3->single_index(2, 2, 2)));
    tryit("idx.3d si(3,0,0)", [&] { emit(eil::format("  %d", g3->single_index(3, 0, 0))); });
    tryit("idx.3d si(0,3,0)", [&] { emit(eil::format("  %d", g3->single_index(0, 3, 0))); });
    tryit("idx.3d si(0,0,3)", [&] { emit(eil::format("  %d", g3->single_index(0, 0, 3))); });
    emit(eil::format("idx.ijk 1d(0)=%s 1d(2)=%s 2d(3)=%s 3d(0)=%s 3d(2)=%s 3d(4)=%s 3d(12)=%s "
                     "3d(26)=%s",
                     g1->ijk_indices(0), g1->ijk_indices(2), g2->ijk_indices(3),
                     g3->ijk_indices(0), g3->ijk_indices(2), g3->ijk_indices(4),
                     g3->ijk_indices(12), g3->ijk_indices(26)));
    tryit("idx.ijk 3d(27)", [&] { emit(eil::format("  %s", g3->ijk_indices(27))); });
    auto gt = mk_3d_grid222();
    gt->nvertices = 2;
    tryit("idx.ijk truncated nvertices", [&] {
        emit(eil::format("  %s nvertices=%d vlength=%d", gt->ijk_indices(26), gt->nvertices,
                         gt->vertices.length()));
    });
    emit(eil::format("idx.ijk tri(2)=%s tri(0)=%s", mk_2d_tri()->ijk_indices(2),
                     mk_2d_tri()->ijk_indices(0)));

    //============================= ab: the abstract table through a Grid handle
    std::shared_ptr<Grid> hg = g3;
    emit(eil::format("ab.handle label=%s dims=%d ncells=%d nvertices=%d", hg->label,
                     hg->dimensions, hg->ncells, hg->nvertices));
    dump3("ab.opIndex(0,0,0)", *hg->opIndex(0, 0, 0));
    dump3("ab.opIndex(2,1,0)", *hg->opIndex(2, 1, 0));
    dump3("ab.opIndex(2,1)", *hg->opIndex(2, 1));
    dump3("ab.opIndex(7)", *hg->opIndex(7));
    dump3("ab.opIndex(0)", *hg->opIndex(0));
    emit(eil::format("ab.vtxids(ijk) (0,0,0)=%s (1,1,1)=%s (2,1,0)=%s",
                     hg->get_vtx_id_list_for_cell(0, 0, 0),
                     hg->get_vtx_id_list_for_cell(1, 1, 1),
                     hg->get_vtx_id_list_for_cell(2, 1, 0)));
    emit(eil::format("ab.vtxids(indx) 0=%s 7=%s", hg->get_vtx_id_list_for_cell(0),
                     hg->get_vtx_id_list_for_cell(7)));
    std::array<size_t, 8> vc{};
    size_t vn = 0;
    hg->copy_vtx_id_list_for_cell(vc, vn, 0, 0, 0);
    emit(eil::format("ab.copy(ijk 0,0,0) nvtx=%d list=%s", vn, vc));
    hg->copy_vtx_id_list_for_cell(vc, vn, 1, 1, 1);
    emit(eil::format("ab.copy(ijk 1,1,1) nvtx=%d list=%s", vn, vc));
    hg->copy_vtx_id_list_for_cell(vc, vn, 7);
    emit(eil::format("ab.copy(indx 7) nvtx=%d list=%s", vn, vc));
    hg->copy_vtx_id_list_for_cell(vc, vn, 3);
    emit(eil::format("ab.copy(indx 3) nvtx=%d list=%s", vn, vc));
    emit(eil::format("ab.nvtx 0=%d 7=%d vtktype 0=%d celltype 0=%d",
                     hg->number_of_vertices_for_cell(0), hg->number_of_vertices_for_cell(7),
                     hg->vtk_element_type_for_cell(0), hg->get_cell_type(0)));
    emit(eil::format("ab.bdc 0=%s 1=%s 2=%s", hg->get_list_of_boundary_cells(0),
                     hg->get_list_of_boundary_cells(1), hg->get_list_of_boundary_cells(2)));
    std::shared_ptr<Grid> b0 = hg->get_boundary_grid(1);
    emit(eil::format("ab.bdgrid label=%s dims=%d ncells=%d nvertices=%d", b0->label,
                     b0->dimensions, b0->ncells, b0->nvertices));
    dump3("ab.bdgrid v0", *b0->opIndex(0));
    dump3("ab.bdgrid v1", *b0->opIndex(1));
    std::shared_ptr<Grid> bnull = hg->get_boundary_grid(5);
    emit(eil::format("ab.bdgrid(5) isnull=%s", bnull == nullptr));
    writer_log.clear();
    hg->read_from_gzip_file("in.gz");
    hg->read_from_gzip_file("in.gz", 2.5);
    hg->read_from_raw_binary_file("in.bin");
    hg->read_from_raw_binary_file("in.bin", 0.125);
    hg->write_to_su2_file("out.su2");
    hg->write_to_su2_file("out.su2", 2.5);
    hg->write_to_su2_file("out.su2", 2.5, false);
    for (size_t k = 0; k < writer_log.length(); ++k) {
        emit(eil::format("ab.log[%d]=%s", k, writer_log[k]));
    }
    emit(eil::format("ab.log=%s", writer_log));

    //============================= wr: Grid.write(fileName, fmt) dispatch
    writer_log.clear();
    const std::string wg = "/tmp/probe_g1_write_gziptext.txt";
    const std::string wb = "/tmp/probe_g1_write_rawbinary.txt";
    const std::string wu = "/tmp/probe_g1_write_unknown.txt";
    const std::string we = "/tmp/probe_g1_write_empty.txt";
    const std::string wv = "/tmp/probe_g1_write_vtkfmt.txt";
    g2->write(wg, "gziptext");
    g2->write(wb, "rawbinary");
    g2->write(wu, "voxbin");
    g2->write(we, "");
    g2->write(wv, "vtk");
    for (size_t k = 0; k < writer_log.length(); ++k) {
        emit(eil::format("wr.log[%d]=%s", k, writer_log[k]));
    }
    emit(eil::format("wr.gz len=%d body=%s", len_of(wg), one_line(wg)));
    emit(eil::format("wr.bin len=%d body=%s", len_of(wb), one_line(wb)));
    emit(eil::format("wr.unknown len=%d body=%s", len_of(wu), one_line(wu)));
    emit(eil::format("wr.empty len=%d body=%s", len_of(we), one_line(we)));
    emit(eil::format("wr.vtkfmt len=%d body=%s", len_of(wv), one_line(wv)));

    //============================= ccp: compute_cell_properties
    auto t2 = mk_2d_tri();
    dump_props(t2, 0, false, "ccp.tri2d");
    dump_props(t2, 0, true, "ccp.tri2d");
    auto q2 = mk_2d_quad();
    dump_props(q2, 0, false, "ccp.quad2d");
    dump_props(q2, 0, true, "ccp.quad2d");
    auto fq = mk_2d_flat();
    tryit("ccp.flat2d nvtx=2", [&] { dump_props(fq, 0, false, "  ccp.flat2d"); });
    auto tet = mk_3d_tetra();
    dump_props(tet, 0, false, "ccp.tet3d");
    dump_props(tet, 0, true, "ccp.tet3d");
    auto hx = mk_3d_hex_skew();
    dump_props(hx, 0, false, "ccp.hexskew");
    dump_props(hx, 0, true, "ccp.hexskew");
    auto hu = mk_3d_hex_unit();
    dump_props(hu, 0, false, "ccp.hexunit");
    dump_props(hu, 0, true, "ccp.hexunit");
    auto py = mk_3d_pyramid();
    dump_props(py, 0, false, "ccp.pyr3d");
    dump_props(py, 0, true, "ccp.pyr3d");
    auto wd = mk_3d_wedge();
    dump_props(wd, 0, false, "ccp.wedge3d");
    dump_props(wd, 0, true, "ccp.wedge3d");
    auto g222 = mk_3d_grid222();
    for (size_t i = 0; i < 8; ++i) dump_props(g222, i, false, eil::text("ccp.grid222 cell", i));
    for (size_t i = 0; i < 8; ++i) dump_props(g222, i, true, eil::text("ccp.grid222 tc cell", i));
    tryit("ccp.pent2d nvtx=5", [&] { dump_props(mk_2d_pent(), 0, false, "  ccp.pent2d"); });
    tryit("ccp.hept3d nvtx=7", [&] { dump_props(mk_3d_hept(), 0, false, "  ccp.hept3d"); });
    tryit("ccp.onept nvtx=4 coincident",
          [&] { dump_props(mk_1pt(), 0, false, "  ccp.onept"); });
    // The branch is chosen by `dimensions`, not by the vertex count.
    auto as2d = mk_3d_hept();
    as2d->dimensions = 2;
    tryit("ccp.hept relabelled 2D", [&] { dump_props(as2d, 0, false, "  ccp.hept2d"); });
    auto as3d = mk_2d_pent();
    as3d->dimensions = 3;
    dump_props(as3d, 0, true, "ccp.pent relabelled 3D");
    dump_props(as3d, 0, false, "ccp.pent relabelled 3D");
    auto quad3d = mk_2d_quad();
    quad3d->dimensions = 3;
    dump_props(quad3d, 0, true, "ccp.quad relabelled 3D");
    tryit("ccp.tri relabelled 3D", [&] { dump_props(mk_2d_tri(), 0, false, "  ccp.tri3d"); });
    tryit("ccp.wedge relabelled 2D",
          [&] { dump_props(mk_3d_wedge(), 0, false, "  ccp.wedge2d"); });

    //============================= pic: point_is_inside_cell
    auto qt = mk_2d_quad();
    dump_inside(qt, V(0.5, 0.5, 0.0), 0, "pic.quad2d centre");
    dump_inside(qt, V(0.2, 0.3, 0.0), 0, "pic.quad2d inside");
    dump_inside(qt, V(0.1875, 0.78125, 0.0), 0, "pic.quad2d inside near lo");
    dump_inside(qt, V(1.5, 0.5, 0.0), 0, "pic.quad2d outside");
    dump_inside(qt, V(-0.5, 0.5, 0.0), 0, "pic.quad2d outside neg");
    dump_inside(qt, V(0.875, 0.5, 0.0), 0, "pic.quad2d on face xhi");
    dump_inside(qt, V(0.125, 0.5, 0.0), 0, "pic.quad2d on face xlo");
    dump_inside(qt, V(0.5, 0.25, 0.0), 0, "pic.quad2d on face ylo");
    dump_inside(qt, V(0.5, 0.75, 0.0), 0, "pic.quad2d on face yhi");
    dump_inside(qt, V(0.875, 0.75, 0.0), 0, "pic.quad2d on edge hi");
    dump_inside(qt, V(0.125, 0.25, 0.0), 0, "pic.quad2d on edge lo");
    dump_inside(qt, V(0.5, 0.5, 3.0), 0, "pic.quad2d off plane z");
    dump_inside(qt, V(0.87500011920928955, 0.5, 0.0), 0, "pic.quad2d just out xhi");
    auto tt = mk_2d_tri();
    dump_inside(tt, V(0.25, 0.25, 0.0), 0, "pic.tri2d inside");
    dump_inside(tt, V(0.5, 0.5, 0.0), 0, "pic.tri2d on hypotenuse");
    dump_inside(tt, V(0.75, 0.75, 0.0), 0, "pic.tri2d outside");
    dump_inside(tt, V(0.0, 0.0, 0.0), 0, "pic.tri2d on vertex 0");
    dump_inside(tt, V(1.0, 0.0, 0.0), 0, "pic.tri2d on vertex 1");
    dump_inside(tt, V(0.5, 0.0, 0.0), 0, "pic.tri2d on edge ylo");
    dump_inside(tt, V(0.0, 0.5, 0.0), 0, "pic.tri2d on edge xlo");
    dump_inside(tt, V(0.125, 0.8125, 0.0), 0, "pic.tri2d inside upper");
    auto h1 = mk_3d_hex_unit();
    dump_inside(h1, V(0.5, 0.5, 0.5), 0, "pic.hexunit centre");
    dump_inside(h1, V(0.125, 0.375, 0.625), 0, "pic.hexunit inside");
    dump_inside(h1, V(1.5, 0.5, 0.5), 0, "pic.hexunit outside");
    dump_inside(h1, V(1.0, 0.5, 0.5), 0, "pic.hexunit on face xhi");
    dump_inside(h1, V(0.0, 0.5, 0.5), 0, "pic.hexunit on face xlo");
    dump_inside(h1, V(0.5, 0.5, 1.0), 0, "pic.hexunit on face zhi");
    dump_inside(h1, V(1.0, 1.0, 0.5), 0, "pic.hexunit on edge");
    dump_inside(h1, V(0.0, 0.0, 0.0), 0, "pic.hexunit on vertex");
    dump_inside(h1, V(0.5, 1.0, 0.0), 0, "pic.hexunit on edge 2");
    auto hsk = mk_3d_hex_skew();
    dump_inside(hsk, V(0.5, 0.5, 0.5), 0, "pic.hexskew centre");
    dump_inside(hsk, V(1.2, 0.5, 0.5), 0, "pic.hexskew outside");
    auto t3 = mk_3d_tetra();
    dump_inside(t3, V(0.125, 0.125, 0.125), 0, "pic.tet3d inside");
    dump_inside(t3, V(0.5, 0.5, 0.5), 0, "pic.tet3d outside");
    dump_inside(t3, V(0.25, 0.25, 0.5), 0, "pic.tet3d on slanted face");
    dump_inside(t3, V(0.5, 0.0, 0.5), 0, "pic.tet3d on xz face");
    dump_inside(t3, V(0.0, 0.0, 0.0), 0, "pic.tet3d on vertex");
    auto p3 = mk_3d_pyramid();
    dump_inside(p3, V(0.125, 0.25, 0.25), 0, "pic.pyr3d inside");
    dump_inside(p3, V(0.5, 0.5, 1.5), 0, "pic.pyr3d outside");
    dump_inside(p3, V(0.5, 0.5, 0.0), 0, "pic.pyr3d on base");
    dump_inside(p3, V(0.125, 0.25, 1.0), 0, "pic.pyr3d on apex");
    auto w3 = mk_3d_wedge();
    dump_inside(w3, V(0.5, 0.25, 0.5), 0, "pic.wedge3d inside");
    dump_inside(w3, V(0.5, 0.3125, 0.5), 0, "pic.wedge3d inside 2");
    dump_inside(w3, V(0.9, 0.5, 0.5), 0, "pic.wedge3d outside");
    dump_inside(w3, V(0.5, 0.0, 0.5), 0, "pic.wedge3d on face ylo");
    dump_inside(w3, V(0.5, 0.25, 1.0), 0, "pic.wedge3d on face zhi");
    dump_inside(w3, V(0.75, 0.5, 0.5), 0, "pic.wedge3d on slant face");
    dump_inside(w3, V(0.5, 1.0, 0.5), 0, "pic.wedge3d on edge apex");
    tryit("pic.1d", [&] { dump_inside(g1, V(0.5, 0.0, 0.0), 0, "  pic.1d"); });
    tryit("pic.4d", [&] { dump_inside(mk_4d(), V(0.5, 0.5, 0.0), 0, "  pic.4d"); });
    auto d0 = mk_4d();
    d0->dimensions = 0;
    tryit("pic.0d", [&] { dump_inside(d0, V(0.5, 0.5, 0.0), 0, "  pic.0d"); });
    tryit("pic.2d nvtx=5",
          [&] { dump_inside(mk_2d_pent(), V(0.5, 0.5, 0.0), 0, "  pic.pent2d"); });
    tryit("pic.3d nvtx=7",
          [&] { dump_inside(mk_3d_hept(), V(0.5, 0.5, 0.5), 0, "  pic.hept3d"); });
    tryit("pic.2d nvtx=2", [&] { dump_inside(fq, V(0.5, 0.5, 0.0), 0, "  pic.flat2d"); });
    auto wd2 = mk_3d_wedge();
    wd2->dimensions = 4;
    tryit("pic.dims4 nvtx=6", [&] { dump_inside(wd2, V(0.5, 0.25, 0.5), 0, "  pic.wedge4d"); });

    //============================= cbc: cell_barycentre
    dump3("cbc.tri2d", t2->cell_barycentre(0));
    dump3("cbc.quad2d", qt->cell_barycentre(0));
    dump3("cbc.flat2d", fq->cell_barycentre(0));
    dump3("cbc.tet3d", t3->cell_barycentre(0));
    dump3("cbc.hexunit", h1->cell_barycentre(0));
    dump3("cbc.hexskew", hx->cell_barycentre(0));
    dump3("cbc.pyr3d", p3->cell_barycentre(0));
    dump3("cbc.wedge3d", w3->cell_barycentre(0));
    dump3("cbc.pent2d", mk_2d_pent()->cell_barycentre(0));
    dump3("cbc.hept3d", mk_3d_hept()->cell_barycentre(0));
    dump3("cbc.onept", mk_1pt()->cell_barycentre(0));
    for (size_t i = 0; i < 8; ++i) {
        dump3(eil::text("cbc.grid222 cell", i), g222->cell_barycentre(i));
    }
    dump3("cbc.nan2d", mk_2d_nan()->cell_barycentre(0));

    //============================= bin: get_bin_* and sort_cells_into_bins
    auto b3 = mk_3d_grid222();
    emit(eil::format("bin.unsorted nbx=%d nby=%d nbz=%d deltax=%.17g deltay=%.17g "
                     "deltaz=%.17g sorted=%s depth=%d",
                     b3->nbx, b3->nby, b3->nbz, b3->deltax, b3->deltay, b3->deltaz,
                     b3->cells_are_sorted_into_bins, b3->bins.length()));
    dump3("bin.unsorted bb0", b3->bb0);
    dump3("bin.unsorted bb1", b3->bb1);
    b3->sort_cells_into_bins();
    dump_bins(b3, "bin.222.default");
    dump_bin_indices(b3, V(0.0, 0.0, 0.0), "bin.222 corner lo");
    dump_bin_indices(b3, V(1.0, 1.0, 1.0), "bin.222 corner hi");
    dump_bin_indices(b3, V(0.5, 0.5, 0.5), "bin.222 centre");
    dump_bin_indices(b3, V(0.125, 0.875, 0.375), "bin.222 inside");
    dump_bin_indices(b3, V(-0.5, -0.5, -0.5), "bin.222 outside lo");
    dump_bin_indices(b3, V(2.0, 3.0, 4.0), "bin.222 outside hi");
    dump_bin_indices(b3, V(1.0, 0.0, 1.0), "bin.222 edge mix");
    dump_bin_indices(b3, V(0.0, 1.0, 0.5), "bin.222 edge mix2");
    dump_bin_indices(b3, V(-1000.0, 500.0, -0.25), "bin.222 far");
    dump_bin_indices(b3, V(0.2, 0.3, 0.30000000000000004), "bin.222 nonexact");
    auto b3c = mk_3d_grid222();
    b3c->sort_cells_into_bins(3, 4, 2);
    dump_bins(b3c, "bin.222.n342");
    dump_bin_indices(b3c, V(0.5, 0.5, 0.5), "bin.n342 centre");
    dump_bin_indices(b3c, V(-0.5, -0.5, -0.5), "bin.n342 outside lo");
    dump_bin_indices(b3c, V(1.5, 1.5, 1.5), "bin.n342 outside hi");
    dump_bin_indices(b3c, V(0.33333333333333331, 0.66666666666666663, 1.0), "bin.n342 thirds");
    auto b2 = mk_2d_grid32();
    b2->sort_cells_into_bins();
    dump_bins(b2, "bin.32.default");
    auto b2z = mk_2d_grid32();
    b2z->sort_cells_into_bins(2, 3, 7);
    dump_bins(b2z, "bin.32.nbinz7");
    dump_bin_indices(b2z, V(1.5, 1.0, 0.0), "bin.32 centre");
    dump_bin_indices(b2z, V(-0.5, -0.5, 4.0), "bin.32 outside");
    dump_bin_indices(b2z, V(3.0, 2.0, 0.0), "bin.32 corner hi");
    emit(eil::format("bin.iz 2d p.z=4 -> iz=%d", b2z->get_bin_iz(V(1.0, 1.0, 4.0))));
    auto b1 = mk_1d();
    b1->sort_cells_into_bins(2, 1, 1);
    dump_bins(b1, "bin.1d");
    dump_bin_indices(b1, V(1.5, 0.0, 0.0), "bin.1d far hi");
    dump_bin_indices(b1, V(-1.5, 3.0, 3.0), "bin.1d far lo");
    tryit("bin.nbinx=0", [&] { mk_3d_grid222()->sort_cells_into_bins(0, 5, 5); });
    tryit("bin.nbiny=0", [&] { mk_3d_grid222()->sort_cells_into_bins(5, 0, 5); });
    tryit("bin.nbinz=0", [&] { mk_3d_grid222()->sort_cells_into_bins(5, 5, 0); });
    tryit("bin.all zero", [&] { mk_3d_grid222()->sort_cells_into_bins(0, 0, 0); });
    auto bbf = mk_3d_grid222();
    tryit("bin.reorder after throw", [&] { bbf->sort_cells_into_bins(0, 2, 2); });
    dump_bins(bbf, "bin.after throw");
    auto bf = mk_2d_flat();
    bf->sort_cells_into_bins(2, 2, 2);
    dump_bins(bf, "bin.flat floor");
    dump_bin_indices(bf, V(0.5, 0.5, 0.0), "bin.flat centre");
    dump_bin_indices(bf, V(0.5, 100.0, 0.0), "bin.flat far y");
    auto bp = mk_1pt();
    bp->sort_cells_into_bins(4, 4, 4);
    dump_bins(bp, "bin.onept floor");
    dump_bin_indices(bp, V(0.25, 0.25, 0.25), "bin.onept same");
    dump_bin_indices(bp, V(9.0, 9.0, 9.0), "bin.onept far");
    auto bt = mk_3d_grid222();
    bt->sort_cells_into_bins(2, 2, 2);
    bt->sort_cells_into_bins(2, 2, 2);
    dump_bins(bt, "bin.twice");
    bt->sort_cells_into_bins(1, 1, 1);
    dump_bins(bt, "bin.thrice shrunk");
    auto bn = mk_3d_grid222();
    bn->nvertices = 8;
    bn->sort_cells_into_bins(2, 2, 2);
    dump_bins(bn, "bin.nvtx truncated");

    // The clamp of grid.d:252-253, replayed with D's chosen operand type. D's
    // `min(size_t, int)` has result type `int` and compares SIGNED (probe rows
    // bin.clamp print D's typeof() as "int"), so the size_t bound is brought into
    // int here rather than ix being promoted to size_t.
    size_t nbx_probe = 4;
    for (int ix : {-5, -1, 0, 1, 3, 4, 99}) {
        int m = std::min(static_cast<int>(nbx_probe - 1), ix);
        emit(eil::format("bin.clamp ix=%d min=%s (int) max=%d", ix, eil::to_string(m),
                         std::max(0, m)));
    }
    for (int n : {-7, -3, -1, 0, 1, 3, 7, 9}) {
        double d = static_cast<double>(n) / 2.0;
        emit(eil::format("bin.trunc n=%d d=%.17g castint=%d", n, d, static_cast<int>(d)));
    }

    //============================= fsc: enclosing-cell search
    auto fs = mk_3d_grid222();
    dump_search(fs, V(0.75, 0.25, 0.25), "fsc.unsorted a");
    dump_search(fs, V(0.5, 0.5, 0.5), "fsc.unsorted b");
    dump_search(fs, V(0.0, 0.0, 0.0), "fsc.unsorted c");
    dump_search(fs, V(1.0, 1.0, 1.0), "fsc.unsorted d");
    dump_search(fs, V(2.0, 0.5, 0.5), "fsc.unsorted e");
    dump_search(fs, V(-1.0, -1.0, -1.0), "fsc.unsorted f");
    fs->sort_cells_into_bins(2, 2, 2);
    dump_search(fs, V(0.75, 0.25, 0.25), "fsc.sorted a");
    dump_search(fs, V(0.5, 0.5, 0.5), "fsc.sorted b");
    dump_search(fs, V(0.0, 0.0, 0.0), "fsc.sorted c");
    dump_search(fs, V(1.0, 1.0, 1.0), "fsc.sorted d");
    dump_search(fs, V(2.0, 0.5, 0.5), "fsc.sorted e");
    dump_search(fs, V(-1.0, -1.0, -1.0), "fsc.sorted f");
    dump_search(fs, V(0.125, 0.875, 0.625), "fsc.sorted g");
    dump_search(fs, V(0.9999998807907104, 0.5, 0.5), "fsc.sorted near hi face");
    dump_search(fs, V(0.75, 0.75, 0.25), "fsc.sorted h");
    auto fm = mk_3d_grid222();
    fm->sort_cells_into_bins(1, 1, 1);
    dump_search(fm, V(0.75, 0.25, 0.25), "fsc.onebin a");
    dump_search(fm, V(-1.0, -1.0, -1.0), "fsc.onebin out");
    auto ff = mk_2d_grid32();
    ff->sort_cells_into_bins(3, 2, 1);
    dump_search(ff, V(1.5, 1.5, 0.0), "fsc.2d centre");
    dump_search(ff, V(0.5, 0.5, 0.0), "fsc.2d cell0");
    dump_search(ff, V(2.5, 1.5, 0.0), "fsc.2d cell5");
    dump_search(ff, V(1.5, 1.5, 5.0), "fsc.2d off plane");
    dump_search(ff, V(3.5, 1.0, 0.0), "fsc.2d outside");
    dump_search(ff, V(0.0, 0.0, 0.0), "fsc.2d corner");
    auto ftet = mk_3d_tetra();
    ftet->sort_cells_into_bins(2, 2, 2);
    dump_search(ftet, V(0.125, 0.125, 0.125), "fsc.tet inside");
    dump_search(ftet, V(0.5, 0.5, 0.5), "fsc.tet outside");

    //============================= fncc: nearest cell centre
    nearest(fs, V(0.25, 0.25, 0.25), "fncc.222 a");
    nearest(fs, V(0.75, 0.75, 0.75), "fncc.222 b");
    nearest(fs, V(5.0, 5.0, 5.0), "fncc.222 far");
    nearest(fs, V(0.25, 0.75, 0.25), "fncc.222 c");
    nearest(fs, V(0.0, 0.0, 0.0), "fncc.222 corner");
    nearest(fs, V(0.5, 0.5, 0.5), "fncc.222 centre tie");
    nearest(hx, V(0.5, 0.5, 0.5), "fncc.hexskew");
    nearest(tt, V(0.5, 0.5, 0.5), "fncc.tri2d");
    auto tg = std::make_shared<ProbeGrid>(Grid_t::unstructured_grid, 2, "tie2d");
    {
        ProbeGrid::Bins2 tids;
        tids.append(ids({0, 1, 2}));
        tids.append(ids({1, 3, 2}));
        tg->load(ArrV{V(0.0, 0.0, 0.0), V(1.0, 0.0, 0.0), V(0.0, 1.0, 0.0), V(1.0, 1.0, 0.0)},
                 4, 1, 1, tids);
    }
    nearest(tg, V(0.5, 0.5, 0.0), "fncc.tie exact");
    nearest(tg, V(0.375, 0.375, 0.0), "fncc.tie lo");
    nearest(tg, V(0.625, 0.625, 0.0), "fncc.tie hi");
    nearest(fq, V(0.5, 0.5, 0.0), "fncc.flat");
    nearest(mk_1pt(), V(1.0, 1.0, 1.0), "fncc.onept");

    //============================= stl: write_to_stl_file
    const std::string s_tri = "/tmp/probe_g1_tri.stl";
    const std::string s_quad = "/tmp/probe_g1_quad.stl";
    const std::string s_scaled = "/tmp/probe_g1_quad_scaled.stl";
    const std::string s_grid = "/tmp/probe_g1_grid32.stl";
    t2->label = "tri label";
    t2->write_to_stl_file(s_tri);
    emit(eil::format("stl.tri len=%d body=%s", len_of(s_tri), one_line(s_tri)));
    qt->write_to_stl_file(s_quad);
    emit(eil::format("stl.quad len=%d body=%s", len_of(s_quad), one_line(s_quad)));
    qt->write_to_stl_file(s_scaled, 2.5);
    emit(eil::format("stl.quad scaled len=%d body=%s", len_of(s_scaled), one_line(s_scaled)));
    auto g32 = mk_2d_grid32();
    g32->write_to_stl_file(s_grid, 0.5);
    emit(eil::format("stl.grid32 len=%d body=%s", len_of(s_grid), one_line(s_grid)));
    tryit("stl.3D", [&] { hx->write_to_stl_file("/tmp/probe_g1_3d.stl"); });
    tryit("stl.nvtx=5", [&] { mk_2d_pent()->write_to_stl_file("/tmp/probe_g1_pent.stl"); });
    tryit("stl.nvtx=2", [&] { fq->write_to_stl_file("/tmp/probe_g1_flat.stl"); });
    const std::string s_nan = "/tmp/probe_g1_nan.stl";
    auto gn = mk_2d_nan();
    gn->write_to_stl_file(s_nan);
    emit(eil::format("stl.nan len=%d body=%s", len_of(s_nan), one_line(s_nan)));
    gn->write_to_stl_file(s_nan, 2.5);
    emit(eil::format("stl.nan scaled len=%d body=%s", len_of(s_nan), one_line(s_nan)));

    //============================= vtk: the abstract writer through a handle
    const std::string v_hex = "/tmp/probe_g1_hex.vtk";
    const std::string v_quad = "/tmp/probe_g1_q2.vtk";
    hg->write_to_vtk_file(v_hex);
    emit(eil::format("vtk.222 len=%d body=%s", len_of(v_hex), one_line(v_hex)));
    std::shared_ptr<Grid> hq = q2;
    hq->write_to_vtk_file(v_quad);
    emit(eil::format("vtk.quad len=%d body=%s", len_of(v_quad), one_line(v_quad)));

    //============================= rot: rotate(Quaternion, ctr)
    auto r0 = mk_3d_hex_unit();
    r0->rotate(Quaternion{1.0, 0.0, 0.0, 0.0});
    dump_verts(r0, "rot.identity");
    auto r1 = mk_3d_hex_unit();
    r1->rotate(Quaternion{0.0, 0.0, 0.0, 1.0});
    dump_verts(r1, "rot.180 z");
    auto r2 = mk_3d_hex_unit();
    r2->rotate(Quaternion{0.7071067811865476, 0.0, 0.0, 0.7071067811865476});
    dump_verts(r2, "rot.90 z");
    auto r3 = mk_3d_hex_unit();
    r3->rotate(Quaternion{0.7071067811865476, 0.7071067811865476, 0.0, 0.0});
    dump_verts(r3, "rot.90 x");
    auto r4 = mk_3d_hex_unit();
    r4->rotate(Quaternion{0.0, 0.0, 1.0, 0.0}, V(0.5, 0.5, 0.5));
    dump_verts(r4, "rot.180 y ctr");
    auto r5 = mk_3d_hex_unit();
    r5->rotate(Quaternion{0.5, 0.5, 0.5, 0.5}, V(0.25, -0.5, 1.0));
    dump_verts(r5, "rot.q half ctr");
    auto r5b = mk_2d_quad();
    r5b->rotate(Quaternion{0.5, 0.5, 0.5, 0.5});
    dump_verts(r5b, "rot.q half quad2d");
    Quaternion qnan = eil::geom::quaternion_init(); // D's `Quaternion q;` -> four NaNs
    emit(eil::format("rot.qinit=%s", qnan));
    auto r6 = mk_3d_hex_unit();
    r6->rotate(qnan);
    dump_verts(r6, "rot.q nan");
    auto r7 = mk_3d_hex_unit();
    tryit("rot norm 2", [&] { r7->rotate(Quaternion{2.0, 0.0, 0.0, 0.0}); });
    dump_verts(r7, "rot norm 2 unchanged");
    tryit("rot norm near 0.9999",
          [&] { mk_3d_hex_unit()->rotate(Quaternion{0.9999, 0.0, 0.0, 0.0}); });
    tryit("rot norm zero", [&] { mk_3d_hex_unit()->rotate(Quaternion{0.0, 0.0, 0.0, 0.0}); });
    tryit("rot norm just inside",
          [&] { mk_3d_hex_unit()->rotate(Quaternion{1.000000001, 0.0, 0.0, 0.0}); });
    tryit("rot norm just outside",
          [&] { mk_3d_hex_unit()->rotate(Quaternion{1.00000002, 0.0, 0.0, 0.0}); });
    tryit("rot norm negative", [&] { mk_3d_hex_unit()->rotate(Quaternion{-1.0, 0.0, 0.0, 0.0}); });

    //============================= rot: rotate(theta, axis, ctr)
    auto a0 = mk_3d_hex_unit();
    a0->rotate(0.0, V(0.0, 0.0, 1.0));
    dump_verts(a0, "rot.a identity");
    auto a1 = mk_3d_hex_unit();
    a1->rotate(3.141592653589793, V(0.0, 0.0, 1.0));
    dump_verts(a1, "rot.a pi z");
    auto a2 = mk_3d_hex_unit();
    a2->rotate(1.5707963267948966, V(1.0, 0.0, 0.0));
    dump_verts(a2, "rot.a halfpi x");
    auto a3 = mk_3d_hex_unit();
    a3->rotate(1.0471975511965976, V(1.0, 1.0, 1.0), V(0.5, 0.5, 0.5));
    dump_verts(a3, "rot.a thirdpi diag ctr");
    auto a4 = mk_3d_hex_unit();
    a4->rotate(2.356194490192345, V(0.0, 0.0, 2.0));
    dump_verts(a4, "rot.a 3pi/4 unnormalized axis");
    auto a5 = mk_3d_hex_unit();
    tryit("rot.a pi zero axis",
          [&] { a5->rotate(3.141592653589793, V(0.0, 0.0, 0.0)); });
    dump_verts(a5, "rot.a pi zero axis unchanged");
    tryit("rot.a nan theta", [&] {
        mk_3d_hex_unit()->rotate(std::numeric_limits<double>::quiet_NaN(), V(1.0, 0.0, 0.0));
    });
    Vector3 ax = V(0.0, 2.0, 0.0);
    auto a6 = mk_3d_hex_unit();
    a6->rotate(1.5707963267948966, ax);
    dump3("rot.a axis after call", ax);
    dump_verts(a6, "rot.a axis len2");
    auto a7 = mk_3d_hex_unit();
    a7->rotate(-1.5707963267948966, V(0.0, 1.0, 0.0));
    dump_verts(a7, "rot.a minus halfpi y");

    // One grid exercised through both writers, with a label that needs escaping.
    auto both = mk_2d_quad();
    both->label = "both\"label";
    const std::string s_both = "/tmp/probe_g1_both.stl";
    const std::string v_both = "/tmp/probe_g1_both.vtk";
    both->write_to_stl_file(s_both);
    both->write_to_vtk_file(v_both);
    emit(eil::format("both.stl len=%d body=%s", len_of(s_both), one_line(s_both)));
    emit(eil::format("both.vtk len=%d body=%s", len_of(v_both), one_line(v_both)));
    writer_log.clear();
    both->write("/tmp/probe_g1_both.gz.txt", "gziptext");
    emit(eil::format("both.log=%s", writer_log));
    emit(eil::format("both.gz body=%s", one_line("/tmp/probe_g1_both.gz.txt")));

    for (const std::string& path : {wg, wb, wu, we, wv, s_tri, s_quad, s_scaled, s_grid, s_nan,
                                    v_hex, v_quad, s_both, v_both,
                                    std::string("/tmp/probe_g1_both.gz.txt")}) {
        std::remove(path.c_str());
    }

    // Replay against the ldc2 corpus.
    std::ifstream in(argv[1]);
    if (!in) {
        std::fprintf(stderr, "cannot open baseline %s\n", argv[1]);
        return 2;
    }
    std::vector<std::string> expected;
    for (std::string l; std::getline(in, l);) expected.push_back(l);

    int diffs = 0, shown = 0;
    size_t matched = 0;
    for (size_t i = 0; i < expected.size() || i < g_lines.size(); ++i) {
        const bool have_e = i < expected.size();
        const bool have_g = i < g_lines.size();
        const std::string e = have_e ? expected[i] : "<none>";
        const std::string g = have_g ? g_lines[i] : "<none>";
        if (have_e && have_g && e == g) { ++matched; continue; }
        if (shown < 12) {
            std::printf("line %zu\n  D:   |%s|\n  C++: |%s|\n", i + 1, e.c_str(), g.c_str());
            ++shown;
        }
        ++diffs;
    }
    std::printf("%s: geom_grid_grid, probe replay %zu D lines, %zu C++ lines, %d differ; "
                "%zu of %zu rows byte-identical\n",
                diffs == 0 ? "PASS" : "FAIL", expected.size(), g_lines.size(), diffs, matched,
                expected.size());
    return diffs == 0 ? 0 : 1;
}
