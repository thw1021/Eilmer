// tests/test_nm_stmatrix.cpp — the unittest block from nm/stmatrix.d:63-68.
//
// The D block compiles and passes in the reference tree. It reads the Matrix
// Market file "test_data/b1_ss.mtx" relative to the working directory (D's
// literal), which is src/nm in the gdtk tree. The port keeps D's literal as the
// default and takes an explicit path as argv[1], which is how
// tests/test_format_corpus.cpp already passes a data file to CMake. The file
// itself is copied byte for byte to tests/test_data/b1_ss.mtx.
//
// D's key type is Tuple!(size_t,size_t); std::pair<size_t,size_t> is the same
// data (see the header note in src/nm/stmatrix.hpp), and D's `matrix.val[k]`
// read -- which raises "Range violation" when the key is absent -- is val().get(k).

#include <cstdio>
#include <cstring>
#include <string>
#include <utility>

#include "eil/eil.hpp"
#include "nm/number.hpp"
#include "nm/stmatrix.hpp"

namespace {

namespace stmatrix = eil::nm::stmatrix;

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

int main(int argc, char** argv) {
    std::string fName = "test_data/b1_ss.mtx";   // stmatrix.d:64
    if (argc > 1) fName = argv[1];

    // D's unittest would surface an unreadable file as an uncaught
    // std.file.FileException from the test runner; the harness reports it as a
    // failure line instead of aborting.
    try {
    auto matrix = stmatrix::readFromMatrixMarketFile<double>(fName);
    check(matrix.n_rows == 7 && matrix.n_cols == 7 && matrix.n_nzeros == 15,
          "stmatrix: header gives a 7x7 matrix with 15 entries");
    check(eil::nm::approxEqualNumbers(matrix.val().get(stmatrix::TripletKey<double>(4, 0)),
                                      -0.03599942, 1.0e-7),
          "stmatrix: value at (4,0) matches the file");
    check(eil::nm::approxEqualNumbers(matrix.val().get(stmatrix::TripletKey<double>(6, 6)),
                                      1.0, 1.0e-7),
          "stmatrix: value at (6,6) matches the file");

    // Reading an absent key must fail the way D's `matrix.val[k]` does. D raises
    // core.exception.RangeError with msg "Range violation" here -- confirmed
    // against the reference build for the key (0,0), which b1_ss.mtx omits.
    bool caught = false;
    try {
        auto missing = matrix.val().get(stmatrix::TripletKey<double>(0, 0));
        (void)missing;
    } catch (const eil::RangeViolation& e) {
        caught = std::strcmp(e.what(), "Range violation") == 0;
    }
    check(caught, "stmatrix: absent key raises D's Range violation text");
    } catch (const eil::ExceptionBase& e) {
        std::printf("FAIL: stmatrix: cannot read %s: %s\n", fName.c_str(), e.what());
        ++failures;
    }

    std::printf("%s: nm.stmatrix unittests (%d failures)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
