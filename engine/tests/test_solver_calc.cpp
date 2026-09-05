#include "tests.hpp"

#include "../src/json.hpp"
#include "../src/solver_action_family_contract.hpp"
#include "../src/solver_internal.hpp"
#include "../src/solver_solve_types.hpp"
#include "../src/solver_phase_lower.hpp"
#include "poecraft/bitset.h"
#include "poecraft/item_state.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <sstream>
#include <set>
#include <string>
#include <tuple>
#include <vector>

using namespace poecraft;
using namespace poecraft::solver;

namespace {

constexpr std::uint32_t kTagFire = 3;
constexpr std::uint32_t kTagCold = 4;
constexpr std::uint32_t kTagResistance = 6;

/*
 * Same eight ordinary mods as test_solver_abstract.cpp, plus dedicated
 * prefix/suffix veiled placeholders and base signature weights so weighted
 * pools build. Spawn weights are 100 for every mod except mod 7 (speed suffix)
 * at 400, making the hand-computed distributions non-uniform:
 *   0 prefix life T1 {10} fam100      1 prefix life T2 {10} fam100
 *   2 prefix hybrid  {10,11} fam101   3 prefix attack {12}
 *   4 prefix caster  {13}             5 suffix fire res {20}
 *   6 suffix cold res {21}            7 suffix speed {22} weight 400
 *   8 prefix veiled placeholder {30}  9 suffix veiled placeholder {31}
 */
std::shared_ptr<SessionImpl> make_calc_session() {
    auto data = std::make_shared<DataImpl>();
    data->mod_global_ids = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    data->spawn_offsets = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    data->spawn_tag_ids.assign(10, 0);
    data->spawn_weights = {
        100, 100, 100, 100, 100, 100, 100, 400, 100, 100};
    data->gen_offsets =
        {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    data->gen_tag_ids.assign(10, 0);
    data->gen_weights.assign(10, 100);
    data->mod_gen_type_code.assign(10, 0);
    data->tag_id_by_name = {
        {"fire", kTagFire},
        {"cold", kTagCold},
        {"resistance", kTagResistance},
    };
    data->tag_name_by_id = {
        {kTagFire, "fire"},
        {kTagCold, "cold"},
        {kTagResistance, "resistance"},
    };

    auto session = std::make_shared<SessionImpl>();
    session->data = data;
    session->mod_count = 10;
    session->words = pc_bitset_words(10);
    session->global_index = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9};
    session->gen_type = {0, 0, 0, 0, 0, 1, 1, 1, 0, 1};
    session->primary_group = {10, 10, 10, 12, 13, 20, 21, 22, 30, 31};
    session->required_level.assign(10, 1);
    session->group_offsets = {0, 1, 2, 4, 5, 6, 7, 8, 9, 10, 11};
    session->group_ids = {10, 10, 10, 11, 12, 13, 20, 21, 22, 30, 31};
    session->family_id = {
        100, 100, 101, 102, 103, 104, 105, 106, 107, 108};
    session->family_tier_index.assign(10, 1);
    session->family_tier_index[1] = 2;
    session->metamod_type.assign(10, -1);
    session->special_kind.assign(10, -1);
    session->flags.assign(10, 0);
    session->influence_code.assign(10, -1);
    session->class_offsets = {0, 0, 0, 0, 1, 2, 4, 6, 7, 7, 7};
    session->class_tag_ids = {1, 2, 3, 6, 4, 6, 5};
    session->rare_affix_cap = 3;
    session->base_spawn_weight = {
        100, 100, 100, 100, 100, 100, 100, 400, 100, 100};
    session->base_gen_pct.assign(10, 100);
    session->base_roll_weight = session->base_spawn_weight;
    session->effective_base_tag_ids = {0};

    const std::size_t words = session->words;
    session->normal_random_roll_mask.assign(words, 0);
    session->positive_spawn_weight_mask.assign(words, 0);
    session->positive_base_weight_mask.assign(words, 0);
    session->prefix_mask.assign(words, 0);
    session->suffix_mask.assign(words, 0);
    session->unveiled_mask.assign(words, 0);
    session->unveiled_generic_mask.assign(words, 0);
    session->implicit_tag_masks.assign(7, {});
    session->group_masks.assign(32, {});
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
    for (std::uint32_t mod = 0; mod < 8; ++mod) {
        pc_bitset_set(session->unveiled_mask.data(), mod);
        pc_bitset_set(session->unveiled_generic_mask.data(), mod);
    }
    session->veiled_prefix_mod_id = 8;
    session->veiled_suffix_mod_id = 9;
    session->eldritch_eligible = true;
    session->eldritch_searing_tier_mod_ids.resize(5);
    session->eldritch_eater_tier_mod_ids.resize(5);
    session->eldritch_searing_tier_mod_ids[1] = {0};
    session->eldritch_eater_tier_mod_ids[1] = {5};
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
           std::uint16_t group, std::uint8_t flags = 0) {
    pc_item_add_mod(item, side, mod_id, group, flags, nullptr);
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

bool same_distribution(
    const OutcomeDistribution& left,
    const OutcomeDistribution& right) {
    if (left.supported != right.supported ||
        left.entries.size() != right.entries.size()) {
        return false;
    }
    for (const OutcomeEntry& entry : left.entries) {
        if (!near(
                entry.probability,
                probability_of(right, entry.state))) {
            return false;
        }
    }
    return true;
}

std::map<std::uint32_t, double> project_distribution(
    CalcContext& source,
    CalcContext& semantic,
    const OutcomeDistribution& distribution) {
    std::map<std::uint32_t, double> projected;
    for (const OutcomeEntry& entry : distribution.entries) {
        pc_item_state item;
        PC_CHECK(source.materialize(entry.state, item));
        projected[semantic.intern_item(item)] += entry.probability;
    }
    return projected;
}

using AbstractMassMap = std::unordered_map<
    AbstractState,
    double,
    decltype(&abstract_state_hash)>;

AbstractMassMap abstract_distribution(
    const CalcContext& source,
    const OutcomeDistribution& distribution) {
    AbstractMassMap mass(0, abstract_state_hash);
    for (const OutcomeEntry& entry : distribution.entries) {
        mass[source.state(entry.state)] += entry.probability;
    }
    return mass;
}

bool item_contains_mod(
    const pc_item_state& item,
    const std::uint32_t mod_id) {
    for (std::uint8_t i = 0; i < item.prefix_count; ++i) {
        if (item.prefixes[i].mod_id == mod_id) return true;
    }
    for (std::uint8_t i = 0; i < item.suffix_count; ++i) {
        if (item.suffixes[i].mod_id == mod_id) return true;
    }
    return false;
}

void run_product_dead_feature_reduction_tests() {
    auto session = make_calc_session();
    auto data = std::const_pointer_cast<DataImpl>(session->data);
    data->strings = {
        "ordinary_test", "Ordinary Test Fossil",
        "mirror_test", "Mirror Test Fossil"};
    data->fossil_count = 2;
    data->fossil_key_sids = {0, 2};
    data->fossil_name_sids = {1, 3};
    data->fossil_rolls_lucky = {0, 0};
    data->fossil_mirrors = {0, 1};
    data->fossil_weight_offsets = {0, 0, 0};
    session->fossil_added_mod_ids = {{}, {}};
    session->fossil_forced_mod_ids = {{}, {}};
    session->fossil_sell_price_mod_ids = {{}, {}};

    const ActionRegistry registry = build_action_registry(*session);
    PC_CHECK(registry.index_by_id.contains("fossil:ordinary_test"));
    PC_CHECK(!registry.index_by_id.contains("fossil:mirror_test"));
    PC_CHECK(!registry.index_by_id.contains(
        "fossil:mirror_test+ordinary_test"));

    const GoalSpec goal = family_goal_100();
    const std::vector<std::uint32_t> actions{
        registry.index_by_id.at("transmute"),
        registry.index_by_id.at("alteration"),
        registry.index_by_id.at("restart")};
    CalcContext product(
        session, goal, registry, actions,
        false, false, false, std::nullopt, {}, true);
    CalcContext exact(
        session, goal, registry, actions,
        false, false, false, std::nullopt, {}, false);
    pc_item_state clean;
    pc_item_clear(&clean);
    pc_item_state mirrored = clean;
    mirrored.item_flags = PC_ITEM_MIRRORED;
    pc_item_state synthesised = clean;
    synthesised.item_flags = PC_ITEM_SYNTHESISED;
    const std::uint32_t clean_product = product.intern_item(clean);
    PC_CHECK(product.intern_item(mirrored) == clean_product);
    PC_CHECK(product.intern_item(synthesised) == clean_product);
    PC_CHECK(exact.intern_item(mirrored) != exact.intern_item(clean));
    PC_CHECK(exact.intern_item(synthesised) != exact.intern_item(clean));

    const std::unordered_map<std::string, double> prices{
        {"transmute", 1.0}, {"alteration", 1.0}, {"base", 1.0}};
    for (const pc_item_state* rejected : {&mirrored, &synthesised}) {
        bool rejected_start = false;
        try {
            (void)solve(product, *rejected, prices);
        } catch (const std::invalid_argument&) {
            rejected_start = true;
        }
        PC_CHECK(rejected_start);
    }
}

void run_semantic_exclusion_equivalence_tests() {
    auto session = make_calc_session();
    /*
     * Mods 3 and 4 remain distinct modifier ids but acquire the same complete
     * group-exclusion effect. The semantic projection may merge them only
     * when every admitted action kernel remains identical after projection;
     * the production raw-witness partition performs the final proof.
     */
    session->primary_group[4] = session->primary_group[3];
    session->group_ids[session->group_offsets[4]] =
        session->primary_group[3];
    for (std::vector<std::uint64_t>& mask :
         session->group_masks) {
        std::fill(mask.begin(), mask.end(), 0);
    }
    for (std::uint32_t mod = 0; mod < session->mod_count; ++mod) {
        for (std::uint32_t group = session->group_offsets[mod];
             group < session->group_offsets[mod + 1]; ++group) {
            std::vector<std::uint64_t>& mask =
                session->group_masks[session->group_ids[group]];
            if (mask.empty()) mask.assign(session->words, 0);
            pc_bitset_set(mask.data(), mod);
        }
    }

    const ActionRegistry registry = build_action_registry(*session);
    const std::vector<std::uint32_t> actions{
        registry.index_by_id.at("chaos"),
        registry.index_by_id.at("exalt"),
        registry.index_by_id.at("annul"),
        registry.index_by_id.at("scour")};
    const GoalSpec goal = family_goal_100();
    CalcContext semantic(
        session, goal, registry, actions,
        false, false, true, std::nullopt, {}, true);
    CalcContext concrete(
        session, goal, registry, actions,
        false, false, true, std::nullopt, {}, true, {}, true);

    pc_item_state left;
    pc_item_clear(&left);
    left.rarity = PC_RARITY_RARE;
    place(
        &left, PC_SIDE_PREFIX, 3,
        static_cast<std::uint16_t>(session->primary_group[3]));
    pc_item_state right;
    pc_item_clear(&right);
    right.rarity = PC_RARITY_RARE;
    place(
        &right, PC_SIDE_PREFIX, 4,
        static_cast<std::uint16_t>(session->primary_group[4]));

    PC_CHECK(
        semantic.intern_item(left) ==
        semantic.intern_item(right));
    const std::uint32_t concrete_left =
        concrete.intern_item(left);
    const std::uint32_t concrete_right =
        concrete.intern_item(right);
    PC_CHECK(concrete_left != concrete_right);
    for (const std::uint32_t action : actions) {
        const OutcomeDistribution& left_distribution =
            concrete.outcomes(concrete_left, action);
        const OutcomeDistribution& right_distribution =
            concrete.outcomes(concrete_right, action);
        PC_CHECK(left_distribution.supported);
        PC_CHECK(right_distribution.supported);
        const auto projected_left = project_distribution(
            concrete, semantic, left_distribution);
        const auto projected_right = project_distribution(
            concrete, semantic, right_distribution);
        PC_CHECK(projected_left.size() == projected_right.size());
        for (const auto& [state, probability] : projected_left) {
            const auto found = projected_right.find(state);
            PC_CHECK(found != projected_right.end());
            if (found != projected_right.end()) {
                PC_CHECK(near(probability, found->second));
            }
        }
    }

    /*
     * Semantic carrier activation is contract-driven. This future-shaped
     * Exalt descriptor keeps its ordinary primitive type but declares that it
     * observes Veiled carriers. A template known only through the complete
     * session mask must therefore split without adding an action-type branch
     * to CalcContext.
     */
    session->veiled_template_mask.assign(session->words, 0);
    pc_bitset_set(session->veiled_template_mask.data(), 4);
    ActionDescriptor observes_veiled =
        registry.actions.at(registry.index_by_id.at("exalt"));
    observes_veiled.id = "test:contract-veiled-observer";
    PC_CHECK(
        observes_veiled.refinement.affix_observations.size() == 1);
    observes_veiled.refinement.affix_observations.front().features |=
        refinement_feature(RefinementFeature::ModifierVeiled);
    validate_action_refinement_contract(observes_veiled);
    ActionRegistry observer_registry;
    observer_registry.index_by_id.emplace(observes_veiled.id, 0);
    observer_registry.actions.push_back(observes_veiled);
    CalcContext observed(
        session, goal, observer_registry, {0},
        false, false, false);
    PC_CHECK(
        observed.layout().junk_class_by_mod[3] !=
        observed.layout().junk_class_by_mod[4]);
}

void run_identity_reforge_factorization_tests() {
    const auto make_shared_exclusion_session = [] {
        auto session = make_calc_session();
        session->primary_group[4] = session->primary_group[3];
        session->group_ids[session->group_offsets[4]] =
            session->primary_group[3];
        for (std::vector<std::uint64_t>& mask :
             session->group_masks) {
            std::fill(mask.begin(), mask.end(), 0);
        }
        for (std::uint32_t mod = 0; mod < session->mod_count; ++mod) {
            for (std::uint32_t group = session->group_offsets[mod];
                 group < session->group_offsets[mod + 1]; ++group) {
                std::vector<std::uint64_t>& mask =
                    session->group_masks[session->group_ids[group]];
                if (mask.empty()) mask.assign(session->words, 0);
                pc_bitset_set(mask.data(), mod);
            }
        }
        return session;
    };
    const auto compare_projected =
        [](CalcContext& exact,
           CalcContext& semantic,
           const OutcomeDistribution& exact_distribution,
           const OutcomeDistribution& semantic_distribution) {
            const auto projected = project_distribution(
                exact, semantic, exact_distribution);
            PC_CHECK(
                projected.size() ==
                semantic_distribution.entries.size());
            for (const OutcomeEntry& entry :
                 semantic_distribution.entries) {
                const auto found = projected.find(entry.state);
                PC_CHECK(found != projected.end());
                if (found != projected.end()) {
                    PC_CHECK(near(
                        found->second, entry.probability, 1e-12));
                }
            }
        };

    /*
     * Chaos uses one structural bucket for the shared physical exclusion
     * family, but the exact evaluator must still emit both literal modifier
     * witnesses. Projecting those witnesses back through the semantic layout
     * must reproduce its complete kernel.
     */
    {
        const auto session = make_shared_exclusion_session();
        const ActionRegistry registry = build_action_registry(*session);
        const std::vector<std::uint32_t> actions{
            registry.index_by_id.at("chaos")};
        CalcContext semantic(
            session, family_goal_100(), registry, actions,
            false, false, true);
        CalcContext exact(
            session, family_goal_100(), registry, actions,
            false, false, true, std::nullopt, {}, false, {}, true);
        PC_CHECK(!semantic.distinguishes_modifier_identity());
        PC_CHECK(exact.distinguishes_modifier_identity());

        pc_item_state rare;
        pc_item_clear(&rare);
        rare.rarity = PC_RARITY_RARE;
        const OutcomeDistribution& exact_distribution =
            exact.outcomes(exact.intern_item(rare), actions.front());
        const OutcomeDistribution& semantic_distribution =
            semantic.outcomes(
                semantic.intern_item(rare), actions.front());
        PC_CHECK(exact_distribution.supported);
        PC_CHECK(semantic_distribution.supported);
        PC_CHECK(sums_to_one(exact_distribution));
        compare_projected(
            exact, semantic, exact_distribution,
            semantic_distribution);

        std::map<std::uint32_t, std::uint32_t> witnesses_per_class;
        for (const OutcomeEntry& entry : exact_distribution.entries) {
            pc_item_state item;
            PC_CHECK(exact.materialize(entry.state, item));
            ++witnesses_per_class[semantic.intern_item(item)];
        }
        PC_CHECK(std::any_of(
            witnesses_per_class.begin(), witnesses_per_class.end(),
            [](const auto& entry) { return entry.second > 1; }));

        CalcContext capped(
            session, family_goal_100(), registry, actions,
            false, false, true, std::nullopt, {}, false, {}, true);
        capped.set_solve_resource_caps(100000, 1, false);
        bool reforge_cap_hit = false;
        try {
            (void)capped.outcomes(
                capped.intern_item(rare), actions.front());
        } catch (const SolverResourceLimit& error) {
            reforge_cap_hit =
                error.cap_name() == "max_reforge_work";
        }
        PC_CHECK(reforge_cap_hit);

        /*
         * Exact reforge scratch shares the calculator's owned-byte budget.
         * A cap below the first outcome-map reserve must refuse with the
         * stable named cap before attempting that reserve.
         */
        CalcContext reserve_capped(
            session, family_goal_100(), registry, actions,
            false, false, true, std::nullopt, {}, false, {}, true);
        const std::uint32_t reserve_state =
            reserve_capped.intern_item(rare);
        const std::uint64_t reserve_owned =
            reserve_capped.fast_estimated_owned_bytes();
        reserve_capped.set_solve_resource_caps(
            100000,
            std::numeric_limits<std::uint64_t>::max(),
            false,
            reserve_owned + 1);
        bool reserve_cap_hit = false;
        try {
            (void)reserve_capped.outcomes(
                reserve_state, actions.front());
        } catch (const SolverResourceLimit& error) {
            reserve_cap_hit =
                error.cap_name() == "max_owned_bytes";
        }
        PC_CHECK(reserve_cap_hit);

        /*
         * With a one-state outcome reserve, leave exactly enough budget for
         * that map plus one byte. Raw family classification then runs, but
         * the stationary bucket/conflict-matrix preflight must refuse before
         * allocating the projected matrix.
         */
        CalcContext matrix_capped(
            session, family_goal_100(), registry, actions,
            false, false, true, std::nullopt, {}, false, {}, true);
        const std::uint32_t matrix_state =
            matrix_capped.intern_item(rare);
        const std::uint64_t matrix_owned =
            matrix_capped.fast_estimated_owned_bytes();
        constexpr std::uint64_t one_outcome_entry_bytes =
            sizeof(std::pair<
                const std::uint32_t,
                solve_detail::WideFloat>) +
            5 * sizeof(void*);
        matrix_capped.set_solve_resource_caps(
            1,
            std::numeric_limits<std::uint64_t>::max(),
            false,
            matrix_owned + one_outcome_entry_bytes + 1);
        bool matrix_cap_hit = false;
        try {
            (void)matrix_capped.outcomes(
                matrix_state, actions.front());
        } catch (const SolverResourceLimit& error) {
            matrix_cap_hit =
                error.cap_name() == "max_owned_bytes";
        }
        PC_CHECK(matrix_cap_hit);
        PC_CHECK(
            matrix_capped.telemetry()
                    .reforge_effort.pool_entries_scanned > 0);
        PC_CHECK(
            matrix_capped.telemetry()
                    .reforge_effort.rows_interrupted > 0);

        /*
         * A sufficient cap is observational only: exact raw witnesses and
         * their semantic projection remain byte-for-byte deterministic.
         */
        CalcContext memory_bounded(
            session, family_goal_100(), registry, actions,
            false, false, true, std::nullopt, {}, false, {}, true);
        const std::uint32_t bounded_state =
            memory_bounded.intern_item(rare);
        const std::uint64_t bounded_owned =
            memory_bounded.fast_estimated_owned_bytes();
        memory_bounded.set_solve_resource_caps(
            100000,
            std::numeric_limits<std::uint64_t>::max(),
            false,
            bounded_owned + 64ull * 1024ull * 1024ull);
        const OutcomeDistribution& bounded_distribution =
            memory_bounded.outcomes(
                bounded_state, actions.front());
        PC_CHECK(bounded_distribution.supported);
        PC_CHECK(sums_to_one(bounded_distribution));
        compare_projected(
            memory_bounded, semantic, bounded_distribution,
            semantic_distribution);
    }

    /*
     * Harvest's guaranteed first draw has a narrower raw-identity channel
     * than the subsequent ordinary draws. Mod 3 is the sole fire member;
     * mod 4 shares its complete exclusion family but is not fire. A sound
     * deferred expansion must therefore remember that the family was picked
     * by the guaranteed channel and never substitute mod 4 at commit.
     */
    {
        const auto session = make_shared_exclusion_session();
        session->class_tag_ids[0] = kTagFire;
        session->class_tag_ids[2] = 1;
        session->implicit_tag_masks.assign(
            7, std::vector<std::uint64_t>(session->words, 0));
        for (std::uint32_t mod = 0; mod < session->mod_count; ++mod) {
            for (std::uint32_t index = session->class_offsets[mod];
                 index < session->class_offsets[mod + 1]; ++index) {
                pc_bitset_set(
                    session->implicit_tag_masks[
                        session->class_tag_ids[index]]
                        .data(),
                    mod);
            }
        }

        ActionRegistry registry;
        ActionDescriptor reforge;
        reforge.id = "harvest_reforge:fire";
        reforge.params.type = ActionType::HarvestReforge;
        reforge.params.target_tag_id = kTagFire;
        reforge.kind = TransitionKind::Reforge;
        reforge.cost_keys = {reforge.id};
        reforge.legality.rarity_mask = 1u << PC_RARITY_RARE;
        reforge.discriminating_tag_ids = {kTagFire};
        reforge.refinement =
            derive_action_refinement_contract(*session, reforge);
        validate_action_refinement_contract(reforge);
        registry.index_by_id.emplace(reforge.id, 0);
        registry.actions.push_back(reforge);

        CalcContext semantic(
            session, family_goal_100(), registry, {},
            false, true, true);
        CalcContext exact(
            session, family_goal_100(), registry, {},
            false, true, true, std::nullopt, {}, false, {}, true);
        pc_item_state rare;
        pc_item_clear(&rare);
        rare.rarity = PC_RARITY_RARE;
        const OutcomeDistribution& exact_distribution =
            exact.outcomes(exact.intern_item(rare), 0);
        const OutcomeDistribution& semantic_distribution =
            semantic.outcomes(semantic.intern_item(rare), 0);
        PC_CHECK(exact_distribution.supported);
        PC_CHECK(sums_to_one(exact_distribution));
        compare_projected(
            exact, semantic, exact_distribution,
            semantic_distribution);
        for (const OutcomeEntry& entry : exact_distribution.entries) {
            pc_item_state item;
            PC_CHECK(exact.materialize(entry.state, item));
            PC_CHECK(item_contains_mod(item, 3));
            PC_CHECK(!item_contains_mod(item, 4));
        }
    }
}

void run_projected_reforge_frontier_equivalence_tests() {
    ReforgeEffortBreakdown saturated;
    saturated.rows_begun =
        std::numeric_limits<std::uint64_t>::max() - 1;
    saturated.pool_entries_scanned = 7;
    ReforgeEffortBreakdown overflow;
    overflow.rows_begun = 9;
    overflow.pool_entries_scanned = 5;
    merge_reforge_effort(saturated, overflow);
    PC_CHECK(
        saturated.rows_begun ==
        std::numeric_limits<std::uint64_t>::max());
    PC_CHECK(saturated.pool_entries_scanned == 12);
    ReforgeEffortBreakdown smaller;
    smaller.rows_begun = 1;
    const ReforgeEffortBreakdown clamped_delta =
        reforge_effort_delta(smaller, saturated);
    PC_CHECK(clamped_delta.rows_begun == 0);

    auto session = make_calc_session();
    session->essence_guaranteed_mod_ids = {3};
    session->fossil_added_mod_ids = {{4}};
    session->fossil_forced_mod_ids = {{3}};
    session->fossil_sell_price_mod_ids = {{}};
    auto data = std::const_pointer_cast<DataImpl>(session->data);
    data->strings.push_back("Projected Frontier Test Fossil");
    data->fossil_count = 1;
    data->fossil_name_sids = {0};
    data->fossil_rolls_lucky = {0};
    data->fossil_mirrors = {0};
    data->fossil_weight_positive_code = 1;
    data->fossil_weight_negative_code = 2;
    data->fossil_weight_offsets = {0, 2};
    data->fossil_weight_kind_codes = {1, 2};
    data->fossil_weight_tag_ids = {kTagFire, kTagCold};
    data->fossil_weight_values = {200, 0};

    ActionRegistry registry;
    const auto add_action = [&](ActionDescriptor action) {
        action.kind = TransitionKind::Reforge;
        action.cost_keys = {action.id};
        action.legality.rarity_mask = 1u << PC_RARITY_RARE;
        action.refinement =
            derive_action_refinement_contract(*session, action);
        validate_action_refinement_contract(action);
        const std::uint32_t index =
            static_cast<std::uint32_t>(registry.actions.size());
        registry.index_by_id.emplace(action.id, index);
        registry.actions.push_back(std::move(action));
    };
    ActionDescriptor chaos;
    chaos.id = "test:projected-chaos";
    chaos.params.type = ActionType::Chaos;
    add_action(std::move(chaos));
    ActionDescriptor essence;
    essence.id = "test:projected-essence";
    essence.params.type = ActionType::Essence;
    essence.params.essence_index = 0;
    add_action(std::move(essence));
    ActionDescriptor harvest;
    harvest.id = "test:projected-harvest";
    harvest.params.type = ActionType::HarvestReforge;
    harvest.params.target_tag_id = kTagFire;
    harvest.discriminating_tag_ids = {kTagFire};
    add_action(std::move(harvest));
    ActionDescriptor fossil;
    fossil.id = "test:projected-fossil";
    fossil.params.type = ActionType::Fossil;
    fossil.params.fossil_indices = {0};
    fossil.discriminating_tag_ids = {kTagFire, kTagCold};
    add_action(std::move(fossil));

    const std::vector<std::uint32_t> actions{0, 1, 2, 3};
    CalcContext raw(
        session, family_goal_100(), registry, actions,
        false, false, true, std::nullopt, {}, false, {}, false,
        true, false);
    CalcContext raw_without_resource_accounting(
        session, family_goal_100(), registry, actions,
        false, false, true, std::nullopt, {}, false, {}, false,
        true, false);
    raw_without_resource_accounting.set_reforge_resource_accounting(
        false);
    CalcContext projected(
        session, family_goal_100(), registry, actions,
        false, false, true, std::nullopt, {}, false, {}, false,
        true, true);
    CalcContext raw_reverse(
        session, family_goal_100(), registry, actions,
        false, false, true, std::nullopt, {}, false, {}, false,
        true, false, true);
    CalcContext projected_reverse(
        session, family_goal_100(), registry, actions,
        false, false, true, std::nullopt, {}, false, {}, false,
        true, true, true);
    CalcContext factored(
        session, family_goal_100(), registry, actions,
        false, false, true, std::nullopt, {}, false, {}, false,
        true, true, false, true);
    CalcContext factored_reverse(
        session, family_goal_100(), registry, actions,
        false, false, true, std::nullopt, {}, false, {}, false,
        true, true, true, true);
    pc_item_state rare;
    pc_item_clear(&rare);
    rare.rarity = PC_RARITY_RARE;
    const std::uint32_t raw_start = raw.intern_item(rare);
    const std::uint32_t raw_without_resource_accounting_start =
        raw_without_resource_accounting.intern_item(rare);
    const std::uint32_t projected_start = projected.intern_item(rare);
    const std::uint32_t raw_reverse_start =
        raw_reverse.intern_item(rare);
    const std::uint32_t projected_reverse_start =
        projected_reverse.intern_item(rare);
    const std::uint32_t factored_start = factored.intern_item(rare);
    const std::uint32_t factored_reverse_start =
        factored_reverse.intern_item(rare);
    for (const std::uint32_t action : actions) {
        const OutcomeDistribution& raw_distribution =
            raw.outcomes(raw_start, action);
        const OutcomeDistribution&
            raw_without_resource_accounting_distribution =
                raw_without_resource_accounting.outcomes(
                    raw_without_resource_accounting_start, action);
        const OutcomeDistribution& projected_distribution =
            projected.outcomes(projected_start, action);
        const OutcomeDistribution& raw_reverse_distribution =
            raw_reverse.outcomes(raw_reverse_start, action);
        const OutcomeDistribution& projected_reverse_distribution =
            projected_reverse.outcomes(
                projected_reverse_start, action);
        const OutcomeDistribution& factored_distribution =
            factored.outcomes(factored_start, action);
        const OutcomeDistribution& factored_reverse_distribution =
            factored_reverse.outcomes(
                factored_reverse_start, action);
        PC_CHECK(raw_distribution.supported);
        PC_CHECK(raw_without_resource_accounting_distribution.supported);
        PC_CHECK(projected_distribution.supported);
        PC_CHECK(sums_to_one(raw_distribution));
        PC_CHECK(sums_to_one(
            raw_without_resource_accounting_distribution));
        PC_CHECK(sums_to_one(projected_distribution));
        PC_CHECK(sums_to_one(raw_reverse_distribution));
        PC_CHECK(sums_to_one(projected_reverse_distribution));
        PC_CHECK(sums_to_one(factored_distribution));
        PC_CHECK(sums_to_one(factored_reverse_distribution));
        std::map<std::uint32_t, double> raw_mass;
        for (const OutcomeEntry& entry : raw_distribution.entries) {
            raw_mass[entry.state] += entry.probability;
        }
        const auto projected_mass = project_distribution(
            projected, raw, projected_distribution);
        const auto raw_without_resource_accounting_mass =
            project_distribution(
                raw_without_resource_accounting,
                raw,
                raw_without_resource_accounting_distribution);
        const auto raw_reverse_mass = project_distribution(
            raw_reverse, raw, raw_reverse_distribution);
        const auto projected_reverse_mass = project_distribution(
            projected_reverse, raw,
            projected_reverse_distribution);
        const auto factored_mass = project_distribution(
            factored, raw, factored_distribution);
        const auto factored_reverse_mass = project_distribution(
            factored_reverse, raw,
            factored_reverse_distribution);
        const auto check_mass =
            [&](const std::map<std::uint32_t, double>& candidate) {
                PC_CHECK(raw_mass.size() == candidate.size());
                for (const auto& [state, probability] : raw_mass) {
                    const auto found = candidate.find(state);
                    PC_CHECK(found != candidate.end());
                    if (found != candidate.end()) {
                        PC_CHECK(near(
                            probability, found->second, 1e-12));
                    }
                }
            };
        check_mass(projected_mass);
        check_mass(raw_without_resource_accounting_mass);
        check_mass(raw_reverse_mass);
        check_mass(projected_reverse_mass);
        check_mass(factored_mass);
        check_mass(factored_reverse_mass);
        for (const OutcomeDistribution* candidate : {
                 &projected_distribution,
                 &raw_reverse_distribution,
                 &projected_reverse_distribution,
                 &factored_distribution,
                 &factored_reverse_distribution}) {
            for (std::size_t slot = 0;
                 slot < kMaxGoalSlots; ++slot) {
                PC_CHECK(near(
                    raw_distribution.slot_satisfied_probability[slot],
                    candidate->slot_satisfied_probability[slot],
                    1e-12));
            }
        }
    }
    const std::array<ReforgeRowFamily, 4> expected_families{
        ReforgeRowFamily::Ordinary,
        ReforgeRowFamily::Essence,
        ReforgeRowFamily::Harvest,
        ReforgeRowFamily::Fossil};
    const auto check_completed_samples = [&expected_families](
            const CalcContext& context,
            const ReforgeEvaluatorVersion evaluator) {
        const CalcTelemetry& telemetry = context.telemetry();
        PC_CHECK(
            telemetry.reforge_row_samples.size() ==
            expected_families.size());
        PC_CHECK(telemetry.reforge_row_samples_omitted == 0);
        ReforgeEffortBreakdown sampled;
        for (std::size_t i = 0;
             i < telemetry.reforge_row_samples.size(); ++i) {
            const ReforgeRowTelemetry& row =
                telemetry.reforge_row_samples[i];
            PC_CHECK(row.sequence == i);
            PC_CHECK(row.action_index == i);
            PC_CHECK(row.owner == ReforgeRowOwner::Coarse);
            PC_CHECK(row.family == expected_families[i]);
            PC_CHECK(row.evaluator == evaluator);
            PC_CHECK(!row.cache_reused);
            PC_CHECK(
                row.disposition ==
                ReforgeRowDisposition::Completed);
            PC_CHECK(row.components.rows_begun == 1);
            PC_CHECK(row.components.rows_completed == 1);
            PC_CHECK(row.components.rows_interrupted == 0);
            PC_CHECK(row.components.pool_entries_scanned > 0);
            PC_CHECK(row.components.physical_families_built > 0);
            PC_CHECK(row.components.roll_buckets_built > 0);
            PC_CHECK(row.components.frontier_nodes > 0);
            PC_CHECK(row.components.terminal_contributions > 0);
            PC_CHECK(
                row.components.successor_publication_attempts > 0);
            PC_CHECK(
                row.components.successor_unique_insertions > 0);
            PC_CHECK(row.components.state_interning_attempts > 0);
            merge_reforge_effort(sampled, row.components);
        }
        PC_CHECK(sampled == telemetry.reforge_effort);
    };
    check_completed_samples(raw, ReforgeEvaluatorVersion::V1Raw);
    check_completed_samples(
        raw_reverse, ReforgeEvaluatorVersion::V1Raw);
    check_completed_samples(
        projected, ReforgeEvaluatorVersion::V2Sparse);
    check_completed_samples(
        projected_reverse, ReforgeEvaluatorVersion::V2Sparse);
    check_completed_samples(
        factored, ReforgeEvaluatorVersion::V3Factored);
    check_completed_samples(
        factored_reverse, ReforgeEvaluatorVersion::V3Factored);

    /* The historic V1 ledger charges one-plus-all-buckets for every
     * frontier node even when goal/terminal control flow does not enter the
     * dense scan. The component ledger records only probes actually made. */
    const ReforgeRowTelemetry& ordinary_raw_row =
        raw.telemetry().reforge_row_samples.at(0);
    PC_CHECK(
        ordinary_raw_row.logical_work_v1 ==
        ordinary_raw_row.components.frontier_nodes *
            (1 + ordinary_raw_row.components.roll_buckets_built));
    PC_CHECK(
        ordinary_raw_row.components.dense_bucket_probes <
        ordinary_raw_row.components.frontier_nodes *
            ordinary_raw_row.components.roll_buckets_built);

    /* Harvest traverses guaranteed support once for its denominator and
     * again for branch publication, while the preserved V1 ledger charges
     * only one of those scans. */
    const ReforgeRowTelemetry& harvest_raw_row =
        raw.telemetry().reforge_row_samples.at(2);
    const ReforgeBuildAttribution& harvest_raw_attribution =
        raw.telemetry().reforge_build_attribution_samples.at(2);
    PC_CHECK(harvest_raw_attribution.guaranteed_scan_work > 0);
    PC_CHECK(
        harvest_raw_attribution.guaranteed_scan_work ==
        harvest_raw_row.components.roll_buckets_built);
    PC_CHECK(
        harvest_raw_row.logical_work_v1 ==
        harvest_raw_attribution.guaranteed_scan_work +
            harvest_raw_row.components.frontier_nodes *
                (1 + harvest_raw_row.components.roll_buckets_built));
    PC_CHECK(
        harvest_raw_row.components.dense_bucket_probes >=
        2 * harvest_raw_attribution.guaranteed_scan_work);

    PC_CHECK(
        raw.telemetry().reforge_effort ==
        raw_reverse.telemetry().reforge_effort);
    PC_CHECK(
        raw_without_resource_accounting.telemetry().reforge_effort ==
        ReforgeEffortBreakdown{});
    PC_CHECK(
        raw_without_resource_accounting.telemetry()
            .reforge_row_samples.empty());
    PC_CHECK(
        raw_without_resource_accounting.telemetry()
            .reforge_row_samples_omitted == 0);
    PC_CHECK(
        raw_without_resource_accounting.telemetry()
            .reforge_frontier_work ==
        raw.telemetry().reforge_frontier_work);
    PC_CHECK(
        raw_without_resource_accounting.telemetry()
            .reforge_logical_work_v1 ==
        raw.telemetry().reforge_logical_work_v1);
    PC_CHECK(
        raw_without_resource_accounting.telemetry()
            .reforge_raw_equivalent_work ==
        raw.telemetry().reforge_raw_equivalent_work);
    PC_CHECK(
        raw_without_resource_accounting.telemetry()
            .reforge_projected_work ==
        raw.telemetry().reforge_projected_work);
    PC_CHECK(
        raw_without_resource_accounting.telemetry()
            .reforge_factored_work ==
        raw.telemetry().reforge_factored_work);
    PC_CHECK(
        projected.telemetry().reforge_effort ==
        projected_reverse.telemetry().reforge_effort);
    PC_CHECK(
        factored.telemetry().reforge_effort ==
        factored_reverse.telemetry().reforge_effort);
    PC_CHECK(
        raw.telemetry().reforge_frontier_work ==
        raw.telemetry().reforge_raw_equivalent_work);
    PC_CHECK(
        raw.telemetry().reforge_logical_work_v1 ==
        raw.telemetry().reforge_raw_equivalent_work);
    PC_CHECK(
        projected.telemetry().reforge_logical_work_v1 ==
        raw.telemetry().reforge_logical_work_v1);
    PC_CHECK(
        factored.telemetry().reforge_logical_work_v1 ==
        raw.telemetry().reforge_logical_work_v1);
    PC_CHECK(
        projected.telemetry().reforge_frontier_work ==
        projected.telemetry().reforge_projected_work);
    PC_CHECK(
        raw.telemetry().reforge_raw_equivalent_work ==
        projected.telemetry().reforge_raw_equivalent_work);
    PC_CHECK(
        projected.telemetry().reforge_projected_work <
        projected.telemetry().reforge_raw_equivalent_work);
    PC_CHECK(
        raw.telemetry().reforge_build_attribution_samples.size() ==
        actions.size());
    PC_CHECK(
        projected.telemetry().reforge_build_attribution_samples.size() ==
        actions.size());
    for (const ReforgeBuildAttribution& row :
         projected.telemetry().reforge_build_attribution_samples) {
        PC_CHECK(row.completed);
        PC_CHECK(row.projected_sparse_frontier);
        PC_CHECK(
            row.projected_reforge_work <
            row.raw_equivalent_reforge_work);
    }

    const std::uint64_t raw_work_before_cache =
        raw.telemetry().reforge_frontier_work;
    const std::uint64_t logical_work_before_cache =
        raw.telemetry().reforge_logical_work_v1;
    raw.release_outcome(raw_start, 0);
    (void)raw.outcomes(raw_start, 0);
    PC_CHECK(
        raw.telemetry().reforge_frontier_work ==
        raw_work_before_cache);
    PC_CHECK(
        raw.telemetry().reforge_logical_work_v1 ==
        logical_work_before_cache);
    PC_CHECK(raw.telemetry().reforge_effort.rows_cache_reused == 1);
    PC_CHECK(
        raw.telemetry().reforge_row_samples.back().cache_reused);
    PC_CHECK(
        raw.telemetry().reforge_row_samples.back().disposition ==
        ReforgeRowDisposition::Completed);

    /* One stable logical cap admits the same complete row envelope for all
     * evaluator versions. The final over-cap row is observed as interrupted
     * and never retained, while legacy active effort remains versioned. */
    const std::uint64_t shared_logical_cap =
        raw.telemetry().reforge_logical_work_v1 - 1;
    CalcContext capped_raw(
        session, family_goal_100(), registry, actions,
        false, false, true, std::nullopt, {}, false, {}, false,
        true, false);
    CalcContext capped_projected(
        session, family_goal_100(), registry, actions,
        false, false, true, std::nullopt, {}, false, {}, false,
        true, true);
    CalcContext capped_factored(
        session, family_goal_100(), registry, actions,
        false, false, true, std::nullopt, {}, false, {}, false,
        true, true, false, true);
    const auto run_capped = [&](CalcContext& context) {
        context.set_solve_resource_caps(
            100000, shared_logical_cap, false);
        const std::uint32_t start = context.intern_item(rare);
        bool capped = false;
        for (const std::uint32_t action : actions) {
            try {
                (void)context.outcomes(start, action);
            } catch (const SolverResourceLimit& limit) {
                PC_CHECK(limit.cap_name() == "max_reforge_work");
                capped = true;
                break;
            }
        }
        PC_CHECK(capped);
        PC_CHECK(
            context.telemetry().reforge_logical_work_v1 <=
            shared_logical_cap);
        PC_CHECK(
            context.telemetry().reforge_effort.rows_interrupted == 1);
        PC_CHECK(
            context.telemetry().reforge_row_samples.back().disposition ==
            ReforgeRowDisposition::Interrupted);
    };
    run_capped(capped_raw);
    run_capped(capped_projected);
    run_capped(capped_factored);
    PC_CHECK(
        capped_raw.telemetry().reforge_effort.rows_completed ==
        capped_projected.telemetry().reforge_effort.rows_completed);
    PC_CHECK(
        capped_raw.telemetry().reforge_effort.rows_completed ==
        capped_factored.telemetry().reforge_effort.rows_completed);
    PC_CHECK(
        capped_raw.telemetry().reforge_raw_equivalent_work ==
        capped_projected.telemetry().reforge_raw_equivalent_work);
    PC_CHECK(
        capped_raw.telemetry().reforge_raw_equivalent_work ==
        capped_factored.telemetry().reforge_raw_equivalent_work);
    PC_CHECK(
        capped_raw.telemetry().reforge_frontier_work ==
        shared_logical_cap);
    PC_CHECK(
        capped_projected.telemetry().reforge_frontier_work !=
        capped_raw.telemetry().reforge_frontier_work);
    PC_CHECK(
        capped_factored.telemetry().reforge_frontier_work !=
        capped_raw.telemetry().reforge_frontier_work);

    CalcContext provenance(
        session, family_goal_100(), registry, actions,
        false, false, true);
    const std::uint32_t provenance_start =
        provenance.intern_item(rare);
    {
        ScopedReforgeRowProvenance published(
            provenance, ReforgeRowOwner::StrictSelected);
        (void)provenance.outcomes(provenance_start, 0);
        published.publish();
    }
    provenance.release_outcome(provenance_start, 0);
    {
        ScopedReforgeRowProvenance discarded(
            provenance, ReforgeRowOwner::StrictAlternative);
        (void)provenance.outcomes(provenance_start, 0);
    }
    PC_CHECK(provenance.telemetry().reforge_effort.rows_published == 1);
    PC_CHECK(provenance.telemetry().reforge_effort.rows_discarded == 1);
    PC_CHECK(
        provenance.telemetry().reforge_effort.rows_cache_reused == 1);
    PC_CHECK(
        provenance.telemetry().reforge_row_samples.size() == 2);
    if (provenance.telemetry().reforge_row_samples.size() == 2) {
        PC_CHECK(
            provenance.telemetry().reforge_row_samples[0].owner ==
            ReforgeRowOwner::StrictSelected);
        PC_CHECK(
            provenance.telemetry().reforge_row_samples[0].disposition ==
            ReforgeRowDisposition::Published);
        PC_CHECK(
            provenance.telemetry().reforge_row_samples[1].owner ==
            ReforgeRowOwner::StrictAlternative);
        PC_CHECK(
            provenance.telemetry().reforge_row_samples[1].disposition ==
            ReforgeRowDisposition::Discarded);
    }

    const OutcomeDistribution& gated =
        projected.outcomes(projected_start, 0, true);
    const OutcomeDistribution& gated_reverse =
        projected_reverse.outcomes(
            projected_reverse_start, 0, true);
    const OutcomeDistribution& gated_factored =
        factored.outcomes(factored_start, 0, true);
    const OutcomeDistribution& gated_factored_reverse =
        factored_reverse.outcomes(
            factored_reverse_start, 0, true);
    PC_CHECK(gated.supported);
    PC_CHECK(gated_reverse.supported);
    PC_CHECK(sums_to_one(gated));
    PC_CHECK(sums_to_one(gated_reverse));
    PC_CHECK(gated_factored.supported);
    PC_CHECK(gated_factored_reverse.supported);
    PC_CHECK(sums_to_one(gated_factored));
    PC_CHECK(sums_to_one(gated_factored_reverse));
    PC_CHECK(
        gated_factored.gated_kernel_bits_hash ==
        gated_factored_reverse.gated_kernel_bits_hash);
    const auto gated_mass = project_distribution(
        projected, raw, gated);
    const auto gated_factored_mass = project_distribution(
        factored, raw, gated_factored);
    const auto gated_factored_reverse_mass = project_distribution(
        factored_reverse, raw, gated_factored_reverse);
    PC_CHECK(gated_mass.size() == gated_factored_mass.size());
    PC_CHECK(gated_mass.size() == gated_factored_reverse_mass.size());
    for (const auto& [state, probability] : gated_mass) {
        const auto direct = gated_factored_mass.find(state);
        const auto reversed = gated_factored_reverse_mass.find(state);
        PC_CHECK(direct != gated_factored_mass.end());
        PC_CHECK(reversed != gated_factored_reverse_mass.end());
        if (direct != gated_factored_mass.end()) {
            PC_CHECK(near(probability, direct->second, 1e-12));
        }
        if (reversed != gated_factored_reverse_mass.end()) {
            PC_CHECK(near(probability, reversed->second, 1e-12));
        }
    }

    CalcContext resumed_factored(
        session, family_goal_100(), registry, actions,
        false, false, true, std::nullopt, {}, false, {}, false,
        true, true, false, true);
    const std::uint32_t resumed_start = resumed_factored.intern_item(rare);
    std::shared_ptr<const OutcomeDistribution> resumed_distribution;
    std::uint64_t resume_calls = 0;
    while (!resumed_factored.advance_outcomes(
        resumed_start, 0, true, resumed_distribution, 1)) {
        ++resume_calls;
        PC_CHECK(resumed_factored.outcome_cursor_bytes() > 0);
    }
    ++resume_calls;
    PC_CHECK(resumed_distribution != nullptr);
    PC_CHECK(resumed_factored.outcome_cursor_bytes() == 0);
    PC_CHECK(
        resumed_factored.telemetry().reforge_continuation_resumes ==
        resume_calls);
    PC_CHECK(
        resumed_factored.telemetry().reforge_continuation_suspensions + 1 ==
        resume_calls);
    PC_CHECK(
        resumed_factored.telemetry().reforge_continuation_completions == 1);
    const auto resumed_mass = project_distribution(
        resumed_factored, raw, *resumed_distribution);
    PC_CHECK(resumed_mass.size() == gated_factored_mass.size());
    for (const auto& [state, probability] : gated_factored_mass) {
        const auto found = resumed_mass.find(state);
        PC_CHECK(found != resumed_mass.end());
        if (found != resumed_mass.end()) {
            PC_CHECK(
                std::bit_cast<std::uint64_t>(probability) ==
                std::bit_cast<std::uint64_t>(found->second));
        }
    }

    CalcContext cancelled_factored(
        session, family_goal_100(), registry, actions,
        false, false, true, std::nullopt, {}, false, {}, false,
        true, true, false, true);
    const std::uint32_t cancelled_start =
        cancelled_factored.intern_item(rare);
    std::shared_ptr<const OutcomeDistribution> cancelled_distribution;
    PC_CHECK(!cancelled_factored.advance_outcomes(
        cancelled_start, 0, true, cancelled_distribution, 1));
    PC_CHECK(cancelled_distribution == nullptr);
    PC_CHECK(cancelled_factored.outcome_cursor_bytes() > 0);
    cancelled_factored.cancel_outcomes();
    PC_CHECK(cancelled_factored.outcome_cursor_bytes() == 0);
    PC_CHECK(
        cancelled_factored.telemetry().reforge_continuation_cancellations ==
        1);
    while (!cancelled_factored.advance_outcomes(
        cancelled_start, 0, true, cancelled_distribution, 1)) {}
    PC_CHECK(cancelled_distribution != nullptr);
    PC_CHECK(
        cancelled_factored.telemetry().reforge_continuation_completions == 1);

    /* Focused Gate 5 V1/V3 contract: compare the exact gated target map for
     * every destructive reforge family with two goal slots. Zero-progress
     * outcomes (including present-below-tier carriers) must aggregate into
     * retry, while genuine one-slot progress remains an exact successor. */
    GoalSpec progress_goal;
    GoalSlot life;
    life.family_id = 100;
    life.min_tier = 1;
    GoalSlot fire_resistance;
    fire_resistance.family_id = 104;
    fire_resistance.min_tier = 1;
    progress_goal.slots = {life, fire_resistance};
    CalcContext gated_raw(
        session, progress_goal, registry, actions,
        false, false, true, std::nullopt, {}, false, {}, false,
        true, false);
    CalcContext gated_v3(
        session, progress_goal, registry, actions,
        false, false, true, std::nullopt, {}, false, {}, false,
        true, true, false, true);
    const std::uint32_t gated_raw_start = gated_raw.intern_item(rare);
    const std::uint32_t gated_v3_start = gated_v3.intern_item(rare);
    bool saw_zero_progress = false;
    bool saw_present_below_tier = false;
    bool saw_partial_progress = false;
    double raw_best_goal_probability = -1.0;
    double v3_best_goal_probability = -1.0;
    std::uint32_t raw_selected_action = kNoId;
    std::uint32_t v3_selected_action = kNoId;
    for (const std::uint32_t action : actions) {
        const OutcomeDistribution& raw_ungated =
            gated_raw.outcomes(gated_raw_start, action, false);
        const OutcomeDistribution& v3_ungated =
            gated_v3.outcomes(gated_v3_start, action, false);
        const OutcomeDistribution& raw_gated =
            gated_raw.outcomes(gated_raw_start, action, true);
        const OutcomeDistribution& v3_gated =
            gated_v3.outcomes(gated_v3_start, action, true);
        PC_CHECK(raw_ungated.supported);
        PC_CHECK(v3_ungated.supported);
        PC_CHECK(raw_gated.supported);
        PC_CHECK(v3_gated.supported);
        PC_CHECK(sums_to_one(raw_ungated));
        PC_CHECK(sums_to_one(v3_ungated));
        PC_CHECK(sums_to_one(raw_gated));
        PC_CHECK(sums_to_one(v3_gated));

        const AbstractMassMap raw_ungated_mass =
            abstract_distribution(gated_raw, raw_ungated);
        const AbstractMassMap v3_ungated_mass =
            abstract_distribution(gated_v3, v3_ungated);
        const AbstractMassMap raw_gated_mass =
            abstract_distribution(gated_raw, raw_gated);
        const AbstractMassMap v3_gated_mass =
            abstract_distribution(gated_v3, v3_gated);
        const auto compare_mass = [](const AbstractMassMap& expected,
                                     const AbstractMassMap& actual) {
            PC_CHECK(expected.size() == actual.size());
            for (const auto& [state, probability] : expected) {
                const auto found = actual.find(state);
                PC_CHECK(found != actual.end());
                if (found != actual.end()) {
                    PC_CHECK(near(probability, found->second, 1e-12));
                }
            }
        };
        compare_mass(raw_ungated_mass, v3_ungated_mass);
        compare_mass(raw_gated_mass, v3_gated_mass);
        PC_CHECK(near(
            raw_gated.gated_retry_probability,
            v3_gated.gated_retry_probability, 1e-12));
        for (std::size_t slot = 0; slot < progress_goal.slots.size(); ++slot) {
            PC_CHECK(near(
                raw_ungated.slot_satisfied_probability[slot],
                v3_ungated.slot_satisfied_probability[slot], 1e-12));
            PC_CHECK(near(
                raw_gated.slot_satisfied_probability[slot],
                v3_gated.slot_satisfied_probability[slot], 1e-12));
        }

        double zero_progress_mass = 0.0;
        double below_tier_mass = 0.0;
        double partial_progress_mass = 0.0;
        for (const OutcomeEntry& entry : raw_ungated.entries) {
            const AbstractState& state = gated_raw.state(entry.state);
            std::size_t satisfied = 0;
            for (std::size_t slot = 0;
                 slot < progress_goal.slots.size(); ++slot) {
                satisfied +=
                    state.slot_status[slot] ==
                    static_cast<std::uint8_t>(
                        GoalSlotStatus::Satisfied);
                if (state.slot_status[slot] ==
                    static_cast<std::uint8_t>(
                        GoalSlotStatus::PresentBelowTier)) {
                    below_tier_mass += entry.probability;
                }
            }
            if (satisfied == 0) zero_progress_mass += entry.probability;
            if (satisfied == 1) partial_progress_mass += entry.probability;
        }
        PC_CHECK(near(
            zero_progress_mass,
            raw_gated.gated_retry_probability, 1e-12));
        double raw_goal_probability = 0.0;
        double v3_goal_probability = 0.0;
        for (const OutcomeEntry& entry : raw_gated.entries) {
            if (gated_raw.is_goal_state(gated_raw.state(entry.state))) {
                raw_goal_probability += entry.probability;
            }
        }
        for (const OutcomeEntry& entry : v3_gated.entries) {
            if (gated_v3.is_goal_state(gated_v3.state(entry.state))) {
                v3_goal_probability += entry.probability;
            }
        }
        PC_CHECK(near(
            raw_goal_probability, v3_goal_probability, 1e-12));
        if (raw_goal_probability > raw_best_goal_probability) {
            raw_best_goal_probability = raw_goal_probability;
            raw_selected_action = action;
        }
        if (v3_goal_probability > v3_best_goal_probability) {
            v3_best_goal_probability = v3_goal_probability;
            v3_selected_action = action;
        }
        saw_zero_progress =
            saw_zero_progress || zero_progress_mass > 0.0;
        saw_present_below_tier =
            saw_present_below_tier || below_tier_mass > 0.0;
        saw_partial_progress =
            saw_partial_progress || partial_progress_mass > 0.0;
        if (partial_progress_mass > 0.0) {
            double gated_partial_mass = 0.0;
            for (const OutcomeEntry& entry : raw_gated.entries) {
                const AbstractState& state = gated_raw.state(entry.state);
                std::size_t satisfied = 0;
                for (std::size_t slot = 0;
                     slot < progress_goal.slots.size(); ++slot) {
                    satisfied +=
                        state.slot_status[slot] ==
                        static_cast<std::uint8_t>(
                            GoalSlotStatus::Satisfied);
                }
                if (satisfied == 1) {
                    PC_CHECK(state.goal_progress_retry_basin == 0);
                    gated_partial_mass += entry.probability;
                }
            }
            PC_CHECK(near(
                gated_partial_mass, partial_progress_mass, 1e-12));
        }
    }
    PC_CHECK(saw_zero_progress);
    PC_CHECK(saw_present_below_tier);
    PC_CHECK(saw_partial_progress);
    PC_CHECK(raw_selected_action != kNoId);
    PC_CHECK(v3_selected_action != kNoId);
    PC_CHECK(
        registry.actions[raw_selected_action].id ==
        registry.actions[v3_selected_action].id);
    PC_CHECK(
        factored.telemetry().reforge_frontier_work ==
        factored.telemetry().reforge_factored_work);
    PC_CHECK(
        factored_reverse.telemetry().reforge_frontier_work ==
        factored_reverse.telemetry().reforge_factored_work);
    PC_CHECK(
        factored.telemetry().reforge_effort.v3_predecessor_index_entries >
        0);
    PC_CHECK(factored.telemetry().reforge_effort.v3_denominator_edges > 0);
    PC_CHECK(factored.telemetry().reforge_effort.v3_subset_checks > 0);
    PC_CHECK(factored.telemetry().reforge_effort.v3_candidate_sets > 0);
    PC_CHECK(factored.telemetry().reforge_effort.v3_recurrence_terms > 0);
    PC_CHECK(factored.telemetry().reforge_effort.v3_commits > 0);
    const ReforgeBuildAttribution& factored_terminal =
        factored.telemetry().reforge_build_attribution_samples.back();
    const ReforgeBuildAttribution& factored_terminal_reverse =
        factored_reverse.telemetry()
            .reforge_build_attribution_samples.back();
    for (const ReforgeBuildAttribution* row : {
             &factored_terminal,
             &factored_terminal_reverse}) {
        PC_CHECK(row->completed);
        PC_CHECK(row->projected_sparse_frontier);
        PC_CHECK(row->factored_terminal_accumulator);
        PC_CHECK(row->factored_terminal_predecessors > 0);
        PC_CHECK(row->factored_terminal_candidates > 0);
        PC_CHECK(row->factored_last_pick_terms > 0);
        PC_CHECK(
            row->factored_terminal_candidates ==
            row->factored_terminal_commits);
        PC_CHECK(row->factored_subset_cache_hits > 0);
        PC_CHECK(row->factored_subset_identity_mismatches == 0);
    }
    PC_CHECK(
        factored_terminal.factored_terminal_predecessors ==
        factored_terminal_reverse.factored_terminal_predecessors);
    PC_CHECK(
        factored_terminal.factored_terminal_candidates ==
        factored_terminal_reverse.factored_terminal_candidates);
    PC_CHECK(
        factored_terminal.factored_last_pick_terms ==
        factored_terminal_reverse.factored_last_pick_terms);
    PC_CHECK(factored_terminal.total_reforge_work > 1);
    CalcContext factored_capped(
        session, family_goal_100(), registry, actions,
        false, false, true, std::nullopt, {}, false, {}, false,
        true, true, false, true);
    const std::uint32_t factored_capped_start =
        factored_capped.intern_item(rare);
    factored_capped.set_solve_resource_caps(
        100000,
        factored_terminal.total_reforge_work - 1,
        false);
    bool factored_work_cap_hit = false;
    try {
        (void)factored_capped.outcomes(
            factored_capped_start, 0, true);
    } catch (const SolverResourceLimit& error) {
        factored_work_cap_hit =
            error.cap_name() == "max_reforge_work";
    }
    PC_CHECK(factored_work_cap_hit);
    PC_CHECK(
        factored_capped.telemetry().reforge_effort.rows_begun == 1);
    PC_CHECK(
        factored_capped.telemetry().reforge_effort.rows_completed == 0);
    PC_CHECK(
        factored_capped.telemetry().reforge_effort.rows_interrupted == 1);
    PC_CHECK(
        factored_capped.telemetry().reforge_effort.rows_published == 0);
    PC_CHECK(
        factored_capped.telemetry().reforge_row_samples.size() == 1);
    PC_CHECK(
        factored_capped.telemetry().reforge_row_samples.front().disposition ==
        ReforgeRowDisposition::Interrupted);
    PC_CHECK(
        factored_capped.telemetry().reforge_row_samples.front().logical_work_v1 >
        0);
    PC_CHECK(
        factored.layout().junk_class_by_mod[3] !=
        factored.layout().junk_class_by_mod[4]);
    const auto compare_goal_shape =
        [&](const GoalSpec& shape, const bool require_below_mass) {
            CalcContext shape_raw(
                session, shape, registry, actions,
                false, false, true);
            CalcContext shape_v3(
                session, shape, registry, actions,
                false, false, true, std::nullopt, {}, false, {}, false,
                true, true, false, true);
            const std::uint32_t shape_raw_start =
                shape_raw.intern_item(rare);
            const std::uint32_t shape_v3_start =
                shape_v3.intern_item(rare);
            const OutcomeDistribution& shape_raw_distribution =
                shape_raw.outcomes(shape_raw_start, 0, true);
            const OutcomeDistribution& shape_v3_distribution =
                shape_v3.outcomes(shape_v3_start, 0, true);
            PC_CHECK(shape_raw_distribution.supported);
            PC_CHECK(shape_v3_distribution.supported);
            PC_CHECK(sums_to_one(shape_raw_distribution));
            PC_CHECK(sums_to_one(shape_v3_distribution));
            const auto shape_raw_mass = abstract_distribution(
                shape_raw, shape_raw_distribution);
            const auto shape_v3_mass = abstract_distribution(
                shape_v3, shape_v3_distribution);
            PC_CHECK(
                shape_raw_mass.size() == shape_v3_mass.size());
            for (const auto& [state, probability] : shape_raw_mass) {
                const auto found = shape_v3_mass.find(state);
                PC_CHECK(found != shape_v3_mass.end());
                if (found != shape_v3_mass.end()) {
                    PC_CHECK(near(
                        probability, found->second, 1e-12));
                }
            }
            for (std::size_t slot = 0;
                 slot < shape.slots.size(); ++slot) {
                PC_CHECK(near(
                    shape_raw_distribution
                        .slot_satisfied_probability[slot],
                    shape_v3_distribution
                        .slot_satisfied_probability[slot],
                    1e-12));
                double raw_below = 0.0;
                double v3_below = 0.0;
                for (const OutcomeEntry& entry :
                     shape_raw_distribution.entries) {
                    if (shape_raw.state(entry.state)
                            .slot_status[slot] ==
                        static_cast<std::uint8_t>(
                            GoalSlotStatus::PresentBelowTier)) {
                        raw_below += entry.probability;
                    }
                }
                for (const OutcomeEntry& entry :
                     shape_v3_distribution.entries) {
                    if (shape_v3.state(entry.state)
                            .slot_status[slot] ==
                        static_cast<std::uint8_t>(
                            GoalSlotStatus::PresentBelowTier)) {
                        v3_below += entry.probability;
                    }
                }
                PC_CHECK(near(raw_below, v3_below, 1e-12));
                if (require_below_mass && slot == 0) {
                    PC_CHECK(raw_below > 0.0);
                }
            }
        };
    GoalSpec prefix_goal = family_goal_100();
    compare_goal_shape(prefix_goal, false);
    GoalSpec suffix_goal;
    GoalSlot suffix_slot;
    suffix_slot.family_id = 104;
    suffix_slot.min_tier = 1;
    suffix_goal.slots = {suffix_slot};
    compare_goal_shape(suffix_goal, false);
    GoalSpec mixed_goal = prefix_goal;
    mixed_goal.slots.push_back(suffix_slot);
    compare_goal_shape(mixed_goal, true);
    const ReforgeBuildAttribution& terminal =
        projected.telemetry().reforge_build_attribution_samples.back();
    const ReforgeBuildAttribution& terminal_reverse =
        projected_reverse.telemetry()
            .reforge_build_attribution_samples.back();
    const auto check_terminal_factorization =
        [&](const ReforgeBuildAttribution& row) {
            PC_CHECK(row.completed);
            PC_CHECK(row.final_depth_predecessors > 0);
            PC_CHECK(row.final_depth_branches > 0);
            PC_CHECK(
                row.final_depth_canonical_commits ==
                row.final_depth_unique_canonical_successors +
                    row.final_depth_duplicate_canonical_commits);
            PC_CHECK(
                row.final_depth_duplicate_canonical_commits ==
                row.final_depth_duplicates_same_terminal_bucket +
                    row.final_depth_duplicates_different_terminal_bucket);
            PC_CHECK(
                row.final_depth_branches ==
                row.terminal_side_counts[0].branches +
                    row.terminal_side_counts[1].branches);
            PC_CHECK(
                row.final_depth_branches ==
                row.terminal_bucket_kind_counts[0].branches +
                    row.terminal_bucket_kind_counts[1].branches +
                    row.terminal_bucket_kind_counts[2].branches);
            PC_CHECK(
                row.terminal_target_counts[6]
                    .final_modifier_branches ==
                row.final_depth_branches);
            PC_CHECK(
                row.terminal_contribution_samples.size() <= 16);
            PC_CHECK(
                row.terminal_contribution_samples.size() +
                    row.terminal_contribution_samples_omitted ==
                row.final_depth_canonical_commits);
            PC_CHECK(
                row.final_depth_represented_order_paths >=
                row.final_depth_branches);
        };
    check_terminal_factorization(terminal);
    check_terminal_factorization(terminal_reverse);
    PC_CHECK(
        terminal.final_depth_predecessors ==
        terminal_reverse.final_depth_predecessors);
    PC_CHECK(
        terminal.final_depth_branches ==
        terminal_reverse.final_depth_branches);
    PC_CHECK(
        terminal.final_depth_unique_canonical_successors ==
        terminal_reverse.final_depth_unique_canonical_successors);
    PC_CHECK(
        terminal.final_depth_duplicate_canonical_commits ==
        terminal_reverse.final_depth_duplicate_canonical_commits);
    PC_CHECK(
        terminal.final_depth_duplicates_different_terminal_bucket ==
        terminal_reverse
            .final_depth_duplicates_different_terminal_bucket);
    PC_CHECK(
        terminal.final_depth_terminal_order_excess ==
        terminal_reverse.final_depth_terminal_order_excess);
    PC_CHECK(near(
        terminal.final_depth_probability_mass,
        terminal_reverse.final_depth_probability_mass,
        1e-12));
}

void run_harvest_targeted_natural_regression() {
    constexpr std::uint32_t kGoodFire = 5;
    constexpr std::uint32_t kColdSource = 6;
    constexpr std::uint32_t kZeroGenerationFire = 7;

    auto session = make_calc_session();
    /* Mod 7 remains an ordinary random member with positive spawn weight,
     * but zero ordinary generation weight. Give it the same target tags as
     * the valid fire-resistance mod so only the weight contract excludes it. */
    session->base_gen_pct[kZeroGenerationFire] = 0;
    session->base_roll_weight[kZeroGenerationFire] = 0;
    pc_bitset_clear(
        session->positive_base_weight_mask.data(),
        kZeroGenerationFire);
    session->class_tag_ids = {
        1, 2, kTagFire, kTagResistance, kTagCold, kTagResistance,
        kTagFire, kTagResistance};
    session->class_offsets = {0, 0, 0, 0, 1, 2, 4, 6, 8, 8, 8};
    session->implicit_tag_masks.assign(
        7, std::vector<std::uint64_t>(session->words, 0));
    for (std::uint32_t mod = 0; mod < session->mod_count; ++mod) {
        for (std::uint32_t index = session->class_offsets[mod];
             index < session->class_offsets[mod + 1]; ++index) {
            pc_bitset_set(
                session->implicit_tag_masks[
                    session->class_tag_ids[index]]
                    .data(),
                mod);
        }
    }

    pc_item_state empty_rare;
    pc_item_clear(&empty_rare);
    empty_rare.rarity = PC_RARITY_RARE;

    ActionContextImpl debug_context(1);
    debug_context.session = session;
    PoolBuildRequest targeted;
    targeted.weight_kind = PoolWeightKind::TargetedNatural;
    targeted.target_tag_id = kTagFire;
    std::vector<PoolDebugRow> debug_rows;
    WeightedPool debug_summary;
    build_pool_debug_rows(
        debug_context, &empty_rare, targeted, true, debug_rows,
        &debug_summary, nullptr);
    PC_CHECK(debug_summary.entries.size() == 1);
    PC_CHECK(
        !debug_summary.entries.empty() &&
        debug_summary.entries.front().session_mod_id == kGoodFire);
    bool saw_zero_generation_debug = false;
    for (const PoolDebugRow& row : debug_rows) {
        if (row.entry.session_mod_id != kZeroGenerationFire) continue;
        saw_zero_generation_debug = true;
        PC_CHECK(row.normal_random_member);
        PC_CHECK(row.mechanic_allowed);
        PC_CHECK(!row.positively_weighted);
        PC_CHECK(row.entry.spawn_weight == 400);
        PC_CHECK(row.entry.generation_pct == 0);
        PC_CHECK(row.entry.final_weight == 0);
        PC_CHECK(row.generation_applied);
        PC_CHECK(row.first_failure == 6);
    }
    PC_CHECK(saw_zero_generation_debug);

    ActionRegistry registry;
    const auto add_action = [&](ActionDescriptor action) {
        action.refinement =
            derive_action_refinement_contract(*session, action);
        validate_action_refinement_contract(action);
        const std::uint32_t index =
            static_cast<std::uint32_t>(registry.actions.size());
        registry.index_by_id.emplace(action.id, index);
        registry.actions.push_back(std::move(action));
    };
    ActionDescriptor reforge;
    reforge.id = "harvest_reforge:fire";
    reforge.params.type = ActionType::HarvestReforge;
    reforge.params.target_tag_id = kTagFire;
    reforge.kind = TransitionKind::Reforge;
    reforge.cost_keys = {reforge.id};
    reforge.discriminating_tag_ids = {kTagFire};
    add_action(reforge);

    ActionDescriptor augment;
    augment.id = "harvest_augment:fire";
    augment.params.type = ActionType::HarvestAugment;
    augment.params.target_tag_id = kTagFire;
    augment.kind = TransitionKind::Special;
    augment.cost_keys = {augment.id};
    augment.discriminating_tag_ids = {kTagFire};
    add_action(augment);

    ActionDescriptor resist;
    resist.id = "harvest_resist:cold:fire";
    resist.params.type = ActionType::HarvestResist;
    resist.params.source_tag_id = kTagCold;
    resist.params.target_tag_id = kTagFire;
    resist.kind = TransitionKind::Special;
    resist.cost_keys = {"harvest_resist:fire"};
    resist.discriminating_tag_ids = {
        kTagCold, kTagFire, kTagResistance};
    add_action(resist);

    GoalSpec zero_generation_goal;
    GoalSlot zero_generation_slot;
    zero_generation_slot.family_id =
        session->family_id[kZeroGenerationFire];
    zero_generation_slot.min_tier = 1;
    zero_generation_goal.slots = {zero_generation_slot};
    CalcContext calc(session, zero_generation_goal, registry, {});

    pc_item_state augment_start = empty_rare;
    place(&augment_start, PC_SIDE_PREFIX, 0, 10);
    pc_item_state resist_start = empty_rare;
    place(
        &resist_start, PC_SIDE_SUFFIX, kColdSource,
        static_cast<std::uint16_t>(
            session->primary_group[kColdSource]));

    const auto check_exact = [&](const pc_item_state& start,
                                 const std::uint32_t action_index) {
        const OutcomeDistribution& distribution =
            calc.outcomes(calc.intern_item(start), action_index);
        PC_CHECK(distribution.supported);
        PC_CHECK(sums_to_one(distribution));
        PC_CHECK(distribution.slot_satisfied_probability[0] == 0.0);
        for (const OutcomeEntry& entry : distribution.entries) {
            pc_item_state successor;
            PC_CHECK(calc.materialize(entry.state, successor));
            PC_CHECK(
                !item_contains_mod(
                    successor, kZeroGenerationFire));
            PC_CHECK(item_contains_mod(successor, kGoodFire));
        }
    };
    check_exact(empty_rare, 0);
    check_exact(augment_start, 1);
    check_exact(resist_start, 2);

    const std::array<std::pair<ActionParameters, pc_item_state>, 3>
        sampled_cases = {{
            {registry.actions[0].params, empty_rare},
            {registry.actions[1].params, augment_start},
            {registry.actions[2].params, resist_start},
        }};
    for (const auto& [action, start] : sampled_cases) {
        for (std::uint64_t seed = 1; seed <= 128; ++seed) {
            ActionContextImpl sampled(seed);
            sampled.session = session;
            pc_item_state result = start;
            const ActionOutcome outcome =
                apply_action(sampled, &result, action);
            PC_CHECK(outcome.applied);
            PC_CHECK(
                !item_contains_mod(result, kZeroGenerationFire));
            PC_CHECK(item_contains_mod(result, kGoodFire));
        }
    }
}

void run_exact_goal_member_materialization_test() {
    const auto session = make_calc_session();
    session->family_id[0] = 999;
    session->family_id[1] = 999;
    session->family_id[3] = 999;
    session->family_id[4] = 999;
    ActionRegistry registry = build_action_registry(*session);
    GoalSpec goal;
    GoalSlot slot;
    slot.family_id = 999;
    slot.min_tier = 0;
    goal.slots.push_back(slot);
    CalcContext calc(
        session, goal, std::move(registry), {}, false, true, true);

    const auto intern_mod = [&](const std::uint32_t mod) {
        pc_item_state item;
        pc_item_clear(&item);
        item.rarity = PC_RARITY_MAGIC;
        place(
            &item, PC_SIDE_PREFIX, mod,
            static_cast<std::uint16_t>(session->primary_group[mod]));
        return calc.intern_item(item);
    };
    const std::uint32_t same_zero = intern_mod(0);
    const std::uint32_t same_one = intern_mod(1);
    const std::uint32_t different = intern_mod(3);
    PC_CHECK(same_zero == same_one);
    PC_CHECK(same_zero != different);
    PC_CHECK(calc.state(different).goal_member_class_tokens[0] != 0);

    pc_item_state materialized;
    PC_CHECK(calc.materialize(different, materialized));
    PC_CHECK(
        project_item(*session, calc.layout(), materialized) ==
        calc.state(different));
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
    state.prefix_count = 1;
    PC_CHECK(calc.is_goal_state(state));
    state.slot_status[0] =
        static_cast<std::uint8_t>(GoalSlotStatus::Absent);
    state.prefix_count = 0;
    PC_CHECK(!calc.is_goal_state(state));
    state.slot_status[1] =
        static_cast<std::uint8_t>(GoalSlotStatus::Satisfied);
    state.suffix_count = 1;
    PC_CHECK(calc.is_goal_state(state));
    state.prefix_count = 1;
    PC_CHECK(!calc.is_goal_state(state));
    state.prefix_count = 0;
    state.rarity = PC_RARITY_MAGIC;
    PC_CHECK(!calc.is_goal_state(state));

    /* Fracture status is exact carrier/action state, not itself a terminal
     * requirement. A fractured requested mod is valid, while an unrelated
     * fractured explicit remains junk and prevents exact success. */
    pc_item_state fractured_item;
    pc_item_clear(&fractured_item);
    fractured_item.rarity = PC_RARITY_RARE;
    place(
        &fractured_item, PC_SIDE_PREFIX, 0, 10,
        PC_MOD_SLOT_FRACTURED);
    place(
        &fractured_item, PC_SIDE_SUFFIX, 7, 22,
        PC_MOD_SLOT_FRACTURED);
    const std::uint32_t fractured_state =
        calc.intern_item(fractured_item);
    const AbstractState& fractured = calc.state(fractured_state);
    PC_CHECK((fractured.fractured_goal_mask & 1u) != 0);
    PC_CHECK((fractured.flags & kFlagFractured) != 0);
    PC_CHECK(!calc.is_goal_state(fractured));

    pc_item_state exact_fractured_item;
    pc_item_clear(&exact_fractured_item);
    exact_fractured_item.rarity = PC_RARITY_RARE;
    place(
        &exact_fractured_item, PC_SIDE_PREFIX, 0, 10,
        PC_MOD_SLOT_FRACTURED);
    const std::uint32_t exact_fractured_state =
        calc.intern_item(exact_fractured_item);
    PC_CHECK(calc.is_goal_state(calc.state(exact_fractured_state)));

    /* Direct GoalSpec callers retain the old all-slots default. */
    goal.min_satisfied_slots = 0;
    CalcContext all_calc(session, goal, registry, basic_indices(registry));
    state.rarity = PC_RARITY_RARE;
    state.prefix_count = 0;
    PC_CHECK(!all_calc.is_goal_state(state));
    state.slot_status[0] =
        static_cast<std::uint8_t>(GoalSlotStatus::Satisfied);
    state.prefix_count = 1;
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

    /* An already-magic carrier containing only a fracture cannot be Scoured.
     * The native action reports not-applied, so exposing a deterministic
     * self-loop here would let a solver option compile to a strategy that the
     * simulator rejects. */
    {
        pc_item_state item;
        pc_item_clear(&item);
        item.rarity = PC_RARITY_MAGIC;
        place(
            &item, PC_SIDE_PREFIX, 0, 10,
            PC_MOD_SLOT_FRACTURED);
        const std::uint32_t start = calc.intern_item(item);
        PC_CHECK(!action_legal(
            *session, registry.actions.at(scour), calc.state(start)));
        const OutcomeDistribution& no_op = calc.outcomes(start, scour);
        PC_CHECK(no_op.supported);
        PC_CHECK(!no_op.applicable);
        PC_CHECK(no_op.entries.empty());

        pc_item_state normal;
        pc_item_clear(&normal);
        const std::uint32_t normal_state = calc.intern_item(normal);
        PC_CHECK(!action_legal(
            *session, registry.actions.at(scour),
            calc.state(normal_state)));
        const OutcomeDistribution& illegal =
            calc.outcomes(normal_state, scour);
        PC_CHECK(illegal.supported);
        PC_CHECK(!illegal.applicable);
        PC_CHECK(illegal.entries.empty());
    }

    /* Illegal ordinary actions retain the calculator's native self-loop
     * convention. Scour is stricter above because it is emitted inside
     * compound programs and a not-applied Scour cannot be skipped at runtime. */
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

const char* snapshot_source_for_provenance(
    const ExpectedPriceProvenance provenance) {
    switch (provenance) {
    case ExpectedPriceProvenance::Quote: return "quote";
    case ExpectedPriceProvenance::Recipe: return "recipe";
    case ExpectedPriceProvenance::Zero: return "zero";
    case ExpectedPriceProvenance::OwnerDefault: return "owner_default";
    case ExpectedPriceProvenance::Manual: return nullptr;
    case ExpectedPriceProvenance::Mixed: return nullptr;
    }
    return nullptr;
}

void check_action_family_contract(
    const SessionImpl& session,
    const ActionRegistry& registry,
    const std::string& artifact_dir) {
    validate_action_registry_family_contract(registry);

    std::array<bool, kActionTypeCount> emitted_types{};
    bool emitted_restart = false;
    for (const ActionDescriptor& action : registry.actions) {
        const ResolvedRegistryIdentity resolved =
            resolve_registry_identity_contract(action.id);
        if (action.synthetic) {
            emitted_restart = true;
            PC_CHECK(resolved.synthetic);
            PC_CHECK(resolved.price_provenance ==
                     ExpectedPriceProvenance::Manual);
            continue;
        }
        const ActionFamilyContract& contract =
            action_family_contract(action.params.type);
        const std::size_t type =
            static_cast<std::size_t>(action.params.type);
        emitted_types[type] = true;
        PC_CHECK(resolved.family == contract.telemetry_family);
        PC_CHECK(resolved.price_provenance == contract.price_provenance);
        PC_CHECK(contract.telemetry_family !=
                 PrimitiveTelemetryFamily::Other);
        PC_CHECK(support_paths_are_complete(contract.support_paths));
        if (contract.product_reason_group == ProductReasonGroup::Veiled) {
            PC_CHECK(veiled_support_paths_are_deferred(
                contract.support_paths));
        }
        PC_CHECK(action_identity_matches_contract(contract, action.id));
        PC_CHECK(action_cost_keys_match_contract(action, contract));
    }
    PC_CHECK(emitted_restart);
    for (const bool emitted : emitted_types) PC_CHECK(emitted);

    PC_CHECK(primitive_family_for_action(ActionType::EldritchEmber) ==
             PrimitiveTelemetryFamily::EldritchSetup);
    PC_CHECK(primitive_family_for_action(ActionType::EldritchIchor) ==
             PrimitiveTelemetryFamily::EldritchSetup);
    PC_CHECK(primitive_family_for_action(ActionType::EldritchChaos) ==
             PrimitiveTelemetryFamily::EldritchChaos);
    PC_CHECK(primitive_family_for_action(ActionType::EldritchAnnul) ==
             PrimitiveTelemetryFamily::EldritchAnnul);
    PC_CHECK(primitive_family_for_action(ActionType::EldritchExalt) ==
             PrimitiveTelemetryFamily::EldritchExalt);
    PC_CHECK(primitive_family_for_action(ActionType::InfluenceExalt) ==
             PrimitiveTelemetryFamily::InfluenceExalt);
    PC_CHECK(primitive_family_for_action(ActionType::VeiledChaos) ==
             PrimitiveTelemetryFamily::VeiledChaos);
    PC_CHECK(primitive_family_for_action(ActionType::VeiledExalt) ==
             PrimitiveTelemetryFamily::VeiledExalt);
    PC_CHECK(primitive_family_for_action(ActionType::Unveil) ==
             PrimitiveTelemetryFamily::Unveil);

    std::set<std::string_view> automatic_identities;
    constexpr std::array<std::string_view,
                         kAutomaticCandidateKindCount>
        expected_automatic_identities{{
            "none", "fracture", "permanent_bench",
            "temporary_bench_blocker", "protected_metamod",
            "multimod_finish", "imprint", "constructive_renewal",
            "eldritch_side", "cannot_roll", "veiled"}};
    PC_CHECK(automatic_telemetry_kind_for_candidate(
                 AutomaticCandidateKind::None) ==
             AutomaticTelemetryKind::None);
    PC_CHECK(kAutomaticFamilyContracts[0].candidate_identity ==
             expected_automatic_identities[0]);
    for (std::size_t index = 1;
         index < kAutomaticFamilyContracts.size(); ++index) {
        const AutomaticFamilyContract& contract =
            kAutomaticFamilyContracts[index];
        PC_CHECK(static_cast<std::size_t>(contract.candidate_kind) == index);
        PC_CHECK(contract.candidate_identity ==
                 expected_automatic_identities[index]);
        PC_CHECK(contract.telemetry_kind != AutomaticTelemetryKind::None);
        PC_CHECK(automatic_telemetry_kind_for_candidate(
                     contract.candidate_kind) == contract.telemetry_kind);
        PC_CHECK(support_paths_are_complete(contract.support_paths));
        PC_CHECK(contract.support_paths.carrier_generation ==
                 CarrierGenerationPath::CarrierLocalAutomatic);
        PC_CHECK(contract.support_paths.scheduler_admission ==
                 SchedulerAdmissionPath::CarrierLocalAutomatic);
        PC_CHECK(contract.support_paths.row_completion ==
                 RowCompletionPath::AutomaticOption);
        PC_CHECK(contract.support_paths.bellman_q ==
                 BellmanQPath::AutomaticOption);
        PC_CHECK(
            automatic_identities.insert(contract.candidate_identity).second);
    }

    ActionRegistryBuildOptions product_options;
    product_options.exhaustive_fossils = false;
    product_options.goal_relevant_actions = true;
    product_options.automatic_candidates = true;
    const ActionRegistry product_registry =
        build_action_registry(session, product_options);
    validate_action_registry_family_contract(product_registry);
    const auto veiled = product_registry.product_reason_counts.find(
        "filtered_veiled_option_deferred");
    PC_CHECK(veiled != product_registry.product_reason_counts.end());
    if (veiled != product_registry.product_reason_counts.end()) {
        PC_CHECK(veiled->second == 3);
    }
    for (const PrimitiveTelemetryFamily family : {
             PrimitiveTelemetryFamily::VeiledChaos,
             PrimitiveTelemetryFamily::VeiledExalt,
             PrimitiveTelemetryFamily::Unveil}) {
        PC_CHECK(product_registry.product_role_family_counts[
                     static_cast<std::size_t>(ProductActionRole::Filtered)]
                    [static_cast<std::size_t>(family)] == 1);
    }

    namespace fs = std::filesystem;
    const fs::path repo_root =
        fs::absolute(fs::path(artifact_dir))
            .parent_path()
            .parent_path()
            .parent_path();
    const fs::path snapshot_path =
        repo_root / "apps" / "web" / "public" / "economy" /
        "snapshots" /
        "de282eecf6cfdab50666412b94791b68634944ff31921b95e52eeae7758c0fe0.json";
    std::string snapshot_text;
    PC_CHECK(read_text_file(snapshot_path.string(), snapshot_text));
    if (snapshot_text.empty()) return;

    const json::Value snapshot =
        json::Parser(snapshot_text.data(), snapshot_text.size()).parse();
    PC_CHECK(snapshot.at("metadata").at("content_sha256").as_string() ==
             "de282eecf6cfdab50666412b94791b68634944ff31921b95e52eeae7758c0fe0");
    std::map<std::string, std::string> sources;
    for (const auto& [key, source] : snapshot.at("sources").object) {
        sources.emplace(key, source.as_string());
    }
    std::set<std::string> missing;
    for (const json::Value& key :
         snapshot.at("metadata").at("missing_keys").array) {
        missing.insert(key.as_string());
    }

    std::array<bool, 6> observed_provenance{};
    for (const ActionDescriptor& action : registry.actions) {
        const ExpectedPriceProvenance expected =
            expected_price_provenance_for_action(action);
        if (action.synthetic) {
            PC_CHECK(action.cost_keys.size() == 1 &&
                     action.cost_keys.front() == "base");
            PC_CHECK(!sources.contains("base"));
            PC_CHECK(missing.contains("base"));
            observed_provenance[static_cast<std::size_t>(
                ExpectedPriceProvenance::Manual)] = true;
            continue;
        }
        if (action.cost_keys.empty()) {
            PC_CHECK(expected == ExpectedPriceProvenance::Zero);
            const ActionFamilyContract& contract =
                action_family_contract(action.params.type);
            const auto source = sources.find(
                std::string(contract.operation_id));
            PC_CHECK(source != sources.end());
            if (source != sources.end()) {
                PC_CHECK(source->second == "zero");
                observed_provenance[static_cast<std::size_t>(expected)] =
                    true;
            }
            continue;
        }
        const char* expected_source =
            snapshot_source_for_provenance(expected);
        PC_CHECK(expected_source != nullptr);
        for (const std::string& key : action.cost_keys) {
            const auto source = sources.find(key);
            if (source == sources.end()) {
                PC_CHECK(missing.contains(key));
                continue;
            }
            PC_CHECK(source->second == expected_source);
            observed_provenance[static_cast<std::size_t>(expected)] = true;
        }
    }

    PC_CHECK(kSeparateOperationFamilyContracts[0].operation_id ==
             "bestiary:imprint");
    PC_CHECK(kSeparateOperationFamilyContracts[1].operation_id ==
             "bestiary:restore_imprint");
    for (const SeparateOperationFamilyContract& contract :
         kSeparateOperationFamilyContracts) {
        PC_CHECK(contract.telemetry_family ==
                 PrimitiveTelemetryFamily::Bestiary);
        PC_CHECK(support_paths_are_complete(contract.support_paths));
        PC_CHECK(contract.support_paths.compiler ==
                 CompilerCoveragePath::DedicatedSeparateOperation);
        PC_CHECK(contract.support_paths.exact_evaluator ==
                 ExactEvaluatorCoveragePath::DedicatedSeparateOperation);
        PC_CHECK(contract.support_paths.simulator ==
                 SimulatorCoveragePath::DedicatedSeparateOperation);
        PC_CHECK(contract.support_paths.c_api ==
                 CApiCoveragePath::DedicatedBestiaryFacade);
    }
    for (const SeparateOperationPriceComponentContract& component :
         kSeparateOperationPriceComponentContracts) {
        if (component.price_key.empty()) {
            PC_CHECK(component.price_provenance ==
                     ExpectedPriceProvenance::Zero);
            continue;
        }
        const auto source = sources.find(std::string(component.price_key));
        PC_CHECK(source != sources.end());
        const char* expected_source =
            snapshot_source_for_provenance(component.price_provenance);
        PC_CHECK(expected_source != nullptr);
        if (source != sources.end() && expected_source != nullptr) {
            PC_CHECK(source->second == expected_source);
            observed_provenance[static_cast<std::size_t>(
                component.price_provenance)] = true;
        }
    }
    PC_CHECK(observed_provenance[
        static_cast<std::size_t>(ExpectedPriceProvenance::Quote)]);
    PC_CHECK(observed_provenance[
        static_cast<std::size_t>(ExpectedPriceProvenance::Recipe)]);
    PC_CHECK(observed_provenance[
        static_cast<std::size_t>(ExpectedPriceProvenance::Zero)]);
    PC_CHECK(observed_provenance[
        static_cast<std::size_t>(ExpectedPriceProvenance::OwnerDefault)]);
    PC_CHECK(observed_provenance[
        static_cast<std::size_t>(ExpectedPriceProvenance::Manual)]);
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
        const double sigma = std::sqrt(std::max(
            0.0,
            entry.probability * (1.0 - entry.probability) / samples));
        const double tolerance = 5.0 * sigma + slack;
        if (std::fabs(observed - entry.probability) >= tolerance) {
            std::printf(
                "solver calc MC mismatch %s state=%u exact=%.8f observed=%.8f tol=%.8f\n",
                action.id.c_str(), entry.state, entry.probability, observed,
                tolerance);
            std::fflush(stdout);
        }
        PC_CHECK(std::fabs(observed - entry.probability) < tolerance);
        total_checked += observed;
    }
    /* Sampled successors outside the exact support are bounded by the
     * evaluator's truncation budget. */
    PC_CHECK(total_checked > 1.0 - coverage_tolerance - 1e-9);
}

void unveil_mc_cross_check(
    CalcContext& calc,
    ActionContextImpl& mc,
    std::uint32_t veiled_state,
    std::uint32_t veiled_exalt,
    std::uint32_t unveil,
    std::uint32_t samples) {
    const OutcomeDistribution& exact = calc.outcomes(veiled_state, unveil);
    PC_CHECK(exact.supported);
    PC_CHECK(sums_to_one(exact));
    PC_CHECK(!exact.choice_groups.empty());
    double offer_probability = 0.0;
    for (const OutcomeChoiceGroup& group : exact.choice_groups) {
        offer_probability += group.probability;
        PC_CHECK(group.observation_state == veiled_state);
    }
    PC_CHECK(near(offer_probability, 1.0, 1e-8));

    std::map<std::uint32_t, std::uint32_t> state_by_mod;
    for (const OutcomeChoiceOption& option : exact.choice_options) {
        PC_CHECK(option.observation_state == veiled_state);
        PC_CHECK(option.actual_state == option.state);
        state_by_mod.emplace(option.mod_id, option.state);
    }
    const auto better = [&](std::uint32_t a, std::uint32_t b) {
        const auto score = [&](std::uint32_t id) {
            const AbstractState& value = calc.state(id);
            int satisfied = 0;
            int below = 0;
            for (std::size_t i = 0; i < calc.layout().slots.size(); ++i) {
                satisfied += value.slot_status[i] ==
                             static_cast<std::uint8_t>(
                                 GoalSlotStatus::Satisfied);
                below += value.slot_status[i] ==
                         static_cast<std::uint8_t>(
                             GoalSlotStatus::PresentBelowTier);
            }
            return std::tuple<int, int, int>{
                calc.is_goal_state(value) ? 1 : 0, satisfied, below};
        };
        return score(a) != score(b) ? score(a) > score(b) : a < b;
    };

    pc_item_state empty;
    pc_item_clear(&empty);
    empty.rarity = PC_RARITY_RARE;
    std::map<std::uint32_t, std::uint32_t> histogram;
    std::uint32_t accepted = 0;
    while (accepted < samples) {
        pc_item_state offered = empty;
        const ActionOutcome veiled = apply_action(
            mc, &offered,
            calc.registry().actions[veiled_exalt].params);
        if (!veiled.applied || calc.intern_item(offered) != veiled_state) {
            continue;
        }
        int side = -1;
        std::uint32_t index = 0;
        PC_CHECK(pc_item_find_veiled(&offered, &side, &index) ==
                 PC_RESULT_OK);
        const pc_mod_slot& slot = side == PC_SIDE_PREFIX
                                      ? offered.prefixes[index]
                                      : offered.suffixes[index];
        std::uint32_t chosen_mod = kNoId;
        std::uint32_t chosen_state = kNoId;
        for (std::uint8_t option = 0;
             option < slot.veiled_option_count; ++option) {
            const std::uint32_t mod = slot.veiled_option_mod_ids[option];
            const auto found = state_by_mod.find(mod);
            PC_CHECK(found != state_by_mod.end());
            if (found == state_by_mod.end()) continue;
            if (chosen_state == kNoId ||
                better(found->second, chosen_state)) {
                chosen_mod = mod;
                chosen_state = found->second;
            }
        }
        PC_CHECK(chosen_mod != kNoId);
        if (chosen_mod == kNoId) continue;
        ActionParameters choice = calc.registry().actions[unveil].params;
        choice.mod_id = chosen_mod;
        const ActionOutcome revealed = apply_action(mc, &offered, choice);
        PC_CHECK(revealed.applied);
        if (!revealed.applied) continue;
        ++histogram[calc.intern_item(offered)];
        ++accepted;
    }
    for (const OutcomeEntry& entry : exact.entries) {
        const double observed =
            static_cast<double>(histogram[entry.state]) / samples;
        const double sigma = std::sqrt(
            entry.probability * (1.0 - entry.probability) / samples);
        PC_CHECK(std::fabs(observed - entry.probability) <
                 5.0 * sigma + 2e-3);
    }
}

void run_special_evaluator_tests() {
    auto session = make_calc_session();
    ActionRegistry registry = build_action_registry(*session);
    const std::vector<std::string> ids{
        "veiled_chaos", "veiled_exalt", "unveil",
        "eldritch_ember:1", "eldritch_ichor:1", "eldritch_exalt",
        "eldritch_chaos", "eldritch_annul"};
    std::vector<std::uint32_t> candidates = basic_indices(registry);
    for (const std::string& id : ids) {
        const std::uint32_t action = registry.index_by_id.at(id);
        candidates.push_back(action);
        PC_CHECK(calc_supports(registry.actions[action]));
    }
    CalcContext calc(session, family_goal_100(), registry, candidates);
    ActionContextImpl mc(0x5eed);
    mc.session = session;

    pc_item_state empty_rare;
    pc_item_clear(&empty_rare);
    empty_rare.rarity = PC_RARITY_RARE;
    const std::uint32_t empty = calc.intern_item(empty_rare);
    const std::uint32_t veiled_exalt =
        registry.index_by_id.at("veiled_exalt");
    const std::uint32_t veiled_chaos =
        registry.index_by_id.at("veiled_chaos");
    mc_cross_check(calc, mc, empty, veiled_exalt, 20000, 2e-3);
    mc_cross_check(
        calc, mc, empty, veiled_chaos, 20000, 4e-3, 5e-3);

    const OutcomeDistribution& added =
        calc.outcomes(empty, veiled_exalt);
    std::uint32_t veiled_state = kNoId;
    for (const OutcomeEntry& entry : added.entries) {
        if (calc.state(entry.state).veiled_side == PC_SIDE_PREFIX) {
            veiled_state = entry.state;
            break;
        }
    }
    PC_CHECK(veiled_state != kNoId);
    if (veiled_state != kNoId) {
        unveil_mc_cross_check(
            calc, mc, veiled_state, veiled_exalt,
            registry.index_by_id.at("unveil"), 20000);
    }

    mc_cross_check(
        calc, mc, empty, registry.index_by_id.at("eldritch_ember:1"),
        20000);
    mc_cross_check(
        calc, mc, empty, registry.index_by_id.at("eldritch_ichor:1"),
        20000);

    pc_item_state dominant;
    pc_item_clear(&dominant);
    dominant.rarity = PC_RARITY_RARE;
    dominant.searing_exarch_tier = 2;
    dominant.eater_of_worlds_tier = 1;
    place(&dominant, PC_SIDE_PREFIX, 3, 12);
    place(&dominant, PC_SIDE_SUFFIX, 6, 21);
    const std::uint32_t dominated = calc.intern_item(dominant);
    mc_cross_check(
        calc, mc, dominated, registry.index_by_id.at("eldritch_exalt"),
        20000);
    mc_cross_check(
        calc, mc, dominated, registry.index_by_id.at("eldritch_annul"),
        20000);
    mc_cross_check(
        calc, mc, dominated, registry.index_by_id.at("eldritch_chaos"),
        20000, 4e-3, 5e-3);

    /* Owner-approved S7.1 fixture: absent or tied dominance makes all three
     * Eldritch explicit currencies exactly match their ordinary counterparts.
     * Side-specific intent is represented later as an explicit setup option. */
    for (const std::uint8_t tied_tier : {std::uint8_t{0}, std::uint8_t{2}}) {
        pc_item_state tied;
        pc_item_clear(&tied);
        tied.rarity = PC_RARITY_RARE;
        tied.searing_exarch_tier = tied_tier;
        tied.eater_of_worlds_tier = tied_tier;
        place(&tied, PC_SIDE_PREFIX, 3, 12);
        place(&tied, PC_SIDE_SUFFIX, 6, 21);
        const std::uint32_t tied_state = calc.intern_item(tied);
        for (const auto& [ordinary, eldritch] : {
                 std::pair{"exalt", "eldritch_exalt"},
                 std::pair{"chaos", "eldritch_chaos"},
                 std::pair{"annul", "eldritch_annul"},
             }) {
            PC_CHECK(same_distribution(
                calc.outcomes(tied_state, registry.index_by_id.at(ordinary)),
                calc.outcomes(tied_state, registry.index_by_id.at(eldritch))));
        }
    }
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
        reforge_fire.refinement =
            derive_action_refinement_contract(*session, reforge_fire);
        validate_action_refinement_contract(reforge_fire);
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
        augment_fire.refinement =
            derive_action_refinement_contract(*session, augment_fire);
        validate_action_refinement_contract(augment_fire);
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
    check_action_family_contract(*session, registry, dir);
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
                action.params.fossil_indices.size() == 1) {
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
            const std::string& name = data->tag_name_by_id.at(tag);
            if (registry.index_by_id.count("harvest_reforge:" + name) == 0 ||
                registry.index_by_id.count("harvest_augment:" + name) == 0) {
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
        CalcContext projected_reforge_calc(
            session, goal, registry, candidates,
            false, false, false, std::nullopt, {}, false, {}, false,
            true, true);
        CalcContext factored_reforge_calc(
            session, goal, registry, candidates,
            false, false, false, std::nullopt, {}, false, {}, false,
            true, true, false, true);
        ActionContextImpl reforge_mc(31337);
        reforge_mc.session = session;
        const std::uint32_t reforge_start =
            reforge_calc.intern_item(empty_rare);
        const std::uint32_t projected_reforge_start =
            projected_reforge_calc.intern_item(empty_rare);
        const std::vector<std::uint32_t> projected_actions{
            registry.index_by_id.at("chaos"), essence_index,
            harvest_reforge_index, fossil_index};
        const auto compare_real_projected_kernel =
            [&](const pc_item_state& item,
                const std::uint32_t action_index) {
                const std::uint32_t raw_state =
                    reforge_calc.intern_item(item);
                const std::uint32_t projected_state =
                    projected_reforge_calc.intern_item(item);
                const std::uint32_t factored_state =
                    factored_reforge_calc.intern_item(item);
                PC_CHECK(
                    action_legal(
                        *session, registry.actions[action_index],
                        reforge_calc.state(raw_state)) ==
                    action_legal(
                        *session, registry.actions[action_index],
                        projected_reforge_calc.state(projected_state)));
                PC_CHECK(
                    action_legal(
                        *session, registry.actions[action_index],
                        reforge_calc.state(raw_state)) ==
                    action_legal(
                        *session, registry.actions[action_index],
                        factored_reforge_calc.state(factored_state)));
                const OutcomeDistribution& raw_distribution =
                    reforge_calc.outcomes(raw_state, action_index);
                const OutcomeDistribution& projected_distribution =
                    projected_reforge_calc.outcomes(
                        projected_state, action_index);
                PC_CHECK(raw_distribution.supported);
                PC_CHECK(projected_distribution.supported);
                PC_CHECK(sums_to_one(raw_distribution));
                PC_CHECK(sums_to_one(projected_distribution));
                std::map<std::uint32_t, double> raw_mass;
                for (const OutcomeEntry& entry :
                     raw_distribution.entries) {
                    raw_mass[entry.state] += entry.probability;
                }
                const auto projected_mass = project_distribution(
                    projected_reforge_calc, reforge_calc,
                    projected_distribution);
                PC_CHECK(raw_mass.size() == projected_mass.size());
                for (const auto& [state, probability] : raw_mass) {
                    const auto found = projected_mass.find(state);
                    PC_CHECK(found != projected_mass.end());
                    if (found != projected_mass.end()) {
                        PC_CHECK(near(
                            probability, found->second, 1e-11));
                    }
                }
                for (std::size_t goal_slot = 0;
                     goal_slot < kMaxGoalSlots; ++goal_slot) {
                    PC_CHECK(near(
                        raw_distribution
                            .slot_satisfied_probability[goal_slot],
                        projected_distribution
                            .slot_satisfied_probability[goal_slot],
                        1e-11));
                }
                const OutcomeDistribution& raw_gated_distribution =
                    reforge_calc.outcomes(
                        raw_state, action_index, true);
                const OutcomeDistribution& factored_distribution =
                    factored_reforge_calc.outcomes(
                        factored_state, action_index, true);
                PC_CHECK(raw_gated_distribution.supported);
                PC_CHECK(factored_distribution.supported);
                PC_CHECK(sums_to_one(raw_gated_distribution));
                PC_CHECK(sums_to_one(factored_distribution));
                const auto raw_gated_mass = abstract_distribution(
                    reforge_calc, raw_gated_distribution);
                const auto factored_mass = abstract_distribution(
                    factored_reforge_calc, factored_distribution);
                PC_CHECK(
                    raw_gated_mass.size() == factored_mass.size());
                for (const auto& [state, probability] :
                     raw_gated_mass) {
                    const auto found = factored_mass.find(state);
                    PC_CHECK(found != factored_mass.end());
                    if (found != factored_mass.end()) {
                        PC_CHECK(near(
                            probability, found->second, 1e-11));
                    }
                }
                for (std::size_t goal_slot = 0;
                     goal_slot < kMaxGoalSlots; ++goal_slot) {
                    PC_CHECK(near(
                        raw_gated_distribution
                            .slot_satisfied_probability[goal_slot],
                        factored_distribution
                            .slot_satisfied_probability[goal_slot],
                        1e-11));
                    double raw_below = 0.0;
                    double factored_below = 0.0;
                    for (const OutcomeEntry& entry :
                         raw_gated_distribution.entries) {
                        if (reforge_calc.state(entry.state)
                                .slot_status[goal_slot] ==
                            static_cast<std::uint8_t>(
                                GoalSlotStatus::PresentBelowTier)) {
                            raw_below += entry.probability;
                        }
                    }
                    for (const OutcomeEntry& entry :
                         factored_distribution.entries) {
                        if (factored_reforge_calc.state(entry.state)
                                .slot_status[goal_slot] ==
                            static_cast<std::uint8_t>(
                                GoalSlotStatus::PresentBelowTier)) {
                            factored_below += entry.probability;
                        }
                    }
                    PC_CHECK(near(
                        raw_below, factored_below, 1e-11));
                }
            };
        for (const std::uint32_t action_index : projected_actions) {
            compare_real_projected_kernel(empty_rare, action_index);
        }
        std::uint32_t sampled_fractured_mod = kNoId;
        for (std::uint32_t mod = 0; mod < session->mod_count; ++mod) {
            if (session->gen_type[mod] == PC_SIDE_PREFIX &&
                pc_bitset_test(
                    session->normal_random_roll_mask.data(), mod) &&
                pc_bitset_test(
                    session->positive_base_weight_mask.data(), mod)) {
                sampled_fractured_mod = mod;
                break;
            }
        }
        PC_CHECK(sampled_fractured_mod != kNoId);
        if (sampled_fractured_mod != kNoId) {
            pc_item_state sampled_fractured = empty_rare;
            place(
                &sampled_fractured, PC_SIDE_PREFIX,
                sampled_fractured_mod,
                static_cast<std::uint16_t>(
                    session->primary_group[sampled_fractured_mod]),
                PC_MOD_SLOT_FRACTURED);
            for (const std::uint32_t action_index : projected_actions) {
                compare_real_projected_kernel(
                    sampled_fractured, action_index);
            }
        }
        /* The goal slot must appear in the chaos successor support with
         * its exact pool-share probability, however small. */
        const OutcomeDistribution& chaos_dist = reforge_calc.outcomes(
            reforge_start, registry.index_by_id.at("chaos"));
        PC_CHECK(chaos_dist.supported);
        PC_CHECK(chaos_dist.slot_satisfied_probability[0] > 0.0);
        PC_CHECK(!chaos_dist.entries.empty());
        if (!chaos_dist.entries.empty()) {
            const OutcomeDistribution& shared_chaos = reforge_calc.outcomes(
                chaos_dist.entries.front().state,
                registry.index_by_id.at("chaos"));
            PC_CHECK(
                &shared_chaos == &chaos_dist &&
                "reforge states with the same preserved base must share one distribution");
        }
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

        /* Give the policy-parity solve an exact one-affix escape row. Under
         * the exact-goal contract, destructive Rare reforges cannot finish a
         * one-slot goal by themselves because their additional affixes are
         * deliberately nonterminal. The bench row keeps this a Bellman
         * policy test while the reforge rows remain admitted off policy. */
        GoalSpec bellman_goal;
        std::uint32_t bellman_goal_bench_action = kNoId;
        for (std::uint32_t action_index = 0;
             action_index < registry.actions.size() &&
             bellman_goal_bench_action == kNoId;
             ++action_index) {
            const ActionDescriptor& action = registry.actions[action_index];
            if (action.params.type != ActionType::Bench ||
                action.params.mod_id == kNoId) {
                continue;
            }
            const std::uint32_t candidate_group =
                session->primary_group[action.params.mod_id];
            bool naturally_rollable = false;
            for (std::uint32_t mod = 0; mod < session->mod_count; ++mod) {
                if (session->primary_group[mod] == candidate_group &&
                    pc_bitset_test(
                        session->normal_random_roll_mask.data(), mod) &&
                    pc_bitset_test(
                        session->positive_base_weight_mask.data(), mod)) {
                    naturally_rollable = true;
                    break;
                }
            }
            if (!naturally_rollable) continue;
            GoalSlot bellman_slot;
            bellman_slot.group_id = candidate_group;
            bellman_goal.slots = {bellman_slot};
            bellman_goal_bench_action = action_index;
        }
        PC_CHECK(bellman_goal_bench_action != kNoId);
        if (bellman_goal_bench_action == kNoId) return;
        std::vector<std::uint32_t> bellman_candidates = candidates;
        bellman_candidates.push_back(bellman_goal_bench_action);
        CalcContext raw_bellman_calc(
            session, bellman_goal, registry, bellman_candidates);
        CalcContext projected_bellman_calc(
            session, bellman_goal, registry, bellman_candidates,
            false, false, false, std::nullopt, {}, false, {}, false,
            true, true);
        CalcContext factored_bellman_calc(
            session, bellman_goal, registry, bellman_candidates,
            false, false, false, std::nullopt, {}, false, {}, false,
            true, true, false, true);

        std::set<std::string> bellman_price_keys;
        for (const std::uint32_t action_index : bellman_candidates) {
            for (const std::string& key :
                 registry.actions[action_index].cost_keys) {
                bellman_price_keys.insert(key);
            }
        }
        std::unordered_map<std::string, double> bellman_prices;
        std::uint32_t bellman_price_ordinal = 0;
        for (const std::string& key : bellman_price_keys) {
            bellman_prices[key] =
                1.0 + 0.125 * bellman_price_ordinal++;
        }
        SolveOptions bellman_options;
        bellman_options.max_states = 50000;
        bellman_options.max_discovered_states = 50000;
        bellman_options.max_expanded_states = 50000;
        bellman_options.max_state_action_rows = 500000;
        bellman_options.max_transitions = 5000000;
        bellman_options.max_reforge_work = 50000000;
        bellman_options.goal_progress_gated_reforges = true;
        const SolveResult raw_bellman = solve(
            raw_bellman_calc, empty_rare, bellman_prices, bellman_options);
        const SolveResult projected_bellman = solve(
            projected_bellman_calc, empty_rare, bellman_prices,
            bellman_options);
        const SolveResult factored_bellman = solve(
            factored_bellman_calc, empty_rare, bellman_prices,
            bellman_options);
        PC_CHECK(raw_bellman.policy_available);
        PC_CHECK(projected_bellman.policy_available);
        PC_CHECK(factored_bellman.policy_available);
        PC_CHECK(near(
            raw_bellman.evaluated_policy_cost,
            projected_bellman.evaluated_policy_cost,
            1e-9));
        PC_CHECK(near(
            raw_bellman.evaluated_policy_cost,
            factored_bellman.evaluated_policy_cost,
            1e-9));
        if (raw_bellman.policy_available &&
            projected_bellman.policy_available) {
            const PolicyOperatorRef raw_selected =
                raw_bellman.policy[raw_bellman.start_state];
            const PolicyOperatorRef projected_selected =
                projected_bellman.policy[
                    projected_bellman.start_state];
            PC_CHECK(
                raw_selected.kind == projected_selected.kind);
            PC_CHECK(
                raw_bellman_calc.operators()[raw_selected.index].id ==
                projected_bellman_calc
                    .operators()[projected_selected.index].id);
        }
        if (raw_bellman.policy_available &&
            factored_bellman.policy_available) {
            const PolicyOperatorRef raw_selected =
                raw_bellman.policy[raw_bellman.start_state];
            const PolicyOperatorRef factored_selected =
                factored_bellman.policy[
                    factored_bellman.start_state];
            PC_CHECK(raw_selected.kind == factored_selected.kind);
            PC_CHECK(
                raw_bellman_calc.operators()[raw_selected.index].id ==
                factored_bellman_calc
                    .operators()[factored_selected.index].id);
        }

        /* S8.2 owner correction: Fossil and Essence share the same preserved
         * base as the metamod-disabled transition. Locks and cannot-roll
         * modifiers cannot influence either exact kernel, while fractured
         * slots remain independent. */
        const std::uint32_t prefix_lock = registry.index_by_id.at(
            "bench:StrMasterItemGenerationCannotChangePrefixes");
        const std::uint32_t suffix_lock = registry.index_by_id.at(
            "bench:DexMasterItemGenerationCannotChangeSuffixes");
        const auto bench_mod = [&](const std::uint32_t action_index) {
            return registry.actions[action_index].params.mod_id;
        };
        std::uint32_t prefix_marker = kNoId;
        std::uint32_t suffix_marker = kNoId;
        for (std::uint32_t mod = 0; mod < session->mod_count; ++mod) {
            if (!pc_bitset_test(
                    session->normal_random_roll_mask.data(), mod) ||
                !pc_bitset_test(
                    session->positive_base_weight_mask.data(), mod)) {
                continue;
            }
            if (session->gen_type[mod] == 0 && prefix_marker == kNoId) {
                prefix_marker = mod;
            }
            if (session->gen_type[mod] == 1 && suffix_marker == kNoId) {
                suffix_marker = mod;
            }
        }
        PC_CHECK(prefix_marker != kNoId && suffix_marker != kNoId);
        for (const std::uint32_t action_index :
             {fossil_index, essence_index}) {
            const ActionDescriptor& descriptor =
                registry.actions[action_index];
            PC_CHECK(descriptor.preservation.destructive_renewal);
            PC_CHECK(descriptor.preservation.preserves_fractured_affixes);
            PC_CHECK(!descriptor.preservation.respects_prefix_lock);
            PC_CHECK(!descriptor.preservation.respects_suffix_lock);
            PC_CHECK(
                !descriptor.preservation.respects_cannot_roll_attack);
            PC_CHECK(
                !descriptor.preservation.respects_cannot_roll_caster);

            pc_item_state locked_prefix = empty_rare;
            place(
                &locked_prefix, PC_SIDE_PREFIX, prefix_marker,
                session->primary_group[prefix_marker]);
            place(
                &locked_prefix, session->gen_type[bench_mod(prefix_lock)],
                bench_mod(prefix_lock),
                session->primary_group[bench_mod(prefix_lock)],
                PC_MOD_SLOT_CRAFTED);
            const std::uint32_t locked_prefix_state =
                reforge_calc.intern_item(locked_prefix);
            const OutcomeDistribution& prefix_result =
                reforge_calc.outcomes(locked_prefix_state, action_index);
            PC_CHECK(prefix_result.supported);
            for (const OutcomeEntry& entry : prefix_result.entries) {
                const AbstractState& successor =
                    reforge_calc.state(entry.state);
                PC_CHECK((successor.flags &
                          (kFlagPrefixesLocked | kFlagCraftedMod)) == 0);
            }

            pc_item_state locked_suffix = empty_rare;
            place(
                &locked_suffix, PC_SIDE_SUFFIX, suffix_marker,
                session->primary_group[suffix_marker]);
            place(
                &locked_suffix, session->gen_type[bench_mod(suffix_lock)],
                bench_mod(suffix_lock),
                session->primary_group[bench_mod(suffix_lock)],
                PC_MOD_SLOT_CRAFTED);
            const std::uint32_t locked_suffix_state =
                reforge_calc.intern_item(locked_suffix);
            const OutcomeDistribution& suffix_result =
                reforge_calc.outcomes(locked_suffix_state, action_index);
            PC_CHECK(suffix_result.supported);
            for (const OutcomeEntry& entry : suffix_result.entries) {
                const AbstractState& successor =
                    reforge_calc.state(entry.state);
                PC_CHECK((successor.flags &
                          (kFlagSuffixesLocked | kFlagCraftedMod)) == 0);
            }

            pc_item_state fractured = empty_rare;
            place(
                &fractured, session->gen_type[prefix_marker], prefix_marker,
                session->primary_group[prefix_marker],
                PC_MOD_SLOT_FRACTURED);
            const std::uint32_t fractured_state =
                reforge_calc.intern_item(fractured);
            const OutcomeDistribution& fractured_dist =
                reforge_calc.outcomes(fractured_state, action_index);
            for (const OutcomeEntry& entry : fractured_dist.entries) {
                PC_CHECK(
                    reforge_calc.state(entry.state).fractured_goal_mask & 1u);
            }
        }
        PC_CHECK(
            registry.actions[registry.index_by_id.at("chaos")]
                .preservation.respects_prefix_lock);
        PC_CHECK(
            registry.actions[registry.index_by_id.at("chaos")]
                .preservation.respects_cannot_roll_attack);

        /* Vaal Regalia has no rollable attack/caster affixes. Use a wand
         * session for the exact no-filter proof because its live pool
         * contains both tag families. */
        auto tagged_session = std::make_shared<SessionImpl>();
        tagged_session->data = data;
        const auto tagged_base = data->base_by_path.find(
            "Metadata/Items/Weapons/OneHandWeapons/Wands/Wand16");
        PC_CHECK(tagged_base != data->base_by_path.end());
        if (tagged_base == data->base_by_path.end()) return;
        tagged_session->base_index = tagged_base->second;
        tagged_session->item_level = 86;
        build_session(*tagged_session);
        ActionRegistry tagged_registry =
            build_action_registry(*tagged_session);
        std::uint32_t tagged_essence = kNoId;
        std::uint32_t tagged_fossil = kNoId;
        for (std::uint32_t i = 0;
             i < static_cast<std::uint32_t>(
                     tagged_registry.actions.size());
             ++i) {
            const ActionDescriptor& action = tagged_registry.actions[i];
            if (tagged_essence == kNoId &&
                action.params.type == ActionType::Essence) {
                tagged_essence = i;
            }
            if (tagged_fossil == kNoId &&
                action.params.type == ActionType::Fossil &&
                action.params.fossil_indices.size() == 1) {
                tagged_fossil = i;
            }
        }
        PC_CHECK(tagged_essence != kNoId && tagged_fossil != kNoId);
        if (tagged_essence == kNoId || tagged_fossil == kNoId) return;
        for (const auto& [tag_name, metamod_id] :
             std::vector<std::pair<const char*, const char*>>{
                 {"attack",
                  "bench:IntMasterItemGenerationCannotRollAttackAffixes"},
                 {"caster",
                  "bench:StrDexMasterItemGenerationCannotRollCasterAffixes"}}) {
            const auto tag = data->tag_id_by_name.find(tag_name);
            PC_CHECK(tag != data->tag_id_by_name.end());
            if (tag == data->tag_id_by_name.end() ||
                tag->second >= tagged_session->implicit_tag_masks.size() ||
                tagged_session->implicit_tag_masks[tag->second].empty()) {
                PC_CHECK(false);
                continue;
            }
            std::uint32_t goal_mod = kNoId;
            const auto& mask = tagged_session->implicit_tag_masks[tag->second];
            pc_bitset_for_each(
                mask.data(), tagged_session->words,
                [&](const std::size_t raw) {
                    if (goal_mod != kNoId ||
                        raw >= tagged_session->mod_count) {
                        return;
                    }
                    const auto mod = static_cast<std::uint32_t>(raw);
                    if (pc_bitset_test(
                            tagged_session->normal_random_roll_mask.data(),
                            mod) &&
                        pc_bitset_test(
                            tagged_session->positive_base_weight_mask.data(),
                            mod)) {
                        goal_mod = mod;
                    }
                });
            PC_CHECK(goal_mod != kNoId);
            if (goal_mod == kNoId) continue;
            GoalSpec tag_goal;
            GoalSlot tag_slot;
            tag_slot.group_id = tagged_session->primary_group[goal_mod];
            tag_goal.slots.push_back(tag_slot);
            for (const std::uint32_t action_index :
                 {tagged_fossil, tagged_essence}) {
                CalcContext tag_calc(
                    tagged_session, tag_goal, tagged_registry,
                    {action_index});
                pc_item_state protected_item = empty_rare;
                const ActionDescriptor& metamod_action =
                    tagged_registry.actions[
                        tagged_registry.index_by_id.at(metamod_id)];
                const std::uint32_t metamod = metamod_action.params.mod_id;
                place(
                    &protected_item, tagged_session->gen_type[metamod],
                    metamod, tagged_session->primary_group[metamod],
                    PC_MOD_SLOT_CRAFTED | PC_MOD_SLOT_FRACTURED);
                const std::uint32_t protected_state =
                    tag_calc.intern_item(protected_item);
                const OutcomeDistribution& corrected =
                    tag_calc.outcomes(protected_state, action_index);
                PC_CHECK(corrected.slot_satisfied_probability[0] > 0.0);
                ActionContextImpl corrected_mc(700 + action_index);
                corrected_mc.session = tagged_session;
                mc_cross_check(
                    tag_calc, corrected_mc, protected_state, action_index,
                    4000, 1.2e-2, 1.2e-2);
            }
        }
    }

    /* --- S6 Phase 4 gate: every veiled/eldritch evaluator on the real
     * Vaal Regalia session, again against engine Monte Carlo. */
    {
        const std::vector<std::string> fixed_ids{
            "veiled_chaos", "veiled_exalt", "unveil",
            "eldritch_exalt", "eldritch_chaos", "eldritch_annul"};
        std::vector<std::uint32_t> candidates = basic_indices(registry);
        for (const std::string& id : fixed_ids) {
            PC_CHECK(registry.index_by_id.count(id) == 1);
            candidates.push_back(registry.index_by_id.at(id));
        }
        std::uint32_t ember = kNoId;
        std::uint32_t ichor = kNoId;
        for (std::uint32_t i = 0; i < registry.actions.size(); ++i) {
            if (ember == kNoId &&
                registry.actions[i].params.type ==
                    ActionType::EldritchEmber) {
                ember = i;
            }
            if (ichor == kNoId &&
                registry.actions[i].params.type ==
                    ActionType::EldritchIchor) {
                ichor = i;
            }
        }
        PC_CHECK(ember != kNoId && ichor != kNoId);
        if (ember == kNoId || ichor == kNoId) return;
        candidates.push_back(ember);
        candidates.push_back(ichor);

        CalcContext special_calc(session, goal, registry, candidates);
        ActionContextImpl special_mc(0x51a1);
        special_mc.session = session;
        const std::uint32_t special_start =
            special_calc.intern_item(empty_rare);
        const std::uint32_t veiled_exalt =
            registry.index_by_id.at("veiled_exalt");
        mc_cross_check(
            special_calc, special_mc, special_start, veiled_exalt, 20000,
            2e-3);
        mc_cross_check(
            special_calc, special_mc, special_start,
            registry.index_by_id.at("veiled_chaos"), 20000, 4e-3, 5e-3);

        std::uint32_t veiled_state = kNoId;
        for (const OutcomeEntry& entry :
             special_calc.outcomes(special_start, veiled_exalt).entries) {
            if (special_calc.state(entry.state).veiled_side ==
                PC_SIDE_PREFIX) {
                veiled_state = entry.state;
                break;
            }
        }
        PC_CHECK(veiled_state != kNoId);
        if (veiled_state != kNoId) {
            unveil_mc_cross_check(
                special_calc, special_mc, veiled_state, veiled_exalt,
                registry.index_by_id.at("unveil"), 20000);
        }
        mc_cross_check(
            special_calc, special_mc, special_start, ember, 20000);
        mc_cross_check(
            special_calc, special_mc, special_start, ichor, 20000);

        pc_item_state dominant = empty_rare;
        dominant.searing_exarch_tier = 2;
        dominant.eater_of_worlds_tier = 1;
        std::uint32_t prefix_mod = kNoId;
        std::uint32_t suffix_mod = kNoId;
        for (std::uint32_t mod = 0; mod < session->mod_count; ++mod) {
            if (!pc_bitset_test(
                    session->normal_random_roll_mask.data(), mod) ||
                !pc_bitset_test(
                    session->positive_base_weight_mask.data(), mod)) {
                continue;
            }
            if (session->gen_type[mod] == 0 && prefix_mod == kNoId) {
                prefix_mod = mod;
            }
            if (session->gen_type[mod] == 1 && suffix_mod == kNoId) {
                suffix_mod = mod;
            }
        }
        PC_CHECK(prefix_mod != kNoId && suffix_mod != kNoId);
        if (prefix_mod == kNoId || suffix_mod == kNoId) return;
        place(&dominant, PC_SIDE_PREFIX, prefix_mod,
              static_cast<std::uint16_t>(
                  session->primary_group[prefix_mod]));
        place(&dominant, PC_SIDE_SUFFIX, suffix_mod,
              static_cast<std::uint16_t>(
                  session->primary_group[suffix_mod]));
        const std::uint32_t dominated = special_calc.intern_item(dominant);
        mc_cross_check(
            special_calc, special_mc, dominated,
            registry.index_by_id.at("eldritch_exalt"), 20000);
        mc_cross_check(
            special_calc, special_mc, dominated,
            registry.index_by_id.at("eldritch_annul"), 20000);
        mc_cross_check(
            special_calc, special_mc, dominated,
            registry.index_by_id.at("eldritch_chaos"), 20000, 4e-3,
            5e-3);
        std::printf(
            "solver special artifact: %u states after veiled/eldritch\n",
            special_calc.state_count());
    }
}

} // namespace

void run_solver_action_family_contract_tests(const char* artifact_dir) {
    if (artifact_dir == nullptr) {
        std::printf(
            "solver family-contract test skipped (missing artifact path)\n");
        return;
    }
    const std::string dir = artifact_dir;
    std::string manifest_text;
    std::string strings_text;
    std::string game_text;
    if (!read_text_file(dir + "/manifest.json", manifest_text) ||
        !read_text_file(dir + "/strings.json", strings_text) ||
        !read_text_file(dir + "/game-data.json", game_text)) {
        std::printf(
            "solver family-contract test skipped (unreadable artifact)\n");
        PC_CHECK(false);
        return;
    }
    try {
        const std::shared_ptr<DataImpl> data =
            load_data_impl(manifest_text, strings_text, game_text);
        const auto base = data->base_by_path.find(
            "Metadata/Items/Armours/BodyArmours/BodyInt17");
        PC_CHECK(base != data->base_by_path.end());
        if (base == data->base_by_path.end()) return;
        auto session = std::make_shared<SessionImpl>();
        session->data = data;
        session->base_index = base->second;
        session->item_level = 86;
        build_session(*session);
        const ActionRegistry registry = build_action_registry(*session);
        check_action_family_contract(*session, registry, dir);
    } catch (const std::exception& ex) {
        std::printf("solver family-contract test: %s\n", ex.what());
        PC_CHECK(false);
    }
}

void run_solver_calc_tests(const char* artifact_dir) {
    run_product_dead_feature_reduction_tests();
    run_exact_goal_member_materialization_test();
    run_goal_threshold_tests();
    run_exact_distribution_tests();
    run_semantic_exclusion_equivalence_tests();
    run_identity_reforge_factorization_tests();
    run_projected_reforge_frontier_equivalence_tests();
    run_harvest_targeted_natural_regression();
    run_reforge_tests();
    run_special_evaluator_tests();
    run_artifact_calc_tests(artifact_dir);
}

void run_solver_calc_gated_equivalence_tests() {
    run_projected_reforge_frontier_equivalence_tests();
    run_harvest_targeted_natural_regression();
}

void run_solver_phase_lower_tests() {
    using namespace poecraft::solver::quotient;
    const auto rejects = [](const auto& operation) {
        bool refused = false;
        try { operation(); } catch (const std::exception&) { refused = true; }
        PC_CHECK(refused);
    };
    for (const auto& [n, d] : std::vector<std::pair<std::uint64_t, std::uint64_t>>{
            {0, 7}, {1, 3}, {2, 3}, {7, 7}, {1, UINT64_MAX}, {UINT64_MAX-1, UINT64_MAX}}) {
        const auto p = phase_weight_probability(n, d);
        const long double exact = static_cast<long double>(n) / static_cast<long double>(d);
        PC_CHECK(p.lower <= exact && exact <= p.upper);
    }
    rejects([] { phase_weight_probability(2, 1); });
    rejects([] { phase_weight_probability(0, 0); });
    PC_CHECK(phase_two_exit_lower(0, {0.1, 0.3}, 0, 10) <= 7);
    PC_CHECK(phase_two_exit_lower(0, {0.1, 0.3}, 0, 10) > 6.999);
    PC_CHECK(phase_two_exit_lower(0, {0.1, 0.3}, 10, 0) <= 1);
    PC_CHECK(phase_two_exit_lower(0, {0.1, 0.3}, 10, 0) > .999);
    rejects([] { phase_two_exit_lower(0, {.8, .2}, 0, 10); });

    auto session = make_calc_session();
    auto registry = build_action_registry(*session);
    // A complete fixture economy; even Unveil consumes one fixture unit.
    for (auto& action : registry.actions) action.cost_keys = {"fixture:step"};
    const PhaseLowerPrices prices{{"fixture:step", 2}};
    // A fresh-base zero is not a uniform support exclusion: an influence
    // signature can activate it. The ordinary helper retains its old result.
    auto signature_session = make_calc_session();
    pc_bitset_clear(signature_session->positive_spawn_weight_mask.data(), 0);
    const auto& draw_descriptor = registry.actions.at(registry.index_by_id.at("exalt"));
    const auto ordinary_support = action_explicit_affix_reachable_mask(*signature_session, draw_descriptor);
    const auto uniform_support = action_explicit_affix_reachable_mask(*signature_session, draw_descriptor, true);
    PC_CHECK(!pc_bitset_test(ordinary_support.data(), 0));
    PC_CHECK(pc_bitset_test(uniform_support.data(), 0));
    auto goal = family_goal_100(); goal.rarity = PC_RARITY_RARE;
    CalcContext calc(session, goal, registry, basic_indices(registry));
    pc_item_state blocked{}; blocked.rarity = PC_RARITY_RARE; blocked.searing_exarch_tier = 1;
    place(&blocked, PC_SIDE_PREFIX, 2, 10);
    pc_item_state open{}; open.rarity = PC_RARITY_RARE; open.searing_exarch_tier = 1;
    place(&open, PC_SIDE_PREFIX, 3, 12);
    auto donor = PhaseLowerProducer::prepare(calc, prices, blocked, {100, 0});
    PC_CHECK(!donor->original_candidate_accepted);
    PC_CHECK(donor->values[0] > 1.999 && donor->values[0] <= 2);
    PC_CHECK(donor->lookup(calc, prices, blocked) == donor->lookup(calc, prices, open));
    // Same goal/occupancy projection, genuinely different native kernels. The
    // blocked representative accepts h=100, but its hidden open member refutes
    // it. The all-member pointwise repair is valid for both.
    const auto probability = [&](const pc_item_state& item) {
        std::uint64_t hit = 0;
        const auto total = calc.phase_lower_add_weights(item, registry.index_by_id.at("eldritch_exalt"),
            [&](const pc_item_state& exit, std::uint64_t weight) {
                if (item_contains_mod(exit, 0)) hit += weight;
            });
        return phase_weight_probability(hit, total);
    };
    const auto blocked_p = probability(blocked), open_p = probability(open);
    PC_CHECK(blocked_p.upper == 0 && open_p.lower > .02);
    PC_CHECK(2 + (1-blocked_p.upper)*100 >= 100);
    PC_CHECK(2 + (1-open_p.lower)*100 < 100);
    PC_CHECK(phase_two_exit_lower(2, blocked_p, 0, donor->values[0]) >= donor->values[0]);
    PC_CHECK(phase_two_exit_lower(2, open_p, 0, donor->values[0]) >= donor->values[0]);
    auto phase = open; phase.eater_of_worlds_tier = 1;
    PC_CHECK(!donor->lookup(calc, prices, phase));
    auto repriced = prices; repriced["fixture:step"] = 3;
    PC_CHECK(!donor->lookup(calc, repriced, open));
    auto fresh = PhaseLowerProducer::prepare(calc, prices, open, {100, 0});
    PC_CHECK(fresh->identity == donor->identity && fresh->values == donor->values);
    auto changed = PhaseLowerProducer::prepare(calc, repriced, open, {100, 0});
    PC_CHECK(changed->identity != donor->identity && changed->values != donor->values);
    auto new_phase = PhaseLowerProducer::prepare(calc, prices, phase, {100, 0});
    PC_CHECK(new_phase->lookup(calc, prices, phase).has_value());
    auto free_prices = prices; free_prices["fixture:step"] = 0;
    auto zero_escape = PhaseLowerProducer::prepare(calc, free_prices, open, {100, 0});
    PC_CHECK(zero_escape->values[0] == 0);
    QuotientLowerBudget cancel; cancel.cancelled = [] { return true; };
    rejects([&] { PhaseLowerProducer::prepare(calc, prices, open, {100, 0}, cancel); });
    QuotientLowerBudget tiny; tiny.max_scratch_bytes = 64;
    rejects([&] { PhaseLowerProducer::prepare(calc, prices, open, {100, 0}, tiny); });
    auto unknown = goal; unknown.automatic_candidate_kind_mask |= 1u << 31;
    CalcContext unknown_calc(session, unknown, registry, basic_indices(registry));
    rejects([&] { PhaseLowerProducer::prepare(unknown_calc, prices, open, {100, 0}); });

    GoalSlot suffix; suffix.family_id = 104; suffix.min_tier = 1;
    goal.slots.push_back(suffix); goal.automatic_candidates = true;
    goal.automatic_candidate_kind_mask = automatic_candidate_kind_bit(AutomaticCandidateKind::EldritchSide);
    CalcContext program_calc(session, goal, registry, basic_indices(registry));
    pc_item_state source{}; source.rarity = PC_RARITY_RARE;
    place(&source, PC_SIDE_PREFIX, 0, 10);
    auto post = source; post.eater_of_worlds_tier = 1;
    auto program_donor = PhaseLowerProducer::prepare(program_calc, prices, post, {100, 100, 100, 0});
    const std::string id = "option:eldritch_side_intent:suffix:eldritch_exalt:eldritch_ichor:1";
    const auto before = program_donor->memory_snapshot().total_bytes;
    {
        const auto proof = PhaseLowerProducer::compose(program_calc, prices, source, id, *program_donor);
        const auto& r = proof.record;
        PC_CHECK(r.cost_lower <= 4 && r.cost_lower > 3.999);
        PC_CHECK(r.goal_weight > 0 && r.goal_weight < r.total_weight);
        const long double exact = 4 + (1-static_cast<long double>(r.goal_weight)/r.total_weight)*r.failure_lower_min;
        PC_CHECK(r.lower <= exact && r.lower > exact-1e-10L);
        PC_CHECK(r.physical_exits > 1);
        auto second = source; place(&second, PC_SIDE_PREFIX, 3, 12);
        const auto other = PhaseLowerProducer::compose(program_calc, prices, second, id, *program_donor);
        PC_CHECK(other.record.source != r.source);
        PC_CHECK(other.record.goal_weight == 0 && other.record.failure_lower_min == 0);
        PC_CHECK(other.record.lower <= 4);
    }
    PC_CHECK(program_donor->memory_snapshot().total_bytes == before);
    rejects([&] { PhaseLowerProducer::compose(program_calc, prices, source, id, *program_donor, cancel); });
    rejects([&] { PhaseLowerProducer::compose(program_calc, prices, source, "missing internal choice", *program_donor); });
    rejects([&] { PhaseLowerProducer::compose(program_calc, repriced, source, id, *program_donor); });
    PC_CHECK(program_donor->memory_snapshot().total_bytes == before);
    static_assert(!std::is_default_constructible_v<PreparedPhaseLowerView>);
    static_assert(!std::is_default_constructible_v<PhaseProgramLowerWitness>);
    static_assert(!std::is_constructible_v<PreparedPhaseLowerView, QuotientLowerCertificate>);
    static_assert(!std::is_move_constructible_v<PhaseProgramLowerWitness>);
}
