#include "solver_selective_completion.hpp"
#include "solver_options_helpers.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace poecraft::solver {
namespace {

std::uint32_t packed_tiers(const AbstractState& state) {
    return static_cast<std::uint32_t>(state.searing_exarch_tier) |
        (static_cast<std::uint32_t>(state.eater_of_worlds_tier) << 8u);
}

bool uses_protected_scour(const SelectiveCompletionVariant variant) {
    return variant == SelectiveCompletionVariant::ProtectedScour ||
        variant == SelectiveCompletionVariant::ProtectedScourFill;
}

bool uses_growth(const SelectiveCompletionVariant variant) {
    return variant == SelectiveCompletionVariant::EldritchGrowthRepair ||
        variant == SelectiveCompletionVariant::EldritchGrowthWithBlocker;
}

} // namespace

std::vector<std::uint64_t> finder_program_occurrence_key(
        const CalcContext& calc, const FinderProgramBinding& binding) {
    auto key = planner_operator_semantic_key(calc.operators().at(binding.operator_index));
    if (binding.intent != FinderProgramIntent::ExactOperator) {
        // A new compiler-owned occurrence has a different immutable identity
        // from an exact-operator occurrence. The full source native key stays.
        key.push_back(0x46494e4445524931ull);
        key.push_back(static_cast<std::uint64_t>(binding.intent));
        key.push_back(binding.held_goal_mask);
    }
    return key;
}

bool finder_program_is_single_temporary_attempt(const CalcContext& calc,
        const std::uint32_t state, const std::uint32_t index) {
    if (index >= calc.operators().size() ||
        !calc.is_candidate_operator_admitted_for_state(state, index)) return false;
    const auto& option = calc.operators()[index];
    if (option.kind != PlannerOperatorKind::FixedOption ||
        option.option_kind != FixedOptionKind::TemporaryBenchRepeat ||
        (option.automatic_kind != AutomaticCandidateKind::TemporaryBenchBlocker &&
         option.automatic_kind != AutomaticCandidateKind::CannotRoll) ||
        option.primitive_program != std::vector<std::uint32_t>{
            option.setup_action, option.followup_action, option.cleanup_action} ||
        option.setup_action == kNoId || option.followup_action == kNoId ||
        option.cleanup_action == kNoId ||
        calc.registry().actions.at(option.setup_action).params.type != ActionType::Bench ||
        calc.registry().actions.at(option.followup_action).params.type != ActionType::Exalt ||
        calc.registry().actions.at(option.cleanup_action).params.type != ActionType::RemoveCraftedModifiers)
        return false;
    const auto* kernel = calc.cached_option_kernel(state, index);
    return kernel != nullptr && kernel->supported && kernel->legal &&
        kernel->terminates_almost_surely && kernel->automatic.eligible &&
        kernel->automatic.setup_complete && kernel->automatic.cleanup_complete &&
        kernel->automatic.recovery_complete && kernel->automatic.exits_complete &&
        !kernel->exits.empty() && kernel->retry_states.empty() &&
        kernel->observation_choice_groups.empty() && !kernel->entry_continues &&
        // The supported, fully legal three-step programme and empty retry
        // set certify one attempt. The diagnostic action count is a weighted
        // floating-point sum and may differ from three by one rounding bit.
        // Keep the complete native resource obligation exact instead.
        kernel->expected_resources == option.resource_quantities &&
        option.resource_quantities == aggregate_resources(calc.registry(), option.primitive_program);
}

SelectiveCompletionProducer::SelectiveCompletionProducer(
    CalcContext& problem, const pc_item_state& original_start,
    const std::unordered_map<std::string, double>& prices,
    const SolveOptions& limits, const SelectiveCompletionVariant variant,
    const std::uint32_t acquisition_action, const std::uint32_t held_side)
    : problem_(problem), original_start_(original_start), prices_(prices),
      limits_(limits), variant_(variant),
      requested_acquisition_(acquisition_action), requested_held_side_(held_side) {}

void SelectiveCompletionProducer::refuse(std::string reason) {
    status_ = std::move(reason);
    phase_ = Phase::Done;
}

std::uint64_t SelectiveCompletionProducer::estimated_owned_bytes() const {
    std::uint64_t result = sizeof(*this) + status_.capacity();
    for (const auto& slots : side_slots_)
        result += slots.capacity() * sizeof(std::uint32_t);
    if (candidate_) {
        result += candidate_->control.nodes.capacity() *
            sizeof(FinderControlNode);
        result += candidate_->control.programs.capacity() *
            sizeof(FinderProgramBinding);
    }
    return result;
}

bool product_original_root_continuation_scope(
        const CalcContext& calc, const pc_item_state& root, const SolveOptions& options) {
    if (options.solve_profile != SolveProfile::CalculatorProductV1 ||
        !options.goal_progress_gated_reforges || !options.high_impact_executable_uppers ||
        options.max_absolute_optimality_gap > 0.0 || options.max_relative_optimality_gap > 0.0 ||
        (options.solve_profile_override_mask & PC_SOLVE_PROFILE_OVERRIDE_GOAL_PROGRESS_GATED_REFORGES) != 0 ||
        !calc.goal().automatic_candidates || calc.goal().rarity != PC_RARITY_RARE ||
        calc.goal().required_satisfied_slots() != calc.goal().slots.size() ||
        root.rarity != PC_RARITY_RARE || root.lifecycle != PC_ITEM_LIVE ||
        root.socket_count != 0 || root.link_mask != 0 || root.memory_strands != 0 ||
        root.enchantment_count != 0) return false;
    std::array<unsigned,2> slots{};
    for (const auto& goal : calc.goal().slots) {
        const auto side = goal_slot_side(calc.session(),goal);
        if (side != PC_SIDE_PREFIX && side != PC_SIDE_SUFFIX) return false;
        ++slots[side];
    }
    const auto held = std::max(slots[0],slots[1]);
    const auto target = std::min(slots[0],slots[1]);
    if (held < 2 || held > 3) return false;
    if (target == 0) return (calc.goal().automatic_candidate_kind_mask &
        automatic_candidate_kind_bit(AutomaticCandidateKind::ProtectedMetamod)) != 0;
    if (!calc.session().eldritch_eligible)
        return target <= 2 && (calc.goal().automatic_candidate_kind_mask &
            automatic_candidate_kind_bit(AutomaticCandidateKind::ProtectedMetamod)) != 0;
    return (held == 3 || target == 2) &&
        (calc.goal().automatic_candidate_kind_mask &
         automatic_candidate_kind_bit(AutomaticCandidateKind::EldritchSide)) != 0;
}

SelectiveCompletionVariant product_completion_variant(const CalcContext& problem,
        const std::uint32_t held_side) {
    std::array<unsigned,2> slots{};
    for (const auto& goal : problem.goal().slots) {
        const auto side = goal_slot_side(problem.session(),goal);
        if (side == PC_SIDE_PREFIX || side == PC_SIDE_SUFFIX) ++slots[side];
    }
    if (std::min(slots[0],slots[1]) == 0) return SelectiveCompletionVariant::ProtectedScour;
    if (!problem.session().eldritch_eligible)
        return SelectiveCompletionVariant::ProtectedScourFill;
    const auto held_count = held_side == kNoId
        ? std::max(slots[0],slots[1]) : slots.at(held_side);
    return held_count == 3
        ? SelectiveCompletionVariant::RerollVersusRepair : SelectiveCompletionVariant::RetentionControl;
}

bool product_completion_has_two_orientations(const CalcContext& problem) {
    std::array<unsigned,2> slots{};
    for (const auto& goal : problem.goal().slots) {
        const auto side = goal_slot_side(problem.session(),goal);
        if (side != PC_SIDE_PREFIX && side != PC_SIDE_SUFFIX) return false;
        ++slots[side];
    }
    // A protected reset needs a free target-side slot for its paid bench lock.
    // Ordinary filling therefore covers at most two target goals. Eldritch
    // continuations retain their existing three-goal target orientation.
    if (!problem.session().eldritch_eligible)
        return slots[0] == 2 && slots[1] == 2;
    return (slots[0] == 2 && (slots[1] == 2 || slots[1] == 3)) ||
        (slots[0] == 3 && slots[1] == 2);
}

std::uint32_t product_completion_proposal_count(const CalcContext& problem) {
    if (!product_completion_has_two_orientations(problem)) return 1;
    // Existing proposals stay first. A five-goal Eldritch request additionally
    // offers incremental growth. The optional blocker remains diagnostic.
    return problem.session().eldritch_eligible && problem.goal().slots.size() == 5 ? 3 : 2;
}

SelectiveCompletionVariant product_completion_proposal_variant(
        const CalcContext& problem, const std::uint32_t proposal) {
    if (proposal == 2) return SelectiveCompletionVariant::EldritchGrowthRepair;
    if (proposal == 3) return SelectiveCompletionVariant::EldritchGrowthWithBlocker;
    return product_completion_variant(problem, product_completion_held_side(problem, proposal));
}

std::uint32_t product_completion_held_side(const CalcContext& problem,
        const std::uint32_t orientation) {
    std::array<unsigned,2> slots{};
    for (const auto& goal : problem.goal().slots) {
        const auto side = goal_slot_side(problem.session(), goal);
        if (side == PC_SIDE_PREFIX || side == PC_SIDE_SUFFIX) ++slots[side];
    }
    const auto larger = slots[0] >= slots[1] ? PC_SIDE_PREFIX : PC_SIDE_SUFFIX;
    return orientation == 0 ? larger : 1u - larger;
}

void SelectiveCompletionProducer::begin() {
    if (variant_ == SelectiveCompletionVariant::PartialHeldRecoveryResearch) {
        refuse("private_partial_recovery_requires_composition_owner");
        return;
    }
    if (!problem_.goal().automatic_candidates ||
        (!uses_protected_scour(variant_) &&
         !problem_.session().eldritch_eligible) ||
        original_start_.rarity != PC_RARITY_RARE) {
        refuse("native_family_not_requested_or_ineligible");
        return;
    }
    for (std::uint32_t slot = 0; slot < problem_.goal().slots.size(); ++slot) {
        const std::int8_t side = goal_slot_side(
            problem_.session(), problem_.goal().slots[slot]);
        if (side != PC_SIDE_PREFIX && side != PC_SIDE_SUFFIX) {
            refuse("goal_side_not_pure");
            return;
        }
        side_slots_[side].push_back(slot);
    }
    held_side_ = side_slots_[PC_SIDE_PREFIX].size() >=
            side_slots_[PC_SIDE_SUFFIX].size()
        ? PC_SIDE_PREFIX : PC_SIDE_SUFFIX;
    if (requested_held_side_ != kNoId) {
        if (requested_held_side_ != PC_SIDE_PREFIX && requested_held_side_ != PC_SIDE_SUFFIX) {
            refuse("invalid_requested_held_side");
            return;
        }
        held_side_ = requested_held_side_;
    }
    target_side_ = held_side_ == PC_SIDE_PREFIX
        ? PC_SIDE_SUFFIX : PC_SIDE_PREFIX;
    if (side_slots_[held_side_].size() < 2) {
        refuse("held_side_has_fewer_than_two_goals");
        return;
    }
    if (variant_ == SelectiveCompletionVariant::ProtectedScour &&
        (!side_slots_[target_side_].empty() ||
         problem_.goal().required_satisfied_slots() != problem_.goal().slots.size())) {
        refuse("protected_scour_requires_single_side_all_goals");
        return;
    }
    if (variant_ == SelectiveCompletionVariant::ProtectedScourFill &&
        (side_slots_[held_side_].size() > 3 ||
         side_slots_[target_side_].empty() ||
         side_slots_[target_side_].size() > 2 ||
         problem_.goal().required_satisfied_slots() != problem_.goal().slots.size())) {
        refuse("protected_scour_fill_requires_all_goals_and_target_craft_space");
        return;
    }
    if (variant_ == SelectiveCompletionVariant::RerollVersusRepair &&
        (side_slots_[held_side_].size() != 3 ||
         side_slots_[target_side_].empty())) {
        refuse("family_requires_full_three_goal_held_side");
        return;
    }
    if (uses_growth(variant_) &&
        (side_slots_[held_side_].size() != 2 || side_slots_[target_side_].size() != 3 ||
         problem_.goal().required_satisfied_slots() != problem_.goal().slots.size())) {
        refuse("growth_requires_two_held_and_three_target_goals");
        return;
    }
    for (const std::uint32_t slot : side_slots_[held_side_])
        held_mask_ |= 1u << slot;
    if (requested_held_mask_ != 0) {
        if (variant_ != SelectiveCompletionVariant::EldritchGrowthRepair ||
            (requested_held_mask_ & ~held_mask_) != 0 ||
            requested_held_mask_ == held_mask_) {
            refuse("private_partial_mask_is_not_a_proper_held_subset");
            return;
        }
        held_mask_ = requested_held_mask_;
    }

    const std::uint32_t root = problem_.intern_item(original_start_);
    bool selected_guarantee = false;
    for (const std::uint32_t index : problem_.candidates()) {
        if (index >= problem_.registry().actions.size()) continue;
        const ActionDescriptor& action = problem_.registry().actions[index];
        if (action.synthetic || action.uses_companion_state ||
            solver_action_disabled(problem_.goal(), action) ||
            (requested_acquisition_ == kNoId
                ? (action.params.type != ActionType::Chaos &&
                   !(limits_.product_original_root_continuations && action.params.type == ActionType::Essence))
                : index != requested_acquisition_) ||
            (action.params.type != ActionType::Chaos &&
             action.params.type != ActionType::Essence) ||
            !action_legal(problem_.session(), action,
                problem_.state(root))) continue;
        if (action.params.type == ActionType::Essence) {
            const auto essence = action.params.essence_index;
            const auto& guarantees = problem_.session().essence_guaranteed_mod_ids;
            if (essence >= guarantees.size()) continue;
            const auto mod = guarantees[essence];
            bool held_goal = false;
            for (const auto slot : side_slots_[held_side_]) {
                const auto& mask = problem_.layout().slots[slot].satisfying_mask;
                held_goal |= mod / 64 < mask.size() &&
                    ((mask[mod / 64] >> (mod % 64)) & 1ull) != 0;
            }
            if (!held_goal) continue;
        }
        double price = 0.0;
        bool complete = true;
        for (const std::string& key : action.cost_keys) {
            const auto it = prices_.find(key);
            if (it == prices_.end() || !std::isfinite(it->second) ||
                it->second < 0.0) { complete = false; break; }
            price += it->second;
        }
        const bool guaranteed = action.params.type == ActionType::Essence;
        // A guarantee orders a product proposal; complete native checking
        // compares its paid acquisition/rewrite cost with the incumbent.
        const bool prefer_guarantee = limits_.product_original_root_continuations &&
            requested_acquisition_ == kNoId;
        if (complete && std::isfinite(price) &&
            (acquisition_action_ == kNoId ||
             (prefer_guarantee && guaranteed && !selected_guarantee) ||
             ((!prefer_guarantee || guaranteed == selected_guarantee) &&
              price < acquisition_price_))) {
            acquisition_action_ = index;
            acquisition_price_ = price;
            selected_guarantee = guaranteed;
        }
    }
    if (acquisition_action_ == kNoId) {
        refuse("no_priced_root_acquisition");
        return;
    }
    if (variant_ == SelectiveCompletionVariant::ProtectedScourFill) {
        for (const auto index : problem_.candidates()) {
            if (index >= problem_.registry().actions.size()) continue;
            const auto& action = problem_.registry().actions[index];
            if (action.synthetic || action.uses_companion_state ||
                action.params.type != ActionType::Exalt ||
                solver_action_disabled(problem_.goal(), action)) continue;
            bool complete = true;
            for (const auto& key : action.cost_keys) {
                const auto it = prices_.find(key);
                if (it == prices_.end() || !std::isfinite(it->second) || it->second < 0.0) {
                    complete = false;
                    break;
                }
            }
            if (complete) { fill_action_ = index; break; }
        }
        if (fill_action_ == kNoId) {
            refuse("no_priced_requested_exalt_fill");
            return;
        }
    }
    if (uses_protected_scour(variant_)) {
        primary_.intended = ActionType::Scour;
    } else primary_.intended = variant_ ==
            SelectiveCompletionVariant::RetentionControl
        ? (side_slots_[target_side_].empty()
            ? ActionType::EldritchAnnul : ActionType::EldritchChaos)
        : ActionType::EldritchChaos;
    secondary_.intended = ActionType::EldritchAnnul;
    tertiary_.intended = ActionType::EldritchExalt;
    blocker_.intended = ActionType::Exalt;
    const bool repair = variant_ ==
        SelectiveCompletionVariant::RerollVersusRepair || uses_growth(variant_);
    // These native-pool items are admission proposals, not reached states.
    // Enumerating the full root acquisition law here spent the state/work cap
    // before a candidate existed. Original-root checking and the complete
    // positive-entry census remain the only acceptance authorities.
    const auto propose = [&](const std::uint32_t target_count) {
        pc_item_state exact;
        if (!problem_.propose_native_held_context(original_start_, held_mask_,
                target_count, acquisition_action_, exact)) return kNoId;
        const auto state = problem_.intern_item(exact);
        if ((satisfied_goal_mask(problem_.state(state)) & held_mask_) != held_mask_ ||
            problem_.is_goal_state(problem_.state(state))) return kNoId;
        pc_item_state materialized;
        return problem_.materialize(state, materialized) ? state : kNoId;
    };
    const auto primary_count = variant_ == SelectiveCompletionVariant::RetentionControl
        ? std::max<std::uint32_t>(1, std::min<std::uint32_t>(2,
            side_slots_[target_side_].size())) : 1u;
    primary_.source = propose(primary_count);
    if (repair) secondary_.source = propose(3);
    if (uses_growth(variant_)) tertiary_.source = propose(1);
    if (variant_ == SelectiveCompletionVariant::EldritchGrowthWithBlocker)
        blocker_.source = propose(2);
    if (primary_.source == kNoId ||
        (repair && secondary_.source == kNoId) ||
        (uses_growth(variant_) && tertiary_.source == kNoId) ||
        (variant_ == SelectiveCompletionVariant::EldritchGrowthWithBlocker && blocker_.source == kNoId)) {
        refuse("no_native_pool_materializable_held_proposal");
        return;
    }
    if (variant_ == SelectiveCompletionVariant::ProtectedScourFill &&
        !action_legal(problem_.session(), problem_.registry().actions.at(fill_action_),
            problem_.state(primary_.source))) {
        refuse("native_exalt_fill_ineligible");
        return;
    }
    const std::uint64_t owned = problem_.estimated_owned_bytes() +
        estimated_owned_bytes();
    if (owned >= limits_.max_solver_owned_bytes)
        throw std::length_error(
            "selective completion has no native admission memory");
    admission_.max_solver_owned_bytes =
        limits_.max_solver_owned_bytes - estimated_owned_bytes();
    admission_.max_state_action_rows = limits_.max_state_action_rows;
    admission_.max_transitions = limits_.max_transitions;
    admission_.max_imprint_program_depth =
        limits_.max_imprint_program_depth;
    admission_.max_imprint_program_work =
        limits_.max_imprint_program_work;
    admission_.consider_imprint_programs = limits_.consider_imprint_programs;
    admission_.prices = &prices_;
    admission_.cheap_programs_only = uses_protected_scour(variant_);
    phase_ = Phase::Primary;
}

bool SelectiveCompletionProducer::advance_programme(
    Programme& programme, const bool direct,
    const std::uint32_t max_work_items) {
    const std::uint32_t state = direct
        ? programme.ready : programme.source;
    // Query only the programme intent this consumer already selects below.
    // Temporary and protected variants retain their unrestricted admission.
    admission_.query = !uses_protected_scour(variant_) && &programme != &blocker_
        ? eldritch_admission_query(target_side_, programme.intended, direct)
        : AutomaticAdmissionQuery::Unrestricted;
    StateLocalAutomaticBatch batch;
    if (!problem_.advance_state_local_automatic_candidates(
            state, admission_, batch,
            std::max<std::uint32_t>(1, max_work_items))) return false;
    if (batch.status != StateLocalAutomaticBatchStatus::Complete) {
        refuse("native_admission_resource_deferred:" +
            batch.resource_cap);
        return true;
    }
    std::uint32_t selected = kNoId;
    double selected_price = std::numeric_limits<double>::infinity();
    const bool temporary = &programme == &blocker_;
    std::string certificate_refusal;
    for (const std::uint32_t index : batch.admitted_operators) {
        const PlannerOperator& option = problem_.operators().at(index);
        const bool protected_scour = uses_protected_scour(variant_);
        if (temporary) {
            if (!finder_program_is_single_temporary_attempt(problem_, state, index)) {
                if (option.option_kind == FixedOptionKind::TemporaryBenchRepeat && certificate_refusal.empty()) {
                    const auto* kernel = problem_.cached_option_kernel(state, index);
                    if (kernel && kernel->automatic.eligible) {
                        certificate_refusal = ":finite_attempt_source=" + option.id +
                            ":actions_bits=" + std::to_string(std::bit_cast<std::uint64_t>(kernel->expected_primitive_actions)) +
                            ":retries=" + std::to_string(kernel->retry_states.size()) +
                            ":resources_match=" + std::to_string(kernel->expected_resources == option.resource_quantities);
                        const auto resources = aggregate_resources(problem_.registry(), option.primitive_program);
                        for (std::size_t r=0; r<option.resource_quantities.size() && r<3; ++r)
                            certificate_refusal += ":resource=" + option.resource_quantities[r].first +
                                ":quantity_bits=" + std::to_string(std::bit_cast<std::uint64_t>(option.resource_quantities[r].second));
                        certificate_refusal += ":aggregate_match=" + std::to_string(resources == option.resource_quantities);
                    }
                }
                continue;
            }
            if (
                option.exit_goal_slots.size() != 1 ||
                goal_slot_side(problem_.session(), problem_.goal().slots.at(option.exit_goal_slots.front())) != target_side_)
                continue;
            const auto& kernel = problem_.option_kernel(state, index);
            if ((satisfied_goal_mask(problem_.state(state)) & held_mask_) != held_mask_ ||
                !std::all_of(kernel.exits.begin(), kernel.exits.end(), [&](const OutcomeEntry& exit) {
                    return (satisfied_goal_mask(problem_.state(exit.state)) & held_mask_) == held_mask_;
                })) continue;
            double price = 0.0;
            bool priced = true;
            for (const auto& [key, quantity] : kernel.expected_resources) {
                const auto it = prices_.find(key);
                if (it == prices_.end() || !std::isfinite(it->second) || it->second < 0.0) {
                    priced = false; break;
                }
                price += it->second * quantity;
            }
            if (priced && std::isfinite(price) && price < selected_price) {
                selected = index; selected_price = price;
            }
            continue;
        }
        if (option.kind != PlannerOperatorKind::FixedOption ||
            option.option_kind != (protected_scour ? FixedOptionKind::ProtectedSide : FixedOptionKind::EldritchSideIntent) ||
            option.automatic_kind != (protected_scour ? AutomaticCandidateKind::ProtectedMetamod : AutomaticCandidateKind::EldritchSide) ||
            option.intended_side != (protected_scour ? held_side_ : target_side_) ||
            option.primitive_program.empty() ||
            (direct && option.primitive_program.size() != 1) ||
            problem_.registry().actions.at(
                option.primitive_program.back()).params.type !=
                    programme.intended) continue;
        const OptionKernel& kernel = problem_.option_kernel(state, index);
        if (kernel.supported && kernel.legal &&
            kernel.automatic.eligible && !kernel.exits.empty()) {
            selected = index;
            break;
        }
    }
    if (selected == kNoId) {
        std::string detail = certificate_refusal;
        for (const auto& decision : batch.decisions) {
            if (detail.size() > 1024) break;
            detail += ":" + decision.id + ":" + decision.evidence.reason;
        }
        refuse((direct ? std::string("no_admitted_direct_continuation") :
            std::string("no_admitted_held_side_program")) + detail);
        return true;
    }
    if (direct) {
        programme.direct = selected;
        return true;
    }
    programme.initial = selected;
    programme.ready = state;
    if (uses_protected_scour(variant_) || temporary) {
        programme.direct = selected;
        return true;
    }
    const auto& steps = problem_.operators().at(selected).primitive_program;
    for (std::size_t step = 0; step + 1 < steps.size(); ++step) {
        const OutcomeDistribution& law =
            problem_.outcomes(programme.ready, steps[step]);
        if (!law.supported || !law.applicable ||
            !law.choice_groups.empty() || law.entries.size() != 1 ||
            std::abs(law.entries.front().probability - 1.0) > 1e-12) {
            refuse("setup_is_not_deterministic");
            return true;
        }
        programme.ready = law.entries.front().state;
    }
    if (programme.ready == programme.source)
        programme.direct = programme.initial;
    return true;
}

void SelectiveCompletionProducer::build() {
    FinderControlGraph graph;
    graph.entry = 0;
    const auto append = [&](FinderControlKind kind,
            std::uint32_t binding = kNoId) {
        const std::uint32_t index = static_cast<std::uint32_t>(
            graph.nodes.size());
        graph.nodes.push_back({kind, binding});
        return index;
    };
    const auto bind = [&](const Programme& p) {
        const std::uint32_t first = static_cast<std::uint32_t>(
            graph.programs.size());
        const auto intent = &p == &tertiary_ ? FinderProgramIntent::NativeMissingEldritchGoal :
            &p == &blocker_ ? FinderProgramIntent::NativeTemporaryGoalAttempt : FinderProgramIntent::ExactOperator;
        graph.programs.push_back({p.initial, p.source, held_mask_, intent});
        if (p.ready != p.source)
            graph.programs.push_back({p.direct, p.ready, held_mask_, intent});
        return first;
    };
    const std::uint32_t primary_binding = bind(primary_);
    if (uses_protected_scour(variant_)) {
        const auto goal = append(FinderControlKind::TestGoal);
        const auto acquire = append(FinderControlKind::RunPrimitive, acquisition_action_);
        const auto success = append(FinderControlKind::GoalTerminal);
        const auto programme = append(FinderControlKind::RunNativeProgram, primary_binding);
        const auto held_junk = side_slots_[held_side_].size() < 3
            ? append(FinderControlKind::TestSideCountAtLeast,
                (held_side_ << 8u) | (side_slots_[held_side_].size() + 1)) : kNoId;
        const auto full_craft_side = append(FinderControlKind::TestSideCountAtLeast,
            (target_side_ << 8u) | 3u);
        graph.nodes[goal].on_true = success;
        graph.nodes[goal].on_false = held_junk == kNoId ? full_craft_side : held_junk;
        if (held_junk != kNoId) {
            graph.nodes[held_junk].on_true = acquire;
            graph.nodes[held_junk].on_false = full_craft_side;
        }
        graph.nodes[full_craft_side].on_true = acquire;
        std::uint32_t previous = full_craft_side;
        for (auto slot : side_slots_[held_side_]) {
            const auto test = append(FinderControlKind::TestSlot,slot);
            if (previous == full_craft_side) graph.nodes[previous].on_false = test;
            else graph.nodes[previous].on_true = test;
            graph.nodes[test].on_false = acquire;
            previous = test;
        }
        if (variant_ == SelectiveCompletionVariant::ProtectedScourFill) {
            const auto occupied = append(FinderControlKind::TestSideCountAtLeast,
                (target_side_ << 8u) | side_slots_[target_side_].size());
            const auto fill = append(FinderControlKind::RunPrimitive, fill_action_);
            graph.nodes[previous].on_true = occupied;
            // Test the unchanged complete goal after each paid operation.
            // At the requested target occupancy, an unsuccessful attempt is
            // reset by the admitted paid lock/Scour programme. Filling stops
            // before it consumes the slot needed for that lock.
            graph.nodes[occupied].on_true = programme;
            graph.nodes[occupied].on_false = fill;
            graph.nodes[fill].next = goal;
        } else graph.nodes[previous].on_true = programme;
        graph.nodes[programme].next = goal;
        graph.nodes[acquire].next = goal;
        candidate_ = SelectiveCompletionCandidate{std::move(graph), acquisition_action_, acquisition_price_, variant_};
        status_ = "complete";
        phase_ = Phase::Done;
        return;
    }
    const bool repair = variant_ ==
        SelectiveCompletionVariant::RerollVersusRepair || uses_growth(variant_);
    const std::uint32_t secondary_binding = repair
        ? bind(secondary_) : kNoId;
    const auto tertiary_binding = uses_growth(variant_) ? bind(tertiary_) : kNoId;
    const auto blocker_binding = variant_ == SelectiveCompletionVariant::EldritchGrowthWithBlocker
        ? bind(blocker_) : kNoId;
    const std::uint32_t goal = append(FinderControlKind::TestGoal);
    const bool clean_held = problem_.goal().terminal.extras == ExtraExplicitPolicy::ForbidUnmatched &&
        side_slots_[held_side_].size() < 3 && requested_held_mask_ == 0;
    const auto held_junk = clean_held
        ? append(FinderControlKind::TestSideCountAtLeast,
            (held_side_ << 8u) | (side_slots_[held_side_].size() + 1)) : kNoId;
    std::vector<std::uint32_t> held_tests;
    for (const std::uint32_t slot : side_slots_[held_side_])
        if ((held_mask_ & (1u << slot)) != 0)
            held_tests.push_back(append(FinderControlKind::TestSlot, slot));
    // Eldritch Chaos produces two or three target-side affixes. For a clean
    // one-affix target, annulling only at three traps the held controller in
    // the closed two/three-affix class. Continue repair at two as well; the
    // original goal test still decides success after every paid operation.
    const std::uint32_t repair_test =
        variant_ == SelectiveCompletionVariant::RetentionControl ? kNoId :
        append(FinderControlKind::TestSideCountAtLeast,
            (target_side_ << 8u) |
                (side_slots_[target_side_].size() == 1 ? 2u : 3u));
    // A three-goal target must retain its held side after native rerolls return
    // two affixes. The unchanged exact goal test still requires all three.
    const std::uint32_t occupied_test = uses_growth(variant_) ? kNoId : append(
        FinderControlKind::TestSideCountAtLeast,
        (target_side_ << 8u) |
            (variant_ == SelectiveCompletionVariant::RetentionControl
                ? std::max<std::uint32_t>(
                    1, std::min<std::uint32_t>(2,
                        side_slots_[target_side_].size())) : 1u));
    const std::uint32_t acquire = append(
        FinderControlKind::RunPrimitive, acquisition_action_);
    const std::uint32_t success = append(FinderControlKind::GoalTerminal);
    graph.nodes[goal].on_true = success;
    graph.nodes[goal].on_false = clean_held ? held_junk : held_tests.front();
    if (clean_held) {
        graph.nodes[held_junk].on_true = acquire;
        graph.nodes[held_junk].on_false = held_tests.front();
    }
    const auto compatibility = escape_persistent_blockers_ ?
        append(FinderControlKind::TestTargetGoalsCompatible,target_side_) : kNoId;
    if (compatibility != kNoId) {
        graph.nodes[compatibility].on_true = repair_test == kNoId ? occupied_test : repair_test;
        graph.nodes[compatibility].on_false = acquire;
    }
    for (std::size_t i = 0; i < held_tests.size(); ++i) {
        graph.nodes[held_tests[i]].on_true = i + 1 < held_tests.size()
            ? held_tests[i + 1] :
                (compatibility != kNoId ? compatibility :
                    (repair_test == kNoId ? occupied_test : repair_test));
        graph.nodes[held_tests[i]].on_false = acquire;
    }
    graph.nodes[acquire].next = goal;
    if (repair_test != kNoId)
        graph.nodes[repair_test].on_false = occupied_test;
    if (occupied_test != kNoId) graph.nodes[occupied_test].on_false = acquire;
    const auto branch = [&](const Programme& programme,
            std::uint32_t binding) {
        const std::uint32_t ready_test = append(
            FinderControlKind::TestEldritchTiers,
            packed_tiers(problem_.state(programme.ready)));
        const std::uint32_t source_test =
            programme.ready == programme.source ? kNoId : append(
                FinderControlKind::TestEldritchTiers,
                packed_tiers(problem_.state(programme.source)));
        const std::uint32_t initial_run = append(
            FinderControlKind::RunNativeProgram, binding);
        const std::uint32_t direct_run = source_test == kNoId
            ? initial_run : append(FinderControlKind::RunNativeProgram,
                binding + 1);
        graph.nodes[ready_test].on_true = direct_run;
        graph.nodes[ready_test].on_false =
            source_test == kNoId ? acquire : source_test;
        if (source_test != kNoId) {
            graph.nodes[source_test].on_true = initial_run;
            graph.nodes[source_test].on_false = acquire;
        }
        graph.nodes[initial_run].next = goal;
        graph.nodes[direct_run].next = goal;
        return ready_test;
    };
    const std::uint32_t primary_branch = branch(
        primary_, primary_binding);
    if (uses_growth(variant_)) {
        // Keep partial target progress. At full capacity repair only when a
        // target goal is present; otherwise reroll that side. Below capacity
        // add through the native dominant-side intent. The blocker proposal
        // uses an independently admitted finite attempt at two target affixes.
        const auto repair_branch = branch(secondary_, secondary_binding);
        const auto fill_branch = branch(tertiary_, tertiary_binding);
        auto guarded_fill = fill_branch;
        if (guard_missing_goal_rollability_) {
            guarded_fill = append(FinderControlKind::TestMissingGoalRollable, target_side_);
            graph.nodes[guarded_fill].on_true = fill_branch;
            // Preserve the held subset and pay the existing native side reroll.
            // A refused lookup is never permission to repeat a no-progress draw.
            graph.nodes[guarded_fill].on_false = primary_branch;
        }
        auto full_miss = primary_branch;
        for (auto slot : side_slots_[target_side_]) {
            const auto test = append(FinderControlKind::TestSlot, slot);
            graph.nodes[test].on_true = repair_branch;
            graph.nodes[test].on_false = full_miss;
            full_miss = test;
        }
        graph.nodes[repair_test].on_true = full_miss;
        graph.nodes[repair_test].on_false = fill_branch;
        if (reroll_without_target_progress_) {
            // This private composition distinguishes empty target progress
            // below capacity too. The legacy proposals keep their old route.
            auto below_miss = primary_branch;
            for (const auto slot : side_slots_[target_side_]) {
                const auto test = append(FinderControlKind::TestSlot, slot);
                graph.nodes[test].on_true = guarded_fill;
                graph.nodes[test].on_false = below_miss;
                below_miss = test;
            }
            graph.nodes[repair_test].on_false = below_miss;
        }
        if (blocker_binding != kNoId) {
            const auto two = append(FinderControlKind::TestSideCountAtLeast,
                (target_side_ << 8u) | 2u);
            const auto run = append(FinderControlKind::RunNativeProgram, blocker_binding);
            graph.nodes[two].on_true = run;
            graph.nodes[two].on_false = fill_branch;
            graph.nodes[run].next = goal;
            graph.nodes[repair_test].on_false = two;
        }
    } else if (variant_ == SelectiveCompletionVariant::RetentionControl) {
        graph.nodes[occupied_test].on_true = primary_branch;
    } else {
        graph.nodes[occupied_test].on_true = primary_branch;
        graph.nodes[repair_test].on_true =
            branch(secondary_, secondary_binding);
    }
    candidate_ = SelectiveCompletionCandidate{
        std::move(graph), acquisition_action_, acquisition_price_, variant_};
    status_ = "complete";
    phase_ = Phase::Done;
}

bool SelectiveCompletionProducer::advance(
        const std::uint32_t max_work_items) {
    if (done()) return true;
    try {
        switch (phase_) {
        case Phase::Begin:
            begin();
            break;
        case Phase::Primary:
            if (advance_programme(primary_, false, max_work_items) &&
                !done()) phase_ = primary_.direct == kNoId
                    ? Phase::PrimaryDirect :
                    (variant_ == SelectiveCompletionVariant::RerollVersusRepair || uses_growth(variant_)
                        ? Phase::Secondary : Phase::Build);
            break;
        case Phase::PrimaryDirect:
            if (advance_programme(primary_, true, max_work_items) &&
                !done()) phase_ =
                    (variant_ == SelectiveCompletionVariant::RerollVersusRepair || uses_growth(variant_))
                        ? Phase::Secondary : Phase::Build;
            break;
        case Phase::Secondary:
            if (advance_programme(secondary_, false, max_work_items) &&
                !done()) phase_ = secondary_.direct == kNoId
                    ? Phase::SecondaryDirect : (uses_growth(variant_) ? Phase::Tertiary : Phase::Build);
            break;
        case Phase::SecondaryDirect:
            if (advance_programme(secondary_, true, max_work_items) &&
                !done()) phase_ = uses_growth(variant_) ? Phase::Tertiary : Phase::Build;
            break;
        case Phase::Tertiary:
            if (advance_programme(tertiary_, false, max_work_items) && !done())
                phase_ = tertiary_.direct == kNoId ? Phase::TertiaryDirect :
                    (variant_ == SelectiveCompletionVariant::EldritchGrowthWithBlocker ? Phase::Blocker : Phase::Build);
            break;
        case Phase::TertiaryDirect:
            if (advance_programme(tertiary_, true, max_work_items) && !done())
                phase_ = variant_ == SelectiveCompletionVariant::EldritchGrowthWithBlocker ? Phase::Blocker : Phase::Build;
            break;
        case Phase::Blocker:
            if (advance_programme(blocker_, false, max_work_items) && !done()) phase_ = Phase::Build;
            break;
        case Phase::Build:
            build();
            break;
        case Phase::Done:
            break;
        }
    } catch (const SolverResourceLimit& ex) {
        if (problem_.reforge_work_budget_owner() != nullptr &&
            ex.cap_name() == "max_reforge_work") throw;
        refuse(std::string("native_construction_capacity:") + ex.what());
    } catch (const std::length_error& ex) {
        refuse(std::string("native_construction_capacity:") + ex.what());
    } catch (const std::exception& ex) {
        refuse(std::string("native_construction_refused:") + ex.what());
    }
    return done();
}

std::optional<PartialHeldRecoveryScope> partial_held_recovery_scope(
        const CalcContext& problem, const pc_item_state& original_start,
        const SolveOptions& limits) {
    if (!product_original_root_continuation_scope(problem, original_start, limits) ||
        !problem.session().eldritch_eligible || problem.session().rare_affix_cap != 3 ||
        original_start.prefix_count != 0 || original_start.suffix_count != 0 ||
        original_start.quality != 0 || original_start.implicit_count != 0 ||
        original_start.item_flags != 0 || original_start.generic_influence_bits != 0 ||
        original_start.searing_exarch_tier > 4 || original_start.eater_of_worlds_tier > 4 ||
        problem.goal().slots.size() != 5 ||
        problem.goal().terminal.extras != ExtraExplicitPolicy::ForbidUnmatched)
        return std::nullopt;
    // The count law is an engine capability, never a base-name predicate.
    // Three preserved goals then imply at least one opposite affix after
    // acquisition/reforge; final repair only annuls at opposite count three.
    const auto law = rare_reforge_count_law(problem.session().rare_reforge_count_kind);
    for (const auto& draw : law.draws)
        if (draw.weight != 0 && draw.count < 4) return std::nullopt;
    std::array<std::uint32_t, 2> masks{};
    std::array<std::uint32_t, 5> families{};
    for (std::uint32_t slot = 0; slot < 5; ++slot) {
        const auto& goal = problem.goal().slots[slot];
        const auto side = goal_slot_side(problem.session(), goal);
        if (side != PC_SIDE_PREFIX && side != PC_SIDE_SUFFIX) return std::nullopt;
        if (goal.family_id == kNoId || goal.group_id != kNoId ||
            std::find(families.begin(), families.begin() + slot, goal.family_id) !=
                families.begin() + slot ||
            !mask_intersects(problem.layout().slots[slot].satisfying_mask,
                problem.session().normal_random_roll_mask)) return std::nullopt;
        families[slot] = goal.family_id;
        masks[side] |= 1u << slot;
    }
    const auto small = std::popcount(masks[0]) == 2 && std::popcount(masks[1]) == 3
        ? 0u : std::popcount(masks[1]) == 2 && std::popcount(masks[0]) == 3 ? 1u : kNoId;
    if (small == kNoId) return std::nullopt;
    PartialHeldRecoveryScope result;
    result.held_side = small;
    unsigned anchor = 0;
    for (unsigned slot = 0; slot < 5; ++slot)
        if ((masks[small] & (1u << slot)) != 0) result.anchor_masks[anchor++] = 1u << slot;
    return result;
}

PartialHeldRecoveryProducer::PartialHeldRecoveryProducer(CalcContext& problem,
        const pc_item_state& original_start,
        const std::unordered_map<std::string, double>& prices,
        const SolveOptions& limits, const std::uint32_t anchor_mask,
        const bool private_gate, const bool guard_missing_rollability,
        const bool escape_persistent_blockers)
    : problem_(problem), original_start_(original_start), prices_(prices),
      limits_(limits), anchor_mask_(anchor_mask), private_gate_(private_gate),
      guard_missing_rollability_(guard_missing_rollability),
      escape_persistent_blockers_(escape_persistent_blockers) {}

void PartialHeldRecoveryProducer::refuse(std::string reason) {
    status_ = std::move(reason);
    done_ = true;
}

std::uint64_t PartialHeldRecoveryProducer::estimated_owned_bytes() const {
    auto bytes = sizeof(*this) + status_.capacity();
    const auto graph_bytes = [](const auto& candidate) -> std::uint64_t {
        return candidate ? candidate->control.nodes.capacity() * sizeof(FinderControlNode) +
            candidate->control.programs.capacity() * sizeof(FinderProgramBinding) : 0;
    };
    for (const auto& stage : stages_) bytes += graph_bytes(stage);
    return bytes + graph_bytes(candidate_) + (active_ ? active_->estimated_owned_bytes() : 0);
}

void PartialHeldRecoveryProducer::begin() {
    begun_ = true;
    if (!private_gate_) { refuse("private_recovery_gate_disabled"); return; }
    scope_ = partial_held_recovery_scope(problem_, original_start_, limits_);
    if (!scope_ || std::find(scope_->anchor_masks.begin(), scope_->anchor_masks.end(),
            anchor_mask_) == scope_->anchor_masks.end()) {
        refuse("unsupported_partial_held_capability_or_anchor"); return;
    }
    const auto root = problem_.intern_item(original_start_);
    for (const auto index : problem_.candidates()) {
        if (index >= problem_.registry().actions.size()) continue;
        const auto& action = problem_.registry().actions[index];
        if (action.params.type != ActionType::Chaos || action.synthetic ||
            action.uses_companion_state || solver_action_disabled(problem_.goal(), action) ||
            !action_legal(problem_.session(), action, problem_.state(root))) continue;
        double price = 0;
        bool complete = true;
        for (const auto& key : action.cost_keys) {
            const auto it = prices_.find(key);
            if (it == prices_.end() || !std::isfinite(it->second) || it->second < 0) {
                complete = false; break;
            }
            price += it->second;
        }
        if (complete && std::isfinite(price) &&
            (acquisition_ == kNoId || price < acquisition_price_)) {
            acquisition_ = index;
            acquisition_price_ = price;
        }
    }
    if (acquisition_ == kNoId) refuse("no_priced_requested_chaos_acquisition");
}

void PartialHeldRecoveryProducer::begin_stage() {
    auto stage_limits = limits_;
    const auto retained = estimated_owned_bytes();
    if (retained >= stage_limits.max_solver_owned_bytes ||
        problem_.estimated_owned_bytes() >= stage_limits.max_solver_owned_bytes - retained)
        throw std::length_error("partial recovery has no native stage memory");
    stage_limits.max_solver_owned_bytes -= retained;
    auto source = original_start_; // An admission proposal context, never a new original root.
    if (stage_ == 3) {
        source.searing_exarch_tier = static_cast<std::uint8_t>(growth_ready_tiers_ & 255u);
        source.eater_of_worlds_tier = static_cast<std::uint8_t>(growth_ready_tiers_ >> 8u);
    }
    active_ = std::make_unique<SelectiveCompletionProducer>(problem_, source, prices_,
        stage_limits, stage_ < 2 ? SelectiveCompletionVariant::EldritchGrowthRepair :
            SelectiveCompletionVariant::RerollVersusRepair,
        acquisition_, stage_ < 2 ? scope_->held_side : 1u - scope_->held_side);
    active_->escape_persistent_blockers_ = escape_persistent_blockers_;
    if (stage_ == 0) active_->requested_held_mask_ = anchor_mask_;
    if (stage_ < 2) {
        active_->reroll_without_target_progress_ = true;
        active_->guard_missing_goal_rollability_ = guard_missing_rollability_;
    }
}

void PartialHeldRecoveryProducer::finish_stage() {
    if (!active_->candidate_) {
        refuse("partial_stage_" + std::to_string(stage_) + ":" + active_->status());
        return;
    }
    const auto ready = packed_tiers(problem_.state(active_->primary_.ready));
    if (ready != packed_tiers(problem_.state(active_->secondary_.ready)) ||
        (stage_ < 2 && ready != packed_tiers(problem_.state(active_->tertiary_.ready)))) {
        refuse("native_stage_setup_tiers_disagree"); return;
    }
    if (stage_ == 0) growth_ready_tiers_ = ready;
    if (stage_ == 1 && ready != growth_ready_tiers_) {
        refuse("singleton_and_full_growth_setup_disagree"); return;
    }
    if (stage_ == 2) final_original_ready_tiers_ = ready;
    if (stage_ == 3) final_growth_ready_tiers_ = ready;
    stages_[stage_] = std::move(active_->candidate_);
    active_.reset(); // The next stage has a single new admission cursor.
    ++stage_;
}

void PartialHeldRecoveryProducer::compose() {
    std::size_t nodes = 32, programs = 0;
    for (const auto& stage : stages_) {
        nodes += stage->control.nodes.size();
        programs += stage->control.programs.size();
    }
    if (nodes > limits_.max_compiled_nodes)
        throw std::length_error("partial recovery exceeds bounded control node cap");
    const auto assembly = nodes * sizeof(FinderControlNode) +
        programs * sizeof(FinderProgramBinding) + 65536ull;
    const auto retained = estimated_owned_bytes();
    if (retained + assembly > limits_.max_solver_owned_bytes ||
        problem_.estimated_owned_bytes() > limits_.max_solver_owned_bytes - retained - assembly)
        throw std::length_error("partial recovery has no accounted composition memory");
    FinderControlGraph graph;
    graph.nodes.reserve(nodes);
    graph.programs.reserve(programs);
    const auto append = [&](const FinderControlKind kind, const std::uint32_t binding = kNoId) {
        const auto node = static_cast<std::uint32_t>(graph.nodes.size());
        graph.nodes.push_back({kind, binding});
        return node;
    };
    const auto root_goal = append(FinderControlKind::TestGoal);
    const auto acquire = append(FinderControlKind::RunPrimitive, acquisition_);
    const auto success = append(FinderControlKind::GoalTerminal);
    const auto failure = append(FinderControlKind::FailureTerminal);
    const auto dispatch = append(FinderControlKind::TestGoal);
    graph.entry = root_goal;
    graph.nodes[root_goal].on_true = success;
    graph.nodes[root_goal].on_false = acquire;
    graph.nodes[acquire].next = dispatch;
    graph.nodes[dispatch].on_true = success;
    std::array<std::uint32_t, 4> entries{};
    for (unsigned stage = 0; stage < 4; ++stage) {
        const auto& source = stages_[stage]->control;
        const auto node_offset = static_cast<std::uint32_t>(graph.nodes.size());
        const auto program_offset = static_cast<std::uint32_t>(graph.programs.size());
        const auto remap = [&](const std::uint32_t target) {
            if (target == kNoId) return kNoId;
            const auto& node = source.nodes.at(target);
            if (node.kind == FinderControlKind::RunPrimitive && node.binding == acquisition_)
                return stage < 2 ? acquire : failure;
            return node_offset + target;
        };
        entries[stage] = node_offset + source.entry;
        graph.programs.insert(graph.programs.end(), source.programs.begin(), source.programs.end());
        for (auto node : source.nodes) {
            if (node.kind == FinderControlKind::RunPrimitive && node.binding == acquisition_) {
                node = {FinderControlKind::FailureTerminal}; // All incoming acquisition edges were remapped.
            } else {
                node.on_true = remap(node.on_true);
                node.on_false = node.kind == FinderControlKind::TestTargetGoalsCompatible ?
                    acquire : remap(node.on_false);
                // Persistent blockers require the original paid whole-item
                // acquisition even in final stages. Chaos preserves tiers;
                // unrepresented later tier frames retain their refusal ports.
                node.next = node.kind == FinderControlKind::RunNativeProgram ? dispatch : remap(node.next);
                if (node.kind == FinderControlKind::RunNativeProgram) node.binding += program_offset;
            }
            graph.nodes.push_back(node);
        }
    }
    const auto tier_entry = [&](const std::uint32_t source, const std::uint32_t ready,
            const std::uint32_t entry, const std::uint32_t fallback) {
        auto next = fallback;
        const auto test = append(FinderControlKind::TestEldritchTiers, ready);
        graph.nodes[test].on_true = entry;
        graph.nodes[test].on_false = next;
        next = test;
        if (source != ready) {
            const auto initial = append(FinderControlKind::TestEldritchTiers, source);
            graph.nodes[initial].on_true = entry;
            graph.nodes[initial].on_false = next;
            next = initial;
        }
        return next;
    };
    const auto original_tiers = static_cast<std::uint32_t>(original_start_.searing_exarch_tier) |
        (static_cast<std::uint32_t>(original_start_.eater_of_worlds_tier) << 8u);
    auto final = tier_entry(growth_ready_tiers_, final_growth_ready_tiers_, entries[3], failure);
    final = tier_entry(original_tiers, final_original_ready_tiers_, entries[2], final);
    const auto all_slots = [&](const std::uint32_t side, const std::uint32_t hit,
            const std::uint32_t miss) {
        auto entry = hit;
        for (unsigned slot = 0; slot < problem_.goal().slots.size(); ++slot) {
            if (goal_slot_side(problem_.session(), problem_.goal().slots[slot]) != side) continue;
            const auto test = append(FinderControlKind::TestSlot, slot);
            graph.nodes[test].on_true = entry;
            graph.nodes[test].on_false = miss;
            entry = test;
        }
        return entry;
    };
    auto partial = entries[0];
    for (unsigned slot = 0; slot < problem_.goal().slots.size(); ++slot) {
        if (goal_slot_side(problem_.session(), problem_.goal().slots[slot]) != scope_->held_side) continue;
        const auto test = append(FinderControlKind::TestSlot, slot);
        if ((anchor_mask_ & (1u << slot)) != 0) {
            graph.nodes[test].on_true = partial;
            graph.nodes[test].on_false = acquire;
        } else {
            // Exact singleton identity: destructive programme keys include
            // every satisfied opposite-side goal, not just the chosen anchor.
            graph.nodes[test].on_true = acquire;
            graph.nodes[test].on_false = partial;
        }
        partial = test;
    }
    const auto full_small = all_slots(scope_->held_side, entries[1], partial);
    graph.nodes[dispatch].on_false = all_slots(1u - scope_->held_side, final, full_small);
    if (graph.nodes.size() > nodes || graph.programs.size() != programs)
        throw std::logic_error("partial recovery escaped its finite composition bound");
    candidate_ = SelectiveCompletionCandidate{std::move(graph), acquisition_,
        acquisition_price_, SelectiveCompletionVariant::PartialHeldRecoveryResearch};
    status_ = "constructed_private_unchecked";
    done_ = true;
}

bool PartialHeldRecoveryProducer::advance(const std::uint32_t max_work_items) {
    if (done_) return true;
    try {
        if (!begun_) begin();
        else if (stage_ == stages_.size()) compose();
        else if (!active_) begin_stage();
        else if (active_->advance(max_work_items)) finish_stage();
    } catch (const SolverResourceLimit& error) {
        if (problem_.reforge_work_budget_owner() && error.cap_name() == "max_reforge_work") throw;
        refuse(std::string("private_recovery_capacity:") + error.what());
    } catch (const std::length_error& error) {
        refuse(std::string("private_recovery_capacity:") + error.what());
    } catch (const std::exception& error) {
        refuse(std::string("private_recovery_refused:") + error.what());
    }
    return done_;
}

SelectiveProgrammeEntryValidator::SelectiveProgrammeEntryValidator(
    const CalcContext& problem,
    std::shared_ptr<const SessionImpl> session,
    const FinderControlGraph& control,
    const StrategyPolicyEntryCertificate& census,
    const std::unordered_map<std::string, double>& prices,
    const SolveOptions& limits)
    : problem_(problem), session_(std::move(session)), control_(control),
      census_(census), prices_(prices), limits_(limits) {
    if (session_ == nullptr || !census_.requested ||
        census_.entries.empty() || census_.reached_decisions == 0 ||
        census_.refused_entries != 0)
        throw StrategyEvalUnsupported(
            "native programme has no complete reached entry census");
    for (const auto& entry : census_.entries)
        positive_entries_ += entry.root_expected_visits > 0.0;
}

std::uint64_t SelectiveProgrammeEntryValidator::logical_work() const {
    return calc_ == nullptr ? 0 :
        calc_->telemetry().reforge_logical_work_v1 +
        calc_->telemetry().automatic_admission_reforge_logical_work_v1;
}

std::uint64_t SelectiveProgrammeEntryValidator::active_work() const {
    return calc_ == nullptr ? 0 :
        calc_->telemetry().reforge_frontier_work +
        calc_->telemetry().automatic_admission_reforge_active_work;
}

std::uint64_t SelectiveProgrammeEntryValidator::estimated_owned_bytes() const {
    return sizeof(*this) + (calc_ == nullptr ? 0 :
        calc_->estimated_owned_bytes());
}

bool SelectiveProgrammeEntryValidator::advance(
        const std::uint32_t max_work_items) {
    if (done()) return true;
    if (calc_ == nullptr) {
        calc_ = std::make_unique<CalcContext>(
            session_, problem_.goal(), problem_.registry(),
            problem_.candidates(), false, false, false,
            std::nullopt, std::vector<CountObservation>{}, false,
            std::vector<std::uint64_t>{}, true);
        calc_->set_reforge_work_budget_owner(
            problem_.reforge_work_budget_owner());
        if (estimated_owned_bytes() >= limits_.max_solver_owned_bytes)
            throw std::length_error(
                "native programme has no exact admission memory");
        admission_.max_solver_owned_bytes =
            limits_.max_solver_owned_bytes - sizeof(*this);
        admission_.max_state_action_rows = limits_.max_state_action_rows;
        admission_.max_transitions = limits_.max_transitions;
        admission_.max_imprint_program_depth =
            limits_.max_imprint_program_depth;
        admission_.max_imprint_program_work =
            limits_.max_imprint_program_work;
        admission_.consider_imprint_programs =
            limits_.consider_imprint_programs;
        admission_.prices = &prices_;
    }
    const std::uint32_t budget = std::max<std::uint32_t>(
        1, max_work_items);
    for (std::uint32_t item = 0; item < budget && !done(); ++item) {
        const StrategyPolicyEntryResult& entry =
            census_.entries[cursor_];
        if (!entry.available() ||
            !(entry.root_expected_visits > 0.0) ||
            entry.checkpoint_active || entry.observed_offer_active)
            throw StrategyEvalUnsupported(
                "native programme entry has incomplete exact root coverage");
        const auto bound_node = std::find_if(control_.nodes.begin(),
            control_.nodes.end(), [&](const FinderControlNode& node) {
                const std::size_t index = &node - control_.nodes.data();
                return node.kind == FinderControlKind::RunNativeProgram &&
                    entry.compiled_node_id ==
                        "c" + std::to_string(index);
            });
        if (bound_node == control_.nodes.end())
            throw StrategyEvalUnsupported(
                "native programme entry has no trusted occurrence");
        const FinderProgramBinding& binding =
            control_.programs.at(bound_node->binding);
        const PlannerOperator& expected =
            problem_.operators().at(binding.operator_index);
        if (!entry.graph_local || entry.selected_operator_identity !=
                finder_program_occurrence_key(problem_, binding))
            throw StrategyEvalUnsupported("native programme occurrence identity mismatch");
        const bool protected_scour = expected.option_kind == FixedOptionKind::ProtectedSide &&
            expected.followup_action != kNoId &&
            problem_.registry().actions.at(expected.followup_action).params.type == ActionType::Scour;
        const bool temporary = binding.intent == FinderProgramIntent::NativeTemporaryGoalAttempt &&
            finder_program_is_single_temporary_attempt(problem_, binding.admitted_state, binding.operator_index);
        const bool missing_eldritch = binding.intent == FinderProgramIntent::NativeMissingEldritchGoal &&
            expected.option_kind == FixedOptionKind::EldritchSideIntent &&
            problem_.registry().actions.at(expected.primitive_program.back()).params.type == ActionType::EldritchExalt;
        if ((expected.option_kind != FixedOptionKind::EldritchSideIntent && !protected_scour && !temporary) ||
            (binding.intent != FinderProgramIntent::ExactOperator && !temporary && !missing_eldritch))
            throw StrategyEvalUnsupported("unsupported native programme occurrence");
        admission_.cheap_programs_only = protected_scour;
        admission_.query = expected.option_kind == FixedOptionKind::EldritchSideIntent
            ? eldritch_admission_query(expected.intended_side,
                problem_.registry().actions.at(expected.primitive_program.back()).params.type,
                expected.primitive_program.size() == 1)
            : AutomaticAdmissionQuery::Unrestricted;
        if (state_ == kNoId) {
            state_ = calc_->intern_item(entry.item);
            pc_item_state reproduced;
            if (!calc_->materialize(state_, reproduced) ||
                exact_item_state_key(reproduced) !=
                    exact_item_state_key(entry.item))
                throw StrategyEvalUnsupported(
                    "native programme exact entry cannot be rematerialized");
        }
        StateLocalAutomaticBatch batch;
        if (!calc_->advance_state_local_automatic_candidates(
                state_, admission_, batch, 1)) continue;
        if (batch.status != StateLocalAutomaticBatchStatus::Complete)
            throw std::length_error(
                "native programme admission resource deferred");
        bool admitted = false;
        bool preserves_held = false;
        bool matched_semantic = false;
        std::string admission_detail;
        for (const std::uint32_t index : batch.admitted_operators) {
            const PlannerOperator& candidate =
                calc_->operators().at(index);
            // Admission rederives the full intent from this exact carrier and
            // the original goal. Only these native state-dependent intent
            // fields vary; the complete programme, resources and other
            // semantic fields remain bound to the source occurrence.
            PlannerOperator reached = expected;
            if (missing_eldritch || temporary)
                reached.relevant_goal_mask = candidate.relevant_goal_mask;
            if (temporary) {
                reached.exit_goal_slots = candidate.exit_goal_slots;
                reached.exit_min_satisfied = candidate.exit_min_satisfied;
            }
            const auto native_key = planner_operator_semantic_key(candidate);
            const auto bound_key = planner_operator_semantic_key(reached);
            if (native_key != bound_key) {
                if (temporary && candidate.option_kind == FixedOptionKind::TemporaryBenchRepeat &&
                    admission_detail.size() < 512) {
                    const auto mismatch = std::mismatch(native_key.begin(), native_key.end(),
                        bound_key.begin(), bound_key.end());
                    admission_detail += ":native=" + candidate.id + ":semantic_word=" +
                        std::to_string(mismatch.first - native_key.begin());
                    if (mismatch.first != native_key.end() && mismatch.second != bound_key.end())
                        admission_detail += ":native_word=" + std::to_string(*mismatch.first) +
                            ":bound_word=" + std::to_string(*mismatch.second);
                }
                continue;
            }
            if (temporary && !finder_program_is_single_temporary_attempt(*calc_, state_, index)) {
                const auto* native_kernel = calc_->cached_option_kernel(state_, index);
                if (native_kernel) admission_detail += ":finite_attempt_refused:actions_bits=" +
                    std::to_string(std::bit_cast<std::uint64_t>(native_kernel->expected_primitive_actions)) +
                    ":retries=" + std::to_string(native_kernel->retry_states.size()) +
                    ":resources_match=" + std::to_string(native_kernel->expected_resources == candidate.resource_quantities);
                continue;
            }
            matched_semantic = true;
            const OptionKernel& kernel = calc_->option_kernel(
                state_, index);
            admitted = kernel.supported && kernel.legal &&
                kernel.terminates_almost_surely &&
                kernel.automatic.eligible && !kernel.exits.empty() &&
                kernel.expected_resources == expected.resource_quantities;
            if (!admitted)
                admission_detail = std::string("kernel:") +
                    (kernel.supported ? "supported" : "unsupported") + ':' +
                    (kernel.legal ? "legal" : "illegal") + ':' +
                    (kernel.automatic.eligible ? "eligible" : "ineligible");
            if (admitted) {
                preserves_held = (satisfied_goal_mask(
                    calc_->state(state_)) &
                    binding.held_goal_mask) == binding.held_goal_mask &&
                    std::all_of(kernel.exits.begin(), kernel.exits.end(),
                        [&](const OutcomeEntry& exit) {
                            return (satisfied_goal_mask(
                                calc_->state(exit.state)) &
                                binding.held_goal_mask) ==
                                binding.held_goal_mask;
                        });
            }
            break;
        }
        if (!admitted || !preserves_held) {
            if (temporary && !matched_semantic) {
                for (const auto& decision : batch.decisions) {
                    if (decision.kind != AutomaticCandidateKind::TemporaryBenchBlocker &&
                        decision.kind != AutomaticCandidateKind::CannotRoll) continue;
                    if (admission_detail.size() >= 1024) break;
                    admission_detail += ":" + decision.id + ":" + decision.evidence.reason;
                }
            }
            const AbstractState& observed = calc_->state(state_);
            throw StrategyEvalUnsupported(
                std::string("native programme reached entry ") +
                (admitted ? "loses held goals" :
                    matched_semantic ? "unadmitted" :
                        "semantic mismatch") +
                ":node=" + entry.compiled_node_id +
                ":prefixes=" +
                    std::to_string(observed.prefix_count) +
                ":suffixes=" +
                    std::to_string(observed.suffix_count) +
                ":goal_mask=" + std::to_string(
                    satisfied_goal_mask(observed)) +
                ":tiers=" + std::to_string(
                    observed.searing_exarch_tier) + ',' +
                    std::to_string(observed.eater_of_worlds_tier) +
                (admission_detail.empty() ? std::string{} :
                    ":" + admission_detail));
        }
        ++cursor_;
        state_ = kNoId;
    }
    return done();
}

} // namespace poecraft::solver
