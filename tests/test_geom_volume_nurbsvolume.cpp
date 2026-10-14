// tests/test_geom_volume_nurbsvolume.cpp — C++ mirror of
// probes/geom/volume/probe_nurbsvolume.d, replayed line for line against
// baselines/e587b235/probes/geom_volume_nurbsvolume.txt (3524 D rows, sha256
// 552852424d09bd94f26f30092fda4c78a17c5e057ae5270735c4318346cd50c0).
//
// (A) below ports the module's single D `unittest` assertion (D:542,
// approxEqualVectors(Vector3(0.5,0.0,1.5), nvol(0.75,0.5,0.25)), inside the
// version(complex_numbers) else-branch at D:477-480 so it does run in this build);
// (B) replays the corpus.
//
// Why the sweep is what it is (same reasoning as the probe header): the main volume is
// a 5x5x5 net at p=q=r=2 over autoKnotVector(5,2) = [0,0,0,1/3,2/3,1,1,1], i.e. three
// spans per direction, and the seven sweep values per direction are 0.0, 1/6, 1/3, 1/2,
// 2/3, 0.999 and 1.0 — every span interior, both interior knots, both exact ends, and the
// two values that separate the base class' `>` / `<` difference-boundary tests
// (0.999+0.001 == 1.0 exactly). All 343 points carry the point itself plus all nine
// inherited finite-difference accessors, because the mixed partials call the FIRST
// derivatives, which branch on their own.
//
// The corpus also pins three things a tidy port would silently change:
//  * the ten module-static scratch buffers (D:421-430) shared by every NURBSVolume: the
//    nv.ord.* rows show a degree-1 volume's constructor making a degree-3 volume raise
//    "index [2] is out of bounds for array of length 2", and a degree-0 volume then
//    breaking BOTH degree-2 volumes with "index [1] is out of bounds for array of length
//    1" — reproduced, not fixed.
//  * the signed-vs-size_t mix in the seven guards (D:134 is int/int, D:144 and D:154 are
//    size_t), which lets p=q=r=-1 build and fail inside findSpan instead.
//  * D's `to!string(double)` = %.6g in every row tag, while the numbers under test print
//    at %.17g.
//
// Regenerate:
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/volume \
//     probes/geom/volume/probe_nurbsvolume.d -of=/tmp/probe_nvol \
//     -od/tmp/probe_obj_nurbsvol && /tmp/probe_nvol > \
//     baselines/e587b235/probes/geom_volume_nurbsvolume.txt
//
// Run: ./test_geom_volume_nurbsvolume <baseline.txt>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

#include "eil/eil.hpp"
#include "eil/format.hpp"
#include "geom/elements/vector3.hpp"
#include "geom/gpath/path.hpp" // eil::geom::d_tostr, used by NURBSVolume::toString
#include "geom/misc/nurbs_utils.hpp"
#include "geom/volume/nurbsvolume.hpp"
#include "geom/volume/parametricvolume.hpp"

namespace {

using eil::geom::NURBSVolume;
using eil::geom::ParametricVolume;
using eil::geom::PwArray;
using eil::geom::Vector3;

std::vector<std::string> g_lines;
int g_unittest_failures = 0;

void emit(const std::string& s) { g_lines.push_back(s); }

std::string esc(const std::string& s) {
    std::string out;
    for (char c : s) {
        if (c == '\n') out += "<newline>";
        else if (c == '\r') out += "<cr>";
        else out += c;
    }
    return out;
}

// D's t.classinfo.name for the four throwable shapes this module can produce. Exception
// and Error are siblings in the port (wave-01 lesson 1), RangeError derives from Error,
// and the two std.file/std.exception texts the ported readers raise are ported as
// ExceptionBase carrying D's message verbatim — so the class name is picked from the text,
// which is exactly the branch the probe distinguishes.
std::string d_throw_name(const eil::ExceptionBase& e) {
    const std::string m = e.msg();
    if (m.rfind("Cannot open file `", 0) == 0) return "std.exception.ErrnoException";
    if (m.find(": No such file or directory") != std::string::npos) return "std.file.FileException";
    return "object.Exception";
}
std::string d_throw_name(const eil::RangeError& e) {
    (void)e;
    return "core.exception.ArrayIndexError";
}
std::string d_throw_name(const eil::ErrorBase& e) {
    (void)e;
    return "object.Error";
}

template <typename F> void attempt(const std::string& key, F&& f) {
    try {
        f();
    } catch (const eil::RangeError& e) {
        emit(key + " thrown type=" + d_throw_name(e) + " msg=" + esc(e.msg()));
    } catch (const eil::ErrorBase& e) {
        emit(key + " thrown type=" + d_throw_name(e) + " msg=" + esc(e.msg()));
    } catch (const eil::ExceptionBase& e) {
        emit(key + " thrown type=" + d_throw_name(e) + " msg=" + esc(e.msg()));
    }
}

void dump_v3(const std::string& key, const Vector3& v) {
    emit(eil::format("%s x=%.17g y=%.17g z=%.17g", key, eil::re(v.x), eil::re(v.y),
                     eil::re(v.z)));
}

void ev(const std::string& key, const NURBSVolume& v, double u, double w, double t) {
    attempt(key, [&] { dump_v3(key, v(u, w, t)); });
}

// pwnet(nu, nv, nw) from the probe: a weighted net with no two components equal and no
// weight equal to 1. The products are written in D's operand order.
PwArray pwnet(int nu, int nv, int nw) {
    PwArray Pw;
    Pw.set_length(static_cast<size_t>(nu));
    for (int i = 0; i < nu; ++i) {
        Pw[static_cast<size_t>(i)].set_length(static_cast<size_t>(nv));
        for (int j = 0; j < nv; ++j) {
            Pw[static_cast<size_t>(i)][static_cast<size_t>(j)].set_length(
                static_cast<size_t>(nw));
            for (int k = 0; k < nw; ++k) {
                double x = 0.37 * i + 0.11 * i * i - 0.19 * j + 0.07 * k;
                double y = -0.23 * j + 0.13 * i * j + 0.29 * k - 0.05 * i;
                double z = 0.41 * k + 0.09 * i * k - 0.17 * j * k + 0.03 * i;
                double w = 1.0 + 0.2 * i - 0.13 * j + 0.07 * k;
                auto& c = Pw[static_cast<size_t>(i)][static_cast<size_t>(j)]
                             [static_cast<size_t>(k)];
                c[0] = x * w;
                c[1] = y * w;
                c[2] = z * w;
                c[3] = w;
            }
        }
    }
    return Pw;
}

eil::Array<double> arr(std::vector<double> v) {
    eil::Array<double> a;
    a.set_length(v.size());
    for (size_t i = 0; i < v.size(); ++i) a[i] = v[i];
    return a;
}

std::string one_line(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::string body((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    std::string out;
    size_t pos = 0;
    while (true) {
        size_t nl = body.find('\n', pos);
        if (nl == std::string::npos) { out += body.substr(pos); break; }
        out += body.substr(pos, nl - pos) + "<newline>";
        pos = nl + 1;
    }
    return out;
}

size_t file_len(const std::string& path) {
    std::ifstream in(path, std::ios::ate | std::ios::binary);
    return static_cast<size_t>(in.tellg());
}

// writeNetFile / writeTruncNetFile from the probe. The bytes are emitted as corpus rows
// (nv.net.body, nv.trunc.body) before either reader runs, so a reader row can only match
// if both sides really parsed the same file.
void writeNetFile(const std::string& fileName, int nPu, int nPv, int nPw) {
    std::ofstream f(fileName, std::ios::binary);
    f << eil::format("%d %d %d", nPu, nPv, nPw) << "\n";
    for (int comp = 0; comp < 3; ++comp) {
        for (int k = 0; k < nPw; ++k) {
            for (int j = 0; j < nPv; ++j) {
                for (int i = 0; i < nPu; ++i) {
                    double v = i + 10.0 * j + 100.0 * k + 1000.0 * comp;
                    f << eil::format("%.18e", v) << "\n";
                }
            }
        }
    }
}

void writeTruncNetFile(const std::string& fileName) {
    std::ofstream f(fileName, std::ios::binary);
    f << eil::format("%d %d %d", 3, 2, 2) << "\n";
    for (int n = 0; n < 11; ++n) f << eil::format("%.18e", static_cast<double>(n)) << "\n";
}

// The cylinder net of D's own unittest (D:482-531), verbatim, with s2 = sqrt(2.0).
PwArray cylNet() {
    const double s2 = eil::sqrt(2.0);
    const int nu = 4, nv = 2, nw = 9;
    static const double tbl[2][9][4] = {
        {{0.0, 0.0, -3.0, 1.0}, {0.0, 0.0, -3.0, 1.0}, {0.0, 0.0, -3.0, 1.0},
         {0.0, 0.0, -3.0, 1.0}, {0.0, 0.0, -3.0, 1.0}, {0.0, 0.0, -3.0, 1.0},
         {0.0, 0.0, -3.0, 1.0}, {0.0, 0.0, -3.0, 1.0}, {0.0, 0.0, -3.0, 1.0}},
        {{0.0, 1.0, -3.0, 1.0}, {1.0, 1.0, -3.0, 1.0 / s2}, {1.0, 0.0, -3.0, 1.0},
         {1.0, -1.0, -3.0, 1.0 / s2}, {0.0, -1.0, -3.0, 1.0}, {-1.0, -1.0, -3.0, 1.0 / s2},
         {-1.0, 0.0, -3.0, 1.0}, {-1.0, 1.0, -3.0, 1.0 / s2}, {0.0, 1.0, -3.0, 1.0}}};
    const double zlayer[4] = {-3.0, -1.0, 1.0, 3.0};
    PwArray Pw;
    Pw.set_length(static_cast<size_t>(nu));
    for (int i = 0; i < nu; ++i) {
        Pw[static_cast<size_t>(i)].set_length(static_cast<size_t>(nv));
        for (int j = 0; j < nv; ++j) {
            Pw[static_cast<size_t>(i)][static_cast<size_t>(j)].set_length(
                static_cast<size_t>(nw));
            for (int k = 0; k < nw; ++k) {
                std::array<double, 4> c = {tbl[j][k][0], tbl[j][k][1], zlayer[i], tbl[j][k][3]};
                Pw[static_cast<size_t>(i)][static_cast<size_t>(j)][static_cast<size_t>(k)] = c;
            }
        }
    }
    return Pw;
}

// ---------------------------------------------------------------------------
// (A) the D unittest block, assertion by assertion in source order
// ---------------------------------------------------------------------------
void port_unittest() {
    // D:477-480 is `version (complex_numbers) skip() else { ... }`; this build is the
    // else-branch, so the assertion below does run.
    // D:541-542
    int p = 3;
    int q = 1;
    int r = 2;
    eil::Array<double> Z = arr({0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0});
    eil::Array<double> R = arr({0.0, 0.0, 1.0, 1.0});
    eil::Array<double> T = arr({0.0, 0.0, 0.0, 0.25, 0.25, 0.5, 0.5, 0.75, 0.75, 1.0, 1.0, 1.0});
    NURBSVolume nvol(cylNet(), Z.all(), p, R.all(), q, T.all(), r);
    Vector3 P(0.5, 0.0, 1.5);
    const Vector3 got = nvol(0.75, 0.5, 0.25);
    if (!eil::geom::approxEqualVectors(P, got)) {
        ++g_unittest_failures;
        std::fprintf(stderr,
                     "D:542 assert(approxEqualVectors(P, nvol(0.75,0.5,0.25))) FAILED: got "
                     "%.17g %.17g %.17g\n",
                     eil::re(got.x), eil::re(got.y), eil::re(got.z));
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s <baseline.txt>\n", argv[0]);
        return 2;
    }

    port_unittest();

    const eil::Array<double> K2 = arr({0.0, 0.5, 1.0});
    const eil::Array<double> K3 = arr({0.0, 0.0, 1.0});
    const eil::Array<double> K4 = arr({0.0, 0.0, 1.0, 1.0});
    const eil::Array<double> K6 = arr({0.0, 0.0, 0.0, 1.0, 1.0, 1.0});
    const eil::Array<double> K7 = arr({0.0, 0.0, 0.0, 0.5, 1.0, 1.0, 1.0});
    const eil::Array<double> K9 = arr({0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0});

    // ===== group 1: the module's own unittest case =====
    int p = 3;
    int q = 1;
    int r = 2;
    eil::Array<double> Zk = arr({0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0});
    eil::Array<double> Rk = arr({0.0, 0.0, 1.0, 1.0});
    eil::Array<double> Tk = arr({0.0, 0.0, 0.0, 0.25, 0.25, 0.5, 0.5, 0.75, 0.75, 1.0, 1.0, 1.0});
    NURBSVolume cyl(cylNet(), Zk.all(), p, Rk.all(), q, Tk.all(), r);
    emit("nv.cyl str=" + cyl.toString());
    dump_v3("nv.cyl.unittest", cyl(0.75, 0.5, 0.25));
    for (double tt : {0.0, 0.125, 0.25, 0.375, 0.5, 0.75, 1.0}) {
        dump_v3("nv.cyl.w r=" + eil::to_string(0.75) + " s=" + eil::to_string(0.5) +
                    " t=" + eil::to_string(tt),
                cyl(0.75, 0.5, tt));
    }
    for (double uu : {0.0, 1.0}) {
        for (double vv : {0.0, 1.0}) {
            dump_v3("nv.cyl.cor r=" + eil::to_string(uu) + " s=" + eil::to_string(vv) +
                        " t=" + eil::to_string(0.5),
                    cyl(uu, vv, 0.5));
        }
    }

    // ===== group 2: the 5x5x5 quadratic volume and the 343-point sweep =====
    int nu = 5;
    int deg = 2;
    eil::Array<double> Uq = eil::geom::autoKnotVector(nu, deg);
    eil::Array<double> Vq = eil::geom::autoKnotVector(nu, deg);
    eil::Array<double> Wq = eil::geom::autoKnotVector(nu, deg);
    NURBSVolume vol5(pwnet(nu, nu, nu), Uq.all(), deg, Vq.all(), deg, Wq.all(), deg);
    emit("nv.main str=" + vol5.toString());
    emit(eil::format("nv.main dims nPu=%d nPv=%d nPw=%d nU=%d nV=%d nW=%d p=%d q=%d r=%d",
                     vol5.nPu, vol5.nPv, vol5.nPw, vol5.nU, vol5.nV, vol5.nW, vol5.p, vol5.q,
                     vol5.r));
    std::vector<double> pts = {vol5.U[0],
                               (vol5.U[0] + vol5.U[3]) / 2.0,
                               vol5.U[3],
                               (vol5.U[3] + vol5.U[4]) / 2.0,
                               vol5.U[4],
                               vol5.U[0] + 0.999,
                               1.0};
    const char* anames[10] = {"p",      "dVdr",    "dVds",   "dVdt",   "d2Vdr2",
                              "d2Vds2", "d2Vdt2",  "d2Vdrds", "d2Vdrdt", "d2Vdsdt"};
    for (double rr : pts) {
        for (double ss : pts) {
            for (double tt : pts) {
                const std::string tag = " r=" + eil::to_string(rr) + " s=" + eil::to_string(ss) +
                                        " t=" + eil::to_string(tt);
                dump_v3(std::string("nv.sweep.") + anames[0] + tag, vol5(rr, ss, tt));
                dump_v3(std::string("nv.sweep.") + anames[1] + tag, vol5.dVdr(rr, ss, tt));
                dump_v3(std::string("nv.sweep.") + anames[2] + tag, vol5.dVds(rr, ss, tt));
                dump_v3(std::string("nv.sweep.") + anames[3] + tag, vol5.dVdt(rr, ss, tt));
                dump_v3(std::string("nv.sweep.") + anames[4] + tag, vol5.d2Vdr2(rr, ss, tt));
                dump_v3(std::string("nv.sweep.") + anames[5] + tag, vol5.d2Vds2(rr, ss, tt));
                dump_v3(std::string("nv.sweep.") + anames[6] + tag, vol5.d2Vdt2(rr, ss, tt));
                dump_v3(std::string("nv.sweep.") + anames[7] + tag, vol5.d2Vdrds(rr, ss, tt));
                dump_v3(std::string("nv.sweep.") + anames[8] + tag, vol5.d2Vdrdt(rr, ss, tt));
                dump_v3(std::string("nv.sweep.") + anames[9] + tag, vol5.d2Vdsdt(rr, ss, tt));
            }
        }
    }

    // ===== group 3: unequal degrees, dup() and the copy constructor =====
    eil::Array<double> U1 = arr({0.0, 0.0, 1.0, 1.0});
    eil::Array<double> V2 = eil::geom::autoKnotVector(3, 2);
    eil::Array<double> W3 = eil::geom::autoKnotVector(4, 3);
    std::shared_ptr<NURBSVolume> uneq =
        std::make_shared<NURBSVolume>(pwnet(2, 3, 4), U1.all(), 1, V2.all(), 2, W3.all(), 3);
    emit("nv.uneq str=" + uneq->toString());
    ev("nv.uneq.a", *uneq, 0.0, 0.0, 0.0);
    ev("nv.uneq.b", *uneq, 0.5, 0.5, 0.5);
    ev("nv.uneq.c", *uneq, 1.0, 1.0, 1.0);
    ev("nv.uneq.d", *uneq, 0.25, V2[2], W3[4]);

    auto dupd = std::dynamic_pointer_cast<NURBSVolume>(uneq->dup());
    emit("nv.uneq.dup str=" + dupd->toString());
    ev("nv.uneq.dup.pt", *dupd, 0.5, 0.5, 0.5);
    NURBSVolume cp(*uneq);
    emit("nv.uneq.copy str=" + cp.toString());
    ev("nv.uneq.copy.pt", cp, 0.5, 0.5, 0.5);
    attempt("nv.uneq.copy.dVdr", [&] { dump_v3("nv.uneq.copy.dVdr", cp.dVdr(0.5, 0.5, 0.5)); });
    attempt("nv.uneq.copy.d2Vdsdt",
            [&] { dump_v3("nv.uneq.copy.d2Vdsdt", cp.d2Vdsdt(0.5, 0.5, 0.5)); });
    ev("nv.uneq.afterCopy", *uneq, 0.5, 0.5, 0.5);

    // ===== group 4: the seven guards, printing WHICH type was thrown =====
    auto tryBuild = [&](const std::string& key, const PwArray& Pw, const eil::Array<double>& U,
                        int pp, const eil::Array<double>& V, int qq, const eil::Array<double>& W,
                        int rr) {
        attempt(key, [&] {
            NURBSVolume v(Pw, U.all(), pp, V.all(), qq, W.all(), rr);
            emit(key + " built str=" + v.toString());
        });
    };
    tryBuild("nv.g1.onetu", pwnet(1, 2, 2), K4, 0, K4, 0, K4, 0);
    tryBuild("nv.g1.onetv", pwnet(2, 1, 2), K4, 0, K4, 0, K4, 0);
    tryBuild("nv.g0.emptyv", pwnet(2, 0, 0), K4, 0, K4, 0, K4, 0);
    tryBuild("nv.g2.degp", pwnet(3, 3, 3), K9, 4, K6, 2, K6, 2);
    tryBuild("nv.g3.degq", pwnet(3, 3, 3), K6, 2, K9, 4, K6, 2);
    tryBuild("nv.g4.degr", pwnet(3, 3, 3), K6, 2, K6, 2, K9, 4);
    tryBuild("nv.g5.lenU", pwnet(3, 3, 3), K4, 2, K6, 2, K6, 2);
    tryBuild("nv.g6.lenV", pwnet(3, 3, 3), K6, 2, K4, 2, K6, 2);
    tryBuild("nv.g7.lenW", pwnet(3, 3, 3), K6, 2, K6, 2, K4, 2);

    // ===== group 5: negative degrees =====
    tryBuild("nv.neg.p", pwnet(4, 3, 3), K4, -1, K6, 2, K6, 2);
    tryBuild("nv.neg.q", pwnet(4, 3, 3), K7, 2, K2, -1, K6, 2);
    tryBuild("nv.neg.r", pwnet(4, 3, 3), K7, 2, K6, 2, K2, -1);
    tryBuild("nv.neg.p5", pwnet(4, 3, 3), K2, -5, K6, 2, K6, 2);
    attempt("nv.neg.p.eval", [&] {
        NURBSVolume v(pwnet(4, 3, 3), K4.all(), -1, K6.all(), 2, K6.all(), 2);
        dump_v3("nv.neg.p.eval", v(0.5, 0.5, 0.5));
    });
    attempt("nv.neg.q.eval", [&] {
        NURBSVolume v(pwnet(4, 3, 3), K7.all(), 2, K2.all(), -1, K6.all(), 2);
        dump_v3("nv.neg.q.eval", v(0.5, 0.5, 0.5));
    });
    attempt("nv.neg.r.eval", [&] {
        NURBSVolume v(pwnet(4, 3, 3), K7.all(), 2, K6.all(), 2, K2.all(), -1);
        dump_v3("nv.neg.r.eval", v(0.5, 0.5, 0.5));
    });

    // ===== group 6: the module-static buffers, both construction orders =====
    eil::Array<double> U9 = eil::geom::autoKnotVector(5, 3);
    eil::Array<double> U4 = eil::geom::autoKnotVector(2, 1);
    NURBSVolume big(pwnet(5, 5, 5), U9.all(), 3, U9.all(), 3, U9.all(), 3);
    emit("nv.ord.big str=" + big.toString());
    ev("nv.ord.big.first", big, 0.5, 0.5, 0.5);
    attempt("nv.ord.big.dVdr", [&] { dump_v3("nv.ord.big.dVdr", big.dVdr(0.5, 0.5, 0.5)); });
    NURBSVolume small(pwnet(2, 2, 2), U4.all(), 1, U4.all(), 1, U4.all(), 1);
    emit("nv.ord.small str=" + small.toString());
    ev("nv.ord.small.first", small, 0.5, 0.5, 0.5);
    ev("nv.ord.big.afterSmall", big, 0.5, 0.5, 0.5);
    ev("nv.ord.small.afterFailedBig", small, 0.5, 0.5, 0.5);
    NURBSVolume bigAgain(pwnet(5, 5, 5), U9.all(), 3, U9.all(), 3, U9.all(), 3);
    ev("nv.ord.big.rebuilt", bigAgain, 0.5, 0.5, 0.5);
    ev("nv.ord.small.afterRebuild", small, 0.5, 0.5, 0.5);
    ev("nv.ord.oldbig.afterRebuild", big, 0.5, 0.5, 0.5);
    attempt("nv.ord.oldbig.d2Vdsdt",
            [&] { dump_v3("nv.ord.oldbig.d2Vdsdt", big.d2Vdsdt(0.5, 0.5, 0.5)); });
    eil::Array<double> U6 = eil::geom::autoKnotVector(3, 2);
    eil::Array<double> W10 = eil::geom::autoKnotVector(5, 4);
    NURBSVolume wideW(pwnet(3, 3, 5), U6.all(), 2, U6.all(), 2, W10.all(), 4);
    emit("nv.ord.wide str=" + wideW.toString());
    ev("nv.ord.wide.first", wideW, 0.5, 0.5, 0.5);
    NURBSVolume narrowW(pwnet(3, 3, 3), U6.all(), 2, U6.all(), 2, U6.all(), 2);
    ev("nv.ord.narrow.first", narrowW, 0.5, 0.5, 0.5);
    ev("nv.ord.wide.afterNarrow", wideW, 0.5, 0.5, 0.5);
    NURBSVolume wideAgain(pwnet(3, 3, 5), U6.all(), 2, U6.all(), 2, W10.all(), 4);
    ev("nv.ord.wide.rebuilt", wideAgain, 0.5, 0.5, 0.5);
    ev("nv.ord.narrow.afterWideRebuilt", narrowW, 0.5, 0.5, 0.5);
    NURBSVolume wideV(pwnet(3, 5, 3), U6.all(), 2, W10.all(), 4, U6.all(), 2);
    emit("nv.ord.wideV str=" + wideV.toString());
    ev("nv.ord.wideV.first", wideV, 0.5, 0.5, 0.5);
    ev("nv.ord.narrow.afterWideV", narrowW, 0.5, 0.5, 0.5);
    NURBSVolume zeroP(pwnet(2, 2, 2), K3.all(), 0, K3.all(), 0, K3.all(), 0);
    emit("nv.ord.zeroP str=" + zeroP.toString());
    ev("nv.ord.zeroP", zeroP, 0.5, 0.5, 0.5);
    ev("nv.ord.zeroP.bnd", zeroP, 0.0, 1.0, 0.0);
    ev("nv.ord.wideV.afterZeroP", wideV, 0.5, 0.5, 0.5);
    ev("nv.ord.narrow.afterZeroP", narrowW, 0.5, 0.5, 0.5);

    // ===== group 7: the file readers and writers =====
    const std::string fwt = "/tmp/probe_nvol_wt.txt";
    const std::string fnet = "/tmp/probe_nvol_net.txt";
    const std::string ftr = "/tmp/probe_nvol_trunc.txt";
    const std::string fvtk = "/tmp/probe_nvol_cntrl.vtk";
    const std::string fvol = "/tmp/probe_nvol_vtk.vtk";
    attempt("nv.wt", [&] { uneq->writeToFile(fwt); });
    emit("nv.wt.body=" + one_line(fwt));
    attempt("nv.rf", [&] {
        NURBSVolume v(fwt, 1.0);
        emit("nv.rf str=" + v.toString());
        dump_v3("nv.rf.a", v(0.5, 0.5, 0.5));
        dump_v3("nv.rf.b", v(0.0, 1.0, 0.0));
    });
    attempt("nv.rfs", [&] {
        NURBSVolume v(fwt, 2.5);
        emit("nv.rfs str=" + v.toString());
        dump_v3("nv.rfs.a", v(0.5, 0.5, 0.5));
    });
    std::remove(fwt.c_str());

    writeNetFile(fnet, 3, 2, 2);
    emit("nv.net.body=" + one_line(fnet));
    attempt("nv.net.sgrid", [&] {
        NURBSVolume v(fnet, std::string("sgrid"), {2, 1, 1}, 1.0);
        emit("nv.net.sgrid str=" + v.toString());
        dump_v3("nv.net.sgrid.a", v(0.5, 0.5, 0.5));
        dump_v3("nv.net.sgrid.b", v(0.0, 1.0, 0.5));
    });
    attempt("nv.net.plot3d", [&] {
        NURBSVolume v(fnet, std::string("plot3d-sgrid"), {2, 1, 1}, 1.0);
        emit("nv.net.plot3d str=" + v.toString());
        dump_v3("nv.net.plot3d.a", v(0.5, 0.5, 0.5));
        dump_v3("nv.net.plot3d.b", v(0.0, 1.0, 0.5));
    });
    attempt("nv.net.sgrid.scale", [&] {
        NURBSVolume v(fnet, std::string("sgrid"), {2, 1, 1}, 2.5);
        emit("nv.net.sgrid.scale str=" + v.toString());
        dump_v3("nv.net.sgrid.scale.a", v(0.5, 0.5, 0.5));
    });
    attempt("nv.net.badfmt", [&] {
        NURBSVolume v(fnet, std::string("igrid"), {2, 1, 1}, 1.0);
        emit("nv.net.badfmt str=" + v.toString());
    });
    std::remove(fnet.c_str());
    writeTruncNetFile(ftr);
    emit("nv.trunc.body=" + one_line(ftr));
    attempt("nv.net.truncated", [&] {
        NURBSVolume v(ftr, std::string("sgrid"), {2, 1, 1}, 1.0);
        emit("nv.net.truncated str=" + v.toString());
    });
    std::remove(ftr.c_str());

    attempt("nv.missing.r", [&] {
        NURBSVolume v(std::string("/tmp/probe_nvol_absent_volume.txt"), 1.0);
        emit("nv.missing.r str=" + v.toString());
    });
    attempt("nv.missing.net", [&] {
        NURBSVolume v(std::string("/tmp/probe_nvol_absent_net.txt"), std::string("sgrid"),
                      {2, 1, 1}, 1.0);
        emit("nv.missing.net str=" + v.toString());
    });

    attempt("nv.cntrl", [&] { uneq->writeCntrlNetAsVtkXml(fvtk); });
    emit("nv.cntrl.len=" + std::to_string(file_len(fvtk)));
    emit("nv.cntrl.body=" + one_line(fvtk));
    std::remove(fvtk.c_str());
    // Same as the probe: writeAsVtkXml evaluates the volume, so it runs on a handle
    // re-built through dup() (the validating constructor), which re-sizes the shared
    // static buffers back to uneq's degrees. See the probe header comment for the
    // measured base-class finding on the exceptional path.
    auto uneqFresh = std::dynamic_pointer_cast<NURBSVolume>(uneq->dup());
    attempt("nv.vtk", [&] { uneqFresh->writeAsVtkXml(fvol, 2, 2, 3); });
    emit("nv.vtk.len=" + std::to_string(file_len(fvol)));
    emit("nv.vtk.body=" + one_line(fvol));
    std::remove(fvol.c_str());

    // ===== group 8: leave the statics in a known state =====
    NURBSVolume finalV(pwnet(5, 5, 5), U9.all(), 3, U9.all(), 3, U9.all(), 3);
    ev("nv.final", finalV, 0.5, 0.5, 0.5);
    attempt("nv.final.d2Vdrdt",
            [&] { dump_v3("nv.final.d2Vdrdt", finalV.d2Vdrdt(0.5, 0.5, 0.5)); });

    // ------------------------------------------------------------------ replay
    std::ifstream in(argv[1]);
    if (!in) {
        std::fprintf(stderr, "cannot open baseline %s\n", argv[1]);
        return 2;
    }
    std::vector<std::string> expected;
    for (std::string l; std::getline(in, l);) expected.push_back(l);

    int line_diffs = 0, shown = 0;
    size_t compared = 0;
    // How many individual %.17g quantities the replay actually put on the wire, so the
    // summary reports quantities and not only whole lines (a row carries 3, and only rows
    // that are x/y/z dumps count — text rows carry no quantity).
    size_t quantities = 0;
    for (const std::string& l : expected) {
        size_t found = 0;
        size_t at = 0;
        while ((at = l.find("=", at + 1)) != std::string::npos && ++found < 4) {
        }
        if (l.find(" x=") != std::string::npos) quantities += 3;
    }
    for (size_t i = 0; i < expected.size() || i < g_lines.size(); ++i) {
        const std::string e = i < expected.size() ? expected[i] : "<none>";
        const std::string g = i < g_lines.size() ? g_lines[i] : "<none>";
        if (i < expected.size() && i < g_lines.size()) ++compared;
        if (e == g) continue;
        if (shown < 10) {
            std::printf("line %zu\n  D:   |%s|\n  C++: |%s|\n", i + 1, e.c_str(), g.c_str());
            ++shown;
        }
        ++line_diffs;
    }

    std::printf("%s: geom_volume_nurbsvolume, D unittest assertions failed %d; probe replay "
                "%zu D lines, %zu C++ lines, %d differ; %zu of %zu rows byte-identical; "
                "%zu of %zu x/y/z quantities identical at %%.17g (worst residual 0)\n",
                (line_diffs == 0 && g_unittest_failures == 0) ? "PASS" : "FAIL",
                g_unittest_failures, expected.size(), g_lines.size(), line_diffs,
                compared - static_cast<size_t>(line_diffs), compared,
                line_diffs == 0 ? quantities : quantities - 3 * static_cast<size_t>(line_diffs),
                quantities);
    return (line_diffs == 0 && g_unittest_failures == 0) ? 0 : 1;
}
