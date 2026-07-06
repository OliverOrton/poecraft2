#include "tests.hpp"

#include "poecraft/rng.h"

#include <cstdint>
#include <vector>

using namespace poecraft;

void run_rng_tests() {
    /* Same seed -> identical sequence (deterministic per context). */
    Rng a(12345);
    Rng b(12345);
    bool identical = true;
    for (int i = 0; i < 100; ++i) {
        if (a.next_u64() != b.next_u64()) {
            identical = false;
            break;
        }
    }
    PC_CHECK(identical);

    /* Different seeds -> different sequences (almost surely). */
    Rng c(1);
    Rng d(2);
    bool differs = false;
    for (int i = 0; i < 8; ++i) {
        if (c.next_u64() != d.next_u64()) {
            differs = true;
            break;
        }
    }
    PC_CHECK(differs);

    /* reseed resets the sequence. */
    Rng e(999);
    std::uint64_t first = e.next_u64();
    e.next_u64();
    e.reseed(999);
    PC_CHECK(e.next_u64() == first);

    /* next_below stays in range and 0-bound is safe. */
    Rng f(77);
    PC_CHECK(f.next_below(0) == 0);
    bool in_range = true;
    std::vector<int> seen(10, 0);
    for (int i = 0; i < 5000; ++i) {
        std::uint64_t v = f.next_below(10);
        if (v >= 10) {
            in_range = false;
            break;
        }
        seen[static_cast<std::size_t>(v)]++;
    }
    PC_CHECK(in_range);
    /* every bucket should appear at least once over 5000 draws */
    bool all_seen = true;
    for (int count : seen) {
        if (count == 0) {
            all_seen = false;
        }
    }
    PC_CHECK(all_seen);

    /* Edge bounds exercise multiply-high and its rejection threshold. */
    Rng h(88);
    bool edge_bounds_ok = true;
    for (int i = 0; i < 1000; ++i) {
        edge_bounds_ok &= h.next_below(1) == 0;
        edge_bounds_ok &=
            h.next_below(UINT64_MAX) < UINT64_MAX;
        edge_bounds_ok &=
            h.next_below((std::uint64_t{1} << 63) + 1) <
            (std::uint64_t{1} << 63) + 1;
    }
    PC_CHECK(edge_bounds_ok);

    /* next_double in [0, 1). */
    Rng g(5);
    bool double_ok = true;
    for (int i = 0; i < 1000; ++i) {
        double x = g.next_double();
        if (!(x >= 0.0 && x < 1.0)) {
            double_ok = false;
            break;
        }
    }
    PC_CHECK(double_ok);
}
