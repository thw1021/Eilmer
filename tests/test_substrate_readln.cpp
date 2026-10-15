// tests/test_substrate_readln.cpp
//
// Replay of probes/substrate/probe_readln.d: the row-for-row behaviour of D's buffered text
// reader, against eil::readln in substrate/include/eil/file.hpp. Backlog T-926 item 3.
//
// What this gate pins, and why it is worth a file of its own: the port had two local copies
// of the reader (src/geom/grid/{usgrid,sgrid}.cpp's d_readln) and they had already drifted —
// one raised StdioException on a FAILED read, the other treated every EOF as end of file.
// Both also stripped the terminator that D's readln returns, which is invisible today only
// because every D call site goes on to `.strip()` (backlog T-936 tracks that substitution).
// The rule now lives here, once, with the measurement attached to it:
//   * the raw line KEEPS `\n` and keeps `\r` (rows term.r0, crlf.r0, nul.r1);
//   * a blank line is `\n`, not the empty string (row blank.r1), and the empty string means
//     the stream is exhausted (rows term.r3, empty.r0, past.first/second) — T-923's
//     "EOF returns an empty line and does not throw" rule;
//   * eof() is false right after a terminator-terminated last line and true after a line
//     that ran off the end without one (term.r2 vs noterm.r2);
//   * a failed read throws std.stdio.StdioException(strerror(errno)) from Phobos' own site
//     std/stdio.d:5171 (row directory, reading a directory: fopen succeeds, fgetc EISDIRs),
//     while an unopenable path fails in the constructor as std.exception.ErrnoException at
//     std/stdio.d:624 (row missing) — two classes, two messages, two sites.
//
// The scratch tree is fixed (/tmp/eil_readln_probe) because one row embeds the path; every
// file is rewritten before it is read, so the corpus is reproducible.
//
// Regenerate the baseline:
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -O0 probes/substrate/probe_readln.d -of=/tmp/probe_readln \
//     && /tmp/probe_readln > baselines/e587b235/probes/substrate_readln.txt
//
// Run: ./test_substrate_readln <baseline.txt>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

#include "eil/error.hpp"
#include "eil/file.hpp"
#include "eil/format.hpp"

namespace {

const char* kDir = "/tmp/eil_readln_probe";

std::string esc(const std::string& s) {
    std::string o;
    for (size_t i = 0; i < s.size(); ++i) {
        const char c = s[i];
        if (c == '\n') o += "\\n";
        else if (c == '\r') o += "\\r";
        else if (c == '\0') o += "\\0";
        else o += c;
    }
    return o;
}

void produce(const std::string& path, const std::string& bytes) {
    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) { std::fprintf(stderr, "cannot write %s\n", path.c_str()); std::exit(2); }
    std::fwrite(bytes.data(), 1, bytes.size(), f);
    std::fclose(f);
}

void read_all(const std::string& tag, const std::string& path) {
    std::FILE* f = nullptr;
    try {
        f = std::fopen(path.c_str(), "r");
        if (!f) {
            throw eil::ExceptionBase(
                "Cannot open file `" + path + "' in mode `r' (" +
                    std::strerror(errno) + ")",
                "std/stdio.d", 624);  // D: std.exception.ErrnoException from the File ctor
        }
        for (int i = 0; i < 6; ++i) {
            bool at_eof = false;
            const std::string line = eil::readln(f, at_eof);
            eil::writefln("%s.r%d|len=%d|[%s]|eof=%s", tag, i, static_cast<int>(line.size()),
                          esc(line), at_eof ? "true" : "false");
            if (line.empty() && at_eof) break;
        }
        std::fclose(f);
    } catch (const eil::ExceptionBase& e) {
        if (f) std::fclose(f);
        const bool stdio = dynamic_cast<const eil::StdioException*>(&e) != nullptr;
        eil::writefln("%s|EXC|%s|msg=[%s]", tag,
                      stdio ? "std.stdio.StdioException" : "std.exception.ErrnoException",
                      esc(e.msg()));
        eil::writefln("%s.site|file=[%s]|line=%d", tag, e.file(), static_cast<int>(e.line()));
    }
}


void emit_all() {
    ::mkdir(kDir, 0777);
    ::mkdir(kDir, 0777);
    const std::string d(kDir);
    produce(d + "/term.txt", "alpha\nbeta\ngamma\n");
    produce(d + "/noterm.txt", "alpha\nbeta\ngamma");
    produce(d + "/crlf.txt", "alpha\r\nbeta\r\n");
    produce(d + "/empty.txt", "");
    produce(d + "/blank.txt", "one\n\nthree\n\n");
    {
        const char nul[] = {'a', '\0', 'b', '\n', 'c', '\r', 'd', '\n'};
        std::FILE* f = std::fopen((d + "/nul.txt").c_str(), "wb");
        std::fwrite(nul, 1, sizeof nul, f);
        std::fclose(f);
    }
    read_all("term", d + "/term.txt");
    read_all("noterm", d + "/noterm.txt");
    read_all("crlf", d + "/crlf.txt");
    read_all("empty", d + "/empty.txt");
    read_all("blank", d + "/blank.txt");
    {
        std::FILE* f = std::fopen((d + "/term.txt").c_str(), "r");
        bool eof = false;
        eil::readln(f, eof);
        eil::readln(f, eof);
        eil::readln(f, eof);
        eil::writefln("past.eofflag|[%s]", eof ? "true" : "false");
        bool a_eof = false;
        const std::string a = eil::readln(f, a_eof);
        eil::writefln("past.first|len=%d|[%s]", static_cast<int>(a.size()), esc(a));
        bool b_eof = false;
        const std::string b = eil::readln(f, b_eof);
        eil::writefln("past.second|len=%d|[%s]", static_cast<int>(b.size()), esc(b));
        std::fclose(f);
    }
    read_all("directory", d);
    read_all("missing", d + "/nope.txt");
    read_all("nul", d + "/nul.txt");
    {
        std::FILE* f = std::fopen((d + "/term.txt").c_str(), "r");
        bool eof = false;
        const std::string l = eil::readln(f, eof);  // D's readln('\n'): same default here
        eil::writefln("termarg|len=%d|[%s]", static_cast<int>(l.size()), esc(l));
        std::fclose(f);
    }
    eil::writefln("END|%s", "done");
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }
    const char* capture = "/tmp/eil_readln_capture.txt";
    int saved = ::dup(1);
    std::fflush(stdout);
    if (std::freopen(capture, "w", stdout) == nullptr) {
        std::fprintf(stderr, "cannot open capture file %s\n", capture);
        return 2;
    }
    emit_all();
    std::fflush(stdout);
    ::dup2(saved, 1);
    ::close(saved);

    std::ifstream in(argv[1]);
    if (!in) {
        std::fprintf(stderr, "cannot open baseline %s\n", argv[1]);
        return 2;
    }
    std::vector<std::string> expected;
    for (std::string l; std::getline(in, l);) {
        if (!l.empty() && l[0] != '#') expected.push_back(l);
    }
    std::ifstream got(capture);
    std::vector<std::string> actual;
    for (std::string l; std::getline(got, l);) actual.push_back(l);
    int diffs = 0, shown = 0;
    size_t matched = 0;
    for (size_t i = 0; i < expected.size() || i < actual.size(); ++i) {
        const bool have_e = i < expected.size();
        const bool have_g = i < actual.size();
        const std::string e = have_e ? expected[i] : "<none>";
        const std::string g = have_g ? actual[i] : "<none>";
        if (have_e && have_g && e == g) { ++matched; continue; }
        if (shown < 12) {
            std::printf("line %zu\n  D:   |%s|\n  C++: |%s|\n", i + 1, e.c_str(), g.c_str());
            ++shown;
        }
        ++diffs;
    }
    std::printf("%s: substrate_readln, %zu D rows, %zu C++ rows, %d differ; %zu byte-identical\n",
                diffs == 0 ? "PASS" : "FAIL", expected.size(), actual.size(), diffs, matched);
    return diffs == 0 ? 0 : 1;
}
