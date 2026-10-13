#pragma once
// eil/array.hpp — D dynamic arrays and slices.
//
// D gives a function of type `T[]` a *view* into someone else's buffer, while
// `new T[n]` creates an owned GC block, and `~=` may silently relocate that block
// and leave earlier views pointing at stale storage. Across src/eilmer there are
// 1,727 `~=`, 530 `.length =` and 55 `$`-slices, so this file exists to make the
// distinction explicit and to catch the aliasing bug rather than reproduce it.
//
// Translation rules this supports:
//   T[] x = new T[n];        ->  Array<T> x(n);          (owned, zero-filled)
//   x.length = m;            ->  x.set_length(m);
//   x ~= v;                  ->  x.append(v);
//   x ~= other[];            ->  x.append_slice(other.all());
//   x[a..b] / x[a..$]        ->  x.slice(a,b) / x.from(a)
//   void f(T[] y)            ->  void f(Slice<T> y)      (a view, as in D)
//   auto y = x;              ->  Slice<T> y = x.all();   (D copies the slice, not
//                                                         the data — say so in the
//                                                         ported file)
//   x[] = 0.0;               ->  x.fill(0.0);
//
// Copying an Array is a deep copy here, which is deliberately *not* what
// `auto b = a` does in D. Ported code that needs the aliasing behaviour takes a
// Slice; ported code that needs a copy calls dup().
//
// With -DEIL_SLICE_CHECK, every Slice dereference verifies that the owning Array
// has not reallocated since the Slice was taken (generation counter), and throws
// SliceError instead of reading freed memory.

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <new>
#include <string>
#include <utility>
#include <vector>

#include "eil/error.hpp"

namespace eil {

template <typename T> class Array;

template <typename T> class Slice {
  public:
    Slice() = default;
    Slice(T* d, size_t n, const uint32_t* gen, uint32_t stamp)
        : d_(d), n_(n), gen_(gen), stamp_(stamp) {}

    size_t length() const { return n_; }
    size_t size() const { return n_; }  // D's `$`
    bool empty() const { return n_ == 0; }

    T& operator[](size_t i) {
        check_live();
        if (i >= n_) throw RangeError(i, n_);
        return d_[i];
    }
    const T& operator[](size_t i) const {
        check_live();
        if (i >= n_) throw RangeError(i, n_);
        return d_[i];
    }
    T& at(size_t i) { return (*this)[i]; }
    T& front() { return (*this)[0]; }
    T& back() { return (*this)[n_ - 1]; }
    T* data() { check_live(); return d_; }
    const T* data() const { check_live(); return d_; }

    // Nested views of a view keep the owner's generation stamp.
    Slice slice(size_t lo, size_t hi) const {
        check_live();
        if (lo > hi || hi > n_) throw RangeError(hi > n_ ? hi : lo, n_);
        return Slice(d_ + lo, hi - lo, gen_, stamp_);
    }
    Slice from(size_t lo) const { return slice(lo, n_); }  // [lo..$]
    Slice to(size_t hi) const { return slice(0, hi); }     // [0..hi]
    Slice all() const { return slice(0, n_); }             // s[]

    void fill(const T& v) {
        check_live();
        for (size_t i = 0; i < n_; ++i) d_[i] = v;
    }
    // D's `dst[] = src[]` copies element-wise (it is not a re-alias).
    void assign_from(const Slice& src) {
        if (src.length() != n_) throw SliceError("length mismatch in slice assignment");
        check_live();
        src.check_live();
        for (size_t i = 0; i < n_; ++i) d_[i] = src.d_[i];
    }

    // Range-for and std algorithms iterate unchecked; sites that need the
    // generation guarantee index through operator[].
    T* begin() { return d_; }
    T* end() { return d_ + n_; }
    const T* begin() const { return d_; }
    const T* end() const { return d_ + n_; }

    void check_live() const {
#ifdef EIL_SLICE_CHECK
        if (gen_ && *gen_ != stamp_) {
            throw SliceError("slice refers to a reallocated array");
        }
#endif
    }

  private:
    T* d_ = nullptr;
    size_t n_ = 0;
    const uint32_t* gen_ = nullptr;
    uint32_t stamp_ = 0;
};

template <typename T> class Array {
  public:
    Array() = default;

    // D's `new T[n]` hands back zeroed GC memory, so this value-initializes.
    explicit Array(size_t n) { set_length(n); }
    Array(size_t n, const T& fill_value) {
        reserve(n);
        for (size_t i = 0; i < n; ++i) push(fill_value);
    }
    Array(std::initializer_list<T> init) {
        reserve(init.size());
        for (const T& v : init) push(v);
    }
    Array(const Slice<T>& src) {
        reserve(src.length());
        for (size_t i = 0; i < src.length(); ++i) push(src[i]);
    }

    Array(const Array& o) { assign(o); }
    Array& operator=(const Array& o) {
        if (this != &o) assign(o);
        return *this;
    }
    Array(Array&& o) noexcept
        : p_(o.p_), n_(o.n_), cap_(o.cap_), gen_(o.gen_), gen_value_(o.gen_value_) {
        o.p_ = nullptr; o.n_ = 0; o.cap_ = 0; o.gen_ = nullptr;
    }
    Array& operator=(Array&& o) noexcept {
        if (this != &o) {
            destroy_storage();
            p_ = o.p_; n_ = o.n_; cap_ = o.cap_; gen_ = o.gen_; gen_value_ = o.gen_value_;
            o.p_ = nullptr; o.n_ = 0; o.cap_ = 0; o.gen_ = nullptr;
        }
        return *this;
    }
    ~Array() { destroy_storage(); }

    size_t length() const { return n_; }
    size_t size() const { return n_; }
    size_t dollar() const { return n_; }  // spelling for `$` in ported expressions
    bool empty() const { return n_ == 0; }
    size_t capacity() const { return cap_; }

    // D's `arr.length = m`: keeps elements, value-initializes grown tail, and
    // preserves pointer identity when capacity allows (626 @nogc sites in the
    // solver cache `.ptr` across such calls).
    void set_length(size_t m) {
        if (m <= n_) {
            n_ = m;
            return;
        }
        reserve(m);
        for (size_t i = n_; i < m; ++i) new (p_ + i) T();
        n_ = m;
    }

    T& operator[](size_t i) {
        if (i >= n_) throw RangeError(i, n_);
        return p_[i];
    }
    const T& operator[](size_t i) const {
        if (i >= n_) throw RangeError(i, n_);
        return p_[i];
    }
    T& front() { return (*this)[0]; }
    T& back() { return (*this)[n_ - 1]; }
    T* data() { return p_; }
    const T* data() const { return p_; }
    T* ptr() { return p_; }  // D spelling, used by @nogc translation sites

    // Views.
    Slice<T> all() { return Slice<T>(p_, n_, gen_, gen_value_); }
    Slice<T> all() const { return Slice<T>(const_cast<T*>(p_), n_, gen_, gen_value_); }
    Slice<T> slice(size_t lo, size_t hi) {
        if (lo > hi || hi > n_) throw RangeError(hi > n_ ? hi : lo, n_);
        return Slice<T>(p_ + lo, hi - lo, gen_, gen_value_);
    }
    Slice<T> from(size_t lo) { return slice(lo, n_); }
    Slice<T> to(size_t hi) { return slice(0, hi); }

    // Append (`~=`). Growing within capacity keeps the data pointer stable; a
    // reallocation bumps the generation so any live Slice fails loudly.
    void append(const T& v) {
        if (n_ == cap_) grow(n_ + 1);
        new (p_ + n_) T(v);
        ++n_;
    }
    void append_slice(const Slice<T>& src) {
        reserve(n_ + src.length());
        for (size_t i = 0; i < src.length(); ++i) append(src[i]);
    }
    void append(const Array<T>& src) { append_slice(src.all()); }
    void push(const T& v) { append(v); }

    // `a ~ b` is a new array, not an in-place append.
    Array<T> concat(const Slice<T>& extra) const {
        Array<T> out;
        out.reserve(n_ + extra.length());
        for (size_t i = 0; i < n_; ++i) out.push(p_[i]);
        for (size_t i = 0; i < extra.length(); ++i) out.push(extra[i]);
        return out;
    }

    Array<T> dup() const { return Array<T>(*this); }  // `.dup`
    void fill(const T& v) {
        for (size_t i = 0; i < n_; ++i) p_[i] = v;
    }
    void clear() { n_ = 0; }
    void reserve(size_t want) {
        if (want > cap_) grow(want);
    }

    T* begin() { return p_; }
    T* end() { return p_ + n_; }
    const T* begin() const { return p_; }
    const T* end() const { return p_ + n_; }

  private:
    void assign(const Array& o) {
        clear_allocation_for_assign(o.n_);
        for (size_t i = 0; i < o.n_; ++i) new (p_ + i) T(o.p_[i]);
        n_ = o.n_;
    }
    void clear_allocation_for_assign(size_t want) {
        if (cap_ < want) {
            destroy_storage();
            p_ = nullptr; n_ = 0; cap_ = 0;
            gen_ = nullptr;
            alloc_gen();
            grow(want);
        } else {
            for (size_t i = 0; i < n_; ++i) p_[i].~T();
            n_ = 0;
        }
    }
    void alloc_gen() {
        if (!gen_) {
            gen_ = new uint32_t(1);
            gen_value_ = *gen_;
        }
    }
    void grow(size_t want) {
        alloc_gen();
        size_t newcap = want;
        if (newcap < 16) newcap = 16;
        if (newcap < cap_ * 2) newcap = cap_ * 2;
        T* np = static_cast<T*>(::operator new(newcap * sizeof(T), std::nothrow));
        if (!np) throw OutOfMemoryError("Array allocation failed");
        for (size_t i = 0; i < n_; ++i) {
            new (np + i) T(std::move(p_[i]));
            p_[i].~T();
        }
        ::operator delete(p_);
        p_ = np;
        cap_ = newcap;
        // Reallocation invalidates every previously handed-out view.
        *gen_ = ++gen_value_;
    }
    void destroy_storage() {
        for (size_t i = 0; i < n_; ++i) p_[i].~T();
        ::operator delete(p_);
        delete gen_;
        p_ = nullptr; n_ = 0; cap_ = 0; gen_ = nullptr;
    }

    T* p_ = nullptr;
    size_t n_ = 0;
    size_t cap_ = 0;
    // Separate heap cell so the generation survives in-place growth and stays
    // addressable by every view ever taken.
    uint32_t* gen_ = nullptr;
    uint32_t gen_value_ = 0;
};

// Numeric containers render through eil::to_string in format.hpp, which detects
// anything with length() and operator[](size_t) so no include order is required.

// D's `new T[n][m]` for the jagged cases in the solver.
template <typename T> Array<T> make_array(size_t n) { return Array<T>(n); }

} // namespace eil
