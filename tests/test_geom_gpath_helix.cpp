// tests/test_geom_gpath_helix.cpp
//
// Two layers, both required (the pattern of tests/test_geom_elements_properties.cpp):
//
//  A. the `unittest` block of geom/gpath/helix.d (D:95-112) ported assertion for
//     assertion, in source order, with D's own tolerance (approxEqualVectors'
//     default 1.0e-2 relative / 1.0e-5 absolute, vector3.d).
//
//  B. a line-for-line replay of probes/geom/gpath/probe_helix.d against
//     baselines/e587b235/probes/geom_gpath_helix.txt: 438 lines covering both
//     Helix constructors (helix.d:27-37 fundamental parameters, :41-60 the
//     point/axis route), the eight derived members at %.17g, the parametric curve
//     and its derivatives over the shared t sweep of pathdump.d, the copy
//     constructor (:61-67) and dup() (:68-71), a conical multi-turn helix and one
//     whose axis points along -z (to pin the cross-product handedness at :36/:51),
//     and toString() (:79-88). Numeric fields are `key=%.17g` so they can be
//     paired positionally with D's.
//
// Helix does not override dpdt()/d2pdt2(), so those rows run Path's finite
// differences (path.d:34-79) with the three boundary branches: a port that added a
// Helix-specific derivative would show up on every dpdt/d2pdt2 row.
//
// Regenerate the baseline (see the header of probe_helix.d for the full recipe):
//   ldc2 -i -I/tmp/gdtk_shadow ... probes/geom/gpath/probe_helix.d -of=/tmp/x
//   /tmp/x > baselines/e587b235/probes/geom_gpath_helix.txt
//
// Run: ./test_geom_gpath_helix ../baselines/e587b235/probes/geom_gpath_helix.txt
#include <cmath>
#include <memory>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/helix.hpp"
#include "geom/gpath/path.hpp"
#include "nm/number.hpp"

#include "gpath3_dump.hpp"

using eil::geom::Helix;
using eil::geom::Path;
using eil::geom::Vector3;
using eil_test_gpath3::emitf;

namespace {

// ============================================================================
// A. the D unittest block, assertion for assertion, in D's order.
// ============================================================================
void port_d_unittest() {
    // D:96-102 — the point/axis constructor. D writes `Vector3([0.0, 0.0, 0.0])`,
    // the array form of the same three-component constructor.
    Vector3 axis0(0.0, 0.0, 0.0);
    Vector3 axis1(1.0, 0.0, 0.0);
    Vector3 pstart(0.0, 1.0, 0.0);
    Vector3 pend(1.0, 0.0, 1.0);
    auto h1 = std::make_shared<Helix>(pstart, pend, axis0, axis1);
    Vector3 p = (*h1)(0.5);
    CHECK3(eil::geom::approxEqualVectors(p, Vector3(0.5, 0.7071068, 0.7071068)),
           "D:102 approxEqualVectors(p, Vector3(0.5, 0.7071068, 0.7071068))");

    // D:103-111 — the fundamental-parameters constructor. `auto dtheta = PI/2;`
    // is an 80-bit real in D and narrows to double at the call (helix.d:28), so
    // the long-double EIL_PI is divided first and converted after.
    Vector3 a0(0.0, 0.0, 0.0);
    Vector3 a1(1.0, 0.0, 0.0);
    Vector3 xlocal(0.0, 1.0, 0.0);
    double r0 = 1.0;
    double r1 = 1.0;
    double dtheta = static_cast<double>(EIL_PI / 2.0L);
    auto h2 = std::make_shared<Helix>(a0, a1, xlocal, r0, r1, dtheta);
    Vector3 p2 = (*h2)(0.5);
    CHECK3(eil::geom::approxEqualVectors(p2, Vector3(0.5, 0.7071068, 0.7071068)),
           "D:111 approxEqualVectors(p2, ...) \"Helix from fundamental parameters\"");
}

// ============================================================================
// B. the probe replay, in probe order.
// ============================================================================
void dump_members(const Helix& h) {
    // probe_helix.d:57-62
    emitf("helix.members a0 x=%.17g y=%.17g z=%.17g a1 x=%.17g y=%.17g z=%.17g", h.a0.x, h.a0.y,
          h.a0.z, h.a1.x, h.a1.y, h.a1.z);
    emitf("helix.members xdsh x=%.17g y=%.17g z=%.17g ydsh x=%.17g y=%.17g z=%.17g "
          "zdsh x=%.17g y=%.17g z=%.17g r0=%.17g r1=%.17g theta01=%.17g",
          h.xdsh.x, h.xdsh.y, h.xdsh.z, h.ydsh.x, h.ydsh.y, h.ydsh.z, h.zdsh.x, h.zdsh.y,
          h.zdsh.z, h.r0, h.r1, h.theta01);
}

void replay_probe() {
    using namespace eil::geom;
    using namespace eil_test_gpath3;

    // probe_helix.d:36-41 — helix.d:96-102, the axis-based constructor.
    Vector3 axis0(0.0, 0.0, 0.0);
    Vector3 axis1(1.0, 0.0, 0.0);
    Vector3 pstart(0.0, 1.0, 0.0);
    Vector3 pend(1.0, 0.0, 1.0);
    auto h1 = std::make_shared<Helix>(pstart, pend, axis0, axis1);
    dump_path("helix.axis", h1);

    // probe_helix.d:45-53 — the fundamental-parameters constructor, dtheta = PI/2
    // printed as the 80-bit real D holds before the narrowing conversion.
    Vector3 a0(0.0, 0.0, 0.0);
    Vector3 a1(1.0, 0.0, 0.0);
    Vector3 xlocal(0.0, 1.0, 0.0);
    double r0 = 1.0;
    double r1 = 1.0;
    long double dtheta = EIL_PI / 2.0L;
    emitf("helix.consts dtheta=%.20Lg", dtheta);
    auto h2 = std::make_shared<Helix>(a0, a1, xlocal, r0, r1, static_cast<double>(dtheta));
    dump_path("helix.fund", h2);

    // probe_helix.d:56-63 — the members of both, side by side.
    dump_members(*h1);
    dump_members(*h2);

    // probe_helix.d:66-68 — a conical helix with more than one turn. `4.0 * PI` is
    // real arithmetic in D, narrowed on the way into the double parameter.
    auto h3 = std::make_shared<Helix>(Vector3(0.0, 0.0, 0.0), Vector3(0.0, 0.0, 2.0),
                                      Vector3(1.0, 0.0, 0.0), 0.5, 2.0,
                                      static_cast<double>(4.0L * EIL_PI));
    dump_path("helix.cone", h3);

    // probe_helix.d:71-72 — the copy constructor (D:61-67) and dup() (D:68-71).
    auto h3copy = std::make_shared<Helix>(*h3);
    dump_path("helix.copy", h3copy);
    dump_path("helix.dup", h3->dup());

    // probe_helix.d:75-78 — axis pointing along -z, pinning cross(zdsh, xdsh).
    auto h4 = std::make_shared<Helix>(Vector3(0.0, 1.0, 0.0), Vector3(1.0, 0.0, -3.0),
                                      Vector3(0.0, 0.0, 0.0), Vector3(0.0, 0.0, -3.0));
    dump_path("helix.down", h4);
    emitf("helix.str fund=%s", h2->toString().c_str());
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <probes/geom_gpath_helix.txt>\n";
        return 2;
    }
    port_d_unittest();
    replay_probe();
    return eil_test_gpath3::report("helix", argv[1]);
}
