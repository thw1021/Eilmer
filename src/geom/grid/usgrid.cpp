// geom/grid/usgrid.d -> src/geom/grid/usgrid.cpp (module geom.grid.usgrid)
//
// Statement-for-statement port of usgrid.d in D source order. See
// src/geom/grid/usgrid.hpp for the header-level non-1:1 notes (adapted-handles, the
// sgrid/usgrid cycle, the throw-site rule, the signed-length rule).
//
// ONE measured rule that overrides the wave-02 mapping table for this whole file, and
// it comes from the reference, not from the port: `in` CONTRACTS ON AN OVERRIDE OF AN
// ABSTRACT METHOD ARE NOT ENFORCED BY D. probes/geom/grid/probe_usgrid.d records it
// three ways (its MEASURED FINDINGS block, item 1):
//   * us.idx.vtx.badi / badj / badk and us.idx.op.j / op.k are no-throw rows — a call
//     with j=1 or k=7 succeeds and returns the list for i alone;
//   * us.idx.op.i and us.idx.vtx.oob report core.exception.ArrayIndexError raised in
//     the BODY (usgrid.d:872, :1034), not AssertError at the contract lines 860-862,
//     870, 1013-1015, 1030;
//   * an isolated 20-line module reproduces the split, and the trigger is
//     "implementation of an abstract member" (an override of a CONCRETE virtual does
//     run its contract — grid.d:71 single_index and :81 ijk_indices fire, and those
//     contracts ARE ported, in src/geom/grid/grid.cpp).
// So the eight asserts of usgrid.d:859-863, :870, :1012-1016, :1029-1032 are NOT
// turned into eil_assert here, and their message texts stay dead text as they are dead
// in D. The bounds failures D does produce come from the array indexing in the bodies,
// which eil::Array reproduces with the identical ArrayIndexError text
// ("index [N] is out of bounds for array of length L", substrate/include/eil/error.hpp).
// Body asserts (D:821-823, :963, :136-137, :1136, :1181, :1263, :1499, :1659, :1728,
// :1977, :1983, :2001, :2008, :2174-2179, :2334, :2338) DO become eil_assert /
// eil_assert_msg, because those run in D.
//
// Port-local pieces, all in an anonymous namespace so no other module picks them up:
// the D text/binary file shims (D's std.stdio.File is a GC handle with readln/eof/
// rewind/rawRead/rawWrite; every ported reader here already uses a raw std::FILE* plus
// a scope guard — src/nm/stmatrix.hpp:96-116,
// src/geom/surface/parametricsurface.cpp:52-70), D's strip/split, the std.algorithm
// pieces (sort, equal, isPermutation, countUntil, reverse, uniq) and
// std.format.formattedRead.

#include "geom/grid/usgrid.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <tuple>
#include <vector>

#include "eil/conv.hpp"
#include "eil/error.hpp"
#include "eil/math.hpp"
#include "extern/gzip/gzip.hpp"
#include "geom/elements/properties.hpp"
#include "geom/geometry_exception.hpp"
#include "geom/grid/paver.hpp"
#include "geom/grid/paver2d.hpp"
#include "geom/grid/sgrid.hpp"       // the cycle partner: complete type only here
#include "geom/misc/kdtree.hpp"

namespace eil {
namespace geom {

namespace {

// ---- D's std.string.strip / std.array.split as usgrid.d uses them ----
// split() with no argument breaks on runs of whitespace and yields no empty tokens.
eil::Array<std::string> split_ws(const std::string& s) {
    eil::Array<std::string> out;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        if (i >= s.size()) break;
        size_t j = i;
        while (j < s.size() && !std::isspace(static_cast<unsigned char>(s[j]))) ++j;
        out.append(s.substr(i, j - i));
        i = j;
    }
    return out;
}

std::string d_strip(const std::string& s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return s.substr(a, b - a);
}

// ---- D's std.stdio.File, as far as this module uses it ----
// File(name, mode) raises std.exception.ErrnoException on a failed open; the measured
// text (src/extern/gzip/gzip.cpp:22-34 reproduces the same Phobos message) is
//   Cannot open file `<name>' in mode `<mode>' (No such file or directory)
// and substrate has no ErrnoException class yet, so — as every other ported reader here
// already does — the message is carried by eil::ExceptionBase with the throw site.
std::FILE* d_open(const std::string& name, const char* mode) {
    std::FILE* h = std::fopen(name.c_str(), mode);
    if (h == nullptr) {
        const std::string msg = "Cannot open file `" + name + "' in mode `" + mode + "' (" +
                                std::strerror(errno) + ")";
        throw eil::ExceptionBase(msg, __FILE__, __LINE__);
    }
    return h;
}

// D's File closes (and flushes) in its destructor, including while an exception is
// unwinding, so a scope guard is required: a leaked handle would leave different bytes
// on disk than D does.
struct DFileGuard {
    std::FILE* h = nullptr;
    DFileGuard() = default;
    explicit DFileGuard(std::FILE* p) : h(p) {}
    ~DFileGuard() {
        if (h != nullptr) std::fclose(h);
    }
    DFileGuard(const DFileGuard&) = delete;
    DFileGuard& operator=(const DFileGuard&) = delete;
    // So a ported call site can keep D's own name for the handle (`f`, `fout`, `fin`)
    // instead of inventing a second one.
    operator std::FILE*() const { return h; }
};

// D's f.readln(): the line without its terminator; null at EOF, which D's callers
// immediately .strip().split() — an empty token list.
std::string d_readln(std::FILE* f, bool& at_eof) {
    std::string out;
    int c = std::fgetc(f);
    if (c == EOF) {
        at_eof = true;
        return out;
    }
    while (c != EOF && c != '\n') {
        out += static_cast<char>(c);
        c = std::fgetc(f);
    }
    if (!out.empty() && out.back() == '\r') out.pop_back();
    at_eof = (c == EOF);
    return out;
}

void d_write(std::FILE* f, const std::string& s) {
    std::fwrite(s.data(), 1, s.size(), f);
}
void d_writeln(std::FILE* f, const std::string& s) {
    d_write(f, s);
    d_write(f, "\n");
}
template <typename... Args>
void d_writefln(std::FILE* f, const std::string& fmt, const Args&... args) {
    d_writeln(f, eil::format(fmt, args...));
}
template <typename... Args>
void d_writef(std::FILE* f, const std::string& fmt, const Args&... args) {
    d_write(f, eil::format(fmt, args...));
}

// D's rawRead/rawWrite for 32-bit ints, 64-bit doubles and chars: the memory image.
void d_raw_write(std::FILE* f, const void* p, size_t n) {
    std::fwrite(p, 1, n, f);
}
size_t d_raw_read(std::FILE* f, void* p, size_t n) {
    return std::fread(p, 1, n, f);
}

// ---- the std.algorithm pieces usgrid.d names ----
bool d_equal(eil::Slice<size_t> a, eil::Slice<size_t> b) { // D:140, :146 equal()
    if (a.length() != b.length()) return false;
    for (size_t i = 0; i < a.length(); ++i) {
        if (!(a[i] == b[i])) return false;
    }
    return true;
}

bool d_is_permutation(eil::Slice<size_t> a, eil::Slice<size_t> b) { // D:137
    if (a.length() != b.length()) return false;
    std::vector<size_t> sa, sb;
    for (size_t i = 0; i < a.length(); ++i) sa.push_back(a[i]);
    for (size_t i = 0; i < b.length(); ++i) sb.push_back(b[i]);
    std::sort(sa.begin(), sa.end());
    std::sort(sb.begin(), sb.end());
    return sa == sb;
}

// D:949-952 countUntil(list, value) -> ptrdiff_t, -1 when absent, and usgrid.d assigns
// it into a size_t, so "not found" becomes 18446744073709551615 and the following index
// is D's out-of-bounds access (corpus row us.gap.963 prints exactly that index).
size_t d_count_until(eil::Slice<size_t> list, size_t value) {
    for (size_t i = 0; i < list.length(); ++i) {
        if (list[i] == value) return i;
    }
    return static_cast<size_t>(-1);
}

void d_reverse(eil::Array<size_t>& a) { // D:1528, :2062
    size_t n = a.length();
    for (size_t i = 0; i < n / 2; ++i) {
        size_t t = a[i];
        a[i] = a[n - 1 - i];
        a[n - 1 - i] = t;
    }
}

// D:1285 `array(x.sort().uniq())` in getVertexIdsOnBoundaries (D:2475).
eil::Array<size_t> d_sorted_uniq(const eil::Array<size_t>& in) {
    std::vector<size_t> v;
    for (size_t i = 0; i < in.length(); ++i) v.push_back(in[i]);
    std::sort(v.begin(), v.end());
    v.erase(std::unique(v.begin(), v.end()), v.end());
    eil::Array<size_t> out;
    for (size_t x : v) out.append(x);
    return out;
}

// ---- std.format.formattedRead, the shapes used by these two readers ----
// Literal characters must match at the current position; a space in the format string
// means "any run of whitespace". The conversion specs used here are %s (destination
// string: the rest of the line), %d (int/size_t) and %g (double).
//
// Phobos' failure texts are part of the contract, because they reach the corpus:
//   input exhausted at a literal -> "parseToFormatSpec: Cannot find character '<c>' in
//                                    the input string."   10 of the 18 sg.frmt.* rows of
//                                    geom_grid_sgrid.txt are this text, each naming the
//                                    FIRST literal character of the format string, i.e.
//                                    the input line was empty.
//   %d / %g on non-numeric input -> std.conv.to!int / to!double's own wording
//                                    (rows sg.frmt.dimsword and sg.frmt.wordvtx).
struct FormattedInput {
    const std::string& s;
    size_t p = 0;
    explicit FormattedInput(const std::string& str) : s(str) {}
    bool exhausted() const { return p >= s.size(); }
    char next() const { return s[p]; }
};

[[noreturn]] void fmt_fail_missing_char(char c) {
    throw eil::format_error(std::string("parseToFormatSpec: Cannot find character '") + c +
                            "' in the input string.");
}

void fmt_read_lit(FormattedInput& in, char c) {
    if (c == ' ') {
        while (!in.exhausted() && std::isspace(static_cast<unsigned char>(in.next()))) ++in.p;
        return;
    }
    // Phobos' parseToFormatSpec (std/format/spec.d:564-567) raises the SAME text whether
    // the input ran out or the character at the current position simply differs:
    //   enforceFmt(!r.empty && r.front == trailing.front,
    //              text("parseToFormatSpec: Cannot find character '", c, "' ..."));
    // Corpus row 861 (us.gz.wronghead) pins the mismatch arm: "structured_grid 1.0" read
    // against "unstructured_grid %s" names the expected 'u', not the 's' that was there,
    // and a forward *search* would name 'n' instead — so the match is positional and the
    // wording is the missing-character one.
    if (in.exhausted() || in.next() != c) fmt_fail_missing_char(c);
    ++in.p;
}

std::string fmt_read_token(FormattedInput& in) {
    while (!in.exhausted() && std::isspace(static_cast<unsigned char>(in.next()))) ++in.p;
    const size_t start = in.p;
    while (!in.exhausted() && !std::isspace(static_cast<unsigned char>(in.next()))) ++in.p;
    return in.s.substr(start, in.p - start);
}

std::string fmt_read_rest(FormattedInput& in) {
    while (!in.exhausted() && std::isspace(static_cast<unsigned char>(in.next()))) ++in.p;
    const std::string out = in.s.substr(in.p);
    in.p = in.s.size();
    return out;
}

// D's `to!int(string)` (usgrid.d:184, :185, :256-:268, :353-:360, :1407, :1425, :1721) is
// Phobos' strict [+-]?digits scan with a RANGE CHECK: as soon as the accumulated value has
// left the int range it raises std.conv.ConvOverflowException "Overflow in integral
// conversion" (corpus row 208: `new USGFace("9999999999999999999 1")` reports that class,
// not the OutOfMemoryError a negative length would give — row 207 — because the conversion
// is checked BEFORE `vtx_id_list.length = ...` can allocate).
// eil::to_int (substrate/include/eil/conv.hpp:293) is `static_cast<int>(parse_int_strict(s,
// "int", true))` with no range check at all, so an overflowing count silently becomes a
// negative number and the port reached the wrong failure path. SUBSTRATE GAP (reported to
// the main thread): substrate has neither the check nor the ConvOverflowException child of
// ConversionException, and this stage may not modify it, so both live here —
// ConvOverflowException in src/geom/grid/usgrid.hpp, this scanner next to the other
// port-local std.conv/std.format shims. The grammar failure texts stay substrate's
// (eil::detail::unexpected_end / unexpected_char_at), so the non-numeric rows
// (us.face.str.nonint, us.face.str.hex, us.cell.str.badint) are unaffected.
// D's `arr.length = n` when n is larger than the allocator can satisfy raises
// core.exception.OutOfMemoryError "Memory allocation failed" with the runtime's OWN
// location (.file == "rt/lifetime.d", .line == 1454 — see substrate T-914 and
// probes/substrate/probe_throw_identity.d). substrate's Array reports its own wording
// ("Array allocation failed", array.hpp:287) from its own site, and this stage may not
// modify substrate, so the port funnels every `arr.length = ...` here and re-raises with
// D's message. Corpus row: us.sgbad.zeroj (a 3x0x1 lattice gives
// nfaces = 3*(0-1) = a size_t wrap, which D cannot allocate).
template <typename T> void set_length_d(eil::Array<T>& a, size_t n) {
    try {
        a.set_length(n);
    } catch (const eil::OutOfMemoryError&) {
        throw eil::ErrorBase("Memory allocation failed");
    }
}

int d_to_int(const std::string& s) {    const size_t n = s.size();
    size_t i = 0;
    bool neg = false;
    if (i < n && (s[i] == '+' || s[i] == '-')) { neg = (s[i] == '-'); ++i; }
    if (i >= n) eil::detail::conv_fail(eil::detail::unexpected_end("int"));
    const long long limit_pos = 2147483647LL;   // INT_MAX
    const long long limit_neg = 2147483648LL;   // |INT_MIN|
    long long acc = 0;
    for (; i < n; ++i) {
        const unsigned char c = static_cast<unsigned char>(s[i]);
        if (!std::isdigit(c)) {
            eil::detail::conv_fail(eil::detail::unexpected_char_at(s, i, "int"));
        }
        acc = acc * 10 + static_cast<long long>(c - '0');
        if (neg ? (acc > limit_neg) : (acc > limit_pos)) {
            throw ConvOverflowException("Overflow in integral conversion");
        }
    }
    return static_cast<int>(neg ? -acc : acc);
}

template <typename T> void fmt_into(FormattedInput& in, T* dst);
// Phobos' numeric unformat (std/format/internal/read.d:196) is `std.conv.parse!T(input)`
// after the whitespace skip, and `parse` on an EXHAUSTED input leaves the destination
// exactly as it was and raises nothing. Measured directly against ldc2 1.40.0:
//   formattedRead("", " %g %g %g", &x,&x,&x) -> ok, x unchanged;
//   formattedRead("", "%d", &sz)              -> ok, sz unchanged.
// That is what makes corpus rows 868-871 (us.gz.truncated4..7) come out the way they do:
// the gzipped text declares 10 vertices but the file has run out, so ten reads of
// " %g %g %g" over an exhausted GzipByLine (which yields "" per T-923) all succeed with
// x=y=z=0, and the FIRST failure is the literal 'f' of "faces: %d" one step later. A
// conversion attempt on the empty string instead would report
// `Floating point conversion error for input ""` and the row would name the wrong class.
template <> void fmt_into<int>(FormattedInput& in, int* dst) {
    const std::string tok = fmt_read_token(in);
    if (tok.empty()) return; // exhausted: D writes nothing
    *dst = d_to_int(tok);
}
template <> void fmt_into<long>(FormattedInput& in, long* dst) {
    const std::string tok = fmt_read_token(in);
    if (tok.empty()) return;
    *dst = eil::to_long(tok);
}
template <> void fmt_into<size_t>(FormattedInput& in, size_t* dst) {
    const std::string tok = fmt_read_token(in);
    if (tok.empty()) return;
    // The size_t targets here are D's nvertices/ncells/nfaces/nboundaries (grid.d:45-46,
    // usgrid.d:426), so the failure text must name ulong and reject a sign, exactly as the
    // direct `to!size_t` sites above do (substrate_format_int_target.txt, size_t.word).
    *dst = eil::to_size_t(tok);
}
template <> void fmt_into<double>(FormattedInput& in, double* dst) {
    const std::string tok = fmt_read_token(in);
    if (tok.empty()) return;
    *dst = eil::to_double(tok);
}
template <> void fmt_into<std::string>(FormattedInput& in, std::string* dst) {
    *dst = fmt_read_rest(in);
}

template <typename... Args> struct FmtArgs {
    std::tuple<Args*...> t;
    size_t i = 0;
    template <size_t... Is>
    void apply(char spec, FormattedInput& in, std::index_sequence<Is...>) {
        ((Is == i ? (fmt_into(in, std::get<Is>(t)), 0) : 0) + ...);
        ++i;
    }
};

// D:1285-1321 here, and sgrid.d:861-914 there, all spell
// formattedRead(line, fmt, &a, &b, ...).
template <typename... Args>
void d_formatted_read(const std::string& line, const std::string& fmt, Args*... dst) {
    FmtArgs<Args...> a{std::tuple<Args*...>{dst...}, 0};
    FormattedInput in(line);
    for (size_t fi = 0; fi < fmt.size();) {
        const char c = fmt[fi];
        if (c != '%') {
            fmt_read_lit(in, c);
            ++fi;
            continue;
        }
        ++fi;
        if (fi >= fmt.size()) throw eil::format_error("format string ends inside a conversion");
        a.apply(fmt[fi], in, std::index_sequence_for<Args...>{});
        ++fi;
    }
}

// D's generated to!string of a USGCell_type member (see the public definition below).
std::string cell_type_text(USGCell_type ct) {
    const int v = static_cast<int>(ct);
    if (v < 0 || v >= static_cast<int>(cell_names.length())) {
        return "cast(USGCell_type)" + std::to_string(v);
    }
    return cell_names[static_cast<size_t>(v)];
}

} // namespace

// ---- D's associative-array iteration order (declarations in usgrid.hpp) ----
// Measured against ldc2 1.40.0's own druntime, not guessed: 12 key sets printed by a
// scratch probe (integral, uint, string, 8-element with a 1e12 key) all reproduce.
size_t d_aa_hash(size_t key) { return key; }                      // hashOf(size_t) == key

// hashOf(string) is std.digest murmurHash3-32 over the bytes with seed 0
// (core/internal/hash.d:720-770, chosen for a 1-byte-aligned element type by
// bytesHashAlignedBy at :670-672). Measured: hashOf("0-1") == 0x8700d778.
size_t d_aa_hash(const std::string& key) {
    uint32_t h1 = 0;
    const size_t n = key.size();
    const size_t nblocks = n / 4;
    const uint32_t c1 = 0xcc9e2d51u, c2 = 0x1b873593u, c3 = 0xe6546b64u;
    auto rd4 = [&](size_t at) {
        return static_cast<uint32_t>(static_cast<unsigned char>(key[at]))
             | static_cast<uint32_t>(static_cast<unsigned char>(key[at + 1])) << 8
             | static_cast<uint32_t>(static_cast<unsigned char>(key[at + 2])) << 16
             | static_cast<uint32_t>(static_cast<unsigned char>(key[at + 3])) << 24;
    };
    for (size_t b = 0; b < nblocks; ++b) {
        uint32_t k1 = rd4(b * 4);
        k1 *= c1; k1 = (k1 << 15) | (k1 >> 17); k1 *= c2;
        h1 ^= k1; h1 = (h1 << 13) | (h1 >> 19); h1 = h1 * 5 + c3;
    }
    uint32_t k1 = 0;
    const size_t tail = nblocks * 4;
    const size_t rem = n & 3;
    if (rem == 3) k1 ^= static_cast<unsigned char>(key[tail + 2]) << 16;
    if (rem >= 2) k1 ^= static_cast<unsigned char>(key[tail + 1]) << 8;
    if (rem >= 1) {
        k1 ^= static_cast<unsigned char>(key[tail]);
        k1 *= c1; k1 = (k1 << 15) | (k1 >> 17); k1 *= c2; h1 ^= k1;
    }
    h1 ^= static_cast<uint32_t>(n);
    h1 = (h1 ^ (h1 >> 16)) * 0x85ebca6bu;
    h1 = (h1 ^ (h1 >> 13)) * 0xc2b2ae35u;
    h1 ^= h1 >> 16;
    return static_cast<size_t>(h1);
}

size_t d_aa_mix(size_t h) {                        // newaa.d:69-77
    h ^= h >> 13;
    h *= 0x5bd1e995ULL;
    h ^= h >> 15;
    return h;
}

size_t d_aa_bucket_count(size_t n) {               // newaa.d:100-102
    size_t dim = 8;
    while (n * 5 > dim * 4) dim *= 4;
    return dim;
}

// ---- usgrid.d:40-110: the tag tables and the three translation functions ----
USGCell_type convert_cell_type(int vtk_element_type) { // D:62
    switch (vtk_element_type) {
    case static_cast<int>(VTKElement::triangle): return USGCell_type::triangle;
    case static_cast<int>(VTKElement::quad): return USGCell_type::quad;
    case static_cast<int>(VTKElement::tetra): return USGCell_type::tetra;
    case static_cast<int>(VTKElement::wedge): return USGCell_type::wedge;
    case static_cast<int>(VTKElement::hexahedron): return USGCell_type::hexahedron;
    case static_cast<int>(VTKElement::pyramid): return USGCell_type::pyramid;
    case static_cast<int>(VTKElement::line): return USGCell_type::line;
    default: return USGCell_type::none;
    }
}

int n_vertices_for_vtk_element(int vtk_element_type) { // D:78
    switch (vtk_element_type) {
    case static_cast<int>(VTKElement::triangle): return 3;
    case static_cast<int>(VTKElement::quad): return 4;
    case static_cast<int>(VTKElement::tetra): return 4;
    case static_cast<int>(VTKElement::wedge): return 6;
    case static_cast<int>(VTKElement::hexahedron): return 8;
    case static_cast<int>(VTKElement::pyramid): return 5;
    case static_cast<int>(VTKElement::line): return 2;
    default: return 0;
    }
}

// D:94. The commented-out `case "polygon"` at D:101 is a deliberate refusal in the
// reference, so "polygon" maps to none (corpus row us.name.fromname [polygon]->0).
USGCell_type cell_type_from_name(const std::string& name) {
    if (name == "none") return USGCell_type::none;
    if (name == "triangle") return USGCell_type::triangle;
    if (name == "quad") return USGCell_type::quad;
    if (name == "tetra") return USGCell_type::tetra;
    if (name == "wedge") return USGCell_type::wedge;
    if (name == "hexahedron") return USGCell_type::hexahedron;
    if (name == "pyramid") return USGCell_type::pyramid;
    if (name == "line") return USGCell_type::line;
    return USGCell_type::none;
}

// D:278 `to!USGCell_type(buf1[0])`. Out-of-range raises std.conv.ConvException with
// Phobos' wording, measured at rows us.name.tenum[9]/[42]/[-1]/[-2147483648]/
// [2147483647]. Those rows also carry Phobos' own file/line (std/conv.d:2119), which the
// port cannot name; per T-922's rule for anything raised from inside the runtime, no
// site is attached here.
USGCell_type to_usg_cell_type(int v) {
    switch (v) {
    case 0: return USGCell_type::none;
    case 1: return USGCell_type::triangle;
    case 2: return USGCell_type::quad;
    case 3: return USGCell_type::polygon;
    case 4: return USGCell_type::tetra;
    case 5: return USGCell_type::wedge;
    case 6: return USGCell_type::hexahedron;
    case 7: return USGCell_type::pyramid;
    case 8: return USGCell_type::line;
    default:
        throw eil::ConversionException("Value (" + std::to_string(v) +
                                       ") does not match any member value of enum "
                                       "'USGCell_type'");
    }
}

// ---- usgrid.d:112-132 makeFaceTag ----
// Sorted ascending copy, joined with '-' under `%d`. The caller's array is NOT reordered
// (corpus us.tag.noreorder).
std::string makeFaceTag(eil::Slice<size_t> vtx_id_list) {
    std::vector<size_t> my_id_list;
    for (size_t i = 0; i < vtx_id_list.length(); ++i) my_id_list.push_back(vtx_id_list[i]);
    std::sort(my_id_list.begin(), my_id_list.end());
    std::string tag;
    const size_t n = my_id_list.size();
    for (size_t i = 0; i < n; ++i) {
        if (i > 0) tag += "-";
        tag += eil::format("%d", my_id_list[i]);
    }
    return tag;
}

// ---- usgrid.d:134-152 sameOrientation ----
// Both asserts are BODY asserts (D:136, :137) and both DO fire in D: rows
// us.orient.lenmismatch and us.orient.notpermu give AssertError with these exact
// messages at usgrid.d lines 136 and 137.
//
// The loop is D's, not the intended one. `bringToFront(test_list[1..$],
// test_list[$-1..$])` rotates the TAIL right by one and never moves element 0, so D
// compares 0 against the tail rotations only. The measured sequences are corpus rows
// us.orient.seq3 ([2,0,1] -> [2,1,0] -> [2,0,1]) and us.orient.seq4
// ([2,3,0,1] -> [2,1,3,0] -> [2,0,1,3] -> back). Reproduced as tested, because
// joinGrid's flip_new_face (D:2299) depends on which orderings come back true.
bool sameOrientation(eil::Slice<size_t> vtx_id_list_a, eil::Slice<size_t> vtx_id_list_b) {
    eil_assert_msg(vtx_id_list_a.length() == vtx_id_list_b.length(),
                   "Mismatch in vtx list lengths.");
    eil_assert_msg(d_is_permutation(vtx_id_list_a, vtx_id_list_b),
                   "Lists should be permutations of vtx ids");
    if (vtx_id_list_a.length() == 2) {
        // In 2D, faces have only 2 points, so they must be in order.
        return d_equal(vtx_id_list_a, vtx_id_list_b);
    }
    eil::Array<size_t> test_list = eil::Array<size_t>(vtx_id_list_b);
    const size_t n = test_list.length();
    for (size_t j = 0; j < n; ++j) {
        if (d_equal(vtx_id_list_a, test_list.all())) return true;
        if (n > 1) {
            const size_t last = test_list[n - 1];
            for (size_t i = n - 1; i > 1; --i) test_list[i] = test_list[i - 1];
            test_list[1] = last;
        }
    }
    return false;
}

// ---- usgrid.d:154-216 USGFace ----
USGFace::USGFace(eil::Slice<size_t> vtx_id_list) {
    this->vtx_id_list = eil::Array<size_t>(vtx_id_list); // D:169 `.dup()`
}

USGFace::USGFace(const USGFace& other) {
    vtx_id_list = other.vtx_id_list.dup();
    // D:174-177: the const constraint means left_cell/right_cell are NOT copied, and
    // is_on_boundary keeps its default false (measured: corpus us.face.copy.*).
}

USGFace::USGFace(const std::string& str) {
    // D:183-185. tokens is D's string[], so an over-long index is ArrayIndexError;
    // eil::Array reproduces it. The length comes from the FILE as a signed int (T-914).
    eil::Array<std::string> tokens = split_ws(d_strip(str));
    vtx_id_list.set_length_signed(d_to_int(tokens[0]));
    for (size_t i = 0; i < vtx_id_list.length(); ++i) {
        vtx_id_list[i] = static_cast<size_t>(d_to_int(tokens[i + 1]));  // D:185 to!int
    }
}

USGFace::USGFace(std::FILE* fin) {
    int buf1[1] = {0};                                  // D:191 `int[1] buf1`
    d_raw_read(fin, buf1, 4);
    vtx_id_list.set_length_signed(buf1[0]);
    for (size_t i = 0; i < vtx_id_list.length(); ++i) {
        d_raw_read(fin, buf1, 4);
        vtx_id_list[i] = static_cast<size_t>(buf1[0]);
    }
}

std::string USGFace::toString() const {
    return eil::format("USGFace(vtx_id_list=%s)", vtx_id_list); // D:198
}

std::string USGFace::toIOString() const {
    std::string str = eil::to_string(vtx_id_list.length()); // D:204
    for (size_t i = 0; i < vtx_id_list.length(); ++i) { str += " "; str += eil::to_string(vtx_id_list[i]); }
    return str;
}

void USGFace::rawWrite(std::FILE* fout) const {
    int buf1[1] = {0};
    buf1[0] = static_cast<int>(vtx_id_list.length()); d_raw_write(fout, buf1, 4); // D:212
    for (size_t i = 0; i < vtx_id_list.length(); ++i) {
        buf1[0] = static_cast<int>(vtx_id_list[i]); d_raw_write(fout, buf1, 4);
    }
}

// ---- usgrid.d:218-326 USGCell ----
USGCell::USGCell(USGCell_type cell_type, size_t id, eil::Slice<size_t> vtx_id_list,
                 eil::Slice<size_t> face_id_list, eil::Slice<int> outsign_list) {
    this->id = id;                                         // D:229
    this->cell_type = cell_type;                           // D:230
    this->vtx_id_list = eil::Array<size_t>(vtx_id_list);   // D:231 .dup
    this->face_id_list = eil::Array<size_t>(face_id_list); // D:232 .dup
    this->outsign_list = eil::Array<int>(outsign_list);    // D:233 .dup
}

USGCell::USGCell(const USGCell& other, size_t id) {
    // D:238 `id = other.id;` assigns to the PARAMETER (D shadowing), so this->id is
    // never written and keeps size_t.init == 0. Measured: corpus row us.cell.copyid
    // shows orig=3, passed=77, copy=0, and every cell of a duplicated grid therefore
    // comes back with id 0 (us.dup.*), which in turn collapses get_list_of_boundary_
    // cells' dictionary (us.blc.dup). Kept verbatim.
    id = other.id;
    cell_type = other.cell_type;
    vtx_id_list = other.vtx_id_list.dup();
    face_id_list = other.face_id_list.dup();
    outsign_list = other.outsign_list.dup();
}

USGCell::USGCell(const std::string& str, size_t id) {
    // D:248-269. The `auto junk = tokens[itok++]` values are read and discarded.
    this->id = id;
    eil::Array<std::string> tokens = split_ws(d_strip(str));
    size_t itok = 0;
    cell_type = cell_type_from_name(tokens[itok++]);
    if (cell_type == USGCell_type::none) {
        EIL_THROW_GEOM("Unexpected cell type from line: " + str); // D:253
    }
    std::string junk = tokens[itok++]; // "vtx"
    (void)junk;
    vtx_id_list.set_length_signed(d_to_int(tokens[itok++]));
    for (size_t i = 0; i < vtx_id_list.length(); ++i) {
        vtx_id_list[i] = static_cast<size_t>(d_to_int(tokens[itok++]));  // D:258 to!int
    }
    junk = tokens[itok++]; // "faces"
    face_id_list.set_length_signed(d_to_int(tokens[itok++]));
    for (size_t i = 0; i < face_id_list.length(); ++i) {
        face_id_list[i] = static_cast<size_t>(d_to_int(tokens[itok++]));  // D:263, :355 to!int
    }
    junk = tokens[itok++]; // "outsigns"
    outsign_list.set_length_signed(d_to_int(tokens[itok++]));
    for (size_t i = 0; i < outsign_list.length(); ++i) {
        outsign_list[i] = d_to_int(tokens[itok++]);
    }
}

USGCell::USGCell(std::FILE* fin, size_t id) {
    this->id = id;
    int buf1[1] = {0};
    d_raw_read(fin, buf1, 4);
    cell_type = to_usg_cell_type(buf1[0]); // D:278 to!USGCell_type
    if (cell_type == USGCell_type::none) {
        // D:280 goes through to!string of the enum, which for `none` is "none".
        EIL_THROW_GEOM("Unexpected cell type: " + cell_type_text(cell_type));
    }
    d_raw_read(fin, buf1, 4); vtx_id_list.set_length_signed(buf1[0]);
    for (size_t i = 0; i < vtx_id_list.length(); ++i) {
        d_raw_read(fin, buf1, 4); vtx_id_list[i] = static_cast<size_t>(buf1[0]);
    }
    d_raw_read(fin, buf1, 4); face_id_list.set_length_signed(buf1[0]);
    for (size_t i = 0; i < face_id_list.length(); ++i) {
        d_raw_read(fin, buf1, 4); face_id_list[i] = static_cast<size_t>(buf1[0]);
    }
    d_raw_read(fin, buf1, 4); outsign_list.set_length_signed(buf1[0]);
    for (size_t i = 0; i < outsign_list.length(); ++i) {
        d_raw_read(fin, buf1, 4); outsign_list[i] = buf1[0];
    }
}

std::string USGCell::toString() const {
    std::string repr = "USGCell(";                          // D:292
    repr += eil::format("id=%d cell_type=%s", id, cell_type_text(cell_type)); // D:293
    repr += eil::format(", vtx_id_list=%s", vtx_id_list);
    repr += eil::format(", face_id_list=%s", face_id_list);
    repr += eil::format(", outsign_list=%s", outsign_list);
    repr += ")";
    return repr;
}

std::string USGCell::toIOString() const {
    // D:304 `cell_names[cell_type]` — an enum index into a 9-element array, so an
    // out-of-range cell_type is D's ArrayIndexError and eil::Array reproduces it.
    std::string str = cell_names[static_cast<int>(cell_type)];
    str += " vtx ";
    str += eil::to_string(vtx_id_list.length());
    for (size_t i = 0; i < vtx_id_list.length(); ++i) { str += " "; str += eil::to_string(vtx_id_list[i]); }
    str += " faces ";
    str += eil::to_string(face_id_list.length());
    for (size_t i = 0; i < face_id_list.length(); ++i) { str += " "; str += eil::to_string(face_id_list[i]); }
    str += " outsigns ";
    str += eil::to_string(outsign_list.length());
    for (size_t i = 0; i < outsign_list.length(); ++i) { str += " "; str += eil::to_string(outsign_list[i]); }
    return str;
}

void USGCell::rawWrite(std::FILE* fout) const {
    int buf1[1] = {0};
    buf1[0] = static_cast<int>(cell_type); d_raw_write(fout, buf1, 4); // D:317
    buf1[0] = static_cast<int>(vtx_id_list.length()); d_raw_write(fout, buf1, 4);
    for (size_t i = 0; i < vtx_id_list.length(); ++i) {
        buf1[0] = static_cast<int>(vtx_id_list[i]); d_raw_write(fout, buf1, 4);
    }
    buf1[0] = static_cast<int>(face_id_list.length()); d_raw_write(fout, buf1, 4);
    for (size_t i = 0; i < face_id_list.length(); ++i) {
        buf1[0] = static_cast<int>(face_id_list[i]); d_raw_write(fout, buf1, 4);
    }
    buf1[0] = static_cast<int>(outsign_list.length()); d_raw_write(fout, buf1, 4);
    for (size_t i = 0; i < outsign_list.length(); ++i) {
        buf1[0] = outsign_list[i]; d_raw_write(fout, buf1, 4);
    }
}
namespace {

// The label the StructuredGrid constructor gives the new grid (D:555, and the same
// expression at D:443 and :524). A helper keeps the base-initialiser order defined:
// the alternative would be two calls whose evaluation order is unspecified.
std::string block_label_for(const std::string& existing, const std::string& new_label) {
    return (new_label == "") ? existing : new_label;
}

// D:518-521 builds the double[][] that PavedGrid's constructor consumes; it is a local
// of usgrid.d's constructor body, so the conversion lives in the body, not here.

} // namespace

// ---- usgrid.d:424-846: the six constructors and dup() ----
// D:434-438 a new empty grid; dimensions is NOT validated (corpus rows us.ctor.d0 and
// us.ctor.dm1 build 0 and -1).
UnstructuredGrid::UnstructuredGrid(int dimensions, const std::string& label)
    : Grid(Grid_t::unstructured_grid, dimensions, label) {}

// D:440-456 copy constructor.
UnstructuredGrid::UnstructuredGrid(const UnstructuredGrid& other, const std::string& new_label)
    : Grid(Grid_t::unstructured_grid, other.dimensions, block_label_for(other.label, new_label)) {
    nvertices = other.nvertices;
    ncells = other.ncells;
    nfaces = other.nfaces;
    nboundaries = other.nboundaries;
    for (size_t i = 0; i < other.vertices.length(); ++i) {
        vertices.append(Vector3(other.vertices[i])); // D:448 Vector3(v) copies the struct
    }
    for (size_t i = 0; i < other.faces.length(); ++i) {
        const USGFace& f = *other.faces[i];
        set_face_index(makeFaceTag(f.vtx_id_list.all()), faces.length()); // D:450
        faces.append(std::make_shared<USGFace>(f));                        // D:451
    }
    for (size_t id = 0; id < other.cells.length(); ++id) {
        cells.append(std::make_shared<USGCell>(*other.cells[id], id)); // D:453
    }
    for (size_t i = 0; i < other.boundaries.length(); ++i) {
        boundaries.append(std::make_shared<BoundaryFaceSet>(*other.boundaries[i])); // D:454
    }
    niv = vertices.length(); njv = 1; nkv = 1;                                      // D:455
}

// D:458-512 paving in 2D from one or more closed boundary point lists.
UnstructuredGrid::UnstructuredGrid(const eil::Array<eil::Array<Vector3>>& bPoints,
                                   const std::string& new_label, double tol)
    : Grid(Grid_t::unstructured_grid, 2, new_label) {
    // D:466-470 the first check is made against bPoints[0][0] and the last point of the
    // LAST list; both index operations are D array reads, so an empty bPoints is
    // ArrayIndexError here (corpus us.pave.empty).
    // D evaluates its arguments left to right, so bPoints[0][0] is read BEFORE
    // bPoints[$-1][$-1]: an empty outer list (corpus row 695) and an empty FIRST list
    // (row 697) both report index [0], while an empty LAST list reports the underflowed
    // index [18446744073709551615] (row 696). C++ leaves argument order unspecified, so
    // the two reads are sequenced explicitly here.
    const Vector3& first_pt = bPoints[0][0];
    const eil::Array<Vector3>& last_list = bPoints[bPoints.length() - 1];
    const Vector3& last_pt = last_list[last_list.length() - 1];
    if (distance_between(first_pt, last_pt) > tol) {
        const std::string msg = "Beginning point and end point do not coincide: " +
            eil::to_string(first_pt.toString()) + " " + eil::to_string(last_pt.toString());
        throw eil::ErrorBase(msg); // D:469 bare Error: no file/line (T-922)
    }
    eil::Array<size_t> bndry_vtx_ids;
    for (size_t i = 0; i < bPoints.length(); ++i) {
        const eil::Array<Vector3>& bpa = bPoints[i];
        eil::Array<size_t> face_id_list;
        eil::Array<int> outsign_list;
        if (i == 0) {
            vertices.append(Vector3(bpa[0]));
            bndry_vtx_ids.append(vertices.length() - 1);
        }
        for (size_t j = 1; j < bpa.length(); ++j) {
            vertices.append(Vector3(bpa[j]));
            bndry_vtx_ids.append(vertices.length() - 1);
            // Also, build a face object on the boundary.
            eil::Array<size_t> vtx_id_list = {vertices.length() - 2, vertices.length() - 1};
            std::string faceTag = makeFaceTag(vtx_id_list.all());
            set_face_index(faceTag, faces.length()); // before new face (D:485)
            faces.append(std::make_shared<USGFace>(vtx_id_list.all()));
            face_id_list.append(faces.length() - 1);
            outsign_list.append(1); // always out for a counter-clockwise vertex order
        }
        boundaries.append(std::make_shared<BoundaryFaceSet>(
            "bndry-" + eil::to_string(i), face_id_list.all(), outsign_list.all())); // D:490
        if (i > 0) {
            if (distance_between(bPoints[i][0],
                                 bPoints[i - 1][bPoints[i - 1].length() - 1]) > tol) {
                const std::string msg =
                    "Beginning point for segment[" + eil::to_string(i) + "] and end point " +
                    "for segment [" + eil::to_string(i - 1) + "] do not coincide: " +
                    eil::to_string(bPoints[i][0].toString()) + " " +
                    eil::to_string(bPoints[i - 1][bPoints[i - 1].length() - 1].toString());
                throw eil::ErrorBase(msg); // D:496 bare Error
            }
        }
    }
    // At this point, we should have a closed region defined by its bounding polygon.
    // Need to fill in interior points, faces and cells.
    eil::geom::fill_interior(vertices, faces, faceIndices, cells, bndry_vtx_ids); // D:502
    // fill_interior (src/geom/grid/paver2d.hpp:178-182, stage G3's file) inserts into
    // faceIndices itself and cannot be routed through set_face_index, so the key log is
    // reconciled here in the order that function performs its inserts.
    for (size_t k = 0; k < bndry_vtx_ids.length(); ++k) {
        eil::Array<size_t> two = {bndry_vtx_ids[k], vertices.length() - 1};
        const std::string tag = makeFaceTag(two.all());
        if (!faceIndices.contains(tag)) faceIndices_insertion.append(tag);
    }
    niv = vertices.length(); njv = 1; nkv = 1;                                    // D:506
    nvertices = vertices.length();
    nfaces = faces.length();
    ncells = cells.length();
    nboundaries = boundaries.length();
    sort_faces_in_boundary_order();                                               // D:511
}

std::shared_ptr<UnstructuredGrid> UnstructuredGrid::dup() const {
    return std::make_shared<UnstructuredGrid>(*this); // D:848-851
}

// ---- usgrid.d:857-1043: indexing and the cell-type table ----
// D:857 opIndex(i,j,k). The three in-contract asserts at D:860-862 are NOT ported:
// they are dead in D because this is an override of an abstract member (see the file
// header note); the only failure D produces here is the vertices[i] index in the body.
Vector3* UnstructuredGrid::opIndex(size_t i, size_t j, size_t k) {
    (void)j; (void)k;
    return &(vertices[i]); // D:865
}

Vector3* UnstructuredGrid::opIndex(size_t indx) {
    return &(vertices[indx]); // D:872
}

std::string UnstructuredGrid::toString() const {
    // D:875-886. Every element of `boundaries` goes through its own toString (D:883
    // formats the whole array), and a D `%s` array renders as "[a, b]".
    std::string repr = "UnstructuredGrid(";
    repr += eil::format("dimensions=%d, nvertices=%d, nfaces=%d, ncells=%d", dimensions,
                        nvertices, nfaces, ncells);
    // D's order of evaluation is part of the observable behaviour here: on an EMPTY grid
    // D reads vertices[0] first and reports `index [0] is out of bounds for array of
    // length 0` (corpus rows 267-282, us.ctor.d{1,2,3,0,-1,7}.tostring, raised at
    // usgrid.d:880). C++ leaves argument order unspecified and g++ evaluates right to
    // left, which reads vertices[length-1] first and reports the underflowed
    // 18446744073709551615 instead. Same for faces (D:881) and cells (D:882).
    const std::string v_first = vertices[0].toString();
    const std::string v_last = vertices[vertices.length() - 1].toString();
    repr += eil::format(", vertices=[%s ... %s]", v_first, v_last);
    const std::string f_first = faces[0]->toString();
    const std::string f_last = faces[faces.length() - 1]->toString();
    repr += eil::format(", faces=[%s ... %s]", f_first, f_last);
    const std::string c_first = cells[0]->toString();
    const std::string c_last = cells[cells.length() - 1]->toString();
    repr += eil::format(", cells=[%s ... %s]", c_first, c_last);
    std::string b = "[";
    for (size_t i = 0; i < boundaries.length(); ++i) {
        if (i > 0) b += ", ";
        b += boundaries[i]->toString();
    }
    b += "]";
    repr += eil::format(", boundaries=%s", b);
    repr += ")";
    return repr;
}

// D:888-991. In-place face sort: interior faces first, then the boundary sets in the
// order they appear in `boundaries`. Four bookkeeping sites per swap, and D:974 swaps
// the references.
void UnstructuredGrid::sort_faces_in_boundary_order() {
    eil::AA<size_t, size_t> faceidx_to_boundary_table;     // D:907
    eil::AA<size_t, size_t> faceidx_to_boundary_idx_table; // D:908

    size_t nboundaryfaces = 0;
    for (size_t i = 0; i < boundaries.length(); ++i) {
        const BoundaryFaceSet& bfs = *boundaries[i];
        for (size_t j = 0; j < bfs.face_id_list.length(); ++j) {
            const size_t faceid = bfs.face_id_list[j];
            faces[faceid]->is_on_boundary = true;                    // D:914
            faceidx_to_boundary_table.set(faceid, i);                // D:915
            faceidx_to_boundary_idx_table.set(faceid, j);            // D:916
            nboundaryfaces += 1;
        }
    }

    ninteriorfaces = nfaces - nboundaryfaces; // D:921 (a size_t subtraction; D:1844...
                                              // underflows when a boundary set names
                                              // more faces than the grid has — corpus
                                              // row us.gap.963)

    size_t nswaps = 0;
    size_t j = nfaces - 1;
    for (size_t i = 0; i < ninteriorfaces; ++i) {
        if (!faces[i]->is_on_boundary) continue;

        while (faces[j]->is_on_boundary) { j -= 1; } // D:933

        // 1.) the faceIndices map from vertex ids to face ids (D:941-944)
        std::string ifaceTag = makeFaceTag(faces[i]->vtx_id_list.all());
        std::string jfaceTag = makeFaceTag(faces[j]->vtx_id_list.all());
        set_face_index(ifaceTag, j);
        set_face_index(jfaceTag, i);

        // 2.) the cells' face_id_list entries; D:949's countUntil can come back -1,
        // which is the 18446744073709551615 index the corpus records at us.gap.963.
        size_t il = 0, ir = 0, jl = 0, jr = 0;
        if (faces[i]->left_cell)  il = d_count_until(faces[i]->left_cell->face_id_list.all(), i);
        if (faces[i]->right_cell) ir = d_count_until(faces[i]->right_cell->face_id_list.all(), i);
        if (faces[j]->left_cell)  jl = d_count_until(faces[j]->left_cell->face_id_list.all(), j);
        if (faces[j]->right_cell) jr = d_count_until(faces[j]->right_cell->face_id_list.all(), j);

        if (faces[i]->left_cell)  faces[i]->left_cell->face_id_list[il] = j;
        if (faces[i]->right_cell) faces[i]->right_cell->face_id_list[ir] = j;
        if (faces[j]->left_cell)  faces[j]->left_cell->face_id_list[jl] = i;
        if (faces[j]->right_cell) faces[j]->right_cell->face_id_list[jr] = i;

        // 3.) the boundary set that records i (D:961-964)
        const size_t ibfs = faceidx_to_boundary_table.get(i);
        const size_t ibfsidx = faceidx_to_boundary_idx_table.get(i);
        eil_assert(boundaries[ibfs]->face_id_list[ibfsidx] == i); // D:963, bare assert
        boundaries[ibfs]->face_id_list[ibfsidx] = j;

        // 4.) the tables themselves, for the face that now sits at j (D:970-971)
        faceidx_to_boundary_table.set(j, ibfs);
        faceidx_to_boundary_idx_table.set(j, ibfsidx);

        // 5.) finally, swap the face references (D:974)
        std::swap(faces[i], faces[j]);
        nswaps += 1;
    }
    (void)nswaps; // D:990's writefln of it is commented out in the reference
}

size_t UnstructuredGrid::number_of_vertices_for_cell(size_t i) { // D:993
    return cells[i]->vtx_id_list.length();
}

int UnstructuredGrid::vtk_element_type_for_cell(size_t i) { // D:999
    return vtk_element_types[static_cast<int>(cells[i]->cell_type)];
}

int UnstructuredGrid::get_cell_type(size_t i) { // D:1005
    return static_cast<int>(cells[i]->cell_type);
}

// D:1011-1019. The three in-contract asserts (D:1013-1015, whose texts are built with
// text(...)) are NOT ported — measured dead, and rows us.idx.vtx.badj/badk are
// no-throw: a wrong j or k is simply ignored, exactly as the body shows.
eil::Array<size_t> UnstructuredGrid::get_vtx_id_list_for_cell(size_t i, size_t j,
                                                             size_t k) const {
    (void)j; (void)k;
    return cells[i]->vtx_id_list.dup(); // D:1018
}

void UnstructuredGrid::copy_vtx_id_list_for_cell(std::array<size_t, 8>& vtx_list_copy,
                                                 size_t& nvtx, size_t indx, size_t j,
                                                 size_t k) const {
    (void)j; (void)k;
    copy_vtx_id_list_for_cell(vtx_list_copy, nvtx, indx); // D:1025
}

eil::Array<size_t> UnstructuredGrid::get_vtx_id_list_for_cell(size_t indx) const {
    return cells[indx]->vtx_id_list.dup(); // D:1034
}

void UnstructuredGrid::copy_vtx_id_list_for_cell(std::array<size_t, 8>& vtx_list_copy,
                                                 size_t& nvtx, size_t indx) const {
    nvtx = cells[indx]->vtx_id_list.length();               // D:1041
    for (size_t i = 0; i < nvtx; ++i) vtx_list_copy[i] = cells[indx]->vtx_id_list[i];
}

// D's `faceIndices[tag] = idx`, with the first-appearance order of the keys logged so that
// the bucket order D's AA produces stays reproducible (see usgrid.hpp's d_aa_key_order).
// Re-assigning an existing key changes its value but not its slot, exactly as D does.
void UnstructuredGrid::set_face_index(const std::string& tag, size_t idx) {
    if (!faceIndices.contains(tag)) faceIndices_insertion.append(tag);
    faceIndices.set(tag, idx);
}

// The raw key order D's `foreach (k; faceIndices.byKey)` walks (corpus rows
// us.dup.faceidx.aaord, us.sg2d.fidx.aaord).
eil::Array<std::string> UnstructuredGrid::d_face_indices_raw_key_order() const {
    return d_aa_key_order(faceIndices_insertion);
}

// D:1045-1055. `connectedCells.keys.dup` is the one place where D's associative-array
// bucket order reaches an observable value: corpus row us.conn.cell0 is [0, 4, 1] and
// us.conn.cellast is [7, 10, 11], neither of which is the sorted order a std::map gives.
// The port therefore lays the keys out in druntime's table (d_aa_key_order, validated
// against this ldc2 1.40.0's druntime on 12 key sets) rather than returning
// connectedCells.keys(), and logs first-appearance order because re-setting an existing
// key leaves its slot where it was.
eil::Array<size_t> UnstructuredGrid::get_connected_cell_ids_for_cell(size_t i) {
    eil::AA<size_t, bool> connectedCells;
    eil::Array<size_t> inserted;                  // keys in first-appearance order
    for (size_t k = 0; k < cells[i]->face_id_list.length(); ++k) {
        const size_t iFace = cells[i]->face_id_list[k];
        if (faces[iFace]->left_cell) {
            const size_t id = faces[iFace]->left_cell->id;
            if (!connectedCells.contains(id)) inserted.append(id);
            connectedCells.set(id, true);
        }
        if (faces[iFace]->right_cell) {
            const size_t id = faces[iFace]->right_cell->id;
            if (!connectedCells.contains(id)) inserted.append(id);
            connectedCells.set(id, true);
        }
    }
    return d_aa_key_order(inserted);              // D:1054 `.keys.dup`
}

// D:1057-1104. Returns the grid for one boundary of this grid.
std::shared_ptr<Grid> UnstructuredGrid::get_boundary_grid(size_t boundary_indx) {
    int new_dimensions = dimensions - 1;
    if (new_dimensions < 1 || new_dimensions > 2) {
        // D:1067 GeometryException(format("Invalid new_dimensions=%d", ...)) — carries
        // the throw site (T-922).
        EIL_THROW_GEOM(eil::format("Invalid new_dimensions=%d", new_dimensions));
    }
    std::shared_ptr<BoundaryFaceSet> bfs = boundaries[boundary_indx]; // D:1069 (alias)
    auto new_grid = std::make_shared<UnstructuredGrid>(new_dimensions, bfs->tag);
    // A full copy of the original grid's vertices, so no renumbering is needed.
    new_grid->nvertices = nvertices;                     // D:1073
    set_length_d(new_grid->vertices, nvertices);            // D:1074
    for (size_t i = 0; i < nvertices; ++i) new_grid->vertices[i].set(vertices[i]);
    for (size_t kf = 0; kf < bfs->face_id_list.length(); ++kf) {
        const size_t fid = bfs->face_id_list[kf];
        USGCell_type new_cell_type = USGCell_type::none;
        switch (faces[fid]->vtx_id_list.length()) {
        case 2:
            eil_assert_msg(dimensions == 2, "Assumed 2D but it was not so."); // D:1082
            new_cell_type = USGCell_type::line;
            break;
        case 3:
            eil_assert_msg(dimensions == 3, "Assumed 3D but it was not so."); // D:1086
            new_cell_type = USGCell_type::triangle;
            break;
        case 4:
            eil_assert_msg(dimensions == 3, "Assumed 3D but it was not so."); // D:1090
            new_cell_type = USGCell_type::quad;
            break;
        default:
            EIL_THROW_GEOM("Did not know what to do with this number of vertices."); // D:1094
        }
        eil::Array<size_t> newCell_face_id_list; // empty (D:1096)
        eil::Array<int> newCell_outsign_list;    // empty (D:1097)
        const size_t id = new_grid->cells.length();
        new_grid->cells.append(std::make_shared<USGCell>(
            new_cell_type, id, faces[fid]->vtx_id_list.all(), newCell_face_id_list.all(),
            newCell_outsign_list.all()));
    }
    new_grid->ncells = new_grid->cells.length();
    return new_grid;
}

// D:1106-1130.
eil::Array<size_t> UnstructuredGrid::get_list_of_boundary_cells(size_t boundary_indx) {
    eil::Array<size_t> cellList;
    eil::AA<size_t, size_t> cell_indx;                       // D:1114
    for (size_t i = 0; i < cells.length(); ++i) cell_indx.set(cells[i]->id, i); // D:1115
    std::shared_ptr<BoundaryFaceSet> bfs = boundaries[boundary_indx];
    for (size_t i = 0; i < bfs->face_id_list.length(); ++i) {
        const size_t fid = bfs->face_id_list[i];
        // D:1120: outsign +1 means the left cell is the inside one.
        std::shared_ptr<USGCell> insideCell =
            (bfs->outsign_list[i] == 1) ? faces[fid]->left_cell : faces[fid]->right_cell;
        if (insideCell) {
            cellList.append(cell_indx.get(insideCell->id));
        } else {
            // D:1126 is the ONE plain Exception in the file, so it carries the site.
            const std::string msg =
                eil::format("Oops, while processing boundary faces, "
                            "i=%d, insideCell is null for face.id=%d", i, fid);
            throw eil::ExceptionBase(msg, __FILE__, __LINE__);
        }
    }
    return cellList;
}

// D:1133-1165 vectorAreaOfFacet.
Vector3 UnstructuredGrid::vectorAreaOfFacet(eil::Slice<size_t> vtx_id_list) {
    const size_t nv = vtx_id_list.length();
    eil_assert_msg(nv > 1, "do not have a valid list of vertex ids"); // D:1136
    Vector3 varea = Vector3(0, 0, 0);
    switch (nv) {
    case 2: {
        Vector3 p0 = vertices[vtx_id_list[0]];
        Vector3 p1 = vertices[vtx_id_list[1]];
        varea = cross(p1 - p0, Vector3(0, 0, 1.0)); // unit depth in z for a 2D face
        break;
    }
    case 3: {
        Vector3 p0 = vertices[vtx_id_list[0]];
        Vector3 p1 = vertices[vtx_id_list[1]];
        Vector3 p2 = vertices[vtx_id_list[2]];
        varea = 0.5 * cross(p1 - p0, p2 - p0);
        break;
    }
    default: {
        Vector3 pmid = Vector3(0, 0, 0);
        for (size_t q = 0; q < nv; ++q) pmid += vertices[vtx_id_list[q]];
        pmid /= static_cast<number>(nv);
        for (size_t i = 0; i < nv - 1; ++i) {
            Vector3 p0 = vertices[vtx_id_list[i]];
            Vector3 p1 = vertices[vtx_id_list[i + 1]];
            varea += 0.5 * cross(p1 - p0, pmid - p0);
        }
        Vector3 p0 = vertices[vtx_id_list[nv - 1]];
        Vector3 p1 = vertices[vtx_id_list[0]];
        varea += 0.5 * cross(p1 - p0, pmid - p0);
        break;
    }
    }
    return varea;
}

// D:1167-1174 vectorAreaOfCell.
Vector3 UnstructuredGrid::vectorAreaOfCell(const USGCell& c) {
    Vector3 varea = Vector3(0, 0, 0);
    for (size_t j = 0; j < c.face_id_list.length(); ++j) {
        const size_t fid = c.face_id_list[j];
        varea += static_cast<number>(c.outsign_list[j]) * vectorAreaOfFacet(faces[fid]->vtx_id_list.all());
    }
    return varea;
}

// D:1179-1250 splitTriangleCell.
void UnstructuredGrid::splitTriangleCell(size_t id) {
    eil_assert(id < cells.length()); // D:1181, a bare assert
    if (dimensions != 2) {
        // D:1183-1185: the message is built by ~= and contains real newlines, which the
        // corpus shows as <newline>.
        std::string excMsg = "Exception in UnstructureGrid:splitTriangleCell():\n";
        excMsg += "This method can only be used on 2D grids.\n";
        EIL_THROW_GEOM(excMsg);
    }
    if (cells[id]->cell_type != USGCell_type::triangle) {
        std::string excMsg = "Exception in UnstructureGrid:splitTriangleCell():\n";
        excMsg += "This method can only be used on TRIANGLE cells.\n";
        EIL_THROW_GEOM(excMsg);
    }

    eil::Array<size_t> vtxList = get_vtx_id_list_for_cell(id, 0, 0); // D:1193
    Vector3 p0 = vertices[vtxList[0]];
    Vector3 p1 = vertices[vtxList[1]];
    Vector3 p2 = vertices[vtxList[2]];
    Vector3 centroid = Vector3(0.0, 0.0, 0.0);
    centroid.set((p0.x + p1.x + p2.x) / 3.0, (p0.y + p1.y + p2.y) / 3.0,
                 (p0.z + p1.z + p2.z) / 3.0);
    vertices.append(centroid); // D:1202
    nvertices = vertices.length();
    const size_t ctrId = vertices.length() - 1;

    // D:1206-1216: three new faces, each with its tag recorded in faceIndices.
    eil::Array<size_t> f0 = {vtxList[0], ctrId};
    faces.append(std::make_shared<USGFace>(f0.all()));
    const size_t if0_id = faces.length() - 1;
    set_face_index(makeFaceTag(f0.all()), if0_id);

    eil::Array<size_t> f1 = {vtxList[1], ctrId};
    faces.append(std::make_shared<USGFace>(f1.all()));
    const size_t if1_id = faces.length() - 1;
    set_face_index(makeFaceTag(f1.all()), if1_id);

    eil::Array<size_t> f2 = {vtxList[2], ctrId};
    faces.append(std::make_shared<USGFace>(f2.all()));
    const size_t if2_id = faces.length() - 1;
    set_face_index(makeFaceTag(f2.all()), if2_id);

    nfaces = faces.length();
    // D:1221-1224: the present cell becomes sub-triangle 0.
    cells[id]->vtx_id_list[0] = ctrId;
    cells[id]->face_id_list[0] = if1_id;
    cells[id]->face_id_list[2] = if2_id;
    cells[id]->outsign_list[0] = -1;

    const size_t nc1_id = cells.length();
    eil::Array<size_t> c1v = {vtxList[0], ctrId, vtxList[2]};
    eil::Array<size_t> c1f = {if0_id, if2_id, cells[id]->face_id_list[2]};
    eil::Array<int> c1o = {1, -1, 1};
    cells.append(std::make_shared<USGCell>(USGCell_type::triangle, nc1_id, c1v.all(), c1f.all(),
                                           c1o.all()));

    const size_t nc2_id = cells.length();
    eil::Array<size_t> c2v = {vtxList[0], vtxList[1], ctrId};
    eil::Array<size_t> c2f = {cells[id]->face_id_list[0], if1_id, if0_id};
    eil::Array<int> c2o = {1, 1, -1};
    cells.append(std::make_shared<USGCell>(USGCell_type::triangle, nc2_id, c2v.all(), c2f.all(),
                                           c2o.all()));

    ncells = cells.length();

    // D:1242-1249: the face-to-cell links for the three new faces.
    faces[if0_id]->left_cell = cells[nc1_id];
    faces[if0_id]->right_cell = cells[nc2_id];
    faces[if1_id]->left_cell = cells[nc2_id];
    faces[if1_id]->right_cell = cells[id];
    faces[if2_id]->left_cell = cells[id];
    faces[if2_id]->right_cell = cells[nc1_id];
}

// D:1258-1273.
void UnstructuredGrid::link_faces_back_to_cells() {
    for (size_t ci = 0; ci < cells.length(); ++ci) {
        USGCell& c = *cells[ci];
        const size_t nf = c.face_id_list.length();
        eil_assert_msg(nf == c.outsign_list.length(),
                       "Mismatch in number of faces and number of outsigns"); // D:1263
        for (size_t j = 0; j < nf; ++j) {
            std::shared_ptr<USGFace> f = faces[c.face_id_list[j]]; // D:1265 (handle)
            if (c.outsign_list[j] == 1) {
                f->left_cell = cells[ci];
            } else {
                f->right_cell = cells[ci];
            }
        }
    }
}
// ---- usgrid.d:328-421 BoundaryFaceSet ----
BoundaryFaceSet::BoundaryFaceSet(const std::string& tag, eil::Slice<size_t> face_id_list,
                                 eil::Slice<int> outsign_list) {
    this->tag = tag;
    this->face_id_list = eil::Array<size_t>(face_id_list); // D:337 .dup
    this->outsign_list = eil::Array<int>(outsign_list);    // D:338 .dup
}

BoundaryFaceSet::BoundaryFaceSet(const std::string& str) {
    // D:344-363. One token means a tag-only set (the shape the StructuredGrid
    // constructor uses at D:576); the assert at D:362 is a body assert and fires.
    eil::Array<std::string> tokens = split_ws(d_strip(str));
    if (tokens.length() == 1) {
        this->tag = tokens[0];
    } else {
        size_t itok = 0;
        this->tag = tokens[itok++];
        std::string junk = tokens[itok++]; // "faces"
        (void)junk;
        face_id_list.set_length_signed(d_to_int(tokens[itok++]));
        for (size_t i = 0; i < face_id_list.length(); ++i) {
            face_id_list[i] = static_cast<size_t>(d_to_int(tokens[itok++]));  // D:355 to!int
        }
        junk = tokens[itok++]; // "outsigns"
        outsign_list.set_length_signed(d_to_int(tokens[itok++]));
        for (size_t i = 0; i < outsign_list.length(); ++i) {
            outsign_list[i] = d_to_int(tokens[itok++]);
        }
        eil_assert_msg(face_id_list.length() == outsign_list.length(),
                       "Mismatch in numbers of faces and outsigns");
    }
}

BoundaryFaceSet::BoundaryFaceSet(std::FILE* fin) {
    // D:369-379. A tag length of 0 leaves `tag` at its empty default (the D:371 branch
    // the corpus exercises as us.raw.zerotag).
    int buf1[1] = {0};
    d_raw_read(fin, buf1, 4);
    const int tag_length = buf1[0];
    if (tag_length > 0) {
        std::vector<char> found_tag(static_cast<size_t>(tag_length), '\0');
        d_raw_read(fin, found_tag.data(), static_cast<size_t>(tag_length));
        tag = std::string(found_tag.data(), static_cast<size_t>(tag_length)); // to!string
    }
    d_raw_read(fin, buf1, 4); face_id_list.set_length_signed(buf1[0]);
    for (size_t i = 0; i < face_id_list.length(); ++i) {
        d_raw_read(fin, buf1, 4); face_id_list[i] = static_cast<size_t>(buf1[0]);
    }
    d_raw_read(fin, buf1, 4); outsign_list.set_length_signed(buf1[0]);
    for (size_t i = 0; i < outsign_list.length(); ++i) {
        d_raw_read(fin, buf1, 4); outsign_list[i] = buf1[0];
    }
}

BoundaryFaceSet::BoundaryFaceSet(const BoundaryFaceSet& other) {
    tag = other.tag;
    face_id_list = other.face_id_list.dup();
    outsign_list = other.outsign_list.dup();
}

std::string BoundaryFaceSet::toString() const {
    // D:393-394 index face_id_list[0]/[length-1] and the outsigns, so a tag-only set
    // throws ArrayIndexError here — measured, corpus row us.bfs.tagonly.tostr.
    std::string repr = "BoundaryFaceSet(";
    repr += eil::format("tag=\"%s\"", tag);
    // D's own order of evaluation: face_id_list[0] is read (and can throw) before
    // face_id_list[$-1]. C++ leaves argument order unspecified and g++ evaluates
    // right-to-left, which reports the underflowed index instead (measured at corpus row
    // us.bfs.tagonly.tostring: D gives index [0] on a list of length 0).
    const size_t f_first = face_id_list[0];
    const size_t f_last = face_id_list[face_id_list.length() - 1];
    // D:332 declares outsign_list as `int[]` (a face normal points out for +1 and in for
    // -1) and D:394 prints it with %d, so a -1 prints as "-1". Reading it into a size_t
    // here rendered -1 as 18446744073709551615 (corpus rows 312-314).
    const int o_first = outsign_list[0];
    const int o_last = outsign_list[outsign_list.length() - 1];
    repr += eil::format(", face_id_list=[%d ... %d]", f_first, f_last);
    repr += eil::format(", outsign_list=[%d ... %d]", o_first, o_last);
    repr += ")";
    return repr;
}

std::string BoundaryFaceSet::toIOString() const {
    std::string str = tag;
    str += " faces ";
    str += eil::to_string(face_id_list.length());
    for (size_t i = 0; i < face_id_list.length(); ++i) { str += " "; str += eil::to_string(face_id_list[i]); }
    str += " outsigns ";
    str += eil::to_string(outsign_list.length());
    for (size_t i = 0; i < outsign_list.length(); ++i) { str += " "; str += eil::to_string(outsign_list[i]); }
    return str;
}

void BoundaryFaceSet::rawWrite(std::FILE* fout) const {
    int buf1[1] = {0};
    buf1[0] = static_cast<int>(tag.length()); d_raw_write(fout, buf1, 4); // D:413
    d_raw_write(fout, tag.data(), tag.size());                             // D:414
    buf1[0] = static_cast<int>(face_id_list.length()); d_raw_write(fout, buf1, 4);
    for (size_t i = 0; i < face_id_list.length(); ++i) {
        buf1[0] = static_cast<int>(face_id_list[i]); d_raw_write(fout, buf1, 4);
    }
    buf1[0] = static_cast<int>(outsign_list.length()); d_raw_write(fout, buf1, 4);
    for (size_t i = 0; i < outsign_list.length(); ++i) {
        buf1[0] = outsign_list[i]; d_raw_write(fout, buf1, 4);
    }
}
// ---- usgrid.d:514-551 Heather Muir's PavedGrid constructor ----
// D:522 constructs the PavedGrid BEFORE super() runs, because the base initialiser
// reads grid.dimensions and grid.label. A C++ base subobject initialises first, so the
// two fields super() would have set are assigned again at the top of the body, from the
// same object: Grid's constructor only stores its three arguments (grid.d:62-67), so the
// re-ordering is not observable, and PavedGrid is still constructed exactly once — which
// matters, because paver.d's constructor appends to the module-level POINT_LIST/
// FACE_LIST/CELL_LIST (src/geom/grid/paver.hpp:194-197) and those lists are what the
// corpus rows us.paved.* count.
UnstructuredGrid::UnstructuredGrid(const eil::Array<Vector3>& boundary,
                                   const eil::Array<std::shared_ptr<BoundaryFaceSet>>& in_boundaries,
                                   const std::string& new_label)
    : Grid(Grid_t::unstructured_grid, 2, "") {
    eil::Array<eil::Array<double>> boundary_points; // D:518-521
    for (size_t i = 0; i < boundary.length(); ++i) {
        const Vector3& p = boundary[i];
        eil::Array<double> v = {eil::re(p.x), eil::re(p.y), eil::re(p.z)};
        boundary_points.append(v);
    }
    PavedGrid grid(boundary_points); // D:522 — the super() arguments come from here
    grid_type = Grid_t::unstructured_grid;
    dimensions = grid.dimensions;                                          // D:523
    label = block_label_for(grid.label, new_label);                        // D:524

    for (size_t i = 0; i < in_boundaries.length(); ++i) {
        boundaries.append(std::make_shared<BoundaryFaceSet>(*in_boundaries[i])); // D:526
    }
    nvertices = grid.nvertices;   // D:527-530
    ncells = grid.ncells;
    nfaces = grid.nfaces;
    nboundaries = grid.nboundaries;
    for (size_t i = 0; i < POINT_LIST.length(); ++i) { // D:531-534
        const Point& p = *POINT_LIST[i];
        eil::Array<number> v = {p.x, p.y, p.z};
        vertices.append(Vector3(v.all()));
    }
    for (size_t i = 0; i < FACE_LIST.length(); ++i) { // D:535-539
        eil::Array<size_t> vtx_id_list = {FACE_LIST[i]->point_IDs[0], FACE_LIST[i]->point_IDs[1]};
        set_face_index(makeFaceTag(vtx_id_list.all()), faces.length());
        faces.append(std::make_shared<USGFace>(vtx_id_list.all()));
    }
    for (size_t id = 0; id < CELL_LIST.length(); ++id) { // D:540-548
        CELL_LIST[id]->auto_cell_type();
        if (CELL_LIST[id]->cell_type != "quad") {
            // D:543 text("paver generated a non-quad cell") — a bare Error, no site.
            throw eil::ErrorBase(eil::text("paver generated a non-quad cell"));
        } else {
            USGCell_type cell_type = cell_type_from_name(CELL_LIST[id]->cell_type);
            cells.append(std::make_shared<USGCell>(
                cell_type, id, CELL_LIST[id]->point_IDs.all(), CELL_LIST[id]->face_IDs.all(),
                CELL_LIST[id]->outsigns.all()));
        }
    }
    niv = vertices.length(); njv = 1; nkv = 1; // D:549
    sort_faces_in_boundary_order();            // D:550
}

// ---- usgrid.d:553-827 construction from a StructuredGrid ----
// This is the direction of the sgrid/usgrid cycle that forces the two modules to be one
// work package (grid brief section 10.1): the body reads sg.niv/njv/nkv/vertices/bcTags,
// which needs src/geom/grid/sgrid.hpp — included above.
UnstructuredGrid::UnstructuredGrid(const StructuredGrid& sg, const std::string& new_label)
    : Grid(Grid_t::unstructured_grid, sg.dimensions, block_label_for(sg.label, new_label)) {
    const std::string my_block_label = label; // D:555 (the same expression super() used)
    set_length_d(vertices, 0);                   // D:557-560
    set_length_d(faces, 0);
    set_length_d(cells, 0);
    set_length_d(boundaries, 0);
    if (dimensions == 2) {
        nvertices = sg.niv * sg.njv;                                          // D:562
        nfaces = (sg.niv) * (sg.njv - 1) + (sg.niv - 1) * (sg.njv);            // D:563
        ncells = (sg.niv - 1) * (sg.njv - 1);                                  // D:564
        nboundaries = 4;                                                       // D:565
        for (size_t ib = 0; ib < nboundaries; ++ib) {                          // D:566-577
            std::string boundary_tag;
            if (sg.bcTags[ib].length() > 0) {
                boundary_tag = sg.bcTags[ib];
            } else {
                if (my_block_label.length() > 0) boundary_tag = my_block_label + "-";
                boundary_tag += face_name[ib]; // 0=west, 1=east, 2=south, 3=north
            }
            boundaries.append(std::make_shared<BoundaryFaceSet>(boundary_tag));
        }
        eil::Array<eil::Array<size_t>> vtx_id;                                 // D:579-583
        set_length_d(vtx_id, sg.niv);
        for (size_t i = 0; i < sg.niv; ++i) set_length_d(vtx_id[i], sg.njv);
        for (size_t j = 0; j < sg.njv; ++j) {                                 // D:585-590
            for (size_t i = 0; i < sg.niv; ++i) {
                vertices.append(Vector3(sg.vertices[sg.single_index(i, j)]));
                vtx_id[i][j] = vertices.length() - 1;
            }
        }
        eil::Array<eil::Array<size_t>> iface_id;                               // D:594-598
        set_length_d(iface_id, sg.niv);
        for (size_t i = 0; i < sg.niv; ++i) set_length_d(iface_id[i], sg.njv - 1);
        for (size_t j = 0; j < sg.njv - 1; ++j) {                              // D:599-614
            for (size_t i = 0; i < sg.niv; ++i) {
                eil::Array<size_t> vtx_id_list = {vtx_id[i][j], vtx_id[i][j + 1]};
                set_face_index(makeFaceTag(vtx_id_list.all()), faces.length());
                faces.append(std::make_shared<USGFace>(vtx_id_list.all()));
                iface_id[i][j] = faces.length() - 1;
                if (i == 0) {
                    boundaries[Face::west]->face_id_list.append(iface_id[i][j]);
                    boundaries[Face::west]->outsign_list.append(-1);
                }
                if (i == sg.niv - 1) {
                    boundaries[Face::east]->face_id_list.append(iface_id[i][j]);
                    boundaries[Face::east]->outsign_list.append(+1);
                }
            }
        }
        eil::Array<eil::Array<size_t>> jface_id;                               // D:616-620
        set_length_d(jface_id, sg.niv - 1);
        for (size_t i = 0; i < sg.niv - 1; ++i) set_length_d(jface_id[i], sg.njv);
        for (size_t j = 0; j < sg.njv; ++j) {                                  // D:621-636
            for (size_t i = 0; i < sg.niv - 1; ++i) {
                eil::Array<size_t> vtx_id_list = {vtx_id[i + 1][j], vtx_id[i][j]};
                set_face_index(makeFaceTag(vtx_id_list.all()), faces.length());
                faces.append(std::make_shared<USGFace>(vtx_id_list.all()));
                jface_id[i][j] = faces.length() - 1;
                if (j == 0) {
                    boundaries[Face::south]->face_id_list.append(jface_id[i][j]);
                    boundaries[Face::south]->outsign_list.append(-1);
                }
                if (j == sg.njv - 1) {
                    boundaries[Face::north]->face_id_list.append(jface_id[i][j]);
                    boundaries[Face::north]->outsign_list.append(+1);
                }
            }
        }
        for (size_t j = 0; j < sg.njv - 1; ++j) {                              // D:638-661
            for (size_t i = 0; i < sg.niv - 1; ++i) {
                eil::Array<size_t> cell_vertices = {vtx_id[i][j], vtx_id[i + 1][j],
                                                    vtx_id[i + 1][j + 1], vtx_id[i][j + 1]};
                // Clockwise around the cell, so faces 0 and 1 are nearly orthogonal.
                eil::Array<size_t> cell_faces = {jface_id[i][j + 1], iface_id[i + 1][j],
                                                 jface_id[i][j], iface_id[i][j]};
                eil::Array<int> outsigns = {+1, +1, -1, -1};
                const size_t id = cells.length();
                std::shared_ptr<USGCell> my_cell = std::make_shared<USGCell>(
                    USGCell_type::quad, id, cell_vertices.all(), cell_faces.all(), outsigns.all());
                cells.append(my_cell);
                faces[iface_id[i][j]]->right_cell = my_cell;
                faces[iface_id[i + 1][j]]->left_cell = my_cell;
                faces[jface_id[i][j]]->right_cell = my_cell;
                faces[jface_id[i][j + 1]]->left_cell = my_cell;
            }
        }
    } else {
        // D:662: "Assume dimensions == 3" — a 1D lattice falls in here and reads
        // bcTags[2] of a 2-element array (corpus rows us.sgbad.*).
        nvertices = sg.niv * sg.njv * sg.nkv;                                   // D:664
        nfaces = (sg.niv) * (sg.njv - 1) * (sg.nkv - 1) +                       // D:665-667
                 (sg.niv - 1) * (sg.njv) * (sg.nkv - 1) +
                 (sg.niv - 1) * (sg.njv - 1) * (sg.nkv);
        ncells = (sg.niv - 1) * (sg.njv - 1) * (sg.nkv - 1);                     // D:668
        nboundaries = 6;                                                         // D:669
        for (size_t ib = 0; ib < nboundaries; ++ib) {                             // D:670-681
            std::string boundary_tag;
            if (sg.bcTags[ib].length() > 0) {
                boundary_tag = sg.bcTags[ib];
            } else {
                if (my_block_label.length() > 0) boundary_tag = my_block_label + "-";
                boundary_tag += face_name[ib]; // 0=west..5=top
            }
            boundaries.append(std::make_shared<BoundaryFaceSet>(boundary_tag));
        }
        eil::Array<eil::Array<eil::Array<size_t>>> vtx_id;                       // D:683-690
        set_length_d(vtx_id, sg.niv);
        for (size_t i = 0; i < sg.niv; ++i) {
            set_length_d(vtx_id[i], sg.njv);
            for (size_t j = 0; j < sg.njv; ++j) set_length_d(vtx_id[i][j], sg.nkv);
        }
        for (size_t k = 0; k < sg.nkv; ++k) {                                    // D:692-698
            for (size_t j = 0; j < sg.njv; ++j) {
                for (size_t i = 0; i < sg.niv; ++i) {
                    vertices.append(Vector3(sg.vertices[sg.single_index(i, j, k)]));
                    vtx_id[i][j][k] = vertices.length() - 1;
                }
            }
        }
        eil::Array<eil::Array<eil::Array<size_t>>> iface_id;                     // D:704-711
        set_length_d(iface_id, sg.niv);
        for (size_t i = 0; i < sg.niv; ++i) {
            set_length_d(iface_id[i], sg.njv - 1);
            for (size_t j = 0; j < sg.njv - 1; ++j) set_length_d(iface_id[i][j], sg.nkv - 1);
        }
        for (size_t k = 0; k < sg.nkv - 1; ++k) {                                // D:712-730
            for (size_t j = 0; j < sg.njv - 1; ++j) {
                for (size_t i = 0; i < sg.niv; ++i) {
                    eil::Array<size_t> vtx_id_list = {vtx_id[i][j][k], vtx_id[i][j + 1][k],
                                                      vtx_id[i][j + 1][k + 1], vtx_id[i][j][k + 1]};
                    set_face_index(makeFaceTag(vtx_id_list.all()), faces.length());
                    faces.append(std::make_shared<USGFace>(vtx_id_list.all()));
                    iface_id[i][j][k] = faces.length() - 1;
                    if (i == 0) {
                        boundaries[Face::west]->face_id_list.append(iface_id[i][j][k]);
                        boundaries[Face::west]->outsign_list.append(-1);
                    }
                    if (i == sg.niv - 1) {
                        boundaries[Face::east]->face_id_list.append(iface_id[i][j][k]);
                        boundaries[Face::east]->outsign_list.append(+1);
                    }
                }
            }
        }
        eil::Array<eil::Array<eil::Array<size_t>>> jface_id;                     // D:732-739
        set_length_d(jface_id, sg.niv - 1);
        for (size_t i = 0; i < sg.niv - 1; ++i) {
            set_length_d(jface_id[i], sg.njv);
            for (size_t j = 0; j < sg.njv; ++j) set_length_d(jface_id[i][j], sg.nkv - 1);
        }
        for (size_t k = 0; k < sg.nkv - 1; ++k) {                                // D:740-758
            for (size_t j = 0; j < sg.njv; ++j) {
                for (size_t i = 0; i < sg.niv - 1; ++i) {
                    eil::Array<size_t> vtx_id_list = {vtx_id[i + 1][j][k], vtx_id[i][j][k],
                                                      vtx_id[i][j][k + 1], vtx_id[i + 1][j][k + 1]};
                    set_face_index(makeFaceTag(vtx_id_list.all()), faces.length());
                    faces.append(std::make_shared<USGFace>(vtx_id_list.all()));
                    jface_id[i][j][k] = faces.length() - 1;
                    if (j == 0) {
                        boundaries[Face::south]->face_id_list.append(jface_id[i][j][k]);
                        boundaries[Face::south]->outsign_list.append(-1);
                    }
                    if (j == sg.njv - 1) {
                        boundaries[Face::north]->face_id_list.append(jface_id[i][j][k]);
                        boundaries[Face::north]->outsign_list.append(+1);
                    }
                }
            }
        }
        eil::Array<eil::Array<eil::Array<size_t>>> kface_id;                     // D:760-767
        set_length_d(kface_id, sg.niv - 1);
        for (size_t i = 0; i < sg.niv - 1; ++i) {
            set_length_d(kface_id[i], sg.njv - 1);
            for (size_t j = 0; j < sg.njv - 1; ++j) set_length_d(kface_id[i][j], sg.nkv);
        }
        for (size_t k = 0; k < sg.nkv; ++k) {                                    // D:768-786
            for (size_t j = 0; j < sg.njv - 1; ++j) {
                for (size_t i = 0; i < sg.niv - 1; ++i) {
                    eil::Array<size_t> vtx_id_list = {vtx_id[i][j][k], vtx_id[i + 1][j][k],
                                                      vtx_id[i + 1][j + 1][k], vtx_id[i][j + 1][k]};
                    set_face_index(makeFaceTag(vtx_id_list.all()), faces.length());
                    faces.append(std::make_shared<USGFace>(vtx_id_list.all()));
                    kface_id[i][j][k] = faces.length() - 1;
                    if (k == 0) {
                        boundaries[Face::bottom]->face_id_list.append(kface_id[i][j][k]);
                        boundaries[Face::bottom]->outsign_list.append(-1);
                    }
                    if (k == sg.nkv - 1) {
                        boundaries[Face::top]->face_id_list.append(kface_id[i][j][k]);
                        boundaries[Face::top]->outsign_list.append(+1);
                    }
                }
            }
        }
        for (size_t k = 0; k < sg.nkv - 1; ++k) {                                // D:788-819
            for (size_t j = 0; j < sg.njv - 1; ++j) {
                for (size_t i = 0; i < sg.niv - 1; ++i) {
                    eil::Array<size_t> cell_vertices = {
                        vtx_id[i][j][k],         vtx_id[i + 1][j][k],
                        vtx_id[i + 1][j + 1][k], vtx_id[i][j + 1][k],
                        vtx_id[i][j][k + 1],     vtx_id[i + 1][j][k + 1],
                        vtx_id[i + 1][j + 1][k + 1], vtx_id[i][j + 1][k + 1]};
                    eil::Array<size_t> cell_faces = {
                        jface_id[i][j + 1][k], iface_id[i + 1][j][k], // north, east
                        jface_id[i][j][k],     iface_id[i][j][k],     // south, west
                        kface_id[i][j][k + 1], kface_id[i][j][k]};    // top, bottom
                    eil::Array<int> outsigns = {+1, +1, -1, -1, +1, -1};
                    const size_t id = cells.length();
                    std::shared_ptr<USGCell> my_cell = std::make_shared<USGCell>(
                        USGCell_type::hexahedron, id, cell_vertices.all(), cell_faces.all(),
                        outsigns.all());
                    cells.append(my_cell);
                    faces[iface_id[i][j][k]]->right_cell = my_cell;
                    faces[iface_id[i + 1][j][k]]->left_cell = my_cell;
                    faces[jface_id[i][j][k]]->right_cell = my_cell;
                    faces[jface_id[i][j + 1][k]]->left_cell = my_cell;
                    faces[kface_id[i][j][k]]->right_cell = my_cell;
                    faces[kface_id[i][j][k + 1]]->left_cell = my_cell;
                }
            }
        }
    } // end if dimensions
    eil_assert_msg(nvertices == vertices.length(), "mismatch in number of vertices"); // D:821
    eil_assert_msg(nfaces == faces.length(), "mismatch in number of faces");           // D:822
    eil_assert_msg(ncells == cells.length(), "mismatch in number of cells");           // D:823
    niv = vertices.length(); njv = 1; nkv = 1;                                          // D:824
    sort_faces_in_boundary_order();                                                     // D:826
}

// ---- usgrid.d:830-846 construction by importing a file ----
UnstructuredGrid::UnstructuredGrid(const std::string& fileName, const std::string& fmt,
                                   bool true_centroids, double scale,
                                   bool expect_gmsh_order_for_wedges,
                                   const std::string& new_label)
    : Grid(Grid_t::unstructured_grid, 0, new_label) {
    // D:835-842 switch on the format name. `dimensions will be reset on reading grid`.
    if (fmt == "gziptext") {
        read_from_gzip_file(fileName, scale);
    } else if (fmt == "rawbinary") {
        read_from_raw_binary_file(fileName, scale);
    } else if (fmt == "su2text") {
        read_from_su2_file(fileName, true_centroids, scale, expect_gmsh_order_for_wedges);
    } else if (fmt == "vtktext") {
        read_from_vtk_text_file(fileName, scale);
    } else if (fmt == "vtkxml") {
        throw eil::ErrorBase("Reading from VTK XML format not implemented."); // D:840
    } else {
        throw eil::ErrorBase("Import an UnstructuredGrid, unknown format: " + fmt); // D:841
    }
    if (new_label != "") label = new_label; // D:843
    niv = vertices.length(); njv = 1; nkv = 1;
    sort_faces_in_boundary_order(); // D:845
}
namespace {

// D's `File` text reader with the two pieces usgrid.d uses: readln() and the eof
// property, which D resets at a rewind (D:1411, :1477 search the file again from the
// start). The flag is D's "the buffered range is exhausted" rather than C's feof, which
// only becomes true after a read past the end.
struct DTextReader {
    std::FILE* f = nullptr;
    bool at_eof = false;
    std::string readln() { return d_readln(f, at_eof); }
    void do_rewind() { std::rewind(f); at_eof = false; }
};

// stdout, the way usgrid.d drives it: D:1390 writef, D:1417/:1483/:1632
// stdout.write(".") + stdout.flush().
template <typename... Args> void d_stdout_writef(const std::string& fmt, const Args&... args) {
    const std::string s = eil::format(fmt, args...);
    std::fwrite(s.data(), 1, s.size(), stdout);
}
void d_stdout_write(const std::string& s) {
    std::fwrite(s.data(), 1, s.size(), stdout); // D:1417 stdout.write(".")
}
void d_stdout_flush() { std::fflush(stdout); }

// D:1401 `line.split("=")` — a literal separator, empty fields KEPT (unlike split()).
eil::Array<std::string> split_on(const std::string& s, const std::string& sep) {
    eil::Array<std::string> out;
    size_t start = 0;
    while (true) {
        const size_t at = s.find(sep, start);
        if (at == std::string::npos) { out.append(s.substr(start)); break; }
        out.append(s.substr(start, at - start));
        start = at + sep.size();
    }
    return out;
}

} // namespace

// ---- usgrid.d:1275-1328 read_from_gzip_file ----
// This, with the three text constructors above, is the Eilmer4 native format.
void UnstructuredGrid::read_from_gzip_file(const std::string& fileName, double scale) {
    eil::gzip::GzipByLine byLine(fileName); // D:1282 `new GzipByLine(fileName)`
    auto next_line = [&byLine]() {
        const std::string l = byLine.front(); // D:1283 `byLine.front`
        byLine.popFront();
        return l;
    };
    std::string line = next_line();
    std::string format_version;
    d_formatted_read(line, "unstructured_grid %s", &format_version); // D:1285
    if (format_version != "1.0") {
        // D:1287 a bare Error, message text copied byte for byte.
        throw eil::ErrorBase("UnstructuredGrid.read_from_gzip_file(): "
                             "format version found: " + format_version);
    }
    line = next_line();
    d_formatted_read(line, "label: %s", &label);          // D:1291
    line = next_line();
    d_formatted_read(line, "dimensions: %d", &dimensions); // D:1293
    line = next_line();
    d_formatted_read(line, "vertices: %d", &nvertices);    // D:1295
    double x = 0.0, y = 0.0, z = 0.0;
    set_length_d(vertices, 0);                                // D:1297
    for (size_t i = 0; i < nvertices; ++i) {
        line = next_line();
        d_formatted_read(line, " %g %g %g", &x, &y, &z); // D:1301 (the line starts with ws)
        vertices.append(Vector3(scale * x, scale * y, scale * z));
    }
    line = next_line();
    d_formatted_read(line, "faces: %d", &nfaces);   // D:1305
    set_length_d(faces, 0);                            // D:1306
    for (size_t i = 0; i < nfaces; ++i) {
        line = next_line();
        std::shared_ptr<USGFace> myFace = std::make_shared<USGFace>(line); // D:1309
        set_face_index(makeFaceTag(myFace->vtx_id_list.all()), faces.length());
        faces.append(myFace);
    }
    line = next_line();
    d_formatted_read(line, "cells: %d", &ncells); // D:1314
    set_length_d(cells, 0);
    for (size_t i = 0; i < ncells; ++i) {
        line = next_line();
        cells.append(std::make_shared<USGCell>(line, i)); // D:1318
    }
    line = next_line();
    d_formatted_read(line, "boundaries: %d", &nboundaries); // D:1321
    set_length_d(boundaries, 0);
    for (size_t i = 0; i < nboundaries; ++i) {
        line = next_line();
        boundaries.append(std::make_shared<BoundaryFaceSet>(line)); // D:1325
    }
    link_faces_back_to_cells(); // D:1327
}

// ---- usgrid.d:1330-1380 read_from_raw_binary_file ----
void UnstructuredGrid::read_from_raw_binary_file(const std::string& fileName, double scale) {
    std::FILE* raw = d_open(fileName, "rb");
    DFileGuard fin(raw);
    const std::string expected_header = "unstructured_grid 1.0"; // D:1338
    // D:1339's `new char[expected_header.length]` is UNINITIALISED memory, and a short
    // rawRead leaves the tail of it intact, so the tail bytes reach the error message
    // (D:1343 pastes the whole buffer with to!string(char[])). Three corpus rows carry
    // them: 924 has 2 non-printable bytes after "structured_grid 1.0", 925 has 7 after
    // "unstructured_g", 926 has 21 for an empty file — probe MEASURED FINDINGS item 10
    // states that the printable prefix and the COUNT are the contract and the bytes
    // themselves are not portable in either direction. The port therefore fills the tail
    // with a byte that is (a) non-printable, so upr() renders it as '?' and counts it
    // exactly like D's garbage, and (b) not NUL, because eil::ThrowableBase carries its
    // message through std::runtime_error, whose what() would truncate at the first NUL
    // and lose the count entirely.
    std::vector<char> found_header(expected_header.size(), '\x01');
    d_raw_read(raw, found_header.data(), found_header.size());   // D:1340
    if (std::string(found_header.data(), found_header.size()) != expected_header) {
        // D:1342-1343. to!string(found_header) of a char[] keeps every byte, including
        // the NULs a short rawRead left behind — three corpus rows carry those as
        // msgunprint>0 (probe MEASURED FINDINGS item 10).
        throw eil::ErrorBase("UnstructuredGrid.read_from_raw_binary_file(): "
                             "unexpected header: " +
                             std::string(found_header.data(), found_header.size()));
    }
    int buf1[1] = {0}; d_raw_read(raw, buf1, 4);            // D:1345
    const int label_length = buf1[0];
    if (label_length > 0) {
        std::vector<char> found_label(static_cast<size_t>(label_length), '\0');
        d_raw_read(raw, found_label.data(), static_cast<size_t>(label_length));
        label = std::string(found_label.data(), static_cast<size_t>(label_length));
    }
    int buf5[5] = {0, 0, 0, 0, 0}; d_raw_read(raw, buf5, 5 * 4);        // D:1352
    dimensions = buf5[0];
    nvertices = static_cast<size_t>(buf5[1]);
    nfaces = static_cast<size_t>(buf5[2]);
    ncells = static_cast<size_t>(buf5[3]);
    nboundaries = static_cast<size_t>(buf5[4]);
    double xyz[3] = {0.0, 0.0, 0.0};
    set_length_d(vertices, 0);                           // D:1360
    for (size_t i = 0; i < nvertices; ++i) {
        d_raw_read(raw, xyz, 3 * 8);
        vertices.append(Vector3(scale * xyz[0], scale * xyz[1], scale * xyz[2]));
    }
    set_length_d(faces, 0);
    for (size_t i = 0; i < nfaces; ++i) {
        std::shared_ptr<USGFace> myFace = std::make_shared<USGFace>(raw); // D:1367
        set_face_index(makeFaceTag(myFace->vtx_id_list.all()), faces.length());
        faces.append(myFace);
    }
    set_length_d(cells, 0);
    for (size_t i = 0; i < ncells; ++i) cells.append(std::make_shared<USGCell>(raw, i));
    set_length_d(boundaries, 0);
    for (size_t i = 0; i < nboundaries; ++i)
        boundaries.append(std::make_shared<BoundaryFaceSet>(raw));
    link_faces_back_to_cells();
}
// ---- usgrid.d:1382-1783 read_from_su2_file ----
// The SU2 text format (https://su2code.github.io/docs_v7/Mesh-File/). The three
// progress dots at D:1417, :1483 and :1632 and the closing " (done)" line at D:1781 are
// part of the contract, so they are emitted through the same stdout stream D uses.
void UnstructuredGrid::read_from_su2_file(const std::string& fileName, bool true_centroids,
                                          double scale, bool expect_gmsh_order_for_wedges) {
    d_stdout_writef("Importing: %s ", fileName); // D:1390
    std::FILE* raw = d_open(fileName, "r");
    DFileGuard closer(raw);
    DTextReader f;
    f.f = raw;

    // D:1392-1406 the nested helper: proceed line-by-line until the target appears,
    // return the text after the '=', and leave the file on the next line.
    auto getHeaderContent = [&f](const std::string& target) -> std::string {
        while (!f.at_eof) {
            const std::string line = d_strip(f.readln());
            if (line.find(target) != std::string::npos) { // D:1400 canFind
                eil::Array<std::string> tokens = split_on(line, "=");
                return d_strip(tokens[1]); // D:1402 (ArrayIndexError if there is no '=')
            }
        }
        return ""; // D:1405 didn't find the target
    };

    dimensions = d_to_int(getHeaderContent("NDIME")); // D:1407
    f.do_rewind();                                       // D:1411
    ncells = eil::to_size_t(getHeaderContent("NELEM")); // D:1412
    set_length_d(cells, ncells);                            // D:1413
    for (size_t i = 0; i < ncells; ++i) {
        if ((i % 100000) == 0) { d_stdout_write("."); d_stdout_flush(); } // D:1417
        const std::string lineContent = d_strip(f.readln());
        eil::Array<std::string> tokens = split_ws(lineContent);
        const int vtk_element_type = d_to_int(tokens[0]);       // D:1425
        size_t indx = i;                                           // D:1426
        const int nv_expected = n_vertices_for_vtk_element(vtk_element_type);
        eil::Array<size_t> vtx_id_list;
        if (tokens.length() == static_cast<size_t>(2 + nv_expected)) {
            // All items present, including the cell index at the end. D:1431
            indx = eil::to_size_t(tokens[tokens.length() - 1]);
            for (size_t j = 1; j + 1 < tokens.length(); ++j) {
                vtx_id_list.append(eil::to_size_t(tokens[j]));
            }
        } else if (tokens.length() == static_cast<size_t>(1 + nv_expected)) {
            for (size_t j = 1; j < tokens.length(); ++j) { // D:1435
                vtx_id_list.append(eil::to_size_t(tokens[j]));
            }
        } else {
            std::string msg = "Incorrect number of items on line for element."; // D:1437
            msg += eil::format("\nCell i: %d line:\"%s\"", i, lineContent);
            EIL_THROW_GEOM(msg);
        }
        USGCell_type cell_type = convert_cell_type(vtk_element_type);
        if (cell_type == USGCell_type::none) {
            std::string msg = "Unknown element type."; // D:1443
            msg += eil::format("\nCell i: %d line:\"%s\"", i, lineContent);
            EIL_THROW_GEOM(msg);
        }
        if (cell_type == USGCell_type::wedge && expect_gmsh_order_for_wedges) {
            // D:1450-1451: GMSH wedge order in, VTK order out.
            eil::Array<size_t> r = {vtx_id_list[1], vtx_id_list[0], vtx_id_list[2],
                                    vtx_id_list[4], vtx_id_list[3], vtx_id_list[5]};
            vtx_id_list = r;
        }
        eil::Array<size_t> face_id_list; // empty so far (D:1453)
        eil::Array<int> outsign_list;    // empty so far (D:1454)
        cells[indx] = std::make_shared<USGCell>(cell_type, indx, vtx_id_list.all(),
                                                face_id_list.all(), outsign_list.all());
    } // end foreach i .. ncells

    size_t uninitialized_cell_count = 0; // D:1461-1464
    for (size_t i = 0; i < cells.length(); ++i) {
        if (!cells[i]) { ++uninitialized_cell_count; }
    }
    if (uninitialized_cell_count > 0) {
        eil::writefln("Warning: There are %d initialized cells and %d uninitialized cells.",
                      ncells, uninitialized_cell_count); // D:1466
    }

    f.do_rewind();                                                  // D:1477
    nvertices = eil::to_size_t(getHeaderContent("NPOIN")); // D:1478
    set_length_d(vertices, nvertices);                                 // D:1479
    for (size_t i = 0; i < nvertices; ++i) {
        if ((i % 100000) == 0) { d_stdout_write("."); d_stdout_flush(); }
        eil::Array<std::string> tokens = split_ws(d_strip(f.readln()));
        const size_t ntokens = tokens.length();
        double x = 0.0, y = 0.0, z = 0.0;
        size_t indx = 0;
        if (dimensions == 2) {                                      // D:1490-1497
            x = scale * eil::to_double(tokens[0]);
            y = scale * eil::to_double(tokens[1]);
            if (ntokens == 3) {
                indx = eil::to_size_t(tokens[2]);
            } else {
                indx = i;
            }
        } else {
            eil_assert_msg(dimensions == 3, "invalid dimensions"); // D:1499, a body assert
            x = scale * eil::to_double(tokens[0]);
            y = scale * eil::to_double(tokens[1]);
            z = scale * eil::to_double(tokens[2]);
            if (ntokens == 4) {
                indx = eil::to_size_t(tokens[3]);
            } else {
                indx = i;
            }
        }
        vertices[indx].set(x, y, z);                                // D:1509
    } // end foreach i .. nvertices

    if (dimensions == 2) {
        // D:1512-1547: make every cell's vertex cycle counter-clockwise.
        for (size_t ci = 0; ci < cells.length(); ++ci) {
            USGCell& c = *cells[ci]; // D:1519 dereferences whatever is there, including a
                                     // cell the file never filled; the corpus does not
                                     // reach that path (see the probe's us.gap.* rows).
            switch (c.cell_type) {
            case USGCell_type::triangle: {
                Vector3 a = Vector3(vertices[c.vtx_id_list[1]]);
                a -= vertices[c.vtx_id_list[0]];
                Vector3 b = Vector3(vertices[c.vtx_id_list[2]]);
                b -= vertices[c.vtx_id_list[0]];
                Vector3 n;
                cross(n, a, b);
                if (eil::re(n.z) < 0.0) { d_reverse(c.vtx_id_list); }
                break;
            }
            case USGCell_type::quad: {
                Vector3 a = Vector3(vertices[c.vtx_id_list[1]]);
                a -= vertices[c.vtx_id_list[0]];
                a += vertices[c.vtx_id_list[2]];
                a -= vertices[c.vtx_id_list[3]];
                Vector3 b = Vector3(vertices[c.vtx_id_list[3]]);
                b -= vertices[c.vtx_id_list[0]];
                b += vertices[c.vtx_id_list[2]];
                b -= vertices[c.vtx_id_list[1]];
                Vector3 n;
                cross(n, a, b);
                if (eil::re(n.z) < 0.0) { d_reverse(c.vtx_id_list); }
                break;
            }
            default:
                EIL_THROW_GEOM("invalid cell type for 2D grid"); // D:1544
            }
        }
    }

    // D:1552-1627 the nested add_face_to_cell, closing over faceIndices, faces and
    // vertices (wave-02-brief 4b item 5: a capturing lambda is the port of a D nested
    // function).
    auto add_face_to_cell = [&](USGCellPtr& cell, const eil::Array<size_t>& corners,
                                const Vector3& cell_centroid) {
        eil::Array<size_t> my_vtx_id_list;
        for (size_t q = 0; q < corners.length(); ++q) {
            my_vtx_id_list.append(cell->vtx_id_list[corners[q]]); // D:1559
        }
        size_t face_indx = 0;
        std::string faceTag = makeFaceTag(my_vtx_id_list.all());
        if (faceIndices.contains(faceTag)) {          // D:1562 `faceTag in faceIndices`
            face_indx = faceIndices.get(faceTag);
        } else {
            face_indx = faces.length();
            faces.append(std::make_shared<USGFace>(my_vtx_id_list.all())); // D:1568
            set_face_index(faceTag, face_indx);
        }
        cell->face_id_list.append(face_indx);         // D:1571
        // Pointers to all vertices on the face (D:1578-1581).
        std::vector<Vector3*> v;
        for (size_t i = 0; i < corners.length(); ++i) {
            v.push_back(&vertices[faces[face_indx]->vtx_id_list[i]]);
        }
        bool onLeft = false;
        switch (corners.length()) {
        case 2:
            // Two-dimensional: the face is a line in the xy-plane.
            onLeft = on_left_of_xy_line(*(v[0]), *(v[1]), cell_centroid); // D:1590
            break;
        case 3:
            onLeft = (tetrahedron_volume(*(v[0]), *(v[1]), *(v[2]), cell_centroid) < 0.0);
            break;                                              // D:1598
        case 4: {
            Vector3 vmid = 0.25 * (*(v[0]) + *(v[1]) + *(v[2]) + *(v[3])); // D:1606
            onLeft = (tetragonal_dipyramid_volume(*(v[0]), *(v[1]), *(v[2]), *(v[3]), vmid,
                                                  cell_centroid) < 0.0);
            break;
        }
        default:
            EIL_THROW_GEOM("invalid number of corners on face."); // D:1611
        }
        if (onLeft) {
            if (faces[face_indx]->left_cell) {
                EIL_THROW_GEOM("face already has a left cell"); // D:1615
            }
            faces[face_indx]->left_cell = cell;  // D:1617 (the handle, as in D)
            cell->outsign_list.append(1);
        } else {
            if (faces[face_indx]->right_cell) {
                EIL_THROW_GEOM("face already has a right cell"); // D:1621
            }
            faces[face_indx]->right_cell = cell;
            cell->outsign_list.append(-1);
        }
    }; // end add_face_to_cell

    for (size_t i = 0; i < cells.length(); ++i) { // D:1629-1704
        if ((i % 100000) == 0) { d_stdout_write("."); d_stdout_flush(); }
        if (!cells[i]) continue; // D:1633
        Vector3 centroid;
        number volume = 0.0;
        compute_cell_properties(i, true_centroids, centroid, volume); // D:1641
        if (dimensions == 2) {
            switch (cells[i]->cell_type) {
            case USGCell_type::triangle: {
                const eil::Array<size_t> c01 = {0, 1}, c12 = {1, 2}, c20 = {2, 0};
                add_face_to_cell(cells[i], c01, centroid);
                add_face_to_cell(cells[i], c12, centroid);
                add_face_to_cell(cells[i], c20, centroid);
                break;
            }
            case USGCell_type::quad: {
                const eil::Array<size_t> n_ = {2, 3}, e_ = {1, 2}, s_ = {0, 1}, w_ = {3, 0};
                add_face_to_cell(cells[i], n_, centroid); // north
                add_face_to_cell(cells[i], e_, centroid); // east
                add_face_to_cell(cells[i], s_, centroid); // south
                add_face_to_cell(cells[i], w_, centroid); // west
                break;
            }
            default:
                EIL_THROW_GEOM("invalid cell type in 2D"); // D:1656
            }
        } else {
            eil_assert_msg(dimensions == 3, "invalid dimensions"); // D:1659
            switch (cells[i]->cell_type) {
            case USGCell_type::tetra: {
                const eil::Array<size_t> a = {0, 1, 2}, b = {0, 1, 3}, c = {1, 2, 3},
                                       d = {2, 0, 3};
                add_face_to_cell(cells[i], a, centroid);
                add_face_to_cell(cells[i], b, centroid);
                add_face_to_cell(cells[i], c, centroid);
                add_face_to_cell(cells[i], d, centroid);
                break;
            }
            case USGCell_type::hexahedron: {
                const eil::Array<size_t> a = {2, 3, 7, 6}, b = {1, 2, 6, 5}, c = {1, 0, 4, 5},
                                       d = {0, 3, 7, 4}, e = {4, 5, 6, 7}, g = {0, 1, 2, 3};
                add_face_to_cell(cells[i], a, centroid); // north
                add_face_to_cell(cells[i], b, centroid); // east
                add_face_to_cell(cells[i], c, centroid); // south
                add_face_to_cell(cells[i], d, centroid); // west
                add_face_to_cell(cells[i], e, centroid); // top
                add_face_to_cell(cells[i], g, centroid); // bottom
                break;
            }
            case USGCell_type::wedge: {
                const eil::Array<size_t> a = {0, 1, 2}, b = {3, 4, 5}, c = {0, 2, 5, 3},
                                       d = {0, 3, 4, 1}, e = {1, 4, 5, 2};
                add_face_to_cell(cells[i], a, centroid);
                add_face_to_cell(cells[i], b, centroid);
                add_face_to_cell(cells[i], c, centroid);
                add_face_to_cell(cells[i], d, centroid);
                add_face_to_cell(cells[i], e, centroid);
                break;
            }
            case USGCell_type::pyramid: {
                const eil::Array<size_t> a = {0, 1, 2, 3}, b = {0, 1, 4}, c = {1, 2, 4},
                                       d = {2, 3, 4}, e = {3, 0, 4};
                add_face_to_cell(cells[i], a, centroid);
                add_face_to_cell(cells[i], b, centroid);
                add_face_to_cell(cells[i], c, centroid);
                add_face_to_cell(cells[i], d, centroid);
                add_face_to_cell(cells[i], e, centroid);
                break;
            }
            default:
                EIL_THROW_GEOM("invalid cell type in 3D"); // D:1690
            }
        }
        Vector3 varea = vectorAreaOfCell(*cells[i]); // D:1693
        const double relTol = 1.0e-9, absTol = 1.0e-9;
        if (!approxEqualVectors(varea, Vector3(0, 0, 0), relTol, absTol)) {
            std::string errMsg = eil::format("cell id=%d, non-zero vector area=%s\n   cell is %s",
                                             i, varea.toString(), cells[i]->toIOString());
            for (size_t j = 0; j < cells[i]->face_id_list.length(); ++j) {
                const size_t fid = cells[i]->face_id_list[j];
                errMsg += eil::format("\n    j=%d outsign=%d vectorArea=%s", j,
                                      cells[i]->outsign_list[j],
                                      vectorAreaOfFacet(faces[fid]->vtx_id_list.all()).toString());
            }
            EIL_THROW_GEOM(errMsg); // D:1702
        }
    } // end foreach cell
    nfaces = faces.length(); // D:1705

    for (size_t i = 0; i < faces.length(); ++i) faces[i]->is_on_boundary = false; // D:1710
    nboundaries = eil::to_size_t(getHeaderContent("NMARK"));    // D:1713
    for (size_t i = 0; i < nboundaries; ++i) {
        const std::string tag = getHeaderContent("MARKER_TAG");   // D:1715
        eil::Array<size_t> face_id_list;
        eil::Array<int> outsign_list;
        const size_t nelem = eil::to_size_t(getHeaderContent("MARKER_ELEMS"));
        for (size_t j = 0; j < nelem; ++j) {
            eil::Array<std::string> tokens = split_ws(d_strip(f.readln()));
            const int vtk_type = d_to_int(tokens[0]); // D:1721 (read, not used further)
            (void)vtk_type;
            eil::Array<size_t> my_vtx_id_list;
            for (size_t k = 1; k < tokens.length(); ++k) {
                my_vtx_id_list.append(eil::to_size_t(tokens[k]));
            }
            const std::string faceTag = makeFaceTag(my_vtx_id_list.all());
            if (faceIndices.contains(faceTag)) {                    // D:1725
                const size_t face_indx = faceIndices.get(faceTag);
                std::shared_ptr<USGFace> my_face = faces[face_indx];
                eil_assert_msg(my_face->left_cell || my_face->right_cell, // D:1728
                               "face is not properly connected");
                if (my_face->left_cell && !(my_face->right_cell)) {
                    outsign_list.append(1);
                } else if (!(my_face->left_cell) && my_face->right_cell) {
                    outsign_list.append(-1);
                } else {
                    EIL_THROW_GEOM("appears to be an interior face"); // D:1734
                }
                face_id_list.append(face_indx);
                my_face->is_on_boundary = true;
            } else {
                EIL_THROW_GEOM("cannot find face in collection"); // D:1739
            }
        } // end foreach j
        boundaries.append(
            std::make_shared<BoundaryFaceSet>(tag, face_id_list.all(), outsign_list.all()));
    } // end foreach i

    // D:1750-1766: any face with one cell that no marker claimed becomes an orphan set.
    eil::Array<size_t> orphan_face_id_list;
    eil::Array<int> orphan_face_outsign_list;
    for (size_t i = 0; i < faces.length(); ++i) {
        const std::shared_ptr<USGFace>& myface = faces[i];
        if (myface->is_on_boundary) continue;
        if (myface->left_cell && myface->right_cell) continue;
        orphan_face_id_list.append(i);
        if (myface->left_cell && !(myface->right_cell)) {
            orphan_face_outsign_list.append(1);
        } else if (!(myface->left_cell) && myface->right_cell) {
            orphan_face_outsign_list.append(-1);
        }
    }
    if (orphan_face_id_list.length() > 0) {
        boundaries.append(std::make_shared<BoundaryFaceSet>(
            "ORPHAN_FACES", orphan_face_id_list.all(), orphan_face_outsign_list.all()));
        nboundaries = boundaries.length();
    }

    Vector3 varea = Vector3(0, 0, 0); // D:1768-1773
    for (size_t bi = 0; bi < boundaries.length(); ++bi) {
        const BoundaryFaceSet& b = *boundaries[bi];
        for (size_t j = 0; j < b.face_id_list.length(); ++j) {
            varea += static_cast<number>(b.outsign_list[j]) *
                     vectorAreaOfFacet(faces[b.face_id_list[j]]->vtx_id_list.all());
        }
    }
    const double relTol = 1.0e-9, absTol = 1.0e-9;
    if (!approxEqualVectors(varea, Vector3(0, 0, 0), relTol, absTol)) {
        // D:1776 is format("SU2 grid has non-zero bounding vector area=", varea) — a
        // format string with one argument and no conversion spec, so Phobos raises
        // std.format.FormatException "Orphan format arguments: args[0..1]" and the
        // GeometryException of D:1777 is never constructed. Measured at corpus row
        // us.su2rd.noclose. The text is raised directly rather than through
        // eil::format(..., varea) because that template instantiates its argument pack
        // first, and Vector3 has no eil::to_string (it prints through its own
        // toString()). Nothing about the observable behaviour changes: the same class
        // family and the same message reach the caller, and varea is never rendered.
        throw eil::format_error("Orphan format arguments: args[0..1]"); // D:1776
    }
    // D:1781 the import succeeded.
    eil::writefln(" (done) scale=%s dim=%s ncells=%d nbfs= %d", scale, dimensions, ncells,
                  boundaries.length());
}

// ---- usgrid.d:1785-1798 read_from_vtk_text_file ----
// Unconditionally unfinished in the reference: the header walk runs, then D:1795 throws
// a bare Error before anything else can happen.
void UnstructuredGrid::read_from_vtk_text_file(const std::string& fileName, double scale) {
    (void)scale;
    eil::Array<std::string> tokens;                    // D:1788
    std::FILE* raw = d_open(fileName, "r");            // D:1789 `auto f = File(...)`
    DFileGuard closer(raw);                             // D's File closes in its destructor
    DTextReader f;
    f.f = raw;
    tokens = read_VTK_header_line("vtk", raw);         // D:1790 (sgrid.d:1544)
    // D:1791 `label = f.readln().strip()` — ONE line consumed here. Reading twice (as an
    // earlier port did) pushes the "ASCII" scan one line too far, and corpus rows 1199 and
    // 1201 (which reach D:1795) plus 1203 (which reaches D:1793) all came back as
    // "Did not find ASCII while reading VTK grid file" instead.
    label = d_strip(f.readln());
    tokens = read_VTK_header_line("ASCII", raw);       // D:1792
    tokens = read_VTK_header_line("UNSTRUCTURED_GRID", raw); // D:1793
    tokens = split_ws(d_strip(f.readln()));            // D:1794
    // D:1795 is reached from every path: the function is unfinished in the reference, and
    // the throw is a bare Error (no file/line).
    throw eil::ErrorBase("read_from_vtk_text_file() is not finished, yet.");
}
// ---- usgrid.d:1800-1821 write_to_gzip_file ----
// Together with the three text constructors, this IS the Eilmer4 native format. The
// container bytes are not a contract (they carry an mtime); the payload is.
void UnstructuredGrid::write_to_gzip_file(const std::string& fileName) {
    eil::gzip::GzipOut fout(fileName);                     // D:1806
    fout.compress("unstructured_grid 1.0\n");              // D:1807
    fout.compress(eil::format("label: %s\n", label));
    fout.compress(eil::format("dimensions: %d\n", dimensions));
    fout.compress(eil::format("vertices: %d\n", nvertices));
    for (size_t i = 0; i < vertices.length(); ++i) {       // D:1811-1813
        const Vector3& v = vertices[i];
        fout.compress(eil::format("%.18e %.18e %.18e\n", eil::re(v.x), eil::re(v.y),
                                  eil::re(v.z)));
    }
    fout.compress(eil::format("faces: %d\n", nfaces));
    for (size_t i = 0; i < faces.length(); ++i) fout.compress(faces[i]->toIOString() + "\n");
    fout.compress(eil::format("cells: %d\n", ncells));
    for (size_t i = 0; i < cells.length(); ++i) fout.compress(cells[i]->toIOString() + "\n");
    fout.compress(eil::format("boundaries: %d\n", boundaries.length())); // D:1818
    for (size_t i = 0; i < boundaries.length(); ++i)
        fout.compress(boundaries[i]->toIOString() + "\n");
    fout.finish();
}

// ---- usgrid.d:1823-1848 write_to_raw_binary_file ----
void UnstructuredGrid::write_to_raw_binary_file(const std::string& fileName) {
    std::FILE* raw = d_open(fileName, "wb");
    DFileGuard fout(raw);
    const std::string hdr = "unstructured_grid 1.0"; // D:1830 to!(char[])(...)
    d_raw_write(raw, hdr.data(), hdr.size());
    int buf1[1] = {0};
    buf1[0] = static_cast<int>(label.length()); d_raw_write(raw, buf1, 4); // D:1831
    if (label.length() > 0) d_raw_write(raw, label.data(), label.size());  // D:1832
    int buf5[5] = {0, 0, 0, 0, 0};                                                          // D:1833-1838
    buf5[0] = static_cast<int>(dimensions);
    buf5[1] = static_cast<int>(nvertices);
    buf5[2] = static_cast<int>(nfaces);
    buf5[3] = static_cast<int>(ncells);
    buf5[4] = static_cast<int>(boundaries.length());
    d_raw_write(raw, buf5, 5 * 4);
    double xyz[3] = {0.0, 0.0, 0.0};
    for (size_t i = 0; i < vertices.length(); ++i) { // D:1840-1843
        const Vector3& v = vertices[i];
        xyz[0] = eil::re(v.x); xyz[1] = eil::re(v.y); xyz[2] = eil::re(v.z);
        d_raw_write(raw, xyz, 3 * 8);
    }
    for (size_t i = 0; i < faces.length(); ++i) faces[i]->rawWrite(raw);
    for (size_t i = 0; i < cells.length(); ++i) cells[i]->rawWrite(raw);
    for (size_t i = 0; i < boundaries.length(); ++i) boundaries[i]->rawWrite(raw);
}

// ---- usgrid.d:1850-1874 write_to_vtk_file ----
void UnstructuredGrid::write_to_vtk_file(const std::string& fileName) {
    std::FILE* raw = d_open(fileName, "w");
    DFileGuard f(raw);
    d_writeln(f, "# vtk DataFile Version 2.0");
    d_writeln(f, label);
    d_writeln(f, "ASCII");
    d_writeln(f, "");
    d_writeln(f, "DATASET UNSTRUCTURED_GRID");
    d_writefln(f, "POINTS %d float", nvertices);
    for (size_t i = 0; i < vertices.length(); ++i) {
        const Vector3& v = vertices[i];
        d_writefln(f, "%.18e %.18e %.18e", eil::re(v.x), eil::re(v.y), eil::re(v.z));
    }
    d_writeln(f, "");
    int n_ints = 0;                                        // D:1861
    for (size_t i = 0; i < cells.length(); ++i) n_ints += 1 + static_cast<int>(cells[i]->vtx_id_list.length());
    d_writefln(f, "CELLS %d %d", ncells, n_ints);
    for (size_t i = 0; i < cells.length(); ++i) {          // D:1864-1868
        d_write(f, eil::to_string(cells[i]->vtx_id_list.length()));
        for (size_t k = 0; k < cells[i]->vtx_id_list.length(); ++k) {
            d_write(f, " "); d_write(f, eil::to_string(cells[i]->vtx_id_list[k]));
        }
        d_writeln(f, "");
    }
    d_writefln(f, "CELL_TYPES %d", ncells);
    for (size_t i = 0; i < cells.length(); ++i) {
        d_writeln(f, eil::format("%d", vtk_element_types[static_cast<int>(cells[i]->cell_type)]));
    }
}

// ---- usgrid.d:1876-1928 write_to_su2_file ----
void UnstructuredGrid::write_to_su2_file(const std::string& fileName, double scale,
                                         bool use_gmsh_order_for_wedges) {
    std::FILE* raw = d_open(fileName, "w");
    DFileGuard f(raw);
    d_writefln(f, "NDIME= %d", dimensions);
    d_writeln(f, "");
    d_writefln(f, "NELEM= %d", ncells);
    for (size_t i = 0; i < cells.length(); ++i) { // D:1883-1896
        d_write(f, eil::format("%d", vtk_element_types[static_cast<int>(cells[i]->cell_type)]));
        if (cells[i]->cell_type == USGCell_type::wedge && use_gmsh_order_for_wedges) {
            const eil::Array<size_t> src = cells[i]->vtx_id_list.dup();
            // VTK order in the grid, GMSH order on the way out (D:1886-1891).
            const eil::Array<size_t> vtx_id_list = {src[1], src[0], src[2], src[4], src[3], src[5]};
            for (size_t k = 0; k < vtx_id_list.length(); ++k) {
                d_write(f, " "); d_write(f, eil::to_string(vtx_id_list[k]));
            }
        } else {
            for (size_t k = 0; k < cells[i]->vtx_id_list.length(); ++k) {
                d_write(f, " "); d_write(f, eil::to_string(cells[i]->vtx_id_list[k]));
            }
        }
        d_writefln(f, " %d", i); // the cell index (D:1895)
    }
    d_writeln(f, "");
    d_writefln(f, "NPOIN= %d", nvertices);
    for (size_t i = 0; i < vertices.length(); ++i) { // D:1899-1905
        const Vector3& v = vertices[i];
        if (dimensions == 3) {
            d_writefln(f, "%.18e %.18e %.18e %d", eil::re(v.x) / scale, eil::re(v.y) / scale,
                       eil::re(v.z) / scale, i);
        } else {
            d_writefln(f, "%.18e %.18e %d", eil::re(v.x) / scale, eil::re(v.y) / scale, i);
        }
    }
    d_writeln(f, "");
    d_writefln(f, "NMARK= %d", nboundaries);
    for (size_t bi = 0; bi < boundaries.length(); ++bi) { // D:1908-1926
        d_writefln(f, "MARKER_TAG= %s", boundaries[bi]->tag);
        d_writefln(f, "MARKER_ELEMS= %d", boundaries[bi]->face_id_list.length());
        for (size_t q = 0; q < boundaries[bi]->face_id_list.length(); ++q) {
            const eil::Array<size_t> vtx_id_list =
                faces[boundaries[bi]->face_id_list[q]]->vtx_id_list.dup();
            if (dimensions == 3) {
                switch (vtx_id_list.length()) {
                case 3: d_writef(f, "%d", static_cast<int>(VTKElement::triangle)); break;
                case 4: d_writef(f, "%d", static_cast<int>(VTKElement::quad)); break;
                default: d_write(f, "oops, should not have this number of vertices"); break;
                }
            } else {
                d_writef(f, "%d", static_cast<int>(VTKElement::line)); // 2D: lines only
            }
            for (size_t k = 0; k < vtx_id_list.length(); ++k) d_writef(f, " %d", vtx_id_list[k]);
            d_writeln(f, "");
        }
    }
}

// ---- usgrid.d:1930-2162 write_openFoam_polyMesh ----
void UnstructuredGrid::write_openFoam_polyMesh(const std::string& topLevelDir) {
    // D:1952 `int[USGCell] cellId` — an associative array keyed by a class handle; the
    // port keys it by the handle's address, which is the same identity D compares.
    eil::AA<USGCellPtr, int> cellId;
    for (size_t i = 0; i < cells.length(); ++i) cellId.set(cells[i], static_cast<int>(i));

    eil::Array<size_t> internal_face_id_list;  // D:1957-1959
    eil::Array<int> owner_cell_id_list;
    eil::Array<int> neighbour_cell_id_list;
    for (size_t i = 0; i < faces.length(); ++i) { // D:1960-1980
        int owner_id = -1;
        int neighbour_id = -1;
        if (!faces[i]->left_cell) {
            owner_id = cellId.get(faces[i]->right_cell); // D:1964 (RangeError if null)
        } else if (!faces[i]->right_cell) {
            owner_id = cellId.get(faces[i]->left_cell);
        } else {
            internal_face_id_list.append(i);
            if (cellId.get(faces[i]->left_cell) < cellId.get(faces[i]->right_cell)) {
                owner_id = cellId.get(faces[i]->left_cell);
                neighbour_id = cellId.get(faces[i]->right_cell);
            } else {
                owner_id = cellId.get(faces[i]->right_cell);
                neighbour_id = cellId.get(faces[i]->left_cell);
            }
        }
        eil_assert_msg(owner_id >= 0, "face seems to be not owned by a cell"); // D:1977
        owner_cell_id_list.append(owner_id);
        neighbour_cell_id_list.append(neighbour_id);
    }
    size_t face_tally = internal_face_id_list.length();                     // D:1981-1982
    for (size_t bi = 0; bi < boundaries.length(); ++bi)
        face_tally += boundaries[bi]->face_id_list.length();
    eil_assert_msg(face_tally == faces.length(),
                   "mismatch in number of faces in boundary sets");         // D:1983

    // D:1991-2030: work through the cells and decide the vertex order of each face.
    eil::Array<bool> reverse_vtx_order_for_face;
    set_length_d(reverse_vtx_order_for_face, faces.length()); // D:1992 (value-init false)
    eil::Array<bool> face_already_checked;
    for (size_t fid = 0; fid < faces.length(); ++fid) face_already_checked.append(false);
    for (size_t i = 0; i < cells.length(); ++i) {
        USGCell& c = *cells[i];
        for (size_t j = 0; j < c.face_id_list.length(); ++j) {
            const size_t fid = c.face_id_list[j];
            std::shared_ptr<USGFace> f = faces[fid];
            if (face_already_checked[fid]) continue;
            if (!f->left_cell) {
                eil_assert_msg(c.outsign_list[j] == -1, "expected inward-pointing face"); // D:2001
                reverse_vtx_order_for_face[fid] = true;
                face_already_checked[fid] = true;
                continue;
            }
            if (!f->right_cell) {
                eil_assert_msg(c.outsign_list[j] == 1, "expected outward-pointing face"); // D:2008
                reverse_vtx_order_for_face[fid] = false;
                face_already_checked[fid] = true;
                continue;
            }
            size_t other_cell_id = 0;
            if (c.outsign_list[j] == 1) {
                other_cell_id = static_cast<size_t>(cellId.get(f->right_cell));
            } else {
                other_cell_id = static_cast<size_t>(cellId.get(f->left_cell));
            }
            if (((static_cast<int>(i) < static_cast<int>(other_cell_id)) &&
                 (c.outsign_list[j] == 1)) ||
                ((static_cast<int>(i) > static_cast<int>(other_cell_id)) &&
                 (c.outsign_list[j] == -1))) {
                reverse_vtx_order_for_face[fid] = false;   // consistent with openFoam
            } else {
                reverse_vtx_order_for_face[fid] = true;
            }
            face_already_checked[fid] = true;
        }
    }

    const std::string polyMeshDir = topLevelDir + "/polyMesh"; // D:2034-2035
    if (!std::filesystem::exists(polyMeshDir)) {
        std::filesystem::create_directories(polyMeshDir);      // mkdirRecurse
    }
    // The five files, in D's order, each with the FoamFile header block.
    {
        std::FILE* raw = d_open(polyMeshDir + "/points", "w");
        DFileGuard f(raw);
        d_writeln(f, "FoamFile\n{");
        d_writeln(f, " version   2.0;");
        d_writeln(f, " format    ascii;");
        d_writeln(f, " class     vectorField;");
        d_writeln(f, " location  \"constant/polyMesh\";");
        d_writeln(f, " object    points;");
        d_writeln(f, "}");
        d_writefln(f, "%d\n(", vertices.length());
        for (size_t i = 0; i < vertices.length(); ++i) {
            const Vector3& v = vertices[i];
            d_writefln(f, " (%.18e %.18e %.18e)", eil::re(v.x), eil::re(v.y), eil::re(v.z));
        }
        d_writeln(f, ")");
    }
    // D:2058-2069 the nested index_str.
    auto index_str = [&](const USGFace& fo, bool reverse_vtx_order) {
        std::string str = " ";
        str += eil::to_string(fo.vtx_id_list.length());
        str += "(";
        eil::Array<size_t> my_vtx_id_list = fo.vtx_id_list.dup();
        if (reverse_vtx_order) { d_reverse(my_vtx_id_list); }
        for (size_t j = 0; j < my_vtx_id_list.length(); ++j) {
            if (j > 0) str += " ";
            str += eil::to_string(my_vtx_id_list[j]);
        }
        str += ")";
        return str;
    };
    {
        std::FILE* raw = d_open(polyMeshDir + "/faces", "w");
        DFileGuard f(raw);
        d_writeln(f, "FoamFile\n{");
        d_writeln(f, " version   2.0;");
        d_writeln(f, " format    ascii;");
        d_writeln(f, " class     faceList;");
        d_writeln(f, " location  \"constant/polyMesh\";");
        d_writeln(f, " object    faces;");
        d_writeln(f, "}");
        d_writefln(f, "%d\n(", faces.length());
        for (size_t q = 0; q < internal_face_id_list.length(); ++q) {
            const size_t fid = internal_face_id_list[q];
            d_writeln(f, index_str(*faces[fid], reverse_vtx_order_for_face[fid]));
        }
        for (size_t bi = 0; bi < boundaries.length(); ++bi) {
            for (size_t q = 0; q < boundaries[bi]->face_id_list.length(); ++q) {
                const size_t fid = boundaries[bi]->face_id_list[q];
                d_writeln(f, index_str(*faces[fid], reverse_vtx_order_for_face[fid]));
            }
        }
        d_writeln(f, ")");
    }
    {
        std::FILE* raw = d_open(polyMeshDir + "/owner", "w");
        DFileGuard f(raw);
        d_writeln(f, "FoamFile\n{");
        d_writeln(f, " version   2.0;");
        d_writeln(f, " format    ascii;");
        d_writeln(f, " class     labelList;");
        d_writeln(f, " location  \"constant/polyMesh\";");
        d_writeln(f, " object    owner;");
        d_writeln(f, "}");
        d_writefln(f, "%d\n(", faces.length());
        for (size_t q = 0; q < internal_face_id_list.length(); ++q)
            d_writefln(f, " %d", owner_cell_id_list[internal_face_id_list[q]]);
        for (size_t bi = 0; bi < boundaries.length(); ++bi) {
            for (size_t q = 0; q < boundaries[bi]->face_id_list.length(); ++q)
                d_writefln(f, " %d", owner_cell_id_list[boundaries[bi]->face_id_list[q]]);
        }
        d_writeln(f, ")");
    }
    {
        std::FILE* raw = d_open(polyMeshDir + "/neighbour", "w");
        DFileGuard f(raw);
        d_writeln(f, "FoamFile\n{");
        d_writeln(f, " version   2.0;");
        d_writeln(f, " format    ascii;");
        d_writeln(f, " class     labelList;");
        d_writeln(f, " location  \"constant/polyMesh\";");
        d_writeln(f, " object    neighbour;");
        d_writeln(f, "}");
        d_writefln(f, "%d\n(", internal_face_id_list.length());
        for (size_t q = 0; q < internal_face_id_list.length(); ++q)
            d_writefln(f, " %d", neighbour_cell_id_list[internal_face_id_list[q]]);
        d_writeln(f, ")");
    }
    size_t bndry_count = 0;                                            // D:2119-2120
    for (size_t bi = 0; bi < boundaries.length(); ++bi) {
        if (boundaries[bi]->face_id_list.length() > 0) { ++bndry_count; }
    }
    size_t startFace = internal_face_id_list.length(); // D:2122 (declared outside the loop)
    {
        std::FILE* raw = d_open(polyMeshDir + "/boundary", "w");
        DFileGuard f(raw);
        d_writeln(f, "FoamFile\n{");
        d_writeln(f, " version   2.0;");
        d_writeln(f, " format    ascii;");
        d_writeln(f, " class     polyBoundaryMesh;");
        d_writeln(f, " location  \"constant/polyMesh\";");
        d_writeln(f, " object    boundary;");
        d_writeln(f, "}");
        d_writefln(f, "%d\n(", bndry_count);
        for (size_t i = 0; i < boundaries.length(); ++i) { // D:2123-2134
            if (boundaries[i]->face_id_list.length() == 0) continue;
            std::string lbl = boundaries[i]->tag;
            if (lbl.length() == 0) { lbl = eil::format("boundary%04d", i); }
            d_writefln(f, " %s\n {", lbl);
            d_writefln(f, "  type      %s;", "patch");
            d_writefln(f, "  nFaces    %d;", boundaries[i]->face_id_list.length());
            d_writefln(f, "  startFace %d;", startFace);
            d_writeln(f, " }");
            startFace += boundaries[i]->face_id_list.length();
        }
        d_writeln(f, ")");
    }
    // D:2140-2161 the face sets, one file per non-empty boundary.
    const std::string setsDir = topLevelDir + "/polyMesh/sets";
    if (!std::filesystem::exists(setsDir)) std::filesystem::create_directories(setsDir);
    startFace = internal_face_id_list.length();
    for (size_t i = 0; i < boundaries.length(); ++i) {
        const size_t nf = boundaries[i]->face_id_list.length();
        if (nf == 0) continue;
        std::string objName = boundaries[i]->tag;
        // D:2148 `if (objName.length == 0) { label = format("boundary%04d", i); }` —
        // `label` here is the GRID MEMBER, not a local (the local of D:2125 is out of
        // scope). Measured at corpus row us.foam.emptytag, which shows both the mutated
        // grid label and the file opened under an empty object name. Kept verbatim.
        if (objName.length() == 0) { label = eil::format("boundary%04d", i); }
        std::FILE* raw = d_open(setsDir + "/" + objName, "w");
        DFileGuard f(raw);
        d_writeln(f, "FoamFile\n{");
        d_writeln(f, " version   2.0;");
        d_writeln(f, " format    ascii;");
        d_writeln(f, " class     faceSet;");
        d_writeln(f, " location  \"constant/polyMesh/sets\";");
        d_writefln(f, " object    %s;", objName);
        d_writeln(f, "}");
        d_writefln(f, "%d\n(", nf);
        for (size_t j = 0; j < nf; ++j) d_writefln(f, " %d", j + startFace);
        d_writeln(f, ")");
        startFace += nf;
    }
}

// ---- usgrid.d:2164-2414 joinGrid ----
std::shared_ptr<UnstructuredGrid> UnstructuredGrid::joinGrid(
    const std::shared_ptr<UnstructuredGrid>& other, double relTol, double absTol,
    bool checkBoundariesOnly, bool openFoam, int dimensions_unused) {
    (void)dimensions_unused; // D:2167 declares the parameter and never uses it
    if (other.get() == static_cast<UnstructuredGrid*>(this)) return other; // D:2172
    // D:2174-2179: six body asserts on the master's and the other's counts.
    eil_assert_msg(nvertices == vertices.length(), "wrong number of vertices in master grid");
    eil_assert_msg(other->nvertices == other->vertices.length(),
                   "wrong number of vertices in other grid");
    eil_assert_msg(nfaces == faces.length(), "wrong number of faces in master grid");
    eil_assert_msg(other->nfaces == other->faces.length(), "wrong number of faces in other grid");
    eil_assert_msg(ncells == cells.length(), "wrong number of cells in master grid");
    eil_assert_msg(other->ncells == other->cells.length(), "wrong number of cells in other grid");

    eil::Array<size_t> new_vtx_ids;
    if (!checkBoundariesOnly) {
        // D:2183-2205 the exhaustive pairwise search.
        set_length_d(new_vtx_ids, other->vertices.length());
        for (size_t i = 0; i < other->vertices.length(); ++i) {
            const Vector3& vtx = other->vertices[i];
            bool found = false;
            size_t jsave = 0;
            for (size_t j = 0; j < vertices.length(); ++j) {
                if (approxEqualVectors(vertices[j], vtx, relTol, absTol)) {
                    found = true; jsave = j; break;
                }
            }
            if (!found) {
                vertices.append(vtx);
                new_vtx_ids[i] = vertices.length() - 1;
            } else {
                new_vtx_ids[i] = jsave;
            }
        }
        nvertices = vertices.length();
    } else {
        // D:2206-2269 the boundary-only search, through the KD tree.
        eil::Array<size_t> vtx_ids_boundary = getVertexIdsOnBoundaries();
        eil::Array<size_t> vtx_ids_other_boundary = other->getVertexIdsOnBoundaries();
        eil::Array<bool> other_vtx_is_on_boundary;
        set_length_d(other_vtx_is_on_boundary, other->vertices.length());
        for (size_t q = 0; q < vtx_ids_other_boundary.length(); ++q)
            other_vtx_is_on_boundary[vtx_ids_other_boundary[q]] = true;

        const size_t nvtxs = vtx_ids_boundary.length();
        eil::Array<Node> nodes;                              // D:2220-2226
        set_length_d(nodes, nvtxs);
        for (size_t i = 0; i < nvtxs; ++i) {
            const size_t id = vtx_ids_boundary[i];
            // D:2223 `Node node = {[x, y, z]};` — the array literal initialises the
            // static x[3] member only; blkid and cellid keep their .init until the next
            // line sets cellid.
            nodes[i].x = {eil::re(vertices[id].x), eil::re(vertices[id].y),
                          eil::re(vertices[id].z)};
            nodes[i].cellid = id;
        }
        Node* root = makeTree<0>(nodes.all());               // D:2227

        set_length_d(new_vtx_ids, other->vertices.length());
        for (size_t qi = 0; qi < vtx_ids_other_boundary.length(); ++qi) {
            const size_t i = vtx_ids_other_boundary[qi];
            bool found = false;
            size_t jsave = 0;
            Vector3 vtx = other->vertices[i];
            Node vtxnode;                                    // D:2238
            vtxnode.x = {eil::re(vtx.x), eil::re(vtx.y), eil::re(vtx.z)};
            const Node* foundNode = nullptr;
            double bestDist = 0.0;
            size_t nVisited = 0;
            fast_nearest(root, vtxnode, 0, foundNode, bestDist, nVisited); // D:2242
            const double dist = eil::sqrt(bestDist);                       // D:2243
            if (eil::isClose(dist, 0.0, absTol, relTol)) {
                found = true;
                jsave = foundNode->cellid;
            }
            if (!found) {
                vertices.append(vtx);
                new_vtx_ids[i] = vertices.length() - 1;
            } else {
                new_vtx_ids[i] = jsave;
            }
        }
        for (size_t i = 0; i < other->vertices.length(); ++i) { // D:2261-2267
            if (!other_vtx_is_on_boundary[i]) {
                vertices.append(other->vertices[i]);
                new_vtx_ids[i] = vertices.length() - 1;
            }
        }
        nvertices = vertices.length();
    }

    // D:2274-2277: make sure every master face is in the dictionary.
    for (size_t i = 0; i < faces.length(); ++i) {
        std::string faceTag = makeFaceTag(faces[i]->vtx_id_list.all());
        if (!faceIndices.contains(faceTag)) set_face_index(faceTag, i);
    }

    eil::Array<size_t> new_face_ids; set_length_d(new_face_ids, other->faces.length()); // D:2283
    eil::Array<bool> flip_new_face; set_length_d(flip_new_face, other->faces.length()); // D:2284
    for (size_t i = 0; i < other->faces.length(); ++i) {
        const USGFace& fo = *other->faces[i];
        eil::Array<size_t> new_vtx_id_list;
        for (size_t q = 0; q < fo.vtx_id_list.length(); ++q) {
            new_vtx_id_list.append(new_vtx_ids[fo.vtx_id_list[q]]);
        }
        std::string faceTag = makeFaceTag(new_vtx_id_list.all());
        if (!faceIndices.contains(faceTag)) {                       // D:2289
            faces.append(std::make_shared<USGFace>(new_vtx_id_list.all()));
            const size_t j = faces.length() - 1;
            set_face_index(faceTag, j);
            new_face_ids[i] = j;
            flip_new_face[i] = false;
        } else {
            const size_t j = faceIndices.get(faceTag);
            new_face_ids[i] = j;
            flip_new_face[i] = !sameOrientation(faces[j]->vtx_id_list.all(),
                                               new_vtx_id_list.all()); // D:2299
        }
    }
    nfaces = faces.length();

    for (size_t i = 0; i < other->cells.length(); ++i) { // D:2307-2322
        const USGCell& co = *other->cells[i];
        eil::Array<size_t> new_vtx_id_list;
        for (size_t q = 0; q < co.vtx_id_list.length(); ++q) {
            new_vtx_id_list.append(new_vtx_ids[co.vtx_id_list[q]]);
        }
        eil::Array<size_t> new_face_id_list;
        eil::Array<int> new_outsign_list;
        for (size_t j = 0; j < co.face_id_list.length(); ++j) {
            const size_t fid = co.face_id_list[j];
            new_face_id_list.append(new_face_ids[fid]);
            if (flip_new_face[fid]) {
                new_outsign_list.append(-co.outsign_list[j]);
            } else {
                new_outsign_list.append(co.outsign_list[j]);
            }
        }
        cells.append(std::make_shared<USGCell>(co.cell_type, i, new_vtx_id_list.all(),
                                               new_face_id_list.all(), new_outsign_list.all()));
    }
    ncells = cells.length();

    for (size_t i = 0; i < faces.length(); ++i) { // D:2329
        faces[i]->left_cell = nullptr;
        faces[i]->right_cell = nullptr;
    }
    for (size_t i = 0; i < cells.length(); ++i) { // D:2330-2345
        for (size_t j = 0; j < cells[i]->face_id_list.length(); ++j) {
            const size_t fid = cells[i]->face_id_list[j];
            switch (cells[i]->outsign_list[j]) {
            case 1:
                eil_assert_msg(!faces[fid]->left_cell,
                               "Oops, seem to already have a cell on left."); // D:2334
                faces[fid]->left_cell = cells[i];
                break;
            case -1:
                eil_assert_msg(!faces[fid]->right_cell,
                               "Oops, seem to already have a cell on right."); // D:2338
                faces[fid]->right_cell = cells[i];
                break;
            default:
                EIL_THROW_GEOM("Oops, we seem to have an invalid value for outsign."); // D:2342
            }
        }
    }
    for (size_t i = 0; i < faces.length(); ++i) { // D:2346-2350
        if (!faces[i]->left_cell && !faces[i]->right_cell) {
            eil::writefln("Warning, face[%d] in the merged collection is unattached.", i);
        }
    }
    // D:2357-2379: drop the faces that have become internal.
    eil::Array<std::shared_ptr<BoundaryFaceSet>> new_boundaries;
    for (size_t i = 0; i < boundaries.length(); ++i) {
        eil::Array<size_t> new_face_id_list;
        eil::Array<int> new_outsign_list;
        BoundaryFaceSet& b = *boundaries[i];
        for (size_t j = 0; j < b.face_id_list.length(); ++j) {
            const size_t fid = b.face_id_list[j];
            const USGFace& fo = *faces[fid];
            if (fo.left_cell && fo.right_cell) {
                // Omit the face from the boundary set.
            } else {
                new_face_id_list.append(fid);
                new_outsign_list.append(b.outsign_list[j]);
            }
        }
        if (openFoam) {
            b.face_id_list = new_face_id_list;   // D:2371 (mutates the original set)
            b.outsign_list = new_outsign_list;
        } else if (new_face_id_list.length() != 0) {
            new_boundaries.append(std::make_shared<BoundaryFaceSet>(
                b.tag, new_face_id_list.all(), new_outsign_list.all()));
        }
    }
    if (!openFoam) boundaries = new_boundaries; // D:2379

    for (size_t i = 0; i < other->boundaries.length(); ++i) { // D:2384-2410
        const BoundaryFaceSet& bo = *other->boundaries[i];
        eil::Array<size_t> new_face_id_list;
        eil::Array<int> new_outsign_list;
        for (size_t j = 0; j < bo.face_id_list.length(); ++j) {
            const size_t fid = bo.face_id_list[j];
            const USGFace& fo = *faces[new_face_ids[fid]];
            if (fo.left_cell && fo.right_cell) {
                // Omit.
            } else {
                new_face_id_list.append(new_face_ids[fid]);
                if (flip_new_face[fid]) {
                    new_outsign_list.append(-bo.outsign_list[j]);
                } else {
                    new_outsign_list.append(bo.outsign_list[j]);
                }
            }
        }
        if (openFoam || new_face_id_list.length() != 0) {
            boundaries.append(std::make_shared<BoundaryFaceSet>(
                bo.tag, new_face_id_list.all(), new_outsign_list.all()));
        }
    }
    nboundaries = boundaries.length();
    // D:2413 returns `this`, the same handle, so chained calls mutate the master. The
    // port returns a non-owning alias of the same object, which is what D's handle is.
    return std::shared_ptr<UnstructuredGrid>(std::shared_ptr<UnstructuredGrid>(), this);
}

// ---- usgrid.d:2416-2448 writeStats ----
void UnstructuredGrid::writeStats(double relTol, double absTol) {
    eil::writefln("UnstructuredGrid: label=%s", label);                 // D:2418
    eil::writefln("  nvertices=%d nfaces=%d ncells=%d", vertices.length(), faces.length(),
                  cells.length());                                       // D:2419
    for (size_t i = 0; i < boundaries.length(); ++i) {
        eil::writefln("  BoundarySet[%d] tag=%s nfaces=%d", i, boundaries[i]->tag,
                      boundaries[i]->face_id_list.length());              // D:2421
    }
    eil::writeln("Check that each of the cells is closed.");             // D:2423
    bool allOK = true;
    for (size_t i = 0; i < cells.length(); ++i) {
        Vector3 varea = vectorAreaOfCell(*cells[i]);
        if (!approxEqualVectors(varea, Vector3(0, 0, 0), relTol, absTol)) {
            eil::writefln("  cell id=%d, non-zero vector area=%s", i, varea.toString());
            allOK = false;
        }
    }
    if (allOK) { eil::writeln("  OK"); }                                  // D:2432
    eil::writeln("Check that the region bounded by the grid is closed."); // D:2434
    Vector3 varea = Vector3(0, 0, 0);
    for (size_t bi = 0; bi < boundaries.length(); ++bi) {
        const BoundaryFaceSet& b = *boundaries[bi];
        for (size_t j = 0; j < b.face_id_list.length(); ++j) {
            varea += static_cast<number>(b.outsign_list[j]) *
                     vectorAreaOfFacet(faces[b.face_id_list[j]]->vtx_id_list.all());
        }
    }
    if (!approxEqualVectors(varea, Vector3(0, 0, 0), relTol, absTol)) {
        // D:2442 writeln("  Region non-zero bounding vector area=", varea) — two
        // arguments, concatenated with no separator, the second through Vector3's
        // toString.
        eil::write("  Region non-zero bounding vector area=");
        eil::writeln(varea.toString());
    } else {
        eil::writeln("  OK");
    }
}

// ---- usgrid.d:2450-2477 getVertexIdsOnBoundaries ----
eil::Array<size_t> UnstructuredGrid::getVertexIdsOnBoundaries() const {
    size_t n_boundary_vtxs = 0;                                        // D:2455-2460
    for (size_t bi = 0; bi < boundaries.length(); ++bi) {
        const BoundaryFaceSet& b = *boundaries[bi];
        for (size_t q = 0; q < b.face_id_list.length(); ++q) {
            n_boundary_vtxs += faces[b.face_id_list[q]]->vtx_id_list.length();
        }
    }
    eil::Array<size_t> vtx_ids_boundary;
    set_length_d(vtx_ids_boundary, n_boundary_vtxs);                      // D:2464
    size_t j = 0;
    for (size_t bi = 0; bi < boundaries.length(); ++bi) {
        const BoundaryFaceSet& b = *boundaries[bi];
        for (size_t q = 0; q < b.face_id_list.length(); ++q) {
            const eil::Array<size_t>& ids = faces[b.face_id_list[q]]->vtx_id_list;
            for (size_t k = 0; k < ids.length(); ++k) {
                vtx_ids_boundary[j] = ids[k];
                j += 1;
            }
        }
    }
    return d_sorted_uniq(vtx_ids_boundary);                            // D:2475
}
//<<<APPEND-NOW>>>

} // namespace geom
} // namespace eil
