#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <stdexcept>

namespace poecraft {

// Version the mechanic independently of compiled game data and evaluator shape.
inline constexpr std::uint64_t kRareReforgeCountLawVersion = 3;

enum class RareReforgeCountKind : std::uint8_t {
    Equipment = 0,
    LegacyJewel = 1, // Preserve the unresolved ordinary/Abyss jewel behavior.
    ClusterJewel = 2 // Configured cluster sessions select this in their owner.
};

struct RareReforgeCountLaw {
    struct Draw { int count; std::uint32_t weight; };
    std::array<Draw, 3> draws;
    std::uint32_t denominator;

    int select(std::uint64_t draw) const {
        for (const auto& entry : draws) {
            if (draw < entry.weight) return entry.count;
            draw -= entry.weight;
        }
        throw std::out_of_range("rare reforge count draw outside its law");
    }
};

inline constexpr RareReforgeCountLaw rare_reforge_count_law(
        const RareReforgeCountKind kind) {
    switch (kind) {
    case RareReforgeCountKind::Equipment:
        return {{{{4, 8}, {5, 3}, {6, 1}}}, 12};
    case RareReforgeCountKind::LegacyJewel:
        return {{{{4, 1}, {5, 1}, {6, 1}}}, 3};
    case RareReforgeCountKind::ClusterJewel:
        return {{{{3, 65}, {4, 35}, {0, 0}}}, 100};
    }
    throw std::invalid_argument("unknown rare reforge count kind");
}

// Owner-approved working model for a single-side rare reforge: the draw is
// the ordinary total, preserved modifiers count toward it, and only the
// selected side can fill. Capacity/existing fractures coalesce draw mass;
// they never condition or redraw the count. This is not a verified game rate.
inline constexpr int single_side_rare_reforge_target_total(
        const int drawn_total, const int opposite_count,
        const int preserved_side_count, const int side_capacity) {
    const int preserved_total = opposite_count + preserved_side_count;
    return std::max(preserved_total,
                    std::min(drawn_total, opposite_count + side_capacity));
}

} // namespace poecraft
