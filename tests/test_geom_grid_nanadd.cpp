// tests/test_geom_grid_nanadd.cpp — C++ mirror of probes/geom/grid/probe_grid_nanadd.d,
// replayed row for row against baselines/e587b235/probes/geom_grid_nanadd.txt
// (638 rows captured from ldc2 1.40.0 at gdtk e587b235, with NO -O flag, i.e. -O0).
//
// This is the T-919 gate. geom_grid_grid row 178 (`cbc.nan2d x=-nan') was the only
// red row in the suite and only in Release, because an SSE add whose two operands are
// both NaN returns the NaN sitting in the *destination register*, and which value the
// compiler puts there is register allocation, not arithmetic. ldc2 -O0 and g++ -O0 put
// the addend there; g++ -O1..-O3 put the receiver there. The port therefore printed
// "nan" where the -O0 capture says "-nan".
//
// The fix is eil::add_rhs_nan / eil::mul_rhs_nan (substrate/include/eil/math.hpp),
// applied to Vector3::add(7 overloads), Vector3::scale and Vector3::operator+= /
// operator*=, which writes the -O0 choice out instead of inheriting it. This corpus is
// what pins that choice: every row is a raw 64-bit pattern, so the test says which
// NaN won, not just whether the text differs. operator+ / operator- / operator-= /
// operator/= are replayed here too and are deliberately NOT pinned — measured, their
// choice does not move with -O in either language.
//
// Row groups (see the probe header for the full contract):
//   nan.init      polarity of double.nan / -double.nan / double.init / Vector3.init,
//                 the folded 1.0/0 and 1.0/3.0, the %.17g text, and the NaN that
//                 0.0*+inf generates (0xfff8000000000000 — the hardware default).
//   nan.walk      grid.d:231-241's statement order with a print after EVERY step, for
//                 the row-178 cell and its sign variants, the nvtx=1 and nvtx=0 cells,
//                 and the 0.0 times 1.0/0 case.
//   nan.<op>      49 operand pairs x 11 operations, result bits.
//
// The whole corpus is byte-identical at -O0, -O1, -O2 and -O3 in this build, which is
// the property the pin exists to buy.
//
// Regenerate the baseline:
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/grid
//     probes/geom/grid/probe_grid_nanadd.d -of=/tmp/probe_g1nan -od=/tmp/probe_obj_g1nan
//     and then /tmp/probe_g1nan > baselines/e587b235/probes/geom_grid_nanadd.txt
//
// Run: ./test_geom_grid_nanadd <baseline.txt>
#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include "eil/format.hpp"
#include "eil/math.hpp"
#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"

using eil::geom::Vector3;

static std::vector<std::string> g_lines;

static void emit(const std::string& s) { g_lines.push_back(s); }

static std::string line(const char* f, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return std::string(buf);
}

static uint64_t bits(double d) {
    uint64_t u;
    std::memcpy(&u, &d, sizeof u);
    return u;
}

static double frombits(uint64_t u) {
    double d;
    std::memcpy(&d, &u, sizeof d);
    return d;
}

// 7 patterns, in the same order as the probe's PAT.
static const uint64_t PAT[7] = {
    0x7ff8000000000000ull, 0xfff8000000000000ull, 0x7ff8000000000001ull,
    0x7ff0000000000001ull, 0xfff0000000000002ull, 0x3ff0000000000000ull,
    0x7ff0000000000000ull,
};

static void show(const std::string& tag, const Vector3& v) {
    emit(line("%s x=%016llx y=%016llx z=%016llx", tag.c_str(), (unsigned long long)bits(v.x),
              (unsigned long long)bits(v.y), (unsigned long long)bits(v.z)));
}

// grid.d:231-241 verbatim, one print per step.
static Vector3 walk(const std::string& tag, const std::vector<Vector3>& verts, size_t nvtx) {
    Vector3 cbc;
    show(tag + " step0.init", cbc);
    cbc.set(verts[0]);
    show(tag + " step1.set", cbc);
    for (size_t i = 1; i < nvtx; ++i) {
        cbc.add(verts[i]);
        show(tag + line(" step2.add%zu", i), cbc);
    }
    cbc.scale(1.0 / static_cast<double>(nvtx));
    show(tag + " step3.scale", cbc);
    return cbc;
}

// Out-of-line so 1.0/0 and 0.0/0.0 go through the hardware at run time rather than
// being folded to a compile-time constant: the folded payload is not what the
// reference prints, and nan.walk's `0 times inf' row shows the hardware default quiet
// NaN here is the NEGATIVE one, 0xfff8000000000000.
__attribute__((noinline)) static double divide(double a, double b) { return a / b; }

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }

    //---------------------------- nan.init
    const double dnan = frombits(0x7ff8000000000000ull);  // D double.nan
    emit(line("nan.init double.nan %016llx", (unsigned long long)bits(dnan)));
    emit(line("nan.init -double.nan %016llx", (unsigned long long)bits(-dnan)));
    emit(line("nan.init double.init %016llx", (unsigned long long)bits(eil::d_init<double>())));
    Vector3 vi;
    emit(line("nan.init Vector3.init %016llx %016llx %016llx",
              (unsigned long long)bits(vi.x), (unsigned long long)bits(vi.y),
              (unsigned long long)bits(vi.z)));
    Vector3 vc = Vector3(dnan, 0.0, 0.0);
    emit(line("nan.init Vector3(nan,0,0) %016llx", (unsigned long long)bits(vc.x)));
    emit(line("nan.init 1.0/0 %016llx 1.0/3 %016llx",
              (unsigned long long)bits(divide(1.0, 0.0)),
              (unsigned long long)bits(divide(1.0, 3.0))));
    emit(eil::format("nan.init text %.17g %.17g %.17g", dnan, -dnan, divide(0.0, 0.0)));

    //---------------------------- nan.walk
    Vector3 vNaN = Vector3(dnan, 0.0, 0.0);
    Vector3 vNeg = Vector3(-dnan, 1.0, 0.0);
    Vector3 vOne = Vector3(1.0, 0.0, 0.0);
    Vector3 vNaNy = vNaN;
    vNaNy.y = dnan;
    Vector3 vNegy = vNeg;
    vNegy.y = -dnan;
    walk("nan.walk nan2d", {vNaN, vNeg, vOne}, 3);
    walk("nan.walk nan2d swapped", {vNeg, vNaN, vOne}, 3);
    walk("nan.walk same sign", {vNaN, Vector3(dnan, 1.0, 0.0), vOne}, 3);
    walk("nan.walk nan in slot2", {vOne, vNaN, vNeg}, 3);
    walk("nan.walk two nan", {vNaN, vNeg, Vector3(-dnan, 0.0, 1.0)}, 3);
    walk("nan.walk nan y", {vNaNy, vNegy}, 2);
    walk("nan.walk nvtx1", {vNaN}, 1);
    walk("nan.walk nvtx0 inf", {vNaN}, 0);
    walk("nan.walk 0 times inf", {Vector3(0.0, 1.0, 0.0)}, 0);
    walk("nan.walk nan2d x only", {vNaN, Vector3(dnan, 2.0, 0.0),
                                   Vector3(dnan, 3.0, 0.0)}, 3);

    //---------------------------- the 49-case operand matrix
    for (int i = 0; i < 7; ++i) {
        for (int j = 0; j < 7; ++j) {
            const uint64_t a = PAT[i], b = PAT[j];
            emit(line("nan.pair %016llx %016llx", (unsigned long long)a,
                      (unsigned long long)b));
            Vector3 A = Vector3(frombits(a), 0.0, 0.0);
            Vector3 B = Vector3(frombits(b), 0.0, 0.0);
            Vector3 v = A; v.add(B);            // add(ref const(Vector3))
            Vector3 w = A; w.add(&B);           // add(Vector3*)
            Vector3 e = A; e.add(B, 2.0);       // add(other, factor)
            Vector3 p = A; p += B;              // operator+=
            Vector3 q = A + B;                  // operator+
            Vector3 s = A - B;                  // operator-
            Vector3 m = A; m -= B;              // operator-=
            Vector3 f = A; f.scale(frombits(b));   // scale(number)
            Vector3 g = A; g *= frombits(b);       // operator*=
            Vector3 h = A; h /= frombits(b);       // operator/=
            Vector3 d = A / frombits(b);           // operator/
            emit(line("nan.add.ref %016llx", (unsigned long long)bits(v.x)));
            emit(line("nan.add.ptr %016llx", (unsigned long long)bits(w.x)));
            emit(line("nan.add.fac %016llx", (unsigned long long)bits(e.x)));
            emit(line("nan.opplus_eq %016llx", (unsigned long long)bits(p.x)));
            emit(line("nan.opplus %016llx", (unsigned long long)bits(q.x)));
            emit(line("nan.opminus %016llx", (unsigned long long)bits(s.x)));
            emit(line("nan.opminus_eq %016llx", (unsigned long long)bits(m.x)));
            emit(line("nan.scale %016llx", (unsigned long long)bits(f.x)));
            emit(line("nan.opmul_eq %016llx", (unsigned long long)bits(g.x)));
            emit(line("nan.opdiv_eq %016llx", (unsigned long long)bits(h.x)));
            emit(line("nan.opdiv %016llx", (unsigned long long)bits(d.x)));
        }
    }

    //---------------------------- replay
    std::ifstream in(argv[1]);
    if (!in) {
        std::fprintf(stderr, "cannot open baseline %s\n", argv[1]);
        return 2;
    }
    std::vector<std::string> expected;
    for (std::string l; std::getline(in, l);) expected.push_back(l);

    int diffs = 0, shown = 0;
    size_t matched = 0;
    for (size_t i = 0; i < expected.size() || i < g_lines.size(); ++i) {
        const bool have_e = i < expected.size();
        const bool have_g = i < g_lines.size();
        const std::string e = have_e ? expected[i] : "<none>";
        const std::string g = have_g ? g_lines[i] : "<none>";
        if (have_e && have_g && e == g) { ++matched; continue; }
        if (shown < 12) {
            std::printf("line %zu\n  D:   |%s|\n  C++: |%s|\n", i + 1, e.c_str(), g.c_str());
            ++shown;
        }
        ++diffs;
    }
    std::printf("%s: geom_grid_nanadd, probe replay %zu D lines, %zu C++ lines, %d differ; "
                "%zu of %zu rows byte-identical\n",
                diffs == 0 ? "PASS" : "FAIL", expected.size(), g_lines.size(), diffs, matched,
                expected.size());
    return diffs == 0 ? 0 : 1;
}
