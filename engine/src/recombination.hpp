#pragma once
#include "multi_item.hpp"
#include "poecraft/session.h"
#include <array>

namespace poecraft {
inline constexpr char kRandomRecombModel[] = "poe1-random-spawn-proxy-preserve-tier-roll-no-upgrade-v1";
inline constexpr char kRandomRecombExtendedModel[] = "poe1-random-spawn-proxy-native-constraints-preserve-tier-roll-no-upgrade-v2";
inline constexpr char kRandomRecombProjection[] = "structural-output-preserve-tier-roll-v1";

// Exact thousandths of the adopted estimated coefficients, never game-exact.
std::string random_recomb_item_json(const pc_item_state&, const SessionImpl&);
const std::array<unsigned, 4>& random_recomb_count_row(unsigned physical_count);
std::uint32_t random_recomb_item_level(std::uint32_t a, std::uint32_t b);

struct RecombOccurrence {
    std::uint8_t input = 0, slot = 0;
    std::uint32_t global_mod_id = PC_MOD_NONE;
    std::uint32_t output_mod_id = PC_MOD_NONE;
    std::uint32_t spawn_weight = 0; // zero means ineligible on this carrier
    std::vector<std::uint32_t> groups; // full canonical membership
    bool exclusive = false; // known output constraint; never an inferred count/order law
};
struct RecombSideOutcome {
    double probability = 0;
    unsigned requested_count = 0;
    std::vector<unsigned> occurrences; // indices into the physical side pool
};
std::vector<RecombSideOutcome> enumerate_random_recomb_side(
    const std::vector<RecombOccurrence>& physical_pool);
RecombSideOutcome sample_random_recomb_side(Rng& rng,
    const std::vector<RecombOccurrence>& physical_pool);

struct RandomRecombCarrier {
    std::shared_ptr<const SessionImpl> output_session;
    // Carrier-owned properties only: affix arrays are empty. Full outputs
    // materialize selected occurrences separately under the approved tier/roll
    // preservation approximation; upgrades are omitted and game odds estimated.
    pc_item_state properties{};
    std::array<std::vector<RecombOccurrence>, 2> sides;
};
struct RandomRecombPair {
    std::uint32_t version = 1;
    std::string model_id = kRandomRecombModel;
    std::array<CraftResource, 2> inputs;
    std::array<RandomRecombCarrier, 2> carriers;
    std::array<std::string, 4> data_identity;
    bool game_odds_estimated = true;
    bool full_item_apply_supported = true;
    bool gold_cost_complete = false, dust_cost_complete = false;
};
struct RandomRecombOutcome {
    double probability = 0;
    unsigned carrier = 0;
    RecombSideOutcome prefixes, suffixes;
};
// Read-only preparation/enumeration. No acquisition, RNG draw, receiver-only
// currencyCalc mapping or solver admission. Rolled-total/defence goals are out
// of scope even though recorded numeric rolls are preserved by Apply.
RandomRecombPair prepare_random_recomb_pair(const CraftResource& a, const CraftResource& b);
std::vector<RandomRecombOutcome> enumerate_random_recomb_pair(const RandomRecombPair& pair);
RandomRecombOutcome sample_random_recomb_selection(Rng& rng, const RandomRecombPair& pair);
pc_item_state materialize_random_recomb_outcome(const RandomRecombPair& pair,
    const RandomRecombOutcome& outcome);
CraftTransaction prepare_random_recomb_transaction(ActionContextImpl& context,
    const RandomRecombPair& pair, const std::string& output_identity);
/* Inventory commit and sampling form one atomic operation. Stale, replay and
 * output-identity collision failures restore RNG and every resource. */
CraftTransaction apply_random_recomb_transaction(ActionContextImpl& context,
    std::vector<CraftResource>& resources, const RandomRecombPair& pair,
    const std::string& output_identity);
}
