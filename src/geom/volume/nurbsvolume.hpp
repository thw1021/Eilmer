#pragma once
// geom/volume/nurbsvolume.d -> src/geom/volume/nurbsvolume.hpp
// (module geom.volume.nurbsvolume).
//
// A ParametricVolume that is a trivariate NURBS: Piegl & Tiller's A4.4 de Boor
// evaluation (D:432-468) over a weighted control net Pw, three knot vectors U/V/W and
// three degrees p/q/r, using the shared kernels from geom/misc/nurbs_utils.{hpp,cpp}
// (findSpan, basisFuns, NURBSWorkspace, duplicatePw, autoKnotVector) rather than a
// second copy of them, exactly as src/geom/surface/nurbssurface.hpp and
// src/geom/gpath/nurbs.hpp do for the surface and curve cases of the same algorithm.
//
// Namespace: eil::geom, flat. NURBSVolume collides with nothing in geom (grep over
// gdtk src/geom).
//
// D's aggregate headers are replaced by the modules actually used (wave-02-brief
// section 4 item 5): `import geom.elements` (D:15) -> geom/elements/vector3.hpp,
// `import geom.volume.parametricvolume` (D:16) stays, `import geom.misc.nurbs_utils`
// (D:17) stays, `import nm.number` (D:19) -> eil/number.hpp. `import ntypes.complex`
// (D:20) is needed only by the uncompiled version(complex_numbers) branch and
// `std.{array,conv,file,format,stdio,string}` (D:8-13) by the substrate equivalents.
//
// Non-1:1 notes (all registered in provenance/fidelity.yml):
//   * adapted-handles: D:69 `override NURBSVolume dup() const` returns a class handle,
//     so the port returns std::shared_ptr<ParametricVolume> (wave-02-brief 4b item 1).
//     dup() rebuilds through the VALIDATING constructor as D:71 does, so a dup of a
//     valid volume re-derives nPu/m_a/... ; the copy constructor (D:41-68), which
//     validates nothing, is the only unchecked way to make one.
//   * D:421-430 declares mSw, mS, temp1, temp2, mNu, mNv, mNw and the three
//     NURBSWorkspaces as *static* class fields: one set per process, shared by every
//     NURBSVolume, and every constructor re-sizes them to its own degrees (D:178-186,
//     D:59-67). The port keeps them static, interference and all, as
//     src/geom/gpath/nurbs.hpp:15-24 and nurbssurface.hpp:24-34 do. Measured on the D
//     binary (rows nv.ord.* of
//     baselines/e587b235/probes/geom_volume_nurbsvolume.txt): building a degree-1
//     volume after a degree-3 one leaves mNu/mNv/mNw at length 2, so the degree-3
//     volume's next evaluation raises "index [2] is out of bounds for array of length
//     2" from nurbs_utils.d:63; rebuilding the degree-3 volume clears it, and the
//     degree-1 volume keeps working because it only reads a subset. A degree-0 volume
//     shrinks every buffer to length 1 and then BOTH degree-2 volumes fail with
//     "index [1] is out of bounds for array of length 1" (rows
//     nv.ord.wideV.afterZeroP / nv.ord.narrow.afterZeroP) — the sharing is not merely
//     a resize race, it can break a volume that has not been touched since it was
//     built. The three workspaces are D `static NURBSWorkspace`, which has only an
//     explicit NURBSWorkspace(int p) constructor, so they are held as std::unique_ptr
//     and reassigned by both constructors (nurbs.cpp:38 does the same for the curve).
//   * The public data members keep D's names and D's visibility (D:24-28) because
//     probes and later consumers read them; `initialise` assigns them directly while it
//     validates and then re-assigns them from themselves at D:169-177. That
//     self-assignment is preserved (it is what makes the guards report the values the
//     object ends up with) rather than tidied away.
//   * The seven guards are `throw new Error(text(errMsg))` (D:99, :109, :119, :129,
//     :139, :149, :159) -> eil::ErrorBase, and the invalid-format guard at D:221 is also
//     an Error. Only findSpan's out-of-range parameter is an Exception
//     (nurbs_utils.d:33). Exception and Error are siblings (wave-01 lesson 1), so a
//     caller's catch (ExceptionBase) does not swallow these Errors, as in D. The probe
//     prints which type fired.
//   * The guards are NOT all the same comparison, and that is load-bearing: D:104, :114,
//     :124 compare `p > m_a` between two ints (signed), D:134 compares the int nU
//     against the int m_a+p+2 (signed), while D:144 and :154 compare `V.length` /
//     `W.length` (size_t) against the int sum, which D converts to size_t. So a negative
//     degree passes every degree guard and only the U check can reject it, and only when
//     m_a+p+2 is not equal to the supplied length. Measured: p=-1, q=-1 and r=-1 each
//     build (rows nv.neg.p/q/r built), and each then fails in findSpan at U[n+1] — one
//     past a knot vector that is short by exactly the degree deficit (rows
//     nv.neg.*.eval, "index [4] is out of bounds for array of length 4" for p=-1 on a
//     4-point net). p=-5 is rejected by the U guard, which reports "Required number of
//     knots in direction u: 0" (row nv.neg.p5).
//   * D:91-93 read Pw[0][0].length before guard 1 runs, so a net whose middle direction
//     is empty is a RangeError rather than the "at least two control points" Error (row
//     nv.g0.emptyv: "index [0] is out of bounds for array of length 0").
//   * D:185 `foreach (i; 0 .. temp1.length) temp1[i].length = r+1;` re-sizes EVERY row of
//     the shared temp1, so rows keep their old identity when the array shrinks and grows;
//     the port does the same over the current length.
//   * `mNu.length = p+1` (D:178) in D leaves the grown tail at double.init (NaN,
//     wave-02-brief section 5), while eil::Array::set_length value-initialises; the port
//     restores d_init so the buffers' initial state is D's. It is not observable here —
//     basisFuns writes N[0..p] before eval reads them — but see nurbs_utils.hpp:37-44 for
//     why the NaN is not hidden.
//   * D:200 `readText(fileName).split` and D:275 `f.readln().strip().split()` are
//     whitespace tokenisation with empty fields dropped; the file access itself is
//     std.file/std.stdio in D and std::ifstream/std::ofstream here, because the substrate
//     has no File type (finding, see the wave report). D's two failure texts are
//     reproduced verbatim rather than dropped: `File(name,"r")` raises
//     std.exception.ErrnoException "Cannot open file `<name>' in mode `r' (No such file
//     or directory)" and `readText` raises std.file.FileException
//     "<name>: No such file or directory" (measured rows nv.missing.r / nv.missing.net).
//   * D:400-404's `version (complex_numbers)` branch is kept as
//     #ifdef EIL_COMPLEX_NUMBERS and not compiled (wave-02-brief section 2). eil::re() is
//     the single spelling correct for both variant states.
//   * There is no classString() override, because D:77-85 declares only toString():
//     `grep -n classString /home/tang/packages/gdtk/src/geom/volume/*.d` is empty, and
//     geom/volume/parametricvolume.d declares no such virtual (unlike
//     geom/gpath/path.d:30). So the volume hierarchy has no classString to print.

#include <array>
#include <cstddef>
#include <memory>
#include <string>

#include "eil/array.hpp"
#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/misc/nurbs_utils.hpp"
#include "geom/volume/parametricvolume.hpp"

namespace eil {
namespace geom {

class NURBSVolume : public ParametricVolume {
  public:
    // D:24-28. Public in D, and read by consumers (and by the probe), so they stay
    // public here. number[4][][][] is PwArray (nurbs_utils.hpp:85).
    PwArray Pw;         // D:24 weighted control point array
    eil::Array<double> U, V, W; // D:25 knot vectors
    int p = eil::d_init<int>(); // D:26 spline degrees
    int q = eil::d_init<int>();
    int r = eil::d_init<int>();
    int nPu = eil::d_init<int>(); // D:27 dimensions of control net
    int nPv = eil::d_init<int>();
    int nPw = eil::d_init<int>();
    int nU = eil::d_init<int>();  // D:28 lengths of knot vector arrays
    int nV = eil::d_init<int>();
    int nW = eil::d_init<int>();

    // D:30-34. The validating constructor; it just calls initialise().
    NURBSVolume(const PwArray& Pw, eil::Slice<double> U, int p, eil::Slice<double> V, int q,
                eil::Slice<double> W, int r);
    // D:35-37, `this(string fileName, string fmt, int[3] degrees, double scale=1.0)`.
    NURBSVolume(const std::string& fileName, const std::string& fmt,
                const std::array<int, 3>& degrees, double scale = 1.0);
    // D:38-40, `this(string fileName, double scale=1.0)`.
    explicit NURBSVolume(const std::string& fileName, double scale = 1.0);
    // D:41-68, `this(ref const NURBSVolume other)`. Validates nothing, as in D.
    NURBSVolume(const NURBSVolume& other);

    std::shared_ptr<ParametricVolume> dup() const override; // D:69-72
    Vector3 operator()(double u, double v, double w) const override; // D:73-76 opCall
    std::string toString() const override;                 // D:77-85

    // D:87-187. Validates and then assigns everything, including the static buffers.
    void initialise(const PwArray& Pw, eil::Slice<double> U, int p, eil::Slice<double> V,
                    int q, eil::Slice<double> W, int r);

    // D:189-263. fmt is "plot3d-sgrid" (order n,k,j,i) or "sgrid" (order i,j,k,n).
    void readFromCntrlNetFile(const std::string& fileName, const std::string& fmt,
                              const std::array<int, 3>& degrees, double scale);
    // D:265-339.
    void readFromFile(const std::string& fileName, double scale = 1.0);
    // D:341-373.
    void writeToFile(const std::string& fileName);
    // D:375-378, defaults are D's 20/20/20.
    void writeAsVtkXml(const std::string& fileName, int nuPts = 20, int nvPts = 20,
                       int nwPts = 20);
    // D:380-414.
    void writeCntrlNetAsVtkXml(const std::string& fileName);

  private:
    int m_a = eil::d_init<int>(); // D:417 a+1 = number of control points in direction u
    int m_b = eil::d_init<int>(); // D:418
    int m_c = eil::d_init<int>(); // D:419

    // D:421-430. Static in D, so shared by every NURBSVolume in the process.
    static std::array<number, 4> mSw;
    static std::array<number, 3> mS;
    static eil::Array<eil::Array<std::array<number, 4>>> temp1;
    static eil::Array<std::array<number, 4>> temp2;
    static eil::Array<double> mNu;
    static eil::Array<double> mNv;
    static eil::Array<double> mNw;
    static std::unique_ptr<NURBSWorkspace> mNws_u;
    static std::unique_ptr<NURBSWorkspace> mNws_v;
    static std::unique_ptr<NURBSWorkspace> mNws_w;

    // D:432-468, algorithm A4.4 from Piegl and Tiller (1997) - 'The NURBS Book'. A const
    // method in D that writes the static buffers above; C++ permits the same.
    Vector3 eval(double u, double v, double w) const;
}; // end class NURBSVolume

} // namespace geom
} // namespace eil
