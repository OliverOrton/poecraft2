#include "solver_eval_helpers.hpp"
#include "solver_cooperative_task.hpp"

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
    std::vector<ObservationRequirement>
        node_observation_requirements;

    bool is_policy_route_node(std::uint32_t node) const {
        return node < strategy->nodes.size() &&
               strategy->nodes[node].kind == StrategyNodeKind::Router &&
               strategy->nodes[node].id.rfind("policy_route_", 0) == 0;
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
                row.absorptions.capacity() * sizeof(EvalAbsorption);
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

    void retire_pair_discovery_indexes() {
        output.pair_discovery_index_peak_bytes = std::max(
            output.pair_discovery_index_peak_bytes,
            pair_index_owned_bytes());
        std::vector<std::uint32_t>().swap(pair_bucket_heads);
        pair_next.release();
        decltype(row_by_distribution){}.swap(row_by_distribution);
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
        output.pair_discovery_index_peak_bytes = std::max(
            output.pair_discovery_index_peak_bytes,
            pair_index_owned_bytes());
        return id;
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

    void ensure_transition_budget(std::uint64_t additional) const {
        if (stored_transitions > options.max_transitions ||
            additional > options.max_transitions - stored_transitions) {
            throw std::length_error(
                "strategy evaluation exceeded max_transitions (" +
                std::to_string(options.max_transitions) +
                "; stored=" + std::to_string(stored_transitions) +
                ", pairs=" + std::to_string(pairs.size()) +
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
            const StrategyEdge* selected =
                select_edge(node, state, offered_mods);
            if (selected == nullptr) {
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
                    add_absorption(
                        {static_cast<int>(
                             EvalAbsorptionKind::NoMatchingEdge),
                         resolution.failure_node, state, edge, policy_route},
                        probability);
                    return;
                }
                target_node = resolution.target_node;
            }
            const StrategyNode& target = strategy->nodes.at(target_node);
            if (target.kind == StrategyNodeKind::Terminal) {
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
                    if (outcomes.choice_groups.empty()) {
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
                    /* A compiled shared gated-renewal region deliberately
                     * observes only whether this action made goal progress:
                     * every zero-progress physical outcome immediately
                     * repeats the same operation. Use the calculator's exact
                     * gated kernel when that retry state selects this node.
                     * Expanding and then re-merging the unobserved physical
                     * junk would be algebraically equivalent, but its stored
                     * double normalization can accumulate across a very long
                     * renewal and drift away from the solver's kernel bits. */
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
                            if (retry != nullptr &&
                                retry->target == node_index) {
                                selected_outcomes = &gated_candidate;
                            }
                        }
                    }
                    if (selected_outcomes == nullptr) {
                        selected_outcomes = &exact_outcomes(
                            state_id, action_index, false);
                    }
                    const OutcomeDistribution& outcomes =
                        *selected_outcomes;
                    if (!outcomes.supported) {
                        throw StrategyEvalUnsupported(
                            "strategy evaluation unsupported:\n- node '" +
                            node.id + "' operation '" + action.id +
                            "' has no exact distribution for a reachable "
                            "state");
                    }
                    ensure_state_limit();
                    if (outcomes.stable_shared_kernel) {
                        const auto shared = row_by_distribution.find(
                            {node_index, checkpoint_state_id, &outcomes});
                        if (shared != row_by_distribution.end()) {
                            EvalPair& pair = pairs.at(pair_id);
                            pair.consumes = consumes;
                            pair.row = shared->second;
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
                            std::find(
                                active_unveil_offer->begin(),
                                active_unveil_offer->end(),
                                node.action.mod_id) ==
                                active_unveil_offer->end()) {
                            throw std::logic_error(
                                "authored modifier selection is not present "
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
                                "authored modifier selection is absent from "
                                "its reachable exact offer vocabulary at "
                                "node '" + node.id + "'");
                        }
                        const std::uint32_t successor =
                            selected->actual_state != kNoId
                                ? selected->actual_state
                                : selected->state;
                        distribution_mass = 1.0;
                        route(
                            successor, 1.0, nullptr,
                            successor_checkpoint);
                    } else {
                        for (const OutcomeEntry& outcome : outcomes.entries) {
                            distribution_mass += outcome.probability;
                            route(
                                outcome.state, outcome.probability,
                                nullptr, successor_checkpoint);
                        }
                    }
                    if (std::fabs(distribution_mass - 1.0) > 1e-9) {
                        throw std::runtime_error(
                            "strategy evaluation action distribution does "
                            "not sum to one at node '" + node.id + "'");
                    }
                }
            }
        }

        EvalPair& pair = pairs.at(pair_id);
        pair.consumes = consumes;
        EvalRow row;
        row.transitions.reserve(transitions.size());
        for (const auto& [key, probability] : transitions) {
            row.transitions.push_back(
                {std::get<0>(key), probability.value(),
                 std::get<1>(key),
                 std::get<2>(key), std::get<3>(key)});
        }
        row.absorptions.reserve(absorptions.size());
        for (const auto& [key, probability] : absorptions) {
            row.absorptions.push_back(
                {static_cast<EvalAbsorptionKind>(std::get<0>(key)),
                  std::get<1>(key), std::get<2>(key),
                  probability.value(),
                  std::get<3>(key), std::get<4>(key)});
        }
        solve_detail::WideFloat row_mass = 0.0;
        for (const EvalTransition& transition : row.transitions) {
            row_mass +=
                solve_detail::WideFloat{transition.probability};
        }
        for (const EvalAbsorption& absorption : row.absorptions) {
            row_mass +=
                solve_detail::WideFloat{absorption.probability};
        }
        if (std::fabs(row_mass.value() - 1.0) > 1e-9) {
            throw std::runtime_error(
                "strategy evaluation transition row does not sum to one at "
                "node '" + node.id + "'");
        }
        stored_transitions += row.transitions.size() + row.absorptions.size();
        row_payload_owned_bytes +=
            row.transitions.capacity() * sizeof(EvalTransition) +
            row.transition_via.capacity() * sizeof(std::uint32_t) +
            row.absorptions.capacity() * sizeof(EvalAbsorption);
        pair.row = static_cast<std::uint32_t>(rows.size());
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
        const ObservationRequirement& requirement =
            node_observation_requirements.at(pair.node);
        if (requirement.item_features == 0 &&
            requirement.modifier_tag_ids.empty() &&
            requirement.affix_observations.empty()) {
            return refinement::StableKey{
                0x6576616c6f627330ull}; /* "evalobs0" */
        }
        const refinement::AbstractFeatureExtraction extraction =
            refinement::extract_strict_abstract_features(
                model.calc->session(),
                model.calc->layout(),
                model.calc->state(pair.state),
                requirement);
        if (!extraction.complete()) {
            throw StrategyEvalUnsupported(
                "strategy evaluation unsupported:\n- node '" +
                strategy->nodes.at(pair.node).id +
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
        const EvalTransition& transition) const {
        refinement::StableKey key{
            0x6576616c74726e31ull}; /* "evaltrn1" */
        append_optional_u32(key, transition.edge);
        append_unveil_offer(
            key, pairs.at(transition.target).unveil_offer);
        append_compressed_policy_trace(
            key, transition.policy_route,
            transition.policy_state);
        return key;
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
        const bool use_replay_partition = pairs.size() >= 4096;
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
                row.transitions.size() + row.absorptions.size());
            for (const EvalTransition& transition : row.transitions) {
                node.arcs.push_back({
                    transition_partition_label(transition),
                    std::optional<std::uint32_t>{transition.target},
                    transition.probability});
            }
            for (const EvalAbsorption& absorption : row.absorptions) {
                node.arcs.push_back({
                    absorption_partition_label(absorption),
                    std::nullopt,
                    absorption.probability});
            }
            return node;
        };
        ClosedPartitionResult refined = use_replay_partition
            ? refinement::refine_closed_probabilistic_partition_replay(
                  static_cast<std::uint32_t>(pairs.size()),
                  replay_pair, {}, false, limits, false,
                  &replay_arc_source)
            : refinement::refine_closed_probabilistic_partition(
                  std::move(closed), limits);
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
                            saturated_product(
                                row.absorptions.capacity(),
                                sizeof(EvalAbsorption)))));
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
                for (const EvalTransition& transition :
                     source.transitions) {
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
                }
                for (const EvalAbsorption& absorption :
                     source.absorptions) {
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
                }

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
            retire_pair_discovery_indexes();
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
                            capped_product(
                                row.absorptions.capacity(),
                                sizeof(EvalAbsorption)))));
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
            std::uint64_t edge_count = 0;
            std::uint32_t validated_rows = 0;
            for (const EvalRow& row : attribution_rows) {
                edge_count = capped_add(
                    edge_count, row.transitions.size());
                for (const EvalTransition& transition : row.transitions) {
                    if (transition.target >= count ||
                        attribution_pairs[transition.target].row >=
                            row_count) {
                        throw std::logic_error(
                            "strategy evaluation exact attribution row "
                            "target is missing");
                    }
                }
                if ((++validated_rows & 127u) == 0u) {
                    co_await solve_detail::CooperativeCheckpoint{};
                }
            }
            if (edge_count >
                std::numeric_limits<std::uint32_t>::max()) {
                throw std::length_error(
                    "strategy evaluation exact row attribution edge count "
                    "overflowed");
            }
            check_owned_cap(capped_add(
                capped_product(edge_count, sizeof(PolicyEdge)),
                capped_product(
                    row_count,
                    sizeof(PolicyRow) +
                        5 * sizeof(std::uint32_t) +
                        2 * sizeof(std::uint8_t) +
                        3 * sizeof(double))));

            std::vector<std::uint32_t> incoming_counts(row_count, 0);
            std::uint32_t counted_rows = 0;
            for (const EvalRow& row : attribution_rows) {
                for (const EvalTransition& transition : row.transitions) {
                    const std::uint32_t target_row =
                        attribution_pairs[transition.target].row;
                    if (incoming_counts[target_row] ==
                        std::numeric_limits<std::uint32_t>::max()) {
                        throw std::length_error(
                            "strategy evaluation exact row attribution "
                            "incoming edge count overflowed");
                    }
                    ++incoming_counts[target_row];
                }
                if ((++counted_rows & 127u) == 0u) {
                    co_await solve_detail::CooperativeCheckpoint{
                        capped_product(
                            incoming_counts.capacity(),
                            sizeof(std::uint32_t))};
                }
            }

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
            for (std::uint32_t source = 0;
                 source < row_count; ++source) {
                for (const EvalTransition& transition :
                     attribution_rows[source].transitions) {
                    const std::uint32_t target_row =
                        attribution_pairs[transition.target].row;
                    transpose_edges[cursors[target_row]++] = {
                        source, transition.probability};
                }
                if ((source & 127u) == 127u) {
                    co_await solve_detail::CooperativeCheckpoint{
                        capped_add(
                            capped_product(
                                transpose_rows.capacity(),
                                sizeof(PolicyRow)),
                            capped_product(
                                transpose_edges.capacity(),
                                sizeof(PolicyEdge)))};
                }
            }

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
                    const EvalRow& row = attribution_rows[source];
                    if (!row.absorptions.empty()) has_exit = true;
                    for (const EvalTransition& transition :
                         row.transitions) {
                        const std::uint32_t target_row =
                            attribution_pairs[transition.target].row;
                        if (workspace.component_by_state[target_row] !=
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
                    for (const EvalTransition& transition :
                         attribution_rows[source].transitions) {
                        const std::uint32_t target_row =
                            attribution_pairs[transition.target].row;
                        if (workspace.component_by_state[target_row] !=
                            component) {
                            external_incoming[target_row] +=
                                row_visits[source].value() *
                                transition.probability;
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
            for (const EvalTransition& transition :
                 attribution_rows[row_id].transitions) {
                reconstructed.at(transition.target) +=
                    (value < 0.0
                         ? solve_detail::WideFloat{0.0}
                         : row_visits[row_id]) *
                    solve_detail::WideFloat{transition.probability};
            }
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
        std::fill(
            reconstructed.begin(), reconstructed.end(),
            solve_detail::WideFloat{0.0});
        reconstructed[attribution_start_pair] =
            solve_detail::WideFloat{1.0};
        for (std::uint32_t row_id = 0; row_id < row_count; ++row_id) {
            for (const EvalTransition& transition :
                 attribution_rows[row_id].transitions) {
                reconstructed.at(transition.target) +=
                    checked_row_mass[row_id] *
                    solve_detail::WideFloat{transition.probability};
            }
        }
        for (std::uint32_t state = 0; state < count; ++state) {
            const double expected_value = reconstructed[state].value();
            const double residual = std::fabs(
                (solve_detail::WideFloat{exact_visits[state]} -
                 reconstructed[state])
                    .value());
            const double scale = std::max(
                {1.0, std::fabs(exact_visits[state]),
                 std::fabs(expected_value)});
            const double tolerance =
                std::max(
                    options.epsilon,
                    8.0 * std::numeric_limits<double>::epsilon()) *
                scale;
            if (!std::isfinite(residual) || residual > tolerance) {
                throw std::runtime_error(
                    "strategy evaluation shared-row exact attribution raw "
                    "residual exceeded epsilon");
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
        if (attribution_rows.size() < count ||
            edge_count > std::numeric_limits<std::uint32_t>::max() ||
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
                for (const EvalTransition& transition : row.transitions) {
                    add_compressed_policy_incoming(
                        transition.policy_route,
                        transition.policy_state,
                        visits * transition.probability);
                }
                for (const EvalAbsorption& absorption : row.absorptions) {
                    const solve_detail::WideFloat wide_mass =
                        wide_visits *
                        solve_detail::WideFloat{
                            absorption.probability};
                    const double mass = wide_mass.value();
                    add_compressed_policy_incoming(
                        absorption.policy_route,
                        absorption.state, mass);
                    if (absorption.kind == EvalAbsorptionKind::Terminal) {
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
                }
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
        std::uint64_t compressed_top_owned_bytes =
            compressed_top_classes.capacity() *
            sizeof(std::vector<std::pair<std::uint32_t, double>>);

        std::uint64_t compressed_routes_seen = 0;
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
                    auto& top = compressed_top_classes[cursor];
                    if (options.top_classes_per_node != 0) {
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
        finalization_transient_floor += compressed_top_owned_bytes;

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
            if (compress_policy_routes && is_policy_route_node(
                    static_cast<std::uint32_t>(node))) {
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
            if (compress_policy_routes && is_policy_route_node(
                    static_cast<std::uint32_t>(node))) {
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
                        retire_pair_discovery_indexes();
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
    output.raw_pairs_discovered = static_cast<std::uint32_t>(
        std::min<std::size_t>(
            impl_->pairs.size(),
            std::numeric_limits<std::uint32_t>::max()));
    output.refined_pairs = output.raw_pairs_discovered;
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
