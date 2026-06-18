#include "tests.hpp"

#include "poecraft/bitset.h"

#include <cstdint>
#include <vector>

using namespace poecraft;

void run_bitset_tests() {
    PC_CHECK(pc_bitset_words(0) == 0);
    PC_CHECK(pc_bitset_words(1) == 1);
    PC_CHECK(pc_bitset_words(64) == 1);
    PC_CHECK(pc_bitset_words(65) == 2);
    PC_CHECK(pc_bitset_words(130) == 3);

    const std::size_t bits = 130;
    const std::size_t words = pc_bitset_words(bits);
    std::vector<std::uint64_t> a(words, 0);

    pc_bitset_set(a.data(), 0);
    pc_bitset_set(a.data(), 63);
    pc_bitset_set(a.data(), 64);
    pc_bitset_set(a.data(), 129);
    PC_CHECK(pc_bitset_test(a.data(), 0));
    PC_CHECK(pc_bitset_test(a.data(), 63));
    PC_CHECK(pc_bitset_test(a.data(), 64));
    PC_CHECK(pc_bitset_test(a.data(), 129));
    PC_CHECK(!pc_bitset_test(a.data(), 1));
    PC_CHECK(pc_bitset_count(a.data(), words) == 4);

    pc_bitset_clear(a.data(), 63);
    PC_CHECK(!pc_bitset_test(a.data(), 63));
    PC_CHECK(pc_bitset_count(a.data(), words) == 3);

    /* iteration yields ascending ids, never beyond the universe */
    std::vector<std::size_t> ids;
    pc_bitset_for_each(a.data(), words, [&](std::size_t id) { ids.push_back(id); });
    PC_CHECK(ids.size() == 3);
    PC_CHECK(ids[0] == 0 && ids[1] == 64 && ids[2] == 129);

    /* set / and / or / andnot */
    std::vector<std::uint64_t> b(words, 0);
    pc_bitset_set(b.data(), 0);
    pc_bitset_set(b.data(), 64);
    pc_bitset_set(b.data(), 100);

    std::vector<std::uint64_t> out(words, 0);
    pc_bitset_and(out.data(), a.data(), b.data(), words);
    PC_CHECK(pc_bitset_count(out.data(), words) == 2); /* 0 and 64 */
    PC_CHECK(pc_bitset_test(out.data(), 0) && pc_bitset_test(out.data(), 64));

    pc_bitset_or(out.data(), a.data(), b.data(), words);
    PC_CHECK(pc_bitset_count(out.data(), words) == 4); /* 0,64,100,129 */

    pc_bitset_andnot(out.data(), a.data(), b.data(), words);
    PC_CHECK(pc_bitset_count(out.data(), words) == 1); /* only 129 */
    PC_CHECK(pc_bitset_test(out.data(), 129));

    /* padding bits above bit_count are zeroed */
    std::vector<std::uint64_t> full(words, ~std::uint64_t{0});
    pc_bitset_zero_padding(full.data(), bits);
    PC_CHECK(pc_bitset_count(full.data(), words) == bits);
    /* a bit just above the universe must be gone */
    PC_CHECK(!pc_bitset_test(full.data(), bits)); /* bit 130 */
    PC_CHECK(pc_bitset_test(full.data(), bits - 1));
}
