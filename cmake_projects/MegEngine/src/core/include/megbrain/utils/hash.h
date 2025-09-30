#pragma once

#include <cstddef>
#include <cstdint>
#include "megbrain/utils/thin/function.h"
#include "megbrain_build_config.h"

namespace mgb {

/*!
 * \brief combine two hash values
 */
static constexpr inline size_t hash_pair_combine(size_t a, size_t b) {
    return a * 20141203 + b;
}

template <typename T>
struct HashTrait {
    static size_t eval(const T& val) { return std::hash<T>()(val); }
};

template <typename T>
static inline size_t hash(const T& val) {
    return HashTrait<T>::eval(val);
}

template <typename T>
struct StdHashAdaptor {
    size_t operator()(const T& val) const { return hash<T>(val); }
};

/*!
 * \brief hash for std::pair
 */
struct pairhash {
public:
    template <typename T, typename U>
    size_t operator()(const std::pair<T, U>& x) const {
        return hash_pair_combine(hash<T>(x.first), hash<U>(x.second));
    }
};

/*!
 * \brief wrapper of the xxHash algorithm
 */
class XXHash {
    long long m_state[11];

public:
    MGE_WIN_DECLSPEC_FUC XXHash();
    MGE_WIN_DECLSPEC_FUC void reset();

    //! update internal state, and return *this
    MGE_WIN_DECLSPEC_FUC XXHash& update(const void* data, size_t len);

    //! get hash value, guaranteed to be non-zero
    MGE_WIN_DECLSPEC_FUC uint64_t digest() const;
};

/*!
 * \brief hash for enum class
 */
struct enumhash {
public:
    template <typename E, typename = std::enable_if_t<std::is_enum<E>::value>>
    size_t operator()(const E& e) const {
        return std::hash<typename std::underlying_type<E>::type>()(
                static_cast<typename std::underlying_type<const E>::type>(e));
    }
};

}  // namespace mgb
// vim: syntax=cpp.doxygen foldmethod=marker foldmarker=f{{{,f}}}
