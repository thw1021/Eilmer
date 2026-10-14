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
using eil::geom::StructuredGrid;
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

//<<<SECTIONS-NOW>>>

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
