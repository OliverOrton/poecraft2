#include "poecraft/rng.h"

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
    if (bound == 0) {
        return 0;
    }
    /* Unbiased bounded sampling via rejection on the largest multiple of
     * bound that fits in 64 bits. */
    const std::uint64_t limit = UINT64_MAX - (UINT64_MAX % bound);
    std::uint64_t value;
    do {
        value = next_u64();
    } while (value >= limit);
    return value % bound;
}

} // namespace poecraft
