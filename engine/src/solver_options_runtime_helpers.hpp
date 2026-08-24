#pragma once

#include "solver_options_helpers.hpp"

namespace poecraft {
namespace solver {
namespace {

bool same_complete_option_kernel(
    const OptionKernel& left,
    const OptionKernel& right) {
    return left.legal == right.legal &&
           left.supported == right.supported &&
           left.terminates_almost_surely == right.terminates_almost_surely &&
           left.expected_primitive_actions ==
               right.expected_primitive_actions &&
           left.expected_resources == right.expected_resources &&
           left.exits == right.exits &&
           left.observation_choice_groups ==
               right.observation_choice_groups &&
           left.observation_choice_options ==
               right.observation_choice_options &&
           left.retry_states == right.retry_states &&
           left.continuation_states == right.continuation_states &&
           left.entry_continues == right.entry_continues;
}

bool same_option_transition_kernel(
    const OptionKernel& left,
    const OptionKernel& right) {
    return left.legal == right.legal &&
           left.supported == right.supported &&
           left.terminates_almost_surely == right.terminates_almost_surely &&
           left.expected_primitive_actions ==
               right.expected_primitive_actions &&
           left.exits == right.exits &&
           left.observation_choice_groups ==
               right.observation_choice_groups &&
           left.observation_choice_options ==
               right.observation_choice_options &&
           left.retry_states == right.retry_states &&
           left.continuation_states == right.continuation_states &&
           left.entry_continues == right.entry_continues;
}

bool same_option_template_planner(
    const PlannerOperator& left,
    const PlannerOperator& right) {
    return planner_operator_structurally_equal(left, right);
}

std::uint64_t option_planner_hash(const PlannerOperator& planner) {
    std::uint64_t hash = 1469598103934665603ull;
    const auto mix = [&](const std::uint64_t value) {
        hash ^= value;
        hash *= 1099511628211ull;
    };
    for (const std::uint64_t word :
         planner_operator_semantic_key(planner)) {
        mix(word);
    }
    return hash == 0 ? 1 : hash;
}

std::uint64_t option_template_hash(
    const PlannerOperator& planner,
    const OptionKernel& kernel) {
    std::uint64_t hash = 1469598103934665603ull;
    const auto mix = [&](const std::uint64_t value) {
        hash ^= value;
        hash *= 1099511628211ull;
    };
    for (const std::uint64_t word :
         planner_operator_semantic_key(planner)) {
        mix(word);
    }
    mix(kernel.legal ? 1u : 0u);
    mix(kernel.supported ? 1u : 0u);
    mix(kernel.terminates_almost_surely ? 1u : 0u);
    mix(kernel.entry_continues ? 1u : 0u);
    mix(std::bit_cast<std::uint64_t>(kernel.expected_primitive_actions));
    for (const auto& [key, quantity] : kernel.expected_resources) {
        for (const unsigned char c : key) mix(c);
        mix(0xffu);
        mix(std::bit_cast<std::uint64_t>(quantity));
    }
    mix(kNoId);
    for (const OutcomeEntry& exit : kernel.exits) {
        mix(exit.state);
        mix(std::bit_cast<std::uint64_t>(exit.probability));
    }
    mix(kNoId);
    for (const OutcomeChoiceGroup& group :
         kernel.observation_choice_groups) {
        mix(std::bit_cast<std::uint64_t>(group.probability));
        mix(group.observation_state);
        for (const std::uint32_t state : group.states) mix(state);
        mix(kNoId);
    }
    for (const OutcomeChoiceOption& choice :
         kernel.observation_choice_options) {
        mix(choice.mod_id);
        mix(choice.state);
        mix(choice.observation_state);
        mix(choice.actual_state);
    }
    for (const std::uint32_t state : kernel.retry_states) mix(state);
    mix(kNoId);
    for (const std::uint32_t state : kernel.continuation_states) mix(state);
    return hash == 0 ? 1 : hash;
}

std::uint64_t option_transition_hash(const OptionKernel& kernel) {
    std::uint64_t hash = 1469598103934665603ull;
    const auto mix = [&](const std::uint64_t value) {
        hash ^= value;
        hash *= 1099511628211ull;
    };
    mix(kernel.legal ? 1u : 0u);
    mix(kernel.supported ? 1u : 0u);
    mix(kernel.terminates_almost_surely ? 1u : 0u);
    mix(kernel.entry_continues ? 1u : 0u);
    mix(std::bit_cast<std::uint64_t>(kernel.expected_primitive_actions));
    for (const OutcomeEntry& exit : kernel.exits) {
        mix(exit.state);
        mix(std::bit_cast<std::uint64_t>(exit.probability));
    }
    mix(kNoId);
    for (const OutcomeChoiceGroup& group :
         kernel.observation_choice_groups) {
        mix(std::bit_cast<std::uint64_t>(group.probability));
        mix(group.observation_state);
        for (const std::uint32_t state : group.states) mix(state);
        mix(kNoId);
    }
    for (const OutcomeChoiceOption& choice :
         kernel.observation_choice_options) {
        mix(choice.mod_id);
        mix(choice.state);
        mix(choice.observation_state);
        mix(choice.actual_state);
    }
    for (const std::uint32_t state : kernel.retry_states) mix(state);
    mix(kNoId);
    for (const std::uint32_t state : kernel.continuation_states) mix(state);
    return hash == 0 ? 1 : hash;
}

std::uint64_t option_kernel_selected_bytes(const OptionKernel& kernel) {
    std::uint64_t bytes = sizeof(kernel);
    bytes += kernel.expected_resources.capacity() *
             sizeof(std::pair<std::string, double>);
    for (const auto& [key, unused] : kernel.expected_resources) {
        (void)unused;
        bytes += key.capacity() + 1;
    }
    bytes += kernel.exits.capacity() * sizeof(OutcomeEntry);
    bytes += kernel.observation_choice_groups.capacity() *
             sizeof(OutcomeChoiceGroup);
    for (const OutcomeChoiceGroup& group :
         kernel.observation_choice_groups) {
        bytes += group.states.capacity() * sizeof(std::uint32_t);
    }
    bytes += kernel.observation_choice_options.capacity() *
             sizeof(OutcomeChoiceOption);
    bytes += kernel.retry_states.capacity() * sizeof(std::uint32_t);
    bytes += kernel.continuation_states.capacity() * sizeof(std::uint32_t);
    bytes += kernel.automatic_candidate_attempt_entries.capacity() *
             sizeof(OutcomeEntry);
    bytes += kernel.automatic.legality_result.capacity() + 1;
    bytes += kernel.automatic.reason.capacity() + 1;
    return bytes;
}

std::string temporary_evaluation_key(
    const SessionImpl& session,
    const ActionRegistry& registry,
    const PlannerOperator& planner) {
    if (planner.option_kind != FixedOptionKind::TemporaryBenchRepeat ||
        planner.setup_action == kNoId ||
        planner.followup_action == kNoId ||
        planner.cleanup_action == kNoId) {
        return {};
    }
    const ActionDescriptor& blocker =
        registry.actions.at(planner.setup_action);
    if (blocker.params.mod_id >= session.mod_count) return {};
    std::string key =
        std::to_string(planner.followup_action) + ':' +
        std::to_string(planner.cleanup_action) + ':' +
        std::to_string(planner.exit_min_satisfied) + ':' +
        std::to_string(
            planner.primitive_program.size() == 4 &&
            planner.primitive_program.front() == planner.cleanup_action) + ':' +
        std::to_string(session.metamod_type[blocker.params.mod_id]) + ':' +
        std::to_string(
            static_cast<int>(session.gen_type[blocker.params.mod_id])) + ':';
    for (const std::uint32_t slot : planner.exit_goal_slots) {
        key += std::to_string(slot) + ',';
    }
    key.push_back(':');
    std::vector<std::uint32_t> groups;
    for (std::uint32_t row = session.group_offsets[blocker.params.mod_id];
         row < session.group_offsets[blocker.params.mod_id + 1]; ++row) {
        groups.push_back(session.group_ids[row]);
    }
    std::sort(groups.begin(), groups.end());
    groups.erase(std::unique(groups.begin(), groups.end()), groups.end());
    for (const std::uint32_t group : groups) {
        key += std::to_string(group) + ',';
    }
    return key;
}

PlannerOperator temporary_variant_planner(
    const ActionRegistry& registry,
    const PlannerOperator& representative,
    const OptionKernel& kernel,
    const std::uint32_t blocker_action) {
    PlannerOperator variant = representative;
    const ActionDescriptor& old_blocker =
        registry.actions.at(representative.setup_action);
    const ActionDescriptor& blocker = registry.actions.at(blocker_action);
    const ActionDescriptor& followup =
        registry.actions.at(representative.followup_action);
    variant.setup_action = blocker_action;
    if (!variant.primitive_program.empty()) {
        const std::size_t blocker_position =
            variant.primitive_program.size() == 4 &&
                    variant.primitive_program.front() ==
                        variant.cleanup_action
                ? 1
                : 0;
        variant.primitive_program[blocker_position] = blocker_action;
    }
    variant.id = "option:temporary_bench_repeat:" + blocker.id + ':' +
                 followup.id + ":until:" +
                 std::to_string(variant.exit_min_satisfied) + ':';
    for (std::size_t i = 0; i < variant.exit_goal_slots.size(); ++i) {
        if (i != 0) variant.id.push_back('+');
        variant.id += std::to_string(variant.exit_goal_slots[i]);
    }
    variant.display_name = "Temporary " + blocker.display_name +
                           " then repeat " + followup.display_name;

    std::map<std::string, double> resources(
        kernel.expected_resources.begin(), kernel.expected_resources.end());
    for (const std::string& key : old_blocker.cost_keys) {
        auto found = resources.find(key);
        if (found == resources.end()) continue;
        found->second -= 1.0;
        if (std::abs(found->second) <= 1e-15) resources.erase(found);
    }
    for (const std::string& key : blocker.cost_keys) resources[key] += 1.0;
    variant.resource_quantities.assign(resources.begin(), resources.end());
    bind_planner_primitive_action_ids(registry, variant);
    return variant;
}

bool same_complete_distribution(
    const OutcomeDistribution& left,
    const OutcomeDistribution& right) {
    return left.supported == right.supported &&
           left.applicable == right.applicable &&
           left.entries == right.entries &&
           left.choice_groups == right.choice_groups &&
           left.choice_options == right.choice_options &&
           left.slot_satisfied_probability ==
               right.slot_satisfied_probability;
}

std::uint64_t mapped_kernel_hash(const OptionKernel& kernel) {
    AttemptKernel attempt;
    attempt.supported = kernel.supported;
    attempt.fully_legal = kernel.legal;
    attempt.entries = kernel.exits;
    attempt.choice_groups = kernel.observation_choice_groups;
    attempt.choice_options = kernel.observation_choice_options;
    return attempt_kernel_hash(attempt);
}

std::uint32_t map_local_state(
    CalcContext& local,
    CalcContext& destination,
    const std::uint32_t local_state,
    std::unordered_map<std::uint32_t, std::uint32_t>& mapped) {
    if (local_state == kNoId) return kNoId;
    const auto found = mapped.find(local_state);
    if (found != mapped.end()) return found->second;
    pc_item_state item;
    if (!local.materialize(local_state, item)) {
        throw std::runtime_error(
            "automatic candidate local state cannot be materialized");
    }
    const std::uint32_t result = destination.intern_item(item);
    mapped.emplace(local_state, result);
    return result;
}

OutcomeDistribution map_local_distribution(
    CalcContext& local,
    CalcContext& destination,
    const OutcomeDistribution& source,
    std::unordered_map<std::uint32_t, std::uint32_t>& mapped) {
    OutcomeDistribution result = source;
    std::map<std::uint32_t, double> entries;
    for (const OutcomeEntry& entry : source.entries) {
        entries[map_local_state(
            local, destination, entry.state, mapped)] += entry.probability;
    }
    result.entries.clear();
    for (const auto& [state, probability] : entries) {
        result.entries.push_back({state, probability});
    }
    for (OutcomeChoiceGroup& group : result.choice_groups) {
        group.observation_state = map_local_state(
            local, destination, group.observation_state, mapped);
        for (std::uint32_t& state : group.states) {
            state = map_local_state(local, destination, state, mapped);
        }
        std::sort(group.states.begin(), group.states.end());
        group.states.erase(
            std::unique(group.states.begin(), group.states.end()),
            group.states.end());
    }
    for (OutcomeChoiceOption& choice : result.choice_options) {
        choice.state = map_local_state(
            local, destination, choice.state, mapped);
        choice.observation_state = map_local_state(
            local, destination, choice.observation_state, mapped);
        choice.actual_state = map_local_state(
            local, destination, choice.actual_state, mapped);
    }
    result.stable_shared_kernel = false;
    return result;
}

OptionKernel map_local_option_kernel(
    CalcContext& local,
    CalcContext& destination,
    const OptionKernel& source,
    std::unordered_map<std::uint32_t, std::uint32_t>& mapped) {
    OptionKernel result = source;
    result.retained_template_storage = false;
    std::map<std::uint32_t, double> exits;
    for (const OutcomeEntry& exit : source.exits) {
        exits[map_local_state(
            local, destination, exit.state, mapped)] += exit.probability;
    }
    result.exits.clear();
    for (const auto& [state, probability] : exits) {
        result.exits.push_back({state, probability});
    }
    for (OutcomeChoiceGroup& group : result.observation_choice_groups) {
        group.observation_state = map_local_state(
            local, destination, group.observation_state, mapped);
        for (std::uint32_t& state : group.states) {
            state = map_local_state(local, destination, state, mapped);
        }
        std::sort(group.states.begin(), group.states.end());
        group.states.erase(
            std::unique(group.states.begin(), group.states.end()),
            group.states.end());
    }
    for (OutcomeChoiceOption& choice : result.observation_choice_options) {
        choice.state = map_local_state(
            local, destination, choice.state, mapped);
        choice.observation_state = map_local_state(
            local, destination, choice.observation_state, mapped);
        choice.actual_state = map_local_state(
            local, destination, choice.actual_state, mapped);
    }
    for (std::uint32_t& state : result.retry_states) {
        state = map_local_state(local, destination, state, mapped);
    }
    for (std::uint32_t& state : result.continuation_states) {
        state = map_local_state(local, destination, state, mapped);
    }
    result.automatic_candidate_attempt_entries.clear();
    result.automatic_candidate_attempt_entries.shrink_to_fit();
    std::sort(result.retry_states.begin(), result.retry_states.end());
    result.retry_states.erase(
        std::unique(result.retry_states.begin(), result.retry_states.end()),
        result.retry_states.end());
    std::sort(
        result.continuation_states.begin(),
        result.continuation_states.end());
    result.continuation_states.erase(
        std::unique(
            result.continuation_states.begin(),
            result.continuation_states.end()),
        result.continuation_states.end());
    result.automatic.candidate_kernel_hash = mapped_kernel_hash(result);
    return result;
}

void require_import_reference_shape(
    const std::uint32_t index,
    const std::string& id,
    const char* field) {
    if ((index == kNoId) != id.empty()) {
        throw std::invalid_argument(
            std::string("planner operator has an incomplete ") +
            field + " reference");
    }
}

std::uint32_t resolve_imported_primitive(
    const ActionRegistry& registry,
    const std::string& id,
    const char* field) {
    const auto found = registry.index_by_id.find(id);
    if (found == registry.index_by_id.end() ||
        found->second >= registry.actions.size() ||
        registry.actions[found->second].id != id) {
        throw std::invalid_argument(
            std::string("planner operator ") + field +
            " dependency is absent from the destination registry: " + id);
    }
    const ActionDescriptor& action = registry.actions[found->second];
    if (!calc_supports(action)) {
        throw std::invalid_argument(
            std::string("planner operator ") + field +
            " dependency has no exact calculator authority: " + id);
    }
    validate_action_refinement_contract(action);
    return found->second;
}

std::uint32_t resolve_imported_bestiary(
    const SessionImpl& session,
    const std::string& id,
    const char* field) {
    std::uint32_t result = kNoId;
    for (std::uint32_t index = 0;
         index < session.data->bestiary_actions.size(); ++index) {
        if (session.data->bestiary_actions[index].id != id) continue;
        if (result != kNoId) {
            throw std::invalid_argument(
                std::string("planner operator ") + field +
                " dependency is ambiguous in the destination session: " +
                id);
        }
        result = index;
    }
    if (result == kNoId) {
        throw std::invalid_argument(
            std::string("planner operator ") + field +
            " dependency is absent from the destination session: " + id);
    }
    return result;
}

} // namespace

} // namespace solver
} // namespace poecraft
