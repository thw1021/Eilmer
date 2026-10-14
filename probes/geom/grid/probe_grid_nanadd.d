// probes/geom/grid/probe_grid_nanadd.d — what geom/grid/grid.d:231-241
// cell_barycentre does to a NaN, measured against ldc2 1.40.0 at gdtk e587b235.
//
// This is the T-919 localiser, made permanent. geom_grid_grid row 178
// (`cbc.nan2d x=-nan') was byte-identical in Debug and flipped to `x=nan' in the
// port's Release build, and the reason is that IEEE 754-2008 6.3 leaves the result
// of an operation on TWO NaN operands unspecified: x86-64 SSE answers "whichever
// operand sits in the add's destination register", and which one that is is register
// allocation. Both compilers therefore change their answer with -O. Nothing here is
// a value-level rule; it is a register-role rule, and it is why the port has to
// write the choice out (eil::add_rhs_nan / eil::mul_rhs_nan in substrate/include/eil/
// math.hpp) rather than inherit it by accident.
//
// Build and capture (reproducible; the gdtk tree itself is read-only):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   rm -rf /tmp/gdtk_shadow && cp -a /home/tang/packages/gdtk/src /tmp/gdtk_shadow
//   grep -rl "@nogc" --include=*.d /tmp/gdtk_shadow | xargs sed -i 's/@nogc//g'
//   ldc2 -i -I/tmp/gdtk_shadow -I/tmp/gdtk_shadow/extern/gzip \
//     probes/geom/grid/probe_grid_nanadd.d -of=/tmp/probe_g1nan -od/tmp/probe_obj_g1nan
//   /tmp/probe_g1nan > baselines/e587b235/probes/geom_grid_nanadd.txt
// NO -O flag: every baseline in this repository is an ldc2 -O0 capture, and that is
// the level whose choice this probe records. Run the same source at -O1/-O2/-O3 and
// the `add.*'/`scale.*' rows move (measured, and reported under T-919) — which is the
// whole point of pinning it in the port.
//
// Row groups:
//   nan.init   polarity of the NaN the reference actually uses: double.nan, its
//              negation, double.init and Vector3.init. (quiet_NaN() on this machine is
//              0x7ff8000000000000, sign bit CLEAR; the substrate comment that claimed
//              0xfff8... is stale. Both languages agree.)
//   nan.walk   grid.d:231-241's own statement order — `Vector3 cbc; cbc.set(v0);
//              foreach(1..nvtx) cbc.add(vi); cbc.scale(1.0/nvtx)' — printed after
//              EVERY step, for the nan2d fixture of probe_grid.d:502-510 (the row-178
//              cell) and for variants that put the two NaN signs in the other order,
//              in a later slot, or in y/z. nvtx is taken from the cell, including the
//              nvtx=0 cell where 1.0/nvtx is +inf.
//   nan.<op>   the 7x7 = 49-combination operand matrix (two quiet NaNs of opposite
//              sign, two payload NaNs, two signalling NaNs, 1.0, +inf) for each of
//              add(ref), add(ptr), add(other,factor), operator+=, operator+, operator-=,
//              scale(f), operator*=, operator/=. add/ref/ptr/op+= pick the ADDEND's NaN
//              at -O0; scale/op*= pick the FACTOR's; operator+ picks the RECEIVER's;
//              -= and /= are not commutative in the register sense and never move.
//
// Run the mirror: ./test_geom_grid_nanadd <baseline.txt>
module probe_grid_nanadd;

import std.format, std.stdio;

import geom.elements.vector3;

ulong bits(double d) { return *cast(ulong*)&d; }
double frombits(ulong u) { return *cast(double*)&u; }

// 7 patterns, in this order, for every matrix section.
const ulong[] PAT = [
    0x7ff8000000000000UL,  // +qNaN           (== double.nan)
    0xfff8000000000000UL,  // -qNaN           (== -double.nan)
    0x7ff8000000000001UL,  // +qNaN, payload 1
    0x7ff0000000000001UL,  // +sNaN, payload 1
    0xfff0000000000002UL,  // -sNaN, payload 2
    0x3ff0000000000000UL,  // 1.0
    0x7ff0000000000000UL,  // +inf
];

void show(const char[] tag, Vector3 v)
{
    writefln("%s x=%016x y=%016x z=%016x", tag, bits(v.x), bits(v.y), bits(v.z));
}

// grid.d:231-241 verbatim, one print per step.
Vector3 walk(const char[] tag, Vector3[] verts, size_t nvtx)
{
    Vector3 cbc;
    show(format("%s step0.init", tag), cbc);
    cbc.set(verts[0]);
    show(format("%s step1.set", tag), cbc);
    foreach (i; 1 .. nvtx) {
        cbc.add(verts[i]);
        show(format("%s step2.add%d", tag, i), cbc);
    }
    cbc.scale(1.0/nvtx);
    show(format("%s step3.scale", tag), cbc);
    return cbc;
}

void main()
{
    writefln("nan.init double.nan %016x", bits(double.nan));
    writefln("nan.init -double.nan %016x", bits(-double.nan));
    writefln("nan.init double.init %016x", bits(double.init));
    Vector3 vi;
    writefln("nan.init Vector3.init %016x %016x %016x", bits(vi.x), bits(vi.y), bits(vi.z));
    Vector3 vc = Vector3(double.nan, 0.0, 0.0);
    writefln("nan.init Vector3(nan,0,0) %016x", bits(vc.x));
    writefln("nan.init 1.0/0 %016x 1.0/3 %016x", bits(1.0/0), bits(1.0/3.0));
    writefln("nan.init text %.17g %.17g %.17g", double.nan, -double.nan, 0.0/0.0);

    //-------- the row-178 fixture (probe_grid.d:502-510) and its sign variants -----
    auto vNaN = Vector3(double.nan, 0.0, 0.0);
    auto vNeg = Vector3(-double.nan, 1.0, 0.0);
    auto vOne = Vector3(1.0, 0.0, 0.0);
    auto vNaNy = vNaN.dup; vNaNy.y = double.nan;
    auto vNegy = vNeg.dup; vNegy.y = -double.nan;
    walk("nan.walk nan2d", [vNaN, vNeg, vOne], 3);
    walk("nan.walk nan2d swapped", [vNeg, vNaN, vOne], 3);
    walk("nan.walk same sign", [vNaN, Vector3(double.nan, 1.0, 0.0), vOne], 3);
    walk("nan.walk nan in slot2", [vOne, vNaN, vNeg], 3);
    walk("nan.walk two nan", [vNaN, vNeg, Vector3(-double.nan, 0.0, 1.0)], 3);
    walk("nan.walk nan y", [vNaNy, vNegy], 2);
    walk("nan.walk nvtx1", [vNaN], 1);
    walk("nan.walk nvtx0 inf", [vNaN], 0);
    walk("nan.walk 0 times inf", [Vector3(0.0, 1.0, 0.0)], 0);
    walk("nan.walk nan2d x only", [vNaN, Vector3(double.nan, 2.0, 0.0),
                                   Vector3(double.nan, 3.0, 0.0)], 3);

    //-------- the 49-case operand matrix, one section per operation ---------------
    foreach (a; PAT) foreach (b; PAT) {
        auto A = Vector3(frombits(a), 0.0, 0.0);
        auto B = Vector3(frombits(b), 0.0, 0.0);
        writefln("nan.pair %016x %016x", a, b);

        auto v = A.dup; v.add(B);      writefln("nan.add.ref %016x", bits(v.x));
        Vector3* bp = &B;
        auto w = A.dup; w.add(bp);     writefln("nan.add.ptr %016x", bits(w.x));
        auto e = A.dup; e.add(B, 2.0); writefln("nan.add.fac %016x", bits(e.x));
        auto p = A.dup; p += B;        writefln("nan.opplus_eq %016x", bits(p.x));
        auto q = A + B;                writefln("nan.opplus %016x", bits(q.x));
        auto s = A - B;                writefln("nan.opminus %016x", bits(s.x));
        auto m = A.dup; m -= B;        writefln("nan.opminus_eq %016x", bits(m.x));
        auto f = A.dup; f.scale(frombits(b)); writefln("nan.scale %016x", bits(f.x));
        auto g = A.dup; g *= frombits(b);     writefln("nan.opmul_eq %016x", bits(g.x));
        auto h = A.dup; h /= frombits(b);     writefln("nan.opdiv_eq %016x", bits(h.x));
        auto d = A / frombits(b);             writefln("nan.opdiv %016x", bits(d.x));
    }
}
