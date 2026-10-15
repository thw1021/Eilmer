#pragma once
// eil/file.hpp — D's buffered text reader, one copy of the rule.
//
// What D actually does, measured (probes/substrate/probe_readln.d ->
// baselines/e587b235/probes/substrate_readln.txt, 30 rows, two runs byte-identical):
//
//   * `f.readln()` RETURNS THE TERMINATOR. The row is `alpha\n`, not `alpha`; a blank line
//     is `\n`, not the empty string; a CRLF file yields `alpha\r\n` (the \r is kept, it is
//     not a line-ending mode). Every D call site in this codebase then calls `.strip()`,
//     which is why the distinction is invisible at those sites — and why a port that strips
//     inside the reader has to keep stripping consistently forever, at readers that do not
//     strip in D. That substitution is recorded as backlog T-936; this function does NOT do
//     it, so a new call site can write what D writes.
//   * At true end of file it returns the EMPTY STRING and does not throw, and keeps doing so
//     (rows term.r3, noterm.r3, empty.r0, past.first/past.second). `eof()` is false right
//     after the last terminator-terminated line and true after the read that found nothing
//     (row past.eofflag), and it is already true on the line returned without a terminator
//     (row noterm.r2). This is the same EOF-is-not-an-error rule T-923 pinned for the
//     .mtx reader, and a caller that treats "empty line" as EOF is wrong twice over:
//     a blank line is `\n`, and the empty string means the stream is exhausted.
//   * A FAILED READ is not EOF either: it throws std.stdio.StdioException whose message is
//     strerror(errno) — row directory reports "Is a directory", because glibc's fopen
//     succeeds on a directory and the first read fails with EISDIR. Its .file/.line are the
//     RUNTIME's (`std/stdio.d` 5171), not the caller's — same rule as T-922's
//     OutOfMemoryError@rt/lifetime.d(1454), so they are attached verbatim rather than
//     substituted with this TU's location.
//   * An unopenable path fails in the CONSTRUCTOR with std.exception.ErrnoException
//     (row missing: "Cannot open file `...' in mode `r' (No such file or directory)"),
//     which is a different class, message and site from the read failure.
//   * Binary content passes through: row nul.r0 is `a\0b\n` (a NUL inside the line is kept).
//
// readln_line() below is the strip-normalised convenience the existing readers use. It is
// named separately so a reviewer can see which sites are D's shape and which are the port's
// (T-936 tracks collapsing the latter once the file-format corpora cover them).
//
// The class-name prefix of `toString()` cannot match D's (`std.stdio.StdioException@...`),
// which is the pre-existing substrate limit backlog T-903 records; only .msg is compared by
// the gates that reach this path.

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>

#include "eil/error.hpp"

namespace eil {

// std.stdio.StdioException. D raises it from std/stdio.d:5171 with strerror(errno) as the
// message; the location is the library's, so it is fixed here rather than __FILE__/__LINE__.
class StdioException : public ExceptionBase {
  public:
    explicit StdioException(const std::string& message)
        : ExceptionBase(message, "std/stdio.d", 5171) {}
};

// D's `f.readln()` with its default terminator: the line INCLUDING the `\n`, the empty
// string at end of file, StdioException when the read itself fails.
// `at_eof` reproduces D's `f.eof()` after the call.
inline std::string readln(std::FILE* f, bool& at_eof) {
    std::string out;
    errno = 0;
    int c = std::fgetc(f);
    if (c == EOF) {
        if (std::ferror(f)) throw StdioException(std::strerror(errno));
        at_eof = true;
        return out;  // "" and no throw: T-923's rule, rows term.r3 / empty.r0
    }
    for (;;) {
        out += static_cast<char>(c);
        if (c == '\n') break;
        errno = 0;
        c = std::fgetc(f);
        if (c == EOF) {
            if (std::ferror(f)) throw StdioException(std::strerror(errno));
            break;  // last line, no terminator: row noterm.r2 keeps `gamma` and eof is true
        }
    }
    // glibc only raises the EOF indicator once a read has run past the end, which is exactly
    // when D's eof() is true (false after `gamma\n`, true after `gamma`) — rows term.r2 and
    // noterm.r2.
    at_eof = (std::feof(f) != 0);
    return out;
}

// The strip-normalised line: what every current call site in this tree ends up using.
// Deliberately separate from readln() so the substitution T-936 covers is visible per site.
inline std::string strip(const std::string& s) {
    size_t b = 0, e = s.size();
    while (b < e && (s[b] == ' ' || s[b] == '\t' || s[b] == '\n' || s[b] == '\r' ||
                     s[b] == '\v' || s[b] == '\f')) { ++b; }
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\n' ||
                     s[e - 1] == '\r' || s[e - 1] == '\v' || s[e - 1] == '\f')) { --e; }
    return s.substr(b, e - b);
}

inline std::string readln_line(std::FILE* f, bool& at_eof) {
    return strip(readln(f, at_eof));
}

} // namespace eil
