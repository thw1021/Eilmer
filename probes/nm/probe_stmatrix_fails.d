// probes/nm/probe_stmatrix_fails.d — what D actually raises when
// nm/stmatrix.d's readFromMatrixMarketFile goes wrong.
//
// Registered as backlog T-923. The reader's failure paths had no gate at all: the port
// asserted on them (tests/test_nm_stmatrix.cpp only replays D's `unittest` blocks), so an
// earlier transcription could and did invent its own exception texts there. Measured, the
// D original raises only two kinds of thing, and one of them is not an exception the port
// expected:
//
//   * an unusable path raises std.exception.ErrnoException from `File(fName, "r")`
//     (stmatrix.d:34), whose text embeds the path, the mode and strerror();
//   * every truncated / empty / whitespace-only input raises
//     core.exception.ArrayIndexError "index [0] is out of bounds for array of length 0",
//     because D never tests the stream: `readln()` at end of file yields an empty string,
//     `strip()` keeps it empty, and the next `line[0]` (or `tks[0]` after an empty split)
//     is what fires. There is no "unexpected EOF" exception in this function at all.
//
// Build and capture (reproducible; the gdtk tree itself is read-only):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip \
//     probes/nm/probe_stmatrix_fails.d -of=/tmp/probe_stmf -od/tmp/probe_obj_stmf
//   /tmp/probe_stmf > baselines/e587b235/probes/nm_stmatrix_fails.txt
// No -O flag: every baseline in this repository is an ldc2 -O0 capture.
//
// Paths are fixed under /tmp so the corpus is byte-stable across runs; the missing-file row
// embeds the path D echoes back, which is why the file name is not derived from anything
// that varies.

import std.file;
import std.stdio;
import nm.stmatrix;

void attempt(const string tag, const string path)
{
    try {
        auto m = readFromMatrixMarketFile!double(path);
        writefln("%s|ok|rows=%d|cols=%d|declared=%d|stored=%d", tag,
                 m.n_rows, m.n_cols, m.n_nzeros, m.val.length);
    } catch (Throwable t) {
        writefln("%s|class=%s|msg=[%s]", tag, t.classinfo.name, t.msg);
    }
}

void main()
{
    const string dir = "/tmp/probe_stmatrix_fails";
    dir.mkdirForProbe();

    attempt("missing_file", dir ~ "/does_not_exist.mtx");

    // Header declares 2 entries, file holds 1: EOF inside the entry loop.
    std.file.write(dir ~ "/trunc.mtx", "%comment\n3 3 2\n1 1 5.0\n");
    attempt("truncated_entries", dir ~ "/trunc.mtx");

    // Zero bytes: EOF at the very first readln(), so line[0] is what fires.
    std.file.write(dir ~ "/empty.mtx", "");
    attempt("empty_file", dir ~ "/empty.mtx");

    // Comments only: the discard loop runs off the end.
    std.file.write(dir ~ "/onlycomment.mtx", "%c1\n%c2\n");
    attempt("comments_then_eof", dir ~ "/onlycomment.mtx");

    // Header line with too few tokens: fires at tks[2].
    std.file.write(dir ~ "/few.mtx", "3 3\n");
    attempt("few_tokens", dir ~ "/few.mtx");

    // A blank line where the header should be: D strips it to "" and indexes it.
    std.file.write(dir ~ "/blank.mtx", "\n3 3 1\n1 1 5.0\n");
    attempt("blank_first_line", dir ~ "/blank.mtx");

    // And one good file, so the corpus proves the reader still works and these rows are
    // about the failure paths rather than about the reader being broken.
    std.file.write(dir ~ "/good.mtx", "%comment\n3 3 2\n1 1 5.0\n2 3 7.5\n");
    attempt("valid_file", dir ~ "/good.mtx");
}

// mkdir, ignoring "already exists", written without std.exception to keep the probe's
// failure surface away from the code under measurement.
void mkdirForProbe(string d)
{
    try { mkdir(d); } catch (Throwable t) { }
}
