// nm/stmatrix.d -> src/nm/stmatrix.cpp
//
// STMatrix and readFromMatrixMarketFile are templates (on the value type) and so
// live in the header. The two text helpers they use are not templated, and are
// defined here: they reproduce std.string's strip()/split() as stmatrix.d:38-42
// and :51-55 use them.

#include "nm/stmatrix.hpp"

#include <cctype>
#include <string>

#include "eil/array.hpp"

namespace eil::nm {
namespace stmatrix {

// stmatrix.d:38 `f.readln().strip()` -- whitespace at both ends goes.
std::string strip(const std::string& s) {
    size_t b = 0;
    size_t e = s.length();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

// stmatrix.d:42 `line.split()` -- D splits on runs of whitespace and never
// returns an empty piece, so leading space does not produce a "" element.
eil::Array<std::string> split(const std::string& s) {
    eil::Array<std::string> out;
    size_t i = 0;
    const size_t n = s.length();
    while (i < n) {
        while (i < n && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        if (i >= n) break;
        const size_t start = i;
        while (i < n && !std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        out.append(s.substr(start, i - start));
    }
    return out;
}

} // namespace stmatrix
} // namespace eil::nm
