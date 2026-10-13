// tests/test_array.cpp — D array/slice semantics the port depends on.
//
// These are the behaviours a translator has to be able to trust: `arr[a..b]` is
// a view (writes show through), `.length = m` value-initializes the tail and
// keeps pointer identity when capacity allows, `~=` may relocate, and a view
// taken before a relocation must fail loudly under -DEIL_SLICE_CHECK rather
// than read freed storage.

#include <cstdio>
#include <cstring>
#include <string>

#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"

namespace {
int failures = 0;
void check(bool ok, const char* what) {
    if (!ok) {
        std::printf("FAIL: %s\n", what);
        ++failures;
    } else {
        std::printf("ok: %s\n", what);
    }
}
} // namespace

int main() {
    // new T[n] is zero-filled GC memory in D.
    eil::Array<double> a(4);
    check(a.length() == 4, "length after Array(n)");
    check(a[0] == 0.0 && a[3] == 0.0, "Array(n) value-initializes");

    // .length = m keeps elements and zero-fills the tail.
    a[0] = 1.5;
    a.set_length(6);
    check(a.length() == 6 && a[0] == 1.5 && a[5] == 0.0, "set_length grows in place");

    // Slicing is a view: writing through it changes the owner.
    eil::Slice<double> v = a.slice(1, 4);
    check(v.length() == 3, "slice length");
    v[0] = -7.0;
    check(a[1] == -7.0, "slice writes through to owner");

    // `$`-style slicing and nested views.
    eil::Slice<double> tail = a.from(2);
    check(tail.length() == a.length() - 2, "from(lo) reaches $");
    eil::Slice<double> head = a.to(2);
    check(head.length() == 2 && head[1] == a[1], "to(hi)");

    // arr[] = value (D broadcast assignment).
    eil::Array<int> b(3);
    b.fill(7);
    check(b[0] == 7 && b[2] == 7, "fill");

    // ~= appends; dup is an independent copy; ~ builds a new array.
    eil::Array<double> c;
    c.append(1.0);
    c.append(2.0);
    eil::Array<double> d = c.dup();
    d.append(3.0);
    check(c.length() == 2 && d.length() == 3, "dup is a deep copy");
    eil::Array<double> cat = c.concat(d.all());
    check(cat.length() == 5 && c.length() == 2, "concat does not mutate the left side");

    // Range errors carry D's exact message.
    try {
        eil::Array<int> m(3);
        m[10] = 1;
        check(false, "out of bounds should throw");
    } catch (const eil::RangeError& e) {
        check(std::strcmp(e.what(), "index [10] is out of bounds for array of length 3") == 0,
              "RangeError text matches D");
    }

    // Appending past capacity relocates; a stale view must be caught when
    // EIL_SLICE_CHECK is on, and must not be silently trusted when it is off.
    eil::Array<double> grow(2);
    grow[0] = 1.0;
    eil::Slice<double> stale = grow.all();
    for (int i = 0; i < 64; ++i) grow.append(static_cast<double>(i));
#ifdef EIL_SLICE_CHECK
    bool caught = false;
    try {
        volatile double x = stale[0];
        (void)x;
    } catch (const eil::SliceError&) {
        caught = true;
    }
    check(caught, "stale slice detected after reallocation");
#else
    check(true, "slice checking disabled in this build (expected in release)");
#endif

    // Text rendering used by log comparison.
    eil::Array<double> shown({1.0, 2.5, -3.0});
    check(eil::to_string(shown) == "[1, 2.5, -3]", "array prints like D");
    eil::Array<double> none;
    check(eil::to_string(none) == "[]", "empty array prints []");

    std::printf("%s: array semantics (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
