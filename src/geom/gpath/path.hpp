#pragma once
// geom/gpath/path.d -> src/geom/gpath/path.hpp (module geom.gpath.path).
//
// The abstract base of the one-parameter geometry elements. Everything in
// src/geom/gpath/* and the surface/volume/grid layers that take a boundary curve
// derive from this class, so the finite-difference stencils here (dt = 0.001 with
// the two one-sided branches at the t boundaries) are part of the reference
// solver's observable behaviour and are copied statement for statement.
//
// Non-1:1 notes (registered in provenance/fidelity.yml):
//   * D path.d:24 `import geom.surface;` is a DEAD IMPORT and is not carried.
//     Measured: path.d references 0 symbols from geom.surface (no Surface,
//     ParametricSurface, Patch or helper appears in its 177 lines), while under
//     ldc2 1.40 that import drags geom.surface/geom.grid into the compile closure
//     of every gpath module — which is why the D probes in
//     probes/geom/gpath/*.d need the @nogc fix-up described in their headers.
//     Path does not depend on surface in the port; nothing else in gpath needed
//     it either (checked module by module).
//   * adapted-handles: D's `abstract Path dup() const` returns a Path *handle*,
//     and D's covariant overrides return the concrete handle (e.g. `Line dup()`).
//     C++ covariance only works for raw pointers, so every dup() in the hierarchy
//     returns std::shared_ptr<Path> (wave-02-brief section 4b item 1). Call sites
//     that D writes `cast(Line)p` keep the same shape as dynamic_cast (see below).
//   * D's class parameters (`Path pth`, `const Path other`) are references; they
//     port to std::shared_ptr<Path> (PathPtr) so that "copying" a Path aliases the
//     same object, as D does. Polyline/modifiedpath/svgpath store PathPtr.
//   * `Path::intersect2D` (this class, D:124) hides the free function
//     eil::geom::intersect2D from geom/elements/projection.hpp:56 (D:16) inside
//     its own body. The call at D:142 is the FREE one and is written
//     ::eil::geom::intersect2D in path.cpp so it cannot silently recurse.
//   * D:136 `if (cast(Line)this !is null)` is a run-time type check, kept as
//     dynamic_cast<const Line*>(this) rather than a virtual flag (brief 4b item 4).
//     This is why path.cpp includes line.hpp: Line must be a complete type there.
//   * D:151 `ref double t` ports to `double& t`; D:102 `out double t` also ports to
//     `double& t`. The distinction D makes (out resets the caller's variable to
//     double.init == NaN on entry, ref leaves it) is invisible at every gpath call
//     site because both functions assign t on every path through the body.
//   * D:154 the nested function `distanceToPoint` closes over `this` and `p`; it
//     becomes the capturing lambda passed to the callable overload of
//     eil::nm::linesearch::minimize (added for exactly this site). The two entry
//     points share minimize_fn, so the golden-section arithmetic is identical to
//     the NTTP form D uses — measured bit-identical bracket in wave 1.
//   * Namespace: everything here is flat in eil::geom. Checked for collisions:
//     none of Path/Line/Arc/Arc3/Bezier/Polynomial/Polyline/Helix/NURBS/SVGPath/
//     XSpline/XSplineLsq/modifiedpath's 8 classes/gpath_utils' 4 free functions is
//     declared by any other geom module (grep over gdtk src/geom, luawrap
//     excluded), so no module needed the eil::geom::gpath::<module> escape hatch.

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "eil/array.hpp"
#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"

namespace eil {
namespace geom {

class Path;
// D's `Path[]` and a `Path` parameter (D class = reference).
using PathPtr = std::shared_ptr<Path>;
using PathList = eil::Array<PathPtr>;

class Path {
  public:
    virtual ~Path() = default;

    virtual std::shared_ptr<Path> dup() const = 0;
    virtual Vector3 operator()(double t) const = 0; // D opCall
    virtual Vector3 dpdt(double t) const;
    virtual Vector3 d2pdt2(double t) const;
    virtual number partial_length(double ta, double tb) const;
    virtual number length() const;
    virtual Vector3 point_from_length(number length, double& t) const;
    virtual std::string toString() const = 0;
    virtual std::string classString() const = 0;

    bool intersect2D(const Vector3& ps, const Vector3& d, double& t, int nseg = 20) const;
    double closestDistance(const Vector3& p, double& t) const;
};

// ---------------------------------------------------------------------------
// d_tostr(): D's `to!string` for the payload types the gpath toString() bodies
// use. It is not a D symbol; it exists because eil::to_string has no case for a
// class with a toString() method (see tests/demo_geom_elements_vector3.cpp:13-14,
// which records the same convention for Vector3), and because the ported
// toString()s print the same bytes as D: a Vector3 renders as "Vector3(1, 2.2, 3)",
// a Path through its virtual toString(), and an array as "[e1, e2]" with string
// elements quoted and numbers bare (measured against ldc2, see
// baselines/e587b235/probes/geom_gpath_formats.txt).
// ---------------------------------------------------------------------------
namespace detail_d_tostr {

template <typename C, typename = void> struct has_len : std::false_type {};
template <typename C>
struct has_len<C, std::void_t<decltype(std::declval<const C&>().length())>> : std::true_type {};

template <typename C, typename = void> struct has_size : std::false_type {};
template <typename C>
struct has_size<C, std::void_t<decltype(std::declval<const C&>().size())>> : std::true_type {};

template <typename C, typename = void> struct has_index : std::false_type {};
template <typename C>
struct has_index<C, std::void_t<decltype(std::declval<const C&>()[std::size_t(0)])>>
    : std::true_type {};

template <typename C>
struct is_indexed : std::bool_constant<(has_len<C>::value || has_size<C>::value)
                                       && has_index<C>::value && !std::is_array_v<C>
                                       && !std::is_convertible_v<C, std::string_view>> {};

template <typename C> size_t range_length(const C& c) {
    if constexpr (has_len<C>::value) return c.length();
    else return c.size();
}

} // namespace detail_d_tostr

inline std::string d_tostr(const Vector3& v) { return v.toString(); }
inline std::string d_tostr(const Path& p) { return p.toString(); }
inline std::string d_tostr(const PathPtr& p) { return p ? p->toString() : std::string("null"); }
// String literals reach the leaf renderer rather than the array one (D's
// to!string("abc") is the string itself, unquoted).
template <size_t N> std::string d_tostr(const char (&s)[N]) { return std::string(s); }
// The scalar/leaf cases forward to the substrate renderer, which is the
// %.6g-precision one D uses.
template <typename T>
typename std::enable_if<std::is_arithmetic<T>::value || std::is_same_v<T, std::string> ||
                            std::is_same_v<T, const char*> || std::is_same_v<T, char>,
                        std::string>::type
d_tostr(const T& v) {
    return eil::to_string(v);
}
// Arrays/slices of the above, bracketed and comma-separated like D's %s.
template <typename C>
typename std::enable_if<detail_d_tostr::is_indexed<C>::value, std::string>::type
d_tostr(const C& c) {
    std::string out = "[";
    const size_t n = detail_d_tostr::range_length(c);
    for (size_t i = 0; i < n; ++i) {
        if (i) out += ", ";
        out += d_tostr(c[i]);
    }
    return out + "]";
}

namespace detail {

// The four gpath modules that read coordinate files (polyline.d:111-130,
// xspline.d:34-51, xsplinelsq.d:35-66, gpath_utils.d:30-43) all do it the same
// way: `line.strip().split()`, skip a line with no tokens, skip a line whose
// first token is exactly "#", then read up to three whitespace-separated fields.
// This helper is the shared spelling of that tokenisation so the four cannot
// drift apart; it is not a D symbol. Returns the tokens of one line (already
// trimmed); an empty vector means "skip this line", matching D's two `continue`s.
std::vector<std::string> tokenize_data_line(const std::string& line);

// Read a whole text file into lines the way std.stdio.File.byLine does (the
// trailing newline is not part of the line; a final line without a newline is
// still returned).
std::vector<std::string> read_file_lines(const std::string& fileName);

} // namespace detail

} // namespace geom
} // namespace eil
