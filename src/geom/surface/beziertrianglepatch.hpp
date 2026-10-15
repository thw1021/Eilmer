#pragma once
// geom/surface/beziertrianglepatch.d -> src/geom/surface/beziertrianglepatch.hpp
// (module geom.surface.beziertrianglepatch)
//
// A Bezier triangle: a degree-n patch over barycentric (u,v,w) with w = 1-u-v,
// held as the control net B of (n+1)(n+2)/2 points and evaluated by de Casteljau.
// D:5-9 states, and the code confirms, that this class is NOT part of the
// ParametricSurface family — it has no base class, so wave-02-surfvol-brief
// section 1's virtual contract (operator()/dup()/toString()/classString()) does not
// apply and is not invented here: D provides opCall, projectPoint and the private
// working space only, and no toString()/classString()/dup()/derivative accessor
// exists in the 723 lines of the module. The four file writers and the two
// bezierTriangleFromPointCloud fitters are module-level free functions.
//
// Non-1:1 notes (registered in provenance/fidelity.yml):
//   * D:48 `import geom;` is the package aggregate (wave-02-brief section 4.5 says
//     not to build one in C++). The symbols this module actually takes from it are
//     Vector3 / distance_between / approxEqualVectors (geom/elements/vector3.d) and
//     Bezier (geom/gpath/bezier.d), so the port includes exactly those two headers
//     plus nm/nelmin.hpp; `import ntypes.complex` (D:43) exists so that `x.re`
//     compiles on a double in the trunk build, which is what eil::re() is for.
//   * adapted-handles: D's `BezierTrianglePatch` is a class handle. Call sites that
//     pass one (the writers, the fitters' initGuess, the fitters' return value) use
//     std::shared_ptr / const reference as the two other ported hierarchies do. The
//     guess argument is nullable in D (`initGuess !is null`, D:380, :556) so it is a
//     shared_ptr there; a null shared_ptr is the same "no guess" case D models with
//     null. b0/b1/b2 (D:487) are never tested for null in D, so they are references.
//   * D:51 `size_t toSingleIndex()(size_t i, size_t j, size_t k)` is a manifest
//     constant template with no template parameters. C++ makes it an ordinary
//     inline-able function; nothing about the value changes, and D's own body
//     discards its first argument (n=i+j+k then m=n-i, so m=j+k): measured over the
//     n=1,2,3,5 lattice in baselines/e587b235/probes/geom_surface_beziertrianglepatch.
//     txt (40 btp.tsi rows), including the u/v/w neighbours that read the same slot
//     (uIdx == idx for every lattice point).
//   * D:63 `this(const Vector3[] _B, int _n)` takes a *view* (wave-02-brief section
//     3), as do the fitters' `Vector3[] points` (D:348, :487). The port takes
//     eil::Slice<Vector3>; callers pass `.all()`.
//   * D:67 compares `int nCtrlPts != size_t _B.length`; D promotes the int to the
//     unsigned type, and so does the port (explicitly, to keep -Wsign-compare clean).
//     nCtrlPts is never negative for an integral degree, so the promotion is exact.
//   * D's `import std.algorithm.comparison : max` (D:42) is NOT std::max: for two
//     doubles comparison.d:1681 is `a < b ? b : a`, and the 3-argument form at
//     comparison.d:1644 is max(max(a,b), c) for T.length == 3. Measured with ldc2
//     1.40 (btp.max rows in the same baseline): max(nan,1)=nan but max(1,nan)=1,
//     max3(1,nan,2)=2, max3(1,2,nan)=2, max3(nan,1,2)=nan, max(-0.0,0.0)=-0. The
//     helper is file-local in the .cpp because it is D's private detail, not this
//     module's API.
//   * `a ^^ b` (D:219, :273, :332) is routed through eil::sqr: the exponent is 2 and
//     `x ^^ 2` is one multiply in D (T-931), not eil::pow(x,2).
//     These three sites are the integer arm: measured `typeof(3 ^^ 2) == int` with
//     value 9 and `(5-1) ^^ 2 == 16`, so D's result is an int and Phobos' `%d` on it
//     prints an integer (a real under %d throws std.format.FormatException —
//     measured). The port truncates eil::pow's double back to int, which is exact
//     for every degree a patch can use (pow(n,2) is exactly representable up to
//     n = 2^26, far past the 32-bit int range of D's own nCells).
//   * D's 26 in-module `iota` sites (28 counting the two float scans in the
//     unittest at D:696-697; 29 counting the `import std.range : iota` line the
//     brief counts) are all descending-index traversals of the triangle lattice with
//     an int step; they become eil::iota one-for-one (wave-02-brief section 5:
//     element n is begin+n*step and the far bound is strict). The two float iotas
//     are ported in the test's replay of the D unittest. Measured sensitivity of the
//     far bound: making D:112's inner walk inclusive of 0 aborts the replay with an
//     uncaught "Error in BezierTrianglePatch.projectPoint()" (10003 evaluations).
//   * D has no helper here. The design-vector write-back loop (D:461-475 and
//     D:647-661) and the "Optimizer stats:" block (D:193-198 and D:479-484) are each
//     written out twice in D; the port reproduces both duplicates inline rather than
//     factoring them, so the two call sites keep their own `if (success)` shape.
//   * D:104 clamps a negative w to 0.0 *before* de Casteljau, so u+v > 1 evaluates a
//     non-convex combination and a NaN w is kept (NaN < 0.0 is false). Both are
//     observable: btp.opCall.farin u=1.1 gives x=7.2600000000000016, and
//     btp.file.trivtk ne=1 prints "-nan" because `double(i)/n` is 0.0/0.0 there.
//   * D:206-345 write through std.stdio.File. The port uses FILE* with the same
//     Phobos open-failure text as src/geom/surface/parametricsurface.cpp:52-60
//     (std.exception.ErrnoException has no substrate class yet).
//   * D:155-171 and D:420-446/597-630 are nested functions closing over `this`, the
//     out-parameter q and the design vector. They become callable structs holding
//     pointers to the same objects, which is wave-02-brief section 4b item 5's
//     convention for objectives handed to nelmin. The inner name `d` of
//     nelmin.d's argument shadows the enclosing `number[] d` in D (D:420, :597); the
//     struct's operator() keeps that parameter name so the two stay distinguishable.

#include <cstddef>
#include <memory>
#include <string>

#include "eil/array.hpp"
#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/bezier.hpp"

namespace eil {
namespace geom {

// D:51-56. The triangle-lattice index; see the header note on the discarded i.
inline std::size_t toSingleIndex(std::size_t i, std::size_t j, std::size_t k) {
    auto n = i + j + k;
    auto m = n - i;
    return (m * (m + 5) / 2) - 2 * j - k;
}

class BezierTrianglePatch {
  public:
    eil::Array<Vector3> B; // control points
    int n = eil::d_init<int>(); // degree of patch (D's `int n;` is int.init == 0)

    // D:63-74
    BezierTrianglePatch(eil::Slice<Vector3> _B, int _n);

    // D:76-89
    void initialiseWorkingSpace();

    // D:91-96
    void updateWorkingSpace();

    // D:98-127. Not const, exactly as in D: opCall rewrites the private working
    // space _b on every call, which is why the writers below take a mutable patch.
    Vector3 operator()(double u, double v);

    // D:129-199
    number projectPoint(const Vector3& p, Vector3& q, double& uFound, double& vFound,
                        bool returnOnFailure = false);

    // D's class handle, as the other ported geom classes spell it.
    using Ptr = std::shared_ptr<BezierTrianglePatch>;

  private:
    eil::Array<eil::Array<Vector3>> _b; // working space for de Casteljau algorithm
};

// D:206-213
void writeBezierTriangleCtrlPtsAsText(BezierTrianglePatch& btp, const std::string& fileName);

// D:215-267
void writeBezierTriangleCtrlPtsAsVtkXml(BezierTrianglePatch& btp, const std::string& fileName);

// D:269-326
void writeBezierTriangleAsVtkXml(BezierTrianglePatch& btp, const std::string& fileName,
                                 int nEdgePts);

// D:328-345
void writeBezierTriangleAsDat(BezierTrianglePatch& btp, const std::string& fileName,
                              int nEdgePts);

// D:348-485 (corner form)
BezierTrianglePatch::Ptr bezierTriangleFromPointCloud(
    eil::Slice<Vector3> points, const Vector3& p0, const Vector3& p1, const Vector3& p2, int n,
    const BezierTrianglePatch::Ptr& initGuess);

// D:487-665 (Bezier-edge form; `ref bool success` is D's out-parameter)
BezierTrianglePatch::Ptr bezierTriangleFromPointCloud(
    eil::Slice<Vector3> points, const Bezier& b0, const Bezier& b1, const Bezier& b2, int n,
    const BezierTrianglePatch::Ptr& initGuess, bool& success);

} // namespace geom
} // namespace eil
