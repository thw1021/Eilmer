// tests/test_geom_grid_sgrid.cpp — C++ mirror of probes/geom/grid/probe_sgrid.d,
// replayed row for row against baselines/e587b235/probes/geom_grid_sgrid.txt
// (2097 rows captured from ldc2 1.40.0 at gdtk e587b235; the probe is 1673 lines and
// runs rc=0, and two consecutive runs of it are byte-identical).
//
// Regenerate the baseline (the gdtk tree is read-only; the shadow copy has @nogc
// stripped because ldc2 1.40 rejects `throw new ...` inside sgrid.d's own @nogc
// members — D:217, :235, :246, :257, :308, :356, :1534):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/grid \
//        probes/geom/grid/probe_sgrid.d -of=/tmp/probe_g4 -od/tmp/probe_obj_g4
//   /tmp/probe_g4 > baselines/e587b235/probes/geom_grid_sgrid.txt
//
// Sections covered by this file, in corpus order (grid brief section 10.5: the gate is
// all-or-nothing, so the add_test() line is written only when the WHOLE corpus replays
// at 0 differ; the row ranges below are what has been reached so far — the harness is
// built cluster by cluster in the order of grid brief section 11.2):
//   sg.ctor.*   rows 1-71     the size lattice (sgrid.d:35-70)
//   sg.idx.*    rows 72-138   single_index / ijk_indices (grid.d:70-91)
//   sg.opidx.*  rows 139-185  StructuredGrid.opIndex (sgrid.d:206-221)
//   sg.cell.*   rows 186-291  vertex id lists (sgrid.d:262-374)
//   sg.abst.*   rows 292-415  the abstract table through a Grid handle (sgrid.d:93-122)
//   sg.str.*    rows 416-420  toString (sgrid.d:223-232)
//   sg.tag.*    rows 421-433  set_tag / set_tags (sgrid.d:174-204)
//   sg.dup.*    rows 434-459  dup and the copy constructor (sgrid.d:162-172)
//   sg.sub.*    rows 460-527  subgrid and its six Error wordings (sgrid.d:376-420)
//   sg.bnd/blc.* rows 528-633 get_boundary_grid / get_list_of_boundary_cells over a
//               3x4x5 lattice (sgrid.d:422-578)
//   sg.path.*   rows 634-662  the Path constructor, linear and quadratic clustering (D:73)
//   sg.surf.*   rows 663-751  the ParametricSurface constructor, all four interpolation
//               spellings, the padding of a short clusterf, and measure_of_badness (D:82)
//   sg.rgrid.*  rows 752-830  the redistributed-parameter constructor and the
//               list_to_rs_grids / rs_grids_to_list pair (D:104, :648, :1472-1522)
//   sg.vol.*    rows 831-944  the ParametricVolume constructor, four interpolation
//               spellings, clusterf padding to 12, unknown interpolation (D:115-136)
//   sg.readtxt.* rows 945-1031 read_from_text_file with and without a VTK header, the four
//               dimension-inference shapes, bad tokens, a missing file, an early EOF, and
//               the format dispatch of the import constructor (D:783-852, :138-160)
//   sg.wgz/readgz rows 1032-1095 the gzip round trip: payload, scale on read, versions 1.0
//               and 1.1, a bad version, a truncated file, ntags: 0, and Grid.write's fmt
//               dispatch (D:854-922, :999-1040; compared as DECOMPRESSED payload only)
//   sg.frmt.*   rows 1096-1113 every formattedRead shape of read_from_gzip_file: literal
//               mismatch under Phobos' forward search, a conversion at exhausted input
//               (which raises nothing and leaves the destination untouched), conv text that
//               names the RESOLVED target type, and an oversized dimension that reaches
//               set_length and comes back as "Memory allocation failed" (T-927, T-914)
//   sg.wraw/readraw rows 1114-1168 raw binary: exact container bytes by hex, round trip,
//               scale, an empty label, a bad version, a missing file, a header-only file
//               (D's rawRead does not raise on a short read), and Grid.write "rawbinary"
//               (D:924-997, :1042-1082)
//   sg.vtk/su2  rows 1169-1173 write_to_vtk_file (D:1084-1102) and write_to_su2_file
//               (D:1312-1318), the latter purely delegated to UnstructuredGrid
//   sg.join.*   rows 1174-1270 joinGrid east/imax/north/jmax plus its six Error wordings
//               (multi-line messages, so the rows carry <newline>) (D:1130-1256)
//   sg.slab/wedge rows 1271-1406 makeSlabGrid and makeWedgeGrid, including the :1294 atan2
//               and the :1299/:1304 cos/sin pair of one argument (T-910, T-920) (D:1258-1310)
//   sg.bad.*    rows 1407-1415 measure_of_badness, two of them -nan (T-919)
//   sg.rs.*     rows 1416-1441 rs_grids_to_list / list_to_rs_grids and their short-array
//               asserts (D:1472-1522)
//   sg.determ.* rows 1442-1493 determine_rs_grids through nm/nelmin, then the redistributed
//               grid it implies (D:1396-1423)
//   sg.rotq.*   rows 1494-1537 rotateGrid (D:1425-1465)
//   sg.rot.*    rows 1538-1594 the two inherited Grid.rotate entries (grid.d:443-500)
//   sg.uflowz.* rows 1595-1619 the underflow/overflow guard, incl. the +0 that comes out of
//               the underflow arm and inf/nan (D:1534-1540)
//   sg.vtkhdr.* rows 1620-1628 read_VTK_header_line, case-insensitive and mid-word
//               (D:1544-1561)
//   sg.gridpro.* rows 1629-1670 import_gridpro_grid; rows 1629-1630 and 1646-1647, 1664 are
//               printed from INSIDE the ported function (D:1565-1612)
//   sg.rsgp.*   rows 1671-1701 rotate_gridpro_blocks, incl. pointer identity of the pass-
//               through arms (D:1756-1798)
//   sg.fswap.*  rows 1702-1742 grid_faceswap, incl. isnull=true when neither flag is set
//   sg.plot3d.* rows 1743-1842 writeGridsAsPlot3D and importPlot3DGrid, incl. the gzipped
//               read through popen("gunzip -c ...") (D:1614-1754)
//   sg.ccp.*    rows 1843-1861 compute_cell_properties over 3D/2D/1D lattices (grid.d:124)
//   sg.pic/cbc/bin/fsc/fncc rows 1862-2087 the search services, the ULP-exact bin-boundary
//               clamps and the three search entries (grid.d:198-420)
//   sg.unit.*   rows 2088-2096 sgrid.d's own unittest (D:1833-1865); row 2097 is sg.done
//
// Not yet emitted: none. The whole 2097-row corpus replays at 0 differing rows
// (/tmp/w2_g4b/test_geom_grid_sgrid <corpus> prints
// `PASS: geom_grid_sgrid, probe replay 2097 D lines, 2097 C++ lines, 0 differ;
//  2097 of 2097 rows byte-identical`), so add_test() in cmake/wave2_grid_g4.cmake is
// ready to switch on once the main thread has re-run the three configurations.
//
// Substrate gap worked around HERE (not in src/, so it is test-local and does not mask a
// port defect): eil::Array::set_length value-initialises the grown tail
// (substrate/include/eil/array.hpp:162-170) while D's `arr.length = n` fills it with the
// element type's .init, which for double is NaN. probe_sgrid.d:712-713 and :1666-1667
// rely on that — corpus rows 757-760 (`sg.rgrid.identity.s row[*] 0 nan nan 1`) are the
// NaN slots. d_init_doubles() reproduces D's fill for the harness's own arrays.
//
// ROW TEXT IS PRODUCED BY THE SAME HELPER LOGIC AS THE REFERENCE, not by an equivalent
// of our own. probe_sgrid.d's failure-capture helper is tryit() / tryit_cut()
// (probe:254-281), NOT the usgrid probe's tw/twd pair: every throw row in this corpus is
// `<tag> err=<msg>' or `<tag> exc=<msg>' with the bare Throwable.msg and no class, file
// or line field (grep -c 'class=' of the corpus = 0, grep -c 'dline=' = 0). So this file
// implements tryit/tryit_cut with classify() choosing between the two arms the way D's
// catch list does — ExceptionBase and ErrorBase are SIBLINGS in the substrate
// (substrate/include/eil/error.hpp:91,:96, both under ThrowableBase), which is what makes
// `catch (Exception)` miss an Error exactly as D does (backlog T-922).
//
// The port->D class mapping used by classify(), from the same measured table:
//   eil::ErrorBase without a site      object.Error                  (file "", line 0)
//   eil::ErrorBase with a site         core.exception.AssertError    (assert, T-918)
//   eil::RangeError                    core.exception.ArrayIndexError
//   eil::RangeViolation                core.exception.RangeError
//   eil::OutOfMemoryError              core.exception.OutOfMemoryError
//   eil::ExceptionBase with a site     object.Exception
//   eil::geom::GeometryException       geom.geometry_exception.GeometryException
//   eil::ConversionException           std.conv.ConvException   (an Exception subclass)
//   eil::geom::ConvOverflowException   std.conv.ConvOverflowException
//   eil::gzip::ZlibException           std.zlib.ZlibException
//   eil::format_error                  std.format.FormatException (an Exception)
// All of those render as `err=` except the last five, which render as `exc=`.
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <functional>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "eil/aa.hpp"
#include "eil/array.hpp"
#include "eil/conv.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/math.hpp"
#include "eil/number.hpp"
#include "extern/gzip/gzip.hpp"
#include "geom/elements/nomenclature.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/grid/grid.hpp"
#include "geom/grid/sgrid.hpp"
#include "geom/gpath/line.hpp"
#include "geom/misc/univariatefunctions.hpp"
#include "geom/surface/coonspatch.hpp"
#include "geom/volume/parametricvolume.hpp"

namespace {

using eil::geom::ClusterF;
using eil::geom::CoonsPatch;
using eil::geom::Grid;
using eil::geom::LinearFunction;
using eil::geom::ParametricSurface;
using eil::geom::ParametricVolume;
using eil::geom::Quaternion;
using eil::geom::StructuredGrid;
using eil::geom::StructuredGridArray;
using eil::geom::UnivariateFunctionPtr;
using eil::geom::Vector3;

// The probe writes its artefacts under TD (probe_sgrid.d:156), and the corpus embeds
// those exact absolute paths in the failure texts, so the paths are not negotiable. The
// capture file is this stage's own: /tmp/eil_g5_capture.txt belongs to stage G5.
const char* kCapture = "/tmp/eil_g4_capture.txt";
const std::string TD = "/tmp/probe_g4_dir";

// ------------------------------------------------------------------
// row emission
// ------------------------------------------------------------------
void emit(const std::string& s) { std::printf("%s\n", s.c_str()); }

// probe_sgrid.d:158-161 nl_escape.
std::string nl_escape(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\n') o += "<newline>";
        else o += s[i];
    }
    return o;
}

// probe_sgrid.d:163-166 len_of — D's read() of an absent file THROWS, so a missing file
// is -1 here and the call that asked for it is wrapped in tryit by the probe.
int len_of(const std::string& p) {
    std::FILE* f = std::fopen(p.c_str(), "rb");
    if (f == nullptr) return -1;
    std::fseek(f, 0, SEEK_END);
    const long n = std::ftell(f);
    std::fclose(f);
    return static_cast<int>(n);
}

std::string read_whole(const std::string& p, bool& ok) {
    std::ifstream in(p, std::ios::binary);
    if (!in) { ok = false; return ""; }
    std::string s((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    ok = true;
    return s;
}

// probe_sgrid.d:169-172 body_of.
std::string body_of(const std::string& p) {
    bool ok = false;
    const std::string s = read_whole(p, ok);
    if (!ok) return "<absent>";
    return nl_escape(s);
}

// probe_sgrid.d:184-190 hex_of.
std::string hexb(const std::string& s) {
    std::string o;
    char buf[8];
    for (size_t i = 0; i < s.size(); ++i) {
        std::snprintf(buf, sizeof buf, "%02x", static_cast<unsigned char>(s[i]));
        o += buf;
    }
    return o;
}

std::string hex_of(const std::string& p) {
    bool ok = false;
    const std::string s = read_whole(p, ok);
    if (!ok) return "<absent>";
    return hexb(s);
}

// probe_sgrid.d:176-182 gunzip_body_of — a .gz artefact is compared as its DECOMPRESSED
// payload, because the container carries an mtime and differs between two runs of D
// itself (grid brief section 2 item 1).
std::string gunzip_body_of(const std::string& p) {
    std::ifstream probe(p, std::ios::binary);
    if (!probe) return "<absent>";
    probe.close();
    std::string out;
    eil::gzip::GzipByLine r(p);
    bool first = true;
    while (!r.empty()) {
        if (!first) out += "<newline>";
        first = false;
        out += r.front();
        r.popFront();
    }
    return out;
}

// The bit pattern of a double, for the rows that pin NaN/signed-zero provenance.
std::string bits(double x) {
    uint64_t u;
    std::memcpy(&u, &x, sizeof u);
    char buf[32];
    std::snprintf(buf, sizeof buf, "%016llx", static_cast<unsigned long long>(u));
    return buf;
}

// probe_sgrid.d:192-195 mk().
std::string mkr(const std::string& p, const std::string& body) {
    std::FILE* f = std::fopen(p.c_str(), "wb");
    if (f) { std::fwrite(body.data(), 1, body.size(), f); std::fclose(f); }
    return p;
}

// ------------------------------------------------------------------
// failure rows: probe_sgrid.d:254-288 tryit / tryit_cut / cut_at
// ------------------------------------------------------------------
// Which arm of D's `catch (Exception e) ... catch (Error e) ...` the throw belongs to,
// and the message it carries. D's split is by class, and the substrate reproduces it:
// ExceptionBase and ErrorBase are siblings, so catching the first cannot swallow the
// second. eil::format_error is std.format.FormatException, an Exception in Phobos but a
// sibling of ExceptionBase in the substrate (substrate/include/eil/format.hpp:40), so it
// needs its own arm or it would fall through to UNMAPPED.
struct Failure {
    const char* arm; // "exc" or "err", the probe's two writefln spellings
    std::string msg;
};

Failure classify(const std::exception_ptr& ep) {
    try {
        std::rethrow_exception(ep);
    } catch (const eil::format_error& e) {
        return {"exc", e.what()};
    } catch (const eil::ExceptionBase& e) {
        // Covers GeometryException, ConversionException (and its ConvOverflowException
        // child), ZlibException and the ErrnoException-shaped file-open failures.
        return {"exc", e.msg()};
    } catch (const eil::ErrorBase& e) {
        // object.Error (a bare `throw new Error`), AssertError, ArrayIndexError,
        // RangeError and OutOfMemoryError all land here (T-922).
        return {"err", e.msg()};
    } catch (const std::exception& e) {
        // Nothing in the ported code is expected to reach this; if a row does, it shows
        // up as a difference rather than as a silent pass.
        return {"UNMAPPED", e.what()};
    }
    return {"UNMAPPED", ""};
}

// probe_sgrid.d:254-266.
void tryit(const std::string& tag, const std::function<void()>& dg) {
    try {
        dg();
        emit(tag + " done");
    }
    catch (...) {
        const Failure f = classify(std::current_exception());
        emit(tag + " " + f.arm + "=" + nl_escape(f.msg));
    }
}

// probe_sgrid.d:269-288 — like tryit, but the message is cut at the "exception=" marker,
// because the tail of those D messages names a Phobos source line no port can produce.
void tryit_cut(const std::string& tag, const std::function<void()>& dg) {
    try {
        dg();
        emit(tag + " done");
    }
    catch (...) {
        const Failure f = classify(std::current_exception());
        std::string m = nl_escape(f.msg);
        const size_t at = m.find("exception=");
        if (at != std::string::npos) {
            m = m.substr(0, at) + "exception=<truncated>";
        }
        emit(tag + " " + f.arm + "=" + m);
    }
}

// ------------------------------------------------------------------
// dump helpers (probe_sgrid.d:198-252)
// ------------------------------------------------------------------
void dump_verts(const std::string& tag, const eil::Array<Vector3>& vv) {
    for (size_t i = 0; i < vv.length(); ++i) {
        emit(eil::format("%s vtx[%d] %.17g %.17g %.17g", tag, i, eil::re(vv[i].x),
                         eil::re(vv[i].y), eil::re(vv[i].z)));
    }
}

void dump_grid(const std::string& tag, const std::shared_ptr<StructuredGrid>& g) {
    if (g == nullptr) { emit(tag + " isnull=true"); return; } // probe:207
    emit(eil::format("%s dims=%d niv=%d njv=%d nkv=%d ncells=%d nvertices=%d label=%s", tag,
                     g->dimensions, g->niv, g->njv, g->nkv, g->ncells, g->nvertices, g->label));
    dump_verts(tag, g->vertices);
    emit(eil::format("%s vtx_id=%s", tag, g->vtx_id));
    emit(eil::format("%s bcTags=%s", tag, g->bcTags));
}

void dump_grid_base(const std::string& tag, const std::shared_ptr<Grid>& g) {
    if (g == nullptr) { emit(tag + " isnull=true"); return; }
    emit(eil::format("%s dims=%d niv=%d njv=%d nkv=%d ncells=%d nvertices=%d label=%s", tag,
                     g->dimensions, g->niv, g->njv, g->nkv, g->ncells, g->nvertices, g->label));
    dump_verts(tag, g->vertices);
}

void dump_ids(const std::string& tag, const eil::Array<size_t>& ids) {
    emit(eil::format("%s ids=%s", tag, ids));
}

void dump_copy(const std::string& tag, const std::array<size_t, 8>& c, size_t nvtx) {
    emit(eil::format("%s copy=%s nvtx=%d", tag, c, nvtx));
}

// probe_sgrid.d:234-239 prefilled — an 8-slot array pre-set to 999 so an unwritten tail
// is observable (the D:337 arm that forgets to write nvtx shows up here).
std::array<size_t, 8> prefilled() {
    std::array<size_t, 8> c;
    for (size_t i = 0; i < 8; ++i) c[i] = 999;
    return c;
}

void dump_doubles(const std::string& tag, const eil::Array<double>& d) {
    for (size_t i = 0; i < d.length(); ++i) {
        emit(eil::format("%s d[%d] %.17g", tag, i, eil::re(d[i])));
    }
}

// probe_sgrid.d:246-252 dump_g2d, over D's double[4][4].
void dump_g2d(const std::string& tag, const eil::geom::RSGrid& a) {
    for (size_t i = 0; i < 4; ++i) {
        emit(eil::format("%s row[%d] %.17g %.17g %.17g %.17g", tag, i, a[i][0], a[i][1],
                         a[i][2], a[i][3]));
    }
}

// ------------------------------------------------------------------
// shared builders (probe_sgrid.d:293-339)
// ------------------------------------------------------------------
// probe_sgrid.d:293-311. A concrete trilinear volume local to the probe: sgrid only
// needs the abstract base.
class ProbeVolume : public ParametricVolume {
  public:
    Vector3 lo, dx, dy, dz, c1, c2;

    ProbeVolume(Vector3 lo, Vector3 dx, Vector3 dy, Vector3 dz)
        : lo(lo), dx(dx), dy(dy), dz(dz), c1(0.11, -0.07, 0.05), c2(0.03, 0.02, -0.04) {}

    Vector3 operator()(double r, double s, double t) const override {
        Vector3 p = lo + dx * r + dy * s + dz * t;
        p += c1 * (r * s);
        p += c2 * (r * t);
        return p;
    }
    std::shared_ptr<ParametricVolume> dup() const override {
        return std::make_shared<ProbeVolume>(lo, dx, dy, dz);
    }
    std::string toString() const override { return "ProbeVolume"; }
};

std::shared_ptr<StructuredGrid> mk2d(size_t niv, size_t njv, const std::string& label = "g2d") {
    return std::make_shared<StructuredGrid>(niv, njv, 1, label);
}

// probe_sgrid.d:321-331 — a 5x4 grid on a Coons patch with linear clustering.
std::shared_ptr<StructuredGrid> surf_grid2d(const std::string& label = "sg2d") {
    const Vector3 p00(0.0, 0.1), p10(1.0, 0.1), p11(1.0, 1.1), p01(0.0, 1.1);
    auto patch = std::make_shared<CoonsPatch>(p00, p10, p11, p01);
    ClusterF cf;
    cf.append(std::make_shared<LinearFunction>(0.0, 1.0));
    cf.append(std::make_shared<LinearFunction>(0.0, 1.0));
    cf.append(std::make_shared<LinearFunction>(0.0, 1.0));
    cf.append(std::make_shared<LinearFunction>(0.0, 1.0));
    return std::make_shared<StructuredGrid>(*patch, 5, 4, cf, "simple", label);
}

// probe_sgrid.d:333-339 — a 3x3x2 grid on ProbeVolume with no clustering functions, so
// the D:123 padding to 12 runs.
std::shared_ptr<StructuredGrid> vol_grid3d(const std::string& label = "sg3d") {
    auto vol = std::make_shared<ProbeVolume>(Vector3(0.1, 0.2, 0.3), Vector3(1.0, 0.0, 0.0),
                                             Vector3(0.0, 1.0, 0.0), Vector3(0.0, 0.0, 1.0));
    ClusterF cf;
    return std::make_shared<StructuredGrid>(*vol, 3, 3, 2, cf, "simple", label);
}

// ------------------------------------------------------------------
// sg.ctor.* — size lattice (probe_sgrid.d:345-367, sgrid.d:35-70)
// ------------------------------------------------------------------
void grp_ctor() {
    auto g1 = std::make_shared<StructuredGrid>(5, 1, 1, "line");
    dump_grid("sg.ctor.1d", g1);
    auto g2 = std::make_shared<StructuredGrid>(5, 4); // nkv defaults to 1 (D:35)
    dump_grid("sg.ctor.2d", g2);
    auto g3 = std::make_shared<StructuredGrid>(3, 3, 2, "cube");
    dump_grid("sg.ctor.3d", g3);
    auto g4 = std::make_shared<StructuredGrid>(1, 1, 1, "pt");
    dump_grid("sg.ctor.1pt", g4);
    // niv == 0: ncells = niv-1 wraps (D:44), vertices stays empty.
    auto g5 = std::make_shared<StructuredGrid>(0, 1, 1, "zero");
    emit(eil::format("sg.ctor.n0 dims=%d niv=%d njv=%d nkv=%d ncells=%d nvertices=%d vlen=%d",
                     g5->dimensions, g5->niv, g5->njv, g5->nkv, g5->ncells, g5->nvertices,
                     g5->vertices.length()));
    // corpus row 55: the wrap makes vertices[] empty, so toString's vertices[0] read is
    // the failure, and it is an ArrayIndexError (an Error).
    tryit("sg.ctor.n0.tostr", [&g5]() {
        emit(eil::format("sg.ctor.n0.repr=%s", g5->toString()));
    });
    // Untouched vertices are .init (NaN), and vtx_id is filled (D:62-69).
    auto g6 = std::make_shared<StructuredGrid>(2, 2, 2, "nan");
    dump_grid("sg.ctor.nan", g6);
    emit(eil::format("sg.ctor.gridtype=%s", eil::geom::gridTypeName(g1->grid_type)));
    emit(eil::format("sg.ctor.dims 1d=%d 2d=%d 3d=%d 1pt=%d", g1->dimensions, g2->dimensions,
                     g3->dimensions, g4->dimensions));
}

// ------------------------------------------------------------------
// sg.idx.* — single_index / ijk_indices (probe_sgrid.d:369-400, grid.d:70-91)
// ------------------------------------------------------------------
void grp_idx() {
    auto g = std::make_shared<StructuredGrid>(3, 3, 2, "idx");
    for (size_t k = 0; k < 2; ++k) {
        for (size_t j = 0; j < 3; ++j) {
            for (size_t i = 0; i < 3; ++i) {
                emit(eil::format("sg.idx.3d i=%d j=%d k=%d si=%d", i, j, k,
                                 g->single_index(i, j, k)));
            }
        }
    }
    emit(eil::format("sg.idx.kdefault i=2 j=1 si=%d", g->single_index(2, 1)));
    for (size_t n = 0; n < 18; ++n) {
        const auto t = g->ijk_indices(n);
        emit(eil::format("sg.idx.inv n=%d ijk=%s", n, t));
    }
    auto g2 = std::make_shared<StructuredGrid>(5, 4, 1, "idx2");
    for (size_t n = 0; n < 20; ++n) {
        const auto t = g2->ijk_indices(n);
        emit(eil::format("sg.idx.inv2d n=%d ijk=%s si=%d", n, t,
                         g2->single_index(t[0], t[1], t[2])));
    }
    emit(eil::format("sg.idx.2d.kdefault si(1,2)=%d", g2->single_index(1, 2)));
    tryit("sg.idx.oob.i", [&g]() { g->single_index(3, 0, 0); });
    tryit("sg.idx.oob.j", [&g]() { g->single_index(0, 3, 0); });
    tryit("sg.idx.oob.k", [&g]() { g->single_index(0, 0, 2); });
    tryit("sg.idx.oob.ijk", [&g]() { g->ijk_indices(18); });
    // 1D lattice: njv==nkv==1, so j and k must be 0.
    auto g1 = std::make_shared<StructuredGrid>(4, 1, 1, "idx1");
    emit(eil::format("sg.idx.1d si(2,0,0)=%d", g1->single_index(2, 0, 0)));
    for (size_t n = 0; n < 4; ++n) {
        emit(eil::format("sg.idx.1d.inv n=%d ijk=%s", n, g1->ijk_indices(n)));
    }
}

// ------------------------------------------------------------------
// sg.opidx.* — opIndex (probe_sgrid.d:402-423, sgrid.d:206-221)
// ------------------------------------------------------------------
void grp_opidx() {
    auto g = surf_grid2d("opx");
    for (size_t j = 0; j < 4; ++j) {
        for (size_t i = 0; i < 5; ++i) {
            const Vector3* v = g->opIndex(i, j);
            emit(eil::format("sg.opidx.2d i=%d j=%d %.17g %.17g %.17g", i, j, eil::re(v->x),
                             eil::re(v->y), eil::re(v->z)));
        }
    }
    emit(eil::format("sg.opidx.kdef=%.17g", eil::re(g->opIndex(2, 1)->x)));
    for (size_t n = 0; n < 20; ++n) {
        emit(eil::format("sg.opidx.1arg n=%d %.17g", n, eil::re(g->opIndex(n)->x)));
    }
    tryit("sg.opidx.oob.i", [&g]() { auto q = g->opIndex(5, 0); (void)q; });
    tryit("sg.opidx.oob.j", [&g]() { auto q = g->opIndex(0, 4); (void)q; });
    tryit("sg.opidx.oob.k", [&g]() { auto q = g->opIndex(0, 0, 1); (void)q; });
    tryit("sg.opidx.oob.indx", [&g]() { auto q = g->opIndex(20); (void)q; });
    auto g3 = vol_grid3d("opx3d");
    tryit("sg.opidx.3d.oob", [&g3]() { auto q = g3->opIndex(0, 0, 2); (void)q; });
    const Vector3* v = g3->opIndex(1, 1, 1);
    emit(eil::format("sg.opidx.3d i=1 j=1 k=1 %.17g %.17g %.17g", eil::re(v->x), eil::re(v->y),
                     eil::re(v->z)));
}

// ------------------------------------------------------------------
// sg.cell.* — vertex id lists (probe_sgrid.d:425-501, sgrid.d:262-374)
// ------------------------------------------------------------------
void grp_cell() {
    auto g = std::make_shared<StructuredGrid>(4, 3, 2, "cell3d");
    for (size_t k = 0; k < 1; ++k) {
        for (size_t j = 0; j < 2; ++j) {
            for (size_t i = 0; i < 3; ++i) {
                const std::string tag = eil::format("sg.cell.3d i=%d j=%d k=%d", i, j, k);
                dump_ids(tag, g->get_vtx_id_list_for_cell(i, j, k));
                auto c = prefilled();
                size_t nvtx = 0;
                const std::string ctag = eil::format("sg.cell.3dc i=%d j=%d k=%d", i, j, k);
                g->copy_vtx_id_list_for_cell(c, nvtx, i, j, k);
                dump_copy(ctag, c, nvtx);
            }
        }
    }
    emit(eil::format("sg.cell.kdef ids=%s", g->get_vtx_id_list_for_cell(0, 0)));
    auto g2 = std::make_shared<StructuredGrid>(4, 3, 1, "cell2d");
    for (size_t j = 0; j < 2; ++j) {
        for (size_t i = 0; i < 3; ++i) {
            dump_ids(eil::format("sg.cell.2d i=%d j=%d", i, j),
                     g2->get_vtx_id_list_for_cell(i, j));
            auto c = prefilled();
            size_t nvtx = 0;
            g2->copy_vtx_id_list_for_cell(c, nvtx, i, j);
            dump_copy(eil::format("sg.cell.2dc i=%d j=%d", i, j), c, nvtx);
        }
    }
    auto g1 = std::make_shared<StructuredGrid>(5, 1, 1, "cell1d");
    for (size_t i = 0; i < 4; ++i) {
        dump_ids(eil::format("sg.cell.1d i=%d", i), g1->get_vtx_id_list_for_cell(i));
        auto c = prefilled();
        size_t nvtx = 0;
        g1->copy_vtx_id_list_for_cell(c, nvtx, i);
        dump_copy(eil::format("sg.cell.1dc i=%d", i), c, nvtx);
    }
    // The single-index forms. For a 3D lattice with njv != nkv, D:345's `nkc = njv-1` and
    // D:363's `nkc = nkv-1` must diverge: 3x3x2 -> both are 2, so a 4x3x2 lattice is the
    // one that shows it (nic=3, njc=2, nkc(D:345)=2).
    auto gd = std::make_shared<StructuredGrid>(4, 3, 5, "diverge");
    emit(eil::format("sg.cell.div niv=%d njv=%d nkv=%d ncells=%d", gd->niv, gd->njv, gd->nkv,
                     gd->ncells));
    for (const long n : {0L, 1L, 2L, 3L, 6L, 9L, 12L, 18L, 23L, 24L, 30L}) {
        tryit(eil::format("sg.cell.div.iindx n=%d", n), [&gd, n]() {
            dump_ids(eil::format("sg.cell.div.iindx.out n=%d", n),
                     gd->get_vtx_id_list_for_cell(static_cast<size_t>(n)));
        });
        tryit(eil::format("sg.cell.div.copy n=%d", n), [&gd, n]() {
            auto c = prefilled();
            size_t nvtx = 0;
            gd->copy_vtx_id_list_for_cell(c, nvtx, static_cast<size_t>(n));
            dump_copy(eil::format("sg.cell.div.copy.out n=%d", n), c, nvtx);
        });
    }
    for (const long n : {0L, 1L, 2L, 3L, 5L, 6L, 9L, 12L}) {
        tryit(eil::format("sg.cell.2dindx n=%d", n), [&g2, n]() {
            dump_ids(eil::format("sg.cell.2dindx.out n=%d", n),
                     g2->get_vtx_id_list_for_cell(static_cast<size_t>(n)));
        });
    }
    for (const long n : {0L, 1L, 2L, 3L, 4L, 7L}) {
        tryit(eil::format("sg.cell.1dindx n=%d", n), [&g1, n]() {
            dump_ids(eil::format("sg.cell.1dindx.out n=%d", n),
                     g1->get_vtx_id_list_for_cell(static_cast<size_t>(n)));
        });
    }
    tryit("sg.cell.1d.jnonzero", [&g1]() { g1->get_vtx_id_list_for_cell(0, 1, 0); });
    tryit("sg.cell.1d.knonzero", [&g1]() { g1->get_vtx_id_list_for_cell(0, 0, 1); });
    tryit("sg.cell.2d.jbad", [&g2]() { g2->get_vtx_id_list_for_cell(0, 3); });
    tryit("sg.cell.2d.kbad", [&g2]() { g2->get_vtx_id_list_for_cell(0, 0, 1); });
    tryit("sg.cell.3d.kbad", [&g]() { g->get_vtx_id_list_for_cell(0, 0, 2); });
    tryit("sg.cell.3d.ibad", [&g]() { g->get_vtx_id_list_for_cell(3, 0, 0); });
    // Zero cells in a direction (D:366-368) — a GeometryException, so the `exc=` arm.
    auto gz = std::make_shared<StructuredGrid>(1, 1, 1, "onept");
    tryit("sg.cell.zero", [&gz]() {
        auto c = prefilled();
        size_t nvtx = 0;
        gz->copy_vtx_id_list_for_cell(c, nvtx, 0);
    });
    tryit("sg.cell.zero.ijk", [&gz]() { gz->get_vtx_id_list_for_cell(0, 0, 0); });
    // A 1D grid asked for a j!=0 cell through the index form.
    tryit("sg.cell.1dindx.oob", [&g1]() { g1->get_vtx_id_list_for_cell(4); });
}

// ------------------------------------------------------------------
// sg.abst.* — the abstract table through a Grid handle
// (probe_sgrid.d:503-542, sgrid.d:93-122)
// ------------------------------------------------------------------
void grp_abst() {
    std::shared_ptr<Grid> g = surf_grid2d("abst2d");
    emit(eil::format("sg.abst.2d nvtx=%d vtktype=%d celltype=%d", g->number_of_vertices_for_cell(0),
                     g->vtk_element_type_for_cell(0), g->get_cell_type(0)));
    std::shared_ptr<Grid> g3 = vol_grid3d("abst3d");
    emit(eil::format("sg.abst.3d nvtx=%d vtktype=%d celltype=%d",
                     g3->number_of_vertices_for_cell(0), g3->vtk_element_type_for_cell(0),
                     g3->get_cell_type(0)));
    std::shared_ptr<Grid> g1 = std::make_shared<StructuredGrid>(5, 1, 1, "abst1d");
    emit(eil::format("sg.abst.1d nvtx=%d vtktype=%d celltype=%d",
                     g1->number_of_vertices_for_cell(0), g1->vtk_element_type_for_cell(0),
                     g1->get_cell_type(0)));
    dump_grid_base("sg.abst.handle2d", g);
    dump_ids("sg.abst.vtxlist", g->get_vtx_id_list_for_cell(1, 1));
    {
        auto c = prefilled();
        size_t nvtx = 0;
        g->copy_vtx_id_list_for_cell(c, nvtx, 1, 1);
        dump_copy("sg.abst.copy", c, nvtx);
    }
    emit(eil::format("sg.abst.opidx %.17g %.17g", eil::re(g->opIndex(0, 0, 0)->x),
                     eil::re(g->opIndex(0, 0, 0)->y)));
    // get_boundary_grid returns a Grid handle; print the six faces.
    std::shared_ptr<Grid> v3 = std::make_shared<StructuredGrid>(3, 4, 2, "bnd");
    for (size_t j = 0; j < 4; ++j) {
        for (size_t i = 0; i < 3; ++i) {
            v3->opIndex(i, j, 0)->set(static_cast<double>(i) * 1.0,
                                      static_cast<double>(j) * 2.0,
                                      static_cast<double>(i + j) * 0.5);
        }
    }
    dump_grid("sg.abst.src3d", std::static_pointer_cast<StructuredGrid>(v3));
    for (size_t b = 0; b < 6; ++b) {
        const std::string tag = eil::format("sg.abst.bnd b=%d", b);
        dump_grid_base(tag, v3->get_boundary_grid(b));
        dump_ids(eil::format("sg.abst.blc b=%d", b), v3->get_list_of_boundary_cells(b));
    }
    tryit("sg.abst.bnd.2d", [&g]() { g->get_boundary_grid(0); });
    // 2D boundary cell lists (faces 0..3 only; Face.bottom/top is out of range).
    std::shared_ptr<Grid> q2 = mk2d(4, 3, "bnd2d");
    for (size_t b = 0; b < 4; ++b) {
        dump_ids(eil::format("sg.abst.blc2d b=%d", b), q2->get_list_of_boundary_cells(b));
    }
    // corpus row 415: D's final switch on an out-of-range face index is a SwitchError,
    // whose text is "No appropriate switch clause found" and which is an Error.
    tryit("sg.abst.blc2d.k", [&q2]() { q2->get_list_of_boundary_cells(4); });
}

// ------------------------------------------------------------------
// sg.str.* — toString (probe_sgrid.d:544-558, sgrid.d:223-232)
// ------------------------------------------------------------------
void grp_str() {
    emit(eil::format("sg.str.2d=%s", surf_grid2d("str")->toString()));
    emit(eil::format("sg.str.3d=%s", vol_grid3d("str3d")->toString()));
    emit(eil::format("sg.str.1d=%s",
                     std::make_shared<StructuredGrid>(3, 1, 1, "str1d")->toString()));
    auto gq = mk2d(3, 3, "q");
    gq->set_tag(eil::geom::Face::west, "W");
    gq->set_tag(99, "nope");
    emit(eil::format("sg.str.tags=%s", gq->toString()));
    // A NaN vertex prints as Vector3(nan, nan, nan) inside the repr.
    emit(eil::format("sg.str.nan=%s",
                     std::make_shared<StructuredGrid>(2, 2, 1, "nan")->toString()));
}

// ------------------------------------------------------------------
// sg.tag.* — set_tag / set_tags (probe_sgrid.d:560-582, sgrid.d:174-204)
// ------------------------------------------------------------------
eil::AA<std::string, std::string> tags(const std::vector<std::pair<std::string, std::string>>& kv) {
    eil::AA<std::string, std::string> a;
    for (const auto& p : kv) a.set(p.first, p.second);
    return a;
}

void grp_tag() {
    auto g = mk2d(4, 3, "tag");
    emit(eil::format("sg.tag.0=%s", g->set_tag(0, "west-tag")));
    emit(eil::format("sg.tag.inrange=%s", g->set_tag(5, "out")));
    emit(eil::format("sg.tag.bcs=%s", g->bcTags));
    emit(eil::format("sg.tag.n=%d", g->set_tags(tags({{"west", "W"}, {"east", "E"}}))));
    emit(eil::format("sg.tag.bcs2=%s", g->bcTags));
    emit(eil::format("sg.tag.n2=%d", g->set_tags(tags({{"south", "S"}, {"north", "N"},
                                                       {"bottom", "B"}, {"top", "T"}}))));
    emit(eil::format("sg.tag.bcs3=%s", g->bcTags));
    auto g3 = std::make_shared<StructuredGrid>(2, 2, 2, "tag3");
    emit(eil::format("sg.tag.3d.n=%d",
                     g3->set_tags(tags({{"west", "W"}, {"east", "E"}, {"south", "S"},
                                        {"north", "N"}, {"bottom", "B"}, {"top", "T"}}))));
    emit(eil::format("sg.tag.3d.bcs=%s", g3->bcTags));
    auto g1 = std::make_shared<StructuredGrid>(3, 1, 1, "tag1");
    emit(eil::format("sg.tag.1d.n=%d",
                     g1->set_tags(tags({{"west", "W"}, {"east", "E"}, {"south", "S"},
                                        {"north", "N"}, {"bottom", "B"}, {"top", "T"}}))));
    emit(eil::format("sg.tag.1d.bcs=%s", g1->bcTags));
    eil::AA<std::string, std::string> notags;
    emit(eil::format("sg.tag.empty.n=%d", g1->set_tags(notags)));
    emit(eil::format("sg.tag.unknown.n=%d", g1->set_tags(tags({{"nonsense", "X"}}))));
}

// ------------------------------------------------------------------
// sg.dup.* — copy constructor and dup (probe_sgrid.d:584-598, sgrid.d:162-172)
// ------------------------------------------------------------------
void grp_dup() {
    auto g = surf_grid2d("dupsrc");
    g->set_tag(eil::geom::Face::top, "T"); // index 5 on a 4-slot bcTags: not set (D:177)
    auto c = std::make_shared<StructuredGrid>(*g);
    emit(eil::format("sg.dup.copy=%s", c->toString()));
    emit(eil::format("sg.dup.aliased=%s", c == g));
    c->opIndex(0, 0)->x = 42.0;
    emit(eil::format("sg.dup.after.write src=%.17g copy=%.17g", eil::re(g->opIndex(0, 0)->x),
                     eil::re(c->opIndex(0, 0)->x)));
    auto d = g->dup();
    emit(eil::format("sg.dup.dup=%s", d->toString()));
    emit(eil::format("sg.dup.dup.aliased=%s", d == g));
    auto c3 = std::make_shared<StructuredGrid>(*vol_grid3d("dup3d"));
    dump_grid("sg.dup.3d", c3);
}

// ------------------------------------------------------------------
// sg.sub.* — subgrid (probe_sgrid.d:600-632, sgrid.d:376-420)
// ------------------------------------------------------------------
void grp_sub() {
    auto g = std::make_shared<StructuredGrid>(4, 4, 1, "sub");
    for (size_t j = 0; j < 4; ++j) {
        for (size_t i = 0; i < 4; ++i) {
            g->opIndex(i, j)->set(static_cast<double>(i) * 2.0, static_cast<double>(j) * 3.0,
                                  static_cast<double>(i) * static_cast<double>(j));
        }
    }
    dump_grid("sg.sub.a", g->subgrid(1, 3, 1, 2));
    dump_grid("sg.sub.b", g->subgrid(0, 2, 0, 2));
    dump_grid("sg.sub.c", g->subgrid(2, 2, 2, 2));
    dump_grid("sg.sub.d", g->subgrid(0, 4, 0, 4));
    auto g3 = std::make_shared<StructuredGrid>(3, 3, 3, "sub3");
    for (size_t k = 0; k < 3; ++k) {
        for (size_t j = 0; j < 3; ++j) {
            for (size_t i = 0; i < 3; ++i) {
                g3->opIndex(i, j, k)->set(static_cast<double>(i), static_cast<double>(j),
                                          static_cast<double>(k));
            }
        }
    }
    dump_grid("sg.sub.3d", g3->subgrid(1, 2, 1, 2, 1, 2));
    auto g1 = std::make_shared<StructuredGrid>(5, 1, 1, "sub1");
    dump_grid("sg.sub.1d", g1->subgrid(1, 3));
    // The six Error wordings of D:381-401 — bare `throw new Error`, so `err=`.
    tryit("sg.sub.1d.ni1", [&g1]() { g1->subgrid(0, 1); });
    tryit("sg.sub.1d.nj2", [&g1]() { g1->subgrid(0, 2, 0, 2); });
    tryit("sg.sub.1d.nk2", [&g1]() { g1->subgrid(0, 2, 0, 1, 0, 2); });
    tryit("sg.sub.2d.nj1", [&g]() { g->subgrid(0, 2, 0, 1); });
    tryit("sg.sub.2d.nk2", [&g]() { g->subgrid(0, 2, 0, 2, 0, 2); });
    tryit("sg.sub.2d.ovr.i", [&g]() { g->subgrid(2, 4, 0, 2); });
    tryit("sg.sub.2d.ovr.j", [&g]() { g->subgrid(0, 2, 2, 4); });
    tryit("sg.sub.3d.nk1", [&g3]() { g3->subgrid(0, 2, 0, 2, 0, 1); });
    tryit("sg.sub.3d.ovr.k", [&g3]() { g3->subgrid(0, 2, 0, 2, 1, 3); });
}

// ------------------------------------------------------------------
// sg.bnd.* / sg.blc.* — boundary extraction with distinct sizes
// (probe_sgrid.d:634-652, sgrid.d:422-578)
// ------------------------------------------------------------------
void grp_bnd() {
    // niv, njv, nkv all different so any mix-up in D:427-446 is visible.
    std::shared_ptr<Grid> g = std::make_shared<StructuredGrid>(3, 4, 5, "bd");
    for (size_t k = 0; k < 5; ++k) {
        for (size_t j = 0; j < 4; ++j) {
            for (size_t i = 0; i < 3; ++i) {
                g->opIndex(i, j, k)->set(
                    static_cast<double>(i) * 1.0 + 100.0 * static_cast<double>(k) +
                        10.0 * static_cast<double>(j),
                    static_cast<double>(j), static_cast<double>(k));
            }
        }
    }
    auto sg = std::static_pointer_cast<StructuredGrid>(g);
    for (size_t b = 0; b < 6; ++b) {
        dump_grid_base(eil::format("sg.bnd b=%d", b), sg->get_boundary_grid(b));
        // probe:649-650 calls the getter twice — once for the count in the tag, once for
        // the list — so the mirror does too rather than reusing one result.
        dump_ids(eil::format("sg.blc b=%d n=%d", b, sg->get_list_of_boundary_cells(b).length()),
                 sg->get_list_of_boundary_cells(b));
    }
}

// ------------------------------------------------------------------
// sg.path.* — Path constructor and make_grid_from_path
// (probe_sgrid.d:654-667, sgrid.d:73, :580)
// ------------------------------------------------------------------
void grp_path() {
    std::shared_ptr<eil::geom::Path> pth =
        std::make_shared<eil::geom::Line>(Vector3(0.0, 0.1, 0.2), Vector3(1.0, 0.3, 0.4));
    auto g = std::make_shared<StructuredGrid>(*pth, 6,
                                             std::make_shared<LinearFunction>(0.0, 1.0), "p1d");
    dump_grid("sg.path.linear", g);
    auto g2 = std::make_shared<StructuredGrid>(*pth, 6,
                                               std::make_shared<eil::geom::QuadraticFunction>(0.3,
                                                                                             false),
                                               "p1dc");
    dump_grid("sg.path.quad", g2);
    auto g3 = std::make_shared<StructuredGrid>(*pth, 6); // clusterf and label defaults (D:74-75)
    dump_grid("sg.path.default", g3);
    emit(eil::format("sg.path.dims=%d ncells=%d", g->dimensions, g->ncells));
    tryit("sg.path.niv1", [&pth]() { (void)std::make_shared<StructuredGrid>(*pth, 1); });
}

// ------------------------------------------------------------------
// sg.surf.* — ParametricSurface constructor (probe_sgrid.d:669-696, sgrid.d:82-101)
// ------------------------------------------------------------------
void grp_surf() {
    const Vector3 p00(0.0, 0.1), p10(1.0, 0.1), p11(1.0, 1.1), p01(0.0, 1.1);
    std::shared_ptr<ParametricSurface> patch =
        std::make_shared<CoonsPatch>(p00, p10, p11, p01);
    ClusterF cf4;
    cf4.append(std::make_shared<LinearFunction>(0.0, 1.0));
    cf4.append(std::make_shared<eil::geom::RobertsFunction>(true, true, 1.2));
    cf4.append(std::make_shared<LinearFunction>(0.0, 1.0));
    cf4.append(std::make_shared<eil::geom::QuadraticFunction>(0.7, true));
    auto gs = std::make_shared<StructuredGrid>(*patch, 5, 4, cf4, "simple", "sfs");
    dump_grid("sg.surf.simple", gs);
    emit(eil::format("sg.surf.simple.bad=%.17g", gs->measure_of_badness()));
    auto gl = std::make_shared<StructuredGrid>(*patch, 5, 4, cf4, "linear", "sfl");
    dump_grid("sg.surf.bilinear.by.linear", gl);
    auto gb = std::make_shared<StructuredGrid>(*patch, 5, 4, cf4, "bilinear", "sfb");
    dump_grid("sg.surf.bilinear", gb);
    // clusterf shorter than 4 is padded with LinearFunction (D:89).
    ClusterF cf2;
    cf2.append(std::make_shared<eil::geom::QuadraticFunction>(0.3, false));
    auto gp = std::make_shared<StructuredGrid>(*patch, 4, 3, cf2, "simple", "sfpad");
    dump_grid("sg.surf.pad", gp);
    emit(eil::format("sg.surf.pad.bad=%.17g", gp->measure_of_badness()));
    tryit("sg.surf.unknown", [&patch, &cf2]() {
        (void)std::make_shared<StructuredGrid>(*patch, 4, 3, cf2, "cubic", "x");
    });
    tryit("sg.surf.empty", [&patch, &cf2]() {
        (void)std::make_shared<StructuredGrid>(*patch, 4, 3, cf2, "", "x");
    });
    // The 12-argument padding for the volume constructor (D:123) is in sg.vol.*.
    emit(eil::format("sg.surf.tagcount=%d", gs->set_tags(tags({{"north", "N"}}))));
}

// ------------------------------------------------------------------
// sg.rgrid.* — redistributed parameters (probe_sgrid.d:698-725, :1664-1673,
// sgrid.d:104-113, :648-688, :1472-1522)
// ------------------------------------------------------------------
// D's `arr.length = n` extends with the element type's .init, and double.init is NaN.
// eil::Array::set_length value-initialises instead (substrate/include/eil/array.hpp:162-170),
// so the probe's half-filled 16-slot arrays are built through this helper to keep D's
// untouched slots observable — corpus rows 757-760 (`sg.rgrid.identity.s row[*] 0 nan nan 1`)
// are exactly those slots.
eil::Array<double> d_init_doubles(size_t n) {
    eil::Array<double> a;
    a.set_length(n);
    for (size_t i = 0; i < n; ++i) a[i] = eil::d_init<double>();
    return a;
}

// probe_sgrid.d:1664-1673.
void rs_grids_to_list_from_data(const std::string& tag, eil::geom::RSGrid& rg,
                                eil::geom::RSGrid& sgr) {
    eil::Array<double> id = d_init_doubles(16);
    id[2] = 0.0; id[3] = 0.0; id[5] = 0.0; id[7] = 1.0;
    id[8] = 0.0; id[9] = 0.0; id[11] = 0.0; id[13] = 1.0;
    eil::geom::list_to_rs_grids(id, rg, sgr);
    dump_g2d(tag + ".r", rg);
    dump_g2d(tag + ".s", sgr);
}

void grp_rgrid() {
    const Vector3 p00(0.0, 0.1), p10(1.0, 0.1), p11(1.0, 1.1), p01(0.0, 1.1);
    std::shared_ptr<ParametricSurface> patch = std::make_shared<CoonsPatch>(p00, p10, p11, p01);
    ClusterF cf;
    cf.append(std::make_shared<LinearFunction>(0.0, 1.0));
    cf.append(std::make_shared<LinearFunction>(0.0, 1.0));
    cf.append(std::make_shared<LinearFunction>(0.0, 1.0));
    cf.append(std::make_shared<LinearFunction>(0.0, 1.0));
    eil::geom::RSGrid rg{}, sgr{};
    rs_grids_to_list_from_data("sg.rgrid.identity", rg, sgr);
    // The identity redistribution maps every (r,s) through a NaN s_star, so all twenty
    // vertices stay .init — corpus rows 761-780 say so, and they are the reason this row
    // group is in the gate at all.
    auto g0 = std::make_shared<StructuredGrid>(*patch, 5, 4, cf, rg, sgr, "rgid");
    dump_grid("sg.rgrid.ident", g0);
    // A non-trivial redistribution: the 16 free values of D:1479-1494.
    eil::Array<double> data = d_init_doubles(16);
    for (size_t i = 0; i < 16; ++i) data[i] = 0.05 * static_cast<double>(i);
    eil::geom::list_to_rs_grids(data, rg, sgr);
    dump_g2d("sg.rgrid.r", rg);
    dump_g2d("sg.rgrid.s", sgr);
    auto g1 = std::make_shared<StructuredGrid>(*patch, 5, 4, cf, rg, sgr, "rg");
    dump_grid("sg.rgrid.grid", g1);
    emit(eil::format("sg.rgrid.bad=%.17g", g1->measure_of_badness()));
    eil::Array<double> back = d_init_doubles(16);
    eil::geom::rs_grids_to_list(rg, sgr, back);
    dump_doubles("sg.rgrid.roundtrip", back);
}

// ------------------------------------------------------------------
// sg.join.* — joinGrid (probe_sgrid.d:1052-1107, sgrid.d:1130-1256)
// ------------------------------------------------------------------
void grp_join() {
    auto a = mk2d(4, 3, "A");
    for (size_t j = 0; j < 3; ++j) {
        for (size_t i = 0; i < 4; ++i) {
            a->opIndex(i, j)->set(static_cast<double>(i) * 1.0,
                                  static_cast<double>(j) * 1.0, 0.0);
        }
    }
    auto b = mk2d(3, 3, "B");
    for (size_t j = 0; j < 3; ++j) {
        for (size_t i = 0; i < 3; ++i) {
            b->opIndex(i, j)->set(static_cast<double>(i + 3) * 1.0,
                                  static_cast<double>(j) * 1.0, 0.0);
        }
    }
    auto aj = a->dup();
    aj->joinGrid(b, "east");
    dump_grid("sg.join.east", aj);
    auto bj = a->dup();
    bj->joinGrid(b, "imax");
    dump_grid("sg.join.imax", bj);
    emit(eil::format("sg.join.same=%s", aj->toString() == bj->toString()));
    // north join
    auto c = mk2d(4, 3, "C");
    for (size_t j = 0; j < 3; ++j) {
        for (size_t i = 0; i < 4; ++i) {
            c->opIndex(i, j)->set(static_cast<double>(i) * 1.0,
                                  static_cast<double>(j) * 1.0 + 2.0, 0.0);
        }
    }
    auto nj = a->dup();
    nj->joinGrid(c, "north");
    dump_grid("sg.join.north", nj);
    auto nj2 = a->dup();
    nj2->joinGrid(c, "jmax");
    dump_grid("sg.join.jmax", nj2);
    // guards
    tryit("sg.join.badloc", [&a, &b]() { a->joinGrid(b, "west"); });
    tryit("sg.join.emptyloc", [&a, &b]() { a->joinGrid(b, ""); });
    std::shared_ptr<Grid> g3 = vol_grid3d("3d");
    tryit("sg.join.3d", [&g3, &b]() {
        std::static_pointer_cast<StructuredGrid>(g3)->joinGrid(b, "east");
    });
    auto mismatch = mk2d(3, 2, "M");
    for (size_t j = 0; j < 2; ++j) {
        for (size_t i = 0; i < 3; ++i) {
            mismatch->opIndex(i, j)->set(static_cast<double>(i + 3) * 1.0,
                                         static_cast<double>(j) * 1.0, 0.0);
        }
    }
    tryit("sg.join.njv", [&a, &mismatch]() { a->joinGrid(mismatch, "east"); });
    auto nkvbad = mk2d(3, 3, "K");
    emit(eil::format("sg.join.nkv %d %d", a->nkv, nkvbad->nkv));
    // non-coincident vertex
    auto apart = mk2d(3, 3, "AP");
    for (size_t j = 0; j < 3; ++j) {
        for (size_t i = 0; i < 3; ++i) {
            apart->opIndex(i, j)->set(static_cast<double>(i) * 1.0 + 1.0e-3,
                                      static_cast<double>(j) * 1.0, 0.0);
        }
    }
    tryit("sg.join.coincide", [&a, &apart]() { a->joinGrid(apart, "east"); });
    auto nbad = mk2d(5, 3, "NB");
    for (size_t j = 0; j < 3; ++j) {
        for (size_t i = 0; i < 5; ++i) {
            nbad->opIndex(i, j)->set(static_cast<double>(i) * 1.0,
                                     static_cast<double>(j) * 1.0, 0.0);
        }
    }
    tryit("sg.join.niv", [&a, &nbad]() { a->joinGrid(nbad, "north"); });
    auto nbad2 = mk2d(4, 3, "NB2");
    for (size_t j = 0; j < 3; ++j) {
        for (size_t i = 0; i < 4; ++i) {
            nbad2->opIndex(i, j)->set(static_cast<double>(i) * 1.0,
                                      static_cast<double>(j) * 1.0 + 1.0e-2, 0.0);
        }
    }
    tryit("sg.join.coincide.north", [&a, &nbad2]() { a->joinGrid(nbad2, "north"); });
}

// ------------------------------------------------------------------
// sg.slab.* / sg.wedge.* (probe_sgrid.d:1110-1133, sgrid.d:1258-1310)
// ------------------------------------------------------------------
void grp_slab_wedge() {
    auto g = mk2d(3, 2, "slabsrc");
    for (size_t j = 0; j < 2; ++j) {
        for (size_t i = 0; i < 3; ++i) {
            g->opIndex(i, j)->set(static_cast<double>(i) * 1.0,
                                  static_cast<double>(j) * 1.0, 0.5);
        }
    }
    dump_grid("sg.slab.sym", g->makeSlabGrid(Vector3(0.0, 0.0, 2.0), true, ""));
    dump_grid("sg.slab.asym", g->makeSlabGrid(Vector3(0.0, 0.0, 2.0), false, ""));
    dump_grid("sg.slab.dbl", g->makeSlabGrid(3.0, true, "dbl"));
    dump_grid("sg.slab.dblasym", g->makeSlabGrid(3.0, false, "dblasym"));
    emit(eil::format("sg.slab.srclabel=%s", g->makeSlabGrid(1.0)->label));
    // makeWedgeGrid around the x-axis. D:1294 atan2 and the :1299/:1304 cos/sin pair of one
    // argument go through eil::atan2 / eil::cos / eil::sin (T-910, T-920).
    auto w = mk2d(3, 2, "wedgesrc");
    for (size_t j = 0; j < 2; ++j) {
        for (size_t i = 0; i < 3; ++i) {
            w->opIndex(i, j)->set(static_cast<double>(i) * 0.5,
                                  1.0 + static_cast<double>(j) * 0.25,
                                  0.1 * static_cast<double>(i));
        }
    }
    dump_grid("sg.wedge.sym", w->makeWedgeGrid(0.3, true, "", 3));
    dump_grid("sg.wedge.asym", w->makeWedgeGrid(0.3, false, "", 3));
    dump_grid("sg.wedge.sym2", w->makeWedgeGrid(1.0, true));
    dump_grid("sg.wedge.nkv2", w->makeWedgeGrid(0.75, false, "w2", 2));
    emit(eil::format("sg.wedge.pi=%s",
                     w->makeWedgeGrid(3.141592653589793, true, "wpi", 5)->toString()));
    tryit("sg.slab.3d", []() { (void)vol_grid3d("x")->makeSlabGrid(1.0); });
    tryit("sg.wedge.3d", []() { (void)vol_grid3d("x")->makeWedgeGrid(0.3); });
}

// ------------------------------------------------------------------
// sg.bad.* — measure_of_badness (probe_sgrid.d:1135-1179, sgrid.d:1320-1394)
// ------------------------------------------------------------------
void grp_bad() {
    const Vector3 p00(0.0, 0.1), p10(1.0, 0.1), p11(1.0, 1.1), p01(0.0, 1.1);
    std::shared_ptr<ParametricSurface> patch = std::make_shared<CoonsPatch>(p00, p10, p11, p01);
    ClusterF cflin;
    for (int n = 0; n < 4; ++n) cflin.append(std::make_shared<LinearFunction>(0.0, 1.0));
    auto gid = std::make_shared<StructuredGrid>(*patch, 5, 5, cflin, "simple", "badid");
    emit(eil::format("sg.bad.identical=%.17g", gid->measure_of_badness()));
    ClusterF cfclust;
    cfclust.append(std::make_shared<LinearFunction>(0.0, 1.0));
    cfclust.append(std::make_shared<eil::geom::RobertsFunction>(true, true, 2.0));
    cfclust.append(std::make_shared<LinearFunction>(0.0, 1.0));
    cfclust.append(std::make_shared<eil::geom::RobertsFunction>(true, true, -2.0));
    auto gc = std::make_shared<StructuredGrid>(*patch, 5, 5, cfclust, "simple", "badc");
    emit(eil::format("sg.bad.clustered=%.17g", gc->measure_of_badness()));
    emit(eil::format("sg.bad.clustered.bilinear=%.17g",
                     std::make_shared<StructuredGrid>(*patch, 5, 5, cfclust, "bilinear",
                                                     "badcb")
                         ->measure_of_badness()));
    // A skewed (non-orthogonal) quadrilateral mesh.
    auto sk = mk2d(4, 4, "skew");
    for (size_t j = 0; j < 4; ++j) {
        for (size_t i = 0; i < 4; ++i) {
            sk->opIndex(i, j)->set(static_cast<double>(i) * 1.0 + 0.3 * static_cast<double>(j),
                                   static_cast<double>(j) * 1.0, 0.0);
        }
    }
    emit(eil::format("sg.bad.skew=%.17g", sk->measure_of_badness()));
    // An inverted cell: negative area triggers the penalty (D:1373).
    auto inv = mk2d(3, 2, "inv");
    inv->opIndex(0, 0)->set(0.0, 0.0, 0.0);
    inv->opIndex(1, 0)->set(1.0, 0.0, 0.0);
    inv->opIndex(2, 0)->set(2.0, 0.0, 0.0);
    inv->opIndex(0, 1)->set(0.0, 1.0, 0.0);
    inv->opIndex(1, 1)->set(1.0, -1.0, 0.0);
    inv->opIndex(2, 1)->set(2.0, 1.0, 0.0);
    emit(eil::format("sg.bad.inverted=%.17g", inv->measure_of_badness()));
    // Degenerate: a zero-area cell (repeated vertices).
    auto deg = mk2d(3, 3, "deg");
    for (size_t j = 0; j < 3; ++j) {
        for (size_t i = 0; i < 3; ++i) {
            deg->opIndex(i, j)->set(static_cast<double>(i) * 1.0,
                                    static_cast<double>(i) * 1.0, 0.0);
        }
    }
    emit(eil::format("sg.bad.degenerate=%.17g", deg->measure_of_badness()));
    // A single-row mesh (njv == 2) and a single-column one (niv == 2).
    emit(eil::format("sg.bad.2row=%.17g", surf_grid2d("b2")->measure_of_badness()));
    tryit("sg.bad.3d", []() { (void)vol_grid3d("b3")->measure_of_badness(); });
    tryit("sg.bad.1d", []() {
        (void)std::make_shared<StructuredGrid>(5, 1, 1, "b1")->measure_of_badness();
    });
}

// ------------------------------------------------------------------
// sg.rs.* — the two list/grid mappers (probe_sgrid.d:1181-1194, sgrid.d:1472-1522)
// ------------------------------------------------------------------
void grp_rs() {
    eil::Array<double> data = d_init_doubles(16);
    for (size_t i = 0; i < 16; ++i) data[i] = 0.01 * static_cast<double>(i) + 0.02;
    eil::geom::RSGrid rg{}, sgr{};
    eil::geom::list_to_rs_grids(data, rg, sgr);
    dump_g2d("sg.rs.r", rg);
    dump_g2d("sg.rs.s", sgr);
    eil::Array<double> back = d_init_doubles(16);
    eil::geom::rs_grids_to_list(rg, sgr, back);
    dump_doubles("sg.rs.roundtrip", back);
    // The assert on a short array (D:1478, :1504).
    tryit("sg.rs.short.list", [&rg, &sgr]() {
        eil::Array<double> d = d_init_doubles(15);
        eil::geom::rs_grids_to_list(rg, sgr, d);
    });
    tryit("sg.rs.short.get", [&rg, &sgr]() {
        eil::Array<double> d = d_init_doubles(0);
        eil::geom::list_to_rs_grids(d, rg, sgr);
    });
}

// ------------------------------------------------------------------
// sg.determ.* — determine_rs_grids (probe_sgrid.d:1205-1234, sgrid.d:1396-1423),
// which minimises through nm/nelmin
// ------------------------------------------------------------------
void grp_determ() {
    const Vector3 p00(0.0, 0.1), p10(1.0, 0.1), p11(1.0, 1.1), p01(0.0, 1.1);
    std::shared_ptr<ParametricSurface> patch = std::make_shared<CoonsPatch>(p00, p10, p11, p01);
    ClusterF cf;
    cf.append(std::make_shared<LinearFunction>(0.0, 1.0));
    cf.append(std::make_shared<eil::geom::RobertsFunction>(true, true, 1.5));
    cf.append(std::make_shared<LinearFunction>(0.0, 1.0));
    cf.append(std::make_shared<eil::geom::RobertsFunction>(true, true, -1.5));
    auto g = std::make_shared<StructuredGrid>(*patch, 6, 5, cf, "simple", "det");
    emit(eil::format("sg.determ.before=%.17g", g->measure_of_badness()));
    eil::geom::RSGrid rg{}, sgr{};
    // Start from the identity redistribution (D:1407 reads these values). D's `id[] = 0.0`
    // writes every slot, so there is no .init tail to reproduce here.
    eil::Array<double> id = d_init_doubles(16);
    for (size_t i = 0; i < 16; ++i) id[i] = 0.0;
    id[2] = 0.0; id[7] = 1.0; id[13] = 1.0;
    eil::geom::list_to_rs_grids(id, rg, sgr);
    dump_g2d("sg.determ.rg0", rg);
    dump_g2d("sg.determ.sg0", sgr);
    const bool conv = g->determine_rs_grids(*patch, cf, rg, sgr);
    emit(eil::format("sg.determ.conv=%s", conv));
    dump_g2d("sg.determ.rg", rg);
    dump_g2d("sg.determ.sg", sgr);
    emit(eil::format("sg.determ.after=%.17g", g->measure_of_badness()));
    auto gr = std::make_shared<StructuredGrid>(*patch, 6, 5, cf, rg, sgr, "detr");
    dump_grid("sg.determ.grid", gr);
}

// ------------------------------------------------------------------
// sg.rotq.* — rotateGrid (probe_sgrid.d:1236-1262, sgrid.d:1425-1465)
// ------------------------------------------------------------------
void grp_rotq() {
    auto g = std::make_shared<StructuredGrid>(2, 2, 2, "rotq");
    for (size_t k = 0; k < 2; ++k) {
        for (size_t j = 0; j < 2; ++j) {
            for (size_t i = 0; i < 2; ++i) {
                g->opIndex(i, j, k)->set(static_cast<double>(i), static_cast<double>(j),
                                         static_cast<double>(k));
            }
        }
    }
    auto a = g->dup();
    a->rotateGrid(1.0, 0.0, 0.0, 0.0);
    dump_verts("sg.rotq.ident", a->vertices);
    auto b = g->dup();
    b->rotateGrid(0.7071067811865476, 0.0, 0.7071067811865476, 0.0);
    dump_verts("sg.rotq.pi2.y", b->vertices);
    auto c = g->dup();
    c->rotateGrid(0.0, 0.0, 0.0, 1.0);
    dump_verts("sg.rotq.pi.y", c->vertices);
    auto d = g->dup();
    d->rotateGrid(0.5, 0.5, 0.5, 0.5);
    dump_verts("sg.rotq.diag", d->vertices);
    tryit("sg.rotq.norm", [&g]() { g->dup()->rotateGrid(1.0, 1.0, 0.0, 0.0); });
    tryit("sg.rotq.zero", [&g]() { g->dup()->rotateGrid(0.0, 0.0, 0.0, 0.0); });
    tryit("sg.rotq.nan", [&g]() {
        g->dup()->rotateGrid(std::numeric_limits<double>::quiet_NaN(), 0.0, 0.0, 0.0);
    });
    // norm exactly at the 1e-8 band edge.
    emit(eil::format("sg.rotq.band=%.17g", (std::abs(1.0 + 1e-9) - 1.0) > 1e-8));
    auto e = g->dup();
    e->rotateGrid(0.9999999999999999, 0.0, 0.0, 0.0);
    dump_verts("sg.rotq.justunder", e->vertices);
}

// ------------------------------------------------------------------
// sg.rot.* — the two inherited Grid.rotate entries (probe_sgrid.d:1264-1287,
// grid.d:443-500). The handle h is a Grid, so these replay the base implementations.
// ------------------------------------------------------------------
void grp_rot() {
    auto g = std::make_shared<StructuredGrid>(3, 3, 1, "rot");
    for (size_t j = 0; j < 3; ++j) {
        for (size_t i = 0; i < 3; ++i) {
            g->opIndex(i, j)->set(static_cast<double>(i), static_cast<double>(j), 0.0);
        }
    }
    std::shared_ptr<Grid> h = g;
    Quaternion q = {{1.0, 0.0, 0.0, 0.0}};
    h->rotate(q);
    dump_verts("sg.rot.q.ident", g->vertices);
    Quaternion q180 = {{0.0, 0.0, 0.0, 1.0}};
    h->rotate(q180);
    dump_verts("sg.rot.q.pi", g->vertices);
    h->rotate(3.141592653589793, Vector3(0.0, 0.0, 1.0));
    dump_verts("sg.rot.a.pi", g->vertices);
    h->rotate(1.5707963267948966, Vector3(0.0, 0.0, 1.0), Vector3(1.0, 1.0, 0.0));
    dump_verts("sg.rot.a.pi2 ctr", g->vertices);
    Vector3 ax = Vector3(0.0, 2.0, 0.0);
    h->rotate(0.5, ax);
    emit(eil::format("sg.rot.axis.after %.17g %.17g %.17g", eil::re(ax.x), eil::re(ax.y),
                     eil::re(ax.z)));
    dump_verts("sg.rot.a.len2", g->vertices);
    tryit("sg.rot.q.badnorm", [&h]() {
        Quaternion bad = {{1.0, 1.0, 0.0, 0.0}};
        h->rotate(bad);
    });
    // D's `Quaternion nq;` is double[4].init, i.e. four NaNs (nomenclature.d:68).
    Quaternion nq = eil::geom::quaternion_init();
    tryit("sg.rot.q.naninit", [&h, &nq]() { h->rotate(nq); });
    dump_verts("sg.rot.q.naninit.after", g->vertices);
}

// ------------------------------------------------------------------
// probe-side stand-in for D's `File(name, "r")`. The constructor itself raises when
// the open fails (T-923: a *failed* open is a StdioException/ErrnoException whose msg
// is `Cannot open file `NAME' in mode `MODE' (strerror)`, distinct from readln() at EOF
// which returns an empty string and raises nothing). This is the probe's own call, not
// an expected output, so reproducing it here keeps the gate honest.
std::FILE* d_file(const std::string& name, const char* mode) {
    std::FILE* h = std::fopen(name.c_str(), mode);
    if (h == nullptr) {
        throw eil::ExceptionBase("Cannot open file `" + name + "' in mode `" + mode + "' (" +
                                 std::strerror(errno) + ")");
    }
    return h;
}

// ------------------------------------------------------------------
// sg.uflowz.* — the underflow/overflow guard (probe_sgrid.d:1290-1306, D:1534-1540)
// ------------------------------------------------------------------
void grp_uflowz() {
    const double vals[] = {1.0, 1.0e-32, 1.0e-29, 1.0e+29, 1.0e+32,
                           -1.0, -1.0e-32, -1.0e-29, -1.0e+29, -1.0e+32,
                           0.0, -0.0, 1.0e-30, 1.0e30, 1.0e-31, 1.0e31,
                           std::numeric_limits<double>::max(),
                           std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::infinity(),
                           -std::numeric_limits<double>::infinity(),
                           9.999999999999999e+29};
    for (const double v : vals) {
        emit(eil::format("sg.uflowz %.17g -> %.17g", v, eil::geom::uflowz(v)));
    }
    emit(eil::format("sg.uflowz.custom %.17g %.17g -> %.17g", 1.0e-5, 1.0e-4,
                     eil::geom::uflowz(1.0e-5, 1.0e-4)));
    emit(eil::format("sg.uflowz.custom2 %.17g -> %.17g", 5.0,
                     eil::geom::uflowz(5.0, 1.0e-30, 4.0)));
    emit(eil::format("sg.uflowz.negcustom %.17g -> %.17g", -5.0,
                     eil::geom::uflowz(-5.0, 1.0e-30, 4.0)));
    // sign of the zero that comes out of the underflow branch.
    emit(eil::format("sg.uflowz.sign %.17g -> %.17g signbit=%d", -1.0e-32,
                     eil::geom::uflowz(-1.0e-32),
                     static_cast<int>(std::signbit(eil::geom::uflowz(-1.0e-32)))));
}

// ------------------------------------------------------------------
// sg.vtkhdr.* — read_VTK_header_line (probe_sgrid.d:1308-1339, D:1544-1561)
// ------------------------------------------------------------------
void grp_vtkhdr() {
    const std::string p = TD + "/hdr.vtk";
    mkr(p, "# vtk DataFile Version 2.0\nLabel Line\nascii\ndataset structured_grid\n"
           "DIMENSIONS 3 2 1\nPOINTS 6 float\n");
    {
        std::FILE* f = d_file(p, "r");
        emit(eil::format("sg.vtkhdr.0=%s", eil::geom::read_VTK_header_line("vtk", f)));
        emit(eil::format("sg.vtkhdr.1=%s", eil::geom::read_VTK_header_line("ASCII", f)));
        emit(eil::format("sg.vtkhdr.2=%s", eil::geom::read_VTK_header_line("STRUCTURED_GRID", f)));
        emit(eil::format("sg.vtkhdr.3=%s", eil::geom::read_VTK_header_line("DIMENSIONS", f)));
        emit(eil::format("sg.vtkhdr.4=%s", eil::geom::read_VTK_header_line("POINTS", f)));
        tryit("sg.vtkhdr.eof", [f]() { (void)eil::geom::read_VTK_header_line("CELLS", f); });
        std::fclose(f);
    }
    // A target that appears mid-word, and one that never appears.
    {
        std::FILE* f = d_file(p, "r");
        emit(eil::format("sg.vtkhdr.mid=%s", eil::geom::read_VTK_header_line("structured", f)));
        std::fclose(f);
    }
    {
        std::FILE* f = d_file(p, "r");
        tryit("sg.vtkhdr.absent", [f]() { (void)eil::geom::read_VTK_header_line("LOOKUP", f); });
        std::fclose(f);
    }
    tryit("sg.vtkhdr.missing", [&]() {
        std::FILE* f = d_file(TD + "/nope.vtk", "r");
        (void)eil::geom::read_VTK_header_line("vtk", f);
    });
}

// ------------------------------------------------------------------
// sg.gridpro.* — import_gridpro_grid (probe_sgrid.d:1341-1366, D:1565-1612).
// Two of these rows are printed from INSIDE the ported function (D's own writefln),
// which is why main() redirects stdout into the capture file.
// ------------------------------------------------------------------
void grp_gridpro() {
    const std::string p = TD + "/gp.dat";
    {
        std::string body = "# a comment line\n2 2 1\n";
        body += "0.0 0.0 0.0\n1000.0 0.0 0.0\n0.0 1000.0 0.0\n1000.0 1000.0 1.0e-40\n";
        body += "2 2 1\n";
        body += "1.0e+40 0.0 0.0\n2.0 3.0 4.0\n5.0 6.0 7.0\n8.0 9.0 1.0e-35\n";
        mkr(p, body);
    }
    auto gr = eil::geom::import_gridpro_grid(p);
    emit(eil::format("sg.gridpro.count=%d", gr.length()));
    for (size_t i = 0; i < gr.length(); ++i) {
        dump_grid(eil::format("sg.gridpro[%d]", i), gr[i]);
    }
    auto gr2 = eil::geom::import_gridpro_grid(p, 0.001);
    emit(eil::format("sg.gridpro.count2=%d", gr2.length()));
    for (size_t i = 0; i < gr2.length(); ++i) {
        dump_grid(eil::format("sg.gridpro.scale[%d]", i), gr2[i]);
    }
    tryit("sg.gridpro.missing", [&]() { (void)eil::geom::import_gridpro_grid(TD + "/nope.dat"); });
    // A single block with no trailing blank line.
    const std::string p1 = TD + "/gp1.dat";
    mkr(p1, "2 1 1\n1.0 2.0 3.0\n4.0 5.0 6.0\n");
    auto g1 = eil::geom::import_gridpro_grid(p1, 1.0);
    emit(eil::format("sg.gridpro.1=%d", g1.length()));
    dump_grid("sg.gridpro.1blk", g1[0]);
}

// ------------------------------------------------------------------
// sg.rsgp.* — rotate_gridpro_blocks (probe_sgrid.d:1368-1383, D:1756-1798)
// ------------------------------------------------------------------
void grp_rsgp() {
    auto g = mk2d(3, 2, "gp");
    for (size_t j = 0; j < 2; ++j) {
        for (size_t i = 0; i < 3; ++i) {
            g->opIndex(i, j)->set(static_cast<double>(i) * 1.0,
                                  static_cast<double>(j) * 10.0,
                                  static_cast<double>(i + j) * 0.25);
        }
    }
    auto r1 = eil::geom::rotate_gridpro_blocks(g, "East", "South");
    dump_grid("sg.rsgp.east.south", r1);
    emit(eil::format("sg.rsgp.east.south.aliased=%s", r1 == g));
    auto r2 = eil::geom::rotate_gridpro_blocks(g, "North", "East");
    dump_grid("sg.rsgp.north.east", r2);
    auto r3 = eil::geom::rotate_gridpro_blocks(g, "West", "North");
    dump_grid("sg.rsgp.west.north", r3);
    auto r4 = eil::geom::rotate_gridpro_blocks(g);
    emit(eil::format("sg.rsgp.default.aliased=%s", r4 == g));
    auto r5 = eil::geom::rotate_gridpro_blocks(g, "North", "South");
    emit(eil::format("sg.rsgp.other.aliased=%s", r5 == g));
    emit(eil::format("sg.rsgp.north.west=%s",
                    eil::geom::rotate_gridpro_blocks(g, "North", "West")->toString()));
}

// ------------------------------------------------------------------
// sg.fswap.* — grid_faceswap (probe_sgrid.d:1385-1398, D:1800-1828)
// ------------------------------------------------------------------
void grp_fswap() {
    auto g = mk2d(3, 2, "fs");
    for (size_t j = 0; j < 2; ++j) {
        for (size_t i = 0; i < 3; ++i) {
            g->opIndex(i, j)->set(static_cast<double>(i) * 1.0,
                                  static_cast<double>(j) * 10.0, 0.0);
        }
    }
    dump_grid("sg.fswap.ns", eil::geom::grid_faceswap(g, true, false));
    dump_grid("sg.fswap.ew", eil::geom::grid_faceswap(g, false, true));
    dump_grid("sg.fswap.both", eil::geom::grid_faceswap(g, true, true));
    dump_grid("sg.fswap.none", eil::geom::grid_faceswap(g, false, false));
    dump_grid("sg.fswap.default", eil::geom::grid_faceswap(g));
    // 3D source: the readers only touch the (i,j) lattice, so z survives oddly.
    auto g3 = vol_grid3d("fs3d");
    dump_grid("sg.fswap.3d", eil::geom::grid_faceswap(g3, true, false));
}

// ------------------------------------------------------------------
// sg.plot3d.* — writeGridsAsPlot3D / importPlot3DGrid
// (probe_sgrid.d:1400-1471, D:1614-1754). The gzipped read goes through
// popen("gunzip -c ...") exactly as D's pipeShell does (D:1623).
// ------------------------------------------------------------------
void grp_plot3d() {
    StructuredGridArray gs;
    gs.append(mk2d(3, 2, "p1"));
    for (size_t j = 0; j < 2; ++j) {
        for (size_t i = 0; i < 3; ++i) {
            gs[0]->opIndex(i, j)->set(static_cast<double>(i) * 1.25,
                                      static_cast<double>(j) * 2.5, 0.0);
        }
    }
    gs.append(std::make_shared<StructuredGrid>(2, 2, 2, "p2"));
    for (size_t k = 0; k < 2; ++k) {
        for (size_t j = 0; j < 2; ++j) {
            for (size_t i = 0; i < 2; ++i) {
                gs[1]->opIndex(i, j, k)->set(static_cast<double>(i) + 0.1,
                                             static_cast<double>(j) + 0.2,
                                             static_cast<double>(k) + 0.3);
            }
        }
    }
    const std::string f2 = TD + "/out2.dat";
    {
        StructuredGridArray one;
        one.append(gs[0]);
        eil::geom::writeGridsAsPlot3D(f2, one, 2);
    }
    emit(eil::format("sg.plot3d.2d.len=%d body=%s", len_of(f2), body_of(f2)));
    const std::string f3 = TD + "/out3.dat";
    eil::geom::writeGridsAsPlot3D(f3, gs, 3);
    emit(eil::format("sg.plot3d.3d.len=%d body=%s", len_of(f3), body_of(f3)));
    tryit("sg.plot3d.baddim.write", [&f3, &gs]() { eil::geom::writeGridsAsPlot3D(f3, gs, 4); });
    tryit("sg.plot3d.dim1.write", [&f3, &gs]() { eil::geom::writeGridsAsPlot3D(f3, gs, 1); });
    // Round trip through the reader.
    auto rb2 = eil::geom::importPlot3DGrid(f2, 2);
    emit(eil::format("sg.plot3d.read2.count=%d", rb2.length()));
    for (size_t i = 0; i < rb2.length(); ++i) {
        dump_grid(eil::format("sg.plot3d.read2[%d]", i), rb2[i]);
    }
    auto rb3 = eil::geom::importPlot3DGrid(f3, 3);
    emit(eil::format("sg.plot3d.read3.count=%d", rb3.length()));
    for (size_t i = 0; i < rb3.length(); ++i) {
        dump_grid(eil::format("sg.plot3d.read3[%d]", i), rb3[i]);
    }
    auto rb3as2 = eil::geom::importPlot3DGrid(f3, 2);
    emit(eil::format("sg.plot3d.read3as2.count=%d", rb3as2.length()));
    for (size_t i = 0; i < rb3as2.length(); ++i) {
        dump_grid(eil::format("sg.plot3d.read3as2[%d]", i), rb3as2[i]);
    }
    tryit("sg.plot3d.baddim.read", [&f2]() { (void)eil::geom::importPlot3DGrid(f2, 5); });
    // A single-block file (the size line has more than one token).
    const std::string s1 = TD + "/single.dat";
    {
        std::string body = "2 2 1\n";
        for (size_t n = 0; n < 4; ++n) body += eil::format(" %.18e\n", static_cast<double>(n) * 0.5);
        for (size_t n = 0; n < 4; ++n) body += eil::format(" %.18e\n", static_cast<double>(n) * 0.25);
        body += " 0.0 0.0 0.0 0.0\n";
        mkr(s1, body);
    }
    auto rs = eil::geom::importPlot3DGrid(s1, 2);
    emit(eil::format("sg.plot3d.single.count=%d", rs.length()));
    for (size_t i = 0; i < rs.length(); ++i) {
        dump_grid(eil::format("sg.plot3d.single[%d]", i), rs[i]);
    }
    // Values spread over several lines and in scientific format.
    const std::string s2 = TD + "/messy.dat";
    mkr(s2, " 2 2 \n1.0 2.0\n  3.0,4.0\n");
    mkr(s2, "2 2 1\n1.0 2.0 3.0 4.0\n1.0e+0 2.0e+0 3.0e+0\n4.0e+0\n");
    auto rm = eil::geom::importPlot3DGrid(s2, 2);
    dump_grid("sg.plot3d.messy", rm[0]);
    // The gzipped path, through pipeShell("gunzip -c ...") (D:1623).
    const std::string gzf = TD + "/out3.dat.gz";
    {
        eil::gzip::GzipOut o(gzf);
        bool ok = false;
        o.compress(read_whole(f3, ok));
        o.finish();
    }
    auto rg = eil::geom::importPlot3DGrid(gzf, 3, 1.0, true);
    emit(eil::format("sg.plot3d.gz.count=%d", rg.length()));
    for (size_t i = 0; i < rg.length(); ++i) {
        dump_grid(eil::format("sg.plot3d.gz[%d]", i), rg[i]);
    }
    auto rgsc = eil::geom::importPlot3DGrid(gzf, 3, 0.001, true);
    dump_grid("sg.plot3d.gz.scale", rgsc[0]);
    tryit("sg.plot3d.gz.missing", [&]() {
        (void)eil::geom::importPlot3DGrid(TD + "/nope.dat.gz", 3, 1.0, true);
    });
    tryit("sg.plot3d.missing", [&]() { (void)eil::geom::importPlot3DGrid(TD + "/nope.dat", 3); });
    std::remove(gzf.c_str());
}

// ------------------------------------------------------------------
// sg.ccp.* — compute_cell_properties on a real lattice
// (probe_sgrid.d:1473-1516, grid.d:124)
// ------------------------------------------------------------------
void grp_ccp() {
    std::shared_ptr<Grid> g3 = std::make_shared<StructuredGrid>(2, 2, 2, "ccp3d");
    auto sg = std::static_pointer_cast<StructuredGrid>(g3);
    for (size_t k = 0; k < 2; ++k) {
        for (size_t j = 0; j < 2; ++j) {
            for (size_t i = 0; i < 2; ++i) {
                sg->opIndex(i, j, k)->set(static_cast<double>(i), static_cast<double>(j),
                                          static_cast<double>(k));
            }
        }
    }
    Vector3 c;
    double vol = eil::d_init<double>();
    for (const bool tc : {true, false}) {
        g3->compute_cell_properties(0, tc, c, vol);
        emit(eil::format("sg.ccp.hex tc=%s %.17g %.17g %.17g %.17g", tc, eil::re(c.x),
                         eil::re(c.y), eil::re(c.z), eil::re(vol)));
    }
    // Degenerate: all 8 vertices coincident -> zero volume.
    auto deg = std::make_shared<StructuredGrid>(2, 2, 2, "ccpdeg");
    for (size_t k = 0; k < 2; ++k) {
        for (size_t j = 0; j < 2; ++j) {
            for (size_t i = 0; i < 2; ++i) {
                deg->opIndex(i, j, k)->set(1.0, 2.0, 3.0);
            }
        }
    }
    std::shared_ptr<Grid> dh = deg;
    dh->compute_cell_properties(0, true, c, vol);
    emit(eil::format("sg.ccp.degenerate %.17g %.17g %.17g %.17g", eil::re(c.x), eil::re(c.y),
                     eil::re(c.z), eil::re(vol)));
    // Inverted (mirrored) cell: negative volume.
    auto inv = std::make_shared<StructuredGrid>(2, 2, 2, "ccpinv");
    for (size_t k = 0; k < 2; ++k) {
        for (size_t j = 0; j < 2; ++j) {
            for (size_t i = 0; i < 2; ++i) {
                inv->opIndex(i, j, k)->set(static_cast<double>(i), static_cast<double>(j),
                                           -static_cast<double>(k));
            }
        }
    }
    std::shared_ptr<Grid> ih = inv;
    ih->compute_cell_properties(0, true, c, vol);
    emit(eil::format("sg.ccp.inverted %.17g %.17g %.17g %.17g", eil::re(c.x), eil::re(c.y),
                     eil::re(c.z), eil::re(vol)));
    ih->compute_cell_properties(0, false, c, vol);
    emit(eil::format("sg.ccp.inverted.false %.17g %.17g %.17g %.17g", eil::re(c.x),
                     eil::re(c.y), eil::re(c.z), eil::re(vol)));
    // 2D lattice cells.
    std::shared_ptr<Grid> g2 = surf_grid2d("ccp2d");
    for (size_t n = 0; n < g2->ncells; ++n) {
        g2->compute_cell_properties(n, true, c, vol);
        emit(eil::format("sg.ccp.2d n=%d %.17g %.17g %.17g %.17g", n, eil::re(c.x), eil::re(c.y),
                         eil::re(c.z), eil::re(vol)));
    }
    // 1D lattice: nvtx==2 falls to the default arm of the 2D/3D switch.
    std::shared_ptr<Grid> g1 = std::make_shared<StructuredGrid>(3, 1, 1, "ccp1d");
    tryit("sg.ccp.1d", [&g1, &c, &vol]() { g1->compute_cell_properties(0, true, c, vol); });
    tryit("sg.ccp.oob", [&g3, &c, &vol]() { g3->compute_cell_properties(1, true, c, vol); });
}

// ------------------------------------------------------------------
// sg.pic / sg.cbc / sg.bin / sg.fsc / sg.fncc — the search services
// (probe_sgrid.d:1518-1626, grid.d:198-420)
// ------------------------------------------------------------------
void grp_search() {
    auto g = std::make_shared<StructuredGrid>(3, 3, 2, "srch");
    for (size_t k = 0; k < 2; ++k) {
        for (size_t j = 0; j < 3; ++j) {
            for (size_t i = 0; i < 3; ++i) {
                g->opIndex(i, j, k)->set(static_cast<double>(i) * 1.0,
                                         static_cast<double>(j) * 2.0,
                                         static_cast<double>(k) * 3.0);
            }
        }
    }
    std::shared_ptr<Grid> h = g;
    const std::vector<Vector3> probes = {
        Vector3(0.5, 1.0, 1.5), Vector3(1.0, 1.0, 1.5), Vector3(1.5, 3.0, 1.5),
        Vector3(0.5, 1.0, 3.0), Vector3(0.0, 0.0, 0.0), Vector3(2.0, 4.0, 3.0),
        Vector3(2.5, 4.5, 3.5), Vector3(0.5, 1.0, 1.5)};
    // point_is_inside_cell at inside / face / edge / vertex / outside points.
    for (size_t i = 0; i < probes.size(); ++i) {
        for (size_t c = 0; c < h->ncells; ++c) {
            emit(eil::format("sg.pic p=%d c=%d inside=%s", i, c,
                             h->point_is_inside_cell(probes[i], c)));
        }
    }
    // cell_barycentre and find_nearest_cell_centre.
    for (size_t c = 0; c < h->ncells; ++c) {
        const Vector3 b = h->cell_barycentre(c);
        emit(eil::format("sg.cbc c=%d %.17g %.17g %.17g", c, eil::re(b.x), eil::re(b.y),
                         eil::re(b.z)));
    }
    // Binning.
    h->sort_cells_into_bins();
    emit(eil::format("sg.bin.default nbx=%d nby=%d nbz=%d dx=%.17g dy=%.17g dz=%.17g", h->nbx,
                     h->nby, h->nbz, h->deltax, h->deltay, h->deltaz));
    emit(eil::format("sg.bin.bb0 %.17g %.17g %.17g", eil::re(h->bb0.x), eil::re(h->bb0.y),
                     eil::re(h->bb0.z)));
    emit(eil::format("sg.bin.bb1 %.17g %.17g %.17g", eil::re(h->bb1.x), eil::re(h->bb1.y),
                     eil::re(h->bb1.z)));
    for (size_t c = 0; c < h->ncells; ++c) {
        const Vector3 b = h->cell_barycentre(c);
        emit(eil::format("sg.bin.ix c=%d b=%.17g %.17g %.17g ix=%d iy=%d iz=%d", c, eil::re(b.x),
                         eil::re(b.y), eil::re(b.z), h->get_bin_ix(b), h->get_bin_iy(b),
                         h->get_bin_iz(b)));
    }
    const auto dump_bins = [&h](const std::string& tag) {
        for (size_t ix = 0; ix < h->nbx; ++ix) {
            for (size_t iy = 0; iy < h->nby; ++iy) {
                for (size_t iz = 0; iz < h->nbz; ++iz) {
                    emit(eil::format("%s bin[%d][%d][%d]=%s", tag, ix, iy, iz,
                                     h->bins[ix][iy][iz]));
                }
            }
        }
    };
    dump_bins("sg.bin.contents");
    // Points exactly on a bin boundary and one ULP either side of it.
    std::vector<double> bx;
    for (size_t ix = 1; ix < h->nbx; ++ix) {
        bx.push_back(eil::re(h->bb0.x) + static_cast<double>(ix) * h->deltax);
    }
    for (const double v : bx) {
        for (const char* off : {"exact", "nextup", "nextdown"}) {
            const double x = (std::strcmp(off, "nextup") == 0)
                                 ? std::nextafter(v, std::numeric_limits<double>::infinity())
                                 : ((std::strcmp(off, "nextdown") == 0)
                                        ? std::nextafter(v, -std::numeric_limits<double>::infinity())
                                        : v);
            const Vector3 p = Vector3(x, 1.0, 1.5);
            emit(eil::format("sg.bin.edge v=%.17g off=%s ix=%d iy=%d iz=%d", x, off,
                             h->get_bin_ix(p), h->get_bin_iy(p), h->get_bin_iz(p)));
        }
    }
    // y and z boundaries too.
    const Vector3 py = Vector3(0.5, eil::re(h->bb0.y) + h->deltay, 1.5);
    emit(eil::format("sg.bin.edgey iy=%d", h->get_bin_iy(py)));
    const Vector3 pyn = Vector3(0.5, std::nextafter(eil::re(h->bb0.y) + h->deltay,
                                                    -std::numeric_limits<double>::infinity()),
                                1.5);
    emit(eil::format("sg.bin.edgey.down iy=%d", h->get_bin_iy(pyn)));
    const Vector3 pz = Vector3(0.5, 1.0, eil::re(h->bb0.z) + h->deltaz);
    emit(eil::format("sg.bin.edgez iz=%d", h->get_bin_iz(pz)));
    const Vector3 pzn = Vector3(0.5, 1.0, std::nextafter(eil::re(h->bb0.z) + h->deltaz,
                                                         -std::numeric_limits<double>::infinity()));
    emit(eil::format("sg.bin.edgez.down iz=%d", h->get_bin_iz(pzn)));
    // far outside / left of the box (the clamp rows).
    const Vector3 far = Vector3(-1.0, -1.0, -1.0);
    emit(eil::format("sg.bin.far ix=%d iy=%d iz=%d", h->get_bin_ix(far), h->get_bin_iy(far),
                     h->get_bin_iz(far)));
    const Vector3 over = Vector3(1.0e6, 1.0e6, 1.0e6);
    emit(eil::format("sg.bin.over ix=%d iy=%d iz=%d", h->get_bin_ix(over), h->get_bin_iy(over),
                     h->get_bin_iz(over)));
    const Vector3 nanp = Vector3(std::numeric_limits<double>::quiet_NaN(), 1.0, 1.5);
    emit(eil::format("sg.bin.nan ix=%d iy=%d iz=%d", h->get_bin_ix(nanp), h->get_bin_iy(nanp),
                     h->get_bin_iz(nanp)));
    // fast / slow / dispatcher search at a set of points
    size_t idx = 0;
    bool found = false;
    for (const Vector3& p : probes) {
        h->find_enclosing_cell_fast(p, idx, found);
        emit(eil::format("sg.fsc.fast p=%.17g %.17g %.17g idx=%d found=%s", eil::re(p.x),
                         eil::re(p.y), eil::re(p.z), idx, found));
        h->find_enclosing_cell_slow(p, idx, found);
        emit(eil::format("sg.fsc.slow p=%.17g %.17g %.17g idx=%d found=%s", eil::re(p.x),
                         eil::re(p.y), eil::re(p.z), idx, found));
        h->find_enclosing_cell(p, idx, found);
        emit(eil::format("sg.fsc.dis p=%.17g %.17g %.17g idx=%d found=%s", eil::re(p.x),
                         eil::re(p.y), eil::re(p.z), idx, found));
    }
    // On-boundary cells searched at a shared face of two cells.
    const Vector3 onface = Vector3(1.0, 1.0, 1.5);
    h->find_enclosing_cell_fast(onface, idx, found);
    emit(eil::format("sg.fsc.shared fast idx=%d found=%s", idx, found));
    h->find_enclosing_cell_slow(onface, idx, found);
    emit(eil::format("sg.fsc.shared slow idx=%d found=%s", idx, found));
    // A 1x1x1 bin grid (the degenerate delta floor of grid.d:301-304).
    std::shared_ptr<Grid> s1 = std::make_shared<StructuredGrid>(2, 2, 1, "singlebin");
    s1->sort_cells_into_bins(1, 1, 1);
    emit(eil::format("sg.bin.one nbx=%d dx=%.17g contents=%s", s1->nbx, s1->deltax,
                     s1->bins[0][0][0]));
    tryit("sg.bin.zero", [&s1]() { s1->sort_cells_into_bins(0, 1, 1); });
    // A second sort appends into the same bins (grid.d:306-310 keeps contents).
    std::shared_ptr<Grid> app = std::make_shared<StructuredGrid>(2, 2, 1, "append");
    app->sort_cells_into_bins(2, 2, 1);
    emit(eil::format("sg.bin.twice a=%s b=%s", app->bins[0][0][0], app->bins[1][1][0]));
    app->sort_cells_into_bins(2, 2, 1);
    emit(eil::format("sg.bin.thrice a=%s b=%s", app->bins[0][0][0], app->bins[1][1][0]));
    // 2D: nbinz is forced to 1 (grid.d:280).
    std::shared_ptr<Grid> g2d = surf_grid2d("bin2d");
    g2d->sort_cells_into_bins(3, 2, 7);
    emit(eil::format("sg.bin.2d nbz=%d nby=%d contents0=%s", g2d->nbz, g2d->nby,
                     g2d->bins[0][0][0]));
    // nearest cell centre
    double dist = 0.0;
    for (const Vector3& p : probes) {
        h->find_nearest_cell_centre(p, idx, dist);
        emit(eil::format("sg.fncc p=%.17g %.17g %.17g idx=%d d=%.17g", eil::re(p.x),
                         eil::re(p.y), eil::re(p.z), idx, dist));
    }
}

// ------------------------------------------------------------------
// sg.unit.* — sgrid.d's own unittest (probe_sgrid.d:1628-1656, D:1833-1865).
// The unittest's 15 asserts are put in as executable assertions, in source order with
// their D line numbers, exactly as the probe header asks (probe:115-118); the corpus
// shows every one of them holding.
// ------------------------------------------------------------------
void grp_unit() {
    const Vector3 p00(0.0, 0.1), p10(1.0, 0.1), p11(1.0, 1.1), p01(0.0, 1.1);
    auto my_patch = std::make_shared<CoonsPatch>(p00, p10, p11, p01);
    ClusterF cf;
    for (int n = 0; n < 4; ++n) cf.append(std::make_shared<LinearFunction>());
    auto my_grid = std::make_shared<StructuredGrid>(*my_patch, 11, 21, cf);
    my_grid->set_tag(eil::geom::Face::north, "north-face");
    my_grid->set_tags(tags({{"south", "south-face-here-sir"},
                            {"west", "west-face-here-sir!"}}));
    const std::string repr = my_grid->toString();
    emit("my_grid=" + repr);
    emit(eil::format("sg.unit.canFind.north=%s", repr.find("north-face") != std::string::npos));
    emit(eil::format("sg.unit.canFind.south=%s",
                     repr.find("south-face-here-sir") != std::string::npos));
    emit(eil::format("sg.unit.canFind.west=%s",
                     repr.find("west-face-here-sir!") != std::string::npos));
    // D:1846-1847 assert(repr.canFind(...)) x3, D:1848 the vector comparison
    eil_assert(repr.find("north-face") != std::string::npos); // D:1846;
    eil_assert(repr.find("south-face-here-sir") != std::string::npos); // D:1847;
    eil_assert(repr.find("west-face-here-sir!") != std::string::npos); // D:1848;
    emit(eil::format("sg.unit.v55=%.17g %.17g %.17g", eil::re(my_grid->opIndex(5, 5)->x),
                     eil::re(my_grid->opIndex(5, 5)->y), eil::re(my_grid->opIndex(5, 5)->z)));
    emit(eil::format("sg.unit.v55.approx=%s",
                     eil::geom::approxEqualVectors(*my_grid->opIndex(5, 5),
                                                   Vector3(0.5, 0.35, 0.0))));
    eil_assert(eil::geom::approxEqualVectors(*my_grid->opIndex(5, 5),
                                            Vector3(0.5, 0.35, 0.0))); // D:1851
    auto my_subgrid = my_grid->subgrid(4, 3, 4, 5);
    emit(eil::format("sg.unit.sub=%.17g %.17g %.17g", eil::re(my_subgrid->opIndex(1, 1)->x),
                     eil::re(my_subgrid->opIndex(1, 1)->y),
                     eil::re(my_subgrid->opIndex(1, 1)->z)));
    emit(eil::format("sg.unit.sub.approx=%s",
                     eil::geom::approxEqualVectors(*my_subgrid->opIndex(1, 1),
                                                   Vector3(0.5, 0.35, 0.0))));
    eil_assert(eil::geom::approxEqualVectors(*my_subgrid->opIndex(1, 1),
                                            Vector3(0.5, 0.35, 0.0))); // D:1855
    // D:1857-1864, the ten uflowz asserts of the unittest
    eil_assert(eil::geom::uflowz(1.0) == 1.0); // D:1857;
    eil_assert(eil::geom::uflowz(1.0e-32) == 0.0); // D:1858;
    eil_assert(eil::geom::uflowz(1.0e-29) == 1.0e-29); // D:1859;
    eil_assert(eil::geom::uflowz(1.0e+29) == 1.0e+29); // D:1860;
    eil_assert(eil::geom::uflowz(1.0e+32) == 1.0e+30); // D:1861;
    eil_assert(eil::geom::uflowz(-1.0) == -1.0); // D:1863;
    eil_assert(eil::geom::uflowz(-1.0e-32) == 0.0); // D:1864;
    eil_assert(eil::geom::uflowz(-1.0e-29) == -1.0e-29); // D:1865;
    eil_assert(eil::geom::uflowz(-1.0e+29) == -1.0e+29); // D:1866;
    eil_assert(eil::geom::uflowz(-1.0e+32) == -1.0e+30); // D:1867;
    emit(eil::format("sg.unit.niv=%d njv=%d nkv=%d ncells=%d label=%s", my_grid->niv,
                     my_grid->njv, my_grid->nkv, my_grid->ncells, my_grid->label));
}

//<<<SECTIONS-NOW>>>

// ------------------------------------------------------------------
// sg.vol.* — the ParametricVolume constructor (probe_sgrid.d:727-747, sgrid.d:115-136)
// ------------------------------------------------------------------
void grp_vol() {
    auto vol = std::make_shared<ProbeVolume>(Vector3(0.1, 0.2, 0.3), Vector3(1.0, 0.0, 0.0),
                                             Vector3(0.0, 1.0, 0.0), Vector3(0.0, 0.0, 1.0));
    ClusterF cf;  // empty, so D:123 pads it to 12 LinearFunctions
    auto gs = std::make_shared<StructuredGrid>(*vol, 3, 3, 2, cf, "simple", "volS");
    dump_grid("sg.vol.simple", gs);
    auto gt = std::make_shared<StructuredGrid>(*vol, 3, 3, 2, cf, "trilinear", "volT");
    dump_grid("sg.vol.trilinear", gt);
    auto gl = std::make_shared<StructuredGrid>(*vol, 3, 3, 2, cf, "linear", "volL");
    dump_grid("sg.vol.linear", gl);
    auto gtfi = std::make_shared<StructuredGrid>(*vol, 3, 3, 2, cf, "tfi", "volTFI");
    dump_grid("sg.vol.tfi", gtfi);
    ClusterF cf2;
    cf2.append(std::make_shared<LinearFunction>(0.0, 1.0));
    cf2.append(std::make_shared<eil::geom::QuadraticFunction>(0.4, false));
    auto gc = std::make_shared<StructuredGrid>(*vol, 3, 4, 2, cf2, "simple", "volC");
    dump_grid("sg.vol.cluster", gc);
    tryit("sg.vol.unknown", [&vol, &cf]() {
        (void)std::make_shared<StructuredGrid>(*vol, 2, 2, 2, cf, "quad", "x");
    });
    tryit("sg.vol.n1", [&vol, &cf]() {
        (void)std::make_shared<StructuredGrid>(*vol, 1, 1, 1, cf, "simple", "x");
    });
    emit(eil::format("sg.vol.dims=%d nkv=%d", gs->dimensions, gs->nkv));
}

// ------------------------------------------------------------------
// sg.readtxt.* — read_from_text_file (probe_sgrid.d:749-809, sgrid.d:783-852)
// and the format dispatch of the import constructor (sgrid.d:138-160)
// ------------------------------------------------------------------
void grp_readtxt() {
    const std::string plain = TD + "/plain.txt";
    {
        std::string body = "4 3 2\n";
        for (size_t k = 0; k < 2; ++k) {
            for (size_t j = 0; j < 3; ++j) {
                for (size_t i = 0; i < 4; ++i) {
                    body += eil::format("%.17g %.17g %.17g\n", static_cast<double>(i) + 0.5,
                                        static_cast<double>(j) * 2.0,
                                        static_cast<double>(k) * 4.0);
                }
            }
        }
        mkr(plain, body);
    }
    {
        auto g = std::make_shared<StructuredGrid>(plain, "text", "plain");
        dump_grid("sg.readtxt.plain", g);
    }
    const std::string vtk = TD + "/head.vtk";
    {
        std::string body = "# vtk DataFile Version 2.0\nmy label here\nASCII\n"
                           "DATASET STRUCTURED_GRID\nDIMENSIONS 5 4 1\nPOINTS 20 float\n";
        for (size_t j = 0; j < 4; ++j) {
            for (size_t i = 0; i < 5; ++i) {
                body += eil::format("%.17g %.17g %.17g\n", static_cast<double>(i) * 0.25,
                                    static_cast<double>(j) * 0.5, 0.0);
            }
        }
        mkr(vtk, body);
        auto g = std::make_shared<StructuredGrid>(vtk, "vtk", "vtklabel");
        dump_grid("sg.readtxt.vtk", g);
        emit(eil::format("sg.readtxt.vtk.label=%s", g->label));
        auto gh = std::make_shared<StructuredGrid>(5, 4, 1, "nohdr");
        gh->read_from_text_file(vtk, true);
        dump_grid("sg.readtxt.vtk.over", gh);
    }
    // dimension inference (D:805-819): 1D, degenerate, and nkv>1 with njv==1.
    for (const std::array<std::string, 3> spec :
         {std::array<std::string, 3>{"3", "1", "1"}, std::array<std::string, 3>{"1", "1", "1"},
          std::array<std::string, 3>{"1", "4", "2"}, std::array<std::string, 3>{"4", "1", "2"}}) {
        const std::string p = TD + "/dim_" + spec[0] + "_" + spec[1] + "_" + spec[2] + ".txt";
        const std::string body = spec[0] + " " + spec[1] + " " + spec[2] + "\n";
        const size_t nv = static_cast<size_t>(eil::to_int(spec[0])) *
                          static_cast<size_t>(eil::to_int(spec[1])) *
                          static_cast<size_t>(eil::to_int(spec[2]));
        std::string full = body;
        for (size_t n = 0; n < nv; ++n) {
            full += eil::format("%d %d %d\n", n, n + 1, n + 2);
        }
        mkr(p, full);
        tryit(eil::format("sg.readtxt.dim %s %s %s", spec[0], spec[1], spec[2]), [&p]() {
            auto g = std::make_shared<StructuredGrid>(p, "text", "d");
            emit(eil::format("sg.readtxt.dim.done dims=%d ncells=%d nvertices=%d", g->dimensions,
                             g->ncells, g->nvertices));
        });
    }
    // Bad tokens: two tokens only, and a non-numeric token.
    const std::string bad1 = TD + "/bad1.txt";
    mkr(bad1, "2 1 1\n1.0 2.0\n3.0 4.0\n");
    tryit_cut("sg.readtxt.bad.tokens", [&bad1]() {
        (void)std::make_shared<StructuredGrid>(bad1, "text", "b");
    });
    const std::string bad2 = TD + "/bad2.txt";
    mkr(bad2, "2 1 1\n1.0 xx 3.0\n3.0 4.0 5.0\n");
    tryit_cut("sg.readtxt.bad.conv", [&bad2]() {
        (void)std::make_shared<StructuredGrid>(bad2, "text", "b");
    });
    // Missing file: the measured Phobos open-failure text.
    tryit("sg.readtxt.missing", [&]() {
        (void)std::make_shared<StructuredGrid>(TD + "/nope.txt", "text", "m");
    });
    // Unknown format and the not-implemented VTK XML arm (D:142-159).
    tryit("sg.file.vtkxml", [&vtk]() { (void)std::make_shared<StructuredGrid>(vtk, "vtkxml", "x"); });
    tryit("sg.file.unknown", [&vtk]() { (void)std::make_shared<StructuredGrid>(vtk, "su2", "x"); });
    tryit("sg.file.empty", [&vtk]() { (void)std::make_shared<StructuredGrid>(vtk, "", "x"); });
    // A file that ends early (D's readln past EOF gives null).
    const std::string short_file = TD + "/short.txt";
    mkr(short_file, "3 3 1\n1.0 1.0 1.0\n");
    tryit("sg.readtxt.eof", [&short_file]() {
        (void)std::make_shared<StructuredGrid>(short_file, "text", "s");
    });
}

// ------------------------------------------------------------------
// sg.wgz.* / sg.readgz.* — the gzip round trip (probe_sgrid.d:812-894,
// read D:854-922, write D:999-1040)
// ------------------------------------------------------------------
void grp_wgz() {
    auto g = surf_grid2d("gzlabel");
    g->set_tag(eil::geom::Face::west, "west-tag");
    g->set_tag(eil::geom::Face::top, "top-tag");
    const std::string gz = TD + "/grid.gz";
    g->write_to_gzip_file(gz);
    emit(eil::format("sg.wgz.len=%d", len_of(gz)));
    emit(eil::format("sg.wgz.body=%s", gunzip_body_of(gz)));
    {
        auto r = std::make_shared<StructuredGrid>(gz, "gziptext", "ignored");
        dump_grid("sg.readgz.roundtrip", r);
        emit(eil::format("sg.readgz.repr=%s", r->toString()));
    }
    // scale on read (D:854 default 1.0; the reader multiplies each coordinate).
    {
        auto r = std::make_shared<StructuredGrid>(1, 1, 1, "s");
        r->read_from_gzip_file(gz, 0.001);
        dump_grid("sg.readgz.scale", r);
    }
    // A gzip file with an unsupported version line.
    const std::string gzbad = TD + "/badver.gz";
    {
        eil::gzip::GzipOut o(gzbad);
        o.compress("structured_grid 2.0\nlabel: x\ndimensions: 2\nniv: 2\nnjv: 2\nnkv: 1\n");
        o.compress(" 0 0 0\n 1 0 0\n 0 1 0\n 1 1 0\n");
        o.finish();
    }
    tryit("sg.readgz.badver", [&gzbad]() {
        (void)std::make_shared<StructuredGrid>(gzbad, "gziptext", "b");
    });
    tryit("sg.readgz.badver.direct", [&gzbad]() {
        auto r = std::make_shared<StructuredGrid>(2, 2, 1, "b");
        r->read_from_gzip_file(gzbad);
    });
    // A gzip file with no tag section at all (version 1.0).
    const std::string gz10 = TD + "/v10.gz";
    {
        eil::gzip::GzipOut o(gz10);
        o.compress("structured_grid 1.0\nlabel: ten\ndimensions: 2\nniv: 2\nnjv: 2\nnkv: 1\n");
        o.compress(" 0 0 0\n 0.5 0 0\n 0 0.25 0\n 0.5 0.25 0\n");
        o.finish();
    }
    {
        auto r = std::make_shared<StructuredGrid>(gz10, "gziptext", "ten");
        dump_grid("sg.readgz.v10", r);
    }
    tryit("sg.readgz.missing", [&]() {
        auto r = std::make_shared<StructuredGrid>(2, 2, 1, "m");
        r->read_from_gzip_file(TD + "/nope.gz");
    });
    // Truncated file: fewer lines than the header promises.
    const std::string gztr = TD + "/trunc.gz";
    {
        eil::gzip::GzipOut o(gztr);
        o.compress("structured_grid 1.1\nlabel: t\ndimensions: 2\nniv: 3\nnjv: 3\nnkv: 1\n");
        o.compress(" 0 0 0\n 1 0 0\n");
        o.compress("ntags: 4\ntag[0]: a\ntag[1]: b\ntag[2]: c\ntag[3]: d\n");
        o.finish();
    }
    tryit("sg.readgz.trunc", [&gztr]() {
        auto r = std::make_shared<StructuredGrid>(1, 1, 1, "t");
        r->read_from_gzip_file(gztr);
    });
    // A grid with an empty bcTags list writes ntags: 0 (D:1029).
    {
        auto e = std::make_shared<StructuredGrid>(2, 2, 1, "notags");
        e->bcTags = eil::Array<std::string>();
        const std::string gz2 = TD + "/notags.gz";
        e->write_to_gzip_file(gz2);
        emit(eil::format("sg.wgz.notags=%s", gunzip_body_of(gz2)));
        auto r = std::make_shared<StructuredGrid>(1, 1, 1, "x");
        r->read_from_gzip_file(gz2);
        emit(eil::format("sg.readgz.notags bcTags=%s ntagsread=true", r->bcTags));
    }
    // The dispatch of Grid.write (grid.d:104-111) with fmt "gziptext".
    {
        std::shared_ptr<Grid> h = surf_grid2d("via.write");
        const std::string gz3 = TD + "/via.write.gz";
        h->write(gz3, "gziptext");
        emit(eil::format("sg.wgz.viafmt=%s", gunzip_body_of(gz3)));
        h->write(gz3, "unknown-format");
        emit(eil::format("sg.wgz.viaunknown=%s", gunzip_body_of(gz3)));
    }
}

// ------------------------------------------------------------------
// sg.frmt.* — the formattedRead patterns of read_from_gzip_file
// (probe_sgrid.d:896-955, D:861-914). One crafted .gz per shape: literal
// mismatch, %s on an empty remainder, %d on non-digits, %d out of range for the
// target type, and %g with too few numbers on the vertex line.
// ------------------------------------------------------------------
void grp_frmt() {
    const auto probe_case = [](const std::string& name,
                               const std::vector<std::string>& lines) {
        std::string body;
        for (const std::string& l : lines) body += l + "\n";
        const std::string path = TD + "/frmt_" + name + ".gz";
        {
            eil::gzip::GzipOut o(path);
            o.compress(body);
            o.finish();
        }
        tryit("sg.frmt." + name, [&path, &name]() {
            auto r = std::make_shared<StructuredGrid>(1, 1, 1, "x");
            r->read_from_gzip_file(path);
            emit(eil::format("sg.frmt.%s.after dims=%d niv=%d njv=%d nkv=%d label=%s bcTags=%s",
                             name, r->dimensions, r->niv, r->njv, r->nkv, r->label, r->bcTags));
        });
    };
    probe_case("badfirst", {"not_a_grid 1.1", "label: f", "dimensions: 2", "niv: 2", "njv: 2",
                            "nkv: 1", " 0 0 0", " 1 0 0", " 0 1 0", " 1 1 0"});
    probe_case("emptyver", {"structured_grid", "label: f", "dimensions: 2", "niv: 2", "njv: 2",
                            "nkv: 1", " 0 0 0", " 1 0 0", " 0 1 0", " 1 1 0"});
    probe_case("twospace", {"structured_grid  1.1", "label: f", "dimensions: 2", "niv: 2",
                            "njv: 2", "nkv: 1", " 0 0 0", " 1 0 0", " 0 1 0", " 1 1 0"});
    probe_case("nolabel", {"structured_grid 1.1", "dimensions: 2", "niv: 2", "njv: 2", "nkv: 1",
                           " 0 0 0", " 1 0 0", " 0 1 0", " 1 1 0"});
    probe_case("dimsword", {"structured_grid 1.1", "label: f", "dimensions: two", "niv: 2",
                            "njv: 2", "nkv: 1", " 0 0 0", " 1 0 0", " 0 1 0", " 1 1 0"});
    probe_case("dimsfloat", {"structured_grid 1.1", "label: f", "dimensions: 2.5", "niv: 2",
                             "njv: 2", "nkv: 1", " 0 0 0", " 1 0 0", " 0 1 0", " 1 1 0"});
    probe_case("dimsneg", {"structured_grid 1.1", "label: f", "dimensions: -2", "niv: 2",
                           "njv: 2", "nkv: 1", " 0 0 0", " 1 0 0", " 0 1 0", " 1 1 0"});
    probe_case("nivbig", {"structured_grid 1.1", "label: f", "dimensions: 2",
                          "niv: 99999999999999", "njv: 2", "nkv: 1", " 0 0 0", " 1 0 0",
                          " 0 1 0", " 1 1 0"});
    probe_case("shortvtx", {"structured_grid 1.1", "label: f", "dimensions: 2", "niv: 2",
                            "njv: 2", "nkv: 1", " 0 0 0", " 1 0"});
    probe_case("wordvtx", {"structured_grid 1.1", "label: f", "dimensions: 2", "niv: 2",
                           "njv: 2", "nkv: 1", " 0 0 0", " a b c", " 0 1 0", " 1 1 0"});
    probe_case("nospaces", {"structured_grid 1.1", "label: f", "dimensions: 2", "niv: 2",
                            "njv: 2", "nkv: 1", "0 0 01 0 00 1 01 1 0"});
    probe_case("extraspace", {"structured_grid 1.1", "label: f", "dimensions: 2", "niv: 2",
                               "njv: 2", "nkv: 1", "   0   0   0  ", " 1 0 0", " 0 1 0",
                               " 1 1 0"});
    probe_case("extrainline", {"structured_grid 1.1", "label: f", "dimensions: 2", "niv: 2",
                               "njv: 2", "nkv: 1", " 0 0 0", " 1 0 0", " 0 1 0", " 1 1 0",
                               "ntags: 2", "tag[0]: A", "tag[1]: B", "extra line"});
    probe_case("notagsline", {"structured_grid 1.1", "label: f", "dimensions: 2", "niv: 2",
                              "njv: 2", "nkv: 1", " 0 0 0", " 1 0 0", " 0 1 0", " 1 1 0",
                              "ntags: 3", "tag[0]: A"});
    probe_case("tagcountbad", {"structured_grid 1.1", "label: f", "dimensions: 2", "niv: 2",
                               "njv: 2", "nkv: 1", " 0 0 0", " 1 0 0", " 0 1 0", " 1 1 0",
                               "tags: 4"});
    probe_case("emptylabel", {"structured_grid 1.1", "label:", "dimensions: 2", "niv: 2",
                              "njv: 2", "nkv: 1", " 0 0 0", " 1 0 0", " 0 1 0", " 1 1 0"});
    probe_case("version10x", {"structured_grid 1.01", "label: f", "dimensions: 2", "niv: 2",
                              "njv: 2", "nkv: 1", " 0 0 0", " 1 0 0", " 0 1 0", " 1 1 0"});
}

// ------------------------------------------------------------------
// sg.wraw.* / sg.readraw.* — raw binary (probe_sgrid.d:959-1021,
// write D:1042-1082, read D:924-997)
// ------------------------------------------------------------------
void grp_raw() {
    auto g = surf_grid2d("rawlabel");
    g->set_tag(eil::geom::Face::east, "E-tag");
    const std::string rb = TD + "/grid.raw";
    g->write_to_raw_binary_file(rb);
    emit(eil::format("sg.wraw.len=%d", len_of(rb)));
    emit(eil::format("sg.wraw.hex=%s", hex_of(rb)));
    {
        auto r = std::make_shared<StructuredGrid>(rb, "rawbinary", "ignored");
        dump_grid("sg.readraw.roundtrip", r);
    }
    {
        auto r = std::make_shared<StructuredGrid>(1, 1, 1, "s");
        r->read_from_raw_binary_file(rb, 2.0);
        dump_grid("sg.readraw.scale", r);
    }
    // Empty label (D:940 `if (label_length > 0)`).
    {
        auto e = std::make_shared<StructuredGrid>(2, 2, 1, "");
        const std::string rb2 = TD + "/nolog.raw";
        e->write_to_raw_binary_file(rb2);
        emit(eil::format("sg.wraw.nolabel.hex=%s", hex_of(rb2)));
        auto r = std::make_shared<StructuredGrid>(1, 1, 1, "x");
        r->read_from_raw_binary_file(rb2);
        emit(eil::format("sg.readraw.nolabel label=%s bcTags=%s", r->label, r->bcTags));
    }
    // Bad version in the header, and a missing file.
    const std::string badraw = TD + "/badver.raw";
    {
        std::FILE* fp = std::fopen(badraw.c_str(), "wb");
        const char hdr[] = "structured_grid 7.7";
        std::fwrite(hdr, 1, sizeof hdr - 1, fp);
        const int zero = 0;
        std::fwrite(&zero, 1, sizeof zero, fp);
        std::fclose(fp);
    }
    tryit("sg.readraw.badver", [&badraw]() {
        auto r = std::make_shared<StructuredGrid>(1, 1, 1, "b");
        r->read_from_raw_binary_file(badraw);
    });
    tryit("sg.readraw.missing", [&]() {
        auto r = std::make_shared<StructuredGrid>(1, 1, 1, "m");
        r->read_from_raw_binary_file(TD + "/nope.raw");
    });
    tryit("sg.file.rawviafmt", [&badraw]() {
        (void)std::make_shared<StructuredGrid>(badraw, "rawbinary", "b");
    });
    // Truncated: header only, then the readers run out of data.
    const std::string trh = TD + "/trunchead.raw";
    {
        std::FILE* fp = std::fopen(trh.c_str(), "wb");
        const char hdr[] = "structured_grid 1.1";
        std::fwrite(hdr, 1, sizeof hdr - 1, fp);
        std::fclose(fp);
    }
    tryit("sg.readraw.trunc", [&trh]() {
        auto r = std::make_shared<StructuredGrid>(1, 1, 1, "t");
        r->read_from_raw_binary_file(trh);
    });
    // Grid.write with "rawbinary" goes to the raw writer (grid.d:106).
    {
        std::shared_ptr<Grid> h = surf_grid2d("rw");
        const std::string rb3 = TD + "/via.write.raw";
        h->write(rb3, "rawbinary");
        emit(eil::format("sg.wraw.viafmt.hex=%s", hex_of(rb3)));
    }
}

// ------------------------------------------------------------------
// sg.vtk.* — write_to_vtk_file (probe_sgrid.d:1023-1036, D:1084-1102)
// sg.su2.* — write_to_su2_file (probe_sgrid.d:1038-1049, D:1312-1318), which
// delegates to UnstructuredGrid (grid brief section 10.1).
// ------------------------------------------------------------------
void grp_vtk_su2() {
    auto g = surf_grid2d("vtklabel");
    const std::string v = TD + "/grid.vtk";
    g->write_to_vtk_file(v);
    emit(eil::format("sg.vtk.len=%d body=%s", len_of(v), body_of(v)));
    std::shared_ptr<Grid> h = std::make_shared<StructuredGrid>(3, 2, 1, "vtk3d");
    for (size_t j = 0; j < 2; ++j) {
        for (size_t i = 0; i < 3; ++i) {
            h->opIndex(i, j, 0)->set(static_cast<double>(i) / 3.0, static_cast<double>(j),
                                     1.0e-40);
        }
    }
    const std::string v2 = TD + "/grid2.vtk";
    h->write_to_vtk_file(v2);
    emit(eil::format("sg.vtk.len2=%d body2=%s", len_of(v2), body_of(v2)));
    // ---- sg.su2.*
    auto gs = surf_grid2d("su2label");
    const std::string s = TD + "/grid.su2";
    gs->write_to_su2_file(s);
    emit(eil::format("sg.su2.len=%d body=%s", len_of(s), body_of(s)));
    gs->write_to_su2_file(s, 0.001);
    emit(eil::format("sg.su2.scaled.len=%d body=%s", len_of(s), body_of(s)));
    auto g3 = vol_grid3d("su23d");
    const std::string s3 = TD + "/grid3.su2";
    g3->write_to_su2_file(s3);
    emit(eil::format("sg.su2.3d.len=%d body=%s", len_of(s3), body_of(s3)));
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }
    ::mkdir(TD.c_str(), 0777); // the probe writes its artefacts under here
    const int saved = ::dup(1);
    std::fflush(stdout);
    // D's probe puts some of its rows on stdout by WRITELN from inside the ported code
    // (import_gridpro_grid, sgrid.d:1565-1612), so the capture has to take those lines in
    // stream order too — the same freopen shape as the usgrid harness, with this stage's
    // own capture path.
    if (std::freopen(kCapture, "w", stdout) == nullptr) {
        std::fprintf(stderr, "cannot open capture file %s\n", kCapture);
        return 2;
    }

    grp_ctor();
    grp_idx();
    grp_opidx();
    grp_cell();
    grp_abst();
    grp_str();
    grp_tag();
    grp_dup();
    grp_sub();
    grp_bnd();
    grp_path();
    grp_surf();
    grp_rgrid();
    grp_vol();
    grp_readtxt();
    grp_wgz();
    grp_frmt();
    grp_raw();
    grp_vtk_su2();
    grp_join();
    grp_slab_wedge();
    grp_bad();
    grp_rs();
    grp_determ();
    grp_rotq();
    grp_rot();
    grp_uflowz();
    grp_vtkhdr();
    grp_gridpro();
    grp_rsgp();
    grp_fswap();
    grp_plot3d();
    grp_ccp();
    grp_search();
    grp_unit();
    emit("sg.done");   // probe_sgrid.d:1627, the last corpus row (2097)
    //<<<GROUP-CALLS-NOW>>>

    std::fflush(stdout);
    ::dup2(saved, 1);
    ::close(saved);

    std::ifstream in(argv[1]);
    if (!in) {
        std::fprintf(stderr, "cannot open baseline %s\n", argv[1]);
        return 2;
    }
    std::vector<std::string> expected;
    for (std::string l; std::getline(in, l);) expected.push_back(l);
    std::ifstream got(kCapture);
    std::vector<std::string> actual;
    for (std::string l; std::getline(got, l);) actual.push_back(l);

    int diffs = 0, shown = 0;
    size_t matched = 0;
    for (size_t i = 0; i < expected.size() || i < actual.size(); ++i) {
        const bool have_e = i < expected.size();
        const bool have_g = i < actual.size();
        const std::string e = have_e ? expected[i] : "<none>";
        const std::string g = have_g ? actual[i] : "<none>";
        if (have_e && have_g && e == g) { ++matched; continue; }
        if (shown < 400) {
            std::printf("line %zu\n  D:   |%s|\n  C++: |%s|\n", i + 1, e.c_str(), g.c_str());
            ++shown;
        }
        ++diffs;
    }
    std::printf("%s: geom_grid_sgrid, probe replay %zu D lines, %zu C++ lines, %d differ; "
                "%zu of %zu rows byte-identical\n",
                diffs == 0 ? "PASS" : "FAIL", expected.size(), actual.size(), diffs, matched,
                expected.size());
    return diffs == 0 ? 0 : 1;
}
