// probes/substrate/probe_tostring_dispatch.d — what `to!string(x)` does with a user type
// that has (or has not) a `toString()` member, and what the resulting text is.
//
// Backlog T-926 item 1. The substrate's `eil::to_string` formats arithmetic types and
// `static_assert`s on anything else (substrate/include/eil/format.hpp:139), so every ported
// class so far has had to hand-write its own text at each call site. That is about to stop
// being tenable: `gas/` (Wave 3), `fluidblock` (Wave 4) and the solver log lines (Wave 5)
// put objects into message text constantly, and in D those sites are just `to!string(x)` or
// `"%s"`. Before substrate grows a dispatch rule, the rule has to be measured, because the
// obvious answers are wrong in non-obvious ways:
//
//   * D prefers the member `toString()` over the generic formatter -- but does it for a
//     *struct* whose toString returns string? for a *class*? for a struct WITHOUT toString
//     (what does the fallback look like: field dump, or the type name)?
//   * Does a `toString()` that takes a format string (`toString(string fmt, args...)`, which
//     several gdtk classes define) get used by `to!string` with an empty format?
//   * Containers: does `to!string(int[])` call toString on the array (there is none) or use
//     Phobos' `[a, b, c]` dump -- and inside such a dump, is an element with toString routed
//     through its toString or field-dumped again?
//   * Enums, string-of-enum-name, nested structs, `null` class reference, and a struct with
//     an *ambiguous* pair (both `toString()` and `toString(string)`).
//
// Each row prints: the expression, the static type name, and either the produced text or the
// exception class+msg. Nothing here is inferred from signatures.
//
// Build and capture (reproducible; needs only Phobos, so no gdtk shadow tree):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -O0 -of=/tmp/probe_tostring_dispatch probes/substrate/probe_tostring_dispatch.d
//   /tmp/probe_tostring_dispatch > baselines/e587b235/probes/substrate_tostring_dispatch.txt

import std.array;
import std.conv;
import std.format;
import std.range;
import std.stdio;

struct NoToString {
    int a;
    double b;
}

struct Simple {
    int v;
    string toString() { return format("Simple(%d)", v); }
}

struct FormatToString {
    int v;
    // gdtk has this shape (e.g. a toString that forwards to formattedWrite)
    string toString(string fmt, Args...)() const { return format(fmt, v); }
}

struct Both {
    int v;
    string toString() { return "plain"; }
    string toString(string fmt) { return format("fmt:%s", v); }
}

struct Nested {
    Simple s;
    int k;
}

class Cls {
    int v;
    // Measured while writing this probe: a class `toString()` without `override` does not
    // even compile ("cannot implicitly override base class method object.Object.toString"),
    // so every class in gdtk that customises its text says `override string toString()`.
    override string toString() { return format("Cls<%d>", v); }
}

class Bare {
    int v;
}

enum Colour { red, green, blue }

/// Print one measured row: label, expression text, and the result or the failure.
void row(T)(string label, string expr, T value) {
    try {
        string s = to!string(value);
        string tf = T.stringof;
        writeln(label, "|", expr, "|type=", tf, "|text=[", s, "]");
    } catch (Exception e) {
        string tf = T.stringof;
        writeln(label, "|", expr, "|type=", tf, "|class=", e.classinfo.name,
                "|msg=[", e.msg, "]");
    } catch (Error e) {
        string tf = T.stringof;
        writeln(label, "|", expr, "|type=", tf, "|class=", e.classinfo.name,
                "|msg=[", e.msg, "]");
    }
}

void main() {
    Simple s = Simple(7);
    NoToString n = NoToString(1, 2.5);
    FormatToString f = FormatToString(3);
    Both b = Both(9);
    Nested ne = Nested(Simple(4), 5);
    Cls c = new Cls(); c.v = 6;
    Bare bare = new Bare(); bare.v = 8;

    row("struct.with.toString", "to!string(Simple(7))", s);
    row("struct.no.toString", "to!string(NoToString(1,2.5))", n);
    row("struct.fmt.toString", "to!string(FormatToString(3))", f);
    row("struct.both", "to!string(Both(9))", b);
    row("struct.nested", "to!string(Nested(Simple(4),5))", ne);
    row("class.with.toString", "to!string(c)", c);
    row("class.no.toString", "to!string(bare)", bare);
    row("class.null", "to!string(nullCls)", cast(Cls) null);

    // Integers and floats for contrast with the "%s" default
    row("int", "to!string(42)", 42);
    row("enum", "to!string(Colour.green)", Colour.green);
    row("bool", "to!string(true)", true);
    row("dstring", `to!string("x")`, "x");

    // Containers: the element text inside a dump is where the dispatch question bites.
    int[] ai = [1, 2, 3];
    Simple[] sa = [Simple(1), Simple(2)];
    row("array.int", "to!string(ai)", ai);
    row("array.struct", "to!string(sa)", sa);
    row("aa.int", "to!string(aa)", ["a": 1]);
    row("nested.array.of.array", "to!string(oa)", [ai, ai]);
    NoToString[] an = [NoToString(1, 2.0)];
    row("array.no.toString", "to!string(an)", an);

    // Same values through "%s", which is what writefln/format use internally.
    writeln("format.simple|`\"%s\".format(s)`|text=[", format("%s", s), "]");
    writeln("format.nested|`\"%s\".format(ne)`|text=[", format("%s", ne), "]");
    writeln("format.array.struct|`\"%s\".format(sa)`|text=[", format("%s", sa), "]");
    writeln("format.class.null|`\"%s\".format(nullCls)`|text=[", format("%s", cast(Cls) null), "]");
    writeln("format.enum|`\"%s\".format(Colour.blue)`|text=[", format("%s", Colour.blue), "]");

    // Does a toString receive a user format string?
    try {
        writeln("explicit.fmt|text=[", format("%d", s), "]");
    } catch (Exception e) {
        writeln("explicit.fmt|class=", e.classinfo.name, "|msg=[", e.msg, "]");
    }

    // toString called directly on the two-shape struct, to show which overload wins.
    writeln("direct.both|`b.toString()`|text=[", b.toString(), "]");
    writeln("direct.both.fmt|`b.toString(\"%d\")`|text=[", b.toString("%d"), "]");
}
