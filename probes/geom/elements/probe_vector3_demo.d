// probes/geom/elements/probe_vector3_demo.d — the D reference for
// tests/demo_geom_elements_vector3.cpp.
//
// This is geom/elements/vector3_demo.d with ONE change, on line 12: its
// `import geom;` is replaced by the four module imports the demo actually uses
// (the aggregate header has no C++ counterpart, wave-02-brief section 4 item 5).
// `import geom;` also cannot be built with `-i` at all: pulling in geom.grid,
// geom.misc and geom.gpath surfaces @nogc/`new` errors that are not in this
// stage's scope (sgrid.d:335, grid.d:144, univariatefunctions.d:603, bbla.d:709
// via spline.d:51). The demo's stdout depends only on geom.elements.*, and D's
// package import adds no declarations the demo could resolve differently, so the
// captured text is the same either way.
//
// projection.d is compiled from a shadow copy because projection.d:136 marks the
// four-point barycentricCoords @nogc while :154 throws `new GeometryException`,
// which ldc2 rejects whenever that body is analysed (-i or -unittest). The demo
// never calls it; deleting the token changes no value.
//
// Build and capture:
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   mkdir -p /tmp/shadow/geom/elements
//   sed '136{/^@nogc$/d}' /home/tang/packages/gdtk/src/geom/elements/projection.d \
//     > /tmp/shadow/geom/elements/projection.d
//   ldc2 -i -I/tmp/shadow -I/home/tang/packages/gdtk/src \
//     probes/geom/elements/probe_vector3_demo.d -of=/tmp/v3demo_d && /tmp/v3demo_d \
//     > baselines/e587b235/probes/geom_elements_vector3_demo.txt
/**
 * vector3_demo.d
 * Demonstrate some of the behaviour of the geometric primitives.
 *
 * Author: Peter J.
 * Version:
 *  2014-06-16
 *  2017-11-26: Refactor package.
 */

import std.stdio;
import geom.geometry_exception;
import geom.elements.nomenclature;
import geom.elements.properties;
import geom.elements.projection;
import geom.elements.vector3;

void main()
{
    writeln("Begin demonstration of the geometric primitives.");
    Vector3 a = Vector3([1.0, 2.2, 3.0]);
    Vector3 b = Vector3(1.0);
    writeln("a = ", a, ", b = ", b);
    Vector3 c = a + b;
    writeln("c = a + b = ", c);
    Vector3 d = a;
    a.y = 99.0;
    writeln("a = ", a, ", d = ", d);
    Vector3 e = a * 2.0;
    Vector3 f = 3 * d; // int promoted to double OK
    writeln("e = ", e, " f = ", f);
    Vector3 g = d / 3.0;
    writeln("g = ", g);
    g += f;
    writeln("g += f -> ", g);
    g /= 2.0;
    writeln("g /= 2.0 -> ", g);
    Vector3 gg = Vector3(g*2);
    Vector3 u = unit(gg);
    writeln("unit(g*2) = ", u, " magnitude = ", abs(u));
    Vector3 x = Vector3(1.0, 0.0, 0.0);
    Vector3 y = Vector3(0.0, 1.0, 0.0);
    Vector3 z = cross(x,y);
    writeln("z = ", z);

    Vector3 n = Vector3(1.0,1.0,0.0); n = unit(n);
    Vector3 t1 = Vector3(-1.0,1.0,0.0); t1 = unit(t1);
    Vector3 t2 = cross(n, t1);
    Vector3 h = Vector3(1.0,0.0,1.0);
    Vector3 h_ref = Vector3(h);
    writeln("original h = ", h);
    h.transform_to_local_frame(n, t1, t2);
    writeln("in local frame h = ", h);
    h.transform_to_global_frame(n, t1, t2);
    writeln("back to global frame h = ", h);

    writeln("Try out geometric functions that build on Vector3 objects.");
    a = Vector3(1.0, 0.0, 0.0); // plane through a,b,c
    b = Vector3(1.0, 1.0, 0.0);
    c = Vector3(0.5, 0.0, 0.0);
    Vector3 qr = Vector3(3.0, 3.0, -3.0); // direction
    Vector3 q = Vector3(0.0, 0.0, 1.0); // start point
    int flag =  project_onto_plane(q, qr, a, b, c);
    writeln("projected point q = ", q);

    writeln("Done.");
}
