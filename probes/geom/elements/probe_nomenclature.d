// probes/geom/elements/probe_nomenclature.d — pin the measured semantics of
// geom/elements/nomenclature.d before it is ported.
//
// What this probe decides:
//   1. `enum Face` has deliberately aliased enumerators (west == iminus == 0, ...),
//      so both spellings must survive into C++ and both must print the same uint.
//   2. `string[] face_name` is a mutable module global. Its length, element text and
//      whole-array rendering ("[west, east, south, north, bottom, top]") are what
//      the ported printing has to reproduce.
//   3. `face_index` throws `new Error(text("Invalid face name: ", name))` — a D
//      *Error*, not an Exception. The exact message bytes and the fact that
//      `catch (Exception e)` does NOT catch it are measured here (wave-1 lesson 1).
//   4. `double radians(double degrees) { return PI*degrees/180.0; }` computes in
//      D's 80-bit real and rounds only on return, so it is NOT (M_PI*deg/180).
//      The probe prints radians() beside the pure-double form and counts where they
//      differ, and beside the same expression computed wide then cast.
//   5. `alias Quaternion = double[4]` is a D *static* array: `Quaternion q;` is
//      all-zero (static arrays carry .init), which std::array<T,4> does not do.
//
// Build and capture (gdtk is read-only; the shadow dir only adds -unittest hooks):
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i -I/home/tang/packages/gdtk/src probes/geom/elements/probe_nomenclature.d \
//     -of=/tmp/nomen_d && /tmp/nomen_d \
//     > baselines/e587b235/probes/geom_elements_nomenclature.txt
import std.conv;
import std.format;
import std.math;
import std.stdio;

import geom.elements.nomenclature;

void main()
{
    writeln("== Face enumerators, both spellings ==");
    writefln("west=%u iminus=%u", Face.west, Face.iminus);
    writefln("east=%u iplus=%u", Face.east, Face.iplus);
    writefln("south=%u jminus=%u", Face.south, Face.jminus);
    writefln("north=%u jplus=%u", Face.north, Face.jplus);
    writefln("bottom=%u kminus=%u", Face.bottom, Face.kminus);
    writefln("top=%u kplus=%u", Face.top, Face.kplus);

    writeln("== face_index over all 12 accepted names ==");
    string[] names = [ "west", "iminus", "east", "iplus", "south", "jminus",
                       "north", "jplus", "bottom", "kminus", "top", "kplus" ];
    foreach (n; names) {
        writefln("face_index(%s)=%u", n, face_index(n));
    }

    writeln("== face_name global ==");
    writefln("length=%u", face_name.length);
    foreach (i, f; face_name) {
        writefln("face_name[%u]=%s", i, f);
    }
    writefln("array=%s", face_name);
    writefln("roundtrip=%u %u %u %u %u %u",
             face_index(face_name[0]), face_index(face_name[1]),
             face_index(face_name[2]), face_index(face_name[3]),
             face_index(face_name[4]), face_index(face_name[5]));

    writeln("== face_index failure path: which catch clause fires ==");
    foreach (bad; ["northwest", "", "West"]) {
        bool caughtException = false;
        bool caughtError = false;
        string msg;
        try {
            face_index(bad);
        } catch (Exception e) {
            caughtException = true;
            msg = e.msg;
        } catch (Error e) {
            caughtError = true;
            msg = e.msg;
        }
        writefln("bad[%s] asException=%s asError=%s msg=%s", bad,
                 to!string(caughtException), to!string(caughtError), msg);
    }

    writeln("== radians: 80-bit PI, rounded on return ==");
    double[] deg = [ 0.0, 30.0, 45.0, 60.0, 90.0, 180.0, 270.0, 360.0, 0.1,
                     1.0/3.0, 123.456, 1.0e-8, 1.0e8, 7.0 ];
    foreach (d; deg) {
        double wide = radians(d);                       // D: real math, double store
        double narrow = PI_double_form(d);              // double PI * d / 180.0
        writefln("rad %.17g d17=%.17g narrow17=%.17g same=%s d6=%s tostr=%s",
                 d, wide, narrow, to!string(wide == narrow),
                 form6(wide), to!string(wide));
    }
    writefln("PI_bits %.17g %.17g", cast(real)PI, cast(double)PI);
    writefln("PI4 %.17g %.17g", cast(double)(PI/4.0), radians(45.0));

    writeln("== VTKElement values ==");
    writefln("vertex=%u polyvertex=%u line=%u polyline=%u",
             VTKElement.vertex, VTKElement.polyvertex, VTKElement.line,
             VTKElement.polyline);
    writefln("triangle=%u triangle_strip=%u polygon=%u pixel=%u",
             VTKElement.triangle, VTKElement.triangle_strip, VTKElement.polygon,
             VTKElement.pixel);
    writefln("quad=%u tetra=%u voxel=%u hexahedron=%u",
             VTKElement.quad, VTKElement.tetra, VTKElement.voxel,
             VTKElement.hexahedron);
    writefln("wedge=%u pyramid=%u", VTKElement.wedge, VTKElement.pyramid);

    writeln("== Quaternion is a static double[4] ==");
    writefln("quaternion_init=%s", text("[", to!string(Quaternion.init[0]), ",",
             to!string(Quaternion.init[1]), "]"));
    Quaternion q;
    writefln("default_local=%s", q);
    Quaternion q2 = [ 1.0, 2.0, 3.0, 4.0 ];
    writefln("assigned=%s len=%u", q2, q2.length);
}

// The pure-double form a naive C++ port would write: (M_PI*degrees/180.0).
double PI_double_form(double d)
{
    double pi = 3.141592653589793;
    return pi*d/180.0;
}

string form6(double v) { return format("%.6g", v); }
