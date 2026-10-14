// probes/substrate/probe_throw_identity.d — which D exception class carries what.
//
// Registered as backlog T-922. The port has 43 sites that construct `eil::ErrorBase` with
// `__FILE__, __LINE__`, on the assumption that D's throw recorded the throw site. That
// assumption is true for some of those forms and false for others, and the difference is
// user-visible: it is the text that ends up in solver logs and in `toString()`, which
// requirement 4 says must stay identical. The gates never caught it because they compare
// `.msg`, and `.msg` is the same either way.
//
// Measured here, per throw form the port actually reproduces: the runtime class name,
// `.file`, `.line`, and the first line of `.toString()`. Nothing is inferred from
// signatures -- each row is what ldc2 1.40.0 printed at run time.
//
// Build and capture (reproducible; the gdtk tree itself is read-only):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip \
//     probes/substrate/probe_throw_identity.d -of=/tmp/probe_ti -od/tmp/probe_obj_ti
//   /tmp/probe_ti > baselines/e587b235/probes/substrate_throw_identity.txt
// No -O flag: every baseline in this repository is an ldc2 -O0 capture.
//
// toString() is printed as ONLY its first line: D appends "\n----------------\n" and a
// full backtrace of "??:? [0x...]" frames whose addresses change every run (measured).
// A port cannot reproduce a backtrace it does not have, so the backtrace is out of scope
// by construction and the first line is the portable contract. That is backlog T-903.

import std.algorithm;
import std.conv;
import std.exception;
import std.range;
import std.algorithm.searching;
import std.stdio;
import core.exception;

import geom.geometry_exception;

void report(const string tag, in string expected, Throwable t)
{
    // First line of toString only, per the header note.
    string full = t.toString();
    string first = full.length ? full[0 .. min(full.length, indexOfOrEnd(full))] : "";
    writefln("%s|class=%s|msg=[%s]|file=[%s]|line=%d|toString1=[%s]|want=%s",
             tag, t.classinfo.name, t.msg, t.file, t.line, first, expected);
}

size_t indexOfOrEnd(const string s)
{
    foreach (i, c; s) if (c == '\n') return i;
    return s.length;
}

enum Kind { a, b, c }

void main()
{
    // 1. The two bare D base classes. These two rows are the whole reason T-922 exists:
    //    Exception defaults its file/line to the call site, Error defaults to nothing.
    try { throw new Exception("m-exception"); }
    catch (Exception e) report("bare.Exception", "file/line of THIS call site", e);

    try { throw new Error("m-error"); }
    catch (Error e) report("bare.Error", "empty file, line 0", e);

    try { throw new Throwable("m-throwable"); }
    catch (Throwable t) report("bare.Throwable", "empty file, line 0", t);

    // 2. The solver's own subclasses.
    try { throw new GeometryException("m-geom"); }
    catch (Exception e) report("subclass.GeometryException", "file/line of THIS call site", e);

    // 3. Compiler-generated throws.
    int[] a = [1, 2, 3];
    try { auto v = a[9]; }
    catch (Throwable t) report("gen.array_index", "ArrayIndexError, caller file/line", t);

    Kind k = cast(Kind)7;   // outside the declared set
    try {
        final switch (k) {  // exhaustive over a, b, c -- so this raises at run time
        case Kind.a: break;
        case Kind.b: break;
        case Kind.c: break;
        }
    }
    catch (Throwable t) report("gen.final_switch", "SwitchError, and does it carry a site?", t);

    long[] neg;
    try { neg.length = -1; }
    catch (Throwable t) report("gen.negative_length", "the Memory allocation failed case", t);

    // 4. Phobos-internal throws whose text the port reproduces verbatim. The file/line
    //    here belongs to Phobos' own source, not to the caller -- which is why porting
    //    them with __FILE__ invents a location the reference never prints.
    try { auto d = to!double("#zz"); }
    catch (Throwable t) report("phobos.to_double", "ConvException inside std/conv.d", t);

    // bbla.d:341 `enforce(cond, new Error("invalid subrange"))` -- the object the port
    // builds for those two sites is a bare Error, so no file/line can be attached.
    try { enforce(false, new Error("invalid subrange")); }
    catch (Throwable t) report("phobos.enforce_with_Error", "bare Error: no file/line", t);
    try { enforce(false, "plain enforce text"); }
    catch (Throwable t) report("phobos.enforce_Exception", "Exception: which file/line", t);

    // smla.d:212 `ja.maxElement` over an empty range -- what the port's
    // src/nm/smla.hpp:127 ErrorBase("range is empty") stands for.
    int[] empty;
    try { auto m = empty.maxElement; }
    catch (Throwable t) report("phobos.maxElement_empty", "the range is empty case", t);

    // nelmin.d:96 `smplx.vertices.sort!(less)` -- the port keeps Phobos' partition-guard
    // assert text; the file/line of that assert lives inside std/algorithm/sorting.d.
    int[] bad = [3, 1, 2];
    try { sort!((int x, int y) => true)(bad); }
    catch (Throwable t) report("phobos.sort_bad_cmp", "sort: invalid comparison function", t);

    try { throw new ErrnoException("m-errno", 17); }
    catch (Throwable t) report("phobos.errno", "ErrnoException defaults to null/0", t);

    // 5. assert: both forms, because the port's eil_assert / eil_assert_msg split (T-918)
    //    is keyed on exactly these two .msg values.
    try { assert(false); }
    catch (Throwable t) report("assert.bare", "AssertError, msg == Assertion failure", t);

    try { assert(false, "assert with message"); }
    catch (Throwable t) report("assert.msg", "AssertError, msg is only the message", t);
}
