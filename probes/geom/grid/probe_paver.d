// probes/geom/grid/probe_paver.d — what geom/grid/paver.d (module geom.grid.paver)
// produces, measured against ldc2 1.40.0 at gdtk e587b235.
//
// paver.d is Heather Muir's 2D quad-paving algorithm: it takes a discretized
// anti-clockwise boundary, paves rows of quad cells inward (node_gen /
// generate_new_row), smooths them (Laplacian + a squareness-driven correction),
// seams the acute corners (seam) and closes the last region (closing_seam), all
// through the three module-global lists POINT_LIST / FACE_LIST / CELL_LIST that
// usgrid.d later consumes. It declares NO `unittest` block — proved by
//   grep -c unittest /home/tang/packages/gdtk/src/geom/grid/paver.d  ->  0
// and it has nothing else that could gate it: paver.d contains no `refine`, no
// `write_to_vtk_file`/`write_to_su2_file` and no `single_index`/`ijk_indices`
// override (grep -E "refine|write_to_|vtk|su2|single_index|ijk" -> no match), and
// `class PavedGrid` (paver.d:1598) does NOT derive from Grid, so this replay IS the
// gate. The exported contract usgrid.d:26 needs — PavedGrid, POINT_LIST, FACE_LIST,
// CELL_LIST — is dumped here in D's own order.
//
// Build and capture (reproducible; the gdtk tree itself is read-only):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/grid \
//     probes/geom/grid/probe_paver.d -of=/tmp/probe_g2 -od/tmp/probe_obj_g2
//   /tmp/probe_g2 > baselines/e587b235/probes/geom_grid_paver.txt
// @nogc is a compile-time attribute with no runtime effect and the port has no
// @nogc to carry; nothing else in the shadow copy differs from gdtk (measured:
// `diff /tmp/gdtk_shadow/geom/grid/paver.d <gdtk>/geom/grid/paver.d` shows 9 lines,
// each one an @nogc removal).
//
// Every double is printed twice, as %.17g and as its IEEE-754 bit pattern, because
// %.17g alone cannot tell +0.0 from -0.0 or two different NaNs (wave-02-brief T-912).
// Exception texts have their newlines escaped to <newline> so that one thrown case
// stays one row.
//
// What each group pins down:
//   cls.*    the Point / UFace / Cell classes and the three global lists: field-by-
//            field .init of a fresh object (x/y/interior_angle/V_R/V_L are NaN,
//            node_cat is "none", z/growth/rowID/first_seam_flag are 0, the three
//            Point links are null, cell_type is a null string that PRINTS EMPTY —
//            measured, unlike a null class handle which prints "null"), the ID
//            counters, the print() texts, Cell.auto_cell_type over 0..8 faces
//            (the out-of-range arms print Phobos' own `cast(CellType)7`), and
//            assign_outsigns' `assert(o == -1 || o == 1)` (paver.d:125).
//   sec2.*   Section 2 (paver.d:170-293): left_rot_matrix / the module-level
//            left90matrix (whose cos term is NOT libm's cos(PI/2) — ldc2 constant-
//            folds the initializer and stores -2.7600944370494486e-33 where a
//            runtime call gives +6.123233995736766e-17; both rows are here),
//            vector_rot, midpoint (which does NOT append to POINT_LIST and does NOT
//            bump Point.ID_count), point_dist / norm (both through `^^2` and
//            `^^0.5`, i.e. std.math.pow), vector2D, LeftorRight (its 1 / -1 / 2
//            arms AND the fourth possibility: every comparison false on a NaN
//            determinant leaves the uninitialised `int ans` = 0), unit_normal_vector,
//            unit_leftV, squareness (the assert(n==4) guard, the two-triangle area
//            split and 16*area/perimeter^^2), winding_number.
//   sec3.*   Section 3 (paver.d:300-435): clockwise_check (its sign convention, and
//            that it calls closed_row_check so a broken link throws), calc_area,
//            intersecting_boundary, intersecting_faces (which reads
//            FACE_LIST[$-npoints_row..$]), average_face_length (empty surface ->
//            sum/count with count==0 -> the -nan that 0.0/0.0 really is),
//            angle_between_vectors (both arms, and acos of a NaN/-1.0000000000000002
//            argument), inside_prev_row_check.
//   make.*   make2DPoint / makeFace / makeCell / findFace (paver.d:442-526): the ID
//            sequencing, which faces are new versus reused, the outsigns that come
//            out of makeCell for each of the three findFace outcomes, and findFace's
//            `size_t[2] reverse_case;` default of [0,0], which makes a directional
//            query match any face that itself is [0,0].
//   node.*   assign_node_properties (paver.d:528-607) — the cosine rule with
//            `^^2`, the `assert(r<1.01 && r>-1.01)` guard (which fires on a NaN r,
//            because NaN comparisons are false), the r>1.0 / r<-1.0 clamps, the
//            3.141/3.142 PI exemption, the exterior-angle fix; node_category
//            (paver.d:765-792) at each tolerance and one ULP either side — the
//            tolerances themselves are ldc2-folded constants, dumped as bit
//            patterns — and above 2*PI, which throws object.Error; adjust_row_ends
//            (paver.d:610-762) for boundary==1 and boundary==0, with the
//            "matching row end enforced", "growth node detected - wedge inserted"
//            and "row end enforced to manage cell size reduction" arms and the
//            row-end / with-row-end relabelling pass.
//   row.*    node_gen (paver.d:795-967) for every category (row end, with row end,
//            row side, row corner, row reversal) including the neighbour-category
//            sub-arms, generate_new_row (paver.d:970-1014) — its start-node search,
//            its two-part loop over start_node.rowID, the final closing cell and the
//            rowID renumbering — and summed_squareness (paver.d:1016-1035) with its
//            `assert(cell_count != 0)`.
//   smk.*    smoothing (paver.d:1039-1113): the biggest_move return, the
//            "apparently this node has no friends!" arm, the boundary/first_row
//            matrix, the 5-step squareness loop; even_out_size, biggest_cell,
//            fix_IDs, point_ID_check (ok and the corrupted-ID Error), seam
//            (paver.d:1115-1260) in the non-closing and closing modes with its
//            Flag/ind/nodes state machine and its three writeln arms, and the
//            `assert(Final_Row.length%2 == 0, ...)` message of closing_seam.
//   seam2.*  seaming_procedure (paver.d:1530-1592), including the
//            "seaming from smallest angle node:" arm and the row-ID/classification
//            reset.
//   pave.*   the whole PavedGrid constructor (paver.d:1598-1731) on boundaries of
//            increasing complexity — 8-point square, 16, 32, a 2:1 rectangle, an
//            L-shape (this one reaches the seaming procedure), a wedge and a circle —
//            with every console line it writes, then the complete POINT_LIST,
//            FACE_LIST and CELL_LIST in D's order, and the grid's own
//            dimensions/label/nvertices/nfaces/ncells/nboundaries. The guard rows are
//            the odd-length boundary, the empty boundary, the single-point boundary,
//            the clockwise boundary and the self-intersecting boundary.
//
// Rows are positionally compared by tests/test_geom_grid_paver.cpp; nothing here is
// filtered, and no case is skipped because it fails.

import std.stdio;
import std.format;
import std.conv;
import std.math;
import std.math : nextafter;
import std.array : replace;

import geom.grid.paver;

//------------------------------------------------------------ dump helpers

string esc(string s)
{
    pragma(once);
    return s.replace("\n", "<newline>");
}

// One observable double: value and bit pattern.
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

void S(string tag, string v)
{
    pragma(once);
    writefln("%s|len=%d|[%s]", tag, v.length, esc(v));
}

void T(string tag, string v)
{
    pragma(once);
    writefln("%s|%s", tag, v);
}

// A Point, field by field, with links rendered as the ID they point at (or -1).
void dumpP(string tag, Point p)
{
    pragma(once);
    if (p is null)
    {
        T(tag ~ ".null", "null");
        return;
    }
    writefln("%s|ID=%d|x=%.17g/%016x|y=%.17g/%016x|z=%.17g/%016x|ang=%.17g/%016x|cat=%s|VR=%.17g/%016x,%.17g/%016x|VL=%.17g/%016x,%.17g/%016x|L=%d|R=%d|prev=%d|growth=%.17g/%016x|fsf=%d|rowID=%d",
             tag, p.ID, p.x, *cast(ulong*) &p.x, p.y, *cast(ulong*) &p.y, p.z,
             *cast(ulong*) &p.z, p.interior_angle, *cast(ulong*) &p.interior_angle,
             p.node_cat, p.V_R[0], *cast(ulong*) &p.V_R[0], p.V_R[1],
             *cast(ulong*) &p.V_R[1], p.V_L[0], *cast(ulong*) &p.V_L[0], p.V_L[1],
             *cast(ulong*) &p.V_L[1], (p.pointL is null) ? -1 : cast(int) p.pointL.ID,
             (p.pointR is null) ? -1 : cast(int) p.pointR.ID,
             (p.prev_node is null) ? -1 : cast(int) p.prev_node.ID, p.growth,
             *cast(ulong*) &p.growth, cast(int) p.first_seam_flag, p.rowID);
}

void dumpF(string tag, UFace f)
{
    pragma(once);
    if (f is null)
    {
        T(tag ~ ".null", "null");
        return;
    }
    writefln("%s|faceID=%d|pts=%d,%d", tag, f.face_ID, f.point_IDs[0], f.point_IDs[1]);
}

void dumpC(string tag, Cell c)
{
    pragma(once);
    if (c is null)
    {
        T(tag ~ ".null", "null");
        return;
    }
    string ps, fs, os;
    foreach (t; c.point_IDs)
        ps ~= (ps.length == 0 ? "" : ",") ~ to!string(t);
    foreach (t; c.face_IDs)
        fs ~= (fs.length == 0 ? "" : ",") ~ to!string(t);
    foreach (t; c.outsigns)
        os ~= (os.length == 0 ? "" : ",") ~ to!string(t);
    writefln("%s|cellID=%d|type=%s|pts=%s|faces=%s|out=%s", tag, c.cell_ID,
             c.cell_type, ps, fs, os);
}

void dumpLists(string tag)
{
    pragma(once);
    writefln("%s|counts|%d|%d|%d|%d", tag, POINT_LIST.length, FACE_LIST.length,
             CELL_LIST.length, ndimensions);
    foreach (t; POINT_LIST)
        dumpP(tag ~ ".P" ~ to!string(t.ID), t);
    foreach (t; FACE_LIST)
        dumpF(tag ~ ".F" ~ to!string(t.face_ID), t);
    foreach (t; CELL_LIST)
        dumpC(tag ~ ".C" ~ to!string(t.cell_ID), t);
}

void resetAll()
{
    pragma(once);
    POINT_LIST.length = 0;
    FACE_LIST.length = 0;
    CELL_LIST.length = 0;
    Point.ID_count = 0;
    UFace.face_ID_count = 0;
    Cell.cell_ID_count = 0;
    initial_size = 0;
    avr_area = 0;
}

// Every throw path is tagged by which side of D's Throwable split it lands on, so
// the Error/Exception sibling relationship (a `catch (Exception)` does not catch an
// Error) is part of the replay.
void tryExc(string tag, void delegate() dg)
{
    pragma(once);
    try
    {
        dg();
        T(tag, "no-throw");
    }
    catch (Exception e)
    {
        writefln("%s|EXC|%s", tag, esc(e.msg));
    }
    catch (Error e)
    {
        writefln("%s|ERR|%s", tag, esc(e.msg));
    }
}

Point[] mkrow(double[][] xy)
{
    pragma(once);
    Point[] r;
    foreach (p; xy)
        r ~= make2DPoint(p[0], p[1]);
    return r;
}

//---------------------------------------------------------------- group cls
void grpCls()
{
    resetAll();
    Point p = new Point(7, 1.25, -2.5);
    dumpP("cls.fresh", p);
    S("cls.fresh.node_cat", p.node_cat);
    S("cls.fresh.print", p.print());
    p.store_node_angle(0.75);
    p.store_node_cat("row end");
    p.store_vectors([1.0, 2.0], [3.0, 4.0]);
    p.store_rowID(11);
    dumpP("cls.stored", p);
    // store_node_cat only writes while the category is still "none" (paver.d:50-54)
    p.store_node_cat("row side");
    S("cls.cat.sticky", p.node_cat);
    Point q = new Point(8, 3.5, 4.5);
    p.store_adjacent_points(q, q);
    I("cls.links.L", cast(long) p.pointL.ID);
    I("cls.links.R", cast(long) p.pointR.ID);
    T("cls.links.print", p.print());
    I("cls.IDcount", cast(long) Point.ID_count);
    UFace f = new UFace(3, [10, 20]);
    dumpF("cls.face", f);
    S("cls.face.print", f.print());
    I("cls.facecount", cast(long) UFace.face_ID_count);
    Cell c = new Cell(5);
    dumpC("cls.cell.fresh", c);
    S("cls.cell.fresh.print", c.print());
    c.point_IDs = [cast(size_t) 1, 2, 3];
    S("cls.cell.partial.print", c.print());
    Cell c2 = new Cell(6);
    c2.face_IDs.length = 4;
    c2.auto_cell_type();
    S("cls.celltype.4", c2.cell_type);
    foreach (n; 0 .. 9)
    {
        Cell cx = new Cell(90 + n);
        cx.face_IDs.length = n;
        tryExc("cls.auto." ~ to!string(n) ~ ".throw", () { cx.auto_cell_type(); });
        S("cls.celltype." ~ to!string(n), cx.cell_type);
    }
    Cell c3 = new Cell(7);
    tryExc("cls.outsigns.bad", () { c3.assign_outsigns([1, 0]); });
    Cell c4 = new Cell(8);
    c4.assign_outsigns([-1, 1, -1]);
    dumpC("cls.outsigns.ok", c4);
    c4.assign_outsigns([1]);
    dumpC("cls.outsigns.append", c4);
    I("cls.cellcount", cast(long) Cell.cell_ID_count);
    // a null Point prints as "null" where a null string prints as empty
    Point np;
    T("cls.nullpoint", to!string(np));
    dumpP("cls.nullpoint.dump", np);
}

//---------------------------------------------------------------- group sec2
void grpSec2()
{
    resetAll();
    double[] angs = [0.0, 0.5, 1.0, PI / 2, PI, 3 * PI / 2, 2 * PI, -0.25, double.init];
    foreach (i, th; angs)
    {
        auto m = left_rot_matrix(th);
        string tg = "sec2.lrm" ~ to!string(i);
        F(tg ~ ".th", th);
        F(tg ~ ".00", m[0][0]);
        F(tg ~ ".01", m[0][1]);
        F(tg ~ ".10", m[1][0]);
        F(tg ~ ".11", m[1][1]);
    }
    F("sec2.gbl.00", left90matrix[0][0]);
    F("sec2.gbl.01", left90matrix[0][1]);
    F("sec2.gbl.10", left90matrix[1][0]);
    F("sec2.gbl.11", left90matrix[1][1]);
    double[2][] vs = [[1.0, 0.0], [0.0, 1.0], [1.0, 1.0], [0.5, -2.0], [3.0, 4.0]];
    foreach (i, v; vs)
    {
        auto r = vector_rot(v, left90matrix);
        F("sec2.vrot" ~ to!string(i) ~ ".0", r[0]);
        F("sec2.vrot" ~ to!string(i) ~ ".1", r[1]);
    }
    size_t nbefore = POINT_LIST.length;
    size_t cbefore = Point.ID_count;
    Point a = make2DPoint(0.0, 0.0);
    Point b = make2DPoint(2.0, 1.0);
    Point m = midpoint(a, b);
    dumpP("sec2.midpoint", m);
    I("sec2.mid.listlen", cast(long) (POINT_LIST.length - nbefore));
    I("sec2.mid.idcount", cast(long) (Point.ID_count - cbefore));
    Point bad = new Point(99, double.init, 1.0);
    bad.z = 0.0;
    Point nanp = new Point(98, 1.0, double.init);
    nanp.z = 0.0;
    F("sec2.dist.ab", point_dist(a, b));
    F("sec2.dist.aa", point_dist(a, a));
    F("sec2.dist.ba", point_dist(b, a));
    F("sec2.dist.nan", point_dist(a, bad));
    F("sec2.dist.nan2", point_dist(nanp, b));
    Point big = new Point(97, 1.0e200, 1.0e200);
    F("sec2.dist.big", point_dist(a, big));
    Point tiny = new Point(96, 1.0e-200, 0.0);
    F("sec2.dist.tiny", point_dist(a, tiny));
    auto vab = vector2D(a, b);
    F("sec2.v2d.0", vab[0]);
    F("sec2.v2d.1", vab[1]);
    auto vba = vector2D(b, a);
    F("sec2.v2d.r0", vba[0]);
    F("sec2.v2d.r1", vba[1]);
    // LeftorRight: 1 left, -1 right, 2 on the line, 0 when the determinant is NaN
    Point c1 = make2DPoint(1.0, 0.0);
    Point lft = make2DPoint(0.5, 1.0);
    Point rgt = make2DPoint(0.5, -1.0);
    Point onl = make2DPoint(2.0, 0.0);
    I("sec2.lr.left", LeftorRight(a, b, lft));
    I("sec2.lr.right", LeftorRight(a, b, rgt));
    I("sec2.lr.online", LeftorRight(a, c1, onl));
    I("sec2.lr.degen", LeftorRight(a, a, b));
    I("sec2.lr.nan", LeftorRight(a, bad, b));
    I("sec2.lr.nan2", LeftorRight(bad, a, b));
    double[2][] ns = [[0.0, 0.0], [3.0, 4.0], [-3.0, -4.0], [1.0, 1.0e-8],
                     [1.0e200, 1.0e200], [double.init, 1.0]];
    foreach (i, v; ns)
        F("sec2.norm" ~ to!string(i), norm(v));
    Point p0 = make2DPoint(0.0, 0.0);
    Point pR = make2DPoint(1.0, 0.0);
    auto un = unit_normal_vector(p0, pR);
    F("sec2.unv.0", un[0]);
    F("sec2.unv.1", un[1]);
    Point pS = make2DPoint(1.5, 2.0);
    auto un2 = unit_normal_vector(p0, pS);
    F("sec2.unv2.0", un2[0]);
    F("sec2.unv2.1", un2[1]);
    Point pZ = make2DPoint(0.0, 0.0);
    auto un3 = unit_normal_vector(p0, pZ);
    F("sec2.unv3.0", un3[0]);
    F("sec2.unv3.1", un3[1]);
    double[] ths = [0.0, PI / 2, PI / 3, 2 * PI / 3, PI];
    foreach (i, th; ths)
    {
        auto uv = unit_leftV([1.0, 0.0], th);
        F("sec2.ulv" ~ to!string(i) ~ ".0", uv[0]);
        F("sec2.ulv" ~ to!string(i) ~ ".1", uv[1]);
        auto uv2 = unit_leftV([-0.5, 0.75], th);
        F("sec2.ulv2_" ~ to!string(i) ~ ".0", uv2[0]);
        F("sec2.ulv2_" ~ to!string(i) ~ ".1", uv2[1]);
    }
    auto uvz = unit_leftV([0.0, 0.0], 1.0);
    F("sec2.ulvz.0", uvz[0]);
    F("sec2.ulvz.1", uvz[1]);
    // squareness over hand-made cells: unit square, 2x1, skewed, degenerate, NaN
    string[] names = ["sq", "rect", "skew", "degen", "nan"];
    double[][][] quads = [[[0.0, 0.0], [1.0, 0.0], [1.0, 1.0], [0.0, 1.0]],
                          [[0.0, 0.0], [2.0, 0.0], [2.0, 1.0], [0.0, 1.0]],
                          [[0.0, 0.0], [2.0, 0.0], [3.0, 1.0], [0.0, 1.0]],
                          [[0.0, 0.0], [1.0, 0.0], [1.0, 0.0], [0.0, 0.0]],
                          [[0.0, 0.0], [1.0, 0.0], [1.0, double.init], [0.0, 1.0]]];
    foreach (i, q; quads)
    {
        resetAll();
        Point[] ps;
        foreach (xy; q)
            ps ~= make2DPoint(xy[0], xy[1]);
        Cell cc = makeCell(ps);
        F("sec2.sq." ~ names[i], squareness(cc));
        dumpC("sec2.sq." ~ names[i] ~ ".cell", cc);
    }
    resetAll();
    Point[] tri;
    tri ~= make2DPoint(0.0, 0.0);
    tri ~= make2DPoint(1.0, 0.0);
    tri ~= make2DPoint(0.0, 1.0);
    Cell tc = makeCell(tri);
    tryExc("sec2.sq.tri.throw", () { F("sec2.sq.tri", squareness(tc)); });
    // winding_number on a closed anti-clockwise square and a clockwise one
    resetAll();
    Point[] ccw = mkrow([[0.0, 0.0], [1.0, 0.0], [1.0, 1.0], [0.0, 1.0]]);
    assign_node_properties(ccw);
    Point pin = new Point(500, 0.25, 0.25);
    Point pout = new Point(501, 2.0, 0.5);
    Point pedge = new Point(502, 0.5, 0.0);
    Point pvert = new Point(503, 1.0, 1.0);
    I("sec2.wn.inside", winding_number(pin, ccw));
    I("sec2.wn.outside", winding_number(pout, ccw));
    I("sec2.wn.edge", winding_number(pedge, ccw));
    I("sec2.wn.vertex", winding_number(pvert, ccw));
    I("sec2.wn.empty", winding_number(pin, []));
    resetAll();
    Point[] cw = mkrow([[0.0, 0.0], [0.0, 1.0], [1.0, 1.0], [1.0, 0.0]]);
    assign_node_properties(cw);
    I("sec2.wn.cw.inside", winding_number(pin, cw));
    I("sec2.wn.cw.outside", winding_number(pout, cw));
}

//---------------------------------------------------------------- group sec3
void grpSec3()
{
    resetAll();
    Point[] ccw = mkrow([[0.0, 0.0], [1.0, 0.0], [1.0, 1.0], [0.0, 1.0]]);
    assign_node_properties(ccw);
    F("sec3.cw.ccw", clockwise_check(ccw));
    Point[] cw = mkrow([[0.0, 0.0], [0.0, 1.0], [1.0, 1.0], [1.0, 0.0]]);
    assign_node_properties(cw);
    F("sec3.cw.cw", clockwise_check(cw));
    Point[] tri = mkrow([[0.0, 0.0], [2.0, 0.0], [1.0, 1.5]]);
    assign_node_properties(tri);
    F("sec3.cw.tri", clockwise_check(tri));
    F("sec3.cw.empty", clockwise_check([]));
    // a row whose links do not hold hands: closed_row_check throws
    resetAll();
    Point[] broken = mkrow([[0.0, 0.0], [1.0, 0.0], [1.0, 1.0], [0.0, 1.0]]);
    assign_node_properties(broken);
    broken[1].pointR = broken[3];
    tryExc("sec3.crc.broken", () { closed_row_check(broken); });
    resetAll();
    Point[] ok2 = mkrow([[0.0, 0.0], [1.0, 0.0], [1.0, 1.0], [0.0, 1.0]]);
    assign_node_properties(ok2);
    tryExc("sec3.crc.ok", () { closed_row_check(ok2); });
    tryExc("sec3.cw.broken", () { F("sec3.cw.broken.v", clockwise_check(broken)); });
    // calc_area
    F("sec3.area.ccw", calc_area(ok2));
    F("sec3.area.cw", calc_area(cw));
    F("sec3.area.tri", calc_area(tri));
    F("sec3.area.one", calc_area([ok2[0]]));
    F("sec3.area.two", calc_area([ok2[0], ok2[2]]));
    F("sec3.area.nan", calc_area([ok2[0], ok2[1], badPt(), ok2[3]]));
    tryExc("sec3.area.empty", () { F("sec3.area.empty.v", calc_area([])); });
    // intersecting_boundary
    resetAll();
    Point[] cross = mkrow([[0.0, 0.0], [1.0, 1.0], [1.0, 0.0], [0.0, 1.0]]);
    assign_node_properties(cross);
    auto ib = intersecting_boundary(cross);
    I("sec3.ib.cross.n", cast(long) ib.length);
    foreach (i, q; ib)
        writefln("sec3.ib.cross.%d|%d,%d,%d,%d", i, q[0].ID, q[1].ID, q[2].ID, q[3].ID);
    resetAll();
    Point[] sq = mkrow([[0.0, 0.0], [1.0, 0.0], [1.0, 1.0], [0.0, 1.0]]);
    assign_node_properties(sq);
    auto ib2 = intersecting_boundary(sq);
    I("sec3.ib.square.n", cast(long) ib2.length);
    resetAll();
    Point[] hex = mkrow([[0.0, 0.0], [2.0, 0.0], [3.0, 1.0], [2.0, 2.0], [0.0, 2.0],
                         [-1.0, 1.0]]);
    assign_node_properties(hex);
    auto ib3 = intersecting_boundary(hex);
    I("sec3.ib.hex.n", cast(long) ib3.length);
    I("sec3.ib.empty.n", cast(long) intersecting_boundary([]).length);
    // intersecting_faces works off FACE_LIST
    resetAll();
    Point[] f4 = mkrow([[0.0, 0.0], [1.0, 0.0], [1.0, 1.0], [0.0, 1.0]]);
    assign_node_properties(f4);
    foreach (p; f4)
        makeFace(p.ID, p.pointR.ID);
    auto ifc = intersecting_faces(f4);
    I("sec3.ifc.square.n", cast(long) ifc.length);
    resetAll();
    Point[] g8 = mkrow([[0.0, 0.0], [2.0, 0.0], [2.0, 1.0], [1.0, 1.0],
                        [1.0, 2.0], [0.0, 2.0], [0.0, 1.0], [2.0, 2.0]]);
    assign_node_properties(g8);
    foreach (p; g8)
        makeFace(p.ID, p.pointR.ID);
    auto ifc2 = intersecting_faces(g8);
    I("sec3.ifc.cross.n", cast(long) ifc2.length);
    foreach (i, q; ifc2)
        writefln("sec3.ifc.cross.%d|%d,%d,%d,%d", i, q[0].ID, q[1].ID, q[2].ID, q[3].ID);
    I("sec3.ifc.empty.n", cast(long) intersecting_faces([]).length);
    // average_face_length
    F("sec3.afl.square", average_face_length(sq));
    F("sec3.afl.hex", average_face_length(hex));
    F("sec3.afl.one", average_face_length([sq[0]]));
    F("sec3.afl.empty", average_face_length([]));
    F("sec3.afl.dup", average_face_length([sq[0], sq[0]]));
    // angle_between_vectors: both arms, and the degenerate ones
    F("sec3.abv.90", angle_between_vectors([1.0, 0.0], [0.0, 1.0]));
    F("sec3.abv.m90", angle_between_vectors([1.0, 0.0], [0.0, -1.0]));
    F("sec3.abv.0", angle_between_vectors([1.0, 0.0], [2.0, 0.0]));
    F("sec3.abv.180", angle_between_vectors([1.0, 0.0], [-1.0, 0.0]));
    F("sec3.abv.45", angle_between_vectors([1.0, 0.0], [1.0, 1.0]));
    F("sec3.abv.m45", angle_between_vectors([1.0, 0.0], [1.0, -1.0]));
    F("sec3.abv.60", angle_between_vectors([1.0, 0.0], [0.5, 0.8660254037844386]));
    F("sec3.abv.zero", angle_between_vectors([1.0, 0.0], [0.0, 0.0]));
    F("sec3.abv.zerobo", angle_between_vectors([0.0, 0.0], [0.0, 0.0]));
    F("sec3.abv.nan", angle_between_vectors([1.0, 0.0], [double.init, 1.0]));
    F("sec3.abv.big", angle_between_vectors([1.0e200, 1.0e200], [1.0e200, -1.0e200]));
    F("sec3.abv.tiny", angle_between_vectors([1.0e-200, 0.0], [0.0, 1.0e-200]));
    // inside_prev_row_check
    Point pin = new Point(500, 0.25, 0.25);
    Point pout = new Point(501, 2.0, 0.5);
    I("sec3.ipr.inside", cast(int) inside_prev_row_check([pin], sq));
    I("sec3.ipr.outside", cast(int) inside_prev_row_check([pout], sq));
    I("sec3.ipr.mixed", cast(int) inside_prev_row_check([pin, pout], sq));
    I("sec3.ipr.emptynew", cast(int) inside_prev_row_check([], sq));
    I("sec3.ipr.emptyrow", cast(int) inside_prev_row_check([pin], []));
}

Point badPt()
{
    return new Point(700, 1.0, double.init);
}

//---------------------------------------------------------------- group make
void grpMake()
{
    resetAll();
    Point a = make2DPoint(0.0, 0.0);
    Point b = make2DPoint(1.0, 0.0);
    Point c = make2DPoint(1.0, 1.0);
    Point d = make2DPoint(0.0, 1.0);
    I("make.npoints", cast(long) POINT_LIST.length);
    I("make.idcount", cast(long) Point.ID_count);
    UFace f0 = makeFace(0, 1);
    dumpF("make.f0", f0);
    UFace f1 = makeFace(1, 0);
    dumpF("make.f1", f1);
    I("make.nfaces", cast(long) FACE_LIST.length);
    I("make.facecount", cast(long) UFace.face_ID_count);
    auto q1 = findFace([cast(size_t) 0, 1], 1);
    dumpF("make.q01", q1);
    auto q2 = findFace([cast(size_t) 1, 0], 1);
    dumpF("make.q10", q2);
    auto q3 = findFace([cast(size_t) 0, 1], 0);
    dumpF("make.q01nd", q3);
    auto q4 = findFace([cast(size_t) 5, 6], 1);
    T("make.q56", q4 is null ? "null" : "found");
    // the [0,0] default of `reverse_case` (paver.d:516) matches any directional query
    UFace fz = makeFace(0, 0);
    auto q5 = findFace([cast(size_t) 5, 6], 1);
    dumpF("make.q56.afterzero", q5);
    auto q6 = findFace([cast(size_t) 5, 6], 0);
    dumpF("make.q56.nd", q6);
    // makeCell: fresh faces, a reused face with each sign, and both present
    resetAll();
    Point[] ps = mkrow([[0.0, 0.0], [1.0, 0.0], [1.0, 1.0], [0.0, 1.0]]);
    Cell c1 = makeCell(ps);
    dumpC("make.cell.fresh", c1);
    dumpLists("make.lists.aftercell");
    Cell c2 = makeCell([ps[2], ps[3], ps[0], ps[1]]);
    dumpC("make.cell.second", c2);
    I("make.cells.n", cast(long) CELL_LIST.length);
    UFace pre = makeFace(0, 3);
    dumpF("make.pre", pre);
    Cell c3 = makeCell([ps[0], ps[1], ps[2], ps[3]]);
    dumpC("make.cell.withface", c3);
    resetAll();
    Point[] trio = mkrow([[0.0, 0.0], [1.0, 0.0], [0.5, 1.0]]);
    Cell c4 = makeCell(trio);
    dumpC("make.cell.tri", c4);
    S("make.cell.tri.print", c4.print());
    resetAll();
    Cell c5 = makeCell([]);
    dumpC("make.cell.empty", c5);
    resetAll();
    Point rep = make2DPoint(0.0, 0.0);
    Cell c6 = makeCell([rep, rep, rep, rep]);
    dumpC("make.cell.repeated", c6);
}

//---------------------------------------------------------------- group node
void grpNode()
{
    // node_category and its three thresholds, ±1 ULP, with the thresholds dumped
    // straight out of the same expression paver.d:774 writes.
    double[] tols = [(110. / 180.) * PI, (240. / 180.) * PI, (315. / 180.) * PI];
    foreach (i, t; tols)
    {
        F("node.tol" ~ to!string(i), t);
    }
    foreach (i, t; tols)
    {
        string tg = "node.cat" ~ to!string(i);
        // exactly one ULP either side of the threshold, through std.math.nextafter
        double dl = nextafter(t, -1.0 / 0.0);
        double du = nextafter(t, 1.0 / 0.0);
        F(tg ~ ".below", dl);
        S(tg ~ ".belowcat", node_category(dl));
        S(tg ~ ".exactcat", node_category(t));
        F(tg ~ ".above", du);
        S(tg ~ ".abovecat", node_category(du));
        S(tg ~ ".twoup", node_category(nextafter(du, 1.0 / 0.0)));
        S(tg ~ ".twodn", node_category(nextafter(dl, -1.0 / 0.0)));
    }
    S("node.cat.zero", node_category(0.0));
    S("node.cat.pi", node_category(PI));
    S("node.cat.2pi", node_category(2 * PI));
    tryExc("node.cat.2piup.throw", ()
            { S("node.cat.2piup", node_category(6.283185307179587)); });
    S("node.cat.nan", node_category(double.init));
    tryExc("node.cat.over.throw", () { S("node.cat.over", node_category(2 * PI + 0.1)); });
    tryExc("node.cat.over2.throw", ()
            { S("node.cat.over2", node_category(1.0e300)); });
    // assign_node_properties over the fixtures
    resetAll();
    Point[] sq = mkrow([[0.0, 0.0], [1.0, 0.0], [1.0, 1.0], [0.0, 1.0]]);
    assign_node_properties(sq);
    foreach (i, p; sq)
    {
        F("node.sq" ~ to!string(i) ~ ".ang", p.interior_angle);
        S("node.sq" ~ to!string(i) ~ ".cat", p.node_cat);
        F("node.sq" ~ to!string(i) ~ ".VR0", p.V_R[0]);
        F("node.sq" ~ to!string(i) ~ ".VR1", p.V_R[1]);
        F("node.sq" ~ to!string(i) ~ ".VL0", p.V_L[0]);
        F("node.sq" ~ to!string(i) ~ ".VL1", p.V_L[1]);
        I("node.sq" ~ to!string(i) ~ ".L", cast(long) p.pointL.ID);
        I("node.sq" ~ to!string(i) ~ ".R", cast(long) p.pointR.ID);
    }
    resetAll();
    Point[] hex = mkrow([[0.0, 0.0], [2.0, 0.0], [3.0, 1.0], [2.0, 2.0], [0.0, 2.0],
                         [-1.0, 1.0]]);
    assign_node_properties(hex);
    foreach (i, p; hex)
    {
        F("node.hex" ~ to!string(i) ~ ".ang", p.interior_angle);
        S("node.hex" ~ to!string(i) ~ ".cat", p.node_cat);
    }
    resetAll();
    Point[] cwq = mkrow([[0.0, 0.0], [0.0, 1.0], [1.0, 1.0], [1.0, 0.0]]);
    assign_node_properties(cwq);
    foreach (i, p; cwq)
        F("node.cw" ~ to!string(i) ~ ".ang", p.interior_angle);
    // a straight row: the middle node sits exactly on pi
    resetAll();
    Point[] line = mkrow([[0.0, 0.0], [1.0, 0.0], [2.0, 0.0]]);
    assign_node_properties(line);
    foreach (i, p; line)
    {
        F("node.line" ~ to!string(i) ~ ".ang", p.interior_angle);
        S("node.line" ~ to!string(i) ~ ".cat", p.node_cat);
    }
    // the 3.141..3.142 exemption: an angle inside that band is left alone
    resetAll();
    Point[] band = mkrow([[0.0, 0.0], [1.0, 0.0001], [2.0, 0.0], [1.0, -1.0]]);
    assign_node_properties(band);
    foreach (i, p; band)
    {
        F("node.band" ~ to!string(i) ~ ".ang", p.interior_angle);
        S("node.band" ~ to!string(i) ~ ".cat", p.node_cat);
    }
    // two points: pointL and pointR are the same node, r clamps to 1
    resetAll();
    Point[] two = mkrow([[0.0, 0.0], [1.0, 0.0]]);
    tryExc("node.two.throw", () { assign_node_properties(two); });
    foreach (i, p; two)
    {
        F("node.two" ~ to!string(i) ~ ".ang", p.interior_angle);
        S("node.two" ~ to!string(i) ~ ".cat", p.node_cat);
    }
    // one point: every distance is 0, r is 0/0, the assert fires
    resetAll();
    Point[] one = mkrow([[0.0, 0.0]]);
    tryExc("node.one.throw", () { assign_node_properties(one); });
    dumpP("node.one.after", one[0]);
    // a duplicate point in a longer row: a==0 -> r NaN -> assert
    resetAll();
    Point[] dup = mkrow([[0.0, 0.0], [0.0, 0.0], [1.0, 1.0]]);
    tryExc("node.dup.throw", () { assign_node_properties(dup); });
    resetAll();
    Point[] nanrow = mkrow([[0.0, 0.0], [1.0, double.init], [1.0, 1.0]]);
    tryExc("node.nanrow.throw", () { assign_node_properties(nanrow); });
    resetAll();
    tryExc("node.emptyrow.throw", () { assign_node_properties([]); });
    // adjust_row_ends
    resetAll();
    Point[] oct = mkrow([[0.0, 0.0], [1.0, 0.0], [2.0, 0.0], [2.0, 1.0], [2.0, 2.0],
                         [1.0, 2.0], [0.0, 2.0], [0.0, 1.0]]);
    assign_node_properties(oct);
    string[] before;
    foreach (p; oct)
        before ~= p.node_cat;
    adjust_row_ends(oct, initial_size, 1);
    foreach (i, p; oct)
    {
        S("node.are.b1." ~ to!string(i), before[i]);
        S("node.are.a1." ~ to!string(i), p.node_cat);
    }
    resetAll();
    Point[] wedge = mkrow([[0.0, 0.0], [4.0, 0.0], [4.0, 0.4], [0.5, 2.0], [0.0, 1.0],
                           [0.0, 0.5]]);
    assign_node_properties(wedge);
    adjust_row_ends(wedge, initial_size, 1);
    foreach (i, p; wedge)
        S("node.are.wedge." ~ to!string(i), p.node_cat);
    adjust_row_ends(wedge, initial_size, 0);
    foreach (i, p; wedge)
        S("node.are.wedge.b0." ~ to!string(i), p.node_cat);
    // growth / reduction arms, driven by hand-set prev_node and vectors
    resetAll();
    Point[] ch = mkrow([[0.0, 0.0], [1.0, 0.0], [2.0, 0.0], [2.0, 1.0], [1.0, 1.0],
                        [0.0, 1.0]]);
    assign_node_properties(ch);
    adjust_row_ends(ch, initial_size, 1);
    ch[3].prev_node = ch[0];
    ch[3].V_R = [4.0, 0.0];
    ch[3].V_L = [0.0, 4.0];
    ch[0].V_R = [1.0, 0.0];
    ch[0].V_L = [0.0, 1.0];
    adjust_row_ends(ch, initial_size, 0);
    foreach (i, p; ch)
        S("node.are.growth." ~ to!string(i), p.node_cat);
    F("node.are.growth.g", ch[3].growth);
    resetAll();
    Point[] ch2 = mkrow([[0.0, 0.0], [1.0, 0.0], [2.0, 0.0], [2.0, 1.0], [1.0, 1.0],
                         [0.0, 1.0]]);
    assign_node_properties(ch2);
    adjust_row_ends(ch2, 1.0, 1);
    ch2[3].prev_node = ch2[0];
    ch2[3].V_R = [0.01, 0.0];
    ch2[3].V_L = [0.0, 0.01];
    ch2[0].V_R = [1.0, 0.0];
    ch2[0].V_L = [0.0, 1.0];
    adjust_row_ends(ch2, 1.0, 0);
    foreach (i, p; ch2)
        S("node.are.shrink." ~ to!string(i), p.node_cat);
    F("node.are.shrink.g", ch2[3].growth);
    // no row end at all in the row: the smallest-angled node is promoted
    resetAll();
    Point[] nr = mkrow([[0.0, 0.0], [2.0, 0.0], [2.0, 1.0], [1.0, 1.6], [0.0, 1.0]]);
    assign_node_properties(nr);
    foreach (p; nr)
        p.node_cat = "row side";
    adjust_row_ends(nr, initial_size, 0);
    foreach (i, p; nr)
        S("node.are.norend." ~ to!string(i), p.node_cat);
    // neighbouring row ends: the larger-angle one loses (paver.d:745-746)
    resetAll();
    Point[] ends = mkrow([[0.0, 0.0], [1.0, 0.0], [2.0, 0.0], [2.0, 1.0], [2.0, 2.0],
                          [1.0, 2.0], [0.0, 2.0], [0.0, 1.0]]);
    assign_node_properties(ends);
    foreach (p; ends)
        p.node_cat = "row side";
    ends[0].node_cat = "row end";
    ends[1].node_cat = "row end";
    ends[4].node_cat = "row end";
    ends[5].node_cat = "row end";
    adjust_row_ends(ends, initial_size, 1);
    foreach (i, p; ends)
        S("node.are.ends." ~ to!string(i), p.node_cat);
    // the cell-size-reduction arm (paver.d:737-740): a node whose two vectors are far
    // shorter than its predecessor's, with no row end among its four neighbours
    resetAll();
    Point[] red = mkrow([[0.0, 0.0], [1.0, 0.0], [2.0, 0.0], [3.0, 0.0], [3.0, 1.0],
                         [3.0, 2.0], [2.0, 2.0], [1.0, 2.0], [0.0, 2.0], [0.0, 1.0]]);
    assign_node_properties(red);
    foreach (p; red)
        p.node_cat = "row side";
    red[0].node_cat = "row end";
    red[1].node_cat = "row end";
    red[7].prev_node = red[2];
    red[7].V_R = [0.01, 0.0];
    red[7].V_L = [0.0, 0.01];
    red[2].V_R = [1.0, 0.0];
    red[2].V_L = [0.0, 1.0];
    adjust_row_ends(red, 1.0, 0);
    foreach (i, p; red)
        S("node.are.reduce." ~ to!string(i), p.node_cat);
    F("node.are.reduce.g", red[7].growth);
}

//---------------------------------------------------------------- group row
void grpRow()
{
    resetAll();
    Point[] oct = mkrow([[0.0, 0.0], [1.0, 0.0], [2.0, 0.0], [2.0, 1.0], [2.0, 2.0],
                         [1.0, 2.0], [0.0, 2.0], [0.0, 1.0]]);
    assign_node_properties(oct);
    adjust_row_ends(oct, initial_size, 1);
    foreach (i, p; oct)
        p.store_rowID(i);
    foreach (p; oct)
        makeFace(p.ID, p.pointR.ID);
    dumpLists("row.before");
    Point[] New_Nodes;
    foreach (i, p; oct)
    {
        S("row.gen." ~ to!string(i) ~ ".cat", p.node_cat);
        node_gen(oct, i, New_Nodes);
        writefln("row.gen.%d.after|new=%d|cells=%d|faces=%d|pts=%d", i,
                 New_Nodes.length, CELL_LIST.length, FACE_LIST.length,
                 POINT_LIST.length);
        foreach (j, q; New_Nodes)
            dumpP("row.gen." ~ to!string(i) ~ ".N" ~ to!string(j), q);
    }
    dumpLists("row.after");
    // generate_new_row: needs two consecutive row-side nodes
    resetAll();
    Point[] sq = mkrow([[0.0, 0.0], [1.0, 0.0], [1.0, 1.0], [0.0, 1.0]]);
    assign_node_properties(sq);
    adjust_row_ends(sq, initial_size, 1);
    foreach (i, p; sq)
        p.store_rowID(i);
    foreach (p; sq)
        makeFace(p.ID, p.pointR.ID);
    tryExc("row.gnr.sq.throw", ()
            {
                Point[] r = generate_new_row(sq);
                writefln("row.gnr.sq.n|%d", r.length);
                foreach (j, q; r)
                    dumpP("row.gnr.sq." ~ to!string(j), q);
            });
    dumpLists("row.gnr.sq.lists");
    resetAll();
    Point[] hex = mkrow([[0.0, 0.0], [2.0, 0.0], [3.0, 1.0], [2.0, 2.0], [0.0, 2.0],
                         [-1.0, 1.0]]);
    assign_node_properties(hex);
    adjust_row_ends(hex, initial_size, 1);
    foreach (i, p; hex)
        p.store_rowID(i);
    tryExc("row.gnr.hex.throw", ()
            {
                Point[] r2 = generate_new_row(hex);
                writefln("row.gnr.hex.n|%d", r2.length);
                foreach (j, q; r2)
                    dumpP("row.gnr.hex." ~ to!string(j), q);
            });
    dumpLists("row.gnr.hex.lists");
    // a row with no row-side node at all
    resetAll();
    Point[] noside = mkrow([[0.0, 0.0], [1.0, 0.0], [2.0, 0.0], [2.0, 1.0], [2.0, 2.0],
                            [1.0, 2.0], [0.0, 2.0], [0.0, 1.0]]);
    assign_node_properties(noside);
    foreach (p; noside)
        p.node_cat = "row end";
    tryExc("row.gnr.none.throw", () { generate_new_row(noside); });
    tryExc("row.gnr.empty.throw", () { generate_new_row([]); });
    // summed_squareness
    resetAll();
    Point[] ps = mkrow([[0.0, 0.0], [1.0, 0.0], [1.0, 1.0], [0.0, 1.0], [2.0, 0.5]]);
    Cell cc = makeCell([ps[0], ps[1], ps[2], ps[3]]);
    F("row.ss.in", summed_squareness(ps[0]));
    F("row.ss.in2", summed_squareness(ps[2]));
    F("row.ss.cell", squareness(cc));
    tryExc("row.ss.orphan.throw", () { F("row.ss.orphan", summed_squareness(ps[4])); });
}

//---------------------------------------------------------------- group smk
void grpSmk()
{
    resetAll();
    Point[] sq = mkrow([[0.0, 0.0], [1.0, 0.0], [1.0, 1.0], [0.0, 1.0]]);
    assign_node_properties(sq);
    foreach (p; sq)
        makeFace(p.ID, p.pointR.ID);
    makeCell([sq[0], sq[1], sq[2], sq[3]]);
    foreach (i, p; sq)
        dumpP("smk.before." ~ to!string(i), p);
    F("smk.move.b1f0", smoothing(sq, sq, 1, 0));
    foreach (i, p; sq)
        dumpP("smk.after.b1f0." ~ to!string(i), p);
    F("smk.move.b0f1", smoothing(sq, sq, 0, 1));
    foreach (i, p; sq)
        dumpP("smk.after.b0f1." ~ to!string(i), p);
    F("smk.move.b1f1", smoothing(sq, sq, 1, 1));
    F("smk.move.b0f0", smoothing(sq, sq, 0, 0));
    foreach (i, p; sq)
        dumpP("smk.after.b0f0." ~ to!string(i), p);
    // a node that no face mentions
    resetAll();
    Point[] sq2 = mkrow([[0.0, 0.0], [1.0, 0.0], [1.0, 1.0], [0.0, 1.0]]);
    assign_node_properties(sq2);
    foreach (p; sq2)
        makeFace(p.ID, p.pointR.ID);
    Point orphan = make2DPoint(5.0, 5.0);
    orphan.node_cat = "row side";
    F("smk.orphan", smoothing([orphan], sq2, 0, 0));
    F("smk.orphan.b1", smoothing([orphan], sq2, 1, 0));
    // empty surface
    F("smk.empty", smoothing([], sq2, 0, 0));
    // biggest_cell / even_out_size / fix_IDs / point_ID_check
    resetAll();
    Point[] grid = mkrow([[0.0, 0.0], [1.0, 0.0], [2.0, 0.0], [2.0, 1.0], [2.0, 2.0],
                          [1.0, 2.0], [0.0, 2.0], [0.0, 1.0]]);
    assign_node_properties(grid);
    adjust_row_ends(grid, 1.0, 1);
    foreach (i, p; grid)
        p.store_rowID(i);
    foreach (p; grid)
        makeFace(p.ID, p.pointR.ID);
    Cell ca = makeCell([grid[0], grid[1], grid[5], grid[6]]);
    Cell cb = makeCell([grid[1], grid[2], grid[3], grid[4]]);
    F("smk.biggest.1", biggest_cell([ca, cb], 1.0));
    F("smk.biggest.0", biggest_cell([ca, cb], 0.0));
    F("smk.biggest.empty", biggest_cell([], 2.0));
    F("smk.biggest.nan", biggest_cell([ca], double.init));
    F("smk.sq.a", squareness(ca));
    F("smk.sq.b", squareness(cb));
    dumpLists("smk.before.fix");
    FACE_LIST[0].face_ID = 77;
    CELL_LIST[0].cell_ID = 88;
    fix_IDs(FACE_LIST, CELL_LIST);
    dumpLists("smk.after.fix");
    tryExc("smk.pidc.ok", () { point_ID_check(); });
    POINT_LIST[2].ID = 4242;
    tryExc("smk.pidc.bad", () { point_ID_check(); });
    POINT_LIST[2].ID = 2;
    // even_out_size moves nodes in place
    resetAll();
    Point[] row = mkrow([[0.0, 0.0], [1.0, 0.0], [2.5, 0.2], [4.0, 0.0], [4.0, 1.0],
                         [2.0, 1.4], [0.0, 1.0]]);
    assign_node_properties(row);
    adjust_row_ends(row, 1.0, 1);
    foreach (i, p; row)
        dumpP("smk.eos.before." ~ to!string(i), p);
    even_out_size(row);
    foreach (i, p; row)
        dumpP("smk.eos.after." ~ to!string(i), p);
    resetAll();
    Point[] row2 = mkrow([[0.0, 0.0], [1.0, 0.0], [1.0, 1.0]]);
    assign_node_properties(row2);
    tryExc("smk.eos.tri", () { even_out_size(row2); });
    // seam: non-closing on a hand-built row with an acute corner
    resetAll();
    Point[] wedge = mkrow([[0.0, 0.0], [3.0, 0.0], [3.0, 0.6], [2.0, 0.6], [2.0, 1.2],
                           [1.0, 1.2], [0.0, 1.2], [0.0, 0.6]]);
    assign_node_properties(wedge);
    adjust_row_ends(wedge, 1.0, 1);
    foreach (i, p; wedge)
        p.store_rowID(i);
    foreach (p; wedge)
        makeFace(p.ID, p.pointR.ID);
    dumpLists("seam.b0.before");
    size_t[] adjust_IDs;
    size_t left = wedge.length;
    Point[] inner = seam(wedge[2], wedge, adjust_IDs, left, 0);
    writefln("seam.b0.done|inner=%d|adjust=%d|left=%d|cells=%d|faces=%d|pts=%d",
             inner.length, adjust_IDs.length, left, CELL_LIST.length, FACE_LIST.length,
             POINT_LIST.length);
    foreach (i, p; inner)
        dumpP("seam.b0.inner." ~ to!string(i), p);
    foreach (i, t; adjust_IDs)
        I("seam.b0.adj." ~ to!string(i), cast(long) t);
    dumpLists("seam.b0.lists");
    // the special four-node closing case
    resetAll();
    Point[] four = mkrow([[0.0, 0.0], [1.0, 0.0], [1.0, 1.0], [0.0, 1.0]]);
    assign_node_properties(four);
    foreach (i, p; four)
        p.store_rowID(i);
    foreach (p; four)
        makeFace(p.ID, p.pointR.ID);
    size_t[] ad2;
    size_t left2 = four.length;
    Point[] inner2 = seam(four[0], four, ad2, left2, 1);
    writefln("seam.sp.done|inner=%d|adjust=%d|left=%d|cells=%d", inner2.length,
             ad2.length, left2, CELL_LIST.length);
    // closing_seam over an odd row trips the assert with its D message
    resetAll();
    Point[] three = mkrow([[0.0, 0.0], [1.0, 0.0], [0.5, 1.0]]);
    assign_node_properties(three);
    tryExc("seam.cs.odd", () { closing_seam(three); });
    tryExc("seam.cs.empty", () { closing_seam([]); });
    resetAll();
    Point[] six = mkrow([[0.0, 0.0], [2.0, 0.0], [3.0, 1.0], [2.0, 2.0], [0.0, 2.0],
                         [-1.0, 1.0]]);
    assign_node_properties(six);
    foreach (i, p; six)
        p.store_rowID(i);
    foreach (p; six)
        makeFace(p.ID, p.pointR.ID);
    dumpLists("seam.cs6.before");
    tryExc("seam.cs6", () { closing_seam(six); });
    dumpLists("seam.cs6.lists");
    // the >5-cell-spaces arm of seam (paver.d:1227-1230). The two chains are made to
    // converge sharply (so the loop condition holds) while staying far apart (so the
    // third arm is taken): in the non-closing mode it writes its line and breaks.
    resetAll();
    Point st = make2DPoint(0.0, 0.0);
    Point L1 = make2DPoint(-1.0, 0.0);
    Point L2 = make2DPoint(-1.0, -0.05);
    Point L3 = make2DPoint(-1.0, -0.2);
    Point R1 = make2DPoint(1.0, 0.0);
    Point R2 = make2DPoint(0.98, -0.05);
    Point R3 = make2DPoint(0.96, -0.2);
    st.pointL = L1; st.pointR = R1;
    L1.pointL = L2; L1.pointR = st; L1.pointL.pointR = L1;
    R1.pointR = R2; R1.pointL = st; R1.pointR.pointL = R1;
    L2.pointL = L3; R2.pointR = R3;
    L3.pointR = L2; R3.pointL = R2;
    Point L4 = make2DPoint(-1.0, -0.5);
    Point R4 = make2DPoint(0.9, -0.5);
    L3.pointL = L4; R3.pointR = R4;
    Point[] wide = [st, R1, R2, R3, R4, L4, L3, L2, L1];
    foreach (i, p; wide)
        p.store_rowID(i);
    foreach (p; [st, R1, R2, L1, L2])
        makeFace(p.ID, p.pointR.ID);
    dumpLists("seam.wide.before");
    size_t[] ad3;
    size_t left3 = wide.length;
    Point[] inner3 = seam(st, wide, ad3, left3, 0);
    writefln("seam.wide.done|inner=%d|adjust=%d|left=%d|cells=%d", inner3.length,
             ad3.length, left3, CELL_LIST.length);
    foreach (i, p; inner3)
        dumpP("seam.wide.inner." ~ to!string(i), p);
    foreach (i, t; ad3)
        I("seam.wide.adj." ~ to!string(i), cast(long) t);
    dumpLists("seam.wide.lists");
}

//---------------------------------------------------------------- group seam2
void grpSeam2()
{
    // seaming_procedure: the acute-node arm and the "smallest angle node" arm
    resetAll();
    Point[] wedge = mkrow([[0.0, 0.0], [3.0, 0.0], [3.0, 0.6], [2.0, 0.6], [2.0, 1.2],
                           [1.0, 1.2], [0.0, 1.2], [0.0, 0.6]]);
    assign_node_properties(wedge);
    adjust_row_ends(wedge, 1.0, 1);
    foreach (i, p; wedge)
        p.store_rowID(i);
    foreach (p; wedge)
        makeFace(p.ID, p.pointR.ID);
    dumpLists("seam2.a.before");
    Point[] wcopy = wedge;
    tryExc("seam2.a", () { seaming_procedure(wcopy); });
    writefln("seam2.a.done|row=%d|cells=%d|faces=%d|pts=%d", wcopy.length,
             CELL_LIST.length, FACE_LIST.length, POINT_LIST.length);
    foreach (i, p; wcopy)
        dumpP("seam2.a.row." ~ to!string(i), p);
    dumpLists("seam2.a.lists");
    resetAll();
    Point[] obt = mkrow([[0.0, 0.0], [4.0, 0.0], [4.0, 2.0], [2.0, 2.4], [0.0, 2.0]]);
    assign_node_properties(obt);
    adjust_row_ends(obt, 1.0, 1);
    foreach (i, p; obt)
        p.store_rowID(i);
    foreach (p; obt)
        makeFace(p.ID, p.pointR.ID);
    Point[] ocopy = obt;
    tryExc("seam2.b", () { seaming_procedure(ocopy); });
    writefln("seam2.b.done|row=%d|cells=%d", ocopy.length, CELL_LIST.length);
    foreach (i, p; ocopy)
        dumpP("seam2.b.row." ~ to!string(i), p);
    resetAll();
    Point[] none = [];
    tryExc("seam2.empty", () { seaming_procedure(none); });
}

//---------------------------------------------------------------- group pave
pragma(once) void pave(string tag, double[][] b)
{
    resetAll();
    try
    {
        PavedGrid g = new PavedGrid(b);
        writefln("pave.%s.fields|dims=%d|nv=%d|nf=%d|nc=%d|nb=%d", tag, g.dimensions,
                 g.nvertices, g.nfaces, g.ncells, g.nboundaries);
        S("pave." ~ tag ~ ".label", g.label);
        F("pave." ~ tag ~ ".initial_size", initial_size);
        F("pave." ~ tag ~ ".avr_area", avr_area);
        dumpLists("pave." ~ tag);
    }
    catch (Exception e)
    {
        writefln("pave.%s|EXC|%s", tag, esc(e.msg));
        writefln("pave.%s.after|pts=%d|faces=%d|cells=%d", tag, POINT_LIST.length,
                 FACE_LIST.length, CELL_LIST.length);
    }
    catch (Error e)
    {
        writefln("pave.%s|ERR|%s", tag, esc(e.msg));
        writefln("pave.%s.after|pts=%d|faces=%d|cells=%d", tag, POINT_LIST.length,
                 FACE_LIST.length, CELL_LIST.length);
    }
    stdout.flush();
}

double[][] square(size_t n, double w, double h)
{
    double[][] b;
    for (size_t i; i < n; ++i)
        b ~= [w * i / cast(double) n, 0.0];
    for (size_t i; i < n; ++i)
        b ~= [w, h * i / cast(double) n];
    for (size_t i; i < n; ++i)
        b ~= [w - w * i / cast(double) n, h];
    for (size_t i; i < n; ++i)
        b ~= [0.0, h - h * i / cast(double) n];
    return b;
}

double[][] lshape(size_t n)
{
    double[][] b;
    void side(double[2] p, double[2] q)
    {
        for (size_t i; i < n; ++i)
            b ~= [p[0] + (q[0] - p[0]) * i / cast(double) n,
                  p[1] + (q[1] - p[1]) * i / cast(double) n];
    }
    side([0.0, 0.0], [2.0, 0.0]);
    side([2.0, 0.0], [2.0, 1.0]);
    side([2.0, 1.0], [1.0, 1.0]);
    side([1.0, 1.0], [1.0, 2.0]);
    side([1.0, 2.0], [0.0, 2.0]);
    side([0.0, 2.0], [0.0, 0.0]);
    return b;
}

double[][] wedge(size_t n)
{
    double[][] b;
    void side(double[2] p, double[2] q)
    {
        for (size_t i; i < n; ++i)
            b ~= [p[0] + (q[0] - p[0]) * i / cast(double) n,
                  p[1] + (q[1] - p[1]) * i / cast(double) n];
    }
    side([0.0, 0.0], [4.0, 0.0]);
    side([4.0, 0.0], [4.0, 0.4]);
    side([4.0, 0.4], [0.5, 2.0]);
    side([0.5, 2.0], [0.0, 1.0]);
    return b;
}

double[][] circle(size_t m, double r)
{
    double[][] b;
    size_t cnt = 2 * m;
    for (size_t i; i < cnt; ++i)
    {
        double t = 2 * PI * i / cast(double) cnt;
        b ~= [r * cos(t), r * sin(t)];
    }
    return b;
}

void grpPave()
{
    pave("sq1", square(1, 1, 1));
    pave("odd", [[0.0, 0.0], [1.0, 0.0], [1.0, 1.0]]);
    pave("empty", []);
    pave("single", [[0.0, 0.0]]);
    pave("cw", [[0.0, 0.0], [0.0, 1.0], [1.0, 1.0], [1.0, 0.0]]);
    pave("bowtie", [[0.0, 0.0], [1.0, 1.0], [1.0, 0.0], [0.0, 1.0]]);
    pave("rect8", square(2, 1, 1));
    pave("rect16", square(4, 1, 1));
    pave("wide", square(2, 3, 0.6));
    pave("lshape", lshape(3));
    pave("circle6", circle(6, 1));
    pave("wide16", square(4, 3, 0.6));
    pave("pwedge", wedge(3));
    pave("rect32", square(8, 1, 1));
}

void main()
{
    grpCls();
    grpSec2();
    grpSec3();
    grpMake();
    grpNode();
    grpRow();
    grpSmk();
    grpSeam2();
    grpPave();
    T("END", "done");
}
