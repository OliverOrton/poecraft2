#include "poecraft/rng.h"

#if defined(_MSC_VER) && defined(_M_X64)
#include <intrin.h>
#endif

namespace poecraft {

namespace {

inline std::uint64_t splitmix64(std::uint64_t& x) {
    x += 0x9e3779b97f4a7c15ULL;
    std::uint64_t z = x;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

inline std::uint64_t rotl(std::uint64_t x, int k) {
    return (x << k) | (x >> (64 - k));
}

inline std::uint64_t multiply_high(
    std::uint64_t a,
    std::uint64_t b,
    std::uint64_t& low) {
#if defined(_MSC_VER) && defined(_M_X64)
    std::uint64_t high = 0;
    low = _umul128(a, b, &high);
    return high;
#elif defined(__SIZEOF_INT128__)
    const unsigned __int128 product =
        static_cast<unsigned __int128>(a) * b;
    low = static_cast<std::uint64_t>(product);
    return static_cast<std::uint64_t>(product >> 64);
#else
    // Portable fallback for unusual 64-bit targets without multiply-high.
    const std::uint64_t a0 = static_cast<std::uint32_t>(a);
    const std::uint64_t a1 = a >> 32;
    const std::uint64_t b0 = static_cast<std::uint32_t>(b);
    const std::uint64_t b1 = b >> 32;
    const std::uint64_t p00 = a0 * b0;
    const std::uint64_t p01 = a0 * b1;
    const std::uint64_t p10 = a1 * b0;
    const std::uint64_t p11 = a1 * b1;
    const std::uint64_t middle =
        (p00 >> 32) + static_cast<std::uint32_t>(p01) +
        static_cast<std::uint32_t>(p10);
    low = (middle << 32) | static_cast<std::uint32_t>(p00);
    return p11 + (p01 >> 32) + (p10 >> 32) + (middle >> 32);
#endif
}

} // namespace

void Rng::reseed(std::uint64_t seed) {
    std::uint64_t s = seed;
    for (auto& word : state_) {
        word = splitmix64(s);
    }
    /* xoshiro256** requires a non-zero state; splitmix64 guarantees this for
     * any seed, but guard against a pathological all-zero result. */
    if ((state_[0] | state_[1] | state_[2] | state_[3]) == 0) {
        state_[0] = 0x9e3779b97f4a7c15ULL;
    }
}

std::uint64_t Rng::next_u64() {
    const std::uint64_t result = rotl(state_[1] * 5, 7) * 9;
    const std::uint64_t t = state_[1] << 17;

    state_[2] ^= state_[0];
    state_[3] ^= state_[1];
    state_[1] ^= state_[2];
    state_[0] ^= state_[3];
    state_[2] ^= t;
    state_[3] = rotl(state_[3], 45);

    return result;
}

double Rng::next_double() {
    /* Top 53 bits give a uniform double in [0, 1). */
    return static_cast<double>(next_u64() >> 11) * (1.0 / 9007199254740992.0);
}

std::uint64_t Rng::next_below(std::uint64_t bound) {
    if (bound == 0) return 0;
    std::uint64_t low = 0;
    std::uint64_t high = multiply_high(next_u64(), bound, low);
    if (low < bound) {
        const std::uint64_t threshold = -bound % bound;
        while (low < threshold) {
            high = multiply_high(next_u64(), bound, low);
        }
    }
    return high;
}

} // namespace poecraft
