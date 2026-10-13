#pragma once
// gasdyn/gasflowexception.d -> src/gasdyn/gasflowexception.hpp
// (module gasdyn.gasflowexception).
//
// D declares exactly one type:
//
//     class GasFlowException : Exception {
//         @nogc this(string message, string file=__FILE__, size_t line=__LINE__,
//              Throwable next=null) { super(message, file, line, next); }
//     }
//
// Non-1:1 notes (registered in provenance/fidelity.yml):
//   * `: Exception` maps to eil::ExceptionBase, NOT eil::ErrorBase: in D Exception
//     and Error are siblings, and `catch (Exception e)` at the call sites
//     (idealgasflow.d:467-485, 726-736, plus nm/bracketing.d:63-70) must keep
//     catching this type while a ported Error stays uncatchable by them.
//   * D's `__FILE__`/`__LINE__` *caller* defaults cannot be spelled in C++: a
//     default argument here would name this header. Call sites use
//     EIL_THROW_GASFLOW, which passes the throw site's __FILE__/__LINE__, so the
//     location that reaches solver logs is the call site's, as in D. Same device
//     as eil/error.hpp:69 (EIL_THROW_FLOW) and src/nm/nm_exception.hpp:33.
//   * The `Throwable next` chaining slot is not represented (nothing in the tree
//     passes it, and ExceptionBase has no chain member), exactly as for
//     nm.nm_exception.
//   * `@nogc` is a GC contract on the D side only; the C++ constructor allocates
//     its std::string, which is what D does when the collector is enabled.

#include <cstddef>
#include <string>

#include "eil/error.hpp"

namespace eil {
namespace gasdyn {

class GasFlowException : public eil::ExceptionBase {
  public:
    GasFlowException(const std::string& message, const char* file = "", size_t line = 0)
        : eil::ExceptionBase(message, file, line) {}
};

} // namespace gasdyn
} // namespace eil

#define EIL_THROW_GASFLOW(message) \
    throw ::eil::gasdyn::GasFlowException((message), __FILE__, __LINE__)
