// probes/substrate/probe_readln.d — what std.stdio.File.readln() does at the boundaries,
// measured against ldc2 1.40.0. Backlog T-926 item 3 / T-923.
//
// Why this probe exists: the port had TWO local copies of `d_readln`
// (src/geom/grid/usgrid.cpp and src/geom/grid/sgrid.cpp) and they had already drifted —
// one raises D's StdioException when the READ FAILS, the other treats every EOF as end of
// file. That distinction is observable: glibc's fopen succeeds on a directory and the first
// fgetc fails with EISDIR, and the reference reports `StdioException("Is a directory")`
// there (witnessed by corpus row 1267 of geom_grid_usgrid.txt). Moving the reader into the
// substrate needs the whole rule measured first, not just the one row: what readln returns
// past EOF, whether it throws, what a file whose last line has no terminator does, what a
// CRLF line keeps, and what an empty file does.
//
// Build and capture (reproducible; writes its own scratch files under /tmp, so the gdtk
// tree is not touched):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -O0 probes/substrate/probe_readln.d -of=/tmp/probe_readln -od/tmp/probe_obj_readln
//   /tmp/probe_readln > baselines/e587b235/probes/substrate_readln.txt
// Replay: ctest -R substrate_readln (tests/test_substrate_readln.cpp).
//
// Rows print the returned line escaped (\n, \r and "" are all observable here) plus the
// eof() flag, so "empty string because EOF" and "empty string because the line was empty"
// cannot be conflated.
import std.algorithm : canFind;
import std.file;
import std.format;
import std.stdio;
import std.string;

string esc(string s)
{
    pragma(once);
    string o;
    foreach (dchar c; s)
    {
        if (c == '\n') o ~= "\\n";
        else if (c == '\r') o ~= "\\r";
        else if (c == '\0') o ~= "\\0";
        else o ~= cast(char)c;
    }
    return o;
}

void readAll(string tag, string path)
{
    pragma(once);
    try
    {
        File f = File(path, "r");
        for (int i; i < 6; ++i)
        {
            string line = f.readln();
            writefln("%s.r%d|len=%d|[%s]|eof=%s", tag, i, line.length, esc(line),
                     f.eof() ? "true" : "false");
            if (line.length == 0 && f.eof()) break;
        }
        f.close();
    }
    catch (Exception e)
    {
        writefln("%s|EXC|%s|msg=[%s]", tag, e.classinfo.name, esc(e.msg));
        writefln("%s.site|file=[%s]|line=%d", tag, e.file, e.line);
    }
}

void main()
{
    // Fixed scratch path: the rows must not carry a directory name, or the corpus would
    // differ between runs. Every file below is written fresh before it is read.
    string dir = "/tmp/eil_readln_probe";
    if (!dir.exists) dir.mkdir;

    // (a) three lines, each terminated
    std.file.write(dir ~ "/term.txt", "alpha\nbeta\ngamma\n");
    readAll("term", dir ~ "/term.txt");

    // (b) three lines, the LAST one unterminated
    std.file.write(dir ~ "/noterm.txt", "alpha\nbeta\ngamma");
    readAll("noterm", dir ~ "/noterm.txt");

    // (c) CRLF file: does readln keep the \r?
    std.file.write(dir ~ "/crlf.txt", "alpha\r\nbeta\r\n");
    readAll("crlf", dir ~ "/crlf.txt");

    // (d) empty file
    std.file.write(dir ~ "/empty.txt", "");
    readAll("empty", dir ~ "/empty.txt");

    // (e) blank line in the middle, and a trailing blank line
    std.file.write(dir ~ "/blank.txt", "one\n\nthree\n\n");
    readAll("blank", dir ~ "/blank.txt");

    // (f) reading past EOF a second and third time (the "returns empty forever" question)
    {
        File f = File(dir ~ "/term.txt", "r");
        f.readln(); f.readln(); f.readln();
        writefln("past.eofflag|[%s]", f.eof() ? "true" : "false");
        string a = f.readln();
        writefln("past.first|len=%d|[%s]", a.length, esc(a));
        string b = f.readln();
        writefln("past.second|len=%d|[%s]", b.length, esc(b));
        f.close();
    }

    // (g) a DIRECTORY: fopen succeeds on Linux and the first read fails. This is the row
    //     that separates "EOF" from "failed read".
    readAll("directory", dir);

    // (h) a file that cannot be opened at all (the constructor's failure, for contrast)
    readAll("missing", dir ~ "/nope.txt");

    // (i) binary-ish content: a NUL byte and a lone \r line
    std.file.write(dir ~ "/nul.txt", "a\0b\nc\rd\n");
    readAll("nul", dir ~ "/nul.txt");

    // (j) readln with an explicit terminator (the shims never use it, but the default
    //     argument is what the empty-line-at-EOF behaviour hangs off)
    {
        File f = File(dir ~ "/term.txt", "r");
        string l = f.readln('\n');
        writefln("termarg|len=%d|[%s]", l.length, esc(l));
        f.close();
    }
    writefln("END|done");
}
