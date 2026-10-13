// fmt_probe.d
//
// Ground-truth probe for the C++ substrate.
//
// Phobos is shipped precompiled on this machine (no import/std/math.d), so the
// exact byte output of std.format / std.conv and the truth table of
// std.math.isClose cannot be read from source. Instead we run the real thing
// and commit its output as the corpus that substrate/include/eil/format.hpp and
// substrate/include/eil/math.hpp must reproduce byte for byte.
//
// Build (read-only against the gdtk toolchain, no gdtk modules needed):
//   PATH=/home/tang/packages/dlang/bin:$PATH \
//     ldc2 -Onone -of=tools/fmt_probe tools/fmt_probe.d
// Capture:
//   ./tools/fmt_probe > tools/fmt_corpus.txt
//
// The C++ counterpart is tests/test_format_corpus.cpp, which must emit the
// identical lines. Keep the two case tables in step when adding specs.
//
// -Onone on purpose: constant folding through extended intermediates would make
// the probe describe a code path the real solver never takes.

import std.stdio;
import std.format;
import std.conv;
import std.math;
import std.exception;
import std.array : array;
import std.algorithm.sorting : sort;

void main()
{
    // ------------------------------------------------------------------
    // 1. Shortest round-trip conversion, D's `to!string(double)`.
    // ------------------------------------------------------------------
    double[] roundTrip = [
        0.0, -0.0, 1.0, -1.0, 0.5, 3.14, 3.141592653589793,
        1.0e-5, 1.0e-4, 1.0e15, 1.0e16, 1.0e17, 1.2345678901234567e-8,
        1.0/3.0, 2.0/3.0, 1e-300, nextafter(0.0, 1.0), 1.7976931348623157e308,
        double.max, double.min_normal,
    ];
    foreach (v; roundTrip) writefln("rt %s", to!string(v));

    // Integers and bools, as the solver prints them.
    foreach (v; [0, 1, -1, 2_147_483_647, int.min, 4_294_967_295L])
        writefln("int %s", to!string(v));
    foreach (v; [true, false]) writefln("bool %s", to!string(v));

    // ------------------------------------------------------------------
    // 2. Numeric format specs actually used by the solver.
    //    (grep of src/eilmer shows %g, %e, %.Ne, %.Nf, %d, %s, %x, %5.3f etc.)
    // ------------------------------------------------------------------
    string[] specs = ["%g", "%.6f", "%.3f", "%e", "%14.6e", "%8.3f",
                      "%.0e", "%.10e", "%20.12f", "%x", "%d", "%s", "%%"];
    double[] vals = [
        0.0, -0.0, 1.0, 3.141592653589793, 1.0e-7, 1.23456789e12,
        9.9999999999, 0.0001, 0.00001, 1.0/3.0, -2.5, 1e300,
        double.infinity, -double.infinity, double.nan, 12345.0, -12345.0,
        1e-13, 4.0,
    ];
    foreach (sp; specs) foreach (v; vals) {
        try writefln("spec %s %a => |%s|", sp, v, format(sp, v));
        catch (Exception e) writefln("spec %s %a => ERR %s", sp, v, e.msg);
    }
    // Same specs against integers, where the solver prints indices and counts.
    foreach (sp; ["%d", "%5d", "%s", "%x", "%g"]) foreach (v; [0, 7, -7, 1000000]) {
        try writefln("ispec %s %d => |%s|", sp, v, format(sp, v));
        catch (Exception e) writefln("ispec %s %d => ERR %s", sp, v, e.msg);
    }

    // ------------------------------------------------------------------
    // 3. Array printing. `writeln(number[])` is how the solver dumps state
    //    vectors and what post.lua-facing text relies on.
    // ------------------------------------------------------------------
    double[] a = [1.0, 2.5, -3.0];
    writefln("arr %s", to!string(a));
    writeln("arr ", a);
    double[] e = [];
    writeln("arr ", e);
    double[] one = [0.0];
    writeln("arr ", one);
    int[] ia = [1, 2, 3];
    writeln("arr ", ia);

    // ------------------------------------------------------------------
    // 4. std.math.isClose truth table.
    //    nm/number.d:approxEqualNumbers is defined entirely in terms of it, and
    //    the port's tolerance gates call that function.
    // ------------------------------------------------------------------
    struct Case { double x; double y; double rel; double absd; }
    Case[] cases;
    double[2][] pairs = [
        [0.0, 0.0], [0.0, -0.0], [1.0, 1.0], [1.0, 1.0 + 1e-16],
        [1.0, 1.0 + 1e-14], [1.0, 1.001], [1.0, 1.01], [1.0, 1.02],
        [1.0, 1.1], [-1.0, 1.0], [0.0, 1e-6], [0.0, 1e-4],
        [1e-300, 1.00001e-300], [1e100, 1.001e100],
        [100.0, 100.9], [100.0, 101.0], [100.0, 100.99999],
        [double.infinity, double.infinity], [double.nan, double.nan],
        [double.nan, 1.0], [1.0, double.nan],
        [double.max, double.max], [0.0, double.min_normal],
        [1.0e-5, 0.0], [1.0e-3, 0.0],
    ];
    double[] rels = [1.0e-2, 1.0e-8, 0.0, 1.0];
    double[] abss = [1.0e-5, 0.0, 1.0e-9, 1.0];
    foreach (p; pairs) foreach (r; rels) foreach (ad; abss)
        cases ~= Case(p[0], p[1], r, ad);
    foreach (c; cases)
        writefln("isclose %.17g %.17g %.17g %.17g => %s",
                 c.x, c.y, c.rel, c.absd,
                 isClose(c.x, c.y, c.rel, c.absd));

    // Boundary rows: these decide `<` versus `<=` in each branch, which is the
    // difference between a gate that passes at exactly the tolerance and one
    // that does not.
    writefln("bound_abs_y_nonzero %s", isClose(1.0, 1.00001, 0.0, 1e-5 / 1.0));
    writefln("bound_rel_y_nonzero %s", isClose(2.0, 1.0, 1.0, 0.0));
    writefln("bound_rel_y_nonzero_lt %s", isClose(2.0, 1.0, 0.9999999999, 0.0));
    writefln("bound_abs_eq %s", isClose(3.0, 2.0, 0.0, 1.0));
    writefln("bound_abs_lt %s", isClose(3.0, 2.0, 0.0, 0.9999999999));
    writefln("bound_y0_abs_eq %s", isClose(1e-5, 0.0, 0.0, 1e-5));
    writefln("bound_y0_rel_eq %s", isClose(1e-5, 0.0, 1.0, 0.0));
    writefln("bound_y0_rel_big %s", isClose(1e-5, 0.0, 2.0, 0.0));
    writefln("bound_y0_rel_small %s", isClose(1e-5, 0.0, 0.5, 0.0));
    writefln("bound_y0_zero_x %s", isClose(0.0, 0.0, 0.0, 0.0));
    writefln("bound_symmetric %s %s", isClose(1.0, 2.0, 1.0, 0.0), isClose(2.0, 1.0, 1.0, 0.0));

    // Default-argument form, which is what approxEqualNumbers overrides call
    // when the caller omits tolerances.
    foreach (p; pairs) writefln("isclose_dflt %.17g %.17g => %s",
                                p[0], p[1], isClose(p[0], p[1]));

    // ------------------------------------------------------------------
    // 5. Error/exception text the solver surfaces to stdout.
    // ------------------------------------------------------------------
    try enforce(false, "enforce with %s and %d", "a string", 42);
    catch (Exception e) writefln("enforce %s", e.msg);
    try to!double("not a number");
    catch (Exception e) writefln("conv_err %s", e.msg);
    try { int[] s = [1, 2]; writeln(s[5]); }
    catch (Throwable t) writefln("range_err %s", t.msg);
    try { auto m = new int[3]; m[10] = 1; }
    catch (Throwable t) writefln("bounds %s", t.msg);
    // Runtime barrier: ldc2 folds sqrt(-1.0) and 0.0/0.0 during CTFE, and its
    // folded NaN does not carry the same sign bit the sqrtsd instruction leaves
    // behind at run time. atoi() is opaque to the folder, so both this probe and
    // the C++ test (volatile operands) exercise real instructions.
    import core.stdc.stdlib : atoi;
    const double n1 = cast(double)atoi("1");
    const double z0 = cast(double)atoi("0");
    writefln("nan_math %s %s %s", sqrt(-n1), log(-n1), z0/z0);
    writefln("div_zero %s %s", 1.0/z0, -1.0/z0);

    // ------------------------------------------------------------------
    // 6. Associative array semantics (45 declarations across the solver).
    //    Missing-key access text and the .keys/.values rendering decide how
    //    eil::AA has to behave.
    // ------------------------------------------------------------------
    int[string] aa;
    aa["alpha"] = 1;
    aa["beta"] = 2;
    writefln("aa_len %d", aa.length);
    writefln("aa_get %d", aa["alpha"]);
    writefln("aa_has %s %s", ("alpha" in aa) !is null, ("gamma" in aa) !is null);
    // Sorted so the corpus is stable despite druntime's per-process hash seed.
    string[] ks = aa.keys.array;
    ks.sort;
    writefln("aa_keys %s", ks);
    int[] vs = aa.values.array;
    vs.sort;
    writefln("aa_values %s", vs);
    try { auto missing = aa["gamma"]; writeln(missing); }
    catch (Throwable t) writefln("aa_missing %s", t.msg);
    string[int] rev;
    rev[7] = "seven";
    writefln("aa_int_key %s", rev[7]);
    // Nested associative array, as used by bc/ghost_cell_effect/mapped_cell_copy.d:63
    string[string][size_t] nested;
    nested[3]["tag"] = "value";
    writefln("aa_nested %s", nested[3]["tag"]);

    // ------------------------------------------------------------------
    // 7. Extended precision: D's `real` and the Phobos constants built from it.
    //    Even though src/eilmer declares no `real` variables, every expression
    //    that touches std.math.PI is evaluated in `real` before being stored.
    // ------------------------------------------------------------------
    writefln("real_sizes %d %d %d %d", real.sizeof, double.sizeof, real.dig, real.mant_dig);
    // What a ported assignment actually consumes. D evaluates an expression
    // through PI in `real`, then rounds once when storing to double; ldc2 also
    // constant-folds, so both the folded and the runtime form are pinned here.
    // Only the double results are compared: glibc renders the 80-bit %a with
    // x87's explicit integer bit ("0xc.90f...p-2" for the same number Phobos
    // prints as "0x1.921fb...p+1"), which is a formatting difference, not a
    // value difference.
    writefln("ct_pi %a", cast(double)PI);
    writefln("ct_two_pi %a", cast(double)(2.0 * PI));
    writefln("ct_quarter_pi %a", cast(double)(PI / 4.0));
    writefln("ct_third %a", cast(double)(1.0L / 3.0L));
    writefln("ct_e %a", cast(double)E);
    import core.stdc.stdlib : atoi;
    const real scale = cast(real)atoi("1");
    const real unit = cast(real)atoi("1");
    const real four = cast(real)atoi("4");
    writefln("rt_pi %a", cast(double)(PI * scale));
    writefln("rt_two_pi %a", cast(double)(2.0 * PI * scale));
    writefln("rt_quarter_pi %a", cast(double)(PI / four * scale));
    writefln("rt_third %a", cast(double)(unit / 3.0L));
    writefln("rt_hypot %a %a", cast(double)hypot(3.0 * unit, 4.0 * unit),
             cast(double)sqrt(2.0 * unit));
    double pa = 1.0, pb = 1.0 + 1e-16;
    writefln("sub %a", pa - pb);

    // ------------------------------------------------------------------
    // 8. Conversion error texts, by failure mode, and bool under each spec.
    // ------------------------------------------------------------------
    try to!int("xx"); catch (Exception e) writefln("to_int_err %s", e.msg);
    try to!int("12x"); catch (Exception e) writefln("to_int_partial %s", e.msg);
    try to!double("not a number"); catch (Exception e) writefln("to_dbl_none %s", e.msg);
    try to!double(" 12x"); catch (Exception e) writefln("to_dbl_junk %s", e.msg);
    try to!double(""); catch (Exception e) writefln("to_dbl_empty %s", e.msg);
    try to!int(""); catch (Exception e) writefln("to_int_empty %s", e.msg);
    try to!int("12x"); catch (Exception e) writefln("to_int_junk %s", e.msg);
    try to!int("-"); catch (Exception e) writefln("to_int_sign_only %s", e.msg);
    writefln("to_int_ok_a %d", to!int("42"));
    writefln("to_int_ok_b %d", to!int("-7"));
    try to!int("12 "); catch (Exception e) writefln("to_int_trailing_space %s", e.msg);
    try to!int(" 42 "); catch (Exception e) writefln("to_int_space_both %s", e.msg);
    foreach (probe; ["12x", "12 ", "12", "+1e5", "nan", "inf", "-inf", "1.5e", "0x10", ".", "-"]) {
        try writefln("dbl_parse %s => %a", probe, to!double(probe));
        catch (Exception e) writefln("dbl_parse %s => ERR %s", probe, e.msg);
    }
    writefln("bool_specs %d %g %x %s %s", true, true, true, true, false);
}
