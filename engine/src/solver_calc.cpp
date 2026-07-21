#include "solver_internal.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <functional>
#include <limits>
#include <map>
#include <stdexcept>
#include <tuple>
#include <unordered_set>
#include <vector>

#include "poecraft/bitset.h"
#include "poecraft/item_state.h"

/*
 * Solver S2: the calculation engine's exact deterministic and single-slot
 * transition paths (docs/solver/crafting-solver-plan.md, Calculation Engine).
 *
 * Exactness strategy: materialize one representative concrete item for the
 * abstract state, then either apply the engine action directly (RNG-free
 * deterministic actions) or enumerate the same weighted pool the action
 * samples (get_weighted_pool with the action's request) and group candidate
 * outcomes by projected abstract successor. The engine's action code stays
 * the single execution authority; nothing here re-derives weight rules.
 *
 * Reforge-class and bespoke evaluators share the same projected state and
 * engine-owned pool semantics. Unveil additionally records sampled offer
 * sets so Bellman backups can choose the cheapest offered successor.
 */
namespace poecraft {
namespace solver {

namespace {

PrimitiveTelemetryFamily primitive_family(const ActionType type) {
    switch (type) {
    case ActionType::Essence:
        return PrimitiveTelemetryFamily::Essence;
    case ActionType::Fossil:
        return PrimitiveTelemetryFamily::Fossil;
    case ActionType::HarvestReforge:
    case ActionType::HarvestAugment:
    case ActionType::HarvestResist:
        return PrimitiveTelemetryFamily::Harvest;
    case ActionType::Bench:
    case ActionType::RemoveCraftedModifiers:
        return PrimitiveTelemetryFamily::Bench;
    case ActionType::Fracture:
        return PrimitiveTelemetryFamily::Fracture;
    case ActionType::Transmute:
    case ActionType::Augment:
    case ActionType::Alteration:
    case ActionType::Regal:
    case ActionType::Alchemy:
    case ActionType::Chaos:
    case ActionType::Exalt:
    case ActionType::Annul:
    case ActionType::Scour:
        return PrimitiveTelemetryFamily::Currency;
    default:
        return PrimitiveTelemetryFamily::Other;
    }
}

std::uint64_t selected_string_bytes(const std::string& value) {
    return static_cast<std::uint64_t>(value.capacity() + 1);
}

std::uint64_t planner_operator_nested_bytes(const PlannerOperator& value) {
    std::uint64_t bytes =
        selected_string_bytes(value.id) +
        selected_string_bytes(value.display_name);
    bytes += value.primitive_program.capacity() * sizeof(std::uint32_t);
    bytes += value.exit_goal_slots.capacity() * sizeof(std::uint32_t);
    bytes += value.resource_quantities.capacity() *
             sizeof(std::pair<std::string, double>);
    for (const auto& [key, quantity] : value.resource_quantities) {
        (void)quantity;
        bytes += selected_string_bytes(key);
    }
    return bytes;
}

std::uint64_t distribution_selected_bytes(
    const OutcomeDistribution& value) {
    std::uint64_t bytes = sizeof(OutcomeDistribution);
    bytes += value.entries.capacity() * sizeof(OutcomeEntry);
    bytes += value.choice_groups.capacity() * sizeof(OutcomeChoiceGroup);
    for (const OutcomeChoiceGroup& group : value.choice_groups) {
        bytes += group.states.capacity() * sizeof(std::uint32_t);
    }
    bytes += value.choice_options.capacity() * sizeof(OutcomeChoiceOption);
    return bytes;
}

std::uint64_t option_kernel_selected_bytes_for_ledger(
    const OptionKernel& value) {
    std::uint64_t bytes = sizeof(OptionKernel);
    bytes += value.expected_resources.capacity() *
             sizeof(std::pair<std::string, double>);
    for (const auto& [key, quantity] : value.expected_resources) {
        (void)quantity;
        bytes += selected_string_bytes(key);
    }
    bytes += value.exits.capacity() * sizeof(OutcomeEntry);
    bytes += value.observation_choice_groups.capacity() *
             sizeof(OutcomeChoiceGroup);
    for (const OutcomeChoiceGroup& group :
         value.observation_choice_groups) {
        bytes += group.states.capacity() * sizeof(std::uint32_t);
    }
    bytes += value.observation_choice_options.capacity() *
             sizeof(OutcomeChoiceOption);
    bytes += value.retry_states.capacity() * sizeof(std::uint32_t);
    bytes += value.continuation_states.capacity() * sizeof(std::uint32_t);
    bytes += value.automatic_candidate_attempt_entries.capacity() *
             sizeof(OutcomeEntry);
    bytes += selected_string_bytes(value.automatic.legality_result);
    bytes += selected_string_bytes(value.automatic.reason);
    return bytes;
}

} // namespace

std::uint8_t rarity_affix_cap(const SessionImpl& session, std::uint8_t rarity) {
    switch (rarity) {
    case PC_RARITY_NORMAL:
        return 0;
    case PC_RARITY_MAGIC:
        return 1;
    default:
        return session.rare_affix_cap;
    }
}

namespace {

bool slot_metamod_matches(
    const SessionImpl& session,
    const pc_mod_slot& slot,
    int code) {
    return code >= 0 && slot.mod_id < session.metamod_type.size() &&
           session.metamod_type[slot.mod_id] == code;
}

bool item_side_locked(
    const SessionImpl& session,
    const pc_item_state& item,
    int side) {
    const int code = side == PC_SIDE_PREFIX
                         ? session.data->metamod_prefixes_locked_code
                         : session.data->metamod_suffixes_locked_code;
    for (std::uint8_t i = 0; i < item.prefix_count; ++i) {
        if (slot_metamod_matches(session, item.prefixes[i], code)) return true;
    }
    for (std::uint8_t i = 0; i < item.suffix_count; ++i) {
        if (slot_metamod_matches(session, item.suffixes[i], code)) return true;
    }
    return false;
}

void mod_group_list(
    const SessionImpl& session,
    std::uint32_t mod_id,
    std::vector<std::uint32_t>& out) {
    for (std::uint32_t i = session.group_offsets[mod_id];
         i < session.group_offsets[mod_id + 1]; ++i) {
        out.push_back(session.group_ids[i]);
    }
}

int dominant_eldritch(const pc_item_state& item) {
    if (item.searing_exarch_tier > item.eater_of_worlds_tier) return 0;
    if (item.eater_of_worlds_tier > item.searing_exarch_tier) return 1;
    return -1;
}

bool mod_groups_conflict(
    const SessionImpl& session,
    const pc_item_state& item,
    std::uint32_t mod_id,
    int skip_side = -1,
    std::uint32_t skip_index = kNoId) {
    const auto conflicts = [&](const pc_mod_slot& slot) {
        if (slot.mod_id >= session.mod_count) return false;
        for (std::uint32_t a = session.group_offsets[slot.mod_id];
             a < session.group_offsets[slot.mod_id + 1]; ++a) {
            for (std::uint32_t b = session.group_offsets[mod_id];
                 b < session.group_offsets[mod_id + 1]; ++b) {
                if (session.group_ids[a] == session.group_ids[b]) return true;
            }
        }
        return false;
    };
    for (std::uint8_t i = 0; i < item.prefix_count; ++i) {
        if (skip_side == PC_SIDE_PREFIX && skip_index == i) continue;
        if (conflicts(item.prefixes[i])) return true;
    }
    for (std::uint8_t i = 0; i < item.suffix_count; ++i) {
        if (skip_side == PC_SIDE_SUFFIX && skip_index == i) continue;
        if (conflicts(item.suffixes[i])) return true;
    }
    return false;
}

bool has_class_tag(
    const SessionImpl& session,
    std::uint32_t mod_id,
    std::uint32_t tag_id) {
    if (mod_id >= session.mod_count) return false;
    for (std::uint32_t i = session.class_offsets[mod_id];
         i < session.class_offsets[mod_id + 1]; ++i) {
        if (session.class_tag_ids[i] == tag_id) return true;
    }
    return false;
}

std::uint32_t unveil_weight(
    const SessionImpl& session,
    std::uint32_t mod_id) {
    if (mod_id >= session.mod_count) return 0;
    const DataImpl& data = *session.data;
    const std::uint32_t global = session.global_index[mod_id];
    if (global + 1 >= data.spawn_offsets.size()) return 0;
    std::unordered_set<std::uint32_t> tags(
        session.effective_base_tag_ids.begin(),
        session.effective_base_tag_ids.end());
    for (std::uint32_t i = data.spawn_offsets[global];
         i < data.spawn_offsets[global + 1]; ++i) {
        if (tags.count(data.spawn_tag_ids[i])) {
            return data.spawn_weights[i] > 0
                       ? static_cast<std::uint32_t>(data.spawn_weights[i])
                       : 0;
        }
    }
    return 0;
}

} // namespace

CalcContext::CalcContext(
    std::shared_ptr<const SessionImpl> session,
    const GoalSpec& goal,
    ActionRegistry registry,
    const std::vector<std::uint32_t>& action_indices,
    bool allow_empty_goal,
    bool empty_actions_mean_all,
    bool distinguish_junk_exclusion_effects,
    std::optional<std::uint32_t> state_cap,
    const std::vector<CountObservation>& count_observations)
    : session_(std::move(session)),
      goal_(goal),
      registry_(std::move(registry)),
      candidates_(action_indices),
      context_(0),
      state_cap_(state_cap) {
    if (candidates_.empty() && empty_actions_mean_all) {
        candidates_.reserve(registry_.actions.size());
        for (std::uint32_t i = 0; i < registry_.actions.size(); ++i) {
            if (!registry_.actions[i].automatic_dependency_only) {
                candidates_.push_back(i);
            }
        }
    }
    const auto planner_started = std::chrono::steady_clock::now();
    operators_ = build_planner_operators(
        *session_, goal_, registry_, candidates_);
    planner_build_ns_ = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - planner_started)
            .count());
    action_control_.explicit_envelope = goal_.primitive_actions_explicit;
    action_control_.registry_actions = static_cast<std::uint32_t>(
        registry_.actions.size());
    action_control_.included_primitives = static_cast<std::uint32_t>(
        candidates_.size());
    action_control_.automatic_options = static_cast<std::uint32_t>(
        std::count_if(
            candidates_.begin(), candidates_.end(),
            [&](const std::uint32_t index) {
                return operators_.at(index).automatic_kind !=
                       AutomaticCandidateKind::None;
            }));
    action_control_.automatic_dependency_primitives = 0;
    action_control_.pruned_outside_goal_relevance =
        registry_.goal_relevant_actions_pruned;
    action_control_.pruned_outside_envelope =
        goal_.primitive_actions_explicit
            ? static_cast<std::uint32_t>(
                  registry_.actions.size() - candidates_.size() +
                  registry_.fossil_loadouts_deferred)
            : 0;
    action_control_.deferred_fossil_loadouts =
        goal_.primitive_actions_explicit
            ? 0
            : registry_.fossil_loadouts_deferred;
    /* Option dependencies participate in abstraction without silently widening
     * an explicit primitive candidate subset. Callers can select the option
     * alone, or list any dependency in actions when it should also remain an
     * independently selectable primitive. */
    std::vector<std::uint32_t> layout_actions = candidates_;
    for (std::uint32_t operator_index =
             static_cast<std::uint32_t>(registry_.actions.size());
         operator_index < operators_.size(); ++operator_index) {
        for (const std::uint32_t dependency :
             operators_[operator_index].primitive_program) {
            if (std::find(layout_actions.begin(), layout_actions.end(),
                          dependency) == layout_actions.end()) {
                layout_actions.push_back(dependency);
                ++action_control_.dependency_primitives;
            }
        }
        const std::uint32_t conditional =
            operators_[operator_index].conditional_action;
        if (conditional != kNoId &&
            std::find(layout_actions.begin(), layout_actions.end(),
                      conditional) == layout_actions.end()) {
            layout_actions.push_back(conditional);
            ++action_control_.dependency_primitives;
        }
    }
    candidate_operators_ = candidates_;
    for (std::uint32_t operator_index =
             static_cast<std::uint32_t>(registry_.actions.size());
         operator_index < operators_.size(); ++operator_index) {
        candidate_operators_.push_back(operator_index);
    }
    static_candidate_operator_count_ = candidate_operators_.size();
    const bool exact_group_effects =
        distinguish_junk_exclusion_effects ||
        std::any_of(
            layout_actions.begin(), layout_actions.end(),
            [&](std::uint32_t index) {
                const ActionType type =
                    registry_.actions.at(index).params.type;
                return type == ActionType::Unveil ||
                       type == ActionType::HarvestResist ||
                       type == ActionType::Fracture ||
                       type == ActionType::RemoveCraftedModifiers;
            });
    const auto layout_started = std::chrono::steady_clock::now();
    layout_ = build_abstract_layout(
        *session_, goal_, registry_, layout_actions, allow_empty_goal,
        false, exact_group_effects, count_observations);
    layout_build_ns_ = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - layout_started)
            .count());
    context_.session = session_;
    /* Exact paths never sample, and evaluation must not depend on trace
     * bookkeeping from earlier queries. */
    context_.capture_action_trace = false;
    initialize_temporary_bench_effect_classes();
    const auto ledger_started = std::chrono::steady_clock::now();
    initialize_owned_bytes_ledger();
    owned_byte_ledger_init_ns_ = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - ledger_started)
            .count());
}

void CalcContext::initialize_owned_bytes_ledger() {
    owned_bytes_dynamic_shallow_base_ = dynamic_shallow_owned_bytes();
    owned_bytes_base_ = estimated_owned_bytes();
    owned_bytes_ledger_initialized_ = true;
}

std::uint64_t CalcContext::dynamic_shallow_owned_bytes() const {
    std::uint64_t bytes = 0;
    bytes += candidate_operators_.capacity() * sizeof(std::uint32_t);
    bytes += state_local_automatic_operators_.bucket_count() * sizeof(void*);
    bytes += state_local_automatic_operators_.size() *
             (sizeof(std::pair<const std::uint32_t,
                               std::vector<std::uint32_t>>) +
              2 * sizeof(void*));
    bytes += admitted_automatic_dependencies_.bucket_count() * sizeof(void*);
    bytes += admitted_automatic_dependencies_.size() *
             (sizeof(std::uint32_t) + 2 * sizeof(void*));
    bytes += state_local_automatic_operator_indices_.bucket_count() *
             sizeof(void*);
    bytes += state_local_automatic_operator_indices_.size() *
             (sizeof(std::uint32_t) + 2 * sizeof(void*));
    bytes += operators_.capacity() * sizeof(PlannerOperator);
    bytes += states_.capacity() * sizeof(AbstractState);
    bytes += state_ids_by_hash_.bucket_count() * sizeof(void*);
    bytes += state_ids_by_hash_.size() *
             (sizeof(std::pair<const std::size_t, StateHashBucket>) +
              2 * sizeof(void*));
    bytes += distribution_cache_.bucket_count() * sizeof(void*);
    bytes += distribution_cache_.size() *
             (sizeof(std::pair<
                  const std::uint64_t,
                  std::shared_ptr<const OutcomeDistribution>>) +
              2 * sizeof(void*));
    bytes += owned_distribution_payload_refs_.bucket_count() * sizeof(void*);
    bytes += owned_distribution_payload_refs_.size() *
             (sizeof(std::pair<const OutcomeDistribution* const,
                               std::uint32_t>) +
              2 * sizeof(void*));
    bytes += option_kernel_cache_.bucket_count() * sizeof(void*);
    bytes += option_kernel_cache_.size() *
             (sizeof(std::pair<
                  const std::uint64_t,
                  std::shared_ptr<const OptionKernel>>) +
              2 * sizeof(void*));
    bytes += option_kernel_templates_.bucket_count() * sizeof(void*);
    bytes += option_kernel_templates_.size() *
             (sizeof(std::pair<
                  const std::uint64_t,
                  std::vector<OptionKernelTemplateMemo>>) +
              2 * sizeof(void*));
    bytes += option_transition_templates_.bucket_count() * sizeof(void*);
    bytes += option_transition_templates_.size() *
             (sizeof(std::pair<
                  const std::uint64_t,
                  std::vector<std::shared_ptr<const OptionKernel>>>) +
              2 * sizeof(void*));
    bytes += option_operator_templates_.bucket_count() * sizeof(void*);
    bytes += option_operator_templates_.size() *
             (sizeof(std::pair<
                  const std::uint64_t,
                  std::vector<std::uint32_t>>) +
              2 * sizeof(void*));
    bytes += option_kernel_template_hit_keys_.bucket_count() * sizeof(void*);
    bytes += option_kernel_template_hit_keys_.size() *
             (sizeof(std::uint64_t) + 2 * sizeof(void*));
    bytes += reforge_cache_.size() *
             (sizeof(std::pair<
                   const std::pair<std::uint32_t, std::uint64_t>,
                   std::vector<ReforgeCacheMemo>>) +
               3 * sizeof(void*));
    bytes += telemetry_rows_.bucket_count() * sizeof(void*);
    bytes += telemetry_rows_.size() *
             (sizeof(std::pair<const std::uint64_t, std::uint8_t>) +
              2 * sizeof(void*));
    return bytes;
}

std::uint64_t CalcContext::fast_estimated_owned_bytes() const {
    const auto started = std::chrono::steady_clock::now();
    const std::uint64_t shallow = dynamic_shallow_owned_bytes();
    std::uint64_t bytes = owned_bytes_base_;
    if (shallow >= owned_bytes_dynamic_shallow_base_) {
        bytes += shallow - owned_bytes_dynamic_shallow_base_;
    } else {
        bytes -= std::min(
            bytes, owned_bytes_dynamic_shallow_base_ - shallow);
    }
    bytes += owned_state_hash_collision_bytes_;
    bytes += owned_state_local_operator_bytes_;
    bytes += owned_added_operator_nested_bytes_;
    bytes += owned_distribution_payload_bytes_;
    bytes += owned_option_cache_payload_bytes_;
    bytes += owned_option_template_nested_bytes_;
    bytes += owned_transition_template_nested_bytes_;
    bytes += owned_operator_template_nested_bytes_;
    bytes += owned_reforge_payload_bytes_;
    if (automatic_comparison_context_ != nullptr) {
        bytes += automatic_comparison_context_->fast_estimated_owned_bytes();
    }
    bytes += automatic_admission_contexts_.bucket_count() * sizeof(void*);
    bytes += automatic_admission_contexts_.size() *
             (sizeof(std::pair<const std::string,
                               std::unique_ptr<CalcContext>>) +
              2 * sizeof(void*));
    for (const auto& [key, context] : automatic_admission_contexts_) {
        bytes += key.capacity() + 1;
        bytes += context->fast_estimated_owned_bytes();
    }
    ++telemetry_.owned_byte_ledger_requests;
    telemetry_.owned_byte_ledger_ns += static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - started)
            .count());
    return bytes;
}

void CalcContext::account_new_operator(const PlannerOperator& value) {
    owned_added_operator_nested_bytes_ +=
        planner_operator_nested_bytes(value);
}

void CalcContext::account_state_local_operators(
    const std::vector<std::uint32_t>& values) {
    owned_state_local_operator_bytes_ +=
        values.capacity() * sizeof(std::uint32_t);
}

void CalcContext::account_distribution_cache_insert(
    const std::uint64_t key,
    const std::shared_ptr<const OutcomeDistribution>& value) {
    const auto existing = distribution_cache_.find(key);
    if (existing != distribution_cache_.end()) {
        release_distribution_payload(existing->second);
    }
    retain_distribution_payload(value);
}

void CalcContext::account_distribution_cache_erase(
    const std::uint64_t key) {
    const auto existing = distribution_cache_.find(key);
    if (existing == distribution_cache_.end()) return;
    release_distribution_payload(existing->second);
}

void CalcContext::retain_distribution_payload(
    const std::shared_ptr<const OutcomeDistribution>& value) {
    auto [it, inserted] =
        owned_distribution_payload_refs_.try_emplace(value.get(), 0);
    if (inserted) {
        owned_distribution_payload_bytes_ +=
            distribution_selected_bytes(*value);
    }
    ++it->second;
}

void CalcContext::release_distribution_payload(
    const std::shared_ptr<const OutcomeDistribution>& value) {
    const auto it = owned_distribution_payload_refs_.find(value.get());
    if (it == owned_distribution_payload_refs_.end() || it->second == 0) {
        throw std::logic_error(
            "selected-owned distribution payload released without owner");
    }
    if (--it->second == 0) {
        owned_distribution_payload_bytes_ -=
            distribution_selected_bytes(*value);
        owned_distribution_payload_refs_.erase(it);
    }
}

void CalcContext::account_option_cache_insert(
    const std::uint64_t key,
    const std::shared_ptr<const OptionKernel>& value) {
    const auto existing = option_kernel_cache_.find(key);
    if (existing != option_kernel_cache_.end() &&
        !existing->second->retained_template_storage) {
        owned_option_cache_payload_bytes_ -=
            option_kernel_selected_bytes_for_ledger(*existing->second);
    }
    if (!value->retained_template_storage) {
        owned_option_cache_payload_bytes_ +=
            option_kernel_selected_bytes_for_ledger(*value);
    }
}

void CalcContext::account_option_cache_erase(const std::uint64_t key) {
    const auto existing = option_kernel_cache_.find(key);
    if (existing == option_kernel_cache_.end() ||
        existing->second->retained_template_storage) {
        return;
    }
    owned_option_cache_payload_bytes_ -=
        option_kernel_selected_bytes_for_ledger(*existing->second);
}

void CalcContext::account_option_template_insert(
    const std::size_t old_capacity,
    const OptionKernelTemplateMemo& value) {
    const auto& bucket =
        option_kernel_templates_.at(value.kernel->retained_template_id);
    owned_option_template_nested_bytes_ +=
        (bucket.capacity() - old_capacity) *
        sizeof(OptionKernelTemplateMemo);
    owned_option_template_nested_bytes_ +=
        value.expected_resources.capacity() *
        sizeof(std::pair<std::string, double>);
    for (const auto& [key, quantity] : value.expected_resources) {
        (void)quantity;
        owned_option_template_nested_bytes_ +=
            selected_string_bytes(key);
    }
    if (!value.kernel->retained_template_storage) {
        value.kernel->retained_template_storage = true;
        owned_option_template_nested_bytes_ +=
            option_kernel_selected_bytes_for_ledger(*value.kernel);
    }
}

void CalcContext::account_transition_template_insert(
    const std::size_t old_capacity,
    const std::shared_ptr<const OptionKernel>& value) {
    const auto& bucket =
        option_transition_templates_.at(value->retained_template_id);
    owned_transition_template_nested_bytes_ +=
        (bucket.capacity() - old_capacity) *
        sizeof(std::shared_ptr<const OptionKernel>);
    if (!value->retained_template_storage) {
        value->retained_template_storage = true;
        owned_transition_template_nested_bytes_ +=
            option_kernel_selected_bytes_for_ledger(*value);
    }
}

void CalcContext::account_operator_template_insert(
    const std::size_t old_capacity,
    const std::vector<std::uint32_t>& values) {
    owned_operator_template_nested_bytes_ +=
        (values.capacity() - old_capacity) * sizeof(std::uint32_t);
}

void CalcContext::account_reforge_cache_insert(
    const std::size_t old_capacity,
    const std::size_t new_capacity,
    const ReforgeCacheMemo& value) {
    owned_reforge_payload_bytes_ +=
        (new_capacity - old_capacity) * sizeof(ReforgeCacheMemo);
    owned_reforge_payload_bytes_ +=
        value.observation_signature.capacity() * sizeof(std::uint64_t);
    retained_reforge_distribution_bytes_ +=
        distribution_selected_bytes(*value.distribution);
    retain_distribution_payload(value.distribution);
}

bool CalcContext::can_retain_reforge_distribution(
    const OutcomeDistribution& value) const {
    constexpr std::uint64_t kMaxRetainedRawReforgeBytes = 268435456;
    const std::uint64_t bytes = distribution_selected_bytes(value);
    return bytes <= kMaxRetainedRawReforgeBytes &&
           retained_reforge_distribution_bytes_ <=
               kMaxRetainedRawReforgeBytes - bytes;
}

bool calc_supports(const ActionDescriptor& action) {
    if (action.synthetic) return true;
    switch (action.params.type) {
    case ActionType::Transmute:
    case ActionType::Augment:
    case ActionType::Alteration:
    case ActionType::Regal:
    case ActionType::Alchemy:
    case ActionType::Chaos:
    case ActionType::Exalt:
    case ActionType::Annul:
    case ActionType::Scour:
    case ActionType::RemoveCraftedModifiers:
    case ActionType::Essence:
    case ActionType::Fossil:
    case ActionType::Bench:
    case ActionType::HarvestReforge:
    case ActionType::HarvestAugment:
    case ActionType::InfluenceExalt:
    case ActionType::VeiledChaos:
    case ActionType::VeiledExalt:
    case ActionType::Unveil:
    case ActionType::EldritchEmber:
    case ActionType::EldritchIchor:
    case ActionType::EldritchExalt:
    case ActionType::EldritchChaos:
    case ActionType::EldritchAnnul:
    case ActionType::HarvestResist:
    case ActionType::Fracture:
        return true;
    default:
        return false;
    }
}

bool CalcContext::is_goal_state(const AbstractState& state) const {
    if (state.rarity != goal_.rarity) return false;
    std::size_t satisfied = 0;
    for (std::size_t i = 0; i < layout_.slots.size(); ++i) {
        if (state.slot_status[i] ==
            static_cast<std::uint8_t>(GoalSlotStatus::Satisfied)) {
            ++satisfied;
        }
    }
    return satisfied >= goal_.required_satisfied_slots();
}

std::uint32_t CalcContext::intern_state(const AbstractState& state) {
    const std::size_t hash = abstract_state_hash(state);
    const auto found = state_ids_by_hash_.find(hash);
    if (found != state_ids_by_hash_.end()) {
        if (states_[found->second.first] == state) {
            return found->second.first;
        }
        for (std::uint32_t id : found->second.collisions) {
            if (states_[id] == state) return id;
        }
    }
    if (state_cap_.has_value() && states_.size() >= *state_cap_) {
        throw std::length_error(
            "calculation context exceeded max_states (" +
            std::to_string(*state_cap_) + ")");
    }
    if (solve_discovered_state_cap_.has_value() &&
        states_.size() >= *solve_discovered_state_cap_) {
        throw SolverResourceLimit(
            "max_discovered_states", *solve_discovered_state_cap_);
    }
    if (states_.size() >= std::numeric_limits<std::uint32_t>::max()) {
        throw std::length_error("calculation context state id space exhausted");
    }
    const std::uint32_t id = static_cast<std::uint32_t>(states_.size());
    states_.push_back(state);
    auto [bucket, inserted] = state_ids_by_hash_.try_emplace(hash);
    if (inserted) {
        bucket->second.first = id;
    } else {
        const std::size_t old_capacity =
            bucket->second.collisions.capacity();
        bucket->second.collisions.push_back(id);
        owned_state_hash_collision_bytes_ +=
            (bucket->second.collisions.capacity() - old_capacity) *
            sizeof(std::uint32_t);
    }
    return id;
}

const AbstractState& CalcContext::state(std::uint32_t state_id) const {
    return states_.at(state_id);
}

std::uint32_t CalcContext::state_count() const {
    return static_cast<std::uint32_t>(states_.size());
}

std::uint32_t CalcContext::intern_item(const pc_item_state& item) {
    return intern_state(project_item(*session_, layout_, item));
}

bool CalcContext::materialize(
    std::uint32_t state_id,
    pc_item_state& out) const {
    const SessionImpl& session = *session_;
    const AbstractState& target = states_.at(state_id);
    pc_item_clear(&out);
    out.rarity = target.rarity;
    if (target.flags & kFlagCorrupted) out.item_flags |= PC_ITEM_CORRUPTED;
    if (target.flags & kFlagMirrored) out.item_flags |= PC_ITEM_MIRRORED;
    if (target.flags & kFlagSplit) out.item_flags |= PC_ITEM_SPLIT;
    if (target.flags & kFlagSynthesised) {
        out.item_flags |= PC_ITEM_SYNTHESISED;
    }
    out.generic_influence_bits = target.influence_bits;
    out.searing_exarch_tier = target.searing_exarch_tier;
    out.eater_of_worlds_tier = target.eater_of_worlds_tier;

    /* Metamod abstract flags must come from actual metamod mods so the
     * engine's own side-lock/blocking checks see them on the item. */
    const DataImpl& data = *session.data;
    std::vector<int> needed_codes;
    const auto need = [&](std::uint32_t flag, int code) {
        if ((target.flags & flag) == 0) return true;
        if (code < 0) return false;
        needed_codes.push_back(code);
        return true;
    };
    if (!need(kFlagMultimod, data.metamod_multimod_code) ||
        !need(kFlagNoAttack, data.metamod_no_attack_code) ||
        !need(kFlagNoCaster, data.metamod_no_caster_code) ||
        !need(kFlagPrefixesLocked, data.metamod_prefixes_locked_code) ||
        !need(kFlagSuffixesLocked, data.metamod_suffixes_locked_code)) {
        return false;
    }

    std::vector<std::uint32_t> occupied_groups;
    std::vector<std::uint32_t> used_mods;
    std::vector<std::uint32_t> scratch_groups;
    const auto try_add = [&](std::uint32_t mod, std::uint8_t flags) {
        if (std::find(used_mods.begin(), used_mods.end(), mod) !=
            used_mods.end()) {
            return false;
        }
        scratch_groups.clear();
        mod_group_list(session, mod, scratch_groups);
        for (std::uint32_t group : scratch_groups) {
            if (std::find(occupied_groups.begin(), occupied_groups.end(),
                          group) != occupied_groups.end()) {
                return false;
            }
        }
        const std::int8_t gen = session.gen_type[mod];
        if (gen != 0 && gen != 1) return false;
        if (pc_item_add_mod(
                &out, gen == 0 ? PC_SIDE_PREFIX : PC_SIDE_SUFFIX, mod,
                static_cast<std::uint16_t>(session.primary_group[mod]), flags,
                nullptr) != PC_RESULT_OK) {
            return false;
        }
        occupied_groups.insert(occupied_groups.end(), scratch_groups.begin(),
                               scratch_groups.end());
        used_mods.push_back(mod);
        return true;
    };
    const auto first_from_mask =
        [&](const std::vector<std::uint64_t>& include,
            const std::vector<std::uint64_t>* exclude,
            std::uint8_t flags) {
            bool added = false;
            pc_bitset_for_each(
                include.data(), session.words, [&](std::size_t bit) {
                    if (added) return;
                    const std::uint32_t mod =
                        static_cast<std::uint32_t>(bit);
                    if (exclude != nullptr &&
                        pc_bitset_test(exclude->data(), bit)) {
                        return;
                    }
                    added = try_add(mod, flags);
                });
            return added;
        };

    for (std::size_t i = 0; i < layout_.slots.size(); ++i) {
        const ResolvedGoalSlot& slot = layout_.slots[i];
        const auto status =
            static_cast<GoalSlotStatus>(target.slot_status[i]);
        if (status == GoalSlotStatus::Absent) continue;
        const bool satisfied = status == GoalSlotStatus::Satisfied;
        std::uint8_t slot_flags = 0;
        if ((target.fractured_goal_mask & (1u << i)) != 0) {
            slot_flags |= PC_MOD_SLOT_FRACTURED;
        }
        if ((target.crafted_goal_mask & (1u << i)) != 0) {
            slot_flags |= PC_MOD_SLOT_CRAFTED;
        }
        if (!first_from_mask(
                satisfied ? slot.satisfying_mask : slot.member_mask,
                satisfied ? nullptr : &slot.satisfying_mask, slot_flags)) {
            return false;
        }
        const pc_mod_slot& added = session.gen_type[used_mods.back()] == 0
                                       ? out.prefixes[out.prefix_count - 1]
                                       : out.suffixes[out.suffix_count - 1];
        const std::int32_t metamod = session.metamod_type[added.mod_id];
        if (metamod >= 0) {
            const auto found = std::find(
                needed_codes.begin(), needed_codes.end(), metamod);
            if (found != needed_codes.end()) needed_codes.erase(found);
        }
    }

    for (std::size_t c = 0; c < layout_.junk_classes.size(); ++c) {
        const std::uint32_t total =
            c < target.junk_counts.size() ? target.junk_counts[c] : 0;
        if (total == 0) continue;
        const JunkClass& junk = layout_.junk_classes[c];
        const std::uint32_t fractured =
            c < target.fractured_junk_counts.size()
                ? target.fractured_junk_counts[c]
                : 0;
        const std::uint32_t crafted =
            c < target.crafted_junk_counts.size()
                ? target.crafted_junk_counts[c]
                : 0;
        const std::uint32_t both =
            c < target.fractured_crafted_junk_counts.size()
                ? target.fractured_crafted_junk_counts[c]
                : 0;
        if (both > fractured || both > crafted ||
            fractured + crafted - both > total) {
            return false;
        }
        std::vector<std::uint8_t> desired_flags;
        desired_flags.insert(
            desired_flags.end(), both,
            PC_MOD_SLOT_FRACTURED | PC_MOD_SLOT_CRAFTED);
        desired_flags.insert(
            desired_flags.end(), fractured - both, PC_MOD_SLOT_FRACTURED);
        desired_flags.insert(
            desired_flags.end(), crafted - both, PC_MOD_SLOT_CRAFTED);
        desired_flags.insert(
            desired_flags.end(), total - (fractured + crafted - both), 0);
        /* Prefer members that satisfy a still-needed metamod flag. */
        for (auto it = needed_codes.begin();
             !desired_flags.empty() && it != needed_codes.end();) {
            bool found = false;
            std::size_t flag_index = desired_flags.size();
            for (std::size_t i = 0; i < desired_flags.size(); ++i) {
                if ((desired_flags[i] & PC_MOD_SLOT_CRAFTED) != 0) {
                    flag_index = i;
                    break;
                }
            }
            if (flag_index == desired_flags.size()) {
                ++it;
                continue;
            }
            pc_bitset_for_each(
                junk.member_mask.data(), session.words,
                [&](std::size_t bit) {
                    if (found) return;
                    const std::uint32_t mod =
                        static_cast<std::uint32_t>(bit);
                    if (session.metamod_type[mod] == *it &&
                        try_add(mod, desired_flags[flag_index])) {
                        found = true;
                    }
                });
            if (found) {
                desired_flags.erase(desired_flags.begin() + flag_index);
                it = needed_codes.erase(it);
            } else {
                ++it;
            }
        }
        bool exhausted = false;
        pc_bitset_for_each(
            junk.member_mask.data(), session.words, [&](std::size_t bit) {
                if (desired_flags.empty() || exhausted) return;
                if (try_add(static_cast<std::uint32_t>(bit),
                            desired_flags.front())) {
                    desired_flags.erase(desired_flags.begin());
                }
            });
        if (!desired_flags.empty()) return false;
    }
    /* A metamod can exist on the queried item even when the candidate action
     * set cannot create bench modifiers, in which case it has no junk class.
     * Materialize any still-needed protection flags directly from their
     * native metamod descriptors. The affix is an ordinary crafted modifier;
     * its fractured identity is tracked separately from its protection. */
    for (auto it = needed_codes.begin(); it != needed_codes.end();) {
        std::uint32_t abstract_flag = 0;
        if (*it == data.metamod_multimod_code) {
            abstract_flag = kFlagMultimod;
        } else if (*it == data.metamod_no_attack_code) {
            abstract_flag = kFlagNoAttack;
        } else if (*it == data.metamod_no_caster_code) {
            abstract_flag = kFlagNoCaster;
        } else if (*it == data.metamod_prefixes_locked_code) {
            abstract_flag = kFlagPrefixesLocked;
        } else if (*it == data.metamod_suffixes_locked_code) {
            abstract_flag = kFlagSuffixesLocked;
        }
        std::uint8_t slot_flags = PC_MOD_SLOT_CRAFTED;
        if ((target.fractured_metamod_flags & abstract_flag) != 0) {
            slot_flags |= PC_MOD_SLOT_FRACTURED;
        }
        bool added = false;
        for (const std::uint32_t mod : session.bench_mod_ids) {
            if (mod < session.metamod_type.size() &&
                session.metamod_type[mod] == *it &&
                try_add(mod, slot_flags)) {
                added = true;
                break;
            }
        }
        if (!added) return false;
        it = needed_codes.erase(it);
    }
    if (!needed_codes.empty()) return false;
    if (out.prefix_count != target.prefix_count ||
        out.suffix_count != target.suffix_count) {
        return false;
    }

    if (target.flags & kFlagVeiledMod) {
        pc_mod_slot* slots = target.veiled_side == PC_SIDE_SUFFIX
                                 ? out.suffixes
                                 : out.prefixes;
        const std::uint8_t count = target.veiled_side == PC_SIDE_SUFFIX
                                       ? out.suffix_count
                                       : out.prefix_count;
        if (count == 0) return false;
        const std::uint32_t expected =
            target.veiled_side == PC_SIDE_SUFFIX
                ? session.veiled_suffix_mod_id
                : session.veiled_prefix_mod_id;
        pc_mod_slot* carrier = nullptr;
        for (std::uint8_t i = 0; i < count; ++i) {
            if (slots[i].mod_id == expected) {
                carrier = &slots[i];
                break;
            }
        }
        if (carrier == nullptr) return false;
        carrier->flags |= PC_MOD_SLOT_VEILED;
    }

    return project_item(session, layout_, out) == target;
}

const OutcomeDistribution& CalcContext::outcomes(
    std::uint32_t state_id,
    std::uint32_t action_index) {
    const std::uint64_t key =
        (static_cast<std::uint64_t>(state_id) << 32) | action_index;
    ++telemetry_.distribution_requests;
    PrimitiveFamilyTelemetry& family =
        telemetry_.primitive_families.at(static_cast<std::size_t>(
            primitive_family(registry_.actions.at(action_index).params.type)));
    ++family.requests;
    const auto cached = distribution_cache_.find(key);
    const OutcomeDistribution* result = nullptr;
    if (cached != distribution_cache_.end()) {
        ++telemetry_.distribution_hits;
        ++family.cache_hits;
        result = cached->second.get();
    } else {
        ++telemetry_.distribution_misses;
        const auto started = std::chrono::steady_clock::now();
        std::shared_ptr<const OutcomeDistribution> distribution =
            evaluate(state_id, action_index);
        const auto build_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started)
                .count());
        telemetry_.distribution_build_ns += build_ns;
        family.build_ns += build_ns;
        account_distribution_cache_insert(key, distribution);
        result = distribution_cache_.emplace(key, std::move(distribution))
                     .first->second.get();
    }

    if (telemetry_rows_.emplace(key, 1).second) {
        ++telemetry_.state_action_rows;
        ++family.rows;
        telemetry_.outcome_entries += result->entries.size();
        family.raw_outcomes += result->entries.size();
        telemetry_.choice_groups += result->choice_groups.size();
        std::uint64_t choice_successors = 0;
        for (const OutcomeChoiceGroup& group : result->choice_groups) {
            choice_successors += group.states.size();
        }
        telemetry_.choice_successor_entries += choice_successors;
        telemetry_.transition_entries +=
            result->choice_groups.empty()
                ? result->entries.size()
                : choice_successors;
        family.transitions += result->choice_groups.empty()
                                  ? result->entries.size()
                                  : choice_successors;
        std::uint64_t selected_bytes = sizeof(OutcomeDistribution);
        selected_bytes += result->entries.capacity() * sizeof(OutcomeEntry);
        selected_bytes += result->choice_groups.capacity() *
                          sizeof(OutcomeChoiceGroup);
        for (const OutcomeChoiceGroup& group : result->choice_groups) {
            selected_bytes +=
                group.states.capacity() * sizeof(std::uint32_t);
        }
        selected_bytes += result->choice_options.capacity() *
                          sizeof(OutcomeChoiceOption);
        family.selected_bytes += selected_bytes;
    }
    return *result;
}

void CalcContext::reset_solve_telemetry() {
    telemetry_ = {};
    telemetry_rows_.clear();
    /* State-local automatic admission is price-scoped. Retained operator and
     * kernel templates remain exact reusable structure, but the per-state
     * decision and any graph built from it must be recomputed for each solve. */
    state_local_automatic_operators_.clear();
    state_local_automatic_operators_.rehash(0);
    owned_state_local_operator_bytes_ = 0;
    if (goal_.automatic_candidates) {
        solve_transition_cache_.reset();
    }
    if (automatic_comparison_context_ != nullptr) {
        automatic_comparison_context_->reset_solve_telemetry();
    }
    for (auto& [unused_key, context] : automatic_admission_contexts_) {
        (void)unused_key;
        context->reset_solve_telemetry();
    }
}

void CalcContext::set_solve_resource_caps(
    const std::uint32_t max_discovered_states,
    const std::uint64_t max_reforge_work,
    const bool reserve_storage) {
    solve_discovered_state_cap_ = max_discovered_states;
    solve_reforge_work_cap_ = max_reforge_work;
    if (!reserve_storage) return;
    const std::size_t practical_reserve = std::min<std::size_t>(
        max_discovered_states, 65536);
    states_.reserve(std::max(states_.size(), practical_reserve));
    state_ids_by_hash_.reserve(practical_reserve);
}

void CalcContext::consume_reforge_work(const std::uint64_t amount) {
    if (solve_reforge_work_cap_.has_value() &&
        amount > *solve_reforge_work_cap_ -
                     std::min(telemetry_.reforge_frontier_work,
                              *solve_reforge_work_cap_)) {
        telemetry_.reforge_frontier_work = *solve_reforge_work_cap_;
        throw SolverResourceLimit(
            "max_reforge_work", *solve_reforge_work_cap_);
    }
    telemetry_.reforge_frontier_work += amount;
}

void CalcContext::record_primitive_row_time(
    const std::uint32_t action_index,
    const std::uint64_t elapsed_ns) {
    PrimitiveFamilyTelemetry& family =
        telemetry_.primitive_families.at(static_cast<std::size_t>(
            primitive_family(registry_.actions.at(action_index).params.type)));
    family.row_ns += elapsed_ns;
}

void CalcContext::release_solve_transition_caches() {
    distribution_cache_.clear();
    owned_distribution_payload_bytes_ = 0;
    owned_distribution_payload_refs_.clear();
    for (auto it = option_kernel_cache_.begin();
         it != option_kernel_cache_.end();) {
        const std::uint32_t operator_index =
            static_cast<std::uint32_t>(it->first);
        if (is_state_local_automatic_operator(operator_index)) {
            ++it;
        } else {
            if (!it->second->retained_template_storage) {
                owned_option_cache_payload_bytes_ -=
                    option_kernel_selected_bytes_for_ledger(*it->second);
            }
            it = option_kernel_cache_.erase(it);
        }
    }
    reforge_cache_.clear();
    owned_reforge_payload_bytes_ = 0;
    retained_reforge_distribution_bytes_ = 0;
    if (automatic_comparison_context_ != nullptr) {
        automatic_comparison_context_->release_solve_transition_caches();
    }
    for (auto& [unused_key, context] : automatic_admission_contexts_) {
        (void)unused_key;
        context->release_solve_transition_caches();
    }
    telemetry_rows_.clear();
}

void CalcContext::release_outcome(
    const std::uint32_t state_id,
    const std::uint32_t action_index) {
    const std::uint64_t key =
        (static_cast<std::uint64_t>(state_id) << 32) | action_index;
    account_distribution_cache_erase(key);
    distribution_cache_.erase(key);
}

void CalcContext::release_option_kernel(
    const std::uint32_t state_id,
    const std::uint32_t operator_index) {
    const std::uint64_t key =
        (static_cast<std::uint64_t>(state_id) << 32) | operator_index;
    if (is_state_local_automatic_operator(operator_index)) return;
    account_option_cache_erase(key);
    option_kernel_cache_.erase(key);
}

std::uint64_t CalcContext::calculate_owned_bytes() const {
    /* This deliberately reports a conservative selected-allocation estimate.
     * Standard-library node overhead, allocator metadata, and pool-cache
     * storage inside ActionContextImpl are not portable enough to claim as an
     * exact byte count. Process/WASM heap measurements complement this value. */
    std::uint64_t bytes = sizeof(*this);
    const auto string_bytes = [](const std::string& value) {
        return static_cast<std::uint64_t>(value.capacity() + 1);
    };
    bytes += registry_.actions.capacity() * sizeof(ActionDescriptor);
    for (const ActionDescriptor& action : registry_.actions) {
        bytes += string_bytes(action.id) + string_bytes(action.display_name);
        bytes += action.cost_keys.capacity() * sizeof(std::string);
        for (const std::string& key : action.cost_keys) {
            bytes += string_bytes(key);
        }
        bytes += action.discriminating_tag_ids.capacity() *
                 sizeof(std::uint32_t);
        bytes += action.params.fossil_indices.capacity() *
                 sizeof(std::uint32_t);
    }
    bytes += registry_.index_by_id.bucket_count() * sizeof(void*);
    bytes += registry_.index_by_id.size() *
             (sizeof(std::pair<const std::string, std::uint32_t>) +
              2 * sizeof(void*));
    for (const auto& [key, unused] : registry_.index_by_id) {
        (void)unused;
        bytes += string_bytes(key);
    }
    bytes += candidates_.capacity() * sizeof(std::uint32_t);
    bytes += candidate_operators_.capacity() * sizeof(std::uint32_t);
    bytes += automatic_goal_bench_actions_.capacity() *
             sizeof(std::uint32_t);
    bytes += temporary_bench_effect_classes_.capacity() *
             sizeof(TemporaryBenchEffectClass);
    for (const TemporaryBenchEffectClass& effect :
         temporary_bench_effect_classes_) {
        bytes += effect.conflict_mask.capacity() * sizeof(std::uint64_t);
        bytes += effect.target_mask.capacity() * sizeof(std::uint64_t);
        bytes += effect.blocker_actions.capacity() * sizeof(std::uint32_t);
    }
    bytes += state_local_automatic_operators_.bucket_count() * sizeof(void*);
    bytes += state_local_automatic_operators_.size() *
             (sizeof(std::pair<const std::uint32_t,
                               std::vector<std::uint32_t>>) +
              2 * sizeof(void*));
    for (const auto& [unused, indices] :
         state_local_automatic_operators_) {
        (void)unused;
        bytes += indices.capacity() * sizeof(std::uint32_t);
    }
    bytes += admitted_automatic_dependencies_.bucket_count() * sizeof(void*);
    bytes += admitted_automatic_dependencies_.size() *
             (sizeof(std::uint32_t) + 2 * sizeof(void*));
    bytes += state_local_automatic_operator_indices_.bucket_count() *
             sizeof(void*);
    bytes += state_local_automatic_operator_indices_.size() *
             (sizeof(std::uint32_t) + 2 * sizeof(void*));
    bytes += operators_.capacity() * sizeof(PlannerOperator);
    for (const PlannerOperator& op : operators_) {
        bytes += string_bytes(op.id) + string_bytes(op.display_name);
        bytes += op.primitive_program.capacity() * sizeof(std::uint32_t);
        bytes += op.exit_goal_slots.capacity() * sizeof(std::uint32_t);
        bytes += op.resource_quantities.capacity() *
                 sizeof(std::pair<std::string, double>);
        for (const auto& [key, quantity] : op.resource_quantities) {
            (void)quantity;
            bytes += string_bytes(key);
        }
    }
    bytes += layout_.slots.capacity() * sizeof(ResolvedGoalSlot);
    for (const ResolvedGoalSlot& slot : layout_.slots) {
        bytes += slot.member_mask.capacity() * sizeof(std::uint64_t);
        bytes += slot.satisfying_mask.capacity() * sizeof(std::uint64_t);
        bytes += slot.blocking_group_ids.capacity() * sizeof(std::uint32_t);
    }
    bytes += layout_.discriminating_tag_ids.capacity() *
             sizeof(std::uint32_t);
    bytes += layout_.count_observations.capacity() *
             sizeof(CountObservation);
    for (const CountObservation& observation : layout_.count_observations) {
        bytes += observation.ids.capacity() * sizeof(std::uint32_t);
        bytes += observation.memo_slots.capacity() * sizeof(std::uint32_t);
        bytes += observation.member_mask.capacity() * sizeof(std::uint64_t);
        bytes += observation.junk_class_indices.capacity() *
                 sizeof(std::uint32_t);
    }
    bytes += layout_.count_observation_by_memo_slot.capacity() *
             sizeof(std::uint32_t);
    bytes += layout_.junk_classes.capacity() * sizeof(JunkClass);
    for (const JunkClass& junk : layout_.junk_classes) {
        bytes += junk.exclusion_effect_mask.capacity() * sizeof(std::uint64_t);
        bytes += junk.count_observation_bits.capacity() *
                 sizeof(std::uint64_t);
        bytes += junk.member_mask.capacity() * sizeof(std::uint64_t);
    }
    bytes += layout_.junk_class_by_mod.capacity() * sizeof(std::uint32_t);
    bytes += states_.capacity() * sizeof(AbstractState);
    bytes += state_ids_by_hash_.bucket_count() * sizeof(void*);
    bytes += state_ids_by_hash_.size() *
             (sizeof(std::pair<const std::size_t, StateHashBucket>) +
              2 * sizeof(void*));
    for (const auto& [unused, bucket] : state_ids_by_hash_) {
        (void)unused;
        bytes += bucket.collisions.capacity() * sizeof(std::uint32_t);
    }
    bytes += distribution_cache_.bucket_count() * sizeof(void*);
    bytes += distribution_cache_.size() *
             (sizeof(std::pair<
                  const std::uint64_t,
                  std::shared_ptr<const OutcomeDistribution>>) +
                  2 * sizeof(void*));
    bytes += owned_distribution_payload_refs_.bucket_count() * sizeof(void*);
    bytes += owned_distribution_payload_refs_.size() *
             (sizeof(std::pair<const OutcomeDistribution* const,
                               std::uint32_t>) +
              2 * sizeof(void*));
    std::unordered_set<const OutcomeDistribution*> counted_distributions;
    const auto distribution_bytes = [](const OutcomeDistribution& value) {
        std::uint64_t total = sizeof(OutcomeDistribution);
        total += value.entries.capacity() * sizeof(OutcomeEntry);
        total += value.choice_groups.capacity() * sizeof(OutcomeChoiceGroup);
        for (const OutcomeChoiceGroup& group : value.choice_groups) {
            total += group.states.capacity() * sizeof(std::uint32_t);
        }
        total += value.choice_options.capacity() *
                 sizeof(OutcomeChoiceOption);
        return total;
    };
    for (const auto& [unused, distribution] : distribution_cache_) {
        (void)unused;
        if (counted_distributions.insert(distribution.get()).second) {
            bytes += distribution_bytes(*distribution);
        }
    }
    bytes += option_kernel_cache_.bucket_count() * sizeof(void*);
    bytes += option_kernel_cache_.size() *
             (sizeof(std::pair<
                  const std::uint64_t,
                  std::shared_ptr<const OptionKernel>>) +
              2 * sizeof(void*));
    std::unordered_set<const OptionKernel*> counted_option_kernels;
    const auto add_option_kernel_bytes = [&](const OptionKernel& kernel) {
        bytes += sizeof(OptionKernel);
        bytes += kernel.expected_resources.capacity() *
                 sizeof(std::pair<std::string, double>);
        for (const auto& [key, quantity] : kernel.expected_resources) {
            (void)quantity;
            bytes += string_bytes(key);
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
        bytes += kernel.continuation_states.capacity() *
                 sizeof(std::uint32_t);
        bytes += string_bytes(kernel.automatic.legality_result);
        bytes += string_bytes(kernel.automatic.reason);
    };
    for (const auto& [unused, kernel] : option_kernel_cache_) {
        (void)unused;
        if (counted_option_kernels.insert(kernel.get()).second) {
            add_option_kernel_bytes(*kernel);
        }
    }
    bytes += option_kernel_templates_.bucket_count() * sizeof(void*);
    bytes += option_kernel_templates_.size() *
             (sizeof(std::pair<
                  const std::uint64_t,
                  std::vector<OptionKernelTemplateMemo>>) +
              2 * sizeof(void*));
    for (const auto& [unused, templates] : option_kernel_templates_) {
        (void)unused;
        bytes += templates.capacity() * sizeof(OptionKernelTemplateMemo);
        for (const OptionKernelTemplateMemo& memo : templates) {
            bytes += memo.expected_resources.capacity() *
                     sizeof(std::pair<std::string, double>);
            for (const auto& [key, unused_quantity] :
                 memo.expected_resources) {
                (void)unused_quantity;
                bytes += string_bytes(key);
            }
            if (counted_option_kernels.insert(memo.kernel.get()).second) {
                add_option_kernel_bytes(*memo.kernel);
            }
        }
    }
    bytes += option_transition_templates_.bucket_count() * sizeof(void*);
    bytes += option_transition_templates_.size() *
             (sizeof(std::pair<
                  const std::uint64_t,
                  std::vector<std::shared_ptr<const OptionKernel>>>) +
              2 * sizeof(void*));
    for (const auto& [unused, templates] : option_transition_templates_) {
        (void)unused;
        bytes += templates.capacity() *
                 sizeof(std::shared_ptr<const OptionKernel>);
        for (const auto& kernel : templates) {
            if (counted_option_kernels.insert(kernel.get()).second) {
                add_option_kernel_bytes(*kernel);
            }
        }
    }
    bytes += option_operator_templates_.bucket_count() * sizeof(void*);
    bytes += option_operator_templates_.size() *
             (sizeof(std::pair<
                  const std::uint64_t,
                  std::vector<std::uint32_t>>) +
              2 * sizeof(void*));
    for (const auto& [unused, operators] : option_operator_templates_) {
        (void)unused;
        bytes += operators.capacity() * sizeof(std::uint32_t);
    }
    bytes += option_kernel_template_hit_keys_.bucket_count() * sizeof(void*);
    bytes += option_kernel_template_hit_keys_.size() *
             (sizeof(std::uint64_t) + 2 * sizeof(void*));
    bytes += reforge_cache_.size() *
             (sizeof(std::pair<
                   const std::pair<std::uint32_t, std::uint64_t>,
                   std::vector<ReforgeCacheMemo>>) +
               3 * sizeof(void*));
    for (const auto& [unused, memos] : reforge_cache_) {
        (void)unused;
        bytes += memos.capacity() * sizeof(ReforgeCacheMemo);
        for (const ReforgeCacheMemo& memo : memos) {
            bytes += memo.observation_signature.capacity() *
                     sizeof(std::uint64_t);
            if (counted_distributions.insert(memo.distribution.get()).second) {
                bytes += distribution_bytes(*memo.distribution);
            }
        }
    }
    bytes += telemetry_rows_.bucket_count() * sizeof(void*);
    bytes += telemetry_rows_.size() *
             (sizeof(std::pair<const std::uint64_t, std::uint8_t>) +
              2 * sizeof(void*));
    if (automatic_comparison_context_ != nullptr) {
        bytes += automatic_comparison_context_->calculate_owned_bytes();
    }
    bytes += automatic_admission_contexts_.bucket_count() * sizeof(void*);
    bytes += automatic_admission_contexts_.size() *
             (sizeof(std::pair<const std::string,
                               std::unique_ptr<CalcContext>>) +
              2 * sizeof(void*));
    for (const auto& [key, context] : automatic_admission_contexts_) {
        bytes += key.capacity() + 1;
        bytes += context->calculate_owned_bytes();
    }
    return bytes;
}

std::uint64_t CalcContext::estimated_owned_bytes() const {
    return calculate_owned_bytes();
}

std::uint64_t CalcContext::audited_estimated_owned_bytes() const {
    const auto audit_started = std::chrono::steady_clock::now();
    const std::uint64_t bytes = calculate_owned_bytes();
    ++telemetry_.owned_byte_audit_requests;
    telemetry_.owned_byte_audit_ns += static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - audit_started)
            .count());
    if (owned_bytes_ledger_initialized_) {
        const std::uint64_t fast = fast_estimated_owned_bytes();
        ++telemetry_.owned_byte_reconciliations;
        if (fast < bytes) {
            throw std::logic_error(
                "incremental selected-owned-byte ledger undercounted by " +
                std::to_string(bytes - fast) +
                " (full=" + std::to_string(bytes) +
                ", fast=" + std::to_string(fast) +
                ", base=" + std::to_string(owned_bytes_base_) +
                ", shallow=" +
                std::to_string(dynamic_shallow_owned_bytes()) +
                ", shallow_base=" +
                std::to_string(owned_bytes_dynamic_shallow_base_) +
                ", state_collision=" +
                std::to_string(owned_state_hash_collision_bytes_) +
                ", state_local=" +
                std::to_string(owned_state_local_operator_bytes_) +
                ", operators=" +
                std::to_string(owned_added_operator_nested_bytes_) +
                ", distributions=" +
                std::to_string(owned_distribution_payload_bytes_) +
                ", option_cache=" +
                std::to_string(owned_option_cache_payload_bytes_) +
                ", option_templates=" +
                std::to_string(owned_option_template_nested_bytes_) +
                ", transition_templates=" +
                std::to_string(owned_transition_template_nested_bytes_) +
                ", operator_templates=" +
                std::to_string(owned_operator_template_nested_bytes_) +
                ", reforge=" +
                std::to_string(owned_reforge_payload_bytes_) + ")");
        }
        telemetry_.owned_byte_ledger_max_overestimate = std::max(
            telemetry_.owned_byte_ledger_max_overestimate, fast - bytes);
    }
    return bytes;
}

std::shared_ptr<const OutcomeDistribution> CalcContext::evaluate(
    std::uint32_t state_id,
    std::uint32_t action_index) {
    const ActionDescriptor& action = registry_.actions.at(action_index);
    const SessionImpl& session = *session_;
    OutcomeDistribution result;

    std::map<std::uint32_t, double> accumulated;
    const auto self_loop = [&]() { accumulated[state_id] += 1.0; };
    const auto add_successor = [&](const pc_item_state& item, double p) {
        accumulated[intern_item(item)] += p;
    };

    /* Illegal actions leave the item unchanged (engine semantics). */
    if (!action_legal(session, action, states_.at(state_id))) {
        result.supported = true;
        self_loop();
    } else if (action.synthetic) {
        /* restart: a fresh base with probability 1. */
        pc_item_state fresh;
        pc_item_clear(&fresh);
        result.supported = true;
        add_successor(fresh, 1.0);
    } else if (action.params.type == ActionType::Transmute ||
               action.params.type == ActionType::Alteration ||
               action.params.type == ActionType::Alchemy ||
               action.params.type == ActionType::Chaos ||
               action.params.type == ActionType::Essence ||
               action.params.type == ActionType::Fossil ||
               action.params.type == ActionType::HarvestReforge ||
               action.params.type == ActionType::VeiledChaos ||
               action.params.type == ActionType::EldritchChaos) {
        /* Sequential multi-mod rolls: the S3 roll DP (harvest adds a
         * guaranteed tag-targeted first pick). */
        return evaluate_reforge(state_id, action_index);
    } else if (action.params.type == ActionType::Unveil) {
        return evaluate_unveil(state_id);
    } else {
        pc_item_state item;
        if (!materialize(state_id, item)) {
            return std::make_shared<OutcomeDistribution>(std::move(result));
        }
        switch (action.params.type) {
        case ActionType::Scour:
        case ActionType::RemoveCraftedModifiers:
        case ActionType::Bench: {
            /* RNG-free engine actions: apply and project. */
            pc_item_state copy = item;
            apply_action(context_, &copy, action.params);
            result.supported = true;
            add_successor(copy, 1.0);
            break;
        }
        case ActionType::Augment:
        case ActionType::Exalt: {
            result.supported = true;
            if (!evaluate_pool_add(item, PoolBuildRequest{}, accumulated)) {
                self_loop();
            }
            break;
        }
        case ActionType::Regal: {
            /* Magic -> rare always applies; the added mod comes from the
             * pool of the upgraded item. An empty pool still upgrades. */
            pc_item_state upgraded = item;
            upgraded.rarity = PC_RARITY_RARE;
            result.supported = true;
            if (!evaluate_pool_add(
                    upgraded, PoolBuildRequest{}, accumulated)) {
                add_successor(upgraded, 1.0);
            }
            break;
        }
        case ActionType::InfluenceExalt: {
            pc_item_state influenced = item;
            const std::uint8_t bit = static_cast<std::uint8_t>(
                1u << (action.params.influence_code - 1));
            influenced.generic_influence_bits |= bit;
            PoolBuildRequest request;
            request.influence_only_code = action.params.influence_code;
            result.supported = true;
            /* The engine reverts the influence bit when no mod can be
             * added, leaving the item unchanged. */
            if (!evaluate_pool_add(influenced, request, accumulated)) {
                self_loop();
            }
            break;
        }
        case ActionType::VeiledExalt: {
            result.supported = true;
            const std::uint8_t cap = rarity_affix_cap(session, item.rarity);
            const bool prefix_open = item.prefix_count < cap;
            const bool suffix_open = item.suffix_count < cap;
            const double side_probability =
                prefix_open && suffix_open ? 0.5 : 1.0;
            const auto add_side = [&](int side, bool open) {
                if (!open) return;
                const std::uint32_t mod_id =
                    side == PC_SIDE_PREFIX ? session.veiled_prefix_mod_id
                                           : session.veiled_suffix_mod_id;
                pc_item_state copy = item;
                if (mod_id == kNoId ||
                    pc_item_add_mod(
                        &copy, side, mod_id,
                        static_cast<std::uint16_t>(
                            session.primary_group[mod_id]),
                        PC_MOD_SLOT_VEILED, nullptr) != PC_RESULT_OK) {
                    accumulated[state_id] += side_probability;
                    return;
                }
                add_successor(copy, side_probability);
            };
            add_side(PC_SIDE_PREFIX, prefix_open);
            add_side(PC_SIDE_SUFFIX, suffix_open);
            if (accumulated.empty()) self_loop();
            break;
        }
        case ActionType::EldritchEmber:
        case ActionType::EldritchIchor: {
            result.supported = true;
            pc_item_state copy = item;
            if (action.params.type == ActionType::EldritchEmber) {
                copy.searing_exarch_tier =
                    static_cast<std::uint8_t>(action.params.tier);
            } else {
                copy.eater_of_worlds_tier =
                    static_cast<std::uint8_t>(action.params.tier);
            }
            add_successor(copy, 1.0);
            break;
        }
        case ActionType::EldritchExalt: {
            result.supported = true;
            PoolBuildRequest request;
            request.side_filter = dominant_eldritch(item);
            if (!evaluate_pool_add(item, request, accumulated)) self_loop();
            break;
        }
        case ActionType::HarvestAugment: {
            /* Add one tag-targeted mod (spawn-weight-only pool), then
             * remove one uniform other non-fractured mod on an unlocked
             * side — the add-then-remove semantics are intentional. */
            result.supported = true;
            PoolBuildRequest request;
            request.weight_kind = PoolWeightKind::HarvestSpawnOnly;
            request.target_tag_id = action.params.target_tag_id;
            const std::uint8_t cap = rarity_affix_cap(session, item.rarity);
            const bool prefix_open = item.prefix_count < cap;
            const bool suffix_open = item.suffix_count < cap;
            if (!prefix_open && !suffix_open) {
                self_loop();
                break;
            }
            request.side_filter =
                prefix_open && suffix_open ? -1 : (prefix_open ? 0 : 1);
            const WeightedPool& pool =
                get_weighted_pool(context_, &item, request);
            if (pool.total_weight == 0) {
                self_loop();
                break;
            }
            for (const PoolEntry& entry : pool.entries) {
                if (entry.final_weight == 0) continue;
                pc_item_state added = item;
                if (pc_item_add_mod(
                        &added,
                        entry.gen_type == 0 ? PC_SIDE_PREFIX
                                            : PC_SIDE_SUFFIX,
                        entry.session_mod_id,
                        static_cast<std::uint16_t>(entry.primary_group), 0,
                        nullptr) != PC_RESULT_OK) {
                    continue;
                }
                const double p_add =
                    static_cast<double>(entry.final_weight) /
                    static_cast<double>(pool.total_weight);
                struct Ref {
                    int side;
                    std::uint32_t index;
                };
                std::vector<Ref> removable;
                const auto collect = [&](int side, const pc_mod_slot* slots,
                                         std::uint8_t count) {
                    if (item_side_locked(session, added, side)) return;
                    for (std::uint8_t i = 0; i < count; ++i) {
                        if (!(slots[i].flags & PC_MOD_SLOT_FRACTURED) &&
                            slots[i].mod_id != entry.session_mod_id) {
                            removable.push_back({side, i});
                        }
                    }
                };
                collect(PC_SIDE_PREFIX, added.prefixes, added.prefix_count);
                collect(PC_SIDE_SUFFIX, added.suffixes, added.suffix_count);
                if (removable.empty()) {
                    add_successor(added, p_add);
                    continue;
                }
                const double each =
                    p_add / static_cast<double>(removable.size());
                for (const Ref& pick : removable) {
                    pc_item_state removed = added;
                    pc_item_remove_at(&removed, pick.side, pick.index);
                    add_successor(removed, each);
                }
            }
            if (accumulated.empty()) self_loop();
            break;
        }
        case ActionType::HarvestResist: {
            result.supported = true;
            const auto resistance =
                session.data->tag_id_by_name.find("resistance");
            if (resistance == session.data->tag_id_by_name.end()) {
                self_loop();
                break;
            }
            struct SourceOutcome {
                pc_item_state removed{};
                int side = -1;
                std::vector<std::pair<std::uint32_t, std::uint64_t>> targets;
                std::uint64_t total_weight = 0;
            };
            std::vector<SourceOutcome> viable;
            const auto collect = [&](int side, const pc_mod_slot* slots,
                                     std::uint8_t count) {
                if (item_side_locked(session, item, side)) return;
                for (std::uint8_t i = 0; i < count; ++i) {
                    const pc_mod_slot& source = slots[i];
                    if ((source.flags & PC_MOD_SLOT_FRACTURED) != 0 ||
                        !has_class_tag(
                            session, source.mod_id, resistance->second) ||
                        !has_class_tag(
                            session, source.mod_id,
                            action.params.source_tag_id)) {
                        continue;
                    }
                    SourceOutcome option;
                    option.removed = item;
                    option.side = side;
                    pc_item_remove_at(&option.removed, side, i);
                    PoolBuildRequest request;
                    request.weight_kind = PoolWeightKind::HarvestSpawnOnly;
                    request.target_tag_id = action.params.target_tag_id;
                    request.side_filter = side;
                    const WeightedPool& pool = get_weighted_pool(
                        context_, &option.removed, request);
                    for (const PoolEntry& entry : pool.entries) {
                        if (entry.final_weight == 0 ||
                            entry.required_level !=
                                session.required_level[source.mod_id] ||
                            !has_class_tag(
                                session, entry.session_mod_id,
                                resistance->second) ||
                            has_class_tag(
                                session, entry.session_mod_id,
                                action.params.source_tag_id)) {
                            continue;
                        }
                        option.total_weight += entry.final_weight;
                        option.targets.push_back(
                            {entry.session_mod_id, entry.final_weight});
                    }
                    if (option.total_weight > 0) {
                        viable.push_back(std::move(option));
                    }
                }
            };
            collect(PC_SIDE_PREFIX, item.prefixes, item.prefix_count);
            collect(PC_SIDE_SUFFIX, item.suffixes, item.suffix_count);
            if (viable.empty()) {
                self_loop();
                break;
            }
            /* The engine retries invalid source carriers without replacement;
             * equivalently, the first viable carrier in that random ordering
             * is uniform over the viable carriers. */
            const double source_probability =
                1.0 / static_cast<double>(viable.size());
            for (const SourceOutcome& option : viable) {
                for (const auto& [mod_id, weight] : option.targets) {
                    pc_item_state converted = option.removed;
                    if (pc_item_add_mod(
                            &converted, option.side, mod_id,
                            static_cast<std::uint16_t>(
                                session.primary_group[mod_id]),
                            0, nullptr) != PC_RESULT_OK) {
                        continue;
                    }
                    add_successor(
                        converted,
                        source_probability *
                            static_cast<double>(weight) /
                            static_cast<double>(option.total_weight));
                }
            }
            if (accumulated.empty()) self_loop();
            break;
        }
        case ActionType::Fracture: {
            result.supported = true;
            const std::uint32_t total =
                static_cast<std::uint32_t>(
                    item.prefix_count + item.suffix_count);
            if (total < 4 || item.generic_influence_bits != 0 ||
                (item.item_flags & PC_ITEM_SYNTHESISED) != 0 ||
                (states_.at(state_id).flags & kFlagFractured) != 0) {
                self_loop();
                break;
            }
            const double each = 1.0 / static_cast<double>(total);
            for (std::uint32_t pick = 0; pick < total; ++pick) {
                pc_item_state fractured = item;
                if (pick < item.prefix_count) {
                    fractured.prefixes[pick].flags |=
                        PC_MOD_SLOT_FRACTURED;
                } else {
                    fractured.suffixes[pick - item.prefix_count].flags |=
                        PC_MOD_SLOT_FRACTURED;
                }
                add_successor(fractured, each);
            }
            break;
        }
        case ActionType::Annul:
        case ActionType::EldritchAnnul: {
            result.supported = true;
            const int eldritch_side =
                action.params.type == ActionType::EldritchAnnul
                    ? dominant_eldritch(item)
                    : -1;
            const bool prefix_locked =
                eldritch_side == PC_SIDE_SUFFIX ||
                (eldritch_side < 0 &&
                 item_side_locked(session, item, PC_SIDE_PREFIX));
            const bool suffix_locked =
                eldritch_side == PC_SIDE_PREFIX ||
                (eldritch_side < 0 &&
                 item_side_locked(session, item, PC_SIDE_SUFFIX));
            struct Ref {
                int side;
                std::uint32_t index;
            };
            std::vector<Ref> removable;
            if (!prefix_locked) {
                for (std::uint8_t i = 0; i < item.prefix_count; ++i) {
                    if (!(item.prefixes[i].flags & PC_MOD_SLOT_FRACTURED)) {
                        removable.push_back({PC_SIDE_PREFIX, i});
                    }
                }
            }
            if (!suffix_locked) {
                for (std::uint8_t i = 0; i < item.suffix_count; ++i) {
                    if (!(item.suffixes[i].flags & PC_MOD_SLOT_FRACTURED)) {
                        removable.push_back({PC_SIDE_SUFFIX, i});
                    }
                }
            }
            if (removable.empty()) {
                self_loop();
                break;
            }
            const double each = 1.0 / static_cast<double>(removable.size());
            for (const Ref& pick : removable) {
                pc_item_state copy = item;
                pc_item_remove_at(&copy, pick.side, pick.index);
                add_successor(copy, each);
            }
            break;
        }
        default:
            /* Mechanics without an exact evaluator remain unsupported. */
            return std::make_shared<OutcomeDistribution>(std::move(result));
        }
    }

    for (const auto& [successor, probability] : accumulated) {
        result.entries.push_back({successor, probability});
    }
    for (const OutcomeEntry& entry : result.entries) {
        const AbstractState& successor = states_.at(entry.state);
        for (std::size_t i = 0; i < layout_.slots.size(); ++i) {
            if (successor.slot_status[i] ==
                static_cast<std::uint8_t>(GoalSlotStatus::Satisfied)) {
                result.slot_satisfied_probability[i] += entry.probability;
            }
        }
    }
    return std::make_shared<OutcomeDistribution>(std::move(result));
}

std::shared_ptr<const OutcomeDistribution> CalcContext::evaluate_unveil(
    std::uint32_t state_id) {
    OutcomeDistribution result;
    result.supported = true;
    pc_item_state item;
    if (!materialize(state_id, item)) {
        result.supported = false;
        return std::make_shared<OutcomeDistribution>(std::move(result));
    }
    int side = -1;
    std::uint32_t index = 0;
    if (pc_item_find_veiled(&item, &side, &index) != PC_RESULT_OK) {
        result.entries.push_back({state_id, 1.0});
        return std::make_shared<OutcomeDistribution>(std::move(result));
    }

    struct Candidate {
        std::uint32_t mod_id = kNoId;
        std::uint32_t state = kNoId;
        double weight = 0.0;
    };
    std::vector<Candidate> candidates;
    const SessionImpl& session = *session_;
    pc_bitset_for_each(
        session.unveiled_generic_mask.data(), session.words,
        [&](std::size_t bit) {
            const std::uint32_t mod_id = static_cast<std::uint32_t>(bit);
            if (session.gen_type[mod_id] != side ||
                mod_groups_conflict(session, item, mod_id, side, index)) {
                return;
            }
            const std::uint32_t weight = unveil_weight(session, mod_id);
            if (weight == 0) return;
            pc_item_state copy = item;
            pc_mod_slot& slot = side == PC_SIDE_PREFIX
                                    ? copy.prefixes[index]
                                    : copy.suffixes[index];
            slot.mod_id = mod_id;
            slot.group_id = static_cast<std::uint16_t>(
                session.primary_group[mod_id]);
            slot.flags &= static_cast<std::uint8_t>(~PC_MOD_SLOT_VEILED);
            slot.veiled_chosen_mod_id = mod_id;
            slot.veiled_option_count = 0;
            candidates.push_back(
                {mod_id, intern_item(copy), static_cast<double>(weight)});
        });
    if (candidates.empty()) {
        result.entries.push_back({state_id, 1.0});
        return std::make_shared<OutcomeDistribution>(std::move(result));
    }
    std::sort(
        candidates.begin(), candidates.end(),
        [](const Candidate& a, const Candidate& b) {
            return a.mod_id < b.mod_id;
        });
    for (const Candidate& candidate : candidates) {
        result.choice_options.push_back(
            {candidate.mod_id, candidate.state});
    }

    /* The engine samples up to three weighted options without replacement.
     * Aggregate ordered draw paths by the set of abstract successors they
     * offer; Bellman backups consume these choice groups directly. */
    const std::size_t draw_count = std::min<std::size_t>(
        PC_MAX_VEILED_OPTIONS, candidates.size());
    double total_weight = 0.0;
    for (const Candidate& candidate : candidates) {
        total_weight += candidate.weight;
    }
    std::map<std::vector<std::uint32_t>, double> offered;
    std::vector<std::uint8_t> used(candidates.size(), 0);
    std::vector<std::uint32_t> picked_states;
    const std::function<void(std::size_t, double, double)> draw =
        [&](std::size_t depth, double remaining, double probability) {
            if (depth == draw_count) {
                std::vector<std::uint32_t> states = picked_states;
                std::sort(states.begin(), states.end());
                states.erase(std::unique(states.begin(), states.end()),
                             states.end());
                offered[std::move(states)] += probability;
                return;
            }
            for (std::size_t i = 0; i < candidates.size(); ++i) {
                if (used[i]) continue;
                used[i] = 1;
                picked_states.push_back(candidates[i].state);
                draw(depth + 1, remaining - candidates[i].weight,
                     probability * candidates[i].weight / remaining);
                picked_states.pop_back();
                used[i] = 0;
            }
        };
    draw(0, total_weight, 1.0);

    std::map<std::uint32_t, double> displayed;
    const auto immediate_better = [&](std::uint32_t a, std::uint32_t b) {
        const AbstractState& left = state(a);
        const AbstractState& right = state(b);
        const auto score = [&](const AbstractState& value) {
            int satisfied = 0;
            int below = 0;
            for (std::size_t i = 0; i < layout_.slots.size(); ++i) {
                satisfied += value.slot_status[i] ==
                             static_cast<std::uint8_t>(
                                 GoalSlotStatus::Satisfied);
                below += value.slot_status[i] ==
                         static_cast<std::uint8_t>(
                             GoalSlotStatus::PresentBelowTier);
            }
            return std::tuple<int, int, int>{
                is_goal_state(value) ? 1 : 0, satisfied, below};
        };
        const auto left_score = score(left);
        const auto right_score = score(right);
        return left_score != right_score ? left_score > right_score : a < b;
    };
    for (const auto& [states, probability] : offered) {
        result.choice_groups.push_back({probability, states});
        std::uint32_t chosen = states.front();
        for (std::uint32_t successor : states) {
            if (immediate_better(successor, chosen)) chosen = successor;
        }
        displayed[chosen] += probability;
    }
    for (const auto& [successor, probability] : displayed) {
        result.entries.push_back({successor, probability});
    }
    for (const OutcomeEntry& entry : result.entries) {
        const AbstractState& successor = state(entry.state);
        for (std::size_t i = 0; i < layout_.slots.size(); ++i) {
            if (successor.slot_status[i] ==
                static_cast<std::uint8_t>(GoalSlotStatus::Satisfied)) {
                result.slot_satisfied_probability[i] += entry.probability;
            }
        }
    }
    return std::make_shared<OutcomeDistribution>(std::move(result));
}

/*
 * Exact single-add distribution: enumerate the same weighted pool the
 * engine's add_random_mod samples (open_side_filter semantics: caps and the
 * request's side filter only — metamod locks never close a side for adds)
 * and accumulate each candidate's projected successor at weight/total.
 * Returns false when no side is open or the pool is empty, which the engine
 * reports as an unapplied action.
 */
bool CalcContext::evaluate_pool_add(
    const pc_item_state& item,
    const PoolBuildRequest& base_request,
    std::map<std::uint32_t, double>& accumulated) {
    const SessionImpl& session = *session_;
    const std::uint8_t cap = rarity_affix_cap(session, item.rarity);
    const bool prefix_open =
        item.prefix_count < cap && base_request.side_filter != 1;
    const bool suffix_open =
        item.suffix_count < cap && base_request.side_filter != 0;
    if (!prefix_open && !suffix_open) return false;
    PoolBuildRequest request = base_request;
    request.side_filter =
        prefix_open && suffix_open ? -1 : (prefix_open ? 0 : 1);
    const WeightedPool& pool = get_weighted_pool(context_, &item, request);
    if (pool.total_weight == 0) return false;
    for (const PoolEntry& entry : pool.entries) {
        if (entry.final_weight == 0) continue;
        pc_item_state copy = item;
        if (pc_item_add_mod(
                &copy,
                entry.gen_type == 0 ? PC_SIDE_PREFIX : PC_SIDE_SUFFIX,
                entry.session_mod_id,
                static_cast<std::uint16_t>(entry.primary_group), 0,
                nullptr) != PC_RESULT_OK) {
            continue;
        }
        accumulated[intern_item(copy)] +=
            static_cast<double>(entry.final_weight) /
            static_cast<double>(pool.total_weight);
    }
    return true;
}

} // namespace solver
} // namespace poecraft
