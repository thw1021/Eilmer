// gzip.d

#include "extern/gzip/gzip.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include <zlib.h>

#include "eil/error.hpp"

namespace eil {
namespace gzip {

namespace {

// std.stdio.File(name, mode) raises ErrnoException, whose text the reference
// probe pinned:
//   Cannot open file `no_such_file.gz' in mode `r' (No such file or directory)
// `d_mode` is the spelling D puts in the message ("r"/"w"), `c_mode` the one
// fopen needs; the binary flag matters on Windows only, and the reference
// behaviour measured here is Linux.
FILE* open_file(const std::string& name, const char* d_mode, const char* c_mode) {
    FILE* h = std::fopen(name.c_str(), c_mode);
    if (!h) {
        const int e = errno;
        throw eil::ExceptionBase("Cannot open file `" + name + "' in mode `" + d_mode +
                                 "' (" + std::strerror(e) + ")");
    }
    return h;
}

} // namespace

// ---------------------------------------------------------------------------
// std.zlib UnCompress

UnCompress::UnCompress(HeaderFormat format) : format(format) {}

UnCompress::~UnCompress() {
    if (inited) {
        inited = 0;
        inflateEnd(&zs);
    }
    done = 1;
}

void UnCompress::error(int err) {
    if (inited) {
        inflateEnd(&zs);
        inited = 0;
    }
    throw ZlibException(err, __FILE__, __LINE__);
}

std::vector<char> UnCompress::uncompress(const std::vector<char>& buf) {
    // std/zlib.d in-contract: assert(!done, "Buffer has been flushed.")
    eil_assert(!done);
    if (inputEnded || buf.empty()) return std::vector<char>();

    if (!inited) {
        int windowBits = 15;
        if (format == HeaderFormat::gzip) windowBits += 16;
        else if (format == HeaderFormat::determineFromData) windowBits += 32;
        int err = inflateInit2(&zs, windowBits);
        if (err) error(err);
        inited = 1;
    }

    if (destbufsize == 0) destbufsize = static_cast<size_t>(buf.size()) * 2;
    std::vector<char> destbuf(destbufsize);
    size_t destFill = 0;

    zs.next_in = reinterpret_cast<Bytef*>(const_cast<char*>(buf.data()));
    zs.avail_in = static_cast<uint>(buf.size());

    while (true) {
        zs.next_out = reinterpret_cast<Bytef*>(destbuf.data() + destFill);
        zs.avail_out = static_cast<uint>(destbuf.size() - destFill);

        int err = inflate(&zs, Z_NO_FLUSH);
        if (err == Z_STREAM_END) {
            inputEnded = true;
            break;
        } else if (err != Z_OK) {
            error(err);
        } else if (zs.avail_in == 0) {
            break;
        }
        /*
            According to the zlib manual inflate() stops when either there's
            no more data to uncompress or the output buffer is full
            So at this point, the output buffer is too full
        */
        destFill = destbuf.size();
        // Phobos grows the GC block in place where it can; resize() keeps the
        // bytes already written either way, which is all the returned slice sees.
        destbuf.resize(destbuf.size() + destbufsize);
    }

    destbuf.resize(destbuf.size() - zs.avail_out);
    return destbuf;
}

std::vector<char> UnCompress::flush() {
    // std/zlib.d in-contract: assert(!done, "Buffer has been flushed before.")
    eil_assert(!done);
    done = 1;
    // Phobos' flush() decompresses nothing further and returns null; the out
    // contract is satisfied by the line above.
    return std::vector<char>();
}

// ---------------------------------------------------------------------------
// std.zlib Compress

Compress::Compress(int level, HeaderFormat header)
    : level(level), gzip(header == HeaderFormat::gzip) {
    // std/zlib.d in-contract: "Legal compression level are in [1, 9]."
    eil_assert(1 <= this->level && this->level <= 9);
}

Compress::Compress(HeaderFormat header) : gzip(header == HeaderFormat::gzip) {}

Compress::~Compress() {
    if (inited) {
        inited = 0;
        deflateEnd(&zs);
    }
}

void Compress::error(int err) {
    if (inited) {
        deflateEnd(&zs);
        inited = 0;
    }
    throw ZlibException(err, __FILE__, __LINE__);
}

std::vector<char> Compress::compress(const std::vector<char>& buf) {
    if (buf.empty()) return std::vector<char>();   // D: `if (buf.length == 0) return null;`

    if (!inited) {
        int err = deflateInit2(&zs, level, Z_DEFLATED, 15 + (gzip ? 16 : 0), 8,
                               Z_DEFAULT_STRATEGY);
        if (err) error(err);
        inited = 1;
    }

    // D: `if (zs.avail_in) buf = zs.next_in[0 .. zs.avail_in] ~ cast(ubyte[]) buf;`
    // -- the input this call sees is last call's unconsumed tail plus the new
    // bytes. The tail lives in inbuf (see the header note) instead of in the
    // stream's next_in, so it is still valid here.
    if (inited && zs.avail_in) {
        // Drop what has already been consumed, keep the tail.
        inbuf.erase(inbuf.begin(), inbuf.end() - static_cast<long>(zs.avail_in));
    } else {
        inbuf.clear();
    }
    inbuf.insert(inbuf.end(), buf.begin(), buf.end());

    // Output room = whatever is still pending from the last call + this buffer,
    // exactly as D sizes it (zs.avail_in + buf.length).
    std::vector<char> destbuf(inbuf.size());
    zs.next_out = reinterpret_cast<Bytef*>(destbuf.data());
    zs.avail_out = static_cast<uint>(destbuf.size());

    zs.next_in = reinterpret_cast<Bytef*>(inbuf.data());
    zs.avail_in = static_cast<uint>(inbuf.size());

    int err = deflate(&zs, Z_NO_FLUSH);
    if (err != Z_STREAM_END && err != Z_OK) error(err);

    // Keep inbuf alive while anything is unconsumed: flush() and the next
    // compress() both read on from zs.next_in, which points into it.
    if (zs.avail_in == 0) inbuf.clear();
    destbuf.resize(destbuf.size() - zs.avail_out);
    return destbuf;
}

std::vector<char> Compress::flush(int mode) {
    // std/zlib.d in-contract on the mode argument.
    eil_assert(mode == Z_FINISH || mode == Z_SYNC_FLUSH || mode == Z_FULL_FLUSH);
    if (!inited) return std::vector<char>();

    std::vector<char> destbuf;
    std::vector<char> tmpbuf(512);
    int err;

    /* may be  zs.avail_out+<some constant>
     * zs.avail_out is set nonzero by deflate in previous compress()
     */
    zs.next_out = reinterpret_cast<Bytef*>(tmpbuf.data());
    zs.avail_out = static_cast<uint>(tmpbuf.size());

    while ((err = deflate(&zs, mode)) != Z_STREAM_END) {
        if (err == Z_OK) {
            if (zs.avail_out != 0 && mode != Z_FINISH) {
                break;
            } else if (zs.avail_out == 0) {
                destbuf.insert(destbuf.end(), tmpbuf.begin(), tmpbuf.end());
                zs.next_out = reinterpret_cast<Bytef*>(tmpbuf.data());
                zs.avail_out = static_cast<uint>(tmpbuf.size());
                continue;
            }
            err = Z_BUF_ERROR;
        }
        error(err);
    }
    destbuf.insert(destbuf.end(), tmpbuf.begin(),
                   tmpbuf.begin() + static_cast<long>(tmpbuf.size() - zs.avail_out));

    if (mode == Z_FINISH) {
        err = deflateEnd(&zs);
        inited = 0;
        if (err) error(err);
    }
    return destbuf;
}

// ---------------------------------------------------------------------------
// gzip.d GzipInputRange

GzipInputRange::GzipInputRange(const std::string& filename)
    : uncompressObj(std::make_unique<UnCompress>()), f(open_file(filename, "r", "rb")) {
    chunk.resize(static_cast<size_t>(CHUNKSIZE));
    load();
}

GzipInputRange::~GzipInputRange() {
    if (f) std::fclose(f);
}

bool GzipInputRange::chunk_range_empty() {
    // One-chunk lookahead standing in for D's lazy f.byChunk(CHUNKSIZE): the read
    // happens when the range is first interrogated, and an empty read is the only
    // way D's chunkRange reports empty.
    if (!chunk_pending && !chunk_eof) {
        const size_t n = std::fread(chunk.data(), 1, static_cast<size_t>(CHUNKSIZE), f);
        chunk_len = n;
        chunk_pending = n > 0;
        if (n == 0) chunk_eof = true;
    }
    return chunk_len == 0;
}

void GzipInputRange::load() {
    if (!chunk_range_empty()) {
        std::vector<char> raw(chunk.begin(), chunk.begin() + static_cast<long>(chunk_len));
        chunk_pending = false;   // chunkRange.popFront()
        uncompressedBuffer = uncompressObj->uncompress(raw);
        bufferIndex = 0;
    } else {
        if (!exhausted) {
            uncompressedBuffer = uncompressObj->flush();
            exhausted = true;
            bufferIndex = 0;
        } else {
            uncompressedBuffer.clear();   // uncompressedBuffer.length = 0
        }
    }
}

char GzipInputRange::front() const {
    // D indexes the array unchecked and raises core.exception.RangeError, which is
    // an Error, so the port throws eil::RangeError (a D `catch (Exception e)` must
    // not swallow it).
    if (bufferIndex >= uncompressedBuffer.size()) {
        throw eil::RangeError(bufferIndex, uncompressedBuffer.size());
    }
    return uncompressedBuffer[bufferIndex];
}

void GzipInputRange::popFront() {
    bufferIndex += 1;
    if (bufferIndex >= uncompressedBuffer.size()) {
        load();
        bufferIndex = 0;
    }
}

bool GzipInputRange::empty() const { return uncompressedBuffer.size() == 0; }

// ---------------------------------------------------------------------------
// gzip.d GzipByLine

GzipByLine::GzipByLine(const std::string& filename) : range(filename) { popFront(); }

bool GzipByLine::empty() const { return buf.size() == 0; }

void GzipByLine::popFront() {
    buf.clear();
    while (!range.empty() && range.front() != '\n') {
        buf.push_back(range.front());
        range.popFront();
    }
    range.popFront();
}

std::string GzipByLine::front() const { return std::string(buf.begin(), buf.end()); }

// ---------------------------------------------------------------------------
// gzip.d GzipOut

GzipOut::GzipOut(const std::string& filename) : f(open_file(filename, "w", "wb")) {
    compressObj = std::make_unique<Compress>(HeaderFormat::gzip);
}

GzipOut::~GzipOut() {
    // D relies on the GC to close the handle; C++ closes it here too, but only if
    // finish() did not already do it.
    if (f) std::fclose(f);
}

void GzipOut::compress(const std::string& s) {
    const std::vector<char> payload(s.begin(), s.end());
    std::vector<char> compressed = compressObj->compress(payload);
    if (!compressed.empty()) {
        std::fwrite(compressed.data(), 1, compressed.size(), f);   // f.rawWrite(compressed)
    }
}

void GzipOut::finish() {
    std::vector<char> compressed = compressObj->flush();
    if (!compressed.empty()) std::fwrite(compressed.data(), 1, compressed.size(), f);
    // 2015-Oct-18 PJ Let's force the clean-up in the file system.
    std::fflush(f);
    std::fclose(f);
    f = nullptr;
}

} // namespace gzip
} // namespace eil
