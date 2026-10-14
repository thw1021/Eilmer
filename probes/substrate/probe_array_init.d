// probes/substrate/probe_array_init.d — what D fills a grown/newly-allocated array with.
//
// Registered as backlog T-929. The port's eil::Array::set_length fills a grown tail with
// T() (zero for double), and its Array(size_t n) constructor delegates to set_length, on
// the strength of a comment claiming "D's new T[n] hands back zeroed GC memory". Both
// halves of that claim are measured here rather than trusted, because double.init is a
// quiet NaN, not 0.0, and the sgrid corpus (rows 757-760, `sg.rgrid.identity.s`) already
// shows literal `nan` in slots the reference never wrote.
//
// Each row prints the IEEE-754 bit pattern (not %a text) so a NaN payload, a -0.0 and a
// +0.0 are distinguishable, plus isnan/isinf flags. Nothing is inferred from the spec.
//
// Build and capture (reproducible; the gdtk tree itself is read-only):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -i probes/substrate/probe_array_init.d -of=/tmp/probe_ai -od/tmp/probe_obj_ai
//   /tmp/probe_ai > baselines/e587b235/probes/substrate_array_init.txt
// No -O flag: every baseline in this repository is an ldc2 -O0 capture.

import std.stdio;
import std.format;

string drow(const string tag, double x)
{
    double y = x;                 // lvalue copy so the reinterpret is well-defined
    ulong u = *cast(ulong*)&y;
    bool sign = (u >> 63) != 0;
    return format("%s|bits=%016x|isnan=%s|isinf=%s|iszero=%s|signbit=%s",
                  tag, u, x != x, x == double.infinity || x == -double.infinity,
                  x == 0.0, sign ? "1" : "0");
}

string dbits(double x)
{
    double y = x;
    ulong u = *cast(ulong*)&y;
    return format("%016x", u);
}

// A struct whose only field is an uninitialized double: D's S.init propagates double.init
// (NaN) field by field, which is why `Vector3 v;` prints nan,nan,nan in the port too.
struct S
{
    double v;
    int i;
}

class C
{
    int q;
}

void main()
{
    // 1. `new double[n]` — does the GC block come back zeroed, or double.init (NaN)?
    double[] nd = new double[4];
    foreach (i, x; nd) writefln("new_double[%d]|%s", i, drow(format("v=%d", i), x));

    // 2. `double[] b; b.length = n;` — the exact path set_length models.
    double[] bl;
    bl.length = 4;
    foreach (i, x; bl) writefln("len_double[%d]|%s", i, drow(format("v=%d", i), x));

    // 3. Growing an existing array: only the NEW tail is re-initialized; kept elements stay.
    double[] gr = [1.5];
    gr.length = 4;
    foreach (i, x; gr) writefln("grow_double[%d]|%s", i, drow(format("v=%d", i), x));

    // 4. int: int.init is 0, so zero-fill and init-fill are indistinguishable — confirm.
    int[] ni = new int[3];
    foreach (i, x; ni) writefln("new_int[%d]|v=%d", i, x);
    int[] il;
    il.length = 3;
    foreach (i, x; il) writefln("len_int[%d]|v=%d", i, x);

    // 5. bool: bool.init is false.
    bool[] nb = new bool[2];
    foreach (i, x; nb) writefln("new_bool[%d]|v=%s", i, x);

    // 6. struct with a double field: S.init sets v to NaN, i to 0.
    S[] ns = new S[2];
    foreach (i, ref s; ns)
        writefln("new_struct[%d]|v.bits=%s|i=%d", i, dbits(s.v), s.i);
    S[] sl;
    sl.length = 2;
    foreach (i, ref s; sl)
        writefln("len_struct[%d]|v.bits=%s|i=%d", i, dbits(s.v), s.i);

    // 7. class array: C.init is null for each element.
    C[] nc = new C[2];
    foreach (i, c; nc) writefln("new_class[%d]|isnull=%s", i, c is null ? "1" : "0");

    // 8. Shrink then regrow: D re-initializes the slots that come back (they are not the
    //    stale values that were there before the shrink).
    double[] sr = [10.0, 20.0, 30.0, 40.0];
    sr.length = 1;
    sr.length = 4;
    foreach (i, x; sr) writefln("regrow_double[%d]|%s", i, drow(format("v=%d", i), x));
}
