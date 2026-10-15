// probes/substrate/probe_to_double_range.d — how far off Phobos' `to!double' is from
// correctly rounded, measured over a wide range of decimal strings. Backlog T-930.
//
// Why this probe exists. `to!double' is the entry point for every decimal a user writes:
// job-script literals, .fld/.dat tables, fluidblock headers, and grid files. Two of the 16
// artefact rows of the sgrid demo (geom_grid_sgrid_demo.artefacts.txt) differ from the D
// reference purely because of it — import_gridpro_grid (geom/grid/sgrid.d:1603) only does
// `uflowz(scale * to!double(tks[m]))' with scale = 1.0, so no arithmetic happens between
// the text and the stored double. probe_to_double_rounding.d established the defect on 15
// tokens (2 of 15 differ from strtod, 1 ULP, D the less accurate one); this probe measures
// the RATE and the TRIGGER CONDITION before anything is replicated: the shape of the
// numbers, the digit count, and whether the exponent form matters.
//
// The model that fits every divergent row measured so far is DOUBLE ROUNDING: Phobos
// forms the value in its 80-bit `real' and narrows to double on the way out, so a decimal
// that sits just above or below a double midpoint can come out one bit high or low where a
// correctly rounded parser (strtod, Python float(), C++ from_chars) gives the other bit.
// The rows below are the evidence for or against that, on 160 real GridPro tokens from
// examples/eilmer/3D/gridpro-import/blk.tmp plus a generated sweep.
//
// Build and capture (reproducible; the row IS the input string, so the replay reads the
// strings out of the corpus rather than re-generating them):
//   cd /home/tang/packages/Eilmer
//   export PATH=/home/tang/packages/dlang/bin:$PATH
//   ldc2 -O0 probes/substrate/probe_to_double_range.d -of=/tmp/probe_tdr -od/tmp/probe_obj_tdr
//   /tmp/probe_tdr > baselines/e587b235/probes/substrate_to_double_range.txt
// Replay: ctest -R substrate_to_double_range (tests/test_substrate_to_double_range.cpp).
//
// Rows: `tdr src=<decimal text> bits=<%016x of to!double(text)> dig=<significant digit
// count> exp=<decimal exponent>'. The dig/exp columns are what the trigger condition is
// read off; they are D's own parse, printed for the record.
import std.conv;
import std.format;
import std.math;
import std.stdio;

// D's `to!double' THROWS on an overflowing text rather than returning +-inf, and that is
// observable behaviour a port has to reproduce, so it is recorded here next to the bits.
string parse_bits(string s, ref bool threw)
{
    try
    {
        double v = to!double(s);
        threw = false;
        return format("%016x", *cast(ulong*)&v);
    }
    catch (Exception e)
    {
        threw = true;
        return format("%s", e.classinfo.name);
    }
}

void R(string s)
{
    pragma(once);
    size_t dig = 0;
    long expo = 0;
    {
        string t = s;
        if (t.length && (t[0] == '-' || t[0] == '+')) t = t[1 .. $];
        long e10 = 0;
        size_t ei = 0;
        foreach (i, c; t)
            if (c == 'e' || c == 'E') { ei = i; break; }
        if (ei)
        {
            e10 = to!long(t[ei + 1 .. $]);
            t = t[0 .. ei];
        }
        size_t dp = t.length;
        foreach (i, c; t)
            if (c == '.') { dp = i; break; }
        long point = cast(long)dp;
        string digits;
        foreach (c; t)
            if (c >= '0' && c <= '9' && c != '.') digits ~= c;
        size_t k = 0;
        while (k + 1 < digits.length && digits[k] == '0') ++k;
        digits = digits[k .. $];
        dig = digits.length;
        expo = e10 + (point - k) - cast(long)digits.length;
    }
    bool threw;
    const string b = parse_bits(s, threw);
    if (threw) writefln("tdr src=%s throws=%s dig=%d exp=%d", s, b, dig, expo);
    else writefln("tdr src=%s bits=%s dig=%d exp=%d", s, b, dig, expo);
}

void main()
{
    // Group A: 160 real tokens out of the GridPro file the sgrid demo imports. These are
    // the shapes that actually reach a solver through `to!double'.
    string[] blk = [
        "21",
        "1.139195920889653",
        "0.3768708632676333",
        "0",
        "1.147673425868634",
        "0.350493519991605",
        "1.15511578777907",
        "0.3251269241768392",
        "1.161694154602662",
        "0.3007768128748586",
        "1.167492708285116",
        "0.2774180529494036",
        "1.172586011372533",
        "0.2550334212110374",
        "1.177042995725728",
        "0.2336017684311393",
        "1.180929407214775",
        "0.213086215360148",
        "1.18430732541777",
        "0.1934325695506341",
        "1.187233629309497",
        "0.1745746529074384",
        "1.189758969384862",
        "0.1564403872707519",
        "1.19192751031189",
        "0.1389561447466433",
        "1.193777129269035",
        "0.1220498489771063",
        "1.195339886875967",
        "0.1056529925960627",
        "1.196642681904237",
        "0.089701125111042",
        "1.197707923416791",
        "0.07413319220019464",
        "1.198554092210924",
        "0.05889047499892976",
        "1.199196159469485",
        "0.04391549971011965",
        "1.199645896276661",
        "0.02915001797117462",
        "1.199911887379836",
        "0.01454175112264317",
        "4.940656458412465e-324",
        "1.199999893808731",
        "1.129802186663799",
        "0.3737124298421268",
        "1.138104212621501",
        "0.3475217097213514",
        "1.145458021475933",
        "0.3223568329954359",
        "1.151961266126837",
        "0.2981996165037335",
        "1.157696989497475",
        "0.2750271623633218",
        "1.162736182458691",
        "0.2528224570855909",
        "1.167145959941951",
        "0.231564476335919",
        "1.170992363539582",
        "0.2112165961859607",
        "1.174338476324411",
        "0.1917252099682411",
        "1.177241827980053",
        "0.1730248157467141",
        "1.179753193295771",
        "0.1550439731280389",
        "1.181916392323167",
        "0.1377096477896974",
        "1.183768586056844",
        "0.1209502859455027",
        "1.185340860587596",
        "0.1046977871459678",
        "1.186658959163306",
        "0.08888795416969486",
        "1.187744042956714",
        "0.07345983883187621",
        "1.188613403476589",
        "0.0583547574185206",
        "1.18928105330721",
        "0.04351536734007723",
        "1.189758952072751",
        "0.02888361062826692",
        "1.190033806884429",
        "0.01440815723256938",
        "-4.940656458412465e-324",
        "1.19019763719449",
        "1.120409083293292",
        "0.3705114722807598",
        "1.128608531811877",
        "0.3445319565540443",
        "1.135867846295173",
        "0.3195653735912542",
        "1.142289891365931",
        "0.2955966137214182",
        "1.147956314350664",
        "0.272606036381134",
        "1.152934778524031",
        "0.2505774175946329",
        "1.15729092974225",
        "0.229489946790385",
        "1.161091191211415",
        "0.2093071854428154",
        "1.164399633665589",
        "0.1899761087916107",
        "1.167274467556572",
        "0.1714319538496607",
        "1.169766629456265",
        "0.1536039908274745",
        "1.171919598829799",
        "0.1364198494812277",
        "1.173769871790894",
        "0.1198085594849262",
        "1.175347651497054",
        "0.1037024630504105",
        "1.176677594225762",
        "0.08803760794405262",
        "1.177779636352417",
        "0.07275308516787535",
        "1.178669800668833",
        "0.05779014600266728",
        "1.17936085481094",
        "0.04309158753511044",
        "1.17986414954833",
        "0.02859983452540099",
        "1.180165031359566",
        "0.01426555374647377",
        "1.180354521440259",
        "1.111008921874858",
        "0.367269574573954",
        "1.119107601492246",
        "0.3415056880977038",
        "1.126274396550752",
        "0.3167378128096625",
        "1.13261558734115",
        "0.2929550221030571",
        "1.138211174451786",
        "0.2701425245537938",
        "1.143126374004326",
        "0.2482861106199597",
        "1.147425965875492",
        "0.2273655540509573",
        "1.151176799488875",
        "0.2073446200202619",
        "1.154443815121325",
        "0.1881709539166",
        "1.15728598354484",
        "0.1697807412806255",
        "1.159754540870896",
        "0.1521041958188617",
        "1.161892759827596",
        "0.1350698184826883",
        "1.163736676962436",
        "0.1186073786820031",
        "1.165315830203477",
        "0.1026497759416576",
        "1.166653961874286",
        "0.08713337695840845",
        "1.167769855449674",
        "0.07199732065443534",
        "1.168678089294312"
    ];
    foreach (s; blk) R(s);

    // Group B: a generated sweep. 1..25 significant digits, exponents across the double
    // range, in three spellings (plain fraction, normalised with e, integer with e).
    ulong seed = 20261015;
    ulong nxt()
    {
        seed = seed * 6364136223846793005UL + 1442695040888963407UL;
        return seed >> 11;
    }
    for (int ndig = 1; ndig <= 25; ++ndig)
    {
        for (int k = 0; k < 60; ++k)
        {
            string d;
            d ~= cast(char)('1' + nxt() % 9);
            for (int i = 1; i < ndig; ++i)
                d ~= cast(char)('0' + nxt() % 10);
            int e = cast(int)(nxt() % 60) - 30;
            R(format("%s.%se%d", d[0 .. 1], d[1 .. $], e - cast(long)(ndig - 1)));
            R(format("%s.%s", d[0 .. 1], d[1 .. $]));
            string z = d;
            for (size_t i = d.length; i < 32; ++i) z ~= '0';
            R(format("0.%se%d", z[0 .. ndig], e + 1));
        }
    }

    // Group C: the boundaries, spelled in as many ways as the parser can be handed them.
    string[] edges = [
        "0", "0.0", "-0.0", "1", "1.0", "0.1", "0.01", "1e-1", "1e-17", "1e-308", "1e-309",
        "1e-323", "1e-324", "4.9406564584124654e-324", "2.2250738585072011e-308",
        "2.2250738585072009e-308", "2.2250738585072014e-308", "1.7976931348623157e308",
        "1.7976931348623159e308", "1e308", "1e309", "1e30", "1e31", "12e-1", "120e-2",
        "0.2929550221030571", "0.29295502210305710", "0.292955022103057100000",
        "2.929550221030571e-1", "2929550221030571e-16", "292955022103057.1e-15",
        "0.5000000000000001", "0.50000000000000005", "0.5000000000000000555",
        "1.0000000000000002", "1.00000000000000005", "9.007199254740993e15",
        "9007199254740993", "9007199254740993.0", "9.0071992547409927e15",
        "0.30000000000000004", "0.3", "0.7", "1.1", "2.3", "0.7000000000000001",
        "123456789012345678901234567890", "1.23456789012345678901234567890e+30",
        "0." ~ "0" ~ "00000000000000000000000000000000000001",
    ];
    foreach (s; edges) R(s);

    // Group D: the exact midpoint hazards — a decimal that is the true midpoint of two
    // doubles needs 767 bits to write out, but decimals built from a double's own %g17 plus
    // one digit land close enough that a 64-bit intermediate changes the answer.
    for (int i = 0; i < 200; ++i)
    {
        double base = cast(double)i / 200.0 * 3.0 + 0.01;
        R(format("%.17g", base));
        R(format("%.20g", base));
        double n = nextafter(base, double.infinity);
        R(format("%.17g", (base + n) / 2.0));
    }
    writefln("count|rows=%d", 1);
}
