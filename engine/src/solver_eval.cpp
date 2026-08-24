#include "solver_eval_helpers.hpp"
#include "solver_action_family_contract.hpp"
#include "solver_cooperative_task.hpp"

#include <array>
#include <iomanip>
#include <numeric>
#include <sstream>

namespace poecraft {
namespace solver {

namespace {

bool bestiary_action_legal(
        const BestiaryActionDescriptor& action,
        const AbstractState& state,
        const std::uint32_t checkpoint_state) {
    const std::uint8_t rarity_bit =
        state.rarity < 8
            ? static_cast<std::uint8_t>(1u << state.rarity)
            : 0;
    if ((action.rarity_mask & rarity_bit) == 0) return false;
    if ((action.forbidden_item_flags & PC_ITEM_CORRUPTED) != 0 &&
        (state.flags & kFlagCorrupted) != 0) {
        return false;
    }
    if ((action.forbidden_item_flags & PC_ITEM_MIRRORED) != 0 &&
        (state.flags & kFlagMirrored) != 0) {
        return false;
    }
    const bool checkpoint_active = checkpoint_state != kNoId;
    if (action.checkpoint_requirement ==
            BestiaryCheckpointRequirement::Absent &&
        checkpoint_active) {
        return false;
    }
    if (action.checkpoint_requirement ==
            BestiaryCheckpointRequirement::Present &&
        !checkpoint_active) {
        return false;
    }
    /* Every evaluator checkpoint is created from the current live item.
     * Ordinary crafts keep item identity, while Restart clears the checkpoint
     * before changing it. Therefore an active checkpoint is necessarily bound
     * to this live item. */
    if (action.identity_requirement ==
            BestiaryIdentityRequirement::SameItem &&
        !checkpoint_active) {
        return false;
    }
    return true;
}

} // namespace

struct StrategyEvalWork::Impl {
    using Clock = std::chrono::steady_clock;

    struct ActiveTimer {
        std::uint64_t* counter = nullptr;
        Clock::time_point started = Clock::now();

        explicit ActiveTimer(std::uint64_t& value)
            : counter(&value) {}

        ~ActiveTimer() {
            if (counter == nullptr) return;
            const std::uint64_t elapsed =
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        Clock::now() - started)
                        .count());
            *counter = elapsed >
                    std::numeric_limits<std::uint64_t>::max() - *counter
                ? std::numeric_limits<std::uint64_t>::max()
                : *counter + elapsed;
        }

        ActiveTimer(const ActiveTimer&) = delete;
        ActiveTimer& operator=(const ActiveTimer&) = delete;
    };

    struct PolicyRouteResolution {
        std::uint32_t target_node = kNoId;
        std::uint32_t failure_node = kNoId;
        std::uint32_t trace = kNoId;
        bool resolved = false;
    };

    struct DeterministicRouteResolution {
        std::uint32_t target_node = kNoId;
        std::uint32_t failure_node = kNoId;
        std::uint32_t trace = kNoId;
    };

    struct DeterministicRouteFlow {
        double mass = 0.0;
        /* Encoded trace authority until finalization expands it, then the
         * exact skipped router node for class aggregation. */
        std::uint32_t authority = kNoId;
        std::uint32_t state = kNoId;
    };

    static_assert(
        sizeof(DeterministicRouteFlow) == 16,
        "deterministic route flow must remain a compact transient carrier");

    struct OperationRowActionCensus {
        std::uint64_t materialized_rows = 0;
        std::uint64_t shared_row_reuses = 0;
        std::uint64_t stable_shared_rows = 0;
        std::uint64_t state_local_rows = 0;
        std::uint64_t direct_repeat_rows = 0;
        std::uint64_t local_gated_route_rows = 0;
        std::uint64_t other_operation_rows = 0;
        std::uint64_t goal_progress_gated_rows = 0;
        std::uint64_t full_physical_rows = 0;
        std::uint64_t local_gated_route_proved_rows = 0;
        std::uint64_t local_gated_full_outcome_entries = 0;
        std::uint64_t local_gated_full_routed_transitions = 0;
        std::uint64_t local_gated_full_outcome_payload_bytes = 0;
        std::uint64_t local_gated_full_routed_payload_bytes = 0;
        std::uint64_t exact_outcome_entries = 0;
        std::uint64_t routed_transitions = 0;
        std::uint64_t absorptions = 0;
        std::uint64_t exact_outcome_payload_bytes = 0;
    };

    enum class ReplayRouteKind : std::uint32_t {
        Transition = 0,
        Terminal = 1,
        NoMatchingEdge = 2,
    };

    enum class LocalGatedRouteProof : std::uint8_t {
        NotCandidate = 0,
        Proved = 1,
        ShapeRejected = 2,
        ConditionRejected = 3,
        TargetRejected = 4,
        RootRejected = 5,
    };

    struct ReplayRouteResult {
        ReplayRouteKind kind = ReplayRouteKind::Transition;
        std::uint32_t node = kNoId;
        std::uint32_t edge = kNoId;
        std::uint32_t policy_route = kNoId;

        bool operator==(const ReplayRouteResult&) const = default;
    };

    static_assert(
        sizeof(ReplayRouteResult) == 16,
        "replay route results must remain compact");

    struct FallbackState {
        std::uint32_t component = kNoId;
        std::vector<std::uint32_t> members;
        std::vector<std::int32_t> local_index_by_pair;
        std::vector<solve_detail::PolicyRow> transpose_rows;
        std::vector<solve_detail::PolicyEdge> transpose_edges;
        std::vector<double> incoming;
        std::vector<double> previous_values;
        std::unique_ptr<solve_detail::SparsePolicyResume> resume;
        double input_mass = 0.0;
    };

    std::shared_ptr<const StrategyImpl> strategy;
    StrategyEvalOptions options;
    Clock::time_point construction_started = Clock::now();
    EvalModel model;
    std::vector<ReviewSectionSpec> review_sections;
    StrategyEvalResult output;
    StrategyEvalPhase phase = StrategyEvalPhase::Discovery;
    StrategyEvalSubphase subphase = StrategyEvalSubphase::ModelSetup;
    std::optional<solve_detail::CooperativeTask<bool>> component_build_task;
    std::optional<solve_detail::CooperativeTask<bool>> finalization_task;
    bool skip_pair_refinement_once = false;
    bool pair_refinement_identity_graph_intact = true;
    std::uint64_t pair_refinement_peak_before = 0;

    /* Collision-safe chained hash index for raw pair discovery. Pair keys
     * already live in `pairs`; the index retains only bucket heads and one
     * next link per pair instead of duplicating every four-word key in an
     * ordered-tree node. Full key equality remains authoritative. */
    std::vector<std::uint32_t> pair_bucket_heads;
    solve_detail::SegmentedVector<std::uint32_t> pair_next;
    solve_detail::SegmentedVector<EvalPair> pairs;
    std::map<std::vector<std::uint32_t>, std::uint32_t>
        unveil_offer_by_mods;
    std::vector<std::vector<std::uint32_t>> unveil_offer_sets;
    std::vector<EvalRow> rows;
    /* The behavioral quotient solves value/flow at the minimum observable
     * carrier. Retain the pre-quotient graph until finalization solely to
     * disaggregate that flow back onto exact evaluator states and terminal
     * states; no representative state is authoritative for reporting. */
    solve_detail::SegmentedVector<EvalPair> attribution_pairs;
    std::vector<EvalRow> attribution_rows;
    std::vector<std::uint32_t> attribution_class_by_pair;
    std::vector<solve_detail::WideFloat>
        attribution_exact_row_visits;
    std::uint32_t attribution_start_pair = kNoId;
    std::uint64_t attribution_row_payload_owned_bytes = 0;
    std::map<
        std::tuple<
            std::uint32_t, std::uint32_t,
            const OutcomeDistribution*>,
        std::uint32_t> row_by_distribution;
    std::uint64_t stored_transitions = 0;
    std::array<std::uint64_t, 4> raw_pairs_by_kind{};
    std::array<std::uint64_t, 4> expanded_pairs_by_kind{};
    std::uint64_t deterministic_pairs_expanded = 0;
    std::uint64_t shared_row_pairs_expanded = 0;
    std::uint64_t retained_transitions_with_edge = 0;
    std::uint64_t retained_transitions_with_policy_route = 0;
    std::uint64_t retained_policy_state_matches_target = 0;
    std::uint64_t retained_policy_state_differs_from_target = 0;
    std::uint64_t retained_absorptions = 0;
    std::uint64_t deterministic_router_nodes_skipped = 0;
    std::uint64_t deterministic_router_edges_skipped = 0;
    std::uint64_t deterministic_router_cycles_retained = 0;
    std::uint64_t row_payload_owned_bytes = 0;
    std::uint64_t component_payload_owned_bytes = 0;
    std::uint64_t review_payload_owned_bytes = 0;
    std::uint64_t edge_index_owned_bytes = 0;
    std::uint64_t terminal_incoming_owned_bytes = 0;
    std::uint64_t compressed_policy_incoming_owned_bytes = 0;
    std::uint64_t observation_requirement_owned_bytes = 0;
    const char* memory_probe_stage = "steady_state";
    std::uint64_t memory_probe_units = 0;
    std::uint64_t memory_probe_unit_bytes = 0;
    std::size_t discover_index = 0;
    std::uint32_t start_pair = kNoId;

    std::vector<std::vector<std::uint32_t>> components;
    std::vector<std::uint32_t> component_by_pair;
    std::size_t component_index = 0;
    std::vector<double> external_incoming;
    std::vector<double> pair_visits;
    std::vector<double> unresolved_pair;
    /* Deterministic pass-through contraction (contract_pass_through).
     * A contracted pair keeps its single outgoing transition in
     * chain_next/chain_edge; chain_inflow accumulates the mass that
     * entered it so its visits and edge traversal are settled during
     * finalization. */
    std::vector<std::uint8_t> pair_contracted;
    std::vector<std::uint32_t> chain_next;
    std::vector<std::uint32_t> chain_edge;
    std::vector<std::uint32_t> chain_policy_route;
    std::vector<std::uint32_t> chain_policy_state;
    std::vector<std::uint32_t> chain_terminal;
    std::vector<double> chain_inflow;
    std::unique_ptr<FallbackState> fallback;
    std::uint64_t fallback_sweeps = 0;
    bool hard_unresolved = false;

    std::vector<double> terminal_mass;
    std::vector<double> action_not_applied;
    std::vector<double> no_matching_edge;
    std::vector<std::map<std::uint32_t, double>> terminal_incoming;
    std::vector<std::map<std::uint32_t, double>>
        compressed_policy_incoming;
    std::map<std::string, std::uint32_t> edge_index_by_id;
    std::vector<double> edge_traversals;
    std::uint64_t peak_owned_bytes_value = 0;
    bool compress_policy_routes = false;
    std::uint32_t compressed_policy_root = kNoId;
    std::vector<PolicyRouteResolution> policy_route_cache;
    std::map<refinement::StableKey, std::uint32_t>
        policy_route_trace_by_key;
    std::vector<refinement::StableKey> policy_route_traces;
    std::uint64_t policy_route_trace_payload_owned_bytes = 0;
    /* Collision-safe chained index over the exact traces retained once in
     * deterministic_route_traces. This is the same full-equality authority
     * used by raw pair discovery, not a hash-only route identity. */
    std::vector<std::uint32_t> deterministic_route_bucket_heads;
    std::vector<std::uint32_t> deterministic_route_next;
    std::vector<refinement::StableKey> deterministic_route_traces;
    refinement::StableKey deterministic_route_trace_scratch;
    std::vector<std::uint32_t> deterministic_route_walk_scratch;
    std::uint64_t deterministic_route_trace_payload_owned_bytes = 0;
    std::vector<DeterministicRouteFlow> deterministic_route_flows;
    std::vector<OperationRowActionCensus> operation_row_census_by_action;
    std::vector<LocalGatedRouteProof> local_gated_route_proof_by_node;
    std::vector<const OutcomeDistribution*> census_stable_distributions;
    /* Exact keys for a deterministic 1/256 sample of (route root, state).
     * Packing two uint32 words is collision-free; the chained index uses the
     * full packed key as equality authority. */
    std::vector<std::uint32_t> route_sample_bucket_heads;
    std::vector<std::uint32_t> route_sample_next;
    std::vector<std::uint64_t> route_sample_keys;
    std::vector<std::uint32_t> replay_route_bucket_heads;
    std::vector<std::uint32_t> replay_route_next;
    std::vector<ReplayRouteResult> replay_route_results;
    std::vector<ObservationRequirement>
        node_observation_requirements;

    bool is_policy_route_node(std::uint32_t node) const {
        return node < strategy->nodes.size() &&
               strategy->nodes[node].kind == StrategyNodeKind::Router &&
               strategy->nodes[node].id.rfind("policy_route_", 0) == 0;
    }

    static std::size_t node_kind_index(const StrategyNodeKind kind) {
        switch (kind) {
        case StrategyNodeKind::Start: return 0;
        case StrategyNodeKind::Operation: return 1;
        case StrategyNodeKind::Router: return 2;
        case StrategyNodeKind::Terminal: return 3;
        }
        throw std::logic_error("strategy evaluation has an invalid node kind");
    }

    bool goal_leaf_matches_target(
            const CompiledCondition& condition,
            const GoalSlot& target) const {
        if (condition.min_value != static_cast<int>(target.min_tier)) {
            return false;
        }
        if (condition.kind == ConditionKind::HasModFamily) {
            return target.family_id != kNoId &&
                   condition.family_id == target.family_id;
        }
        if (condition.kind == ConditionKind::HasModGroup) {
            return target.group_id != kNoId &&
                   condition.group_id == target.group_id;
        }
        return false;
    }

    bool condition_is_exact_zero_goal_progress(
            const CompiledCondition& condition) const {
        if (condition.kind != ConditionKind::Not ||
            condition.children.size() != 1 || model.targets.empty()) {
            return false;
        }
        const CompiledCondition& positive = condition.children.front();
        const std::vector<CompiledCondition>* leaves = nullptr;
        std::vector<CompiledCondition> single;
        if (positive.kind == ConditionKind::Any) {
            leaves = &positive.children;
        } else {
            single.push_back(positive);
            leaves = &single;
        }
        if (leaves->size() != model.targets.size()) return false;
        std::vector<std::uint8_t> matched(model.targets.size(), 0);
        for (const CompiledCondition& leaf : *leaves) {
            std::size_t selected = model.targets.size();
            for (std::size_t target = 0;
                 target < model.targets.size(); ++target) {
                if (!matched[target] &&
                    goal_leaf_matches_target(leaf, model.targets[target])) {
                    selected = target;
                    break;
                }
            }
            if (selected == model.targets.size()) return false;
            matched[selected] = 1;
        }
        return std::all_of(
            matched.begin(), matched.end(),
            [](const std::uint8_t value) { return value != 0; });
    }

    LocalGatedRouteProof classify_local_gated_route(
            const std::uint32_t operation) const {
        if (operation >= strategy->nodes.size()) {
            return LocalGatedRouteProof::NotCandidate;
        }
        const StrategyNode& source = strategy->nodes[operation];
        if (source.kind != StrategyNodeKind::Operation ||
            source.edges.size() != 1) {
            return LocalGatedRouteProof::NotCandidate;
        }
        const StrategyEdge& source_edge = source.edges.front();
        if (source_edge.target >= strategy->nodes.size()) {
            return LocalGatedRouteProof::ShapeRejected;
        }
        const StrategyNode& route = strategy->nodes[source_edge.target];
        const bool named_candidate =
            route.id.ends_with("_gated_route");
        if (!named_candidate) return LocalGatedRouteProof::NotCandidate;
        if (!source_edge.is_default ||
            route.kind != StrategyNodeKind::Router ||
            route.edges.size() != 2 ||
            node_observes_modifier_offer(route)) {
            return LocalGatedRouteProof::ShapeRejected;
        }
        const StrategyEdge* retry = nullptr;
        const StrategyEdge* progress = nullptr;
        for (const StrategyEdge& edge : route.edges) {
            if (edge.is_default) {
                if (progress != nullptr) {
                    return LocalGatedRouteProof::ShapeRejected;
                }
                progress = &edge;
            } else {
                if (retry != nullptr) {
                    return LocalGatedRouteProof::ShapeRejected;
                }
                retry = &edge;
            }
        }
        if (retry == nullptr || progress == nullptr) {
            return LocalGatedRouteProof::ShapeRejected;
        }
        if (!condition_is_exact_zero_goal_progress(retry->condition)) {
            return LocalGatedRouteProof::ConditionRejected;
        }
        if (retry->target >= strategy->nodes.size() ||
            strategy->nodes[retry->target].kind !=
                StrategyNodeKind::Operation) {
            return LocalGatedRouteProof::TargetRejected;
        }
        if (!compress_policy_routes ||
            progress->target != compressed_policy_root) {
            return LocalGatedRouteProof::RootRejected;
        }
        return LocalGatedRouteProof::Proved;
    }

    void record_expanded_pair(
            const std::uint32_t pair_id,
            const EvalRow& row,
            const bool shared_row) {
        const EvalPair& pair = pairs.at(pair_id);
        ++expanded_pairs_by_kind.at(node_kind_index(
            strategy->nodes.at(pair.node).kind));
        if (row.absorptions.empty() && row.transitions.size() == 1 &&
            row.transitions.front().probability == 1.0) {
            ++deterministic_pairs_expanded;
        }
        if (shared_row) ++shared_row_pairs_expanded;
    }

    static std::uint64_t string_bytes(const std::string& value) {
        return sizeof(std::string) + value.capacity() + 1;
    }

    static std::uint64_t string_vector_bytes(
        const std::vector<std::string>& values) {
        std::uint64_t bytes = values.capacity() * sizeof(std::string);
        for (const std::string& value : values) {
            bytes += value.capacity() + 1;
        }
        return bytes;
    }

    static std::uint64_t observation_requirement_nested_bytes(
        const ObservationRequirement& requirement) {
        std::uint64_t bytes =
            requirement.modifier_tag_ids.capacity() *
            sizeof(std::uint32_t);
        bytes += requirement.affix_observations.capacity() *
                 sizeof(RefinementAffixObservation);
        for (const RefinementAffixObservation& observation :
             requirement.affix_observations) {
            bytes +=
                observation.selector.required_tag_ids.capacity() *
                sizeof(std::uint32_t);
        }
        return bytes;
    }

    static std::uint64_t action_total_bytes(
        const StrategyEvalActionTotal& action) {
        std::uint64_t bytes = sizeof(action);
        bytes += action.id.capacity() + action.display_name.capacity() + 2;
        bytes += string_vector_bytes(action.price_keys);
        bytes += string_vector_bytes(action.classifications);
        bytes += action.nodes.capacity() * sizeof(StrategyEvalActionNode);
        for (const StrategyEvalActionNode& node : action.nodes) {
            bytes += node.node_id.capacity() + 1;
        }
        bytes += action.regions.capacity() *
                 sizeof(StrategyEvalActionRegion);
        return bytes;
    }

    std::uint64_t output_owned_bytes() const {
        std::uint64_t bytes = sizeof(output);
        bytes += output.economy_id.capacity() + 1;
        bytes += output.targets.capacity() * sizeof(GoalSlot);
        bytes += output.terminal_nodes.capacity() *
                 sizeof(StrategyEvalTerminalNode);
        for (const auto& node : output.terminal_nodes) {
            bytes += node.node_id.capacity() + 1;
        }
        bytes += output.unresolved_by_node.capacity() *
                 sizeof(StrategyEvalNodeMass);
        for (const auto& node : output.unresolved_by_node) {
            bytes += node.node_id.capacity() + 1;
        }
        bytes += output.failures_by_node.capacity() * sizeof(StrategyEvalFailure);
        for (const auto& failure : output.failures_by_node) {
            bytes += failure.node_id.capacity() + failure.reason.capacity() + 2;
        }
        bytes += output.nodes.capacity() * sizeof(StrategyEvalNode);
        for (const StrategyEvalNode& node : output.nodes) {
            bytes += node.id.capacity() + 1;
            bytes += node.classes.capacity() * sizeof(StrategyEvalClass);
        }
        bytes += output.edges.capacity() * sizeof(StrategyEvalEdge);
        for (const StrategyEvalEdge& edge : output.edges) {
            bytes += edge.id.capacity() + 1;
        }
        bytes += output.occupancy_states.capacity() * sizeof(AbstractState);
        bytes += output.occupancy.capacity() *
                 sizeof(StrategyEvalOccupancyEntry);
        bytes += output.reforge_row_samples.capacity() *
                 sizeof(ReforgeRowTelemetry);
        bytes += output.action_totals.capacity() * sizeof(StrategyEvalActionTotal);
        for (const auto& action : output.action_totals) {
            bytes += action_total_bytes(action) - sizeof(action);
        }
        bytes += output.material_totals.capacity() *
                 sizeof(StrategyEvalMaterialTotal);
        for (const auto& material : output.material_totals) {
            bytes += material.price_key.capacity() + 1;
        }
        bytes += output.review_sections.capacity() *
                 sizeof(StrategyEvalReviewSection);
        for (const StrategyEvalReviewSection& section : output.review_sections) {
            bytes += section.id.capacity() + section.label.capacity() +
                     section.role.capacity() + 3;
            bytes += string_vector_bytes(section.raw_node_ids);
            bytes += string_vector_bytes(section.raw_edge_ids);
            bytes += section.actions.capacity() * sizeof(StrategyEvalActionTotal);
            for (const auto& action : section.actions) {
                bytes += action_total_bytes(action) - sizeof(action);
            }
            bytes += section.materials.capacity() *
                     sizeof(StrategyEvalMaterialTotal);
            for (const auto& material : section.materials) {
                bytes += material.price_key.capacity() + 1;
            }
            bytes += section.techniques.size() *
                     (sizeof(std::pair<const std::string, double>) +
                      3 * sizeof(void*));
            for (const auto& [key, unused] : section.techniques) {
                (void)unused;
                bytes += key.capacity() + 1;
            }
        }
        const auto add_string_map = [&](const auto& values) {
            std::uint64_t map_bytes = values.size() *
                (sizeof(typename std::decay_t<decltype(values)>::value_type) +
                 3 * sizeof(void*));
            for (const auto& [key, unused] : values) {
                (void)unused;
                map_bytes += key.capacity() + 1;
            }
            return map_bytes;
        };
        bytes += add_string_map(output.expected_consumption);
        bytes += add_string_map(output.technique_totals);
        bytes += add_string_map(output.material_quantity_differences);
        bytes += add_string_map(output.section_material_differences);
        return bytes;
    }

    std::uint64_t estimated_owned_bytes() const {
        std::uint64_t bytes = sizeof(*this);
        if (model.calc != nullptr) bytes += model.calc->estimated_owned_bytes();
        bytes += model.operation_by_node.capacity() *
                 sizeof(ResolvedStrategyOperation);
        bytes += model.action_by_node.capacity() * sizeof(std::uint32_t);
        bytes += model.targets.capacity() * sizeof(GoalSlot);
        bytes += review_sections.capacity() * sizeof(ReviewSectionSpec);
        for (const ReviewSectionSpec& section : review_sections) {
            bytes += section.id.capacity() + section.label.capacity() +
                     section.role.capacity() + 3;
            bytes += section.nodes.capacity() * sizeof(std::uint32_t);
            bytes += string_vector_bytes(section.edges);
        }
        bytes += pair_bucket_heads.capacity() * sizeof(std::uint32_t);
        bytes += pair_next.owned_bytes();
        bytes += pairs.owned_bytes();
        bytes += node_observation_requirements.capacity() *
                 sizeof(ObservationRequirement);
        for (const ObservationRequirement& requirement :
             node_observation_requirements) {
            bytes += observation_requirement_nested_bytes(requirement);
        }
        bytes += unveil_offer_by_mods.size() *
                 (sizeof(decltype(unveil_offer_by_mods)::value_type) +
                  3 * sizeof(void*));
        for (const auto& [mods, unused] : unveil_offer_by_mods) {
            (void)unused;
            bytes += mods.capacity() * sizeof(std::uint32_t);
        }
        bytes += unveil_offer_sets.capacity() *
                 sizeof(std::vector<std::uint32_t>);
        for (const auto& mods : unveil_offer_sets) {
            bytes += mods.capacity() * sizeof(std::uint32_t);
        }
        bytes += rows.capacity() * sizeof(EvalRow);
        for (const EvalRow& row : rows) {
            bytes += row.transitions.capacity() * sizeof(EvalTransition);
            bytes += row.transition_via.capacity() * sizeof(std::uint32_t);
            bytes += row.absorptions.capacity() * sizeof(EvalAbsorption);
            bytes += row.replay_route_tokens.capacity() *
                     sizeof(std::uint32_t);
        }
        bytes += attribution_pairs.owned_bytes();
        bytes += attribution_rows.capacity() * sizeof(EvalRow);
        bytes += attribution_class_by_pair.capacity() *
                 sizeof(std::uint32_t);
        bytes += attribution_exact_row_visits.capacity() *
                 sizeof(solve_detail::WideFloat);
        for (const EvalRow& row : attribution_rows) {
            bytes += row.transitions.capacity() * sizeof(EvalTransition);
            bytes += row.transition_via.capacity() * sizeof(std::uint32_t);
            bytes += row.absorptions.capacity() * sizeof(EvalAbsorption);
            bytes += row.replay_route_tokens.capacity() *
                     sizeof(std::uint32_t);
        }
        bytes += row_by_distribution.size() *
                 (sizeof(decltype(row_by_distribution)::value_type) +
                  3 * sizeof(void*));
        bytes += components.capacity() * sizeof(std::vector<std::uint32_t>);
        for (const auto& component : components) {
            bytes += component.capacity() * sizeof(std::uint32_t);
        }
        bytes += component_by_pair.capacity() * sizeof(std::uint32_t);
        bytes += external_incoming.capacity() * sizeof(double);
        bytes += pair_visits.capacity() * sizeof(double);
        bytes += unresolved_pair.capacity() * sizeof(double);
        bytes += pair_contracted.capacity() * sizeof(std::uint8_t);
        bytes += chain_next.capacity() * sizeof(std::uint32_t);
        bytes += chain_edge.capacity() * sizeof(std::uint32_t);
        bytes += chain_policy_route.capacity() * sizeof(std::uint32_t);
        bytes += chain_policy_state.capacity() * sizeof(std::uint32_t);
        bytes += chain_terminal.capacity() * sizeof(std::uint32_t);
        bytes += chain_inflow.capacity() * sizeof(double);
        if (fallback != nullptr) {
            bytes += sizeof(FallbackState);
            bytes += fallback->members.capacity() * sizeof(std::uint32_t);
            bytes += fallback->local_index_by_pair.capacity() *
                     sizeof(std::int32_t);
            bytes += fallback->transpose_rows.capacity() *
                     sizeof(solve_detail::PolicyRow);
            bytes += fallback->transpose_edges.capacity() *
                     sizeof(solve_detail::PolicyEdge);
            bytes += fallback->incoming.capacity() * sizeof(double);
            bytes += fallback->previous_values.capacity() * sizeof(double);
            if (fallback->resume != nullptr) {
                bytes += sizeof(solve_detail::SparsePolicyResume);
                bytes += fallback->resume->members.capacity() *
                         sizeof(std::uint32_t);
                bytes += fallback->resume->b.capacity() *
                         sizeof(solve_detail::WideFloat);
                bytes += fallback->resume->x.capacity() *
                         sizeof(solve_detail::WideFloat);
                bytes += fallback->resume->r.capacity() *
                         sizeof(solve_detail::WideFloat);
                bytes += fallback->resume->r0.capacity() *
                         sizeof(solve_detail::WideFloat);
                bytes += fallback->resume->p.capacity() *
                         sizeof(solve_detail::WideFloat);
                bytes += fallback->resume->v.capacity() *
                         sizeof(solve_detail::WideFloat);
                bytes += fallback->resume->s.capacity() *
                         sizeof(solve_detail::WideFloat);
                bytes += fallback->resume->t.capacity() *
                         sizeof(solve_detail::WideFloat);
            }
        }
        bytes += terminal_mass.capacity() * sizeof(double);
        bytes += action_not_applied.capacity() * sizeof(double);
        bytes += no_matching_edge.capacity() * sizeof(double);
        bytes += terminal_incoming.capacity() *
                 sizeof(std::map<std::uint32_t, double>);
        for (const auto& incoming : terminal_incoming) {
            bytes += incoming.size() *
                     (sizeof(std::decay_t<decltype(incoming)>::value_type) +
                      3 * sizeof(void*));
        }
        bytes += compressed_policy_incoming.capacity() *
                 sizeof(std::map<std::uint32_t, double>);
        for (const auto& incoming : compressed_policy_incoming) {
            bytes += incoming.size() *
                     (sizeof(std::decay_t<decltype(incoming)>::value_type) +
                      3 * sizeof(void*));
        }
        bytes += edge_index_by_id.size() *
                 (sizeof(decltype(edge_index_by_id)::value_type) +
                  3 * sizeof(void*));
        for (const auto& [id, unused] : edge_index_by_id) {
            (void)unused;
            bytes += id.capacity() + 1;
        }
        bytes += edge_traversals.capacity() * sizeof(double);
        bytes += policy_route_cache.capacity() *
                 sizeof(PolicyRouteResolution);
        bytes += policy_route_trace_by_key.size() *
                 (sizeof(decltype(policy_route_trace_by_key)::value_type) +
                  3 * sizeof(void*));
        for (const auto& [trace, unused] : policy_route_trace_by_key) {
            (void)unused;
            bytes += trace.capacity() * sizeof(std::uint64_t);
        }
        bytes += policy_route_traces.capacity() *
                 sizeof(refinement::StableKey);
        for (const refinement::StableKey& trace : policy_route_traces) {
            bytes += trace.capacity() * sizeof(std::uint64_t);
        }
        bytes += deterministic_route_bucket_heads.capacity() *
                 sizeof(std::uint32_t);
        bytes += deterministic_route_next.capacity() *
                 sizeof(std::uint32_t);
        bytes += deterministic_route_traces.capacity() *
                 sizeof(refinement::StableKey);
        for (const refinement::StableKey& trace :
             deterministic_route_traces) {
            bytes += trace.capacity() * sizeof(std::uint64_t);
        }
        bytes += deterministic_route_trace_scratch.capacity() *
                 sizeof(std::uint64_t);
        bytes += deterministic_route_walk_scratch.capacity() *
                 sizeof(std::uint32_t);
        bytes += deterministic_route_flows.capacity() *
                 sizeof(DeterministicRouteFlow);
        bytes += operation_row_census_by_action.capacity() *
                 sizeof(OperationRowActionCensus);
        bytes += local_gated_route_proof_by_node.capacity() *
                 sizeof(LocalGatedRouteProof);
        bytes += census_stable_distributions.capacity() *
                 sizeof(const OutcomeDistribution*);
        bytes += route_sample_bucket_heads.capacity() *
                 sizeof(std::uint32_t);
        bytes += route_sample_next.capacity() * sizeof(std::uint32_t);
        bytes += route_sample_keys.capacity() * sizeof(std::uint64_t);
        bytes += replay_route_bucket_heads.capacity() *
                 sizeof(std::uint32_t);
        bytes += replay_route_next.capacity() * sizeof(std::uint32_t);
        bytes += replay_route_results.capacity() *
                 sizeof(ReplayRouteResult);
        bytes += output_owned_bytes();
        if (finalization_task.has_value()) {
            bytes += finalization_task->retained_bytes();
        }
        if (component_build_task.has_value()) {
            bytes += component_build_task->retained_bytes();
        }
        return bytes;
    }

    /* Constant-time selected-allocation estimate for per-work-item cap
     * enforcement. The full estimator remains the audit authority; these
     * counters mirror its nested-capacity terms at each mutation boundary. */
    std::uint64_t fast_estimated_owned_bytes() const {
        std::uint64_t bytes = sizeof(*this);
        if (model.calc != nullptr) {
            bytes += model.calc->fast_estimated_owned_bytes();
        }
        bytes += model.operation_by_node.capacity() *
                 sizeof(ResolvedStrategyOperation);
        bytes += model.action_by_node.capacity() * sizeof(std::uint32_t);
        bytes += model.targets.capacity() * sizeof(GoalSlot);
        bytes += review_sections.capacity() * sizeof(ReviewSectionSpec);
        bytes += review_payload_owned_bytes;
        bytes += pair_bucket_heads.capacity() * sizeof(std::uint32_t);
        bytes += pair_next.owned_bytes();
        bytes += pairs.owned_bytes();
        bytes += node_observation_requirements.capacity() *
                 sizeof(ObservationRequirement);
        bytes += observation_requirement_owned_bytes;
        bytes += unveil_offer_by_mods.size() *
                 (sizeof(decltype(unveil_offer_by_mods)::value_type) +
                  3 * sizeof(void*));
        bytes += unveil_offer_sets.capacity() *
                 sizeof(std::vector<std::uint32_t>);
        for (const auto& [mods, unused] : unveil_offer_by_mods) {
            (void)unused;
            bytes += mods.capacity() * sizeof(std::uint32_t);
        }
        for (const auto& mods : unveil_offer_sets) {
            bytes += mods.capacity() * sizeof(std::uint32_t);
        }
        bytes += rows.capacity() * sizeof(EvalRow);
        bytes += row_payload_owned_bytes;
        bytes += attribution_pairs.owned_bytes();
        bytes += attribution_rows.capacity() * sizeof(EvalRow);
        bytes += attribution_class_by_pair.capacity() *
                 sizeof(std::uint32_t);
        bytes += attribution_exact_row_visits.capacity() *
                 sizeof(solve_detail::WideFloat);
        bytes += attribution_row_payload_owned_bytes;
        bytes += row_by_distribution.size() *
                 (sizeof(decltype(row_by_distribution)::value_type) +
                  3 * sizeof(void*));
        bytes += components.capacity() * sizeof(std::vector<std::uint32_t>);
        bytes += component_payload_owned_bytes;
        bytes += component_by_pair.capacity() * sizeof(std::uint32_t);
        bytes += external_incoming.capacity() * sizeof(double);
        bytes += pair_visits.capacity() * sizeof(double);
        bytes += unresolved_pair.capacity() * sizeof(double);
        bytes += pair_contracted.capacity() * sizeof(std::uint8_t);
        bytes += chain_next.capacity() * sizeof(std::uint32_t);
        bytes += chain_edge.capacity() * sizeof(std::uint32_t);
        bytes += chain_policy_route.capacity() * sizeof(std::uint32_t);
        bytes += chain_policy_state.capacity() * sizeof(std::uint32_t);
        bytes += chain_terminal.capacity() * sizeof(std::uint32_t);
        bytes += chain_inflow.capacity() * sizeof(double);
        if (fallback != nullptr) {
            bytes += sizeof(FallbackState);
            bytes += fallback->members.capacity() * sizeof(std::uint32_t);
            bytes += fallback->local_index_by_pair.capacity() *
                     sizeof(std::int32_t);
            bytes += fallback->transpose_rows.capacity() *
                     sizeof(solve_detail::PolicyRow);
            bytes += fallback->transpose_edges.capacity() *
                     sizeof(solve_detail::PolicyEdge);
            bytes += fallback->incoming.capacity() * sizeof(double);
            bytes += fallback->previous_values.capacity() * sizeof(double);
            if (fallback->resume != nullptr) {
                bytes += sizeof(solve_detail::SparsePolicyResume);
                bytes += fallback->resume->members.capacity() *
                         sizeof(std::uint32_t);
                bytes += fallback->resume->b.capacity() *
                         sizeof(solve_detail::WideFloat);
                bytes += fallback->resume->x.capacity() *
                         sizeof(solve_detail::WideFloat);
                bytes += fallback->resume->r.capacity() *
                         sizeof(solve_detail::WideFloat);
                bytes += fallback->resume->r0.capacity() *
                         sizeof(solve_detail::WideFloat);
                bytes += fallback->resume->p.capacity() *
                         sizeof(solve_detail::WideFloat);
                bytes += fallback->resume->v.capacity() *
                         sizeof(solve_detail::WideFloat);
                bytes += fallback->resume->s.capacity() *
                         sizeof(solve_detail::WideFloat);
                bytes += fallback->resume->t.capacity() *
                         sizeof(solve_detail::WideFloat);
            }
        }
        bytes += terminal_mass.capacity() * sizeof(double);
        bytes += action_not_applied.capacity() * sizeof(double);
        bytes += no_matching_edge.capacity() * sizeof(double);
        bytes += terminal_incoming.capacity() *
                 sizeof(std::map<std::uint32_t, double>);
        bytes += terminal_incoming_owned_bytes;
        bytes += compressed_policy_incoming.capacity() *
                 sizeof(std::map<std::uint32_t, double>);
        bytes += compressed_policy_incoming_owned_bytes;
        bytes += edge_index_owned_bytes;
        bytes += edge_traversals.capacity() * sizeof(double);
        bytes += policy_route_cache.capacity() *
                 sizeof(PolicyRouteResolution);
        bytes += policy_route_trace_by_key.size() *
                 (sizeof(decltype(policy_route_trace_by_key)::value_type) +
                  3 * sizeof(void*));
        bytes += policy_route_traces.capacity() *
                 sizeof(refinement::StableKey);
        bytes += policy_route_trace_payload_owned_bytes;
        bytes += deterministic_route_bucket_heads.capacity() *
                 sizeof(std::uint32_t);
        bytes += deterministic_route_next.capacity() *
                 sizeof(std::uint32_t);
        bytes += deterministic_route_traces.capacity() *
                 sizeof(refinement::StableKey);
        bytes += deterministic_route_trace_payload_owned_bytes;
        bytes += deterministic_route_trace_scratch.capacity() *
                 sizeof(std::uint64_t);
        bytes += deterministic_route_walk_scratch.capacity() *
                 sizeof(std::uint32_t);
        bytes += deterministic_route_flows.capacity() *
                 sizeof(DeterministicRouteFlow);
        bytes += operation_row_census_by_action.capacity() *
                 sizeof(OperationRowActionCensus);
        bytes += local_gated_route_proof_by_node.capacity() *
                 sizeof(LocalGatedRouteProof);
        bytes += census_stable_distributions.capacity() *
                 sizeof(const OutcomeDistribution*);
        bytes += route_sample_bucket_heads.capacity() *
                 sizeof(std::uint32_t);
        bytes += route_sample_next.capacity() * sizeof(std::uint32_t);
        bytes += route_sample_keys.capacity() * sizeof(std::uint64_t);
        bytes += replay_route_bucket_heads.capacity() *
                 sizeof(std::uint32_t);
        bytes += replay_route_next.capacity() * sizeof(std::uint32_t);
        bytes += replay_route_results.capacity() *
                 sizeof(ReplayRouteResult);
        bytes += output_owned_bytes();
        if (finalization_task.has_value()) {
            bytes += finalization_task->retained_bytes();
        }
        if (component_build_task.has_value()) {
            bytes += component_build_task->retained_bytes();
        }
        return bytes;
    }

    void audit_owned_bytes() {
        const std::uint64_t fast = fast_estimated_owned_bytes();
        const std::uint64_t audited = estimated_owned_bytes();
        if (fast < audited) {
            throw std::logic_error(
                "strategy evaluation owned-byte ledger undercounted by " +
                std::to_string(audited - fast) + " bytes");
        }
        peak_owned_bytes_value = std::max(peak_owned_bytes_value, audited);
    }

    void check_owned_cap(std::uint64_t transient_bytes = 0) {
        const std::uint64_t owned = fast_estimated_owned_bytes();
        const std::uint64_t live =
            transient_bytes > std::numeric_limits<std::uint64_t>::max() - owned
                ? std::numeric_limits<std::uint64_t>::max()
                : owned + transient_bytes;
        peak_owned_bytes_value = std::max(peak_owned_bytes_value, live);
        output.owned_bytes_estimate = owned;
        output.peak_owned_bytes_estimate = peak_owned_bytes_value;
        if (live > options.max_owned_bytes) {
            throw std::length_error(
                "strategy evaluation exceeded max_owned_bytes (" +
                std::to_string(options.max_owned_bytes) +
                "; owned=" + std::to_string(owned) +
                ", transient=" + std::to_string(transient_bytes) +
                ", calc=" +
                    std::to_string(
                        model.calc == nullptr
                            ? 0
                            : model.calc->fast_estimated_owned_bytes()) +
                ", states=" +
                    std::to_string(
                        model.calc == nullptr ? 0 : model.calc->state_count()) +
                ", pairs=" + std::to_string(pairs.size()) +
                ", pair_index=" +
                    std::to_string(
                        pair_bucket_heads.capacity() *
                            sizeof(std::uint32_t) +
                        pair_next.owned_bytes()) +
                ", rows=" + std::to_string(rows.size()) +
                ", transitions=" + std::to_string(stored_transitions) +
                ", graph_nodes=" +
                    std::to_string(strategy == nullptr
                                       ? 0
                                       : strategy->nodes.size()) +
                ", graph_edges=" +
                    std::to_string(
                        strategy == nullptr
                            ? 0
                            : std::accumulate(
                                  strategy->nodes.begin(),
                                  strategy->nodes.end(),
                                  std::uint64_t{0},
                                  [](const std::uint64_t total,
                                     const StrategyNode& node) {
                                      return total + node.edges.size();
                                  })) +
                ", component_count=" +
                    std::to_string(components.size()) +
                ", largest_component=" +
                    std::to_string(
                        components.empty()
                            ? 0
                            : std::max_element(
                                  components.begin(), components.end(),
                                  [](const auto& left, const auto& right) {
                                      return left.size() < right.size();
                                  })->size()) +
                ", path=" +
                    (components.empty()
                         ? std::string{"pre_component"}
                         : std::string{"sparse_component"}) +
                ", probe=" + memory_probe_stage +
                ", probe_units=" +
                    std::to_string(memory_probe_units) +
                ", probe_unit_bytes=" +
                    std::to_string(memory_probe_unit_bytes) +
                ", row_payload=" +
                    std::to_string(row_payload_owned_bytes) +
                ", attribution_row_payload=" +
                    std::to_string(attribution_row_payload_owned_bytes) +
                ", observation_requirements=" +
                    std::to_string(observation_requirement_owned_bytes) +
                ", component_payload=" +
                    std::to_string(component_payload_owned_bytes) +
                ", output=" + std::to_string(output_owned_bytes()) +
                ", phase=" +
                    std::to_string(static_cast<int>(phase)) +
                ")");
        }
    }

    void refresh_row_payload_owned_bytes() {
        row_payload_owned_bytes = 0;
        output.transition_via_owned_bytes = 0;
        for (const EvalRow& row : rows) {
            output.transition_via_owned_bytes +=
                row.transition_via.capacity() * sizeof(std::uint32_t);
            row_payload_owned_bytes +=
                row.transitions.capacity() * sizeof(EvalTransition) +
                row.transition_via.capacity() * sizeof(std::uint32_t) +
                row.absorptions.capacity() * sizeof(EvalAbsorption) +
                row.replay_route_tokens.capacity() *
                    sizeof(std::uint32_t);
        }
    }

    void add_terminal_incoming(
        std::uint32_t node, std::uint32_t state, double mass) {
        auto [entry, inserted] =
            terminal_incoming[node].try_emplace(state, 0.0);
        if (inserted) {
            terminal_incoming_owned_bytes +=
                sizeof(std::map<std::uint32_t, double>::value_type) +
                3 * sizeof(void*);
        }
        entry->second += mass;
    }

    void add_compressed_policy_incoming(
        std::uint32_t node, std::uint32_t state, double mass) {
        if (node == kNoId || !(mass > 0.0)) return;
        if (is_deterministic_route_authority(node)) {
            if (state == kNoId) {
                throw std::logic_error(
                    "deterministic route flow has no exact state");
            }
            const std::size_t capacity_before =
                deterministic_route_flows.capacity();
            deterministic_route_flows.push_back({mass, node, state});
            if (deterministic_route_flows.capacity() != capacity_before) {
                check_owned_cap();
            }
            return;
        }
        auto [entry, inserted] =
            compressed_policy_incoming[node].try_emplace(state, 0.0);
        if (inserted) {
            compressed_policy_incoming_owned_bytes +=
                sizeof(std::map<std::uint32_t, double>::value_type) +
                3 * sizeof(void*);
        }
        entry->second += mass;
    }

    void check_owned_projection(
        std::uint64_t owned,
        std::uint64_t transient_bytes) {
        const std::uint64_t live =
            transient_bytes > std::numeric_limits<std::uint64_t>::max() - owned
                ? std::numeric_limits<std::uint64_t>::max()
                : owned + transient_bytes;
        peak_owned_bytes_value = std::max(peak_owned_bytes_value, live);
        if (live > options.max_owned_bytes) {
            throw std::length_error(
                "strategy evaluation exceeded max_owned_bytes (" +
                std::to_string(options.max_owned_bytes) +
                "; owned=" + std::to_string(owned) +
                ", transient=" + std::to_string(transient_bytes) +
                ", calc=" +
                    std::to_string(
                        model.calc == nullptr
                            ? 0
                            : model.calc->fast_estimated_owned_bytes()) +
                ", states=" +
                    std::to_string(
                        model.calc == nullptr ? 0 : model.calc->state_count()) +
                ", pairs=" + std::to_string(pairs.size()) +
                ", pair_index=" +
                    std::to_string(
                        pair_bucket_heads.capacity() *
                            sizeof(std::uint32_t) +
                        pair_next.owned_bytes()) +
                ", rows=" + std::to_string(rows.size()) +
                ", transitions=" + std::to_string(stored_transitions) +
                ", graph_nodes=" +
                    std::to_string(strategy == nullptr
                                       ? 0
                                       : strategy->nodes.size()) +
                ", graph_edges=" +
                    std::to_string(
                        strategy == nullptr
                            ? 0
                            : std::accumulate(
                                  strategy->nodes.begin(),
                                  strategy->nodes.end(),
                                  std::uint64_t{0},
                                  [](const std::uint64_t total,
                                     const StrategyNode& node) {
                                      return total + node.edges.size();
                                  })) +
                ", component_count=" +
                    std::to_string(components.size()) +
                ", largest_component=" +
                    std::to_string(
                        components.empty()
                            ? 0
                            : std::max_element(
                                  components.begin(), components.end(),
                                  [](const auto& left, const auto& right) {
                                      return left.size() < right.size();
                                  })->size()) +
                ", path=" +
                    (components.empty()
                         ? std::string{"pre_component"}
                         : std::string{"sparse_component"}) +
                ", probe=" + memory_probe_stage +
                ", probe_units=" +
                    std::to_string(memory_probe_units) +
                ", probe_unit_bytes=" +
                    std::to_string(memory_probe_unit_bytes) +
                ", row_payload=" +
                    std::to_string(row_payload_owned_bytes) +
                ", attribution_row_payload=" +
                    std::to_string(attribution_row_payload_owned_bytes) +
                ", observation_requirements=" +
                    std::to_string(observation_requirement_owned_bytes) +
                ", component_payload=" +
                    std::to_string(component_payload_owned_bytes) +
                ", output=" + std::to_string(output_owned_bytes()) +
                ", phase=" +
                    std::to_string(static_cast<int>(phase)) +
                ")");
        }
    }

    Impl(
        std::shared_ptr<const StrategyImpl> strategy_in,
        const StrategyEvalOptions& options_in)
        : strategy(std::move(strategy_in)),
          options(options_in),
          model(derive_checked_model(
              strategy, options_in.max_states,
              options_in.use_exact_exchangeable_family_compression)),
          review_sections(parse_review_sections(
              *strategy, options_in.review_projection_json)) {
        if (strategy == nullptr || strategy->session == nullptr ||
            strategy->start_node >= strategy->nodes.size()) {
            throw std::invalid_argument("invalid compiled strategy");
        }
        if (!std::isfinite(options.epsilon) || options.epsilon <= 0.0 ||
            options.max_sweeps == 0 || options.max_states == 0 ||
            options.max_pairs == 0 || options.max_transitions == 0 ||
            options.max_owned_bytes == 0 ||
            options.max_output_json_bytes == 0) {
            throw std::invalid_argument("invalid strategy evaluation options");
        }
        output.stage_timings.model_setup_ns =
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    Clock::now() - construction_started)
                    .count());
        output.refined_pair_limit = options.max_pairs;
        output.max_owned_bytes = options.max_owned_bytes;
        output.max_output_json_bytes = options.max_output_json_bytes;
        model.calc->set_reforge_provenance_context(
            ReforgeRowOwner::ExactEvaluation);
        model.calc->set_solve_resource_caps(
            options.max_states,
            options.max_reforge_work,
            false,
            options.max_owned_bytes);
        operation_row_census_by_action.resize(
            model.calc->registry().actions.size());
        output.targets = model.targets;
        for (const ReviewSectionSpec& section : review_sections) {
            review_payload_owned_bytes +=
                section.id.capacity() + section.label.capacity() +
                section.role.capacity() + 3;
            review_payload_owned_bytes +=
                section.nodes.capacity() * sizeof(std::uint32_t);
            review_payload_owned_bytes += string_vector_bytes(section.edges);
        }
        const std::size_t node_count = strategy->nodes.size();
        check_owned_cap();
        subphase = StrategyEvalSubphase::ObservationPreparation;
        const auto observation_started =
            Clock::now();
        node_observation_requirements =
            derive_node_observation_requirements(
                *strategy, model, options.max_sweeps,
                &output.observation_propagation,
                [&](const std::uint64_t transient_bytes,
                    const char* stage,
                    const std::uint64_t units,
                    const std::uint64_t unit_bytes) {
                    memory_probe_stage = stage;
                    memory_probe_units = units;
                    memory_probe_unit_bytes = unit_bytes;
                    check_owned_cap(transient_bytes);
                });
        output.observation_propagation.duration_ns =
            static_cast<std::uint64_t>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(
                    Clock::now() -
                    observation_started)
                    .count());
        output.stage_timings.observation_preparation_ns =
            output.observation_propagation.duration_ns;
        memory_probe_stage = "steady_state";
        memory_probe_units = 0;
        memory_probe_unit_bytes = 0;
        for (const ObservationRequirement& requirement :
             node_observation_requirements) {
            observation_requirement_owned_bytes +=
                observation_requirement_nested_bytes(requirement);
        }
        std::vector<std::uint32_t> policy_roots;
        for (std::uint32_t source = 0; source < node_count; ++source) {
            if (is_policy_route_node(source)) continue;
            for (const StrategyEdge& edge : strategy->nodes[source].edges) {
                if (is_policy_route_node(edge.target) &&
                    std::find(
                        policy_roots.begin(), policy_roots.end(),
                        edge.target) == policy_roots.end()) {
                    policy_roots.push_back(edge.target);
                }
            }
        }
        /* A single external root means each state has one deterministic walk
         * through the compiler DAG. This permits exact online contraction and
         * bounded top-class selection without per-(router,state) graph rows. */
        compress_policy_routes = policy_roots.size() == 1;
        if (compress_policy_routes) {
            compressed_policy_root = policy_roots.front();
        }
        local_gated_route_proof_by_node.resize(
            node_count, LocalGatedRouteProof::NotCandidate);
        for (std::uint32_t node = 0; node < node_count; ++node) {
            local_gated_route_proof_by_node[node] =
                classify_local_gated_route(node);
        }
        terminal_mass.assign(node_count, 0.0);
        action_not_applied.assign(node_count, 0.0);
        no_matching_edge.assign(node_count, 0.0);
        terminal_incoming.resize(node_count);
        compressed_policy_incoming.resize(node_count);
        for (const StrategyNode& node : strategy->nodes) {
            for (const StrategyEdge& edge : node.edges) {
                const std::uint32_t index =
                    static_cast<std::uint32_t>(edge_traversals.size());
                const auto [entry, inserted] =
                    edge_index_by_id.emplace(edge.id, index);
                if (!inserted) {
                    throw std::logic_error(
                        "compiled strategy contains a duplicate edge id");
                }
                edge_index_owned_bytes +=
                    sizeof(decltype(edge_index_by_id)::value_type) +
                    3 * sizeof(void*) + entry->first.capacity() + 1;
                edge_traversals.push_back(0.0);
            }
        }

        const std::uint32_t start_state =
            model.calc->intern_item(strategy->start_item);
        ensure_state_limit();
        if (strategy->nodes[strategy->start_node].kind ==
            StrategyNodeKind::Terminal) {
            terminal_mass[strategy->start_node] = 1.0;
            add_terminal_incoming(
                strategy->start_node, start_state, 1.0);
            phase = StrategyEvalPhase::Finalization;
        } else {
            subphase = StrategyEvalSubphase::PairDiscovery;
            start_pair = intern_pair(strategy->start_node, start_state);
        }
        check_owned_cap();
    }

    void ensure_state_limit() const {
        if (model.calc->state_count() > options.max_states) {
            throw std::length_error(
                "strategy evaluation exceeded max_states (" +
                std::to_string(options.max_states) + ")");
        }
    }

    std::uint32_t intern_unveil_offer(
        std::vector<std::uint32_t> mods) {
        std::sort(mods.begin(), mods.end());
        mods.erase(std::unique(mods.begin(), mods.end()), mods.end());
        const auto found = unveil_offer_by_mods.find(mods);
        if (found != unveil_offer_by_mods.end()) return found->second;
        const std::uint32_t id =
            static_cast<std::uint32_t>(unveil_offer_sets.size());
        unveil_offer_sets.push_back(mods);
        unveil_offer_by_mods.emplace(std::move(mods), id);
        return id;
    }

    static std::uint64_t mix_pair_index_word(std::uint64_t value) {
        value += 0x9e3779b97f4a7c15ull;
        value = (value ^ (value >> 30u)) *
                0xbf58476d1ce4e5b9ull;
        value = (value ^ (value >> 27u)) *
                0x94d049bb133111ebull;
        return value ^ (value >> 31u);
    }

    static std::uint64_t route_sample_key(
            const std::uint32_t root,
            const std::uint32_t state) {
        return (static_cast<std::uint64_t>(root) << 32u) | state;
    }

    static bool census_sample(const std::uint64_t key) {
        return (mix_pair_index_word(key) & 0xffu) == 0;
    }

    void rehash_route_sample_index(const std::size_t bucket_count) {
        if (bucket_count == 0 ||
            (bucket_count & (bucket_count - 1)) != 0) {
            throw std::logic_error(
                "strategy evaluation route-sample buckets are not a power "
                "of two");
        }
        check_owned_cap(bucket_count * sizeof(std::uint32_t));
        std::vector<std::uint32_t> replacement(bucket_count, kNoId);
        route_sample_next.resize(route_sample_keys.size(), kNoId);
        for (std::uint32_t index = 0;
             index < route_sample_keys.size(); ++index) {
            const std::size_t bucket =
                mix_pair_index_word(route_sample_keys[index]) &
                (bucket_count - 1);
            route_sample_next[index] = replacement[bucket];
            replacement[bucket] = index;
        }
        route_sample_bucket_heads.swap(replacement);
    }

    void observe_sampled_route_key(
            const std::uint32_t root,
            const std::uint32_t state) {
        const std::uint64_t key = route_sample_key(root, state);
        if (!census_sample(key)) return;
        auto& census = output.operation_row_census;
        ++census.sampled_route_keys;
        if (route_sample_bucket_heads.empty()) {
            rehash_route_sample_index(64);
        } else if (route_sample_keys.size() >=
                   route_sample_bucket_heads.size() * 6) {
            rehash_route_sample_index(
                route_sample_bucket_heads.size() * 2);
        }
        const std::size_t bucket =
            mix_pair_index_word(key) &
            (route_sample_bucket_heads.size() - 1);
        for (std::uint32_t candidate = route_sample_bucket_heads[bucket];
             candidate != kNoId;
             candidate = route_sample_next[candidate]) {
            if (route_sample_keys[candidate] == key) {
                ++census.sampled_route_key_reuses;
                return;
            }
        }
        const std::uint32_t index =
            static_cast<std::uint32_t>(route_sample_keys.size());
        route_sample_keys.push_back(key);
        route_sample_next.push_back(route_sample_bucket_heads[bucket]);
        route_sample_bucket_heads[bucket] = index;
        check_owned_cap();
    }

    static std::uint64_t replay_route_hash(
            const ReplayRouteResult& result) {
        std::uint64_t hash = 0x6576616c72746f6bull; /* "evalrtok" */
        for (const std::uint32_t word :
             {static_cast<std::uint32_t>(result.kind), result.node,
              result.edge, result.policy_route}) {
            hash = mix_pair_index_word(
                hash ^ mix_pair_index_word(word));
        }
        return hash;
    }

    void rehash_replay_route_index(const std::size_t bucket_count) {
        if (bucket_count == 0 ||
            (bucket_count & (bucket_count - 1)) != 0) {
            throw std::logic_error(
                "strategy evaluation replay-route buckets are not a power "
                "of two");
        }
        check_owned_cap(bucket_count * sizeof(std::uint32_t));
        std::vector<std::uint32_t> replacement(bucket_count, kNoId);
        replay_route_next.resize(replay_route_results.size(), kNoId);
        for (std::uint32_t index = 0;
             index < replay_route_results.size(); ++index) {
            const std::size_t bucket =
                replay_route_hash(replay_route_results[index]) &
                (bucket_count - 1);
            replay_route_next[index] = replacement[bucket];
            replacement[bucket] = index;
        }
        replay_route_bucket_heads.swap(replacement);
    }

    std::uint32_t intern_replay_route_result(
            const ReplayRouteResult& result) {
        if (replay_route_bucket_heads.empty()) {
            rehash_replay_route_index(64);
        } else if (replay_route_results.size() >=
                   replay_route_bucket_heads.size() * 6) {
            rehash_replay_route_index(
                replay_route_bucket_heads.size() * 2);
        }
        const std::size_t bucket = replay_route_hash(result) &
            (replay_route_bucket_heads.size() - 1);
        for (std::uint32_t candidate = replay_route_bucket_heads[bucket];
             candidate != kNoId;
             candidate = replay_route_next[candidate]) {
            if (replay_route_results[candidate] == result) {
                return candidate;
            }
        }
        const std::uint32_t index =
            static_cast<std::uint32_t>(replay_route_results.size());
        replay_route_results.push_back(result);
        replay_route_next.push_back(replay_route_bucket_heads[bucket]);
        replay_route_bucket_heads[bucket] = index;
        output.operation_row_census.replay_route_result_authorities =
            replay_route_results.size();
        check_owned_cap();
        return index;
    }

    static std::uint64_t outcome_payload_bytes(
            const OutcomeDistribution& outcomes) {
        std::uint64_t bytes =
            outcomes.entries.capacity() * sizeof(OutcomeEntry) +
            outcomes.choice_groups.capacity() * sizeof(OutcomeChoiceGroup) +
            outcomes.choice_options.capacity() * sizeof(OutcomeChoiceOption);
        for (const OutcomeChoiceGroup& group : outcomes.choice_groups) {
            bytes += group.states.capacity() * sizeof(std::uint32_t);
        }
        return bytes;
    }

    refinement::StableKey node_observation_key(
            const std::uint32_t node,
            const std::uint32_t state) const {
        const ObservationRequirement& requirement =
            node_observation_requirements.at(node);
        if (requirement.item_features == 0 &&
            requirement.modifier_tag_ids.empty() &&
            requirement.affix_observations.empty()) {
            return refinement::StableKey{
                0x6576616c6f627330ull}; /* "evalobs0" */
        }
        const refinement::AbstractFeatureExtraction extraction =
            refinement::extract_strict_abstract_features(
                model.calc->session(), model.calc->layout(),
                model.calc->state(state), requirement);
        if (!extraction.complete()) {
            throw StrategyEvalUnsupported(
                "strategy evaluation unsupported:\n- node '" +
                strategy->nodes.at(node).id +
                "' requires an exact observation discarded by the "
                "evaluation carrier");
        }
        const refinement::FeatureSignature observed =
            refinement::observe_features(
                extraction.features, requirement);
        refinement::StableKey key{
            0x6576616c6f627331ull}; /* "evalobs1" */
        append_feature_signature(key, observed);
        return key;
    }

    static std::uint64_t pair_index_hash(
        const std::uint32_t node,
        const std::uint32_t state,
        const std::uint32_t unveil_offer,
        const std::uint32_t checkpoint_state) {
        std::uint64_t hash = 0x6576616c70616972ull;
        for (const std::uint32_t word :
             {node, state, unveil_offer, checkpoint_state}) {
            hash = mix_pair_index_word(
                hash ^ mix_pair_index_word(word));
        }
        return hash;
    }

    std::uint64_t pair_index_owned_bytes() const {
        return
            pair_bucket_heads.capacity() * sizeof(std::uint32_t) +
            pair_next.owned_bytes();
    }

    void rehash_pair_index(const std::size_t bucket_count) {
        if (bucket_count == 0 ||
            (bucket_count & (bucket_count - 1)) != 0) {
            throw std::logic_error(
                "strategy evaluation pair-index buckets are not a power "
                "of two");
        }
        check_owned_cap(
            bucket_count * sizeof(std::uint32_t));
        std::vector<std::uint32_t> replacement(
            bucket_count, kNoId);
        for (std::uint32_t id = 0; id < pairs.size(); ++id) {
            const EvalPair& pair = pairs[id];
            const std::size_t bucket =
                pair_index_hash(
                    pair.node, pair.state, pair.unveil_offer,
                    pair.checkpoint_state) &
                (bucket_count - 1);
            pair_next[id] = replacement[bucket];
            replacement[bucket] = id;
        }
        pair_bucket_heads.swap(replacement);
        output.pair_discovery_index_peak_bytes = std::max(
            output.pair_discovery_index_peak_bytes,
            pair_index_owned_bytes());
    }

    void ensure_pair_index_for_insert() {
        if (pair_bucket_heads.empty()) {
            rehash_pair_index(64);
            return;
        }
        /* Chaining keeps the index compact: a load of six still performs a
         * small expected number of full, collision-safe key comparisons and
         * avoids a late power-of-two rehash near the five-T1 boundary. */
        constexpr std::size_t kMaximumAverageChain = 6;
        if (pair_next.size() >=
            pair_bucket_heads.size() * kMaximumAverageChain) {
            if (pair_bucket_heads.size() >
                std::numeric_limits<std::size_t>::max() / 2) {
                throw std::length_error(
                    "strategy evaluation pair index is too large");
            }
            rehash_pair_index(pair_bucket_heads.size() * 2);
        }
    }

    void retire_pair_lookup_index() {
        output.pair_discovery_index_peak_bytes = std::max(
            output.pair_discovery_index_peak_bytes,
            pair_index_owned_bytes());
        std::vector<std::uint32_t>().swap(pair_bucket_heads);
        pair_next.release();
    }

    void retire_row_discovery_indexes() {
        decltype(row_by_distribution){}.swap(row_by_distribution);
        std::vector<std::uint32_t>().swap(replay_route_bucket_heads);
        std::vector<std::uint32_t>().swap(replay_route_next);
    }

    void retire_pair_discovery_indexes() {
        retire_pair_lookup_index();
        retire_row_discovery_indexes();
    }

    std::uint32_t intern_pair(
        std::uint32_t node,
        std::uint32_t state,
        std::uint32_t unveil_offer = kNoId,
        std::uint32_t checkpoint_state = kNoId) {
        ActiveTimer timer(output.stage_timings.pair_interning_ns);
        /*
         * This is deliberately the raw exact pair identity. Observation
         * equality is only a seed for the shared split-only fixed point after
         * the reachable graph is closed; it must never irreversibly select a
         * representative during discovery.
         */
        ensure_pair_index_for_insert();
        const std::size_t bucket =
            pair_index_hash(
                node, state, unveil_offer, checkpoint_state) &
            (pair_bucket_heads.size() - 1);
        for (std::uint32_t candidate = pair_bucket_heads[bucket];
             candidate != kNoId;
             candidate = pair_next.at(candidate)) {
            const EvalPair& pair = pairs.at(candidate);
            if (pair.node == node && pair.state == state &&
                pair.unveil_offer == unveil_offer &&
                pair.checkpoint_state == checkpoint_state) {
                return candidate;
            }
        }
        const std::uint32_t id = static_cast<std::uint32_t>(pairs.size());
        check_owned_cap(capped_add(
            pairs.additional_owned_bytes_for_push(),
            pair_next.additional_owned_bytes_for_push()));
        EvalPair pair;
        pair.node = node;
        pair.state = state;
        pair.checkpoint_state = checkpoint_state;
        pair.unveil_offer = unveil_offer;
        pairs.push_back(std::move(pair));
        pair_next.push_back(pair_bucket_heads[bucket]);
        pair_bucket_heads[bucket] = id;
        ++raw_pairs_by_kind.at(node_kind_index(
            strategy->nodes.at(node).kind));
        output.pair_discovery_index_peak_bytes = std::max(
            output.pair_discovery_index_peak_bytes,
            pair_index_owned_bytes());
        return id;
    }

    std::uint32_t find_pair_in(
            const solve_detail::SegmentedVector<EvalPair>& carrier,
            const std::uint32_t node,
            const std::uint32_t state,
            const std::uint32_t unveil_offer = kNoId,
            const std::uint32_t checkpoint_state = kNoId) const {
        if (pair_bucket_heads.empty()) return kNoId;
        const std::size_t bucket =
            pair_index_hash(
                node, state, unveil_offer, checkpoint_state) &
            (pair_bucket_heads.size() - 1);
        for (std::uint32_t candidate = pair_bucket_heads[bucket];
             candidate != kNoId;
             candidate = pair_next.at(candidate)) {
            const EvalPair& pair = carrier.at(candidate);
            if (pair.node == node && pair.state == state &&
                pair.unveil_offer == unveil_offer &&
                pair.checkpoint_state == checkpoint_state) {
                return candidate;
            }
        }
        return kNoId;
    }

    std::uint32_t find_pair(
            const std::uint32_t node,
            const std::uint32_t state,
            const std::uint32_t unveil_offer = kNoId,
            const std::uint32_t checkpoint_state = kNoId) const {
        return find_pair_in(
            pairs, node, state, unveil_offer, checkpoint_state);
    }

    template <typename TransitionVisitor, typename AbsorptionVisitor>
    void visit_eval_row(
            const EvalRow& row,
            const solve_detail::SegmentedVector<EvalPair>& carrier,
            TransitionVisitor&& visit_transition,
            AbsorptionVisitor&& visit_absorption) const {
        if (!row.replayable()) {
            for (const EvalTransition& transition : row.transitions) {
                visit_transition(transition);
            }
            for (const EvalAbsorption& absorption : row.absorptions) {
                visit_absorption(absorption);
            }
            return;
        }
        const OutcomeDistribution& distribution =
            *row.replay_distribution;
        if (!distribution.stable_shared_kernel ||
            distribution.entries.size() !=
                row.replay_route_tokens.size() ||
            !row.transitions.empty() || !row.absorptions.empty()) {
            throw std::logic_error(
                "strategy evaluation replay row has invalid stable "
                "kernel authority");
        }
        for (std::size_t index = 0;
             index < distribution.entries.size(); ++index) {
            const OutcomeEntry& outcome = distribution.entries[index];
            const ReplayRouteResult& route = replay_route_results.at(
                row.replay_route_tokens[index]);
            if (route.kind == ReplayRouteKind::Transition) {
                const std::uint32_t target = find_pair_in(
                    carrier, route.node, outcome.state, kNoId,
                    row.replay_checkpoint_state);
                if (target == kNoId) {
                    throw std::logic_error(
                        "strategy evaluation replay row names an "
                        "undiscovered successor");
                }
                visit_transition(EvalTransition{
                    target, outcome.probability, route.edge,
                    route.policy_route, outcome.state});
                continue;
            }
            visit_absorption(EvalAbsorption{
                route.kind == ReplayRouteKind::Terminal
                    ? EvalAbsorptionKind::Terminal
                    : EvalAbsorptionKind::NoMatchingEdge,
                route.node, outcome.state, outcome.probability,
                route.edge, route.policy_route});
        }
    }

    static std::size_t eval_row_arc_count(const EvalRow& row) {
        return row.replayable()
            ? row.replay_route_tokens.size()
            : row.transitions.size() + row.absorptions.size();
    }

    static bool has_replayable_rows(const std::vector<EvalRow>& source) {
        return std::any_of(
            source.begin(), source.end(),
            [](const EvalRow& row) { return row.replayable(); });
    }

    void materialize_replay_rows_for_legacy_consumers() {
        for (EvalRow& row : rows) {
            if (!row.replayable()) continue;
            const OutcomeDistribution& distribution =
                *row.replay_distribution;
            if (!distribution.stable_shared_kernel ||
                distribution.entries.size() !=
                    row.replay_route_tokens.size() ||
                !row.transitions.empty() || !row.absorptions.empty()) {
                throw std::logic_error(
                    "strategy evaluation replay row has invalid stable "
                    "kernel authority");
            }
            std::size_t transition_count = 0;
            for (const std::uint32_t token : row.replay_route_tokens) {
                if (replay_route_results.at(token).kind ==
                    ReplayRouteKind::Transition) {
                    ++transition_count;
                }
            }
            row.transitions.reserve(transition_count);
            row.absorptions.reserve(
                distribution.entries.size() - transition_count);
            solve_detail::WideFloat mass = 0.0;
            for (std::size_t index = 0;
                 index < distribution.entries.size(); ++index) {
                const OutcomeEntry& outcome = distribution.entries[index];
                const ReplayRouteResult& route =
                    replay_route_results.at(
                        row.replay_route_tokens[index]);
                mass += solve_detail::WideFloat{outcome.probability};
                if (route.kind == ReplayRouteKind::Transition) {
                    const std::uint32_t target = find_pair(
                        route.node, outcome.state, kNoId,
                        row.replay_checkpoint_state);
                    if (target == kNoId) {
                        throw std::logic_error(
                            "strategy evaluation replay row names an "
                            "undiscovered successor");
                    }
                    row.transitions.push_back({
                        target, outcome.probability, route.edge,
                        route.policy_route, outcome.state});
                    continue;
                }
                row.absorptions.push_back({
                    route.kind == ReplayRouteKind::Terminal
                        ? EvalAbsorptionKind::Terminal
                        : EvalAbsorptionKind::NoMatchingEdge,
                    route.node, outcome.state, outcome.probability,
                    route.edge, route.policy_route});
            }
            std::sort(
                row.transitions.begin(), row.transitions.end(),
                [](const EvalTransition& left,
                   const EvalTransition& right) {
                    return std::tie(
                               left.target, left.edge,
                               left.policy_route, left.policy_state) <
                           std::tie(
                               right.target, right.edge,
                               right.policy_route, right.policy_state);
                });
            std::sort(
                row.absorptions.begin(), row.absorptions.end(),
                [](const EvalAbsorption& left,
                   const EvalAbsorption& right) {
                    return std::tuple{
                               static_cast<int>(left.kind), left.node,
                               left.state, left.edge, left.policy_route} <
                           std::tuple{
                               static_cast<int>(right.kind), right.node,
                               right.state, right.edge,
                               right.policy_route};
                });
            if (std::fabs(mass.value() - 1.0) > 1e-9) {
                throw std::runtime_error(
                    "strategy evaluation replayed row does not sum to one");
            }
            row.replay_distribution = nullptr;
            row.replay_checkpoint_state = kNoId;
            std::vector<std::uint32_t>().swap(row.replay_route_tokens);
            check_owned_cap();
        }
        std::vector<std::uint32_t>().swap(replay_route_bucket_heads);
        std::vector<std::uint32_t>().swap(replay_route_next);
        std::vector<ReplayRouteResult>().swap(replay_route_results);
        refresh_row_payload_owned_bytes();
        check_owned_cap();
    }

    const OutcomeDistribution& exact_outcomes(
        const std::uint32_t state,
        const std::uint32_t action,
        const bool goal_progress_gated = false) {
        ActiveTimer timer(output.stage_timings.exact_kernel_ns);
        return model.calc->outcomes(
            state, action, goal_progress_gated);
    }

    const EvalRow& pair_row(std::uint32_t pair) const {
        return rows.at(pairs.at(pair).row);
    }

    static std::uint32_t transition_via(
        const EvalRow& row,
        const std::size_t transition) {
        if (row.transition_via.empty()) return kNoId;
        if (row.transition_via.size() != row.transitions.size()) {
            throw std::logic_error(
                "strategy evaluation transition-via sidecar is not "
                "parallel to its row");
        }
        return row.transition_via.at(transition);
    }

    std::string operation_row_census_summary() const {
        std::ostringstream out;
        out << '[';
        bool first = true;
        const auto& actions = model.calc->registry().actions;
        for (std::size_t index = 0;
             index < operation_row_census_by_action.size(); ++index) {
            const OperationRowActionCensus& census =
                operation_row_census_by_action[index];
            if (census.materialized_rows == 0 &&
                census.shared_row_reuses == 0) {
                continue;
            }
            if (!first) out << ';';
            first = false;
            out << actions.at(index).id
                << ":family="
                << static_cast<unsigned>(primitive_family_for_action(
                       actions.at(index).params.type))
                << ":rows=" << census.materialized_rows
                << ":reuses=" << census.shared_row_reuses
                << ":stable=" << census.stable_shared_rows
                << ":local=" << census.state_local_rows
                << ":direct_repeat=" << census.direct_repeat_rows
                << ":local_gated=" << census.local_gated_route_rows
                << ":other=" << census.other_operation_rows
                << ":gated_kernel=" << census.goal_progress_gated_rows
                << ":full_kernel=" << census.full_physical_rows
                << ":local_gated_proved="
                << census.local_gated_route_proved_rows
                << ":local_gated_full_outcomes="
                << census.local_gated_full_outcome_entries
                << ":local_gated_full_routed="
                << census.local_gated_full_routed_transitions
                << ":local_gated_full_outcome_payload="
                << census.local_gated_full_outcome_payload_bytes
                << ":local_gated_full_routed_payload="
                << census.local_gated_full_routed_payload_bytes
                << ":outcomes=" << census.exact_outcome_entries
                << ":routed=" << census.routed_transitions
                << ":absorptions=" << census.absorptions
                << ":outcome_payload="
                << census.exact_outcome_payload_bytes;
        }
        out << ']';
        return out.str();
    }

    void ensure_transition_budget(std::uint64_t additional) const {
        if (stored_transitions > options.max_transitions ||
            additional > options.max_transitions - stored_transitions) {
            throw std::length_error(
                "strategy evaluation exceeded max_transitions (" +
                std::to_string(options.max_transitions) +
                "; stored=" + std::to_string(stored_transitions) +
                ", pairs=" + std::to_string(pairs.size()) +
                ", expanded=" + std::to_string(discover_index) +
                ", pending=" +
                    std::to_string(pairs.size() - discover_index) +
                ", pair_start=" +
                    std::to_string(raw_pairs_by_kind[0]) +
                ", pair_operation=" +
                    std::to_string(raw_pairs_by_kind[1]) +
                ", pair_router=" +
                    std::to_string(raw_pairs_by_kind[2]) +
                ", pair_terminal=" +
                    std::to_string(raw_pairs_by_kind[3]) +
                ", expanded_start=" +
                    std::to_string(expanded_pairs_by_kind[0]) +
                ", expanded_operation=" +
                    std::to_string(expanded_pairs_by_kind[1]) +
                ", expanded_router=" +
                    std::to_string(expanded_pairs_by_kind[2]) +
                ", expanded_terminal=" +
                    std::to_string(expanded_pairs_by_kind[3]) +
                ", deterministic_expanded=" +
                    std::to_string(deterministic_pairs_expanded) +
                ", shared_row_pairs=" +
                    std::to_string(shared_row_pairs_expanded) +
                ", rows=" + std::to_string(rows.size()) +
                ", retained_absorptions=" +
                    std::to_string(retained_absorptions) +
                ", transitions_with_edge=" +
                    std::to_string(retained_transitions_with_edge) +
                ", transitions_with_policy_route=" +
                    std::to_string(
                        retained_transitions_with_policy_route) +
                ", deterministic_route_traces=" +
                    std::to_string(deterministic_route_traces.size()) +
                ", deterministic_router_nodes_skipped=" +
                    std::to_string(deterministic_router_nodes_skipped) +
                ", deterministic_router_edges_skipped=" +
                    std::to_string(deterministic_router_edges_skipped) +
                ", deterministic_router_cycles_retained=" +
                    std::to_string(deterministic_router_cycles_retained) +
                ", policy_state_target_match=" +
                    std::to_string(
                        retained_policy_state_matches_target) +
                ", policy_state_target_differ=" +
                    std::to_string(
                        retained_policy_state_differs_from_target) +
                ", calc_states=" +
                    std::to_string(model.calc->state_count()) +
                ", calc_owned=" +
                    std::to_string(
                        model.calc->fast_estimated_owned_bytes()) +
                ", pair_record_bytes=" +
                    std::to_string(sizeof(EvalPair)) +
                ", pair_carrier=" +
                    std::to_string(pairs.owned_bytes()) +
                ", pair_links=" +
                    std::to_string(pair_next.owned_bytes()) +
                ", row_payload=" +
                    std::to_string(row_payload_owned_bytes) +
                ", transition_record_bytes=" +
                    std::to_string(sizeof(EvalTransition)) +
                ", transition_via=" +
                    std::to_string(output.transition_via_owned_bytes) +
                ", deterministic_route_trace_payload=" +
                    std::to_string(
                        deterministic_route_trace_payload_owned_bytes) +
                ", deterministic_route_flow=" +
                    std::to_string(
                        deterministic_route_flows.capacity() *
                        sizeof(DeterministicRouteFlow)) +
                ", operation_rows=" +
                    std::to_string(
                        output.operation_row_census.materialized_rows) +
                ", operation_replayable_rows=" +
                    std::to_string(
                        output.operation_row_census.replayable_rows) +
                ", operation_row_reuses=" +
                    std::to_string(
                        output.operation_row_census.shared_row_reuses) +
                ", operation_stable_rows=" +
                    std::to_string(
                        output.operation_row_census.stable_shared_rows) +
                ", operation_unique_stable_kernels=" +
                    std::to_string(
                        output.operation_row_census.unique_stable_kernels) +
                ", operation_state_local_rows=" +
                    std::to_string(
                        output.operation_row_census.state_local_rows) +
                ", operation_direct_repeat_rows=" +
                    std::to_string(
                        output.operation_row_census.direct_repeat_rows) +
                ", operation_local_gated_route_rows=" +
                    std::to_string(
                        output.operation_row_census
                            .local_gated_route_rows) +
                ", operation_other_rows=" +
                    std::to_string(
                        output.operation_row_census.other_operation_rows) +
                ", operation_gated_kernel_rows=" +
                    std::to_string(
                        output.operation_row_census
                            .goal_progress_gated_rows) +
                ", operation_full_kernel_rows=" +
                    std::to_string(
                        output.operation_row_census.full_physical_rows) +
                ", operation_local_gated_proved_rows=" +
                    std::to_string(
                        output.operation_row_census
                            .local_gated_route_proved_rows) +
                ", operation_local_gated_shape_rejections=" +
                    std::to_string(
                        output.operation_row_census
                            .local_gated_route_shape_rejections) +
                ", operation_local_gated_condition_rejections=" +
                    std::to_string(
                        output.operation_row_census
                            .local_gated_route_condition_rejections) +
                ", operation_local_gated_target_rejections=" +
                    std::to_string(
                        output.operation_row_census
                            .local_gated_route_target_rejections) +
                ", operation_local_gated_root_rejections=" +
                    std::to_string(
                        output.operation_row_census
                            .local_gated_route_root_rejections) +
                ", operation_local_gated_full_outcomes=" +
                    std::to_string(
                        output.operation_row_census
                            .local_gated_full_outcome_entries) +
                ", operation_local_gated_full_routed=" +
                    std::to_string(
                        output.operation_row_census
                            .local_gated_full_routed_transitions) +
                ", operation_local_gated_full_outcome_payload=" +
                    std::to_string(
                        output.operation_row_census
                            .local_gated_full_outcome_payload_bytes) +
                ", operation_local_gated_full_routed_payload=" +
                    std::to_string(
                        output.operation_row_census
                            .local_gated_full_routed_payload_bytes) +
                ", operation_exact_outcomes=" +
                    std::to_string(
                        output.operation_row_census.exact_outcome_entries) +
                ", operation_outcome_payload=" +
                    std::to_string(
                        output.operation_row_census
                            .exact_outcome_payload_bytes) +
                ", operation_unique_stable_kernel_payload=" +
                    std::to_string(
                        output.operation_row_census
                            .unique_stable_kernel_payload_bytes) +
                ", operation_routed_payload=" +
                    std::to_string(
                        output.operation_row_census.routed_payload_bytes) +
                ", replay_route_token_payload=" +
                    std::to_string(
                        output.operation_row_census
                            .replay_route_token_bytes) +
                ", replay_route_result_authorities=" +
                    std::to_string(
                        output.operation_row_census
                            .replay_route_result_authorities) +
                ", projected_u32_route_tokens=" +
                    std::to_string(
                        output.operation_row_census
                            .projected_u32_route_tokens_bytes) +
                ", operation_source_edge_selections=" +
                    std::to_string(
                        output.operation_row_census.source_edge_selections) +
                ", operation_deterministic_routes=" +
                    std::to_string(
                        output.operation_row_census
                            .deterministic_route_resolutions) +
                ", route_sample_requests=" +
                    std::to_string(
                        output.operation_row_census.sampled_route_keys) +
                ", route_sample_reuses=" +
                    std::to_string(
                        output.operation_row_census
                            .sampled_route_key_reuses) +
                ", route_sample_unique=" +
                    std::to_string(route_sample_keys.size()) +
                ", source_edge_sample_calls=" +
                    std::to_string(
                        output.operation_row_census
                            .sampled_source_edge_selections) +
                ", source_edge_sample_ns=" +
                    std::to_string(
                        output.operation_row_census
                            .sampled_source_edge_ns) +
                ", deterministic_route_sample_calls=" +
                    std::to_string(
                        output.operation_row_census
                            .sampled_deterministic_routes) +
                ", deterministic_route_sample_ns=" +
                    std::to_string(
                        output.operation_row_census
                            .sampled_deterministic_route_ns) +
                ", row_completion_sample_ns=" +
                    std::to_string(
                        output.operation_row_census
                            .sampled_row_completion_ns) +
                ", operation_action_census=" +
                    operation_row_census_summary() +
                ", owned=" +
                    std::to_string(fast_estimated_owned_bytes()) +
                ")");
        }
    }

    const StrategyEdge* select_edge(
        const StrategyNode& node,
        std::uint32_t state,
        const std::vector<std::uint32_t>* offered_mods = nullptr) const {
        const std::function<bool(const CompiledCondition&)>
            evaluate_condition =
                [&](const CompiledCondition& condition) -> bool {
                    switch (condition.kind) {
                    case ConditionKind::HasUnveilOption:
                        return offered_mods != nullptr &&
                               std::any_of(
                                   condition.mod_ids.begin(),
                                   condition.mod_ids.end(),
                                   [&](const std::uint32_t mod) {
                                       return std::find(
                                                  offered_mods->begin(),
                                                  offered_mods->end(),
                                                  mod) != offered_mods->end();
                                   });
                    case ConditionKind::All:
                        return std::all_of(
                            condition.children.begin(),
                            condition.children.end(),
                            evaluate_condition);
                    case ConditionKind::Any:
                        return std::any_of(
                            condition.children.begin(),
                            condition.children.end(),
                            evaluate_condition);
                    case ConditionKind::Not:
                        return !evaluate_condition(
                            condition.children.front());
                    case ConditionKind::AtLeast:
                        return std::count_if(
                                   condition.children.begin(),
                                   condition.children.end(),
                                   evaluate_condition) >=
                               condition.min_value;
                    default:
                        return evaluate_abstract_condition(
                            condition, model.calc->session(),
                            model.calc->layout(),
                            model.calc->state(state));
                    }
                };
        const StrategyEdge* fallback_edge = nullptr;
        for (const StrategyEdge& edge : node.edges) {
            if (edge.is_default) {
                fallback_edge = &edge;
            } else if (evaluate_condition(edge.condition)) {
                return &edge;
            }
        }
        return fallback_edge;
    }

    PolicyRouteResolution& resolve_policy_route(
            const std::uint32_t state) {
        if (!compress_policy_routes ||
            compressed_policy_root == kNoId) {
            throw std::logic_error(
                "policy route resolution requested without a compressed "
                "root");
        }
        if (policy_route_cache.size() <= state) {
            policy_route_cache.resize(
                static_cast<std::size_t>(state) + 1);
        }
        PolicyRouteResolution& resolution =
            policy_route_cache[state];
        if (resolution.resolved) return resolution;

        refinement::StableKey trace;
        std::uint32_t cursor = compressed_policy_root;
        std::size_t policy_steps = 0;
        while (is_policy_route_node(cursor)) {
            if (++policy_steps > strategy->nodes.size()) {
                throw std::logic_error(
                    "compiled policy router contains a cycle");
            }
            const StrategyEdge* route_edge =
                select_edge(strategy->nodes[cursor], state);
            if (route_edge == nullptr) {
                resolution.failure_node = cursor;
                trace.push_back(0); /* no-matching-edge trace */
                trace.push_back(cursor);
                break;
            }
            trace.push_back(1); /* selected router edge */
            trace.push_back(edge_index_by_id.at(route_edge->id));
            cursor = route_edge->target;
        }
        if (resolution.failure_node == kNoId) {
            trace.push_back(2); /* resolved non-router target */
            trace.push_back(cursor);
        }
        const std::uint32_t candidate =
            static_cast<std::uint32_t>(policy_route_traces.size());
        const auto [stored, inserted] =
            policy_route_trace_by_key.emplace(
                std::move(trace), candidate);
        if (inserted) {
            policy_route_trace_payload_owned_bytes +=
                stored->first.capacity() * sizeof(std::uint64_t);
            policy_route_traces.push_back(stored->first);
            policy_route_trace_payload_owned_bytes +=
                policy_route_traces.back().capacity() *
                sizeof(std::uint64_t);
        }
        resolution.target_node = cursor;
        resolution.trace = stored->second;
        resolution.resolved = true;
        return resolution;
    }

    bool node_observes_modifier_offer(const StrategyNode& node) const {
        const std::function<bool(const CompiledCondition&)> contains =
            [&](const CompiledCondition& condition) {
                return condition.kind == ConditionKind::HasUnveilOption ||
                       std::any_of(
                           condition.children.begin(),
                           condition.children.end(), contains);
            };
        return std::any_of(
            node.edges.begin(), node.edges.end(),
            [&](const StrategyEdge& edge) {
                return !edge.is_default && contains(edge.condition);
            });
    }

    bool is_deterministic_route_authority(
            const std::uint32_t authority) const {
        return authority != kNoId &&
               authority >= strategy->nodes.size();
    }

    std::uint32_t encode_deterministic_route_authority(
            const std::uint32_t trace) const {
        const std::uint64_t encoded =
            static_cast<std::uint64_t>(strategy->nodes.size()) + trace;
        if (encoded >= kNoId) {
            throw std::length_error(
                "strategy evaluation exceeded deterministic route trace "
                "identity space");
        }
        return static_cast<std::uint32_t>(encoded);
    }

    std::uint32_t decode_deterministic_route_authority(
            const std::uint32_t authority) const {
        if (!is_deterministic_route_authority(authority)) {
            throw std::logic_error(
                "strategy evaluation route authority is not an interned "
                "deterministic trace");
        }
        const std::uint32_t trace =
            authority - static_cast<std::uint32_t>(strategy->nodes.size());
        if (trace >= deterministic_route_traces.size()) {
            throw std::logic_error(
                "strategy evaluation deterministic route trace is missing");
        }
        return trace;
    }

    bool online_deterministic_router(const std::uint32_t node) const {
        return node < strategy->nodes.size() &&
               strategy->nodes[node].kind == StrategyNodeKind::Router &&
               !node_observes_modifier_offer(strategy->nodes[node]);
    }

    static std::uint64_t deterministic_route_trace_hash(
            const refinement::StableKey& trace) {
        std::uint64_t hash = 1469598103934665603ull;
        for (const std::uint64_t token : trace) {
            for (std::uint32_t shift = 0; shift < 64; shift += 8) {
                hash ^= (token >> shift) & 0xffu;
                hash *= 1099511628211ull;
            }
        }
        return hash;
    }

    void rehash_deterministic_route_index(
            const std::size_t bucket_count) {
        if (bucket_count == 0 ||
            (bucket_count & (bucket_count - 1)) != 0) {
            throw std::logic_error(
                "strategy evaluation deterministic-route index buckets "
                "are not a power of two");
        }
        check_owned_cap(bucket_count * sizeof(std::uint32_t));
        std::vector<std::uint32_t> replacement(
            bucket_count, kNoId);
        if (deterministic_route_next.size() !=
            deterministic_route_traces.size()) {
            throw std::logic_error(
                "strategy evaluation deterministic-route index is not "
                "parallel to its traces");
        }
        for (std::uint32_t trace = 0;
             trace < deterministic_route_traces.size(); ++trace) {
            const std::size_t bucket =
                deterministic_route_trace_hash(
                    deterministic_route_traces[trace]) &
                (bucket_count - 1);
            deterministic_route_next[trace] = replacement[bucket];
            replacement[bucket] = trace;
        }
        deterministic_route_bucket_heads.swap(replacement);
    }

    void ensure_deterministic_route_index_for_insert() {
        if (deterministic_route_bucket_heads.empty()) {
            rehash_deterministic_route_index(64);
            return;
        }
        constexpr std::size_t kMaximumAverageChain = 6;
        if (deterministic_route_next.size() >=
            deterministic_route_bucket_heads.size() *
                kMaximumAverageChain) {
            if (deterministic_route_bucket_heads.size() >
                std::numeric_limits<std::size_t>::max() / 2) {
                throw std::length_error(
                    "strategy evaluation deterministic-route index is too "
                    "large");
            }
            rehash_deterministic_route_index(
                deterministic_route_bucket_heads.size() * 2);
        }
    }

    std::uint32_t intern_deterministic_route_trace() {
        ensure_deterministic_route_index_for_insert();
        const std::size_t bucket =
            deterministic_route_trace_hash(
                deterministic_route_trace_scratch) &
            (deterministic_route_bucket_heads.size() - 1);
        for (std::uint32_t candidate =
                 deterministic_route_bucket_heads[bucket];
             candidate != kNoId;
             candidate = deterministic_route_next[candidate]) {
            if (deterministic_route_traces[candidate] ==
                deterministic_route_trace_scratch) {
                return candidate;
            }
        }
        const std::uint32_t candidate = static_cast<std::uint32_t>(
            deterministic_route_traces.size());
        deterministic_route_traces.push_back(
            deterministic_route_trace_scratch);
        deterministic_route_trace_payload_owned_bytes +=
            deterministic_route_traces.back().capacity() *
            sizeof(std::uint64_t);
        deterministic_route_next.push_back(
            deterministic_route_bucket_heads[bucket]);
        deterministic_route_bucket_heads[bucket] = candidate;
        check_owned_cap();
        return candidate;
    }

    DeterministicRouteResolution resolve_deterministic_route(
            const std::uint32_t root,
            const std::uint32_t state) {
        DeterministicRouteResolution resolution;
        resolution.target_node = root;
        if (!online_deterministic_router(root)) return resolution;

        deterministic_route_trace_scratch.clear();
        deterministic_route_walk_scratch.clear();
        deterministic_route_trace_scratch.push_back(
            0x6576616c726f7574ull); /* "evalrout" */
        deterministic_route_trace_scratch.push_back(0);
        std::uint32_t cursor = root;
        while (online_deterministic_router(cursor)) {
            if (std::find(
                    deterministic_route_walk_scratch.begin(),
                    deterministic_route_walk_scratch.end(), cursor) !=
                deterministic_route_walk_scratch.end()) {
                ++deterministic_router_cycles_retained;
                resolution.target_node = root;
                return resolution;
            }
            deterministic_route_walk_scratch.push_back(cursor);
            deterministic_route_trace_scratch.push_back(cursor);
            const StrategyEdge* selected =
                select_edge(strategy->nodes[cursor], state);
            if (selected == nullptr) {
                deterministic_route_trace_scratch.push_back(0);
                resolution.failure_node = cursor;
                break;
            }
            const std::uint32_t edge =
                edge_index_by_id.at(selected->id);
            deterministic_route_trace_scratch.push_back(
                static_cast<std::uint64_t>(edge) + 1);
            cursor = selected->target;
        }

        const std::uint32_t steps = static_cast<std::uint32_t>(
            deterministic_route_walk_scratch.size());
        if (steps == 0) return resolution;
        deterministic_route_trace_scratch[1] = steps;
        deterministic_route_trace_scratch.push_back(
            resolution.failure_node == kNoId ? 1 : 0);
        deterministic_route_trace_scratch.push_back(
            resolution.failure_node == kNoId
                ? cursor
                : resolution.failure_node);

        resolution.trace = intern_deterministic_route_trace();
        deterministic_router_nodes_skipped += steps;
        for (std::uint32_t step = 0; step < steps; ++step) {
            if (deterministic_route_trace_scratch[3 + 2 * step] != 0) {
                ++deterministic_router_edges_skipped;
            }
        }
        resolution.target_node = cursor;
        return resolution;
    }

    std::uint32_t modifier_offer_action_index(
            const StrategyNode& node) const {
        std::uint32_t selected = kNoId;
        for (const StrategyEdge& edge : node.edges) {
            if (edge.is_default ||
                edge.target >= model.action_by_node.size()) {
                continue;
            }
            const std::uint32_t action =
                model.action_by_node[edge.target];
            if (action == kNoId ||
                !action_observes_modifier_offer(
                    model.calc->registry().actions[action])) {
                continue;
            }
            if (selected != kNoId && selected != action) {
                throw StrategyEvalUnsupported(
                    "strategy evaluation unsupported:\n- one modifier-offer "
                    "router selects multiple observation actions");
            }
            selected = action;
        }
        if (selected != kNoId) return selected;

        for (std::uint32_t action = 0;
             action < model.calc->registry().actions.size(); ++action) {
            if (!action_observes_modifier_offer(
                    model.calc->registry().actions[action])) {
                continue;
            }
            if (selected != kNoId) {
                return kNoId;
            }
            selected = action;
        }
        return selected;
    }

    void expand_pair(std::uint32_t pair_id) {
        const std::uint64_t owned_before_expansion =
            fast_estimated_owned_bytes();
        const std::uint32_t node_index = pairs.at(pair_id).node;
        const std::uint32_t state_id = pairs.at(pair_id).state;
        const std::uint32_t checkpoint_state_id =
            pairs.at(pair_id).checkpoint_state;
        const std::uint32_t unveil_offer_id =
            pairs.at(pair_id).unveil_offer;
        const std::vector<std::uint32_t>* active_unveil_offer =
            unveil_offer_id == kNoId
                ? nullptr
                : &unveil_offer_sets.at(unveil_offer_id);
        const StrategyNode& node = strategy->nodes.at(node_index);
        bool consumes = false;
        std::uint32_t action_index = kNoId;
        const OutcomeDistribution* shared_distribution = nullptr;
        bool release_operation_outcome = false;
        bool release_goal_progress_gated_outcome = false;
        bool census_operation_distribution = false;
        bool census_stable_shared_kernel = false;
        bool census_direct_repeat = false;
        bool census_goal_progress_gated = false;
        const LocalGatedRouteProof census_local_gated_route =
            node_index < local_gated_route_proof_by_node.size()
                ? local_gated_route_proof_by_node[node_index]
                : LocalGatedRouteProof::NotCandidate;
        std::uint64_t census_outcome_entries = 0;
        std::uint64_t census_outcome_payload_bytes = 0;
        const OutcomeDistribution* replay_distribution = nullptr;
        std::uint32_t replay_checkpoint_state = kNoId;
        std::vector<std::uint32_t> replay_route_tokens;
        bool capture_replay_routes = false;
        std::map<
            std::tuple<
                std::uint32_t, std::uint32_t, std::uint32_t,
                std::uint32_t>,
            solve_detail::WideFloat> transitions;
        std::map<
            std::tuple<
                int, std::uint32_t, std::uint32_t, std::uint32_t,
                std::uint32_t>,
            solve_detail::WideFloat> absorptions;

        const auto add_transition = [&](const std::tuple<
                                            std::uint32_t,
                                            std::uint32_t,
                                            std::uint32_t,
                                            std::uint32_t>& key,
                                        double probability) {
            auto found = transitions.find(key);
            if (found == transitions.end()) {
                ensure_transition_budget(
                    transitions.size() + absorptions.size() + 1);
                check_owned_projection(
                    owned_before_expansion,
                    (transitions.size() + absorptions.size() + 1) * 104ull);
                transitions.emplace(
                    key, solve_detail::WideFloat{probability});
            } else {
                found->second +=
                    solve_detail::WideFloat{probability};
            }
        };
        const auto add_absorption = [&](const std::tuple<
                                            int,
                                            std::uint32_t,
                                            std::uint32_t,
                                            std::uint32_t,
                                            std::uint32_t>& key,
                                        double probability) {
            auto found = absorptions.find(key);
            if (found == absorptions.end()) {
                ensure_transition_budget(
                    transitions.size() + absorptions.size() + 1);
                check_owned_projection(
                    owned_before_expansion,
                    (transitions.size() + absorptions.size() + 1) * 120ull);
                absorptions.emplace(
                    key, solve_detail::WideFloat{probability});
            } else {
                found->second +=
                    solve_detail::WideFloat{probability};
            }
        };
        const auto capture_replay_route = [&](
                const ReplayRouteKind kind,
                const std::uint32_t route_node,
                const std::uint32_t edge,
                const std::uint32_t policy_route) {
            if (!capture_replay_routes) return false;
            ensure_transition_budget(replay_route_tokens.size() + 1);
            check_owned_projection(
                owned_before_expansion,
                (replay_route_tokens.size() + 1) *
                    sizeof(std::uint32_t));
            replay_route_tokens.push_back(intern_replay_route_result({
                kind, route_node, edge, policy_route}));
            return true;
        };

        const auto route = [&](
                               std::uint32_t state,
                               double probability,
                               const std::vector<std::uint32_t>*
                                   offered_mods,
                               std::uint32_t checkpoint_state) {
            if (!(probability > 0.0)) return;
            if (!std::isfinite(probability)) {
                throw std::runtime_error(
                    "strategy evaluation found a non-finite transition");
            }
            auto& census = output.operation_row_census;
            const bool operation_route =
                node.kind == StrategyNodeKind::Operation;
            if (operation_route) ++census.source_edge_selections;
            const bool sampled_source = operation_route && census_sample(
                route_sample_key(node_index, state));
            const Clock::time_point source_started =
                sampled_source ? Clock::now() : Clock::time_point{};
            const StrategyEdge* selected =
                select_edge(node, state, offered_mods);
            if (sampled_source) {
                ++census.sampled_source_edge_selections;
                census.sampled_source_edge_ns +=
                    static_cast<std::uint64_t>(
                        std::chrono::duration_cast<std::chrono::nanoseconds>(
                            Clock::now() - source_started)
                            .count());
            }
            if (selected == nullptr) {
                if (capture_replay_route(
                        ReplayRouteKind::NoMatchingEdge,
                        node_index, kNoId, kNoId)) {
                    return;
                }
                add_absorption(
                    {static_cast<int>(EvalAbsorptionKind::NoMatchingEdge),
                     node_index, state, kNoId, kNoId},
                    probability);
                return;
            }
            const std::uint32_t edge = edge_index_by_id.at(selected->id);
            std::uint32_t target_node = selected->target;
            std::uint32_t policy_route = kNoId;
            if (compress_policy_routes &&
                target_node == compressed_policy_root) {
                policy_route = target_node;
                PolicyRouteResolution& resolution =
                    resolve_policy_route(state);
                if (resolution.failure_node != kNoId) {
                    if (capture_replay_route(
                            ReplayRouteKind::NoMatchingEdge,
                            resolution.failure_node, edge,
                            policy_route)) {
                        return;
                    }
                    add_absorption(
                        {static_cast<int>(
                             EvalAbsorptionKind::NoMatchingEdge),
                         resolution.failure_node, state, edge, policy_route},
                        probability);
                    return;
                }
                target_node = resolution.target_node;
            }
            if (policy_route == kNoId) {
                const bool deterministic =
                    online_deterministic_router(target_node);
                if (deterministic && operation_route) {
                    ++census.deterministic_route_resolutions;
                    observe_sampled_route_key(target_node, state);
                }
                const bool sampled_route =
                    deterministic && operation_route && census_sample(
                        route_sample_key(target_node, state));
                const Clock::time_point route_started =
                    sampled_route ? Clock::now() : Clock::time_point{};
                const DeterministicRouteResolution resolution =
                    resolve_deterministic_route(target_node, state);
                if (sampled_route) {
                    ++census.sampled_deterministic_routes;
                    census.sampled_deterministic_route_ns +=
                        static_cast<std::uint64_t>(
                            std::chrono::duration_cast<
                                std::chrono::nanoseconds>(
                                Clock::now() - route_started)
                                .count());
                }
                if (resolution.trace != kNoId) {
                    policy_route = encode_deterministic_route_authority(
                        resolution.trace);
                    if (resolution.failure_node != kNoId) {
                        if (capture_replay_route(
                                ReplayRouteKind::NoMatchingEdge,
                                resolution.failure_node, edge,
                                policy_route)) {
                            return;
                        }
                        add_absorption(
                            {static_cast<int>(
                                 EvalAbsorptionKind::NoMatchingEdge),
                             resolution.failure_node, state, edge,
                             policy_route},
                            probability);
                        return;
                    }
                    target_node = resolution.target_node;
                }
            }
            const StrategyNode& target = strategy->nodes.at(target_node);
            if (target.kind == StrategyNodeKind::Terminal) {
                if (capture_replay_route(
                        ReplayRouteKind::Terminal,
                        target_node, edge, policy_route)) {
                    return;
                }
                add_absorption(
                    {static_cast<int>(EvalAbsorptionKind::Terminal),
                     target_node, state, edge, policy_route},
                    probability);
                return;
            }
            const std::uint32_t target_unveil_offer =
                offered_mods == nullptr
                    ? kNoId
                    : intern_unveil_offer(*offered_mods);
            const std::uint32_t target_pair =
                intern_pair(
                    target_node, state, target_unveil_offer,
                    checkpoint_state);
            if (capture_replay_route(
                    ReplayRouteKind::Transition,
                    target_node, edge, policy_route)) {
                return;
            }
            add_transition(
                {target_pair, edge, policy_route, state}, probability);
        };

        if (node.kind != StrategyNodeKind::Operation) {
            if (!node_observes_modifier_offer(node)) {
                route(
                    state_id, 1.0, active_unveil_offer,
                    checkpoint_state_id);
            } else if (active_unveil_offer != nullptr) {
                route(
                    state_id, 1.0, active_unveil_offer,
                    checkpoint_state_id);
            } else {
                const std::uint32_t observed_action =
                    modifier_offer_action_index(node);
                if (observed_action == kNoId) {
                    throw StrategyEvalUnsupported(
                        "strategy evaluation unsupported:\n- node '" +
                        node.id +
                        "' observes modifier offers but has no unique "
                        "admitted observation action");
                }
                const ActionDescriptor& action =
                    model.calc->registry().actions.at(observed_action);
                if (!action_legal(
                        model.calc->session(), action,
                        model.calc->state(state_id))) {
                    route(
                        state_id, 1.0, nullptr,
                        checkpoint_state_id);
                } else {
                    const OutcomeDistribution& outcomes =
                        exact_outcomes(state_id, observed_action);
                    if (!outcomes.supported) {
                        throw StrategyEvalUnsupported(
                            "strategy evaluation unsupported:\n- node '" +
                            node.id +
                            "' has no exact modifier-offer distribution for "
                            "a reachable state");
                    }
                    ensure_state_limit();
                    if (!outcomes.applicable ||
                        outcomes.choice_groups.empty()) {
                        route(
                            state_id, 1.0, nullptr,
                            checkpoint_state_id);
                    } else {
                        double distribution_mass = 0.0;
                        for (const OutcomeChoiceGroup& group :
                             outcomes.choice_groups) {
                            std::vector<std::uint32_t> offered_mods;
                            for (const OutcomeChoiceOption& option :
                                 outcomes.choice_options) {
                                if (std::find(
                                        group.states.begin(),
                                        group.states.end(),
                                        option.state) !=
                                    group.states.end()) {
                                    offered_mods.push_back(option.mod_id);
                                }
                            }
                            std::sort(
                                offered_mods.begin(), offered_mods.end());
                            offered_mods.erase(
                                std::unique(
                                    offered_mods.begin(),
                                    offered_mods.end()),
                                offered_mods.end());
                            distribution_mass += group.probability;
                            route(
                                state_id, group.probability,
                                &offered_mods,
                                checkpoint_state_id);
                        }
                        if (std::fabs(distribution_mass - 1.0) > 1e-9) {
                            throw std::runtime_error(
                                "strategy evaluation modifier-offer "
                                "distribution does not sum to one at node '" +
                                node.id + "'");
                        }
                    }
                    if (!outcomes.stable_shared_kernel) {
                        model.calc->release_outcome(
                            state_id, observed_action, false);
                    }
                }
            }
        } else {
            const ResolvedStrategyOperation& resolved =
                model.operation_by_node.at(node_index);
            if (resolved.kind ==
                ResolvedStrategyOperationKind::Bestiary) {
                const BestiaryActionDescriptor& action =
                    model.calc->session().data->bestiary_actions.at(
                        resolved.descriptor_index);
                if (!bestiary_action_legal(
                        action, model.calc->state(state_id),
                        checkpoint_state_id)) {
                    add_absorption(
                        {static_cast<int>(
                             EvalAbsorptionKind::ActionNotApplied),
                         node_index, state_id, kNoId, kNoId},
                        1.0);
                } else {
                    consumes = true;
                    std::uint32_t successor_state = state_id;
                    std::uint32_t successor_checkpoint =
                        checkpoint_state_id;
                    if (action.checkpoint_effect ==
                        BestiaryCheckpointEffect::Create) {
                        successor_checkpoint = state_id;
                    } else {
                        successor_state = checkpoint_state_id;
                        successor_checkpoint = kNoId;
                    }
                    route(
                        successor_state, 1.0, nullptr,
                        successor_checkpoint);
                }
            } else {
                action_index = resolved.descriptor_index;
                const ActionDescriptor& action =
                    model.calc->registry().actions.at(action_index);
                if (!action_legal(
                        model.calc->session(), action,
                        model.calc->state(state_id))) {
                    add_absorption(
                        {static_cast<int>(
                             EvalAbsorptionKind::ActionNotApplied),
                         node_index, state_id, kNoId, kNoId},
                        1.0);
                } else {
                    consumes = true;
                    const OutcomeDistribution* selected_outcomes = nullptr;
                    const bool may_repeat_directly = std::any_of(
                        node.edges.begin(), node.edges.end(),
                        [&](const StrategyEdge& edge) {
                            return edge.target == node_index;
                        });
                    census_direct_repeat = may_repeat_directly;
                    /* A compiled shared gated-renewal region deliberately
                     * observes only whether this action made goal progress:
                     * every zero-progress physical outcome immediately
                     * repeats the same operation. Use the calculator's exact
                     * gated kernel only for that direct self-loop. Local
                     * gated routers remain structural telemetry: the real
                     * five-goal control proved that substituting the compact
                     * normalization there does not preserve exact cost bits. */
                    if (may_repeat_directly) {
                        const OutcomeDistribution& gated_candidate =
                            exact_outcomes(
                                state_id, action_index, true);
                        if (gated_candidate.supported &&
                            gated_candidate.goal_progress_gated &&
                            gated_candidate.gated_retry_probability > 0.0 &&
                            gated_candidate.gated_retry_state != kNoId &&
                            gated_candidate.gated_retry_state <
                                model.calc->state_count()) {
                            const StrategyEdge* retry = select_edge(
                                node,
                                gated_candidate.gated_retry_state,
                                nullptr);
                            const bool retry_is_exact =
                                retry != nullptr &&
                                retry->target == node_index;
                            if (retry_is_exact) {
                                selected_outcomes = &gated_candidate;
                            }
                        }
                    }
                    if (selected_outcomes == nullptr) {
                        selected_outcomes =
                            &exact_outcomes(state_id, action_index, false);
                    }
                    const OutcomeDistribution& outcomes = *selected_outcomes;
                    if (!outcomes.supported) {
                        throw StrategyEvalUnsupported(
                            "strategy evaluation unsupported:\n- node '" +
                            node.id + "' operation '" + action.id +
                            "' has no exact distribution for a reachable "
                            "state");
                    }
                    if (!outcomes.applicable) {
                        consumes = false;
                        add_absorption(
                            {static_cast<int>(
                                 EvalAbsorptionKind::ActionNotApplied),
                             node_index, state_id, kNoId, kNoId},
                            1.0);
                        if (!outcomes.stable_shared_kernel) {
                            model.calc->release_outcome(
                                state_id, action_index,
                                outcomes.goal_progress_gated);
                        }
                    } else {
                        ensure_state_limit();
                        census_operation_distribution = true;
                        census_goal_progress_gated =
                            outcomes.goal_progress_gated;
                        census_stable_shared_kernel =
                            outcomes.stable_shared_kernel;
                        census_outcome_entries =
                            action_observes_modifier_offer(action)
                                ? 1
                                : outcomes.entries.size();
                        census_outcome_payload_bytes =
                            outcome_payload_bytes(outcomes);
                        if (outcomes.stable_shared_kernel) {
                            const auto shared = row_by_distribution.find(
                                {node_index, checkpoint_state_id, &outcomes});
                            if (shared != row_by_distribution.end()) {
                                EvalPair& pair = pairs.at(pair_id);
                                pair.consumes = consumes;
                                pair.row = shared->second;
                                auto& total = output.operation_row_census;
                                ++total.shared_row_reuses;
                                total.projected_u32_route_tokens_bytes =
                                    total.exact_outcome_entries *
                                    sizeof(std::uint32_t);
                                if (action_index <
                                    operation_row_census_by_action.size()) {
                                    ++operation_row_census_by_action
                                          [action_index]
                                              .shared_row_reuses;
                                }
                                record_expanded_pair(pair_id, rows.at(pair.row),
                                                     true);
                                return;
                            }
                            shared_distribution = &outcomes;
                        } else {
                            release_operation_outcome = true;
                            release_goal_progress_gated_outcome =
                                outcomes.goal_progress_gated;
                        }
                        double distribution_mass = 0.0;
                        const std::uint32_t successor_checkpoint =
                            resolved.kind ==
                                    ResolvedStrategyOperationKind::Restart
                                ? kNoId
                                : checkpoint_state_id;
                        if (action_observes_modifier_offer(action)) {
                            if (active_unveil_offer == nullptr ||
                                std::find(active_unveil_offer->begin(),
                                          active_unveil_offer->end(),
                                          node.action.mod_id) ==
                                    active_unveil_offer->end()) {
                                throw std::logic_error(
                                    "authored modifier selection is not "
                                    "present "
                                    "in the sampled offer carried to node '" +
                                    node.id + "'");
                            }
                            const auto selected = std::find_if(
                                outcomes.choice_options.begin(),
                                outcomes.choice_options.end(),
                                [&](const OutcomeChoiceOption& option) {
                                    return option.mod_id == node.action.mod_id;
                                });
                            if (selected == outcomes.choice_options.end()) {
                                throw std::logic_error(
                                    "authored modifier selection is absent "
                                    "from "
                                    "its reachable exact offer vocabulary at "
                                    "node '" +
                                    node.id + "'");
                            }
                            const std::uint32_t successor =
                                selected->actual_state != kNoId
                                    ? selected->actual_state
                                    : selected->state;
                            distribution_mass = 1.0;
                            route(successor, 1.0, nullptr,
                                  successor_checkpoint);
                        } else {
                            if (outcomes.stable_shared_kernel) {
                                replay_distribution = &outcomes;
                                replay_checkpoint_state = successor_checkpoint;
                                check_owned_projection(
                                    owned_before_expansion,
                                    outcomes.entries.size() *
                                        sizeof(std::uint32_t));
                                replay_route_tokens.reserve(
                                    outcomes.entries.size());
                                capture_replay_routes = true;
                            }
                            for (const OutcomeEntry& outcome :
                                 outcomes.entries) {
                                distribution_mass += outcome.probability;
                                route(outcome.state, outcome.probability,
                                      nullptr, successor_checkpoint);
                            }
                            capture_replay_routes = false;
                        }
                        if (std::fabs(distribution_mass - 1.0) > 1e-9) {
                            throw std::runtime_error(
                                "strategy evaluation action distribution does "
                                "not sum to one at node '" +
                                node.id + "'");
                        }
                    }
                }
            }
        }

        const bool sampled_row_completion =
            node.kind == StrategyNodeKind::Operation &&
            census_sample(route_sample_key(node_index, state_id));
        const Clock::time_point row_completion_started =
            sampled_row_completion ? Clock::now() : Clock::time_point{};
        EvalPair& pair = pairs.at(pair_id);
        pair.consumes = consumes;
        EvalRow row;
        solve_detail::WideFloat row_mass = 0.0;
        if (replay_distribution != nullptr) {
            if (replay_route_tokens.size() !=
                replay_distribution->entries.size()) {
                throw std::logic_error(
                    "strategy evaluation replay route count does not match "
                    "its stable kernel");
            }
            row.replay_distribution = replay_distribution;
            row.replay_checkpoint_state = replay_checkpoint_state;
            row.replay_route_tokens = std::move(replay_route_tokens);
            row_mass = solve_detail::WideFloat{1.0};
        }
        row.transitions.reserve(transitions.size());
        for (const auto& [key, probability] : transitions) {
            row.transitions.push_back(
                {std::get<0>(key), probability.value(),
                 std::get<1>(key),
                 std::get<2>(key), std::get<3>(key)});
            const EvalTransition& transition = row.transitions.back();
            row_mass += solve_detail::WideFloat{transition.probability};
            if (transition.edge != kNoId) {
                ++retained_transitions_with_edge;
            }
            if (transition.policy_route != kNoId) {
                ++retained_transitions_with_policy_route;
            }
            if (transition.target < pairs.size() &&
                pairs[transition.target].state == transition.policy_state) {
                ++retained_policy_state_matches_target;
            } else {
                ++retained_policy_state_differs_from_target;
            }
        }
        row.absorptions.reserve(absorptions.size());
        for (const auto& [key, probability] : absorptions) {
            row.absorptions.push_back(
                {static_cast<EvalAbsorptionKind>(std::get<0>(key)),
                  std::get<1>(key), std::get<2>(key),
                  probability.value(),
                  std::get<3>(key), std::get<4>(key)});
            row_mass += solve_detail::WideFloat{
                row.absorptions.back().probability};
        }
        std::uint64_t replay_transition_count = 0;
        std::uint64_t replay_absorption_count = 0;
        for (const std::uint32_t token : row.replay_route_tokens) {
            const ReplayRouteResult& result =
                replay_route_results.at(token);
            if (result.kind == ReplayRouteKind::Transition) {
                ++replay_transition_count;
                if (result.edge != kNoId) {
                    ++retained_transitions_with_edge;
                }
                if (result.policy_route != kNoId) {
                    ++retained_transitions_with_policy_route;
                }
                ++retained_policy_state_matches_target;
            } else {
                ++replay_absorption_count;
            }
        }
        retained_absorptions +=
            row.absorptions.size() + replay_absorption_count;
        if (std::fabs(row_mass.value() - 1.0) > 1e-9) {
            throw std::runtime_error(
                "strategy evaluation transition row does not sum to one at "
                "node '" + node.id + "'");
        }
        stored_transitions +=
            row.transitions.size() + row.absorptions.size() +
            row.replay_route_tokens.size();
        row_payload_owned_bytes +=
            row.transitions.capacity() * sizeof(EvalTransition) +
            row.transition_via.capacity() * sizeof(std::uint32_t) +
            row.absorptions.capacity() * sizeof(EvalAbsorption) +
            row.replay_route_tokens.capacity() * sizeof(std::uint32_t);
        if (node.kind == StrategyNodeKind::Operation) {
            auto& total = output.operation_row_census;
            const std::uint64_t row_routed_transitions =
                row.transitions.size() + replay_transition_count;
            const std::uint64_t row_routed_payload_bytes =
                row.transitions.capacity() * sizeof(EvalTransition) +
                row.absorptions.capacity() * sizeof(EvalAbsorption) +
                row.replay_route_tokens.capacity() *
                    sizeof(std::uint32_t);
            ++total.materialized_rows;
            if (census_direct_repeat) {
                ++total.direct_repeat_rows;
            } else if (census_local_gated_route !=
                       LocalGatedRouteProof::NotCandidate) {
                ++total.local_gated_route_rows;
            } else {
                ++total.other_operation_rows;
            }
            if (census_goal_progress_gated) {
                ++total.goal_progress_gated_rows;
            } else if (census_operation_distribution) {
                ++total.full_physical_rows;
            }
            switch (census_local_gated_route) {
            case LocalGatedRouteProof::Proved:
                ++total.local_gated_route_proved_rows;
                break;
            case LocalGatedRouteProof::ShapeRejected:
                ++total.local_gated_route_shape_rejections;
                break;
            case LocalGatedRouteProof::ConditionRejected:
                ++total.local_gated_route_condition_rejections;
                break;
            case LocalGatedRouteProof::TargetRejected:
                ++total.local_gated_route_target_rejections;
                break;
            case LocalGatedRouteProof::RootRejected:
                ++total.local_gated_route_root_rejections;
                break;
            case LocalGatedRouteProof::NotCandidate:
                break;
            }
            if (row.replayable()) {
                ++total.replayable_rows;
                total.replay_route_token_bytes +=
                    row.replay_route_tokens.capacity() *
                    sizeof(std::uint32_t);
            }
            total.routed_transitions += row_routed_transitions;
            total.absorptions +=
                row.absorptions.size() + replay_absorption_count;
            total.routed_payload_bytes += row_routed_payload_bytes;
            if (census_local_gated_route !=
                    LocalGatedRouteProof::NotCandidate &&
                !census_goal_progress_gated &&
                census_operation_distribution) {
                total.local_gated_full_outcome_entries +=
                    census_outcome_entries;
                total.local_gated_full_routed_transitions +=
                    row_routed_transitions;
                total.local_gated_full_outcome_payload_bytes +=
                    census_outcome_payload_bytes;
                total.local_gated_full_routed_payload_bytes +=
                    row_routed_payload_bytes;
            }
            if (census_operation_distribution) {
                total.exact_outcome_entries += census_outcome_entries;
                total.exact_outcome_payload_bytes +=
                    census_outcome_payload_bytes;
                if (census_stable_shared_kernel) {
                    ++total.stable_shared_rows;
                    if (std::find(
                            census_stable_distributions.begin(),
                            census_stable_distributions.end(),
                            shared_distribution) ==
                        census_stable_distributions.end()) {
                        census_stable_distributions.push_back(
                            shared_distribution);
                        ++total.unique_stable_kernels;
                        total.unique_stable_kernel_payload_bytes +=
                            census_outcome_payload_bytes;
                    }
                } else {
                    ++total.state_local_rows;
                }
            }
            total.projected_u32_route_tokens_bytes =
                total.exact_outcome_entries * sizeof(std::uint32_t);
            if (action_index < operation_row_census_by_action.size()) {
                OperationRowActionCensus& action_census =
                    operation_row_census_by_action[action_index];
                ++action_census.materialized_rows;
                if (census_direct_repeat) {
                    ++action_census.direct_repeat_rows;
                } else if (census_local_gated_route !=
                           LocalGatedRouteProof::NotCandidate) {
                    ++action_census.local_gated_route_rows;
                } else {
                    ++action_census.other_operation_rows;
                }
                if (census_goal_progress_gated) {
                    ++action_census.goal_progress_gated_rows;
                } else if (census_operation_distribution) {
                    ++action_census.full_physical_rows;
                }
                if (census_local_gated_route ==
                    LocalGatedRouteProof::Proved) {
                    ++action_census.local_gated_route_proved_rows;
                }
                action_census.routed_transitions +=
                    row_routed_transitions;
                action_census.absorptions +=
                    row.absorptions.size() + replay_absorption_count;
                if (census_local_gated_route !=
                        LocalGatedRouteProof::NotCandidate &&
                    !census_goal_progress_gated &&
                    census_operation_distribution) {
                    action_census.local_gated_full_outcome_entries +=
                        census_outcome_entries;
                    action_census.local_gated_full_routed_transitions +=
                        row_routed_transitions;
                    action_census.local_gated_full_outcome_payload_bytes +=
                        census_outcome_payload_bytes;
                    action_census.local_gated_full_routed_payload_bytes +=
                        row_routed_payload_bytes;
                }
                if (census_operation_distribution) {
                    action_census.exact_outcome_entries +=
                        census_outcome_entries;
                    action_census.exact_outcome_payload_bytes +=
                        census_outcome_payload_bytes;
                    if (census_stable_shared_kernel) {
                        ++action_census.stable_shared_rows;
                    } else {
                        ++action_census.state_local_rows;
                    }
                }
            }
        }
        if (sampled_row_completion) {
            output.operation_row_census.sampled_row_completion_ns +=
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        Clock::now() - row_completion_started)
                        .count());
        }
        pair.row = static_cast<std::uint32_t>(rows.size());
        record_expanded_pair(pair_id, row, false);
        rows.push_back(std::move(row));
        if (shared_distribution != nullptr) {
            row_by_distribution.emplace(
                std::make_tuple(
                    node_index, checkpoint_state_id,
                    shared_distribution),
                pair.row);
        }
        if (release_operation_outcome) {
            model.calc->release_outcome(
                state_id, action_index,
                release_goal_progress_gated_outcome);
        }
    }

    void append_unveil_offer(
        refinement::StableKey& key,
        const std::uint32_t offer) const {
        if (offer == kNoId) {
            key.push_back(0);
            return;
        }
        key.push_back(1);
        const std::vector<std::uint32_t>& mods =
            unveil_offer_sets.at(offer);
        key.push_back(static_cast<std::uint64_t>(mods.size()));
        key.insert(key.end(), mods.begin(), mods.end());
    }

    void append_compressed_policy_trace(
        refinement::StableKey& key,
        const std::uint32_t root,
        const std::uint32_t state) const {
        append_optional_u32(key, root);
        if (root == kNoId) return;
        if (state == kNoId) {
            throw std::logic_error(
                "compressed policy trace has no exact state");
        }
        if (is_deterministic_route_authority(root)) {
            const std::uint32_t trace =
                decode_deterministic_route_authority(root);
            /* The interned key contains every skipped node, exact selected
             * edge, and resolved/failure endpoint. Equality is full-key
             * equality; the compact trace id is only its retained handle. */
            key.push_back(4);
            key.push_back(trace);
            return;
        }
        if (root != compressed_policy_root ||
            state >= policy_route_cache.size()) {
            throw std::logic_error(
                "compressed policy trace is outside its route cache");
        }
        const PolicyRouteResolution& resolution =
            policy_route_cache[state];
        if (!resolution.resolved ||
            resolution.trace >= policy_route_traces.size()) {
            throw std::logic_error(
                "compressed policy trace was not resolved during "
                "discovery");
        }
        /* Trace ids are interned only after full edge-path equality. The
         * partition needs equality, not a second copy of every path token. */
        key.push_back(3);
        key.push_back(resolution.trace);
    }

    refinement::StableKey raw_pair_stable_key(
        const EvalPair& pair) const {
        refinement::StableKey key{
            0x6576616c70616972ull, /* "evalpair" */
            pair.node};
        if (pair.state == kNoId ||
            pair.state >= model.calc->state_count()) {
            throw std::logic_error(
                "strategy evaluation pair has no exact semantic state");
        }
        append_stable_tokens(
            key,
            exact_abstract_state_key(
                model.calc->state(pair.state), kNoId));
        append_optional_u32(key, pair.checkpoint_state);
        if (pair.checkpoint_state != kNoId) {
            if (pair.checkpoint_state >= model.calc->state_count()) {
                throw std::logic_error(
                    "strategy evaluation pair has an invalid checkpoint "
                    "state");
            }
            append_stable_tokens(
                key,
                exact_abstract_state_key(
                    model.calc->state(pair.checkpoint_state), kNoId));
        }
        append_unveil_offer(key, pair.unveil_offer);
        return key;
    }

    refinement::StableKey pair_observation_key(
        const EvalPair& pair) const {
        return node_observation_key(pair.node, pair.state);
    }

    bool pair_is_operation(const EvalPair& pair) const {
        if (pair.node >= strategy->nodes.size()) {
            throw std::logic_error(
                "strategy evaluation pair has no compiled node");
        }
        return strategy->nodes[pair.node].kind ==
               StrategyNodeKind::Operation;
    }

    std::uint32_t pair_action(const EvalPair& pair) const {
        if (!pair_is_operation(pair)) return kNoId;
        const ResolvedStrategyOperation& operation =
            model.operation_by_node.at(pair.node);
        return operation.kind == ResolvedStrategyOperationKind::Bestiary
            ? kNoId
            : operation.descriptor_index;
    }

    refinement::StableKey pair_immediate_key(
        const EvalPair& pair) const {
        const bool operation = pair_is_operation(pair);
        refinement::StableKey key{
            0x6576616c696d6d31ull, /* "evalimm1" */
            pair.node,
            operation ? 1u : 0u,
            pair.consumes ? 1u : 0u};
        append_optional_u32(key, pair_action(pair));
        append_unveil_offer(key, pair.unveil_offer);
        return key;
    }

    refinement::StableKey transition_partition_label(
        const EvalTransition& transition,
        const solve_detail::SegmentedVector<EvalPair>& carrier) const {
        refinement::StableKey key{
            0x6576616c74726e31ull}; /* "evaltrn1" */
        append_optional_u32(key, transition.edge);
        append_unveil_offer(
            key, carrier.at(transition.target).unveil_offer);
        append_compressed_policy_trace(
            key, transition.policy_route,
            transition.policy_state);
        return key;
    }

    refinement::StableKey transition_partition_label(
        const EvalTransition& transition) const {
        return transition_partition_label(transition, pairs);
    }

    refinement::StableKey absorption_partition_label(
        const EvalAbsorption& absorption) const {
        refinement::StableKey key{
            0x6576616c61627331ull, /* "evalabs1" */
            static_cast<std::uint64_t>(absorption.kind),
            absorption.node};
        append_optional_u32(key, absorption.edge);
        if (absorption.kind == EvalAbsorptionKind::Terminal) {
            key.push_back(
                static_cast<std::uint64_t>(
                    strategy->nodes.at(absorption.node)
                        .terminal_kind));
        }
        append_compressed_policy_trace(
            key, absorption.policy_route, absorption.state);
        return key;
    }

    solve_detail::CooperativeTask<bool> refine_pair_graph() {
        using refinement::ClosedPartitionArc;
        using refinement::ClosedPartitionLimits;
        using refinement::ClosedPartitionNode;
        using refinement::ClosedPartitionResult;
        using refinement::ClosedPartitionStatus;

        if (pairs.empty()) co_return true;
        co_await solve_detail::CooperativeCheckpoint{};
        output.raw_pairs_discovered =
            static_cast<std::uint32_t>(pairs.size());

        const auto saturated_add = [](
                const std::uint64_t left,
                const std::uint64_t right) {
            return right >
                           std::numeric_limits<std::uint64_t>::max() -
                               left
                       ? std::numeric_limits<std::uint64_t>::max()
                       : left + right;
        };
        const auto saturated_product = [](
                const std::size_t count,
                const std::size_t width) {
            return width != 0 &&
                           count >
                               std::numeric_limits<std::uint64_t>::max() /
                                   width
                       ? std::numeric_limits<std::uint64_t>::max()
                       : static_cast<std::uint64_t>(count) * width;
        };
        const auto stable_key_bytes =
            [&](const refinement::StableKey& key) {
                return saturated_product(
                    key.capacity(), sizeof(std::uint64_t));
            };

        pair_refinement_peak_before = peak_owned_bytes_value;
        pair_refinement_identity_graph_intact = true;

        /* Materializing a second closed graph becomes a single long WASM
         * partition unit well before it becomes a memory problem. Replay the
         * retained exact rows from this boundary so certification stays
         * cooperative without changing the partition authority. */
        const bool replayable_operation_rows = has_replayable_rows(rows);
        const bool use_replay_partition =
            replayable_operation_rows || pairs.size() >= 4096;
        std::vector<ClosedPartitionNode> closed;
        std::uint64_t stable_keys_owned_bytes = 0;
        if (!use_replay_partition) {

        /*
         * Small graphs use the materialized shared partition path. Large
         * graphs replay exact retained evaluator rows so the partition proof
         * never owns a second complete carrier graph.
         */
        const std::uint64_t projected_closed_outer =
            saturated_product(
                pairs.size(), sizeof(ClosedPartitionNode));
        check_owned_cap(projected_closed_outer);
        closed.reserve(pairs.size());
        std::uint64_t closed_owned_bytes =
            saturated_product(
                closed.capacity(), sizeof(ClosedPartitionNode));
        check_owned_cap(closed_owned_bytes);
        using KeyAuthorityMap =
            std::map<std::uint64_t, std::vector<std::uint32_t>>;
        KeyAuthorityMap observation_authority_by_hash;
        KeyAuthorityMap immediate_authority_by_hash;
        std::map<refinement::StableKey, std::uint32_t>
            partition_label_by_key;
        const std::uint64_t authority_map_node_bytes =
            sizeof(KeyAuthorityMap::value_type) + 3 * sizeof(void*);
        const std::uint64_t label_map_node_bytes =
            sizeof(decltype(partition_label_by_key)::value_type) +
            3 * sizeof(void*);
        const auto intern_partition_label = [&](
                refinement::StableKey key) {
            const std::uint32_t candidate =
                static_cast<std::uint32_t>(
                    partition_label_by_key.size());
            const auto [stored, inserted] =
                partition_label_by_key.emplace(
                    std::move(key), candidate);
            if (inserted) {
                stable_keys_owned_bytes = saturated_add(
                    stable_keys_owned_bytes,
                    label_map_node_bytes);
                stable_keys_owned_bytes = saturated_add(
                    stable_keys_owned_bytes,
                    stable_key_bytes(stored->first));
            }
            return refinement::StableKey{
                0x6576616c6c626c31ull, /* "evallbl1" */
                stored->second};
        };
        const auto intern_partition_key = [&]<typename AuthorityKey>(
                refinement::StableKey key,
                KeyAuthorityMap& authorities,
                refinement::StableKey& stored_key,
                std::optional<std::uint32_t>& source,
                AuthorityKey&& authority_key) {
            std::uint64_t hash = 1469598103934665603ull;
            for (const std::uint64_t token : key) {
                hash ^= token;
                hash *= 1099511628211ull;
            }
            auto [bucket, inserted] =
                authorities.try_emplace(hash);
            if (inserted) {
                stable_keys_owned_bytes = saturated_add(
                    stable_keys_owned_bytes,
                    authority_map_node_bytes);
            }
            for (const std::uint32_t candidate : bucket->second) {
                if (authority_key(closed.at(candidate)) == key) {
                    source = candidate;
                    return;
                }
            }
            const std::size_t capacity_before =
                bucket->second.capacity();
            bucket->second.push_back(
                static_cast<std::uint32_t>(closed.size()));
            stable_keys_owned_bytes = saturated_add(
                stable_keys_owned_bytes,
                saturated_product(
                    bucket->second.capacity() - capacity_before,
                    sizeof(std::uint32_t)));
            stored_key = std::move(key);
            closed_owned_bytes = saturated_add(
                closed_owned_bytes,
                stable_key_bytes(stored_key));
        };
        for (std::uint32_t pair_id = 0;
             pair_id < pairs.size(); ++pair_id) {
            const EvalPair& pair = pairs[pair_id];
            ClosedPartitionNode node;
            /* The closed partition needs only a unique deterministic node
             * order here. Full semantic pair identity remains authoritative
             * in `pairs` and is recomputed below when choosing each class's
             * representative; the partition result's member keys are not
             * consumed by the evaluator. */
            node.stable_key = refinement::StableKey{
                0x6576616c72617731ull, /* "evalraw1" */
                pair_id};
            closed_owned_bytes = saturated_add(
                closed_owned_bytes,
                stable_key_bytes(node.stable_key));
            check_owned_cap(saturated_add(
                closed_owned_bytes, stable_keys_owned_bytes));
            intern_partition_key(
                pair_observation_key(pair),
                observation_authority_by_hash,
                node.observation_key,
                node.observation_source,
                [](const ClosedPartitionNode& authority)
                    -> const refinement::StableKey& {
                    return authority.observation_key;
                });
            check_owned_cap(saturated_add(
                closed_owned_bytes, stable_keys_owned_bytes));
            intern_partition_key(
                pair_immediate_key(pair),
                immediate_authority_by_hash,
                node.immediate_key,
                node.immediate_source,
                [](const ClosedPartitionNode& authority)
                    -> const refinement::StableKey& {
                    return authority.immediate_key;
                });
            check_owned_cap(saturated_add(
                closed_owned_bytes, stable_keys_owned_bytes));
            const EvalRow& row = pair_row(pair_id);
            const std::size_t arc_count =
                row.transitions.size() + row.absorptions.size();
            check_owned_cap(saturated_add(
                saturated_add(
                    closed_owned_bytes, stable_keys_owned_bytes),
                saturated_product(
                    arc_count, sizeof(ClosedPartitionArc))));
            node.arcs.reserve(
                arc_count);
            closed_owned_bytes = saturated_add(
                closed_owned_bytes,
                saturated_product(
                    node.arcs.capacity(),
                    sizeof(ClosedPartitionArc)));
            check_owned_cap(saturated_add(
                closed_owned_bytes, stable_keys_owned_bytes));
            for (const EvalTransition& transition :
                 row.transitions) {
                refinement::StableKey label =
                    intern_partition_label(
                        transition_partition_label(transition));
                const std::uint64_t label_bytes =
                    stable_key_bytes(label);
                check_owned_cap(saturated_add(
                    saturated_add(
                        closed_owned_bytes,
                        stable_keys_owned_bytes),
                    label_bytes));
                node.arcs.push_back(ClosedPartitionArc{
                    std::move(label),
                    std::optional<std::uint32_t>{
                        transition.target},
                    transition.probability});
                closed_owned_bytes = saturated_add(
                    closed_owned_bytes,
                    stable_key_bytes(node.arcs.back().label));
                check_owned_cap(saturated_add(
                    closed_owned_bytes, stable_keys_owned_bytes));
            }
            for (const EvalAbsorption& absorption :
                 row.absorptions) {
                refinement::StableKey label =
                    intern_partition_label(
                        absorption_partition_label(absorption));
                const std::uint64_t label_bytes =
                    stable_key_bytes(label);
                check_owned_cap(saturated_add(
                    saturated_add(
                        closed_owned_bytes,
                        stable_keys_owned_bytes),
                    label_bytes));
                node.arcs.push_back(ClosedPartitionArc{
                    std::move(label),
                    std::nullopt,
                    absorption.probability});
                closed_owned_bytes = saturated_add(
                    closed_owned_bytes,
                    stable_key_bytes(node.arcs.back().label));
                check_owned_cap(saturated_add(
                    closed_owned_bytes, stable_keys_owned_bytes));
            }
            closed.push_back(std::move(node));
            check_owned_cap(saturated_add(
                closed_owned_bytes, stable_keys_owned_bytes));
            if ((pair_id & 31u) == 31u) {
                co_await solve_detail::CooperativeCheckpoint{
                    saturated_add(
                        closed_owned_bytes,
                        stable_keys_owned_bytes)};
            }
        }
        KeyAuthorityMap{}.swap(observation_authority_by_hash);
        KeyAuthorityMap{}.swap(immediate_authority_by_hash);
        decltype(partition_label_by_key){}.swap(
            partition_label_by_key);
        stable_keys_owned_bytes = 0;
        }

        std::vector<std::uint32_t> replay_observation_id;
        std::vector<std::uint32_t> replay_immediate_id;
        std::vector<std::optional<std::uint32_t>> replay_arc_source;
        std::uint64_t replay_key_cache_owned_bytes = 0;
        if (use_replay_partition) {
            replay_observation_id.resize(pairs.size());
            replay_immediate_id.resize(pairs.size());
            replay_arc_source.resize(pairs.size());
            std::vector<std::uint32_t> first_pair_by_row(
                rows.size(), kNoId);
            std::map<refinement::StableKey, std::uint32_t>
                observation_ids;
            std::map<refinement::StableKey, std::uint32_t>
                immediate_ids;
            for (std::uint32_t pair_id = 0;
                 pair_id < pairs.size(); ++pair_id) {
                const EvalPair& pair = pairs[pair_id];
                const auto [observation, observation_inserted] =
                    observation_ids.emplace(
                        pair_observation_key(pair),
                        static_cast<std::uint32_t>(
                            observation_ids.size()));
                (void)observation_inserted;
                replay_observation_id[pair_id] =
                    observation->second;
                const auto [immediate, immediate_inserted] =
                    immediate_ids.emplace(
                        pair_immediate_key(pair),
                        static_cast<std::uint32_t>(
                            immediate_ids.size()));
                (void)immediate_inserted;
                replay_immediate_id[pair_id] = immediate->second;
                const std::uint32_t row = pair.row;
                if (row >= first_pair_by_row.size()) {
                    throw std::logic_error(
                        "strategy evaluation pair has no retained row");
                }
                if (first_pair_by_row[row] == kNoId) {
                    first_pair_by_row[row] = pair_id;
                } else {
                    replay_arc_source[pair_id] =
                        first_pair_by_row[row];
                }
                if ((pair_id & 63u) == 63u) {
                    co_await solve_detail::CooperativeCheckpoint{
                        replay_key_cache_owned_bytes};
                }
            }
            decltype(observation_ids){}.swap(observation_ids);
            decltype(immediate_ids){}.swap(immediate_ids);
            replay_key_cache_owned_bytes = saturated_add(
                saturated_product(
                    replay_observation_id.capacity(),
                    sizeof(std::uint32_t)),
                saturated_product(
                    replay_immediate_id.capacity(),
                    sizeof(std::uint32_t)));
            replay_key_cache_owned_bytes = saturated_add(
                replay_key_cache_owned_bytes,
                saturated_product(
                    replay_arc_source.capacity(),
                    sizeof(std::optional<std::uint32_t>)));
            check_owned_cap(replay_key_cache_owned_bytes);
        }

        ClosedPartitionLimits limits;
        limits.max_classes = options.max_pairs;
        limits.max_rounds = options.max_pairs;
        limits.retained_estimated_memory_bytes =
            saturated_add(
                fast_estimated_owned_bytes(),
                saturated_add(
                    stable_keys_owned_bytes,
                    replay_key_cache_owned_bytes));
        limits.max_estimated_memory_bytes =
            options.max_owned_bytes;
        limits.probability_sum_tolerance = 1e-9;
        const auto replay_pair = [&](const std::uint32_t pair_id) {
            const EvalPair& pair = pairs.at(pair_id);
            ClosedPartitionNode node;
            node.stable_key = refinement::StableKey{
                0x6576616c72617731ull, /* "evalraw1" */
                pair_id};
            node.observation_key = use_replay_partition
                ? refinement::StableKey{
                      0x6576616c6f627332ull, /* "evalobs2" */
                      replay_observation_id.at(pair_id)}
                : pair_observation_key(pair);
            node.immediate_key = use_replay_partition
                ? refinement::StableKey{
                      0x6576616c696d6d32ull, /* "evalimm2" */
                      replay_immediate_id.at(pair_id)}
                : pair_immediate_key(pair);
            if (use_replay_partition) {
                node.arc_source = replay_arc_source.at(pair_id);
                if (node.arc_source.has_value()) return node;
            }
            const EvalRow& row = pair_row(pair_id);
            node.arcs.reserve(
                eval_row_arc_count(row));
            visit_eval_row(
                row, pairs,
                [&](const EvalTransition& transition) {
                    node.arcs.push_back({
                        transition_partition_label(transition),
                        std::optional<std::uint32_t>{transition.target},
                        transition.probability});
                },
                [&](const EvalAbsorption& absorption) {
                    node.arcs.push_back({
                        absorption_partition_label(absorption),
                        std::nullopt,
                        absorption.probability});
                });
            return node;
        };
        ClosedPartitionResult refined;
        {
            ActiveTimer partition_timer(
                output.stage_timings.pair_partition_ns);
            refined = use_replay_partition
                ? refinement::refine_closed_probabilistic_partition_replay(
                       static_cast<std::uint32_t>(pairs.size()),
                       replay_pair, {}, false, limits, false,
                       &replay_arc_source, false, false, true)
                : refinement::refine_closed_probabilistic_partition(
                      std::move(closed), limits);
        }
        co_await solve_detail::CooperativeCheckpoint{};
        std::vector<std::uint32_t>{}.swap(replay_observation_id);
        std::vector<std::uint32_t>{}.swap(replay_immediate_id);
        std::vector<std::optional<std::uint32_t>>{}.swap(
            replay_arc_source);
        replay_key_cache_owned_bytes = 0;
        peak_owned_bytes_value = std::max(
            peak_owned_bytes_value,
            refined.peak_estimated_memory_bytes);
        output.peak_owned_bytes_estimate =
            peak_owned_bytes_value;
        if (refined.status != ClosedPartitionStatus::Complete ||
            !refined.lumpable) {
            if (refined.status ==
                    ClosedPartitionStatus::ResourceCap &&
                refined.resource_cap == "max_classes") {
                throw std::length_error(
                    "strategy evaluation exceeded max_pairs (" +
                    std::to_string(options.max_pairs) + ")");
            }
            if (refined.status ==
                    ClosedPartitionStatus::ResourceCap &&
                refined.resource_cap ==
                    "max_estimated_memory_bytes") {
                throw std::length_error(
                    "strategy evaluation exceeded max_owned_bytes (" +
                    std::to_string(options.max_owned_bytes) + ")");
            }
            throw std::runtime_error(
                refined.failure_reason.empty()
                    ? "strategy evaluation pair refinement failed"
                    : "strategy evaluation pair refinement failed: " +
                          refined.failure_reason);
        }
        if (refined.final_class_count > options.max_pairs) {
            throw std::length_error(
                "strategy evaluation exceeded max_pairs (" +
                std::to_string(options.max_pairs) + ")");
        }
        output.refined_pairs = refined.final_class_count;
        output.pair_refinement_rounds = refined.rounds;
        output.pair_lumpability_checks =
            refined.lumpability_checks;

        /* A singleton partition already retains exact attribution in the
         * ordinary evaluator graph. Avoid duplicating and re-solving it; the
         * secondary attribution graph exists only when quotienting actually
         * merges concrete evaluator pairs. */
        if (refined.final_class_count == pairs.size()) {
            if (replayable_operation_rows) {
                if (pairs.size() >= 4096) {
                    throw std::length_error(
                        "strategy evaluation replay partition remained "
                        "identity and cannot materialize the scalable raw "
                        "carrier");
                }
                materialize_replay_rows_for_legacy_consumers();
            }
            retire_pair_discovery_indexes();
            check_owned_cap();
            co_return true;
        }

        if (refined.estimated_memory_bytes <
            limits.retained_estimated_memory_bytes) {
            throw std::logic_error(
                "strategy evaluation partition memory ledger regressed");
        }
        const std::uint64_t refined_result_owned_bytes =
            refined.estimated_memory_bytes -
            limits.retained_estimated_memory_bytes;
        const std::uint64_t projected_representative_bytes =
            saturated_product(
                refined.final_class_count,
                sizeof(std::uint32_t));
        check_owned_cap(saturated_add(
            saturated_add(
                stable_keys_owned_bytes,
                refined_result_owned_bytes),
            projected_representative_bytes));
        std::vector<std::uint32_t> representative(
            refined.final_class_count, kNoId);
        check_owned_cap(saturated_add(
            saturated_add(
                stable_keys_owned_bytes,
                refined_result_owned_bytes),
            saturated_product(
                representative.capacity(),
                sizeof(std::uint32_t))));
        for (std::uint32_t raw = 0; raw < pairs.size(); ++raw) {
            const std::uint32_t class_id =
                refined.class_by_node.at(raw);
            std::uint32_t& selected = representative.at(class_id);
            if (selected == kNoId ||
                raw_pair_stable_key(pairs[raw]) <
                    raw_pair_stable_key(pairs[selected])) {
                selected = raw;
            }
            if ((raw & 255u) == 255u) {
                co_await solve_detail::CooperativeCheckpoint{
                    refined_result_owned_bytes};
            }
        }

        const std::uint32_t refined_class_count =
            refined.final_class_count;
        std::vector<std::uint32_t> class_by_node =
            std::move(refined.class_by_node);
        refined = ClosedPartitionResult{};
        const auto conversion_local_bytes = [&]() {
            return saturated_add(
                saturated_product(
                    representative.capacity(),
                    sizeof(std::uint32_t)),
                saturated_product(
                    class_by_node.capacity(),
                    sizeof(std::uint32_t)));
        };
        check_owned_cap(conversion_local_bytes());

        {
            pair_refinement_identity_graph_intact = false;
            solve_detail::SegmentedVector<EvalPair> raw_pairs =
                std::move(pairs);
            std::vector<EvalRow> raw_rows = std::move(rows);
            attribution_start_pair = start_pair;
            stored_transitions = 0;
            row_payload_owned_bytes = 0;
            std::uint64_t raw_graph_bytes =
                saturated_add(
                    raw_pairs.owned_bytes(),
                    saturated_product(
                        raw_rows.capacity(),
                        sizeof(EvalRow)));
            for (const EvalRow& row : raw_rows) {
                raw_graph_bytes = saturated_add(
                    raw_graph_bytes,
                    saturated_add(
                        saturated_product(
                            row.transitions.capacity(),
                            sizeof(EvalTransition)),
                        saturated_add(
                            saturated_product(
                                row.transition_via.capacity(),
                                sizeof(std::uint32_t)),
                            saturated_add(
                                saturated_product(
                                    row.absorptions.capacity(),
                                    sizeof(EvalAbsorption)),
                                saturated_product(
                                    row.replay_route_tokens.capacity(),
                                    sizeof(std::uint32_t))))));
            }
            const auto check_conversion =
                [&](const std::uint64_t scratch = 0) {
                    check_owned_cap(saturated_add(
                        saturated_add(
                            raw_graph_bytes,
                            conversion_local_bytes()),
                        scratch));
                };
            check_conversion();
            check_conversion(saturated_add(
                solve_detail::SegmentedVector<EvalPair>::
                    projected_owned_bytes(refined_class_count),
                saturated_product(
                    refined_class_count, sizeof(EvalRow))));
            pairs.assign(refined_class_count, EvalPair{});
            rows.clear();
            rows.reserve(refined_class_count);
            check_conversion();

            using TransitionKey = std::tuple<
                std::uint32_t, std::uint32_t, std::uint32_t,
                std::uint32_t>;
            using AbsorptionKey = std::tuple<
                int, std::uint32_t, std::uint32_t,
                std::uint32_t, std::uint32_t>;
            using TransitionMap =
                std::map<TransitionKey, double>;
            using AbsorptionMap =
                std::map<AbsorptionKey, double>;
            const std::uint64_t transition_map_node_bytes =
                sizeof(TransitionMap::value_type) +
                3 * sizeof(void*);
            const std::uint64_t absorption_map_node_bytes =
                sizeof(AbsorptionMap::value_type) +
                3 * sizeof(void*);

            for (std::uint32_t class_id = 0;
                 class_id < refined_class_count; ++class_id) {
                if (class_id != 0 && (class_id & 31u) == 0u) {
                    co_await solve_detail::CooperativeCheckpoint{
                        raw_graph_bytes};
                }
                ActiveTimer conversion_timer(
                    output.stage_timings.pair_quotient_conversion_ns);
                const std::uint32_t raw =
                    representative.at(class_id);
                if (raw == kNoId) {
                    throw std::logic_error(
                        "strategy evaluation refinement produced an empty "
                        "pair class");
                }
                EvalPair pair = raw_pairs.at(raw);
                const EvalRow& source =
                    raw_rows.at(pair.row);
                TransitionMap transitions;
                AbsorptionMap absorptions;
                const auto map_bytes = [&]() {
                    return saturated_add(
                        saturated_product(
                            transitions.size(),
                            transition_map_node_bytes),
                        saturated_product(
                            absorptions.size(),
                            absorption_map_node_bytes));
                };
                visit_eval_row(
                    source, raw_pairs,
                    [&](const EvalTransition& transition) {
                        const TransitionKey key{
                            class_by_node.at(transition.target),
                            transition.edge,
                            transition.policy_route,
                            transition.policy_state};
                        const auto found = transitions.find(key);
                        if (found == transitions.end()) {
                            check_conversion(saturated_add(
                                map_bytes(),
                                transition_map_node_bytes));
                            transitions.emplace(
                                key, transition.probability);
                        } else {
                            found->second += transition.probability;
                        }
                    },
                    [&](const EvalAbsorption& absorption) {
                        const AbsorptionKey key{
                            static_cast<int>(absorption.kind),
                            absorption.node,
                            absorption.state,
                            absorption.edge,
                            absorption.policy_route};
                        const auto found = absorptions.find(key);
                        if (found == absorptions.end()) {
                            check_conversion(saturated_add(
                                map_bytes(),
                                absorption_map_node_bytes));
                            absorptions.emplace(
                                key, absorption.probability);
                        } else {
                            found->second += absorption.probability;
                        }
                    });

                const std::uint64_t projected_row_bytes =
                    saturated_add(
                        saturated_product(
                            transitions.size(),
                            sizeof(EvalTransition)),
                        saturated_product(
                            absorptions.size(),
                            sizeof(EvalAbsorption)));
                check_conversion(saturated_add(
                    map_bytes(), projected_row_bytes));
                EvalRow row;
                row.transitions.reserve(transitions.size());
                row.absorptions.reserve(absorptions.size());
                const auto row_bytes = [&]() {
                    return saturated_add(
                        saturated_product(
                            row.transitions.capacity(),
                            sizeof(EvalTransition)),
                        saturated_add(
                            saturated_product(
                                row.transition_via.capacity(),
                                sizeof(std::uint32_t)),
                            saturated_product(
                                row.absorptions.capacity(),
                                sizeof(EvalAbsorption))));
                };
                check_conversion(saturated_add(
                    map_bytes(), row_bytes()));
                for (const auto& [key, probability] : transitions) {
                    row.transitions.push_back({
                        std::get<0>(key),
                        probability,
                        std::get<1>(key),
                        std::get<2>(key),
                        std::get<3>(key)});
                }
                for (const auto& [key, probability] : absorptions) {
                    row.absorptions.push_back({
                        static_cast<EvalAbsorptionKind>(
                            std::get<0>(key)),
                        std::get<1>(key),
                        std::get<2>(key),
                        probability,
                        std::get<3>(key),
                        std::get<4>(key)});
                }
                check_conversion(saturated_add(
                    map_bytes(), row_bytes()));
                pair.row =
                    static_cast<std::uint32_t>(rows.size());
                pairs[class_id] = std::move(pair);
                stored_transitions +=
                    row.transitions.size() +
                    row.absorptions.size();
                const std::uint64_t retained_row_bytes =
                    row_bytes();
                rows.push_back(std::move(row));
                row_payload_owned_bytes = saturated_add(
                    row_payload_owned_bytes,
                    retained_row_bytes);
                check_conversion(map_bytes());
            }

            if (start_pair != kNoId) {
                start_pair = class_by_node.at(start_pair);
            }
            discover_index = pairs.size();
            attribution_pairs = std::move(raw_pairs);
            attribution_rows = std::move(raw_rows);
            attribution_class_by_pair = std::move(class_by_node);
            attribution_row_payload_owned_bytes = 0;
            for (const EvalRow& row : attribution_rows) {
                attribution_row_payload_owned_bytes = capped_add(
                    attribution_row_payload_owned_bytes,
                    capped_add(
                        capped_product(
                            row.transitions.capacity(),
                            sizeof(EvalTransition)),
                        capped_add(
                            capped_product(
                                row.transition_via.capacity(),
                                sizeof(std::uint32_t)),
                            capped_add(
                                capped_product(
                                    row.absorptions.capacity(),
                                    sizeof(EvalAbsorption)),
                                capped_product(
                                    row.replay_route_tokens.capacity(),
                                    sizeof(std::uint32_t))))));
            }
            if (!has_replayable_rows(attribution_rows)) {
                retire_pair_discovery_indexes();
                std::vector<ReplayRouteResult>().swap(
                    replay_route_results);
            }
            check_owned_cap();
        }
        check_owned_cap();
        co_return true;
    }

    bool accept_identity_pair_refinement_fallback(
            const std::length_error& error) {
        const std::string message = error.what();
        if (!pair_refinement_identity_graph_intact ||
            pairs.size() > options.max_pairs ||
            message.find("max_owned_bytes") == std::string::npos) {
            return false;
        }
        /* The unquotiented pair graph is itself an exact, trivially lumpable
         * identity partition. Keep this resource fallback outside coroutine
         * exception regions so WASM's legacy exception lowering never has
         * to merge suspended frames through a handler. */
        output.refined_pairs = static_cast<std::uint32_t>(pairs.size());
        output.pair_refinement_rounds = 0;
        output.pair_lumpability_checks = 0;
        peak_owned_bytes_value = std::max(
            pair_refinement_peak_before, fast_estimated_owned_bytes());
        output.peak_owned_bytes_estimate = peak_owned_bytes_value;
        check_owned_cap();
        return true;
    }

    /* Fold deterministic pass-through pairs â€” exactly one outgoing
     * transition with probability exactly 1 and no absorptions â€” out of
     * the transition relation before components are built. Every
     * transition entering such a pair is redirected to the first
     * non-pass-through pair down its chain, so hub-and-spoke loops
     * (reforge â†” deterministic scour, solver router/restart graphs)
     * become self-loops the closed-form solvers handle instead of
     * multi-thousand-member fallback components. The rewrite is exact:
     * probabilities are only regrouped, never truncated, and the folded
     * pairs' visits and edge traversals are settled from chain_inflow at
     * finalization. Pairs on a purely deterministic cycle are left in
     * place so recurrent classes keep their unresolved treatment. */
    void contract_pass_through() {
        const std::size_t count = pairs.size();
        pair_contracted.assign(count, 0);
        chain_next.assign(count, kNoId);
        chain_edge.assign(count, kNoId);
        chain_policy_route.assign(count, kNoId);
        chain_policy_state.assign(count, kNoId);
        chain_terminal.assign(count, kNoId);
        chain_inflow.assign(count, 0.0);
        if (count == 0) return;

        std::vector<std::uint8_t> pass(count, 0);
        for (std::uint32_t pair = 0; pair < count; ++pair) {
            const EvalRow& row = pair_row(pair);
            if (!row.transition_via.empty()) {
                throw std::logic_error(
                    "strategy evaluation pass-through contraction was "
                    "applied twice");
            }
            if (row.absorptions.empty() && row.transitions.size() == 1 &&
                row.transitions.front().probability == 1.0) {
                pass[pair] = 1;
                chain_next[pair] = row.transitions.front().target;
                chain_edge[pair] = row.transitions.front().edge;
                chain_policy_route[pair] =
                    row.transitions.front().policy_route;
                chain_policy_state[pair] =
                    row.transitions.front().policy_state;
            }
        }

        /* Resolve each pair's forward target: itself when it is not a
         * pass-through, otherwise the end of its deterministic chain.
         * state: 0 unvisited, 1 on the current walk, 2 resolved. */
        std::vector<std::uint8_t> state(count, 0);
        std::vector<std::uint32_t> forward(count, kNoId);
        std::vector<std::uint32_t> path;
        bool any_contracted = false;
        for (std::uint32_t root = 0; root < count; ++root) {
            if (state[root] == 2) continue;
            path.clear();
            std::uint32_t cursor = root;
            while (state[cursor] != 2) {
                if (!pass[cursor]) {
                    state[cursor] = 2;
                    forward[cursor] = cursor;
                    break;
                }
                if (state[cursor] == 1) {
                    /* The walk re-entered itself: cursor..path.back()
                     * form a deterministic cycle. Keep those pairs. */
                    std::size_t cycle = path.size();
                    while (path[cycle - 1] != cursor) --cycle;
                    --cycle;
                    for (std::size_t i = cycle; i < path.size(); ++i) {
                        state[path[i]] = 2;
                        forward[path[i]] = path[i];
                        pass[path[i]] = 0;
                    }
                    path.resize(cycle);
                    break;
                }
                state[cursor] = 1;
                path.push_back(cursor);
                cursor = chain_next[cursor];
            }
            for (std::size_t i = path.size(); i-- > 0;) {
                const std::uint32_t pair = path[i];
                state[pair] = 2;
                forward[pair] = forward[chain_next[pair]];
                chain_terminal[pair] = forward[pair];
                pair_contracted[pair] = 1;
                any_contracted = true;
            }
        }
        if (!any_contracted) return;

        std::uint64_t remaining_transitions = 0;
        for (EvalRow& row : rows) {
            bool touched = false;
            for (const EvalTransition& transition : row.transitions) {
                if (pair_contracted[transition.target]) {
                    touched = true;
                    break;
                }
            }
            if (touched) {
                std::map<
                    std::tuple<
                        std::uint32_t, std::uint32_t, std::uint32_t,
                        std::uint32_t, std::uint32_t>,
                    double> merged;
                for (std::size_t index = 0;
                     index < row.transitions.size(); ++index) {
                    const EvalTransition& transition =
                        row.transitions[index];
                    const std::uint32_t via =
                        pair_contracted[transition.target]
                            ? transition.target
                            : transition_via(row, index);
                    const std::uint32_t target =
                        via == kNoId ? transition.target
                                     : forward[transition.target];
                    merged[{target, transition.edge, via,
                            transition.policy_route,
                            transition.policy_state}] +=
                        transition.probability;
                }
                row.transitions.clear();
                row.transition_via.clear();
                row.transitions.reserve(merged.size());
                row.transition_via.reserve(merged.size());
                for (const auto& [key, probability] : merged) {
                    row.transitions.push_back(
                        {std::get<0>(key), probability, std::get<1>(key),
                         std::get<3>(key), std::get<4>(key)});
                    row.transition_via.push_back(std::get<2>(key));
                }
            }
            remaining_transitions +=
                row.transitions.size() + row.absorptions.size();
        }
        stored_transitions = remaining_transitions;
        refresh_row_payload_owned_bytes();
    }

    /* Settle the mass that flowed through contracted pairs: each visit
     * of a folded chain and its single edge, in chain order. Runs once,
     * at finalization, after every component (or the reference sweep)
     * has committed its flows into chain_inflow. */
    solve_detail::CooperativeTask<bool> propagate_chain_inflow() {
        const std::size_t count = pair_contracted.size();
        std::vector<std::uint32_t> indegree(count, 0);
        for (std::uint32_t pair = 0; pair < count; ++pair) {
            if (pair_contracted[pair]) {
                const std::uint32_t next = chain_next[pair];
                if (pair_contracted[next]) ++indegree[next];
            }
            if ((pair & 255u) == 255u) {
                co_await solve_detail::CooperativeCheckpoint{
                    capped_product(
                        indegree.capacity(), sizeof(std::uint32_t))};
            }
        }
        std::vector<std::uint32_t> ready;
        for (std::uint32_t pair = 0; pair < count; ++pair) {
            if (pair_contracted[pair] && indegree[pair] == 0) {
                ready.push_back(pair);
            }
            if ((pair & 255u) == 255u) {
                co_await solve_detail::CooperativeCheckpoint{
                    capped_product(
                        indegree.capacity() + ready.capacity(),
                        sizeof(std::uint32_t))};
            }
        }
        std::uint64_t propagated = 0;
        while (!ready.empty()) {
            const std::uint32_t pair = ready.back();
            ready.pop_back();
            const double inflow = chain_inflow[pair];
            pair_visits[pair] += inflow;
            if (chain_edge[pair] != kNoId) {
                edge_traversals.at(chain_edge[pair]) += inflow;
            }
            add_compressed_policy_incoming(
                chain_policy_route[pair], chain_policy_state[pair], inflow);
            const std::uint32_t next = chain_next[pair];
            if (pair_contracted[next]) {
                chain_inflow[next] += inflow;
                if (--indegree[next] == 0) ready.push_back(next);
            }
            if ((++propagated & 255u) == 0u) {
                co_await solve_detail::CooperativeCheckpoint{
                    capped_product(
                        indegree.capacity() + ready.capacity(),
                        sizeof(std::uint32_t))};
            }
        }
        co_return true;
    }

    solve_detail::CooperativeTask<bool> build_components() {
        subphase = StrategyEvalSubphase::PairRefinement;
        co_await solve_detail::CooperativeCheckpoint{};
        if (!skip_pair_refinement_once) {
            auto refinement = refine_pair_graph();
            while (!refinement.resume()) {
                co_await solve_detail::CooperativeCheckpoint{
                    refinement.retained_bytes()};
            }
            (void)refinement.take_result();
        }
        skip_pair_refinement_once = false;
        subphase = StrategyEvalSubphase::ComponentConstruction;
        co_await solve_detail::CooperativeCheckpoint{};
        contract_pass_through();
        co_await solve_detail::CooperativeCheckpoint{};
        const std::size_t count = pairs.size();
        struct Frame {
            std::uint32_t pair = kNoId;
            std::size_t next_transition = 0;
        };
        std::vector<std::uint32_t> index(count, kNoId);
        std::vector<std::uint32_t> lowlink(count, kNoId);
        std::vector<std::uint8_t> on_stack(count, 0);
        std::vector<std::uint32_t> tarjan_stack;
        tarjan_stack.reserve(count);
        std::vector<std::vector<std::uint32_t>> raw_components;
        std::uint32_t next_index = 0;

        const auto push_pair = [&](std::uint32_t pair,
                                   std::vector<Frame>& dfs) {
            index[pair] = next_index;
            lowlink[pair] = next_index;
            ++next_index;
            tarjan_stack.push_back(pair);
            on_stack[pair] = 1;
            dfs.push_back({pair, 0});
        };
        for (std::uint32_t root = 0; root < count; ++root) {
            if (pair_contracted[root] || index[root] != kNoId) continue;
            std::vector<Frame> dfs;
            push_pair(root, dfs);
            std::uint64_t tarjan_work = 0;
            while (!dfs.empty()) {
                Frame& frame = dfs.back();
                const auto& transitions = pair_row(frame.pair).transitions;
                if (frame.next_transition < transitions.size()) {
                    const std::uint32_t target =
                        transitions[frame.next_transition++].target;
                    if (index[target] == kNoId) {
                        push_pair(target, dfs);
                    } else if (on_stack[target]) {
                        lowlink[frame.pair] = std::min(
                            lowlink[frame.pair], index[target]);
                    }
                    if ((++tarjan_work & 1023u) == 0u) {
                        co_await solve_detail::CooperativeCheckpoint{};
                    }
                    continue;
                }

                const std::uint32_t completed = frame.pair;
                dfs.pop_back();
                if (!dfs.empty()) {
                    const std::uint32_t parent = dfs.back().pair;
                    lowlink[parent] = std::min(
                        lowlink[parent], lowlink[completed]);
                }
                if (lowlink[completed] == index[completed]) {
                    raw_components.emplace_back();
                    while (true) {
                        const std::uint32_t member = tarjan_stack.back();
                        tarjan_stack.pop_back();
                        on_stack[member] = 0;
                        raw_components.back().push_back(member);
                        if (member == completed) break;
                    }
                    std::sort(
                        raw_components.back().begin(),
                        raw_components.back().end());
                }
                if ((++tarjan_work & 1023u) == 0u) {
                    co_await solve_detail::CooperativeCheckpoint{};
                }
            }
            co_await solve_detail::CooperativeCheckpoint{};
        }

        /* Tarjan emits sink components first. Reverse that order so forward
         * mass reaches every component before it is solved, without copying
         * the (potentially dense) edge relation into adjacency lists. */
        components.clear();
        components.reserve(raw_components.size());
        for (auto it = raw_components.rbegin();
             it != raw_components.rend(); ++it) {
            components.push_back(std::move(*it));
        }
        component_by_pair.assign(count, kNoId);
        for (std::uint32_t component = 0;
             component < components.size(); ++component) {
            for (const std::uint32_t pair : components[component]) {
                component_by_pair[pair] = component;
            }
        }
        component_payload_owned_bytes = 0;
        for (const auto& component : components) {
            component_payload_owned_bytes +=
                component.capacity() * sizeof(std::uint32_t);
        }

        external_incoming.assign(count, 0.0);
        pair_visits.assign(count, 0.0);
        unresolved_pair.assign(count, 0.0);
        if (start_pair != kNoId) {
            if (pair_contracted[start_pair]) {
                chain_inflow[start_pair] = 1.0;
                external_incoming.at(chain_terminal[start_pair]) = 1.0;
            } else {
                external_incoming[start_pair] = 1.0;
            }
        }
        component_index = 0;
        co_return true;
    }

    bool component_has_exit(
        std::uint32_t component,
        const std::vector<std::uint32_t>& members) const {
        for (const std::uint32_t pair : members) {
            const EvalRow& row = pair_row(pair);
            if (!row.absorptions.empty()) return true;
            for (const EvalTransition& transition : row.transitions) {
                if (component_by_pair[transition.target] != component &&
                    transition.probability > 0.0) {
                    return true;
                }
            }
        }
        return false;
    }

    bool dense_solve(
        std::uint32_t component,
        const std::vector<std::uint32_t>& members,
        const std::vector<double>& incoming,
        std::vector<double>& visits) const {
        const std::size_t n = members.size();
        if (n == std::numeric_limits<std::size_t>::max()) return false;
        std::map<std::uint32_t, std::size_t> local;
        for (std::size_t i = 0; i < n; ++i) local[members[i]] = i;
        std::vector<std::vector<long double>> matrix(
            n, std::vector<long double>(n + 1, 0.0L));
        for (std::size_t row = 0; row < n; ++row) {
            matrix[row][row] = 1.0L;
            matrix[row][n] = incoming[row];
        }
        for (std::size_t source = 0; source < n; ++source) {
            for (const EvalTransition& transition :
                 pair_row(members[source]).transitions) {
                if (component_by_pair[transition.target] != component) continue;
                matrix[local.at(transition.target)][source] -=
                    static_cast<long double>(transition.probability);
            }
        }

        long double matrix_scale = 0.0L;
        for (const auto& row : matrix) {
            for (std::size_t col = 0; col < n; ++col) {
                matrix_scale = std::max(matrix_scale, std::fabs(row[col]));
            }
        }
        long double min_pivot = std::numeric_limits<long double>::infinity();
        long double max_pivot = 0.0L;
        for (std::size_t col = 0; col < n; ++col) {
            std::size_t pivot = col;
            long double pivot_abs = std::fabs(matrix[col][col]);
            for (std::size_t row = col + 1; row < n; ++row) {
                const long double candidate = std::fabs(matrix[row][col]);
                if (candidate > pivot_abs) {
                    pivot = row;
                    pivot_abs = candidate;
                }
            }
            if (pivot_abs <= std::max(1e-18L, matrix_scale * 1e-14L)) {
                return false;
            }
            if (pivot != col) std::swap(matrix[pivot], matrix[col]);
            min_pivot = std::min(min_pivot, pivot_abs);
            max_pivot = std::max(max_pivot, pivot_abs);
            for (std::size_t row = col + 1; row < n; ++row) {
                const long double factor = matrix[row][col] / matrix[col][col];
                if (factor == 0.0L) continue;
                for (std::size_t k = col; k <= n; ++k) {
                    matrix[row][k] -= factor * matrix[col][k];
                }
            }
        }
        if (max_pivot > 0.0L && min_pivot / max_pivot < 1e-13L) {
            return false;
        }

        std::vector<long double> solved(n, 0.0L);
        for (std::size_t back = n; back-- > 0;) {
            long double value = matrix[back][n];
            for (std::size_t col = back + 1; col < n; ++col) {
                value -= matrix[back][col] * solved[col];
            }
            solved[back] = value / matrix[back][back];
            if (!std::isfinite(solved[back])) return false;
        }
        long double max_value = 0.0L;
        for (const long double value : solved) {
            max_value = std::max(max_value, std::fabs(value));
        }
        const long double negative_tolerance =
            1e-12L * std::max(1.0L, max_value);
        visits.resize(n);
        for (std::size_t i = 0; i < n; ++i) {
            if (solved[i] < -negative_tolerance) return false;
            visits[i] = static_cast<double>(std::max(0.0L, solved[i]));
            if (!std::isfinite(visits[i])) return false;
        }

        long double max_residual = 0.0L;
        for (std::size_t target = 0; target < n; ++target) {
            long double expected = incoming[target];
            for (std::size_t source = 0; source < n; ++source) {
                for (const EvalTransition& transition :
                     pair_row(members[source]).transitions) {
                    if (transition.target == members[target]) {
                        expected += static_cast<long double>(visits[source]) *
                                    transition.probability;
                    }
                }
            }
            max_residual = std::max(
                max_residual,
                std::fabs(static_cast<long double>(visits[target]) - expected));
        }
        const long double tolerance = std::max(
            1e-12L,
            static_cast<long double>(options.epsilon) *
                std::max(1.0L, max_value) * 10.0L);
        return max_residual <= tolerance;
    }

    bool rank_one_solve(
        std::uint32_t component,
        const std::vector<std::uint32_t>& members,
        const std::vector<double>& incoming,
        std::vector<double>& visits) const {
        if (members.size() < 2) return false;
        const auto row = [&](std::uint32_t pair) {
            std::map<std::uint32_t, double> probabilities;
            for (const EvalTransition& transition : pair_row(pair).transitions) {
                if (component_by_pair[transition.target] == component) {
                    probabilities[transition.target] += transition.probability;
                }
            }
            return probabilities;
        };
        const std::map<std::uint32_t, double> reference = row(members.front());
        for (std::size_t source = 1; source < members.size(); ++source) {
            if (pairs[members[source]].row == pairs[members.front()].row) {
                continue;
            }
            const auto candidate = row(members[source]);
            if (candidate.size() != reference.size()) return false;
            auto a = reference.begin();
            auto b = candidate.begin();
            for (; a != reference.end(); ++a, ++b) {
                if (a->first != b->first ||
                    std::fabs(a->second - b->second) > 1e-15) {
                    return false;
                }
            }
        }
        double internal_probability = 0.0;
        for (const auto& [target, probability] : reference) {
            (void)target;
            internal_probability += probability;
        }
        const double denominator = 1.0 - internal_probability;
        if (!(denominator > 1e-14)) return false;
        double total_incoming = 0.0;
        for (const double mass : incoming) total_incoming += mass;
        const double total_visits = total_incoming / denominator;
        if (!std::isfinite(total_visits)) return false;
        visits = incoming;
        for (std::size_t target = 0; target < members.size(); ++target) {
            const auto found = reference.find(members[target]);
            if (found != reference.end()) {
                visits[target] += found->second * total_visits;
            }
        }
        return true;
    }

    bool shared_row_solve(
        const std::uint32_t component,
        const std::vector<std::uint32_t>& members,
        const std::vector<double>& incoming,
        std::vector<double>& visits) {
        using solve_detail::PolicyEdge;
        using solve_detail::PolicyRow;
        using solve_detail::SparsePolicyComponentResult;
        using solve_detail::SparsePolicyComponentStatus;
        using solve_detail::SparsePolicyComponentView;
        using solve_detail::SparsePolicyResume;

        if (members.size() < 2 || incoming.size() != members.size()) {
            return false;
        }

        std::vector<std::int32_t> row_local_by_id(rows.size(), -1);
        std::vector<std::int32_t> pair_local_by_id(pairs.size(), -1);
        std::vector<std::uint32_t> unique_rows;
        unique_rows.reserve(members.size());
        for (std::size_t local = 0; local < members.size(); ++local) {
            const std::uint32_t pair = members[local];
            pair_local_by_id[pair] = static_cast<std::int32_t>(local);
            const std::uint32_t row = pairs[pair].row;
            if (row_local_by_id[row] < 0) {
                row_local_by_id[row] =
                    static_cast<std::int32_t>(unique_rows.size());
                unique_rows.push_back(row);
            }
        }
        /* With no sharing this is exactly the ordinary pair system, so keep
         * the established fallback path. The compact solve matters when an
         * exact kernel row is reused by many concrete occupancy states. */
        if (unique_rows.size() == members.size()) return false;

        const std::size_t row_count = unique_rows.size();
        if (row_count > std::numeric_limits<std::uint32_t>::max()) {
            throw std::length_error(
                "strategy evaluation shared-row component is too large");
        }

        std::uint64_t edge_count = 0;
        std::vector<std::uint32_t> incoming_counts(row_count, 0);
        for (std::uint32_t source = 0; source < row_count; ++source) {
            for (const EvalTransition& transition :
                 rows[unique_rows[source]].transitions) {
                if (component_by_pair[transition.target] != component) {
                    continue;
                }
                const std::uint32_t target_row =
                    pairs[transition.target].row;
                const std::int32_t target_local =
                    row_local_by_id[target_row];
                if (target_local < 0) {
                    throw std::logic_error(
                        "strategy evaluation shared-row component target "
                        "is missing");
                }
                std::uint32_t& count = incoming_counts[
                    static_cast<std::size_t>(target_local)];
                if (count == std::numeric_limits<std::uint32_t>::max()) {
                    throw std::length_error(
                        "strategy evaluation shared-row component edge "
                        "count overflowed");
                }
                ++count;
                edge_count = capped_add(edge_count, 1);
            }
        }
        if (edge_count > std::numeric_limits<std::uint32_t>::max()) {
            throw std::length_error(
                "strategy evaluation shared-row component edge count "
                "overflowed");
        }

        std::vector<PolicyRow> transpose_rows(row_count);
        std::vector<PolicyEdge> transpose_edges(
            static_cast<std::size_t>(edge_count));
        std::vector<std::uint32_t> cursors(row_count, 0);
        std::uint64_t offset = 0;
        for (std::uint32_t target = 0; target < row_count; ++target) {
            transpose_rows[target].edge_offset = offset;
            transpose_rows[target].edge_count = incoming_counts[target];
            cursors[target] = static_cast<std::uint32_t>(offset);
            offset += incoming_counts[target];
        }
        for (std::uint32_t source = 0; source < row_count; ++source) {
            for (const EvalTransition& transition :
                 rows[unique_rows[source]].transitions) {
                if (component_by_pair[transition.target] != component) {
                    continue;
                }
                const std::int32_t target_local =
                    row_local_by_id[pairs[transition.target].row];
                transpose_edges[cursors[
                    static_cast<std::size_t>(target_local)]++] = {
                    source, transition.probability};
            }
        }

        std::vector<solve_detail::WideFloat> rhs_wide(
            row_count, solve_detail::WideFloat{0.0});
        for (std::size_t local = 0; local < members.size(); ++local) {
            const std::int32_t row_local =
                row_local_by_id[pairs[members[local]].row];
            rhs_wide[static_cast<std::size_t>(row_local)] +=
                solve_detail::WideFloat{incoming[local]};
        }
        std::vector<double> rhs(row_count, 0.0);
        std::vector<double> previous_values(row_count, 0.0);
        std::vector<std::uint32_t> row_members(row_count);
        std::vector<std::uint32_t> row_component(row_count, 0);
        std::vector<std::int32_t> row_local(row_count, -1);
        for (std::uint32_t row = 0; row < row_count; ++row) {
            rhs[row] = rhs_wide[row].value();
            row_members[row] = row;
            row_local[row] = static_cast<std::int32_t>(row);
        }

        const auto transient_bytes = [&](const std::uint64_t scratch = 0) {
            std::uint64_t bytes = scratch;
            const auto add_vector = [&](const auto& values) {
                using Value = typename std::decay_t<
                    decltype(values)>::value_type;
                bytes = capped_add(
                    bytes,
                    capped_product(values.capacity(), sizeof(Value)));
            };
            add_vector(row_local_by_id);
            add_vector(pair_local_by_id);
            add_vector(unique_rows);
            add_vector(incoming_counts);
            add_vector(transpose_rows);
            add_vector(transpose_edges);
            add_vector(cursors);
            add_vector(rhs_wide);
            add_vector(rhs);
            add_vector(previous_values);
            add_vector(row_members);
            add_vector(row_component);
            add_vector(row_local);
            return bytes;
        };

        std::unique_ptr<SparsePolicyResume> resume;
        SparsePolicyComponentResult solved;
        std::vector<solve_detail::WideFloat> solved_wide;
        do {
            std::uint64_t scratch =
                solve_detail::sparse_policy_component_scratch_bytes(
                    row_count, true);
            scratch = capped_add(
                scratch,
                capped_product(
                    row_count,
                    sizeof(double) + sizeof(std::uint32_t)));
            check_owned_cap(transient_bytes(scratch));
            solved = solve_detail::advance_sparse_policy_component(
                SparsePolicyComponentView{
                    row_members, 0, row_component, row_local,
                    transpose_rows, transpose_edges, rhs,
                    previous_values, options.max_sweeps},
                resume, &solved_wide);
        } while (solved.status ==
                 SparsePolicyComponentStatus::Incomplete);

        if (solved.status ==
            SparsePolicyComponentStatus::DidNotConverge) {
            throw std::length_error(
                "strategy evaluation shared-row component reached "
                "max_sweeps (" + std::to_string(options.max_sweeps) +
                ")");
        }
        if (solved.status != SparsePolicyComponentStatus::Complete ||
            solved.values.size() != row_count ||
            solved_wide.size() != row_count) {
            throw std::runtime_error(
                "strategy evaluation shared-row component solve failed");
        }

        for (std::uint32_t target = 0; target < row_count; ++target) {
            solve_detail::WideFloat expected = rhs_wide[target];
            const PolicyRow& row = transpose_rows[target];
            for (std::uint32_t edge_index = 0;
                 edge_index < row.edge_count; ++edge_index) {
                const PolicyEdge& edge = transpose_edges.at(
                    row.edge_offset + edge_index);
                expected += solved_wide[edge.target] *
                            solve_detail::WideFloat{edge.probability};
            }
            const double residual = std::fabs(
                (solved_wide[target] - expected).value());
            const double scale = std::max(
                {1.0, std::fabs(solved.values[target]),
                 std::fabs(expected.value())});
            const double tolerance =
                std::max(
                    options.epsilon,
                    8.0 * std::numeric_limits<double>::epsilon()) *
                scale;
            if (!std::isfinite(residual) || residual > tolerance) {
                throw std::runtime_error(
                    "strategy evaluation shared-row component residual "
                    "exceeded epsilon");
            }
        }

        std::vector<solve_detail::WideFloat> reconstructed;
        reconstructed.reserve(members.size());
        for (const double value : incoming) {
            reconstructed.emplace_back(value);
        }
        for (std::uint32_t source = 0; source < row_count; ++source) {
            for (const EvalTransition& transition :
                 rows[unique_rows[source]].transitions) {
                if (component_by_pair[transition.target] != component) {
                    continue;
                }
                const std::int32_t target_local =
                    pair_local_by_id[transition.target];
                if (target_local < 0) {
                    throw std::logic_error(
                        "strategy evaluation shared-row raw target is "
                        "missing");
                }
                reconstructed[static_cast<std::size_t>(target_local)] +=
                    solved_wide[source] *
                    solve_detail::WideFloat{transition.probability};
            }
        }
        std::vector<solve_detail::WideFloat> checked_rows(
            row_count, solve_detail::WideFloat{0.0});
        visits.resize(members.size());
        for (std::size_t local = 0; local < members.size(); ++local) {
            const double value = reconstructed[local].value();
            if (!std::isfinite(value) || value < -1e-10) {
                throw std::runtime_error(
                    "strategy evaluation shared-row component produced "
                    "an invalid occupancy");
            }
            visits[local] = std::max(0.0, value);
            const std::int32_t row_id =
                row_local_by_id[pairs[members[local]].row];
            checked_rows[static_cast<std::size_t>(row_id)] +=
                solve_detail::WideFloat{visits[local]};
        }
        for (std::uint32_t row = 0; row < row_count; ++row) {
            const double residual = std::fabs(
                (checked_rows[row] - solved_wide[row]).value());
            const double scale = std::max(
                {1.0, std::fabs(checked_rows[row].value()),
                 std::fabs(solved.values[row])});
            const double tolerance =
                std::max(
                    options.epsilon,
                    8.0 * std::numeric_limits<double>::epsilon()) *
                scale;
            if (!std::isfinite(residual) || residual > tolerance) {
                throw std::runtime_error(
                    "strategy evaluation shared-row raw reconstruction "
                    "does not preserve row occupancy");
            }
        }
        check_owned_cap(transient_bytes(capped_add(
            capped_product(
                reconstructed.capacity(),
                sizeof(solve_detail::WideFloat)),
            capped_product(
                checked_rows.capacity(),
                sizeof(solve_detail::WideFloat)))));
        return true;
    }

    void add_absorption(const EvalAbsorption& absorption, double mass) {
        if (!(mass > 0.0)) return;
        if (absorption.edge != kNoId) {
            edge_traversals.at(absorption.edge) += mass;
        }
        add_compressed_policy_incoming(
            absorption.policy_route, absorption.state, mass);
        switch (absorption.kind) {
        case EvalAbsorptionKind::Terminal:
            terminal_mass[absorption.node] += mass;
            add_terminal_incoming(
                absorption.node, absorption.state, mass);
            break;
        case EvalAbsorptionKind::ActionNotApplied:
            action_not_applied[absorption.node] += mass;
            break;
        case EvalAbsorptionKind::NoMatchingEdge:
            no_matching_edge[absorption.node] += mass;
            break;
        }
    }

    void commit_component(
        std::uint32_t component,
        const std::vector<std::uint32_t>& members,
        const std::vector<double>& visits) {
        for (std::size_t i = 0; i < members.size(); ++i) {
            const std::uint32_t pair = members[i];
            const double visit = visits[i];
            pair_visits[pair] += visit;
            const EvalRow& row = pair_row(pair);
            for (std::size_t transition_index = 0;
                 transition_index < row.transitions.size();
                 ++transition_index) {
                const EvalTransition& transition =
                    row.transitions[transition_index];
                const double flow = visit * transition.probability;
                if (transition.edge != kNoId) {
                    edge_traversals.at(transition.edge) += flow;
                }
                add_compressed_policy_incoming(
                    transition.policy_route,
                    transition.policy_state, flow);
                const std::uint32_t via =
                    transition_via(row, transition_index);
                if (via != kNoId) {
                    chain_inflow.at(via) += flow;
                }
                if (component_by_pair[transition.target] != component) {
                    external_incoming[transition.target] += flow;
                }
            }
            for (const EvalAbsorption& absorption : row.absorptions) {
                add_absorption(
                    absorption, visit * absorption.probability);
            }
        }
    }

    void add_unresolved(
        const std::vector<std::uint32_t>& members,
        const std::vector<double>& mass,
        bool hard) {
        for (std::size_t i = 0; i < members.size(); ++i) {
            unresolved_pair[members[i]] += mass[i];
        }
        hard_unresolved = hard_unresolved || hard;
    }

    void begin_fallback(
        std::uint32_t component,
        const std::vector<std::uint32_t>& members,
        const std::vector<double>& incoming) {
        fallback = std::make_unique<FallbackState>();
        fallback->component = component;
        fallback->members = members;
        fallback->local_index_by_pair.assign(pairs.size(), -1);
        fallback->incoming = incoming;
        for (std::size_t i = 0; i < members.size(); ++i) {
            fallback->local_index_by_pair[members[i]] =
                static_cast<std::int32_t>(i);
            fallback->input_mass += incoming[i];
        }

        std::vector<std::uint32_t> incoming_counts(pairs.size(), 0);
        std::uint64_t internal_edges = 0;
        for (const std::uint32_t source : members) {
            for (const EvalTransition& transition :
                 pair_row(source).transitions) {
                if (component_by_pair[transition.target] != component) {
                    continue;
                }
                if (incoming_counts[transition.target] ==
                    std::numeric_limits<std::uint32_t>::max()) {
                    throw std::length_error(
                        "strategy evaluation fallback edge count overflowed");
                }
                ++incoming_counts[transition.target];
                ++internal_edges;
            }
        }
        check_owned_cap(
            capped_add(
                capped_product(
                    incoming_counts.capacity(), sizeof(std::uint32_t)),
                capped_add(
                    capped_product(
                        pairs.size(), sizeof(solve_detail::PolicyRow)),
                    capped_add(
                        capped_product(
                            pairs.size(), sizeof(double)),
                        capped_product(
                            internal_edges,
                            sizeof(solve_detail::PolicyEdge))))));
        fallback->transpose_rows.assign(
            pairs.size(), solve_detail::PolicyRow{});
        fallback->transpose_edges.resize(
            static_cast<std::size_t>(internal_edges));
        fallback->previous_values.assign(pairs.size(), 0.0);
        std::vector<std::uint32_t> cursors(pairs.size(), 0);
        std::uint64_t offset = 0;
        for (const std::uint32_t target : members) {
            fallback->transpose_rows[target].edge_offset = offset;
            fallback->transpose_rows[target].edge_count =
                incoming_counts[target];
            cursors[target] = static_cast<std::uint32_t>(offset);
            offset += incoming_counts[target];
        }
        for (const std::uint32_t source : members) {
            for (const EvalTransition& transition :
                 pair_row(source).transitions) {
                if (component_by_pair[transition.target] != component) {
                    continue;
                }
                fallback->transpose_edges[cursors[transition.target]++] = {
                    source, transition.probability};
            }
        }
        check_owned_cap(capped_add(
            capped_product(cursors.capacity(), sizeof(std::uint32_t)),
            capped_product(
                incoming_counts.capacity(), sizeof(std::uint32_t))));
        phase = StrategyEvalPhase::Fallback;
    }

    void finish_component() {
        ++component_index;
        phase = component_index < components.size()
                    ? StrategyEvalPhase::Solving
                    : StrategyEvalPhase::Finalization;
        subphase = phase == StrategyEvalPhase::Finalization
            ? StrategyEvalSubphase::Finalization
            : StrategyEvalSubphase::ComponentSolve;
    }

    void solve_component() {
        subphase = StrategyEvalSubphase::ComponentSolve;
        if (component_index >= components.size()) {
            phase = StrategyEvalPhase::Finalization;
            subphase = StrategyEvalSubphase::Finalization;
            return;
        }
        const std::uint32_t component =
            static_cast<std::uint32_t>(component_index);
        const auto& members = components[component_index];
        std::vector<double> incoming;
        incoming.reserve(members.size());
        double input_mass = 0.0;
        for (const std::uint32_t pair : members) {
            incoming.push_back(external_incoming[pair]);
            input_mass += external_incoming[pair];
        }
        if (input_mass == 0.0) {
            commit_component(component, members, incoming);
            finish_component();
            return;
        }
        if (!component_has_exit(component, members)) {
            /* A recurrent class has infinite visit counts. Preserve a finite,
             * useful entry snapshot and attribute all entering probability as
             * unresolved without fabricating repeated traversals. */
            for (std::size_t i = 0; i < members.size(); ++i) {
                pair_visits[members[i]] += incoming[i];
            }
            add_unresolved(members, incoming, true);
            finish_component();
            return;
        }

        std::vector<double> visits;
        bool solved = false;
        if (members.size() == 1) {
            double self_probability = 0.0;
            for (const EvalTransition& transition :
                 pair_row(members.front()).transitions) {
                if (transition.target == members.front()) {
                    self_probability += transition.probability;
                }
            }
            const double denominator = 1.0 - self_probability;
            if (denominator > 1e-14) {
                const double value = incoming.front() / denominator;
                if (std::isfinite(value) && value >= 0.0) {
                    visits = {value};
                    solved = true;
                }
            }
        } else if (rank_one_solve(
                       component, members, incoming, visits)) {
            solved = true;
        } else if (shared_row_solve(
                       component, members, incoming, visits)) {
            solved = true;
        } else if (members.size() <= 64) {
            solved = dense_solve(component, members, incoming, visits);
        }
        if (solved) {
            commit_component(component, members, visits);
            finish_component();
        } else {
            begin_fallback(component, members, incoming);
        }
    }

    void run_fallback_batch() {
        subphase = StrategyEvalSubphase::ComponentSolve;
        FallbackState& state = *fallback;
        const std::uint64_t scratch =
            solve_detail::sparse_policy_component_scratch_bytes(
                state.members.size(), false);
        check_owned_cap(scratch);
        solve_detail::SparsePolicyComponentResult solved =
            solve_detail::advance_sparse_policy_component(
                solve_detail::SparsePolicyComponentView{
                    state.members,
                    state.component,
                    component_by_pair,
                    state.local_index_by_pair,
                    state.transpose_rows,
                    state.transpose_edges,
                    state.incoming,
                    state.previous_values,
                    options.max_sweeps},
                state.resume);
        fallback_sweeps += solved.iterations;
        check_owned_cap();
        if (solved.status ==
            solve_detail::SparsePolicyComponentStatus::Incomplete) {
            return;
        }

        bool complete =
            solved.status ==
                solve_detail::SparsePolicyComponentStatus::Complete &&
            solved.values.size() == state.members.size();
        if (complete) {
            for (double& visit : solved.values) {
                if (!std::isfinite(visit) || visit < -1e-10) {
                    complete = false;
                    break;
                }
                visit = std::max(0.0, visit);
            }
        }
        if (complete) {
            commit_component(
                state.component, state.members, solved.values);
        } else {
            for (std::size_t i = 0; i < state.members.size(); ++i) {
                pair_visits[state.members[i]] += state.incoming[i];
            }
            add_unresolved(state.members, state.incoming, true);
        }
        fallback.reset();
        finish_component();
    }

    void validate_exact_attribution_quotient(
            const std::vector<double>& exact_visits,
            const std::uint64_t retained_scratch_bytes) {
        const std::size_t count = attribution_pairs.size();
        if (exact_visits.size() != count) {
            throw std::logic_error(
                "strategy evaluation exact attribution result is incomplete");
        }
        std::vector<double> visits_by_class(pairs.size(), 0.0);
        check_owned_cap(capped_add(
            retained_scratch_bytes,
            capped_product(
                visits_by_class.capacity(), sizeof(double))));
        for (std::size_t raw = 0; raw < count; ++raw) {
            const std::uint32_t class_id =
                attribution_class_by_pair[raw];
            if (class_id >= visits_by_class.size()) {
                throw std::logic_error(
                    "strategy evaluation exact attribution class is "
                    "missing");
            }
            visits_by_class[class_id] += exact_visits[raw];
        }
        const std::uint32_t start_class = start_pair;
        if (start_class >= pairs.size()) {
            throw std::logic_error(
                "strategy evaluation exact attribution start class is "
                "missing");
        }
        check_owned_cap(capped_add(
            retained_scratch_bytes,
            capped_add(
                capped_product(
                    visits_by_class.capacity(), sizeof(double)),
                capped_product(
                    pairs.size(), sizeof(solve_detail::WideFloat)))));
        std::vector<solve_detail::WideFloat> quotient_expected(
            pairs.size(), solve_detail::WideFloat{0.0});
        quotient_expected[start_class] = solve_detail::WideFloat{1.0};
        for (std::uint32_t source = 0; source < pairs.size(); ++source) {
            if (source < pair_contracted.size() &&
                pair_contracted[source]) {
                if (chain_next[source] >= quotient_expected.size()) {
                    throw std::logic_error(
                        "strategy evaluation contracted quotient target is "
                        "missing");
                }
                quotient_expected[chain_next[source]] +=
                    solve_detail::WideFloat{visits_by_class[source]};
                continue;
            }
            const EvalRow& row = pair_row(source);
            for (std::size_t transition_index = 0;
                 transition_index < row.transitions.size();
                 ++transition_index) {
                const EvalTransition& transition =
                    row.transitions[transition_index];
                const std::uint32_t via =
                    transition_via(row, transition_index);
                const std::uint32_t target =
                    via != kNoId ? via : transition.target;
                if (target >= quotient_expected.size()) {
                    throw std::logic_error(
                        "strategy evaluation behavioral quotient target is "
                        "missing");
                }
                quotient_expected[target] +=
                    solve_detail::WideFloat{visits_by_class[source]} *
                    solve_detail::WideFloat{transition.probability};
            }
        }
        for (std::size_t class_id = 0;
             class_id < visits_by_class.size(); ++class_id) {
            const double expected = quotient_expected[class_id].value();
            const double residual = std::fabs(
                (solve_detail::WideFloat{visits_by_class[class_id]} -
                 quotient_expected[class_id])
                    .value());
            const double scale = std::max(
                {1.0, std::fabs(visits_by_class[class_id]),
                 std::fabs(expected)});
            const double tolerance =
                std::max(
                    options.epsilon,
                    8.0 * std::numeric_limits<double>::epsilon()) *
                scale;
            if (!std::isfinite(residual) || residual > tolerance) {
                std::ostringstream detail;
                detail << std::setprecision(17)
                       << "strategy evaluation exact attribution does not "
                          "satisfy the behavioral quotient flow equation "
                          "(class="
                       << class_id
                       << ", visits=" << visits_by_class[class_id]
                       << ", expected=" << expected
                       << ", residual=" << residual
                       << ", tolerance=" << tolerance << ')';
                throw std::runtime_error(detail.str());
            }
        }
    }

    solve_detail::CooperativeTask<std::vector<double>>
    solve_shared_row_exact_attribution() {
        using solve_detail::PolicyEdge;
        using solve_detail::PolicyRow;
        using solve_detail::SparsePolicyComponentResult;
        using solve_detail::SparsePolicyComponentStatus;
        using solve_detail::SparsePolicyComponentView;
        using solve_detail::SparsePolicyComponentWorkspace;
        using solve_detail::SparsePolicyResume;
        using solve_detail::SparsePolicyTarjanView;
        const std::size_t count = attribution_pairs.size();
        const std::size_t row_count = attribution_rows.size();
        std::vector<solve_detail::WideFloat> row_visits(
            row_count, solve_detail::WideFloat{0.0});
        {
            std::vector<std::uint32_t> incoming_counts(row_count, 0);
            std::vector<PolicyRow> forward_rows(row_count);
            std::vector<PolicyEdge> forward_edges;
            std::vector<std::uint8_t> row_has_absorption(
                row_count, 0);
            const std::uint64_t aggregation_node_bytes =
                sizeof(std::map<
                    std::uint32_t,
                    solve_detail::WideFloat>::value_type) +
                3 * sizeof(void*);
            std::uint64_t logical_arc_count = 0;
            for (const EvalRow& row : attribution_rows) {
                logical_arc_count = capped_add(
                    logical_arc_count, eval_row_arc_count(row));
            }
            const std::uint64_t unaggregated_projection = capped_add(
                capped_product(
                    logical_arc_count, 2 * sizeof(PolicyEdge)),
                capped_product(
                    row_count,
                    2 * sizeof(PolicyRow) + sizeof(std::uint32_t) +
                        sizeof(std::uint8_t)));
            const std::uint64_t owned_before_transpose =
                fast_estimated_owned_bytes();
            const bool aggregate_target_rows =
                logical_arc_count >
                    std::numeric_limits<std::uint32_t>::max() ||
                owned_before_transpose > options.max_owned_bytes ||
                unaggregated_projection >
                    options.max_owned_bytes -
                        std::min(
                            owned_before_transpose,
                            options.max_owned_bytes);
            const auto compact_graph_bytes = [&]() {
                return capped_add(
                    capped_product(
                        incoming_counts.capacity(),
                        sizeof(std::uint32_t)),
                    capped_add(
                        capped_product(
                            forward_rows.capacity(),
                            sizeof(PolicyRow)),
                        capped_add(
                            capped_product(
                                forward_edges.capacity(),
                                sizeof(PolicyEdge)),
                            capped_product(
                                row_has_absorption.capacity(),
                                sizeof(std::uint8_t)))));
            };
            memory_probe_stage = aggregate_target_rows
                ? "exact_attribution_compact_row_transpose"
                : "exact_attribution_replay_transpose";
            for (std::uint32_t source = 0;
                 source < row_count; ++source) {
                std::map<std::uint32_t, solve_detail::WideFloat>
                    aggregated;
                forward_rows[source].edge_offset =
                    forward_edges.size();
                visit_eval_row(
                    attribution_rows[source], attribution_pairs,
                    [&](const EvalTransition& transition) {
                        if (transition.target >= count ||
                            attribution_pairs[transition.target].row >=
                                row_count) {
                            throw std::logic_error(
                                "strategy evaluation exact attribution row "
                                "target is missing");
                        }
                        const std::uint32_t target_row =
                            attribution_pairs[transition.target].row;
                        if (!aggregate_target_rows) {
                            if (forward_edges.size() ==
                                std::numeric_limits<std::uint32_t>::max()) {
                                throw std::length_error(
                                    "strategy evaluation exact row "
                                    "attribution edge count overflowed");
                            }
                            if (incoming_counts[target_row] ==
                                std::numeric_limits<std::uint32_t>::max()) {
                                throw std::length_error(
                                    "strategy evaluation exact row "
                                    "attribution incoming edge count "
                                    "overflowed");
                            }
                            ++incoming_counts[target_row];
                            forward_edges.push_back({
                                target_row, transition.probability});
                            return;
                        }
                        auto found = aggregated.find(target_row);
                        if (found == aggregated.end()) {
                            check_owned_cap(capped_add(
                                compact_graph_bytes(),
                                capped_product(
                                    aggregated.size() + 1,
                                    aggregation_node_bytes)));
                            found = aggregated.emplace(
                                target_row,
                                solve_detail::WideFloat{0.0}).first;
                        }
                        found->second += solve_detail::WideFloat{
                            transition.probability};
                    },
                    [&](const EvalAbsorption&) {
                        row_has_absorption[source] = 1;
                    });
                if (aggregate_target_rows &&
                    (aggregated.size() >
                        std::numeric_limits<std::uint32_t>::max() ||
                    forward_edges.size() >
                        std::numeric_limits<std::uint32_t>::max() -
                            aggregated.size())) {
                    throw std::length_error(
                        "strategy evaluation exact row attribution edge "
                        "count overflowed");
                }
                if (aggregate_target_rows) {
                    for (const auto& [target_row, probability] : aggregated) {
                        if (incoming_counts[target_row] ==
                            std::numeric_limits<std::uint32_t>::max()) {
                            throw std::length_error(
                                "strategy evaluation exact row attribution "
                                "incoming edge count overflowed");
                        }
                        ++incoming_counts[target_row];
                        forward_edges.push_back({
                            target_row, probability.value()});
                    }
                }
                forward_rows[source].edge_count =
                    static_cast<std::uint32_t>(
                        forward_edges.size() -
                        forward_rows[source].edge_offset);
                memory_probe_units = forward_edges.size();
                memory_probe_unit_bytes = sizeof(PolicyEdge);
                check_owned_cap(compact_graph_bytes());
                if ((source & 31u) == 31u) {
                    co_await solve_detail::CooperativeCheckpoint{
                        compact_graph_bytes()};
                }
            }

            const std::uint64_t edge_count = forward_edges.size();
            output.operation_row_census.compact_attribution_rows =
                row_count;
            output.operation_row_census.compact_attribution_edges =
                edge_count;
            check_owned_cap(capped_add(
                compact_graph_bytes(),
                capped_add(
                    capped_product(row_count, sizeof(PolicyRow)),
                    capped_product(edge_count, sizeof(PolicyEdge)))));
            std::vector<PolicyRow> transpose_rows(row_count);
            std::vector<PolicyEdge> transpose_edges(
                static_cast<std::size_t>(edge_count));
            std::vector<std::uint32_t> cursors(row_count, 0);
            std::uint64_t offset = 0;
            for (std::uint32_t target = 0;
                 target < row_count; ++target) {
                transpose_rows[target].edge_offset = offset;
                transpose_rows[target].edge_count =
                    incoming_counts[target];
                cursors[target] = static_cast<std::uint32_t>(offset);
                offset += incoming_counts[target];
            }
            for (std::uint32_t source = 0; source < row_count; ++source) {
                const PolicyRow& row = forward_rows[source];
                for (std::uint32_t edge_index = 0;
                     edge_index < row.edge_count; ++edge_index) {
                    const PolicyEdge& edge = forward_edges.at(
                        row.edge_offset + edge_index);
                    transpose_edges[cursors[edge.target]++] = {
                        source, edge.probability};
                }
                if ((source & 127u) == 127u) {
                    co_await solve_detail::CooperativeCheckpoint{
                        capped_add(
                            compact_graph_bytes(),
                            capped_add(
                            capped_product(
                                transpose_rows.capacity(),
                                sizeof(PolicyRow)),
                            capped_product(
                                transpose_edges.capacity(),
                                sizeof(PolicyEdge))))};
                }
            }
            memory_probe_stage = "steady_state";
            memory_probe_units = 0;
            memory_probe_unit_bytes = 0;

            std::vector<std::uint32_t> active_states(row_count);
            std::vector<std::uint8_t> active(row_count, 1);
            std::vector<std::uint8_t> terminal(row_count, 0);
            std::vector<std::uint64_t> policy_rows(row_count);
            for (std::uint32_t row = 0; row < row_count; ++row) {
                active_states[row] = row;
                policy_rows[row] = row;
            }
            const std::vector<std::uint32_t> no_representatives;
            SparsePolicyComponentWorkspace workspace;
            std::vector<double> external_incoming(row_count, 0.0);
            std::vector<double> previous_values(row_count, 0.0);
            std::vector<std::uint32_t> raw_pairs_by_class(
                pairs.size(), 0);
            if (pair_visits.size() != pairs.size() ||
                attribution_class_by_pair.size() != count) {
                throw std::logic_error(
                    "strategy evaluation exact row attribution quotient "
                    "seed is incomplete");
            }
            for (std::uint32_t raw = 0; raw < count; ++raw) {
                const std::uint32_t class_id =
                    attribution_class_by_pair[raw];
                if (class_id >= raw_pairs_by_class.size()) {
                    throw std::logic_error(
                        "strategy evaluation exact row attribution class "
                        "seed is missing");
                }
                ++raw_pairs_by_class[class_id];
            }
            /* The quotient solve has already proved each behavioral class's
             * exact total visits. Distribute that total deterministically
             * across its raw members, then aggregate by shared exact row.
             * This is only the Krylov/Gauss-Seidel initial iterate: the row
             * equations, reconstructed raw equations, and quotient equations
             * below remain the acceptance authority. Seeding the already
             * solved aggregate modes avoids relearning a near-renewal visit
             * total through millions of repeated raw transitions. */
            for (std::uint32_t raw = 0; raw < count; ++raw) {
                const std::uint32_t class_id =
                    attribution_class_by_pair[raw];
                const std::uint32_t class_size =
                    raw_pairs_by_class[class_id];
                if (class_size == 0) {
                    throw std::logic_error(
                        "strategy evaluation exact row attribution class "
                        "seed is empty");
                }
                previous_values.at(attribution_pairs[raw].row) +=
                    pair_visits[class_id] /
                    static_cast<double>(class_size);
            }
            external_incoming.at(
                attribution_pairs.at(attribution_start_pair).row) = 1.0;

            const auto transient_bytes =
                [&](const std::uint64_t scratch = 0) {
                    std::uint64_t bytes = scratch;
                    const auto add_vector = [&](const auto& values) {
                        using Value = typename std::decay_t<
                            decltype(values)>::value_type;
                        bytes = capped_add(
                            bytes,
                            capped_product(
                                values.capacity(), sizeof(Value)));
                    };
                    add_vector(incoming_counts);
                    add_vector(forward_rows);
                    add_vector(forward_edges);
                    add_vector(row_has_absorption);
                    add_vector(transpose_rows);
                    add_vector(transpose_edges);
                    add_vector(cursors);
                    add_vector(active_states);
                    add_vector(active);
                    add_vector(terminal);
                    add_vector(policy_rows);
                    add_vector(workspace.components);
                    for (const auto& component : workspace.components) {
                        add_vector(component);
                    }
                    add_vector(workspace.component_by_state);
                    add_vector(workspace.local);
                    add_vector(workspace.tarjan_index);
                    add_vector(workspace.tarjan_lowlink);
                    add_vector(workspace.tarjan_on_stack);
                    add_vector(workspace.tarjan_stack);
                    add_vector(workspace.tarjan_dfs);
                    add_vector(external_incoming);
                    add_vector(row_visits);
                    add_vector(previous_values);
                    add_vector(raw_pairs_by_class);
                    return bytes;
                };
            check_owned_cap(transient_bytes());
            const SparsePolicyTarjanView tarjan{
                active_states, active, terminal, policy_rows,
                no_representatives, transpose_rows, transpose_edges};
            while (!solve_detail::advance_sparse_policy_components(
                tarjan, workspace, 65536)) {
                check_owned_cap(transient_bytes());
                co_await solve_detail::CooperativeCheckpoint{
                    transient_bytes()};
            }
            check_owned_cap(transient_bytes());
            co_await solve_detail::CooperativeCheckpoint{
                transient_bytes()};

            for (std::uint32_t component = 0;
                 component < workspace.components.size(); ++component) {
                const std::vector<std::uint32_t>& members =
                    workspace.components[component];
                std::vector<double> rhs;
                rhs.reserve(members.size());
                double input_mass = 0.0;
                for (std::size_t local = 0;
                     local < members.size(); ++local) {
                    const std::uint32_t row = members[local];
                    workspace.local[row] =
                        static_cast<std::int32_t>(local);
                    rhs.push_back(external_incoming[row]);
                    input_mass += rhs.back();
                }
                if (!(input_mass > 0.0)) continue;

                bool has_exit = false;
                for (const std::uint32_t source : members) {
                    if (row_has_absorption[source]) has_exit = true;
                    const PolicyRow& row = forward_rows[source];
                    for (std::uint32_t edge_index = 0;
                         edge_index < row.edge_count; ++edge_index) {
                        const PolicyEdge& edge = forward_edges.at(
                            row.edge_offset + edge_index);
                        if (workspace.component_by_state[edge.target] !=
                            component) {
                            has_exit = true;
                        }
                    }
                }
                if (!has_exit) {
                    for (std::size_t local = 0;
                         local < members.size(); ++local) {
                        row_visits[members[local]] +=
                            solve_detail::WideFloat{rhs[local]};
                    }
                    continue;
                }

                std::unique_ptr<SparsePolicyResume> resume;
                SparsePolicyComponentResult solved;
                std::vector<solve_detail::WideFloat> solved_wide;
                do {
                    const std::uint64_t scratch = capped_add(
                        solve_detail::
                            sparse_policy_component_scratch_bytes(
                                members.size(), true),
                        capped_product(
                            members.size(),
                            sizeof(double) + sizeof(std::uint32_t)));
                    std::uint64_t solve_transient = capped_add(
                        transient_bytes(), scratch);
                    solve_transient = capped_add(
                        solve_transient,
                        capped_product(
                            rhs.capacity(), sizeof(double)));
                    check_owned_cap(solve_transient);
                    solved =
                        solve_detail::advance_sparse_policy_component(
                            SparsePolicyComponentView{
                                members, component,
                                workspace.component_by_state,
                                workspace.local, transpose_rows,
                                transpose_edges, rhs, previous_values,
                                options.max_sweeps},
                            resume, &solved_wide);
                    std::uint64_t retained_solve = capped_add(
                        transient_bytes(),
                        capped_add(
                            capped_product(
                                rhs.capacity(), sizeof(double)),
                            capped_product(
                                solved.values.capacity(),
                                sizeof(double))));
                    if (resume != nullptr) {
                        retained_solve = capped_add(
                            retained_solve,
                            sizeof(SparsePolicyResume));
                        retained_solve = capped_add(
                            retained_solve,
                            capped_product(
                                resume->members.capacity(),
                                sizeof(std::uint32_t)));
                        const auto add_wide = [&](const auto& values) {
                            retained_solve = capped_add(
                                retained_solve,
                                capped_product(
                                    values.capacity(),
                                    sizeof(solve_detail::WideFloat)));
                        };
                        add_wide(resume->b);
                        add_wide(resume->x);
                        add_wide(resume->r);
                        add_wide(resume->r0);
                        add_wide(resume->p);
                        add_wide(resume->v);
                        add_wide(resume->s);
                        add_wide(resume->t);
                    }
                    retained_solve = capped_add(
                        retained_solve,
                        capped_product(
                            solved_wide.capacity(),
                            sizeof(solve_detail::WideFloat)));
                    check_owned_cap(retained_solve);
                    co_await solve_detail::CooperativeCheckpoint{
                        retained_solve};
                } while (solved.status ==
                         SparsePolicyComponentStatus::Incomplete);
                if (solved.status ==
                    SparsePolicyComponentStatus::DidNotConverge) {
                    throw std::length_error(
                        "strategy evaluation exact row attribution reached "
                        "max_sweeps (" +
                        std::to_string(options.max_sweeps) + ")");
                }
                if (solved.status !=
                        SparsePolicyComponentStatus::Complete ||
                    solved.values.size() != members.size() ||
                    solved_wide.size() != members.size()) {
                    throw std::runtime_error(
                        "strategy evaluation exact row attribution solve "
                        "failed (component_size=" +
                        std::to_string(members.size()) +
                        ", status=" +
                        std::to_string(static_cast<unsigned int>(
                            solved.status)) +
                        ", iterations=" +
                        std::to_string(solved.total_iterations) + ")");
                }
                for (std::size_t local = 0;
                     local < members.size(); ++local) {
                    solve_detail::WideFloat expected = rhs[local];
                    const PolicyRow& row =
                        transpose_rows[members[local]];
                    for (std::uint32_t edge_index = 0;
                         edge_index < row.edge_count; ++edge_index) {
                        const PolicyEdge& edge = transpose_edges.at(
                            row.edge_offset + edge_index);
                        if (workspace.component_by_state[edge.target] !=
                            component) {
                            continue;
                        }
                        const std::int32_t source_local =
                            workspace.local.at(edge.target);
                        if (source_local < 0) {
                            throw std::logic_error(
                                "strategy evaluation exact row attribution "
                                "has an invalid component-local source");
                        }
                        expected +=
                            solve_detail::WideFloat{edge.probability} *
                            solve_detail::WideFloat{
                                solved.values[
                                    static_cast<std::size_t>(
                                        source_local)]};
                    }
                    const double residual = std::fabs(
                        (solve_detail::WideFloat{solved.values[local]} -
                         expected)
                            .value());
                    const double scale = std::max(
                        {1.0, std::fabs(solved.values[local]),
                         std::fabs(expected.value())});
                    const double tolerance =
                        std::max(
                            options.epsilon,
                            8.0 * std::numeric_limits<double>::epsilon()) *
                        scale;
                    if (!std::isfinite(residual) ||
                        residual > tolerance) {
                        throw std::runtime_error(
                            "strategy evaluation exact row attribution "
                            "component residual exceeded epsilon");
                    }
                }
                for (std::size_t local = 0;
                     local < members.size(); ++local) {
                    const double value = solved.values[local];
                    if (!std::isfinite(value) || value < -1e-10) {
                        throw std::runtime_error(
                            "strategy evaluation exact row attribution "
                            "produced an invalid occupancy");
                    }
                    row_visits[members[local]] =
                        value < 0.0
                            ? solve_detail::WideFloat{0.0}
                            : solved_wide[local];
                }
                for (const std::uint32_t source : members) {
                    const PolicyRow& row = forward_rows[source];
                    for (std::uint32_t edge_index = 0;
                         edge_index < row.edge_count; ++edge_index) {
                        const PolicyEdge& edge = forward_edges.at(
                            row.edge_offset + edge_index);
                        if (workspace.component_by_state[edge.target] !=
                            component) {
                            external_incoming[edge.target] +=
                                row_visits[source].value() *
                                edge.probability;
                        }
                    }
                }
                co_await solve_detail::CooperativeCheckpoint{
                    transient_bytes()};
            }
        }

        check_owned_cap(capped_add(
            capped_product(
                row_visits.capacity(),
                sizeof(solve_detail::WideFloat)),
            capped_product(
                count,
                sizeof(double) +
                    2 * sizeof(solve_detail::WideFloat))));
        std::vector<double> exact_visits(count, 0.0);
        std::vector<solve_detail::WideFloat> reconstructed(
            count, solve_detail::WideFloat{0.0});
        reconstructed[attribution_start_pair] =
            solve_detail::WideFloat{1.0};
        for (std::uint32_t row_id = 0; row_id < row_count; ++row_id) {
            const double value = row_visits[row_id].value();
            if (!std::isfinite(value) || value < -1e-10) {
                throw std::runtime_error(
                    "strategy evaluation shared-row exact attribution "
                    "produced an invalid row occupancy");
            }
            visit_eval_row(
                attribution_rows[row_id], attribution_pairs,
                [&](const EvalTransition& transition) {
                    reconstructed.at(transition.target) +=
                        (value < 0.0
                             ? solve_detail::WideFloat{0.0}
                             : row_visits[row_id]) *
                        solve_detail::WideFloat{
                            transition.probability};
                },
                [](const EvalAbsorption&) {});
            if ((row_id & 255u) == 255u) {
                co_await solve_detail::CooperativeCheckpoint{
                    capped_product(
                        row_visits.capacity() + reconstructed.capacity(),
                        sizeof(solve_detail::WideFloat))};
            }
        }
        for (std::uint32_t state = 0; state < count; ++state) {
            const double value = reconstructed[state].value();
            if (!std::isfinite(value) || value < -1e-10) {
                throw std::runtime_error(
                    "strategy evaluation shared-row exact attribution "
                    "produced an invalid raw occupancy");
            }
            exact_visits[state] = std::max(0.0, value);
        }

        std::vector<solve_detail::WideFloat> checked_row_mass(
            row_count, solve_detail::WideFloat{0.0});
        for (std::uint32_t source = 0; source < count; ++source) {
            checked_row_mass.at(attribution_pairs[source].row) +=
                solve_detail::WideFloat{exact_visits[source]};
        }
        for (std::uint32_t row_id = 0; row_id < row_count; ++row_id) {
            const double residual = std::fabs(
                (checked_row_mass[row_id] - row_visits[row_id])
                    .value());
            const double scale = std::max(
                {1.0, std::fabs(checked_row_mass[row_id].value()),
                 std::fabs(row_visits[row_id].value())});
            const double tolerance =
                std::max(
                    options.epsilon,
                    8.0 * std::numeric_limits<double>::epsilon()) *
                scale;
            if (!std::isfinite(residual) || residual > tolerance) {
                throw std::runtime_error(
                    "strategy evaluation shared-row exact attribution "
                    "disaggregation residual exceeded epsilon");
            }
        }
        std::uint64_t retained_scratch = capped_product(
            row_visits.capacity(), sizeof(solve_detail::WideFloat));
        retained_scratch = capped_add(
            retained_scratch,
            capped_product(exact_visits.capacity(), sizeof(double)));
        retained_scratch = capped_add(
            retained_scratch,
            capped_product(
                checked_row_mass.capacity(),
                sizeof(solve_detail::WideFloat)));
        retained_scratch = capped_add(
            retained_scratch,
            capped_product(
                reconstructed.capacity(),
                sizeof(solve_detail::WideFloat)));
        validate_exact_attribution_quotient(
            exact_visits, retained_scratch);
        attribution_exact_row_visits = std::move(row_visits);
        co_return exact_visits;
    }

    solve_detail::CooperativeTask<std::vector<double>>
    solve_exact_attribution() {
        using solve_detail::PolicyEdge;
        using solve_detail::PolicyRow;
        using solve_detail::SparsePolicyComponentResult;
        using solve_detail::SparsePolicyComponentStatus;
        using solve_detail::SparsePolicyComponentView;
        using solve_detail::SparsePolicyComponentWorkspace;
        using solve_detail::SparsePolicyResume;
        using solve_detail::SparsePolicyTarjanView;

        const std::size_t count = attribution_pairs.size();
        if (count == 0 || attribution_start_pair >= count ||
            attribution_rows.empty() ||
            attribution_class_by_pair.size() != count) {
            throw std::logic_error(
                "strategy evaluation exact attribution graph is incomplete");
        }
        if (attribution_rows.size() < count ||
            has_replayable_rows(attribution_rows)) {
            auto shared = solve_shared_row_exact_attribution();
            while (!shared.resume()) {
                co_await solve_detail::CooperativeCheckpoint{
                    shared.retained_bytes()};
            }
            co_return shared.take_result();
        }

        std::uint64_t edge_count = 0;
        std::uint32_t validated_attribution_rows = 0;
        for (const EvalRow& row : attribution_rows) {
            for (const EvalTransition& transition : row.transitions) {
                if (transition.target >= count) {
                    throw std::logic_error(
                        "strategy evaluation exact attribution target is "
                        "missing");
                }
            }
            if ((++validated_attribution_rows & 127u) == 0u) {
                co_await solve_detail::CooperativeCheckpoint{};
            }
        }
        for (std::uint32_t projected_pairs = 0;
             projected_pairs < attribution_pairs.size();
             ++projected_pairs) {
            const EvalPair& pair = attribution_pairs[projected_pairs];
            if (pair.row >= attribution_rows.size()) {
                throw std::logic_error(
                    "strategy evaluation exact attribution row is missing");
            }
            edge_count = capped_add(
                edge_count,
                attribution_rows[pair.row].transitions.size());
            if (((projected_pairs + 1) & 127u) == 0u) {
                co_await solve_detail::CooperativeCheckpoint{};
            }
        }
        /* The raw attribution transpose expands every shared row once per
         * pair. Prefer the exact shared-row solver whenever that expansion
         * cannot fit the evaluator's remaining owned-byte envelope, not only
         * after the edge count has overflowed a 32-bit index. */
        memory_probe_stage =
            "exact_attribution_expanded_transpose";
        memory_probe_units = edge_count;
        memory_probe_unit_bytes = sizeof(PolicyEdge);
        std::uint64_t transpose_projection = capped_product(
            edge_count, sizeof(PolicyEdge));
        transpose_projection = capped_add(
            transpose_projection,
            capped_product(
                count,
                sizeof(std::uint32_t) + sizeof(PolicyRow) +
                    sizeof(std::uint32_t)));
        const std::uint64_t owned_before_transpose =
            fast_estimated_owned_bytes();
        const bool transpose_exceeds_memory =
            owned_before_transpose > options.max_owned_bytes ||
            transpose_projection >
                options.max_owned_bytes - owned_before_transpose;
        /* Shared rows are exact transition/absorption authorities, and the
         * shared-row solve is followed by raw-pair and quotient flow
         * residual checks. Prefer it whenever it actually contracts the raw
         * attribution graph; expanding one identical transpose row per pair
         * can create a large near-renewal SCC that spends the entire public
         * iteration budget relearning an already-solved aggregate mode. */
        if (edge_count > std::numeric_limits<std::uint32_t>::max() ||
            transpose_exceeds_memory) {
            memory_probe_stage = "steady_state";
            memory_probe_units = 0;
            memory_probe_unit_bytes = 0;
            auto shared = solve_shared_row_exact_attribution();
            while (!shared.resume()) {
                co_await solve_detail::CooperativeCheckpoint{
                    shared.retained_bytes()};
            }
            co_return shared.take_result();
        }
        check_owned_cap(transpose_projection);
        memory_probe_stage = "steady_state";
        memory_probe_units = 0;
        memory_probe_unit_bytes = 0;

        std::vector<std::uint32_t> incoming_counts(count, 0);
        for (std::uint32_t pair_id = 0;
             pair_id < attribution_pairs.size(); ++pair_id) {
            const EvalPair& pair = attribution_pairs[pair_id];
            for (const EvalTransition& transition :
                 attribution_rows[pair.row].transitions) {
                if (incoming_counts[transition.target] ==
                    std::numeric_limits<std::uint32_t>::max()) {
                    throw std::length_error(
                        "strategy evaluation exact attribution edge count "
                        "overflowed");
                }
                ++incoming_counts[transition.target];
            }
        }

        std::vector<PolicyRow> transpose_rows(count);
        std::vector<PolicyEdge> transpose_edges(
            static_cast<std::size_t>(edge_count));
        std::vector<std::uint32_t> cursors(count, 0);
        std::uint64_t offset = 0;
        for (std::uint32_t target = 0; target < count; ++target) {
            transpose_rows[target].edge_offset = offset;
            transpose_rows[target].edge_count = incoming_counts[target];
            cursors[target] = static_cast<std::uint32_t>(offset);
            offset += incoming_counts[target];
        }
        for (std::uint32_t source = 0; source < count; ++source) {
            const EvalRow& row =
                attribution_rows[attribution_pairs[source].row];
            for (const EvalTransition& transition : row.transitions) {
                transpose_edges[cursors[transition.target]++] = {
                    source, transition.probability};
            }
        }

        std::vector<std::uint32_t> active_states(count);
        std::vector<std::uint8_t> active(count, 1);
        std::vector<std::uint8_t> terminal(count, 0);
        std::vector<std::uint64_t> policy_rows(count);
        for (std::uint32_t state = 0; state < count; ++state) {
            active_states[state] = state;
            policy_rows[state] = state;
        }
        const std::vector<std::uint32_t> no_representatives;
        SparsePolicyComponentWorkspace workspace;
        std::vector<double> external_incoming_exact(count, 0.0);
        std::vector<double> exact_visits(count, 0.0);
        std::vector<double> previous_values(count, 0.0);
        std::vector<double> visits_by_class(pairs.size(), 0.0);
        external_incoming_exact[attribution_start_pair] = 1.0;

        const auto transient_bytes = [&](const std::uint64_t scratch = 0) {
            std::uint64_t bytes = scratch;
            const auto add_vector = [&](const auto& values) {
                using Value = typename std::decay_t<decltype(values)>::value_type;
                bytes = capped_add(
                    bytes,
                    capped_product(values.capacity(), sizeof(Value)));
            };
            add_vector(incoming_counts);
            add_vector(transpose_rows);
            add_vector(transpose_edges);
            add_vector(cursors);
            add_vector(active_states);
            add_vector(active);
            add_vector(terminal);
            add_vector(policy_rows);
            add_vector(workspace.components);
            for (const auto& component : workspace.components) {
                add_vector(component);
            }
            add_vector(workspace.component_by_state);
            add_vector(workspace.local);
            add_vector(workspace.tarjan_index);
            add_vector(workspace.tarjan_lowlink);
            add_vector(workspace.tarjan_on_stack);
            add_vector(workspace.tarjan_stack);
            add_vector(workspace.tarjan_dfs);
            add_vector(external_incoming_exact);
            add_vector(exact_visits);
            add_vector(previous_values);
            add_vector(visits_by_class);
            return bytes;
        };
        check_owned_cap(transient_bytes());
        const SparsePolicyTarjanView tarjan{
            active_states, active, terminal, policy_rows,
            no_representatives, transpose_rows, transpose_edges};
        while (!solve_detail::advance_sparse_policy_components(
            tarjan, workspace, 65536)) {
            check_owned_cap(transient_bytes());
        }
        check_owned_cap(transient_bytes());

        for (std::uint32_t component = 0;
             component < workspace.components.size(); ++component) {
            const std::vector<std::uint32_t>& members =
                workspace.components[component];
            std::vector<double> rhs;
            rhs.reserve(members.size());
            double input_mass = 0.0;
            for (std::size_t local = 0; local < members.size(); ++local) {
                const std::uint32_t state = members[local];
                workspace.local[state] = static_cast<std::int32_t>(local);
                rhs.push_back(external_incoming_exact[state]);
                input_mass += rhs.back();
            }
            if (!(input_mass > 0.0)) continue;

            bool has_exit = false;
            for (const std::uint32_t source : members) {
                const EvalRow& row =
                    attribution_rows[attribution_pairs[source].row];
                if (!row.absorptions.empty()) has_exit = true;
                for (const EvalTransition& transition : row.transitions) {
                    if (workspace.component_by_state[transition.target] !=
                        component) {
                        has_exit = true;
                    }
                }
            }
            if (!has_exit) {
                for (std::size_t local = 0; local < members.size(); ++local) {
                    exact_visits[members[local]] += rhs[local];
                }
                continue;
            }

            std::unique_ptr<SparsePolicyResume> resume;
            SparsePolicyComponentResult solved;
            do {
                const std::uint64_t scratch = capped_add(
                    solve_detail::sparse_policy_component_scratch_bytes(
                        members.size(), true),
                    capped_product(
                        members.size(),
                        sizeof(double) + sizeof(std::uint32_t)));
                std::uint64_t solve_transient =
                    capped_add(transient_bytes(), scratch);
                solve_transient = capped_add(
                    solve_transient,
                    capped_product(rhs.capacity(), sizeof(double)));
                /* The shared scratch authority includes both the retained
                 * incomplete result capacity and retained resume, plus the
                 * fresh allocations that coexist with them during this call. */
                check_owned_cap(solve_transient);
                solved = solve_detail::advance_sparse_policy_component(
                    SparsePolicyComponentView{
                        members, component,
                        workspace.component_by_state, workspace.local,
                        transpose_rows, transpose_edges, rhs,
                        previous_values,
                        options.max_sweeps},
                    resume);
                std::uint64_t retained_solve = capped_add(
                    transient_bytes(),
                    capped_add(
                        capped_product(rhs.capacity(), sizeof(double)),
                        capped_product(
                            solved.values.capacity(), sizeof(double))));
                if (resume != nullptr) {
                    retained_solve = capped_add(
                        retained_solve, sizeof(SparsePolicyResume));
                    retained_solve = capped_add(
                        retained_solve,
                        capped_product(
                            resume->members.capacity(),
                            sizeof(std::uint32_t)));
                    const auto add_wide = [&](const auto& values) {
                        retained_solve = capped_add(
                            retained_solve,
                            capped_product(
                                values.capacity(),
                                sizeof(solve_detail::WideFloat)));
                    };
                    add_wide(resume->b);
                    add_wide(resume->x);
                    add_wide(resume->r);
                    add_wide(resume->r0);
                    add_wide(resume->p);
                    add_wide(resume->v);
                    add_wide(resume->s);
                    add_wide(resume->t);
                }
                check_owned_cap(retained_solve);
            } while (solved.status ==
                     SparsePolicyComponentStatus::Incomplete);
            if (solved.status ==
                SparsePolicyComponentStatus::DidNotConverge) {
                throw std::length_error(
                    "strategy evaluation exact attribution reached "
                    "max_sweeps (" +
                    std::to_string(options.max_sweeps) + ")");
            }
            if (solved.status != SparsePolicyComponentStatus::Complete ||
                solved.values.size() != members.size()) {
                throw std::runtime_error(
                    "strategy evaluation exact attribution solve failed "
                    "(component_size=" +
                    std::to_string(members.size()) +
                    ", status=" +
                    std::to_string(static_cast<unsigned int>(
                        solved.status)) +
                    ", iterations=" +
                    std::to_string(solved.total_iterations) + ")");
            }
            /* Validate every raw attribution equation before quotient-class
             * aggregation can cancel opposing errors. The shared solver
             * proves its WideFloat iterate; this second check also covers the
             * returned-double conversion and the dense solve path. */
            for (std::size_t local = 0;
                 local < members.size(); ++local) {
                solve_detail::WideFloat expected = rhs[local];
                const PolicyRow& row =
                    transpose_rows[members[local]];
                for (std::uint32_t edge_index = 0;
                     edge_index < row.edge_count; ++edge_index) {
                    const PolicyEdge& edge =
                        transpose_edges.at(
                            row.edge_offset + edge_index);
                    if (workspace.component_by_state[edge.target] !=
                        component) {
                        continue;
                    }
                    const std::int32_t successor_local =
                        workspace.local.at(edge.target);
                    if (successor_local < 0) {
                        throw std::logic_error(
                            "strategy evaluation exact attribution has an "
                            "invalid component-local successor");
                    }
                    expected +=
                        solve_detail::WideFloat{edge.probability} *
                        solve_detail::WideFloat{
                            solved.values[static_cast<std::size_t>(
                                successor_local)]};
                }
                const double raw_residual = std::fabs(
                    (solve_detail::WideFloat{solved.values[local]} -
                     expected)
                        .value());
                const double residual_scale = std::max(
                    {1.0,
                     std::fabs(solved.values[local]),
                     std::fabs(expected.value())});
                /* The shared component solver proves its WideFloat iterate,
                 * but this audit intentionally rechecks the public double
                 * values. A caller may request an epsilon below what those
                 * returned doubles can represent (the stepped web
                 * cancellation probe uses 1e-30). Retain the strict raw-row
                 * check while flooring only its comparison at a conservative
                 * double round-trip bound. */
                const double representable_relative_tolerance =
                    8.0 * std::numeric_limits<double>::epsilon();
                const double residual_tolerance =
                    std::max(
                        options.epsilon,
                        representable_relative_tolerance) *
                    residual_scale;
                if (!std::isfinite(raw_residual) ||
                    raw_residual > residual_tolerance) {
                    throw std::runtime_error(
                        "strategy evaluation exact attribution raw "
                        "component residual exceeded epsilon");
                }
            }
            for (std::size_t local = 0; local < members.size(); ++local) {
                const double value = solved.values[local];
                if (!std::isfinite(value) || value < -1e-10) {
                    throw std::runtime_error(
                        "strategy evaluation exact attribution produced an "
                        "invalid occupancy");
                }
                exact_visits[members[local]] = std::max(0.0, value);
            }
            for (const std::uint32_t source : members) {
                const EvalRow& row =
                    attribution_rows[attribution_pairs[source].row];
                for (const EvalTransition& transition : row.transitions) {
                    if (workspace.component_by_state[transition.target] !=
                        component) {
                        external_incoming_exact[transition.target] +=
                            exact_visits[source] * transition.probability;
                    }
                }
            }
        }

        for (std::size_t raw = 0; raw < count; ++raw) {
            const std::uint32_t class_id =
                attribution_class_by_pair[raw];
            if (class_id >= visits_by_class.size()) {
                throw std::logic_error(
                    "strategy evaluation exact attribution class is "
                    "missing");
            }
            visits_by_class[class_id] += exact_visits[raw];
        }
        const std::uint32_t start_class = start_pair;
        if (start_class >= pairs.size()) {
            throw std::logic_error(
                "strategy evaluation exact attribution start class is "
                "missing");
        }
        check_owned_cap(capped_add(
            transient_bytes(),
            capped_product(
                pairs.size(), sizeof(solve_detail::WideFloat))));
        std::vector<solve_detail::WideFloat> quotient_expected(
            pairs.size(), solve_detail::WideFloat{0.0});
        quotient_expected[start_class] = solve_detail::WideFloat{1.0};
        for (std::uint32_t source = 0; source < pairs.size(); ++source) {
            if (source < pair_contracted.size() &&
                pair_contracted[source]) {
                if (chain_next[source] >= quotient_expected.size()) {
                    throw std::logic_error(
                        "strategy evaluation contracted quotient target is "
                        "missing");
                }
                quotient_expected[chain_next[source]] +=
                    solve_detail::WideFloat{visits_by_class[source]};
                continue;
            }
            const EvalRow& row = pair_row(source);
            for (std::size_t transition_index = 0;
                 transition_index < row.transitions.size();
                 ++transition_index) {
                const EvalTransition& transition =
                    row.transitions[transition_index];
                const std::uint32_t via =
                    transition_via(row, transition_index);
                const std::uint32_t target =
                    via != kNoId ? via : transition.target;
                if (target >= quotient_expected.size()) {
                    throw std::logic_error(
                        "strategy evaluation behavioral quotient target is "
                        "missing");
                }
                quotient_expected[target] +=
                    solve_detail::WideFloat{visits_by_class[source]} *
                    solve_detail::WideFloat{transition.probability};
            }
        }
        check_owned_cap(capped_add(
            transient_bytes(),
            capped_product(
                quotient_expected.capacity(),
                sizeof(solve_detail::WideFloat))));
        for (std::size_t class_id = 0;
             class_id < visits_by_class.size(); ++class_id) {
            const double expected = quotient_expected[class_id].value();
            const double residual = std::fabs(
                (solve_detail::WideFloat{visits_by_class[class_id]} -
                 quotient_expected[class_id])
                    .value());
            const double scale = std::max(
                {1.0, std::fabs(visits_by_class[class_id]),
                 std::fabs(expected)});
            const double tolerance =
                std::max(
                    options.epsilon,
                    8.0 * std::numeric_limits<double>::epsilon()) *
                scale;
            if (!std::isfinite(residual) || residual > tolerance) {
                std::ostringstream detail;
                detail << std::setprecision(17)
                       << "strategy evaluation exact attribution does not "
                          "satisfy the behavioral quotient flow equation "
                          "(class="
                       << class_id
                       << ", visits=" << visits_by_class[class_id]
                       << ", expected=" << expected
                       << ", residual=" << residual
                       << ", tolerance=" << tolerance << ')';
                throw std::runtime_error(detail.str());
            }
        }
        check_owned_cap(transient_bytes());
        co_return exact_visits;
    }

    solve_detail::CooperativeTask<bool> finalize() {
        co_await solve_detail::CooperativeCheckpoint{};
        auto chain_propagation = propagate_chain_inflow();
        while (!chain_propagation.resume()) {
            co_await solve_detail::CooperativeCheckpoint{
                chain_propagation.retained_bytes()};
        }
        (void)chain_propagation.take_result();
        std::vector<double> exact_pair_visits_owned;
        const std::vector<double>* exact_pair_visits = &pair_visits;
        const solve_detail::SegmentedVector<EvalPair>* exact_pairs = &pairs;
        if (!attribution_pairs.empty()) {
            auto exact_attribution = solve_exact_attribution();
            while (!exact_attribution.resume()) {
                co_await solve_detail::CooperativeCheckpoint{
                    exact_attribution.retained_bytes()};
            }
            exact_pair_visits_owned =
                exact_attribution.take_result();
            exact_pair_visits = &exact_pair_visits_owned;
            exact_pairs = &attribution_pairs;
            for (auto& incoming : terminal_incoming) incoming.clear();
            for (auto& incoming : compressed_policy_incoming) {
                incoming.clear();
            }
            deterministic_route_flows.clear();
            terminal_incoming_owned_bytes = 0;
            compressed_policy_incoming_owned_bytes = 0;
            const bool exact_row_visits_available =
                !attribution_exact_row_visits.empty();
            std::vector<solve_detail::WideFloat> visits_by_row;
            if (!exact_row_visits_available) {
                visits_by_row.assign(
                    attribution_rows.size(),
                    solve_detail::WideFloat{0.0});
            } else {
                if (attribution_exact_row_visits.size() !=
                    attribution_rows.size()) {
                    throw std::logic_error(
                        "strategy evaluation exact row occupancy is "
                        "incomplete");
                }
                visits_by_row =
                    std::move(attribution_exact_row_visits);
            }
            std::vector<solve_detail::WideFloat> exact_terminal_mass(
                terminal_mass.size(), solve_detail::WideFloat{0.0});
            std::vector<solve_detail::WideFloat>
                exact_action_not_applied(
                    action_not_applied.size(),
                    solve_detail::WideFloat{0.0});
            std::vector<solve_detail::WideFloat>
                exact_no_matching_edge(
                    no_matching_edge.size(),
                    solve_detail::WideFloat{0.0});
            check_owned_cap(capped_add(
                capped_product(
                    exact_pair_visits_owned.capacity(),
                    sizeof(double)),
                capped_add(
                    capped_product(
                        visits_by_row.capacity(),
                        sizeof(solve_detail::WideFloat)),
                    capped_product(
                        exact_terminal_mass.capacity() +
                            exact_action_not_applied.capacity() +
                            exact_no_matching_edge.capacity(),
                        sizeof(solve_detail::WideFloat)))));
            if (!exact_row_visits_available) {
                for (std::size_t raw = 0;
                     raw < attribution_pairs.size(); ++raw) {
                    const double visits = exact_pair_visits->at(raw);
                    if (!(visits > 0.0)) continue;
                    visits_by_row.at(attribution_pairs[raw].row) +=
                        solve_detail::WideFloat{visits};
                }
            }
            /* Shared exact rows are identical transition/absorption
             * authorities. Aggregate their reconstructed raw occupancy and
             * replay each row once; iterating the same wide reforge row once
             * per raw pair is algebraically redundant and can multiply
             * finalization work by orders of magnitude. */
            for (std::size_t row_id = 0;
                 row_id < attribution_rows.size(); ++row_id) {
                const solve_detail::WideFloat wide_visits =
                    visits_by_row[row_id];
                const double visits = wide_visits.value();
                if (!(visits > 0.0)) continue;
                const EvalRow& row = attribution_rows[row_id];
                visit_eval_row(
                    row, attribution_pairs,
                    [&](const EvalTransition& transition) {
                        add_compressed_policy_incoming(
                            transition.policy_route,
                            transition.policy_state,
                            visits * transition.probability);
                    },
                    [&](const EvalAbsorption& absorption) {
                        const solve_detail::WideFloat wide_mass =
                            wide_visits *
                            solve_detail::WideFloat{
                                absorption.probability};
                        const double mass = wide_mass.value();
                        add_compressed_policy_incoming(
                            absorption.policy_route,
                            absorption.state, mass);
                        if (absorption.kind ==
                                EvalAbsorptionKind::Terminal) {
                            exact_terminal_mass.at(absorption.node) +=
                                wide_mass;
                            add_terminal_incoming(
                                absorption.node, absorption.state, mass);
                        } else if (absorption.kind ==
                                   EvalAbsorptionKind::ActionNotApplied) {
                            exact_action_not_applied.at(absorption.node) +=
                                wide_mass;
                        } else {
                            exact_no_matching_edge.at(absorption.node) +=
                                wide_mass;
                        }
                    });
                if ((row_id & 255u) == 255u) {
                    check_owned_cap(capped_product(
                        visits_by_row.capacity() +
                            exact_terminal_mass.capacity() +
                            exact_action_not_applied.capacity() +
                            exact_no_matching_edge.capacity(),
                        sizeof(solve_detail::WideFloat)));
                    co_await solve_detail::CooperativeCheckpoint{
                        capped_product(
                            visits_by_row.capacity() +
                                exact_terminal_mass.capacity() +
                                exact_action_not_applied.capacity() +
                                exact_no_matching_edge.capacity(),
                            sizeof(solve_detail::WideFloat))};
                }
            }
            for (std::size_t node = 0;
                 node < terminal_mass.size(); ++node) {
                terminal_mass[node] = exact_terminal_mass[node].value();
                action_not_applied[node] =
                    exact_action_not_applied[node].value();
                no_matching_edge[node] =
                    exact_no_matching_edge[node].value();
            }
            co_await solve_detail::CooperativeCheckpoint{};
        }
        CalcContext& calc = *model.calc;
        output.reforge_work =
            calc.telemetry().reforge_frontier_work;
        output.reforge_logical_work_v1 =
            calc.telemetry().reforge_logical_work_v1;
        output.reforge_evaluator_work_v1 =
            calc.telemetry().reforge_raw_equivalent_work;
        output.reforge_evaluator_work_v2 =
            calc.telemetry().reforge_projected_work;
        output.reforge_evaluator_work_v3 =
            calc.telemetry().reforge_factored_work;
        output.reforge_gated_first_kernel_bits_hash =
            calc.telemetry().gated_first_kernel_bits_hash;
        output.reforge_effort = calc.telemetry().reforge_effort;
        output.reforge_row_samples =
            calc.telemetry().reforge_row_samples;
        output.reforge_row_samples_omitted =
            calc.telemetry().reforge_row_samples_omitted;
        const std::size_t node_count = strategy->nodes.size();
        std::size_t operation_pair_count = 0;
        for (std::size_t pair = 0; pair < exact_pairs->size(); ++pair) {
            if (pair_is_operation((*exact_pairs)[pair])) {
                ++operation_pair_count;
            }
        }
        std::uint64_t finalization_transient_floor =
            capped_product(
                exact_pair_visits_owned.capacity(), sizeof(double)) +
            node_count *
                (4ull * sizeof(double) +
                 sizeof(std::map<std::uint32_t, double>)) +
            exact_pairs->size() *
                (sizeof(std::pair<const std::uint32_t, double>) +
                 3ull * sizeof(void*));
        check_owned_cap(
            finalization_transient_floor +
            static_cast<std::uint64_t>(calc.state_count()) *
                sizeof(AbstractState) +
            static_cast<std::uint64_t>(operation_pair_count) *
                sizeof(StrategyEvalOccupancyEntry));
        output.occupancy_states.reserve(calc.state_count());
        for (std::uint32_t state = 0; state < calc.state_count(); ++state) {
            output.occupancy_states.push_back(calc.state(state));
        }
        co_await solve_detail::CooperativeCheckpoint{
            finalization_transient_floor};
        output.occupancy.reserve(operation_pair_count);
        output.occupancy_reward_complete = options.economy != nullptr;
        std::vector<double> node_visits(node_count, 0.0);
        std::vector<double> operation_visits(node_count, 0.0);
        std::vector<double> operation_applied(node_count, 0.0);
        std::vector<double> unresolved_by_node(node_count, 0.0);
        std::vector<std::map<std::uint32_t, double>> incoming(node_count);
        std::vector<std::vector<std::pair<std::uint32_t, double>>>
            compressed_top_classes(node_count);
        std::vector<std::uint8_t> compressed_class_node(node_count, 0);
        std::uint64_t compressed_top_owned_bytes =
            compressed_top_classes.capacity() *
                sizeof(std::vector<std::pair<std::uint32_t, double>>) +
            compressed_class_node.capacity() * sizeof(std::uint8_t);

        std::uint64_t compressed_routes_seen = 0;
        const std::size_t deterministic_flow_count =
            deterministic_route_flows.size();
        for (std::size_t flow_index = 0;
             flow_index < deterministic_flow_count; ++flow_index) {
            const double mass = deterministic_route_flows[flow_index].mass;
            const std::uint32_t state =
                deterministic_route_flows[flow_index].state;
            const std::uint32_t trace_id =
                decode_deterministic_route_authority(
                    deterministic_route_flows[flow_index].authority);
            const refinement::StableKey& trace =
                deterministic_route_traces.at(trace_id);
            if (trace.size() < 4 ||
                trace[0] != 0x6576616c726f7574ull ||
                trace.size() != 4 + 2 * trace[1]) {
                throw std::logic_error(
                    "strategy evaluation deterministic route trace is "
                    "malformed");
            }
            const std::uint32_t steps = static_cast<std::uint32_t>(trace[1]);
            if (steps == 0) {
                throw std::logic_error(
                    "strategy evaluation retained an empty deterministic "
                    "route trace");
            }
            for (std::uint32_t step = 0; step < steps; ++step) {
                const std::uint32_t node = static_cast<std::uint32_t>(
                    trace[2 + 2 * step]);
                const std::uint64_t edge_token = trace[3 + 2 * step];
                if (node >= node_count ||
                    strategy->nodes[node].kind !=
                        StrategyNodeKind::Router) {
                    throw std::logic_error(
                        "strategy evaluation deterministic route trace has "
                        "a non-router step");
                }
                node_visits[node] += mass;
                if (edge_token != 0) {
                    const std::uint64_t edge = edge_token - 1;
                    if (edge >= edge_traversals.size()) {
                        throw std::logic_error(
                            "strategy evaluation deterministic route trace "
                            "has an invalid edge");
                    }
                    edge_traversals[static_cast<std::size_t>(edge)] += mass;
                }
                if (step == 0) {
                    deterministic_route_flows[flow_index].authority = node;
                } else {
                    const std::size_t capacity_before =
                        deterministic_route_flows.capacity();
                    deterministic_route_flows.push_back(
                        {mass, node, state});
                    if (deterministic_route_flows.capacity() !=
                        capacity_before) {
                        check_owned_cap(
                            finalization_transient_floor +
                            compressed_top_owned_bytes);
                    }
                }
            }
            if ((++compressed_routes_seen & 255u) == 0) {
                co_await solve_detail::CooperativeCheckpoint{
                    finalization_transient_floor +
                    compressed_top_owned_bytes};
            }
        }
        for (std::uint32_t root = 0; root < node_count; ++root) {
            for (const auto& [state, mass] :
                 compressed_policy_incoming[root]) {
                std::uint32_t cursor = root;
                std::size_t steps = 0;
                while (is_policy_route_node(cursor)) {
                    if (++steps > node_count) {
                        throw std::logic_error(
                            "compiled policy router contains a cycle");
                    }
                    node_visits[cursor] += mass;
                    const std::size_t capacity_before =
                        deterministic_route_flows.capacity();
                    deterministic_route_flows.push_back(
                        {mass, cursor, state});
                    if (deterministic_route_flows.capacity() !=
                        capacity_before) {
                        check_owned_cap(
                            finalization_transient_floor +
                            compressed_top_owned_bytes);
                    }
                    const StrategyEdge* selected =
                        select_edge(strategy->nodes[cursor], state);
                    if (selected == nullptr) break;
                    edge_traversals.at(
                        edge_index_by_id.at(selected->id)) += mass;
                    cursor = selected->target;
                }
                if ((++compressed_routes_seen & 255u) == 0) {
                    check_owned_cap(
                        finalization_transient_floor +
                        compressed_top_owned_bytes);
                    co_await solve_detail::CooperativeCheckpoint{
                        finalization_transient_floor +
                        compressed_top_owned_bytes};
                }
            }
        }
        std::stable_sort(
            deterministic_route_flows.begin(),
            deterministic_route_flows.end(),
            [](const DeterministicRouteFlow& left,
               const DeterministicRouteFlow& right) {
                if (left.authority != right.authority) {
                    return left.authority < right.authority;
                }
                return left.state < right.state;
            });
        std::size_t compacted_route_flows = 0;
        for (std::size_t begin = 0;
             begin < deterministic_route_flows.size();) {
            std::size_t end = begin + 1;
            solve_detail::WideFloat mass =
                deterministic_route_flows[begin].mass;
            while (end < deterministic_route_flows.size() &&
                   deterministic_route_flows[end].authority ==
                       deterministic_route_flows[begin].authority &&
                   deterministic_route_flows[end].state ==
                       deterministic_route_flows[begin].state) {
                mass += solve_detail::WideFloat{
                    deterministic_route_flows[end].mass};
                ++end;
            }
            deterministic_route_flows[compacted_route_flows++] = {
                mass.value(),
                deterministic_route_flows[begin].authority,
                deterministic_route_flows[begin].state};
            begin = end;
        }
        deterministic_route_flows.resize(compacted_route_flows);

        for (std::size_t pair = 0; pair < pairs.size(); ++pair) {
            const EvalPair& record = pairs[pair];
            unresolved_by_node[record.node] += unresolved_pair[pair];
            output.residual_mass += unresolved_pair[pair];
        }
        for (std::size_t pair = 0;
             pair < exact_pairs->size(); ++pair) {
            const EvalPair& record = exact_pairs->at(pair);
            const double visits = exact_pair_visits->at(pair);
            node_visits[record.node] += visits;
            incoming[record.node][record.state] += visits;
            if (pair_is_operation(record)) {
                output.expected_actions += visits;
                operation_visits[record.node] += visits;
                StrategyEvalOccupancyEntry retained;
                retained.state = record.state;
                retained.node = record.node;
                const ResolvedStrategyOperation& operation =
                    model.operation_by_node.at(record.node);
                retained.action =
                    operation.kind ==
                            ResolvedStrategyOperationKind::Bestiary
                        ? kNoId
                        : operation.descriptor_index;
                retained.expected_visits = visits;
                retained.expected_applied = record.consumes ? visits : 0.0;
                retained.reward_complete = options.economy != nullptr;
                const std::vector<std::string>& cost_keys =
                    operation_cost_keys(
                        operation, calc.registry(), calc.session());
                if (record.consumes && options.economy != nullptr) {
                    for (const std::string& key : cost_keys) {
                        const auto price = options.economy->prices.find(key);
                        if (price == options.economy->prices.end()) {
                            retained.reward_complete = false;
                            output.occupancy_reward_complete = false;
                        } else {
                            retained.immediate_reward += price->second;
                        }
                    }
                }
                output.occupancy_expected_reward +=
                    retained.expected_applied * retained.immediate_reward;
                output.occupancy.push_back(retained);
                if (record.consumes) {
                    operation_applied[record.node] += visits;
                    for (const std::string& key : cost_keys) {
                        output.expected_consumption[key] += visits;
                    }
                }
            }
            if ((pair & 255u) == 255u) {
                check_owned_cap(finalization_transient_floor);
                co_await solve_detail::CooperativeCheckpoint{
                    finalization_transient_floor};
            }
        }
        const auto consider_compressed_class = [&]
            (const std::uint32_t node,
             const std::uint32_t state,
             const double mass) {
                if (options.top_classes_per_node == 0 ||
                    !(mass > 0.0)) {
                    return;
                }
                auto& top = compressed_top_classes[node];
                const std::size_t capacity_before = top.capacity();
                top.push_back({state, mass});
                if (top.capacity() != capacity_before) {
                    compressed_top_owned_bytes +=
                        (top.capacity() - capacity_before) *
                        sizeof(std::pair<std::uint32_t, double>);
                    check_owned_cap(
                        finalization_transient_floor +
                        compressed_top_owned_bytes);
                }
                std::stable_sort(
                    top.begin(), top.end(),
                    [](const auto& left, const auto& right) {
                        if (left.second != right.second) {
                            return left.second > right.second;
                        }
                        return left.first < right.first;
                    });
                if (top.size() > options.top_classes_per_node) {
                    top.pop_back();
                }
            };
        for (const DeterministicRouteFlow& flow :
             deterministic_route_flows) {
            const std::uint32_t node = flow.authority;
            if (node >= node_count) {
                throw std::logic_error(
                    "strategy evaluation compressed class has an invalid "
                    "node");
            }
            compressed_class_node[node] = 1;
            double total = flow.mass;
            auto raw = incoming[node].find(flow.state);
            if (raw != incoming[node].end()) {
                total += raw->second;
                incoming[node].erase(raw);
            }
            consider_compressed_class(node, flow.state, total);
        }
        for (std::uint32_t node = 0; node < node_count; ++node) {
            if (!compressed_class_node[node]) continue;
            for (const auto& [state, mass] : incoming[node]) {
                consider_compressed_class(node, state, mass);
            }
            incoming[node].clear();
        }
        std::vector<DeterministicRouteFlow>().swap(
            deterministic_route_flows);
        finalization_transient_floor += compressed_top_owned_bytes;
        if (!attribution_pairs.empty() && !hard_unresolved &&
            output.residual_mass < options.epsilon) {
            solve_detail::WideFloat absorbed = 0.0;
            for (const double mass : terminal_mass) {
                absorbed += solve_detail::WideFloat{mass};
            }
            for (const double mass : action_not_applied) {
                absorbed += solve_detail::WideFloat{mass};
            }
            for (const double mass : no_matching_edge) {
                absorbed += solve_detail::WideFloat{mass};
            }
            const double correction =
                (solve_detail::WideFloat{1.0} - absorbed).value();
            const double accumulated_roundoff_tolerance = std::max(
                1e-8,
                256.0 * std::numeric_limits<double>::epsilon() *
                    std::max(1.0, output.expected_actions));
            if (std::isfinite(correction) &&
                std::fabs(correction) <=
                    accumulated_roundoff_tolerance) {
                std::uint32_t terminal = kNoId;
                for (std::uint32_t node = 0;
                     node < strategy->nodes.size(); ++node) {
                    if (strategy->nodes[node].kind !=
                        StrategyNodeKind::Terminal) {
                        continue;
                    }
                    if (terminal == kNoId ||
                        terminal_mass[node] > terminal_mass[terminal]) {
                        terminal = node;
                    }
                }
                if (terminal != kNoId &&
                    terminal_mass[terminal] + correction >= 0.0) {
                    /* Exact row equations are solved in WideFloat, while
                     * public terminal fields are doubles. Near-renewal
                     * policies can amplify the unavoidable stored-double
                     * row-sum residue across hundreds of thousands of
                     * expected actions. Close only that bounded roundoff on
                     * the dominant reached terminal after all raw and
                     * quotient flow equations have independently passed. */
                    terminal_mass[terminal] += correction;
                    output.max_mass_conservation_error = std::max(
                        output.max_mass_conservation_error,
                        std::fabs(correction));
                }
            }
        }
        if (!attribution_pairs.empty()) {
            retire_pair_lookup_index();
            std::vector<ReplayRouteResult>().swap(
                replay_route_results);
        }
        attribution_pairs.release();
        std::vector<EvalRow>().swap(attribution_rows);
        std::vector<std::uint32_t>().swap(
            attribution_class_by_pair);
        attribution_row_payload_owned_bytes = 0;
        for (std::size_t node = 0; node < node_count; ++node) {
            node_visits[node] += terminal_mass[node];
            for (const auto& [state, mass] : terminal_incoming[node]) {
                incoming[node][state] += mass;
            }
            const StrategyNode& source = strategy->nodes[node];
            if (source.kind == StrategyNodeKind::Terminal) {
                output.terminal_nodes.push_back(
                    {source.id, source.terminal_kind, terminal_mass[node]});
                if (source.terminal_kind == PC_TERMINAL_SUCCESS) {
                    output.success_probability += terminal_mass[node];
                } else if (source.terminal_kind == PC_TERMINAL_FAILURE) {
                    output.failure_probability += terminal_mass[node];
                } else {
                    output.stop_probability += terminal_mass[node];
                }
            }
            if (unresolved_by_node[node] > 0.0) {
                output.unresolved_by_node.push_back(
                    {source.id, unresolved_by_node[node]});
            }
            if (action_not_applied[node] > 0.0) {
                output.failures_by_node.push_back(
                    {source.id, "action_not_applied",
                     action_not_applied[node]});
                output.action_not_applied_probability +=
                    action_not_applied[node];
            }
            if (no_matching_edge[node] > 0.0) {
                output.failures_by_node.push_back(
                    {source.id, "no_matching_edge",
                     no_matching_edge[node]});
                output.no_matching_edge_probability +=
                    no_matching_edge[node];
            }

            StrategyEvalNode output_node;
            output_node.id = source.id;
            output_node.expected_visits = node_visits[node];
            std::vector<std::pair<std::uint32_t, double>> classes;
            if (compressed_class_node[node]) {
                classes = compressed_top_classes[node];
            } else {
                classes.assign(
                    incoming[node].begin(), incoming[node].end());
            }
            std::stable_sort(
                classes.begin(), classes.end(), [](const auto& a, const auto& b) {
                    if (a.second != b.second) return a.second > b.second;
                    return a.first < b.first;
                });
            const std::size_t keep = std::min<std::size_t>(
                options.top_classes_per_node, classes.size());
            for (std::size_t c = 0; c < keep; ++c) {
                output_node.classes.push_back(
                    {node_visits[node] == 0.0
                         ? 0.0
                         : classes[c].second / node_visits[node],
                     calc.state(classes[c].first)});
            }
            double truncated = 0.0;
            if (compressed_class_node[node]) {
                double retained = 0.0;
                for (std::size_t c = 0; c < keep; ++c) {
                    retained += classes[c].second;
                }
                truncated = std::max(0.0, node_visits[node] - retained);
            } else {
                for (std::size_t c = keep; c < classes.size(); ++c) {
                    truncated += classes[c].second;
                }
            }
            output_node.classes_truncated_share =
                node_visits[node] == 0.0
                    ? 0.0
                    : truncated / node_visits[node];
            output.nodes.push_back(std::move(output_node));

            for (const StrategyEdge& edge : source.edges) {
                output.edges.push_back(
                    {edge.id, edge_traversals.at(edge_index_by_id.at(edge.id))});
            }
            if ((node & 255u) == 255u) {
                check_owned_cap(finalization_transient_floor);
            }
        }

        output.technique_totals = empty_technique_totals();
        std::vector<std::map<std::string, double>> node_techniques(
            node_count);
        std::vector<std::vector<std::string>> node_classifications(
            node_count);
        std::map<std::string, StrategyEvalActionTotal> actions_by_id;
        double total_applied_actions = 0.0;
        for (std::size_t node_index = 0; node_index < node_count;
             ++node_index) {
            const StrategyNode& node = strategy->nodes[node_index];
            if (node.kind != StrategyNodeKind::Operation) continue;
            const ResolvedStrategyOperation& operation =
                model.operation_by_node.at(node_index);
            const std::string& descriptor_id = operation_id(
                operation, calc.registry(), calc.session());
            const std::string& display_name = operation_display_name(
                operation, calc.registry(), calc.session());
            const std::vector<std::string>& cost_keys =
                operation_cost_keys(
                    operation, calc.registry(), calc.session());
            StrategyEvalActionTotal& action =
                actions_by_id[descriptor_id];
            if (action.id.empty()) {
                action.id = descriptor_id;
                action.display_name = display_name;
                action.price_keys = cost_keys;
            }
            action.expected_visits += operation_visits[node_index];
            action.expected_applied += operation_applied[node_index];
            action.nodes.push_back(
                {node.id, operation_visits[node_index],
                 operation_applied[node_index]});
            total_applied_actions += operation_applied[node_index];

            std::vector<std::string> roles = node.accounting_roles;
            if (operation.kind ==
                ResolvedStrategyOperationKind::Restart) {
                add_classification(roles, "restart");
            } else {
                add_classification(roles, "ordinary_crafting");
            }
            if (operation.kind ==
                ResolvedStrategyOperationKind::Bestiary) {
                /* Descriptor-owned Bestiary operations need no ordinary
                 * ActionType classification. */
            } else {
                const ActionDescriptor& descriptor =
                    calc.registry().actions.at(
                        operation.descriptor_index);
                if (descriptor.params.type == ActionType::Fracture) {
                    add_classification(roles, "fracture");
                } else if (
                    descriptor.params.type ==
                        ActionType::RemoveCraftedModifiers) {
                    add_classification(roles, "cleanup_or_replacement");
                } else if (descriptor.params.type == ActionType::Bench) {
                    const std::uint32_t mod = descriptor.params.mod_id;
                    if (mod < calc.session().metamod_type.size()) {
                        const int metamod =
                            calc.session().metamod_type[mod];
                        if (metamod == calc.session().data
                                            ->metamod_prefixes_locked_code ||
                            metamod == calc.session().data
                                            ->metamod_suffixes_locked_code) {
                            add_classification(roles, "protection_setup");
                        } else if (
                            metamod == calc.session().data
                                           ->metamod_multimod_code) {
                            add_classification(roles, "multimod_setup");
                        }
                    }
                    const bool goal_bench = std::any_of(
                        output.targets.begin(), output.targets.end(),
                        [&](const GoalSlot& target) {
                            return target_contains_mod(
                                calc.session(), target, mod);
                        });
                    if (goal_bench) {
                        add_classification(
                            roles, "permanent_goal_bench");
                        add_classification(
                            roles, "deterministic_finish");
                    }
                }
            }
            for (const std::string& role : roles) {
                add_classification(
                    node_classifications[node_index], role);
                add_classification(action.classifications, role);
                add_role_work(
                    output.technique_totals, role,
                    operation_visits[node_index],
                    operation_applied[node_index]);
                add_role_work(
                    node_techniques[node_index], role,
                    operation_visits[node_index],
                    operation_applied[node_index]);
            }
            if ((node_index & 255u) == 255u) {
                check_owned_cap(finalization_transient_floor);
                co_await solve_detail::CooperativeCheckpoint{
                    finalization_transient_floor};
            }
        }
        struct RetainedRegion {
            StrategyEvalActionRegion totals;
            std::set<std::uint32_t> states;
        };
        using RegionKey = std::tuple<
            std::uint32_t, std::uint8_t, std::uint32_t, std::uint32_t,
            std::uint32_t, std::uint32_t>;
        std::map<std::string, std::map<RegionKey, RetainedRegion>>
            regions_by_action;
        std::map<std::string, std::set<std::uint32_t>> states_by_action;
        const auto bit_count = [](std::uint32_t value) {
            std::uint32_t count = 0;
            while (value != 0) {
                count += value & 1u;
                value >>= 1u;
            }
            return count;
        };
        const auto vector_count = [](const CompactCountVector& values) {
            std::uint32_t count = 0;
            for (const std::uint8_t value : values) count += value;
            return count;
        };
        for (const StrategyEvalOccupancyEntry& entry : output.occupancy) {
            if (entry.state >= output.occupancy_states.size() ||
                entry.node >= model.operation_by_node.size() ||
                entry.expected_visits <= 0.0) {
                continue;
            }
            const ResolvedStrategyOperation& operation =
                model.operation_by_node[entry.node];
            if (!operation.resolved()) continue;
            const AbstractState& state =
                output.occupancy_states[entry.state];
            std::uint32_t progress = 0;
            for (std::size_t slot = 0; slot < output.targets.size(); ++slot) {
                if (state.slot_status[slot] == static_cast<std::uint8_t>(
                        GoalSlotStatus::Satisfied)) {
                    ++progress;
                }
            }
            const std::uint32_t crafted =
                bit_count(state.crafted_goal_mask) +
                vector_count(state.crafted_junk_counts);
            const std::uint32_t fractured =
                bit_count(state.fractured_goal_mask) +
                bit_count(state.fractured_metamod_flags) +
                vector_count(state.fractured_junk_counts);
            const RegionKey key{
                progress, state.rarity, bit_count(state.blocked_mask),
                crafted, state.fractured_goal_mask, fractured};
            const std::string& id = operation_id(
                operation, calc.registry(), calc.session());
            RetainedRegion& region = regions_by_action[id][key];
            region.totals.goal_progress = progress;
            region.totals.rarity = state.rarity;
            region.totals.blocker_count = bit_count(state.blocked_mask);
            region.totals.crafted_count = crafted;
            region.totals.fractured_goal_mask =
                state.fractured_goal_mask;
            region.totals.fractured_count = fractured;
            region.totals.expected_visits += entry.expected_visits;
            region.totals.expected_applied += entry.expected_applied;
            region.states.insert(entry.state);
            states_by_action[id].insert(entry.state);
        }
        for (auto& [id, action] : actions_by_id) {
            const auto action_states = states_by_action.find(id);
            action.reachable_states =
                action_states == states_by_action.end()
                    ? 0
                    : static_cast<std::uint32_t>(
                          action_states->second.size());
            const auto retained_regions = regions_by_action.find(id);
            if (retained_regions != regions_by_action.end()) {
                for (auto& [unused_key, retained] :
                     retained_regions->second) {
                    (void)unused_key;
                    retained.totals.reachable_states =
                        static_cast<std::uint32_t>(
                            retained.states.size());
                    action.regions.push_back(retained.totals);
                }
            }
        }
        for (const auto& [unused, action] : actions_by_id) {
            (void)unused;
            output.action_totals.push_back(action);
        }

        std::unordered_map<std::string, double> traversal_by_edge;
        for (const StrategyEvalEdge& edge : output.edges) {
            traversal_by_edge.emplace(edge.id, edge.expected_traversals);
        }
        std::unordered_map<std::string, std::map<std::string, double>>
            edge_techniques;
        for (const StrategyNode& node : strategy->nodes) {
            for (const StrategyEdge& edge : node.edges) {
                const double traversals = traversal_by_edge.at(edge.id);
                for (const std::string& role : edge.accounting_roles) {
                    add_role_work(
                        output.technique_totals, role, traversals,
                        traversals);
                    add_role_work(
                        edge_techniques[edge.id], role, traversals,
                        traversals);
                }
            }
        }

        output.pricing_enabled = options.economy != nullptr;
        if (options.economy != nullptr) {
            output.economy_id = options.economy->id;
        }
        output.material_totals = price_materials(
            output.expected_consumption, options.economy,
            output.known_expected_cost, output.cost_complete);
        if (output.cost_complete) {
            output.total_expected_cost = output.known_expected_cost;
        }

        double descriptor_visits = 0.0;
        double descriptor_applied = 0.0;
        std::map<std::string, double> action_materials;
        for (const StrategyEvalActionTotal& action : output.action_totals) {
            descriptor_visits += action.expected_visits;
            descriptor_applied += action.expected_applied;
            for (const std::string& key : action.price_keys) {
                action_materials[key] += action.expected_applied;
            }
        }
        output.action_descriptor_visits_difference =
            descriptor_visits - output.expected_actions;
        output.action_descriptor_applied_difference =
            descriptor_applied - total_applied_actions;
        double operation_visit_sum = 0.0;
        for (const double visits : operation_visits) {
            operation_visit_sum += visits;
        }
        output.node_operation_visits_difference =
            operation_visit_sum - output.expected_actions;
        for (const auto& [key, quantity] : output.expected_consumption) {
            output.material_quantity_differences[key] =
                action_materials[key] - quantity;
        }
        double priced_dot_product = 0.0;
        for (const StrategyEvalMaterialTotal& material :
             output.material_totals) {
            if (material.priced) {
                priced_dot_product += material.expected_quantity *
                                      material.unit_price;
            }
        }
        output.cost_dot_product_difference =
            priced_dot_product - output.known_expected_cost;
        output.occupancy_reward_difference =
            output.occupancy_expected_reward - output.known_expected_cost;

        output.review_sections_enabled = !review_sections.empty();
        if (!review_sections.empty()) {
            std::vector<std::size_t> section_by_node(node_count);
            for (std::size_t section = 0; section < review_sections.size();
                 ++section) {
                for (const std::uint32_t node :
                     review_sections[section].nodes) {
                    section_by_node[node] = section;
                }
            }
            std::vector<std::map<std::string, StrategyEvalActionTotal>>
                section_actions(review_sections.size());
            std::vector<std::map<std::string, double>> section_materials(
                review_sections.size());
            output.review_sections.resize(review_sections.size());
            for (std::size_t section = 0; section < review_sections.size();
                 ++section) {
                StrategyEvalReviewSection& target =
                    output.review_sections[section];
                target.id = review_sections[section].id;
                target.label = review_sections[section].label;
                target.role = review_sections[section].role;
                target.raw_edge_ids = review_sections[section].edges;
                target.techniques = empty_technique_totals();
                for (const std::uint32_t node_index :
                     review_sections[section].nodes) {
                    target.raw_node_ids.push_back(
                        strategy->nodes[node_index].id);
                    target.expected_actions += operation_visits[node_index];
                    for (const auto& [key, value] :
                         node_techniques[node_index]) {
                        target.techniques[key] += value;
                    }
                    if (strategy->nodes[node_index].kind !=
                        StrategyNodeKind::Operation) {
                        continue;
                    }
                    const ResolvedStrategyOperation& operation =
                        model.operation_by_node.at(node_index);
                    const std::string& descriptor_id = operation_id(
                        operation, calc.registry(), calc.session());
                    const std::string& display_name =
                        operation_display_name(
                            operation, calc.registry(), calc.session());
                    const std::vector<std::string>& cost_keys =
                        operation_cost_keys(
                            operation, calc.registry(), calc.session());
                    StrategyEvalActionTotal& action =
                        section_actions[section][descriptor_id];
                    if (action.id.empty()) {
                        action.id = descriptor_id;
                        action.display_name = display_name;
                        action.price_keys = cost_keys;
                    }
                    for (const std::string& role :
                         node_classifications[node_index]) {
                        add_classification(action.classifications, role);
                    }
                    action.expected_visits += operation_visits[node_index];
                    action.expected_applied += operation_applied[node_index];
                    action.nodes.push_back(
                        {strategy->nodes[node_index].id,
                         operation_visits[node_index],
                         operation_applied[node_index]});
                    for (const std::string& key : cost_keys) {
                        section_materials[section][key] +=
                            operation_applied[node_index];
                    }
                }
                for (const std::string& edge :
                     review_sections[section].edges) {
                    target.expected_edge_traversals +=
                        traversal_by_edge.at(edge);
                    for (const auto& [key, value] : edge_techniques[edge]) {
                        target.techniques[key] += value;
                    }
                }
                for (auto& [unused, action] : section_actions[section]) {
                    (void)unused;
                    target.actions.push_back(std::move(action));
                }
                target.materials = price_materials(
                    section_materials[section], options.economy,
                    target.known_expected_cost, target.cost_complete);
                if (target.cost_complete) {
                    target.total_expected_cost = target.known_expected_cost;
                }
                check_owned_cap(finalization_transient_floor);
                co_await solve_detail::CooperativeCheckpoint{
                    finalization_transient_floor};
            }
            double section_actions_sum = 0.0;
            std::map<std::string, double> section_material_sum;
            for (const StrategyEvalReviewSection& section :
                 output.review_sections) {
                section_actions_sum += section.expected_actions;
                for (const StrategyEvalMaterialTotal& material :
                     section.materials) {
                    section_material_sum[material.price_key] +=
                        material.expected_quantity;
                }
            }
            output.section_actions_difference =
                section_actions_sum - output.expected_actions;
            for (const auto& [key, quantity] :
                 output.expected_consumption) {
                output.section_material_differences[key] =
                    section_material_sum[key] - quantity;
            }
        }
        output.success_normalized_enabled =
            options.include_success_normalized &&
            output.success_probability > 0.0 &&
            output.success_probability < 1.0;
        output.unresolved_probability = output.residual_mass;
        output.sweeps = static_cast<std::uint32_t>(std::min<std::uint64_t>(
            fallback_sweeps,
            std::numeric_limits<std::uint32_t>::max()));
        output.converged =
            !hard_unresolved && output.residual_mass < options.epsilon;
        const double conservation_error = std::fabs(
            absorbed_probability(output) + output.residual_mass - 1.0);
        output.max_mass_conservation_error = std::max(
            output.max_mass_conservation_error, conservation_error);
        if (conservation_error > 1e-8) {
            throw std::runtime_error(
                "strategy evaluation mass conservation failed");
        }
        check_owned_cap();
        phase = StrategyEvalPhase::Done;
        subphase = StrategyEvalSubphase::Done;
        output.boundary_subphase = subphase;
        co_return true;
    }

    StrategyEvalResult forward_reference() {
        if (phase == StrategyEvalPhase::Finalization) {
            unresolved_pair.assign(pairs.size(), 0.0);
            pair_visits.assign(pairs.size(), 0.0);
            auto reference_finalization = finalize();
            while (!reference_finalization.resume()) {}
            (void)reference_finalization.take_result();
            return output;
        }
        if (discover_index != pairs.size()) {
            throw std::logic_error(
                "strategy evaluation reference requires completed discovery");
        }
        pair_visits.assign(pairs.size(), 0.0);
        unresolved_pair.assign(pairs.size(), 0.0);
        std::vector<double> wave = external_incoming;
        std::uint32_t sweeps = 0;
        for (; sweeps < options.max_sweeps; ++sweeps) {
            std::vector<double> next(pairs.size(), 0.0);
            for (std::size_t pair = 0; pair < pairs.size(); ++pair) {
                const double mass = wave[pair];
                if (!(mass > 0.0)) continue;
                pair_visits[pair] += mass;
                const EvalRow& row = pair_row(static_cast<std::uint32_t>(pair));
                for (std::size_t transition_index = 0;
                     transition_index < row.transitions.size();
                     ++transition_index) {
                    const EvalTransition& transition =
                        row.transitions[transition_index];
                    const double flow = mass * transition.probability;
                    next[transition.target] += flow;
                    if (transition.edge != kNoId) {
                        edge_traversals.at(transition.edge) += flow;
                    }
                    add_compressed_policy_incoming(
                        transition.policy_route,
                        transition.policy_state, flow);
                    const std::uint32_t via =
                        transition_via(row, transition_index);
                    if (via != kNoId) {
                        chain_inflow.at(via) += flow;
                    }
                }
                for (const EvalAbsorption& absorption :
                     row.absorptions) {
                    add_absorption(
                        absorption, mass * absorption.probability);
                }
            }
            double transient = 0.0;
            for (const double mass : next) transient += mass;
            double absorbed = 0.0;
            for (const double mass : terminal_mass) absorbed += mass;
            for (const double mass : action_not_applied) absorbed += mass;
            for (const double mass : no_matching_edge) absorbed += mass;
            const double error = std::fabs(absorbed + transient - 1.0);
            output.max_mass_conservation_error = std::max(
                output.max_mass_conservation_error, error);
            if (error > 1e-8) {
                throw std::runtime_error(
                    "strategy evaluation reference mass conservation failed");
            }
            wave = std::move(next);
            if (transient < options.epsilon) {
                ++sweeps;
                break;
            }
        }
        double residual = 0.0;
        for (std::size_t pair = 0; pair < wave.size(); ++pair) {
            unresolved_pair[pair] = wave[pair];
            residual += wave[pair];
        }
        hard_unresolved = residual >= options.epsilon;
        fallback_sweeps = sweeps;
        phase = StrategyEvalPhase::Finalization;
        auto reference_finalization = finalize();
        while (!reference_finalization.resume()) {}
        (void)reference_finalization.take_result();
        return output;
    }

    void step(std::uint32_t max_work_items) {
        std::uint32_t remaining = std::max<std::uint32_t>(1, max_work_items);
        while (remaining-- > 0 && phase != StrategyEvalPhase::Done) {
            if (phase == StrategyEvalPhase::Discovery) {
                subphase = discover_index < pairs.size()
                    ? StrategyEvalSubphase::PairDiscovery
                    : component_build_task.has_value()
                        ? subphase
                        : StrategyEvalSubphase::PairRefinement;
            } else if (phase == StrategyEvalPhase::Solving ||
                       phase == StrategyEvalPhase::Fallback) {
                subphase = StrategyEvalSubphase::ComponentSolve;
            } else if (phase == StrategyEvalPhase::Finalization) {
                subphase = StrategyEvalSubphase::Finalization;
            }
            std::uint64_t* active_counter = nullptr;
            switch (subphase) {
            case StrategyEvalSubphase::PairDiscovery:
                active_counter = &output.stage_timings.pair_discovery_ns;
                break;
            case StrategyEvalSubphase::PairRefinement:
                active_counter = &output.stage_timings.pair_refinement_ns;
                break;
            case StrategyEvalSubphase::ComponentConstruction:
                active_counter =
                    &output.stage_timings.component_construction_ns;
                break;
            case StrategyEvalSubphase::ComponentSolve:
                active_counter = &output.stage_timings.component_solve_ns;
                break;
            case StrategyEvalSubphase::Finalization:
                active_counter = &output.stage_timings.finalization_ns;
                break;
            case StrategyEvalSubphase::ModelSetup:
            case StrategyEvalSubphase::ObservationPreparation:
            case StrategyEvalSubphase::Done:
                break;
            }
            if (active_counter == nullptr) {
                throw std::logic_error(
                    "strategy evaluation has no active timing subphase");
            }
            const StrategyEvalSubphase active_subphase = subphase;
            const auto work_item_started = Clock::now();
            ActiveTimer active_timer(*active_counter);
            switch (phase) {
            case StrategyEvalPhase::Discovery:
                if (discover_index < pairs.size()) {
                    expand_pair(static_cast<std::uint32_t>(discover_index));
                    ++discover_index;
                    if (discover_index == pairs.size()) {
                        /* Account the true closed-discovery peak before
                         * releasing indexes that have no partition or solve
                         * authority. */
                        check_owned_cap();
                        /* Replay recipes still require the collision-safe raw
                         * pair index through partition, quotient conversion,
                         * and exact attribution. Only discovery-time row and
                         * route interning indexes retire at closure. */
                        retire_row_discovery_indexes();
                    }
                } else {
                    if (!component_build_task.has_value()) {
                        component_build_task.emplace(build_components());
                    }
                    try {
                        if (component_build_task->resume()) {
                            (void)component_build_task->take_result();
                            component_build_task.reset();
                            phase = StrategyEvalPhase::Solving;
                        }
                    } catch (const std::length_error& error) {
                        component_build_task.reset();
                        if (!accept_identity_pair_refinement_fallback(error)) {
                            throw;
                        }
                        skip_pair_refinement_once = true;
                    }
                }
                break;
            case StrategyEvalPhase::Solving:
                solve_component();
                break;
            case StrategyEvalPhase::Fallback:
                run_fallback_batch();
                break;
            case StrategyEvalPhase::Finalization:
                if (!finalization_task.has_value()) {
                    finalization_task.emplace(finalize());
                }
                if (finalization_task->resume()) {
                    (void)finalization_task->take_result();
                    finalization_task.reset();
                }
                break;
            case StrategyEvalPhase::Done:
                break;
            }
            const std::uint64_t work_item_ns =
                static_cast<std::uint64_t>(
                    std::chrono::duration_cast<std::chrono::nanoseconds>(
                        Clock::now() - work_item_started)
                        .count());
            if (work_item_ns > output.max_work_item_ns) {
                output.max_work_item_ns = work_item_ns;
                output.max_work_item_subphase = active_subphase;
            }
            if (phase == StrategyEvalPhase::Discovery &&
                discover_index != 0 &&
                (discover_index & 4095u) == 0) {
                audit_owned_bytes();
            }
            check_owned_cap();
        }
    }

    StrategyEvalProgress progress() const {
        StrategyEvalProgress value;
        value.phase = phase;
        value.subphase = subphase;
        value.done = phase == StrategyEvalPhase::Done;
        value.discovered_pairs = discover_index;
        value.pending_pairs = pairs.size() - discover_index;
        value.solved_sccs = component_index;
        value.total_sccs = components.size();
        value.fallback_sweeps = fallback_sweeps;
        if (fallback != nullptr) {
            value.residual =
                fallback->resume != nullptr &&
                        std::isfinite(
                            fallback->resume->last_true_residual)
                    ? fallback->resume->last_true_residual
                    : fallback->input_mass;
        } else {
            for (const double mass : unresolved_pair) value.residual += mass;
        }
        return value;
    }
};

StrategyEvalWork::StrategyEvalWork(
    std::shared_ptr<const StrategyImpl> strategy,
    const StrategyEvalOptions& options)
    : impl_(std::make_unique<Impl>(std::move(strategy), options)) {}

StrategyEvalWork::~StrategyEvalWork() = default;
StrategyEvalWork::StrategyEvalWork(StrategyEvalWork&&) noexcept = default;
StrategyEvalWork& StrategyEvalWork::operator=(StrategyEvalWork&&) noexcept =
    default;

void StrategyEvalWork::step(std::uint32_t max_work_items) {
    impl_->step(max_work_items);
}

StrategyEvalProgress StrategyEvalWork::progress() const {
    return impl_->progress();
}

const StrategyEvalResult& StrategyEvalWork::result() const {
    if (impl_->phase != StrategyEvalPhase::Done) {
        throw std::logic_error("strategy evaluation is not finished");
    }
    return impl_->output;
}

StrategyEvalResult StrategyEvalWork::take_result() {
    if (impl_->phase != StrategyEvalPhase::Done) {
        throw std::logic_error("strategy evaluation is not finished");
    }
    impl_->output.retained_output_owned_bytes_estimate =
        impl_->output_owned_bytes();
    impl_->output.boundary_subphase = StrategyEvalSubphase::Done;
    return std::move(impl_->output);
}

const StrategyEvalResult& StrategyEvalWork::diagnostic_result() {
    StrategyEvalResult& output = impl_->output;
    output.boundary_subphase = impl_->subphase;
    output.refined_pair_limit = impl_->options.max_pairs;
    if (impl_->model.calc != nullptr) {
        const CalcTelemetry& telemetry =
            impl_->model.calc->telemetry();
        output.reforge_work = telemetry.reforge_frontier_work;
        output.reforge_logical_work_v1 =
            telemetry.reforge_logical_work_v1;
        output.reforge_evaluator_work_v1 =
            telemetry.reforge_raw_equivalent_work;
        output.reforge_evaluator_work_v2 =
            telemetry.reforge_projected_work;
        output.reforge_evaluator_work_v3 =
            telemetry.reforge_factored_work;
        output.reforge_gated_first_kernel_bits_hash =
            telemetry.gated_first_kernel_bits_hash;
        output.reforge_effort = telemetry.reforge_effort;
        output.reforge_row_samples = telemetry.reforge_row_samples;
        output.reforge_row_samples_omitted =
            telemetry.reforge_row_samples_omitted;
    }
    /* Before refinement starts, the live pair carrier is the only exact
     * discovery count available. Once refinement records its raw and
     * quotient counts, keep those authorities: quotient conversion replaces
     * the live carrier before a later resource stop can request diagnostics. */
    if (output.raw_pairs_discovered == 0) {
        output.raw_pairs_discovered = static_cast<std::uint32_t>(
            std::min<std::size_t>(
                impl_->pairs.size(),
                std::numeric_limits<std::uint32_t>::max()));
    }
    if (output.refined_pairs == 0) {
        output.refined_pairs = output.raw_pairs_discovered;
    }
    output.owned_bytes_estimate =
        impl_->fast_estimated_owned_bytes();
    output.retained_output_owned_bytes_estimate =
        impl_->output_owned_bytes();
    output.peak_owned_bytes_estimate = std::max(
        impl_->peak_owned_bytes_value,
        output.owned_bytes_estimate);
    return output;
}

std::uint64_t StrategyEvalWork::live_owned_bytes() const {
    return impl_->fast_estimated_owned_bytes();
}

std::uint64_t StrategyEvalWork::peak_owned_bytes() const {
    return std::max(
        impl_->peak_owned_bytes_value, impl_->fast_estimated_owned_bytes());
}

StrategyEvalResult evaluate_strategy(
    const StrategyImpl& strategy,
    const StrategyEvalOptions& options) {
    std::shared_ptr<const StrategyImpl> borrowed(
        &strategy, [](const StrategyImpl*) {});
    StrategyEvalWork work(std::move(borrowed), options);
    while (!work.progress().done) work.step(4096);
    return work.take_result();
}

StrategyEvalResult evaluate_strategy_forward_reference_for_test(
    const StrategyImpl& strategy,
    const StrategyEvalOptions& options) {
    std::shared_ptr<const StrategyImpl> borrowed(
        &strategy, [](const StrategyImpl*) {});
    StrategyEvalWork work(std::move(borrowed), options);
    while (work.impl_->phase == StrategyEvalPhase::Discovery) {
        work.impl_->step(1);
    }
    return work.impl_->forward_reference();
}


} // namespace solver
} // namespace poecraft
