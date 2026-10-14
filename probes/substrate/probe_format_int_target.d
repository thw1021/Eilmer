// probes/substrate/probe_format_int_target.d — what `formattedRead("%d", &x)` prints when
// the token is not a valid integer, for each integral type of x.
//
// Raised while closing T-011 G4. sgrid.d reads header values through formattedRead into
// `int` (grid.d:43 `int dimensions`), `size_t` (sgrid.d:910 `size_t ntags`) and others, and
// the port has one fmt_into<T> specialisation per target type. The corpus row
// geom_grid_sgrid.txt:1100 (`sg.frmt.dimsword`, from `dimensions: two` into the `int`
// `dimensions`) proves that std.format's integer reader surfaces a *std.conv* message, so
// the printed target name is per-T here too -- which means fmt_into<size_t> must not borrow
// the int wording, exactly as in the direct `to!size_t` case (probe_conv_int_typename.d).
//
// Whether std.format's own scanner (which handles whitespace and the sign before std.conv
// sees it) changes any of these strings is what this probe measures, per target type.
//
// Measured answer, and it is the same rule as std.conv: the name is the RESOLVED type, so
// "%d" into a size_t says "ulong" and into a long says "long"; unsigned targets reject a
// leading sign ("Unexpected '-'" for "-3"), signed ones consume it; a magnitude too large
// for the target raises std.conv.ConvOverflowException "Overflow in integral conversion".
// Two extra shapes only std.format produces: an input that runs out before the literal
// matches raises std.format.FormatException "parseToFormatSpec: Cannot find character 'n'
// in the input string.", and "%x"-free input "0x10" reads as 0 with the scanner left at
// "x10" -- so a hex token does NOT reach conv's grammar here.
//
// Build and capture (reproducible; the gdtk tree itself is read-only):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -O0 -of=/tmp/probe_format_int_target probes/substrate/probe_format_int_target.d
//   /tmp/probe_format_int_target > baselines/e587b235/probes/substrate_format_int_target.txt
module probe.format_int_target;

import std.conv;
import std.format;
import std.stdio;

/// Read `line` with `fmt` into a fresh T, printing the resulting value or the .msg.
string attempt(T)(string label, string line, string fmt) {
    T v = T.init;
    try {
        formattedRead(line, fmt, &v);
        return label ~ "|src=[" ~ line ~ "]|no-throw|value=" ~ to!string(v);
    } catch (Exception e) {
        return label ~ "|src=[" ~ line ~ "]|class=" ~ e.classinfo.name ~ "|msg=[" ~ e.msg ~ "]";
    } catch (Error e) {
        return label ~ "|src=[" ~ line ~ "]|class=" ~ e.classinfo.name ~ "|msg=[" ~ e.msg ~ "]";
    }
}

string toStr(T)(T v) { return format("%s", v); }

void main() {
    // "%d" with a non-numeric word after the literal, matching the sgrid header grammar.
    writeln(attempt!int("int.word", "dimensions: two", "dimensions: %d"));
    writeln(attempt!size_t("size_t.word", "ntags: two", "ntags: %d"));
    writeln(attempt!long("long.word", "n: two", "n: %d"));
    writeln(attempt!uint("uint.word", "n: two", "n: %d"));
    writeln();

    // A negative token into each target: does std.format's scanner or std.conv object?
    writeln(attempt!int("int.neg", "n: -3", "n: %d"));
    writeln(attempt!long("long.neg", "n: -3", "n: %d"));
    writeln(attempt!size_t("size_t.neg", "n: -3", "n: %d"));
    writeln(attempt!uint("uint.neg", "n: -3", "n: %d"));
    writeln(attempt!short("short.neg", "n: -3", "n: %d"));
    writeln(attempt!ubyte("ubyte.neg", "n: -3", "n: %d"));
    writeln();

    // Out-of-range magnitudes.
    writeln(attempt!int("int.overflow", "n: 99999999999999999999", "n: %d"));
    writeln(attempt!size_t("size_t.overflow", "n: 99999999999999999999", "n: %d"));
    writeln(attempt!short("short.overflow", "n: 99999", "n: %d"));
    writeln();

    // Empty and whitespace-only input: std.format leaves the destination untouched.
    writeln(attempt!int("int.empty-input", "", "n: %d"));
    writeln(attempt!size_t("size_t.empty-input", "", "n: %d"));
    writeln(attempt!int("int.exhausted", "n: 5", "n: %d n2: %d"));
    writeln(attempt!size_t("size_t.plus", "n: +7", "n: %d"));
    writeln(attempt!int("int.plus", "n: +7", "n: %d"));
    writeln(attempt!size_t("size_t.hex", "n: 0x10", "n: %d"));
    writeln(attempt!int("int.hex", "n: 0x10", "n: %d"));
    writeln();

    // "%g" into a real, for contrast with the integer rows above.
    writeln(attempt!double("double.word", "v = zero", "v = %g"));
    writeln(attempt!double("double.empty", "", "v = %g"));
}
