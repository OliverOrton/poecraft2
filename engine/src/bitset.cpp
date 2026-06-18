#include "poecraft/bitset.h"

namespace poecraft {

void pc_bitset_zero_padding(std::uint64_t* words, std::size_t bit_count) {
    const std::size_t word_count = pc_bitset_words(bit_count);
    if (word_count == 0) {
        return;
    }
    const std::size_t used_bits = bit_count & 63;
    if (used_bits == 0) {
        return; /* final word is fully used */
    }
    const std::uint64_t keep = (std::uint64_t{1} << used_bits) - 1;
    words[word_count - 1] &= keep;
}

std::size_t pc_bitset_count(const std::uint64_t* words, std::size_t word_count) {
    std::size_t total = 0;
    for (std::size_t i = 0; i < word_count; ++i) {
        total += static_cast<std::size_t>(std::popcount(words[i]));
    }
    return total;
}

void pc_bitset_and(
    std::uint64_t* out,
    const std::uint64_t* a,
    const std::uint64_t* b,
    std::size_t word_count) {
    for (std::size_t i = 0; i < word_count; ++i) {
        out[i] = a[i] & b[i];
    }
}

void pc_bitset_or(
    std::uint64_t* out,
    const std::uint64_t* a,
    const std::uint64_t* b,
    std::size_t word_count) {
    for (std::size_t i = 0; i < word_count; ++i) {
        out[i] = a[i] | b[i];
    }
}

void pc_bitset_andnot(
    std::uint64_t* out,
    const std::uint64_t* a,
    const std::uint64_t* b,
    std::size_t word_count) {
    for (std::size_t i = 0; i < word_count; ++i) {
        out[i] = a[i] & ~b[i];
    }
}

} // namespace poecraft
