// probes/substrate/probe_conv_failtext.d — Phobos' to!double, one row per operand.
//
// WHY A HEX-ENCODED OPERAND FILE INSTEAD OF LITERALS IN THE SOURCE. Several operands
// this gate has to cover contain a newline or a tab (D's own message for
// to!double("1\n") is part of the contract, and so is the fact that '\n' reaches the
// message escaped as backslash-n while '\t' arrives raw). Printing those operands
// straight into the corpus makes it line-oriented by accident: one row becomes two or
// three lines, and a line-by-line replay can no longer tell which operand produced
// which message. Both sides therefore read tests/test_data/conv_inputs.hex, one
// hex-encoded operand per line, and print "<operand-hex> ok|exc|err <result-hex>",
// which is one line per operand by construction and byte-comparable.
//
// WHAT IS PINNED. to!double's error taxonomy and its accepted grammar. 1,121 of the
// 1,393 `!` instantiations in src/eilmer are to!X, the solver prints
// ConversionException text straight out of Lua job-script parsing, and
// harness/normalize_log.py byte-compares it. Two stage agents measured that the
// substrate's table was wrong for to!double("#c"); re-deriving the rule from this
// corpus is what produced the numbered findings in substrate/include/eil/conv.hpp.
//
// Build and capture (run from the repository root; the operand path is relative):
//   cd /home/tang/packages/Eilmer
//   /home/tang/packages/dlang/bin/ldc2 probes/substrate/probe_conv_failtext.d \
//     -of=/tmp/probe_cft && /tmp/probe_cft tests/test_data/conv_inputs.hex \
//     > baselines/e587b235/probes/substrate_conv_failtext.txt
import std.conv;
import std.array;
import std.file;
import std.format;
import std.stdio;

string hexify(const(ubyte)[] b)
{
    string res;
    foreach (x; b) res ~= format("%02x", x);
    return res;
}

// Hand-parsed on purpose: decoding the operands with the same library whose
// conversion is under measurement would hide exactly the bugs this probe looks for.
// (The first version of this used `foreach (j, ref v; [hi, lo])`, which mutates a
// copy of the array literal rather than hi and lo, and silently turned every
// operand into NUL bytes. Written as two explicit calls instead.)
int nib(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return 0;
}

ubyte[] unhexify(const char[] h)
{
    ubyte[] res;
    for (size_t i = 0; i + 1 < h.length; i += 2)
        res ~= cast(ubyte)(nib(h[i]) * 16 + nib(h[i + 1]));
    return res;
}

void one(const char[] hexin)
{
    const string operand = cast(string) unhexify(hexin);
    try {
        const double d = to!double(operand);
        writefln("%s ok %s", hexin, hexify(cast(const(ubyte)[]) format("%.17g", d)));
    } catch (Exception e) {
        writefln("%s exc %s", hexin, hexify(cast(const(ubyte)[]) e.msg));
    } catch (Error e) {
        writefln("%s err %s", hexin, hexify(cast(const(ubyte)[]) e.msg));
    }
}

void main(string[] args)
{
    const string path = args.length > 1 ? args[1] : "tests/test_data/conv_inputs.hex";
    // The empty operand is a legal and measured case, and it encodes as an empty
    // line, so only the file's trailing newline is dropped -- no other skip.
    string[] rows = (cast(string) read(path)).split("\n");
    if (rows.length && rows[$ - 1].length == 0) rows = rows[0 .. $ - 1];
    foreach (line; rows) one(line);
}
