// tests/demo_geom_elements_vector3.cpp
//
// Port of geom/elements/vector3_demo.d (63 lines). stdout is byte-compared against
// the D binary's captured text, baselines/e587b235/probes/geom_elements_vector3_demo.txt,
// through harness/compare_text.sh (CTest name demo_equiv_geom_elements_vector3).
//
// D source order is preserved line for line. Two spelling differences, both
// behaviour-preserving:
//   * D's `import geom;` becomes the module headers the demo actually resolves
//     (geom.elements.vector3 and geom.elements.projection); the aggregate package
//     header has no C++ counterpart (wave-02-brief section 4 item 5).
//   * `writeln(a)` finds Vector3.toString() through D's opPrint; here each print
//     site calls .toString() explicitly, which emits the same bytes (the same
//     convention as tests/demo_nm_bbla.cpp:12-13).
//
// project_onto_plane is the reason this demo sits in this stage rather than with
// vector3: it is defined in projection.d:45.

#include <cstdio>

#include "eil/eil.hpp"
#include "geom/elements/projection.hpp"
#include "geom/elements/vector3.hpp"

using eil::geom::Vector3;
using eil::number;

int main() {
    eil::writeln("Begin demonstration of the geometric primitives.");
    Vector3 a = Vector3(eil::Array<number>{1.0, 2.2, 3.0}.all());
    Vector3 b = Vector3(1.0);
    eil::writeln("a = ", a.toString(), ", b = ", b.toString());
    Vector3 c = a + b;
    eil::writeln("c = a + b = ", c.toString());
    Vector3 d = a;
    a.y = 99.0;
    eil::writeln("a = ", a.toString(), ", d = ", d.toString());
    Vector3 e = a * 2.0;
    Vector3 f = 3 * d; // int promoted to double OK
    eil::writeln("e = ", e.toString(), " f = ", f.toString());
    Vector3 g = d / 3.0;
    eil::writeln("g = ", g.toString());
    g += f;
    eil::writeln("g += f -> ", g.toString());
    g /= 2.0;
    eil::writeln("g /= 2.0 -> ", g.toString());
    Vector3 gg = Vector3(g * 2);
    Vector3 u = eil::geom::unit(gg);
    eil::writeln("unit(g*2) = ", u.toString(), " magnitude = ", eil::to_string(eil::geom::abs(u)));
    Vector3 x = Vector3(1.0, 0.0, 0.0);
    Vector3 y = Vector3(0.0, 1.0, 0.0);
    Vector3 z = eil::geom::cross(x, y);
    eil::writeln("z = ", z.toString());

    Vector3 n = Vector3(1.0, 1.0, 0.0); n = eil::geom::unit(n);
    Vector3 t1 = Vector3(-1.0, 1.0, 0.0); t1 = eil::geom::unit(t1);
    Vector3 t2 = eil::geom::cross(n, t1);
    Vector3 h = Vector3(1.0, 0.0, 1.0);
    Vector3 h_ref = Vector3(h);
    (void)h_ref; // D keeps this copy and never reads it either (vector3_demo.d:46)
    eil::writeln("original h = ", h.toString());
    h.transform_to_local_frame(n, t1, t2);
    eil::writeln("in local frame h = ", h.toString());
    h.transform_to_global_frame(n, t1, t2);
    eil::writeln("back to global frame h = ", h.toString());

    eil::writeln("Try out geometric functions that build on Vector3 objects.");
    a = Vector3(1.0, 0.0, 0.0); // plane through a,b,c
    b = Vector3(1.0, 1.0, 0.0);
    c = Vector3(0.5, 0.0, 0.0);
    Vector3 qr = Vector3(3.0, 3.0, -3.0); // direction
    Vector3 q = Vector3(0.0, 0.0, 1.0); // start point
    int flag = eil::geom::project_onto_plane(q, qr, a, b, c);
    (void)flag; // D discards the return code here too (vector3_demo.d:59)
    eil::writeln("projected point q = ", q.toString());

    eil::writeln("Done.");
    return 0;
}
