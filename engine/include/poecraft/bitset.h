#ifndef POECRAFT_BITSET_H
#define POECRAFT_BITSET_H

/*
 * Word-based bitset helpers over caller-owned uint64_t storage. The engine
 * keeps masks as dense uint64_t arrays indexed by session mod id; see
 * docs/engine-bitsets.md. These helpers operate on raw word spans so masks can
 * live in arenas, scratch, or session data without an owning container.
 *
 * Bit k corresponds to session mod id k. Word i holds bits [64*i, 64*i+63].
 * Padding bits beyond bit_count in the final word must stay zero; callers that
 * fill words wholesale should call pc_bitset_zero_padding.
 */

#include <bit>
#include <cstddef>
#include <cstdint>

namespace poecraft {

/* Number of 64-bit words needed to hold bit_count bits. */
constexpr std::size_t pc_bitset_words(std::size_t bit_count) {
    return (bit_count + 63) / 64;
}

inline void pc_bitset_set(std::uint64_t* words, std::size_t bit) {
    words[bit >> 6] |= (std::uint64_t{1} << (bit & 63));
}

inline void pc_bitset_clear(std::uint64_t* words, std::size_t bit) {
    words[bit >> 6] &= ~(std::uint64_t{1} << (bit & 63));
}

inline bool pc_bitset_test(const std::uint64_t* words, std::size_t bit) {
    return (words[bit >> 6] >> (bit & 63)) & std::uint64_t{1};
}

inline void pc_bitset_zero(std::uint64_t* words, std::size_t word_count) {
    for (std::size_t i = 0; i < word_count; ++i) {
        words[i] = 0;
    }
}

/* Zero the padding bits above bit_count in the final word. */
void pc_bitset_zero_padding(std::uint64_t* words, std::size_t bit_count);

/* Population count over word_count words. */
std::size_t pc_bitset_count(const std::uint64_t* words, std::size_t word_count);

/* out[i] = a[i] & b[i] for word_count words. Aliasing out with a or b is fine. */
void pc_bitset_and(
    std::uint64_t* out,
    const std::uint64_t* a,
    const std::uint64_t* b,
    std::size_t word_count);

void pc_bitset_or(
    std::uint64_t* out,
    const std::uint64_t* a,
    const std::uint64_t* b,
    std::size_t word_count);

/* out[i] = a[i] & ~b[i] (set difference). */
void pc_bitset_andnot(
    std::uint64_t* out,
    const std::uint64_t* a,
    const std::uint64_t* b,
    std::size_t word_count);

/*
 * Visit every set bit in ascending id order, calling fn(id) for each. fn is any
 * callable taking std::size_t. Never yields ids outside [0, word_count*64).
 */
template <typename Fn>
void pc_bitset_for_each(
    const std::uint64_t* words,
    std::size_t word_count,
    Fn&& fn) {
    for (std::size_t w = 0; w < word_count; ++w) {
        std::uint64_t bits = words[w];
        while (bits) {
            const std::size_t bit =
                static_cast<std::size_t>(std::countr_zero(bits));
            fn(w * 64 + bit);
            bits &= bits - 1; /* clear lowest set bit */
        }
    }
}

} // namespace poecraft

#endif
