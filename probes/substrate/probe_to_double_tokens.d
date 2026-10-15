// probes/substrate/probe_to_double_tokens.d — D's `to!double' bits for every decimal token
// in a text file, one per line. Backlog T-930's exposure instrument (route 1: keep strtod).
//
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -O0 probes/substrate/probe_to_double_tokens.d -of=/tmp/tdr_tokens
//   tr -s ' \t' '\n' < <(grep -v '^#' \
//       /home/tang/packages/gdtk/examples/eilmer/3D/gridpro-import/blk.tmp) \
//     | grep -E '^[+-]?([0-9]+\.?[0-9]*|\.[0-9]+)([eE][+-]?[0-9]+)?$' > /tmp/tokens.txt
//   /tmp/tdr_tokens /tmp/tokens.txt > /tmp/d_bits.txt
//   g++ -O1 -std=c++17 -o /tmp/expo tools/to_double_exposure.cpp && /tmp/expo /tmp/d_bits.txt
//
// The gdtk tree is only ever READ here; the token list lives in /tmp.
import std.conv, std.file, std.format, std.stdio, std.string;

void main(string[] args)
{
    foreach (s; splitLines(cast(string)read(args[1])))
    {
        if (s.length == 0) continue;
        try
        {
            double v = to!double(s);
            writefln("d src=%s bits=%016x", s, *cast(ulong*)&v);
        }
        catch (Exception e)
        {
            // D throws instead of saturating: text beyond double.max is a ConvException.
            writefln("d src=%s throws=%s", s, e.classinfo.name);
        }
    }
}
