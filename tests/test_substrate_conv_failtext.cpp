// tests/test_substrate_conv_failtext.cpp — C++ mirror of
// probes/substrate/probe_conv_failtext.d, replayed line for line against
// baselines/e587b235/probes/substrate_conv_failtext.txt (443 rows).
//
// Both sides read tests/test_data/conv_inputs.hex — one hex-encoded operand per line —
// and emit `<operand-hex> ok|exc|err <result-hex>`. The operands include newline and
// tab characters on purpose (D's message for to!double("1\n") is part of the user
// contract, and so is the fact that '\n' reaches that message escaped as backslash-n
// while '\t' goes through raw), so a plain-text operand list would make the corpus
// line-oriented by accident and mis-attribute rows. See the probe header.
//
// This is the gate for the substrate's to!double error taxonomy: 1,121 of the 1,393
// `!` instantiations in src/eilmer are `to!X`, the solver prints ConversionException
// text out of Lua job-script parsing, and harness/normalize_log.py byte-compares it.
//
// Regenerate:
//   ldc2 probes/substrate/probe_conv_failtext.d -of=/tmp/probe_cft \
//     && /tmp/probe_cft tests/test_data/conv_inputs.hex \
//     > baselines/e587b235/probes/substrate_conv_failtext.txt
//
// Run from the repository root:
//   ./test_substrate_conv_failtext <baseline.txt> <conv_inputs.hex>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "eil/conv.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"

namespace {

std::string hexify(const std::string& b) {
    std::string res;
    char buf[4];
    for (unsigned char c : b) { std::snprintf(buf, sizeof buf, "%02x", c); res += buf; }
    return res;
}

std::string unhexify(const std::string& h) {
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    };
    std::string res;
    for (size_t i = 0; i + 1 < h.size(); i += 2)
        res += static_cast<char>(nib(h[i]) * 16 + nib(h[i + 1]));
    return res;
}

std::string one(const std::string& hexin) {
    const std::string operand = unhexify(hexin);
    try {
        const double d = eil::to_double(operand);
        char buf[64];
        std::snprintf(buf, sizeof buf, "%.17g", d);
        return hexin + " ok " + hexify(buf);
    } catch (const eil::ExceptionBase& e) {
        return hexin + " exc " + hexify(e.msg());
    } catch (const eil::ErrorBase& e) {
        return hexin + " err " + hexify(e.msg());
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 3) {
        std::fprintf(stderr, "usage: %s <baseline.txt> <conv_inputs.hex>\n", argv[0]);
        return 2;
    }
    // getline over the operand file yields exactly what the D probe's
    // split("\n")-then-drop-trailing-empty yields, including the empty operand as a
    // real first row (its hex line is empty).
    std::ifstream in_ops(argv[2]);
    if (!in_ops) { std::fprintf(stderr, "cannot open %s\n", argv[2]); return 2; }
    std::vector<std::string> got;
    for (std::string line; std::getline(in_ops, line);) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        got.push_back(one(line));
    }

    std::ifstream in(argv[1]);
    if (!in) { std::fprintf(stderr, "cannot open baseline %s\n", argv[1]); return 2; }
    std::vector<std::string> expected;
    for (std::string l; std::getline(in, l);) expected.push_back(l);

    int diffs = 0, shown = 0;
    for (size_t i = 0; i < expected.size() || i < got.size(); ++i) {
        const std::string e = i < expected.size() ? expected[i] : "<none>";
        const std::string g = i < got.size() ? got[i] : "<none>";
        if (e == g) continue;
        if (shown < 30) {
            std::printf("row %zu\n  D:   |%s|\n  C++: |%s|\n", i + 1, e.c_str(), g.c_str());
            ++shown;
        }
        ++diffs;
    }
    std::printf("%s: substrate_conv_failtext, %zu D rows, %zu C++ rows, %d differ; "
                "%zu of %zu byte-identical\n",
                diffs == 0 ? "PASS" : "FAIL", expected.size(), got.size(), diffs,
                expected.size() - static_cast<size_t>(diffs), expected.size());
    return diffs == 0 ? 0 : 1;
}
