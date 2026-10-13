#pragma once
// geom/surface/ruledsurface.d -> src/geom/surface/ruledsurface.hpp
// (module geom.surface.ruledsurface).
//
// A ParametricSurface that sweeps a straight line ("ruled" direction) between two
// boundary Paths (D:13-88).
//
// Namespace: eil::geom, flat. RuledSurface is declared by no other geom module (grep
// over gdtk src/geom), so no eil::geom::<module> nesting was needed.
//
// Non-1:1 notes (all registered in provenance/fidelity.yml):
//   * adapted-handles: D:15-16 `Path edge0/edge1` are class handles, so they are
//     std::shared_ptr<Path> (PathPtr, geom/gpath/path.hpp:70) and the constructor's
//     class parameters are handles too. dup() (D:59-62) returns
//     std::shared_ptr<ParametricSurface> like the rest of this hierarchy; D's covariant
//     `RuledSurface dup()` is only the static type the caller saw.
//   * D:27-44 and D:67-78 switch on the *string* ruled_direction. C++ has no string
//     switch, so both become if/else chains comparing the same four accepted values
//     ("r"/"i", then "s"/"j") in D's order, with the same `default:` throw. The
//     fall-through of `case "r": case "i":` is the disjunction, as D writes it.
//   * D:21-26 defaults `ruled_direction = "s"` and `pure2D = false` carry over.
//   * D:66 `Vector3 p;` is Vector3.init (NaN in all three components, wave-02-brief
//     section 5) and is then overwritten by the blend; the declaration is kept in the
//     same position so the statement sequence matches D line for line.
//   * D:79 clears z *after* the blend, so pure2D cannot be folded into the edges.
//   * D:83-87 `to!string(edge0)` renders the Path through its virtual toString()
//     (eil::geom::d_tostr, geom/gpath/path.hpp:129-130), and `to!string(pure2D)` is
//     D's bool, i.e. "true"/"false" (measured row rs.dirr.str of
//     baselines/e587b235/probes/geom_surface_ruledsurface.txt).
//   * Both `default:` arms throw `new Exception` (D:43, D:77), which is eil::ExceptionBase
//     — NOT an Error; the two are siblings (wave-01 lesson 1), so a ported
//     `catch (ExceptionBase)` behaves like D's `catch (Exception)`. Measured from the
//     reference binary: D prints `type=object.Exception` and the message
//     "Invalid string for ruled_direction: q". opCall's arm is reachable even though the
//     constructor validates first, because every member is public: writing
//     `rs.ruled_direction = "q"` after construction makes opCall, area() (which calls
//     opCall through the base) and dup() all throw — rows rs.tampered.* prove it.
//   * the copy constructor (D:47-57) is the one D member that does NOT re-run the
//     switch: it dups the two edges and copies ruled_direction, pure2D and the four
//     corners as they are. dup() (D:59-62) does not use it — it goes back through the
//     validating constructor — so the two copy paths differ for a tampered object.
//     Both are ported and both are replayed (rs.copy.*, rs.tampered.copy.*).
//   * the four corner members are declared p00, p10, p11, p01 in that order (D:19) and
//     are filled with different patterns for the two direction families (D:31-33 vs
//     D:38-40); the order and the patterns are preserved verbatim.

#include <memory>
#include <string>

#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"
#include "geom/surface/parametricsurface.hpp"

namespace eil {
namespace geom {

class RuledSurface : public ParametricSurface {
  public:
    PathPtr edge0;                    // D:15
    PathPtr edge1;                    // D:16
    std::string ruled_direction;      // D:17
    bool pure2D = eil::d_init<bool>(); // D:18

    // D:19. The declaration order p00, p10, p11, p01 is D's.
    Vector3 p00, p10, p11, p01;

    // D:21-45
    RuledSurface(const PathPtr& edge0, const PathPtr& edge1,
                 const std::string& ruled_direction = "s", bool pure2D = false);
    // D:47-57, `this(ref const(RuledSurface) other)`. No validation, as in D.
    RuledSurface(const RuledSurface& other);

    std::shared_ptr<ParametricSurface> dup() const override; // D:59-62
    Vector3 operator()(double r, double s) const override;   // D:64-81 opCall
    std::string toString() const override;                   // D:83-87
};

} // namespace geom
} // namespace eil
