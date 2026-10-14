// geom/grid/paver.d -> src/geom/grid/paver.cpp (module geom.grid.paver)
//
// Definitions appear in D's order (D:21 through D:1731). See src/geom/grid/paver.hpp
// for the non-1:1 notes; the ones that show up in the bodies below are the
// `^^` -> eil::pow rule, the 80-bit `real` sites, the ErrorBase/GeometryException
// split and the two ldc2 constant-folded value sets.
//
// Transcendental routing, measured on this box (ldc2 1.40.0 vs g++ 11 -O0/-O2/-O3):
//   sin(0.36166097335306102)  ldc2 0.35382824631297966 (0x3fd6a51f3a942d8d)
//                             glibc sin   "  "...966 (0x3fd6a51f3a942d8d)
//                             glibc sincos  "  "...971 (0x3fd6a51f3a942d8e)
// g++ merges `std::sin(x)` and `std::cos(x)` of one argument into a single glibc
// sincos() call at -O2 and -O3, and that merged call is 1 ULP off. ldc2 lowers
// Phobos' sin/cos (core.math, llvm.sin.f64/llvm.cos.f64) to the two separate libm
// calls, so the wrappers below read the argument through a volatile copy: that keeps
// the pair from merging and reproduces D's bits. `nm` on the merged build shows
// `U sincos@GLIBC_2.2.5`; with the wrappers it shows only sin/cos.

#include "geom/grid/paver.hpp"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>

#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/math.hpp"
#include "geom/geometry_exception.hpp"

namespace {

// libm calls, kept separate so g++ cannot fuse a sin/cos pair into sincos().
inline double d_sin(double x) {
    volatile double v = x;
    return std::sin(v);
}
inline double d_cos(double x) {
    volatile double v = x;
    return std::cos(v);
}

// D's `abs(double)` from std.math (used by squareness and calc_area) is fabs.
inline double d_abs(double x) { return std::fabs(x); }

} // namespace

namespace eil {
namespace geom {

//=========================================================== Section 1 (D:21-162)

// D:40-44
Point::Point(size_t ID, double x, double y) : ID(ID), x(x), y(y) {}

// D:46-48
void Point::store_node_angle(double node_angle) { this->interior_angle = node_angle; }

// D:50-54: the category is only written while it is still "none".
void Point::store_node_cat(const std::string& node_cat) {
    if (this->node_cat == "none") {
        this->node_cat = node_cat;
    }
}

// D:56-59
void Point::store_vectors(const std::array<double, 2>& vr, const std::array<double, 2>& vl) {
    this->V_R = vr;
    this->V_L = vl;
}

// D:61-64
void Point::store_adjacent_points(const PointPtr& pointL, const PointPtr& pointR) {
    this->pointL = pointL;
    this->pointR = pointR;
}

// D:66-68
void Point::store_rowID(size_t rowID) { this->rowID = rowID; }

// D:70-72. `%s` on a double is Phobos' %.6g, so this text is much shorter than the
// %.17g the replay corpus dumps elsewhere.
std::string Point::print() {
    return eil::format("row ID: %s | xyz(%s,%s,%s) \n interior angle: %s | node cat: %s",
                       rowID, x, y, z, interior_angle, node_cat);
}

// D:86-89
UFace::UFace(size_t face_ID, const std::array<size_t, 2>& point_IDs)
    : face_ID(face_ID), point_IDs(point_IDs) {}

// D:90-92
std::string UFace::print() {
    return eil::format("face ID: %s | contains points: [%s, %s])", face_ID, point_IDs[0],
                       point_IDs[1]);
}

// D:107-109
Cell::Cell(size_t cell_ID) : cell_ID(cell_ID) {}

// D:111-115
void Cell::store_point_IDs(eil::Slice<PointPtr> points) {
    for (size_t t = 0; t < points.length(); ++t) {
        point_IDs.append(points[t]->ID);
    }
}

// D:117-121
void Cell::store_face_IDs(eil::Slice<UFacePtr> faces) {
    for (size_t t = 0; t < faces.length(); ++t) {
        face_IDs.append(faces[t]->face_ID);
    }
}

// D:123-128. The bare assert at D:125 keeps D's "Assertion failure" text, and the
// `~=` appends rather than replaces.
void Cell::assign_outsigns(eil::Slice<int> outsigns) {
    for (size_t t = 0; t < outsigns.length(); ++t) {
        eil_assert(outsigns[t] == -1 || outsigns[t] == 1);
    }
    this->outsigns.append_slice(outsigns);
}

// D:130-135: `cast(CellType)num_faces` then `to!string(cell_type)`. Measured (rows
// cls.celltype.0..8) that Phobos renders an in-range enumerator by name and an
// out-of-range one as the literal text `cast(CellType)N`, which is what D's own
// enum-name lookup prints when no member matches.
void Cell::auto_cell_type() {
    int num_faces = static_cast<int>(this->face_IDs.length());
    std::string cell;
    switch (num_faces) {
      case 0: cell = "none"; break;
      case 1: cell = "line"; break;
      case 2: cell = "dubline"; break;
      case 3: cell = "triangle"; break;
      case 4: cell = "quad"; break;
      case 5: cell = "pent"; break;
      case 6: cell = "hex"; break;
      default: cell = "cast(CellType)" + eil::to_string(num_faces); break;
    }
    this->cell_type = cell;
}

// D:137-139. `%s` on a dynamic array is D's "[a, b, c]" rendering.
std::string Cell::print() {
    return eil::format("cell ID: %s | cell type: %s | contains point IDs: %s and face IDs: %s",
                       cell_ID, cell_type, point_IDs, face_IDs);
}

//=========================================================== Section 2 (D:170-293)

// D:170-177
std::array<std::array<double, 2>, 2> left_rot_matrix(double theta) {
    std::array<std::array<double, 2>, 2> matrix = {
        std::array<double, 2>{d_cos(theta), d_sin(theta)},
        std::array<double, 2>{-d_sin(theta), d_cos(theta)}};
    return matrix;
}

// D:181-189
std::array<double, 2> vector_rot(std::array<double, 2> vector,
                                 const std::array<std::array<double, 2>, 2>& matrix) {
    std::array<double, 2> result;
    result[0] = (vector[0] * matrix[0][0]) + (vector[1] * matrix[1][0]);
    result[1] = (vector[0] * matrix[0][1]) + (vector[1] * matrix[1][1]);
    return result;
}

// D:191-195. Note the new Point is NOT appended to POINT_LIST and does NOT advance
// Point.ID_count (measured, rows sec2.mid.listlen / sec2.mid.idcount), and D's
// comment "//^garbage collection here" stays true: it allocates on every call.
PointPtr midpoint(PointPtr a, PointPtr b) {
    return std::make_shared<Point>(0, 0.5 * (a->x + b->x), 0.5 * (a->y + b->y));
}

// D:198-204
double point_dist(PointPtr a, PointPtr b) {
    return eil::pow(eil::pow(a->x - b->x, 2.0) + eil::pow(a->y - b->y, 2.0), 0.5);
}

// D:206-214
std::array<double, 2> vector2D(PointPtr a, PointPtr b) {
    std::array<double, 2> vector;
    vector[0] = b->x - a->x;
    vector[1] = b->y - a->y;
    return vector;
}

// D:216-232. `int ans;` is D's .init, i.e. 0, and a NaN determinant makes every one
// of the three comparisons false — so 0 is a fourth, reachable answer (measured).
int LeftorRight(PointPtr a, PointPtr b, PointPtr X) {
    std::array<double, 2> AB = vector2D(a, b);
    std::array<double, 2> AX = vector2D(a, X);
    double determinant = AB[0] * AX[1] - AB[1] * AX[0];
    int ans = eil::d_init<int>();
    if (determinant == 0) {
        ans = 2;
    } else if (determinant < 0) {
        ans = -1;
    } else if (determinant > 0) {
        ans = 1;
    }
    return ans;
}

// D:234-240
double norm(std::array<double, 2> vector) {
    return eil::pow(eil::pow(vector[0], 2.0) + eil::pow(vector[1], 2.0), 0.5);
}

// D:242-247
std::array<double, 2> unit_normal_vector(PointPtr point0, PointPtr pointR) {
    std::array<double, 2> V_f = vector2D(point0, pointR);
    std::array<double, 2> r = vector_rot(V_f, left90matrix);
    double n = norm(V_f);
    std::array<double, 2> V_norm = {r[0] / n, r[1] / n};
    return V_norm;
}

// D:249-255. `vector[] /= norm(vector)` narrows nothing: both slots divide by the
// same single evaluation of norm.
std::array<double, 2> unit_leftV(std::array<double, 2> vr, double theta) {
    std::array<std::array<double, 2>, 2> matrix = left_rot_matrix(theta);
    std::array<double, 2> vector = vector_rot(vr, matrix);
    double n = norm(vector);
    vector[0] /= n;
    vector[1] /= n;
    return vector;
}

// D:257-275
double squareness(CellPtr cell) {
    int n = static_cast<int>(cell->point_IDs.length());
    eil_assert(n == 4);
    double area;    // D: .init (NaN); both are assigned before any read
    double perimeter;
    eil::Array<PointPtr> points;
    for (int i = 0; i < 4; ++i) {
        points.append(POINT_LIST[cell->point_IDs[static_cast<size_t>(i)]]);
    }

    area = 0.5 * d_abs(vector2D(points[0], points[1])[0] *
                            vector2D(points[0], points[3])[1] -
                        vector2D(points[0], points[1])[1] *
                            vector2D(points[0], points[3])[0]);
    area += 0.5 * d_abs(vector2D(points[2], points[1])[0] *
                            vector2D(points[2], points[3])[1] -
                        vector2D(points[2], points[1])[1] *
                            vector2D(points[2], points[3])[0]);

    perimeter = point_dist(points[0], points[1]) + point_dist(points[1], points[2]) +
                point_dist(points[2], points[3]) + point_dist(points[3], points[0]);

    return 16 * area / eil::pow(perimeter, 2.0);
}

// D:278-293
int winding_number(PointPtr P, eil::Slice<PointPtr> surface) {
    int wn = eil::d_init<int>();
    PointPtr a;
    PointPtr b;
    PointPtr Pr = std::make_shared<Point>(0, P->x + 1, P->y);
    for (size_t t = 0; t < surface.length(); ++t) {
        a = surface[t];
        b = surface[t]->pointR;
        if (LeftorRight(P, Pr, b) == 1 && LeftorRight(P, Pr, a) == -1 &&
            LeftorRight(a, b, P) == 1) {
            ++wn;
        }
        if (LeftorRight(P, Pr, b) == -1 && LeftorRight(P, Pr, a) == 1 &&
            LeftorRight(a, b, P) == -1) {
            --wn;
        }
    }
    return wn;
}

//=========================================================== Section 3 (D:300-435)

// D:300-317. The area is twice-signed by the ordering of the row; the two commented-
// out writeln lines in D are not ported because they are not in the reference binary.
double clockwise_check(eil::Slice<PointPtr> surface) {
    double sum = 0;
    for (size_t t = 0; t < surface.length(); ++t) {
        PointPtr p = surface[t];
        sum += (p->x - p->pointL->x) * (p->y + p->pointL->y);
    }
    double area = sum / 2;

    closed_row_check(surface);

    return area;
}

// D:320-331
double calc_area(eil::Slice<PointPtr> cell_points) {
    double sum = 0;
    int n = static_cast<int>(cell_points.length());
    for (int i = 0; i < n - 1; ++i) {
        sum += (cell_points[static_cast<size_t>(i + 1)]->x -
                cell_points[static_cast<size_t>(i)]->x) *
               (cell_points[static_cast<size_t>(i + 1)]->y +
                cell_points[static_cast<size_t>(i)]->y);
    }
    sum += (cell_points[0]->x - cell_points[static_cast<size_t>(n - 1)]->x) *
           (cell_points[0]->y + cell_points[static_cast<size_t>(n - 1)]->y);
    return d_abs(sum / 2);
}

// D:334-344
void closed_row_check(eil::Slice<PointPtr> surface) {
    for (size_t t = 0; t < surface.length(); ++t) {
        PointPtr point = surface[t];
        if (point->pointL->pointR != point) {
            EIL_THROW_GEOM(eil::format("this point is not linked correctly: \n %s",
                                      point->print()));
        }
    }
}

// D:347-365
eil::Array<std::array<PointPtr, 4>> intersecting_boundary(eil::Slice<PointPtr> surface) {
    eil::Array<std::array<PointPtr, 4>> intersections;
    for (size_t t = 0; t < surface.length(); ++t) {
        PointPtr point = surface[t];
        for (size_t u = 0; u < surface.length(); ++u) {
            PointPtr otherpoint = surface[u];
            if (otherpoint != point && otherpoint != point->pointR &&
                otherpoint != point->pointL) {
                PointPtr a = otherpoint;
                PointPtr b = otherpoint->pointR;
                if (LeftorRight(point, point->pointR, a) ==
                        -1 * LeftorRight(point, point->pointR, b) &&
                    LeftorRight(a, b, point) == -1 * LeftorRight(a, b, point->pointR)) {
                    std::array<PointPtr, 4> points = {point, point->pointR, a, b};
                    intersections.append(points);
                }
            }
        }
    }
    return intersections;
}

// D:368-396
eil::Array<std::array<PointPtr, 4>> intersecting_faces(eil::Slice<PointPtr> surface) {
    eil::Array<std::array<PointPtr, 4>> intersections;
    size_t npoints_row = surface.length();
    eil::Slice<UFacePtr> new_row_faces = FACE_LIST.from(FACE_LIST.length() - npoints_row);
    PointPtr a, b, c, d;
    for (size_t t = 0; t < new_row_faces.length(); ++t) {
        UFacePtr face = new_row_faces[t];
        for (size_t u = 0; u < FACE_LIST.length(); ++u) {
            UFacePtr otherface = FACE_LIST[u];
            if (otherface != face) {
                a = POINT_LIST[face->point_IDs[0]];
                b = POINT_LIST[face->point_IDs[1]];
                c = POINT_LIST[otherface->point_IDs[0]];
                d = POINT_LIST[otherface->point_IDs[1]];
                if (LeftorRight(a, b, c) == -1 * LeftorRight(a, b, d) &&
                    LeftorRight(c, d, a) == -1 * LeftorRight(c, d, b)) {
                    std::array<PointPtr, 4> points = {a, b, c, d};
                    intersections.append(points);
                }
            }
        }
    }
    return intersections;
}

// D:398-408. `int count` starts at D's .init (0), so an empty surface divides 0.0 by
// 0.0 — which is a NEGATIVE NaN in both languages (measured row sec3.afl.empty).
double average_face_length(eil::Slice<PointPtr> surface) {
    double sum = 0;
    int count = eil::d_init<int>();
    for (size_t t = 0; t < surface.length(); ++t) {
        PointPtr point = surface[t];
        ++count;
        sum += point_dist(point, point->pointR);
    }
    return sum / static_cast<double>(count);
}

// D:410-422. The local is named `abs`, which shadows std.math.abs in D; `2*PI - theta`
// is computed in 80-bit and narrowed on the way back into the double.
double angle_between_vectors(std::array<double, 2> a, std::array<double, 2> b) {
    double dot = a[0] * b[0] + a[1] * b[1];
    double abs = norm(a) * norm(b);
    double theta = eil::acos(dot / abs);
    PointPtr pa = std::make_shared<Point>(0, 0.0, 0.0);
    PointPtr pb = std::make_shared<Point>(0, a[0], a[1]);
    PointPtr X = std::make_shared<Point>(0, b[0], b[1]);
    if (LeftorRight(pa, pb, X) != -1) {
        theta = static_cast<double>(2.0 * EIL_PI - static_cast<eil::real>(theta));
    }
    return theta;
}

// D:424-435
bool inside_prev_row_check(eil::Slice<PointPtr> New_Row, eil::Slice<PointPtr> Prev_Row) {
    bool inside = 1;
    for (size_t t = 0; t < New_Row.length(); ++t) {
        PointPtr p = New_Row[t];
        int wn = winding_number(p, Prev_Row);
        if (wn == 0) {
            inside = 0;
            break;
        }
    }
    return inside;
}

//=========================================================== Section 4 (D:442-1369)

// D:442-451
PointPtr make2DPoint(double x, double y) {
    size_t ID = Point::ID_count;
    ++Point::ID_count;

    PointPtr point = std::make_shared<Point>(ID, x, y);
    POINT_LIST.append(point);

    return point;
}

// D:453-465
UFacePtr makeFace(size_t ID_0, size_t ID_1) {
    size_t ID = UFace::face_ID_count;
    ++UFace::face_ID_count;

    UFacePtr face = std::make_shared<UFace>(ID, std::array<size_t, 2>{ID_0, ID_1});
    FACE_LIST.append(face);

    return face;
}

// D:507-526. There is deliberately no break: the LAST match wins. And when
// directional is true, `reverse_case` keeps D's static-array .init of [0, 0], so a
// directional query also matches any face stored as [0,0] (measured, row
// make.q56.afterzero).
UFacePtr findFace(const std::array<size_t, 2>& pointIDs, bool directional) {
    UFacePtr found;
    std::array<size_t, 2> reverse_case = {0, 0};
    if (!directional) {
        reverse_case = {pointIDs[1], pointIDs[0]};
    }
    for (size_t t = 0; t < FACE_LIST.length(); ++t) {
        UFacePtr face = FACE_LIST[t];
        if (face->point_IDs == pointIDs || face->point_IDs == reverse_case) {
            found = face;
        }
    }
    return found;
}

// D:467-505. points must come in anticlockwise order or the outsigns are wrong, as
// D's own comment says.
CellPtr makeCell(eil::Slice<PointPtr> points) {
    size_t ID = Cell::cell_ID_count;
    ++Cell::cell_ID_count;
    eil::Array<UFacePtr> faces;
    eil::Array<int> outsigns;

    for (unsigned int i = 0; i < points.length(); ++i) {
        unsigned int a = i;
        unsigned int b;
        if (a == points.length() - 1) {
            b = 0;
        } else {
            b = a + 1;
        }
        UFacePtr found1 =
            findFace({points[a]->ID, points[b]->ID}, 1);
        UFacePtr found2 =
            findFace({points[b]->ID, points[a]->ID}, 1);
        if (found1 == nullptr && found2 == nullptr) {
            faces.append(makeFace(points[a]->ID, points[b]->ID));
            outsigns.append(-1);
        } else if (found1 == nullptr) {
            faces.append(found2);
            outsigns.append(1);
        } else {
            faces.append(found1);
            outsigns.append(-1);
        }
    }

    CellPtr cell = std::make_shared<Cell>(ID);
    cell->store_point_IDs(points);
    cell->store_face_IDs(faces.all());
    cell->assign_outsigns(outsigns.all());
    cell->auto_cell_type();
    CELL_LIST.append(cell);

    return cell;
}

// D:528-607. The cosine rule at D:575 is three `^^2` sites and a division; the
// bare assert at D:576 fires for a NaN r (both comparisons false), which is what a
// zero-length leg produces, because 0/0 is NaN (measured, rows node.one.throw and
// node.dup.throw).
void assign_node_properties(eil::Slice<PointPtr> surface) {
    size_t n = surface.length();

    for (size_t i = 0; i < n; ++i) {
        size_t nodeID = i;
        PointPtr point0 = surface[nodeID];
        PointPtr pointR; // must be created outside of conditional statements
        PointPtr pointL;

        if (nodeID == n - 1) {
            pointL = surface[nodeID - 1];
            pointR = surface[0];
        } else if (nodeID == 0) {
            pointL = surface[n - 1];
            pointR = surface[nodeID + 1];
        } else {
            pointL = surface[nodeID - 1];
            pointR = surface[nodeID + 1];
        }

        double c = point_dist(pointL, pointR);
        double a = point_dist(pointL, point0);
        double b = point_dist(pointR, point0);
        double r = (eil::pow(a, 2.0) + eil::pow(b, 2.0) - eil::pow(c, 2.0)) / (2 * a * b);
        eil_assert(r < 1.01 && r > -1.01); // checking calculated r is plausible
        if (r > 1.0) r = 1.0;
        if (r < -1.0) r = -1.0;
        double angle = eil::acos(r);

        if (angle > 3.141 && angle < 3.142) {
            // if angle is basically pi it doesn't need adjusting
        } else {
            // determine if the interior or exterior angle was calculated: point0
            // should be to the right for an interior angle calc.
            int ans = LeftorRight(pointL, pointR, point0);
            if (ans == 1) { // exterior angle was calculated - fix:
                angle = static_cast<double>(2.0 * EIL_PI - static_cast<eil::real>(angle));
            }
        }

        point0->store_node_angle(angle);
        std::string node_cat = node_category(angle);
        point0->store_node_cat(node_cat);
        std::array<double, 2> V_R = vector2D(point0, pointR);
        std::array<double, 2> V_L = vector2D(point0, pointL);
        point0->store_vectors(V_R, V_L);
        point0->store_adjacent_points(pointL, pointR);
    }
}

// D:765-792. ldc2 constant-folds the three tolerance literals in D's array into
// 0x3ffeb7c166fdfe3a, 0x4010c152382d7366 and 0x4015fdbbe9bba775; the first two are
// 1 ULP away from any run-time 80-bit evaluation of the same expression (measured
// rows node.tol0/1/2 against a run-time computation in this file's own test).
namespace {
double from_bits(unsigned long long u) {
    double d;
    std::memcpy(&d, &u, sizeof d);
    return d;
}
const double kTol110 = from_bits(0x3ffeb7c166fdfe3aull);
const double kTol240 = from_bits(0x4010c152382d7366ull);
const double kTol315 = from_bits(0x4015fdbbe9bba775ull);
} // namespace

std::string node_category(double angle) {
    std::string node_cat;

    // D: `double[] tols = [(110./180.)*PI, (240./180.)*PI, (315./180.)*PI];`
    const double tols[3] = {kTol110, kTol240, kTol315};

    if (angle <= tols[0]) {
        node_cat = "row end";
    } else if ((tols[0] < angle) && (angle <= tols[1])) {
        node_cat = "row side";
    } else if ((tols[1] < angle) && (angle <= tols[2])) {
        node_cat = "row corner";
    } else if ((tols[2] < angle) &&
               (static_cast<eil::real>(angle) <= 2.0 * EIL_PI)) {
        node_cat = "row reversal";
    } else if (static_cast<eil::real>(angle) > 2.0 * EIL_PI) {
        throw eil::ErrorBase("calculated angle > 2*PI... error");
    }

    return node_cat;
}

// D:610-762. The parameter is named initial_size and shadows the module global for
// the whole body, exactly as in D (the global is not read here at all).
void adjust_row_ends(eil::Slice<PointPtr> surface, double initial_size, bool boundary) {
    auto conditions = [](const PointPtr& node) {
        bool ans = eil::d_init<bool>();
        if (node == nullptr) return ans;
        if (node->pointL->node_cat != "row end" && node->pointR->node_cat != "row end" &&
            node->pointL->pointL->node_cat != "row end" &&
            node->pointR->pointR->node_cat != "row end" &&
            node->pointL->node_cat != "with row end" &&
            node->pointR->node_cat != "with row end" &&
            node->pointL->pointL->node_cat != "with row end" &&
            node->pointR->pointR->node_cat != "with row end" &&
            static_cast<eil::real>(node->interior_angle) < EIL_PI) {
            ans = 1;
        } else {
            ans = 0;
        }
        return ans;
    };

    auto conditions2 = [](const PointPtr& node) {
        bool ans = eil::d_init<bool>();
        if (node == nullptr) return ans;
        if (node->pointL->node_cat != "row corner" && node->pointR->node_cat != "row corner" &&
            node->pointL->node_cat != "row reversal" &&
            node->pointR->node_cat != "row reversal") {
            ans = 1;
        } else {
            ans = 0;
        }
        return ans;
    };

    if (boundary == 0) {
        eil::Array<PointPtr> row_end_list;
        double smallest_angle = static_cast<double>(2.0 * EIL_PI);
        PointPtr smallest_angle_node;

        for (size_t t = 0; t < surface.length(); ++t) {
            PointPtr node = surface[t];
            if (node->node_cat == "row end") {
                row_end_list.append(node);
            } else if (node->interior_angle < smallest_angle) {
                smallest_angle = node->interior_angle;
                smallest_angle_node = node;
            }
        }
        // enforces a row end for the smallest angled node if no row ends exist in row
        if (row_end_list.length() == 0) {
            smallest_angle_node->node_cat = "row end";
            row_end_list.append(smallest_angle_node);
        }
        if (row_end_list.length() == 1) {
            size_t move = surface.length() / 2;
            PointPtr node = smallest_angle_node;
            for (size_t i = 0; i < move; ++i) {
                node = node->pointR;
            }
            if (static_cast<eil::real>(node->interior_angle) < EIL_PI) {
                node->node_cat = "row end";
                eil::writeln("matching row end enforced");
                row_end_list.append(node);
            } else {
                // D:693 keeps the GeometryException of the alternative commented out
            }
        }
        // asserting additional row ends to manage cell size reduction and inserting
        // wedges to manage cell size increases
        double biggest_growth = 0;
        double biggest_reduction = 1.0;
        double avr_face_length = average_face_length(surface);
        PointPtr growth_node;
        PointPtr shrink_node;
        for (size_t t = 0; t < surface.length(); ++t) {
            PointPtr node = surface[t];
            eil::Array<size_t> point_IDs;
            size_t node_count = eil::d_init<size_t>(); // D: .init == 0
            for (size_t u = 0; u < FACE_LIST.length(); ++u) {
                UFacePtr face = FACE_LIST[u];
                point_IDs = eil::Array<size_t>{face->point_IDs[0], face->point_IDs[1]};
                bool found = false;
                for (size_t k = 0; k < point_IDs.length(); ++k) {
                    if (point_IDs[k] == node->ID) found = true;
                }
                if (found) ++node_count;
            }
            if (node_count >= 4 && conditions(node)) {
                node->node_cat = "row end";
            }
            if (node->prev_node != nullptr) {
                double growth = (norm(node->V_L) + norm(node->V_R)) /
                                (norm(node->prev_node->V_L) + norm(node->prev_node->V_R));
                if (node->growth == 0) {
                    node->growth = growth;
                } else {
                    node->growth = node->growth * growth;
                }
                if (node->growth > biggest_growth &&
                    (norm(node->V_L) + norm(node->V_R)) > 4 * avr_face_length &&
                    conditions2(node)) {
                    biggest_growth = node->growth;
                    growth_node = node;
                }
                if (node->growth < biggest_reduction &&
                    (norm(node->V_L) + norm(node->V_R)) < 1 * avr_face_length &&
                    conditions(node)) {
                    biggest_reduction = node->growth;
                    shrink_node = node;
                }
            }
        }
        if (biggest_growth > 1.5) {
            eil::writeln("growth node detected - wedge inserted");
            growth_node->node_cat = "row corner";
        }
        if (biggest_reduction < 0.5) {
            eil::writeln("row end enforced to manage cell size reduction");
            shrink_node->node_cat = "row end";
        }
    }

    for (size_t t = 0; t < surface.length(); ++t) {
        PointPtr node = surface[t];
        if (node->node_cat == "row end" && node->pointR->node_cat == "row end") {
            node->node_cat = "row side";
        }
        if (node->pointL->pointL->node_cat == "row end" &&
            node->pointR->pointR->node_cat == "row end") {
            node->node_cat = "row side";
        }
        if (node->node_cat == "row end") {
            if (node->pointL->pointL->node_cat != "row end" &&
                node->pointL->pointL->node_cat != "with row end") {
                node->pointL->node_cat = "with row end";
            } else {
                node->pointR->node_cat = "with row end";
                node->pointL->node_cat = "row side";
            }
        }
    }
}

// D:795-967. `New_Nodes[$-k]` for k past the end is a wrapped index, which D reports
// as an out-of-bounds RangeError; at_back() reproduces both the index and the text.
namespace {
PointPtr at_back(const eil::Array<PointPtr>& a, size_t k) { return a[a.length() - k]; }
} // namespace

void node_gen(eil::Slice<PointPtr> surface, size_t i, eil::Array<PointPtr>& New_Nodes) {
    PointPtr node = surface[i];

    if (node->node_cat == "row end" || node->node_cat == "with row end") {
        // no new nodes are generated
    }
    if (node->node_cat == "row side") {
        // generates 1 new node, makes 1 new cell
        double V_length =
            0.5 * (norm(node->V_L) + norm(node->V_R) / d_sin(node->interior_angle / 2));
        std::array<double, 2> node_V = {node->x, node->y};
        std::array<double, 2> unit_V = unit_leftV(node->V_R, node->interior_angle / 2);
        std::array<double, 2> node_pos = {
            node_V[0] + V_length * unit_V[0], node_V[1] + V_length * unit_V[1]};
        New_Nodes.append(make2DPoint(node_pos[0], node_pos[1]));
        at_back(New_Nodes, 1)->prev_node = node;

        if (node->pointL->node_cat == "row end") {
            makeCell(eil::Array<PointPtr>{at_back(New_Nodes, 1), node->pointL->pointL,
                                          node->pointL, node}
                         .all());
            at_back(New_Nodes, 1)->node_cat = "row end";
            at_back(New_Nodes, 1)->prev_node = node->pointL;
            if (New_Nodes.length() >= 2) {
                makeCell(eil::Array<PointPtr>{at_back(New_Nodes, 1), at_back(New_Nodes, 2),
                                              node->pointL->pointL->pointL,
                                              node->pointL->pointL}
                             .all());
            }
        } else if (node->pointL->node_cat == "with row end") {
            if (New_Nodes.length() >= 2) {
                makeCell(eil::Array<PointPtr>{at_back(New_Nodes, 2),
                                              node->pointL->pointL->pointL,
                                              node->pointL->pointL, node->pointL}
                             .all());
                at_back(New_Nodes, 2)->node_cat = "row end";
                at_back(New_Nodes, 2)->prev_node = node->pointL->pointL;
                makeCell(eil::Array<PointPtr>{at_back(New_Nodes, 1), at_back(New_Nodes, 2),
                                              node->pointL, node}
                             .all());
            }
        } else if (New_Nodes.length() >= 2) {
            makeCell(eil::Array<PointPtr>{at_back(New_Nodes, 1), at_back(New_Nodes, 2),
                                          node->pointL, node}
                         .all());
        }
    }
    if (node->node_cat == "row corner") {
        // generates 3 new nodes, creates 2 new cells
        double V_length =
            0.5 * (norm(node->V_L) + norm(node->V_R) / d_sin(node->interior_angle / 3));
        std::array<double, 2> node_V = {node->x, node->y};
        std::array<double, 2> unit_V = unit_leftV(node->V_R, 2 * node->interior_angle / 3);
        std::array<double, 2> node_pos = {
            node_V[0] + V_length * unit_V[0], node_V[1] + V_length * unit_V[1]};
        New_Nodes.append(make2DPoint(node_pos[0], node_pos[1])); // new node 1
        at_back(New_Nodes, 1)->prev_node = node;
        // SQRT2 is a Phobos `real`, so D multiplies in 80 bits and narrows on the store
        double V2_length = static_cast<double>(
            static_cast<eil::real>(1.414213562373095048801688724209698078569672L) *
            static_cast<eil::real>(V_length));
        unit_V = unit_leftV(node->V_R, node->interior_angle / 2);
        node_pos = {node_V[0] + V2_length * unit_V[0], node_V[1] + V2_length * unit_V[1]};
        New_Nodes.append(make2DPoint(node_pos[0], node_pos[1])); // new node 2
        at_back(New_Nodes, 1)->prev_node = node;
        unit_V = unit_leftV(node->V_R, node->interior_angle / 3);
        node_pos = {node_V[0] + V_length * unit_V[0], node_V[1] + V_length * unit_V[1]};
        New_Nodes.append(make2DPoint(node_pos[0], node_pos[1])); // new node 3
        at_back(New_Nodes, 1)->prev_node = node;

        if (node->pointL->node_cat == "row end") {
            makeCell(eil::Array<PointPtr>{at_back(New_Nodes, 3), at_back(New_Nodes, 4),
                                          node->pointL->pointL->pointL,
                                          node->pointL->pointL}
                         .all());
            makeCell(eil::Array<PointPtr>{at_back(New_Nodes, 3), node->pointL->pointL,
                                          node->pointL, node}
                         .all());
            at_back(New_Nodes, 3)->node_cat = "row end";
        } else if (node->pointL->node_cat == "with row end") {
            makeCell(eil::Array<PointPtr>{at_back(New_Nodes, 4), node->pointL->pointL->pointL,
                                          node->pointL->pointL, node->pointL}
                         .all());
            makeCell(eil::Array<PointPtr>{at_back(New_Nodes, 3), at_back(New_Nodes, 4),
                                          node->pointL, node}
                         .all());
            at_back(New_Nodes, 4)->node_cat = "row end";
        } else {
            // New_Nodes list should not be empty since the start node is a row side,
            // ie. New_Nodes[$-4] should be accessible
            makeCell(eil::Array<PointPtr>{at_back(New_Nodes, 3), at_back(New_Nodes, 4),
                                          node->pointL, node}
                         .all());
        }
        makeCell(eil::Array<PointPtr>{at_back(New_Nodes, 1), at_back(New_Nodes, 2),
                                      at_back(New_Nodes, 3), node}
                     .all());
    }
    if (node->node_cat == "row reversal") {
        // generates 5 new nodes, creates 3 new cells
        double V_length =
            0.5 * (norm(node->V_L) + norm(node->V_R) / d_sin(node->interior_angle / 4));
        std::array<double, 2> node_V = {node->x, node->y};
        std::array<double, 2> unit_V = unit_leftV(node->V_R, 3 * node->interior_angle / 4);
        std::array<double, 2> node_pos = {
            node_V[0] + V_length * unit_V[0], node_V[1] + V_length * unit_V[1]};
        New_Nodes.append(make2DPoint(node_pos[0], node_pos[1])); // new node 1
        at_back(New_Nodes, 1)->prev_node = node;
        double V2_length = static_cast<double>(
            static_cast<eil::real>(1.414213562373095048801688724209698078569672L) *
            static_cast<eil::real>(V_length));
        unit_V = unit_leftV(node->V_R, 5 * node->interior_angle / 8);
        node_pos = {node_V[0] + V2_length * unit_V[0], node_V[1] + V2_length * unit_V[1]};
        New_Nodes.append(make2DPoint(node_pos[0], node_pos[1])); // new node 2
        at_back(New_Nodes, 1)->prev_node = node;
        unit_V = unit_leftV(node->V_R, node->interior_angle / 2);
        node_pos = {node_V[0] + V_length * unit_V[0], node_V[1] + V_length * unit_V[1]};
        New_Nodes.append(make2DPoint(node_pos[0], node_pos[1])); // new node 3
        at_back(New_Nodes, 1)->prev_node = node;
        unit_V = unit_leftV(node->V_R, 3 * node->interior_angle / 8);
        node_pos = {node_V[0] + V2_length * unit_V[0], node_V[1] + V2_length * unit_V[1]};
        New_Nodes.append(make2DPoint(node_pos[0], node_pos[1])); // new node 4
        at_back(New_Nodes, 1)->prev_node = node;
        unit_V = unit_leftV(node->V_R, node->interior_angle / 4);
        node_pos = {node_V[0] + V_length * unit_V[0], node_V[1] + V_length * unit_V[1]};
        New_Nodes.append(make2DPoint(node_pos[0], node_pos[1])); // new node 5
        at_back(New_Nodes, 1)->prev_node = node;

        if (node->pointL->node_cat == "row end") {
            makeCell(eil::Array<PointPtr>{at_back(New_Nodes, 5), at_back(New_Nodes, 6),
                                          node->pointL->pointL->pointL,
                                          node->pointL->pointL}
                         .all());
            makeCell(eil::Array<PointPtr>{at_back(New_Nodes, 5), node->pointL->pointL,
                                          node->pointL, node}
                         .all());
            at_back(New_Nodes, 5)->node_cat = "row end";
            // row end right beside row reversal
        } else if (node->pointL->node_cat == "with row end") {
            makeCell(eil::Array<PointPtr>{at_back(New_Nodes, 6), node->pointL->pointL->pointL,
                                          node->pointL->pointL, node->pointL}
                         .all());
            makeCell(eil::Array<PointPtr>{at_back(New_Nodes, 5), at_back(New_Nodes, 6),
                                          node->pointL, node}
                         .all());
            at_back(New_Nodes, 6)->node_cat = "row end";
        } else {
            // New_Nodes list should not be empty since the start node is a row side,
            // ie. New_Nodes[$-6] should be accessible
            makeCell(eil::Array<PointPtr>{at_back(New_Nodes, 5), at_back(New_Nodes, 6),
                                          node->pointL, node}
                         .all());
        }
        makeCell(eil::Array<PointPtr>{at_back(New_Nodes, 3), at_back(New_Nodes, 4),
                                      at_back(New_Nodes, 5), node}
                     .all());
        makeCell(eil::Array<PointPtr>{at_back(New_Nodes, 1), at_back(New_Nodes, 2),
                                      at_back(New_Nodes, 3), node}
                     .all());
    }
}

// D:970-1014
eil::Array<PointPtr> generate_new_row(eil::Slice<PointPtr> surface) {
    eil::Array<PointPtr> New_Nodes;
    size_t n = surface.length();

    // the start node will be the second consecutive side node
    PointPtr start_node;
    for (size_t t = 0; t < surface.length(); ++t) {
        PointPtr node = surface[t];
        if (node->node_cat == "row side" && node->pointL->node_cat == "row side") {
            start_node = node;
            break;
        }
    }
    if (start_node == nullptr) {
        EIL_THROW_GEOM("there are not 2 consecutive side nodes in this row");
    }

    // construct order from start node
    for (size_t i = start_node->rowID; i < n; ++i) {
        node_gen(surface, i, New_Nodes);
    }
    for (size_t i = 0; i < start_node->rowID; ++i) {
        node_gen(surface, i, New_Nodes);
    }

    // make final cell (known to be this case because of the second side node start
    // condition)
    makeCell(eil::Array<PointPtr>{surface[start_node->rowID], New_Nodes[0],
                                  at_back(New_Nodes, 1), start_node->pointL}
                 .all());

    // assigning row IDs to the nodes in the new row
    size_t rowID = eil::d_init<size_t>();
    for (size_t t = 0; t < New_Nodes.length(); ++t) {
        New_Nodes[t]->store_rowID(rowID);
        ++rowID;
    }

    return New_Nodes;
}

// D:1016-1035
double summed_squareness(PointPtr node) {
    int cell_count = eil::d_init<int>();
    double summed_squareness = 0;
    eil::Array<size_t> ID_list;
    for (size_t t = 0; t < CELL_LIST.length(); ++t) {
        CellPtr cell = CELL_LIST[t];
        ID_list = cell->point_IDs;
        bool found = false;
        for (size_t k = 0; k < ID_list.length(); ++k) {
            if (ID_list[k] == node->ID) found = true;
        }
        if (found) {
            ++cell_count;
            summed_squareness += squareness(cell);
        }
    }
    eil_assert(cell_count != 0);
    summed_squareness /= cell_count;
    return summed_squareness;
}

// D:1039-1113. Prev_Row is a dead parameter in D's body — it is never read — which is
// why the ported copy/alias difference between D's `New_Row = Prev_Row` and C++'s
// array copy cannot be observed.
double smoothing(eil::Slice<PointPtr> surface, eil::Slice<PointPtr> Prev_Row, bool boundary,
                 bool first_row) {
    (void)Prev_Row;
    double move_length = eil::d_init<double>();
    double biggest_move = 0;
    PointPtr attached_point;
    double x = eil::d_init<double>();
    double y = eil::d_init<double>();
    double prev_squareness = eil::d_init<double>();
    double squareness = eil::d_init<double>(); // D's local shadows the function

    for (size_t t = 0; t < surface.length(); ++t) {
        PointPtr node = surface[t];
        if ((boundary == 1 && node->node_cat != "row corner" &&
             node->node_cat != "row reversal") ||
            boundary == 0) {
            eil::Array<std::array<double, 2>> vectors;
            for (size_t u = 0; u < FACE_LIST.length(); ++u) {
                UFacePtr face = FACE_LIST[u];
                eil::Array<size_t> ID_list;
                ID_list.append(face->point_IDs[0]);
                ID_list.append(face->point_IDs[1]);
                bool found = false;
                for (size_t k = 0; k < ID_list.length(); ++k) {
                    if (ID_list[k] == node->ID) found = true;
                }
                if (found) {
                    if (node->ID == face->point_IDs[0]) {
                        attached_point = POINT_LIST[face->point_IDs[1]];
                    } else {
                        attached_point = POINT_LIST[face->point_IDs[0]];
                    }
                    vectors.append(std::array<double, 2>{attached_point->x - node->x,
                                                         attached_point->y - node->y});
                }
            }

            if (vectors.length() == 0) {
                eil::write("apparently this node has no friends! \n");
                eil::writeln(node->print());
            }

            if (vectors.length() != 0) {
                std::array<double, 2> movement = vectors[0];
                for (unsigned int i = 1; i < vectors.length(); ++i) {
                    movement[0] += vectors[i][0];
                    movement[1] += vectors[i][1];
                }
                double div = 2 * static_cast<int>(vectors.length());
                movement[0] /= div;
                movement[1] /= div;
                move_length = norm(movement);
                if (move_length > biggest_move) biggest_move = move_length;

                if (first_row == 0) {
                    double x_move = movement[0] / 5.;
                    double y_move = movement[1] / 5.;
                    for (int i = 0; i < 5; ++i) {
                        prev_squareness = summed_squareness(node);
                        x = node->x;
                        y = node->y;
                        node->x += x_move;
                        node->y += y_move;
                        squareness = summed_squareness(node);
                        if (squareness < prev_squareness) {
                            node->x = x;
                            node->y = y;
                            break;
                        }
                    }
                } else {
                    node->x += movement[0];
                    node->y += movement[1];
                }
            }
        }
    }

    return biggest_move;
}

// D:1115-1260
eil::Array<PointPtr> seam(PointPtr start, eil::Array<PointPtr>& New_Row,
                          eil::Array<size_t>& adjust_IDs, size_t& original_nodes_left,
                          bool closing) {
    eil::Array<PointPtr> New_Nodes;
    eil::Array<PointPtr> empty;
    eil::Array<PointPtr> inner_nodes;
    (void)empty;
    // special case - only 1 cell remaining to close
    if (closing == 1 && New_Row.length() == 4) {
        makeCell(eil::Array<PointPtr>{start, start->pointR, start->pointR->pointR,
                                      start->pointL}
                     .all());
        eil::writeln("CLOSED (special)");
        return New_Nodes;
    }

    double midx = (start->pointL->x + start->pointR->x) / 2;
    double midy = (start->pointL->y + start->pointR->y) / 2;
    New_Nodes.append(make2DPoint(midx, midy));
    at_back(New_Nodes, 1)->first_seam_flag = 1;
    original_nodes_left -= 3;
    makeCell(eil::Array<PointPtr>{New_Nodes[0], start->pointL, start, start->pointR}.all());

    PointPtr nextpointL = start->pointL;
    PointPtr nextpointR = start->pointR;

    std::array<double, 2> a = vector2D(nextpointL, nextpointL->pointL);
    std::array<double, 2> b = vector2D(nextpointR, nextpointR->pointR);
    double theta = angle_between_vectors(a, b);
    int loop = eil::d_init<int>();
    (void)loop;

    auto conditions = [&](double theta, const PointPtr& nextpointL,
                          const PointPtr& nextpointR, size_t original_nodes_left) {
        bool conditions = eil::d_init<bool>();
        if (closing == 0) { // not doing the closing seam
            bool adjL = false, adjR = false;
            for (size_t k = 0; k < adjust_IDs.length(); ++k) {
                if (adjust_IDs[k] == nextpointL->rowID) adjL = true;
                if (adjust_IDs[k] == nextpointR->rowID) adjR = true;
            }
            if (static_cast<eil::real>(theta) < EIL_PI / 2.0L && original_nodes_left > 2 &&
                point_dist(start, nextpointL) < 2 * point_dist(start, nextpointR) &&
                point_dist(start, nextpointR) < 2 * point_dist(start, nextpointL) && !adjL &&
                !adjR) {
                conditions = 1;
            } else {
                eil::writefln("there are %s remaining nodes \n (not closing seam)",
                              original_nodes_left);
                conditions = 0;
            }
        } else if (closing == 1) {
            if (original_nodes_left > 2) {
                conditions = 1;
            } else {
                eil::writeln("less than 2 remaining nodes");
                eil::writefln("there are %s remaining nodes", original_nodes_left);
                conditions = 0;
            }
        }
        return conditions;
    };

    int ind = 2; // reference index for joining (complicated)
    int nodes = 1;
    int Flag = 1;
    while (conditions(theta, nextpointL, nextpointR, original_nodes_left)) {
        ++loop;
        double avr_length = (norm(a) + norm(b)) / 2;
        if (point_dist(nextpointL->pointL, nextpointR->pointR) < 3 * avr_length) {
            if (Flag == 1) {
                midx = (nextpointL->pointL->x + nextpointR->pointR->x) / 2;
                midy = (nextpointL->pointL->y + nextpointR->pointR->y) / 2;
                New_Nodes.append(make2DPoint(midx, midy));
                original_nodes_left -= 2;
                makeCell(eil::Array<PointPtr>{at_back(New_Nodes, 1), nextpointL->pointL,
                                              nextpointL, at_back(New_Nodes, ind)}
                             .all());
                makeCell(eil::Array<PointPtr>{at_back(New_Nodes, 1), at_back(New_Nodes, ind),
                                              nextpointR, nextpointR->pointR}
                             .all());
                ind = 2;
                nodes = 1;
            } else if (Flag == 2) {
                ind = 4;
                makeCell(eil::Array<PointPtr>{at_back(New_Nodes, 3), nextpointL->pointL,
                                              nextpointL, at_back(New_Nodes, 2)}
                             .all());
                makeCell(eil::Array<PointPtr>{at_back(New_Nodes, 3), at_back(New_Nodes, 1),
                                              nextpointR, nextpointR->pointR}
                             .all());
                original_nodes_left -= 2;
                nodes = 0;
            }
            Flag = 1;
        } else if (closing == 1 ||
                   point_dist(nextpointL->pointL, nextpointR->pointR) < 5 * avr_length) {
            midx = (nextpointL->pointL->x + nextpointR->pointR->x) / 2;
            midy = (nextpointL->pointL->y + nextpointR->pointR->y) / 2;
            New_Nodes.append(make2DPoint(midx, midy));
            midx = (nextpointL->pointL->x + at_back(New_Nodes, 1)->x) / 2;
            midy = (nextpointL->pointL->y + at_back(New_Nodes, 1)->y) / 2;
            New_Nodes.append(make2DPoint(midx, midy));
            midx = (at_back(New_Nodes, 2)->x + nextpointR->pointR->x) / 2;
            midy = (at_back(New_Nodes, 2)->y + nextpointR->pointR->y) / 2;
            New_Nodes.append(make2DPoint(midx, midy));
            original_nodes_left -= 2;
            if (Flag == 1) {
                if (nodes == 0) {
                    ind = 6;
                } else {
                    ind = 4;
                }
                makeCell(eil::Array<PointPtr>{at_back(New_Nodes, 3), at_back(New_Nodes, 2),
                                              at_back(New_Nodes, ind), at_back(New_Nodes, 1)}
                             .all());
                makeCell(eil::Array<PointPtr>{at_back(New_Nodes, 2), nextpointL->pointL,
                                              nextpointL, at_back(New_Nodes, ind)}
                             .all());
                makeCell(eil::Array<PointPtr>{at_back(New_Nodes, 1), at_back(New_Nodes, ind),
                                              nextpointR, nextpointR->pointR}
                             .all());
                ind = 4;
            } else if (Flag == 2) {
                makeCell(eil::Array<PointPtr>{at_back(New_Nodes, 2), nextpointL->pointL,
                                              nextpointL, at_back(New_Nodes, 5)}
                             .all());
                makeCell(eil::Array<PointPtr>{at_back(New_Nodes, 3), at_back(New_Nodes, 2),
                                              at_back(New_Nodes, 5), at_back(New_Nodes, 6)}
                             .all());
                makeCell(eil::Array<PointPtr>{at_back(New_Nodes, 1), at_back(New_Nodes, 3),
                                              at_back(New_Nodes, 6), at_back(New_Nodes, 4)}
                             .all());
                makeCell(eil::Array<PointPtr>{at_back(New_Nodes, 1), at_back(New_Nodes, 4),
                                              nextpointR, nextpointR->pointR}
                             .all());
            }

            Flag = 2;
            nodes = 3;
        } else {
            eil::writeln("more than 5 cell spaces between joining nodes");
            if (closing == 0) {
                break;
            } else {
                // Unreachable in D as written: this else arm needs closing == 1, but
                // the enclosing arm's test already took every closing == 1 case.
                throw eil::ErrorBase(
                    "cannot terminate at >5 node seam for closing procedure - try "
                    "recursion for closing");
            }
        }

        nextpointL = nextpointL->pointL;
        nextpointR = nextpointR->pointR;
        a = vector2D(nextpointL, nextpointL->pointL);
        b = vector2D(nextpointR, nextpointR->pointR);
        theta = angle_between_vectors(a, b);
    }
    if (nodes == 1) {
        inner_nodes.append(at_back(New_Nodes, 1));
    } else if (nodes == 0) {
        inner_nodes.append(at_back(New_Nodes, 3));
    } else if (nodes == 3) {
        inner_nodes = eil::Array<PointPtr>{at_back(New_Nodes, 2), at_back(New_Nodes, 3),
                                           at_back(New_Nodes, 1)};
    }

    if (original_nodes_left == 1 && closing == 1) {
        eil::writeln("closing final cells");
        if (Flag == 1) {
            makeCell(eil::Array<PointPtr>{nextpointR->pointR, nextpointL,
                                          at_back(New_Nodes, ind - 1), nextpointR}
                         .all());
            eil::writeln("CLOSED1");
        } else if (Flag == 2) {
            makeCell(eil::Array<PointPtr>{nextpointR->pointR, nextpointL,
                                          at_back(New_Nodes, 2), at_back(New_Nodes, 3)}
                         .all());
            makeCell(eil::Array<PointPtr>{nextpointR->pointR, at_back(New_Nodes, 3),
                                          at_back(New_Nodes, 1), nextpointR}
                         .all());
            eil::writeln("CLOSED2");
        }
    }

    adjust_IDs.append(nextpointL->rowID);
    adjust_IDs.append(nextpointR->rowID);
    return inner_nodes;
}

// D:1262-1280
void closing_seam(eil::Slice<PointPtr> Final_Row) {
    eil::Array<size_t> adjust_IDs;
    // don't actually use this, but I think it needs to exist in this scope to call
    // the function here.
    ::eil::d_assert_msg(Final_Row.length() % 2 == 0,
                        "final boundary not even number of nodes - this is not ok... and "
                        "shouldn't have happened",
                        __FILE__, __LINE__);
    PointPtr start = Final_Row[0];
    for (size_t t = 0; t < Final_Row.length(); ++t) {
        PointPtr node = Final_Row[t];
        if (node->interior_angle < start->interior_angle) {
            start = node;
        }
    }
    size_t nodes_left = Final_Row.length();
    // D hands its `Point[] Final_Row` parameter straight to seam's `ref Point[]`; seam
    // only ever reads New_Row.length(), so the local copy here cannot diverge.
    eil::Array<PointPtr> final_row = Final_Row;
    seam(start, final_row, adjust_IDs, nodes_left, 1);
}

// D:1283-1301
void fix_IDs(eil::Slice<UFacePtr> face_list, eil::Slice<CellPtr> cell_list) {
    size_t index = 0;
    for (size_t t = 0; t < face_list.length(); ++t) {
        face_list[t]->face_ID = index;
        ++index;
    }
    index = 0;
    for (size_t t = 0; t < cell_list.length(); ++t) {
        cell_list[t]->cell_ID = index;
        ++index;
    }
}

// D:1304-1322
double biggest_cell(eil::Slice<CellPtr> cells, double initial_size) {
    double biggest_area = eil::pow(initial_size, 2.0);
    double current_area = eil::d_init<double>();
    for (size_t t = 0; t < cells.length(); ++t) {
        CellPtr cell = cells[t];
        current_area = calc_area(eil::Array<PointPtr>{
            POINT_LIST[cell->point_IDs[0]], POINT_LIST[cell->point_IDs[1]],
            POINT_LIST[cell->point_IDs[2]], POINT_LIST[cell->point_IDs[3]]}.all());
        if (current_area > biggest_area) {
            biggest_area = current_area;
        }
    }
    return biggest_area;
}

// D:1324-1360
void even_out_size(eil::Slice<PointPtr> surface) {
    size_t n = surface.length();
    PointPtr start = surface[0];
    double biggest = point_dist(start, start->pointL) + point_dist(start, start->pointR);
    double size = eil::d_init<double>();
    for (size_t t = 0; t < surface.length(); ++t) {
        PointPtr node = surface[t];
        size = point_dist(node, node->pointL) + point_dist(node, node->pointR);
        if (size > biggest) {
            biggest = size;
            start = node;
        }
    }

    PointPtr currentL = start;
    PointPtr currentR = start;
    for (int i = 0; i < static_cast<int>(n / 2 + 2); ++i) {
        if (currentL->pointL->node_cat != "row corner" &&
            currentL->pointL->node_cat != "row reversal") {
            currentL->pointL->x =
                (midpoint(currentL, currentL->pointL->pointL)->x + currentL->pointL->x) / 2;
            currentL->pointL->y =
                (midpoint(currentL, currentL->pointL->pointL)->y + currentL->pointL->y) / 2;
        }
        if (currentR->pointR->node_cat != "row corner" &&
            currentR->pointR->node_cat != "row reversal") {
            currentR->pointR->x =
                (midpoint(currentR, currentR->pointR->pointR)->x + currentR->pointR->x) / 2;
            currentR->pointR->y =
                (midpoint(currentR, currentR->pointR->pointR)->y + currentR->pointR->y) / 2;
        }
        currentL = currentL->pointL;
        currentR = currentR->pointR;
    }
}

// D:1362-1369
void point_ID_check() {
    size_t npoints = POINT_LIST.length();
    for (size_t i = 0; i < npoints; ++i) {
        if (POINT_LIST[i]->ID != i) {
            throw eil::ErrorBase("Point ID does not match position in POINT_LIST");
        }
    }
}

//=========================================================== Section 5 (D:1375-1592)

// D:1375-1527
void pave_rows_until_intersection(eil::Array<PointPtr>& New_Row,
                                  eil::Array<PointPtr>& Prev_Row,
                                  size_t npoints_boundary, bool& seam_next) {
    double biggest_move = eil::d_init<double>();
    double move = eil::d_init<double>();
    eil::Array<PointPtr> inside_nodes;

    size_t npoints_row = eil::d_init<size_t>();
    size_t npoints_new_row = eil::d_init<size_t>();

    eil::Array<std::array<PointPtr, 4>> intersections;
    int loop_counter = eil::d_init<int>();
    double remaining_area = eil::d_init<double>();
    bool side_node_Flag = 0;
    bool inside_prev = eil::d_init<bool>();

    eil::Array<PointPtr> point_list_backup;
    size_t point_count_backup = eil::d_init<size_t>();
    eil::Array<UFacePtr> face_list_backup;
    size_t face_count_backup = eil::d_init<size_t>();
    eil::Array<CellPtr> cell_list_backup;
    size_t cell_count_backup = eil::d_init<size_t>();

    size_t cell_index = CELL_LIST.length();

    eil::Array<PointPtr> second_prev_row = Prev_Row;

    while (true) {
        ++loop_counter;
        point_list_backup = POINT_LIST;
        point_count_backup = Point::ID_count;
        face_list_backup = FACE_LIST;
        face_count_backup = UFace::face_ID_count;
        cell_list_backup = CELL_LIST;
        cell_count_backup = Cell::cell_ID_count;

        try {
            New_Row = generate_new_row(New_Row.all());
        } catch (const eil::ExceptionBase& exc) {
            // D catches Exception here, so an Error still propagates.
            eil::writeln("caught the side node exception");
            side_node_Flag = 1;
        }
        npoints_row = New_Row.length();
        eil::writefln("there are %s points in the new paving row", npoints_row);
        assign_node_properties(New_Row.all());
        adjust_row_ends(New_Row.all(), eil::geom::initial_size, 0);
        point_ID_check();

        cell_index = CELL_LIST.length();
        npoints_new_row = New_Row.length();

        // smoothing process:
        biggest_move = smoothing(New_Row.all(), Prev_Row.all(), 1, 0);
        move = biggest_move;
        unsigned int reverse_count = 0;
        for (int i = 0; i < 4; ++i) {
            std::reverse(New_Row.begin(), New_Row.end());
            move = smoothing(New_Row.all(), Prev_Row.all(), 1, 0);
            ++reverse_count;
        }
        if (reverse_count % 2 != 0) {
            std::reverse(New_Row.begin(), New_Row.end());
        }

        remaining_area = clockwise_check(New_Row.all());
        eil::Array<std::array<PointPtr, 4>> by_faces = intersecting_faces(New_Row.all());
        eil::Array<std::array<PointPtr, 4>> by_bounds =
            intersecting_boundary(New_Row.all());
        intersections = by_faces.concat(by_bounds.all());
        inside_prev = inside_prev_row_check(New_Row.all(), Prev_Row.all());

        if (remaining_area > 0 || static_cast<int>(intersections.length()) > 0 ||
            New_Row.length() <= 6 || side_node_Flag == 1) {
            POINT_LIST = point_list_backup;
            FACE_LIST = face_list_backup;
            CELL_LIST = cell_list_backup;
            Point::ID_count = point_count_backup;
            UFace::face_ID_count = face_count_backup;
            Cell::cell_ID_count = cell_count_backup;
            New_Row = Prev_Row;
            break;
        }
        // breaks loop at first case of new row intersecting itself, or if the new row
        // completely inverts, or has 6 or less nodes remaining, or if there are no
        // longer 2 consecutive side nodes, or if a point of the new row is outside the
        // previous row

        // smooth all the inside nodes, forwards then backwards to improve symmetry
        inside_nodes = POINT_LIST.slice(npoints_boundary, New_Row[0]->ID);
        move = biggest_move;
        std::reverse(New_Row.begin(), New_Row.end());
        reverse_count = 1;
        for (int i = 0; i < 1; ++i) {
            move = smoothing(inside_nodes.all(), Prev_Row.all(), 0, 1);
            std::reverse(New_Row.begin(), New_Row.end());
            ++reverse_count;
        }
        if (reverse_count % 2 != 0) {
            std::reverse(New_Row.begin(), New_Row.end());
        }

        // even out size function:
        even_out_size(New_Row.all());

        // final smooth
        move = biggest_move;
        reverse_count = 0;
        for (int i = 0; i < 2; ++i) {
            move = smoothing(New_Row.all(), Prev_Row.all(), 1, 0);
            std::reverse(New_Row.begin(), New_Row.end());
            ++reverse_count;
        }
        if (reverse_count % 2 != 0) {
            std::reverse(New_Row.begin(), New_Row.end());
        }

        even_out_size(New_Row.all());

        second_prev_row = Prev_Row;
        Prev_Row = New_Row;
    } // end: row generation loop

    move = biggest_move;
    unsigned int reverse_count = 0;

    if (loop_counter > 1) {
        for (int i = 0; i < 2; ++i) {
            move = smoothing(New_Row.all(), second_prev_row.all(), 1, 0);
            std::reverse(New_Row.begin(), New_Row.end());
            ++reverse_count;
        }
        if (reverse_count % 2 != 0) {
            std::reverse(New_Row.begin(), New_Row.end());
        }
    }

    npoints_row = New_Row.length();
    std::string reason;
    // if seam_next == 0 the code proceeds to the closing procedure, otherwise the
    // regular seaming procedure is commenced
    if (static_cast<int>(intersections.length()) > 0) {
        seam_next = 1;
        reason = "intersection detected";
        if (inside_prev == 0) {
            reason += " because a point in the new row was outside the previous row";
        }
    } else if (remaining_area > 0) {
        seam_next = 0;
        reason = "the new paving row fully inverted";
    }

    if (npoints_new_row <= 6) {
        seam_next = 0;
        reason = "there were 6 or less nodes in the new paving row";
    } else if (side_node_Flag == 1) {
        seam_next = 0;
        reason = "there are not 2 consecutive side nodes in the paving boundary";
    }

    eil::writefln("reverted back to %s points in the new paving row because: \n %s",
                  npoints_row, reason);

    assign_node_properties(New_Row.all());
    adjust_row_ends(New_Row.all(), eil::geom::initial_size, 0);
    point_ID_check();
    Prev_Row = New_Row;
}

// D:1530-1592
void seaming_procedure(eil::Array<PointPtr>& New_Row) {
    eil::Array<eil::Array<PointPtr>> inner_nodes;
    eil::Array<size_t> adjust_IDs;
    eil::Array<PointPtr> revised_New_Row;
    size_t original_nodes_left = New_Row.length();
    double smallest_angle = static_cast<double>(2.0 * EIL_PI);
    PointPtr smallest_angle_node;
    for (size_t t = 0; t < New_Row.length(); ++t) {
        PointPtr node = New_Row[t];
        if (node->interior_angle < smallest_angle && node->first_seam_flag == 0) {
            smallest_angle = node->interior_angle;
            smallest_angle_node = node;
        }
        bool adj = false;
        for (size_t k = 0; k < adjust_IDs.length(); ++k) {
            if (adjust_IDs[k] == node->rowID) adj = true;
        }
        if (static_cast<eil::real>(node->interior_angle) < EIL_PI / 2.0L &&
            original_nodes_left > 2 && !adj && node->first_seam_flag == 0) {
            inner_nodes.append(seam(node, New_Row, adjust_IDs, original_nodes_left, 0));
        }
    }
    if (inner_nodes.length() == 0 && smallest_angle_node != nullptr) {
        eil::writeln("seaming from smallest angle node:");
        inner_nodes.append(
            seam(smallest_angle_node, New_Row, adjust_IDs, original_nodes_left, 0));
    }

    if (inner_nodes.length() > 0) {
        for (size_t i = 0; i < inner_nodes.length(); ++i) {
            revised_New_Row.append(New_Row[adjust_IDs[2 * i]]);
            revised_New_Row.append_slice(inner_nodes[i].all());
            size_t row_ID = adjust_IDs[2 * i + 1];
            revised_New_Row.append(New_Row[row_ID]);
            row_ID = New_Row[row_ID]->pointR->rowID;
            for (;;) {
                bool adj = false;
                for (size_t k = 0; k < adjust_IDs.length(); ++k) {
                    if (adjust_IDs[k] == row_ID) adj = true;
                }
                if (adj) break;
                revised_New_Row.append(New_Row[row_ID]);
                row_ID = New_Row[row_ID]->pointR->rowID;
            }
        }

        New_Row = revised_New_Row;
        size_t npoints_row = New_Row.length();
        eil::writefln("there are %s points in the new paving row after seaming process",
                      npoints_row);
        // reset row IDs and classifications:
        size_t rowID = 0;
        for (size_t t = 0; t < New_Row.length(); ++t) {
            PointPtr node = New_Row[t];
            node->store_rowID(rowID);
            if (node->node_cat != "row end") {
                node->node_cat = "none";
            }
            ++rowID;
        }

        assign_node_properties(New_Row.all());
        adjust_row_ends(New_Row.all(), eil::geom::initial_size, 1);

        for (size_t t = 0; t < New_Row.length(); ++t) {
            PointPtr node = New_Row[t];
            if (node->first_seam_flag == 1 && node->pointL->first_seam_flag == 1 &&
                node->pointR->first_seam_flag == 1 &&
                node->pointL->pointL->first_seam_flag == 1 &&
                node->pointR->pointR->first_seam_flag == 1) {
                node->first_seam_flag = 0;
            }
        }

        clockwise_check(New_Row.all());
    }
}

//=========================================================== Section 6 (D:1598-1731)

// D:1598-1731. `class PavedGrid` has NO base class in D — see the header note.
PavedGrid::PavedGrid(const eil::Array<eil::Array<double>>& boundary_points) {
    //---------------import the boundary geometry---------------------
    eil::writeln("importing external boundary...");
    for (size_t t = 0; t < boundary_points.length(); ++t) {
        const eil::Array<double>& p = boundary_points[t];
        make2DPoint(p[0], p[1]);
    }
    for (size_t t = 0; t < POINT_LIST.length(); ++t) {
        PointPtr point = POINT_LIST[t];
        point->store_rowID(point->ID);
    }

    eil::Array<PointPtr> external_boundary = POINT_LIST;
    size_t npoints_boundary = external_boundary.length();
    eil::writefln("there are %s points in the external boundary", npoints_boundary);
    if (npoints_boundary % 2 != 0) {
        throw eil::ErrorBase("imported boundary MUST contain an even number of nodes");
    }

    //----calculate the node information for the exterior boundary----
    assign_node_properties(external_boundary.all());
    adjust_row_ends(external_boundary.all(), eil::geom::initial_size, 1);

    for (size_t t = 0; t < external_boundary.length(); ++t) {
        PointPtr point = external_boundary[t];
        makeFace(point->ID, point->pointR->ID);
    }
    // make faces first, in set order, to match boundary conditions

    //----check the imported boundary is: anti-clockwise, closed, non-intersecting----
    if (clockwise_check(external_boundary.all()) > 0) {
        throw eil::ErrorBase("imported external boundary must be anti-clockwise");
    } else {
        eil::writeln("boundary is anti-clockwise and closed - good");
    }

    if (static_cast<int>(intersecting_boundary(external_boundary.all()).length()) > 0) {
        throw eil::ErrorBase("imported boundary intersects itself - not allowed");
    } else {
        eil::writeln("boundary does not self-intersect - good");
    }

    //--------calculate initial properties for later use------------
    eil::geom::initial_size = average_face_length(external_boundary.all());
    eil::geom::avr_area = eil::pow(eil::geom::initial_size, 2.0);

    //---------------generate the first new row---------------------
    eil::writeln("generating first new row...");
    eil::Array<PointPtr> New_Row = generate_new_row(external_boundary.all());
    size_t npoints_row = New_Row.length();
    eil::writefln("there are %s points in the first new paving row", npoints_row);
    assign_node_properties(New_Row.all());
    eil::Array<PointPtr> Prev_Row = New_Row;
    smoothing(New_Row.all(), Prev_Row.all(), 1, 0);
    even_out_size(New_Row.all());

    // search for special case of first row intersections and adjust if necessary
    eil::Array<std::array<PointPtr, 4>> intersections =
        intersecting_boundary(New_Row.all());
    bool effective_intersection = 0;
    for (size_t t = 0; t < New_Row.length(); ++t) {
        PointPtr p = New_Row[t];
        if (p->interior_angle > 6.25 || p->interior_angle < 0.03) {
            effective_intersection = 1;
        }
    }
    if (intersections.length() != 0 || effective_intersection == 1) {
        eil::writeln("first new row intersected itself ... ");
        smoothing(New_Row.all(), Prev_Row.all(), 1, 1);
        smoothing(New_Row.all(), Prev_Row.all(), 1, 0);
    }
    assign_node_properties(New_Row.all());
    adjust_row_ends(New_Row.all(), eil::geom::initial_size, 0);

    //----continue generating/seaming rows until closing conditions are reached----
    bool seam_next = 1;
    int loop_counter = eil::d_init<int>();
    while (true) {
        ++loop_counter;
        eil::writeln("paving new rows until intersection...");
        pave_rows_until_intersection(New_Row, Prev_Row, npoints_boundary, seam_next);
        if (seam_next == 0) {
            break;
        }
        smoothing(New_Row.all(), Prev_Row.all(), 1, 0);
        eil::writeln("commencing seaming procedure...");
        seaming_procedure(New_Row);
        Prev_Row = New_Row;
        eil::writeln("...completed seam.");
        even_out_size(New_Row.all());
    }

    eil::writeln("smoothing before closing");
    eil::Array<PointPtr> inside_nodes = POINT_LIST.from(npoints_boundary);
    double biggest_move = smoothing(inside_nodes.all(), Prev_Row.all(), 0, 1);
    double move = biggest_move;
    for (int i = 0; i < 1; ++i) {
        move = smoothing(inside_nodes.all(), Prev_Row.all(), 0, 0);
    }

    //---------------close the final region-------------------------
    eil::writeln("commencing closing seam...");
    closing_seam(New_Row.all());

    eil::writeln("completing final smoothing...");
    // smooth all the inside nodes:
    inside_nodes = POINT_LIST.from(npoints_boundary);
    biggest_move = smoothing(inside_nodes.all(), Prev_Row.all(), 0, 1);
    move = biggest_move;
    for (int i = 0; i < 2; ++i) {
        move = smoothing(inside_nodes.all(), Prev_Row.all(), 0, 0);
    }
    (void)move;
    (void)move; // D assigns this in the smoothing loops and never reads it back
    eil::writeln("DONE.");
    eil::write("total cell count: ");
    eil::writeln(CELL_LIST.length());

    //-------section for size adjustments: not currently included----

    this->nvertices = POINT_LIST.length();
    this->nfaces = FACE_LIST.length();
    this->ncells = CELL_LIST.length();
    this->nboundaries = npoints_boundary;
} // end: of constructor

} // namespace geom
} // namespace eil
