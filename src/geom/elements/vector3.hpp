#pragma once
// geom/elements/vector3.d -> src/geom/elements/vector3.hpp (module geom.elements.vector3).
//
// Vector3 is the lowest-level type in the whole dependency closure: geom/*, the
// solver's FlowState/CellData and the boundary conditions all build on it, so
// the arithmetic order here is what the 1e-13 gate ultimately measures.
//
// Non-1:1 notes (all registered in provenance/fidelity.yml):
//   * `add(number x, number y, number z, number factor)` (D vector3.d:137-143) has
//     parameters that *shadow the members*, so its body `x += factor*x` operates
//     on the parameters and the object is unchanged. The C++ port renames the
//     parameters and keeps the no-side-effect behaviour; see the definition.
//   * `getJSONVector3` (D vector3.d:794-812) needs std.json, which has no library
//     on this machine (backlog T-902). Its only consumers are lmr/bc/* (Wave 8),
//     so it is not ported here.
//   * `version(complex_numbers)` members are kept under EIL_COMPLEX_NUMBERS and are
//     not compiled in the trunk build (backlog T-904).

#include "eil/eil.hpp"
#include "nm/number.hpp"

namespace eil {
namespace geom {

struct Vector3 {
    // D's `Vector3 v;` and `Vector3()` are Vector3.init, whose number fields are
    // double.init == quiet NaN, not zero (measured: probe line str_init prints
    // "Vector3(nan, nan, nan)"). Zero-filling here would hide a NaN that the
    // reference solver carries into its arithmetic and its logs.
    number x = d_init<number>(), y = d_init<number>(), z = d_init<number>();

    Vector3() = default;
    explicit Vector3(Slice<number> p);
    Vector3(number x, number y = 0.0, number z = number(0.0));
    Vector3(const Vector3& other);
    // D struct assignment is a memberwise copy, and the user-declared copy
    // constructor above would otherwise make the implicit one deprecated in every
    // geom consumer that writes `v = Vector3(...)` (-Wdeprecated-copy). Defaulting it
    // keeps the value semantics identical.
    Vector3& operator=(const Vector3&) = default;

#ifdef EIL_COMPLEX_NUMBERS
    explicit Vector3(Slice<double> p);
    Vector3(double x, double y = 0.0, double z = 0.0);
#endif

    Vector3 dup() const;

    Vector3& set(const Vector3& other);
    Vector3& set(Vector3* other);
    Vector3& set(const Vector3* other);
    Vector3& set(number x, number y, number z = number(0.0));
    Vector3& set(Slice<number> xyz);

#ifdef EIL_COMPLEX_NUMBERS
    Vector3& set(double x, double y, double z = 0.0);
    Vector3& set(Slice<double> xyz);
    Vector3& clear_imaginary_components();
#endif

    Vector3& clear();

    // The 4-number overload is the member-shadowing one described above.
    Vector3& add(number x, number y, number z, number factor);
    Vector3& add(const Vector3& other);
    Vector3& add(Vector3* other);
    Vector3& add(const Vector3* other);
    Vector3& add(const Vector3& other, number factor);
    Vector3& add(Vector3* other, number factor);
    Vector3& add(const Vector3* other, number factor);
    Vector3& scale(number factor);

#ifdef EIL_COMPLEX_NUMBERS
    Vector3& add(double x, double y, double z, double factor);
    Vector3& add(const Vector3& other, double factor);
    Vector3& add(Vector3* other, double factor);
    Vector3& scale(double factor);
#endif

    std::string toString() const;

    Vector3 operator+() const;
    Vector3 operator-() const;
    Vector3 operator+(const Vector3& rhs) const;
    Vector3 operator-(const Vector3& rhs) const;
    Vector3 operator*(number rhs) const;
    Vector3 operator/(number rhs) const;
    Vector3& operator+=(const Vector3& rhs);
    Vector3& operator-=(const Vector3& rhs);
    Vector3& operator*=(number rhs);
    Vector3& operator/=(number rhs);

    void normalize();
    number dot(const Vector3& other) const;
    number dot(const Vector3* other) const;

    void transform_to_local_frame(const Vector3& n, const Vector3& t1, const Vector3& t2);
    void transform_to_global_frame(const Vector3& n, const Vector3& t1, const Vector3& t2);
    void transform_to_local_frame(const Vector3& n, const Vector3& t1);
    void transform_to_global_frame(const Vector3& n, const Vector3& t1);
    void transform_to_local_frame(const Vector3& n, const Vector3& t1, const Vector3& t2,
                                  const Vector3& c);
    void transform_to_global_frame(const Vector3& n, const Vector3& t1, const Vector3& t2,
                                   const Vector3& c);
    void apply_matrix_transform(Slice<number> Rmatrix);

    Vector3& mirror_image(const Vector3& point, const Vector3& normal);
    Vector3& rotate_about_zaxis(double dtheta);
    Vector3& rotate2d(double dtheta);
};

// `lhs * Vector3` — D's opBinaryRight.
Vector3 operator*(number lhs, const Vector3& rhs);

double distance_between(const Vector3& v1, const Vector3& v2);

number dot(const Vector3& v1, const Vector3& v2);
number dot(const Vector3* v1, const Vector3& v2);
number dot(const Vector3& v1, const Vector3* v2);
number dot(const Vector3* v1, const Vector3* v2);

number abs(const Vector3& v);

Vector3 unit(const Vector3& v);

// The out-parameter form first: it is the one used inside hot loops, and it
// writes v3.x, v3.y, v3.z in that order while reading only v1 and v2 (so v3 may
// alias neither without care, exactly as in D).
void cross(Vector3& v3, const Vector3& v1, const Vector3& v2);
Vector3 cross(const Vector3& v1, const Vector3& v2);

number dot_product(number ax, number ay, number az, number bx, number by, number bz);

void cross_product(number ax, number ay, number az, number bx, number by, number bz, number& cx,
                   number& cy, number& cz);

number dot(number ax, number ay, number az, const Vector3& other);

void transform_to_local_frame(number& ax, number& ay, number& az, const Vector3& n,
                              const Vector3& t1, const Vector3& t2);

void transform_to_global_frame(number& ax, number& ay, number& az, const Vector3& n,
                               const Vector3& t1, const Vector3& t2);

bool approxEqualVectors(const Vector3& v1, const Vector3& v2, double maxRelDiff = 1.0e-2,
                        double maxAbsDiff = 1.0e-5);

} // namespace geom
} // namespace eil
