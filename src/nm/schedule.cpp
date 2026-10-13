// nm/schedule.d -> src/nm/schedule.cpp
//
// Schedule(T) is a class template, so its definition lives in schedule.hpp. This
// TU exists because CMakeLists lists it, and it verifies the header compiles on
// its own. It also pins the two instantiations D's unittest block uses: number
// (double) and int.

#include "nm/schedule.hpp"

namespace eil::nm::schedule {

template class Schedule<eil::nm::number>;
template class Schedule<int>;

} // namespace eil::nm::schedule
