// probes/geom/gpath/probe_helix.d — what geom/gpath/helix.d produces.
//
// Helix has two constructors that derive the same eight members by different
// routes (helix.d:27-37 from the fundamental parameters, :41-60 from
// start/end/axis points), so both are dumped and compared: the four-point route
// is the one paver.d uses and it involves two unit() calls, three dot() calls and
// an atan2, and a port that reordered them would differ in the last bits.
//
// The D unittest asserts both routes give the same point at t = 0.5 to
// approxEqualVectors (2% relative), which is a loose gate; the rows below print
// the two parameter sets at %.17g so any difference is visible, not just
// tolerated.
//
// See probe_path.d for why the reference tree is copied before building.
//
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/gpath \
//     probes/geom/gpath/probe_helix.d -of=/tmp/x && /tmp/x \
//     > baselines/e587b235/probes/geom_gpath_helix.txt
import std.conv;
import std.format;
import std.math;
import std.stdio;

import geom.elements;
import geom.gpath.path;
import geom.gpath.helix;
import nm.number;
import pathdump;

void main()
{
    // helix.d:96-102 — the axis-based constructor.
    auto axis0 = Vector3([0.0, 0.0, 0.0]);
    auto axis1 = Vector3([1.0, 0.0, 0.0]);
    auto pstart = Vector3([0.0, 1.0, 0.0]);
    auto pend = Vector3([1.0, 0.0, 1.0]);
    auto h1 = new Helix(pstart, pend, axis0, axis1);
    dump_path("helix.axis", h1);

    // helix.d:103-111 — the fundamental-parameters constructor, with dtheta =
    // PI/2 computed in 80-bit real and rounded on the way into the double member.
    auto a0 = Vector3([0.0, 0.0, 0.0]);
    auto a1 = Vector3([1.0, 0.0, 0.0]);
    auto xlocal = Vector3([0.0, 1.0, 0.0]);
    auto r0 = 1.0;
    auto r1 = 1.0;
    auto dtheta = PI / 2;
    writefln("helix.consts dtheta=%.20g", dtheta);
    auto h2 = new Helix(a0, a1, xlocal, r0, r1, dtheta);
    dump_path("helix.fund", h2);

    // The members of both, side by side.
    foreach (h; [h1, h2]) {
        writefln("helix.members a0 x=%.17g y=%.17g z=%.17g a1 x=%.17g y=%.17g z=%.17g",
                 h.a0.x, h.a0.y, h.a0.z, h.a1.x, h.a1.y, h.a1.z);
        writefln("helix.members xdsh x=%.17g y=%.17g z=%.17g ydsh x=%.17g y=%.17g z=%.17g "
                 ~ "zdsh x=%.17g y=%.17g z=%.17g r0=%.17g r1=%.17g theta01=%.17g",
                 h.xdsh.x, h.xdsh.y, h.xdsh.z, h.ydsh.x, h.ydsh.y, h.ydsh.z,
                 h.zdsh.x, h.zdsh.y, h.zdsh.z, h.r0, h.r1, h.theta01);
    }

    // A conical helix (r0 != r1) and a multi-turn one (dtheta > 2*PI).
    auto h3 = new Helix(Vector3(0.0, 0.0, 0.0), Vector3(0.0, 0.0, 2.0),
                        Vector3(1.0, 0.0, 0.0), 0.5, 2.0, 4.0 * PI);
    dump_path("helix.cone", h3);

    // Copy constructor and dup.
    dump_path("helix.copy", new Helix(h3));
    dump_path("helix.dup", h3.dup());

    // A helix whose axis points along -z, to pin the cross-product handedness.
    auto h4 = new Helix(Vector3(0.0, 1.0, 0.0), Vector3(1.0, 0.0, -3.0),
                        Vector3(0.0, 0.0, 0.0), Vector3(0.0, 0.0, -3.0));
    dump_path("helix.down", h4);
    writefln("helix.str fund=%s", h2.toString());
}
