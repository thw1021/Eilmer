// tests/test_nm_stmatrix_fails.cpp — port side of backlog T-923.
//
// Replays baselines/e587b235/probes/nm_stmatrix_fails.txt: the same seven inputs, written
// to the same paths, with the exception text the port raises compared byte for byte against
// what ldc2 printed. The .class= column is D-side information only (a port cannot reproduce
// `core.exception.ArrayIndexError` as a type name; that gap is backlog T-903), so the gate is
// the message.
//
// Before this test existed, none of these paths were gated at all, and the transcription had
// invented two texts for them ("Failed to open \"...\"" and "Unexpected EOF"). D raises
// ErrnoException-with-strerror for the open failure and ArrayIndexError for every truncated
// or empty input, because it never checks the stream. The comments on the port's reader
// carry the measurements.
//
// usage: test_nm_stmatrix_fails <baseline.txt>

#include <cstdio>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "eil/error.hpp"
#include "nm/stmatrix.hpp"

namespace {

const char* kDir = "/tmp/probe_stmatrix_fails";

void writeFile(const std::string& name, const std::string& body) {
    std::ofstream o(std::string(kDir) + "/" + name, std::ios::binary | std::ios::trunc);
    o << body;
}

// One case: run the reader, return the same row shape the D probe returns.
std::string run(const std::string& tag, const std::string& path) {
    std::string got;
    try {
        auto m = eil::nm::stmatrix::readFromMatrixMarketFile<double>(path);
        char buf[160];
        std::snprintf(buf, sizeof buf, "%s|ok|rows=%llu|cols=%llu|declared=%llu|stored=%llu",
                      tag.c_str(), (unsigned long long)m.n_rows, (unsigned long long)m.n_cols,
                      (unsigned long long)m.n_nzeros, (unsigned long long)m.val().length());
        got = buf;
    } catch (const eil::RangeError& e) {
        got = tag + "|msg=[" + e.msg() + "]";
    } catch (const eil::ErrorBase& e) {
        got = tag + "|msg=[" + e.msg() + "]";
    } catch (const eil::ExceptionBase& e) {
        got = tag + "|msg=[" + e.msg() + "]";
    }
    return got;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }
    std::ifstream in(argv[1]);
    if (!in) { std::fprintf(stderr, "cannot open baseline %s\n", argv[1]); return 2; }
    std::vector<std::string> expected;
    for (std::string l; std::getline(in, l);) expected.push_back(l);

    // Same inputs, same paths, same order as probes/nm/probe_stmatrix_fails.d.
    std::system(("mkdir -p " + std::string(kDir)).c_str());
    std::vector<std::pair<std::string, std::string>> cases = {
        {"missing_file", std::string(kDir) + "/does_not_exist.mtx"},
    };
    writeFile("trunc.mtx", "%comment\n3 3 2\n1 1 5.0\n");
    cases.push_back({"truncated_entries", std::string(kDir) + "/trunc.mtx"});
    writeFile("empty.mtx", "");
    cases.push_back({"empty_file", std::string(kDir) + "/empty.mtx"});
    writeFile("onlycomment.mtx", "%c1\n%c2\n");
    cases.push_back({"comments_then_eof", std::string(kDir) + "/onlycomment.mtx"});
    writeFile("few.mtx", "3 3\n");
    cases.push_back({"few_tokens", std::string(kDir) + "/few.mtx"});
    writeFile("blank.mtx", "\n3 3 1\n1 1 5.0\n");
    cases.push_back({"blank_first_line", std::string(kDir) + "/blank.mtx"});
    writeFile("good.mtx", "%comment\n3 3 2\n1 1 5.0\n2 3 7.5\n");
    cases.push_back({"valid_file", std::string(kDir) + "/good.mtx"});

    int diffs = 0, matched = 0;
    for (size_t i = 0; i < expected.size(); ++i) {
        const std::string& e = expected[i];
        // D's row is `tag|class=X|msg=[...]` or `tag|ok|...`; keep only what the port can match.
        std::string tag = e.substr(0, e.find('|'));
        std::string want;
        size_t mp = e.find("msg=[");
        if (mp != std::string::npos) {
            want = tag + "|msg=[" + e.substr(mp + 5, e.size() - mp - 6) + "]";
        } else {
            want = e;
        }
        std::string got = run(tag, cases[i].second);
        if (got == want) { ++matched; continue; }
        ++diffs;
        std::printf("line %zu\n  D:   |%s|\n  C++: |%s|\n", i + 1, want.c_str(), got.c_str());
    }
    std::printf("%s: nm_stmatrix_fails, probe replay %zu D lines, %d differ; %zu of %zu "
                "messages byte-identical\n",
                diffs == 0 ? "PASS" : "FAIL", expected.size(), diffs, matched, expected.size());
    return diffs == 0 ? 0 : 1;
}
