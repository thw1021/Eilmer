// tools/to_double_exposure.cpp — score a D `to!double' capture against what the port uses.
//
// Backlog T-930, route 1 (decided 2026-10-15): the port keeps a correctly rounded parser
// (strtod) and does NOT adopt a fitted replica of Phobos' decimal->80-bit path. This tool is
// what makes that decision re-checkable instead of folklore: it prints, for any corpus of
// D's bits,
//   * how many tokens strtod gets wrong relative to D (the accepted exposure), and
//   * how many a rejected candidate (exact integer mantissa x a long double binary-powered
//     10^e, narrowed) gets wrong, plus how many it gets wrong that strtod got RIGHT.
// That last number is the whole argument against route 2: a model that fixes 13 and breaks 1
// is not a fidelity improvement, it is a different error distribution, and it only holds for
// inputs of at most 19 digits.
//
// Reproduce with probes/substrate/probe_to_double_tokens.d (its header has the pipeline).
// Measured on blk.tmp's 128,346 numeric tokens: strtod_bad=15 (0.0117%, all 15-16 digits),
// model_bad=3, model_only_bad=1 (1.001179321978101: D and strtod 3ff004d49bd53d05,
// model 3ff004d49bd53d06).
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

static std::string tb(double d) {
    unsigned long long u;
    std::memcpy(&u, &d, sizeof u);
    char b[20];
    std::snprintf(b, sizeof b, "%016llx", u);
    return b;
}

static long double pow10_binaryPowered(int e) {
    long double r = 1.0L, t = 10.0L;
    int n = e < 0 ? -e : e;
    while (n) { if (n & 1) r *= t; t *= t; n >>= 1; }
    return e < 0 ? 1.0L / r : r;
}

int main(int argc, char** argv) {
    if (argc < 2) { std::fprintf(stderr, "usage: %s <d-bits.txt>\n", argv[0]); return 2; }
    std::FILE* f = std::fopen(argv[1], "r");
    if (!f) { std::fprintf(stderr, "cannot open %s\n", argv[1]); return 2; }
    char line[1024];
    long n = 0, thrown = 0, st_bad = 0, md_bad = 0, md_only = 0;
    while (std::fgets(line, sizeof line, f)) {
        if (std::strncmp(line, "d src=", 6)) continue;
        char* p = line + 6;
        char* q = std::strstr(p, " bits=");
        char* r = std::strstr(p, " throws=");
        if (!q && !r) continue;
        if (r) { *r = 0; ++n; ++thrown; continue; }
        *q = 0;
        std::string src(p);
        while (!src.empty() && (src.back() == '\n' || src.back() == '\r')) src.pop_back();
        std::string want = std::string(q + 6, 16);
        ++n;
        const double st = std::strtod(src.c_str(), nullptr);
        if (tb(st) != want) ++st_bad;
        // the rejected candidate
        size_t i = (src[0] == '-' || src[0] == '+') ? 1 : 0;
        bool neg = !src.empty() && src[0] == '-';
        long long im = 0; int frac = 0, e10 = 0; bool dot = false, have = false;
        for (; i < src.size(); ++i) {
            const char c = src[i];
            if (c == 'e' || c == 'E') { have = true; e10 = std::atoi(src.c_str() + i + 1); break; }
            if (c == '.') { dot = true; continue; }
            if (dot) ++frac;
            im = im * 10 + (c - '0');
        }
        if (have) e10 -= frac; else e10 = -frac;
        double md = (double)((long double)im * pow10_binaryPowered(e10));
        if (neg) md = -md;
        if (tb(md) != want) { ++md_bad; if (tb(st) == want) ++md_only; }
    }
    std::fclose(f);
    std::printf("tokens=%ld  D threw (overflow text, not inf)=%ld\n", n, thrown);
    std::printf("  strtod (adopted, route 1)  differs from D on %ld (%.4f%%)\n", st_bad,
                n ? 100.0 * st_bad / n : 0.0);
    std::printf("  80-bit replica (rejected)  differs from D on %ld, of which %ld are rows "
                "strtod gets RIGHT\n", md_bad, md_only);
    return 0;
}
