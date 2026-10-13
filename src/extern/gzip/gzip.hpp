#pragma once
// extern/gzip/gzip.d -> src/extern/gzip/gzip.hpp (module gzip).
//
// Python-inspired handling of gzip files, as Stephan Schiffels wrote it for the
// D forum in 2014 and as cfcfd3 shipped it on 2014-07-21. Three classes:
// GzipInputRange (a char range over a decompressed file), GzipByLine (the same
// file as lines) and GzipOut (a gzip writer). geom/grid/{sgrid,usgrid}.d build
// read_from_gzip_file()/write_to_gzip_file() on them (sgrid.d:858, :1003), and
// lmr/blockio.d, puffin/fluidblock.d, eilmer/* and src/gas do too, so the API
// spellings (front/popFront/empty/load/compress/finish) are kept as D left them.
//
// Namespace. The D module is spelled `module gzip;` (gzip.d:41) even though the
// file lives at src/extern/gzip/gzip.d, and the gdtk makefiles add it to the
// build by path (src/eilmer/makefile:284). The port follows the directory
// convention of the brief and puts it in eil::gzip, so `import gzip;` becomes
// #include "extern/gzip/gzip.hpp" with eil::gzip:: qualifications.
//
// std.zlib. gzip.d is a thin wrapper over Phobos' Compress/UnCompress, and those
// carry behaviour the callers depend on, so they are ported here too rather than
// being replaced by an ad-hoc zlib call. What was read out of the installed
// interface file (/home/tang/packages/dlang/import/std/zlib.d) and then measured
// with probes/extern/gzip/probe_gzip.d:
//
//   * UnCompress's default format is determineFromData, which is
//     inflateInit2(windowBits = 15+32): a bare zlib stream and a gzip stream both
//     decompress (measured: "zlib_autodetect" in the baseline).
//   * uncompress() returns nothing once the stream ended (inputEnded latches), so
//     bytes after the end of the deflate stream are ignored, and it returns
//     nothing for an empty input.
//   * flush() is a stub that only sets `done` and returns an empty buffer, which
//     is why the last chunk of a file never yields extra characters.
//   * Compress(HeaderFormat.gzip) is deflateInit2(Z_DEFAULT_COMPRESSION,
//     Z_DEFLATED, 15+16, memLevel 8, Z_DEFAULT_STRATEGY); compress() returns
//     nothing for an empty buffer and flush() returns nothing if nothing was ever
//     compressed, so writing an empty payload leaves a ZERO-byte file rather than
//     a valid gzip stream (measured: "empty_container bytes=0" plus
//     "empty_readback nlines=0"). Preserved, and reported as D-side behaviour.
//
// Non-1:1 points, none of them numeric:
//
//   * D's classes are GC handles. Nothing in the reference tree copies a reader
//     or a writer (they are constructed with `new` and used in place), so the port
//     makes them non-copyable: a copied read cursor would be a bug in either
//     language.
//   * Phobos' Compress.compress() re-reads the input it could not consume from
//     `zs.next_in`, i.e. from the *caller's* previous buffer (gzip.d:136 chain),
//     which would dangle when that string is gone. The port keeps the unconsumed
//     bytes in a member; same bytes, no dangling read.
//   * D's `File(name, "r")` raises std.exception.ErrnoException whose message the
//     probe pinned:
//       Cannot open file `NAME' in mode `MODE' (No such file or directory)
//     The port throws eil::ExceptionBase with that exact text (an ErrnoException
//     is an Exception, so D's `catch (Exception e)` still catches it).
//   * GzipOut closes its file in finish(), as D does; the destructor also closes
//     it if finish() was never reached, because C++ has no GC to do that.
//   * `uncompressedBuffer` is D's char[]; it is held as std::vector<char> and
//     front() returns char, as D's @property does. Callers that need unsigned
//     semantics must cast through unsigned char, which is what D's `char` already
//     is -- see the note in the test.

#include <cstddef>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include <zlib.h>

#include "eil/error.hpp"

namespace eil {
namespace gzip {

// Phobos spells the flush modes itself (std/zlib.d:59: Z_NO_FLUSH=0,
// Z_SYNC_FLUSH=2, Z_FULL_FLUSH=3, Z_FINISH=4); the port uses the identical values
// from zlib.h rather than redefining them.

// std/zlib.d:333 HeaderFormat.
enum class HeaderFormat { deflate, gzip, determineFromData };

// std/zlib.d:71 ZlibException : Exception. The message table is Phobos' own
// getmsg(), copied so a ported catch clause prints the same text as D.
class ZlibException : public eil::ExceptionBase {
  public:
    explicit ZlibException(int errnum, const char* file = "", size_t line = 0)
        : ExceptionBase(getmsg(errnum), file, line) {}

    static std::string getmsg(int errnum) {
        switch (errnum) {
          case Z_STREAM_END:   return "stream end";
          case Z_NEED_DICT:    return "need dict";
          case Z_ERRNO:        return "errno";
          case Z_STREAM_ERROR: return "stream error";
          case Z_DATA_ERROR:   return "data error";
          case Z_MEM_ERROR:    return "mem error";
          case Z_BUF_ERROR:    return "buf error";
          case Z_VERSION_ERROR: return "version error";
          default:             return "unknown error";
        }
    }
};

// std/zlib.d:530 class UnCompress, the subset gzip.d uses.
class UnCompress {
  public:
    explicit UnCompress(HeaderFormat format = HeaderFormat::determineFromData);
    ~UnCompress();
    UnCompress(const UnCompress&) = delete;
    UnCompress& operator=(const UnCompress&) = delete;

    // Decompress the data in buf and return the decompressed data. The buffers
    // returned from successive calls to this should be concatenated together.
    std::vector<char> uncompress(const std::vector<char>& buf);

    // Decompress and return any remaining data. The UnCompress object cannot be
    // used further. Phobos' flush() is a stub: it sets `done` and returns null.
    std::vector<char> flush();

    // True if all input data has been decompressed and no further data can be
    // decompressed (inflate() returned Z_STREAM_END).
    bool empty() const { return inputEnded; }

  private:
    z_stream zs{};
    int inited = 0;
    int done = 0;
    bool inputEnded = false;
    size_t destbufsize = 0;
    HeaderFormat format = HeaderFormat::determineFromData;

    void error(int err);
};

// std/zlib.d:343 class Compress, the subset gzip.d uses.
class Compress {
  public:
    explicit Compress(int level = Z_DEFAULT_COMPRESSION,
                      HeaderFormat header = HeaderFormat::deflate);
    explicit Compress(HeaderFormat header);
    ~Compress();
    Compress(const Compress&) = delete;
    Compress& operator=(const Compress&) = delete;

    // Compress the data in buf and return the compressed data. Successive return
    // values concatenate into one stream.
    std::vector<char> compress(const std::vector<char>& buf);

    // Compress and return any remaining data (Z_FINISH by default).
    std::vector<char> flush(int mode = Z_FINISH);

  private:
    z_stream zs{};
    int level = Z_DEFAULT_COMPRESSION;
    int inited = 0;
    bool gzip = false;
    // The buffer the stream's next_in points into. Phobos re-reads the input it
    // could not consume through zs.next_in, i.e. out of the caller's previous
    // array; keeping our own copy makes the same bytes available after the caller's
    // string is gone, which is what makes a multi-call compress() sequence safe.
    std::vector<char> inbuf;

    void error(int err);
};

class GzipInputRange {
  public:
    explicit GzipInputRange(const std::string& filename);
    ~GzipInputRange();
    GzipInputRange(const GzipInputRange&) = delete;
    GzipInputRange& operator=(const GzipInputRange&) = delete;

    void load();

    char front() const;

    void popFront();

    bool empty() const;

  private:
    std::unique_ptr<UnCompress> uncompressObj;
    FILE* f = nullptr;
    int CHUNKSIZE = 0x4000;
    // Stand-in for D's `ReturnType!(f.byChunk) chunkRange`: one looked-ahead
    // chunk, empty when the file has no more bytes.
    std::vector<char> chunk;
    size_t chunk_len = 0;
    bool chunk_pending = false;
    bool chunk_eof = false;
    bool exhausted = false;
    std::vector<char> uncompressedBuffer;
    size_t bufferIndex = 0;

    bool chunk_range_empty();
};

class GzipByLine {
  public:
    explicit GzipByLine(const std::string& filename);
    GzipByLine(const GzipByLine&) = delete;
    GzipByLine& operator=(const GzipByLine&) = delete;

    bool empty() const;

    void popFront();

    std::string front() const;

  private:
    GzipInputRange range;
    std::vector<char> buf;
};

class GzipOut {
  public:
    explicit GzipOut(const std::string& filename);
    ~GzipOut();
    GzipOut(const GzipOut&) = delete;
    GzipOut& operator=(const GzipOut&) = delete;

    void compress(const std::string& s);

    void finish();

  private:
    std::unique_ptr<Compress> compressObj;
    FILE* f = nullptr;
};

} // namespace gzip
} // namespace eil
