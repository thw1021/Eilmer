// tests/test_geom_misc_svg.cpp
//
// Two layers, both required (the pattern of tests/test_geom_elements_properties.cpp):
//
//  A. the `unittest` block of geom/misc/svg.d (D:362-364), which is literally
//         unittest { /* [TODO] fill me in with something meaningful. */ }
//     — an empty block. There is no assertion to port, so this layer asserts only that
//     the module still behaves (an SVGContext can be built and a document written), and
//     the real gate is layer B plus the demo's byte comparison. Recorded in
//     provenance/fidelity.yml as "no D unittest".
//
//  B. a line-for-line replay of probes/geom/misc/probe_svg.d against
//     baselines/e587b235/probes/geom_misc_svg.txt: 157 lines, of which 132 are the
//     emitted SVG text of six documents (svg_demo's two, a document that reaches every
//     remaining primitive and branch, and one per unit length), dumped after all writing
//     finished. svg.d is pure text generation, so the criterion is BYTE equality of
//     every line, not a tolerance; the numeric summary below counts the %.2f/%g fields
//     inside those lines as quantities for completeness.
//
// This test writes svgprobe_*.svg into its working directory and reads them back,
// exactly as the D probe does; nothing is compared from the filesystem.
//
// Run: ./test_geom_misc_svg ../baselines/e587b235/probes/geom_misc_svg.txt
//
// Regenerate the baseline:
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   cd /home/tang/packages/Eilmer
//   ldc2 -i -I/home/tang/packages/gdtk/src probes/geom/misc/probe_svg.d -of=/tmp/svg_d
//   mkdir -p /tmp/svgwork && cd /tmp/svgwork
//   /tmp/svg_d > /home/tang/packages/Eilmer/baselines/e587b235/probes/geom_misc_svg.txt
#include <cctype>
#include <cstdarg>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "geom/misc/svg.hpp"
#include "nm/number.hpp"

using eil::geom::SVGContext;

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

std::vector<std::string> produced;

void emitf(const char* fmt, ...) {
    char buf[65536];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    produced.emplace_back(buf);
}

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
            if (c == '-' || c == '+' || std::isdigit(static_cast<unsigned char>(c)) || c == 'n' ||
                c == 'i') {
                char* endp = nullptr;
                const double v = std::strtod(tok.c_str(), &endp);
                if (endp == tok.c_str() + tok.size()) out.push_back(v);
            }
        }
        i = b;
    }
    return out;
}

std::vector<std::string> keys_in(const std::string& s) {
    std::vector<std::string> out;
    size_t i = 0;
    while (i < s.size()) {
        if (s[i] != '=') { ++i; continue; }
        size_t b = s.rfind(' ', i);
        if (b == std::string::npos) b = 0; else ++b;
        out.push_back(s.substr(b, i - b));
        size_t e = s.find(' ', i + 1);
        i = (e == std::string::npos) ? s.size() : e;
    }
    return out;
}

long long ulp_gap(double a, double b) {
    int64_t ia, ib;
    std::memcpy(&ia, &a, 8);
    std::memcpy(&ib, &b, 8);
    if (ia < 0) ia = INT64_MIN - ia;
    if (ib < 0) ib = INT64_MIN - ib;
    return ia > ib ? ia - ib : ib - ia;
}

/// The probe's dump_doc: one stdout line per document line, no trailing empty line.
std::vector<std::string> read_lines(const std::string& name, size_t& bytes) {
    std::ifstream in(name, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    const std::string text = ss.str();
    bytes = text.size();
    std::vector<std::string> lines;
    size_t start = 0;
    while (true) {
        size_t nl = text.find('\n', start);
        if (nl == std::string::npos) {
            if (start < text.size()) lines.push_back(text.substr(start));
            break;
        }
        lines.push_back(text.substr(start, nl - start));
        start = nl + 1;
    }
    return lines;
}

void dump_doc(const char* name) {
    size_t bytes = 0;
    std::vector<std::string> lines = read_lines(name, bytes);
    emitf("--- doc %s bytes=%d ---", name, static_cast<int>(bytes));
    for (const std::string& l : lines) produced.push_back(l);
}

// ============================================================================
// A. D's unittest block is empty (svg.d:362-364). Assert the module is alive.
// ============================================================================
void port_d_unittest() {
    auto s = std::make_shared<SVGContext>();
    s->open("svgprobe_unittest.svg");
    s->line(0.0, 0.0, 1.0, 1.0);
    s->close();
    size_t bytes = 0;
    std::vector<std::string> lines = read_lines("svgprobe_unittest.svg", bytes);
    CHECK(bytes > 0, "default-constructed SVGContext writes a non-empty document");
    // Measured against D, not assumed: probes/geom/misc/probe_svg_default_doc.d
    // reports total_lines=13 for exactly this document (10 header lines including the
    // DOCTYPE and the "drawing begins here" comment, 1 element, 2 trailer lines), and
    // the port's file matches it byte for byte, 529 bytes.
    CHECK(lines.size() == 13, "D-shaped document: 10 header lines, 1 element, 2 trailer");
    CHECK(lines[0] == "<?xml version=\"1.0\"?>", "document starts with the XML prolog");
    CHECK(lines[11] == "<!-- end of drawing -->", "close() writes the comment");
    CHECK(lines[12] == "</svg>", "close() closes the svg element");
}

// ============================================================================
// B. the probe replay. Same drawings, same order, same print statements.
// ============================================================================
void replay_probe() {
    using namespace eil::geom;

    // ---- unit handling and the constructor defaults ----------------------
    const char* units[4] = {"in", "mm", "cm", "m"};
    for (const char* unit : units) {
        auto s = std::make_shared<SVGContext>(12.0, 9.5, unit, std::string("units ") + unit,
                                             "one document per unit");
        s->open(std::string("svgprobe_u_") + unit + ".svg");
        s->line(0.0, 0.0, 12.0, 9.5);
        s->close();
    }
    try {
        auto bad = std::make_shared<SVGContext>(1.0, 1.0, "furlong");
        emitf("unit unknown built=1");
    } catch (const eil::ExceptionBase& e) {
        emitf("unit unknown built=0 isError=0 msg=[%s]", e.msg().c_str());
    } catch (const eil::ErrorBase& e) {
        emitf("unit unknown built=0 isError=1 msg=[%s]", e.msg().c_str());
    }

    // ---- svg_demo.d's two documents, byte for byte -----------------------
    auto s = std::make_shared<SVGContext>(100, 120.0, "mm", "test run");
    s->open("svgprobe_demo1.svg");
    s->line(0.0, 0.0, 100.0, 120.0);
    s->close();

    s->open("svgprobe_demo2.svg");
    s->line(0.0, 0.0, 90.0, 120.0);
    s->setLineWidth(0.5);
    s->setFillColour("yellow");
    s->circle(25.0, 85.0, 12.3);
    s->setLineWidth(0.25);
    {
        eil::Array<double> xs, ys;
        for (double v : {0.0, 10.0, 20.0, 30.0}) xs.append(v);
        for (double v : {50.0, 60.0, 50.0, 60.0}) ys.append(v);
        s->polyline(xs.all(), ys.all(), true);
    }
    s->text(25.0, 85.0, "Circle", -30.0, "middle", 10);
    s->arc(90.0, 0.0, 60.0, 30.0, 60.0, 0.0);
    s->setLineWidth(0.75);
    s->arc(30.0, 0.0, 60.0, 30.0, 60.0, 0.0, true);
    s->bezier3(80.0, 80.0, 40.0, 80.0, 80.0, 100.0, 40.0, 100.0);
    s->setLineWidth(0.01);
    s->dotlabel(70.0, 20.0, "a");
    s->setLineWidth(0.1);
    {
        eil::Array<double> xs, ys;
        for (double v : {60.0, 95.0, 95.0, 60.0}) xs.append(v);
        for (double v : {10.0, 10.0, 50.0, 50.0}) ys.append(v);
        s->polygon(xs.all(), ys.all(), true, true, true);
    }
    s->close();

    // ---- a document that reaches every remaining branch ------------------
    auto t = std::make_shared<SVGContext>(); // all defaults
    t->open("svgprobe_full.svg");
    t->add_comment("first comment");
    t->add_comment("");
    t->begin_group();
    t->line(1.0, 1.0, 2.0, 3.0);
    t->line(1.0, 1.0, 2.0, 3.0, true);
    t->line(1.0, 1.0, 2.0, 3.0, false, 0.25);
    t->end_group();
    t->begin_group("group-with-id", 0.5);
    {
        eil::Array<double> xs, ys;
        for (double v : {0.0, 1.0, 2.0}) xs.append(v);
        for (double v : {0.0, 1.0, 0.0}) ys.append(v);
        t->polyline(xs.all(), ys.all());
        t->polyline(xs.all(), ys.all(), true, 0.3);
    }
    {
        // A deliberately uneven pair of lists: polygon/polyline loop over min(length).
        eil::Array<double> xs, ys;
        for (double v : {0.0, 1.0, 2.0, 3.0}) xs.append(v);
        for (double v : {0.0, 1.0}) ys.append(v);
        t->polyline(xs.all(), ys.all());
    }
    t->end_group();
    t->setLineColour("red");
    t->setFillColour("#00ff00");
    {
        eil::Array<double> xs, ys;
        for (double v : {0.0, 10.0, 10.0}) xs.append(v);
        for (double v : {0.0, 0.0, 10.0}) ys.append(v);
        t->polygon(xs.all(), ys.all()); // defaults fill=true, stroke=true
        t->polygon(xs.all(), ys.all(), true, false);
        t->polygon(xs.all(), ys.all(), false, true, true);
        t->polygon(xs.all(), ys.all(), true, true, false, "poly-id", 0.125);
    }
    t->clearFillColour();
    t->circle(20.0, 20.0, 5.0);
    t->circle(20.0, 20.0, 5.0, false, true, true, "c2", 0.75);
    t->circle(20.0, 20.0, 5.0, true, false);
    // Both arc directions: the crossz sign decides sweep_flag (svg.d:256-257).
    t->arc(0.0, 10.0, 10.0, 10.0, 5.0, 0.0);
    t->arc(10.0, 10.0, 0.0, 10.0, 5.0, 0.0);
    t->arc(0.0, 10.0, 10.0, 10.0, 5.0, 0.0, true, 0.4);
    t->bezier3(0.0, 0.0, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, true, 0.6);
    t->text(30.0, 30.0, "no angle");
    t->text(30.0, 30.0, "rotated", 45.0, "end", 12, "blue", "serif");
    t->text(30.0, 30.0, "", -45.0);
    t->dotlabel(40.0, 40.0);
    t->dotlabel(40.0, 40.0, "lbl", "start", 3.0, 8, "green", "monospace");
    // dashArray with explicit lengths, then back to the defaults.
    t->setDashArray(5.0, 1.5);
    t->line(0.0, 60.0, 10.0, 60.0, true);
    t->setDashArray();
    t->line(0.0, 70.0, 10.0, 70.0, true);
    t->close();

    // ---- an arc whose radii disagree (svg.d:253-255) ---------------------
    try {
        auto u = std::make_shared<SVGContext>(50.0, 50.0, "mm", "arc failure");
        u->open("svgprobe_arc.svg");
        u->line(0.0, 0.0, 1.0, 1.0); // write something first, as the probe's drawing does
        u->arc(0.0, 10.0, 10.0, 10.0, 5.0, 0.0);
        u->arc(0.0, 10.0, 20.0, 30.0, 5.0, 0.0); // r0 = 5.099, r1 = 33.5 -> throws
        u->close();
        emitf("arc threw=0");
    } catch (const eil::ExceptionBase& e) {
        emitf("arc threw=1 msg=[%s]", e.msg().c_str());
    } catch (const eil::ErrorBase& e) {
        emitf("arc threw=1 isError=1 msg=[%s]", e.msg().c_str());
    }

    // ---- styleStr over its whole flag cross-product ----------------------
    auto v = std::make_shared<SVGContext>(60.0, 60.0, "mm", "styles");
    for (bool fill : {false, true})
        for (bool stroke : {false, true})
            for (bool dashed : {false, true}) {
                emitf("style fill=%s stroke=%s dashed=%s s=[%s]",
                      eil::to_string(fill).c_str(), eil::to_string(stroke).c_str(),
                      eil::to_string(dashed).c_str(),
                      v->styleStr(fill, stroke, dashed).c_str());
                emitf("style fill=%s stroke=%s dashed=%s o=[%s]",
                      eil::to_string(fill).c_str(), eil::to_string(stroke).c_str(),
                      eil::to_string(dashed).c_str(),
                      v->styleStr(fill, stroke, dashed, 0.25, 0.75).c_str());
            }
    // The coordinate transforms, which every primitive above routes through.
    emitf("px x0=%.17g x100=%.17g y0=%.17g y60=%.17g", v->toPixelsX(0.0), v->toPixelsX(100.0),
          v->toPixelsY(0.0), v->toPixelsY(60.0));
    v->setLineWidth(0.25);
    v->setLineColour("black");
    v->clearFillColour();

    // ---- now print the documents ----------------------------------------
    dump_doc("svgprobe_demo1.svg");
    dump_doc("svgprobe_demo2.svg");
    dump_doc("svgprobe_full.svg");
    for (const char* unit : units) {
        dump_doc((std::string("svgprobe_u_") + unit + ".svg").c_str());
    }
    emitf("done=1");
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: " << argv[0] << " <probes/geom_misc_svg.txt>\n";
        return 2;
    }

    port_d_unittest();
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
    double worst_d = 0.0, worst_cpp = 0.0, worst_resid = 0.0;
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
            const std::vector<std::string> k = keys_in(expected[i]);
            const std::string tag = expected[i].substr(0, expected[i].find(' '));
            for (size_t j = 0; j < a.size() && j < b.size(); ++j) {
                ++quantities;
                if (std::memcmp(&a[j], &b[j], 8) == 0) {
                    ++identical;
                } else {
                    const long long gap = ulp_gap(a[j], b[j]);
                    if (gap > worst_ulp) {
                        worst_ulp = gap;
                        worst_resid = std::fabs(a[j] - b[j]);
                        worst_d = a[j];
                        worst_cpp = b[j];
                        worst_where = tag + " " + (j < k.size() ? k[j] : std::string("?"));
                    }
                }
            }
        }
    }
    std::cout << (line_diffs == 0 && failures == 0 ? "PASS" : "FAIL") << ": svg, "
              << checks - failures << "/" << checks
              << " smoke assertions hold (svg.d's only unittest block is an empty TODO,"
                 " svg.d:362-364); emitted-text replay "
              << expected.size() << " D lines vs " << produced.size()
              << " C++ lines, " << line_diffs << " differ (byte comparison, no tolerance); "
              << identical << " of " << quantities
              << " embedded numeric quantities bit-identical";
    if (worst_ulp > 0) {
        std::cout << ", worst residual " << worst_resid << " (" << worst_ulp
                  << " ULP) on quantity " << worst_where << " (D " << worst_d << " vs C++ "
                  << worst_cpp << ")";
    } else {
        std::cout << ", worst residual 0 (0 ULP) on quantity none";
    }
    std::cout << "\n";
    return (line_diffs == 0 && failures == 0) ? 0 : 1;
}
