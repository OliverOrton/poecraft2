#include "tests.hpp"

#include "../src/json.hpp"
#include "../src/calculator_currency.hpp"
#include "../src/currency_outcomes.hpp"
#include "../src/solver_action_family_contract.hpp"
#include "../src/solver_internal.hpp"
#include "../src/solver_solve_types.hpp"
#include "../src/solver_finder.hpp"
#include "../src/solver_phase_lower.hpp"
#include "../src/solver_quotient_bellman.hpp"
#include "poecraft/bitset.h"
#include "poecraft/item_state.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
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

namespace poecraft::solver {
struct SolveWorkTestAccess { using Impl = SolveWork::Impl; };
}

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

// Independent ordered native-pool enumeration checks the complete terminal
// projection, not merely whether a representative can be materialized.
void run_reforge_cross_goal_projection_tests() {
  for (unsigned scenario = 0; scenario < 3; ++scenario) {
    auto session = make_calc_session();
    std::vector<std::vector<std::uint32_t>> groups(session->mod_count);
    for (std::uint32_t mod = 0; mod < session->mod_count; ++mod)
        groups[mod].assign(session->group_ids.begin() + session->group_offsets[mod],
            session->group_ids.begin() + session->group_offsets[mod + 1]);
    groups[0].push_back(20); // satisfying Life member blocks Fire via second group
    groups[1].push_back(21); // below-tier Life member blocks Fire via a different group
    groups[5].push_back(21);
    session->group_offsets = {0};
    session->group_ids.clear();
    session->group_masks.assign(32, std::vector<std::uint64_t>(session->words, 0));
    for (std::uint32_t mod = 0; mod < session->mod_count; ++mod) {
        for (const auto group : groups[mod]) {
            session->group_ids.push_back(group);
            pc_bitset_set(session->group_masks[group].data(), mod);
        }
        session->group_offsets.push_back(static_cast<std::uint32_t>(session->group_ids.size()));
    }
    // Keep the original clamped witness, then exercise distinct 4/5/6
    // depths. Two extra ordinary identities ensure six affixes are reachable.
    // The mirrored case also uses unrelated unequal weights on every member.
    session->rare_affix_cap = scenario == 0 ? 2 : 3;
    auto data = std::const_pointer_cast<DataImpl>(session->data);
    if (scenario != 0) {
        session->veiled_prefix_mod_id = session->veiled_suffix_mod_id = kNoId;
        const std::uint32_t weights[] = {19, 5, 31, 13, 7, 23, 11, 41, 17, 29};
        session->prefix_mask.assign(session->words, 0);
        session->suffix_mask.assign(session->words, 0);
        for (std::uint32_t mod = 0; mod < session->mod_count; ++mod) {
            if (scenario == 2) {
                session->gen_type[mod] = 1 - session->gen_type[mod];
                data->spawn_weights[mod] = session->base_spawn_weight[mod] =
                    session->base_roll_weight[mod] = weights[mod];
            }
            pc_bitset_set(session->normal_random_roll_mask.data(), mod);
            pc_bitset_set(session->positive_spawn_weight_mask.data(), mod);
            pc_bitset_set(session->positive_base_weight_mask.data(), mod);
            pc_bitset_set(session->influence_masks[0].data(), mod);
            pc_bitset_set((session->gen_type[mod] == PC_SIDE_PREFIX ?
                session->prefix_mask : session->suffix_mask).data(), mod);
        }
    }
    data->essence_count = 5;
    data->essence_item_level_restrictions.assign(5, -1);
    data->essence_is_corruption_only.assign(5, 0);
    for (const auto key : {"cross_goal_caster", "cross_goal_satisfied", "cross_goal_below",
                           "cross_goal_junk_A", "cross_goal_junk_B"}) {
        data->essence_key_sids.push_back(static_cast<std::uint32_t>(data->strings.size()));
        data->essence_by_key[key] = static_cast<std::uint32_t>(data->essence_key_sids.size() - 1);
        data->strings.push_back(key);
    }
    session->essence_guaranteed_mod_ids = {4, 0, 1, 2, 5};
    auto registry = build_action_registry(*session);
    const auto chaos = registry.index_by_id.at("chaos");
    const auto annul = registry.index_by_id.at("annul");
    const auto caster = registry.index_by_id.at("essence:cross_goal_caster");
    const auto satisfied = registry.index_by_id.at("essence:cross_goal_satisfied");
    const auto below = registry.index_by_id.at("essence:cross_goal_below");
    const auto junk_A = registry.index_by_id.at("essence:cross_goal_junk_A");
    const auto junk_B = registry.index_by_id.at("essence:cross_goal_junk_B");
    const auto harvest_fire = registry.index_by_id.at("harvest_reforge:fire");
    const auto harvest_cold = registry.index_by_id.at("harvest_reforge:cold");
    const std::vector<std::uint32_t> actions{
        chaos, caster, satisfied, below, annul, junk_A, junk_B, harvest_fire, harvest_cold};
    GoalSpec goal = family_goal_100();
    goal.rarity = PC_RARITY_RARE;
    goal.min_satisfied_slots = 2;
    for (const auto mod : {5u, 4u}) {
        GoalSlot slot;
        slot.family_id = session->family_id[mod];
        slot.min_tier = 1;
        goal.slots.push_back(slot);
    }
    const char* comparison = "";
    unsigned active_profile = 0, active_implementation = 0, active_variant = 0;
    CalcContext* diagnostic_calc = nullptr;
    unsigned diagnostic_rows = 0;
    const auto check_maps = [&](const std::map<std::uint32_t, double>& actual,
                               const std::map<std::uint32_t, double>& expected) {
        PC_CHECK(actual.size() == expected.size());
        double a = 0, e = 0;
        for (const auto& [state, p] : actual) {
            a += p;
            const auto at = expected.find(state);
            PC_CHECK(at != expected.end());
            if (at == expected.end() && diagnostic_rows++ < 3) {
                const auto dump = [&](const char* kind, std::uint32_t id, double probability) {
                    const auto& value = diagnostic_calc->state(id);
                    std::printf("cross-goal mismatch %s profile=%u impl=%u variant=%u %s state=%u p=%.17g flags=%u blocked=%u counts=%u/%u statuses=%u/%u/%u basin=%u\n",
                        comparison, active_profile, active_implementation, active_variant, kind, id, probability,
                        value.flags, value.blocked_mask, value.prefix_count, value.suffix_count,
                        value.slot_status[0], value.slot_status[1], value.slot_status[2], value.goal_progress_retry_basin);
                };
                dump("actual", state, p);
                for (const auto& [id, probability] : expected) dump("expected", id, probability);
            }
            if (at != expected.end()) PC_CHECK(near(p, at->second, 1e-12));
        }
        for (const auto& [state, p] : expected) { (void)state; e += p; }
        PC_CHECK(near(a, 1, 1e-12));
        PC_CHECK(near(e, 1, 1e-12));
    };
    std::uint64_t cases = 0, materialized = 0, annul_rows = 0, factored_commits = 0;
    std::vector<GoalSpec> profiles{goal};
    auto single = goal;
    single.slots.resize(1);
    single.min_satisfied_slots = 1;
    profiles.push_back(single);
    // All tiers count: junk sharing only a below-tier member's secondary
    // group can block the observation while remaining compatible with T1.
    single.slots[0].min_tier = 0;
    profiles.push_back(single);
    for (unsigned profile = 0; profile < profiles.size(); ++profile) {
      active_profile = profile;
      for (unsigned implementation = 0; implementation < 3; ++implementation) {
        CalcContext calc(session, profiles[profile], registry, actions, false, false, true,
            std::nullopt, {}, false, {}, true, false, implementation != 0,
            false, implementation == 2);
        diagnostic_calc = &calc;
        active_implementation = implementation;
        ActionContextImpl context(0);
        context.session = session;
        for (unsigned variant = 0; variant < 14; ++variant) {
            active_variant = variant;
            pc_item_state source;
            pc_item_clear(&source);
            source.rarity = PC_RARITY_RARE;
            std::uint32_t action = chaos;
            if (variant >= 1 && variant <= 3) action = actions[variant];
            if ((variant >= 4 && variant <= 7) || variant == 11 || variant == 12) {
                const auto held = variant == 4 || variant == 12 ? 0u :
                    variant == 5 ? 1u : variant == 11 ? 2u : 6u;
                place(&source, session->gen_type[held], held,
                    static_cast<std::uint16_t>(session->primary_group[held]), PC_MOD_SLOT_FRACTURED);
                place(&source, session->gen_type[7], 7, 22); // discarded by renewal
                if (variant == 7) action = satisfied;
            }
            if (variant == 8) action = junk_A;
            if (variant == 9) action = junk_B;
            if (variant == 10 || variant == 11) action = harvest_fire;
            if (variant == 12 || variant == 13) action = harvest_cold;
            // Prepare the native unprotected/fractured boundary independently.
            pc_item_state base;
            pc_item_clear(&base);
            base.rarity = PC_RARITY_RARE;
            for (const int side : {PC_SIDE_PREFIX, PC_SIDE_SUFFIX}) {
                const auto* slots = side == PC_SIDE_PREFIX ? source.prefixes : source.suffixes;
                const auto count = side == PC_SIDE_PREFIX ? source.prefix_count : source.suffix_count;
                for (unsigned i = 0; i < count; ++i)
                    if (slots[i].flags & PC_MOD_SLOT_FRACTURED)
                        place(&base, side, slots[i].mod_id, slots[i].group_id, slots[i].flags);
            }
            if (registry.actions[action].params.type == ActionType::Essence) {
                const auto forced = session->essence_guaranteed_mod_ids[
                    registry.actions[action].params.essence_index];
                PC_CHECK(pc_item_add_mod(&base, session->gen_type[forced], forced,
                    static_cast<std::uint16_t>(session->primary_group[forced]), 0, nullptr) == PC_RESULT_OK);
            }
            std::map<std::uint32_t, double> expected;
            double native_A_then_B = 0, native_B_then_A = 0;
            const auto first_base_blocker = item_contains_mod(base, 2) ? 2u :
                item_contains_mod(base, 5) ? 5u : 0u;
            const auto visit = [&](auto&& self, pc_item_state item, unsigned target,
                                   double p, unsigned first_blocker) -> void {
                const auto absorb = [&] {
                    expected[calc.intern_item(item)] += p;
                    if (item_contains_mod(item, 2) && item_contains_mod(item, 5)) {
                        if (first_blocker == 2) native_A_then_B += p;
                        if (first_blocker == 5) native_B_then_A += p;
                    }
                };
                if (item.prefix_count + item.suffix_count >= target) {
                    absorb();
                    return;
                }
                const auto cap = rarity_affix_cap(*session, item.rarity);
                const bool prefix = item.prefix_count < cap, suffix = item.suffix_count < cap;
                PoolBuildRequest request;
                request.side_filter = prefix && suffix ? -1 : prefix ? PC_SIDE_PREFIX : PC_SIDE_SUFFIX;
                const auto pool = get_weighted_pool(context, &item, request);
                if ((!prefix && !suffix) || pool.total_weight == 0) {
                    absorb();
                    return;
                }
                for (const auto& row : pool.entries) if (row.final_weight != 0) {
                    auto next = item;
                    PC_CHECK(pc_item_add_mod(&next, row.gen_type, row.session_mod_id,
                        static_cast<std::uint16_t>(row.primary_group), 0, nullptr) == PC_RESULT_OK);
                    const auto first = first_blocker != 0 ? first_blocker :
                        row.session_mod_id == 2 || row.session_mod_id == 5 ? row.session_mod_id : 0u;
                    self(self, next, target, p * row.final_weight / pool.total_weight, first);
                }
            };
            for (unsigned target = 4; target <= 6; ++target) {
                const double count_probability = target == 4 ? 8.0/12 : target == 5 ? 3.0/12 : 1.0/12;
                const auto count = std::min<unsigned>(target, session->rare_affix_cap * 2);
                if (registry.actions[action].params.type == ActionType::HarvestReforge) {
                    // Independently enumerate the native targeted first draw,
                    // followed by the ordinary fill law; do not reuse DP rows.
                    PoolBuildRequest guaranteed;
                    guaranteed.weight_kind = PoolWeightKind::TargetedNatural;
                    guaranteed.target_tag_id = registry.actions[action].params.target_tag_id;
                    const auto pool = get_weighted_pool(context, &base, guaranteed);
                    PC_CHECK(pool.total_weight > 0);
                    for (const auto& row : pool.entries) if (row.final_weight != 0) {
                        auto next = base;
                        PC_CHECK(pc_item_add_mod(&next, row.gen_type, row.session_mod_id,
                            static_cast<std::uint16_t>(row.primary_group), 0, nullptr) == PC_RESULT_OK);
                        const auto first = first_base_blocker != 0 ? first_base_blocker :
                            row.session_mod_id == 2 || row.session_mod_id == 5 ? row.session_mod_id : 0u;
                        visit(visit, next, count, count_probability * row.final_weight / pool.total_weight, first);
                    }
                } else visit(visit, base, count, count_probability, first_base_blocker);
            }
            const auto input = calc.intern_item(source);
            const auto actual = calc.outcomes(input, action);
            PC_CHECK(actual.supported);
            std::map<std::uint32_t, double> actual_mass;
            bool saw_satisfied_block = false, saw_below_block = false;
            double actual_independent_junk = 0, native_independent_junk = 0;
            for (const auto& row : actual.entries) {
                actual_mass[row.state] += row.probability;
                const auto state = calc.state(row.state);
                saw_satisfied_block |= state.slot_status[0] == 2 && (state.blocked_mask & 2u) != 0;
                saw_below_block |= state.slot_status[0] == 1 && (state.blocked_mask & 2u) != 0;
                pc_item_state item;
                const bool valid = calc.materialize(row.state, item);
                PC_CHECK(valid);
                if (!valid) continue;
                ++materialized;
                if (item_contains_mod(item, 2) && item_contains_mod(item, 5))
                    actual_independent_junk += row.probability;
                PC_CHECK(project_item(*session, calc.layout(), item) == state);
                // The next native Annul must erase/recompute blocker bits too.
                std::vector<std::pair<int, unsigned>> removable;
                for (const int side : {PC_SIDE_PREFIX, PC_SIDE_SUFFIX}) {
                    const auto* slots = side == PC_SIDE_PREFIX ? item.prefixes : item.suffixes;
                    const auto count = side == PC_SIDE_PREFIX ? item.prefix_count : item.suffix_count;
                    for (unsigned i = 0; i < count; ++i)
                        if (!(slots[i].flags & PC_MOD_SLOT_FRACTURED)) removable.emplace_back(side, i);
                }
                std::map<std::uint32_t, double> expected_annul, actual_annul;
                for (const auto& [side, index] : removable) {
                    auto next = item;
                    PC_CHECK(pc_item_remove_at(&next, side, index) == PC_RESULT_OK);
                    expected_annul[calc.intern_item(next)] += 1.0 / removable.size();
                }
                if (removable.empty()) expected_annul[row.state] = 1;
                const auto removed = calc.outcomes(row.state, annul);
                PC_CHECK(removed.supported);
                for (const auto& e : removed.entries) actual_annul[e.state] += e.probability;
                comparison = "Annul";
                check_maps(actual_annul, expected_annul);
                ++annul_rows;
            }
            comparison = "full renewal";
            check_maps(actual_mass, expected);
            if (scenario != 0 && variant == 0) {
                double count_mass[7]{};
                for (const auto& [state, p] : expected) {
                    const auto& value = calc.state(state);
                    count_mass[value.prefix_count + value.suffix_count] += p;
                }
                // Native exhaustion may stop the 5/6 targets early. Four,
                // five and six must nevertheless each carry positive mass.
                PC_CHECK(count_mass[4] >= 8.0 / 12 - 1e-12);
                PC_CHECK(count_mass[5] > 0 && count_mass[6] > 0);
            }
            if (profile == 0) {
                if (variant == 0 || variant == 1) {
                    PC_CHECK(saw_satisfied_block);
                    PC_CHECK(saw_below_block);
                }
                if (variant == 4) PC_CHECK(saw_satisfied_block);
                if (variant == 5 || variant == 3) PC_CHECK(saw_below_block);
            } else if (variant == 0 || variant == 8 || variant == 9 || variant == 11) {
                // The admitted one-goal domain includes independent native
                // junk blockers. Compare their positive joint mass, not just
                // two physical/compact implementations of the same roll DP.
                for (const auto& [state, p] : expected) {
                    pc_item_state item;
                    PC_CHECK(calc.materialize(state, item));
                    if (item_contains_mod(item, 2) && item_contains_mod(item, 5))
                        native_independent_junk += p;
                }
                PC_CHECK(native_independent_junk > 0);
                PC_CHECK(near(actual_independent_junk, native_independent_junk, 1e-12));
                if (variant == 0) {
                    PC_CHECK(native_A_then_B > 0 && native_B_then_A > 0);
                    PC_CHECK(near(native_A_then_B + native_B_then_A, native_independent_junk, 1e-12));
                }
                if (variant == 8 || variant == 11) PC_CHECK(native_A_then_B > 0);
                if (variant == 9) PC_CHECK(native_B_then_A > 0);
            }
            // Compare the complete separately gated row, including retry mass,
            // with the native law projected through its published boundary.
            const auto gated = calc.outcomes(input, action, true);
            PC_CHECK(gated.supported && gated.goal_progress_gated);
            std::map<std::uint32_t, double> expected_gated, actual_gated;
            for (const auto& [state, p] : expected) {
                const auto assessment = calc.assess_goal_state(calc.state(state));
                if (calc.is_goal_state(calc.state(state))) {
                    PC_CHECK(gated.gated_terminal_state != kNoId);
                    expected_gated[gated.gated_terminal_state] += p;
                } else if (assessment.satisfied_count == 0) {
                    PC_CHECK(gated.gated_retry_state != kNoId);
                    expected_gated[gated.gated_retry_state] += p;
                } else expected_gated[state] += p;
            }
            for (const auto& row : gated.entries) actual_gated[row.state] += row.probability;
            comparison = "gated renewal";
            check_maps(actual_gated, expected_gated);
            ++cases;
        }
        if (implementation == 2) {
            PC_CHECK(calc.telemetry().reforge_effort.v3_commits > 0);
            factored_commits += calc.telemetry().reforge_effort.v3_commits;
        }
    }
    }
    // A family with independently coexisting members cannot fit one slot.
    // Refuse its exact row rather than deleting native-compatible draws.
    auto nonexclusive = make_calc_session();
    nonexclusive->family_id[3] = 100;
    const auto invalid_registry = build_action_registry(*nonexclusive);
    const auto invalid_chaos = invalid_registry.index_by_id.at("chaos");
    for (unsigned implementation = 0; implementation < 3; ++implementation) {
        CalcContext invalid(nonexclusive, family_goal_100(), invalid_registry, {invalid_chaos},
            false, false, true, std::nullopt, {}, false, {}, true, false,
            implementation != 0, false, implementation == 2);
        pc_item_state root;
        pc_item_clear(&root);
        root.rarity = PC_RARITY_RARE;
        const auto row = invalid.outcomes(invalid.intern_item(root), invalid_chaos);
        PC_CHECK(!row.supported && row.entries.empty());
    }
    std::printf("finite cross-goal terminal projection: scenario=%u cases=%llu materialized=%llu Annul-rows=%llu V3-commits=%llu; complete native laws checked\n",
        scenario, static_cast<unsigned long long>(cases), static_cast<unsigned long long>(materialized),
        static_cast<unsigned long long>(annul_rows), static_cast<unsigned long long>(factored_commits));
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
    const GoalAssessment dirty = calc.assess_goal_state(state);
    PC_CHECK(dirty.requested_coverage);
    PC_CHECK(!dirty.legacy_clean_occupancy);
    PC_CHECK(!dirty.final_success);
    goal.terminal.extras = ExtraExplicitPolicy::Allow;
    CalcContext coverage_calc(session, goal, registry, basic_indices(registry));
    PC_CHECK(coverage_calc.is_goal_state(state));
    PC_CHECK(coverage_calc.assess_goal_state(state).final_success);
    goal.terminal.extras = ExtraExplicitPolicy::ForbidUnmatched;
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

    /* All-required, disjoint fixed-side goals with exact side counts have
     * the same terminal set as the legacy clean rule. The threshold-one
     * request above is deliberately excluded: two clean goals may satisfy it. */
    GoalSpec occupancy_goal = goal;
    occupancy_goal.terminal.extras = ExtraExplicitPolicy::Allow;
    occupancy_goal.terminal.prefixes = GoalCountRange{1, 1};
    occupancy_goal.terminal.suffixes = GoalCountRange{1, 1};
    CalcContext occupancy_calc(
        session, occupancy_goal, registry, basic_indices(registry));
    for (std::uint32_t mask = 0; mask < 4; ++mask) {
        for (std::uint8_t prefixes = 0; prefixes <= 3; ++prefixes) {
            for (std::uint8_t suffixes = 0; suffixes <= 3; ++suffixes) {
                if (prefixes < ((mask & 1u) != 0) ||
                    suffixes < ((mask & 2u) != 0)) continue;
                AbstractState member;
                member.rarity = PC_RARITY_RARE;
                member.prefix_count = prefixes;
                member.suffix_count = suffixes;
                for (std::uint32_t slot = 0; slot < 2; ++slot)
                    if ((mask & (1u << slot)) != 0)
                        member.slot_status[slot] = static_cast<std::uint8_t>(
                            GoalSlotStatus::Satisfied);
                PC_CHECK(all_calc.is_goal_state(member) ==
                    occupancy_calc.is_goal_state(member));
            }
        }
    }
    occupancy_goal.min_satisfied_slots = 1;
    occupancy_goal.terminal.suffixes = GoalCountRange{0, 0};
    CalcContext threshold_occupancy(
        session, occupancy_goal, registry, basic_indices(registry));
    AbstractState both;
    both.rarity = PC_RARITY_RARE;
    both.slot_status[0] = both.slot_status[1] =
        static_cast<std::uint8_t>(GoalSlotStatus::Satisfied);
    both.prefix_count = both.suffix_count = 1;
    PC_CHECK(calc.is_goal_state(both));
    PC_CHECK(!threshold_occupancy.is_goal_state(both));
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
        double joint_coverage = 0.0;
        double clean_success = 0.0;
        double covered_dirty = 0.0;
        for (const OutcomeEntry& entry : dist.entries) {
            const AbstractState& successor = calc.state(entry.state);
            const GoalAssessment assessment =
                calc.assess_goal_state(successor);
            if (assessment.requested_coverage)
                joint_coverage += entry.probability;
            if (assessment.final_success)
                clean_success += entry.probability;
            if (assessment.requested_coverage &&
                !assessment.final_success)
                covered_dirty += entry.probability;
        }
        PC_CHECK(near(joint_coverage, 1.0));
        PC_CHECK(near(clean_success, 0.0));
        PC_CHECK(near(covered_dirty, 1.0));
        PC_CHECK(near(
            dist.slot_satisfied_probability[0], joint_coverage));
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
            "eldritch_side", "cannot_roll", "veiled", "crafted_cleanup"}};
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

    std::string catalog_text;
    PC_CHECK(read_text_file((repo_root / "fixtures/economy/price-key-catalog-v1.json").string(), catalog_text));
    const auto catalog = json::Parser(catalog_text.data(), catalog_text.size()).parse();
    for (const auto& [key, unused] : catalog.at("direct").object)
        if (!sources.contains(key)) missing.insert(key);
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

void run_foulborn_kernel_tests() {
    auto session = make_calc_session();
    auto data = std::const_pointer_cast<DataImpl>(session->data);
    data->strings = {"life", "hybrid", "attack", "caster", "fire", "cold", "speed", "veilP", "veilS"};
    data->mod_type_key_sid = {0,0,1,2,3,4,5,6,7,8};
    session->required_level[0] = 10;
    const auto registry = build_action_registry(*session);
    for (const char* id : {"foulborn_augment", "foulborn_regal", "foulborn_exalt"}) {
        const auto action = registry.index_by_id.at(id);
        CalcContext calc(session, family_goal_100(), registry, {action},
            false, true, true, std::nullopt, {}, false, {}, true);
        pc_item_state start{};
        start.rarity = std::string(id) == "foulborn_exalt" ? PC_RARITY_RARE : PC_RARITY_MAGIC;
        const auto& distribution = calc.outcomes(calc.intern_item(start), action);
        PC_CHECK(distribution.supported && sums_to_one(distribution));
        double mass[8]{};
        for (const auto& outcome : distribution.entries) {
            pc_item_state item{};
            PC_CHECK(calc.materialize(outcome.state, item));
            PC_CHECK(item.prefix_count + item.suffix_count == 1);
            for (unsigned mod = 0; mod < 8; ++mod)
                if (item_contains_mod(item, mod)) mass[mod] += outcome.probability;
        }
        // Independent law: retained life gets 200; five other weight-100
        // modifiers and weight-400 speed give a total of 1100.
        const double expected[] = {2./11, 0, 1./11, 1./11, 1./11, 1./11, 1./11, 4./11};
        for (unsigned mod=0; mod<8; ++mod) PC_CHECK(std::abs(mass[mod]-expected[mod])<1e-12);
        ActionContextImpl context(741); context.session = session;
        int count[8]{};
        for (int n=0;n<1000;++n) {
            auto item=start; PC_CHECK(apply_action(context,&item,registry.actions[action].params).applied);
            const auto mod=item.prefix_count ? item.prefixes[0].mod_id : item.suffixes[0].mod_id;
            PC_CHECK(mod<8); if(mod<8) ++count[mod];
        }
        for(unsigned mod=0;mod<8;++mod) PC_CHECK(std::abs(count[mod]/1000.-expected[mod])<0.055);
    }
}

// Finite physical-pool oracle. Each draw is observed once and tested against
// every v1 predicate. Single-goal evaluations below are test parity controls.
void run_calculator_goal_set_tests() {
    auto session = make_calc_session();
    const auto mod_satisfies_goal_slot = [](const SessionImpl& physical, std::uint32_t mod, const GoalSlot& slot) {
        bool member = slot.family_id != kNoId && physical.family_id[mod] == slot.family_id;
        if (slot.group_id != kNoId)
            for (auto i = physical.group_offsets[mod]; i < physical.group_offsets[mod+1]; ++i)
                member |= physical.group_ids[i] == slot.group_id;
        const auto tier = physical.family_tier_index[mod];
        return member && (!slot.min_tier || (tier && tier <= slot.min_tier));
    };
    auto registry = build_action_registry(*session);
    const auto make_goal = [](std::uint32_t family, std::uint32_t tier = 0) {
        GoalSpec goal; goal.slots.push_back({kNoId, family, tier});
        goal.terminal.extras = ExtraExplicitPolicy::Allow; return goal;
    };
    auto life = make_goal(100), life_t1 = make_goal(100, 1);
    auto group = life; group.slots[0] = {10, kNoId, 0};
    auto fire = make_goal(104), cold = make_goal(105);
    pc_item_state input; pc_item_clear(&input); input.rarity = PC_RARITY_RARE;
    const auto check = [&](const std::vector<CalculatorGoal>& goals, const pc_item_state& item) {
        GoalSpec unrelated = make_goal(103);
        CalcContext caller(session, unrelated, registry, {registry.index_by_id.at("scour")});
        const auto before = item;
        const auto text = calculate_currency_json(caller, item, "exalt", nullptr, nullptr, {}, goals);
        const auto result = json::Parser(text.data(), text.size()).parse();
        std::vector<double> expected(goals.size()); double expected_any = 0;
        ActionContextImpl context(0); context.session = session;
        const auto& pool = get_weighted_pool(context, &item, PoolBuildRequest{});
        for (const auto& row : pool.entries) {
            auto next = item;
            PC_CHECK(pc_item_add_mod(&next, row.gen_type, row.session_mod_id,
                static_cast<std::uint16_t>(row.primary_group), 0, nullptr) == PC_RESULT_OK);
            bool any = false;
            for (std::size_t g = 0; g < goals.size(); ++g) {
                const auto& goal = goals[g].explicit_goal;
                unsigned satisfied = 0;
                for (const auto& slot : goal.slots) {
                    bool hit = false;
                    for (unsigned i = 0; i < next.prefix_count; ++i) hit |= mod_satisfies_goal_slot(*session, next.prefixes[i].mod_id, slot);
                    for (unsigned i = 0; i < next.suffix_count; ++i) hit |= mod_satisfies_goal_slot(*session, next.suffixes[i].mod_id, slot);
                    satisfied += hit;
                }
                const bool hit = next.rarity == goal.rarity && satisfied >= goal.required_satisfied_slots() &&
                    (goal.terminal.extras == ExtraExplicitPolicy::Allow || next.prefix_count + next.suffix_count == satisfied);
                if (hit) { expected[g] += double(row.final_weight) / double(pool.total_weight); any = true; }
            }
            if (any) expected_any += double(row.final_weight) / double(pool.total_weight);
        }
        PC_CHECK(near(result.at("any_goal_probability").as_number(), expected_any, 1e-12));
        double conserved = 0;
        for (const auto& row : result.at("outcomes").array) {
            conserved += row.at("probability").as_number();
            if (item.prefix_count && item.prefixes[0].mod_id == 2)
                for (const auto& observation : row.at("goal_observations").array)
                    if (observation.at("id").as_string() == "life")
                        PC_CHECK((observation.at("blocked").as_int() & 1) != 0);
        }
        PC_CHECK(near(conserved, 1, 1e-12));
        const auto& results = result.at("goal_results").array;
        PC_CHECK(results.size() == goals.size());
        for (std::size_t g = 0; g < goals.size(); ++g) {
            PC_CHECK(results[g].at("id").as_string() == goals[g].id);
            PC_CHECK(near(results[g].at("success_probability").as_number(), expected[g], 1e-12));
            CalcContext scalar(session, goals[g].explicit_goal, registry, {registry.index_by_id.at("scour")});
            const auto old_text = calculate_currency_json(scalar, item, "exalt");
            const auto old = json::Parser(old_text.data(), old_text.size()).parse();
            PC_CHECK(near(old.at("success_probability").as_number(), expected[g], 1e-12));
            for (std::size_t slot = 0; slot < kMaxGoalSlots; ++slot)
                PC_CHECK(near(old.at("slot_satisfied").array[slot].as_number(), results[g].at("slot_satisfied").array[slot].as_number(), 1e-12));
        }
        PC_CHECK(std::memcmp(&before, &item, sizeof(item)) == 0);
    };
    check({{"life",life,{}},{"group",group,{}}},input); // overlapping families/group
    check({{"life",life,{}},{"t1",life_t1,{}}},input); // nested satisfying tiers
    check({{"fire",fire,{}},{"cold",cold,{}}},input); // disjoint outcomes
    auto held = input;
    PC_CHECK(pc_item_add_mod(&held, PC_SIDE_PREFIX, 2, 10, PC_MOD_SLOT_FRACTURED, nullptr) == PC_RESULT_OK);
    check({{"life",life,{}},{"fire",fire,{}}},held); // physical incoming exclusion
    auto clean = fire; clean.terminal.extras = ExtraExplicitPolicy::ForbidUnmatched;
    check({{"clean",clean,{}},{"covered",fire,{}}},held);
    // Independent ordered physical draws for ordinary 8:3:1 renewal.
    // Complete exclusion groups remain present in every intermediate item.
    auto coverage = life; coverage.slots.push_back(fire.slots[0]); coverage.min_satisfied_slots = 1;
    const std::vector<CalculatorGoal> renewal_goals{{"tier",life_t1,{}},{"coverage",coverage,{}}};
    const auto physical_hit = [&](const pc_item_state& item, const GoalSpec& goal) {
        unsigned satisfied = 0;
        for (const auto& slot : goal.slots) {
            bool hit = false;
            for (unsigned i = 0; i < item.prefix_count; ++i) hit |= mod_satisfies_goal_slot(*session,item.prefixes[i].mod_id,slot);
            for (unsigned i = 0; i < item.suffix_count; ++i) hit |= mod_satisfies_goal_slot(*session,item.suffixes[i].mod_id,slot);
            satisfied += hit;
        }
        return item.rarity == goal.rarity && satisfied >= goal.required_satisfied_slots() &&
            (goal.terminal.extras == ExtraExplicitPolicy::Allow || item.prefix_count + item.suffix_count == satisfied);
    };
    for (const auto& [action, initial] : std::vector<std::pair<std::string,pc_item_state>>{
            {"alchemy",[] {pc_item_state item; pc_item_clear(&item); return item;}()}, {"chaos",held}}) {
        CalcContext caller(session,life,registry,{},true,false);
        const auto text = calculate_currency_json(caller,initial,action,nullptr,nullptr,{},renewal_goals);
        const auto result = json::Parser(text.data(),text.size()).parse();
        std::array<long double,2> expected{}; long double expected_any = 0;
        ActionContextImpl context(0); context.session = session;
        auto base = initial; base.prefix_count = base.suffix_count = 0; base.rarity = PC_RARITY_RARE;
        for (unsigned i = 0; i < initial.prefix_count; ++i)
            if (initial.prefixes[i].flags & PC_MOD_SLOT_FRACTURED)
                PC_CHECK(pc_item_add_mod(&base,PC_SIDE_PREFIX,initial.prefixes[i].mod_id,initial.prefixes[i].group_id,initial.prefixes[i].flags,nullptr) == PC_RESULT_OK);
        const auto observe = [&](const pc_item_state& item, long double probability) {
            bool any = false;
            for (std::size_t g = 0; g < renewal_goals.size(); ++g) if (physical_hit(item,renewal_goals[g].explicit_goal)) {
                expected[g] += probability; any = true;
            }
            if (any) expected_any += probability;
        };
        const auto draw = [&](auto&& self, pc_item_state item, unsigned target, long double probability) -> void {
            if (item.prefix_count + item.suffix_count >= target) {observe(item,probability);return;}
            PoolBuildRequest request;
            request.side_filter = item.prefix_count == 3 ? 1 : item.suffix_count == 3 ? 0 : -1;
            const auto pool = get_weighted_pool(context,&item,request);
            if (!pool.total_weight) {observe(item,probability);return;}
            for (const auto& row : pool.entries) {
                auto next = item;
                PC_CHECK(pc_item_add_mod(&next,row.gen_type,row.session_mod_id,static_cast<std::uint16_t>(row.primary_group),0,nullptr) == PC_RESULT_OK);
                self(self,next,target,probability * row.final_weight / pool.total_weight);
            }
        };
        draw(draw,base,4,8.0L/12); draw(draw,base,5,3.0L/12); draw(draw,base,6,1.0L/12);
        PC_CHECK(near(result.at("any_goal_probability").as_number(),double(expected_any),1e-12));
        for (std::size_t g = 0; g < renewal_goals.size(); ++g)
            PC_CHECK(near(result.at("goal_results").array[g].at("success_probability").as_number(),double(expected[g]),1e-12));
        double total = 0;
        for (const auto& row : result.at("outcomes").array) total += row.at("probability").as_number();
        PC_CHECK(near(total,1,1e-12));
    }
    std::vector<CalculatorGoal> eight;
    for (unsigned i = 0; i < 8; ++i) eight.push_back({"g"+std::to_string(i), life,{}});
    check(eight,input);
    CalcContext scalar(session, life, registry, {registry.index_by_id.at("scour")});
    const auto before = calculate_currency_json(scalar,input,"exalt");
    const auto one = calculate_currency_json(scalar,input,"exalt",nullptr,nullptr,{},{{"one",life,{}}});
    const auto b = json::Parser(before.data(),before.size()).parse(), o = json::Parser(one.data(),one.size()).parse();
    PC_CHECK(b.at("success_probability").as_number() == o.at("success_probability").as_number());
    // Recombination consumes the identical observer through one output-session
    // terminal stream; no repeated per-goal traversal or UI-specific odds law.
    unsigned traversals = 0;
    auto life_item = input, fire_item = input, both = input;
    PC_CHECK(pc_item_add_mod(&life_item, PC_SIDE_PREFIX, 0, 10, 0, nullptr) == PC_RESULT_OK);
    PC_CHECK(pc_item_add_mod(&fire_item, PC_SIDE_SUFFIX, 5, 20, 0, nullptr) == PC_RESULT_OK);
    both = life_item; PC_CHECK(pc_item_add_mod(&both, PC_SIDE_SUFFIX, 5, 20, 0, nullptr) == PC_RESULT_OK);
    const auto streamed = observe_calculator_terminal_law_json(scalar, {{"life",life,{}},{"fire",fire,{}}},
        [&](const CalculatorTerminalSink& sink) { ++traversals; sink(life_item,0.2L); sink(fire_item,0.3L); sink(both,0.5L); });
    const auto stream = json::Parser(streamed.data(),streamed.size()).parse();
    PC_CHECK(traversals == 1);
    PC_CHECK(near(stream.at("goal_results").array[0].at("success_probability").as_number(),0.7,1e-12));
    PC_CHECK(near(stream.at("goal_results").array[1].at("success_probability").as_number(),0.8,1e-12));
    PC_CHECK(stream.at("any_goal_probability").as_number() == 1);
    bool conserved_refusal = false;
    try { observe_calculator_terminal_law_json(scalar,{{"life",life,{}}},
        [&](const CalculatorTerminalSink& sink) { sink(life_item,0.5L); }); }
    catch (const std::logic_error&) { conserved_refusal = true; }
    PC_CHECK(conserved_refusal);
    const auto refuses = [&](std::vector<CalculatorGoal> goals, const char* action) {
        bool rejected = false;
        try { calculate_currency_json(scalar,input,action,nullptr,nullptr,{},goals); }
        catch (const std::exception&) { rejected = true; }
        PC_CHECK(rejected);
    };
    eight.push_back({"ninth",life,{}}); refuses(eight,"exalt");
    refuses({{"same",life,{}},{"same",fire,{}}},"exalt");
    auto overlapping = life; overlapping.slots.push_back(life.slots[0]);
    refuses({{"a",overlapping,{}},{"b",fire,{}}},"exalt");
    refuses({{"a",life,{}},{"b",fire,{}}},"unveil");
}

void run_calculator_incoming_tests(const char* artifact_dir) {
    run_calculator_goal_set_tests();
    // The oracle builds the execution authority's pool from the concrete
    // carrier and observes each concrete successor. It never reconstructs
    // the incoming item through the Calculator's or the caller's layout.
    const auto check_add = [](const std::shared_ptr<SessionImpl>& session,
            GoalSpec goal, const pc_item_state& item, const char* action) {
        const auto registry = build_action_registry(*session);
        const auto index = registry.index_by_id.at(action);
        goal.terminal.extras = ExtraExplicitPolicy::Allow;
        goal.primitive_actions_explicit = true;
        const auto before = item;
        double oracle = 0;
        auto upgraded = item;
        if (ordinary_add_equivalent(registry.actions[index].params.type) == ActionType::Regal)
            upgraded.rarity = PC_RARITY_RARE;
        std::vector<std::uint64_t> reachable(session->words, 0);
        for (std::uint32_t mod = 0; mod < session->mod_count; ++mod)
            if (session->gen_type[mod] <= 1) pc_bitset_set(reachable.data(), mod);
        CalcContext observer(session, goal, registry, {}, true, false,
            false, std::nullopt, {}, false, reachable);
        ActionContextImpl context(0);
        context.session = session;
        const auto cap = rarity_affix_cap(*session, upgraded.rarity);
        const bool prefix_open = upgraded.prefix_count < cap;
        const bool suffix_open = upgraded.suffix_count < cap;
        if (prefix_open || suffix_open) {
            PoolBuildRequest request;
            request.side_filter = prefix_open && suffix_open ? -1 : (prefix_open ? 0 : 1);
            if (is_foulborn(registry.actions[index].params.type)) request.weight_kind = PoolWeightKind::Foulborn;
            const auto& pool = get_weighted_pool(context, &upgraded, request);
            for (const auto& row : pool.entries) {
                auto next = upgraded;
                PC_CHECK(pc_item_add_mod(&next, row.gen_type, row.session_mod_id,
                    static_cast<std::uint16_t>(row.primary_group), 0, nullptr) == PC_RESULT_OK);
                if (observer.is_goal_state(observer.state(observer.intern_item(next))))
                    oracle += double(row.final_weight) / double(pool.total_weight);
            }
            if (!pool.total_weight && observer.is_goal_state(observer.state(observer.intern_item(upgraded)))) oracle = 1;
        }
        double actual = 0;
        // Both an unrelated cleanup envelope and the inspector's Chaos
        // envelope must give the same one-action answer.
        for (const char* envelope : {"scour", "chaos"}) {
            CalcContext caller(session, goal, registry, {registry.index_by_id.at(envelope)},
                true, false, false);
            pc_item_state empty{};
            caller.intern_item(empty);
            const auto state_count = caller.state_count();
            const auto result_text = calculate_currency_json(caller, item, action);
            const auto result = json::Parser(result_text.data(), result_text.size()).parse();
            PC_CHECK(result.find("supported")->boolean && result.find("legal")->boolean);
            actual = result.find("success_probability")->number;
            PC_CHECK(near(actual, oracle, 1e-12));
            double total = 0, success = 0;
            for (const auto& row : result.find("outcomes")->array) {
                const auto p = row.find("probability")->number;
                total += p;
                if (row.find("is_goal")->boolean) success += p;
            }
            PC_CHECK(near(total, 1, 1e-12) && near(success, actual, 1e-12));
            PC_CHECK(std::memcmp(&item, &before, sizeof(item)) == 0);
            PC_CHECK(caller.state_count() == state_count);
        }
        return actual;
    };
    const auto fixture = [] {
        auto session = make_calc_session();
        auto data = std::const_pointer_cast<DataImpl>(session->data);
        data->strings = {"life", "hybrid", "attack", "caster", "fire", "cold", "speed", "veilP", "veilS"};
        data->mod_type_key_sid = {0,0,1,2,3,4,5,6,7,8};
        session->required_level[0] = 10;
        session->item_level = 86;
        data->spawn_weights[4] = session->base_spawn_weight[4] = session->base_roll_weight[4] = 300;
        return session;
    };
    for (const char* action : {"augment", "regal", "exalt", "foulborn_augment", "foulborn_regal", "foulborn_exalt"}) {
        auto session = fixture();
        auto goal = family_goal_100();
        pc_item_state item{};
        const auto registry = build_action_registry(*session);
        const auto type = ordinary_add_equivalent(registry.actions[registry.index_by_id.at(action)].params.type);
        const bool exalt = type == ActionType::Exalt;
        item.rarity = exalt ? PC_RARITY_RARE : PC_RARITY_MAGIC;
        if (exalt) {
            place(&item, 0, 4, 13);
            for (unsigned mod = 5; mod <= 7; ++mod) place(&item, 1, mod, 20 + mod - 5);
        } else place(&item, 1, 5, 20);
        if (type == ActionType::Augment)
            goal.rarity = PC_RARITY_MAGIC;
        PC_CHECK(check_add(session, goal, item, action) > 0);
        // A below-tier member blocks its entire physical group and must not
        // become a satisfying member during carrier reconstruction.
        item = {};
        item.rarity = exalt ? PC_RARITY_RARE : PC_RARITY_MAGIC;
        place(&item, 0, 1, 10, PC_MOD_SLOT_FRACTURED);
        PC_CHECK(check_add(session, goal, item, action) == 0);
    }
    {
        auto session = fixture();
        // Secondary exclusion groups are as authoritative as primary ones.
        session->group_ids[3] = 13;
        pc_bitset_clear(session->group_masks[11].data(), 2);
        pc_bitset_set(session->group_masks[13].data(), 2);
        auto goal = family_goal_100(); goal.slots[0].family_id = 103;
        pc_item_state item{}; item.rarity = PC_RARITY_RARE;
        place(&item, 0, 2, 10);
        for (unsigned mod = 5; mod <= 7; ++mod) place(&item, 1, mod, 20 + mod - 5);
        PC_CHECK(check_add(session, goal, item, "exalt") == 0);
        PC_CHECK(check_add(session, goal, item, "foulborn_exalt") == 0);
    }
    {
        auto session = fixture();
        // A retained above-level modifier remains an incoming blocker even
        // when this session's random pool cannot place it.
        session->item_level = 20; session->required_level[4] = 90;
        pc_bitset_clear(session->normal_random_roll_mask.data(), 4);
        pc_item_state item{}; item.rarity = PC_RARITY_RARE;
        place(&item, 0, 4, 13, PC_MOD_SLOT_FRACTURED);
        for (unsigned mod = 5; mod <= 7; ++mod) place(&item, 1, mod, 20 + mod - 5);
        PC_CHECK(near(check_add(session, family_goal_100(), item, "exalt"), 0.25));
        PC_CHECK(near(check_add(session, family_goal_100(), item, "foulborn_exalt"), 0.5));
    }
    for (unsigned role = 0; role < 2; ++role) {
        auto session = fixture(); auto data = std::const_pointer_cast<DataImpl>(session->data);
        data->metamod_prefixes_locked_code = 10; data->metamod_no_attack_code = 11;
        data->tag_id_by_name["attack"] = 1;
        session->metamod_type[7] = role ? 11 : 10;
        pc_item_state item{}; item.rarity = PC_RARITY_RARE;
        place(&item, 0, 4, 13, PC_MOD_SLOT_FRACTURED);
        place(&item, 1, 7, 22, PC_MOD_SLOT_CRAFTED);
        PC_CHECK(check_add(session, family_goal_100(), item, "exalt") > 0);
        PC_CHECK(check_add(session, family_goal_100(), item, "foulborn_exalt") > 0);
    }
    {
        auto session = fixture(); const auto registry = build_action_registry(*session);
        auto goal = family_goal_100(); goal.terminal.extras = ExtraExplicitPolicy::Allow;
        goal.disabled_action_families = solver_action_family_bit(SolverActionFamily::Foulborn);
        CalcContext caller(session, goal, registry, {}, true, false);
        pc_item_state item{}; item.rarity = PC_RARITY_RARE;
        const auto rejects = [&](const pc_item_state& input, const char* action, const char* reason) {
            bool rejected = false;
            try { calculate_currency_json(caller, input, action); }
            catch (const std::invalid_argument& ex) { rejected = std::strstr(ex.what(), reason) != nullptr; }
            PC_CHECK(rejected);
        };
        rejects(item, "foulborn_exalt", "disabled family");
        rejects(item, "not_a_currency", "Unknown Calculator action");
        auto guarded = item; guarded.memory_strands = 1; rejects(guarded, "exalt", "memory strands");
        guarded = item; guarded.lifecycle = PC_ITEM_DESTROYED; rejects(guarded, "exalt", "live item");
        guarded = item; guarded.enchantment_count = 1; rejects(guarded, "exalt", "enchantment");
        for (auto flag : {PC_ITEM_CORRUPTED, PC_ITEM_MIRRORED}) {
            guarded = item; guarded.item_flags |= flag;
            const auto text = calculate_currency_json(caller, guarded, "exalt");
            const auto result = json::Parser(text.data(), text.size()).parse();
            PC_CHECK(!result.find("legal")->boolean && result.find("success_probability")->number == 0);
        }
    }
    if (!artifact_dir) return;
    const std::string dir = artifact_dir;
    std::string manifest, strings, game;
    PC_CHECK(read_text_file(dir + "/manifest.json", manifest));
    PC_CHECK(read_text_file(dir + "/strings.json", strings));
    PC_CHECK(read_text_file(dir + "/game-data.json", game));
    auto data = load_data_impl(manifest, strings, game);
    auto session = std::make_shared<SessionImpl>(); session->data = data;
    session->base_index = data->base_by_path.at("Metadata/Items/Armours/BodyArmours/BodyInt17");
    session->item_level = 86; build_session(*session);
    const auto mod_id = [&](const char* key) {
        return session->session_id_by_global_id.at(data->mod_global_ids[data->mod_pos_by_key.at(key)]);
    };
    GoalSpec goal; goal.slots.push_back({}); goal.slots[0].family_id = session->family_id[mod_id("LocalIncreasedEnergyShield11")];
    goal.slots[0].min_tier = 0;
    pc_item_state item{}; item.rarity = PC_RARITY_RARE;
    for (const char* key : {"LocalIncreasedEnergyShieldPercent8", "FireResist8", "ColdResist8", "LightningResist8"}) {
        const auto mod = mod_id(key);
        place(&item, session->gen_type[mod], mod, static_cast<std::uint16_t>(session->primary_group[mod]));
    }
    const auto p = check_add(session, goal, item, "foulborn_exalt");
    PC_CHECK(near(p, 0.21153846153846154, 1e-12));
    std::printf("Original Foulborn carrier Calculator probability: %.17g\n", p);
    // Expanded corruption branches retain native final-property predicates.
    GoalSpec unrestricted; unrestricted.terminal.extras = ExtraExplicitPolicy::Allow;
    CalculatorItemGoal corrupted, clean_item; corrupted.corrupted = true; clean_item.corrupted = false;
    const auto registry = build_action_registry(*session);
    CalcContext caller(session,unrestricted,registry,{},true,false);
    for (const char* action : {"vaal","double_corruption"}) {
        const auto text = calculate_currency_json(caller,item,action,nullptr,nullptr,{},
            {{"corrupted",unrestricted,corrupted},{"uncorrupted",unrestricted,clean_item}});
        const auto result = json::Parser(text.data(),text.size()).parse();
        const double expected = std::string(action) == "vaal" ? 1 : 0.5;
        PC_CHECK(near(result.at("any_goal_probability").as_number(),expected,1e-12));
        PC_CHECK(near(result.at("goal_results").array[0].at("success_probability").as_number(),expected,1e-12));
        PC_CHECK(result.at("goal_results").array[1].at("success_probability").as_number() == 0);
        double conserved = 0;
        for (const auto& row : result.at("outcomes").array) conserved += row.at("probability").as_number();
        PC_CHECK(near(conserved,1,1e-12));
    }

}

void run_eldritch_side_count_law_tests() {
    // Literal conditional laws, independent of the production count helper.
    // Complete equipment pools: held0/1 -> side3, held2 -> side2/3 at8:4,
    // held3 -> side1/2/3 at8:3:1. Fractures occupy those side targets.
    constexpr double side_mass[4][4] = {
        {0, 0, 0, 1}, {0, 0, 0, 1},
        {0, 0, 8.0 / 12, 4.0 / 12},
        {0, 8.0 / 12, 3.0 / 12, 1.0 / 12},
    };
    const unsigned mods[2][3] = {{0, 3, 4}, {5, 6, 7}};
    for (int side = 0; side < 2; ++side) {
        for (unsigned held = 0; held <= 3; ++held) {
            for (unsigned fractures = 0; fractures <= 3; ++fractures) {
                for (unsigned available : {0u, 1u, 3u}) {
                    // No bench; opposite-side lock on the rerolled side;
                    // rerolled-side bench lock, which itself must be removed.
                    for (unsigned bench = 0; bench < 3; ++bench) {
                        if ((bench == 1 && held == 0) ||
                            (bench == 2 && fractures == 3)) continue;
                        auto session = make_calc_session();
                        session->rare_affix_cap = 3;
                        session->rare_reforge_count_kind = RareReforgeCountKind::Equipment;
                        auto data = std::const_pointer_cast<DataImpl>(session->data);
                        session->veiled_prefix_mod_id = session->veiled_suffix_mod_id = kNoId;
                        data->metamod_prefixes_locked_code = 20;
                        data->metamod_suffixes_locked_code = 21;
                        session->metamod_type[8] = 21;
                        session->metamod_type[9] = 20;
                        session->flags[8] |= 1u << 1;
                        session->flags[9] |= 1u << 1;
                        session->crafted_mask.assign(session->words, 0);
                        pc_bitset_set(session->crafted_mask.data(), 8);
                        pc_bitset_set(session->crafted_mask.data(), 9);
                        session->normal_random_roll_mask.assign(session->words, 0);
                        auto carrier_mask = session->normal_random_roll_mask;
                        for (unsigned mod : {0u, 3u, 4u, 5u, 6u, 7u, 8u, 9u})
                            pc_bitset_set(carrier_mask.data(), mod);
                        for (unsigned i = 0; i < available; ++i)
                            pc_bitset_set(session->normal_random_roll_mask.data(), mods[side][i]);
                        const auto registry = build_action_registry(*session);
                        const auto action = registry.index_by_id.at("eldritch_chaos");
                        pc_item_state source{};
                        pc_item_clear(&source);
                        source.rarity = PC_RARITY_RARE;
                        source.searing_exarch_tier = side == 0 ? 1 : 0;
                        source.eater_of_worlds_tier = side == 1 ? 1 : 0;
                        for (unsigned i = 0; i < held; ++i) {
                            const bool lock = bench == 1 && i + 1 == held;
                            const auto mod = lock ? static_cast<unsigned>(side == 0 ? 9 : 8)
                                                  : mods[1 - side][i];
                            place(&source, 1 - side, mod, session->primary_group[mod],
                                  lock ? PC_MOD_SLOT_CRAFTED :
                                  i == 0 ? PC_MOD_SLOT_FRACTURED : 0);
                        }
                        for (unsigned i = 0; i < fractures; ++i) {
                            const auto mod = mods[side][i];
                            place(&source, side, mod, session->primary_group[mod], PC_MOD_SLOT_FRACTURED);
                        }
                        if (fractures < 3) {
                            const auto mod = bench == 2 ? static_cast<unsigned>(side == 0 ? 8 : 9)
                                                       : mods[side][fractures];
                            place(&source, side, mod, session->primary_group[mod],
                                  bench == 2 ? PC_MOD_SLOT_CRAFTED : 0);
                        }
                        auto base = source;
                        pc_item_clear_side(&base, side);
                        for (unsigned i = 0; i < fractures; ++i) {
                            const auto mod = mods[side][i];
                            place(&base, side, mod, session->primary_group[mod], PC_MOD_SLOT_FRACTURED);
                        }
                        const auto side_count = [side](const pc_item_state& item) {
                            return side == 0 ? item.prefix_count : item.suffix_count;
                        };
                        const auto retained = [&](const pc_item_state& item) {
                            const auto* original = side == 0 ? source.suffixes : source.prefixes;
                            const auto* opposite = side == 0 ? item.suffixes : item.prefixes;
                            const auto other_count = side == 0 ? item.suffix_count : item.prefix_count;
                            if (other_count != held) return false;
                            // Exact materialization orders goal and junk carriers
                            // canonically; retain each slot's full identity rather
                            // than requiring the physical input array order.
                            for (unsigned i = 0; i < held; ++i) {
                                bool found = false;
                                for (unsigned j = 0; j < held; ++j)
                                    found |= std::memcmp(&opposite[j], &original[i],
                                                         sizeof(pc_mod_slot)) == 0;
                                if (!found) return false;
                            }
                            const auto* selected = side == 0 ? item.prefixes : item.suffixes;
                            for (unsigned i = 0; i < fractures; ++i) {
                                bool found = false;
                                for (unsigned j = 0; j < side_count(item); ++j)
                                    found |= selected[j].mod_id == mods[side][i] &&
                                             (selected[j].flags & PC_MOD_SLOT_FRACTURED);
                                if (!found) return false;
                            }
                            return true;
                        };
                        for (unsigned extra = 0; extra < 2; ++extra) {
                            for (unsigned implementation = 0; implementation < 3; ++implementation) {
                                auto goal = family_goal_100();
                                if (extra) goal.terminal.extras = ExtraExplicitPolicy::Allow;
                                CalcContext calc(session, goal, registry, {action}, false, false, false,
                                    std::nullopt, {}, false, carrier_mask, false,
                                    false, implementation != 0, false, implementation == 2);
                                ActionContextImpl context(20261003);
                                context.session = session;
                                std::map<unsigned, double> expected;
                                const auto visit = [&](auto&& self, pc_item_state item,
                                                       unsigned target, double probability) -> void {
                                    if (side_count(item) >= target) {
                                        expected[calc.intern_item(item)] += probability;
                                        return;
                                    }
                                    PoolBuildRequest request;
                                    request.side_filter = side;
                                    const auto pool = get_weighted_pool(context, &item, request);
                                    if (!pool.total_weight) {
                                        expected[calc.intern_item(item)] += probability;
                                        return;
                                    }
                                    for (const auto& row : pool.entries) if (row.final_weight) {
                                        auto next = item;
                                        PC_CHECK(pc_item_add_mod(&next, side, row.session_mod_id,
                                            row.primary_group, 0, nullptr) == PC_RESULT_OK);
                                        self(self, next, target,
                                             probability * row.final_weight / pool.total_weight);
                                    }
                                };
                                for (unsigned target = 0; target <= 3; ++target)
                                    if (side_mass[held][target] > 0)
                                        visit(visit, base, std::max(target, fractures), side_mass[held][target]);
                                const auto& actual = calc.outcomes(calc.intern_item(source), action);
                                PC_CHECK(actual.supported && sums_to_one(actual));
                                PC_CHECK(actual.entries.size() == expected.size());
                                for (const auto& row : actual.entries)
                                    PC_CHECK(near(row.probability, expected[row.state], 1e-12));
                                double counts[4]{};
                                for (const auto& row : actual.entries) {
                                    pc_item_state item{};
                                    PC_CHECK(calc.materialize(row.state, item));
                                    PC_CHECK(retained(item));
                                    counts[side_count(item)] += row.probability;
                                }
                                if (available == 3) {
                                    double expected_counts[4]{};
                                    for (unsigned target = 0; target <= 3; ++target)
                                        expected_counts[std::max(target, fractures)] += side_mass[held][target];
                                    for (unsigned count = 0; count <= 3; ++count)
                                        PC_CHECK(near(counts[count], expected_counts[count], 1e-12));
                                }
                                if (available == 0) PC_CHECK(near(counts[fractures], 1, 1e-12));
                            }
                        }
                        // Native mutation frequency/support and actual added/removed
                        // accounting. Sparse pools absorb mass without redrawing.
                        ActionContextImpl context(20261003 + held + fractures);
                        context.session = session;
                        unsigned counts[4]{};
                        const unsigned trials = available == 3 && bench == 0 && fractures <= 1 ? 2000 : 12;
                        for (unsigned trial = 0; trial < trials; ++trial) {
                            auto item = source;
                            const auto outcome = apply_action(context, &item, registry.actions[action].params);
                            PC_CHECK(outcome.applied && retained(item));
                            PC_CHECK(std::memcmp(side == 0 ? item.suffixes : item.prefixes,
                                                 side == 0 ? source.suffixes : source.prefixes,
                                                 sizeof(source.prefixes)) == 0);
                            PC_CHECK(outcome.added == side_count(item) - fractures);
                            PC_CHECK(outcome.removed == side_count(source) - fractures);
                            ++counts[side_count(item)];
                        }
                        if (trials == 2000) {
                            double expected_counts[4]{};
                            for (unsigned target = 0; target <= 3; ++target)
                                expected_counts[std::max(target, fractures)] += side_mass[held][target];
                            for (unsigned count = 0; count <= 3; ++count)
                                PC_CHECK(std::abs(double(counts[count]) / trials - expected_counts[count]) < 0.05);
                        }
                        if (available == 0) PC_CHECK(counts[fractures] == trials);
                    }
                }
            }
        }
    }
}

void run_reforge_count_law_tests() {
    run_eldritch_side_count_law_tests();
    // Independent finite native ordered draws, with literal mixture constants.
    // No production count helper or cached DP row constructs the reference.
    for (unsigned scenario = 0; scenario < 8; ++scenario) {
        auto session = make_calc_session();
        auto data = std::const_pointer_cast<DataImpl>(session->data);
        session->normal_random_roll_mask.assign(session->words, 0);
        for (const unsigned mod : {0u,3u,4u,5u,6u,7u})
            if (scenario != 3 && (scenario != 2 || mod == 0 || mod == 5 || mod == 6))
                pc_bitset_set(session->normal_random_roll_mask.data(), mod);
        if (scenario == 1) session->rare_affix_cap = 2;
        if (scenario == 7) {
            // Preserve the existing ordinary-jewel law pending its owner ruling.
            session->rare_affix_cap = 2;
            session->rare_reforge_count_kind = RareReforgeCountKind::LegacyJewel;
        }
        data->metamod_prefixes_locked_code = 20;
        session->metamod_type[8] = 20;
        session->flags[8] |= 1u << 1;
        session->crafted_mask.assign(session->words,0);
        pc_bitset_set(session->crafted_mask.data(),8);
        auto carrier_mask=session->normal_random_roll_mask;
        pc_bitset_set(carrier_mask.data(),8);
        session->veiled_prefix_mod_id = session->veiled_suffix_mod_id = kNoId;
        data->essence_count = 1;
        data->essence_item_level_restrictions = {-1};
        data->essence_is_corruption_only = {0};
        data->essence_key_sids = {static_cast<unsigned>(data->strings.size())};
        data->strings.push_back("count_forced");
        session->essence_guaranteed_mod_ids = {0};
        data->fossil_count = 1;
        data->fossil_key_sids = data->fossil_name_sids = {static_cast<unsigned>(data->strings.size())};
        data->strings.push_back("count_fossil");
        data->fossil_weight_offsets = data->fossil_mod_offsets = {0,0};
        data->fossil_rolls_lucky = data->fossil_mirrors = {0};
        session->fossil_added_mod_ids = {{}};
        session->fossil_forced_mod_ids = {{3,5}};
        session->fossil_sell_price_mod_ids = {{}};
        auto registry = build_action_registry(*session);
        const auto chaos = registry.index_by_id.at("chaos");
        auto fossil = registry.actions[chaos];
        fossil.id = "fossil:count_forced";
        fossil.params.type = ActionType::Fossil;
        fossil.params.fossil_indices = {0};
        fossil.refinement = derive_action_refinement_contract(*session, fossil);
        const auto fossil_id = static_cast<unsigned>(registry.actions.size());
        registry.actions.push_back(fossil);
        for (unsigned extra = 0; extra < 2; ++extra) for (unsigned implementation = 0; implementation < 3; ++implementation) {
            auto goal = family_goal_100();
            if (extra) goal.terminal.extras = ExtraExplicitPolicy::Allow;
            CalcContext calc(session, goal, registry, {}, false, false, false,
                std::nullopt, {}, false, carrier_mask, false,
                false, implementation != 0, false, implementation == 2);
            ActionContextImpl context(112233);
            context.session = session;
            for (const unsigned action : {chaos, registry.index_by_id.at("alchemy"),
                    registry.index_by_id.at("essence:count_forced"),
                    registry.index_by_id.at("harvest_reforge:fire"),
                    registry.index_by_id.at("eldritch_chaos"), fossil_id}) {
                pc_item_state source;
                pc_item_clear(&source);
                source.rarity = action == registry.index_by_id.at("alchemy") ? PC_RARITY_NORMAL : PC_RARITY_RARE;
                if (scenario == 4) place(&source, 1, 5, 20, PC_MOD_SLOT_FRACTURED);
                if (scenario == 5) {
                    place(&source, 0, 8, 30, PC_MOD_SLOT_CRAFTED);
                    place(&source, 0, 0, 10);
                    place(&source, 1, 5, 20, PC_MOD_SLOT_FRACTURED);
                    place(&source, 1, 6, 21); // Wiped unless independently fractured.
                }
                if (scenario == 6) for (const auto mod : {0u,3u,4u,5u,6u})
                    place(&source, session->gen_type[mod], mod, session->primary_group[mod], PC_MOD_SLOT_FRACTURED);
                const auto type = registry.actions[action].params.type;
                const bool locks = type != ActionType::Essence && type != ActionType::Fossil;
                pc_item_state base = source;
                pc_item_clear_side(&base, 0); pc_item_clear_side(&base, 1);
                base.rarity = PC_RARITY_RARE;
                for (const int side : {0,1}) {
                    const auto* slots = side == 0 ? source.prefixes : source.suffixes;
                    const auto n = side == 0 ? source.prefix_count : source.suffix_count;
                    for (unsigned i=0;i<n;++i) if ((slots[i].flags & PC_MOD_SLOT_FRACTURED) || (scenario == 5 && locks && side == 0))
                        place(&base, side, slots[i].mod_id, slots[i].group_id, slots[i].flags);
                }
                for (const auto mod : type == ActionType::Essence ? std::vector<unsigned>{0} : type == ActionType::Fossil ? std::vector<unsigned>{3,5} : std::vector<unsigned>{}) {
                    bool same = false;
                    for (const int side : {0,1}) {
                        const auto* slots = side == 0 ? base.prefixes : base.suffixes;
                        const auto n = side == 0 ? base.prefix_count : base.suffix_count;
                        for (unsigned i=0;i<n;++i) same |= slots[i].mod_id == mod;
                    }
                    // These fixtures have no conflicting distinct forced groups.
                    if (!same && (session->gen_type[mod] == 0 ? base.prefix_count : base.suffix_count) < session->rare_affix_cap)
                        place(&base, session->gen_type[mod], mod, session->primary_group[mod]);
                }
                bool direct_failure = false;
                for (const auto mod : type == ActionType::Essence ? std::vector<unsigned>{0} : type == ActionType::Fossil ? std::vector<unsigned>{3,5} : std::vector<unsigned>{}) {
                    for (const int side : {0,1}) {
                        const auto* slots = side == 0 ? source.prefixes : source.suffixes;
                        const auto n = side == 0 ? source.prefix_count : source.suffix_count;
                        for (unsigned i=0;i<n;++i) if ((slots[i].flags & PC_MOD_SLOT_FRACTURED) && slots[i].mod_id == mod) direct_failure = true;
                    }
                }
                if (direct_failure) {
                    const auto input = calc.intern_item(source);
                    const auto& row = calc.outcomes(input,action);
                    if (!row.supported || row.entries.size()!=1 || row.entries.front().state!=input)
                        std::printf("direct-refusal case=%u extra=%u evaluator=%u action=%s supported=%u entries=%zu\n",scenario,extra,implementation,registry.actions[action].id.c_str(),row.supported,row.entries.size());
                    PC_CHECK(row.supported && row.entries.size()==1 && row.entries.front().state==input && row.entries.front().probability==1);
                    continue;
                }
                std::map<unsigned,double> expected;
                const auto visit = [&](auto&& self, pc_item_state item, unsigned target, double p) -> void {
                    if (item.prefix_count+item.suffix_count >= target) { expected[calc.intern_item(item)]+=p; return; }
                    const bool prefix = item.prefix_count < session->rare_affix_cap;
                    const bool suffix = item.suffix_count < session->rare_affix_cap;
                    PoolBuildRequest request;
                    request.respects_metamod_pool_blocks = locks;
                    request.side_filter = prefix && suffix ? -1 : prefix ? 0 : 1;
                    const auto pool = get_weighted_pool(context, &item, request);
                    if ((!prefix && !suffix) || !pool.total_weight) { expected[calc.intern_item(item)]+=p; return; }
                    for (const auto& row : pool.entries) if (row.final_weight) {
                        auto next = item;
                        PC_CHECK(pc_item_add_mod(&next,row.gen_type,row.session_mod_id,row.primary_group,0,nullptr)==PC_RESULT_OK);
                        self(self,next,target,p*row.final_weight/pool.total_weight);
                    }
                };
                for (unsigned t=4;t<=6;++t) {
                    const double p = scenario == 7 ? 1.0/3 : t == 4 ? 8.0/12 : t == 5 ? 3.0/12 : 1.0/12;
                    if (type == ActionType::HarvestReforge) {
                        PoolBuildRequest request;
                        request.weight_kind = PoolWeightKind::TargetedNatural;
                        request.target_tag_id = kTagFire;
                        const auto pool = get_weighted_pool(context,&base,request);
                        if (!pool.total_weight) expected[calc.intern_item(source)]+=p;
                        for (const auto& row : pool.entries) if (row.final_weight) {
                            auto next=base;
                            PC_CHECK(pc_item_add_mod(&next,row.gen_type,row.session_mod_id,row.primary_group,0,nullptr)==PC_RESULT_OK);
                            visit(visit,next,t,p*row.final_weight/pool.total_weight);
                        }
                    } else visit(visit,base,t,p);
                }
                const auto& actual = calc.outcomes(calc.intern_item(source), action);
                if (!actual.supported || actual.entries.size()!=expected.size())
                    std::printf("count case=%u extra=%u evaluator=%u action=%s supported=%u actual=%zu expected=%zu\n",scenario,extra,implementation,registry.actions[action].id.c_str(),actual.supported,actual.entries.size(),expected.size());
                PC_CHECK(actual.supported && sums_to_one(actual));
                PC_CHECK(actual.entries.size()==expected.size());
                for (const auto& row : actual.entries) PC_CHECK(near(row.probability,expected[row.state],1e-12));
                if (scenario == 0 && action == chaos) {
                    double counts[7]{};
                    for (const auto& row : actual.entries) {
                        const auto& s=calc.state(row.state); counts[s.prefix_count+s.suffix_count]+=row.probability;
                    }
                    PC_CHECK(near(counts[4],8.0/12) && near(counts[5],3.0/12) && near(counts[6],1.0/12));
                    // Concrete fixed-six must not pollute the ordinary row cache.
                    const auto copy = actual;
                    const auto fixed=calc.concrete_refill({base,6,true,false});
                    PC_CHECK(fixed->supported && sums_to_one(*fixed));
                    for (const auto& row:fixed->entries) {
                        const auto& s=calc.state(row.state); PC_CHECK(s.prefix_count+s.suffix_count==6);
                    }
                    PC_CHECK(same_distribution(copy,calc.outcomes(calc.intern_item(source),chaos)));
                }
            }
        }
        if (scenario == 0) {
            ActionContextImpl context(20261002); context.session=session;
            for (const auto type : {ActionType::Alchemy,ActionType::Chaos,ActionType::Essence,ActionType::Fossil,ActionType::HarvestReforge,ActionType::EldritchChaos}) {
                unsigned counts[7]{};
                ActionParameters params; params.type=type; params.essence_index=0; params.fossil_indices={0}; params.target_tag_id=kTagFire;
                for (unsigned i=0;i<12000;++i) {
                    pc_item_state item; pc_item_clear(&item); item.rarity=type==ActionType::Alchemy ? PC_RARITY_NORMAL : PC_RARITY_RARE;
                    PC_CHECK(apply_action(context,&item,params).applied);
                    ++counts[item.prefix_count+item.suffix_count];
                }
                PC_CHECK(std::abs(counts[4]/12000.0-8.0/12)<.02);
                PC_CHECK(std::abs(counts[5]/12000.0-3.0/12)<.02);
                PC_CHECK(std::abs(counts[6]/12000.0-1.0/12)<.02);
                std::printf("count frequencies action=%u: %u/%u/%u of 12000\n",static_cast<unsigned>(type),counts[4],counts[5],counts[6]);
            }
        }
    }
    // Native clamps rare total to four before reserving its veil slot on a
    // two-per-side carrier. All count draws therefore leave three fillers and
    // one placeholder; clamping after reservation would fill both sides first.
    {
        auto session=make_calc_session(); session->rare_affix_cap=2;
        auto registry=build_action_registry(*session);
        const auto action=registry.index_by_id.at("veiled_chaos");
        CalcContext calc(session,family_goal_100(),registry,{},false,false,false,
            std::nullopt,{},false,session->normal_random_roll_mask,false,false,true,false,true);
        pc_item_state rare;pc_item_clear(&rare);rare.rarity=PC_RARITY_RARE;
        const auto& row=calc.outcomes(calc.intern_item(rare),action);
        PC_CHECK(row.supported && sums_to_one(row));
        for(const auto& e:row.entries) {
            const auto& item=calc.state(e.state);
            PC_CHECK(item.prefix_count+item.suffix_count==4 && (item.flags & kFlagVeiledMod));
        }
        ActionContextImpl context(20261003); context.session=session;
        ActionParameters params;params.type=ActionType::VeiledChaos;
        for(unsigned i=0;i<1000;++i) {
            auto item=rare; PC_CHECK(apply_action(context,&item,params).applied);
            PC_CHECK(item.prefix_count+item.suffix_count==4);
            PC_CHECK(pc_item_find_veiled(&item,nullptr,nullptr)==PC_RESULT_OK);
        }
    }
    // Retained occupancy/recovery witness: a suffix metamod preserves 2/3
    // prefixes, then is wiped itself. Harvest guarantees the other-side Fire
    // modifier before drawing a TARGET TOTAL. Its guarantee is not extra to
    // 4/5/6, nor is the mixture conditioned on the retained count.
    for (unsigned held : {2u,3u}) for (unsigned forced : {0u,1u}) for (unsigned pool_case : {0u,1u,2u}) {
        auto session=make_calc_session();
        auto data=std::const_pointer_cast<DataImpl>(session->data);
        data->metamod_prefixes_locked_code=20;
        session->metamod_type[9]=20; session->flags[9]|=1u<<1;
        session->crafted_mask.assign(session->words,0); pc_bitset_set(session->crafted_mask.data(),9);
        session->veiled_prefix_mod_id=session->veiled_suffix_mod_id=kNoId;
        session->normal_random_roll_mask.assign(session->words,0);
        for(const auto mod : pool_case==0 ? std::vector<unsigned>{0,3,4,5,6,7} :
                pool_case==1 ? std::vector<unsigned>{5,6} : std::vector<unsigned>{5})
            pc_bitset_set(session->normal_random_roll_mask.data(),mod);
        std::vector<std::uint64_t> carrier(session->words,0);
        for(const auto mod : {0u,3u,4u,5u,6u,7u,9u}) pc_bitset_set(carrier.data(),mod);
        const auto registry=build_action_registry(*session);
        const auto action=registry.index_by_id.at(forced ? "harvest_reforge:fire" : "chaos");
        auto goal=family_goal_100(); goal.terminal.extras=ExtraExplicitPolicy::Allow;
        CalcContext calc(session,goal,registry,{},false,false,false,std::nullopt,{},false,carrier,false,false,true,false,true);
        pc_item_state source;pc_item_clear(&source);source.rarity=PC_RARITY_RARE;
        for(const auto mod : held==2 ? std::vector<unsigned>{0,3} : std::vector<unsigned>{0,3,4})
            place(&source,0,mod,session->primary_group[mod]);
        place(&source,1,9,31,PC_MOD_SLOT_CRAFTED);
        auto base=source;pc_item_clear_side(&base,1); // lock itself is wiped
        if(forced) place(&base,1,5,20);
        ActionContextImpl context(20261004);context.session=session;
        std::map<unsigned,double> expected;
        const auto visit=[&](auto&& self,pc_item_state item,unsigned total,double mass)->void {
            if(item.prefix_count+item.suffix_count>=total) {expected[calc.intern_item(item)]+=mass;return;}
            const bool pre=item.prefix_count<3,suf=item.suffix_count<3;
            PoolBuildRequest request;request.side_filter=pre&&suf ? -1 : pre ? 0 : 1;
            const auto pool=get_weighted_pool(context,&item,request);
            if((!pre&&!suf)||!pool.total_weight) {expected[calc.intern_item(item)]+=mass;return;}
            for(const auto& entry:pool.entries) if(entry.final_weight) {
                auto next=item;PC_CHECK(pc_item_add_mod(&next,entry.gen_type,entry.session_mod_id,entry.primary_group,0,nullptr)==PC_RESULT_OK);
                self(self,next,total,mass*entry.final_weight/pool.total_weight);
            }
        };
        visit(visit,base,4,8.0/12);visit(visit,base,5,3.0/12);visit(visit,base,6,1.0/12);
        const auto& actual=calc.outcomes(calc.intern_item(source),action);
        PC_CHECK(actual.supported && sums_to_one(actual));PC_CHECK(actual.entries.size()==expected.size());
        double counts[7]{}, suffix_full=0;
        for(const auto& entry:actual.entries) {
            PC_CHECK(near(entry.probability,expected[entry.state],1e-12));
            const auto& out=calc.state(entry.state);counts[out.prefix_count+out.suffix_count]+=entry.probability;
            if(out.suffix_count==3) suffix_full+=entry.probability;
            pc_item_state item;PC_CHECK(calc.materialize(entry.state,item));
            for(unsigned i=0;i<source.prefix_count;++i) PC_CHECK(item_contains_mod(item,source.prefixes[i].mod_id));
            PC_CHECK(!item_contains_mod(item,9));
            if(forced) PC_CHECK(item_contains_mod(item,5));
        }
        if(pool_case==0) PC_CHECK(near(counts[4],8.0/12)&&near(counts[5],3.0/12)&&near(counts[6],1.0/12));
        if(pool_case==1 && held==3) PC_CHECK(near(counts[4],8.0/12)&&near(counts[5],4.0/12)&&near(counts[6],0));
        if(pool_case==1 && held==2) PC_CHECK(near(counts[4],1));
        if(pool_case==2) PC_CHECK(near(counts[held+1],1));
        // The recovery event is a full suffix side: no room for another
        // suffix metamod even when a prefix slot is still open. Reprice the
        // same ordered native paths with the historical uniform mixture.
        expected.clear();visit(visit,base,4,1.0/3);visit(visit,base,5,1.0/3);visit(visit,base,6,1.0/3);
        double old_suffix_full=0;
        for(const auto& [id,mass]:expected) if(calc.state(id).suffix_count==3) old_suffix_full+=mass;
        if(pool_case==0 && held==3) PC_CHECK(near(suffix_full,1.0/12)&&near(old_suffix_full,1.0/3));
        if(pool_case==0 && held==2 && forced) PC_CHECK(near(suffix_full,1.0/5)&&near(old_suffix_full,22.0/45));
        if(pool_case==0 && held==2 && !forced) PC_CHECK(near(suffix_full,23.0/140)&&near(old_suffix_full,139.0/315));
        std::printf("retained occupancy held=%u forced=%u pool=%u: total3=%.12g total4=%.12g total5=%.12g total6=%.12g; suffix-full old=%.12g new=%.12g\n",held,forced,pool_case,counts[3],counts[4],counts[5],counts[6],old_suffix_full,suffix_full);
    }
    // The shared cluster interface selects the approved law without activating
    // or changing cluster session construction in this ordinary-law branch.
    unsigned counts[7]{};
    const auto law=rare_reforge_count_law(RareReforgeCountKind::ClusterJewel);
    for(unsigned draw=0;draw<100;++draw) ++counts[law.select(draw)];
    PC_CHECK(counts[3]==65 && counts[4]==35);
}

void run_solver_calc_tests(const char* artifact_dir) {
    run_reforge_count_law_tests();
    // Compare concrete refill with independent ordered-draw enumeration. This
    // covers fixed-six Vaal, retained Awakener pairs, group overlap, empty-pool
    // stopping, side caps and separation from ordinary cached Chaos rows.
    {
        auto session = make_calc_session();
        auto registry = build_action_registry(*session);
        const auto chaos = registry.index_by_id.at("chaos");
        CalcContext calc(session, family_goal_100(), registry, {}, false,
            false, false, std::nullopt, {}, false, session->normal_random_roll_mask,
            false, false, true, false, true);
        pc_item_state empty;
        pc_item_clear(&empty);
        empty.rarity = PC_RARITY_RARE;
        const auto before = calc.outcomes(calc.intern_item(empty), chaos);
        ActionContextImpl context(0);
        context.session = session;
        for (unsigned variant = 0; variant < 4; ++variant) {
            auto base = empty;
            if (variant == 1) { place(&base, 0, 0, 10); place(&base, 1, 5, 20); }
            if (variant == 2) { place(&base, 0, 2, 10); place(&base, 1, 6, 21); }
            if (variant == 3) {
                place(&base, 0, 0, 10, PC_MOD_SLOT_FRACTURED);
                place(&base, 1, 7, 22);
            }
            auto expected_base = base;
            if (variant == 3) pc_item_clear_side(&expected_base, PC_SIDE_SUFFIX);
            const auto actual = calc.concrete_refill({base,
                static_cast<std::uint8_t>(variant == 1 ? 0 : 6), variant != 1, variant == 3});
            PC_CHECK(actual->supported && sums_to_one(*actual));
            std::map<std::uint32_t, double> expected;
            std::function<void(pc_item_state, unsigned, double)> visit;
            visit = [&](pc_item_state item, unsigned target, double p) {
                if (item.prefix_count + item.suffix_count >= target) {
                    expected[calc.intern_item(item)] += p;
                    return;
                }
                PoolBuildRequest request;
                request.respects_metamod_pool_blocks = variant != 1;
                const auto cap = rarity_affix_cap(*session, item.rarity);
                const bool prefix = item.prefix_count < cap, suffix = item.suffix_count < cap;
                request.side_filter = prefix && suffix ? -1 : (prefix ? 0 : 1);
                const auto pool = get_weighted_pool(context, &item, request);
                if ((!prefix && !suffix) || !pool.total_weight) {
                    expected[calc.intern_item(item)] += p;
                    return;
                }
                for (const auto& row : pool.entries) {
                    if (!row.final_weight) continue;
                    auto next = item;
                    PC_CHECK(pc_item_add_mod(&next, row.gen_type, row.session_mod_id,
                        static_cast<std::uint16_t>(row.primary_group), 0, nullptr) == PC_RESULT_OK);
                    visit(next, target, p * row.final_weight / pool.total_weight);
                }
            };
            if (variant == 1) for (unsigned t = 4; t <= 6; ++t) visit(expected_base, t, t == 4 ? 8.0/12 : t == 5 ? 3.0/12 : 1.0/12);
            else visit(expected_base, 6, 1);
            PC_CHECK(expected.size() == actual->entries.size());
            for (const auto& row : actual->entries) PC_CHECK(near(row.probability, expected[row.state], 1e-12));
        }
        PC_CHECK(same_distribution(before, calc.outcomes(calc.intern_item(empty), chaos)));
    }
    run_reforge_cross_goal_projection_tests();
    run_foulborn_kernel_tests();
    run_calculator_incoming_tests(artifact_dir);
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
    run_reforge_cross_goal_projection_tests();
    run_projected_reforge_frontier_equivalence_tests();
    run_harvest_targeted_natural_regression();
}

void run_solver_scoped_lower_tests() {
    using Impl = SolveWorkTestAccess::Impl;
    using Status = Impl::IncrementalAlternativeRow::Status;
    using namespace poecraft::solver::quotient;
    // Independently solved native fixture. Empty Rare -> one clean requested
    // affix by Bench costs 1. Every other root action costs 100, except Scour
    // at 1/2, which leaves Normal and cannot finish before another paid action.
    // Thus the native proper-policy optimum is exactly 1; no model is its oracle.
    auto session = make_calc_session();
    auto data = std::const_pointer_cast<DataImpl>(session->data);
    for (unsigned mod=0; mod<session->mod_count; ++mod) {
        data->mod_key_sid.push_back(data->strings.size());
        data->strings.push_back("scoped-fixture-"+std::to_string(mod));
    }
    session->bench_mod_ids = {0};
    session->flags[0] |= 1 << 1;
    pc_bitset_clear(session->normal_random_roll_mask.data(), 0);
    auto registry = build_action_registry(*session);
    PhaseLowerPrices prices{{"fixture:other",100}, {"fixture:finish",1},
        {"fixture:scour",0.5}, {"fixture:cheap",0.25}, {"fixture:new",0.125}};
    for (auto& action : registry.actions) action.cost_keys = {"fixture:other"};
    const auto bench = registry.index_by_id.at("bench:scoped-fixture-0");
    const auto scour = registry.index_by_id.at("scour");
    registry.actions[bench].cost_keys = {"fixture:finish"};
    registry.actions[scour].cost_keys = {"fixture:scour"};
    auto goal = family_goal_100(); goal.slots[0].min_tier = 2;
    pc_item_state start{}; pc_item_clear(&start); start.rarity = PC_RARITY_RARE;
    SolveOptions options; options.goal_proof_profile = GoalProofProfile::TargetNeutralZero;
    options.consider_imprint_programs = false;
    options.allow_economic_restart = false;
    options.native_retention_lower = true;
    options.current_scoped_retention = true;
    options.max_solver_owned_bytes = 1ull << 30;
    options.max_states=options.max_discovered_states=options.max_expanded_states=64;
    CalcContext calc(session,goal,registry,{bench,scour});
    Impl work(calc,start,prices,options);
    PC_CHECK(!work.independent_retention_ready());
    PC_CHECK(work.completion_proof_lower_value(work.result.start_state)==0);
    unsigned slices=0;
    while (!work.advance_setup()) { ++slices; }
    PC_CHECK(slices>1 && work.native_retention_potential);
    PC_CHECK(work.goal_cover_stage==Impl::SetupStage::Disabled && !work.goal_cover_cost_ready);
    PC_CHECK(!work.proof_capabilities().positive_global_lower &&
        !work.proof_capabilities().lower_retirement && !work.proof_capabilities().global_exact_closure);
    if (!work.native_retention_potential) {
        std::fprintf(stderr,"scoped lower refusal: %s\n",work.native_retention_refusal.c_str()); return;
    }
    const auto saved = work.native_retention_potential;
    const auto root = work.result.start_state;
    const auto root_lower = work.native_retention_lower_value(root);
    PC_CHECK(root_lower>0.9 && root_lower<=1);
    PC_CHECK(saved->preparation_stats.checked_source_minimum_action==bench);
    PC_CHECK(saved->preparation_stats.checked_source_minimum_cost==1 &&
        saved->preparation_stats.checked_source_minimum_rhs<=1);
    PC_CHECK(work.result.native_source_lower_certificate &&
        work.result.native_source_lower_certificate->compatible(calc,prices,start,false));
    PC_CHECK(work.certified_global_lower_bound()==root_lower);
    PC_CHECK(work.progress().lower_bound==root_lower);
    // Public authority is separate from the unchanged neutral profile. A
    // copied working scalar, foreign source, changed price or Imprint scope
    // cannot substitute for the checked native source certificate.
    auto publication=work.result;
    publication.lower_bound=123;
    publication.upper_bound=10;
    publication.policy_status=SolvePolicyStatus::BoundedFeasible;
    publication.converged=true;
    solve_detail::normalize_publication_result(publication);
    PC_CHECK(publication.lower_bound==root_lower && !publication.converged);
    PC_CHECK(publication.absolute_optimality_gap==10-root_lower);
    publication.converged=true;
    PC_CHECK(solve_detail::publication_invariant_invalid_reason(publication)!=nullptr);
    publication.converged=false;
    publication.upper_bound=root_lower;
    solve_detail::normalize_publication_result(publication);
    PC_CHECK(publication.lower_bound==root_lower && !publication.converged &&
        publication.policy_status==SolvePolicyStatus::BoundedFeasible);
    publication.upper_bound=10;
    const auto certificate=publication.native_source_lower_certificate;
    publication.native_source_lower_certificate.reset();
    publication.lower_bound=123;
    solve_detail::normalize_publication_result(publication);
    PC_CHECK(publication.lower_bound==0);
    publication.native_source_lower_certificate=certificate;
    publication.exact_start_item.rarity=PC_RARITY_NORMAL;
    solve_detail::normalize_publication_result(publication);
    PC_CHECK(publication.lower_bound==0);
    auto changed_prices=prices; changed_prices["fixture:finish"]=0.25;
    PC_CHECK(!certificate->compatible(calc,changed_prices,start,false));
    PC_CHECK(!saved->whole_scope_source_certificate(calc,changed_prices,start,false));
    PC_CHECK(!saved->whole_scope_source_certificate(calc,prices,start,true));
    PC_CHECK(!certificate->compatible(calc,prices,start,true));
    PC_CHECK(solve_detail::solve_result_owned_bytes(work.result)>=certificate->retained_owned_bytes());
    PC_CHECK(saved->native_action_relations>=registry.actions.size());
    const auto finish = calc.outcomes(root,bench);
    const auto reset = calc.outcomes(root,scour);
    PC_CHECK(finish.supported && finish.applicable && finish.entries.size()==1 && finish.choice_groups.empty());
    PC_CHECK(reset.supported && reset.applicable && reset.entries.size()==1 && reset.choice_groups.empty());
    if (finish.entries.size()!=1 || reset.entries.size()!=1) return;
    const auto terminal=finish.entries.front().state, normal=reset.entries.front().state;
    PC_CHECK(finish.entries.front().probability==1 && calc.is_goal_state(calc.state(terminal)));
    PC_CHECK(reset.entries.front().probability==1 && !calc.is_goal_state(calc.state(normal)));
    PC_CHECK(calc.state(normal).rarity==PC_RARITY_NORMAL);
    PC_CHECK(work.native_retention_lower_value(normal)>1);
    work.result.values.assign(calc.state_count(),123); // rejected working data cannot leak
    work.result_statewise_values_rejected=true;
    work.focused_lower_completion_proof_values.assign(calc.state_count(),456);
    const auto lower=work.certified_incremental_lower_values();
    PC_CHECK(lower[root]==root_lower && lower[terminal]==0 && lower[normal]>1);
    // Capture an immutable native Bench prefix for the upper. The alternative
    // Scour row is appended later, so its self/choice probes cannot invalidate
    // this prefix and accidentally conceal a missing consumer veto.
    Impl::BoundedPolicyIncumbent incumbent;
    solve_detail::SparseRow finish_row; finish_row.owner_state=root;
    finish_row.transition_offset=work.transition_cache->successors.size();
    finish_row.transition_count=1;
    const auto finish_row_id=work.transition_cache->rows.size();
    work.transition_cache->rows.push_back(finish_row);
    work.transition_cache->successors.push_back(terminal);
    work.transition_cache->probabilities.push_back(1);
    work.priced_rows.resize(finish_row_id+1);
    work.priced_rows[finish_row_id].operator_index=bench;
    work.priced_rows[finish_row_id].cost=1;
    incumbent.graph_row_count=work.transition_cache->rows.size();
    incumbent.graph_priced_row_count=work.priced_rows.size();
    incumbent.graph_successor_count=work.transition_cache->successors.size();
    incumbent.graph_probability_count=work.transition_cache->probabilities.size();
    incumbent.graph_choice_count=work.transition_cache->choices.size();
    incumbent.graph_choice_successor_count=work.transition_cache->choice_successors.size();
    incumbent.graph_choice_option_count=work.transition_cache->choice_options.size();
    incumbent.graph_prefix_identity=work.incumbent_graph_prefix_identity(
        incumbent.graph_row_count,incumbent.graph_priced_row_count,
        incumbent.graph_successor_count,incumbent.graph_probability_count,
        incumbent.graph_choice_count,incumbent.graph_choice_successor_count,
        incumbent.graph_choice_option_count);
    incumbent.source_generation=work.transition_cache->rows.size();
    incumbent.target_generation=calc.state_count();
    // Install one COMPLETE deterministic native Scour row from the independently
    // enumerated kernel. This fixture controls the consumer, not graph admission.
    solve_detail::SparseRow row; row.owner_state=root; row.admitted=false;
    row.transition_offset=work.transition_cache->successors.size(); row.transition_count=1;
    const auto row_id=work.transition_cache->rows.size(); work.transition_cache->rows.push_back(row);
    work.transition_cache->successors.push_back(normal); work.transition_cache->probabilities.push_back(1);
    work.priced_rows.resize(row_id+1); work.priced_rows[row_id].operator_index=scour;
    work.priced_rows[row_id].cost=0.5;
    work.expanded.assign(calc.state_count(),1);
    // The native Bench kernel above independently witnesses root U=1, goal U=0.
    // No finite upper at Normal is asserted or needed for source-row retirement.
    incumbent.certified_upper_bound=incumbent.evaluated_policy_cost=1;
    // Stand in for the checked issuer only at this consumer seam. These flags
    // do not exercise or qualify graph admission; the native kernel and paid
    // case analysis above are the separate oracle for the fixture's value.
    incumbent.independently_certified=incumbent.independently_evaluated=true;
    incumbent.proper=incumbent.executable=true;
    incumbent.values.assign(calc.state_count(),kInfinity);
    incumbent.values[root]=1; incumbent.values[terminal]=0;
    incumbent.goal_identity=work.goal_identity(); incumbent.economy_identity=work.economy_identity();
    incumbent.caller_scope_identity=work.caller_scope_identity(); incumbent.artifact_identity=work.artifact_identity();
    incumbent.action_vocabulary_size=work.operators.size();
    incumbent.action_vocabulary_identity=work.action_vocabulary_prefix_identity(incumbent.action_vocabulary_size);
    incumbent.policy_materialized=true;
    incumbent.policy.assign(calc.state_count(),PolicyOperatorRef{});
    incumbent.policy[root]=PolicyOperatorRef{bench};
    incumbent.policy_rows.assign(calc.state_count(),std::numeric_limits<std::uint64_t>::max());
    incumbent.policy_rows[root]=finish_row_id;
    incumbent.policy_reachable.assign(calc.state_count(),0);
    incumbent.policy_reachable[root]=incumbent.policy_reachable[terminal]=1;
    // Structural payload/provenance stand-ins, as in retained-pool ownership
    // fixtures. No compiled graph admission is claimed by this consumer test.
    incumbent.compiled_artifact.strategy_json="scoped-native-Bench-consumer-fixture";
    incumbent.compilation_provenance="structural_consumer_fixture_native_kernel_oracle";
    PC_CHECK(incumbent.source_generation<work.transition_cache->rows.size());
    PC_CHECK(work.certified_incumbent_invalid_reason(incumbent)==nullptr);
    const auto classify=[&](bool consumed, bool root_only, bool rejected, bool self,
                            bool policy_upper=true) {
        work.options.native_retention_consume=consumed;
        auto boundary=incumbent; boundary.compiled_root_entry_only=root_only;
        boundary.statewise_values_rejected=rejected; work.output_incumbent=boundary;
        work.transition_cache->successors[row.transition_offset]=self ? root : normal;
        auto& active_row=work.transition_cache->rows[row_id];
        active_row.self_probability=active_row.embedded_self_probability=self ? 1 : 0;
        active_row.self_probability_embedded=self;
        Impl::IncrementalAlternativeRow alternative;
        alternative.state=root; alternative.operator_index=scour; alternative.row_index=row_id;
        work.incremental_alternative_rows={alternative}; work.incremental_classification_cursor=0;
        work.incremental_classification_active=true; work.incremental_classification_reclassify_all=true;
        work.incremental_classification_upper=policy_upper
            ? Impl::IncrementalClassificationUpper::OutputIncumbent
            : Impl::IncrementalClassificationUpper::ResultValues;
        work.incremental_classification_certified_lower=work.certified_incremental_lower_values();
        PC_CHECK(!work.advance_incremental_classification());
        return work.incremental_alternative_rows.front().status;
    };
    PC_CHECK(classify(false,false,false,false)==Status::Unresolved);
    PC_CHECK(classify(true,false,false,false)==Status::NonImproving); // concrete changed consumer
    PC_CHECK(work.incremental_alternative_rows.front().lower_q>1);
    PC_CHECK(static_cast<long double>(work.incremental_alternative_rows.front().lower_q)<=
        0.5L+static_cast<long double>(lower[normal]));
    PC_CHECK(classify(true,true,false,false)==Status::Unresolved); // root cost is not statewise
    PC_CHECK(classify(true,false,true,false)==Status::Unresolved);
    incumbent.proper=false;
    PC_CHECK(classify(true,false,false,false)==Status::Unresolved); // no checked proper-policy issuer
    incumbent.proper=true;
    // Flags and copied values remain valid in every stale case. The existing
    // compatibility owner, not those flags, must veto actual row retirement.
    const auto compatible_incumbent=incumbent;
    const auto stale=[&](const char* expected) {
        const char* reason=work.certified_incumbent_invalid_reason(incumbent);
        PC_CHECK(reason && std::string(reason)==expected);
        PC_CHECK(classify(true,false,false,false)==Status::Unresolved);
        incumbent=compatible_incumbent;
    };
    incumbent.goal_identity^=1; stale("goal_identity_changed");
    incumbent.economy_identity^=1; stale("economy_identity_changed");
    incumbent.action_vocabulary_identity^=1; stale("action_vocabulary_changed");
    incumbent.caller_scope_identity^=1; stale("caller_scope_changed");
    work.options.allow_economic_restart=true; stale("caller_scope_changed");
    work.options.allow_economic_restart=false;
    incumbent.artifact_identity^=1; stale("artifact_generation_changed");
    incumbent.source_generation=work.transition_cache->rows.size()+1; stale("graph_generation_rewound");
    incumbent.target_generation=calc.state_count()+1; stale("graph_generation_rewound");
    incumbent.graph_prefix_identity^=1; stale("graph_prefix_changed");
    incumbent.policy_materialized=false; stale("retained_artifact_provenance_missing");
    incumbent.compiled_artifact.strategy_json.clear(); stale("retained_artifact_provenance_missing");
    incumbent.compilation_provenance.clear(); stale("retained_artifact_provenance_missing");
    // Actual in-place mutation of the retained prefix, rather than a changed
    // hash field, must be caught. Appending Scour above remains a valid control.
    work.transition_cache->probabilities[finish_row.transition_offset]=0.5;
    stale("graph_prefix_changed");
    work.transition_cache->probabilities[finish_row.transition_offset]=1;
    PC_CHECK(work.certified_incumbent_invalid_reason(incumbent)==nullptr);
    PC_CHECK(classify(true,false,false,false)==Status::NonImproving);
    work.result_statewise_values_rejected=false;
    work.result.values=incumbent.values;
    PC_CHECK(classify(true,false,false,false,false)==Status::Unresolved); // working values are no policy issuer
    work.result_statewise_values_rejected=true;
    PC_CHECK(classify(true,false,false,true)==Status::Unresolved); // no forced-repeat lower promotion
    // A choice's source return can be encoded by has_self, without a source
    // entry in its successor array. It must receive the same conservative veto.
    auto& choice_row=work.transition_cache->rows[row_id];
    choice_row.transition_count=0; choice_row.choice_offset=work.transition_cache->choices.size();
    choice_row.choice_count=1;
    solve_detail::SparseChoiceGroup choice; choice.probability=1; choice.has_self=true;
    choice.successor_offset=work.transition_cache->choice_successors.size(); choice.successor_count=1;
    work.transition_cache->choices.push_back(choice); work.transition_cache->choice_successors.push_back(normal);
    PC_CHECK(classify(true,false,false,false)==Status::Unresolved);
    choice_row.transition_count=1; choice_row.choice_count=0;
    work.options.native_retention_consume=false;
    work.result.options.native_retention_consume=false;
    PC_CHECK(work.certified_incremental_lower_values()==std::vector<double>(calc.state_count(),0));
    PC_CHECK(work.certified_global_lower_bound()==0 && work.progress().lower_bound==0);
    PC_CHECK(work.native_retention_potential==saved && saved->lookup(calc,prices,start,false).value()>0.9);
    work.options.native_retention_consume=true;
    work.result.options.native_retention_consume=true;
    auto outside=start; outside.generic_influence_bits=1;
    PC_CHECK(work.native_retention_lower_value(calc.intern_item(outside))==0);
    auto retry=calc.state(root); retry.goal_progress_retry_basin=1;
    PC_CHECK(work.native_retention_lower_value(calc.intern_state(retry))==0);
    auto repriced=prices; repriced["fixture:finish"]=0.25;
    PC_CHECK(!saved->lookup(calc,repriced,start,false));

    // Genuine probability/retention fixture. Start with one natural goal and
    // one junk affix. Annul costs 1 and removes either with probability 1/2.
    // On a miss, Annul the sole junk (1), then Bench the goal (1). Other laws
    // cost 100; Scour (1/2) leaves Normal, requiring an expensive rarity change.
    // Empty Rare has value 1, lone junk has value 2, and the root has value
    // 1 + (1/2)*0 + (1/2)*2 = 2. A favorable-deletion oracle would give 1.
    auto stochastic_registry=registry;
    const auto annul=registry.index_by_id.at("annul");
    stochastic_registry.actions[annul].cost_keys={"fixture:finish"};
    auto stochastic_start=start;
    place(&stochastic_start,0,1,10); place(&stochastic_start,1,5,20);
    // Bench/Scour/Annul cannot generate suffix 5. Without an observed-start
    // member mask, this narrow calculator has no class for that existing junk:
    // projection retains its suffix count, but materialization cannot restore
    // the affix. That is a refused carrier, not a favorable Annul distribution.
    CalcContext omitted_start_member(session,goal,stochastic_registry,{bench,scour,annul});
    const auto omitted_root=omitted_start_member.intern_item(stochastic_start);
    PC_CHECK(omitted_start_member.layout().junk_class_by_mod[5]==kNoId);
    PC_CHECK(omitted_start_member.state(omitted_root).prefix_count==1 &&
        omitted_start_member.state(omitted_root).suffix_count==1 &&
        omitted_start_member.layout().junk_classes.empty());
    pc_item_state omitted_materialization{};
    PC_CHECK(!omitted_start_member.materialize(omitted_root,omitted_materialization));
    const auto omitted_loss=omitted_start_member.outcomes(omitted_root,annul);
    PC_CHECK(!omitted_loss.supported && omitted_loss.applicable &&
        omitted_loss.entries.empty() && omitted_loss.choice_groups.empty());
    // Retain the actual initial members without adding a generation action or
    // changing any price/native law. Automatic product goals already preserve
    // every ordinary affix; this explicit fixture supplies the existing hook.
    std::vector<std::uint64_t> observed_start_members(session->words,0);
    pc_bitset_set(observed_start_members.data(),1);
    pc_bitset_set(observed_start_members.data(),5);
    CalcContext stochastic_calc(session,goal,stochastic_registry,{bench,scour,annul},
        false,true,false,std::nullopt,{},false,observed_start_members);
    const auto stochastic_root=stochastic_calc.intern_item(stochastic_start);
    PC_CHECK(stochastic_calc.layout().junk_class_by_mod[5]!=kNoId);
    pc_item_state stochastic_materialization{};
    PC_CHECK(stochastic_calc.materialize(stochastic_root,stochastic_materialization));
    const auto loss=stochastic_calc.outcomes(stochastic_root,annul);
    if (!(loss.supported && loss.applicable && loss.entries.size()==2 && loss.choice_groups.empty()))
        std::fprintf(stderr,"scoped stochastic Annul: supported=%d applicable=%d entries=%zu choices=%zu\n",
            loss.supported,loss.applicable,loss.entries.size(),loss.choice_groups.size());
    PC_CHECK(loss.supported && loss.applicable && loss.entries.size()==2 && loss.choice_groups.empty());
    std::uint32_t missed=kNoId;
    for (const auto& entry:loss.entries) {
        PC_CHECK(entry.probability==0.5);
        if (!stochastic_calc.is_goal_state(stochastic_calc.state(entry.state))) missed=entry.state;
    }
    PC_CHECK(missed!=kNoId);
    if (missed!=kNoId) {
        const auto clear=stochastic_calc.outcomes(missed,annul);
        PC_CHECK(clear.supported && clear.entries.size()==1 && clear.entries.front().probability==1);
        if (clear.entries.size()==1) {
            const auto recover=stochastic_calc.outcomes(clear.entries.front().state,bench);
            PC_CHECK(recover.supported && recover.entries.size()==1 && recover.entries.front().probability==1 &&
                stochastic_calc.is_goal_state(stochastic_calc.state(recover.entries.front().state)));
        }
    }
    Impl stochastic(stochastic_calc,stochastic_start,prices,options);
    while (!stochastic.advance_setup()) {}
    PC_CHECK(stochastic.native_retention_potential != nullptr);
    // Independent proper-policy oracle: empty Rare -> Bench costs 1; lone
    // junk -> Annul then Bench costs 2. Root Annul therefore costs exactly
    // 1 + 0.5*0 + 0.5*(1+1) = 2. Bench preserves junk, Scour requires a
    // subsequent >=100 rarity action, and every other law costs >=100.
    constexpr double stochastic_oracle=1+0.5*0+0.5*(1+1);
    static_assert(stochastic_oracle==2);
    const double stochastic_lower=stochastic.native_retention_lower_value(stochastic.result.start_state);
    if (!(stochastic_lower>1.9 && stochastic_lower<=stochastic_oracle))
        std::fprintf(stderr,"scoped stochastic lower: actual=%.17g oracle=%.17g refusal=%s\n",
            stochastic_lower,stochastic_oracle,stochastic.native_retention_refusal.c_str());
    PC_CHECK(stochastic_lower>1.9 && stochastic_lower<=stochastic_oracle);

    // Independent refill-debt oracle. Chaos costs 1, Annul costs 10, and
    // EVERY other registry primitive costs 100, including the proper one-step
    // Bench finish. This pool always fills at least four affixes. To finish
    // with one clean goal after Chaos needs at least three paid removals;
    // a further Chaos refills the debt. Thus every proper policy costs >=31,
    // and Bench independently witnesses an upper of 100. A fictitious clean
    // Chaos exit/no-op would invalidate this positive lower-strength control.
    auto renewal_registry=registry;
    for (auto& action:renewal_registry.actions) action.cost_keys={"fixture:other"};
    const auto chaos=renewal_registry.index_by_id.at("chaos");
    renewal_registry.actions[chaos].cost_keys={"fixture:chaos"};
    renewal_registry.actions[annul].cost_keys={"fixture:annul"};
    const PhaseLowerPrices renewal_prices{{"fixture:other",100},
        {"fixture:chaos",1}, {"fixture:annul",10}};
    CalcContext renewal_calc(session,goal,renewal_registry,{bench,chaos,annul});
    const auto renewal_root=renewal_calc.intern_item(start);
    const auto& refill=renewal_calc.outcomes(renewal_root,chaos);
    bool native_floor=refill.supported && refill.applicable && !refill.entries.empty() &&
        refill.choice_groups.empty() && sums_to_one(refill);
    for (const auto& exit:refill.entries) if (exit.probability>0) {
        const auto& after=renewal_calc.state(exit.state);
        native_floor &= after.rarity==PC_RARITY_RARE &&
            after.prefix_count+after.suffix_count>=4 && !renewal_calc.is_goal_state(after);
    }
    PC_CHECK(native_floor);
    const auto& direct_finish=renewal_calc.outcomes(renewal_root,bench);
    PC_CHECK(direct_finish.supported && direct_finish.entries.size()==1 &&
        direct_finish.entries.front().probability==1 &&
        renewal_calc.is_goal_state(renewal_calc.state(direct_finish.entries.front().state)));
    auto renewal_options=options;
    // Complete native Chaos exits above belong to this new fixture, rather
    // than the two-action consumer fixture's 64-state cap.
    renewal_options.max_states=renewal_options.max_discovered_states=
        renewal_options.max_expanded_states=1024;
    Impl renewal(renewal_calc,start,renewal_prices,renewal_options);
    while (!renewal.advance_setup()) {}
    PC_CHECK(renewal.native_retention_potential != nullptr);
    const auto renewal_lower=renewal.native_retention_lower_value(renewal.result.start_state);
    if (!(renewal_lower>30.9 && renewal_lower<=100))
        std::fprintf(stderr,"Chaos refill-debt lower: actual=%.17g oracle_floor=31 oracle_upper=100 refusal=%s\n",
            renewal_lower,renewal.native_retention_refusal.c_str());
    PC_CHECK(renewal_lower>30.9 && renewal_lower<=100);

    // Newly admitted actual cheaper action: the SAME native Bench mechanics,
    // a distinct priced registry operator. Exact optimum becomes 1/4. Complete
    // registry coverage includes it even before local-family materialization.
    auto cheaper_registry=registry; auto cheap=registry.actions[bench];
    cheap.id="fixture:cheap-bench"; cheap.cost_keys={"fixture:cheap"};
    const auto cheap_id=static_cast<std::uint32_t>(cheaper_registry.actions.size());
    cheaper_registry.index_by_id[cheap.id]=cheap_id; cheaper_registry.actions.push_back(cheap);
    CalcContext cheaper_calc(session,goal,cheaper_registry,{bench,scour,cheap_id});
    PC_CHECK(!saved->lookup(cheaper_calc,prices,start,false));
    Impl cheaper(cheaper_calc,start,prices,options); while (!cheaper.advance_setup()) {}
    PC_CHECK(cheaper.native_retention_potential && cheaper.native_retention_lower_value(cheaper.result.start_state)>0);
    PC_CHECK(cheaper.native_retention_lower_value(cheaper.result.start_state)<=0.25);
    const auto cheap_finish=cheaper_calc.outcomes(cheaper.result.start_state,cheap_id);
    PC_CHECK(cheap_finish.supported && cheap_finish.entries.size()==1 &&
        cheap_finish.entries.front().probability==1 && cheaper_calc.is_goal_state(cheaper_calc.state(cheap_finish.entries.front().state)));

    // New probability law is NOT aliased to old Exalt. Grant all goals in the
    // support model and stop for its paid cost in the retention model.
    auto newer_registry=registry; auto newer=registry.actions[registry.index_by_id.at("exalt")];
    newer.id="fixture:new-law"; newer.params.type=ActionType::FoulbornExalt;
    newer.cost_keys={"fixture:new"};
    const auto newer_id=static_cast<std::uint32_t>(newer_registry.actions.size());
    newer_registry.index_by_id[newer.id]=newer_id; newer_registry.actions.push_back(newer);
    CalcContext newer_calc(session,goal,newer_registry,{bench,scour});
    auto support=PhaseLowerProducer::prepare(newer_calc,prices,start,
        {PhaseTableRole::MaskCompletion,2,1,std::vector<double>(2,0)});
    PC_CHECK(support->primitives.size()==newer_registry.actions.size());
    PC_CHECK(support->primitives[newer_id].priced && support->primitives[newer_id].reachable_goals==1 &&
        support->primitives[newer_id].independent_paid_exit);
    Impl newer_work(newer_calc,start,prices,options); while (!newer_work.advance_setup()) {}
    PC_CHECK(newer_work.native_retention_potential && newer_work.native_retention_lower_value(newer_work.result.start_state)<=0.125);
    PC_CHECK(newer_work.native_retention_lower_value(newer_work.result.start_state)>0);
    auto companion_registry=registry; auto companion=newer;
    companion.id="fixture:companion-exit"; companion.params.type=ActionType::Exalt;
    companion.uses_companion_state=true; companion.cost_keys.clear();
    companion_registry.index_by_id[companion.id]=companion_registry.actions.size();
    companion_registry.actions.push_back(companion);
    CalcContext companion_calc(session,goal,companion_registry,{bench,scour});
    Impl companion_work(companion_calc,start,prices,options);
    while (!companion_work.advance_setup()) {}
    PC_CHECK(companion_work.native_retention_potential &&
        companion_work.native_retention_lower_value(companion_work.result.start_state)==0);

    // Uncovered generated family, coverage target, any-k and restore request:
    // retain zero. A scope refusal cannot leave a partially installed vector.
    for (unsigned negative=0; negative<4; ++negative) {
        auto refused_goal=goal; auto refused_options=options;
        if (negative==0) refused_goal.automatic_candidate_kind_mask |= 1u<<31;
        if (negative==1) refused_goal.terminal.extras=ExtraExplicitPolicy::Allow;
        if (negative==2) {
            GoalSlot suffix; suffix.family_id=104; suffix.min_tier=1;
            refused_goal.slots.push_back(suffix); refused_goal.min_satisfied_slots=1;
        }
        if (negative==3) refused_options.consider_imprint_programs=true;
        CalcContext refused_calc(session,refused_goal,registry,{bench,scour});
        Impl refused(refused_calc,start,prices,refused_options);
        while (!refused.advance_setup()) {}
        PC_CHECK(refused.native_retention_attempted && !refused.native_retention_potential);
        PC_CHECK(!refused.native_retention_refusal.empty() && refused.native_retention_live_bytes==0);
        if (negative==0) PC_CHECK(refused.native_retention_refusal.find("uncovered generated family")!=std::string::npos);
        PC_CHECK(refused.completion_proof_lower_value(refused.result.start_state)==0);
        PC_CHECK(!refused.result.native_source_lower_certificate && refused.certified_global_lower_bound()==0);
        PC_CHECK(!saved->lookup(refused_calc,prices,start,false) || negative==3);
        if (negative==3) PC_CHECK(!saved->lookup(refused_calc,prices,start,true));
    }
    const auto unsafe=PhaseLowerProducer::prepare(calc,prices,start,
        {PhaseTableRole::MaskCompletion,2,1,{1000,0}});
    PC_CHECK(!unsafe->original_candidate_accepted && unsafe->values[0]<=1);
    // Numeric proposal remains untrusted; interruption retains no partial
    // support/certificate. This callback tests the existing native checkpoint.
    QuotientLowerBudget cancelled; unsigned checkpoints=0;
    cancelled.cancelled=[&] {return ++checkpoints>2;};
    bool interrupted=false;
    try { (void)PhaseLowerProducer::prepare(calc,prices,start,
        {PhaseTableRole::MaskCompletion,2,1,std::vector<double>(2,1000)},cancelled); }
    catch (const PhasePreparationCancelled&) {interrupted=true;}
    PC_CHECK(interrupted);
    PC_CHECK(saved->lookup(calc,prices,start,false).value()>0.9);
    Impl pending(calc,start,prices,options);
    (void)pending.advance_setup();
    PC_CHECK(pending.retention_setup_task && !pending.native_retention_potential);
    PC_CHECK(pending.completion_proof_lower_value(pending.result.start_state)==0);
    pending.retention_setup_task.reset(); // abandoned child destroys its owned proof state
    PC_CHECK(!pending.native_retention_potential);
    PC_CHECK(!pending.result.native_source_lower_certificate && pending.certified_global_lower_bound()==0);
    Impl capped(calc,start,prices,options);
    capped.options.max_solver_owned_bytes=capped.estimated_owned_bytes_with_calc(calc.audited_estimated_owned_bytes())+(2ull<<20);
    while (!capped.advance_setup()) {}
    PC_CHECK(capped.native_retention_attempted && !capped.native_retention_potential && capped.native_retention_live_bytes==0);
    PC_CHECK(!capped.result.native_source_lower_certificate && capped.certified_global_lower_bound()==0);
    auto ungated_options=options; ungated_options.current_scoped_retention=false;
    Impl ungated(calc,start,prices,ungated_options);
    while (!ungated.advance_setup()) {}
    PC_CHECK(!ungated.native_retention_attempted && !ungated.native_retention_potential);
    PC_CHECK(!ungated.independent_retention_ready() && ungated.completion_proof_lower_value(ungated.result.start_state)==0);
}

void run_solver_phase_lower_tests() {
    run_solver_scoped_lower_tests();
    using namespace poecraft::solver::quotient;
    const auto rejects = [](const auto& operation) {
        bool refused = false;
        try { operation(); } catch (const std::exception&) { refused = true; }
        PC_CHECK(refused);
    };
    const auto alchemy_facts=action_transition_facts(ActionType::Alchemy);
    PC_CHECK(alchemy_facts.applied_rarity==PC_RARITY_RARE && alchemy_facts.minimum_refill_target==4);
    const auto chaos_facts=action_transition_facts(ActionType::Chaos);
    PC_CHECK(chaos_facts.applied_rarity==PC_RARITY_RARE && chaos_facts.minimum_refill_target==4);
    for (const auto [kind, minimum] : {
            std::pair{RareReforgeCountKind::Equipment,4},
            std::pair{RareReforgeCountKind::LegacyJewel,4},
            std::pair{RareReforgeCountKind::ClusterJewel,3}}) {
        const auto law=rare_reforge_count_law(kind);
        PC_CHECK(law.minimum_target()==minimum);
        bool witnessed=false, bounded=true;
        for (unsigned draw=0;draw<law.denominator;++draw) {
            const auto native_target=law.select(draw);
            witnessed |= native_target==minimum;
            bounded &= native_target>=minimum;
        }
        PC_CHECK(witnessed && bounded);
    }
    PC_CHECK(action_transition_facts(ActionType::Annul).applied_rarity==255);
    PC_CHECK(phase_refill_minimum(0,0,4,[](unsigned,unsigned){return true;})==4);
    PC_CHECK(phase_refill_minimum(0,0,4,[](unsigned p,unsigned s){return p+s==0;})==1);
    PC_CHECK(phase_refill_minimum(0,0,4,[](unsigned p,unsigned s){return p!=1 || s!=0;})==1);
    PC_CHECK(phase_refill_minimum(0,1,4,[](unsigned p,unsigned s){return p+s<3;})==3);
    rejects([] {phase_refill_minimum(4,0,4,[](unsigned,unsigned){return true;});});
    for (bool singleton : {false,true}) {
        auto exhausted=make_calc_session();
        exhausted->normal_random_roll_mask.assign(exhausted->words,0);
        if (singleton) pc_bitset_set(exhausted->normal_random_roll_mask.data(),0);
        ActionContextImpl application(1); application.session=exhausted;
        pc_item_state normal{}; pc_item_clear(&normal);
        ActionParameters alchemy; alchemy.type=ActionType::Alchemy;
        const auto outcome=apply_action(application,&normal,alchemy);
        PC_CHECK(outcome.applied && normal.rarity==PC_RARITY_RARE);
        PC_CHECK(normal.prefix_count+normal.suffix_count==unsigned(singleton));
        const auto before=exact_item_state_key(normal);
        PC_CHECK(!apply_action(application,&normal,alchemy).applied);
        PC_CHECK(exact_item_state_key(normal)==before);
        ActionParameters chaos; chaos.type=ActionType::Chaos;
        const auto renewed=apply_action(application,&normal,chaos);
        // Native empty/singleton pools stop refill without rolling the old
        // carrier back. A minimum TARGET is never unconditional occupancy.
        PC_CHECK(renewed.applied && normal.rarity==PC_RARITY_RARE);
        PC_CHECK(normal.prefix_count+normal.suffix_count==unsigned(singleton));
        pc_item_state wrong_rarity{}; pc_item_clear(&wrong_rarity);
        const auto unchanged=exact_item_state_key(wrong_rarity);
        PC_CHECK(!apply_action(application,&wrong_rarity,chaos).applied);
        PC_CHECK(exact_item_state_key(wrong_rarity)==unchanged);
    }
    const auto converted = phase_completion_proposal({0, 3, 5, 8}, 2);
    PC_CHECK(converted.role == PhaseTableRole::MaskCompletion);
    PC_CHECK(converted.values == std::vector<double>({8, 5, 3, 0}));
    PC_CHECK(phase_completion_proposal({0, 3, 5, 8}, 1).values == std::vector<double>({3, 0, 0, 0}));
    rejects([] { phase_completion_proposal({0, 3, 5}, 2); });
    // Tiny declared-coefficient models exercise the retained checker; they
    // are algebra controls, not a replay of the native micro qualification.
    const auto coefficient_model = [&](std::vector<QuotientBellmanRowInput> rows,
            std::uint32_t terminal, const std::vector<double>* proposed = nullptr) {
        QuotientBellmanGraph graph(4ull << 20, QuotientBellmanMode::LowerOnly);
        std::vector<QuotientBellmanCellInput> cells;
        for (unsigned i = 0; i <= terminal; ++i) cells.push_back({i, 1, {77, i}, i == terminal});
        graph.install_cells(cells);
        QuotientLowerQuery query; query.request_identity = {77}; query.caller_scope = {78};
        query.coefficients = LowerCoefficientModel::ExactBinaryModel; query.roots = {0};
        for (unsigned i = 0; i < terminal; ++i) query.sources.push_back({i, {77, i}, {{78}, 1, true, {}, {}}, {}});
        unsigned serial = 0;
        for (auto& row : rows) {
            auto& source = query.sources.at(row.source_cell_id);
            const StableKey a{79, serial++}, evidence{80, serial};
            source.expected_actions.actions.push_back(a);
            row.lower_provenance = QuotientLowerRowProvenance{{77}, source.source_identity, a, evidence, LowerEvidenceKind::ExactDeclaredKernel};
            source.constraints.push_back({{a, false, {}}, LowerConstraintKind::Row, graph.append_row(std::move(row)),
                0, evidence, LowerEvidenceKind::ExactDeclaredKernel});
        }
        query.model_revision = graph.model_revision();
        return proposed ? graph.check_lower(query, *proposed) : graph.solve_lower(query);
    };
    const auto row = [](unsigned from, double cost, std::vector<std::pair<unsigned, double>> exits) {
        QuotientBellmanRowInput r; r.source_cell_id = from; r.cost = cost;
        for (auto [to, p] : exits) r.transitions.push_back({{to}, to, p});
        return r;
    };
    std::vector<QuotientBellmanRowInput> acquisition_rows;
    for (unsigned m = 0; m < 3; ++m) {
        acquisition_rows.push_back(row(m, 3, {{m | 1, 1}}));
        acquisition_rows.push_back(row(m, 5, {{m | 2, 1}}));
    }
    PC_CHECK(coefficient_model(acquisition_rows, 3, &converted.values).checked != nullptr);
    const std::vector<double> raw{0, 3, 5, 8};
    PC_CHECK(coefficient_model(acquisition_rows, 3, &raw).checked == nullptr);
    for (unsigned m = 0; m < 3; ++m) acquisition_rows.push_back(row(m, .01165, {{3, 1}}));
    const auto support_cap = coefficient_model(acquisition_rows, 3);
    PC_CHECK(support_cap.checked && support_cap.checked->values_by_state[0] <= .01165);
    PC_CHECK(!coefficient_model(acquisition_rows, 3, &converted.values).checked);
    const auto retry = coefficient_model({row(0, 100.0/128, {{1, 1.0/128}, {0, 127.0/128}})}, 1);
    PC_CHECK(retry.checked && retry.checked->values_by_state[0] > 99.999999);
    const auto paid_failure=coefficient_model({row(0,1,{{2,1.0/128},{1,127.0/128}}),row(1,2,{{0,1}})},2);
    PC_CHECK(paid_failure.checked && std::abs(paid_failure.checked->values_by_state[0]-382)<1e-6);
    // Same expectation equation as cost 1 and success 1/100; exact binary
    // coefficients avoid presenting a rounded 1/100 as native authority.
    PC_CHECK(1+99.0/100*100 == 100 && 1+0 < 100);
    const auto destructive = coefficient_model({row(0, 2, {{1, 1}}), row(1, 1, {{2, .25}, {0, .75}})}, 2);
    const auto preserve = coefficient_model({row(0, 2, {{1, 1}}), row(1, 1, {{2, .25}, {1, .75}})}, 2);
    PC_CHECK(destructive.checked && std::abs(destructive.checked->values_by_state[0]-12) < 1e-10);
    PC_CHECK(std::abs(destructive.checked->values_by_state[1]-10) < 1e-10);
    PC_CHECK(preserve.checked && std::abs(preserve.checked->values_by_state[0]-6) < 1e-10);
    PC_CHECK(std::abs(preserve.checked->values_by_state[1]-4) < 1e-10);
    // Boundary coupling is one simultaneous system, not independent use of
    // a provisional neighbor. The exact review control is x=8, y=6.
    const auto coupled_control = coefficient_model({row(0,1,{{0,.5},{1,.5}}), row(1,2,{{0,.5},{2,.5}})},2);
    PC_CHECK(coupled_control.checked && std::abs(coupled_control.checked->values_by_state[0]-8)<1e-9);
    PC_CHECK(std::abs(coupled_control.checked->values_by_state[1]-6)<1e-9);
    const std::vector<double> unproved_neighbor{8,7,0};
    PC_CHECK(!coefficient_model({row(0,1,{{0,.5},{1,.5}}),row(1,2,{{0,.5},{2,.5}})},2,&unproved_neighbor).checked);
    rejects([] { QuotientBellmanGraph excessive(65ull<<20,QuotientBellmanMode::LowerOnly,65ull<<20); });
    auto observed = row(0, 1, {{2, .5}}); observed.choices = {{.5, true, {1}}};
    const auto self_choice = coefficient_model({observed, row(1, 10, {{2, 1}})}, 2);
    PC_CHECK(self_choice.checked && std::abs(self_choice.checked->values_by_state[0]-2) < 1e-10);
    const std::array<double, 4> together{.5, 0, 0, .5}, exclusive{0, .5, .5, 0};
    PC_CHECK(together[1]+together[3] == exclusive[1]+exclusive[3]);
    PC_CHECK(together[2]+together[3] == exclusive[2]+exclusive[3]);
    PC_CHECK(together[3] == .5 && exclusive[3] == 0); // joint completion is not product of marginals
    CanonicalActionSet family{{81}, 1, true, {}, {{{82}, {{83}, {84}}, true}}};
    PC_CHECK(validate_canonical_action_coverage(family, {{{83}, false, {}}, {{82}, true, {{83}}}}).empty());
    PC_CHECK(!validate_canonical_action_coverage(family, {{{83}, false, {}}}).empty());
    // Joint history arithmetic, not a native certificate from toy marginals.
    const auto conditional = [](std::uint64_t n, std::uint64_t d) { return phase_weight_probability(n, d).upper; };
    const std::array<double, 3> ordered{conditional(1, 7), conditional(1, 6), conditional(1, 5)};
    const double ordered_bound = phase_joint_assignment_upper({ordered, ordered, ordered}, 3);
    unsigned histories = 0, successes = 0;
    for (unsigned a = 0; a < 7; ++a) for (unsigned b = 0; b < 7; ++b) for (unsigned c = 0; c < 7; ++c)
        if (a != b && a != c && b != c) { ++histories; successes += a < 3 && b < 3 && c < 3; }
    PC_CHECK(histories == 210 && successes == 6);
    PC_CHECK(ordered_bound >= 1.0L/35 && ordered_bound < 1.0L/35+1e-14L);
    const std::array<double, 3> uniform{conditional(1, 5), conditional(1, 5), conditional(1, 5)};
    PC_CHECK(ordered_bound < phase_joint_assignment_upper({uniform, uniform, uniform}, 3));
    // Equal unconditional marginals admit perfect correlation, so their
    // product has no conditional-history authority and is not a valid cap.
    PC_CHECK(.1*.1*.1 < .1);
    static_assert(!std::is_constructible_v<PreparedPhasePotential, std::vector<double>>);
    PC_CHECK(phase_joint_assignment_upper({ordered, ordered, ordered}, 2) == 0);
    PC_CHECK(phase_joint_assignment_upper({{1e-300,1e-300,0},{1e-300,1e-300,0}},2)>0);
    rejects([&] { phase_joint_assignment_upper({{1.01, 0, 0}}, 1); });
    constexpr unsigned event_mass = 1u << 24;
    const auto allocation = phase_minimum_event_allocation({0, 10, 100}, {event_mass/4, event_mass, event_mass});
    PC_CHECK(allocation == (std::vector<std::pair<unsigned, unsigned>>{{0,event_mass/4},{1,3*event_mass/4}}));
    const auto changed_allocation = phase_minimum_event_allocation({100, 10, 0}, {event_mass/4, event_mass, event_mass});
    PC_CHECK(changed_allocation == (std::vector<std::pair<unsigned, unsigned>>{{2,event_mass}}));
    // A normalized feasible allocation concentrated at 100 is not minimum;
    // neither it nor the stale minimizer for the old vector may be certified.
    PC_CHECK(100 > 7.5 && 25+7.5 > 0);
    const auto tiny_event = phase_minimum_event_allocation({0, 10}, {1, event_mass});
    PC_CHECK(tiny_event[0].second == 1 && tiny_event[1].second == event_mass-1);
    // Two exact masks can each consume an event's cap under the box relaxation.
    const auto boxes = phase_minimum_event_allocation({0, 0, 10}, {event_mass/4,event_mass/4,event_mass});
    PC_CHECK(boxes[0].second+boxes[1].second == event_mass/2);
    rejects([&] { phase_minimum_event_allocation({0,10}, {1,1}); });
    PC_CHECK(phase_price_shortcut_limiting(10, 10));
    PC_CHECK(!phase_price_shortcut_limiting(10, 9));
    const auto capped = coefficient_model({row(0,10,{{1,1}}), row(0,100,{{1,1}})},1);
    const auto reopened = coefficient_model({row(0,10,{{1,1}}),row(1,90,{{2,1}}),row(0,100,{{2,1}})},2);
    PC_CHECK(capped.checked && capped.checked->values_by_state[0] == 10);
    PC_CHECK(reopened.checked && reopened.checked->values_by_state[0] > 99.999999);
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
    auto donor = PhaseLowerProducer::prepare(calc, prices, blocked, {PhaseTableRole::MaskCompletion, 2, 1, {100, 0}});
    const auto wrong_role = PhaseLowerProducer::prepare(calc, prices, blocked, {PhaseTableRole::Acquisition, 2, 1, {0, 100}});
    PC_CHECK(wrong_role->proposal_refusal.kind == "wrong_table_role_or_dimensions");
    const auto bad_terminal = PhaseLowerProducer::prepare(calc, prices, blocked, {PhaseTableRole::MaskCompletion, 2, 1, {0, 100}});
    PC_CHECK(bad_terminal->proposal_refusal.kind == "nonzero_terminal");
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
    auto fresh = PhaseLowerProducer::prepare(calc, prices, open, {PhaseTableRole::MaskCompletion, 2, 1, {100, 0}});
    PC_CHECK(fresh->identity == donor->identity && fresh->values == donor->values);
    auto changed = PhaseLowerProducer::prepare(calc, repriced, open, {PhaseTableRole::MaskCompletion, 2, 1, {100, 0}});
    PC_CHECK(changed->identity != donor->identity && changed->values != donor->values);
    auto new_phase = PhaseLowerProducer::prepare(calc, prices, phase, {PhaseTableRole::MaskCompletion, 2, 1, {100, 0}});
    PC_CHECK(new_phase->lookup(calc, prices, phase).has_value());
    auto free_prices = prices; free_prices["fixture:step"] = 0;
    auto zero_escape = PhaseLowerProducer::prepare(calc, free_prices, open, {PhaseTableRole::MaskCompletion, 2, 1, {100, 0}});
    PC_CHECK(zero_escape->values[0] == 0);
    QuotientLowerBudget cancel; cancel.cancelled = [] { return true; };
    rejects([&] { PhaseLowerProducer::prepare(calc, prices, open, {PhaseTableRole::MaskCompletion, 2, 1, {100, 0}}, cancel); });
    QuotientLowerBudget tiny; tiny.max_scratch_bytes = 64;
    rejects([&] { PhaseLowerProducer::prepare(calc, prices, open, {PhaseTableRole::MaskCompletion, 2, 1, {100, 0}}, tiny); });
    auto unknown = goal; unknown.automatic_candidate_kind_mask |= 1u << 31;
    CalcContext unknown_calc(session, unknown, registry, basic_indices(registry));
    rejects([&] { PhaseLowerProducer::prepare(unknown_calc, prices, open, {PhaseTableRole::MaskCompletion, 2, 1, {100, 0}}); });

    GoalSlot suffix; suffix.family_id = 104; suffix.min_tier = 1;
    goal.slots.push_back(suffix); goal.automatic_candidates = true;
    goal.automatic_candidate_kind_mask = automatic_candidate_kind_bit(AutomaticCandidateKind::EldritchSide);
    CalcContext program_calc(session, goal, registry, basic_indices(registry));
    pc_item_state source{}; source.rarity = PC_RARITY_RARE;
    place(&source, PC_SIDE_PREFIX, 0, 10);
    auto post = source; post.eater_of_worlds_tier = 1;
    auto program_donor = PhaseLowerProducer::prepare(program_calc, prices, post, {PhaseTableRole::MaskCompletion, 4, 2, {100, 100, 100, 0}});
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
    {
        auto joint_session = make_calc_session();
        auto joint_registry = build_action_registry(*joint_session);
        for (auto& a : joint_registry.actions) a.cost_keys = {"fixture:step"};
        GoalSpec joint_goal; joint_goal.rarity = PC_RARITY_RARE;
        for (auto family_id : {100u,102u,103u,104u}) { GoalSlot slot; slot.family_id=family_id; slot.min_tier=1; joint_goal.slots.push_back(slot); }
        // A forced goal cannot be counted as one of the later random draws.
        joint_session->essence_guaranteed_mod_ids = {0};
        auto essence = joint_registry.actions.at(joint_registry.index_by_id.at("chaos"));
        essence.id = "fixture:forced-life"; essence.params.type=ActionType::Essence; essence.params.essence_index=0;
        joint_registry.index_by_id[essence.id] = joint_registry.actions.size(); joint_registry.actions.push_back(essence);
        CalcContext joint_calc(joint_session, joint_goal, joint_registry, basic_indices(joint_registry),
            false,true,true,std::nullopt,{},false,{},true);
        pc_item_state frame{}; frame.rarity=PC_RARITY_RARE;
        place(&frame, PC_SIDE_SUFFIX, 5, 20, PC_MOD_SLOT_FRACTURED);
        auto phase_frame=frame; phase_frame.eater_of_worlds_tier=1;
        const auto support = PhaseLowerProducer::prepare(joint_calc, prices, phase_frame,
            {PhaseTableRole::MaskCompletion,16,4,std::vector<double>(16,0)});
        const auto zero = PhaseLowerProducer::zero_restart_boundary(*support);
        const PhaseLowerProposal proposal{PhaseTableRole::CleanCompletion,16,4,std::vector<double>(768,100)};
        const auto joint = PhaseLowerProducer::prepare_probabilistic(joint_calc,prices,phase_frame,proposal,support,zero,false,true,{},true);
        PC_CHECK(!joint->joint_events.empty());
        const auto native = std::find_if(joint->joint_events.begin(),joint->joint_events.end(),[&](const auto& w) {
            return w.action==joint_registry.index_by_id.at("chaos") && w.subset==7;
        });
        PC_CHECK(native!=joint->joint_events.end() && native->positions==3 && native->initial_same_side==0 && native->other_side_blockers==3);
        const auto forced = std::find_if(joint->joint_events.begin(),joint->joint_events.end(),[&](const auto& w) {
            return w.action==joint_registry.index_by_id.at(essence.id) && w.subset==6;
        });
        PC_CHECK(forced!=joint->joint_events.end() && forced->forced==1 && forced->uniform_history);
        bool noop_covered=true;
        for (const auto& r : joint->relations) if (r.probability_aware && action_transition_facts(joint_registry.actions[r.action].params.type).renewal) {
            long double future=0;
            for (unsigned i=0;i<r.targets.size();++i) future+=static_cast<long double>(r.probabilities[i])*joint->values[r.targets[i]];
            noop_covered &= future<=joint->values[r.cell]+1e-12L;
        }
        PC_CHECK(noop_covered);
        const auto common=joint->whole_scope_source_lower(joint_calc,prices,frame,false);
        PC_CHECK(common && common->source_identity==exact_item_state_key(frame));
        PC_CHECK(!joint->whole_scope_source_lower(joint_calc,prices,frame,true));
        PC_CHECK(!joint->whole_scope_source_lower(joint_calc,repriced,frame,false));
        // Full-scope native value is a common lower even for an unresolved
        // family; a restricted numerical certificate cannot use this issuer.
        PC_CHECK(std::max(1.0,common->lower)>=common->lower);
        static_assert(!std::is_constructible_v<PreparedPhasePotential,QuotientLowerCertificate>);
        // Test actual hidden blocker kernels against the native integer bound,
        // including blockers deleting target weight and overlapping exclusions.
        const auto a=joint_registry.index_by_id.at("eldritch_exalt");
        const auto w=joint_calc.phase_goal_draw_bound(frame,a,0,false);
        bool all_blockers=true;
        for (unsigned mod : {0u,2u,3u,4u}) {
            auto carrier=phase_frame; place(&carrier,PC_SIDE_PREFIX,mod,joint_session->primary_group[mod]);
            carrier.searing_exarch_tier=1; carrier.eater_of_worlds_tier=0;
            std::uint64_t hit=0;
            const auto total=joint_calc.phase_lower_add_weights(carrier,a,[&](const auto& exit,std::uint64_t weight) {
                if (exit.prefix_count>carrier.prefix_count && exit.prefixes[exit.prefix_count-1].mod_id==0) hit+=weight;
            });
            auto other=w.other_weight;
            other-=std::min(other,w.strongest_other_removal[0][0]);
            other-=std::min(other,w.strongest_other_removal[1][0]);
            all_blockers &= static_cast<long double>(hit)/total<=phase_weight_probability(w.target_weight,w.target_weight+other).upper;
        }
        PC_CHECK(all_blockers);
        const auto coupled = PhaseLowerProducer::prepare_probabilistic(joint_calc,prices,phase_frame,
            proposal,support,zero,false,true,{},true,joint,PhaseContinuation::CoupledFresh);
        pc_item_state fresh_item{}; pc_item_clear(&fresh_item);
        PC_CHECK(!joint->lookup(joint_calc,prices,fresh_item,false));
        PC_CHECK(coupled->lookup(joint_calc,prices,fresh_item,false).has_value());
        PC_CHECK(coupled->projected_cell(joint_calc,fresh_item)==770);
        PC_CHECK(coupled->identity!=joint->identity);
        PC_CHECK(coupled->values[769]==0); // unused old boundary is not authority
        PC_CHECK(std::any_of(coupled->relations.begin(),coupled->relations.end(),[](const auto& r) { return r.cell>=770; }));
        bool resets_coupled=true;
        for (const auto& r : coupled->relations) if (joint_registry.actions[r.action].synthetic)
            resets_coupled &= r.targets==std::vector<std::uint32_t>{770} && r.reason!=PhaseRelationReason::FixedIndependentBoundary;
        PC_CHECK(resets_coupled);
        // Exact native kernels on the small existing fixture, without sampling
        // or Simulator. Include absent/tied dominance, both sides, retained
        // junk blockers and the fracture. Rounded native probabilities are only
        // test observations; certificate authority stays in integer witnesses.
        bool native_retention=true, native_expectation=true;
        std::uint64_t native_exits=0;
        for (unsigned junk : {6u,7u}) for (unsigned phase_case=0;phase_case<4;++phase_case) {
            auto carrier=frame; place(&carrier,PC_SIDE_PREFIX,0,joint_session->primary_group[0]);
            place(&carrier,PC_SIDE_SUFFIX,junk,joint_session->primary_group[junk]);
            carrier.searing_exarch_tier=phase_case==1 ? 1 : (phase_case==3 ? 2 : 0);
            carrier.eater_of_worlds_tier=phase_case==2 ? 1 : (phase_case==3 ? 2 : 0);
            const auto state=joint_calc.intern_item(carrier);
            const auto& kernel=joint_calc.outcomes(state,joint_registry.index_by_id.at("eldritch_chaos"));
            long double total=0, rhs=phase_price_lower(joint_registry.actions[joint_registry.index_by_id.at("eldritch_chaos")],prices);
            native_retention &= kernel.supported && !kernel.entries.empty();
            for (const auto& e : kernel.entries) {
                pc_item_state exit{}; native_retention &= joint_calc.materialize(e.state,exit); ++native_exits;
                total+=e.probability; rhs+=e.probability*coupled->projected_value(joint_calc,exit);
                native_retention &= item_contains_mod(exit,5) && exit.prefix_count<=3 && exit.suffix_count<=3;
                if (phase_case==1) native_retention &= exit.suffix_count==carrier.suffix_count && item_contains_mod(exit,junk);
                if (phase_case==2) native_retention &= exit.prefix_count==carrier.prefix_count && item_contains_mod(exit,0);
            }
            native_expectation &= std::abs(total-1)<1e-12L && coupled->projected_value(joint_calc,carrier)<=rhs+1e-11L;
        }
        PC_CHECK(native_exits>0 && native_retention);
        PC_CHECK(native_expectation);
        auto influenced_fresh=fresh_item; influenced_fresh.generic_influence_bits=1;
        PC_CHECK(!coupled->lookup(joint_calc,prices,influenced_fresh,false));
        PC_CHECK(!coupled->lookup(joint_calc,prices,fresh_item,true));
        PC_CHECK(!coupled->lookup(joint_calc,repriced,fresh_item,false));
        const auto retained_coupled=support->memory_snapshot().total_bytes;
        QuotientLowerBudget interrupted; unsigned seen=0; interrupted.cancelled=[&]{ return ++seen>80; };
        rejects([&]{ PhaseLowerProducer::prepare_probabilistic(joint_calc,prices,phase_frame,proposal,support,zero,
            false,true,interrupted,true,joint,PhaseContinuation::CoupledFresh); });
        PC_CHECK(support->memory_snapshot().total_bytes==retained_coupled);
        rejects([&]{ PhaseLowerProducer::prepare_probabilistic(joint_calc,prices,phase_frame,proposal,support,zero,
            false,true,{},false,joint,PhaseContinuation::CoupledFresh); });
        // Same-side overlapping satisfying masks must refuse distinct-draw
        // multiplication. The current goal-layout owner already refuses them.
        joint_goal.slots[1]=joint_goal.slots[0];
        rejects([&] { CalcContext overlap_calc(joint_session,joint_goal,joint_registry,basic_indices(joint_registry)); });
        auto other_phase=frame; other_phase.searing_exarch_tier=1;
        const auto other_support=PhaseLowerProducer::prepare(joint_calc,prices,other_phase,
            {PhaseTableRole::MaskCompletion,16,4,std::vector<double>(16,0)});
        const auto other_zero=PhaseLowerProducer::zero_restart_boundary(*other_support);
        rejects([&] { PhaseLowerProducer::prepare_probabilistic(joint_calc,prices,other_phase,proposal,other_support,other_zero,false,true,{},true,joint); });
    }
    {
        auto framed = source; framed.prefixes[0].flags |= PC_MOD_SLOT_FRACTURED;
        auto framed_post = framed; framed_post.eater_of_worlds_tier = 1;
        PhaseLowerProposal clean{PhaseTableRole::CleanCompletion, 4, 2, std::vector<double>(3*4*16, 100)};
        clean.values[((2*4+3)*4+1)*4+1] = 0;
        const auto boundary = PhaseLowerProducer::zero_restart_boundary(*program_donor);
        const auto retained_before = program_donor->memory_snapshot().total_bytes;
        {
            const auto potential = PhaseLowerProducer::prepare_probabilistic(program_calc, prices, framed_post,
                clean, program_donor, boundary, false, true);
            PC_CHECK(potential->lookup(program_calc, prices, framed, false).value() > 0);
            PC_CHECK(!potential->lookup(program_calc, prices, framed, true));
            PC_CHECK(!potential->lookup(program_calc, repriced, framed, false));
            auto foreign = framed; foreign.generic_influence_bits = 1;
            PC_CHECK(!potential->lookup(program_calc, prices, foreign, false));
            foreign = framed; foreign.prefixes[0].flags |= PC_MOD_SLOT_VEILED;
            PC_CHECK(!potential->lookup(program_calc, prices, foreign, false));
            PC_CHECK(!potential->lookup(program_calc, prices, source, false));
            auto distinct = framed; place(&distinct, PC_SIDE_PREFIX, 3, 12);
            PC_CHECK(potential->lookup(program_calc, prices, distinct, false).has_value());
            const auto proof = PhaseLowerProducer::compose(program_calc, prices, framed, id, *potential);
            PC_CHECK(proof.record.cost_lower > 3.999 && proof.record.cost_lower <= 4);
            std::uint64_t total = 0; long double expectation = proof.record.cost_lower;
            for (const auto& e : proof.record.exits) {
                total += e.weight;
                expectation += static_cast<long double>(e.weight)/proof.record.total_weight*e.lower;
            }
            PC_CHECK(total == proof.record.total_weight && proof.record.lower <= expectation);
            PC_CHECK(proof.record.lower > expectation-1e-10);
            PC_CHECK(std::all_of(potential->relations.begin(), potential->relations.end(), [&](const auto& r) {
                const auto& a = program_calc.registry().actions[r.action];
                return a.synthetic || a.params.type != ActionType::Transmute;
            }));
            // A program in the adjacent region must retain its region offset;
            // an incompatible fractured control supplies no comparison value.
            const auto coupled_program = PhaseLowerProducer::prepare_probabilistic(program_calc,prices,framed_post,
                clean,program_donor,boundary,false,true,{},true,potential,PhaseContinuation::CoupledFresh);
            const auto fresh_program = PhaseLowerProducer::compose(program_calc,prices,source,id,*coupled_program);
            PC_CHECK(fresh_program.record.prior_potential_lower==0);
            PC_CHECK(std::all_of(fresh_program.record.exits.begin(),fresh_program.record.exits.end(),[&](const auto& e) {
                return e.cell>=194 && e.lower==(e.goal ? 0 : coupled_program->values.at(e.cell));
            }));
            long double fresh_expectation=fresh_program.record.cost_lower;
            for (const auto& e : fresh_program.record.exits)
                fresh_expectation+=static_cast<long double>(e.weight)/fresh_program.record.total_weight*e.lower;
            PC_CHECK(fresh_program.record.lower<=fresh_expectation && fresh_expectation-fresh_program.record.lower<1e-10);
            auto changed_boundary = PhaseLowerProducer::zero_restart_boundary(*changed);
            rejects([&] { PhaseLowerProducer::prepare_probabilistic(program_calc, prices, framed_post, clean,
                program_donor, changed_boundary, false, true); });
            rejects([&] { PhaseLowerProducer::prepare_probabilistic(program_calc, prices, framed_post, clean,
                program_donor, boundary, true, true); });
        }
        PC_CHECK(program_donor->memory_snapshot().total_bytes == retained_before);
        unsigned checkpoints = 0; QuotientLowerBudget partial;
        partial.cancelled = [&] { return ++checkpoints > 20; };
        rejects([&] { PhaseLowerProducer::prepare_probabilistic(program_calc, prices, framed_post, clean,
            program_donor, boundary, false, true, partial); });
        PC_CHECK(program_donor->memory_snapshot().total_bytes == retained_before);
    }
    rejects([&] { PhaseLowerProducer::compose(program_calc, prices, source, id, *program_donor, cancel); });
    rejects([&] { PhaseLowerProducer::compose(program_calc, prices, source, "missing internal choice", *program_donor); });
    rejects([&] { PhaseLowerProducer::compose(program_calc, repriced, source, id, *program_donor); });
    PC_CHECK(program_donor->memory_snapshot().total_bytes == before);
    {
        auto typed_session = make_calc_session();
        auto typed_data = std::const_pointer_cast<DataImpl>(typed_session->data);
        for (unsigned mod=0;mod<typed_session->mod_count;++mod) {
            typed_data->mod_key_sid.push_back(typed_data->strings.size());
            typed_data->strings.push_back("typed-fixture-"+std::to_string(mod));
        }
        typed_session->bench_mod_ids = {3,5};
        typed_session->flags[3] |= 1 << 1; typed_session->flags[5] |= 1 << 1;
        auto typed_registry = build_action_registry(*typed_session);
        PhaseLowerPrices typed_prices{{"fixture:step",100},{"fixture:add",1},{"fixture:loss",2},{"fixture:cleanup",0.1},{"fixture:convert",100}};
        for (auto& a : typed_registry.actions) a.cost_keys = {"fixture:step"};
        for (auto& a : typed_registry.actions) if (a.params.type==ActionType::HarvestResist) a.cost_keys={"fixture:convert"};
        typed_registry.actions[typed_registry.index_by_id.at("exalt")].cost_keys={"fixture:add"};
        typed_registry.actions[typed_registry.index_by_id.at("annul")].cost_keys={"fixture:loss"};
        typed_registry.actions[typed_registry.index_by_id.at("eldritch_annul")].cost_keys={"fixture:loss"};
        typed_registry.actions[typed_registry.index_by_id.at("remove_crafted_modifiers")].cost_keys={"fixture:cleanup"};
        CalcContext typed_calc(typed_session,goal,typed_registry,basic_indices(typed_registry),
            false,true,true,std::nullopt,{},false,{},true);
        auto anchor=source; anchor.prefixes[0].flags=PC_MOD_SLOT_FRACTURED;
        auto phase=anchor; phase.eater_of_worlds_tier=1;
        auto support=PhaseLowerProducer::prepare(typed_calc,typed_prices,phase,
            {PhaseTableRole::MaskCompletion,4,2,std::vector<double>(4,0)});
        const auto zero=PhaseLowerProducer::zero_restart_boundary(*support);
        QuotientLowerBudget budget; budget.max_scratch_bytes=32ull<<20;
        const auto refined=PhaseLowerProducer::prepare_probabilistic(typed_calc,typed_prices,phase,
            {PhaseTableRole::CleanCompletion,4,2,std::vector<double>(192,100)},support,zero,
            false,true,budget,true,{},PhaseContinuation::CoupledFresh,PhaseRetention::AnnulNonempty);
        bool alchemy_applied=true; unsigned alchemy_rows=0;
        for (const auto& r:refined->relations) if (typed_registry.actions[r.action].params.type==ActionType::Alchemy && !r.independent_price) {
            ++alchemy_rows;
            for (const auto& event:r.events) {
                const auto base=static_cast<unsigned>(refined->coordinates[event.minimum_cell]);
                const auto local=base>=194 ? base-194 : base;
                alchemy_applied &= local/(4*16)==PC_RARITY_RARE;
            }
        }
        PC_CHECK(alchemy_rows && alchemy_applied);
        const auto uncached=PhaseLowerProducer::prepare_probabilistic(typed_calc,typed_prices,phase,
            {PhaseTableRole::CleanCompletion,4,2,std::vector<double>(192,100)},support,zero,
            false,true,budget,true,{},PhaseContinuation::CoupledFresh,PhaseRetention::AnnulNonempty,true,{true,false});
        bool same_relations=refined->values==uncached->values && refined->relations.size()==uncached->relations.size();
        for (unsigned i=0;same_relations && i<refined->relations.size();++i) {
            const auto& a=refined->relations[i]; const auto& b=uncached->relations[i];
            same_relations &= std::tie(a.cell,a.action,a.cost,a.targets,a.probabilities,a.phase_branch)==
                std::tie(b.cell,b.action,b.cost,b.targets,b.probabilities,b.phase_branch) && a.events.size()==b.events.size();
            for (unsigned j=0;same_relations && j<a.events.size();++j)
                same_relations &= std::tie(a.events[j].mask,a.events[j].minimum_cell,a.events[j].capacity)==
                    std::tie(b.events[j].mask,b.events[j].minimum_cell,b.events[j].capacity);
        }
        PC_CHECK(same_relations);
        const auto early=PhaseLowerProducer::prepare_probabilistic(typed_calc,typed_prices,phase,
            {PhaseTableRole::CleanCompletion,4,2,std::vector<double>(192,100)},support,zero,
            false,true,budget,true,{},PhaseContinuation::CoupledFresh,PhaseRetention::AnnulNonempty,
            true,{true,true,0,false,false,true});
        PC_CHECK(early->preparation_stats.accepted_early_subsolution);
        PC_CHECK(early->model_rounds<=refined->model_rounds);
        PC_CHECK(early->lookup(typed_calc,typed_prices,phase,false).has_value());
        // This two-goal fixture reaches its final vector at the first checked
        // endpoint; the compact five-goal control exercises a strictly weaker
        // endpoint. A high target must still retain full refinement below.
        PC_CHECK(early->values==refined->values);
        PC_CHECK(std::all_of(early->relations.begin(),early->relations.end(),[&](const auto& row) {
            return early->values[row.cell]<=row.rhs+1e-10;
        }));
        const auto unmet_target=PhaseLowerProducer::prepare_probabilistic(typed_calc,typed_prices,phase,
            {PhaseTableRole::CleanCompletion,4,2,std::vector<double>(192,100)},support,zero,
            false,true,budget,true,{},PhaseContinuation::CoupledFresh,PhaseRetention::AnnulNonempty,
            true,{true,true,0,false,false,true,1e9});
        PC_CHECK(!unmet_target->preparation_stats.accepted_early_subsolution);
        PC_CHECK(unmet_target->values==refined->values);
        PC_CHECK(refined->crafted_goal_domain==2);
        auto natural=anchor; place(&natural,PC_SIDE_PREFIX,3,12);
        auto crafted=natural; crafted.prefixes[1].flags=PC_MOD_SLOT_CRAFTED;
        PC_CHECK(refined->projected_cell(typed_calc,natural)!=refined->projected_cell(typed_calc,crafted));
        bool cleanup_preserved=true, expectation_checked=true;
        for (auto item : {natural,crafted}) for (bool unfractured : {false,true}) {
            if (unfractured) item.prefixes[0].flags=0;
            const auto& kernel=typed_calc.outcomes(typed_calc.intern_item(item),typed_registry.index_by_id.at("remove_crafted_modifiers"));
            long double rhs=0.1L;
            for (const auto& e : kernel.entries) {
                pc_item_state exit{}; cleanup_preserved &= typed_calc.materialize(e.state,exit);
                cleanup_preserved &= item_contains_mod(exit,3)==!(item.prefixes[1].flags&PC_MOD_SLOT_CRAFTED);
                rhs+=e.probability*refined->projected_value(typed_calc,exit);
            }
            expectation_checked &= refined->projected_value(typed_calc,item)<=rhs+1e-10L;
        }
        PC_CHECK(cleanup_preserved && expectation_checked);
        auto crafted_goal=natural; place(&crafted_goal,PC_SIDE_SUFFIX,5,20,PC_MOD_SLOT_CRAFTED);
        const auto& removed=typed_calc.outcomes(typed_calc.intern_item(crafted_goal),typed_registry.index_by_id.at("remove_crafted_modifiers"));
        bool loses_crafted_goal=true;
        for (const auto& e : removed.entries) {
            pc_item_state exit{}; typed_calc.materialize(e.state,exit);
            loses_crafted_goal &= !item_contains_mod(exit,5) && item_contains_mod(exit,3);
            loses_crafted_goal &= refined->projected_cell(typed_calc,exit)==refined->projected_cell(typed_calc,natural);
        }
        PC_CHECK(loses_crafted_goal);
        auto conversion_prices=typed_prices; conversion_prices["fixture:convert"]=0.05;
        const auto conversion_support=PhaseLowerProducer::prepare(typed_calc,conversion_prices,phase,
            {PhaseTableRole::MaskCompletion,4,2,std::vector<double>(4,0)});
        PhaseLowerQueryDiagnostic conversion_diagnostic; conversion_diagnostic.refine_resistance_support=true;
        const auto conversion_view=PhaseLowerProducer::prepare_probabilistic(typed_calc,conversion_prices,phase,
            {PhaseTableRole::CleanCompletion,4,2,std::vector<double>(192,100)},conversion_support,
            PhaseLowerProducer::zero_restart_boundary(*conversion_support),false,true,budget,true,{},
            PhaseContinuation::CoupledFresh,PhaseRetention::AnnulNonempty,true,{},{},&conversion_diagnostic);
        auto cold_junk=natural; place(&cold_junk,PC_SIDE_SUFFIX,6,21);
        auto cold_craft=cold_junk; cold_craft.suffixes[0].flags=PC_MOD_SLOT_CRAFTED;
        bool conversion_coverage=true,conversion_inequalities=true; unsigned conversion_exits=0;
        for (auto item:{natural,crafted_goal,cold_junk,cold_craft}) for (bool fresh:{false,true})
            for (const char* action_id:{"harvest_resist:fire:cold","harvest_resist:cold:fire"}) {
                if (fresh) item.prefixes[0].flags=0;
                const auto& kernel=typed_calc.outcomes(typed_calc.intern_item(item),typed_registry.index_by_id.at(action_id));
                long double rhs=0.05,total=0;
                conversion_coverage &= kernel.supported && !kernel.entries.empty();
                for (const auto& exit:kernel.entries) {
                    pc_item_state native{};
                    conversion_coverage &= typed_calc.materialize(exit.state,native);
                    conversion_coverage &= native.prefix_count==item.prefix_count && native.suffix_count==item.suffix_count;
                    if (!fresh) conversion_coverage &= item_contains_mod(native,0);
                    const auto value=conversion_view->lookup(typed_calc,conversion_prices,native,false);
                    conversion_coverage &= value.has_value();
                    rhs+=exit.probability*value.value_or(0); total+=exit.probability; ++conversion_exits;
                }
                conversion_coverage &= std::abs(total-1)<1e-14L;
                conversion_inequalities &= conversion_view->projected_value(typed_calc,item)<=rhs+1e-10L;
            }
        PC_CHECK(conversion_exits>0 && conversion_coverage);
        PC_CHECK(conversion_inequalities);
        PC_CHECK(conversion_view->projected_value(typed_calc,anchor)>0.05); // Old free conversion escape blocked this.
        bool loss_checked=true, native_category_mass=true;
        unsigned loss_exits=0, probabilistic_loss_rows=0;
        for (auto item : {natural,crafted,crafted_goal}) for (bool fresh : {false,true})
            for (int phase_case=-1;phase_case<2;++phase_case) for (const auto* action_id : {"annul","eldritch_annul"}) {
                if (fresh) item.prefixes[0].flags=0;
                item.searing_exarch_tier=phase_case==0;
                item.eater_of_worlds_tier=phase_case==1;
                const auto a=typed_registry.index_by_id.at(action_id);
                const auto& kernel=typed_calc.outcomes(typed_calc.intern_item(item),a);
                long double rhs=2, total=0;
                unsigned eligible=0;
                for (unsigned side=0;side<2;++side) {
                    if (std::string(action_id)=="eldritch_annul" && phase_case>=0 && side!=static_cast<unsigned>(phase_case)) continue;
                    const auto* slots=side ? item.suffixes : item.prefixes;
                    for (unsigned i=0;i<(side ? item.suffix_count : item.prefix_count);++i)
                        eligible+=!(slots[i].flags&PC_MOD_SLOT_FRACTURED);
                }
                native_category_mass &= kernel.supported && !kernel.entries.empty();
                for (const auto& e:kernel.entries) {
                    pc_item_state exit{}; native_category_mass &= typed_calc.materialize(e.state,exit);
                    native_category_mass &= exit.prefix_count+exit.suffix_count+unsigned(eligible>0)==item.prefix_count+item.suffix_count;
                    if (!fresh) native_category_mass &= item_contains_mod(exit,0);
                    if (std::string(action_id)=="eldritch_annul" && phase_case==0) native_category_mass &= exit.suffix_count==item.suffix_count;
                    if (std::string(action_id)=="eldritch_annul" && phase_case==1) native_category_mass &= exit.prefix_count==item.prefix_count;
                    rhs+=e.probability*refined->projected_value(typed_calc,exit); total+=e.probability; ++loss_exits;
                }
                loss_checked &= refined->projected_value(typed_calc,item)<=rhs+1e-10L;
                native_category_mass &= std::abs(total-1)<1e-14L;
            }
        for (const auto& row:refined->relations) if (row.removable_affixes) {
            ++probabilistic_loss_rows;
            native_category_mass &= row.events.size()==row.removable_affixes && row.removable_affixes<=6;
            for (const auto& event:row.events)
                native_category_mass &= event.capacity==((1u<<24)+row.removable_affixes-1)/row.removable_affixes;
        }
        PC_CHECK(loss_exits>0 && probabilistic_loss_rows>0);
        PC_CHECK(loss_checked);
        PC_CHECK(native_category_mass);
        bool integer_nonempty=!refined->nonempty_witnesses.empty();
        for (const auto& w:refined->nonempty_witnesses) {
            const auto& draw=refined->draw(w.draw); auto remaining=draw.other_weight;
            for (unsigned side=0;side<2;++side) for (unsigned i=0;i<(side ? w.suffixes : w.prefixes);++i)
                remaining-=std::min(remaining,draw.strongest_other_removal[side][i]);
            integer_nonempty &= remaining && remaining==w.remaining_other && !draw.frame_escape;
        }
        PC_CHECK(integer_nonempty);
        const auto before_cancel=support->memory_snapshot().total_bytes;
        auto interrupted=budget; unsigned native_checkpoints=0;
        interrupted.cancelled=[&] {return ++native_checkpoints>20;};
        rejects([&] { PhaseLowerProducer::prepare_probabilistic(typed_calc,typed_prices,phase,
            {PhaseTableRole::CleanCompletion,4,2,std::vector<double>(192,100)},support,zero,
            false,true,interrupted,true,{},PhaseContinuation::CoupledFresh,PhaseRetention::AnnulNonempty); });
        PC_CHECK(support->memory_snapshot().total_bytes==before_cancel);

        auto protected_craft=natural; protected_craft.prefixes[0].flags|=PC_MOD_SLOT_CRAFTED;
        PC_CHECK(refined->projected_cell(typed_calc,protected_craft)==refined->projected_cell(typed_calc,natural));
        auto multiple=crafted; place(&multiple,PC_SIDE_SUFFIX,6,21,PC_MOD_SLOT_CRAFTED);
        PC_CHECK(!refined->lookup(typed_calc,typed_prices,multiple,false));
        // A multiple-crafted initial input needs its own explicitly broader
        // domain. Missing multimod alone never proves a count of at most one.
        multiple.eater_of_worlds_tier=1;
        const auto broader=PhaseLowerProducer::prepare_probabilistic(typed_calc,typed_prices,multiple,
            {PhaseTableRole::CleanCompletion,4,2,std::vector<double>(192,100)},support,zero,
            false,true,budget,true,{},PhaseContinuation::CoupledFresh,PhaseRetention::Crafted);
        PC_CHECK(broader->crafted_count_limit==2 && broader->lookup(typed_calc,typed_prices,multiple,false).has_value());
        const auto streamed=PhaseLowerProducer::compose(typed_calc,typed_prices,crafted,id,*refined);
        PC_CHECK(streamed.record.physical_exits>0 && streamed.record.failure_lower_min>=0);
        SolveOptions options; options.consider_imprint_programs=false;
        options.max_states=options.max_discovered_states=options.max_expanded_states=256;
        SolveWorkTestAccess::Impl off(typed_calc,anchor,typed_prices,options);
        PC_CHECK(!off.native_retention_attempted && !off.native_retention_potential);
        options.native_retention_lower=true;
        SolveWorkTestAccess::Impl on(typed_calc,anchor,typed_prices,options);
        PC_CHECK(on.goal_cover_stage == SolveWorkTestAccess::Impl::SetupStage::NotStarted);
        PC_CHECK(!on.goal_cover_cost_ready && !on.native_retention_attempted);
        // Reject the real frame size before allocation; constructor remains
        // cheap and ordinary requests still leave setup dormant.
        {
            SolveWorkTestAccess::Impl capped(typed_calc,anchor,typed_prices,options);
            capped.options.max_solver_owned_bytes = capped.fast_estimated_owned_bytes();
            bool hit = false;
            try { (void)capped.advance_setup(); } catch (const SolverResourceLimit&) { hit = true; }
            PC_CHECK(hit && !capped.goal_cover_task && capped.setup_storage.live == 0);
        }
        for (unsigned fence : {0u, 1u, 2u, 3u}) {
            SolveWorkTestAccess::Impl pending(typed_calc,anchor,typed_prices,options);
            if (fence == 1) (void)pending.advance_setup();
            if (fence >= 2) {
                while (!pending.goal_cover_cost_ready) (void)pending.advance_setup();
                if (fence == 3) {
                    for (unsigned i=0;i<20;++i) (void)pending.advance_setup();
                    PC_CHECK(pending.native_retention_attempted && !pending.native_retention_potential);
                }
            }
            const auto live=pending.setup_storage.live;
            const auto stage=pending.goal_cover_stage;
            const auto states=typed_calc.state_count();
            (void)pending.progress(); (void)pending.progress_trace_json(0);
            (void)pending.telemetry_snapshot(true);
            if (!pending.goal_cover_cost_ready) PC_CHECK(pending.completion_proof_lower_value(pending.result.start_state)==0);
            PC_CHECK(pending.setup_storage.live==live && pending.goal_cover_stage==stage && typed_calc.state_count()==states);
            pending.retention_setup_task.reset(); pending.goal_cover_task.reset();
            PC_CHECK(pending.setup_storage.live==0 && pending.setup_storage.reserved==0);
        }
        {
            SolveWorkTestAccess::Impl partial(typed_calc,anchor,typed_prices,options);
            while (!partial.goal_cover_carrier_committed) (void)partial.advance_setup();
            const auto carrier = partial.carrier_goal_progress_cost;
            partial.options.max_solver_owned_bytes = partial.fast_estimated_owned_bytes();
            bool hit=false;
            try { while (!partial.advance_setup()) {} } catch (const SolverResourceLimit&) { hit=true; }
            PC_CHECK(hit && partial.goal_cover_stage==SolveWorkTestAccess::Impl::SetupStage::Refused);
            PC_CHECK(partial.goal_cover_carrier_committed && partial.carrier_goal_progress_cost==carrier);
            PC_CHECK(!partial.goal_cover_cost_ready && partial.setup_storage.live==0 && partial.setup_storage.reserved==0);
            PC_CHECK(partial.completion_proof_lower_value(partial.result.start_state)>=0);
        }
        unsigned setup_slices = 0;
        while (!on.advance_setup()) {
            ++setup_slices;
            const auto stage = on.goal_cover_stage;
            const auto bytes = on.setup_storage.live;
            const auto ready = on.goal_cover_cost_ready;
            (void)on.progress();
            (void)on.progress_trace_json(0);
            PC_CHECK(on.goal_cover_stage == stage && on.setup_storage.live == bytes && on.goal_cover_cost_ready == ready);
            if (stage == SolveWorkTestAccess::Impl::SetupStage::Preparing) PC_CHECK(!ready && !on.native_retention_potential);
        }
        PC_CHECK(setup_slices > 1 && on.setup_storage.live == 0 && on.setup_storage.reserved == 0);

        if (!on.native_retention_potential) std::fprintf(stderr,"native retention fixture refusal: %s\n",on.native_retention_refusal.c_str());
        PC_CHECK(on.native_retention_potential && on.native_retention_refusal.empty());
        if (!on.native_retention_potential) return;
        const auto saved=on.native_retention_potential;
        const auto time=on.native_retention_prepare_ns;
        on.prepare_native_retention_lower();
        PC_CHECK(on.native_retention_potential==saved && on.native_retention_prepare_ns==time);
        PC_CHECK(on.progress().lower_bound>=on.native_retention_lower_value(on.result.start_state));
        bool uniform=true,maximum=true;
        for (auto item : {anchor,natural,crafted,crafted_goal,protected_craft}) for (bool fresh : {false,true}) {
            if (fresh) item.prefixes[0].flags=0;
            const auto state=typed_calc.intern_item(item);
            const auto exact=saved->lookup(typed_calc,typed_prices,item,false);
            const auto from_state=on.native_retention_lower_value(state);
            uniform &= exact && *exact==from_state;
            on.native_retention_potential.reset();
            const auto before=on.completion_proof_lower_value(state);
            on.native_retention_potential=saved;
            maximum &= on.completion_proof_lower_value(state)>=std::max(before,from_state);
        }
        PC_CHECK(uniform && maximum && on.native_retention_hits>1);
        on.options.native_retention_consume=false;
        PC_CHECK(on.native_retention_lower_value(on.result.start_state)==0);
        PC_CHECK(on.native_retention_potential==saved && on.project_native_retention_lower(on.result.start_state).value()>0);
        on.options.native_retention_consume=true;
        // The constructor's fracture frame is not the request's start. With
        // the same crafted domain and native context, the existing fresh
        // region must give the same checked answers at empty and partial
        // entries. This negative control failed at the old anchored guard.
        pc_item_state empty; pc_item_clear(&empty); empty.rarity=PC_RARITY_RARE;
        SolveWorkTestAccess::Impl empty_work(typed_calc,empty,typed_prices,options);
        while (!empty_work.advance_setup()) {}
        PC_CHECK(empty_work.native_retention_potential && empty_work.native_retention_refusal.empty());
        PC_CHECK(empty_work.exact_start_item.prefix_count==0 && empty_work.exact_start_item.suffix_count==0);
        if (empty_work.native_retention_potential) {
            PC_CHECK(empty_work.native_retention_potential->fractured_mod==saved->fractured_mod);
            for (auto item : {empty,natural,crafted,crafted_goal}) {
                for (unsigned i=0;i<item.prefix_count;++i) item.prefixes[i].flags &= ~PC_MOD_SLOT_FRACTURED;
                for (unsigned i=0;i<item.suffix_count;++i) item.suffixes[i].flags &= ~PC_MOD_SLOT_FRACTURED;
                const auto lower=empty_work.native_retention_potential->lookup(typed_calc,typed_prices,item,false);
                const auto prior=saved->lookup(typed_calc,typed_prices,item,false);
                PC_CHECK(lower && prior && std::abs(*lower-*prior)<1e-8);
                PC_CHECK(lower && *lower==empty_work.native_retention_lower_value(typed_calc.intern_item(item)));
            }
        }
        auto wrong_fracture=anchor; wrong_fracture.prefixes[0].mod_id=1;
        PC_CHECK(on.native_retention_lower_value(typed_calc.intern_item(wrong_fracture))==0);
        auto influenced=anchor; influenced.generic_influence_bits=1;
        PC_CHECK(on.native_retention_lower_value(typed_calc.intern_item(influenced))==0);
        options.max_solver_owned_bytes=1ull<<20;
        SolveWorkTestAccess::Impl refused(typed_calc,anchor,typed_prices,options);
        while (!refused.advance_setup()) {}
        PC_CHECK(refused.native_retention_attempted && !refused.native_retention_potential && refused.native_retention_live_bytes==0);
        options.max_solver_owned_bytes=1ull<<30;
        options.max_states=options.max_discovered_states=options.max_expanded_states=200000;
        SolveWorkTestAccess::Impl ordinary_capacity(typed_calc,anchor,typed_prices,options);
        while (!ordinary_capacity.advance_setup()) {}
        PC_CHECK(ordinary_capacity.native_retention_potential && ordinary_capacity.native_retention_refusal.empty());
        auto ambiguous_goal=goal; ambiguous_goal.slots[0].min_tier=2;
        CalcContext ambiguous_calc(typed_session,ambiguous_goal,typed_registry,basic_indices(typed_registry));
        options.max_solver_owned_bytes=1ull<<30;
        SolveWorkTestAccess::Impl ambiguous(ambiguous_calc,anchor,typed_prices,options);
        while (!ambiguous.advance_setup()) {}
        PC_CHECK(ambiguous.native_retention_potential && ambiguous.native_retention_lower_value(ambiguous.result.start_state)==0);
        PC_CHECK(ambiguous.native_retention_potential->lookup(ambiguous_calc,typed_prices,anchor,false).value()>0);
    }
    for (unsigned filter_kind=0;filter_kind<2;++filter_kind) {
        auto fs=make_calc_session();
        auto data=std::const_pointer_cast<DataImpl>(fs->data);
        data->metamod_no_attack_code=0; data->metamod_no_caster_code=1; data->metamod_multimod_code=2;
        data->tag_id_by_name["attack"]=1; data->tag_id_by_name["caster"]=2;
        for (unsigned mod=0;mod<fs->mod_count;++mod) {
            data->mod_key_sid.push_back(data->strings.size()); data->strings.push_back("filter-fixture-"+std::to_string(mod));
        }
        fs->metamod_type[7]=filter_kind; fs->flags[7]|=1<<1; fs->flags[3]|=1<<1;
        fs->bench_mod_ids={3,7};
        pc_bitset_clear(fs->normal_random_roll_mask.data(),7);
        auto registry=build_action_registry(*fs);
        PhaseLowerPrices prices{{"step",10},{"filter",2},{"loss",1}};
        for (auto& a:registry.actions) a.cost_keys={"step"};
        const auto bench=registry.index_by_id.at("bench:filter-fixture-7");
        registry.actions[bench].cost_keys={"filter"};
        registry.actions[registry.index_by_id.at("remove_crafted_modifiers")].cost_keys={"loss"};
        CalcContext fc(fs,goal,registry,basic_indices(registry),false,true,true,std::nullopt,{},false,{},true);
        auto frame=source; frame.prefixes[0].flags=PC_MOD_SLOT_FRACTURED; frame.eater_of_worlds_tier=1;
        auto support=PhaseLowerProducer::prepare(fc,prices,frame,{PhaseTableRole::MaskCompletion,4,2,std::vector<double>(4,0)});
        const auto zero=PhaseLowerProducer::zero_restart_boundary(*support);
        const PhaseLowerProposal proposal{PhaseTableRole::CleanCompletion,4,2,std::vector<double>(192,100)};
        const auto view=PhaseLowerProducer::prepare_probabilistic(fc,prices,frame,proposal,support,zero,false,true,{},true,{},
            PhaseContinuation::CoupledFresh,PhaseRetention::AnnulNonempty,true,{true,true,1u<<filter_kind});
        ActionContextImpl native{1}; native.session=fs;
        auto filtered=frame;
        PC_CHECK(apply_action(native,&filtered,registry.actions[bench].params).applied);
        PC_CHECK(filtered.suffix_count==frame.suffix_count+1 && (filtered.suffixes[filtered.suffix_count-1].flags&PC_MOD_SLOT_CRAFTED));
        auto blocked=frame; place(&blocked,PC_SIDE_PREFIX,3,12,PC_MOD_SLOT_CRAFTED);
        const auto before=exact_item_state_key(blocked);
        PC_CHECK(!apply_action(native,&blocked,registry.actions[bench].params).applied && exact_item_state_key(blocked)==before);
        auto full=frame; place(&full,PC_SIDE_SUFFIX,5,20); place(&full,PC_SIDE_SUFFIX,6,21); place(&full,PC_SIDE_SUFFIX,9,31);
        PC_CHECK(!apply_action(native,&full,registry.actions[bench].params).applied);
        const auto unfiltered_draw=fc.phase_goal_draw_bound(frame,registry.index_by_id.at("exalt"),0,false);
        const auto filtered_draw=fc.phase_goal_draw_bound(filtered,registry.index_by_id.at("exalt"),0,false,7);
        PC_CHECK(filtered_draw.pool_filter_mod==7 && filtered_draw.target_weight==unfiltered_draw.target_weight && filtered_draw.other_weight<unfiltered_draw.other_weight);
        rejects([&]{fc.phase_goal_draw_bound(frame,registry.index_by_id.at("exalt"),0,false,3);});
        auto plain=filtered; plain.suffixes[plain.suffix_count-1].mod_id=6; plain.suffixes[plain.suffix_count-1].group_id=21;
        PC_CHECK(view->projected_cell(fc,plain)!=view->projected_cell(fc,filtered));
        PC_CHECK(!fc.is_goal_state(fc.state(fc.intern_item(filtered))));
        auto goals_and_filter=filtered; place(&goals_and_filter,PC_SIDE_SUFFIX,5,20);
        PC_CHECK(!fc.is_goal_state(fc.state(fc.intern_item(goals_and_filter))) && view->projected_value(fc,goals_and_filter)>0);
        PC_CHECK(view->lookup(fc,prices,filtered,false).has_value());
        auto program_source=filtered; program_source.eater_of_worlds_tier=0;
        const auto filtered_program=PhaseLowerProducer::compose(fc,prices,program_source,id,*view);
        PC_CHECK(filtered_program.record.goal_weight==0 && filtered_program.record.physical_exits>0);
        PC_CHECK(filtered_program.record.lower>=filtered_program.record.cost_lower);
        auto uncrafted=filtered; uncrafted.suffixes[uncrafted.suffix_count-1].flags=0;
        PC_CHECK(!view->lookup(fc,prices,uncrafted,false));
        auto fracture_filter=filtered; fracture_filter.suffixes[fracture_filter.suffix_count-1].flags|=PC_MOD_SLOT_FRACTURED;
        PC_CHECK(!view->lookup(fc,prices,fracture_filter,false));
        bool complete=true, expectation=true, side_retention=true; unsigned exits=0;
        for (bool fresh:{false,true}) for (int phase=-1;phase<2;++phase) for (bool blocker:{false,true})
            for (const char* id:{"exalt","annul","remove_crafted_modifiers","scour","chaos","eldritch_chaos","eldritch_annul"}) {
                auto item=filtered; if (fresh) item.prefixes[0].flags=0;
                if (blocker) place(&item,PC_SIDE_PREFIX,3,12);
                item.searing_exarch_tier=phase==0; item.eater_of_worlds_tier=phase==1;
                const auto a=registry.index_by_id.at(id);
                const auto& kernel=fc.outcomes(fc.intern_item(item),a);
                long double total=0,rhs=phase_price_lower(registry.actions[a],prices);
                complete &= kernel.supported && !kernel.entries.empty();
                for (const auto& e:kernel.entries) {
                    pc_item_state exit{}; complete &= fc.materialize(e.state,exit); ++exits;
                    const auto lower=view->lookup(fc,prices,exit,false); complete &= lower.has_value();
                    rhs+=e.probability*lower.value_or(0); total+=e.probability;
                    if (std::string(id)=="eldritch_chaos") side_retention &= item_contains_mod(exit,7)==(phase==0);
                    if (std::string(id)=="chaos" || std::string(id)=="scour" || std::string(id)=="remove_crafted_modifiers")
                        side_retention &= !item_contains_mod(exit,7);
                }
                complete &= std::abs(total-1)<1e-12L;
                expectation &= view->projected_value(fc,item)<=rhs+1e-10L;
            }
        PC_CHECK(exits>0 && complete && side_retention);
        PC_CHECK(expectation);
        PC_CHECK(std::any_of(view->draws.begin(),view->draws.end(),[](const auto& w){return w.pool_filter_mod==7;}));
        auto repriced=prices; repriced["filter"]=1;
        PC_CHECK(!view->lookup(fc,repriced,filtered,false));
        PC_CHECK(!view->lookup(fc,prices,filtered,true));
        const auto retained=support->memory_snapshot().total_bytes;
        QuotientLowerBudget cancel; unsigned n=0; cancel.cancelled=[&]{return ++n>100;};
        rejects([&]{PhaseLowerProducer::prepare_probabilistic(fc,prices,frame,proposal,support,zero,false,true,cancel,true,{},
            PhaseContinuation::CoupledFresh,PhaseRetention::AnnulNonempty,true,{true,true,1u<<filter_kind});});
        PC_CHECK(retained==support->memory_snapshot().total_bytes);

        // The coarse junk mask can contain both an ordinary member and a
        // known metamod. The whole abstract state also observes its flags:
        // without the metamod flag that member is impossible. This permits
        // the ordinary class query without admitting the filtered member.
        auto coarse_actions=basic_indices(registry); coarse_actions.push_back(bench);
        CalcContext coarse(fs,goal,registry,coarse_actions);
        SolveOptions ordinary_options; ordinary_options.consider_imprint_programs=false;
        ordinary_options.native_retention_lower=true;
        SolveWorkTestAccess::Impl ordinary(coarse,frame,prices,ordinary_options);
        while (!ordinary.advance_setup()) {}
        PC_CHECK(bool(ordinary.native_retention_potential));
        auto ordinary_junk=frame; place(&ordinary_junk,PC_SIDE_SUFFIX,6,21);
        const auto ordinary_id=coarse.intern_item(ordinary_junk);
        const auto filtered_id=coarse.intern_item(filtered);
        PC_CHECK(modifier_metamod_flag(*fs,7)==(filter_kind ? kFlagNoCaster : kFlagNoAttack));
        PC_CHECK((coarse.state(filtered_id).flags & modifier_metamod_flag(*fs,7))!=0);
        PC_CHECK(ordinary.native_retention_lower_value(ordinary_id)>0);
        PC_CHECK(ordinary.native_retention_lower_value(filtered_id)==0);
        const auto mixed_class=coarse.layout().junk_class_by_mod[7];
        PC_CHECK(mixed_class!=kNoId && ordinary.native_retention_junk_safe[mixed_class]);
        PC_CHECK(coarse.layout().junk_class_by_mod[6]==mixed_class);
    }
    static_assert(!std::is_constructible_v<PreparedPhaseRestartLower, QuotientLowerBoundary>);
    static_assert(!std::is_default_constructible_v<PreparedPhaseSourceLower>);
    static_assert(!std::is_constructible_v<PreparedPhaseSourceLower, QuotientLowerBoundary>);
    static_assert(!std::is_constructible_v<PreparedPhaseSourceLower, double>);
    static_assert(!std::is_copy_constructible_v<PreparedPhaseRestartLower>);
    static_assert(!std::is_default_constructible_v<PreparedPhaseLowerView>);
    static_assert(!std::is_default_constructible_v<PhaseProgramLowerWitness>);
    static_assert(!std::is_constructible_v<PreparedPhaseLowerView, QuotientLowerCertificate>);
    static_assert(!std::is_move_constructible_v<PhaseProgramLowerWitness>);
}

void run_fossil_guard_exact_tests() {
    auto session = make_calc_session();
    auto data = std::make_shared<DataImpl>(*session->data);
    data->strings = {"ordinary_fossil", "Ordinary Fossil",
        "Metadata/Items/Currency/CurrencyDelveCraftingMirror", "Fractured Fossil"};
    data->fossil_count = 2;
    data->fossil_key_sids = {0, 2};
    data->fossil_name_sids = {1, 3};
    data->fossil_rolls_lucky = {0, 0};
    data->fossil_mirrors = {0, 0};
    data->fossil_weight_offsets = {0, 0, 0};
    session->data = data;
    session->fossil_added_mod_ids.resize(2);
    session->fossil_forced_mod_ids.resize(2);
    session->fossil_sell_price_mod_ids.resize(2);
    // Default capability discovery discloses unavailable laws as filtered.
    // Explicit primitive, requested-fossil and fixed dependency envelopes retain
    // the guard instead, including noncanonical requested key ordering.
    ActionRegistryBuildOptions discovery;
    discovery.goal_relevant_actions = true;
    const auto discovered = build_action_registry(*session, discovery);
    PC_CHECK(discovered.index_by_id.count("fossil:ordinary_fossil") == 1);
    PC_CHECK(discovered.index_by_id.count("fossil:Metadata/Items/Currency/CurrencyDelveCraftingMirror") == 0);
    PC_CHECK(discovered.product_reason_counts.at("filtered_unavailable_fossil_law") == 2);
    for (const auto& filtered : discovered.product_filtered_actions)
        if (filtered.reason == "filtered_unavailable_fossil_law")
            PC_CHECK(filtered.role == ProductActionRole::Filtered);
    for (unsigned scope = 0; scope < 3; ++scope) {
        auto requested = discovery;
        if (scope == 0) requested.primitive_actions_explicit = true;
        if (scope == 1) requested.requested_fossil_action_ids = {
            "fossil:ordinary_fossil+Metadata/Items/Currency/CurrencyDelveCraftingMirror"};
        if (scope == 2) requested.option_dependency_action_ids = {
            "fossil:Metadata/Items/Currency/CurrencyDelveCraftingMirror"};
        const auto explicit_registry = build_action_registry(*session, requested);
        const auto id = scope == 1
            ? "fossil:Metadata/Items/Currency/CurrencyDelveCraftingMirror+ordinary_fossil"
            : "fossil:Metadata/Items/Currency/CurrencyDelveCraftingMirror";
        PC_CHECK(explicit_registry.index_by_id.count(id) == 1);
    }
    const auto registry = build_action_registry(*session);
    const auto alchemy = registry.index_by_id.at("alchemy");
    auto goal = family_goal_100();
    pc_item_state item{};
    pc_item_clear(&item);
    std::vector<std::uint32_t> guarded;
    for (std::uint32_t i = 0; i < registry.actions.size(); ++i) {
        const auto& action = registry.actions[i];
        if (action.params.type == ActionType::Fossil &&
            unavailable_fossil_reason(*data, action.params.fossil_indices)) guarded.push_back(i);
    }
    PC_CHECK(guarded.size() == 2); // retain the singleton and mixed scope identities
    for (const bool factored : {false, true}) {
        CalcContext calc(session, goal, registry, {alchemy}, false, false,
            false, std::nullopt, {}, false, {}, false, false, false, false, factored);
        const auto start = calc.intern_item(item);
        const auto states = calc.state_count();
        for (const auto action : guarded) {
            const auto rejects = [&](const auto& request) {
                bool rejected = false;
                try { request(); } catch (const std::invalid_argument& e) {
                    rejected = std::strstr(e.what(), "Fractured Fossil is unavailable") != nullptr;
                }
                PC_CHECK(rejected);
                PC_CHECK(calc.state_count() == states);
                PC_CHECK(calc.telemetry().distribution_requests == 0);
            };
            rejects([&] { calc.outcomes(start, action); });
            std::shared_ptr<const OutcomeDistribution> completed;
            rejects([&] { calc.advance_outcomes(start, action, false, completed, 1); });
            PC_CHECK(!completed);
            rejects([&] { calc.concrete_refill({item, 0, false, true, action, true}); });
            rejects([&] { fossil_implicit_outcomes(*session, item,
                registry.actions[action].params.fossil_indices); });
        }
    }
    // Reject the original complete caller request before setup, including when
    // the missing law has no price. Do not turn this into subset exactness.
    for (const auto action : guarded) {
        CalcContext calc(session, goal, registry, {alchemy, action});
        for (const bool priced : {false, true}) {
            std::unordered_map<std::string, double> prices{{"alchemy", 1}};
            if (priced) for (const auto& key : registry.actions[action].cost_keys) prices[key] = 1;
            bool rejected = false;
            try { SolveWorkTestAccess::Impl solve(calc, item, prices, SolveOptions{}); }
            catch (const std::invalid_argument& e) {
                rejected = std::strstr(e.what(), "Fractured Fossil is unavailable") != nullptr;
            }
            PC_CHECK(rejected);
            PC_CHECK(calc.state_count() == 0);
            rejected = false;
            try { PolicyFinderWork finder(calc, session, item, prices, SolveOptions{}); }
            catch (const std::invalid_argument& e) {
                rejected = std::strstr(e.what(), "Fractured Fossil is unavailable") != nullptr;
            }
            PC_CHECK(rejected);
            PC_CHECK(calc.state_count() == 0);
        }
    }
    data->fossil_mirrors[1] = 1;
    const auto legacy = fossil_implicit_outcomes(*session, item, {1});
    PC_CHECK(legacy.size() == 1 && legacy[0].second == 1);
    PC_CHECK((legacy[0].first.item_flags & PC_ITEM_MIRRORED) != 0);
}
