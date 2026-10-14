// probes/gasdyn/probe_ds_cv.d — isolate the ds_Cv 1-ULP chain with the real D module.
//
// Build (explicit file list, NOT -i: idealgasflow.d:760 instantiates bbla.s
// gaussJordanElimination, whose @nogc `new` lines make -i fail — see
// probes/gasdyn/probe_idealgasflow.d for the full explanation):
//   ldc2 -w -g -dip1008 -I/home/tang/packages/gdtk/src -od/tmp/gd_probe/obj \
//     -of=/tmp/dscv probes/gasdyn/probe_ds_cv.d \
//     /home/tang/packages/gdtk/src/gasdyn/idealgasflow.d \
//     /home/tang/packages/gdtk/src/gasdyn/gasflowexception.d
//
// Capture (the build line above, run from the repo root):
//   /tmp/dscv > baselines/e587b235/probes/gasdyn_ds_cv.txt
// No -O flag: every baseline in this repository is an ldc2 -O0 capture. The file was
// missing until 2026-10-14, which is why the ds_Cv chain's 1-ULP finding lived only in
// prose; both rows are deterministic across runs (verified twice).
module probe_ds_cv;

import std.stdio, std.math;
import gasdyn.idealgasflow;

void main() {
    foreach (M1; [1.2, 1.3]) {
        double g = 1.2;
        double t1 = p2_p1(M1, g);
        double t2 = r2_r1(M1, g);
        double pw = pow(t2, g);
        double prod = t1 * pw;
        writefln("M=%.17g t1=%.17g t2=%.17g pow=%.17g prod=%.17g log=%.17g ds=%.17g",
                 M1, t1, t2, pw, prod, log(prod), ds_Cv(M1, g));
    }
}
