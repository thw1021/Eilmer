# EilmerVariant.cmake — the D `version(...)` identifiers, as C++ definitions.
#
# Defaults mirror the gdtk makefile exactly, because the D reference this port is
# measured against was built with them. Two independent confirmations:
#   * makefile:27-49  — MULTI_SPECIES_GAS ?= 1, MULTI_T_GAS ?= 1, MHD ?= 1,
#     TURBULENCE ?= 1, WITH_FPE ?= 1, WITH_MPI ?= 0, WITH_COMPLEX_NUMBERS ?= 0,
#     WITH_FSI ?= 0, WITH_SSC ?= 0, WITH_NK ?= 0, WITH_PIR ?= 0, FLAVOUR ?= debug
#   * the reference binary's own banner (run.log):
#     "Capabilities: multi-species-gas multi-temperature-gas MHD turbulence."
#     "Build-flavour: debug" / "Parallelism: Shared-memory"
#
# So the trunk is NOT a minimal serial ideal-gas build: multi_species_gas,
# multi_T_gas and MHD are compiled in and therefore change struct layout
# (FlowState gains B, mu_t, T_modes[], Ys[]). A C++ build with a different set
# would not be the same program.
#
# Macros keep the D spelling (lowercase `turbulence`, not `EIL_TURBULENCE`) so a
# ported `version(turbulence)` block becomes `#ifdef turbulence` one-for-one.

# name : default : capability token printed in the banner ("" = not printed)
set(EIL_VARIANTS
    "multi_species_gas:ON:multi-species-gas"
    "multi_T_gas:ON:multi-temperature-gas"
    "MHD:ON:MHD"
    "turbulence:ON:turbulence"
    "enable_fp_exceptions:ON:"
    "flavour_debug:ON:"
    "flavour_fast:OFF:"
    "flavour_profile:OFF:"
    "complex_numbers:OFF:"
    "mpi_parallel:OFF:"
    "mpi_timeouts:OFF:"
    "steady_state:OFF:"
    "nk_accelerator:OFF:"
    "pir:OFF:"
    "shape_sensitivity:OFF:"
    "FSI:OFF:"
    "gpu_chem:OFF:"
    "cuda_gpu_chem:OFF:"
    "opencl_gpu_chem:OFF:"
    "debug_chem:OFF:"
    "diagnostics:OFF:"
    "with_tecplot_binary:OFF:"
    "with_libplot:OFF:"
)

set(EIL_ACTIVE_VARIANTS "")
set(EIL_CAPABILITY_TOKENS "")

foreach(entry ${EIL_VARIANTS})
    string(REPLACE ":" ";" parts "${entry}")
    list(GET parts 0 vname)
    list(GET parts 1 vdefault)
    list(GET parts 2 token)
    option(EIL_${vname} "D version identifier: ${vname}" "${vdefault}")
    if(EIL_${vname})
        list(APPEND EIL_ACTIVE_VARIANTS "${vname}")
        if(NOT "${token}" STREQUAL "")
            list(APPEND EIL_CAPABILITY_TOKENS "${token}")
        endif()
    endif()
endforeach()

list(SORT EIL_ACTIVE_VARIANTS)
list(JOIN EIL_ACTIVE_VARIANTS " " EIL_ACTIVE_VARIANTS_STR)
list(JOIN EIL_CAPABILITY_TOKENS " " EIL_CAPABILITY_TOKENS_STR)

# The variant set changes the layout of shared structs, so a TU compiled with a
# different set silently corrupts memory with no diagnostic. Two guards:
#   1. EIL_ABI_HASH identifies the active set; the symbol below exists only for
#      that set, so mixing flag sets across a link produces an undefined symbol.
#   2. The banner printed at start-up states the set, and the normalized-log
#      gate diffs it.
string(SHA1 EIL_ABI_HASH "${EIL_ACTIVE_VARIANTS_STR}")
string(SUBSTRING "${EIL_ABI_HASH}" 0 12 EIL_ABI_HASH_SHORT)

set(EIL_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/generated/eil")
file(MAKE_DIRECTORY "${EIL_GENERATED}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/eil_abi.hpp.in"
               "${EIL_GENERATED}/abi.hpp" @ONLY)

# One definition target for the whole build: every library links it, so variant
# definitions cannot diverge per target.
add_library(eil_variants INTERFACE)
foreach(v ${EIL_ACTIVE_VARIANTS})
    target_compile_definitions(eil_variants INTERFACE "${v}")
endforeach()
target_compile_definitions(
    eil_variants INTERFACE
    $<$<CONFIG:Debug>:EIL_DEBUG_BUILD>
    "EIL_ABI_STRING=\"${EIL_ACTIVE_VARIANTS_STR}\""
    "EIL_CAPABILITY_STRING=\"${EIL_CAPABILITY_TOKENS_STR}\"")
target_include_directories(eil_variants INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/generated")

message(STATUS "Eilmer variant set (D version identifiers): ${EIL_ACTIVE_VARIANTS_STR}")
message(STATUS "Eilmer capabilities banner text: ${EIL_CAPABILITY_TOKENS_STR}")
message(STATUS "Eilmer ABI hash: ${EIL_ABI_HASH_SHORT}")
