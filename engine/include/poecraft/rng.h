#ifndef POECRAFT_RNG_H
#define POECRAFT_RNG_H

/*
 * Engine-owned random state. Random state belongs to an action context (or the
 * simulator's private context), never to shared session data and never to
 * process-global state. xoshiro256** seeded through splitmix64; deterministic
 * for a given seed but cross-platform/version replay is not guaranteed.
 */

#include <cstdint>

namespace poecraft {

class Rng {
public:
    explicit Rng(std::uint64_t seed) { reseed(seed); }

    void reseed(std::uint64_t seed);

    std::uint64_t next_u64();

    /* Uniform double in [0, 1). */
    double next_double();

    /* Uniform integer in [0, bound). Returns 0 when bound is 0. Uses unbiased
     * multiply-high rejection sampling. */
    std::uint64_t next_below(std::uint64_t bound);

private:
    std::uint64_t state_[4];
};

} // namespace poecraft

#endif
