// probes/gasdyn/probe_tol.d — pin what bare `isClose(a,b)` means in the D reference.
//
// Why: gasdyn/idealgasflow.d's six unittest blocks call std.math.isClose with no
// tolerance arguments (18 sites), so the defaults decide whether each ported
// assertion holds. std/math/operations.d:1250 takes
// maxRelDiff = CommonDefaultFor!(T,U) = 10.0L ^^ -((min(dig,dig)+1)/2 + 1), which
// for (double,double) reads as 1e-9, but the port must use the measured value.
//
// Build and capture (the reference does NOT compile with `-i`; see probe_idealgasflow.d):
//   /home/tang/packages/dlang/bin/ldc2 -w -g -dip1008 -I/home/tang/packages/gdtk/src \
//     -of=/tmp/probe_tol probes/gasdyn/probe_tol.d && /tmp/probe_tol
//
// Build and capture (reproducible; the gdtk tree itself is read-only):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip \
//     probes/gasdyn/probe_tol.d -of=/tmp/probe_tol -od/tmp/probe_obj_tol
//   /tmp/probe_tol > baselines/e587b235/probes/gasdyn_tol.txt
// No -O flag: every baseline in this repository is an ldc2 -O0 capture.
import std.stdio, std.math;

void main() {
    // Ladder of relative gaps at magnitude ~1, ~14.6 and ~0.64.
    foreach (d; [1.0e-10, 5.0e-10, 9.0e-10, 1.0e-9, 1.1e-9, 2.0e-9, 1.0e-8, 1.0e-7]) {
        writefln("relmag1 %.17g %s", d, isClose(1.0, 1.0 + d));
    }
    foreach (d; [1.0e-9, 1.0e-8, 1.0e-7, 1.0e-6, 1.0e-5, 1.2e-4, 2.0e-4]) {
        writefln("reldoc %.17g %s", d, isClose(14.62, 14.62 * (1.0 + d)));
    }
    // Absolute gaps at a small magnitude: distinguishes rel-only from rel+abs.
    foreach (a; [1.0e-9, 1.0e-8, 1.0e-6, 1.0e-5, 1.0e-4]) {
        writefln("absgap %.17g %s", a, isClose(0.0001, 0.0001 + a));
    }
    // The literature values the six unittest blocks actually compare against.
    writefln("pm_0641 %.17g %s", 0.64134795724885846, isClose(0.64134795724885846, 0.6413));
    writefln("p0_1462 %.17g %s", 14.621727489331607, isClose(14.621727489331607, 14.620));
}
