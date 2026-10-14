// probes/geom/misc/probe_sketch.d — dump what geom/misc/sketch.d actually produces.
//
// WHY THIS SHAPE. sketch.d has NO unittest block (measured: `grep -c unittest` over the
// module is 0), so the whole gate for T-019 is this corpus plus the demo's byte
// comparison. The corpus is therefore built to be replayed line for line by
// tests/test_geom_misc_sketch.cpp: every row is either a raw IEEE-754 bit pattern of a
// quantity the module computes, or a line of an SVG document the module emitted.
//
//   S1  the two literal expressions of sketch.d:128-129 and :136, as folded by ldc2 and
//       as computed at run time, side by side (T-909/T-920). The decisive rows are the
//       proj_mat dumps in S4, because those are what the module really stores.
//   S2  Extents: the .init state and set()'s width/height arithmetic.
//   S3  construction: toString(), canvas/viewport, the three Exception sites, the
//       Error/Exception sibling proof (wave-01 integration lesson 1).
//   S4  proj_mat for all three projections and view_mat after two look_at calls.
//   S5  apply_transform / matrix_vector_multiply over a point set.
//   S6  toCanvasX / toCanvasY.
//   S7  seven documents: the three of sketch_demo.d (reproduced call for call, in the
//       same order, on the same continuing Sketch object), a document that reaches every
//       remaining primitive and every renderer-independent branch, an oblique-projection
//       document, and a rule-coverage document (the to!int tick-count truncation, the
//       "a lot of tic marks" writeln, a reversed range whose loops must run zero times,
//       and the "invalid direction" Exception).
//
// Documents are dumped AFTER all drawing is finished (the SVG File buffer and stdout
// must not interleave), so stdout rows appear in program order and the dump order is
// the order of these calls.
//
// Build and capture (reproducible; the gdtk tree itself is read-only):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip \
//     probes/geom/misc/probe_sketch.d -of=/tmp/probe_sk -od/tmp/probe_obj_sk
//   mkdir -p /tmp/skwork && cd /tmp/skwork && /tmp/probe_sk \
//     > /home/tang/packages/Eilmer/baselines/e587b235/probes/geom_misc_sketch.txt
// NO -O flag: every baseline in this repository is an ldc2 -O0 capture, and -O0 is the
// contract (T-919/T-920 were both caused by reading a higher-opt level's behaviour as
// the language's).

import std.array : split;
import std.conv;
import std.file : readText;
import std.format;
import std.math;
import std.stdio;

import geom;

string bits(double x)
{
    return format("%016x", *cast(ulong*)&x);
}

void pv(const string tag, double x)
{
    writefln("%s v=%.17g bits=%s", tag, x, bits(x));
}

void pvm(const string tag, double[4][4] m)
{
    foreach (i; 0 .. 4) {
        writefln("%s r%d %.17g/%s %.17g/%s %.17g/%s %.17g/%s",
                 tag, i,
                 m[i][0], bits(m[i][0]), m[i][1], bits(m[i][1]),
                 m[i][2], bits(m[i][2]), m[i][3], bits(m[i][3]));
    }
}

void pvec(const string tag, double[4] v)
{
    writefln("%s %.17g/%s %.17g/%s %.17g/%s %.17g/%s",
             tag, v[0], bits(v[0]), v[1], bits(v[1]), v[2], bits(v[2]), v[3], bits(v[3]));
}

/// Print one emitted document: one stdout line per document line, and no trailing
/// empty line for the final newline (the rule probe_svg.d established).
void dump_doc(string name)
{
    string text = readText(name);
    auto lines = split(text, "\n");
    if (lines.length > 0 && lines[$ - 1].length == 0) lines = lines[0 .. $ - 1];
    writefln("--- doc %s bytes=%d ---", name, text.length);
    foreach (l; lines) writeln(l);
}

void build_row(string rname, string pname)
{
    try {
        auto s = new Sketch(rname, pname);
        writefln("s3 build [%s]/[%s] ok=1 str=[%s]", rname, pname, s.toString());
    }
    catch (Exception e) {
        writefln("s3 build [%s]/[%s] ok=0 isError=0 msg=[%s]", rname, pname, e.msg);
    }
    catch (Error e) {
        writefln("s3 build [%s]/[%s] ok=0 isError=1 msg=[%s]", rname, pname, e.msg);
    }
}

Vector3 V3(double x, double y, double z=0.0) { return Vector3(x, y, z); }

void main()
{
    // ==================================================================== S1
    // sketch.d:128-129 — cos and sin of ONE literal argument, and :136's
    // 1.0/(2.0*sqrt(2.0)). Both the folded form (what ldc2 hands the module) and a
    // run-time form (operands in variables, so nothing can be folded) are printed.
    pv("s1 arg_30_180_PI folded", 30.0/180.0*PI);
    double l30 = 30.0, l180 = 180.0;
    double lpi = PI;
    double argRun = l30/l180*lpi;
    pv("s1 arg_30_180_PI runtime", argRun);
    pv("s1 cos folded", cos(30.0/180.0*PI));
    pv("s1 sin folded", sin(30.0/180.0*PI));
    double cosPair = cos(30.0/180.0*PI);
    double sinPair = sin(30.0/180.0*PI);
    pv("s1 cos folded stmt", cosPair);
    pv("s1 sin folded stmt", sinPair);
    pv("s1 cos of runtime arg", cos(argRun));
    pv("s1 sin of runtime arg", sin(argRun));
    pv("s1 zfactor folded", 1.0/(2.0*sqrt(2.0)));
    double two = 2.0;
    pv("s1 zfactor runtime", 1.0/(two*sqrt(two)));
    pv("s1 dpi", dpi);

    // ==================================================================== S2
    Extents e;
    pv("s2 init x0", e.x0);
    pv("s2 init x1", e.x1);
    pv("s2 init y0", e.y0);
    pv("s2 init y1", e.y1);
    pv("s2 init width", e.width);
    pv("s2 init height", e.height);
    e.set(0.0, 0.0, 120.0, 120.0);
    pv("s2 a x0", e.x0);
    pv("s2 a x1", e.x1);
    pv("s2 a y0", e.y0);
    pv("s2 a y1", e.y1);
    pv("s2 a width", e.width);
    pv("s2 a height", e.height);
    e.set(-1.5, 2.25, 3.0, 4.0);
    pv("s2 b x0", e.x0);
    pv("s2 b x1", e.x1);
    pv("s2 b y0", e.y0);
    pv("s2 b y1", e.y1);
    pv("s2 b width", e.width);
    pv("s2 b height", e.height);

    // ==================================================================== S3
    auto sd = new Sketch();
    writefln("s3 default str=[%s]", sd.toString());
    pv("s3 default canvas x0", sd.canvas.x0);
    pv("s3 default canvas x1", sd.canvas.x1);
    pv("s3 default canvas y0", sd.canvas.y0);
    pv("s3 default canvas y1", sd.canvas.y1);
    pv("s3 default canvas width", sd.canvas.width);
    pv("s3 default canvas height", sd.canvas.height);
    pv("s3 default viewport x0", sd.viewport.x0);
    pv("s3 default viewport x1", sd.viewport.x1);
    pv("s3 default viewport y0", sd.viewport.y0);
    pv("s3 default viewport y1", sd.viewport.y1);
    pv("s3 default viewport width", sd.viewport.width);
    pv("s3 default viewport height", sd.viewport.height);
    writefln("s3 default title=[%s] desc=[%s]", sd.title, sd.description);
    auto s210 = new Sketch("svg", "xyortho", [0.0, 0.0, 210.0, 297.0]);
    pv("s3 210x297 canvas x0", s210.canvas.x0);
    pv("s3 210x297 canvas y0", s210.canvas.y0);
    pv("s3 210x297 canvas x1", s210.canvas.x1);
    pv("s3 210x297 canvas y1", s210.canvas.y1);
    pv("s3 210x297 width", s210.canvas.width);
    pv("s3 210x297 height", s210.canvas.height);
    auto si = new Sketch("svg", "isometric");
    writefln("s3 iso str=[%s]", si.toString());
    auto so = new Sketch("svg", "oblique");
    writefln("s3 obl str=[%s]", so.toString());
    build_row("svg", "xyortho");
    build_row("svg", "isometric");
    build_row("svg", "oblique");
    build_row("xplot", "xyortho"); // the with_libplot else-branch Exception (:165)
    build_row("SVG", "xyortho"); // case-sensitive name list
    build_row("tgif", "xyortho");
    build_row("svg", "iso");
    build_row("svg", "Orthographic");
    // Exception and Error are siblings in D, and must stay siblings in the port.
    try {
        throw new Exception("s3 probe exception");
    }
    catch (Exception ex) {
        writefln("s3 sibling exception caughtByException=1 msg=[%s]", ex.msg);
    }
    catch (Error ex) {
        writefln("s3 sibling exception caughtByError=1 msg=[%s]", ex.msg);
    }
    try {
        throw new Error("s3 probe error");
    }
    catch (Exception ex) {
        writefln("s3 sibling error caughtByException=1 msg=[%s]", ex.msg);
    }
    catch (Error ex) {
        writefln("s3 sibling error caughtByError=1 msg=[%s]", ex.msg);
    }

    // ==================================================================== S4
    pvm("s4 proj xyortho", sd.proj_mat);
    pvm("s4 proj isometric", si.proj_mat);
    pvm("s4 proj oblique", so.proj_mat);
    pvm("s4 view default", sd.view_mat);
    // The demo's look_at (sketch_demo.d:61), then a second one that exercises
    // normalize/cross/dot on non-axis-aligned input.
    si.look_at(Vector3(0.0,0.0,-1.0), Vector3(0.0,0.0,0.0), Vector3(0.0,1.0,0.0));
    pvm("s4 view lookat1", si.view_mat);
    pv("s4 lookat1 eye x", si.eye.x.re);
    pv("s4 lookat1 eye y", si.eye.y.re);
    pv("s4 lookat1 eye z", si.eye.z.re);
    pv("s4 lookat1 centre x", si.centre.x.re);
    pv("s4 lookat1 centre y", si.centre.y.re);
    pv("s4 lookat1 centre z", si.centre.z.re);
    pv("s4 lookat1 up x", si.up.x.re);
    pv("s4 lookat1 up y", si.up.y.re);
    pv("s4 lookat1 up z", si.up.z.re);
    sd.look_at(Vector3(1.0,2.0,3.0), Vector3(0.5,-0.5,0.0), Vector3(0.0,0.0,1.0));
    pvm("s4 view lookat2", sd.view_mat);
    pv("s4 lookat2 eye x", sd.eye.x.re);
    pv("s4 lookat2 eye y", sd.eye.y.re);
    pv("s4 lookat2 eye z", sd.eye.z.re);
    pv("s4 lookat2 centre x", sd.centre.x.re);
    pv("s4 lookat2 centre y", sd.centre.y.re);
    pv("s4 lookat2 centre z", sd.centre.z.re);
    pv("s4 lookat2 up x", sd.up.x.re);
    pv("s4 lookat2 up y", sd.up.y.re);
    pv("s4 lookat2 up z", sd.up.z.re);

    // ==================================================================== S5
    double[4] vm1 = [1.0, 2.0, 3.0, 1.0];
    si.matrix_vector_multiply(si.proj_mat, vm1);
    pvec("s5 mvm isometric", vm1);
    double[4] vm2 = [1.0, 2.0, 3.0, 1.0];
    so.matrix_vector_multiply(so.proj_mat, vm2);
    pvec("s5 mvm oblique", vm2);
    double[4] vm3 = [1.0, 2.0, 3.0, 1.0];
    sd.matrix_vector_multiply(sd.proj_mat, vm3);
    pvec("s5 mvm xyortho", vm3);
    const double[3][3] pts = [[0.0, 0.0, 1.0], [1.0, 2.0, 3.0], [-2.0, 0.5, 4.0]];
    foreach (k, q; pts) {
        auto sk = new Sketch("svg", "xyortho");
        Vector3 p = V3(q[0], q[1], q[2]);
        double w = 1.0;
        sk.apply_transform("view", p, w);
        sk.apply_transform("projection", p, w);
        pv(format("s5 xy p%d x", k), p.x.re);
        pv(format("s5 xy p%d y", k), p.y.re);
        pv(format("s5 xy p%d z", k), p.z.re);
        pv(format("s5 xy p%d w", k), w);
    }
    foreach (k, q; pts) {
        auto sk = new Sketch("svg", "isometric");
        Vector3 p = V3(q[0], q[1], q[2]);
        double w = 1.0;
        sk.apply_transform("view", p, w);
        sk.apply_transform("projection", p, w);
        pv(format("s5 iso p%d x", k), p.x.re);
        pv(format("s5 iso p%d y", k), p.y.re);
        pv(format("s5 iso p%d z", k), p.z.re);
        pv(format("s5 iso p%d w", k), w);
    }
    foreach (k, q; pts) {
        auto sk = new Sketch("svg", "oblique");
        Vector3 p = V3(q[0], q[1], q[2]);
        double w = 1.0;
        sk.apply_transform("view", p, w);
        sk.apply_transform("projection", p, w);
        pv(format("s5 obl p%d x", k), p.x.re);
        pv(format("s5 obl p%d y", k), p.y.re);
        pv(format("s5 obl p%d z", k), p.z.re);
        pv(format("s5 obl p%d w", k), w);
    }
    // view THEN projection, with the demo's look_at in place.
    foreach (k, q; pts) {
        auto sk = new Sketch("svg", "isometric");
        sk.look_at(Vector3(0.0,0.0,-1.0), Vector3(0.0,0.0,0.0), Vector3(0.0,1.0,0.0));
        Vector3 p = V3(q[0], q[1], q[2]);
        double w = 1.0;
        sk.apply_transform("view", p, w);
        sk.apply_transform("projection", p, w);
        pv(format("s5 isoview p%d x", k), p.x.re);
        pv(format("s5 isoview p%d y", k), p.y.re);
        pv(format("s5 isoview p%d z", k), p.z.re);
        pv(format("s5 isoview p%d w", k), w);
    }
    // The default arm of apply_transform's switch: nothing happens (:227-228).
    {
        auto sk = new Sketch("svg", "isometric");
        Vector3 p = V3(1.0, 2.0, 3.0);
        double w = 1.0;
        sk.apply_transform("shear", p, w);
        pv("s5 bogus x", p.x.re);
        pv("s5 bogus y", p.y.re);
        pv("s5 bogus z", p.z.re);
        pv("s5 bogus w", w);
    }

    // ==================================================================== S6
    {
        auto sk = new Sketch();
        sk.viewport.set(-2.0, -2.0, 2.0, 2.0);
        foreach (u; [-2.0, -1.0, 0.0, 1.0, 2.0, 3.0]) {
            pv(format("s6 cx %.17g", u), sk.toCanvasX(u));
            pv(format("s6 cy %.17g", u), sk.toCanvasY(u));
        }
    }
    {
        auto sk = new Sketch(); // viewport (0,0,1,1) straight from the constructor
        foreach (u; [0.0, 0.25, 0.5, 1.0]) {
            pv(format("s6 dflt cx %.17g", u), sk.toCanvasX(u));
            pv(format("s6 dflt cy %.17g", u), sk.toCanvasY(u));
        }
    }
    {
        auto sk = new Sketch("svg", "xyortho", [10.0, 20.0, 210.0, 297.0]);
        sk.viewport.set(-1.0, -1.0, 1.0, 1.0);
        foreach (u; [-1.0, 0.0, 0.5, 1.0]) {
            pv(format("s6 off cx %.17g", u), sk.toCanvasX(u));
            pv(format("s6 off cy %.17g", u), sk.toCanvasY(u));
        }
    }

    // ==================================================================== S7
    // ---- D1: sketch_demo.d's first document, call for call ---------------
    auto s = new Sketch("svg", "xyortho", [0.0,0.0,120.0,120.0]);
    s.viewport.set(-2.0,-2.0,2.0,2.0);
    auto a = Vector3(2.0, 0.0);
    auto b = Vector3(1.0, 1.0);
    auto c = Vector3(1.0, 0.0);
    auto abc = new Arc(a, b, c);
    s.start("skprobe_test.svg");
    s.line(Vector3(-1.0,1.0,0.0), Vector3(1.0,-1.0,0.0));
    s.render(abc);
    s.rule("x", -1.2, 1.2, 0.4, Vector3(0.0,-1.3), 0.03, "%.1f", 0.12, 0.0, 8);
    s.rule("y", -1.2, 1.2, 0.4, Vector3(-1.3,0.0), 0.03, "%.1f", 0.06, 0.0, 8);
    s.setFillColour("green");
    auto p00 = Vector3(0.0, 0.1);
    auto p10 = Vector3(1.0, 0.1);
    auto p11 = Vector3(1.0, 1.1);
    auto p01 = Vector3(0.0, 1.1);
    auto my_patch = new CoonsPatch(p00, p10, p11, p01);
    s.dotlabel(p00, "p00"); s.dotlabel(p10, "p10");
    s.dotlabel(p11, "p11"); s.dotlabel(p01, "p01");
    s.render(my_patch);
    s.finish();

    // ---- D2: the same Sketch object, second document (state carries) -----
    s.start("skprobe_test2.svg");
    s.viewport.set(0.0,0.0,120.0,120.0);
    s.line(Vector3(0.0,0.0), Vector3(90.0,120.0));
    s.setLineWidth(0.5);
    s.setFillColour("yellow");
    s.circle(Vector3(25.0,85.0), 12.3);
    s.setLineWidth(0.25);
    s.polyline([Vector3(0,50), Vector3(10,60), Vector3(20,50), Vector3(30,60)], true);
    s.text(Vector3(25.0,85.0), "Circle", -30.0, "middle", 10);
    s.arc(Vector3(90.0,0.0), Vector3(60.0,30.0), Vector3(60.0,0.0));
    s.setLineWidth(0.75);
    s.arc(Vector3(30.0,0.0), Vector3(60.0,30.0), Vector3(60.0,0.0), true);
    s.bezier3(Vector3(80.0,80.0), Vector3(40.0,80.0),
              Vector3(80.0,100.0), Vector3(40.0,100.0));
    s.setLineWidth(0.01);
    s.dotlabel(Vector3(70.0,20.0), "a");
    s.setLineWidth(0.5);
    s.polygon([Vector3(60,10), Vector3(95,10), Vector3(95,50), Vector3(60,50)],
              true, true, true);
    s.finish();

    // ---- D3: isometric, the demo's third document ------------------------
    auto s3 = new Sketch("svg", "isometric", [0.0,0.0,120.0,120.0]);
    s3.viewport.set(-2.0,-2.0,2.0,2.0);
    s3.look_at(Vector3(0.0,0.0,-1.0), Vector3(0.0,0.0,0.0), Vector3(0.0,1.0,0.0));
    auto a3 = Vector3(2.0, 0.0);
    auto b3 = Vector3(1.0, 1.0);
    auto c3 = Vector3(1.0, 0.0);
    auto abc3 = new Arc(a3, b3, c3);
    s3.start("skprobe_test3.svg");
    s3.line(Vector3(-1.0,1.0,0.0), Vector3(1.0,-1.0,0.0));
    s3.render(abc3);
    s3.rule("x", -1.2, 1.2, 0.4, Vector3(0.0,0.0,0.0), 0.03, "%.1f", 0.12, -30.0, 8);
    s3.text(Vector3(1.0,0.1), "x", 0.0, "middle", 10);
    s3.rule("y", -1.2, 1.2, 0.4, Vector3(0.0,0.0,0.0), 0.03, "%.1f", 0.20, -30.0, 8);
    s3.text(Vector3(0.1,1.0), "y", 0.0, "middle", 10);
    s3.rule("z", -1.2, 1.2, 0.4, Vector3(0.0,0.0,0.0), 0.03, "%.1f", 0.16, 30.0, 8);
    s3.text(Vector3(0.0,0.1,1.0), "z", 0.0, "middle", 10);
    s3.finish();

    // ---- D4: one document that reaches every remaining branch ------------
    auto f = new Sketch();
    f.viewport.set(0.0, 0.0, 120.0, 120.0);
    f.title = "a title";
    f.description = "a description";
    f.start("skprobe_full.svg");
    f.setLineWidth(0.125);
    f.setLineColour("#ff0000");
    f.setFillColour("blue");
    f.setDashArray();
    f.line(V3(10.0,10.0), V3(30.0,40.0));
    f.line(V3(10.0,15.0), V3(30.0,45.0), true);
    f.setDashArray(4.0, 1.0);
    f.line(V3(10.0,20.0), V3(40.0,20.0), true);
    f.clearFillColour();
    f.line(V3(10.0,25.0), V3(40.0,25.0));
    f.setLineColour("black");
    f.setFillColour("none");
    f.begin_group();
    f.line(V3(10.0,30.0), V3(40.0,30.0));
    f.end_group();
    f.begin_group("grp", 0.25);
    f.line(V3(10.0,35.0), V3(40.0,35.0));
    f.end_group();
    f.polyline([V3(10.0,40.0), V3(20.0,45.0), V3(30.0,40.0), V3(40.0,48.0)]);
    f.polyline([V3(10.0,50.0), V3(20.0,55.0), V3(30.0,50.0), V3(40.0,58.0)], true);
    f.polyline([V3(10.0,60.0)]);
    f.polygon([V3(50.0,10.0), V3(70.0,10.0), V3(70.0,30.0), V3(50.0,30.0)]);
    f.polygon([V3(50.0,35.0), V3(70.0,35.0), V3(70.0,55.0), V3(50.0,55.0)], false, true, false);
    f.polygon([V3(50.0,60.0), V3(70.0,60.0), V3(70.0,80.0), V3(50.0,80.0)], true, false, false);
    f.polygon([V3(50.0,85.0), V3(70.0,85.0), V3(70.0,105.0), V3(50.0,105.0)], false, false, true);
    f.arc(V3(80.0,10.0), V3(100.0,10.0), V3(90.0,20.0));
    f.arc(V3(80.0,20.0), V3(100.0,20.0), V3(90.0,30.0), true);
    f.setFillColour("yellow");
    f.circle(V3(20.0,70.0), 6.0);
    f.circle(V3(20.0,85.0), 6.0, false, true, false);
    f.circle(V3(20.0,100.0), 6.0, true, false, true);
    f.circle(V3(20.0,115.0), 6.0, false, false, false);
    f.bezier3(V3(80.0,40.0), V3(85.0,60.0), V3(95.0,60.0), V3(100.0,40.0));
    f.bezier3(V3(80.0,45.0), V3(85.0,65.0), V3(95.0,65.0), V3(100.0,45.0), true);
    f.text(V3(30.0,115.0), "plain");
    f.text(V3(60.0,115.0), "rot", 30.0, "middle", 14, "red", "serif");
    f.text(V3(90.0,115.0), "end", -45.0, "end", 8);
    f.text(V3(105.0,115.0), "weird-anchor", 0.0, "bogus", 10);
    f.dotlabel(V3(35.0,105.0));
    f.dotlabel(V3(45.0,105.0), "L", "start", 3.0, 12, "blue", "monospace");
    f.dotlabel(V3(55.0,105.0), "E", "end", 1.5, 6);
    f.render(abc);
    f.render(abc, true, 6);
    f.render(my_patch);
    f.render(my_patch, true, true, true, 4);
    f.render(my_patch, true, false, false, 4, true);
    f.render(my_patch, false, true, false, 3);
    auto slab = new SlabVolume(my_patch, Vector3(0.0, 0.0, 15.0));
    f.render(slab);
    f.render(slab, true, true, true, 3);
    f.render(slab, true, false, false, 2, true);
    f.render(slab, false, true, false, 3);
    f.finish();

    // ---- D5: oblique projection through the transform-carrying methods ---
    auto ob = new Sketch("svg", "oblique", [0.0,0.0,120.0,120.0]);
    ob.viewport.set(-2.0,-2.0,2.0,2.0);
    ob.start("skprobe_oblique.svg");
    ob.line(V3(-1.0,1.0,0.0), V3(1.0,-1.0,0.5));
    ob.polyline([V3(-1.0,-1.0,0.0), V3(0.0,-1.0,1.0), V3(1.0,0.0,0.5)], true);
    ob.polygon([V3(-1.0,0.0,0.0), V3(0.0,1.0,1.0), V3(1.0,0.0,0.0), V3(0.0,-1.0,0.5)]);
    ob.bezier3(V3(-1.0,-1.5,0.0), V3(0.0,-1.5,1.0), V3(1.0,-1.5,0.0), V3(1.5,-1.0,0.5), true);
    ob.text(V3(0.5,0.5,0.5), "oblique text", 15.0, "middle", 9);
    ob.dotlabel(V3(-0.5,-0.5,0.25), "q", "middle", 2.5, 7);
    ob.render(abc3);
    ob.render(my_patch, true, true, false, 5);
    ob.rule("z", -1.0, 1.0, 0.5, Vector3(0.0,0.0,0.0), 0.05, "%.2f", 0.1, 20.0, 7);
    ob.finish();

    // ---- D6: rule coverage -----------------------------------------------
    // The tick count comes from to!int(1.000001*(vmax - vmin)/vtic) (:728), so its
    // truncation is measured directly first.
    foreach (r; [[0.0, 6.0, 0.1], [-1.2, 1.2, 0.4], [0.0, 1.0, 3.0],
                 [1.0, -1.0, 0.25], [0.0, 2.0, 0.3333333], [-5.0, 5.0, 0.4]]) {
        double vmin = r[0], vmax = r[1], vtic = r[2];
        int n = to!int(1.000001*(vmax - vmin)/vtic);
        writefln("s6 nrule %.17g %.17g %.17g n=%d gt50=%s", vmin, vmax, vtic, n, n > 50);
    }
    auto rg = new Sketch();
    rg.viewport.set(0.0, 0.0, 120.0, 120.0);
    rg.start("skprobe_rules.svg");
    rg.rule("x", 0.0, 6.0, 0.1, Vector3(10.0,10.0), 1.0, "%.1f", 2.0, 0.0, 6); // 61 tics
    rg.rule("y", 1.0, -1.0, 0.25, Vector3(20.0,20.0), 1.0, "%.2f", 2.0, 45.0, 6); // n < 0
    rg.rule("z", 0.0, 40.0, 20.0, Vector3(30.0,30.0,0.0), 2.0, "%3.1f", 3.0, 0.0, 5);
    rg.finish();
    try {
        rg.rule("q", 0.0, 1.0, 0.5, Vector3(0.0,0.0), 0.1, "%.1f", 0.1, 0.0, 8);
        writeln("s7 invalid rule direction threw=0");
    }
    catch (Exception ex) {
        writefln("s7 invalid rule direction threw=1 isError=0 msg=[%s]", ex.msg);
    }
    catch (Error ex) {
        writefln("s7 invalid rule direction threw=1 isError=1 msg=[%s]", ex.msg);
    }

    // ---- now dump every document, in the order they were drawn -----------
    dump_doc("skprobe_test.svg");
    dump_doc("skprobe_test2.svg");
    dump_doc("skprobe_test3.svg");
    dump_doc("skprobe_full.svg");
    dump_doc("skprobe_oblique.svg");
    dump_doc("skprobe_rules.svg");
    writeln("s7 done=1");
}
