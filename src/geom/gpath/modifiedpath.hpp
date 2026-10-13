#pragma once
// geom/gpath/modifiedpath.d -> src/geom/gpath/modifiedpath.hpp
// (module geom.gpath.modifiedpath).
//
// Paths that wrap other Paths: re-parameterisations (arc length, sub-range,
// reversal) and geometric transforms (translation, mirror image, rotation about
// the z-axis). The 4-level chain Path -> ReParameterizedPath -> SubRangedPath ->
// ReversedPath is why the probe for this module dumps every wrapper against the
// same swept t values as the wrapped path: a wrapper that delegates to the wrong
// handle, or that forgets to re-derive through the chain, still evaluates to
// something plausible at t=0.5.
//
// Non-1:1 notes:
//   * adapted-handles: `Path underlying_path` / `Path original_path` are D class
//     references (D:18, :209) -> std::shared_ptr<Path>. The constructors dup() the
//     argument, as D does, so a wrapper owns an independent copy.
//   * D:39-42 and D:219-222 declare `abstract override string toString() const`
//     WITH a body — D allows an abstract function to carry one, and it is
//     reachable only through a qualified call. C++ has the same feature (a pure
//     virtual with an out-of-line definition), so it ports directly; the subclasses
//     all override it, as in D.
//   * D:74-77 ArcLengthParameterizedPath.classString() returns
//     "ArcLengthParametrizedPath" — missing the 'e' of "Parameterized", while its
//     toString() says "ArcLengthParameterizedPath". Kept verbatim: log comparison
//     and Polyline::toGmshString's classString() switch depend on the exact bytes
//     D writes (modifiedpath.d:76).
//   * D:194-204 ReversedPath overrides only dup(); it therefore reports
//     classString() == "SubRangedPath" and toString() == "SubRangedPath(...)" like
//     its base. Kept verbatim (modifiedpath.d:194).
//   * D:57-61 calls `super(other)` in the middle of the constructor body before
//     set_arc_length_vector(100); in C++ the base is initialised first and
//     set_arc_length_vector() runs in the body — same sequence, because nothing
//     between them reads the object.
//   * D:229 `abstract Vector3 apply_transform(ref Vector3 p) const` -> virtual with
//     a non-const reference parameter (D's `ref` is not const-viewable). The
//     overrides do not modify p; the reference is kept because MirrorImagePath and
//     RotatedAboutZAxisPath call the in-place Vector3 mutators, as D does.

#include <cstddef>
#include <memory>
#include <string>

#include "eil/array.hpp"
#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"

namespace eil {
namespace geom {

class ReParameterizedPath : public Path {
  public:
    PathPtr underlying_path;

    // D:19-22, `this(const Path other)`
    explicit ReParameterizedPath(const PathPtr& other);

    Vector3 operator()(double t) const override;
    Vector3 dpdt(double t) const override;
    Vector3 d2pdt2(double t) const override;
    // D:39-42: abstract, with a body (see the header note).
    std::string toString() const override = 0;
    std::string classString() const override;

  protected:
    virtual double underlying_t(double t) const = 0;
    virtual double d_underlying_t_dt(double t) const = 0;
    virtual double d2_underlying_t_dt2(double t) const = 0;
}; // end class ReParameterizedPath

class ArcLengthParameterizedPath : public ReParameterizedPath {
  public:
    // D:57-61
    explicit ArcLengthParameterizedPath(const PathPtr& other);
    // D:62-65, `this(ref const(ArcLengthParameterizedPath) other)`
    ArcLengthParameterizedPath(const ArcLengthParameterizedPath& other);

    std::shared_ptr<Path> dup() const override;
    std::string toString() const override;
    std::string classString() const override;

  protected:
    eil::Array<double> arc_length_vector; // D:80
    void set_arc_length_vector(int N);    // D:81-100
    double underlying_t(double t) const override;
    double d_underlying_t_dt(double t) const override;
    double d2_underlying_t_dt2(double t) const override;

  private:
    double d_arc_f_dt(double t) const;   // D:133-138
    double d2_arc_f_dt2(double t) const; // D:139-147
}; // end class ArcLengthParameterizedPath

class SubRangedPath : public ReParameterizedPath {
  public:
    double t0 = eil::d_init<double>();
    double t1 = eil::d_init<double>();

    // D:155-160
    SubRangedPath(const PathPtr& other, double newt0, double newt1);
    // D:161-164, `this(ref const(SubRangedPath) other)`
    SubRangedPath(const SubRangedPath& other);

    std::shared_ptr<Path> dup() const override;
    std::string toString() const override;
    std::string classString() const override;

  protected:
    double underlying_t(double t) const override;
    double d_underlying_t_dt(double t) const override;
    double d2_underlying_t_dt2(double t) const override;
}; // end class SubRangedPath

class ReversedPath : public SubRangedPath {
    // Just a particular case of SubRangedPath
  public:
    // D:196-199
    explicit ReversedPath(const PathPtr& other);

    std::shared_ptr<Path> dup() const override;
    // D:194-204 overrides nothing else, so toString()/classString() stay
    // SubRangedPath's.
}; // end class ReversedPath

class TransformedPath : public Path {
  public:
    PathPtr original_path;

    // D:210-213
    explicit TransformedPath(const PathPtr& other);

    Vector3 operator()(double t) const override;
    std::string toString() const override = 0; // D:219-222, abstract with a body
    std::string classString() const override;

  protected:
    virtual Vector3 apply_transform(Vector3& p) const = 0;
}; // end class TransformedPath

class TranslatedPath : public TransformedPath {
  public:
    // D:236-240
    TranslatedPath(const PathPtr& other, const Vector3& shift);
    // D:235, Vector3 shift
    Vector3 shift;

    std::shared_ptr<Path> dup() const override;
    std::string toString() const override;
    std::string classString() const override;

  protected:
    Vector3 apply_transform(Vector3& p) const override;
}; // end class TranslatedPath

class MirrorImagePath : public TransformedPath {
  public:
    // D:266-270
    MirrorImagePath(const PathPtr& other, const Vector3& point, const Vector3& normal);
    Vector3 point;
    Vector3 normal;

    std::shared_ptr<Path> dup() const override;
    std::string toString() const override;
    std::string classString() const override;

  protected:
    Vector3 apply_transform(Vector3& p) const override;
}; // end class MirrorImagePath

class RotatedAboutZAxisPath : public TransformedPath {
  public:
    // D:296-299
    RotatedAboutZAxisPath(const PathPtr& other, double angle);
    double dtheta = eil::d_init<double>(); // in radians, D:295

    std::shared_ptr<Path> dup() const override;
    std::string toString() const override;
    std::string classString() const override;

  protected:
    Vector3 apply_transform(Vector3& p) const override;
}; // end class RotatedAboutZAxisPath

} // namespace geom
} // namespace eil
