// probes/geom/volume/probe_nurbsvolume.d — what geom/volume/nurbsvolume.d produces.
//
// NURBSVolume is a ParametricVolume that is a trivariate NURBS: Piegl & Tiller A4.4
// evaluation (D:432-468) over a weighted control net Pw, three knot vectors U/V/W and
// three degrees p/q/r, plus seven validation guards (D:94-160), two file readers
// (D:189-339), three file writers (D:341-414) and ten module-static scratch buffers
// (D:421-430) shared by every instance in the process.
//
// Why the sweep is what it is. The main volume is a 5x5x5 net with p=q=r=2 over the
// clamped uniform knot vectors autoKnotVector(5,2) = [0, 0, 0, 1/3, 2/3, 1, 1, 1], so
// each parametric direction has exactly three spans: [0,1/3), [1/3,2/3), [2/3,1]. The
// sweep values per direction are therefore
//   0.0    -> first knot (findSpan's u < U[0] tolerance branch is NOT taken; the lower
//             boundary), and the base's `x - d < 0` one-sided difference branch
//   1/6    -> interior of span 0
//   1/3    -> the interior knot itself, i.e. the u == U[n+1] / span-start case
//   0.5    -> interior of span 1
//   2/3    -> the second interior knot
//   0.999  -> interior of span 2, and the base's central branch that 0.999+0.001 == 1.0
//             makes observable (a port using >= fails only here)
//   1.0    -> last knot, upper one-sided branch of every finite difference
// which hits every span at least once, every knot boundary, and both exact ends. The
// knot values are taken from the volume's own public U/V/W (D:25) so both sides of the
// replay read the same doubles, not two spellings of a third.
//
// The nine inherited finite-difference accessors are dumped at all 343 sweep points
// (7^3): the mixed partials call the FIRST derivatives, which branch on their own, so
// one r can put the outer function on its central branch while the inner dVds() takes
// a one-sided one. See probes/geom/volume/probe_parametricvolume.d for the branch
// analysis of the base class itself.
//
// Deliberately not cleaned up, and dumped instead:
//  * the ten module-static buffers (D:421-430) are shared by every NURBSVolume and
//    re-sized by whichever constructor last ran, so evaluating a high-degree volume
//    after a low-degree one is constructed reads past the end of mNu/temp1/temp2 and D
//    raises core.exception.ArrayIndexError. Group nv.ord.* constructs degrees in both
//    orders to pin the interference, exactly as probes/geom/gpath/probe_nurbs.d and the
//    ns.ord.* group of probes/geom/surface/probe_nurbssurface.d do for the curve and
//    surface cases. The interference is asymmetric (a small volume reads a subset of a
//    big volume's buffers and is fine; the reverse throws), which is why both orders are
//    in the corpus and not just one.
//  * initialise() checks `nU != m_a+p+2` with three ints (D:134, signed) but
//    `V.length != m_b+q+2` and `W.length != m_c+r+2` against size_t (D:144, :154), so a
//    negative degree passes guard 2 and only the knot-count guards can catch it.
//    Group nv.neg.* builds volumes with p/q/r = -1 to show which guard fires, and then
//    evaluates one, which fails inside basisFuns.
//  * writeToFile (D:341-373) prints Pw[n]/Pw[3] un-weighted, and readFromFile (D:327-335)
//    multiplies by scale*Pw[3] again, so the round trip is not necessarily bit-exact.
//    Both texts are dumped so the corpus says what D actually does rather than what a
//    round trip should do.
//  * the writeAsVtkXml wrapper (D:375-378) defaults to 20 points per direction; the
//    probe calls it with small explicit counts and dumps the file body.
//
// The module has no classString override (grep '^classString' over gdtk
// src/geom/volume/*.d is empty, and geom.volume.parametricvolume declares none), so no
// row prints one.
//
// Build and capture (the reference tree is copied because ldc2 must not write to it):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip -I probes/geom/volume \
//     probes/geom/volume/probe_nurbsvolume.d -of=/tmp/probe_nvol -od/tmp/probe_obj_nurbsvol
//   /tmp/probe_nvol > baselines/e587b235/probes/geom_volume_nurbsvolume.txt
import std.algorithm;
import std.array;
import std.conv;
import std.file;
import std.format;
import std.math;
import std.stdio;
import std.string;

import geom.elements;
import geom.misc.nurbs_utils;
import geom.volume.nurbsvolume;
import nm.number;

string esc(string s)
{
    return s.replace("\n", "<newline>").replace("\r", "<cr>");
}

/// An evaluation row that survives a cross-instance interference throw.
void ev(string key, NURBSVolume v, double u, double w, double t)
{
    attempt(key, () { dump_v3(key, v.opCall(u, w, t)); });
}

void dump_v3(const char[] key, Vector3 v)
{
    writefln("%s x=%.17g y=%.17g z=%.17g", key, v.x, v.y, v.z);
}

/// One row for a call that may throw, with D's throwable class name and the escaped
/// message. Exception and Error are siblings, so this catches Throwable and prints
/// which one fired (type= is part of the observable).
void attempt(string key, void delegate() dg)
{
    try {
        dg();
    } catch (Throwable t) {
        writefln("%s thrown type=%s msg=%s", key, t.classinfo.name, esc(t.msg));
    }
}

/// A weighted net with no two components equal and no weight equal to 1, so no
/// component of an evaluation can vanish by symmetry and the w-divisions are real.
number[4][][][] pwnet(int nu, int nv, int nw)
{
    auto Pw = new number[4][][][](nu, nv, nw);
    foreach (i; 0 .. nu) {
        foreach (j; 0 .. nv) {
            foreach (k; 0 .. nw) {
                double x = 0.37*i + 0.11*i*i - 0.19*j + 0.07*k;
                double y = -0.23*j + 0.13*i*j + 0.29*k - 0.05*i;
                double z = 0.41*k + 0.09*i*k - 0.17*j*k + 0.03*i;
                double w = 1.0 + 0.2*i - 0.13*j + 0.07*k;
                Pw[i][j][k][0] = to!number(x*w);
                Pw[i][j][k][1] = to!number(y*w);
                Pw[i][j][k][2] = to!number(z*w);
                Pw[i][j][k][3] = to!number(w);
            }
        }
    }
    return Pw;
}

/// The solid-cylinder net copied verbatim from nurbsvolume.d's own unittest (D:482-531),
/// so the corpus carries the module's reference case.
number[4][][][] cylNet()
{
    double s2 = sqrt(2.0);
    number[4][][][] Pw = [
        [
            [
                [0.0, 0.0, -3.0, 1.0], [0.0, 0.0, -3.0, 1.0], [0.0, 0.0, -3.0, 1.0],
                [0.0, 0.0, -3.0, 1.0], [0.0, 0.0, -3.0, 1.0], [0.0, 0.0, -3.0, 1.0],
                [0.0, 0.0, -3.0, 1.0], [0.0, 0.0, -3.0, 1.0], [0.0, 0.0, -3.0, 1.0]
            ],
            [
                [0.0, 1.0, -3.0, 1.0], [1.0, 1.0, -3.0, 1.0 / s2], [1.0, 0.0, -3.0, 1.0],
                [1.0, -1.0, -3.0, 1.0 / s2], [0.0, -1.0, -3.0, 1.0], [-1.0, -1.0, -3.0, 1.0 / s2],
                [-1.0, 0.0, -3.0, 1.0], [-1.0, 1.0, -3.0, 1.0 / s2], [0.0, 1.0, -3.0, 1.0]
            ]
        ],
        [
            [
                [0.0, 0.0, -1.0, 1.0], [0.0, 0.0, -1.0, 1.0], [0.0, 0.0, -1.0, 1.0],
                [0.0, 0.0, -1.0, 1.0], [0.0, 0.0, -1.0, 1.0], [0.0, 0.0, -1.0, 1.0],
                [0.0, 0.0, -1.0, 1.0], [0.0, 0.0, -1.0, 1.0], [0.0, 0.0, -1.0, 1.0]
            ],
            [
                [0.0, 1.0, -1.0, 1.0], [1.0, 1.0, -1.0, 1.0 / s2], [1.0, 0.0, -1.0, 1.0],
                [1.0, -1.0, -1.0, 1.0 / s2], [0.0, -1.0, -1.0, 1.0], [-1.0, -1.0, -1.0, 1.0 / s2],
                [-1.0, 0.0, -1.0, 1.0], [-1.0, 1.0, -1.0, 1.0 / s2], [0.0, 1.0, -1.0, 1.0]
            ]
        ],
        [
            [
                [0.0, 0.0, 1.0, 1.0], [0.0, 0.0, 1.0, 1.0], [0.0, 0.0, 1.0, 1.0],
                [0.0, 0.0, 1.0, 1.0], [0.0, 0.0, 1.0, 1.0], [0.0, 0.0, 1.0, 1.0],
                [0.0, 0.0, 1.0, 1.0], [0.0, 0.0, 1.0, 1.0], [0.0, 0.0, 1.0, 1.0]
            ],
            [
                [0.0, 1.0, 1.0, 1.0], [1.0, 1.0, 1.0, 1.0 / s2], [1.0, 0.0, 1.0, 1.0],
                [1.0, -1.0, 1.0, 1.0 / s2], [0.0, -1.0, 1.0, 1.0], [-1.0, -1.0, 1.0, 1.0 / s2],
                [-1.0, 0.0, 1.0, 1.0], [-1.0, 1.0, 1.0, 1.0 / s2], [0.0, 1.0, 1.0, 1.0]
            ]
        ],
        [
            [
                [0.0, 0.0, 3.0, 1.0], [0.0, 0.0, 3.0, 1.0], [0.0, 0.0, 3.0, 1.0],
                [0.0, 0.0, 3.0, 1.0], [0.0, 0.0, 3.0, 1.0], [0.0, 0.0, 3.0, 1.0],
                [0.0, 0.0, 3.0, 1.0], [0.0, 0.0, 3.0, 1.0], [0.0, 0.0, 3.0, 1.0]
            ],
            [
                [0.0, 1.0, 3.0, 1.0], [1.0, 1.0, 3.0, 1.0 / s2], [1.0, 0.0, 3.0, 1.0],
                [1.0, -1.0, 3.0, 1.0 / s2], [0.0, -1.0, 3.0, 1.0], [-1.0, -1.0, 3.0, 1.0 / s2],
                [-1.0, 0.0, 3.0, 1.0], [-1.0, 1.0, 3.0, 1.0 / s2], [0.0, 1.0, 3.0, 1.0]
            ]
        ]
    ];
    return Pw;
}

// ---------------------------------------------------------------------------
// file generation. The bytes written here are dumped as corpus rows first, so if the
// C++ side ever generates different input bytes the divergence shows up before the
// reader is even exercised — and when the row matches, both sides have provably read
// the same file.
// ---------------------------------------------------------------------------
/// The control-net file readFromCntrlNetFile consumes: 3 dimension tokens then
/// 3*nPu*nPv*nPw coordinates in component-major (plot3d) order, which is also a legal
/// sgrid token stream. Written with %.18e so the text is the same on both sides.
void writeNetFile(string fileName, int nPu, int nPv, int nPw)
{
    auto f = File(fileName, "w");
    // one coordinate per line, which is what std.format's %.18e gives with writefln
    f.writefln("%d %d %d", nPu, nPv, nPw);
    foreach (comp; 0 .. 3) {
        foreach (k; 0 .. nPw) {
            foreach (j; 0 .. nPv) {
                foreach (i; 0 .. nPu) {
                    double v = i + 10.0*j + 100.0*k + 1000.0*comp;
                    f.writefln("%.18e", v);
                }
            }
        }
    }
    f.close();
}

/// A truncated net file: header says 3x2x2 but only part of the data is present, so
/// readFromCntrlNetFile indexes past the end of hullData.
void writeTruncNetFile(string fileName)
{
    auto f = File(fileName, "w");
    f.writefln("%d %d %d", 3, 2, 2);
    foreach (n; 0 .. 11) { f.writefln("%.18e", cast(double) n); }
    f.close();
}

string oneLine(string fileName)
{
    return (cast(string) read(fileName)).split("\n").join("<newline>");
}

void main()
{
    // ================= group 1: the module's own unittest case =====================
    // D:482-542. The cylinder, p=3 q=1 r=2 over Z/R/T.
    int p = 3;
    int q = 1;
    int r = 2;
    double[] Zk = [0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0];
    double[] Rk = [0.0, 0.0, 1.0, 1.0];
    double[] Tk = [0.0, 0.0, 0.0, 0.25, 0.25, 0.5, 0.5, 0.75, 0.75, 1.0, 1.0, 1.0];
    auto cyl = new NURBSVolume(cylNet(), Zk, p, Rk, q, Tk, r);
    writefln("nv.cyl str=%s", cyl.toString());
    // D:542 assert(approxEqualVectors(Vector3(0.5,0.0,1.5), nvol(0.75,0.5,0.25)))
    dump_v3("nv.cyl.unittest", cyl.opCall(0.75, 0.5, 0.25));
    // The same volume's knot-boundary structure: W has four spans, U and V one each.
    foreach (tt; [0.0, 0.125, 0.25, 0.375, 0.5, 0.75, 1.0]) {
        dump_v3(format("nv.cyl.w r=%s s=%s t=%s", to!string(0.75), to!string(0.5),
                       to!string(tt)), cyl.opCall(0.75, 0.5, tt));
    }
    foreach (uu; [0.0, 1.0]) {
        foreach (vv; [0.0, 1.0]) {
            dump_v3(format("nv.cyl.cor r=%s s=%s t=%s", to!string(uu), to!string(vv),
                           to!string(0.5)), cyl.opCall(uu, vv, 0.5));
        }
    }

    // ================= group 2: the 5x5x5 quadratic volume and the sweep ============
    int nu = 5;
    int deg = 2;
    double[] Uq = autoKnotVector(nu, deg);
    double[] Vq = autoKnotVector(nu, deg);
    double[] Wq = autoKnotVector(nu, deg);
    auto vol5 = new NURBSVolume(pwnet(nu, nu, nu), Uq, deg, Vq, deg, Wq, deg);
    writefln("nv.main str=%s", vol5.toString());
    writefln("nv.main dims nPu=%d nPv=%d nPw=%d nU=%d nV=%d nW=%d p=%d q=%d r=%d",
             vol5.nPu, vol5.nPv, vol5.nPw, vol5.nU, vol5.nV, vol5.nW, vol5.p, vol5.q, vol5.r);

    // The seven values justified in the header, read back out of the volume's own knot
    // vectors where they are knots (U[3] = 1/3, U[4] = 2/3) so both sides agree bitwise.
    double[] pts = [vol5.U[0], (vol5.U[0] + vol5.U[3]) / 2.0, vol5.U[3],
                    (vol5.U[3] + vol5.U[4]) / 2.0, vol5.U[4], vol5.U[0] + 0.999, 1.0];
    string[10] anames = ["p", "dVdr", "dVds", "dVdt", "d2Vdr2", "d2Vds2", "d2Vdt2",
                         "d2Vdrds", "d2Vdrdt", "d2Vdsdt"];
    foreach (rr; pts) {
        foreach (ss; pts) {
            foreach (tt; pts) {
                string tag = format(" r=%s s=%s t=%s", to!string(rr), to!string(ss),
                                    to!string(tt));
                dump_v3("nv.sweep." ~ anames[0] ~ tag, vol5.opCall(rr, ss, tt));
                dump_v3("nv.sweep." ~ anames[1] ~ tag, vol5.dVdr(rr, ss, tt));
                dump_v3("nv.sweep." ~ anames[2] ~ tag, vol5.dVds(rr, ss, tt));
                dump_v3("nv.sweep." ~ anames[3] ~ tag, vol5.dVdt(rr, ss, tt));
                dump_v3("nv.sweep." ~ anames[4] ~ tag, vol5.d2Vdr2(rr, ss, tt));
                dump_v3("nv.sweep." ~ anames[5] ~ tag, vol5.d2Vds2(rr, ss, tt));
                dump_v3("nv.sweep." ~ anames[6] ~ tag, vol5.d2Vdt2(rr, ss, tt));
                dump_v3("nv.sweep." ~ anames[7] ~ tag, vol5.d2Vdrds(rr, ss, tt));
                dump_v3("nv.sweep." ~ anames[8] ~ tag, vol5.d2Vdrdt(rr, ss, tt));
                dump_v3("nv.sweep." ~ anames[9] ~ tag, vol5.d2Vdsdt(rr, ss, tt));
            }
        }
    }

    // ================= group 3: unequal degrees, and dup()/copy ctor ================
    // p=1, q=2, r=3 over a 2x3x4 net: nU=2, nV=5, nW=8 knot vectors.
    double[] U1 = [0.0, 0.0, 1.0, 1.0];
    double[] V2 = autoKnotVector(3, 2);
    double[] W3 = autoKnotVector(4, 3);
    auto uneq = new NURBSVolume(pwnet(2, 3, 4), U1, 1, V2, 2, W3, 3);
    writefln("nv.uneq str=%s", uneq.toString());
    ev("nv.uneq.a", uneq, 0.0, 0.0, 0.0);
    ev("nv.uneq.b", uneq, 0.5, 0.5, 0.5);
    ev("nv.uneq.c", uneq, 1.0, 1.0, 1.0);
    ev("nv.uneq.d", uneq, 0.25, V2[2], W3[4]);

    auto dupd = uneq.dup();
    writefln("nv.uneq.dup str=%s", dupd.toString());
    ev("nv.uneq.dup.pt", dupd, 0.5, 0.5, 0.5);
    auto cp = new NURBSVolume(uneq);
    writefln("nv.uneq.copy str=%s", cp.toString());
    ev("nv.uneq.copy.pt", cp, 0.5, 0.5, 0.5);
    attempt("nv.uneq.copy.dVdr", () { dump_v3("nv.uneq.copy.dVdr", cp.dVdr(0.5, 0.5, 0.5)); });
    attempt("nv.uneq.copy.d2Vdsdt", () { dump_v3("nv.uneq.copy.d2Vdsdt", cp.d2Vdsdt(0.5, 0.5, 0.5)); });
    // The original still evaluates after the copy constructor has re-sized every static
    // buffer to the same degrees.
    ev("nv.uneq.afterCopy", uneq, 0.5, 0.5, 0.5);

    // ================= group 4: the seven guards, which type fires ==================
    void tryBuild(string key, number[4][][][] Pw, double[] U, int pp, double[] V, int qq,
                  double[] W, int rr) {
        attempt(key, () {
            auto v = new NURBSVolume(Pw, U, pp, V, qq, W, rr);
            writefln("%s built str=%s", key, v.toString());
        });
    }
    // Every knot vector here is an explicit literal, never autoKnotVector(N, p): that
    // helper has its own Error guard (nurbs_utils.d:103) and D does not fix the
    // evaluation order of arguments, so a call that is meant to reach a NURBSVolume
    // guard could instead report autoKnotVector's message.
    double[] K2 = [0.0, 0.5, 1.0];                     // 3 knots
    double[] K4 = [0.0, 0.0, 1.0, 1.0];                // 4 knots
    double[] K6 = [0.0, 0.0, 0.0, 1.0, 1.0, 1.0];      // 6 knots
    double[] K7 = [0.0, 0.0, 0.0, 0.5, 1.0, 1.0, 1.0]; // 7 knots
    double[] K9 = [0.0, 0.0, 0.0, 0.0, 1.0, 1.0, 1.0, 1.0, 1.0]; // 9 knots
    // guard 1 (D:94): fewer than two control points in u, then in v
    tryBuild("nv.g1.onetu", pwnet(1, 2, 2), K4, 0, K4, 0, K4, 0);
    tryBuild("nv.g1.onetv", pwnet(2, 1, 2), K4, 0, K4, 0, K4, 0);
    // D:93 reads Pw[0][0].length before guard 1 runs, so an empty middle direction is
    // an ArrayIndexError, not a guard message.
    tryBuild("nv.g0.emptyv", pwnet(2, 0, 0), K4, 0, K4, 0, K4, 0);
    // guard 2/3/4 (D:104, :114, :124): degree above control-point count minus one
    tryBuild("nv.g2.degp", pwnet(3, 3, 3), K9, 4, K6, 2, K6, 2);
    tryBuild("nv.g3.degq", pwnet(3, 3, 3), K6, 2, K9, 4, K6, 2);
    tryBuild("nv.g4.degr", pwnet(3, 3, 3), K6, 2, K6, 2, K9, 4);
    // guard 5/6/7 (D:134, :144, :154): knot-vector length mismatch, 3x3x3 net at p=q=r=2
    // requires 6 knots in each direction.
    tryBuild("nv.g5.lenU", pwnet(3, 3, 3), K4, 2, K6, 2, K6, 2);
    tryBuild("nv.g6.lenV", pwnet(3, 3, 3), K6, 2, K4, 2, K6, 2);
    tryBuild("nv.g7.lenW", pwnet(3, 3, 3), K6, 2, K6, 2, K4, 2);

    // ================= group 5: negative degrees =====================================
    // All three degree guards (D:104, :114, :124) compare int to int, so a negative
    // degree always passes them. Only the knot-count guards can catch it, and they
    // differ: D:134 compares the int nU against the int m_a+p+2 (signed), while D:144
    // and :154 compare V.length/W.length (size_t) against the int sum, which D converts
    // to size_t. On a 4x3x3 net (m_a=3, m_b=2, m_c=2):
    //   p=-1 -> required nU = 3-1+2 = 4, so a 4-knot U passes
    //   q=-1 -> required nV = 2-1+2 = 3
    //   r=-1 -> required nW = 2-1+2 = 3
    // each of which builds, sets a static buffer to length 0 or to p+1 = 0, and then
    // fails in findSpan at U[n+1] (one past a knot vector that is short by exactly the
    // degree deficit).
    tryBuild("nv.neg.p", pwnet(4, 3, 3), K4, -1, K6, 2, K6, 2);
    tryBuild("nv.neg.q", pwnet(4, 3, 3), K7, 2, K2, -1, K6, 2);
    tryBuild("nv.neg.r", pwnet(4, 3, 3), K7, 2, K6, 2, K2, -1);
    // p=-5 on the same net: m_a+p+2 = 0, so the signed guard asks for 0 knots and a
    // 3-knot vector fails it with the required count printed as a negative-side value.
    tryBuild("nv.neg.p5", pwnet(4, 3, 3), K2, -5, K6, 2, K6, 2);
    // Evaluate the three that build. m_a/m_b/m_c and the degrees are all intact, so
    // findSpan indexes U[n+1] on a vector one element too short.
    attempt("nv.neg.p.eval", () {
        auto v = new NURBSVolume(pwnet(4, 3, 3), K4, -1, K6, 2, K6, 2);
        dump_v3("nv.neg.p.eval", v.opCall(0.5, 0.5, 0.5));
    });
    attempt("nv.neg.q.eval", () {
        auto v = new NURBSVolume(pwnet(4, 3, 3), K7, 2, K2, -1, K6, 2);
        dump_v3("nv.neg.q.eval", v.opCall(0.5, 0.5, 0.5));
    });
    attempt("nv.neg.r.eval", () {
        auto v = new NURBSVolume(pwnet(4, 3, 3), K7, 2, K6, 2, K2, -1);
        dump_v3("nv.neg.r.eval", v.opCall(0.5, 0.5, 0.5));
    });

    // ================= group 6: the module-static buffers, both orders ==============
    // D:421-430 declares mSw, mS, temp1, temp2, mNu, mNv, mNw and the three
    // NURBSWorkspaces as *static* fields: one set per process, re-sized by whichever
    // constructor last ran (D:178-186, D:59-67). Big is p=q=r=3 over a 5x5x5 net (9
    // knots each), small is p=q=r=1 over a 2x2x2 net (4 knots each), so the buffers are
    // length 4 for big and length 2 for small. The interference is asymmetric, which is
    // why both orders are dumped: a small volume reads a subset of big's buffers and is
    // fine, while big writes mNu[3] and nws.left[2] into small's and raises
    // core.exception.ArrayIndexError from nurbs_utils.d:63.
    double[] U9 = autoKnotVector(5, 3);
    double[] U4 = autoKnotVector(2, 1);
    auto big = new NURBSVolume(pwnet(5, 5, 5), U9, 3, U9.dup(), 3, U9.dup(), 3);
    writefln("nv.ord.big str=%s", big.toString());
    ev("nv.ord.big.first", big, 0.5, 0.5, 0.5);
    attempt("nv.ord.big.dVdr", () { dump_v3("nv.ord.big.dVdr", big.dVdr(0.5, 0.5, 0.5)); });
    auto small = new NURBSVolume(pwnet(2, 2, 2), U4, 1, U4.dup(), 1, U4.dup(), 1);
    writefln("nv.ord.small str=%s", small.toString());
    ev("nv.ord.small.first", small, 0.5, 0.5, 0.5);
    // The small constructor has just re-sized mNu/mNv/mNw to 2 and replaced all three
    // workspaces, so big's next evaluation overruns them.
    ev("nv.ord.big.afterSmall", big, 0.5, 0.5, 0.5);
    ev("nv.ord.small.afterFailedBig", small, 0.5, 0.5, 0.5);
    // Rebuilding big fixes big, and small still works because it only touches
    // mNu[0..1], temp1[0..1][0..1] and temp2[0..1], a subset of the degree-3 buffers.
    auto bigAgain = new NURBSVolume(pwnet(5, 5, 5), U9, 3, U9.dup(), 3, U9.dup(), 3);
    ev("nv.ord.big.rebuilt", bigAgain, 0.5, 0.5, 0.5);
    ev("nv.ord.small.afterRebuild", small, 0.5, 0.5, 0.5);
    ev("nv.ord.oldbig.afterRebuild", big, 0.5, 0.5, 0.5);
    attempt("nv.ord.oldbig.d2Vdsdt", () { dump_v3("nv.ord.oldbig.d2Vdsdt", big.d2Vdsdt(0.5, 0.5, 0.5)); });
    // Only the w degree differs, so temp1's row length (r+1) is what runs off the end
    // while mNu/mNv are still large enough.
    double[] U6 = autoKnotVector(3, 2);
    double[] W10 = autoKnotVector(5, 4);
    auto wideW = new NURBSVolume(pwnet(3, 3, 5), U6, 2, U6.dup(), 2, W10, 4);
    writefln("nv.ord.wide str=%s", wideW.toString());
    ev("nv.ord.wide.first", wideW, 0.5, 0.5, 0.5);
    auto narrowW = new NURBSVolume(pwnet(3, 3, 3), U6, 2, U6.dup(), 2, U6.dup(), 2);
    ev("nv.ord.narrow.first", narrowW, 0.5, 0.5, 0.5);
    ev("nv.ord.wide.afterNarrow", wideW, 0.5, 0.5, 0.5);
    auto wideAgain = new NURBSVolume(pwnet(3, 3, 5), U6, 2, U6.dup(), 2, W10, 4);
    ev("nv.ord.wide.rebuilt", wideAgain, 0.5, 0.5, 0.5);
    ev("nv.ord.narrow.afterWideRebuilt", narrowW, 0.5, 0.5, 0.5);
    // Only the v degree differs, so temp1 itself (q+1 rows) overruns.
    auto wideV = new NURBSVolume(pwnet(3, 5, 3), U6, 2, W10, 4, U6.dup(), 2);
    writefln("nv.ord.wideV str=%s", wideV.toString());
    ev("nv.ord.wideV.first", wideV, 0.5, 0.5, 0.5);
    ev("nv.ord.narrow.afterWideV", narrowW, 0.5, 0.5, 0.5);
    // A degree-0 volume (legal: p=0 with nPu=2 needs 3 knots) needs only mNu[0], so it
    // survives whatever the previous constructor left behind.
    double[] K3 = [0.0, 0.0, 1.0];
    auto zeroP = new NURBSVolume(pwnet(2, 2, 2), K3, 0, K3, 0, K3, 0);
    writefln("nv.ord.zeroP str=%s", zeroP.toString());
    ev("nv.ord.zeroP", zeroP, 0.5, 0.5, 0.5);
    ev("nv.ord.zeroP.bnd", zeroP, 0.0, 1.0, 0.0);
    ev("nv.ord.wideV.afterZeroP", wideV, 0.5, 0.5, 0.5);
    ev("nv.ord.narrow.afterZeroP", narrowW, 0.5, 0.5, 0.5);

    // ================= group 7: the file readers/writers ============================
    // (a) writeToFile -> readFromFile round trip on the unequal-degree volume.
    string fwt = "/tmp/probe_nvol_wt.txt";
    attempt("nv.wt", () { uneq.writeToFile(fwt); });
    writefln("nv.wt.body=%s", oneLine(fwt));
    attempt("nv.rf", () {
        auto v = new NURBSVolume(fwt, 1.0);
        writefln("nv.rf str=%s", v.toString());
        dump_v3("nv.rf.a", v.opCall(0.5, 0.5, 0.5));
        dump_v3("nv.rf.b", v.opCall(0.0, 1.0, 0.0));
    });
    attempt("nv.rfs", () {
        auto v = new NURBSVolume(fwt, 2.5);
        writefln("nv.rfs str=%s", v.toString());
        dump_v3("nv.rfs.a", v.opCall(0.5, 0.5, 0.5));
    });
    remove(fwt);

    // (b) the control-net file, both format strings, from identical bytes.
    string fnet = "/tmp/probe_nvol_net.txt";
    writeNetFile(fnet, 3, 2, 2);
    writefln("nv.net.body=%s", oneLine(fnet));
    attempt("nv.net.sgrid", () {
        auto v = new NURBSVolume(fnet, "sgrid", [2, 1, 1], 1.0);
        writefln("nv.net.sgrid str=%s", v.toString());
        dump_v3("nv.net.sgrid.a", v.opCall(0.5, 0.5, 0.5));
        dump_v3("nv.net.sgrid.b", v.opCall(0.0, 1.0, 0.5));
    });
    attempt("nv.net.plot3d", () {
        auto v = new NURBSVolume(fnet, "plot3d-sgrid", [2, 1, 1], 1.0);
        writefln("nv.net.plot3d str=%s", v.toString());
        dump_v3("nv.net.plot3d.a", v.opCall(0.5, 0.5, 0.5));
        dump_v3("nv.net.plot3d.b", v.opCall(0.0, 1.0, 0.5));
    });
    attempt("nv.net.sgrid.scale", () {
        auto v = new NURBSVolume(fnet, "sgrid", [2, 1, 1], 2.5);
        writefln("nv.net.sgrid.scale str=%s", v.toString());
        dump_v3("nv.net.sgrid.scale.a", v.opCall(0.5, 0.5, 0.5));
    });
    attempt("nv.net.badfmt", () {
        auto v = new NURBSVolume(fnet, "igrid", [2, 1, 1], 1.0);
        writefln("nv.net.badfmt str=%s", v.toString());
    });
    remove(fnet);
    string ftr = "/tmp/probe_nvol_trunc.txt";
    writeTruncNetFile(ftr);
    writefln("nv.trunc.body=%s", oneLine(ftr));
    attempt("nv.net.truncated", () {
        auto v = new NURBSVolume(ftr, "sgrid", [2, 1, 1], 1.0);
        writefln("nv.net.truncated str=%s", v.toString());
    });
    remove(ftr);

    // (c) a missing file for both readers.
    attempt("nv.missing.r", () {
        auto v = new NURBSVolume("/tmp/probe_nvol_absent_volume.txt", 1.0);
        writefln("nv.missing.r str=%s", v.toString());
    });
    attempt("nv.missing.net", () {
        auto v = new NURBSVolume("/tmp/probe_nvol_absent_net.txt", "sgrid", [2, 1, 1], 1.0);
        writefln("nv.missing.net str=%s", v.toString());
    });

    // (d) the two VTK writers, dumped as file bytes.
    string fvtk = "/tmp/probe_nvol_cntrl.vtk";
    attempt("nv.cntrl", () { uneq.writeCntrlNetAsVtkXml(fvtk); });
    writefln("nv.cntrl.len=%d", (cast(string) read(fvtk)).length);
    writefln("nv.cntrl.body=%s", oneLine(fvtk));
    remove(fvtk);
    string fvol = "/tmp/probe_nvol_vtk.vtk";
    attempt("nv.vtk", () { uneq.writeAsVtkXml(fvol, 2, 2, 3); });
    writefln("nv.vtk.len=%d", (cast(string) read(fvol)).length);
    writefln("nv.vtk.body=%s", oneLine(fvol));
    remove(fvol);

    // ================= group 8: leave the statics in a known state ===================
    auto finalV = new NURBSVolume(pwnet(5, 5, 5), U9, 3, U9.dup(), 3, U9.dup(), 3);
    ev("nv.final", finalV, 0.5, 0.5, 0.5);
    attempt("nv.final.d2Vdrdt", () { dump_v3("nv.final.d2Vdrdt", finalV.d2Vdrdt(0.5, 0.5, 0.5)); });
}
