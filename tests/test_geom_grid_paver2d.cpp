// tests/test_geom_grid_paver2d.cpp — C++ mirror of probes/geom/grid/probe_paver2d.d,
// replayed row for row against baselines/e587b235/probes/geom_grid_paver2d.txt
// (480 rows captured from ldc2 1.40.0 at gdtk e587b235; two consecutive runs of the
// probe are byte-identical, checked with cmp, so the capture is reproducible).
//
// geom/grid/paver2d.d declares NO `unittest` block — proved by
//   grep -c unittest /home/tang/packages/gdtk/src/geom/grid/paver2d.d  ->  0
// (and for the whole slice: grep -c unittest paver2d.d paver2d_demo.d paver_demo.d
// gives 0 0 0), so this replay IS the gate for the module, exactly as the probe is.
//
// Regenerate the baseline:
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/grid
//        probes/geom/grid/probe_paver2d.d -of=/tmp/probe_g3 -od/tmp/probe_obj_g3
//   /tmp/probe_g3 > baselines/e587b235/probes/geom_grid_paver2d.txt
//   (the ldc2 line is one command; it is wrapped here without a trailing backslash
//   because a backslash at the end of a // comment is what -Wcomment warns about)
//
// Row groups (mirroring the probe, one for one):
//   nt.*   NodeType (paver2d.d:12) as D prints it, including the out-of-range
//          fallback, which is measured rather than guessed: ldc2 gives
//          "cast(NodeType)7", "cast(NodeType)-1", "cast(NodeType)2147483647".
//   fi.*   fill_interior (paver2d.d:14-52) over the state its only caller builds
//          (usgrid.d:465-499, transcribed as load_from_bpoints below), with the two
//          writeln lines (paver2d.d:19, :47) captured from the real stdout stream in
//          stream order by StdoutCapture — so their position among the rows this test
//          emits is part of the replay, not an assumption.
//
// WHY THE FIXTURE IN eil::geom BELOW EXISTS. paver2d.d's fill_interior takes
// USGFace[]/USGCell[] and calls makeFaceTag, and all three live in
// geom/grid/usgrid.d (needed at paver2d.d:15, :17, :31, :40-42, :44; defined at
// usgrid.d:154, :218, :112) — stage G5, which is not ported yet. The brief's claim
// that G3's dependency is paver.d does not hold for this module:
//   grep -n "PavedGrid\|POINT_LIST\|FACE_LIST\|CELL_LIST" geom/grid/paver2d.d -> empty
// fill_interior is therefore a template over the two class-array containers (see the
// MEASURED DEPENDENCY note in src/geom/grid/paver2d.hpp), and this file supplies
// USGFace / USGCell / USGCell_type / makeFaceTag in namespace eil::geom, transcribed
// from the D lines named above — the same device tests/test_geom_grid_grid.cpp uses
// to make grid.d's abstract base observable through a concrete subclass. What is
// under test is paver2d.d's own arithmetic, text and control flow; the fixture only
// plays the part G5 will play, and it is deliberately NOT used to compute any
// expected value. When src/geom/grid/usgrid.hpp lands, delete the fixture, include
// the real header and re-run: every row here must still match.
//
// The Throwable rows map the port's exception type onto the D class name the probe
// printed, using the measured message shapes: eil::RangeViolation (msg "Range
// violation", substrate/include/eil/aa.hpp:36-39) is core.exception.RangeError, and
// eil::RangeError (msg "index [N] is out of bounds for array of length L",
// substrate/include/eil/error.hpp:72-89) is core.exception.ArrayIndexError. Both are
// Errors, so a ported catch (ExceptionBase) does not catch them — as in D. The
// corpus rows fi.n0 and fi.miss additionally prove the module's second writeln is
// NOT printed when the call dies, i.e. it sits after the loops, as in paver2d.d:47.
//
// Run: ./test_geom_grid_paver2d <baseline.txt>
#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "eil/aa.hpp"
#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/grid/paver2d.hpp"

//------------------------------------------------------------------------
// The parts of geom/grid/usgrid.d (stage G5) that paver2d.d names, in the
// namespace paver2d.hpp forward-declares them in. Transcribed from the D
// source; only the members fill_interior reaches are carried.
//------------------------------------------------------------------------
namespace eil {
namespace geom {

class USGCell; // usgrid.d:216-218 link fields in USGFace

// usgrid.d:40-56 `enum USGCell_type { none = 0, triangle = 1, quad = 2, ... }`
enum class USGCell_type : int {
    none = 0, triangle = 1, quad = 2, polygon = 3, tetra = 4, wedge = 5, hexahedron = 6,
    pyramid = 7, line = 8
};

// usgrid.d:154-216. Fields fill_interior and its caller can observe.
class USGFace {
  public:
    eil::Array<size_t> vtx_id_list; // usgrid.d:156
    std::string tag;                // usgrid.d:157 — never set by fill_interior
    USGCell* left_cell = nullptr;   // usgrid.d:163
    USGCell* right_cell = nullptr;  // usgrid.d:164
    bool is_on_boundary = false;    // usgrid.d:165

    // usgrid.d:168-171: `this(const size_t[] vtx_id_list) { this.vtx_id_list =
    // vtx_id_list.dup(); }`
    explicit USGFace(eil::Slice<size_t> ids) {
        for (size_t i = 0; i < ids.length(); ++i) vtx_id_list.append(ids[i]);
    }
};

// usgrid.d:218-326, the four fields fill_interior fills.
class USGCell {
  public:
    USGCell_type cell_type; // usgrid.d:220
    size_t id = 0;          // usgrid.d:221
    eil::Array<size_t> vtx_id_list;  // usgrid.d:222
    eil::Array<size_t> face_id_list; // usgrid.d:223
    eil::Array<int> outsign_list;    // usgrid.d:224

    // usgrid.d:225-233.
    USGCell(USGCell_type ct, size_t in_id, eil::Slice<size_t> vtx,
            eil::Slice<size_t> fcs, eil::Slice<int> outs)
        : cell_type(ct), id(in_id) {
        for (size_t i = 0; i < vtx.length(); ++i) vtx_id_list.append(vtx[i]);
        for (size_t i = 0; i < fcs.length(); ++i) face_id_list.append(fcs[i]);
        for (size_t i = 0; i < outs.length(); ++i) outsign_list.append(outs[i]);
    }
};

// usgrid.d:112-132 string makeFaceTag(const size_t[] vtx_id_list): sort an ascending
// copy, then join with "-" under `%d`.
std::string makeFaceTag(eil::Slice<size_t> vtx_id_list) {
    std::vector<size_t> my_id_list;
    for (size_t i = 0; i < vtx_id_list.length(); ++i) my_id_list.push_back(vtx_id_list[i]);
    std::sort(my_id_list.begin(), my_id_list.end());
    std::string tag;
    const size_t n = my_id_list.size();
    for (size_t i = 0; i < n; ++i) {
        if (i > 0) tag += "-";
        tag += eil::format("%d", my_id_list[i]);
    }
    return tag;
}

} // namespace geom
} // namespace eil

namespace {

using eil::geom::makeFaceTag;
using eil::geom::NodeType;
using eil::geom::USGCell;
using eil::geom::USGCell_type;
using eil::geom::USGFace;
using eil::geom::Vector3;

// D's `USGFace[]` / `USGCell[]` are class arrays, so the house container spelling is
// std::shared_ptr (src/geom/surface/bezierpatch.hpp:83). fill_interior also
// instantiates for the raw-pointer form, exercised by the control below.
using FaceArray = eil::Array<std::shared_ptr<USGFace>>;
using CellArray = eil::Array<std::shared_ptr<USGCell>>;

std::vector<std::string> g_lines;

void emit(const std::string& s) { g_lines.push_back(s); }

// paver2d.d writes through eil::writeln, i.e. straight to the C stdout stream, just
// as D's std.stdio.writeln does. To replay those lines in stream order, fd 1 is
// pointed at a temporary file for the duration of the guarded statement and whatever
// appeared is appended to g_lines here (the same device as
// tests/test_geom_surface_aopatch.cpp:183-215).
class StdoutCapture {
  public:
    explicit StdoutCapture(bool keep = true) : m_keep(keep) {
        std::strcpy(m_path, "/tmp/eil_g3_stdout_XXXXXX");
        int fd = ::mkstemp(m_path);
        if (fd < 0) { m_saved = -1; return; }
        ::close(fd);
        std::fflush(stdout);
        m_saved = ::dup(STDOUT_FILENO);
        if (m_saved < 0) return;
        if (std::freopen(m_path, "w", stdout) == nullptr) { ::close(m_saved); m_saved = -1; }
    }
    ~StdoutCapture() {
        if (m_saved < 0) return;
        std::fflush(stdout);
        ::dup2(m_saved, STDOUT_FILENO);
        ::close(m_saved);
        std::FILE* in = m_keep ? std::fopen(m_path, "r") : nullptr;
        if (!m_keep) in = nullptr; // the control's lines are dropped, not replayed
        if (in) {
            char buf[4096];
            while (std::fgets(buf, sizeof buf, in)) {
                std::string l(buf);
                while (!l.empty() && (l.back() == '\n' || l.back() == '\r')) l.pop_back();
                emit(l);
            }
            std::fclose(in);
        }
        std::remove(m_path);
    }

  private:
    char m_path[32] = {};
    int m_saved = -1;
    bool m_keep = true;
};

// A NaN with the sign bit set, built from its bit pattern rather than by negating
// quiet_NaN(), so the polarity this row depends on does not depend on how the
// compiler chose to emit the negation (reports/task-log.md T-917).
double neg_nan() {
    const uint64_t u = 0xfff8000000000000ull;
    double r;
    std::memcpy(&r, &u, sizeof r);
    return r;
}

/// The state usgrid.d:465-499 builds before it calls fill_interior: the vertex copy,
/// bndry_vtx_ids, and the boundary faces with their tags already in faceIndices.
/// The distance checks and the BoundaryFaceSet list are usgrid.d's own business and
/// are deliberately NOT reproduced.
void load_from_bpoints(eil::Array<Vector3>& vertices, FaceArray& faces,
                       eil::AA<std::string, size_t>& faceIndices,
                       eil::Array<size_t>& bndry_vtx_ids,
                       const std::vector<std::vector<Vector3>>& bPoints)
{
    for (size_t i = 0; i < bPoints.size(); ++i) {
        const std::vector<Vector3>& bpa = bPoints[i];
        if (i == 0) {
            vertices.append(Vector3(bpa[0]));
            bndry_vtx_ids.append(vertices.length() - 1);
        }
        for (size_t j = 1; j < bpa.size(); ++j) {
            vertices.append(Vector3(bpa[j]));
            bndry_vtx_ids.append(vertices.length() - 1);
            // Also, build a face object on the boundary.
            eil::Array<size_t> vtx_id_list = {vertices.length() - 2,
                                              vertices.length() - 1};
            std::string faceTag = makeFaceTag(vtx_id_list.all());
            faceIndices.set(faceTag, faces.length()); // before new face
            faces.append(std::make_shared<USGFace>(vtx_id_list.all()));
        }
    }
}

/// What the probe prints after a call: counts, every coordinate at %.17g, the face
/// vertex lists with their recomputed tags, the cell fields, and the associative
/// array's keys in sorted order (D's AA iteration order is randomized per process,
/// which is why the probe sorts before printing and why this mirror sorts too —
/// eil::AA is a std::map, so keys() already comes back lexicographic).
void dump_state(const std::string& name, const eil::Array<Vector3>& vertices,
                const FaceArray& faces, const eil::AA<std::string, size_t>& faceIndices,
                const CellArray& cells)
{
    emit(eil::format("case=%s END nvertices=%d nfaces=%d ncells=%d naa=%d", name,
                     vertices.length(), faces.length(), cells.length(),
                     faceIndices.length()));
    for (size_t i = 0; i < vertices.length(); ++i) {
        const Vector3& v = vertices[i];
        emit(eil::format("case=%s vtx=%d x=%.17g y=%.17g z=%.17g", name, i, eil::re(v.x),
                         eil::re(v.y), eil::re(v.z)));
    }
    for (size_t i = 0; i < faces.length(); ++i) {
        emit(eil::format("case=%s face=%d ids=%s tag=%s", name, i, faces[i]->vtx_id_list,
                         makeFaceTag(faces[i]->vtx_id_list.all())));
    }
    for (size_t i = 0; i < cells.length(); ++i) {
        const USGCell& c = *cells[i];
        emit(eil::format("case=%s cell=%d type=%d id=%d vtx=%s faces=%s outsign=%s", name,
                         i, static_cast<int>(c.cell_type), c.id, c.vtx_id_list,
                         c.face_id_list, c.outsign_list));
    }
    eil::Array<std::string> tags = faceIndices.keys();
    for (size_t i = 0; i < tags.length(); ++i) {
        emit(eil::format("case=%s aa=%s val=%d", name, tags[i],
                         faceIndices.get(tags[i])));
    }
}

/// One fill_interior call: the module's own stdout lines are captured in stream
/// order, and a Throwable is reported the way the probe reports it.
void call_fill_interior(const std::string& name, eil::Array<Vector3>& vertices,
                        FaceArray& faces, eil::AA<std::string, size_t>& faceIndices,
                        CellArray& cells, eil::Array<size_t>& bndry_vtx_ids)
{
    bool ok = false;
    std::string cls, msg;
    try {
        // StdoutCapture's destructor runs before the rows below are emitted, on the
        // normal path AND during unwinding, which is what puts the module's lines in
        // the same positions they hold in the D corpus.
        StdoutCapture cap;
        eil::geom::fill_interior(vertices, faces, faceIndices, cells, bndry_vtx_ids);
        ok = true;
    }
    catch (const eil::RangeViolation& e) {
        cls = "core.exception.RangeError"; // D's associative-array key miss
        msg = e.msg();
    }
    catch (const eil::RangeError& e) {
        cls = "core.exception.ArrayIndexError"; // D's array index bounds failure
        msg = e.msg();
    }
    if (ok) {
        emit("case=" + name + " done");
    } else {
        emit("case=" + name + " THROW class=" + cls + " msg=" + msg);
    }
}

void run_case(const std::string& name, const std::vector<std::vector<Vector3>>& bPoints,
              const eil::Array<size_t>& ids_override)
{
    emit("case=" + name + " BEGIN");
    eil::Array<Vector3> vertices;
    FaceArray faces;
    eil::AA<std::string, size_t> faceIndices;
    CellArray cells;
    eil::Array<size_t> bndry_vtx_ids;
    load_from_bpoints(vertices, faces, faceIndices, bndry_vtx_ids, bPoints);
    if (ids_override.length() > 0) bndry_vtx_ids = ids_override.dup();
    emit(eil::format("case=%s PRE nvertices=%d nfaces=%d ncells=%d naa=%d ids=%s", name,
                     vertices.length(), faces.length(), cells.length(),
                     faceIndices.length(), bndry_vtx_ids));
    call_fill_interior(name, vertices, faces, faceIndices, cells, bndry_vtx_ids);
    dump_state(name, vertices, faces, faceIndices, cells);
}

/// A case that hands over ids, and optionally the boundary faces the caller would
/// have made (fi.nonseq, fi.dupids, fi.rev, fi.nanhi, fi.nanlo, fi.inf, fi.n0,
/// fi.miss).
void run_bare_case(const std::string& name, const std::vector<Vector3>& vs,
                   const eil::Array<size_t>& ids, bool make_faces)
{
    emit("case=" + name + " BEGIN");
    eil::Array<Vector3> vertices;
    FaceArray faces;
    eil::AA<std::string, size_t> faceIndices;
    CellArray cells;
    eil::Array<size_t> bndry_vtx_ids;
    for (size_t i = 0; i < vs.size(); ++i) vertices.append(vs[i]);
    for (size_t i = 0; i < ids.length(); ++i) bndry_vtx_ids.append(ids[i]);
    if (make_faces) {
        for (size_t j = 0; j < ids.length() - 1; ++j) {
            eil::Array<size_t> vtx_id_list = {ids[j], ids[j + 1]};
            std::string faceTag = makeFaceTag(vtx_id_list.all());
            faceIndices.set(faceTag, faces.length());
            faces.append(std::make_shared<USGFace>(vtx_id_list.all()));
        }
    }
    emit(eil::format("case=%s PRE nvertices=%d nfaces=%d ncells=%d naa=%d ids=%s", name,
                     vertices.length(), faces.length(), cells.length(),
                     faceIndices.length(), bndry_vtx_ids));
    call_fill_interior(name, vertices, faces, faceIndices, cells, bndry_vtx_ids);
    dump_state(name, vertices, faces, faceIndices, cells);
}

Vector3 V(double x, double y) { return Vector3(x, y); }
Vector3 V3(double x, double y, double z) { return Vector3(x, y, z); }

// The other class-array spelling G5 may choose (raw pointers instead of
// std::shared_ptr). Driving one case through it keeps the header's claim about both
// spellings a measurement rather than a comment; its stdout is captured so it cannot
// disturb the replay, and no corpus row depends on it.
void raw_pointer_spelling_control() {
    eil::Array<Vector3> vertices;
    eil::Array<USGFace*> faces;
    eil::AA<std::string, size_t> faceIndices;
    eil::Array<USGCell*> cells;
    eil::Array<size_t> ids = {0, 1};
    vertices.append(V(0.1, 0.7));
    vertices.append(V(0.2, -0.1));
    eil::Array<size_t> f0 = {0, 1};
    faceIndices.set(makeFaceTag(f0.all()), faces.length());
    faces.append(new USGFace(f0.all()));
    eil::geom::fill_interior(vertices, faces, faceIndices, cells, ids);
    // The two spellings must agree on the numbers, so check the shared state here.
    std::string agree = eil::format(
        "ctrl.ptrspelling nvertices=%d nfaces=%d ncells=%d naa=%d mid=%.17g,%.17g "
        "cell0faces=%s",
        vertices.length(), faces.length(), cells.length(), faceIndices.length(),
        eil::re(vertices[2].x), eil::re(vertices[2].y), cells[0]->face_id_list);
    for (size_t i = 0; i < faces.length(); ++i) delete faces[i];
    for (size_t i = 0; i < cells.length(); ++i) delete cells[i];
    std::FILE* f = std::fopen("/tmp/eil_g3_ptrctrl.txt", "w");
    if (f) { std::fprintf(f, "%s\n", agree.c_str()); std::fclose(f); }
}

} // namespace

int main(int argc, char** argv)
{
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }

    //============================= nt: enum NodeType (paver2d.d:12)
    emit("nt.writerside=" + eil::geom::nodeTypeName(NodeType::side));
    emit("nt.writerrowend=" + eil::geom::nodeTypeName(NodeType::rowend));
    emit("nt.writercorner=" + eil::geom::nodeTypeName(NodeType::corner));
    emit("nt.writerreversal=" + eil::geom::nodeTypeName(NodeType::reversal));
    // D's `NodeType.init` is the member whose value is 0.
    emit("nt.init=" + eil::geom::nodeTypeName(NodeType{}));
    emit("nt.tostring side=" + eil::geom::nodeTypeName(NodeType::side));
    emit("nt.tostring rowend=" + eil::geom::nodeTypeName(NodeType::rowend));
    emit("nt.tostring corner=" + eil::geom::nodeTypeName(NodeType::corner));
    emit("nt.tostring reversal=" + eil::geom::nodeTypeName(NodeType::reversal));
    emit("nt.text all=" + eil::geom::nodeTypeName(NodeType::side)
         + eil::geom::nodeTypeName(NodeType::rowend)
         + eil::geom::nodeTypeName(NodeType::corner)
         + eil::geom::nodeTypeName(NodeType::reversal));
    emit("nt.sizeof=" + eil::to_string(static_cast<unsigned long long>(sizeof(NodeType))));
    // D's struct .init is NaN per field, not zero (wave-02 brief section 5).
    emit("nt.vinit=" + Vector3{}.toString());
    emit(eil::format("nt.dfmt side=%d rowend=%d corner=%d reversal=%d",
                     static_cast<int>(NodeType::side), static_cast<int>(NodeType::rowend),
                     static_cast<int>(NodeType::corner),
                     static_cast<int>(NodeType::reversal)));
    emit(eil::format("nt.sfmt side=%s rowend=%s corner=%s reversal=%s",
                     eil::geom::nodeTypeName(NodeType::side),
                     eil::geom::nodeTypeName(NodeType::rowend),
                     eil::geom::nodeTypeName(NodeType::corner),
                     eil::geom::nodeTypeName(NodeType::reversal)));
    emit(eil::format("nt.cast int side=%d rowend=%d corner=%d reversal=%d",
                     static_cast<int>(NodeType::side), static_cast<int>(NodeType::rowend),
                     static_cast<int>(NodeType::corner),
                     static_cast<int>(NodeType::reversal)));
    emit(eil::format("nt.cast long=%d,%d,%d,%d", static_cast<long>(NodeType::side),
                     static_cast<long>(NodeType::rowend),
                     static_cast<long>(NodeType::corner),
                     static_cast<long>(NodeType::reversal)));
    emit(eil::format("nt.cast ushort=%d,%d,%d,%d",
                     static_cast<unsigned short>(NodeType::side),
                     static_cast<unsigned short>(NodeType::rowend),
                     static_cast<unsigned short>(NodeType::corner),
                     static_cast<unsigned short>(NodeType::reversal)));
    // Out-of-range values: measured fallback, not a guess.
    emit("nt.oor7=" + eil::geom::nodeTypeName(static_cast<NodeType>(7)));
    emit("nt.oor7.tostring=" + eil::geom::nodeTypeName(static_cast<NodeType>(7)));
    emit("nt.oorminus1=" + eil::geom::nodeTypeName(static_cast<NodeType>(-1)));
    emit("nt.oormax=" + eil::geom::nodeTypeName(static_cast<NodeType>(2147483647)));
    emit(eil::format(
        "nt.cmp=%d,%d,%d,%d", static_cast<int>(NodeType::side < NodeType::rowend),
        static_cast<int>(NodeType::corner >= NodeType::reversal),
        static_cast<int>(NodeType::reversal == NodeType::reversal),
        static_cast<int>(NodeType::side != NodeType::corner)));

    //============================= fi: fill_interior (paver2d.d:14-52)
    run_case("fi.sq4", {{V(0, 0), V(1, 0), V(1, 1), V(0, 1), V(0, 0)}}, {});
    run_case("fi.hex6", {{V(0, 0), V(2, 0), V(3, 1), V(2, 2), V(0.5, 3), V(0, 0)}}, {});
    // The 12-point ring: explicit decimal literals on both sides, so no transcendental
    // is re-derived on either (backlog T-909, plus the Phobos/glibc 1 ULP floor).
    run_case("fi.poly12",
             {{V(1.5, 0), V(1.4330127018922194, 0.49999999999999994),
               V(1.25, 0.8660254037844386), V(1, 1),
               V(0.75000000000000011, 0.86602540378443871),
               V(0.56698729810778059, 0.49999999999999994),
               V(0.5, 1.2246467991473532e-16),
               V(0.56698729810778059, -0.49999999999999972),
               V(0.74999999999999978, -0.86602540378443837),
               V(0.99999999999999989, -1), V(1.25, -0.8660254037844386),
               V(1.4330127018922192, -0.50000000000000044), V(1.5, 0)}},
             {});
    run_case("fi.n1", {{V(-0.5, 3.25)}}, {});
    run_case("fi.n2a", {{V(-0.5, 0), V(0.5, 1)}}, {});
    // The degenerate two-point boundary, pinned to sum-then-divide rounding.
    run_case("fi.n2b", {{V(0.1, 0.7), V(0.2, -0.1)}}, {});
    run_case("fi.three",
             {{V(0.1, 0.33333333333333331), V(0.2, 0.2), V(0.3, 7),
               V(0.1, 0.33333333333333331)}},
             {});
    run_case("fi.hollow",
             {{V(0, 0), V(1, 0), V(1, 1)}, {V(1, 1), V(0, 1), V(0, 0)}}, {});
    run_bare_case("fi.nonseq", {V(1, 1), V(2, 3), V(4, 5), V(8, 9), V(16, 17), V(0, 0)},
                  {1, 3, 5}, true);
    run_bare_case("fi.dupids", {V(0.25, 0.75), V(1.5, -2.5)}, {0, 0, 1}, true);
    run_bare_case("fi.rev", {V(0, 0), V(3, 0), V(3, 4), V(1, 2)}, {3, 2, 1, 0}, true);
    {
        Vector3 nanv; // D's `Vector3 nanv;` -> three NaNs
        run_bare_case("fi.nanhi", {nanv, V(1, 2), V(3, 4)}, {0, 1, 2}, true);
    }
    run_bare_case("fi.nanlo", {Vector3(neg_nan(), 0, 0), V(1, 2), V(3, 4)}, {0, 1, 2},
                  true);
    run_bare_case("fi.inf",
                  {V3(1.7976931348623157e308, -1.7976931348623157e308, 0),
                   V3(1.7976931348623157e308, 0, 0)},
                  {0, 1}, true);
    run_bare_case("fi.n0", {V(1, 2)}, {}, false);
    run_bare_case("fi.miss", {V(0, 0), V(1, 0), V(1, 1)}, {0, 1, 2}, false);
    {
        // Two calls on one state: the mid vertex is appended twice and the interior
        // face tags are re-set (paver2d.d:32 is an insert-or-overwrite assignment).
        const std::string name = "fi.twice";
        emit("case=" + name + " BEGIN");
        eil::Array<Vector3> vertices;
        FaceArray faces;
        eil::AA<std::string, size_t> faceIndices;
        CellArray cells;
        eil::Array<size_t> bndry_vtx_ids;
        load_from_bpoints(vertices, faces, faceIndices, bndry_vtx_ids,
                          {{V(0, 0), V(2, 0), V(2, 2), V(0, 0)}});
        emit(eil::format("case=%s PRE nvertices=%d nfaces=%d ncells=%d naa=%d ids=%s",
                         name, vertices.length(), faces.length(), cells.length(),
                         faceIndices.length(), bndry_vtx_ids));
        {
            // D's probe calls fill_interior, prints the mid vertex, and only then
            // makes the second call, so the first call contributes just its two
            // writeln rows and no `done` row of its own.
            StdoutCapture cap;
            eil::geom::fill_interior(vertices, faces, faceIndices, cells,
                                     bndry_vtx_ids);
        }
        // D's probe writes the two doubles with writeln, i.e. through to!string, which
        // is %.6g (wave-02 brief section 5) — mirrored by eil::to_string, not %.17g.
        emit(eil::format("case=%s mid1=%s %s", name,
                         eil::to_string(eil::re(vertices[vertices.length() - 1].x)),
                         eil::to_string(eil::re(vertices[vertices.length() - 1].y))));
        // The second call emits the single `done` row, as in D's probe.
        call_fill_interior(name, vertices, faces, faceIndices, cells, bndry_vtx_ids);
        dump_state(name, vertices, faces, faceIndices, cells);
    }
    {
        StdoutCapture cap(false); // the control's writeln lines are NOT corpus rows
        raw_pointer_spelling_control();
    }

    // Replay against the ldc2 corpus.
    std::vector<std::string> expected;
    {
        std::ifstream in(argv[1]);
        if (!in) {
            std::fprintf(stderr, "cannot open baseline %s\n", argv[1]);
            return 2;
        }
        for (std::string l; std::getline(in, l);) expected.push_back(l);
    }

    int diffs = 0, shown = 0;
    size_t matched = 0;
    std::vector<bool> same(expected.size(), false);
    for (size_t i = 0; i < expected.size() || i < g_lines.size(); ++i) {
        const bool have_e = i < expected.size();
        const bool have_g = i < g_lines.size();
        const std::string e = have_e ? expected[i] : "<none>";
        const std::string g = have_g ? g_lines[i] : "<none>";
        if (have_e && have_g && e == g) {
            ++matched;
            same[i] = true;
            continue;
        }
        if (shown < 12) {
            std::printf("line %zu\n  D:   |%s|\n  C++: |%s|\n", i + 1, e.c_str(), g.c_str());
            ++shown;
        }
        ++diffs;
    }
    // How much of the replay is floating point, and how much of that matched: every
    // `vtx=` row carries three coordinates printed at %.17g.
    size_t coord_rows = 0, coord_same = 0;
    for (size_t i = 0; i < expected.size(); ++i) {
        if (expected[i].find(" vtx=") == std::string::npos) continue;
        ++coord_rows;
        if (same[i]) ++coord_same;
    }
    std::printf("%s: geom_grid_paver2d, probe replay %zu D lines, %zu C++ lines, "
                "%d differ; %zu of %zu rows byte-identical; %zu coordinate rows "
                "(3 doubles each, %%.17g), %zu of them byte-identical\n",
                diffs == 0 ? "PASS" : "FAIL", expected.size(), g_lines.size(), diffs,
                matched, expected.size(), coord_rows, coord_same);
    return diffs == 0 ? 0 : 1;
}
