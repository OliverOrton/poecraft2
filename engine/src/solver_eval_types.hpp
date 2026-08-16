#pragma once

#include "solver_calc_types.hpp"

namespace poecraft {
namespace solver {

// --- exact compiled-strategy evaluator ---------------------------------------

struct StrategyEvalOptions {
    double epsilon = 1e-12;
    std::uint32_t max_sweeps = 100000;
    std::uint32_t max_states = 100000;
    /* `max_pairs` bounds the refined executable quotient. Raw operation/state
     * discovery remains bounded by max_states, max_transitions, and the owned
     * byte cap so semantically equivalent witnesses may merge before this
     * product limit is applied. */
    std::uint32_t max_pairs = 1000000;
    std::uint32_t max_transitions = 10000000;
    std::uint32_t top_classes_per_node = 16;
    std::shared_ptr<const EconomyImpl> economy;
    std::string review_projection_json;
    bool include_success_normalized = false;
    std::uint64_t max_owned_bytes = 536870912;
    std::uint64_t max_output_json_bytes = 67108864;
    /* Internal publication budget. Public standalone evaluation retains its
     * historical effectively-unbounded default. */
    std::uint64_t max_reforge_work =
        std::numeric_limits<std::uint64_t>::max();
    /* Internal A/B control. The default independently rebuilds exact
     * destructive-reforge rows with the calculator's proved exchangeable
     * junk-family symmetry reduction whenever the evaluator carrier is
     * coarse. Identity-observing carriers always retain the raw family path. */
    bool use_exact_exchangeable_family_compression = true;
};

enum class StrategyEvalPhase {
    Discovery,
    Solving,
    Fallback,
    Finalization,
    Done,
};

struct StrategyEvalProgress {
    StrategyEvalPhase phase = StrategyEvalPhase::Discovery;
    bool done = false;
    std::uint64_t discovered_pairs = 0;
    std::uint64_t pending_pairs = 0;
    std::uint64_t solved_sccs = 0;
    std::uint64_t total_sccs = 0;
    std::uint64_t fallback_sweeps = 0;
    double residual = 0.0;
};

struct StrategyEvalClass {
    double share = 0.0;
    AbstractState state;
};

struct StrategyEvalNode {
    std::string id;
    double expected_visits = 0.0;
    std::vector<StrategyEvalClass> classes;
    double classes_truncated_share = 0.0;
};

struct StrategyEvalEdge {
    std::string id;
    double expected_traversals = 0.0;
};

struct StrategyEvalTerminalNode {
    std::string node_id;
    int terminal_kind = PC_TERMINAL_FAILURE;
    double probability = 0.0;
};

struct StrategyEvalNodeMass {
    std::string node_id;
    double mass = 0.0;
};

struct StrategyEvalFailure {
    std::string node_id;
    std::string reason;
    double probability = 0.0;
};

struct StrategyEvalActionNode {
    std::string node_id;
    double expected_visits = 0.0;
    double expected_applied = 0.0;
};

struct StrategyEvalActionRegion {
    std::uint32_t goal_progress = 0;
    std::uint8_t rarity = PC_RARITY_NORMAL;
    std::uint32_t blocker_count = 0;
    std::uint32_t crafted_count = 0;
    std::uint32_t fractured_goal_mask = 0;
    std::uint32_t fractured_count = 0;
    std::uint32_t reachable_states = 0;
    double expected_visits = 0.0;
    double expected_applied = 0.0;
};

struct StrategyEvalActionTotal {
    std::string id;
    std::string display_name;
    std::vector<std::string> price_keys;
    double expected_visits = 0.0;
    double expected_applied = 0.0;
    std::vector<std::string> classifications;
    std::vector<StrategyEvalActionNode> nodes;
    std::uint32_t reachable_states = 0;
    std::vector<StrategyEvalActionRegion> regions;
};

struct StrategyEvalMaterialTotal {
    std::string price_key;
    double expected_quantity = 0.0;
    bool priced = false;
    double unit_price = 0.0;
    double cost_contribution = 0.0;
};

struct StrategyEvalReviewSection {
    std::string id;
    std::string label;
    std::string role;
    std::vector<std::string> raw_node_ids;
    std::vector<std::string> raw_edge_ids;
    double expected_actions = 0.0;
    double expected_edge_traversals = 0.0;
    double known_expected_cost = 0.0;
    bool cost_complete = false;
    double total_expected_cost = 0.0;
    std::vector<StrategyEvalActionTotal> actions;
    std::vector<StrategyEvalMaterialTotal> materials;
    std::map<std::string, double> techniques;
};

/* Retained exact state-operation occupancy. `state` indexes occupancy_states,
 * `node` preserves the typed compiled-operation authority, and `action`
 * preserves the ordinary ActionRegistry index when one exists (kNoId for a
 * companion-state operation). Reward is the immediate priced cost of one
 * applied operation; the expected contribution is expected_applied *
 * immediate_reward. These records remain internal until B4 adds the
 * action-utility report surface. */
struct StrategyEvalOccupancyEntry {
    std::uint32_t state = kNoId;
    std::uint32_t node = kNoId;
    std::uint32_t action = kNoId;
    double expected_visits = 0.0;
    double expected_applied = 0.0;
    double immediate_reward = 0.0;
    bool reward_complete = false;
};

struct StrategyEvalResult {
    bool converged = false;
    std::uint32_t sweeps = 0;
    double residual_mass = 0.0;
    double success_probability = 0.0;
    double failure_probability = 0.0;
    double stop_probability = 0.0;
    double action_not_applied_probability = 0.0;
    double no_matching_edge_probability = 0.0;
    double unresolved_probability = 0.0;
    double expected_actions = 0.0;
    std::map<std::string, double> expected_consumption;
    std::vector<StrategyEvalActionTotal> action_totals;
    std::vector<StrategyEvalMaterialTotal> material_totals;
    std::map<std::string, double> technique_totals;
    std::vector<StrategyEvalReviewSection> review_sections;
    bool review_sections_enabled = false;
    bool pricing_enabled = false;
    std::string economy_id;
    bool cost_complete = false;
    double known_expected_cost = 0.0;
    double total_expected_cost = 0.0;
    bool success_normalized_enabled = false;
    double action_descriptor_visits_difference = 0.0;
    double action_descriptor_applied_difference = 0.0;
    double node_operation_visits_difference = 0.0;
    std::map<std::string, double> material_quantity_differences;
    double cost_dot_product_difference = 0.0;
    double section_actions_difference = 0.0;
    std::map<std::string, double> section_material_differences;
    std::vector<GoalSlot> targets;
    std::vector<StrategyEvalTerminalNode> terminal_nodes;
    std::vector<StrategyEvalNodeMass> unresolved_by_node;
    std::vector<StrategyEvalFailure> failures_by_node;
    std::vector<StrategyEvalNode> nodes;
    std::vector<StrategyEvalEdge> edges;
    std::vector<AbstractState> occupancy_states;
    std::vector<StrategyEvalOccupancyEntry> occupancy;
    double occupancy_expected_reward = 0.0;
    double occupancy_reward_difference = 0.0;
    bool occupancy_reward_complete = false;
    /* White-box diagnostic for the per-sweep conservation property. */
    double max_mass_conservation_error = 0.0;
    std::uint64_t owned_bytes_estimate = 0;
    std::uint64_t peak_owned_bytes_estimate = 0;
    std::uint64_t max_owned_bytes = 0;
    std::uint64_t max_output_json_bytes = 0;
    std::uint64_t reforge_work = 0;
    std::uint64_t reforge_logical_work_v1 = 0;
    std::uint64_t reforge_evaluator_work_v1 = 0;
    std::uint64_t reforge_evaluator_work_v2 = 0;
    std::uint64_t reforge_evaluator_work_v3 = 0;
    /* Canonical state/probability-bit hash for the first goal-gated reforge
     * kernel. Kept in the native result so focused exact-evaluator tests can
     * prove compressed and raw family enumeration are distribution-identical
     * without making this diagnostic part of the JSON contract. */
    std::uint64_t reforge_gated_first_kernel_bits_hash = 0;
    ReforgeEffortBreakdown reforge_effort;
    std::vector<ReforgeRowTelemetry> reforge_row_samples;
    std::uint64_t reforge_row_samples_omitted = 0;
    std::uint32_t raw_pairs_discovered = 0;
    std::uint32_t refined_pairs = 0;
    std::uint32_t pair_refinement_rounds = 0;
    std::uint64_t pair_lumpability_checks = 0;
};

class StrategyEvalUnsupported : public std::runtime_error {
  public:
    explicit StrategyEvalUnsupported(const std::string& message)
        : std::runtime_error(message) {}
};

class StrategyEvalWork {
  public:
    StrategyEvalWork(
        std::shared_ptr<const StrategyImpl> strategy,
        const StrategyEvalOptions& options = {});
    ~StrategyEvalWork();
    StrategyEvalWork(StrategyEvalWork&&) noexcept;
    StrategyEvalWork& operator=(StrategyEvalWork&&) noexcept;
    StrategyEvalWork(const StrategyEvalWork&) = delete;
    StrategyEvalWork& operator=(const StrategyEvalWork&) = delete;

    void step(std::uint32_t max_work_items);
    StrategyEvalProgress progress() const;
    const StrategyEvalResult& result() const;
    StrategyEvalResult take_result();
    /* Partial counters retained after a resource refusal. Diagnostic only;
     * it never converts incomplete work into an evaluable result. */
    const StrategyEvalResult& diagnostic_result();
    std::uint64_t live_owned_bytes() const;
    std::uint64_t peak_owned_bytes() const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    friend StrategyEvalResult evaluate_strategy_forward_reference_for_test(
        const StrategyImpl& strategy,
        const StrategyEvalOptions& options);
};

enum class ResolvedStrategyOperationKind : std::uint8_t {
    None = 0,
    Ordinary = 1,
    Restart = 2,
    Bestiary = 3,
};

/* Engine-owned semantic authority for one parsed strategy operation.
 * Ordinary/Restart indices address ActionRegistry::actions; Bestiary indices
 * address DataImpl::bestiary_actions. The parser has already resolved
 * Bestiary JSON to its descriptor index, so no action-name switch is needed. */
struct ResolvedStrategyOperation {
    ResolvedStrategyOperationKind kind =
        ResolvedStrategyOperationKind::None;
    std::uint32_t descriptor_index = kNoId;

    bool resolved() const {
        return kind != ResolvedStrategyOperationKind::None &&
               descriptor_index != kNoId;
    }
};

ResolvedStrategyOperation resolve_strategy_operation(
    const StrategyNode& node,
    const ActionRegistry& registry,
    const SessionImpl& session);

/* Resolve one compiled ordinary operation to its registry descriptor. kNoId
 * means no exact parameter match exists. Restart resolves to the synthetic
 * descriptor; companion-state operations deliberately do not alias an
 * ordinary ActionParameters default. */
std::uint32_t resolve_strategy_action(
    const StrategyNode& node,
    const ActionRegistry& registry);

/* Abstract-state counterpart of the simulator's compiled condition
 * predicate. The evaluation derivation guarantees all referenced targets are
 * present in layout. */
bool evaluate_abstract_condition(
    const CompiledCondition& condition,
    const SessionImpl& session,
    const AbstractLayout& layout,
    const AbstractState& state);

StrategyEvalResult evaluate_strategy(
    const StrategyImpl& strategy,
    const StrategyEvalOptions& options = {});

/* Test-only numerical oracle: discovers the same finite pair graph, then uses
 * high-precision whole-graph forward propagation instead of the production
 * SCC solver. */
StrategyEvalResult evaluate_strategy_forward_reference_for_test(
    const StrategyImpl& strategy,
    const StrategyEvalOptions& options = {});

std::string serialize_strategy_eval(const StrategyEvalResult& result);


} // namespace solver
} // namespace poecraft
