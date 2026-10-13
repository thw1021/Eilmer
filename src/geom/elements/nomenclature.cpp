// geom/elements/nomenclature.d -> src/geom/elements/nomenclature.cpp
//
// Definitions in D source order: face_name (D:20), face_index (D:23-35),
// radians (D:56), quaternion_init (D:68). See the header for the non-1:1 notes.

#include "geom/elements/nomenclature.hpp"

#include <limits>
#include <string>

#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/number.hpp"

namespace eil {
namespace geom {

// D:20 — the six canonical face names, indexed by the Face values 0..5.
eil::Array<std::string> face_name = { "west", "east", "south", "north", "bottom", "top" };

// D:23-35. The default branch raises a D *Error*, so this throws ErrorBase and is
// deliberately not catchable as ExceptionBase.
unsigned face_index(const std::string& name) {
    if (name == "west" || name == "iminus") return Face::west;
    if (name == "east" || name == "iplus") return Face::east;
    if (name == "south" || name == "jminus") return Face::south;
    if (name == "north" || name == "jplus") return Face::north;
    if (name == "bottom" || name == "kminus") return Face::bottom;
    if (name == "top" || name == "kplus") return Face::top;
    throw eil::ErrorBase(eil::text("Invalid face name: ", name), __FILE__, __LINE__);
} // end face_index

// D:56. D's PI is an 80-bit real, so `PI*degrees/180.0` is evaluated in extended
// precision and rounded once when the double is returned. Writing M_PI here would
// change the last bit for some angles (measured, probe_nomenclature "rad" rows:
// 30, 60, 1/3 and 1e8 degrees all differ by 1 ULP).
double radians(double degrees) {
    return static_cast<double>(EIL_PI * static_cast<eil::real>(degrees) / 180.0L);
}

// D:68 `alias Quaternion = double[4]`; D's default for such a declaration is
// `double[4].init` = four NaN, because `double.init` is NaN.
Quaternion quaternion_init() {
    const double nan_init = std::numeric_limits<double>::quiet_NaN();
    return Quaternion{ nan_init, nan_init, nan_init, nan_init };
}

} // namespace geom
} // namespace eil
