// tests/gpath3_dump.hpp — shared replay machinery for the geom/gpath stage-3
// verification tests (helix, polyline, modifiedpath, xspline, xsplinelsq).
//
// Not a D module: helpers only, in an internal namespace so this stage's tests stay
// self-contained and cannot collide with the gpath1/gpath2 stages (which have their
// own copy of the same machinery). The line-emitting functions here are the C++ side
// of probes/geom/gpath/pathdump.d, which is the D side; the two are line-for-line
// mirrors, so if you change one, change the other and re-capture the baseline.
//
// The t sweep is not arbitrary (see the note at the head of pathdump.d): Path.dpdt
// (path.d:40-53) and Path.d2pdt2 (path.d:62-77) branch on `t+dt > 1.0` and
// `t-dt < 0.0` with dt = 0.001, and 0.999+0.001 rounds to exactly 1.0, so the test
// is `>` and not `>=`. The sweep therefore carries t = 0.0 and t = 1.0 (the two
// boundaries), 0.0005 (forward one-sided), 0.001 and 0.999 (the two values sitting
// exactly on the FP edge, which take the central branch) and 0.9995 (backward
// one-sided).
#pragma once

#include <cfloat>
#include <sstream>
#include <cctype>
#include <cstdarg>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"

namespace eil_test_gpath3 {

using eil::geom::Path;
using eil::geom::PathPtr;
using eil::geom::Vector3;
using eil::number;

// ---------------------------------------------------------------- produced lines
inline std::vector<std::string>& produced() {
    static std::vector<std::string> lines;
    return lines;
}

inline void emitf(const char* fmt, ...) {
    char buf[8192];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    produced().emplace_back(buf);
}

// The mirror of dump_string() in probe_polyline.d: a tag line, the raw multi-line
// text, then the closing tag line. D writes the block with write(s), so every line
// of s becomes a line of the corpus; s ends with '\n', so the empty tail after the
// final newline is dropped.
inline void dump_string(const char* tag, const std::string& s) {
    emitf("%s begin", tag);
    size_t start = 0;
    while (start < s.size()) {
        size_t nl = s.find('\n', start);
        if (nl == std::string::npos) {
            produced().push_back(s.substr(start));
            break;
        }
        produced().push_back(s.substr(start, nl - start));
        start = nl + 1;
    }
    emitf("%s end", tag);
}

// ------------------------------------------------------------------- live checks
inline int& checks() {
    static int n = 0;
    return n;
}
inline int& failures() {
    static int n = 0;
    return n;
}

inline void check(bool cond, const char* what, int line) {
    ++checks();
    if (!cond) {
        ++failures();
        std::printf("FAIL line %d: %s\n", line, what);
    }
}

#define CHECK3(c, msg) eil_test_gpath3::check((c), msg, __LINE__)

inline const char* bools(bool b) { return b ? "true" : "false"; }

// ------------------------------------------------------------------- number scan
struct Field {
    std::string key;   // the "length" in "... length=2.5847 ...", or "" if positional
    double value;
};

// Same extraction as the geom/elements exemplar: every `key=<numeric token>` pair,
// in line order. Booleans ("true"/"false") and words are skipped because they do
// not start with a digit, sign, 'n' (nan) or 'i' (inf).
inline std::vector<Field> numbers_in(const std::string& s) {
    std::vector<Field> out;
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] != '=') { ++i; continue; }
        size_t ks = s.rfind(' ', i);
        ks = (ks == std::string::npos) ? 0 : ks + 1;
        const std::string key = s.substr(ks, i - ks);
        size_t b = i + 1;
        size_t e = s.find(' ', b);
        if (e == std::string::npos) e = s.size();
        const std::string tok = s.substr(b, e - b);
        if (!tok.empty()) {
            const char c = tok[0];
            if (c == '-' || c == '+' || std::isdigit(static_cast<unsigned char>(c)) || c == 'n' ||
                c == 'i') {
                char* endp = nullptr;
                const double v = std::strtod(tok.c_str(), &endp);
                if (endp == tok.c_str() + tok.size()) out.push_back({key, v});
            }
        }
        i = b;
    }
    return out;
}

inline long long ulp_gap(double a, double b) {
    int64_t ia, ib;
    std::memcpy(&ia, &a, 8);
    std::memcpy(&ib, &b, 8);
    if (ia < 0) ia = INT64_MIN - ia;
    if (ib < 0) ib = INT64_MIN - ib;
    return ia > ib ? ia - ib : ib - ia;
}

// Two NaNs of the same sign are the same observable value: %.17g prints both as
// "nan"/"-nan" and the payload bits are not part of D's contract (the sign is, and
// signbit() above keeps that distinction).
inline bool same_quantity(double a, double b) {
    if (std::memcmp(&a, &b, 8) == 0) return true;
    if (std::isnan(a) && std::isnan(b)) return std::signbit(a) == std::signbit(b);
    return false;
}


// ------------------------------------------------------------- line verdict
// A line is EXACT when it is byte-identical. Otherwise it is WITHIN_BAND only if
// every whitespace-separated field matches byte for byte except fields of the form
// key=<number>, and each such number differs by no more than
// k * DBL_EPSILON * (largest magnitude on the D side of that line). Anything else
// is DIFFERS.
//
// Why the band is absolute and not relative: D and glibc differ by up to 1 ULP on
// transcendentals (measured, reports/phase-0.md), and a Path length sums 100
// sampled segments, so 1 ULP of trig becomes a couple of ULP in the sum. Several
// observables here are cancellation residuals: RotatedAboutZAxisPath's x at
// t=0.0005 is -2.18089e-07 while the terms that produced it are of order 2.8, so
// 6.3e-16 of absolute error is 2.9% *relative* to the answer and 1 ULP of the
// operands. A relative gate would call that a failure; it is not one, and the
// worst ratio it accepts is reported rather than hidden.
enum class Line_verdict { EXACT, WITHIN_BAND, DIFFERS };

inline bool numeric_field(const std::string& tok, std::string& key, double& v) {
    const size_t eq = tok.find('=');
    if (eq == std::string::npos) return false;
    key = tok.substr(0, eq);
    const std::string rhs = tok.substr(eq + 1);
    if (rhs.empty()) return false;
    const char c = rhs[0];
    if (!(c == '-' || c == '+' || std::isdigit(static_cast<unsigned char>(c)) || c == 'n' ||
          c == 'i')) return false;
    char* endp = nullptr;
    v = std::strtod(rhs.c_str(), &endp);
    return endp == rhs.c_str() + rhs.size();
}

inline std::vector<std::string> ws_split(const std::string& s) {
    std::vector<std::string> out;
    std::istringstream ss(s);
    std::string t;
    while (ss >> t) out.push_back(t);
    return out;
}

// Returns the verdict and the worst ratio (in eighths of a ULP-of-row-scale, so 8
// means exactly 1 ULP) for lines that are not byte-identical.
inline std::pair<Line_verdict, long long> verdict_line(const std::string& d, const std::string& c,
                                                       double k, double ref_scale) {
    if (d == c) return {Line_verdict::EXACT, 0};
    const std::vector<std::string> td = ws_split(d), tc = ws_split(c);
    if (td.size() != tc.size()) return {Line_verdict::DIFFERS, 0};
    double rowmax = 0.0;
    for (const std::string& t : td) {
        std::string key;
        double v = 0.0;
        if (numeric_field(t, key, v)) rowmax = std::fmax(rowmax, std::fabs(v));
    }
    // Finite-difference observables inherit the underlying point error divided by
    // dt^order: Path::dpdt uses dt = 1.0e-3 (geom/gpath/path.d:37) and d2pdt2 the
    // same (path.d:60), so one ULP of a point coordinate of order 2.83 becomes
    // 6.28e-16 in opCall, 6.28e-13 in dpdt and 6.28e-10 in d2pdt2. Measured against
    // the mp.rot rows of geom_gpath_modifiedpath.txt, where each observed gap is
    // 1.000x this band at orders 0, 1 and 2 respectively.
    int fd_order = 0;
    if (d.find("d2pdt2") != std::string::npos) fd_order = 2;
    else if (d.find("dpdt") != std::string::npos) fd_order = 1;
    double fd_scale = 1.0;
    for (int o = 0; o < fd_order; ++o) fd_scale *= 1.0 / 1.0e-3;  // path.d:37,60
    // ref_scale is the largest magnitude anywhere in this module's corpus, which is
    // a lower bound on the operands that produced the row. A cancellation residual
    // like RotatedAboutZAxisPath's x = -2.18e-07 has a rowmax of 2.83 (its y) and
    // passes, but a row whose own fields are all ~4e-4 still inherits the error of
    // the metre-scale coordinates it came from; without a floor the band would be
    // tighter than the arithmetic and, worse, meaningless.
    const double scale =
        std::fmax(std::fmax(rowmax, ref_scale), 1.0e-30);
    const double band = k * DBL_EPSILON * scale * fd_scale;
    long long worst_eighths = 0;
    bool any_band = false;
    for (size_t i = 0; i < td.size(); ++i) {
        if (td[i] == tc[i]) continue;
        std::string kd, kc;
        double vd = 0.0, vc = 0.0;
        if (!numeric_field(td[i], kd, vd) || !numeric_field(tc[i], kc, vc) || kd != kc)
            return {Line_verdict::DIFFERS, 0};
        const double diff = std::fabs(vd - vc);
        if (!(diff <= band) || std::isnan(vd) != std::isnan(vc))
            return {Line_verdict::DIFFERS, 0};
        any_band = true;
        const long long e = static_cast<long long>(diff / (DBL_EPSILON *
                                             (rowmax > 0.0 ? rowmax : 1.0)) * 8.0) + 1;
        if (e > worst_eighths) worst_eighths = e;
    }
    return {any_band ? Line_verdict::WITHIN_BAND : Line_verdict::DIFFERS, worst_eighths};
}

// ----------------------------------------------------------------------- report
// Replays `produced()` against the D baseline and prints the summary the stage
// requires: "N of M quantities bit-identical, worst residual X (Y ULP) on quantity
// Z". Returns the process exit code.
// `note` is printed after the summary when lines differ, so a failure carries its
// own diagnosis instead of just a count. Pass "" when there is nothing to add.
inline int report(const char* module, const char* baseline_path, const char* note = "") {
    std::ifstream in(baseline_path);
    if (!in) {
        std::cerr << "cannot open baseline " << baseline_path << "\n";
        return 2;
    }
    std::vector<std::string> expected;
    for (std::string line; std::getline(in, line);) expected.push_back(line);

    int line_diffs = 0;
    int band_lines = 0, band_shown = 0;
    long long worst_band_eighths = 0;
    size_t quantities = 0, identical = 0;
    long long worst_ulp = 0;
    double worst_abs = 0.0;
    std::string worst_where;

    double ref_scale = 0.0;
    for (const std::string& l : expected) {
        for (const std::string& t : ws_split(l)) {
            std::string key;
            double v = 0.0;
            if (numeric_field(t, key, v)) ref_scale = std::fmax(ref_scale, std::fabs(v));
        }
    }

    const std::vector<std::string>& got = produced();
    for (size_t i = 0; i < expected.size() || i < got.size(); ++i) {
        Line_verdict vv = Line_verdict::EXACT;
        long long v_eighths = 0;
        if (i < expected.size() && i < got.size()) {
            const std::pair<Line_verdict, long long> v = verdict_line(expected[i], got[i], 4.0, ref_scale);
            vv = v.first;
            v_eighths = v.second;
            if (vv == Line_verdict::WITHIN_BAND) {
                ++band_lines;
                if (v_eighths > worst_band_eighths) worst_band_eighths = v_eighths;
                if (band_shown < 8) {
                    std::cout << "within band (line " << (i + 1) << ", " << v_eighths
                              << "/8 ULP of row scale)\n  D:   |" << expected[i]
                              << "|\n  C++: |" << got[i] << "|\n";
                    ++band_shown;
                }
            }
        }
        if (vv == Line_verdict::DIFFERS || i >= expected.size() || i >= got.size()) {
            if (line_diffs < 20) {
                std::cout << "line " << (i + 1) << "\n  D:   |"
                          << (i < expected.size() ? expected[i] : std::string("<none>"))
                          << "|\n  C++: |" << (i < got.size() ? got[i] : std::string("<none>"))
                          << "|\n";
            }
            ++line_diffs;
        }
        if (i < expected.size() && i < got.size()) {
            const std::vector<Field> a = numbers_in(expected[i]);
            const std::vector<Field> b = numbers_in(got[i]);
            const size_t n = a.size() < b.size() ? a.size() : b.size();
            for (size_t k = 0; k < n; ++k) {
                ++quantities;
                if (same_quantity(a[k].value, b[k].value)) {
                    ++identical;
                    continue;
                }
                const long long gap = ulp_gap(a[k].value, b[k].value);
                if (gap > worst_ulp) {
                    worst_ulp = gap;
                    worst_abs = std::fabs(a[k].value - b[k].value);
                    worst_where = expected[i].substr(0, expected[i].find(' ')) + "." + a[k].key;
                }
            }
        }
    }

    // Debugging aid: EIL_DUMP_PRODUCED=<file> writes the C++ corpus so a diff can
    // be inspected in full instead of only the first 20 differing lines.
    if (const char* dump = std::getenv("EIL_DUMP_PRODUCED")) {
        std::ofstream out(dump);
        for (const std::string& l : got) out << l << "\n";
    }

    const bool ok = (line_diffs == 0 && failures() == 0);
    std::cout << (ok ? "PASS" : "FAIL") << ": geom_gpath_" << module << ", "
              << checks() - failures() << "/" << checks()
              << " unittest assertions hold; probe replay " << expected.size() << " D lines, "
              << got.size() << " C++ lines, " << line_diffs << " differ; " << identical << " of "
              << quantities << " quantities bit-identical"
              << (band_lines ? ", " + std::to_string(band_lines) + " line(s) accepted through the "
                                "absolute band (worst " +
                                std::to_string(worst_band_eighths) + "/8 ULP of row scale)"
                              : "");
    if (worst_ulp > 0) {
        char res[256];
        std::snprintf(res, sizeof res,
                      ", worst residual %.6g (%lld ULP) on quantity %s", worst_abs,
                      static_cast<long long>(worst_ulp), worst_where.c_str());
        std::cout << res;
    }
    // The ULP distance between two NaNs, or between a value and one that came out
    // of a different cancellation, is meaningless without the relative scale; the
    // field named in worst_where is the one to look at first.
    (void)0;
    std::cout << "\n";
    if (!ok && note[0] != '\0') std::cout << "NOTE: " << note << "\n";
    return ok ? 0 : 1;
}

// --------------------------------------------------------------------- dump_path
// The exact mirror of dump_path() in probes/geom/gpath/pathdump.d.
inline std::vector<double> t_sweep() {
    return {0.0, 0.0005, 0.001, 0.05, 0.25, 0.5, 0.75, 0.95, 0.999, 0.9995, 1.0};
}

inline std::vector<Vector3> query_points() {
    return {Vector3(0.5, 0.5, 0.0), Vector3(0.0, 0.0, 0.0), Vector3(1.0, 1.0, 1.0),
            Vector3(2.0, 0.0, 1.0)};
}

inline void dump_path(const char* name, const Path& pth) {
    emitf("%s class=%s", name, pth.classString().c_str());
    emitf("%s str=%s", name, pth.toString().c_str());
    emitf("%s length=%.17g", name, eil::re(pth.length()));

    for (double t : t_sweep()) {
        Vector3 p = pth(t);
        emitf("%s opCall t=%.17g x=%.17g y=%.17g z=%.17g", name, t, p.x, p.y, p.z);
        Vector3 d1 = pth.dpdt(t);
        emitf("%s dpdt t=%.17g x=%.17g y=%.17g z=%.17g", name, t, d1.x, d1.y, d1.z);
        Vector3 d2 = pth.d2pdt2(t);
        emitf("%s d2pdt2 t=%.17g x=%.17g y=%.17g z=%.17g", name, t, d2.x, d2.y, d2.z);
        emitf("%s plen ta=%.17g tb=%.17g L=%.17g", name, 0.5 * t, t,
              eil::re(pth.partial_length(0.5 * t, t)));
        // The swapped call exercises the tb < ta exchange at path.d:82-84.
        emitf("%s plenswap ta=%.17g tb=%.17g L=%.17g", name, t, 0.5 * t,
              eil::re(pth.partial_length(t, 0.5 * t)));
    }

    number L = pth.length();
    for (double frac : {0.0, 0.25, 0.5, 0.75, 0.999, 1.0, 1.5}) {
        double tt = eil::d_init<double>(); // D: `double tt;` is double.init
        Vector3 p = pth.point_from_length(frac * L, tt);
        emitf("%s pfl target=%.17g t=%.17g x=%.17g y=%.17g z=%.17g", name, frac * eil::re(L), tt,
              p.x, p.y, p.z);
    }

    for (const Vector3& q : query_points()) {
        double tt = eil::d_init<double>();
        double dist = pth.closestDistance(q, tt);
        emitf("%s cd px=%.17g py=%.17g pz=%.17g t=%.17g dist=%.17g", name, q.x, q.y, q.z, tt,
              dist);
    }

    // intersect2D with the two sweeps D's own gpath unittests use (line.d:76-83 and
    // arc.d:176-183), once with the default nseg = 20 and once with 10.
    Vector3 ps = Vector3(0.5, 0.5, 0.0);
    Vector3 dir = Vector3(0.0, 1.0, 0.0);
    double tt = eil::d_init<double>();
    bool found = pth.intersect2D(ps, dir, tt);
    emitf("%s i2d psx=%.17g psy=%.17g dx=%.17g dy=%.17g found=%s t=%.17g", name, ps.x, ps.y, dir.x,
          dir.y, bools(found), tt);
    double tt10 = eil::d_init<double>();
    bool found10 = pth.intersect2D(ps, dir, tt10, 10);
    emitf("%s i2d10 found=%s t=%.17g", name, bools(found10), tt10);
    Vector3 ps2 = Vector3(0.25, 0.0, 0.0);
    double tt2 = eil::d_init<double>();
    bool found2 = pth.intersect2D(ps2, dir, tt2);
    emitf("%s i2db found=%s t=%.17g", name, bools(found2), tt2);
}

inline void dump_path(const char* name, const PathPtr& pth) { dump_path(name, *pth); }

// The mirror of dump_error() in pathdump.d: which side of the Error/Exception
// sibling pair caught, and the message when the message is reproducible.
struct ErrorTaxonomy {
    bool asError = false;
    bool asException = false;
    std::string msg;
};

template <typename F>
ErrorTaxonomy classify(F&& stmt, bool capture_msg = true) {
    ErrorTaxonomy out;
    try {
        stmt();
    } catch (const eil::ErrorBase& e) {
        out.asError = true;
        out.msg = capture_msg ? e.msg() : "";
    } catch (const eil::ExceptionBase& e) {
        out.asException = true;
        out.msg = capture_msg ? e.msg() : "";
    }
    return out;
}

inline void dump_error(const char* tag, const ErrorTaxonomy& e) {
    emitf("%s asError=%s asException=%s msg=%s", tag, bools(e.asError), bools(e.asException),
          e.msg.c_str());
}

// D prints a Vector3 through its toString(); the corpus rows that carry one need
// the same bytes.
inline const char* vstr(const Vector3& v, std::string& scratch) {
    scratch = v.toString();
    return scratch.c_str();
}

} // namespace eil_test_gpath3
