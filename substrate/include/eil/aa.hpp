#pragma once
// eil/aa.hpp — D associative arrays.
//
// Measured behaviour (tools/fmt_corpus.txt, section 6):
//   aa_len 2 / aa_get 1 / aa_has true false
//   aa_keys ["alpha", "beta"]      <- elements quoted under %s
//   aa_values [1, 2]
//   aa_missing Range violation     <- reading an absent key is a *RangeError*
//   aa_int_key seven
//   aa_nested value                <- string[string][size_t] works as written
//
// D distinguishes the two spellings of `aa[k]` by context — assignment inserts,
// a read on an absent key throws "Range violation". C++ operator[] cannot tell
// them apart, so the port uses two names and the difference stays visible in
// the source:
//   aa[k] = v;   ->  aa.set(k, v);
//   v = aa[k];   ->  aa.get(k);            (throws when absent)
//   (k in aa)    ->  aa.find(k) / aa.contains(k)
//
// Backing store is std::map, i.e. sorted. D's AA iteration order is randomized
// per process by druntime's hash seed, so any D output that depended on it would
// already be unreproducible; reports/phase-0.md records the repeat-run evidence
// that no solver output does.

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "eil/array.hpp"
#include "eil/error.hpp"

namespace eil {

// core.exception.RangeError text for a missing AA key, as captured from Phobos.
class RangeViolation : public ErrorBase {
  public:
    explicit RangeViolation(const char* file = "", size_t line = 0)
        : ErrorBase("Range violation", file, line) {}
};

template <typename K, typename V> class AA {
  public:
    using map_type = std::map<K, V>;

    // `aa[k] = v`
    V& set(const K& k, const V& v) {
        auto r = m_.insert(std::make_pair(k, v));
        if (!r.second) r.first->second = v;
        return r.first->second;
    }

    // `aa[k]` used as a value: throws RangeError with D's message.
    V& get(const K& k) {
        auto it = m_.find(k);
        if (it == m_.end()) throw RangeViolation();
        return it->second;
    }
    const V& get(const K& k) const {
        auto it = m_.find(k);
        if (it == m_.end()) throw RangeViolation();
        return it->second;
    }
    // Assignment context in D inserts a default before writing; ports that
    // build a value in place use this.
    V& get_or_default(const K& k) { return m_[k]; }

    // `(k in aa)` yields a pointer in D, null when absent.
    V* find(const K& k) {
        auto it = m_.find(k);
        return it == m_.end() ? nullptr : &it->second;
    }
    const V* find(const K& k) const {
        auto it = m_.find(k);
        return it == m_.end() ? nullptr : &it->second;
    }
    bool contains(const K& k) const { return m_.find(k) != m_.end(); }

    size_t length() const { return m_.size(); }
    bool empty() const { return m_.empty(); }
    void remove(const K& k) { m_.erase(k); }
    void clear() { m_.clear(); }

    Array<K> keys() const {
        Array<K> out;
        out.reserve(m_.size());
        for (const auto& e : m_) out.append(e.first);
        return out;
    }
    Array<V> values() const {
        Array<V> out;
        out.reserve(m_.size());
        for (const auto& e : m_) out.append(e.second);
        return out;
    }

    // `foreach (v, k; aa)`
    auto begin() { return m_.begin(); }
    auto end() { return m_.end(); }
    auto begin() const { return m_.begin(); }
    auto end() const { return m_.end(); }

  private:
    map_type m_;
};

} // namespace eil
