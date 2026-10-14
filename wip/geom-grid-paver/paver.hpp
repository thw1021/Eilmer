#pragma once
// geom/grid/paver.d -> src/geom/grid/paver.hpp (module geom.grid.paver)
//
// Heather Muir's 2D paving algorithm (thesis, 15/4/2016): it takes a discretized
// anti-clockwise boundary, paves rows of quadrilateral cells inward, smooths them,
// seams acute corners and closes the last region. Everything it builds goes into the
// three module-global lists below, which geom/grid/usgrid.d imports directly
// (`import geom.grid.paver: PavedGrid, POINT_LIST, FACE_LIST, CELL_LIST`,
// usgrid.d:26) — so those four names, and Cell::auto_cell_type / Cell::cell_type /
// UFace::point_IDs / Point::x,y,z which usgrid.d:522-550 also reads, are an exported
// contract and keep D's spelling.
//
// Measured against ldc2 1.40.0 at gdtk e587b235 through
// probes/geom/grid/probe_paver.d; its captured corpus
// baselines/e587b235/probes/geom_grid_paver.txt (1961 rows) is replayed row for row by
// tests/test_geom_grid_paver.cpp. paver.d has no `unittest` block — proved by
//   grep -c unittest /home/tang/packages/gdtk/src/geom/grid/paver.d  ->  0
// — so that replay is the whole gate.
//
// Non-1:1 notes (also registered in provenance/fidelity.yml):
//   * `class Point` / `class UFace` / `class Cell` are D classes, so the port passes
//     them as std::shared_ptr (adapted-handles): `Point` in D parameter position
//     becomes PointPtr and `Point[]` becomes eil::Slice<PointPtr> /
//     eil::Array<PointPtr> (ref parameters). Copying one of the three global lists is
//     a deep copy of the *list* with the elements still aliased, which is D's
//     semantics for `auto b = a` on a class array.
//   * `PavedGrid` does NOT derive from Grid, and this port does not make it. Measured
//     in the reference: `class PavedGrid` (paver.d:1598) has no base class, paver.d
//     never names Grid/Vector3/Arc3 (grep -c "Grid\b" -> only the section banner), and
//     the only thing usgrid.d does with it is read its six fields and the global lists
//     before calling its own `super(...)` (usgrid.d:522-526). paver.d also contains no
//     `refine`, no `write_to_vtk_file`/`write_to_su2_file` and no
//     `single_index`/`ijk_indices` (grep -E "refine|write_to_|vtk|su2|single_index|ijk"
//     -> no match), so there is no override table to exercise through a Grid handle.
//   * Uninitialized fields carry D's `.init`, not zero: Point::x/y/interior_angle and
//     the four V_R/V_L slots are NaN (measured, row cls.fresh), while z/growth/rowID/
//     first_seam_flag are 0 because D spells an explicit initialiser for them, the
//     three Point links are null, and `Cell::cell_type` / `PavedGrid::label` are D
//     *null strings*, which Phobos prints as nothing at all — measured rows
//     cls.cell.fresh.print and pave.*.label show the empty brackets, whereas a null
//     class handle prints "null" (row cls.nullpoint). std::string models both because
//     only the printed form is observable here.
//   * `double[2]` static arrays become std::array<double,2> and `size_t[2]` becomes
//     std::array<size_t,2>; D's element-wise array operators (`x[] += y[]`,
//     `v[]/norm(v)`) are written out as the two scalar operations they compile to.
//   * `Point[4][]` (a dynamic array of 4-element static arrays, paver.d:347, :368)
//     becomes eil::Array<std::array<PointPtr,4>>.
//   * ldc2 CONSTANT-FOLDS two expressions in this module, and its folded results are
//     what the reference binary produces. Both are ported as measured bit patterns,
//     not as the expression:
//       - `double[2][2] left90matrix = left_rot_matrix(PI/2)` (paver.d:179) is folded,
//         so its cosine slots are -2.5082788063341661e-20 (0xbbdd9cceba3f91f2), the
//         80-bit cos of 80-bit PI/2, where the same function called at run time with
//         the same argument returns +6.123233995736766e-17 (0x3c91a62633145c07).
//         Measured as rows sec2.gbl.00 vs sec2.lrm3.00.
//       - `double[] tols = [(110./180.)*PI, (240./180.)*PI, (315./180.)*PI]`
//         (paver.d:774) folds to 0x3ffeb7c166fdfe3a / 0x4010c152382d7366 /
//         0x4015fdbbe9bba775; the first two are 1 ULP off any expression evaluated at
//         run time in 80-bit (0x3ffeb7c166fdfe3b / 0x4010c152382d7365). Measured as
//         rows node.tol0/1/2, and node.cat0.exactcat vs node.cat0.belowcat/abovecat
//         show the thresholds themselves deciding a node category. This is the
//         "ldc2 constant folding is not correctly rounded" case of backlog T-909,
//         which that entry measured as zero exposure for Wave 2 — paver.d is a
//         non-zero exposure, so the report says so.
//   * g++ merges a `sin(x)` and a `cos(x)` of the same argument into one glibc
//     `sincos()` call, and glibc's sincos is 1 ULP off glibc's own sin
//     (sin(0.36166097335306102): ldc2 and libm `sin` both give 0x3fd6a51f3a942d8d,
//     `sincos` gives 0x3fd6a51f3a942d8e). ldc2 lowers Phobos' sin/cos to the separate
//     libm calls, so paver.cpp routes both through volatile-isolated wrappers;
//     measured at -O0/-O2/-O3 in paver.cpp's header comment.
//   * D's `a ^^ b` is std.math.pow: every `^^` site here (point_dist, norm, squareness,
//     assign_node_properties' cosine rule, biggest_cell, the constructor's avr_area)
//     is eil::pow, never an expanded product.
//   * Phobos constants are 80-bit `real`, so `2*PI - angle`, `node.interior_angle < PI`,
//     `theta < PI/2` and `SQRT2*V_length` are computed and COMPARED in extended
//     precision and only narrowed on store; the port keeps eil::real (long double) at
//     exactly those sites.
//   * D's `assert(cond)` at paver.d:125, :260, :576, :1032 and the message form at
//     :1271 map to eil_assert / eil_assert_msg, which throw eil::ErrorBase (an Error, so
//     a ported `catch (ExceptionBase)` does not catch it — corpus rows cls.outsigns.bad,
//     seam.cs.odd, pave.sq1 prove exactly that). Every `throw new Error(...)`
//     (:788, :1366, :1622, :1638, :1642) is eil::ErrorBase with the message only; every
//     `throw new GeometryException(...)` (:341, :991) is eil::geom::GeometryException.
//   * paver.d:1229's `throw new Error("cannot terminate at >5 node seam for closing
//     procedure - try recursion for closing")` is UNREACHABLE in D: the enclosing `else`
//     is only entered when the second arm's `closing==1 ||` test failed, which requires
//     closing==0, and the throw's own `else` then demands closing==1. The reachable half
//     of that arm (the writeln and the break) is exercised by corpus row
//     seam.wide.done; the throw needs no ported path beyond the literal transcription.
//   * `findFace` (paver.d:507-526) has no `break`, so the LAST matching face wins, and
//     its `size_t[2] reverse_case;` defaults to [0,0], which makes a directional query
//     match any face stored as [0,0]. Both are observable (rows make.q01nd,
//     make.q56.afterzero) and are transcribed as written, not "fixed".
//   * `sort`/`remove` from std.algorithm are imported by paver.d but never called, and
//     `import geom;` is used only for GeometryException (geom/geometry_exception.d).

#include <array>
#include <cstddef>
#include <memory>
#include <string>

#include "eil/array.hpp"
#include "eil/number.hpp"

namespace eil {
namespace geom {

// D:145-153 `enum CellType { none = 0, line = 1, dubline = 2, triangle = 3, quad = 4,
// pent = 5, hex = 6 }`. Note the comment in D: "this is different to USGCell_type in
// usgrid.d". Cell::auto_cell_type renders an out-of-range value the way Phobos'
// `to!string` does — "cast(CellType)7" (measured, rows cls.celltype.7/8).
enum class CellType : int { none = 0, line = 1, dubline = 2, triangle = 3, quad = 4,
                            pent = 5, hex = 6 };

class Point;
using PointPtr = std::shared_ptr<Point>;
class UFace;
using UFacePtr = std::shared_ptr<UFace>;
class Cell;
using CellPtr = std::shared_ptr<Cell>;

// D:21-75
class Point {
  public:
    size_t ID = 0;
    double x = eil::d_init<double>();
    double y = eil::d_init<double>();
    double z = 0;

    double interior_angle = eil::d_init<double>();
    std::string node_cat = "none";
    std::array<double, 2> V_R{eil::d_init<double>(), eil::d_init<double>()}; // vector to
                                                                             // node on
    std::array<double, 2> V_L{eil::d_init<double>(), eil::d_init<double>()}; // right
    PointPtr pointL;
    PointPtr pointR;
    PointPtr prev_node;
    double growth = 0;
    bool first_seam_flag = 0;

    size_t rowID = 0;

    // D:40-44 (note z is NOT taken from the constructor, so it stays at its 0)
    Point(size_t ID, double x, double y);

    void store_node_angle(double node_angle);                    // D:46-48
    void store_node_cat(const std::string& node_cat);            // D:50-54 (sticky)
    void store_vectors(const std::array<double, 2>& vr,
                       const std::array<double, 2>& vl);         // D:56-59
    void store_adjacent_points(const PointPtr& pointL, const PointPtr& pointR); // D:61-64
    void store_rowID(size_t rowID);                              // D:66-68
    std::string print();                                         // D:70-72

    static inline size_t ID_count = 0;                           // D:74
};

// D:79-95
class UFace {
  public:
    size_t face_ID = 0;
    std::array<size_t, 2> point_IDs{0, 0};

    UFace(size_t face_ID, const std::array<size_t, 2>& point_IDs); // D:86-89
    std::string print();                                           // D:90-92

    static inline size_t face_ID_count = 0;                        // D:94
};

// D:99-143
class Cell {
  public:
    size_t cell_ID = 0;
    std::string cell_type;              // D: null string until auto_cell_type()
    eil::Array<size_t> point_IDs;
    eil::Array<size_t> face_IDs;
    eil::Array<int> outsigns;           // order of outsigns corresponds to face_IDs

    explicit Cell(size_t cell_ID);      // D:107-109
    void store_point_IDs(eil::Slice<PointPtr> points); // D:111-115
    void store_face_IDs(eil::Slice<UFacePtr> faces);   // D:117-121
    void assign_outsigns(eil::Slice<int> outsigns);    // D:123-128 (assert D:125)
    void auto_cell_type();                             // D:130-135
    std::string print();                               // D:137-139

    static inline size_t cell_ID_count = 0;            // D:141
};

// D:157-162. The globally accessible state of the algorithm; `initial_size` and
// `avr_area` are D doubles with no initialiser, so they start at .init (NaN) and only
// the PavedGrid constructor gives them values.
inline int ndimensions = 2;
inline eil::Array<PointPtr> POINT_LIST;
inline eil::Array<UFacePtr> FACE_LIST;
inline eil::Array<CellPtr> CELL_LIST;
inline double initial_size = eil::d_init<double>();
inline double avr_area = eil::d_init<double>();

// D:179 — see the header note: the module-initializer value is ldc2's folded one, not
// the run-time value of left_rot_matrix(PI/2).
inline std::array<std::array<double, 2>, 2> left90matrix = {
    std::array<double, 2>{-2.5082788063341661e-20, 1.0},
    std::array<double, 2>{-1.0, -2.5082788063341661e-20}};

// ---- Section 2 (D:170-293): geometry and vector operations ----
std::array<std::array<double, 2>, 2> left_rot_matrix(double theta); // D:170-177
std::array<double, 2> vector_rot(std::array<double, 2> vector,
                                 const std::array<std::array<double, 2>, 2>& matrix); // D:181
PointPtr midpoint(PointPtr a, PointPtr b);                     // D:191-195
double point_dist(PointPtr a, PointPtr b);                     // D:198-204
std::array<double, 2> vector2D(PointPtr a, PointPtr b);        // D:206-214
int LeftorRight(PointPtr a, PointPtr b, PointPtr X);           // D:216-232
double norm(std::array<double, 2> vector);                     // D:234-240
std::array<double, 2> unit_normal_vector(PointPtr point0, PointPtr pointR); // D:242-247
std::array<double, 2> unit_leftV(std::array<double, 2> vr, double theta);   // D:249-255
double squareness(CellPtr cell);                               // D:257-275
int winding_number(PointPtr P, eil::Slice<PointPtr> surface);  // D:278-293

// ---- Section 3 (D:300-435) ----
double clockwise_check(eil::Slice<PointPtr> surface);          // D:300-317
double calc_area(eil::Slice<PointPtr> cell_points);            // D:320-331
void closed_row_check(eil::Slice<PointPtr> surface);           // D:334-344
eil::Array<std::array<PointPtr, 4>> intersecting_boundary(eil::Slice<PointPtr> surface); // D:347-365
eil::Array<std::array<PointPtr, 4>> intersecting_faces(eil::Slice<PointPtr> surface);    // D:368-396
double average_face_length(eil::Slice<PointPtr> surface);      // D:398-408
double angle_between_vectors(std::array<double, 2> a, std::array<double, 2> b); // D:410-422
bool inside_prev_row_check(eil::Slice<PointPtr> New_Row, eil::Slice<PointPtr> Prev_Row); // D:424-435

// ---- Section 4 (D:442-1369): the paving functions ----
PointPtr make2DPoint(double x, double y);                      // D:442-451
UFacePtr makeFace(size_t ID_0, size_t ID_1);                   // D:453-465
CellPtr makeCell(eil::Slice<PointPtr> points);                 // D:467-505
UFacePtr findFace(const std::array<size_t, 2>& pointIDs, bool directional); // D:507-526
void assign_node_properties(eil::Slice<PointPtr> surface);     // D:528-607
void adjust_row_ends(eil::Slice<PointPtr> surface, double initial_size,
                     bool boundary);                          // D:610-762
std::string node_category(double angle);                       // D:765-792
void node_gen(eil::Slice<PointPtr> surface, size_t i, eil::Array<PointPtr>& New_Nodes); // D:795-967
eil::Array<PointPtr> generate_new_row(eil::Slice<PointPtr> surface); // D:970-1014
double summed_squareness(PointPtr node);                       // D:1016-1035
double smoothing(eil::Slice<PointPtr> surface, eil::Slice<PointPtr> Prev_Row, bool boundary,
                 bool first_row);                             // D:1039-1113
eil::Array<PointPtr> seam(PointPtr start, eil::Array<PointPtr>& New_Row,
                          eil::Array<size_t>& adjust_IDs, size_t& original_nodes_left,
                          bool closing);                      // D:1115-1260
void closing_seam(eil::Slice<PointPtr> Final_Row);             // D:1262-1280
void fix_IDs(eil::Slice<UFacePtr> face_list, eil::Slice<CellPtr> cell_list); // D:1283-1301
double biggest_cell(eil::Slice<CellPtr> cells, double initial_size); // D:1304-1322
void even_out_size(eil::Slice<PointPtr> surface);              // D:1324-1360
void point_ID_check();                                        // D:1362-1369

// ---- Section 5 (D:1375-1592): the major paving procedures ----
void pave_rows_until_intersection(eil::Array<PointPtr>& New_Row, eil::Array<PointPtr>& Prev_Row,
                                  size_t npoints_boundary, bool& seam_next); // D:1375-1527
void seaming_procedure(eil::Array<PointPtr>& New_Row);        // D:1530-1592

// ---- Section 6 (D:1598-1731) ----
class PavedGrid {
  public:
    int dimensions = 2;
    std::string label; // D: a null string; never assigned by the constructor

    size_t nvertices = 0, nfaces = 0, ncells = 0, nboundaries = 0;

    explicit PavedGrid(const eil::Array<eil::Array<double>>& boundary_points); // D:1607-1730
};

} // namespace geom
} // namespace eil
