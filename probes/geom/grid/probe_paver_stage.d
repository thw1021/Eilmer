// probes/geom/grid/probe_paver_stage.d — the paving of geom/grid/paver_demo.d, stage by
// stage, measured against ldc2 1.40.0 at gdtk e587b235.
//
// Why this probe exists (backlog T-011 / T-931): the 1961-row corpus of probe_paver.d
// replays byte-identical, and yet geom/grid/paver_demo.d's paved_grid.vtk does NOT —
// 398 of its 1490 vertex lines differ in the last digits, with the first difference at
// vertex 228 (the first 214 are construct_boundary's output, which matches). The
// existing pave.* groups run the whole PavedGrid constructor but only over boundaries of
// 6 to 32 points, so they never reach the geometry the demo reaches. This probe drives the
// demo's exact 214-point boundary through the constructor's statement sequence and dumps
// a fingerprint of the module-global POINT_LIST after every stage that can move a node,
// so the first stage whose fingerprint differs is the stage that contains the divergence,
// and the first point index that differs is the node it diverges on.
//
// The constructor body (paver.d:1607-1730) is transcribed here statement for statement,
// with stage() inserted after each mutation. That is deliberate: paver.d's own writeln and
// writefln lines are NOT filtered out of this corpus, so the interleaved library text also
// pins the control flow (which guard branches were taken, and how many paving iterations
// ran) and not just the numbers. Every such line must come from the same source line as it
// does in paver.d — the replay is only evidence if both sides print the same program.
//
// Build and capture (reproducible; the gdtk tree itself is read-only):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip \
//     probes/geom/grid/probe_paver_stage.d -of=/tmp/probe_g7 -od/tmp/probe_obj_g7
//   /tmp/probe_g7 > baselines/e587b235/probes/geom_grid_paver_stage.txt
// Replay: ctest -R geom_grid_paver_stage (tests/test_geom_grid_paver_stage.cpp).
//
// Fingerprint columns: sx/sy are the wrapping sums of the IEEE-754 bit patterns of x/y
// over POINT_LIST in list order, cx/cy the xor of the same. A sum alone could cancel; the
// pair cannot cancel in practice and both are order-sensitive because the list order is
// itself part of what the algorithm must reproduce.
//
// Every double is printed twice, as %.17g and as its IEEE-754 bit pattern (wave-02-brief
// T-912): %.17g alone cannot tell +0.0 from -0.0.

import std.stdio;
import std.format;
import std.conv;
import std.math;

import geom.grid.paver;
import geom.elements.vector3;
import geom.gpath.path;
import geom.gpath.line;
import geom.gpath.arc;

//------------------------------------------------------------ dump helpers

void F(string tag, double v)
{
    pragma(once);
    writefln("%s|%.17g|%016x", tag, v, *cast(ulong*) &v);
}

void I(string tag, long v)
{
    pragma(once);
    writefln("%s|%d", tag, v);
}

void T(string tag, string v)
{
    pragma(once);
    writefln("%s|%s", tag, v);
}

// One node, field by field. Coordinates alone are NOT enough: the T-931 divergence first
// showed up as an interior_angle one bit off in a node whose x and y were still identical,
// and an x/y-only fingerprint reports that stage as clean.
void dumpNode(string tag, Point p)
{
    pragma(once);
    writefln("%s|ID=%d|x=%.17g/%016x|y=%.17g/%016x|ang=%.17g/%016x|cat=%s|VR=%.17g/%016x,"
             ~ "%.17g/%016x|VL=%.17g/%016x,%.17g/%016x|growth=%.17g/%016x|fsf=%d|rowID=%d",
             tag, p.ID, p.x, *cast(ulong*) &p.x, p.y, *cast(ulong*) &p.y,
             p.interior_angle, *cast(ulong*) &p.interior_angle, p.node_cat, p.V_R[0],
             *cast(ulong*) &p.V_R[0], p.V_R[1], *cast(ulong*) &p.V_R[1], p.V_L[0],
             *cast(ulong*) &p.V_L[0], p.V_L[1], *cast(ulong*) &p.V_L[1], p.growth,
             *cast(ulong*) &p.growth, cast(int) p.first_seam_flag, p.rowID);
}

void stage(string tag, bool dumpPoints)
{
    pragma(once);
    ulong sx = 0, sy = 0, sa = 0, s0 = 0, s1 = 0, s2 = 0, s3 = 0, sg = 0, sc = 0;
    ulong sr = 0;
    foreach (p; POINT_LIST)
    {
        sx += *cast(ulong*) &p.x;
        sy += *cast(ulong*) &p.y;
        sa += *cast(ulong*) &p.interior_angle;
        s0 += *cast(ulong*) &p.V_R[0];
        s1 += *cast(ulong*) &p.V_R[1];
        s2 += *cast(ulong*) &p.V_L[0];
        s3 += *cast(ulong*) &p.V_L[1];
        sg += *cast(ulong*) &p.growth;
        sc += p.node_cat.length;
        sr += p.rowID;
    }
    writefln("st.%s|pts=%d|faces=%d|cells=%d|sx=%016x|sy=%016x|sa=%016x|sVR=%016x,%016x|"
             ~ "sVL=%016x,%016x|sg=%016x|sc=%d|sr=%d",
             tag, POINT_LIST.length, FACE_LIST.length, CELL_LIST.length, sx, sy, sa, s0, s1,
             s2, s3, sg, sc, sr);
    if (dumpPoints)
    {
        foreach (i, p; POINT_LIST)
            dumpNode("pt." ~ tag ~ "." ~ to!string(i), p);
    }
}

//---------------------------------------------------------------- boundary

// paver_demo.d:12-34, transcribed unchanged.
Vector3[] construct_boundary(Path[] edges, size_t[] n)
{
    assert(edges.length == n.length);
    Vector3[] boundary;
    size_t i = 0;
    double[] t_list;
    double t;
    double t_incr;
    foreach (edge; edges)
    {
        t = 0;
        t_incr = 1.0 / (n[i] - 1);
        t_list = [];
        for (int j; j < (n[i] - 1); ++j)
        {
            t_list ~= t;
            t += t_incr;
        }
        foreach (val; t_list)
        {
            boundary ~= edge.opCall(val);
        }
        ++i;
    }
    return boundary;
}

void main()
{
    // paver_demo.d:79-101: the boundary the demo paves.
    Vector3 P0 = Vector3(-0.5, 0);
    Vector3 P1 = Vector3(0, 0);
    Vector3 P2 = Vector3(0.5, 1.0);
    Vector3 P3 = Vector3(3, 3);
    Vector3 P4 = Vector3(3, 5.5);
    Vector3 P5 = Vector3(0, 1.5);
    Vector3 M0 = Vector3(0.108, 0.54);
    Vector3 M1 = Vector3(-0.3, 1.04);
    auto P0P1 = new Line(P0, P1);
    size_t n0 = 10;
    auto P1P2 = new Arc3(P1, M0, P2);
    size_t n1 = 30;
    auto P2P3 = new Line(P2, P3);
    size_t n2 = 50;
    auto P3P4 = new Line(P3, P4);
    size_t n3 = 30;
    auto P4P5 = new Line(P4, P5);
    size_t n4 = 70;
    auto P5P0 = new Arc3(P5, M1, P0);
    size_t n5 = 30;
    Vector3[] boundary_vectors = construct_boundary([P0P1, P1P2, P2P3, P3P4, P4P5, P5P0],
                                                   [n0, n1, n2, n3, n4, n5]);

    // usgrid.d:516-519 (the constructor that paver_demo.d:109 calls) turns the Vector3
    // list into the double[][] that PavedGrid takes.
    double[][] boundary_points;
    foreach (p; boundary_vectors)
    {
        boundary_points ~= [p.x.re, p.y.re, p.z.re];
    }
    foreach (i, b; boundary_points)
        writefln("bp.%d|%.17g/%016x|%.17g/%016x|%.17g/%016x", i, b[0], *cast(ulong*) &b[0],
                 b[1], *cast(ulong*) &b[1], b[2], *cast(ulong*) &b[2]);

    //---------------- paver.d:1607-1730, inlined with a stage after each mutation -------

    writeln("importing external boundary...");                       // paver.d:1611
    foreach (p; boundary_points)
    {
        make2DPoint(p[0], p[1]);
    }
    foreach (point; POINT_LIST)
    {
        point.store_rowID(point.ID);
    }
    stage("0imported", true);

    Point[] external_boundary = POINT_LIST;                          // paver.d:1619
    size_t npoints_boundary = external_boundary.length;
    writefln("there are %s points in the external boundary", npoints_boundary);

    assign_node_properties(external_boundary);                       // paver.d:1625
    stage("1nodeprops", false);
    adjust_row_ends(external_boundary, initial_size, 1);              // paver.d:1626
    stage("2rowends", true);

    foreach (point; external_boundary)                                // paver.d:1628-1630
    {
        makeFace(point.ID, point.pointR.ID);
    }
    stage("3faces", false);

    if (clockwise_check(external_boundary) > 0)                       // paver.d:1636
    {
        throw new Error(text("imported external boundary must be anti-clockwise"));
    }
    else
    {
        writeln("boundary is anti-clockwise and closed - good");       // paver.d:1640
    }
    stage("4cw", false);

    if (to!int(intersecting_boundary(external_boundary).length) > 0)   // paver.d:1642
    {
        throw new Error(text("imported boundary intersects itself - not allowed"));
    }
    else
    {
        writeln("boundary does not self-intersect - good");            // paver.d:1644
    }
    stage("5selfint", false);

    initial_size = average_face_length(external_boundary);             // paver.d:1647
    avr_area = initial_size^^2;
    F("sz.initial_size", initial_size);
    F("sz.avr_area", avr_area);

    writeln("generating first new row...");                           // paver.d:1654
    Point[] New_Row = generate_new_row(external_boundary);             // paver.d:1655
    size_t npoints_row = New_Row.length;
    writefln("there are %s points in the first new paving row", npoints_row);
    stage("6row1", true);
    assign_node_properties(New_Row);                                  // paver.d:1658
    stage("7row1props", false);
    Point[] Prev_Row = New_Row;
    F("mv.8row1smk", smoothing(New_Row, Prev_Row, 1, 0));              // paver.d:1660
    stage("8row1smk", false);
    even_out_size(New_Row);                                           // paver.d:1661
    stage("9row1even", false);

    Point[4][] intersections = intersecting_boundary(New_Row);         // paver.d:1664
    I("ix.firstrow", intersections.length);
    bool effective_intersection = 0;
    foreach (p; New_Row)
    {
        if (p.interior_angle > 6.25 || p.interior_angle < 0.03)
        {
            effective_intersection = 1;
        }
    }
    I("ix.effective", cast(long) effective_intersection);
    if (intersections.length != 0 || effective_intersection == 1)       // paver.d:1669
    {
        writeln("first new row intersected itself ... ");               // paver.d:1671
        smoothing(New_Row, Prev_Row, 1, 1);                             // paver.d:1672
        smoothing(New_Row, Prev_Row, 1, 0);                              // paver.d:1673
    }
    assign_node_properties(New_Row);                                   // paver.d:1675
    stage("10row1pre", false);
    adjust_row_ends(New_Row, initial_size, 0);                          // paver.d:1676
    stage("11row1adj", false);

    bool seam_next = 1;                                                // paver.d:1682
    int loop_counter;
    int iteration;
    while (true)
    {
        ++loop_counter;                                                // paver.d:1685
        writeln("paving new rows until intersection...");               // paver.d:1686
        pave_rows_until_intersection(New_Row, Prev_Row, npoints_boundary, seam_next);
        stage("12loop" ~ to!string(iteration) ~ ".pave", false);
        I("lp" ~ to!string(iteration) ~ ".seamnext", cast(long) seam_next);
        if (seam_next == 0)
        {
            break;
        }
        smoothing(New_Row, Prev_Row, 1, 0);                             // paver.d:1691
        stage("12loop" ~ to!string(iteration) ~ ".smk", false);
        writeln("commencing seaming procedure...");                     // paver.d:1692
        seaming_procedure(New_Row);                                     // paver.d:1693
        stage("12loop" ~ to!string(iteration) ~ ".seam", false);
        Prev_Row = New_Row;                                             // paver.d:1694
        writeln("...completed seam.");                                  // paver.d:1695
        even_out_size(New_Row);                                         // paver.d:1696
        stage("12loop" ~ to!string(iteration) ~ ".even", false);
        ++iteration;
    }
    I("loop.count", loop_counter);

    writeln("smoothing before closing");                               // paver.d:1701
    Point[] inside_nodes = POINT_LIST[npoints_boundary..$];             // paver.d:1702
    double biggest_move = smoothing(inside_nodes, Prev_Row, 0, 1);       // paver.d:1703
    F("mv.13preclose.first", biggest_move);
    stage("13preclose1", false);
    double move = biggest_move;
    for (int i; i < 1; ++i)                                             // paver.d:1705-1707
    {
        move = smoothing(inside_nodes, Prev_Row, 0, 0);
        F("mv.13preclose.second", move);
    }
    stage("13preclose", true);

    writeln("commencing closing seam...");                              // paver.d:1710
    closing_seam(New_Row);                                              // paver.d:1711
    stage("14closing", false);

    writeln("completing final smoothing...");                           // paver.d:1713
    inside_nodes = POINT_LIST[npoints_boundary..$];                      // paver.d:1714
    biggest_move = smoothing(inside_nodes, Prev_Row, 0, 1);              // paver.d:1715
    F("mv.15final.first", biggest_move);
    stage("15final1", false);
    move = biggest_move;
    for (int i; i < 2; ++i)                                             // paver.d:1717-1719
    {
        move = smoothing(inside_nodes, Prev_Row, 0, 0);
        F("mv.15final.loop" ~ to!string(i), move);
    }
    writeln("DONE.");                                                   // paver.d:1720
    writeln("total cell count: ", CELL_LIST.length);                     // paver.d:1721
    stage("15final", true);

    writefln("fields|dims=%d|nv=%d|nf=%d|nc=%d|nb=%d", ndimensions, POINT_LIST.length,
             FACE_LIST.length, CELL_LIST.length, npoints_boundary);
    T("END", "done");
}
