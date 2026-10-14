// probes/substrate/probe_conv_int_typename.d — which type name `to!T` prints when a
// string→integer conversion fails.
//
// Registered while closing T-011 G5. The usgrid corpus rows 1170-1172 came from
// `to!size_t(getHeaderContent("NELEM"))` on an empty string, and Phobos printed
// "converting from type string to type ulong" while the port printed "int". The port had
// routed `to!size_t` through a helper named for `to!int`, so the printed *target type name*
// is a per-T fact that must be measured, not guessed: `size_t` is an alias, and whether
// Phobos prints the alias or the resolved type decides what every integer conversion site
// in the port has to emit.
//
// This probe records, for each integer target type and each failure family, the actual
// `.msg` string ldc2 1.40.0 produced, or the converted value where it did not fail.
//
// Build and capture (reproducible; the gdtk tree itself is read-only):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -O0 -of=/tmp/probe_conv_int_typename probes/substrate/probe_conv_int_typename.d
//   /tmp/probe_conv_int_typename > baselines/e587b235/probes/substrate_conv_int_typename.txt
module probe.conv_int_typename;

import std.conv;
import std.stdio;

/// Emit one row: label, then the .msg of the exception `to!T(text)` raises, or "no-throw"
/// plus the converted value if it succeeded.
string describe(T)(string label, string text) {
    try {
        T v = to!T(text);
        return label ~ "|src=[" ~ text ~ "]|no-throw|value=" ~ to!string(v);
    } catch (Exception e) {
        return label ~ "|src=[" ~ text ~ "]|class=" ~ e.classinfo.name ~
               "|msg=[" ~ e.msg ~ "]";
    } catch (Error e) {
        return label ~ "|src=[" ~ text ~ "]|class=" ~ e.classinfo.name ~
               "|msg=[" ~ e.msg ~ "]";
    }
}

/// Run the same input list across a target type.
void sweep(T)(string typeName, string[] labels, string[] texts) {
    foreach (i, t; texts) {
        writeln(describe!T(typeName ~ "." ~ labels[i], t));
    }
}

void main() {
    // Inputs chosen to hit each failure family: empty, sign-only, trailing junk,
    // leading/trailing space, digit separator, negative into unsigned, exponent.
    enum labels = ["empty", "plus", "minus", "junk", "leadsp", "trailsp",
                   "sep", "neg", "exp"];
    enum texts = ["", "+", "-", "12x", " 12", "12 ", "1_2", "-1", "1e2"];
    string[] t = texts.dup;
    string[] l = labels.dup;

    sweep!int("int", l, t);
    writeln();
    sweep!uint("uint", l, t);
    writeln();
    sweep!long("long", l, t);
    writeln();
    sweep!ulong("ulong", l, t);
    writeln();
    // size_t is the alias the ported code actually writes
    sweep!size_t("size_t", l, t);
    writeln();
    sweep!ptrdiff_t("ptrdiff_t", l, t);
    writeln();
    sweep!short("short", l, t);
    writeln();
    sweep!byte("byte", l, t);
    writeln();
    sweep!ubyte("ubyte", l, t);
    writeln();

    // Range boundaries: exactly representable, then one past.
    writeln(describe!int("int.max", "2147483647"));
    writeln(describe!int("int.max+1", "2147483648"));
    writeln(describe!int("int.min", "-2147483648"));
    writeln(describe!int("int.min-1", "-2147483649"));
    writeln(describe!uint("uint.max", "4294967295"));
    writeln(describe!uint("uint.max+1", "4294967296"));
    writeln(describe!long("long.max", "9223372036854775807"));
    writeln(describe!ulong("ulong.max", "18446744073709551615"));
    writeln(describe!ulong("ulong.max+1", "18446744073709551616"));
    writeln(describe!size_t("size_t.huge", "99999999999999999999999999"));
    writeln(describe!short("short.max+1", "32768"));
    writeln(describe!byte("byte.max+1", "128"));
    writeln(describe!size_t("size_t.negzero", "-0"));
    writeln(describe!size_t("size_t.plus7", "+7"));
    writeln(describe!size_t("size_t.NELEM_empty", ""));
}
