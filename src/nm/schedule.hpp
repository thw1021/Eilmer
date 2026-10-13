#pragma once
// schedule.d
// Select or interpolate a value from a schedule of values.
//
// PJ, 2020-10-10 extracted from l1d/config.d
//
// nm/schedule.d -> src/nm/schedule.hpp (module nm.schedule).
//
// Naming. namespace eil::nm::schedule, matching the convention in nm/brent.hpp.
//
// Reference semantics. Schedule(T) is a D class, so a variable is a reference;
// the two tables are held in a shared body so a copy of a Schedule aliases it,
// as in D. Nothing in the module resizes them after construction.
//
// D's `shared double[] t_change` / `shared T[] values` (schedule.d:59-60) carry
// D's shared qualifier, which has no C++ counterpart and is not used for any
// locking here; the fields are plain arrays in the port.
//
// D's `isIntegral!T` (std.traits) selects the rounding branch of
// interpolate_value; `if constexpr` with std::is_integral is the same test.

#include <cmath>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "eil/array.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/number.hpp"
#include "nm/number.hpp"

namespace eil::nm {
namespace schedule {

template <typename T> class Schedule;

// The `new Schedule!T(...)` forms (see the note at the top of nm/bbla.hpp for
// why array-literal constructors become factories).
template <typename T>
Schedule<T> new_schedule(eil::Slice<double> t_change, eil::Slice<T> values);
template <typename T>
Schedule<T> new_schedule(std::initializer_list<double> t_change,
                         std::initializer_list<T> values);

template <typename T> class Schedule {
  public:
    struct Body {
        eil::Array<double> t_change;
        eil::Array<T> values;
    };

    // schedule.d:16-23 `this(double[] t_change, T[] values)`
    Schedule(eil::Slice<double> t_change_, eil::Slice<T> values_)
        : _body(std::make_shared<Body>()) {
        auto n = t_change_.length();
        eil_assert(n > 0);                    // "Need at least one value in the array."
        eil_assert(n == values_.length());    // "Inconsistent array lengths."
        // D's `this.t_change.length = n; this.t_change[] = t_change[];` -- grow,
        // then copy element by element.
        _body->t_change.set_length(n);
        _body->values.set_length(n);
        for (size_t i = 0; i < n; ++i) {
            _body->t_change[i] = t_change_[i];
            _body->values[i] = values_[i];
        }
    }

    Schedule(const Schedule&) = default;      // aliases, as a D class reference does
    Schedule& operator=(const Schedule&) = default;

    // schedule.d:25-28 `override string toString()`
    std::string toString() const {
        return eil::format("Schedule(t_change=%s, values=%s)", _body->t_change, _body->values);
    }

    // schedule.d:30-37 `@nogc T get_value(double t)`
    T get_value(double t) const {
        // Select one of our tabulated schedule of values.
        int i = static_cast<int>(_body->t_change.length()) - 1;
        while ((i > 0) && (t < _body->t_change[i])) { --i; }
        return _body->values[static_cast<size_t>(i)];
    }

    // schedule.d:39-56 `@nogc T interpolate_value(double t)`
    T interpolate_value(double t) const {
        // Attempt an interpolation of the tabulated schedule of values.
        if (t <= _body->t_change[0]) { return _body->values[0]; }
        if (t >= _body->t_change[_body->t_change.length()-1]) {
            return _body->values[_body->values.length()-1];
        }
        // If we get to this point, we must have at least 2 values in our schedule
        // and we can interpolate between a pair of them.
        int i = static_cast<int>(_body->t_change.length()) - 1;
        while ((i > 0) && (t < _body->t_change[i])) { --i; }
        double frac = (t-_body->t_change[static_cast<size_t>(i)])
                    /(_body->t_change[static_cast<size_t>(i)+1]-_body->t_change[static_cast<size_t>(i)]);
        double value = (1.0-frac)*_body->values[static_cast<size_t>(i)]
                     + frac*_body->values[static_cast<size_t>(i)+1];
        if constexpr (std::is_integral<T>::value) {
            return static_cast<T>(std::round(value));
        } else {
            return value;
        }
    }

  private:
    std::shared_ptr<Body> _body;
}; // end class Schedule

template <typename T>
inline Schedule<T> new_schedule(eil::Slice<double> t_change, eil::Slice<T> values) {
    return Schedule<T>(t_change, values);
}

template <typename T>
inline Schedule<T> new_schedule(std::initializer_list<double> t_change,
                                std::initializer_list<T> values) {
    // Read-only views of the literals' backing arrays, which outlive the call.
    return Schedule<T>(eil::Slice<double>(const_cast<double*>(t_change.begin()),
                                          t_change.size(), nullptr, 0),
                       eil::Slice<T>(const_cast<T*>(values.begin()),
                                     values.size(), nullptr, 0));
}

} // namespace schedule
} // namespace eil::nm
