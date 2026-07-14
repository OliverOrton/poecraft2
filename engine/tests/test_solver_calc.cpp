#include "tests.hpp"

#include "../src/solver_internal.hpp"
#include "poecraft/bitset.h"
#include "poecraft/item_state.h"

#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using namespace poecraft;
using namespace poecraft::solver;

namespace {

constexpr std::uint32_t kTagFire = 3;

/*
 * Same eight-mod white-box universe as test_solver_abstract.cpp, plus base
 * signature weights so weighted pools build. Spawn weights are 100 for every
 * mod except mod 7 (speed suffix) at 400, making the hand-computed
 * distributions non-uniform:
 *   0 prefix life T1 {10} fam100      1 prefix life T2 {10} fam100
 *   2 prefix hybrid  {10,11} fam101   3 prefix attack {12}
 *   4 prefix caster  {13}             5 suffix fire res {20}
 *   6 suffix cold res {21}            7 suffix speed {22} weight 400
 */
std::shared_ptr<SessionImpl> make_calc_session() {
    auto data = std::make_shared<DataImpl>();
    data->mod_global_ids = {0, 1, 2, 3, 4, 5, 6, 7};

    auto session = std::make_shared<SessionImpl>();
    session->data = data;
    session->mod_count = 8;
    session->words = pc_bitset_words(8);
    session->global_index = {0, 1, 2, 3, 4, 5, 6, 7};
    session->gen_type = {0, 0, 0, 0, 0, 1, 1, 1};
    session->primary_group = {10, 10, 10, 12, 13, 20, 21, 22};
    session->required_level = {1, 1, 1, 1, 1, 1, 1, 1};
    session->group_offsets = {0, 1, 2, 4, 5, 6, 7, 8, 9};
    session->group_ids = {10, 10, 10, 11, 12, 13, 20, 21, 22};
    session->family_id = {100, 100, 101, 102, 103, 104, 105, 106};
    session->family_tier_index = {1, 2, 1, 1, 1, 1, 1, 1};
    session->metamod_type.assign(8, -1);
    session->special_kind.assign(8, -1);
    session->flags.assign(8, 0);
    session->influence_code.assign(8, -1);
    session->class_offsets = {0, 0, 0, 0, 1, 2, 4, 6, 7};
    session->class_tag_ids = {1, 2, 3, 6, 4, 6, 5};
    session->rare_affix_cap = 3;
    session->base_spawn_weight = {100, 100, 100, 100, 100, 100, 100, 400};
    session->base_gen_pct.assign(8, 100);
    session->base_roll_weight = session->base_spawn_weight;

    const std::size_t words = session->words;
    session->normal_random_roll_mask.assign(words, 0);
    session->positive_spawn_weight_mask.assign(words, 0);
    session->positive_base_weight_mask.assign(words, 0);
    session->prefix_mask.assign(words, 0);
    session->suffix_mask.assign(words, 0);
    session->unveiled_mask.assign(words, 0);
    session->implicit_tag_masks.assign(7, {});
    session->group_masks.assign(23, {});
    session->influence_masks.assign(1, std::vector<std::uint64_t>(words, 0));
    for (std::uint32_t mod = 0; mod < 8; ++mod) {
        pc_bitset_set(session->normal_random_roll_mask.data(), mod);
        pc_bitset_set(session->positive_spawn_weight_mask.data(), mod);
        pc_bitset_set(session->positive_base_weight_mask.data(), mod);
        pc_bitset_set(session->influence_masks[0].data(), mod);
        pc_bitset_set((mod < 5 ? session->prefix_mask : session->suffix_mask)
                          .data(),
                      mod);
        const std::uint32_t begin = session->group_offsets[mod];
        const std::uint32_t end = session->group_offsets[mod + 1];
        for (std::uint32_t i = begin; i < end; ++i) {
            auto& mask = session->group_masks[session->group_ids[i]];
            if (mask.empty()) mask.assign(words, 0);
            pc_bitset_set(mask.data(), mod);
        }
        const std::uint32_t class_begin = session->class_offsets[mod];
        const std::uint32_t class_end = session->class_offsets[mod + 1];
        for (std::uint32_t i = class_begin; i < class_end; ++i) {
            auto& mask = session->implicit_tag_masks[
                session->class_tag_ids[i]];
            if (mask.empty()) mask.assign(words, 0);
            pc_bitset_set(mask.data(), mod);
        }
    }
    return session;
}

GoalSpec family_goal_100() {
    GoalSpec goal;
    GoalSlot slot;
    slot.family_id = 100;
    slot.min_tier = 1;
    goal.slots.push_back(slot);
    return goal;
}

std::vector<std::uint32_t> basic_indices(const ActionRegistry& registry) {
    std::vector<std::uint32_t> indices;
    for (const char* id : {"transmute", "augment", "alteration", "regal",
                           "alchemy", "chaos", "exalt", "annul", "scour"}) {
        indices.push_back(registry.index_by_id.at(id));
    }
    return indices;
}

void place(pc_item_state* item, int side, std::uint32_t mod_id,
           std::uint16_t group) {
    pc_item_add_mod(item, side, mod_id, group, 0, nullptr);
}

bool sums_to_one(const OutcomeDistribution& distribution) {
    double total = 0.0;
    for (const OutcomeEntry& entry : distribution.entries) {
        total += entry.probability;
    }
    return std::fabs(total - 1.0) < 1e-9;
}

double probability_of(
    const OutcomeDistribution& distribution,
    std::uint32_t state_id) {
    for (const OutcomeEntry& entry : distribution.entries) {
        if (entry.state == state_id) return entry.probability;
    }
    return 0.0;
}

bool near(double a, double b, double tolerance = 1e-9) {
    return std::fabs(a - b) < tolerance;
}

void run_goal_threshold_tests() {
    auto session = make_calc_session();
    ActionRegistry registry = build_action_registry(*session);
    GoalSpec goal;
    GoalSlot prefix;
    prefix.family_id = 100;
    prefix.min_tier = 1;
    GoalSlot suffix;
    suffix.family_id = 104;
    goal.slots = {prefix, suffix};
    goal.rarity = PC_RARITY_RARE;
    goal.min_satisfied_slots = 1;
    CalcContext calc(session, goal, registry, basic_indices(registry));

    AbstractState state;
    state.rarity = PC_RARITY_RARE;
    state.slot_status[0] =
        static_cast<std::uint8_t>(GoalSlotStatus::Satisfied);
    PC_CHECK(calc.is_goal_state(state));
    state.slot_status[0] =
        static_cast<std::uint8_t>(GoalSlotStatus::Absent);
    PC_CHECK(!calc.is_goal_state(state));
    state.slot_status[1] =
        static_cast<std::uint8_t>(GoalSlotStatus::Satisfied);
    PC_CHECK(calc.is_goal_state(state));
    state.rarity = PC_RARITY_MAGIC;
    PC_CHECK(!calc.is_goal_state(state));

    /* Direct GoalSpec callers retain the old all-slots default. */
    goal.min_satisfied_slots = 0;
    CalcContext all_calc(session, goal, registry, basic_indices(registry));
    state.rarity = PC_RARITY_RARE;
    PC_CHECK(!all_calc.is_goal_state(state));
    state.slot_status[0] =
        static_cast<std::uint8_t>(GoalSlotStatus::Satisfied);
    PC_CHECK(all_calc.is_goal_state(state));
}

void run_exact_distribution_tests() {
    auto session = make_calc_session();
    ActionRegistry registry = build_action_registry(*session);
    const std::vector<std::uint32_t> basics = basic_indices(registry);
    CalcContext calc(session, family_goal_100(), registry, basics);
    const std::uint32_t exalt = registry.index_by_id.at("exalt");
    const std::uint32_t augment = registry.index_by_id.at("augment");
    const std::uint32_t regal = registry.index_by_id.at("regal");
    const std::uint32_t annul = registry.index_by_id.at("annul");
    const std::uint32_t scour = registry.index_by_id.at("scour");
    const std::uint32_t chaos = registry.index_by_id.at("chaos");
    const std::uint32_t restart = registry.index_by_id.at("restart");

    /* Exalt from a rare with only the satisfied goal mod. Group 10 blocks
     * mods 0/1/2, so the pool is {3,4 | 5,6,7} with weights {100,100 |
     * 100,100,400}: prefix junk 200/800, suffix junk 600/800. */
    {
        pc_item_state item;
        pc_item_clear(&item);
        item.rarity = PC_RARITY_RARE;
        place(&item, PC_SIDE_PREFIX, 0, 10);
        const std::uint32_t start = calc.intern_item(item);
        const OutcomeDistribution& dist = calc.outcomes(start, exalt);
        PC_CHECK(dist.supported);
        PC_CHECK(dist.entries.size() == 2);
        PC_CHECK(sums_to_one(dist));
        PC_CHECK(near(dist.slot_satisfied_probability[0], 1.0));
        pc_item_state with_prefix_junk = item;
        place(&with_prefix_junk, PC_SIDE_PREFIX, 3, 12);
        pc_item_state with_suffix_junk = item;
        place(&with_suffix_junk, PC_SIDE_SUFFIX, 7, 22);
        PC_CHECK(near(probability_of(
                          dist, calc.intern_item(with_prefix_junk)),
                      200.0 / 800.0));
        PC_CHECK(near(probability_of(
                          dist, calc.intern_item(with_suffix_junk)),
                      600.0 / 800.0));

        /* The distribution cache returns the identical object. */
        PC_CHECK(&calc.outcomes(start, exalt) == &dist);
    }

    /* Exalt from an empty rare: every mod is reachable. Successors group as
     * satisfied {0}, below-tier {1}, blocking junk {2}, prefix junk {3,4},
     * suffix junk {5,6,7} over total weight 1100. */
    {
        pc_item_state item;
        pc_item_clear(&item);
        item.rarity = PC_RARITY_RARE;
        const std::uint32_t start = calc.intern_item(item);
        const OutcomeDistribution& dist = calc.outcomes(start, exalt);
        PC_CHECK(dist.supported);
        PC_CHECK(dist.entries.size() == 5);
        PC_CHECK(sums_to_one(dist));
        PC_CHECK(near(dist.slot_satisfied_probability[0], 100.0 / 1100.0));
        pc_item_state satisfied = item;
        place(&satisfied, PC_SIDE_PREFIX, 0, 10);
        pc_item_state below = item;
        place(&below, PC_SIDE_PREFIX, 1, 10);
        pc_item_state blocked = item;
        place(&blocked, PC_SIDE_PREFIX, 2, 10);
        pc_item_state suffix_junk = item;
        place(&suffix_junk, PC_SIDE_SUFFIX, 5, 20);
        PC_CHECK(near(probability_of(dist, calc.intern_item(satisfied)),
                      100.0 / 1100.0));
        PC_CHECK(near(probability_of(dist, calc.intern_item(below)),
                      100.0 / 1100.0));
        PC_CHECK(near(probability_of(dist, calc.intern_item(blocked)),
                      100.0 / 1100.0));
        PC_CHECK(near(probability_of(dist, calc.intern_item(suffix_junk)),
                      600.0 / 1100.0));
    }

    /* Augment on a magic item with one prefix: only the suffix side is
     * open, and all suffix candidates share one junk class. */
    {
        pc_item_state item;
        pc_item_clear(&item);
        item.rarity = PC_RARITY_MAGIC;
        place(&item, PC_SIDE_PREFIX, 1, 10);
        const std::uint32_t start = calc.intern_item(item);
        const OutcomeDistribution& dist = calc.outcomes(start, augment);
        PC_CHECK(dist.supported);
        PC_CHECK(dist.entries.size() == 1);
        PC_CHECK(sums_to_one(dist));
        pc_item_state with_suffix = item;
        place(&with_suffix, PC_SIDE_SUFFIX, 6, 21);
        PC_CHECK(near(probability_of(dist, calc.intern_item(with_suffix)),
                      1.0));
    }

    /* Regal from the below-tier magic item: rarity upgrades in every
     * successor, and the added mod avoids the occupied group 10. */
    {
        pc_item_state item;
        pc_item_clear(&item);
        item.rarity = PC_RARITY_MAGIC;
        place(&item, PC_SIDE_PREFIX, 1, 10);
        const std::uint32_t start = calc.intern_item(item);
        const OutcomeDistribution& dist = calc.outcomes(start, regal);
        PC_CHECK(dist.supported);
        PC_CHECK(dist.entries.size() == 2);
        PC_CHECK(sums_to_one(dist));
        for (const OutcomeEntry& entry : dist.entries) {
            PC_CHECK(calc.state(entry.state).rarity == PC_RARITY_RARE);
        }
        pc_item_state rare_prefix_junk = item;
        rare_prefix_junk.rarity = PC_RARITY_RARE;
        place(&rare_prefix_junk, PC_SIDE_PREFIX, 4, 13);
        PC_CHECK(near(probability_of(
                          dist, calc.intern_item(rare_prefix_junk)),
                      200.0 / 800.0));
    }

    /* Annul removes uniformly among the three unfractured mods. */
    {
        pc_item_state item;
        pc_item_clear(&item);
        item.rarity = PC_RARITY_RARE;
        place(&item, PC_SIDE_PREFIX, 0, 10);
        place(&item, PC_SIDE_PREFIX, 3, 12);
        place(&item, PC_SIDE_SUFFIX, 5, 20);
        const std::uint32_t start = calc.intern_item(item);
        const OutcomeDistribution& dist = calc.outcomes(start, annul);
        PC_CHECK(dist.supported);
        PC_CHECK(dist.entries.size() == 3);
        PC_CHECK(sums_to_one(dist));
        for (const OutcomeEntry& entry : dist.entries) {
            PC_CHECK(near(entry.probability, 1.0 / 3.0));
        }
        PC_CHECK(near(dist.slot_satisfied_probability[0], 2.0 / 3.0));
    }

    /* Scour with nothing locked or fractured is deterministic: an empty
     * normal item, the same successor restart produces. */
    {
        pc_item_state item;
        pc_item_clear(&item);
        item.rarity = PC_RARITY_RARE;
        place(&item, PC_SIDE_PREFIX, 0, 10);
        place(&item, PC_SIDE_SUFFIX, 7, 22);
        const std::uint32_t start = calc.intern_item(item);
        const OutcomeDistribution& scoured = calc.outcomes(start, scour);
        PC_CHECK(scoured.supported);
        PC_CHECK(scoured.entries.size() == 1);
        const OutcomeDistribution& restarted = calc.outcomes(start, restart);
        PC_CHECK(restarted.supported);
        PC_CHECK(restarted.entries.size() == 1);
        PC_CHECK(scoured.entries[0].state == restarted.entries[0].state);
        const AbstractState& fresh = calc.state(scoured.entries[0].state);
        PC_CHECK(fresh.rarity == PC_RARITY_NORMAL);
        PC_CHECK(fresh.prefix_count == 0 && fresh.suffix_count == 0);
    }

    /* Illegal action: exalt on a full rare self-loops (engine leaves the
     * item unchanged). Reforge evaluators are S3: chaos is unsupported. */
    {
        pc_item_state item;
        pc_item_clear(&item);
        item.rarity = PC_RARITY_RARE;
        place(&item, PC_SIDE_PREFIX, 0, 10);
        place(&item, PC_SIDE_PREFIX, 3, 12);
        place(&item, PC_SIDE_PREFIX, 4, 13);
        place(&item, PC_SIDE_SUFFIX, 5, 20);
        place(&item, PC_SIDE_SUFFIX, 6, 21);
        place(&item, PC_SIDE_SUFFIX, 7, 22);
        const std::uint32_t start = calc.intern_item(item);
        const OutcomeDistribution& full = calc.outcomes(start, exalt);
        PC_CHECK(full.supported);
        PC_CHECK(full.entries.size() == 1);
        PC_CHECK(near(probability_of(full, start), 1.0));
        /* Chaos is a reforge: supported since S3, and it always rerolls
         * the full item even from this dead end. */
        const OutcomeDistribution& rerolled = calc.outcomes(start, chaos);
        PC_CHECK(rerolled.supported);
        PC_CHECK(sums_to_one(rerolled));
    }

    /* Every state reached above materializes back to itself. */
    {
        std::size_t verified = 0;
        pc_item_state scratch;
        for (std::uint32_t id = 0; id < calc.state_count(); ++id) {
            if (calc.materialize(id, scratch)) ++verified;
        }
        PC_CHECK(verified == calc.state_count());
    }
}

bool read_text_file(const std::string& path, std::string& out) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) return false;
    std::ostringstream buffer;
    buffer << stream.rdbuf();
    out = buffer.str();
    return true;
}

/*
 * Monte Carlo cross-check on the real artifact: sample the engine action
 * from the state's own representative item and compare the histogram of
 * projected successors against the exact distribution.
 */
void mc_cross_check(
    CalcContext& calc,
    ActionContextImpl& mc,
    std::uint32_t state_id,
    std::uint32_t action_index,
    std::uint32_t samples,
    double slack = 1e-3,
    double coverage_tolerance = 1e-9) {
    const ActionDescriptor& action =
        calc.registry().actions.at(action_index);
    const OutcomeDistribution& exact = calc.outcomes(state_id, action_index);
    PC_CHECK(exact.supported);
    PC_CHECK(sums_to_one(exact));
    pc_item_state representative;
    PC_CHECK(calc.materialize(state_id, representative));

    std::map<std::uint32_t, std::uint32_t> histogram;
    for (std::uint32_t i = 0; i < samples; ++i) {
        pc_item_state copy = representative;
        const ActionOutcome outcome = apply_action(mc, &copy, action.params);
        /* Internal apply_action may leave a partial mutation on failure;
         * the C ABI (and the evaluator) treat that as unchanged. */
        ++histogram[calc.intern_item(outcome.applied ? copy
                                                     : representative)];
    }
    double total_checked = 0.0;
    for (const OutcomeEntry& entry : exact.entries) {
        const auto it = histogram.find(entry.state);
        const double observed =
            it == histogram.end()
                ? 0.0
                : static_cast<double>(it->second) / samples;
        const double sigma = std::sqrt(
            entry.probability * (1.0 - entry.probability) / samples);
        PC_CHECK(std::fabs(observed - entry.probability) <
                 5.0 * sigma + slack);
        total_checked += observed;
    }
    /* Sampled successors outside the exact support are bounded by the
     * evaluator's truncation budget. */
    PC_CHECK(total_checked > 1.0 - coverage_tolerance - 1e-9);
}

/*
 * S3 reforge DP on the synthetic session: hand-computed sequential-roll
 * probabilities (exact group removal between picks) plus an MC gate.
 */
void run_reforge_tests() {
    auto session = make_calc_session();
    ActionRegistry registry = build_action_registry(*session);
    CalcContext calc(session, family_goal_100(), registry,
                     basic_indices(registry));
    ActionContextImpl mc(777);
    mc.session = session;
    const std::uint32_t transmute = registry.index_by_id.at("transmute");
    const std::uint32_t alteration = registry.index_by_id.at("alteration");
    const std::uint32_t chaos = registry.index_by_id.at("chaos");

    pc_item_state normal;
    pc_item_clear(&normal);
    const std::uint32_t start = calc.intern_item(normal);
    const OutcomeDistribution& roll = calc.outcomes(start, transmute);
    PC_CHECK(roll.supported);
    PC_CHECK(sums_to_one(roll));
    for (const OutcomeEntry& entry : roll.entries) {
        const AbstractState& successor = calc.state(entry.state);
        PC_CHECK(successor.rarity == PC_RARITY_MAGIC);
        const int total = successor.prefix_count + successor.suffix_count;
        PC_CHECK(total == 1 || total == 2);
    }

    /* Target 1 (p 1/2): P(goal mod) = 100/1100. Target 2 never leaves a
     * lone goal mod (the pool cannot dead-end after one pick). */
    pc_item_state sat_item;
    pc_item_clear(&sat_item);
    sat_item.rarity = PC_RARITY_MAGIC;
    place(&sat_item, PC_SIDE_PREFIX, 0, 10);
    PC_CHECK(near(probability_of(roll, calc.intern_item(sat_item)),
                  0.5 * (100.0 / 1100.0)));

    /* Goal + one suffix junk requires target 2. Magic caps one mod per
     * side, so the second pick always comes from the other side:
     *   goal then any suffix junk: (100/1100) * 1
     *   any suffix junk then goal: (600/1100) * (100/500)
     * (after a suffix pick the open prefix pool is goal 100 + below 100 +
     *  blocker 100 + prefix junk 200 = 500).                          */
    {
        pc_item_state sat_junk = sat_item;
        place(&sat_junk, PC_SIDE_SUFFIX, 7, 22);
        const double expected =
            0.5 * ((100.0 / 1100.0) +
                   (600.0 / 1100.0) * (100.0 / 500.0));
        PC_CHECK(near(probability_of(roll, calc.intern_item(sat_junk)),
                      expected));
    }

    /* Slot hit probability across both targets: target 2 hits the goal
     * first pick, or via any suffix junk first; a prefix junk, below-tier,
     * or blocker first pick kills the goal for the roll. */
    {
        const double p_t1 = 100.0 / 1100.0;
        const double p_t2 =
            100.0 / 1100.0 + (600.0 / 1100.0) * (100.0 / 500.0);
        PC_CHECK(near(roll.slot_satisfied_probability[0],
                      0.5 * p_t1 + 0.5 * p_t2));
    }

    /* Alteration from a magic item with no fractured slots rerolls from
     * the same empty base: its distribution is transmute's exactly. */
    {
        const OutcomeDistribution& again =
            calc.outcomes(calc.intern_item(sat_item), alteration);
        PC_CHECK(again.supported);
        PC_CHECK(again.entries.size() == roll.entries.size());
        for (std::size_t i = 0; i < again.entries.size() &&
                                i < roll.entries.size();
             ++i) {
            PC_CHECK(again.entries[i].state == roll.entries[i].state);
            PC_CHECK(near(again.entries[i].probability,
                          roll.entries[i].probability, 1e-12));
        }
    }

    /* Chaos: 4-6 mods against the engine's own sampling. */
    {
        pc_item_state rare;
        pc_item_clear(&rare);
        rare.rarity = PC_RARITY_RARE;
        const std::uint32_t rare_start = calc.intern_item(rare);
        const OutcomeDistribution& dist = calc.outcomes(rare_start, chaos);
        PC_CHECK(dist.supported);
        for (const OutcomeEntry& entry : dist.entries) {
            const AbstractState& successor = calc.state(entry.state);
            const int total =
                successor.prefix_count + successor.suffix_count;
            PC_CHECK(successor.rarity == PC_RARITY_RARE);
            PC_CHECK(total >= 4 && total <= 6);
        }
        mc_cross_check(calc, mc, rare_start, chaos, 50000, 3e-3, 1e-4);
    }

    /* Harvest: the guaranteed tag pick plus normal fills, and the
     * intentional add-then-remove augment. Mod 5 is the only fire mod. */
    {
        ActionRegistry custom;
        ActionDescriptor reforge_fire;
        reforge_fire.id = "harvest_reforge:fire";
        reforge_fire.params.type = ActionType::HarvestReforge;
        reforge_fire.params.target_tag_id = kTagFire;
        reforge_fire.kind = TransitionKind::Reforge;
        reforge_fire.cost_keys = {reforge_fire.id};
        reforge_fire.legality.rarity_mask = 1u << PC_RARITY_RARE;
        reforge_fire.discriminating_tag_ids = {kTagFire};
        custom.index_by_id.emplace(reforge_fire.id, 0);
        custom.actions.push_back(reforge_fire);
        ActionDescriptor augment_fire;
        augment_fire.id = "harvest_augment:fire";
        augment_fire.params.type = ActionType::HarvestAugment;
        augment_fire.params.target_tag_id = kTagFire;
        augment_fire.kind = TransitionKind::Special;
        augment_fire.cost_keys = {augment_fire.id};
        augment_fire.legality.rarity_mask =
            (1u << PC_RARITY_MAGIC) | (1u << PC_RARITY_RARE);
        augment_fire.legality.requires_open_affix = true;
        augment_fire.legality.forbidden_flags |=
            kFlagInfluenced | kFlagEldritchImplicit;
        augment_fire.discriminating_tag_ids = {kTagFire};
        custom.index_by_id.emplace(augment_fire.id, 1);
        custom.actions.push_back(augment_fire);

        CalcContext hcalc(session, family_goal_100(), custom, {});
        ActionContextImpl hmc(1234);
        hmc.session = session;

        /* Augment on a rare with only the goal mod: mod 5 is forced in,
         * then the goal mod is the only removable — one exact outcome. */
        pc_item_state item;
        pc_item_clear(&item);
        item.rarity = PC_RARITY_RARE;
        place(&item, PC_SIDE_PREFIX, 0, 10);
        const std::uint32_t start_state = hcalc.intern_item(item);
        const OutcomeDistribution& augmented =
            hcalc.outcomes(start_state, 1);
        PC_CHECK(augmented.supported);
        PC_CHECK(augmented.entries.size() == 1);
        PC_CHECK(sums_to_one(augmented));
        pc_item_state expected;
        pc_item_clear(&expected);
        expected.rarity = PC_RARITY_RARE;
        place(&expected, PC_SIDE_SUFFIX, 5, 20);
        PC_CHECK(near(probability_of(augmented,
                                     hcalc.intern_item(expected)),
                      1.0));
        mc_cross_check(hcalc, hmc, start_state, 1, 2000);

        /* Reforge always lands at least the fire mod. */
        pc_item_state rare;
        pc_item_clear(&rare);
        rare.rarity = PC_RARITY_RARE;
        const std::uint32_t rare_start = hcalc.intern_item(rare);
        const OutcomeDistribution& reforged =
            hcalc.outcomes(rare_start, 0);
        PC_CHECK(reforged.supported);
        const std::uint32_t fire_class =
            hcalc.layout().junk_class_by_mod[5];
        PC_CHECK(fire_class != kNoId);
        for (const OutcomeEntry& entry : reforged.entries) {
            PC_CHECK(hcalc.state(entry.state).junk_counts[fire_class] >=
                     1);
        }
        mc_cross_check(hcalc, hmc, rare_start, 0, 30000, 3e-3, 1e-4);
    }
}

void run_artifact_calc_tests(const char* artifact_dir) {
    if (artifact_dir == nullptr) {
        std::printf("solver calc artifact suite skipped (missing path)\n");
        return;
    }
    const std::string dir = artifact_dir;
    std::string manifest_text;
    std::string strings_text;
    std::string game_text;
    if (!read_text_file(dir + "/manifest.json", manifest_text) ||
        !read_text_file(dir + "/strings.json", strings_text) ||
        !read_text_file(dir + "/game-data.json", game_text)) {
        std::printf("solver calc artifact suite skipped (unreadable)\n");
        return;
    }
    std::shared_ptr<DataImpl> data;
    std::shared_ptr<SessionImpl> session;
    try {
        data = load_data_impl(manifest_text, strings_text, game_text);
        const auto base = data->base_by_path.find(
            "Metadata/Items/Armours/BodyArmours/BodyInt17");
        PC_CHECK(base != data->base_by_path.end());
        if (base == data->base_by_path.end()) return;
        session = std::make_shared<SessionImpl>();
        session->data = data;
        session->base_index = base->second;
        session->item_level = 86;
        build_session(*session);
    } catch (const std::exception& ex) {
        std::printf("solver calc artifact suite: %s\n", ex.what());
        PC_CHECK(false);
        return;
    }

    ActionRegistry registry = build_action_registry(*session);
    GoalSpec goal;
    /* The goal group must be positively weighted under this base's tag
     * signature, not merely present in the roll mask, or no action can
     * ever hit it. */
    std::uint32_t goal_group = kNoId;
    for (std::uint32_t mod = 0; mod < session->mod_count; ++mod) {
        if (session->gen_type[mod] == 0 &&
            pc_bitset_test(session->normal_random_roll_mask.data(), mod) &&
            pc_bitset_test(session->positive_base_weight_mask.data(), mod)) {
            goal_group = session->primary_group[mod];
            break;
        }
    }
    GoalSlot slot;
    slot.group_id = goal_group;
    goal.slots.push_back(slot);
    CalcContext calc(session, goal, registry, {});
    ActionContextImpl mc(424242);
    mc.session = session;

    pc_item_state empty_rare;
    pc_item_clear(&empty_rare);
    empty_rare.rarity = PC_RARITY_RARE;
    const std::uint32_t start = calc.intern_item(empty_rare);

    /* Exalt from the empty rare: the exact successor classes and their
     * pool-sum probabilities must match the engine's own sampling. */
    const std::uint32_t exalt = registry.index_by_id.at("exalt");
    mc_cross_check(calc, mc, start, exalt, 20000);

    /* Walk to a mid-craft state (highest-probability exalt successor,
     * twice) and cross-check exalt and annul from there. */
    std::uint32_t mid = start;
    for (int step = 0; step < 2; ++step) {
        const OutcomeDistribution& dist = calc.outcomes(mid, exalt);
        PC_CHECK(dist.supported);
        const OutcomeEntry* best = nullptr;
        for (const OutcomeEntry& entry : dist.entries) {
            if (best == nullptr || entry.probability > best->probability) {
                best = &entry;
            }
        }
        PC_CHECK(best != nullptr);
        if (best == nullptr) return;
        mid = best->state;
    }
    mc_cross_check(calc, mc, mid, exalt, 20000);
    const std::uint32_t annul = registry.index_by_id.at("annul");
    mc_cross_check(calc, mc, mid, annul, 20000);

    /* A plain (non-metamod) bench craft is deterministic and marks the
     * crafted flag in the successor. */
    for (const ActionDescriptor& action : registry.actions) {
        if (action.params.type != ActionType::Bench ||
            action.sets_flags != kFlagCraftedMod) {
            continue;
        }
        const std::uint32_t index = registry.index_by_id.at(action.id);
        const OutcomeDistribution& dist = calc.outcomes(start, index);
        PC_CHECK(dist.supported);
        PC_CHECK(dist.entries.size() == 1);
        const AbstractState& successor =
            calc.state(dist.entries[0].state);
        if (dist.entries[0].state != start) {
            PC_CHECK(successor.flags & kFlagCraftedMod);
            PC_CHECK(successor.prefix_count + successor.suffix_count == 1);
        }
        break;
    }

    std::printf("solver calc artifact: %u states interned\n",
                calc.state_count());

    /* --- S3 gate: chaos, essence, fossil on the Vaal Regalia fixture ------
     * A focused candidate set keeps the junk classes coarse, matching how
     * a pruned solve would see these actions. */
    {
        std::vector<std::uint32_t> candidates = basic_indices(registry);
        std::uint32_t essence_index = kNoId;
        std::uint32_t fossil_index = kNoId;
        for (std::uint32_t i = 0;
             i < static_cast<std::uint32_t>(registry.actions.size()); ++i) {
            const ActionDescriptor& action = registry.actions[i];
            if (essence_index == kNoId &&
                action.params.type == ActionType::Essence) {
                essence_index = i;
            }
            if (fossil_index == kNoId &&
                action.params.type == ActionType::Fossil &&
                action.params.fossil_indices.size() == 1 &&
                !action.discriminating_tag_ids.empty()) {
                fossil_index = i;
            }
        }
        PC_CHECK(essence_index != kNoId);
        PC_CHECK(fossil_index != kNoId);
        if (essence_index == kNoId || fossil_index == kNoId) return;
        candidates.push_back(essence_index);
        candidates.push_back(fossil_index);

        /* Harvest actions for a tag with positively-weighted rollable
         * members, so the guaranteed pool is non-empty. */
        std::uint32_t harvest_tag = kNoId;
        for (std::uint32_t tag = 0;
             tag < static_cast<std::uint32_t>(
                       session->implicit_tag_masks.size()) &&
             harvest_tag == kNoId;
             ++tag) {
            const auto& mask = session->implicit_tag_masks[tag];
            if (mask.empty() || data->tag_name_by_id.count(tag) == 0) {
                continue;
            }
            bool viable = false;
            pc_bitset_for_each(
                mask.data(), session->words, [&](std::size_t bit) {
                    if (pc_bitset_test(
                            session->normal_random_roll_mask.data(), bit) &&
                        pc_bitset_test(
                            session->positive_spawn_weight_mask.data(),
                            bit)) {
                        viable = true;
                    }
                });
            if (viable) harvest_tag = tag;
        }
        PC_CHECK(harvest_tag != kNoId);
        if (harvest_tag == kNoId) return;
        const std::string& tag_name = data->tag_name_by_id.at(harvest_tag);
        const std::uint32_t harvest_reforge_index =
            registry.index_by_id.at("harvest_reforge:" + tag_name);
        const std::uint32_t harvest_augment_index =
            registry.index_by_id.at("harvest_augment:" + tag_name);
        candidates.push_back(harvest_reforge_index);
        candidates.push_back(harvest_augment_index);

        CalcContext reforge_calc(session, goal, registry, candidates);
        ActionContextImpl reforge_mc(31337);
        reforge_mc.session = session;
        const std::uint32_t reforge_start =
            reforge_calc.intern_item(empty_rare);
        /* The goal slot must appear in the chaos successor support with
         * its exact pool-share probability, however small. */
        const OutcomeDistribution& chaos_dist = reforge_calc.outcomes(
            reforge_start, registry.index_by_id.at("chaos"));
        PC_CHECK(chaos_dist.supported);
        PC_CHECK(chaos_dist.slot_satisfied_probability[0] > 0.0);
        std::printf("solver reforge artifact: chaos goal-hit p=%.6f over "
                    "%zu outcomes\n",
                    chaos_dist.slot_satisfied_probability[0],
                    chaos_dist.entries.size());
        mc_cross_check(reforge_calc, reforge_mc, reforge_start,
                       registry.index_by_id.at("chaos"), 20000, 4e-3, 5e-3);
        mc_cross_check(reforge_calc, reforge_mc, reforge_start,
                       essence_index, 20000, 4e-3, 5e-3);
        mc_cross_check(reforge_calc, reforge_mc, reforge_start,
                       fossil_index, 20000, 4e-3, 5e-3);
        mc_cross_check(reforge_calc, reforge_mc, reforge_start,
                       harvest_reforge_index, 20000, 4e-3, 5e-3);

        /* Harvest augment from a mid-craft rare with an open affix, so
         * the removal stage actually runs. */
        std::uint32_t augment_state = kNoId;
        for (const OutcomeEntry& entry : chaos_dist.entries) {
            const AbstractState& successor =
                reforge_calc.state(entry.state);
            if (successor.prefix_count + successor.suffix_count < 6) {
                augment_state = entry.state;
                break;
            }
        }
        PC_CHECK(augment_state != kNoId);
        if (augment_state != kNoId) {
            mc_cross_check(reforge_calc, reforge_mc, augment_state,
                           harvest_augment_index, 20000, 4e-3, 5e-3);
        }
        std::printf(
            "solver reforge artifact: %u states after "
            "chaos/essence/fossil/harvest\n",
            reforge_calc.state_count());
    }
}

} // namespace

void run_solver_calc_tests(const char* artifact_dir) {
    run_goal_threshold_tests();
    run_exact_distribution_tests();
    run_reforge_tests();
    run_artifact_calc_tests(artifact_dir);
}
