#pragma once
// eil/error.hpp — failure paths with D-matching text.
//
// The messages below are copied from Phobos output captured by
// tools/fmt_probe.d (tools/fmt_corpus.txt), not invented:
//
//   range:  index [10] is out of bounds for array of length 3
//   conv:   error converting input to floating point for input "not a number".
//
// Two shapes matter for log equivalence:
//   * FlowSolverException is the solver's own exception class
//     (globalconfig.d:70-77). It takes file/line defaults the way D does, and
//     its message is what catch blocks in main.d/steadystate_core.d print.
//   * D's enforce(cond, "fmt", args...) was measured to keep the format string
//     *unsubstituted* in .msg. That is what enforce() does here; if a ported
//     call site is found whose message depends on substitution, record it in
//     reports/ rather than quietly changing this helper.

#include <cstdio>
#include <stdexcept>
#include <string>

namespace eil {

// Common root: D's Throwable carries .msg/.file/.line for *both* branches, so
// ported code that prints e.msg works for Exceptions and Errors alike.
class ThrowableBase : public std::runtime_error {
  public:
    explicit ThrowableBase(const std::string& msg, const char* file = "", size_t line = 0)
        : std::runtime_error(msg), file_(file), line_(line) {}
    std::string msg() const { return std::string(what()); }
    const char* file() const { return file_.c_str(); }
    size_t line() const { return line_; }
    // D's Throwable.toString() is "file(line): message"; some call sites print it.
    std::string toString() const {
        if (file_.empty()) return msg();
        return file_ + "(" + std::to_string(line_) + "): " + msg();
    }

  private:
    std::string file_;
    size_t line_;
};

// D splits Throwable into Exception and Error, and `catch (Exception e)` does not
// catch an Error (RangeError, OutOfMemoryError, assert failures). Deriving
// ErrorBase from ExceptionBase would silently turn a reference-solver abort into
// a recovered error in the port, so the two stay siblings.
class ExceptionBase : public ThrowableBase {
  public:
    using ThrowableBase::ThrowableBase;
};

class ErrorBase : public ThrowableBase {
  public:
    using ThrowableBase::ThrowableBase;
};

// globalconfig.d:70
class FlowSolverException : public ExceptionBase {
  public:
    FlowSolverException(const std::string& message, const char* file = "", size_t line = 0)
        : ExceptionBase(message, file, line) {}
};

// D's exception ctor defaults are `__FILE__`/`__LINE__` at the *caller*. A C++
// default argument would expand to this header instead, so call sites use this
// macro to keep the file/line text that ends up in solver logs.
#define EIL_THROW_FLOW(message) throw ::eil::FlowSolverException((message), __FILE__, __LINE__)

// core.exception.RangeError, as surfaced by `arr[i]` and slice bounds.
class RangeError : public ErrorBase {
  public:
    RangeError(size_t index, size_t length, const char* file = "", size_t line = 0)
        : ErrorBase(range_text(index, length), file, line), index_(index), length_(length) {}
    static std::string range_text(size_t index, size_t length) {
        char buf[96];
        std::snprintf(buf, sizeof buf, "index [%llu] is out of bounds for array of length %llu",
                      static_cast<unsigned long long>(index),
                      static_cast<unsigned long long>(length));
        return buf;
    }
    size_t index() const { return index_; }
    size_t length() const { return length_; }

  private:
    size_t index_;
    size_t length_;
};

class SliceError : public ErrorBase {
  public:
    SliceError(const std::string& m, const char* file = "", size_t line = 0)
        : ErrorBase(m, file, line) {}
};

// std.conv.ConvException. The message arrives already in Phobos' wording — the
// taxonomy has five distinct shapes (see eil/conv.hpp), so this class must not
// re-wrap it in a template.
class ConversionException : public ExceptionBase {
  public:
    explicit ConversionException(const std::string& message, const char* file = "", size_t line = 0)
        : ExceptionBase(message, file, line) {}
};

// std.exception.enforce. Message text is the raw first argument (see header note).
template <typename... Args>
inline void enforce(bool condition, const std::string& msg, const Args&...) {
    if (!condition) throw ExceptionBase(msg);
}
inline void enforce(bool condition, const std::string& msg) {
    if (!condition) throw ExceptionBase(msg);
}

// D's `assert(...)` is a hard runtime check compiled into the release solver
// (249 sites in src/eilmer, 297 in geom alone), so it is not std::assert: it throws
// and stays live.
//
// What `.msg` holds was measured, because the previous version of this helper was
// wrong for every one of those sites. ldc2 1.40.0, caught with `catch (Error e)`:
//
//     assert(i == 3, "index i is invalid")   e.msg == "index i is invalid"
//     assert(i == 3)                          e.msg == "Assertion failure"
//
// i.e. with a message, `.msg` is EXACTLY that message -- no "Assertion failure"
// prefix -- and without one it is the bare words "Assertion failure", NOT
// "Assertion failure: <stringified condition>". The old code appended `#cond` in both
// cases, which is text the reference never produces; it surfaces in logs because
// harness/normalize_log.py byte-compares the exception text a job script sees.
// The file/line the port still carries lives in .file()/.line(), matching
// ThrowableBase; D puts it in the class prefix of toString() instead, and that
// prefix cannot be reproduced here (reports/backlog.md T-903).
inline void d_assert(bool condition, const char* file, size_t line) {
    if (!condition) throw ErrorBase("Assertion failure", file, line);
}
inline void d_assert_msg(bool condition, const char* msg, const char* file, size_t line) {
    if (!condition) throw ErrorBase(std::string(msg), file, line);
}
#define eil_assert(cond) ::eil::d_assert((cond), __FILE__, __LINE__)
#define eil_assert_msg(cond, msg) ::eil::d_assert_msg((cond), (msg), __FILE__, __LINE__)

// core.exception.OutOfMemoryError and friends appear via `new` failing; kept as a
// distinct type so ported catch clauses stay honest.
class OutOfMemoryError : public ErrorBase {
  public:
    using ErrorBase::ErrorBase;
};

} // namespace eil
