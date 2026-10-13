// tests/test_abi.cpp
//
// The variant set changes the layout of FlowState/CellData, so a mixture of TUs
// compiled with different -D sets would corrupt memory silently. The generated
// header names a symbol after the hash of the active set; this test calls it, so
// any executable that forgets to agree fails to *link* rather than to converge.
#include <cstdio>
#include <cstring>
#include <string>

#include "eil/abi.hpp"

int main() {
    int failures = 0;
    if (std::strlen(EIL_ABI_HASH) != 12) {
        std::printf("FAIL: ABI hash is not 12 characters\n");
        ++failures;
    }
    // The trunk set is fixed by the gdtk makefile defaults and confirmed by the
    // reference binary's own banner ("Capabilities: multi-species-gas
    // multi-temperature-gas MHD turbulence").
    const std::string v = EIL_ABI_VARIANTS;
    for (const char* need : {"MHD", "multi_species_gas", "multi_T_gas", "turbulence",
                            "enable_fp_exceptions", "flavour_debug"}) {
        if (v.find(need) == std::string::npos) {
            std::printf("FAIL: trunk variant set is missing %s (got '%s')\n", need, v.c_str());
            ++failures;
        }
    }
    if (std::string(EIL_ABI_CAPABILITIES) != "multi-species-gas multi-temperature-gas MHD turbulence") {
        std::printf("FAIL: capabilities banner text differs from D: got '%s'\n",
                    std::string(EIL_ABI_CAPABILITIES).c_str());
        ++failures;
    }
    EIL_ABI_SYMBOL_NAME();  // link-time dependency on the exact variant set
    std::printf("%s: abi guard (%s = %s)\n", failures ? "FAIL" : "PASS", EIL_ABI_HASH, v.c_str());
    return failures ? 1 : 0;
}
