// geom/elements/vector3.d -> src/geom/elements/vector3.cpp
//
// Definitions are kept in the order they appear in the D source. See the header
// for the two non-1:1 points (the member-shadowing add(), and the deferred
// JSON helper).

#include "geom/elements/vector3.hpp"

#include <cmath>

namespace eil {
namespace geom {

Vector3::Vector3(Slice<number> p) {
    switch (p.length()) {
    case 0: x = y = z = 0.0; break;
    case 1: x = p[0]; y = z = 0.0; break;
    case 2: x = p[0]; y = p[1]; z = 0.0; break;
    default: x = p[0]; y = p[1]; z = p[2]; break;
    }
}

Vector3::Vector3(number vx, number vy, number vz) : x(vx), y(vy), z(vz) {}

Vector3::Vector3(const Vector3& other) : x(other.x), y(other.y), z(other.z) {}

#ifdef EIL_COMPLEX_NUMBERS
Vector3::Vector3(Slice<double> p) {
    switch (p.length()) {
    case 0: x = y = z = 0.0; break;
    case 1: x = p[0]; y = z = 0.0; break;
    case 2: x = p[0]; y = p[1]; z = 0.0; break;
    default: x = p[0]; y = p[1]; z = p[2]; break;
    }
}

Vector3::Vector3(double vx, double vy, double vz) : x(vx), y(vy), z(vz) {}
#endif

Vector3 Vector3::dup() const { return Vector3(*this); }

Vector3& Vector3::set(const Vector3& other) {
    x = other.x; y = other.y; z = other.z;
    return *this;
}

Vector3& Vector3::set(Vector3* other) {
    x = other->x; y = other->y; z = other->z;
    return *this;
}

Vector3& Vector3::set(const Vector3* other) {
    x = other->x; y = other->y; z = other->z;
    return *this;
}

Vector3& Vector3::set(number vx, number vy, number vz) {
    x = vx; y = vy; z = vz;
    return *this;
}

Vector3& Vector3::set(Slice<number> xyz) {
    x = xyz[0]; y = xyz[1]; z = xyz[2];
    return *this;
}

#ifdef EIL_COMPLEX_NUMBERS
Vector3& Vector3::set(double vx, double vy, double vz) {
    x = vx; y = vy; z = vz;
    return *this;
}

Vector3& Vector3::set(Slice<double> xyz) {
    x = xyz[0]; y = xyz[1]; z = xyz[2];
    return *this;
}

Vector3& Vector3::clear_imaginary_components() {
    x.im = 0.0; y.im = 0.0; z.im = 0.0;
    return *this;
}
#endif

Vector3& Vector3::clear() {
    x = 0.0; y = 0.0; z = 0.0;
    return *this;
}

// D vector3.d:137-143. The parameter names shadow the members there, so the body
// `x += factor*x` reads and writes the *parameters* and the object is left
// untouched. The parameters are renamed here; the empty effect is preserved.
Vector3& Vector3::add(number px, number py, number pz, number factor) {
    px += factor * px;
    py += factor * py;
    pz += factor * pz;
    return *this;
}

Vector3& Vector3::add(const Vector3& other) {
    x += other.x; y += other.y; z += other.z;
    return *this;
}

Vector3& Vector3::add(Vector3* other) {
    x += other->x; y += other->y; z += other->z;
    return *this;
}

Vector3& Vector3::add(const Vector3* other) {
    x += other->x; y += other->y; z += other->z;
    return *this;
}

Vector3& Vector3::add(const Vector3& other, number factor) {
    x += other.x * factor; y += other.y * factor; z += other.z * factor;
    return *this;
}

Vector3& Vector3::add(Vector3* other, number factor) {
    x += other->x * factor; y += other->y * factor; z += other->z * factor;
    return *this;
}

Vector3& Vector3::add(const Vector3* other, number factor) {
    x += other->x * factor; y += other->y * factor; z += other->z * factor;
    return *this;
}

Vector3& Vector3::scale(number factor) {
    x *= factor; y *= factor; z *= factor;
    return *this;
}

#ifdef EIL_COMPLEX_NUMBERS
Vector3& Vector3::add(double px, double py, double pz, double factor) {
    px += factor * px; py += factor * py; pz += factor * pz;
    return *this;
}

Vector3& Vector3::add(const Vector3& other, double factor) {
    x += other.x * factor; y += other.y * factor; z += other.z * factor;
    return *this;
}

Vector3& Vector3::add(Vector3* other, double factor) {
    x += other->x * factor; y += other->y * factor; z += other->z * factor;
    return *this;
}

Vector3& Vector3::scale(double factor) {
    x *= factor; y *= factor; z *= factor;
    return *this;
}
#endif

std::string Vector3::toString() const {
    return eil::format("Vector3(%s, %s, %s)", eil::to_string(x), eil::to_string(y),
                       eil::to_string(z));
}

Vector3 Vector3::operator+() const { return Vector3(x, y, z); }

Vector3 Vector3::operator-() const { return Vector3(-x, -y, -z); }

Vector3 Vector3::operator+(const Vector3& rhs) const {
    return Vector3(x + rhs.x, y + rhs.y, z + rhs.z);
}

Vector3 Vector3::operator-(const Vector3& rhs) const {
    return Vector3(x - rhs.x, y - rhs.y, z - rhs.z);
}

Vector3 Vector3::operator*(number rhs) const { return Vector3(x * rhs, y * rhs, z * rhs); }

Vector3 Vector3::operator/(number rhs) const { return Vector3(x / rhs, y / rhs, z / rhs); }

Vector3& Vector3::operator+=(const Vector3& rhs) {
    x += rhs.x; y += rhs.y; z += rhs.z;
    return *this;
}

Vector3& Vector3::operator-=(const Vector3& rhs) {
    x -= rhs.x; y -= rhs.y; z -= rhs.z;
    return *this;
}

Vector3& Vector3::operator*=(number rhs) {
    x *= rhs; y *= rhs; z *= rhs;
    return *this;
}

Vector3& Vector3::operator/=(number rhs) {
    x /= rhs; y /= rhs; z /= rhs;
    return *this;
}

void Vector3::normalize() {
    number magnitude = eil::sqrt(this->dot(*this));
    if (magnitude > 0.0) {
        x /= magnitude; y /= magnitude; z /= magnitude;
    } else {
        // Clean up, in case dot() underflows.
        x = y = z = 0.0;
    }
    // Flush small components to zero.
    const double small = 1.0e-30;
#ifdef EIL_COMPLEX_NUMBERS
    if (std::fabs(re(x)) < small && std::fabs(im(x)) < small) { x = 0.0; }
    if (std::fabs(re(y)) < small && std::fabs(im(y)) < small) { y = 0.0; }
    if (std::fabs(re(z)) < small && std::fabs(im(z)) < small) { z = 0.0; }
#else
    if (std::fabs(x) < small) { x = 0.0; }
    if (std::fabs(y) < small) { y = 0.0; }
    if (std::fabs(z) < small) { z = 0.0; }
#endif
}

number Vector3::dot(const Vector3& other) const {
    return x * other.x + y * other.y + z * other.z;
}

number Vector3::dot(const Vector3* other) const {
    return x * other->x + y * other->y + z * other->z;
}

void Vector3::transform_to_local_frame(const Vector3& n, const Vector3& t1, const Vector3& t2) {
    number v_x = this->dot(n);   // normal component
    number v_y = this->dot(t1);  // tangential component 1
    number v_z = this->dot(t2);  // tangential component 2
    x = v_x; y = v_y; z = v_z;
}

void Vector3::transform_to_global_frame(const Vector3& n, const Vector3& t1, const Vector3& t2) {
    number v_x = x * n.x + y * t1.x + z * t2.x;  // global-x
    number v_y = x * n.y + y * t1.y + z * t2.y;  // global-y
    number v_z = x * n.z + y * t1.z + z * t2.z;  // global-z
    x = v_x; y = v_y; z = v_z;
}

// 2D flavour for change of coordinate system functions.

void Vector3::transform_to_local_frame(const Vector3& n, const Vector3& t1) {
    number v_x = x * n.x + y * n.y;    // normal component
    number v_y = x * t1.x + y * t1.y;  // tangential component 1
    x = v_x; y = v_y; z = 0.0;
}

void Vector3::transform_to_global_frame(const Vector3& n, const Vector3& t1) {
    number v_x = x * n.x + y * t1.x;  // global-x
    number v_y = x * n.y + y * t1.y;  // global-y
    x = v_x; y = v_y; z = 0.0;
}

// Change of coordinate system; rotation with translation.

void Vector3::transform_to_local_frame(const Vector3& n, const Vector3& t1, const Vector3& t2,
                                       const Vector3& c) {
    x -= c.x; y -= c.y; z -= c.z;  // shift to local origin
    number v_x = this->dot(n);     // normal component
    number v_y = this->dot(t1);    // tangential component 1
    number v_z = this->dot(t2);    // tangential component 2
    x = v_x; y = v_y; z = v_z;
}

void Vector3::transform_to_global_frame(const Vector3& n, const Vector3& t1, const Vector3& t2,
                                        const Vector3& c) {
    number v_x = x * n.x + y * t1.x + z * t2.x + c.x;  // global-x
    number v_y = x * n.y + y * t1.y + z * t2.y + c.y;  // global-y
    number v_z = x * n.z + y * t1.z + z * t2.z + c.z;  // global-z
    x = v_x; y = v_y; z = v_z;
}

void Vector3::apply_matrix_transform(Slice<number> Rmatrix) {
    // Write out the matrix multiplication, long-hand.
    number old_x = x;
    number old_y = y;
    number old_z = z;
    x = Rmatrix[0] * old_x + Rmatrix[1] * old_y + Rmatrix[2] * old_z;
    y = Rmatrix[3] * old_x + Rmatrix[4] * old_y + Rmatrix[5] * old_z;
    z = Rmatrix[6] * old_x + Rmatrix[7] * old_y + Rmatrix[8] * old_z;
}

#ifdef EIL_COMPLEX_NUMBERS
void Vector3::apply_matrix_transform(Slice<double> Rmatrix) {
    number old_x = x;
    number old_y = y;
    number old_z = z;
    x = Rmatrix[0] * old_x + Rmatrix[1] * old_y + Rmatrix[2] * old_z;
    y = Rmatrix[3] * old_x + Rmatrix[4] * old_y + Rmatrix[5] * old_z;
    z = Rmatrix[6] * old_x + Rmatrix[7] * old_y + Rmatrix[8] * old_z;
}
#endif

Vector3& Vector3::mirror_image(const Vector3& point, const Vector3& normal) {
    Vector3 n = Vector3(normal.x, normal.y, normal.z);
    n.normalize();
    // Construct tangents to the plane.
    Vector3 different = n + Vector3(1.0, 1.0, 1.0);
    Vector3 t1;
    cross(t1, n, different);
    t1.normalize();
    Vector3 t2;
    cross(t2, n, t1);
    t2.normalize();
    // Mirror image the vector in a frame local to the plane.
    transform_to_local_frame(n, t1, t2, point);
    x = -x;
    transform_to_global_frame(n, t1, t2, point);
    return *this;
}

Vector3& Vector3::rotate_about_zaxis(double dtheta) {
    double theta = eil::atan2(re(y), re(x)) + dtheta;
    double r = eil::sqrt(re(x) * re(x) + re(y) * re(y));
    x = r * std::cos(theta);
    y = r * std::sin(theta);
    return *this;
}

Vector3& Vector3::rotate2d(double dtheta) {
    double sn = std::sin(dtheta);
    double cs = std::cos(dtheta);
    number _x = x * cs - y * sn;
    number _y = y * cs + x * sn;
    x = _x; y = _y;
    return *this;
}

Vector3 operator*(number lhs, const Vector3& rhs) {
    return Vector3(rhs.x * lhs, rhs.y * lhs, rhs.z * lhs);
}

double distance_between(const Vector3& v1, const Vector3& v2) {
    // D spells the squares with `^^2`, which is std.math.pow (see
    // probes/substrate/probe_pow.d); expanding it into multiplications is a
    // 1 ULP change, so eil::pow is used verbatim.
    number d = eil::sqrt(eil::pow(v1.x - v2.x, 2) + eil::pow(v1.y - v2.y, 2) +
                         eil::pow(v1.z - v2.z, 2));
    return re(d);
}

number dot(const Vector3& v1, const Vector3& v2) {
    number result = 0.0;
    // Maybe we should be careful with underflow and overflow...
    result = v1.x * v2.x + v1.y * v2.y + v1.z * v2.z;
    return result;
}

number dot(const Vector3* v1, const Vector3& v2) {
    number result = 0.0;
    result = v1->x * v2.x + v1->y * v2.y + v1->z * v2.z;
    return result;
}

number dot(const Vector3& v1, const Vector3* v2) {
    number result = 0.0;
    result = v1.x * v2->x + v1.y * v2->y + v1.z * v2->z;
    return result;
}

number dot(const Vector3* v1, const Vector3* v2) {
    number result = 0.0;
    result = v1->x * v2->x + v1->y * v2->y + v1->z * v2->z;
    return result;
}

number abs(const Vector3& v) { return eil::sqrt(v.dot(v)); }

Vector3 unit(const Vector3& v) {
    Vector3 v2 = Vector3(v);
    v2.normalize();
    return v2;
}

void cross(Vector3& v3, const Vector3& v1, const Vector3& v2) {
    v3.x = v1.y * v2.z - v2.y * v1.z;
    v3.y = v2.x * v1.z - v1.x * v2.z;
    v3.z = v1.x * v2.y - v2.x * v1.y;
}

Vector3 cross(const Vector3& v1, const Vector3& v2) {
    Vector3 v3;
    cross(v3, v1, v2);
    return v3;
}

number dot_product(number ax, number ay, number az, number bx, number by, number bz) {
    return ax * bx + ay * by + az * bz;
}

void cross_product(number ax, number ay, number az, number bx, number by, number bz, number& cx,
                   number& cy, number& cz) {
    cx = ay * bz - az * by;
    cy = az * bx - ax * bz;
    cz = ax * by - ay * bx;
    return;
}

number dot(number ax, number ay, number az, const Vector3& other) {
    return ax * other.x + ay * other.y + az * other.z;
}

void transform_to_local_frame(number& ax, number& ay, number& az, const Vector3& n,
                              const Vector3& t1, const Vector3& t2) {
    number v_x = dot(ax, ay, az, n);   // normal component
    number v_y = dot(ax, ay, az, t1);  // tangential component 1
    number v_z = dot(ax, ay, az, t2);  // tangential component 2
    ax = v_x; ay = v_y; az = v_z;
}

void transform_to_global_frame(number& ax, number& ay, number& az, const Vector3& n,
                               const Vector3& t1, const Vector3& t2) {
    number v_x = ax * n.x + ay * t1.x + az * t2.x;  // global-x
    number v_y = ax * n.y + ay * t1.y + az * t2.y;  // global-y
    number v_z = ax * n.z + ay * t1.z + az * t2.z;  // global-z
    ax = v_x; ay = v_y; az = v_z;
}

bool approxEqualVectors(const Vector3& v1, const Vector3& v2, double maxRelDiff,
                        double maxAbsDiff) {
    return (eil::nm::approxEqualNumbers(v1.x, v2.x, maxRelDiff, maxAbsDiff) &&
            eil::nm::approxEqualNumbers(v1.y, v2.y, maxRelDiff, maxAbsDiff) &&
            eil::nm::approxEqualNumbers(v1.z, v2.z, maxRelDiff, maxAbsDiff));
}

} // namespace geom
} // namespace eil
