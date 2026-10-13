// tests/test_geom_elements_nomenclature.cpp
//
// geom/elements/nomenclature.d has no `unittest` block (68 lines, all declarations),
// so this test does two things instead:
//   1. asserts the module's observable contract — aliased Face enumerators, the six
//      face names, the 12 accepted spellings of face_index, the VTKElement values,
//      radians() and the Quaternion shape;
//   2. replays probes/geom/elements/probe_nomenclature.d line for line against
//      baselines/e587b235/probes/geom_elements_nomenclature.txt.
//
// The two measurement results that drive the port's shape:
//   * `face_index` throws a D **Error** (nomenclature.d:33). D's `catch (Exception e)`
//     does not catch it, and here the ExceptionBase clause must not fire either — the
//     test catches ExceptionBase first precisely so that a wrong base class shows up
//     as asException=true instead of passing quietly.
//   * `radians` (nomenclature.d:56) computes with D's 80-bit PI and rounds once on
//     return. That differs from the pure-double form for 4 of the 14 angles in the
//     table (30, 60, 1/3 and 1e8 degrees), each by 1 ULP; those rows are in the
//     baseline, so an M_PI-based port fails the diff rather than passing silently.
//
// Run: ./test_geom_elements_nomenclature ../baselines/e587b235/probes/geom_elements_nomenclature.txt

#include <cctype>
#include <cstdarg>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "geom/elements/nomenclature.hpp"

namespace {

int failures = 0;
int checks = 0;

void check(bool cond, const char* what, int line) {
    ++checks;
    if (!cond) {
        ++failures;
        std::printf("FAIL line %d: %s\n", line, what);
    }
}

#define CHECK(c, msg) check((c), msg, __LINE__)

// ---- probe replay -----------------------------------------------------------
std::vector<std::string> produced;

void emitf(const char* fmt, ...) {
    char buf[4096];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    produced.emplace_back(buf);
}

// Every `=<number>` token of a line, in order. Both sides of the comparison go
// through this, so the pairing is structural rather than hand-maintained.
std::vector<double> numbers_in(const std::string& s) {
    std::vector<double> out;
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] != '=') { ++i; continue; }
        size_t b = i + 1;
        size_t e = s.find(' ', b);
        if (e == std::string::npos) e = s.size();
        const std::string tok = s.substr(b, e - b);
        if (!tok.empty()) {
            const char c = tok[0];
            // skip true/false and non-numeric renderings such as Vector3(...)
            if (c == '-' || c == '+' || std::isdigit(static_cast<unsigned char>(c)) ||
                c == 'n' || c == 'i') {
                char* endp = nullptr;
                const double v = std::strtod(tok.c_str(), &endp);
                if (endp == tok.c_str() + tok.size()) out.push_back(v);
            }
        }
        i = b;
    }
    return out;
}

// Distance in ULPs between two doubles, through the sign-magnitude ordering.
long long ulp_gap(double a, double b) {
    int64_t ia, ib;
    std::memcpy(&ia, &a, 8);
    std::memcpy(&ib, &b, 8);
    if (ia < 0) ia = INT64_MIN - ia;
    if (ib < 0) ib = INT64_MIN - ib;
    return ia > ib ? ia - ib : ib - ia;
}

const double kDeg[14] = { 0.0, 30.0, 45.0, 60.0, 90.0, 180.0, 270.0, 360.0, 0.1,
                          1.0 / 3.0, 123.456, 1.0e-8, 1.0e8, 7.0 };

// The pure-double form the D probe prints beside radians(), i.e. what a naive
// M_PI-based port would compute.
double pi_double_form(double d) {
    const double pi = 3.141592653589793;
    return pi * d / 180.0;
}

void replay_probe() {
    using namespace eil::geom;

    emitf("== Face enumerators, both spellings ==");
    emitf("west=%u iminus=%u", static_cast<unsigned>(Face::west),
          static_cast<unsigned>(Face::iminus));
    emitf("east=%u iplus=%u", static_cast<unsigned>(Face::east),
          static_cast<unsigned>(Face::iplus));
    emitf("south=%u jminus=%u", static_cast<unsigned>(Face::south),
          static_cast<unsigned>(Face::jminus));
    emitf("north=%u jplus=%u", static_cast<unsigned>(Face::north),
          static_cast<unsigned>(Face::jplus));
    emitf("bottom=%u kminus=%u", static_cast<unsigned>(Face::bottom),
          static_cast<unsigned>(Face::kminus));
    emitf("top=%u kplus=%u", static_cast<unsigned>(Face::top),
          static_cast<unsigned>(Face::kplus));

    emitf("== face_index over all 12 accepted names ==");
    const char* names[12] = { "west", "iminus", "east", "iplus", "south", "jminus",
                              "north", "jplus", "bottom", "kminus", "top", "kplus" };
    for (const char* n : names) {
        emitf("face_index(%s)=%u", n, face_index(n));
    }

    emitf("== face_name global ==");
    emitf("length=%u", static_cast<unsigned>(face_name.length()));
    for (size_t i = 0; i < face_name.length(); ++i) {
        emitf("face_name[%u]=%s", static_cast<unsigned>(i), face_name[i].c_str());
    }
    emitf("array=%s", eil::to_string(face_name).c_str());
    emitf("roundtrip=%u %u %u %u %u %u", face_index(face_name[0]), face_index(face_name[1]),
          face_index(face_name[2]), face_index(face_name[3]), face_index(face_name[4]),
          face_index(face_name[5]));

    emitf("== face_index failure path: which catch clause fires ==");
    const char* bad[3] = { "northwest", "", "West" };
    for (const char* b : bad) {
        bool caughtException = false;
        bool caughtError = false;
        std::string msg;
        try {
            face_index(b);
        } catch (const eil::ExceptionBase& e) {
            // D's `catch (Exception e)` does not reach here for an Error; this
            // clause is first so that a mis-ported base class is visible.
            caughtException = true;
            msg = e.msg();
        } catch (const eil::ErrorBase& e) {
            caughtError = true;
            msg = e.msg();
        }
        emitf("bad[%s] asException=%s asError=%s msg=%s", b,
              eil::to_string(caughtException).c_str(), eil::to_string(caughtError).c_str(),
              msg.c_str());
    }

    emitf("== radians: 80-bit PI, rounded on return ==");
    for (double d : kDeg) {
        const double wide = radians(d);
        const double narrow = pi_double_form(d);
        emitf("rad %.17g d17=%.17g narrow17=%.17g same=%s d6=%s tostr=%s", d, wide, narrow,
              eil::to_string(wide == narrow).c_str(), eil::to_string(wide).c_str(),
              eil::to_string(wide).c_str());
    }
    emitf("PI_bits %.17Lg %.17g", static_cast<long double>(EIL_PI),
          static_cast<double>(EIL_PI));
    emitf("PI4 %.17g %.17g", static_cast<double>(EIL_PI / 4.0L), radians(45.0));

    emitf("== VTKElement values ==");
    emitf("vertex=%u polyvertex=%u line=%u polyline=%u", static_cast<unsigned>(VTKElement::vertex),
          static_cast<unsigned>(VTKElement::polyvertex), static_cast<unsigned>(VTKElement::line),
          static_cast<unsigned>(VTKElement::polyline));
    emitf("triangle=%u triangle_strip=%u polygon=%u pixel=%u",
          static_cast<unsigned>(VTKElement::triangle),
          static_cast<unsigned>(VTKElement::triangle_strip),
          static_cast<unsigned>(VTKElement::polygon), static_cast<unsigned>(VTKElement::pixel));
    emitf("quad=%u tetra=%u voxel=%u hexahedron=%u", static_cast<unsigned>(VTKElement::quad),
          static_cast<unsigned>(VTKElement::tetra), static_cast<unsigned>(VTKElement::voxel),
          static_cast<unsigned>(VTKElement::hexahedron));
    emitf("wedge=%u pyramid=%u", static_cast<unsigned>(VTKElement::wedge),
          static_cast<unsigned>(VTKElement::pyramid));

    emitf("== Quaternion is a static double[4] ==");
    const Quaternion qi = quaternion_init();
    emitf("quaternion_init=[%s,%s]", eil::to_string(qi[0]).c_str(),
          eil::to_string(qi[1]).c_str());
    // D's `Quaternion q;` (a static array with no initializer) is q.init, i.e. four
    // NaN, because double.init is NaN. The port spells that quaternion_init().
    emitf("default_local=%s", eil::to_string(qi).c_str());
    const Quaternion q2 = { { 1.0, 2.0, 3.0, 4.0 } };
    emitf("assigned=%s len=%u", eil::to_string(q2).c_str(),
          static_cast<unsigned>(q2.size()));
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <probes/geom_elements_nomenclature.txt>\n";
        return 2;
    }

    using namespace eil::geom;

    // ---- contract assertions ------------------------------------------------
    // D:12-19, the aliased enumerator pairs.
    CHECK(Face::west == 0 && Face::iminus == 0, "west/iminus alias");
    CHECK(Face::east == 1 && Face::iplus == 1, "east/iplus alias");
    CHECK(Face::south == 2 && Face::jminus == 2, "south/jminus alias");
    CHECK(Face::north == 3 && Face::jplus == 3, "north/jplus alias");
    CHECK(Face::bottom == 4 && Face::kminus == 4, "bottom/kminus alias");
    CHECK(Face::top == 5 && Face::kplus == 5, "top/kplus alias");

    // D:23-35, all 12 accepted spellings, and the uint return type.
    CHECK(face_index("west") == Face::west, "face_index(west)");
    CHECK(face_index("iminus") == Face::iminus, "face_index(iminus)");
    CHECK(face_index("east") == Face::east, "face_index(east)");
    CHECK(face_index("iplus") == Face::iplus, "face_index(iplus)");
    CHECK(face_index("south") == Face::south, "face_index(south)");
    CHECK(face_index("jminus") == Face::jminus, "face_index(jminus)");
    CHECK(face_index("north") == Face::north, "face_index(north)");
    CHECK(face_index("jplus") == Face::jplus, "face_index(jplus)");
    CHECK(face_index("bottom") == Face::bottom, "face_index(bottom)");
    CHECK(face_index("kminus") == Face::kminus, "face_index(kminus)");
    CHECK(face_index("top") == Face::top, "face_index(top)");
    CHECK(face_index("kplus") == Face::kplus, "face_index(kplus)");

    // D:32-33, `throw new Error(text("Invalid face name: ", name))`.
    {
        bool as_error = false;
        bool as_exception = false;
        std::string m;
        try {
            face_index("northwest");
        } catch (const eil::ExceptionBase& e) {
            as_exception = true;
            m = e.msg();
        } catch (const eil::ErrorBase& e) {
            as_error = true;
            m = e.msg();
        }
        CHECK(as_error && !as_exception, "face_index failure is an Error, not an Exception");
        CHECK(m == "Invalid face name: northwest", "face_index Error text");
    }

    // D:20, the mutable global.
    CHECK(face_name.length() == 6, "face_name length");
    CHECK(face_name[0] == "west" && face_name[1] == "east" && face_name[2] == "south",
          "face_name first three");
    CHECK(face_name[3] == "north" && face_name[4] == "bottom" && face_name[5] == "top",
          "face_name last three");
    CHECK(eil::to_string(face_name) == "[\"west\", \"east\", \"south\", \"north\", \"bottom\", \"top\"]",
          "face_name renders like a D string array");
    // D's out-of-range indexing is a RangeError (an Error), not undefined behaviour.
    {
        bool range_error = false;
        try {
            const std::string s = face_name[6];
            (void)s;
        } catch (const eil::RangeError&) {
            range_error = true;
        }
        CHECK(range_error, "face_name[6] throws RangeError");
    }

    // D:39-54, VTK cell type values.
    CHECK(VTKElement::vertex == 1 && VTKElement::polyvertex == 2 && VTKElement::line == 3 &&
              VTKElement::polyline == 4, "VTK 1..4");
    CHECK(VTKElement::triangle == 5 && VTKElement::triangle_strip == 6 &&
              VTKElement::polygon == 7 && VTKElement::pixel == 8, "VTK 5..8");
    CHECK(VTKElement::quad == 9 && VTKElement::tetra == 10 && VTKElement::voxel == 11 &&
              VTKElement::hexahedron == 12, "VTK 9..12");
    CHECK(VTKElement::wedge == 13 && VTKElement::pyramid == 14, "VTK 13..14");

    // D:56, radians(). The four angles where the 80-bit PI matters are checked
    // against the wide form, and are *not* equal to the double-PI form.
    CHECK(radians(0.0) == 0.0, "radians(0)");
    CHECK(radians(180.0) == static_cast<double>(EIL_PI), "radians(180) is double(PI)");
    CHECK(radians(90.0) == 1.5707963267948966, "radians(90)");
    CHECK(radians(45.0) == static_cast<double>(EIL_PI / 4.0L), "radians(45)");
    CHECK(radians(30.0) == 0.52359877559829893 && radians(30.0) != pi_double_form(30.0),
          "radians(30) keeps the 1 ULP of the wide PI");
    CHECK(radians(60.0) == 1.0471975511965979 && radians(60.0) != pi_double_form(60.0),
          "radians(60) keeps the 1 ULP of the wide PI");
    CHECK(radians(1.0 / 3.0) == 0.0058177641733144318 &&
              radians(1.0 / 3.0) != pi_double_form(1.0 / 3.0),
          "radians(1/3) keeps the 1 ULP of the wide PI");
    CHECK(radians(1.0e8) == 1745329.2519943295 && radians(1.0e8) != pi_double_form(1.0e8),
          "radians(1e8) keeps the 1 ULP of the wide PI");

    // D:68, `alias Quaternion = double[4]`.
    CHECK(sizeof(Quaternion) == 4 * sizeof(double), "Quaternion is a static array of 4");
    CHECK(std::isnan(quaternion_init()[0]), "Quaternion.init is NaN (double.init)");
    {
        const Quaternion q = quaternion_init();
        CHECK(std::isnan(q[0]) && std::isnan(q[1]) && std::isnan(q[2]) && std::isnan(q[3]),
              "all four .init components are NaN");
        const Quaternion zeroed = { { 0.0, 0.0, 0.0, 0.0 } };
        CHECK(zeroed[0] == 0.0 && zeroed[3] == 0.0, "Quaternion{} form is the zeroed one");
    }

    // ---- probe replay -------------------------------------------------------
    replay_probe();

    std::ifstream in(argv[1]);
    if (!in) {
        std::cerr << "cannot open baseline " << argv[1] << "\n";
        return 2;
    }
    std::vector<std::string> expected;
    for (std::string line; std::getline(in, line);) expected.push_back(line);

    int line_diffs = 0;
    size_t quantities = 0, identical = 0;
    long long worst_ulp = 0;
    std::string worst_where;
    double worst_d = 0.0, worst_cpp = 0.0;
    for (size_t i = 0; i < expected.size() || i < produced.size(); ++i) {
        if (i >= expected.size() || i >= produced.size() || expected[i] != produced[i]) {
            if (line_diffs < 20) {
                std::cout << "line " << (i + 1) << "\n  D:   |"
                          << (i < expected.size() ? expected[i] : std::string("<none>"))
                          << "|\n  C++: |"
                          << (i < produced.size() ? produced[i] : std::string("<none>")) << "|\n";
            }
            ++line_diffs;
        }
        if (i < expected.size() && i < produced.size()) {
            const std::vector<double> a = numbers_in(expected[i]);
            const std::vector<double> b = numbers_in(produced[i]);
            for (size_t k = 0; k < a.size() && k < b.size(); ++k) {
                ++quantities;
                const bool same_bits = std::memcmp(&a[k], &b[k], 8) == 0;
                if (same_bits) {
                    ++identical;
                } else {
                    const long long gap = ulp_gap(a[k], b[k]);
                    if (gap > worst_ulp) {
                        worst_ulp = gap;
                        worst_d = a[k];
                        worst_cpp = b[k];
                        worst_where = expected[i].substr(0, expected[i].find(' '));
                    }
                }
            }
        }
    }
    std::cout << (line_diffs == 0 && failures == 0 ? "PASS" : "FAIL") << ": nomenclature, "
              << checks - failures << "/" << checks << " assertions hold; probe replay "
              << expected.size() << " D lines, " << produced.size() << " C++ lines, "
              << line_diffs << " differ; " << identical << " of " << quantities
              << " numeric quantities bit-identical";
    if (worst_ulp > 0) {
        std::cout << ", worst residual " << worst_ulp << " ULP at " << worst_where << " (D "
                  << worst_d << " vs C++ " << worst_cpp << ")";
    }
    std::cout << "\n";
    return (line_diffs == 0 && failures == 0) ? 0 : 1;
}
