#pragma once
/**
 * An exception to signal problems specifically
 * in the numerical methods functions.
 *
 * Authors: Rowan G. and Peter J.
 * Date: 2019-12-05
 */
// nm/nm_exception.d -> src/nm/nm_exception.hpp (module nm.nm_exception).
//
// D's ctor takes `(string message, string file=__FILE__, size_t line=__LINE__,
// Throwable next=null)`. The substrate ExceptionBase carries message/file/line but
// has no chained-Throwable slot, so `next` is not represented; no code in the
// solver passes it. The file/line defaults are taken at the throw site through
// EIL_THROW_NM, because a C++ default argument would name this header instead
// (same reason eil/error.hpp:62 has EIL_THROW_FLOW).

#include <cstddef>
#include <string>

#include "eil/error.hpp"

namespace eil::nm {

class NumericalMethodException : public eil::ExceptionBase {
  public:
    NumericalMethodException(const std::string& message, const char* file = "", size_t line = 0)
        : eil::ExceptionBase(message, file, line) {}
};

} // namespace eil::nm

#define EIL_THROW_NM(message) \
    throw ::eil::nm::NumericalMethodException((message), __FILE__, __LINE__)
