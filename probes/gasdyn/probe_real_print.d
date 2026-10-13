// probes/gasdyn/probe_real_print.d — how ldc2 prints a `real` through %g, and where
// idealgasflow's `x*180.0/PI` expressions change type.
//
// Why: idealgasflow_demo.d:81 writes `writeln("theta_c(deg)=", theta_c*180.0/PI, ...)`.
// Phobos' PI is `real`, so that quotient is an 80-bit value, and the demo baseline is
// text rendered from a real, not from a double. The port has to know whether D's
// %.6g / %.17g round the real to double first or print the wide value.
//
// Build and capture:
//   /home/tang/packages/dlang/bin/ldc2 -w -g -dip1008 -I/home/tang/packages/gdtk/src \
//     -od/tmp/gd_probe/obj -of=/tmp/gd_probe/probe_real_print \
//     probes/gasdyn/probe_real_print.d \
//     /home/tang/packages/gdtk/src/gasdyn/idealgasflow.d \
//     /home/tang/packages/gdtk/src/gasdyn/gasflowexception.d
//   /tmp/gd_probe/probe_real_print > baselines/e587b235/probes/gasdyn_real_print.txt
import std.stdio, std.math, std.format, std.conv;
import gasdyn.idealgasflow;

void main() {
    // A spread of wide quotients: degrees-from-radians, each computed in real
    // because PI is real, then printed three ways.
    double[] xs = [0.34851103526392313, 0.85574177381063774, 1.4835298641951802,
                   0.017453292519943295, 0.64134795719667381, 0.5];
    foreach (x; xs) {
        real wide = x * 180.0 / PI;
        double narrow = cast(double)wide;
        writefln("w17 %.17g", wide);
        writefln("d17 %.17g", narrow);
        writefln("e17 %.17e", wide);
        writefln("w6 %s", wide);
        writefln("d6 %s", narrow);
        writefln("w6g %.6g", wide);
        writefln("cmp %s", wide == narrow);
        // Also the 180/PI form with an int multiplier, as beta*180/PI in the demo.
        writefln("w17b %.17g", x * 180 / PI);
    }
    // A real that is NOT representable as the same-rounded double, to see whether
    // %.17g prints wide digits: PI itself and a wide product.
    writefln("pi17 %.17g", PI);
    writefln("pi6 %s", PI);
    writefln("pid17 %.17g", cast(double)PI);
    writefln("bigg %.17g %.17g", sqrt(real(2.0)/real(3.0)), cast(double)sqrt(real(2.0)/real(3.0)));
    // What the demo actually prints for the cone angle.
    double V1 = 1.5 * sqrt(1.4*287*300.0);
    double[] r = theta_cone(V1, 100.0e3, 300.0, 49.0*PI/180);
    writefln("cone %.17g %s", r[0]*180.0/PI, r[0]*180.0/PI);
    writefln("coneD %.17g", cast(double)(r[0]*180.0/PI));
}
