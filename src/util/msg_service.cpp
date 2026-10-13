// util/msg_service.d -> src/util/msg_service.cpp
//
// D marks these `pure`; C++ has no equivalent annotation that the solver uses, and
// nothing depends on purity here -- the bodies are pure in the D sense (no global
// state touched). See msg_service.hpp for why file/line are not default arguments.

#include "util/msg_service.hpp"

#include "eil/format.hpp"

namespace eil {
namespace util {

// util/msg_service.d:13-19
std::string brokenPreCondition(const std::string& variable, size_t lineNo,
                               const std::string& fileName) {
    return eil::format("Pre-condition contract broken for %s on line %d in file %s\n", variable,
                       lineNo, fileName);
}

// util/msg_service.d:21-27
std::string brokenPostCondition(const std::string& variable, size_t lineNo,
                                const std::string& fileName) {
    return eil::format("Post-condition contract broken for %s on line %d in file %s\n", variable,
                       lineNo, fileName);
}

// util/msg_service.d:29-34
std::string failedUnitTest(size_t lineNo, const std::string& fileName) {
    return eil::format("Unit test failure on line %d in file %s\n", lineNo, fileName);
}

}  // namespace util
}  // namespace eil
