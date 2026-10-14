// tests/test_substrate_conv_int_typename.cpp — replays
// baselines/e587b235/probes/substrate_conv_int_typename.txt row by row against
// eil::parse_int_strict<T>, the strict string->integer scan in substrate/include/eil/conv.hpp.
//
// The corpus was captured from ldc2 1.40.0 by probes/substrate/probe_conv_int_typename.d
// (T-927) but had no gate until T-928 added the range check: parse_int_strict used to
// accumulate into a bare `long long` with NO bounds test, so `to!int("2147483648")`
// silently wrapped negative instead of raising std.conv.ConvOverflowException, and
// `to!ulong("18446744073709551615")` overflowed its own accumulator (UB). This test is
// what pins the fixed behaviour: the type NAME each target prints, the strict grammar
// failures (std.conv.ConvException), and the overflow rows (std.conv.ConvOverflowException
// "Overflow in integral conversion"), including the two asymmetric endpoints of a signed
// target and ulong's own maximum converting cleanly.
//
// The port names five integer targets (int, long, uint, ulong, size_t) plus ptrdiff_t,
// which on this LP64 ABI is `long` and prints "long" exactly as the long rows do. The
// narrow targets short/byte/ubyte are NOT implemented in substrate (d_int_type_name has no
// case for them; see the note at conv.hpp:279-283), so their rows are counted as skipped
// rather than silently dropped — the skip count is printed, and if it moves the row is
// named, so a future narrow-target port cannot pass by accident.
//
// Regenerate the baseline:
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -O0 -of=/tmp/probe_conv_int_typename probes/substrate/probe_conv_int_typename.d
//   /tmp/probe_conv_int_typename > baselines/e587b235/probes/substrate_conv_int_typename.txt
//
// Run: ./test_substrate_conv_int_typename <baseline.txt>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

#include "eil/conv.hpp"
#include "eil/error.hpp"

namespace {

struct Row {
    std::string key;    // e.g. "int.max+1"
    std::string src;    // the string handed to to!T
    bool throws = false;
    std::string cls;    // D class name when it throws
    std::string msg;    // D .msg when it throws
    std::string value;  // D's to!string(result) when it does not
};

// "type<ulong" -> the resolved target the port would use; "" means "port has no target".
std::string target_of(const std::string& key) {
    const std::string t = key.substr(0, key.find('.'));
    if (t == "int" || t == "long" || t == "uint" || t == "ulong" || t == "size_t" ||
        t == "ptrdiff_t")
        return t;
    return "";  // short, byte, ubyte: no substrate target
}

// Between `open` and the next `]|` (fields are `]`-terminated in the corpus).
std::string bracketed(const std::string& s, const std::string& open, size_t* after = nullptr) {
    const size_t b = s.find(open);
    if (b == std::string::npos) return "";
    const size_t start = b + open.size();
    const size_t e = s.find("]|", start);
    if (e == std::string::npos) {
        // last field on the line: runs to the final ']'
        const size_t last = s.rfind(']');
        if (after) *after = s.size();
        return s.substr(start, last == std::string::npos || last < start ? 0 : last - start);
    }
    if (after) *after = e + 2;
    return s.substr(start, e - start);
}

Row parse(const std::string& line) {
    Row r;
    const size_t p = line.find('|');
    r.key = line.substr(0, p);
    const std::string rest = line.substr(p + 1);
    size_t after_src = 0;
    r.src = bracketed(rest, "src=[", &after_src);
    const std::string tail = rest.substr(after_src);
    if (tail.rfind("no-throw", 0) == 0) {
        r.throws = false;
        const size_t v = tail.find("value=");
        r.value = tail.substr(v + 6);
    } else {
        r.throws = true;
        const size_t c = tail.find("class=");
        const size_t ce = tail.find('|', c);
        r.cls = tail.substr(c + 6, ce - (c + 6));
        r.msg = bracketed(tail, "msg=[");
    }
    return r;
}

int g_fail = 0, g_skip = 0, g_checked = 0;

// Run the port's scan for this target and report the D class + msg (or value).
void run_one(const Row& r, const std::string& target) {
    std::string got_cls, got_msg, got_value;
    bool got_threw = false;

    try {
        if (target == "int") {
            got_value = std::to_string(eil::parse_int_strict<int>(r.src, "int", true));
        } else if (target == "long" || target == "ptrdiff_t") {
            got_value = std::to_string(eil::parse_int_strict<long>(r.src, "long", true));
        } else if (target == "uint") {
            got_value = std::to_string(eil::parse_int_strict<unsigned>(r.src, "uint", false));
        } else if (target == "ulong") {
            got_value =
                std::to_string(eil::parse_int_strict<unsigned long>(r.src, "ulong", false));
        } else {  // size_t (== unsigned long here; message says "ulong")
            got_value = std::to_string(eil::to_size_t(r.src));
        }
    } catch (const eil::ConvOverflowException& e) {
        got_threw = true;
        got_cls = "std.conv.ConvOverflowException";
        got_msg = e.msg();
    } catch (const eil::ConversionException& e) {
        got_threw = true;
        got_cls = "std.conv.ConvException";
        got_msg = e.msg();
    }

    ++g_checked;
    bool ok;
    if (r.throws) {
        ok = got_threw && got_cls == r.cls && got_msg == r.msg;
    } else {
        ok = !got_threw && got_value == r.value;
    }
    if (!ok) {
        ++g_fail;
        std::printf("FAIL %s src=[%s]\n", r.key.c_str(), r.src.c_str());
        if (r.throws) {
            std::printf("  D:   threw %s |%s|\n", r.cls.c_str(), r.msg.c_str());
            if (got_threw)
                std::printf("  C++: threw %s |%s|\n", got_cls.c_str(), got_msg.c_str());
            else
                std::printf("  C++: no throw, value=%s\n", got_value.c_str());
        } else {
            std::printf("  D:   no-throw value=%s\n", r.value.c_str());
            if (got_threw)
                std::printf("  C++: threw %s |%s|\n", got_cls.c_str(), got_msg.c_str());
            else
                std::printf("  C++: no throw, value=%s\n", got_value.c_str());
        }
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }
    std::ifstream in(argv[1]);
    if (!in) {
        std::fprintf(stderr, "cannot open %s\n", argv[1]);
        return 2;
    }
    std::vector<std::string> skipped;
    for (std::string line; std::getline(in, line);) {
        if (line.empty()) continue;
        const Row r = parse(line);
        const std::string target = target_of(r.key);
        if (target.empty()) {
            ++g_skip;
            skipped.push_back(r.key);
            continue;
        }
        run_one(r, target);
    }
    if (!skipped.empty()) {
        std::printf("skipped %d narrow-target rows (no substrate target):", g_skip);
        for (const std::string& k : skipped) std::printf(" %s", k.c_str());
        std::printf("\n");
    }
    std::printf("%s: substrate_conv_int_typename, %d rows checked, %d skipped, %d failures\n",
                g_fail == 0 ? "PASS" : "FAIL", g_checked, g_skip, g_fail);
    return g_fail == 0 ? 0 : 1;
}
