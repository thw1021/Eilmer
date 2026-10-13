// probes/extern/gzip/probe_gzip.d — pin what ldc2's extern/gzip/gzip.d does.
//
// Why this probe exists: geom/grid/{sgrid,usgrid}.d put read_from_gzip_file() and
// write_to_gzip_file() on top of these three classes, so the decompressed
// *payload* has to match byte for byte. The gzip container is NOT comparable
// against D: the header carries an MTIME field that Phobos fills from the clock,
// and the deflate stream length depends on the zlib build linked into the
// reference. So stdout (which is the gate) carries payload observables only;
// container sizes and hex go to stderr for the offline comparison, and the
// D-written container is committed under tests/test_data/ so the C++ test can
// decompress a file D itself made.
//
// Phobos' std.zlib is part of what is being measured: UnCompress is constructed
// with the default HeaderFormat.determineFromData (gzip.d:60), i.e.
// inflateInit2(windowBits = 15+32) with destbufsize = 2*chunk on the first call,
// and Compress with HeaderFormat.gzip (gzip.d:132), i.e.
// deflateInit2(Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15+16, memLevel 8,
// Z_DEFAULT_STRATEGY), with compress() returning null for an empty buffer and
// flush() returning null when nothing was ever compressed.
//
// Build and capture (argv[1] = scratch directory, argv[2] = payload text file):
//   mkdir -p /tmp/gzprobe && ldc2 -O0 -i \
//     -I/home/tang/packages/gdtk/src/extern/gzip probes/extern/gzip/probe_gzip.d \
//     -of=/tmp/gzip_d && /tmp/gzip_d /tmp/gzprobe tests/test_data/gzip_payload.txt \
//     > baselines/e587b235/probes/extern_gzip_gzip.txt
// The capture step then copies the D-written container into the reference tree
// copy of the payload:
//   cp /tmp/gzprobe/payload_dgz.gz tests/test_data/gzip_payload.dgz.gz
//   cp /tmp/gzprobe/zlib_stream.gz tests/test_data/gzip_payload.zraw
// Both committed files are containers D itself wrote, so the C++ reader faces
// byte-identical input, and the truncated read below (first half of the .gz) is
// comparable too.
// Replay check: ctest -R extern_gzip_gzip
import std.stdio, std.file, std.path, std.conv, std.format, std.zlib;
import std.algorithm.comparison : min;
import gzip;

string wd;

// Payload observables for a small file: every line, verbatim.
void show_lines(string label, string path) {
    int i = 0;
    size_t total = 0;
    try {
        auto byLine = new GzipByLine(path);
        foreach (line; byLine) {
            writefln("%s line %d len=%d|%s", label, i, line.length, line);
            total += line.length;
            ++i;
        }
    } catch (Exception e) {
        writefln("%s threw Exception msg=%s", label, e.msg);
        return;
    }
    writefln("%s nlines=%d chars=%d", label, i, total);
}

// Payload observables for a big file: count, total bytes, and a crc32 over the
// concatenated lines, so a reordered or truncated read shows up immediately.
void summarise(string label, string path) {
    int i = 0;
    size_t total = 0;
    uint crc = 0u;
    size_t longest = 0;
    try {
        auto byLine = new GzipByLine(path);
        foreach (line; byLine) {
            crc = crc32(crc, line);
            total += line.length;
            if (line.length > longest) longest = line.length;
            ++i;
        }
    } catch (Exception e) {
        writefln("%s threw Exception msg=%s", label, e.msg);
        return;
    }
    writefln("%s nlines=%d chars=%d longest=%d crc=%x", label, i, total, longest, crc);
}

// Writes the parts through GzipOut, then logs the container size and first bytes
// to stderr (never to stdout: the container is not comparable, see the header).
void write_payload(string path, string[] parts) {
    auto f = new GzipOut(path);
    foreach (p; parts) f.compress(p);
    f.finish();
    auto b = cast(ubyte[])read(path);
    string hex;
    foreach (x; b[0 .. min(b.length, 24)]) hex ~= format("%02x", x);
    stderr.writefln("CONTAINER %s bytes=%d head=%s", path, b.length, hex);
}

void raw_range_walk(string label, string path, int nchars) {
    // GzipInputRange used directly: char-by-char through front/popFront, which is
    // what crosses the 0x4000 chunk boundary in gzip.d:64-80.
    auto r = new GzipInputRange(path);
    size_t count = 0;
    int digest = 0;
    string head;
    while (!r.empty) {
        char ch = r.front;
        if (cast(int)count < nchars) head ~= ch;
        digest = (digest + (cast(int)count + 1) * (cast(int)ch)) % 1000003;
        ++count;
        r.popFront();
    }
    writefln("%s chars=%d digest=%d head=%s", label, count, digest, head);
}

void main(string[] args) {
    wd = args[1];
    string payload_path = args[2];
    string payload = cast(string)read(payload_path);
    writefln("payload file=%s len=%d", baseName(payload_path), payload.length);

    // ---- 1. round trip of the committed payload, written in three chunks ----
    // Successive Compress.compress() calls must concatenate into one stream
    // (std/zlib.d keeps the deflate state between calls; sgrid.d's
    // write_to_gzip_file() relies on that by calling compress() per cell row).
    string rt = buildPath(wd, "payload_dgz.gz");
    size_t third = payload.length / 3;
    write_payload(rt, [payload[0 .. third], payload[third .. 2*third],
                       payload[2*third .. $]]);
    show_lines("roundtrip", rt);

    // ---- 2. the whole payload in one compress() call -----------------------
    string one = buildPath(wd, "payload_one.gz");
    write_payload(one, [payload]);
    show_lines("one_shot", one);

    // ---- 3. empty payload --------------------------------------------------
    // compress("") returns null and flush() with nothing inited returns null too,
    // so D writes a zero-byte file rather than a valid gzip stream.
    string emptyf = buildPath(wd, "empty.gz");
    write_payload(emptyf, [""]);
    show_lines("empty_readback", emptyf);

    // ---- 4. no trailing newline / trailing newline / CRLF / blank line -----
    string nonl = buildPath(wd, "no_newline.gz");
    write_payload(nonl, ["alpha\nbravo\ncharlie"]);
    show_lines("no_trailing_newline", nonl);

    string withnl = buildPath(wd, "with_newline.gz");
    write_payload(withnl, ["alpha\nbravo\ncharlie\n"]);
    show_lines("trailing_newline", withnl);

    string crlf = buildPath(wd, "crlf.gz");
    write_payload(crlf, ["one\r\ntwo\r\n"]);
    show_lines("crlf", crlf);

    // A blank line mid-stream: GzipByLine.empty() is `buf.length == 0`
    // (gzip.d:108-110), so iteration stops at the blank line and the rest of the
    // file is never yielded. Preserved, and pinned here.
    string blank = buildPath(wd, "blank_line.gz");
    write_payload(blank, ["first\n\nsecond\nthird\n"]);
    show_lines("blank_line", blank);

    // ---- 5. many chunks: 60000 bytes of text over 1000 lines --------------
    // The file is read in 0x4000-byte chunks, so load() runs 4+ times.
    string big = buildPath(wd, "big.gz");
    char[] bigtext;
    foreach (i; 0 .. 60000) bigtext ~= (i % 60 == 59) ? '\n' : cast(char)('a' + (i % 26));
    write_payload(big, [bigtext.idup]);
    summarise("big", big);
    raw_range_walk("big_raw_range", big, 12);

    // ---- 6. one very compressible run: 100000 identical bytes -------------
    // The deflate stream is far smaller than one 0x4000 file chunk, so a single
    // uncompress() call has to produce 100000 characters (Phobos grows destbuf
    // inside its inflate loop).
    string huge = buildPath(wd, "huge.gz");
    char[] hugetext;
    hugetext.length = 100000;
    foreach (ref ch; hugetext) ch = 'Z';
    write_payload(huge, [hugetext.idup]);
    summarise("huge", huge);
    raw_range_walk("huge_raw_range", huge, 8);

    // ---- 7. raw range walk across a chunk boundary -------------------------
    raw_range_walk("roundtrip_raw_range", rt, 24);

    // ---- 8. truncated container: silent short read ------------------------
    string trunc = buildPath(wd, "truncated.gz");
    auto all = cast(ubyte[])read(rt);  // == tests/test_data/gzip_payload.dgz.gz
    auto half = all[0 .. all.length / 2];
    File ftr = File(trunc, "wb");
    ftr.rawWrite(half);
    ftr.close();
    show_lines("truncated", trunc);

    // ---- 9. corrupt container: Phobos raises ZlibException ---------------
    string junk = buildPath(wd, "junk.gz");
    ubyte[1024] invalidData;   // all zeros, so inflate() returns Z_DATA_ERROR
    File fj = File(junk, "wb");
    fj.rawWrite(invalidData[]);
    fj.close();
    show_lines("corrupt", junk);

    // ---- 10. a zlib (non-gzip) container read through the same range -----
    // HeaderFormat.determineFromData has to detect it, which is what
    // inflateInit2(15+32) buys.
    string zlibf = buildPath(wd, "zlib_stream.gz");
    File fz = File(zlibf, "wb");
    fz.rawWrite(std.zlib.compress(cast(const(void)[])payload));
    fz.close();
    show_lines("zlib_autodetect", zlibf);

    // ---- 11. a path that does not exist ----------------------------------
    // Deliberately a bare relative name: Phobos echoes the name it was given
    // into the exception message, so the line stays reproducible from any
    // working directory (the C++ test replays it from its own scratch cwd).
    show_lines("missing_file", "no_such_file.gz");
}
