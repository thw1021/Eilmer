// geom/volume/nurbsvolume.d -> src/geom/volume/nurbsvolume.cpp
// (module geom.volume.nurbsvolume).
//
// Definitions in D source order: the four constructors (D:30, :35, :38, :41), the three
// overrides (D:69-85), initialise (D:87-187), readFromCntrlNetFile (D:189-263),
// readFromFile (D:265-339), writeToFile (D:341-373), writeAsVtkXml (D:375-378),
// writeCntrlNetAsVtkXml (D:380-414) and eval (D:432-468).
//
// Inside eval the sequence is exactly D's, because every buffer it writes is shared
// process-wide: findSpan(u), basisFuns(u), findSpan(v), basisFuns(v), findSpan(w),
// basisFuns(w), the temp1 accumulation (m outer, l inner, D:441-450), the temp2
// accumulation (D:451-458), THEN the zeroing of mSw and mS (D:459-460 — after the
// accumulation loops, not before), the mSw accumulation over m (D:461-465), the division
// by mSw[3] (D:466) and Vector3(mS) (D:467). Hoisting the zeroing above the loops would
// leave the previous call's values in place and is the reason the order is commented
// rather than tidied.
//
// The arithmetic is additions, multiplications and divisions in D's operand order only:
// this module calls no transcendental function, so there is no `^^`/pow site in it and
// no eil::atan2 site either (grep 'atan\|\^\^' over nurbsvolume.d is empty).
//
// See the header for the non-1:1 notes (the ten static buffers and the construction-order
// interference, the three signed vs size_t guard comparisons, the negative-degree
// consequences, ErrorBase vs ExceptionBase for the eight throw sites, and the two
// D file-exception texts).
#include "geom/volume/nurbsvolume.hpp"

#include <array>
#include <cctype>
#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "eil/array.hpp"
#include "eil/conv.hpp"
#include "eil/error.hpp"
#include "eil/format.hpp"
#include "eil/number.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp" // for eil::geom::d_tostr (path.hpp:129)
#include "geom/misc/nurbs_utils.hpp"
#include "geom/volume/parametricvolume.hpp"

namespace eil {
namespace geom {

namespace {

// D's `readText(fileName)` (D:200) — std.file.readText, which checks existence first and
// therefore raises std.file.FileException "NAME: No such file or directory" (measured row
// nv.missing.net), not the ErrnoException that File(name,"r") raises.
std::string read_text_or_throw(const std::string& fileName) {
    std::ifstream in(fileName, std::ios::binary);
    if (!in) {
        throw eil::ExceptionBase(fileName + ": " + std::strerror(errno), __FILE__, __LINE__);
    }
    std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return body;
}

// D's `File(fileName, "r")` (D:272) — std.stdio, which raises
// std.exception.ErrnoException "Cannot open file `NAME' in mode `r' (reason)" (measured
// row nv.missing.r). The throwable carries D's text; the probe's replay prints the D class
// name for this branch.
eil::Array<std::string> read_lines_or_throw(const std::string& fileName) {
    std::ifstream in(fileName, std::ios::binary);
    if (!in) {
        throw eil::ExceptionBase("Cannot open file `" + fileName + "' in mode `r' (" +
                                     std::strerror(errno) + ")",
                                 __FILE__, __LINE__);
    }
    // D reads the file with File.readln() per line: the terminator is not part of the
    // string, and every readln() in this module is matched by a line the writer produced.
    // D's readln() past the last line yields null rather than an error; the port indexes a
    // bounded eil::Array instead, so such a read raises the same RangeError text D uses for
    // an out-of-range index. Not reachable from the corpus.
    eil::Array<std::string> lines;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        lines.push(line);
    }
    return lines;
}

// D's std.string.strip: both ends, whitespace only.
std::string d_strip(const std::string& s) {
    size_t b = 0;
    size_t e = s.size();
    while (b < e && std::isspace(static_cast<unsigned char>(s[b]))) ++b;
    while (e > b && std::isspace(static_cast<unsigned char>(s[e - 1]))) --e;
    return s.substr(b, e - b);
}

// D's std.array.split with no delimiter (D:200 `.split`, D:275 `.split()`): whitespace
// runs are separators and no empty token is produced. Returned as an eil::Array so that
// an out-of-range index raises the same RangeError text as D's ArrayIndexError (measured
// row nv.net.truncated: "index [14] is out of bounds for array of length 14").
eil::Array<std::string> d_split_ws(const std::string& s) {
    eil::Array<std::string> out;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
        size_t j = i;
        while (j < s.size() && !std::isspace(static_cast<unsigned char>(s[j]))) ++j;
        if (j > i) out.push(s.substr(i, j - i));
        i = j;
    }
    return out;
}

// One D `File(fileName, "w")` handle: writefln/writeln append a newline, as std.format's
// f.writefln does.
struct WriteFile {
    std::ofstream f;
    explicit WriteFile(const std::string& fileName) : f(fileName, std::ios::binary) {}
    void writeln(const std::string& s) { f << s << "\n"; }
    template <typename... A> void writefln(const char* fmt, A&&... a) {
        f << eil::format(fmt, std::forward<A>(a)...) << "\n";
    }
};

} // namespace

// D:421-430. One set for the whole process, as in D. The two static arrays start at
// number.init (NaN) exactly as D's `static number[4] mSw` does; eval writes every lane
// before reading it.
std::array<number, 4> NURBSVolume::mSw = {eil::d_init<number>(), eil::d_init<number>(),
                                          eil::d_init<number>(), eil::d_init<number>()};
std::array<number, 3> NURBSVolume::mS = {eil::d_init<number>(), eil::d_init<number>(),
                                         eil::d_init<number>()};
eil::Array<eil::Array<std::array<number, 4>>> NURBSVolume::temp1;
eil::Array<std::array<number, 4>> NURBSVolume::temp2;
eil::Array<double> NURBSVolume::mNu;
eil::Array<double> NURBSVolume::mNv;
eil::Array<double> NURBSVolume::mNw;
std::unique_ptr<NURBSWorkspace> NURBSVolume::mNws_u;
std::unique_ptr<NURBSWorkspace> NURBSVolume::mNws_v;
std::unique_ptr<NURBSWorkspace> NURBSVolume::mNws_w;

// D:30-34.
NURBSVolume::NURBSVolume(const PwArray& Pw_, eil::Slice<double> U_, int p_, eil::Slice<double> V_,
                         int q_, eil::Slice<double> W_, int r_) {
    initialise(Pw_, U_, p_, V_, q_, W_, r_); // D:33
}

// D:35-37.
NURBSVolume::NURBSVolume(const std::string& fileName, const std::string& fmt,
                         const std::array<int, 3>& degrees, double scale) {
    readFromCntrlNetFile(fileName, fmt, degrees, scale); // D:36
}

// D:38-40.
NURBSVolume::NURBSVolume(const std::string& fileName, double scale) {
    readFromFile(fileName, scale); // D:39
}

// D:41-68, `this(ref const NURBSVolume other)`. No validation at all, so every field is
// copied as-is and the static buffers are re-sized to the other volume's degrees. D:65-67
// use the *members* q and r here (already assigned at D:45-46), which is the same value
// the other path uses for its parameters.
NURBSVolume::NURBSVolume(const NURBSVolume& other) {
    this->Pw = duplicatePw(other.Pw); // D:43
    this->p = other.p;               // D:44
    this->q = other.q;               // D:45
    this->r = other.r;               // D:46
    this->U = other.U.dup();         // D:47
    this->V = other.V.dup();         // D:48
    this->W = other.W.dup();         // D:49
    this->nPu = other.nPu;           // D:50
    this->nPv = other.nPv;           // D:51
    this->nPw = other.nPw;           // D:52
    this->nU = other.nU;             // D:53
    this->nV = other.nV;             // D:54
    this->nW = other.nW;             // D:55
    this->m_a = other.m_a;           // D:56
    this->m_b = other.m_b;           // D:57
    this->m_c = other.m_c;           // D:58
    mNu.set_length(static_cast<size_t>(other.p + 1)); // D:59
    mNu.fill(eil::d_init<double>());                  // D:59's grown tail (see header)
    mNws_u = std::make_unique<NURBSWorkspace>(other.p); // D:60
    mNv.set_length(static_cast<size_t>(other.q + 1));   // D:61
    mNv.fill(eil::d_init<double>());
    mNws_v = std::make_unique<NURBSWorkspace>(other.q); // D:62
    mNw.set_length(static_cast<size_t>(other.r + 1));   // D:63
    mNw.fill(eil::d_init<double>());
    mNws_w = std::make_unique<NURBSWorkspace>(other.r); // D:64
    temp1.set_length(static_cast<size_t>(q + 1));       // D:65, the member q
    for (size_t i = 0; i < temp1.length(); ++i) {       // D:66
        temp1[i].set_length(static_cast<size_t>(r + 1));
    }
    temp2.set_length(static_cast<size_t>(r + 1));       // D:67, the member r
}

// D:69-72. Through the validating constructor, which re-derives every dimension.
std::shared_ptr<ParametricVolume> NURBSVolume::dup() const {
    return std::make_shared<NURBSVolume>(Pw, U.all(), p, V.all(), q, W.all(), r);
}

// D:73-76.
Vector3 NURBSVolume::operator()(double u, double v, double w) const { return eval(u, v, w); }

// D:77-85. to!string of the nested net, of the three knot vectors and of the three
// degrees, concatenated in D's order.
std::string NURBSVolume::toString() const {
    std::string repString = "NURBSVolume(Pw=";
    repString += d_tostr(Pw); // D:80
    repString += ", U=" + d_tostr(U) + ", p=" + eil::to_string(p);   // D:81
    repString += ", V=" + d_tostr(V) + ", q=" + eil::to_string(q);   // D:82
    repString += ", W=" + d_tostr(W) + ", r=" + eil::to_string(r) + ")"; // D:83
    return repString;
}

// D:87-187. The seven guards, in D's order, each keeping D's exact wording.
void NURBSVolume::initialise(const PwArray& Pw_, eil::Slice<double> U_, int p_,
                             eil::Slice<double> V_, int q_, eil::Slice<double> W_, int r_) {
    // Test Pw is minimum viable (D:90-100). D:91-93 assign the members directly and read
    // Pw[0][0].length before the guard runs, so an empty middle direction is a RangeError.
    nPu = eil::to<int>(Pw_.length());       // D:91
    nPv = eil::to<int>(Pw_[0].length());    // D:92
    nPw = eil::to<int>(Pw_[0][0].length()); // D:93
    if (nPu < 2 || nPv < 2) {               // D:94, short-circuits as D's || does
        std::string errMsg = "NURBSVolume() A NURBS volume requires at least two ";
        errMsg += "control points in each direction.\n";
        errMsg += eil::format("Supplied number of control points in direction u: %s\n", nPu);
        errMsg += eil::format("Supplied number of control points in direction v: %s", nPv);
        throw eil::ErrorBase(eil::text(errMsg), __FILE__, __LINE__); // D:99
    }

    // Test that p is valid (D:102-110). int/int comparison, so a negative degree passes.
    m_a = eil::to<int>(nPu - 1); // D:103
    if (p_ > m_a) {              // D:104
        std::string errMsg = "NURBSVolume() Degree of NURBS volume in direction u is not ";
        errMsg += "compatible with given number of control points.\n";
        errMsg += eil::format("Supplied degree in u direction: %s\n", p_);
        errMsg += eil::format("Maximum allowed degree in u direction: %s", Pw_.length() - 1);
        throw eil::ErrorBase(eil::text(errMsg), __FILE__, __LINE__); // D:109
    }

    // Test that q is valid (D:112-120).
    m_b = eil::to<int>(nPv - 1); // D:113
    if (q_ > m_b) {              // D:114
        std::string errMsg = "NURBSVolume() Degree of NURBS volume in direction v is not ";
        errMsg += "compatible with given number of control points.\n";
        errMsg += eil::format("Supplied degree in v direction: %s\n", q_);
        errMsg += eil::format("Maximum allowed degree in v direction: %s", Pw_[0].length() - 1);
        throw eil::ErrorBase(eil::text(errMsg), __FILE__, __LINE__); // D:119
    }

    // Test that r is valid (D:122-130).
    m_c = eil::to<int>(nPw - 1); // D:123
    if (r_ > m_c) {              // D:124
        std::string errMsg = "NURBSVolume() Degree of NURBS volume in direction w is not ";
        errMsg += "compatible with given number of control points.\n";
        errMsg += eil::format("Supplied degree in w direction: %s\n", r_);
        errMsg += eil::format("Maximum allowed degree in w direction: %s", Pw_[0][0].length() - 1);
        throw eil::ErrorBase(eil::text(errMsg), __FILE__, __LINE__); // D:129
    }

    // Test U knot vector is valid (D:132-140). Signed here...
    nU = eil::to<int>(U_.length()); // D:133
    if (nU != m_a + p_ + 2) {       // D:134
        std::string errMsg = "NURBSVolume() Knot vector in direction u is not compatible ";
        errMsg += "with given number control points and volume degree.\n";
        errMsg += eil::format("Supplied number of knots in direction u: %s\n", U_.length());
        errMsg += eil::format("Required number of knots in direction u: %s", m_a + p_ + 2);
        throw eil::ErrorBase(eil::text(errMsg), __FILE__, __LINE__); // D:139
    }

    // Test V knot vector is valid (D:142-150). ...and size_t here (D:144), because D
    // converts the int side of `V.length != m_b+q+2`.
    nV = eil::to<int>(V_.length()); // D:143
    if (V_.length() != static_cast<size_t>(m_b + q_ + 2)) {
        std::string errMsg = "NURBSVolume() Knot vector in direction v is not compatible ";
        errMsg += "with given number control points and volume degree.\n";
        errMsg += eil::format("Supplied number of knots in direction v: %s\n", V_.length());
        errMsg += eil::format("Required number of knots in direction v: %s", m_b + q_ + 2);
        throw eil::ErrorBase(eil::text(errMsg), __FILE__, __LINE__); // D:149
    }

    // Test W knot vector is valid (D:152-160). size_t as at D:154.
    nW = eil::to<int>(W_.length()); // D:153
    if (W_.length() != static_cast<size_t>(m_c + r_ + 2)) {
        std::string errMsg = "NURBSVolume() Knot vector in direction w is not compatible ";
        errMsg += "with given number control points and volume degree.\n";
        errMsg += eil::format("Supplied number of knots in direction w: %s\n", W_.length());
        errMsg += eil::format("Required number of knots in direction w: %s", m_c + r_ + 2);
        throw eil::ErrorBase(eil::text(errMsg), __FILE__, __LINE__); // D:159
    }

    // Everything passed: take ownership of the inputs (D:162-177). The six
    // `this.x = x` lines at D:169-174 assign the members to themselves, because D:91-93
    // and D:133-153 already wrote them; preserved as D wrote it.
    this->Pw = duplicatePw(Pw_); // D:162
    this->p = p_;                // D:163
    this->q = q_;                // D:164
    this->r = r_;                // D:165
    this->U = eil::Array<double>(U_); // D:166 U.dup
    this->V = eil::Array<double>(V_); // D:167 V.dup
    this->W = eil::Array<double>(W_); // D:168 W.dup
    this->nPu = nPu;             // D:169
    this->nPv = nPv;             // D:170
    this->nPw = nPw;             // D:171
    this->nU = nU;               // D:172
    this->nV = nV;               // D:173
    this->nW = nW;               // D:174
    this->m_a = m_a;             // D:175
    this->m_b = m_b;             // D:176
    this->m_c = m_c;             // D:177
    // D:178-186 re-sizes the ten process-wide static buffers to THIS volume's degrees.
    mNu.set_length(static_cast<size_t>(p_ + 1));      // D:178
    mNu.fill(eil::d_init<double>());
    mNws_u = std::make_unique<NURBSWorkspace>(p_);    // D:179
    mNv.set_length(static_cast<size_t>(q_ + 1));      // D:180
    mNv.fill(eil::d_init<double>());
    mNws_v = std::make_unique<NURBSWorkspace>(q_);    // D:181
    mNw.set_length(static_cast<size_t>(r_ + 1));      // D:182
    mNw.fill(eil::d_init<double>());
    mNws_w = std::make_unique<NURBSWorkspace>(r_);    // D:183
    temp1.set_length(static_cast<size_t>(q_ + 1));    // D:184
    for (size_t i = 0; i < temp1.length(); ++i) {     // D:185
        temp1[i].set_length(static_cast<size_t>(r_ + 1));
    }
    temp2.set_length(static_cast<size_t>(r_ + 1));    // D:186
} // end initialise()

// D:189-263.
void NURBSVolume::readFromCntrlNetFile(const std::string& fileName, const std::string& fmt,
                                       const std::array<int, 3>& degrees, double scale) {
    // Constructs NURBS volume from control net file.
    //
    // Supported file formats:
    //   "plot3d-sgrid": Plot3D structured grid format (order n,k,j,i)
    //   "sgrid": standard structured grid format (order i,j,k,n)

    // import hull file (D:199-200)
    eil::Array<std::string> hullData = d_split_ws(read_text_or_throw(fileName));

    // get number of control points in each direction (D:202-205)
    int nPu_ = eil::to<int>(hullData[0]);
    int nPv_ = eil::to<int>(hullData[1]);
    int nPw_ = eil::to<int>(hullData[2]);

    // initialise control point array (D:207-211). D's `new number[4][][][](a,b,c)` gives
    // a.net of a x b x c weighted points; the fourth lane starts at number[4].init and is
    // overwritten below (the coordinate loops cover lanes 0..2, the weight loop lane 3).
    PwArray Pw_;
    Pw_.set_length(static_cast<size_t>(nPu_));
    for (int i = 0; i < nPu_; ++i) {
        Pw_[static_cast<size_t>(i)].set_length(static_cast<size_t>(nPv_));
        for (int j = 0; j < nPv_; ++j) {
            Pw_[static_cast<size_t>(i)][static_cast<size_t>(j)].set_length(
                static_cast<size_t>(nPw_));
        }
    }
    int ni = 0, nj = 0, nk = 0, nl = 0; // D:209
    int iOffset = 0, jOffset = 0, kOffset = 0, offset = 0; // D:210
    int headOffset = 3; // D:211

    // determine order in which control points are imported (D:213-222)
    if (fmt == "plot3d-sgrid") {
        ni = 3; nj = nPw_; nk = nPv_; nl = nPu_; // D:215
    } else if (fmt == "sgrid") {
        ni = nPu_; nj = nPv_; nk = nPw_; nl = 3; // D:217
    } else {
        std::string errMsg = eil::format("Invalid NURBS net format: '%s'. ", fmt);
        errMsg += "Accepted options are 'plot3d-sgrid' or 'sgrid'.";
        throw eil::ErrorBase(errMsg, __FILE__, __LINE__); // D:219-221, a bare Error
    }

    // import control net (D:224-242)
    for (int i = 0; i < ni; ++i) {
        iOffset = i * nj * nk * nl;
        for (int j = 0; j < nj; ++j) {
            jOffset = j * nk * nl;
            for (int k = 0; k < nk; ++k) {
                kOffset = k * nl;
                for (int n = 0; n < nl; ++n) {
                    // add header offset so cntrl net dimensions are not included (D:232-233)
                    offset = iOffset + jOffset + kOffset + headOffset + n;
                    if (fmt == "plot3d-sgrid") {
                        Pw_[static_cast<size_t>(n)][static_cast<size_t>(k)][static_cast<size_t>(j)]
                          [static_cast<size_t>(i)] =
                            eil::to<double>(hullData[static_cast<size_t>(offset)]) * scale; // D:235
                    } else if (fmt == "sgrid") {
                        Pw_[static_cast<size_t>(i)][static_cast<size_t>(j)][static_cast<size_t>(k)]
                          [static_cast<size_t>(n)] =
                            eil::to<double>(hullData[static_cast<size_t>(offset)]) * scale; // D:237
                    }
                }
            }
        }
    }

    // set all weights equal to 1.0 for initial hull (D:244-251)
    for (int i = 0; i < nPu_; ++i) {
        for (int j = 0; j < nPv_; ++j) {
            for (int k = 0; k < nPw_; ++k) {
                Pw_[static_cast<size_t>(i)][static_cast<size_t>(j)][static_cast<size_t>(k)][3] =
                    1.0;
            }
        }
    }

    // create uniform knot vectors that are clamped and normalised (D:253-259)
    int p_ = degrees[0];
    int q_ = degrees[1];
    int r_ = degrees[2];
    eil::Array<double> U_ = autoKnotVector(nPu_, p_);
    eil::Array<double> V_ = autoKnotVector(nPv_, q_);
    eil::Array<double> W_ = autoKnotVector(nPw_, r_);

    // initialise NURBS volume (D:261-262)
    initialise(Pw_, U_.all(), p_, V_.all(), q_, W_.all(), r_);
} // end readFromCntrlNetFile()

// D:265-339.
void NURBSVolume::readFromFile(const std::string& fileName, double scale) {
    int p_, q_, r_, nU_, nV_, nW_, nPu_, nPv_, nPw_; // D:267
    eil::Array<double> U_, V_, W_;                   // D:268

    eil::Array<std::string> lines = read_lines_or_throw(fileName); // D:272
    size_t ln = 0;
    // D:274-278. degrees
    eil::Array<std::string> tokens = d_split_ws(d_strip(lines[ln]));
    ++ln;
    p_ = eil::to<int>(tokens[0]);
    q_ = eil::to<int>(tokens[1]);
    r_ = eil::to<int>(tokens[2]);

    // D:280-287. knot vector U
    nU_ = eil::to<int>(d_strip(lines[ln]));
    ++ln;
    U_.set_length(static_cast<size_t>(nU_));
    for (int i = 0; i < nU_; ++i) {
        U_[static_cast<size_t>(i)] = eil::to<double>(d_strip(lines[ln]));
        ++ln;
    }

    // D:289-296. knot vector V
    nV_ = eil::to<int>(d_strip(lines[ln]));
    ++ln;
    V_.set_length(static_cast<size_t>(nV_));
    for (int i = 0; i < nV_; ++i) {
        V_[static_cast<size_t>(i)] = eil::to<double>(d_strip(lines[ln]));
        ++ln;
    }

    // D:298-305. knot vector W
    nW_ = eil::to<int>(d_strip(lines[ln]));
    ++ln;
    W_.set_length(static_cast<size_t>(nW_));
    for (int i = 0; i < nW_; ++i) {
        W_[static_cast<size_t>(i)] = eil::to<double>(d_strip(lines[ln]));
        ++ln;
    }

    // D:307-311. control net dimensions
    tokens = d_split_ws(d_strip(lines[ln]));
    ++ln;
    nPu_ = eil::to<int>(tokens[0]);
    nPv_ = eil::to<int>(tokens[1]);
    nPw_ = eil::to<int>(tokens[2]);

    // D:313-324. import control net
    PwArray Pw_;
    Pw_.set_length(static_cast<size_t>(nPu_));
    for (int i = 0; i < nPu_; ++i) {
        Pw_[static_cast<size_t>(i)].set_length(static_cast<size_t>(nPv_));
        for (int j = 0; j < nPv_; ++j) {
            Pw_[static_cast<size_t>(i)][static_cast<size_t>(j)].set_length(
                static_cast<size_t>(nPw_));
        }
    }
    for (int i = 0; i < nPu_; ++i) {
        for (int j = 0; j < nPv_; ++j) {
            for (int k = 0; k < nPw_; ++k) {
                tokens = d_split_ws(d_strip(lines[ln]));
                ++ln;
                for (int n = 0; n < 4; ++n) {
                    Pw_[static_cast<size_t>(i)][static_cast<size_t>(j)][static_cast<size_t>(k)]
                      [static_cast<size_t>(n)] = eil::to<double>(tokens[static_cast<size_t>(n)]);
                }
            }
        }
    }

    // D:326-335. multiply control point coords by respective weight and scale
    for (int i = 0; i < nPu_; ++i) {
        for (int j = 0; j < nPv_; ++j) {
            for (int k = 0; k < nPw_; ++k) {
                for (int n = 0; n < 3; ++n) {
                    auto& lane = Pw_[static_cast<size_t>(i)][static_cast<size_t>(j)]
                                    [static_cast<size_t>(k)][static_cast<size_t>(n)];
                    const number wgt = Pw_[static_cast<size_t>(i)][static_cast<size_t>(j)]
                                           [static_cast<size_t>(k)][3];
                    lane *= scale * wgt; // D:331, the product groups as D writes it
                }
            }
        }
    }

    // D:337-338
    initialise(Pw_, U_.all(), p_, V_.all(), q_, W_.all(), r_);
} // end readFromFile()

// D:341-373.
void NURBSVolume::writeToFile(const std::string& fileName) {
    WriteFile f(fileName); // D:343
    std::string line;      // D:344

    f.writefln("%d %d %d", this->p, this->q, this->r); // D:346
    f.writefln("%d", this->nU);                        // D:347
    for (int i = 0; i < this->nU; ++i) {               // D:348-350
        f.writefln("%.18e", this->U[static_cast<size_t>(i)]);
    }
    f.writefln("%d", this->nV);                        // D:351
    for (int i = 0; i < this->nV; ++i) {               // D:352-354
        f.writefln("%.18e", this->V[static_cast<size_t>(i)]);
    }
    f.writefln("%d", this->nW);                        // D:355
    for (int i = 0; i < this->nW; ++i) {               // D:356-358
        f.writefln("%.18e", this->W[static_cast<size_t>(i)]);
    }
    f.writefln("%d %d %d", this->nPu, this->nPv, this->nPw); // D:359
    for (int i = 0; i < nPu; ++i) {
        for (int j = 0; j < nPv; ++j) {
            for (int k = 0; k < nPw; ++k) {
                line = "";
                for (int n = 0; n < 3; ++n) { // D:364-367
                    line += eil::format("%.18e ",
                                        eil::re(this->Pw[static_cast<size_t>(i)][static_cast<size_t>(j)]
                                                    [static_cast<size_t>(k)]
                                                        [static_cast<size_t>(n)]) /
                                            eil::re(this->Pw[static_cast<size_t>(i)]
                                                        [static_cast<size_t>(j)]
                                                            [static_cast<size_t>(k)][3]));
                }
                line += eil::format(
                    "%.18e", eil::re(this->Pw[static_cast<size_t>(i)][static_cast<size_t>(j)]
                                         [static_cast<size_t>(k)][3])); // D:368
                f.writeln(line);                                       // D:369
            }
        }
    }
} // end writeToFile()

// D:375-378.
void NURBSVolume::writeAsVtkXml(const std::string& fileName, int nuPts, int nvPts, int nwPts) {
    write_volume_as_vtk_xml(*this, fileName, nuPts, nvPts, nwPts);
}

// D:380-414.
void NURBSVolume::writeCntrlNetAsVtkXml(const std::string& fileName) {
    // TODO: just convert control net as sgrid, then export (D:382, kept verbatim)
    Vector3 pn;          // D:383. D's `Vector3 p;` is Vector3.init (NaN per field), and
                         // every field is assigned before it is printed (D:397-399), so
                         // the NaN never reaches the file.
    int nrPts = this->nPu; // D:384
    int nsPts = this->nPv; // D:385
    int ntPts = this->nPw; // D:386

    WriteFile f(fileName); // D:388
    f.writeln("<VTKFile type=\"StructuredGrid\" version=\"1.0\" header_type=\"UInt64\">");
    f.writefln("  <StructuredGrid WholeExtent=\"%d %d %d %d %d %d\">", 0, nrPts - 1, 0, nsPts - 1,
               0, ntPts - 1);
    f.writefln("    <Piece Extent=\"%d %d %d %d %d %d\">", 0, nrPts - 1, 0, nsPts - 1, 0,
               ntPts - 1);
    f.writeln("      <Points>");
    f.writeln("        <DataArray type=\"Float32\" Name=\"Points\" NumberOfComponents=\"3\" "
              "format=\"ascii\">");
    for (int k = 0; k < ntPts; ++k) {
        for (int j = 0; j < nsPts; ++j) {
            for (int i = 0; i < nrPts; ++i) {
                const auto& c = this->Pw[static_cast<size_t>(i)][static_cast<size_t>(j)]
                                    [static_cast<size_t>(k)];
                pn.x = eil::re(c[0]) / eil::re(c[3]); // D:397
                pn.y = eil::re(c[1]) / eil::re(c[3]); // D:398
                pn.z = eil::re(c[2]) / eil::re(c[3]); // D:399
#ifdef EIL_COMPLEX_NUMBERS
                // D:400-401, version(complex_numbers): not compiled (wave-02-brief
                // section 2, T-904).
                f.writefln("       %20.16e %20.16e %20.16e", pn.x.re, pn.y.re, pn.z.re);
#else
                f.writefln("       %20.16e %20.16e %20.16e", eil::re(pn.x), eil::re(pn.y),
                           eil::re(pn.z)); // D:403
#endif
            }
        }
    }
    f.writeln("        </DataArray>");
    f.writeln("      </Points>");
    f.writeln("    </Piece>");
    f.writeln("  </StructuredGrid>");
    f.writeln("</VTKFile>");
    f.f.close(); // D:413
} // end writeCntrlNetAsVtkXml()

// D:432-468, algorithm A4.4 from Piegl and Tiller (1997) - 'The NURBS Book'.
Vector3 NURBSVolume::eval(double u, double v, double w) const {
    // Returns the Cartesian coordinates of a point on a NURBS volume at a
    // given set of parameter values
    int uSpan = findSpan(u, m_a, p, U.all());              // D:435
    basisFuns(uSpan, u, p, U.all(), mNu.all(), *mNws_u);   // D:436
    int vSpan = findSpan(v, m_b, q, V.all());              // D:437
    basisFuns(vSpan, v, q, V.all(), mNv.all(), *mNws_v);   // D:438
    int wSpan = findSpan(w, m_c, r, W.all());              // D:439
    basisFuns(wSpan, w, r, W.all(), mNw.all(), *mNws_w);   // D:440
    for (int m = 0; m < r + 1; ++m) {                      // D:441
        for (int l = 0; l < q + 1; ++l) {                  // D:442
            for (int n = 0; n < 4; ++n) {                  // D:443
                temp1[static_cast<size_t>(l)][static_cast<size_t>(m)][static_cast<size_t>(n)] =
                    0.0;
            }
            for (int k = 0; k < p + 1; ++k) {              // D:444
                const auto& c = Pw[static_cast<size_t>(uSpan - p + k)][static_cast<size_t>(
                    vSpan - q + l)][static_cast<size_t>(wSpan - r + m)];
                for (int n = 0; n < 4; ++n) {              // D:445-447
                    temp1[static_cast<size_t>(l)][static_cast<size_t>(m)][static_cast<size_t>(n)] +=
                        mNu[static_cast<size_t>(k)] * c[static_cast<size_t>(n)];
                }
            }
        }
    }
    for (int m = 0; m < r + 1; ++m) {                      // D:451
        for (int n = 0; n < 4; ++n) {                      // D:452
            temp2[static_cast<size_t>(m)][static_cast<size_t>(n)] = 0.0;
        }
        for (int l = 0; l < q + 1; ++l) {                  // D:453
            for (int n = 0; n < 4; ++n) {                  // D:454-456
                temp2[static_cast<size_t>(m)][static_cast<size_t>(n)] +=
                    mNv[static_cast<size_t>(l)] *
                    temp1[static_cast<size_t>(l)][static_cast<size_t>(m)][static_cast<size_t>(n)];
            }
        }
    }
    for (int n = 0; n < 4; ++n) { mSw[n] = 0.0; }          // D:459
    for (int n = 0; n < 3; ++n) { mS[n] = 0.0; }           // D:460
    for (int m = 0; m < r + 1; ++m) {                      // D:461
        for (int n = 0; n < 4; ++n) {                      // D:462-464
            mSw[n] += mNw[static_cast<size_t>(m)] * temp2[static_cast<size_t>(m)][static_cast<size_t>(n)];
        }
    }
    for (int i = 0; i < 3; ++i) { mS[i] = mSw[i] / mSw[3]; } // D:466
    // D:467 Vector3(mS) reads the three elements (geom/elements/vector3.d:82-93).
    return Vector3(eil::Slice<double>(mS.data(), mS.size(), nullptr, 0));
} // end eval()

} // namespace geom
} // namespace eil
