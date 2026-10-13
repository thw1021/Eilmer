// tests/test_extern_gzip_gzip.cpp — a line-for-line replay of
// probes/extern/gzip/probe_gzip.d against
// baselines/e587b235/probes/extern_gzip_gzip.txt, plus self-checks the corpus diff
// cannot express (the port's own container against D's).
//
// extern/gzip/gzip.d has NO unittest block, so this test pins the module's
// observable behaviour instead, in the order the probe exercises it: the payload
// that comes back from a D-written gzip container, the same payload written in one
// compress() call, an empty payload (D writes a ZERO-byte file, not a valid gzip
// stream — the gzip.d:135-145 chain, preserved), a missing trailing newline, CRLF
// line endings, a blank line mid-file (which stops GzipByLine's iteration because
// empty() is `buf.length == 0` at gzip.d:108-110), a 60000-byte file that crosses
// the 0x4000 chunk boundary, a 100000-byte run of one character (one inflate call
// producing far more than its input chunk), a truncated container (silent short
// read, no exception), a corrupt one ("data error" from ZlibException), a
// zlib-format container read through the same range (Phobos' determineFromData
// path), and a file that does not exist (ErrnoException text).
//
// The container bytes are not part of the corpus: they are compared against the
// committed D-written file as a self-check and reported in the summary.
//
// Run: ./test_extern_gzip_gzip <baseline> <payload.txt> <payload.dgz.gz> \
//        <payload.zraw> <scratch-dir>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <zlib.h>

#include "eil/error.hpp"
#include "eil/format.hpp"
#include "extern/gzip/gzip.hpp"

namespace {

int failures = 0;
long diffs_total = 0;
long lines_compared = 0;

std::vector<std::string> produced;
std::vector<std::string> scratch_lines;
// Where emit() puts its text: the corpus vector while replaying, a throwaway vector
// while running the self-checks.
std::vector<std::string>* sink = &produced;

std::string scratch_dir;

void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    } else {
        std::printf("ok: %s\n", what);
    }
}

std::string fmt(const char* f, ...) {
    static char buf[262144];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return std::string(buf);
}

// One D `writefln` can straddle lines because the payload text is echoed verbatim;
// split so the vector stays 1:1 with the corpus file's lines.
void emit(const std::string& s) {
    size_t start = 0;
    while (true) {
        const size_t nl = s.find('\n', start);
        if (nl == std::string::npos) { sink->push_back(s.substr(start)); return; }
        sink->push_back(s.substr(start, nl - start));
        start = nl + 1;
    }
}

std::string tmp_path(const std::string& name) { return scratch_dir + "/" + name; }

std::vector<char> read_bytes(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw eil::ExceptionBase("cannot read " + path);
    return std::vector<char>((std::istreambuf_iterator<char>(in)),
                             std::istreambuf_iterator<char>());
}

std::string basename_of(const std::string& path) {
    const size_t slash = path.find_last_of('/');
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

// gzip.d:99-124 GzipByLine driven the way D's foreach over it does.
void show_lines(const std::string& label, const std::string& path) {
    long i = 0;
    size_t total = 0;
    try {
        eil::gzip::GzipByLine byLine(path);
        while (!byLine.empty()) {
            const std::string line = byLine.front();
            emit(fmt("%s line %ld len=%zu|%s", label.c_str(), i, line.size(), line.c_str()));
            total += line.size();
            ++i;
            byLine.popFront();
        }
    } catch (eil::ExceptionBase& e) {
        // D catches `Exception e`, which is what ZlibException and ErrnoException
        // both derive from; an eil::ErrorBase must not land here.
        emit(fmt("%s threw Exception msg=%s", label.c_str(), e.msg().c_str()));
        return;
    }
    emit(fmt("%s nlines=%ld chars=%zu", label.c_str(), i, total));
}

void summarise(const std::string& label, const std::string& path) {
    long i = 0;
    size_t total = 0;
    unsigned long crc = 0u;
    size_t longest = 0;
    try {
        eil::gzip::GzipByLine byLine(path);
        while (!byLine.empty()) {
            const std::string line = byLine.front();
            crc = ::crc32(static_cast<uLong>(crc),
                          reinterpret_cast<const Bytef*>(line.data()),
                          static_cast<uInt>(line.size()));
            total += line.size();
            if (line.size() > longest) longest = line.size();
            ++i;
            byLine.popFront();
        }
    } catch (eil::ExceptionBase& e) {
        emit(fmt("%s threw Exception msg=%s", label.c_str(), e.msg().c_str()));
        return;
    }
    emit(fmt("%s nlines=%ld chars=%zu longest=%zu crc=%lx", label.c_str(), i, total, longest,
             crc));
}

void write_payload(const std::string& path, const std::vector<std::string>& parts) {
    eil::gzip::GzipOut f(path);
    for (const std::string& p : parts) f.compress(p);
    f.finish();
}

void raw_range_walk(const std::string& label, const std::string& path, size_t nchars) {
    eil::gzip::GzipInputRange r(path);
    size_t count = 0;
    long digest = 0;
    std::string head;
    while (!r.empty()) {
        const char ch = r.front();
        if (count < nchars) head += ch;
        // D's `char` is unsigned; cast the same way before the arithmetic.
        digest = (digest + static_cast<long>(count + 1) *
                  static_cast<int>(static_cast<unsigned char>(ch))) % 1000003;
        ++count;
        r.popFront();
    }
    emit(fmt("%s chars=%zu digest=%ld head=%s", label.c_str(), count, digest, head.c_str()));
}

long diff_against_file(const std::string& path, const std::vector<std::string>& got) {
    std::ifstream in(path);
    if (!in) {
        std::printf("cannot open corpus %s\n", path.c_str());
        return -1;
    }
    std::vector<std::string> expected;
    for (std::string line; std::getline(in, line);) expected.push_back(line);
    const size_t n = std::max(expected.size(), got.size());
    long diffs = 0;
    for (size_t i = 0; i < n; ++i) {
        ++lines_compared;
        if (i >= expected.size() || i >= got.size() || expected[i] != got[i]) {
            if (diffs < 20) {
                std::printf("line %zu\n  D:  |%s|\n  C++:|%s|\n", i + 1,
                            i < expected.size() ? expected[i].c_str() : "<none>",
                            i < got.size() ? got[i].c_str() : "<none>");
            }
            ++diffs;
        }
    }
    std::printf("  corpus: %zu D lines vs %zu C++ lines, %ld differ\n", expected.size(),
                got.size(), static_cast<long>(diffs));
    return diffs;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 6) {
        std::printf("usage: %s <extern_gzip_gzip.txt> <gzip_payload.txt> "
                    "<gzip_payload.dgz.gz> <gzip_payload.zraw> <scratch-dir>\n", argv[0]);
        return 2;
    }
    const std::string baseline_path = argv[1];
    const std::string payload_path = argv[2];
    const std::string d_gz_path = argv[3];
    const std::string d_zraw_path = argv[4];
    scratch_dir = argv[5];

    const std::vector<char> payload_bytes = read_bytes(payload_path);
    const std::string payload(payload_bytes.begin(), payload_bytes.end());
    emit(fmt("payload file=%s len=%zu", basename_of(payload_path).c_str(), payload.size()));

    // ---- 1. the container D wrote, decompressed by the port -----------------
    show_lines("roundtrip", d_gz_path);
    // Remember this block: the self-check compares the port's own three-chunk
    // container against the same expectations.
    std::vector<std::string> roundtrip_lines(produced.begin() + 1, produced.end());

    // ---- 2. written by the port in one compress() call ---------------------
    const std::string one = tmp_path("payload_one_cpp.gz");
    write_payload(one, {payload});
    show_lines("one_shot", one);

    // ---- 3. empty payload --------------------------------------------------
    const std::string emptyf = tmp_path("empty_cpp.gz");
    write_payload(emptyf, {std::string()});
    check(read_bytes(emptyf).empty(),
          "gzip.d: writing an empty payload leaves a zero-byte file, as in D");
    show_lines("empty_readback", emptyf);

    // ---- 4. no trailing newline / trailing newline / CRLF / blank line -----
    const std::string nonl = tmp_path("no_newline_cpp.gz");
    write_payload(nonl, {"alpha\nbravo\ncharlie"});
    show_lines("no_trailing_newline", nonl);

    const std::string withnl = tmp_path("with_newline_cpp.gz");
    write_payload(withnl, {"alpha\nbravo\ncharlie\n"});
    show_lines("trailing_newline", withnl);

    const std::string crlf = tmp_path("crlf_cpp.gz");
    write_payload(crlf, {"one\r\ntwo\r\n"});
    show_lines("crlf", crlf);

    const std::string blank = tmp_path("blank_line_cpp.gz");
    write_payload(blank, {"first\n\nsecond\nthird\n"});
    show_lines("blank_line", blank);

    // ---- 5. 60000 bytes over 1000 lines, crossing the chunk boundary ------
    std::string bigtext;
    for (int i = 0; i < 60000; ++i)
        bigtext += ((i % 60) == 59) ? '\n' : static_cast<char>('a' + (i % 26));
    const std::string big = tmp_path("big_cpp.gz");
    write_payload(big, {bigtext});
    summarise("big", big);
    raw_range_walk("big_raw_range", big, 12);

    // ---- 6. 100000 identical characters -----------------------------------
    const std::string hugetext(100000, 'Z');
    const std::string huge = tmp_path("huge_cpp.gz");
    write_payload(huge, {hugetext});
    summarise("huge", huge);
    raw_range_walk("huge_raw_range", huge, 8);

    // ---- 7. raw range walk over the D container ----------------------------
    raw_range_walk("roundtrip_raw_range", d_gz_path, 24);

    // ---- 8. truncated container: the same bytes D truncated ---------------
    {
        const std::vector<char> all = read_bytes(d_gz_path);
        const std::string trunc = tmp_path("truncated_cpp.gz");
        std::ofstream out(trunc, std::ios::binary);
        out.write(all.data(), static_cast<std::streamsize>(all.size() / 2));
        out.close();
        show_lines("truncated", trunc);
    }

    // ---- 9. corrupt container ---------------------------------------------
    {
        const std::string junk = tmp_path("junk_cpp.gz");
        const std::vector<char> zeros(1024, 0);
        std::ofstream out(junk, std::ios::binary);
        out.write(zeros.data(), static_cast<std::streamsize>(zeros.size()));
        out.close();
        show_lines("corrupt", junk);
    }

    // ---- 10. a zlib (non-gzip) container, D-written ------------------------
    show_lines("zlib_autodetect", d_zraw_path);

    // ---- 11. a path that does not exist -----------------------------------
    show_lines("missing_file", "no_such_file.gz");

    diffs_total += diff_against_file(baseline_path, produced);

    // ---- self-checks that the corpus cannot express ------------------------
    sink = &scratch_lines;
    // (a) the port's own writer/reader round trip keeps every byte: re-joining the
    // lines with newlines reproduces the payload (it has no blank lines and ends
    // with exactly one newline).
    {
        eil::gzip::GzipByLine back(one);
        std::string rebuilt;
        while (!back.empty()) { rebuilt += back.front(); rebuilt += "\n"; back.popFront(); }
        check(rebuilt == payload,
              "gzip round trip through the port's own writer/reader keeps every byte");
    }
    // (b) three compress() calls, exactly as the committed D container was written.
    const std::string rt3 = tmp_path("payload_dgz_cpp.gz");
    {
        // The same three pieces the probe writes: [0..third), [third..2*third),
        // [2*third..$] (probe_gzip.d:118-120).
        const size_t third = payload.size() / 3;
        write_payload(rt3, {payload.substr(0, third), payload.substr(third, third),
                            payload.substr(2*third)});
        scratch_lines.clear();
        show_lines("roundtrip", rt3);
        long bad = 0;
        const size_t n = std::min(roundtrip_lines.size(), scratch_lines.size());
        for (size_t i = 0; i < n; ++i) {
            if (roundtrip_lines[i] != scratch_lines[i]) ++bad;
        }
        check(bad == 0 && scratch_lines.size() == roundtrip_lines.size(),
              "gzip: the port's three-chunk container decompresses to D's line list");
    }
    // (c) container bytes against D's. The brief says a container is not a gate
    // (the gzip header carries an MTIME field, and Phobos' Compress writes it as
    // four zero bytes only because the zlib it links is 1.2.x; the payload is the
    // thing that has to match). Measured here: zlib 1.2.11 on this box produces
    // byte-identical streams to the reference for both the one-call and the
    // three-call writes, so the comparison is reported and checked, but a future
    // zlib that changes its block splitting must not be treated as a port bug.
    {
        const std::vector<char> dref = read_bytes(d_gz_path);
        const std::vector<char> mine = read_bytes(rt3);
        const std::vector<char> mine_one = read_bytes(one);
        long same = 0;
        long first_diff = -1;
        const size_t n = std::min(dref.size(), mine.size());
        for (size_t i = 0; i < n; ++i) {
            if (dref[i] == mine[i]) ++same;
            else if (first_diff < 0) first_diff = static_cast<long>(i);
        }
        long same_one = 0;
        const size_t n1 = std::min(dref.size(), mine_one.size());
        for (size_t i = 0; i < n1; ++i) if (dref[i] == mine_one[i]) ++same_one;
        std::printf("  container (not a gate): three-chunk write %ld/%zu bytes equal to "
                    "D's container, one-call write %ld/%zu bytes equal, MTIME bytes 4..7 "
                    "read as %02x%02x%02x%02x\n",
                    same, dref.size(), same_one, dref.size(),
                    static_cast<unsigned char>(dref.size() > 4 ? dref[4] : 0),
                    static_cast<unsigned char>(dref.size() > 5 ? dref[5] : 0),
                    static_cast<unsigned char>(dref.size() > 6 ? dref[6] : 0),
                    static_cast<unsigned char>(dref.size() > 7 ? dref[7] : 0));
        check(dref.size() == mine.size() && dref.size() == mine_one.size(),
              "gzip: container length equals D's for both write patterns");
    }
    // (d) the structural invariants of the container the port writes: gzip magic,
    // and a trailer carrying crc32(payload) and the payload length.
    {
        const std::vector<char> mine = read_bytes(rt3);
        check(mine.size() > 18 && static_cast<unsigned char>(mine[0]) == 0x1f &&
              static_cast<unsigned char>(mine[1]) == 0x8b &&
              static_cast<unsigned char>(mine[2]) == 0x08,
              "gzip: container starts with the gzip magic 1f 8b 08");
        const unsigned long want = ::crc32(0u, reinterpret_cast<const Bytef*>(payload.data()),
                                           static_cast<uInt>(payload.size()));
        unsigned long crc = 0, isize = 0;
        const size_t t = mine.size() - 8;
        for (int k = 0; k < 4; ++k) {
            crc |= static_cast<unsigned long>(static_cast<unsigned char>(mine[t + k])) << (8*k);
            isize |= static_cast<unsigned long>(static_cast<unsigned char>(mine[t + 4 + k])) << (8*k);
        }
        check(crc == want, "gzip: container CRC32 trailer matches the payload");
        check(isize == static_cast<unsigned long>(payload.size()),
              "gzip: container ISIZE trailer matches the payload length");
    }

    const bool ok = (diffs_total == 0) && (failures == 0);
    std::printf("%s: extern.gzip, %ld corpus lines compared, %ld differ, %d assertion "
                "failures\n", ok ? "PASS" : "FAIL", lines_compared, diffs_total, failures);
    return ok ? 0 : 1;
}
