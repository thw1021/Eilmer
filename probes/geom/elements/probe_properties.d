// probes/geom/elements/probe_properties.d — dump every quantity geom/elements/
// properties.d computes, at %.17g, for a battery of cells.
//
// Why this probe exists: properties.d is 1168 lines of volume/area/normal/centroid
// and mesh-quality arithmetic that the solver's CFL and gradient reconstruction
// consume. The port is gated on bit equality, so each function gets a table of
// cells (the ones its own unittest uses, plus skewed and degenerate variants that
// hit the collapse branches) and every result is printed.
//
// Every point is a named variable because properties.d takes its vertices as
// `ref const(Vector3)`, which D will not bind to an rvalue; the array overloads
// take a `Vector3[]` dynamic array for the same reason (`ref` slice).
//
// Two branches that only degenerate cells reach are measured explicitly:
//   * triangle_properties/quad_properties with area <= area_tol (properties.d:81-85,
//     :134-138) return the arbitrary orthogonal set (1,0,0),(0,1,0),(0,0,1).
//   * hex_cell_properties with a collapsed volume (properties.d:662-668) forces
//     volume = 0.0 using the module globals smallButSignificantVolume (1e-12) and
//     verySmallVolume (1e-20). Rows "hex.collapse_*" bracket both thresholds.
//
// Numeric fields are always written as `key=%.17g` so the C++ replay can pair them
// positionally with its own values; boolean/int fields are written as %s/%d.
//
// Build and capture:
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i -I/home/tang/packages/gdtk/src probes/geom/elements/probe_properties.d \
//     -of=/tmp/props_d && /tmp/props_d \
//     > baselines/e587b235/probes/geom_elements_properties.txt
import std.conv;
import std.format;
import std.math;
import std.stdio;

import nm.number;
import geom.elements.nomenclature;
import geom.elements.properties;
import geom.elements.vector3;

void main()
{
    // ---- inside_bounding_box -------------------------------------------------
    Vector3 bb0 = Vector3(1.0, 1.0, 1.0);
    Vector3 bb1 = Vector3(2.0, 2.0, 2.0);
    Vector3 pm = to!number(0.5) * (bb0 + bb1);
    Vector3 pz = pm + Vector3(0.0, 0.0, 1.0);
    Vector3 bl = Vector3(0.9999999, 1.0, 1.0);
    writefln("bbox.center dim3=%s dim2=%s", to!string(inside_bounding_box(pm, bb0, bb1, 3)),
             to!string(inside_bounding_box(pm, bb0, bb1, 2)));
    writefln("bbox.zplus1 dim3=%s dim2=%s", to!string(inside_bounding_box(pz, bb0, bb1, 3)),
             to!string(inside_bounding_box(pz, bb0, bb1, 2)));
    writefln("bbox.lo dim3=%s dim2=%s", to!string(inside_bounding_box(bb0, bb0, bb1, 3)),
             to!string(inside_bounding_box(bb0, bb0, bb1, 2)));
    writefln("bbox.hi dim3=%s dim2=%s", to!string(inside_bounding_box(bb1, bb0, bb1, 3)),
             to!string(inside_bounding_box(bb1, bb0, bb1, 2)));
    writefln("bbox.below dim3=%s dim2=%s", to!string(inside_bounding_box(bl, bb0, bb1, 3)),
             to!string(inside_bounding_box(bl, bb0, bb1, 2)));

    // ---- triangle_properties -------------------------------------------------
    double alpha = 1.0;
    Vector3 p0 = Vector3(0.0, 0.0, 1.0);
    Vector3 p1 = Vector3(alpha, 0.0, 1.0);
    Vector3 p2 = Vector3(alpha, alpha, 1.0);
    Vector3 centroid, n, t1, t2;
    number area;
    triangle_properties(p0, p1, p2, centroid, n, t1, t2, area);
    emit_cell("tri.unit", centroid, n, t1, t2, area);
    writefln("tri.unit.text centroid=%s n=%s", to!string(centroid), to!string(n));

    alpha = 0.7;
    p1 = Vector3(alpha, 0.0, 1.0);
    p2 = Vector3(alpha, alpha, 1.0);
    triangle_properties(p0, p1, p2, centroid, n, t1, t2, area);
    emit_cell("tri.alpha0.7", centroid, n, t1, t2, area);
    alpha = 1.0;
    p1 = Vector3(alpha, 0.0, 1.0);
    p2 = Vector3(alpha, alpha, 1.0);

    Vector3 s0 = Vector3(0.0, 0.0, 0.0);
    Vector3 s1 = Vector3(1.0, 0.2, 0.3);
    Vector3 s2 = Vector3(-0.4, 1.0, 0.6);
    triangle_properties(s0, s1, s2, centroid, n, t1, t2, area);
    emit_cell("tri.skew3d", centroid, n, t1, t2, area);

    // degenerate: zero area -> the arbitrary orthogonal set
    triangle_properties(s0, s0, s1, centroid, n, t1, t2, area);
    emit_cell("tri.degenerate", centroid, n, t1, t2, area);
    // Just above the area tolerance. These are decimal *literals*, not `1.0 + 1.0e-10`
    // and `0.2 + 1.0e-10`: ldc2 1.40.0 folds float constant expressions to a double that
    // is NOT the correctly-rounded result of the same run-time addition (measured:
    // fold 0.20000000009999999 vs run 0.20000000010000002 for `0.2 + 1.0e-10`, and the
    // same 1 ULP for `0.2 * 1.0e-10`), while g++ folds to the correctly-rounded value.
    // This cell is a cancellation case, so a 1 ULP input shift moves the area by 3e-7
    // relative; literals keep the two sides on identical inputs.
    Vector3 sliver = Vector3(1.0000000001, 0.2000000001, 0.3);
    triangle_properties(s0, s1, sliver, centroid, n, t1, t2, area);
    emit_cell("tri.sliver", centroid, n, t1, t2, area);

    // ---- quad_properties ----------------------------------------------------
    Vector3 p3 = Vector3(0.0, alpha, 1.0);
    quad_properties(p0, p1, p2, p3, centroid, n, t1, t2, area);
    emit_cell("quad.unit", centroid, n, t1, t2, area);
    writefln("quad.unit.text centroid=%s t2=%s", to!string(centroid), to!string(t2));

    Vector3 q0 = Vector3(0.0, 0.0, 0.0);
    Vector3 q1 = Vector3(1.0, 0.1, 0.0);
    Vector3 q2 = Vector3(1.2, 1.0, 0.4);
    Vector3 q3 = Vector3(-0.1, 0.9, 0.2);
    quad_properties(q0, q1, q2, q3, centroid, n, t1, t2, area);
    emit_cell("quad.skew3d", centroid, n, t1, t2, area);

    const(Vector3)[] qarr = [q0, q1, q2, q3];
    quad_properties(qarr, centroid, n, t1, t2, area);
    emit_cell("quad.skew3d.arrayform", centroid, n, t1, t2, area);

    // degenerate quad: collapsed to a line
    quad_properties(q0, q1, q1, q0, centroid, n, t1, t2, area);
    emit_cell("quad.degenerate", centroid, n, t1, t2, area);

    // ---- xyplane_area -------------------------------------------------------
    writefln("xyarea3.unit=%.17g", xyplane_area(p0, p1, p2));
    writefln("xyarea3.neg=%.17g", xyplane_area(p0, p2, p1));
    writefln("xyarea3.skew=%.17g", xyplane_area(q0, q1, q2));
    writefln("xyarea4.unit=%.17g", xyplane_area(p0, p1, p2, p3));
    writefln("xyarea4.skew=%.17g", xyplane_area(q0, q1, q2, q3));
    writefln("xyarea4.reversed=%.17g", xyplane_area(q3, q2, q1, q0));

    // ---- xyplane_quad_cell_properties / squareness --------------------------
    Vector3 c2;
    number a_xy, iLen, jLen, minLen;
    xyplane_quad_cell_properties(p0, p1, p2, p3, c2, a_xy, iLen, jLen, minLen);
    number sq = xyplane_quad_squareness(p0, p1, p2, p3);
    writefln("xyquadcell.unit cx=%.17g cy=%.17g cz=%.17g area=%.17g iLen=%.17g jLen=%.17g minLen=%.17g sq=%.17g",
             c2.x, c2.y, c2.z, a_xy, iLen, jLen, minLen, sq);
    xyplane_quad_cell_properties(q0, q1, q2, q3, c2, a_xy, iLen, jLen, minLen);
    number sq2 = xyplane_quad_squareness(q0, q1, q2, q3);
    writefln("xyquadcell.skew cx=%.17g cy=%.17g cz=%.17g area=%.17g iLen=%.17g jLen=%.17g minLen=%.17g sq=%.17g",
             c2.x, c2.y, c2.z, a_xy, iLen, jLen, minLen, sq2);
    // collapsed sliver quad: minLen follows length_cross, not the edge lengths
    Vector3 r0 = Vector3(0.0, 0.0);
    Vector3 r1 = Vector3(1.0, 0.0);
    Vector3 r2 = Vector3(1.0, 1.0e-4);
    Vector3 r3 = Vector3(0.0, 1.0e-4);
    xyplane_quad_cell_properties(r0, r1, r2, r3, c2, a_xy, iLen, jLen, minLen);
    number sq3 = xyplane_quad_squareness(r0, r1, r2, r3);
    writefln("xyquadcell.sliver cx=%.17g cy=%.17g cz=%.17g area=%.17g iLen=%.17g jLen=%.17g minLen=%.17g sq=%.17g",
             c2.x, c2.y, c2.z, a_xy, iLen, jLen, minLen, sq3);

    xyplane_triangle_cell_properties(p0, p1, p2, c2, a_xy, iLen, jLen, minLen);
    writefln("xytricell.unit cx=%.17g cy=%.17g cz=%.17g area=%.17g iLen=%.17g jLen=%.17g minLen=%.17g",
             c2.x, c2.y, c2.z, a_xy, iLen, jLen, minLen);
    xyplane_triangle_cell_properties(q0, q1, q2, c2, a_xy, iLen, jLen, minLen);
    writefln("xytricell.skew cx=%.17g cy=%.17g cz=%.17g area=%.17g iLen=%.17g jLen=%.17g minLen=%.17g",
             c2.x, c2.y, c2.z, a_xy, iLen, jLen, minLen);
    xyplane_triangle_cell_properties(q2, q1, q0, c2, a_xy, iLen, jLen, minLen);
    writefln("xytricell.negarea cx=%.17g cy=%.17g cz=%.17g area=%.17g iLen=%.17g jLen=%.17g minLen=%.17g",
             c2.x, c2.y, c2.z, a_xy, iLen, jLen, minLen);

    // ---- tetrahedron --------------------------------------------------------
    Vector3 t0 = Vector3(0, 0, 0);
    Vector3 t1v = Vector3(cos(radians(30)), sin(radians(30)), 0.0);
    Vector3 t2v = Vector3(0.0, 1.0, 0.0);
    double dx = 0.5 * tan(radians(30));
    double dL = cos(radians(30));
    double dz = sqrt(dL * dL - dx * dx);
    Vector3 t3v = Vector3(dx, 0.5, dz);
    writefln("tet.consts dx=%.17g dL=%.17g dz=%.17g", dx, dL, dz);
    number volume;
    const(Vector3)[] tet = [t0, t1v, t2v, t3v];
    Vector3 t21 = Vector3(t2v);
    Vector3 t12 = Vector3(t1v);
    writefln("tet.vol=%.17g", tetrahedron_volume(t0, t1v, t2v, t3v));
    writefln("tet.vol.neg=%.17g", tetrahedron_volume(t0, t21, t12, t3v));
    tetrahedron_properties(t0, t1v, t2v, t3v, centroid, volume);
    writefln("tet.prop cx=%.17g cy=%.17g cz=%.17g vol=%.17g", centroid.x, centroid.y, centroid.z,
             volume);
    tetrahedron_properties(tet, centroid, volume);
    writefln("tet.prop.arrayform cx=%.17g cy=%.17g cz=%.17g vol=%.17g", centroid.x, centroid.y,
             centroid.z, volume);
    number L_min;
    tetrahedron_properties(t0, t1v, t2v, t3v, centroid, volume, L_min);
    writefln("tet.propLmin cx=%.17g cy=%.17g cz=%.17g vol=%.17g Lmin=%.17g", centroid.x, centroid.y,
             centroid.z, volume, L_min);
    tetrahedron_properties(tet, centroid, volume, L_min);
    writefln("tet.propLmin.arrayform cx=%.17g cy=%.17g cz=%.17g vol=%.17g Lmin=%.17g", centroid.x,
             centroid.y, centroid.z, volume, L_min);
    // regular tetrahedron, edge 1
    Vector3 o0 = Vector3(0.0, 0.0, 0.0);
    Vector3 rt1 = Vector3(1.0, 0.0, 0.0);
    Vector3 rt2 = Vector3(cos(radians(60)), sin(radians(60)), 0.0);
    Vector3 rt3 = Vector3(0.5, 0.5 * tan(radians(30)), sqrt(6.0)/3.0);
    tetrahedron_properties(rt1, rt2, rt3, o0, centroid, volume, L_min);
    writefln("tet.regular cx=%.17g cy=%.17g cz=%.17g vol=%.17g Lmin=%.17g", centroid.x, centroid.y,
             centroid.z, volume, L_min);
    // degenerate tetra (flat)
    Vector3 flat3 = Vector3(0.3, 0.4, 0.0);
    tetrahedron_properties(t0, t1v, t2v, flat3, centroid, volume, L_min);
    writefln("tet.flat cx=%.17g cy=%.17g cz=%.17g vol=%.17g Lmin=%.17g", centroid.x, centroid.y,
             centroid.z, volume, L_min);

    // ---- tetragonal_dipyramid_volume ----------------------------------------
    Vector3 pmB = Vector3(0.25*(t0.x+t1v.x+t2v.x+t3v.x), 0.25*(t0.y+t1v.y+t2v.y+t3v.y),
                          0.25*(t0.z+t1v.z+t2v.z+t3v.z));
    Vector3 apex = Vector3(t3v);
    writefln("dipyramid.tet=%.17g", tetragonal_dipyramid_volume(t0, t1v, t2v, t3v, pmB, apex));
    Vector3 h0 = Vector3(0, 0, 0);
    Vector3 h1 = Vector3(1, 0, 0);
    Vector3 h2 = Vector3(1, 1, 0);
    Vector3 h3 = Vector3(0, 1, 0);
    Vector3 h4 = Vector3(0, 0, 1);
    Vector3 h5 = Vector3(1, 0, 1);
    Vector3 h6 = Vector3(1, 1, 1);
    Vector3 h7 = Vector3(0, 1, 1);
    Vector3 basem = Vector3(0.25, 0.25, 0.0);
    Vector3 apix = Vector3(h6);
    writefln("dipyramid.unitbase=%.17g", tetragonal_dipyramid_volume(h2, h3, h7, h6, basem, apix));

    // ---- pyramid ------------------------------------------------------------
    Vector3 py0 = Vector3(h0);
    Vector3 py1 = Vector3(h1);
    Vector3 py2 = Vector3(h2);
    Vector3 py3 = Vector3(h3);
    Vector3 py4 = Vector3(0.5, 0.5, 1); // peak
    pyramid_properties(py0, py1, py2, py3, py4, true, centroid, volume);
    writefln("pir.true cx=%.17g cy=%.17g cz=%.17g vol=%.17g", centroid.x, centroid.y, centroid.z,
             volume);
    pyramid_properties(py0, py1, py2, py3, py4, false, centroid, volume);
    writefln("pir.approx cx=%.17g cy=%.17g cz=%.17g vol=%.17g", centroid.x, centroid.y, centroid.z,
             volume);
    const(Vector3)[] pirarr = [py0, py1, py2, py3, py4];
    pyramid_properties(pirarr, true, centroid, volume);
    writefln("pir.true.arrayform cx=%.17g cy=%.17g cz=%.17g vol=%.17g", centroid.x, centroid.y,
             centroid.z, volume);
    pyramid_properties(py0, py1, py2, py3, py4, true, centroid, volume, L_min);
    writefln("pir.trueLmin cx=%.17g cy=%.17g cz=%.17g vol=%.17g Lmin=%.17g", centroid.x, centroid.y,
             centroid.z, volume, L_min);
    pyramid_properties(pirarr, true, centroid, volume, L_min);
    writefln("pir.trueLmin.arrayform cx=%.17g cy=%.17g cz=%.17g vol=%.17g Lmin=%.17g", centroid.x,
             centroid.y, centroid.z, volume, L_min);
    Vector3 wpy4 = Vector3(0.3, 0.7, 1.4);
    pyramid_properties(py0, py1, py2, py3, wpy4, true, centroid, volume, L_min);
    writefln("pir.skew cx=%.17g cy=%.17g cz=%.17g vol=%.17g Lmin=%.17g", centroid.x, centroid.y,
             centroid.z, volume, L_min);
    writefln("pir.skew.text centroid=%s vol6=%s", to!string(centroid), to!string(volume));

    // ---- wedge -------------------------------------------------------------
    Vector3 w0 = Vector3(0, 0, 0);
    Vector3 w1 = Vector3(cos(radians(30)), sin(radians(30)), 0.0);
    Vector3 w2 = Vector3(0.0, 1.0, 0.0);
    Vector3 incz = Vector3(0, 0, -1.0);
    Vector3 w3 = w0 + incz;
    Vector3 w4 = w1 + incz;
    Vector3 w5 = w2 + incz;
    wedge_properties(w0, w1, w2, w3, w4, w5, true, centroid, volume);
    writefln("wed.true cx=%.17g cy=%.17g cz=%.17g vol=%.17g", centroid.x, centroid.y, centroid.z,
             volume);
    wedge_properties(w0, w1, w2, w3, w4, w5, false, centroid, volume);
    writefln("wed.approx cx=%.17g cy=%.17g cz=%.17g vol=%.17g", centroid.x, centroid.y, centroid.z,
             volume);
    const(Vector3)[] wedarr = [w0, w1, w2, w3, w4, w5];
    wedge_properties(wedarr, true, centroid, volume);
    writefln("wed.true.arrayform cx=%.17g cy=%.17g cz=%.17g vol=%.17g", centroid.x, centroid.y,
             centroid.z, volume);
    wedge_properties(w0, w1, w2, w3, w4, w5, true, centroid, volume, L_min);
    writefln("wed.trueLmin cx=%.17g cy=%.17g cz=%.17g vol=%.17g Lmin=%.17g", centroid.x, centroid.y,
             centroid.z, volume, L_min);
    wedge_properties(wedarr, true, centroid, volume, L_min);
    writefln("wed.trueLmin.arrayform cx=%.17g cy=%.17g cz=%.17g vol=%.17g Lmin=%.17g", centroid.x,
             centroid.y, centroid.z, volume, L_min);
    Vector3 w1s = Vector3(1.1, 0.2, 0.0);
    Vector3 w2s = Vector3(-0.2, 1.3, 0.1);
    Vector3 w3s = Vector3(0.05, 0.1, -0.9);
    Vector3 w4s = Vector3(1.05, 0.25, -1.1);
    Vector3 w5s = Vector3(-0.1, 1.2, -0.95);
    wedge_properties(w0, w1s, w2s, w3s, w4s, w5s, true, centroid, volume, L_min);
    writefln("wed.skew cx=%.17g cy=%.17g cz=%.17g vol=%.17g Lmin=%.17g", centroid.x, centroid.y,
             centroid.z, volume, L_min);

    // ---- hex_cell_properties ------------------------------------------------
    number kLen;
    hex_cell_properties(h0, h1, h2, h3, h4, h5, h6, h7, true, centroid, volume, iLen, jLen, kLen);
    writefln("hex.cube cx=%.17g cy=%.17g cz=%.17g vol=%.17g iLen=%.17g jLen=%.17g kLen=%.17g",
             centroid.x, centroid.y, centroid.z, volume, iLen, jLen, kLen);
    hex_cell_properties(h0, h1, h2, h3, h4, h5, h6, h7, false, centroid, volume, iLen, jLen, kLen);
    writefln("hex.cube.approx cx=%.17g cy=%.17g cz=%.17g vol=%.17g iLen=%.17g jLen=%.17g kLen=%.17g",
             centroid.x, centroid.y, centroid.z, volume, iLen, jLen, kLen);
    const(Vector3)[] hexarr = [h0, h1, h2, h3, h4, h5, h6, h7];
    hex_cell_properties(hexarr, true, centroid, volume, iLen, jLen, kLen);
    writefln("hex.cube.arrayform cx=%.17g cy=%.17g cz=%.17g vol=%.17g iLen=%.17g jLen=%.17g kLen=%.17g",
             centroid.x, centroid.y, centroid.z, volume, iLen, jLen, kLen);
    // warped thin hex (the case the comment at properties.d:598-603 describes)
    Vector3 f0 = Vector3(0.0, 0.0, 0.0);
    Vector3 f1 = Vector3(1.0, 0.0, 0.05);
    Vector3 f2 = Vector3(1.1, 1.0, 0.0);
    Vector3 f3 = Vector3(0.0, 1.0, 0.04);
    Vector3 f4 = Vector3(0.0, 0.0, 1.0e-3);
    Vector3 f5 = Vector3(1.0, 0.05, 0.9e-3);
    Vector3 f6 = Vector3(1.1, 1.0, 1.1e-3);
    Vector3 f7 = Vector3(0.0, 1.0, 1.05e-3);
    hex_cell_properties(f0, f1, f2, f3, f4, f5, f6, f7, true, centroid, volume, iLen, jLen, kLen);
    writefln("hex.warped cx=%.17g cy=%.17g cz=%.17g vol=%.17g iLen=%.17g jLen=%.17g kLen=%.17g",
             centroid.x, centroid.y, centroid.z, volume, iLen, jLen, kLen);
    // collapsed planar hex: top face coincides with the bottom -> verySmallVolume
    hex_cell_properties(h0, h1, h2, h3, h0, h1, h2, h3, true, centroid, volume, iLen, jLen, kLen);
    writefln("hex.collapse_exact0 cx=%.17g cy=%.17g cz=%.17g vol=%.17g iLen=%.17g jLen=%.17g kLen=%.17g",
             centroid.x, centroid.y, centroid.z, volume, iLen, jLen, kLen);
    // thin hexes that bracket both thresholds of properties.d:662-663. h is the cell
    // height; the raw signed volume is ~1*h, so h=-1e-13 lands inside
    // smallButSignificantVolume while h=-1e-11 does not, and h=+1e-21 lands inside
    // verySmallVolume while h=+1e-19 does not.
    number[6] heights = [-1.0e-13, -1.0e-11, 1.0e-21, 1.0e-19, 1.0e-3, 0.0];
    foreach (h; heights) {
        Vector3 u4 = Vector3(0.0, 0.0, h);
        Vector3 u5 = Vector3(1.0, 0.0, h);
        Vector3 u6 = Vector3(1.0, 1.0, h);
        Vector3 u7 = Vector3(0.0, 1.0, h);
        hex_cell_properties(h0, h1, h2, h3, u4, u5, u6, u7, true, centroid, volume, iLen, jLen,
                            kLen);
        writefln("hex.thin h=%.17g cx=%.17g cy=%.17g cz=%.17g vol=%.17g iLen=%.17g jLen=%.17g kLen=%.17g",
                 h, centroid.x, centroid.y, centroid.z, volume, iLen, jLen, kLen);
    }
    // thresholds themselves
    writefln("hex.thresholds smallButSignificantVolume=%.17g verySmallVolume=%.17g",
             smallButSignificantVolume, verySmallVolume);
    // pow(negative volume, 1/3) is a NaN and fmin eats it (tetrahedron_properties
    // at properties.d:335-336, pyramid_properties at :478, wedge_properties at :567)
    writefln("nan.cbrt vol=%.17g pow3=%.17g fmin=%.17g fminRev=%.17g", -0.11785113019775791,
             pow(-0.11785113019775791, 1.0/3.0), fmin(pow(-0.11785113019775791, 1.0/3.0), 0.4082),
             fmin(0.4082, pow(-0.11785113019775791, 1.0/3.0)));
    writefln("nan.sqrt sqrtNeg=%s tostr=%s", to!string(sqrt(-0.44)), to!string(sqrt(-1.0e-20)));
    // a big skewed hex, printed forms too
    Vector3 b0 = Vector3(0.1, 0.0, 0.0);
    Vector3 b1 = Vector3(1.0, 0.1, 0.0);
    Vector3 b2 = Vector3(1.2, 1.0, 0.05);
    Vector3 b3 = Vector3(0.05, 0.9, -0.02);
    Vector3 b4 = Vector3(0.12, 0.02, 1.0);
    Vector3 b5 = Vector3(1.05, 0.11, 0.95);
    Vector3 b6 = Vector3(1.3, 1.1, 1.02);
    Vector3 b7 = Vector3(0.06, 0.95, 1.01);
    hex_cell_properties(b0, b1, b2, b3, b4, b5, b6, b7, true, centroid, volume, iLen, jLen, kLen);
    writefln("hex.skew cx=%.17g cy=%.17g cz=%.17g vol=%.17g iLen=%.17g jLen=%.17g kLen=%.17g",
             centroid.x, centroid.y, centroid.z, volume, iLen, jLen, kLen);
    writefln("hex.skew.text centroid=%s vol6=%s iLen6=%s", to!string(centroid), to!string(volume),
             to!string(iLen));

    // ---- point-in-cell predicates ------------------------------------------
    Vector3 d = Vector3(0.65, 0.0, 0.0);
    Vector3 ee = Vector3(0.65, -0.1, 0.0);
    Vector3 a = Vector3(1.0, 0.0, 0.0);
    Vector3 b = Vector3(1.0, 1.0, 0.0);
    Vector3 c = Vector3(0.5, 0.0, 0.0);
    Vector3 da = Vector3(a);
    writefln("inside_tri.d=%d inside_tri.e=%d inside_tri.a=%d", inside_triangle(d, a, b, c),
             inside_triangle(ee, a, b, c), inside_triangle(da, a, b, c));
    writefln("on_left.ab.d=%s on_left.ab.e=%s on_left.ba.d=%s",
             to!string(on_left_of_xy_line(a, b, d)), to!string(on_left_of_xy_line(a, b, ee)),
             to!string(on_left_of_xy_line(b, a, d)));
    Vector3 v0 = Vector3(0.0, 0.0);
    Vector3 v1 = Vector3(1.0, -0.1);
    Vector3 v2 = Vector3(1.0, 1.0);
    Vector3 v3 = Vector3(0.0, 1.0);
    const(Vector3)[] poly = [v0, v1, v2, v3];
    Vector3[] polyW = [v0, v1, v2, v3];
    writefln("xy_polygon.d=%s xy_polygon.e=%s xy_triangle.d=%s xy_triangle.e=%s xy_quad.d=%s xy_quad.e=%s",
             to!string(inside_xy_polygon(poly, d)), to!string(inside_xy_polygon(poly, ee)),
             to!string(inside_xy_triangle(v0, v1, v2, d)),
             to!string(inside_xy_triangle(v0, v1, v2, ee)),
             to!string(inside_xy_quad(v0, v1, v2, v3, d)),
             to!string(inside_xy_quad(v0, v1, v2, v3, ee)));
    Vector3 farp = Vector3(2.0, 2.0);
    Vector3 v0copy = Vector3(v0);
    writefln("winding.d=%d winding.e=%d winding.corner=%d winding.outside=%d",
             winding_number(polyW, d), winding_number(polyW, ee), winding_number(polyW, v0copy),
             winding_number(polyW, farp));
    Vector3[] polyRev = [v3, v2, v1, v0];
    writefln("winding.reversed.d=%d", winding_number(polyRev, d));

    Vector3 hc = Vector3(0.5, 0.5, 0.5);
    Vector3 hOut = Vector3(1.5, 0.5, 0.5);
    Vector3 h0copy = Vector3(h0);
    writefln("in_hex.center=%s in_hex.e=%s in_hex.corner=%s in_hex.outside=%s",
             to!string(inside_hexahedron(h0, h1, h2, h3, h4, h5, h6, h7, hc)),
             to!string(inside_hexahedron(h0, h1, h2, h3, h4, h5, h6, h7, ee)),
             to!string(inside_hexahedron(h0, h1, h2, h3, h4, h5, h6, h7, h0copy)),
             to!string(inside_hexahedron(h0, h1, h2, h3, h4, h5, h6, h7, hOut)));
    Vector3 f1p = Vector3(0.1, 0.0, 0.5);
    Vector3 f2p = Vector3(0.0, 0.2, 0.5);
    Vector3 f3p = Vector3(0.0, -0.2, 0.5);
    Vector3 f4p = Vector3(1.0, 0.0, 1.0);
    Vector3 f5p = Vector3(0.0, 0.0, -0.5);
    writefln("in_tet.f1=%s f2=%s f3=%s f4=%s f5=%s",
             to!string(inside_tetrahedron(h0, h1, h2, h4, f1p)),
             to!string(inside_tetrahedron(h0, h1, h2, h4, f2p)),
             to!string(inside_tetrahedron(h0, h1, h2, h4, f3p)),
             to!string(inside_tetrahedron(h0, h1, h2, h4, f4p)),
             to!string(inside_tetrahedron(h0, h1, h2, h4, f5p)));
    Vector3 pc1 = Vector3(0.4, 0.4, 0.4);
    Vector3 pc2 = Vector3(0.5, 0.5, 1.5);
    Vector3 py0copy = Vector3(py0);
    writefln("in_pyramid.center=%s corner=%s above=%s",
             to!string(inside_pyramid(h0, h1, h2, h3, h4, pc1)),
             to!string(inside_pyramid(h0, h1, h2, h3, h4, py0copy)),
             to!string(inside_pyramid(h0, h1, h2, h3, h4, pc2)));
    Vector3 wc1 = Vector3(0.4, 0.3, 0.5);
    Vector3 wc2 = Vector3(0.4, 0.3, -0.5);
    writefln("in_wedge.center=%s below=%s corner=%s",
             to!string(inside_wedge(h0, h1, h2, h4, h5, h6, wc1)),
             to!string(inside_wedge(h0, h1, h2, h4, h5, h6, wc2)),
             to!string(inside_wedge(h0, h1, h2, h4, h5, h6, h0copy)));
}

/// Emit the centroid/n/t1/t2/area block, one %.17g field per component, in a
/// fixed order (used by both the triangle and the quad sections).
void emit_cell(string tag, ref const(Vector3) c, ref const(Vector3) n, ref const(Vector3) t1,
               ref const(Vector3) t2, number area)
{
    writefln("%s cx=%.17g cy=%.17g cz=%.17g nx=%.17g ny=%.17g nz=%.17g t1x=%.17g t1y=%.17g t1z=%.17g t2x=%.17g t2y=%.17g t2z=%.17g area=%.17g",
             tag, c.x, c.y, c.z, n.x, n.y, n.z, t1.x, t1.y, t1.z, t2.x, t2.y, t2.z, area);
}
