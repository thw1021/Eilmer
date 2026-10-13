#pragma once
/**
 * msg_service.d
 * Home to some commonly used messages in assert statements
 * and error messages.
 *
 * Author: Rowan J. Gollan
 */
// util/msg_service.d -> src/util/msg_service.hpp (module util.msg_service).
//
// D's default arguments `size_t lineNo = __LINE__, string fileName = __FILE__`
// expand at the *call site* (measured against Phobos: a call written on line 20 of
// a file reports that file and line 20). Every call site in the tree either passes
// __LINE__/__FILE__ explicitly -- gas/diffusion/sutherland_viscosity.d:38 and the
// rest -- or relies on that default, as gas/gas_model.d:211 does with
// `brokenPreCondition("Inconsistent array lengths.")`. A C++ default argument would
// name this header, so the ported functions take both values and the macros supply
// the caller's, matching eil/error.hpp:62 and nm/nm_exception.hpp:34.
//
// The messages end in "\n" in D and are passed as the second argument of assert(),
// so the newline is part of the text that reaches the log. Preserved byte for
// byte, including the trailing newline.

#include <cstddef>
#include <string>

namespace eil {
namespace util {

std::string brokenPreCondition(const std::string& variable, size_t lineNo,
                               const std::string& fileName);
std::string brokenPostCondition(const std::string& variable, size_t lineNo,
                                const std::string& fileName);
std::string failedUnitTest(size_t lineNo, const std::string& fileName);

}  // namespace util
}  // namespace eil

#define EIL_BROKEN_PRE_CONDITION(variable) \
    ::eil::util::brokenPreCondition((variable), __LINE__, __FILE__)
#define EIL_BROKEN_POST_CONDITION(variable) \
    ::eil::util::brokenPostCondition((variable), __LINE__, __FILE__)
#define EIL_FAILED_UNIT_TEST() ::eil::util::failedUnitTest(__LINE__, __FILE__)
