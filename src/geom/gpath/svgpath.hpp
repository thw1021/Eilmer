#pragma once
// geom/gpath/svgpath.d -> src/geom/gpath/svgpath.hpp (module geom.gpath.svgpath).
//
// An SVG path mini-language ("M3.0,3.0;L4.0,3.0;v1.0;h-1.0;Z") interpreted into a
// Polyline of Line/Bezier segments (the commands svgpath.d:111-198 covers: M m L l
// H h V v Q q C c Z).
//
// D's import list (svgpath.d:13-22) pulls nm.bbla, ntypes.complex and nm.number for
// the single `to!number(...)` at D:136 and D:148; with number == double (nm/number.hpp)
// that conversion is the identity, so the port has no nm dependency of its own.
//
// Non-1:1 notes (all registered in provenance/fidelity.yml):
//   * adapted-handles: SVGPath *is* a Polyline (D:25), and Polyline stores its
//     segments as shared_ptr<Path>, so dup() returns std::shared_ptr<Path> (brief 4b.1).
//   * D:37-38 calls interpret_svg_path(txt) and THEN super(segments, closed, tolerance).
//     C++ constructs the base first, so the parsing runs in the static parse() helper
//     below, into an SvgParseState that holds exactly D's private fields (D:68-79) in
//     the same names; the base is then built from that state and the fields are
//     copied into the object. Statement order, the arithmetic and the mutation of the
//     current point are unchanged (this is the same adaptation Polyline used for its
//     delegating constructor, see polyline.hpp:15-19).
//   * D's constructor never assigns its own `txt` FIELD (svgpath.d:32-37 sets cp, p0,
//     xtrans..tolerance and calls interpret_svg_path(txt), where `txt` is the
//     PARAMETER, D:27). The field (D:68) therefore stays at string.init == null for the
//     life of the object. That is load-bearing and preserved:
//       - toString() (D:56) prints path="" for every SVGPath (measured: every
//         "svg.*.str" row of baselines/e587b235/probes/geom_gpath_svgpath.txt), and
//       - dup() (D:49-52) and the copy constructor (D:41-47) re-parse the EMPTY string,
//         which produces no segments, so Polyline's guard throws
//         Error "Polyline() No segments present." — SVGPath objects are NOT copyable in
//         D as written. Measured: rows svg.dup / svg.copy / svg.err.moveonly. The port
//         keeps `txt` default-constructed ("" == D's null for printing purposes) and
//         does not store the parameter.
//   * D:77-78 shadow Polyline's public `segments` and `closed` with module-private
//     copies; the port keeps the shadowing (same names, same types) so D's reads of
//     `segments ~= ...` and `closed = true` address the SVGPath field, exactly as in D.
//   * D:86 `char cmd = cmdStr[0]` on an EMPTY command string is a RangeError
//     ("index [0] is out of bounds for array of length 0", measured on the
//     svg.err.emptysegment row: "M1.0,1.0;" has a trailing empty piece). C++ would read
//     the terminating NUL, so the port throws eil::RangeError at that point.
//   * D:113-186 read coordinates with std.format.formattedRead. There is no input-side
//     %g in the substrate (T-9xx candidate), so svgpath_detail::formatted_read_g below
//     implements the subset svgpath.d uses ("%g" conversions, the literal ',' and the
//     whitespace-skipping ' '), including Phobos' three error texts verbatim, because
//     those strings are observable through the exception message. Every rule it follows
//     is measured in the fmt.* rows of the same baseline.
//   * D's `throw new Exception("Unknown SVG command: " ~ cmd)` (D:197) is an Exception;
//     the Polyline guard that D:38 reaches is an Error. Siblings, so the port's catch
//     clauses distinguish them exactly as D does.

#include <memory>
#include <string>
#include <vector>

#include "eil/array.hpp"
#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp"
#include "geom/gpath/polyline.hpp"

namespace eil {
namespace geom {

// The %g input reader standing in for std.format.formattedRead (see the header note).
// `targets` must be the same length as the number of %g conversions in `fmt`; D's call
// sites always pass exactly that (svgpath.d:113-186). A FormatException-equivalent in D
// becomes eil::ExceptionBase carrying the same message text.
namespace svgpath_detail {

void formatted_read_g(const std::string& in, const char* fmt,
                      const std::vector<double*>& targets);

} // namespace svgpath_detail

class SVGPath : public Polyline {
  public:
    // D:27-39
    SVGPath(const std::string& txt, double xtrans = 0.0, double ytrans = 0.0,
            double xscale = 1.0, double yscale = 1.0, double tolerance = 1.0e-10);
    // D:41-47, `this(ref const(SVGPath) other)`. Throws, as D's does: other.txt is
    // the unassigned field, so this re-parses the empty string.
    SVGPath(const SVGPath& other);

    std::shared_ptr<Path> dup() const override;
    std::string toString() const override;
    std::string classString() const override;

  private:
    // The parse state of D:68-79, which D mutates through interpret_svg_path and
    // do_svg_path_command before it calls super(). Defined in svgpath.cpp; the two
    // D member functions are its members there, so they run against these fields and
    // not against a half-constructed SVGPath.
    struct Parsed;
    static Parsed parse(const std::string& spec, double xt, double yt, double xs,
                        double ys, double tol);
    // The real constructor; the public one delegates to it once parse() is done.
    SVGPath(const Parsed& p, const std::string& spec, double xt, double yt, double xs,
            double ys, double tol);

    std::string txt;      // D:68 the original text specification — never assigned (above)
    Vector3 p0;           // D:69 Starting point.
    Vector3 cp;           // D:70 Current point.
    // D:71-75 Apply the translation first, to absolute coordinates. ... Apply the
    // scales second, to both absolute and relative coordinates.
    double xtrans = eil::d_init<number>();
    double ytrans = eil::d_init<number>();
    double xscale = eil::d_init<number>();
    double yscale = eil::d_init<number>();
    // D:76-79 A place to store the generated gpath segments.
    PathList segments;    // shadows Polyline::segments, as D:77 does
    bool closed = false;  // shadows Polyline::closed, as D:78 does
    double tolerance = eil::d_init<number>();
}; // end class SVGPath

} // namespace geom
} // namespace eil
