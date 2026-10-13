#pragma once
// geom/geometry_exception.d -> src/geom/geometry_exception.hpp
// (module geom.geometry_exception).
//
// D is `class GeometryException : Exception`, with the ctor
//
//     this(string message, string file=__FILE__, size_t line=__LINE__,
//          Throwable next=null)                              // geometry_exception.d:11-15
//
// Non-1:1 notes (registered in provenance/fidelity.yml):
//   * It derives from eil::ExceptionBase, NOT eil::ErrorBase, because D's base is
//     Exception. Wave-1 lesson 1 keeps Exception and Error as siblings, so a ported
//     `catch (ExceptionBase e)` catches a GeometryException and a ported
//     `catch (ErrorBase e)` does not — measured in
//     probes/geom/elements/probe_projection.d ("geomthrow asException=true").
//   * `__FILE__`/`__LINE__` are D *default arguments*, which D expands at the
//     caller, so the file/line that reaches a solver log is the throw site's. A C++
//     default argument would name this header instead, so ported call sites throw
//     through EIL_THROW_GEOM (same shape as EIL_THROW_FLOW, eil/error.hpp:66-69).
//     Verified against ldc2: the probe's `throw new GeometryException(...)` reported
//     the probe's own file and line, and the throw inside projection.d reported
//     projection.d, not geometry_exception.d.
//   * The `Throwable next` chain argument is not represented: eil::ThrowableBase has
//     no chained-exception slot (nm_exception.hpp:12 records the same gap), and no
//     code in geom/ passes it.

#include <cstddef>
#include <string>

#include "eil/error.hpp"

namespace eil {
namespace geom {

class GeometryException : public eil::ExceptionBase {
  public:
    GeometryException(const std::string& message, const char* file = "", size_t line = 0)
        : eil::ExceptionBase(message, file, line) {}
};

} // namespace geom
} // namespace eil

// D's `throw new GeometryException(msg)` with the file/line defaults expanded at the
// call site.
#define EIL_THROW_GEOM(message) \
    throw ::eil::geom::GeometryException((message), __FILE__, __LINE__)
