// tests/test_geom_elements_vector3.cpp
//
// Direct port of the `unittest` block in geom/elements/vector3.d:706-792. Every
// assertion is kept, in the original order, with the original tolerance arguments
// (approxEqualVectors defaults are 1.0e-2 relative / 1.0e-5 absolute).
//
// The last three groups are the ones that catch translation errors:
//   * `Vector3 d2 = a` must be a *value* copy (D struct semantics), so the
//     mutation of a.y after the copy leaves d2 at its old value.
//   * rotate_about_zaxis(PI/4) is called with D's 80-bit PI, so the argument is
//     computed wide and rounded on the way in.
//   * the 2D transform_to_{local,global}_frame overloads set z = 0.0 explicitly.

#include <cmath>
#include "eil/math.hpp"
#include <cstdio>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "geom/elements/vector3.hpp"

using eil::geom::Vector3;
using eil::number;

namespace {

int failures = 0;
int checks = 0;

void check(bool cond, const char* what, int line) {
    ++checks;
    if (!cond) {
        ++failures;
        std::printf("FAIL line %d: %s\n", line, what);
    }
}

#define CHECK(c, msg) check((c), msg, __LINE__)

} // namespace

int main() {
    // Check that we have separate data with the correct values.
    Vector3 a = Vector3(eil::Array<number>{1.0, 2.2, 3.0}.all());
    Vector3 b = Vector3(1.0);
    CHECK(a.x == 1.0, "a.x");
    CHECK(a.y == 2.2, "a.y");
    CHECK(a.z == 3.0, "a.z");
    CHECK(a.x == b.x, "a.x == b.x");
    CHECK(b.y == 0.0, "b.y == 0");
    CHECK(b.z == 0.0, "b.z == 0");
    b.set(a);
    CHECK(a.x == b.x && a.y == b.y && a.z == b.z, "b.set(a)");
    b.set(1.0, 0.0, 0.0);
    CHECK(b.x == 1.0 && b.y == 0.0 && b.z == 0.0, "b.set(1,0,0)");

    // Check operators
    b = -a;
    CHECK(b.x == -a.x && b.y == -a.y && b.z == -a.z, "b = -a");

    b = Vector3(1.0);
    Vector3 c = a + b;
    CHECK(c.y == a.y + b.y, "c = a + b");
    c = a - b;
    CHECK(c.y == a.y - b.y, "c = a - b");
    Vector3 d = a.dup();
    a.y = 99.0;
    CHECK(a.y == 99.0 && d.y == 2.2, "dup() is independent");
    Vector3 d2 = a;
    a.y = 3.3;
    CHECK(a.y == 3.3 && d2.y == 99.0, "copy is independent");

    Vector3 e = a * 2.0;
    Vector3 f = 3.0 * d;
    CHECK(e.z == 6.0 && f.z == 9.0, "scalar multiply both sides");
    Vector3 g = d / 3.0;
    CHECK(g.z == 1.0, "g = d/3");

    g += f;
    CHECK(g.z == 10.0, "g += f");
    g /= 2.0;
    CHECK(g.z == 5.0, "g /= 2");

    a = Vector3(1.0, 0.0, 0.0);
    a.rotate_about_zaxis(static_cast<double>(EIL_PI / 4.0L));
    CHECK(eil::geom::approxEqualVectors(a, Vector3(0.7071, 0.7071, 0)), "rotate_about_zaxis(PI/4)");

    a = Vector3(1.0, 0.0, 0.0);
    Vector3 point = Vector3(0.0, 1.0, 0.0);
    Vector3 normal = Vector3(0.0, 1.0, 0.0);
    a.mirror_image(point, normal);
    CHECK(eil::geom::approxEqualVectors(a, Vector3(1.0, 2.0, 0)), "mirror_image");

    Vector3 u = eil::geom::unit(g);
    CHECK(eil::nm::approxEqualNumbers(eil::geom::abs(u), number(1.0)), "unit(g) magnitude");

    Vector3 x = Vector3(1.0, 0.0, 0.0);
    Vector3 y = Vector3(0.0, 1.0, 0.0);
    Vector3 z = eil::geom::cross(x, y);
    Vector3 zref = Vector3(0.0, 0.0, 1.0);
    CHECK(eil::geom::approxEqualVectors(z, zref), "cross(x,y)");

    Vector3 n = Vector3(1.0, 1.0, 0.0);
    n = eil::geom::unit(n);
    Vector3 t1 = Vector3(-1.0, 1.0, 0.0);
    t1 = eil::geom::unit(t1);
    Vector3 t2 = eil::geom::cross(n, t1);
    Vector3 h = Vector3(1.0, 0.0, 1.0);
    Vector3 h_ref = Vector3(h);
    h.transform_to_local_frame(n, t1, t2);
    CHECK(eil::geom::approxEqualVectors(h, Vector3(eil::sqrt(1.0 / 2.0), -eil::sqrt(1.0 / 2.0), 1.0)),
          "3D local frame");
    h.transform_to_global_frame(n, t1, t2);
    CHECK(eil::geom::approxEqualVectors(h, h_ref), "3D global frame round trip");

    // 2D variants, starting with fresh values.
    n.set(1.0, 1.0);
    n = eil::geom::unit(n);
    t1.set(-1.0, 1.0);
    t1 = eil::geom::unit(t1);
    h_ref.set(1.0, 0.0);
    h.set(h_ref);
    h.transform_to_local_frame(n, t1);
    CHECK(eil::geom::approxEqualVectors(h, Vector3(eil::sqrt(1.0 / 2.0), -eil::sqrt(1.0 / 2.0))),
          "2D local frame");
    h.transform_to_global_frame(n, t1);
    CHECK(eil::geom::approxEqualVectors(h, h_ref), "2D global frame round trip");

    // D writes cos(to!number(PI)/4): the 80-bit PI is rounded to a double *first*
    // and the division is then a double division. That is not the same expression
    // as (PI/4) rounded afterwards, so the cast is kept where D puts it.
    const number PI_d = static_cast<number>(EIL_PI);
    Vector3 a45 = Vector3(eil::cos(PI_d / 4), eil::sin(PI_d / 4));
    Vector3 a60 = Vector3(eil::cos(PI_d / 3), eil::sin(PI_d / 3));
    CHECK(eil::geom::approxEqualVectors(
              a45.rotate2d(static_cast<double>(15.0L * EIL_PI / 180.0L)), a60),
          "rotate2d 45->60");
    Vector3 a30 = Vector3(eil::cos(PI_d / 6), eil::sin(PI_d / 6));
    CHECK(eil::geom::approxEqualVectors(
              a30.rotate2d(static_cast<double>(30.0L * EIL_PI / 180.0L)), a60),
          "rotate2d 30->60");

    std::printf("%s: geom/elements/vector3 unittest, %d/%d assertions hold\n",
                failures == 0 ? "PASS" : "FAIL", checks - failures, checks);
    return failures == 0 ? 0 : 1;
}
