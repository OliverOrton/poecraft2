#include "solver_selective_completion.hpp"
#include "solver_options_helpers.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace poecraft::solver {
namespace {

std::uint32_t packed_tiers(const AbstractState& state) {
    return static_cast<std::uint32_t>(state.searing_exarch_tier) |
        (static_cast<std::uint32_t>(state.eater_of_worlds_tier) << 8u);
}

} // namespace

SelectiveCompletionProducer::SelectiveCompletionProducer(
    CalcContext& problem, const pc_item_state& original_start,
    const std::unordered_map<std::string, double>& prices,
    const SolveOptions& limits, const SelectiveCompletionVariant variant)
    : problem_(problem), original_start_(original_start), prices_(prices),
      limits_(limits), variant_(variant) {}

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

void SelectiveCompletionProducer::begin() {
    if (!problem_.goal().automatic_candidates ||
        (variant_ != SelectiveCompletionVariant::ProtectedScour &&
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
    if (variant_ == SelectiveCompletionVariant::RerollVersusRepair &&
        (side_slots_[held_side_].size() != 3 ||
         side_slots_[target_side_].empty())) {
        refuse("family_requires_full_three_goal_held_side");
        return;
    }
    for (const std::uint32_t slot : side_slots_[held_side_])
        held_mask_ |= 1u << slot;

    const std::uint32_t root = problem_.intern_item(original_start_);
    for (const std::uint32_t index : problem_.candidates()) {
        if (index >= problem_.registry().actions.size()) continue;
        const ActionDescriptor& action = problem_.registry().actions[index];
        if (action.synthetic || action.uses_companion_state ||
            action.params.type != ActionType::Chaos ||
            !action_legal(problem_.session(), action,
                problem_.state(root))) continue;
        double price = 0.0;
        bool complete = true;
        for (const std::string& key : action.cost_keys) {
            const auto it = prices_.find(key);
            if (it == prices_.end() || !std::isfinite(it->second) ||
                it->second < 0.0) { complete = false; break; }
            price += it->second;
        }
        if (complete && std::isfinite(price) &&
            (acquisition_action_ == kNoId ||
             price < acquisition_price_)) {
            acquisition_action_ = index;
            acquisition_price_ = price;
        }
    }
    if (acquisition_action_ == kNoId) {
        refuse("no_priced_root_acquisition");
        return;
    }
    const OutcomeDistribution& acquisition =
        problem_.outcomes(root, acquisition_action_);
    if (!acquisition.supported || !acquisition.applicable ||
        !acquisition.choice_groups.empty()) {
        refuse("root_acquisition_law_unavailable");
        return;
    }

    if (variant_ == SelectiveCompletionVariant::ProtectedScour) {
        primary_.intended = ActionType::Scour;
    } else primary_.intended = variant_ ==
            SelectiveCompletionVariant::RetentionControl
        ? (side_slots_[target_side_].empty()
            ? ActionType::EldritchAnnul : ActionType::EldritchChaos)
        : ActionType::EldritchChaos;
    secondary_.intended = ActionType::EldritchAnnul;
    const bool repair = variant_ ==
        SelectiveCompletionVariant::RerollVersusRepair;
    std::uint32_t primary_rank = 0;
    std::uint32_t secondary_rank = 0;
    for (const OutcomeEntry& exit : acquisition.entries) {
        if (!(exit.probability > 0.0)) continue;
        const AbstractState& state = problem_.state(exit.state);
        const std::uint32_t count = target_side_ == PC_SIDE_PREFIX
            ? state.prefix_count : state.suffix_count;
        const std::uint32_t held_count = held_side_ == PC_SIDE_PREFIX
            ? state.prefix_count : state.suffix_count;
        if (variant_ == SelectiveCompletionVariant::ProtectedScour &&
            (held_count != side_slots_[held_side_].size() || count >= 3)) continue;
        if ((satisfied_goal_mask(state) & held_mask_) != held_mask_ ||
            count == 0 || problem_.is_goal_state(state)) continue;
        pc_item_state exact;
        if (!problem_.materialize(exit.state, exact)) continue;
        const std::uint32_t rank =
            variant_ == SelectiveCompletionVariant::RetentionControl
                ? (count == std::max<std::uint32_t>(
                    1, side_slots_[target_side_].size()) ? 2u : 1u)
                : (count == 1 ? 2u : count == 2 ? 1u : 0u);
        if (rank > primary_rank) {
            primary_.source = exit.state;
            primary_rank = rank;
        }
        if (repair) {
            const std::uint32_t repair_rank = count == 3 ? 2u : 1u;
            if (repair_rank > secondary_rank) {
                secondary_.source = exit.state;
                secondary_rank = repair_rank;
            }
        }
    }
    if (primary_.source == kNoId ||
        (repair && secondary_.source == kNoId)) {
        refuse("no_reached_materializable_held_context");
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
    admission_.cheap_programs_only = variant_ == SelectiveCompletionVariant::ProtectedScour;
    phase_ = Phase::Primary;
}

bool SelectiveCompletionProducer::advance_programme(
    Programme& programme, const bool direct,
    const std::uint32_t max_work_items) {
    const std::uint32_t state = direct
        ? programme.ready : programme.source;
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
    for (const std::uint32_t index : batch.admitted_operators) {
        const PlannerOperator& option = problem_.operators().at(index);
        const bool protected_scour = variant_ == SelectiveCompletionVariant::ProtectedScour;
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
        std::string detail;
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
    if (variant_ == SelectiveCompletionVariant::ProtectedScour) {
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
        graph.programs.push_back({p.initial, p.source, held_mask_});
        if (p.ready != p.source)
            graph.programs.push_back({p.direct, p.ready, held_mask_});
        return first;
    };
    const std::uint32_t primary_binding = bind(primary_);
    if (variant_ == SelectiveCompletionVariant::ProtectedScour) {
        const auto goal = append(FinderControlKind::TestGoal);
        const auto acquire = append(FinderControlKind::RunPrimitive, acquisition_action_);
        const auto success = append(FinderControlKind::GoalTerminal);
        const auto programme = append(FinderControlKind::RunNativeProgram, primary_binding);
        const auto held_junk = append(FinderControlKind::TestSideCountAtLeast,
            (held_side_ << 8u) | (side_slots_[held_side_].size() + 1));
        const auto full_craft_side = append(FinderControlKind::TestSideCountAtLeast,
            (target_side_ << 8u) | 3u);
        graph.nodes[goal].on_true = success;
        graph.nodes[goal].on_false = held_junk;
        graph.nodes[held_junk].on_true = acquire;
        graph.nodes[held_junk].on_false = full_craft_side;
        graph.nodes[full_craft_side].on_true = acquire;
        std::uint32_t previous = full_craft_side;
        for (auto slot : side_slots_[held_side_]) {
            const auto test = append(FinderControlKind::TestSlot,slot);
            if (previous == full_craft_side) graph.nodes[previous].on_false = test;
            else graph.nodes[previous].on_true = test;
            graph.nodes[test].on_false = acquire;
            previous = test;
        }
        graph.nodes[previous].on_true = programme;
        graph.nodes[programme].next = goal;
        graph.nodes[acquire].next = goal;
        candidate_ = SelectiveCompletionCandidate{std::move(graph), acquisition_action_, acquisition_price_, variant_};
        status_ = "complete";
        phase_ = Phase::Done;
        return;
    }
    const bool repair = variant_ ==
        SelectiveCompletionVariant::RerollVersusRepair;
    const std::uint32_t secondary_binding = repair
        ? bind(secondary_) : kNoId;
    const std::uint32_t goal = append(FinderControlKind::TestGoal);
    std::vector<std::uint32_t> held_tests;
    for (const std::uint32_t slot : side_slots_[held_side_])
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
    const std::uint32_t occupied_test = append(
        FinderControlKind::TestSideCountAtLeast,
        (target_side_ << 8u) |
            (variant_ == SelectiveCompletionVariant::RetentionControl
                ? std::max<std::uint32_t>(
                    1, side_slots_[target_side_].size()) : 1u));
    const std::uint32_t acquire = append(
        FinderControlKind::RunPrimitive, acquisition_action_);
    const std::uint32_t success = append(FinderControlKind::GoalTerminal);
    graph.nodes[goal].on_true = success;
    graph.nodes[goal].on_false = held_tests.front();
    for (std::size_t i = 0; i < held_tests.size(); ++i) {
        graph.nodes[held_tests[i]].on_true = i + 1 < held_tests.size()
            ? held_tests[i + 1] :
                (repair_test == kNoId ? occupied_test : repair_test);
        graph.nodes[held_tests[i]].on_false = acquire;
    }
    graph.nodes[acquire].next = goal;
    if (repair_test != kNoId)
        graph.nodes[repair_test].on_false = occupied_test;
    graph.nodes[occupied_test].on_false = acquire;
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
    if (variant_ == SelectiveCompletionVariant::RetentionControl) {
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
                    (variant_ == SelectiveCompletionVariant::RerollVersusRepair
                        ? Phase::Secondary : Phase::Build);
            break;
        case Phase::PrimaryDirect:
            if (advance_programme(primary_, true, max_work_items) &&
                !done()) phase_ =
                    (variant_ == SelectiveCompletionVariant::RerollVersusRepair)
                        ? Phase::Secondary : Phase::Build;
            break;
        case Phase::Secondary:
            if (advance_programme(secondary_, false, max_work_items) &&
                !done()) phase_ = secondary_.direct == kNoId
                    ? Phase::SecondaryDirect : Phase::Build;
            break;
        case Phase::SecondaryDirect:
            if (advance_programme(secondary_, true, max_work_items) &&
                !done()) phase_ = Phase::Build;
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
        const bool protected_scour = expected.option_kind == FixedOptionKind::ProtectedSide &&
            expected.followup_action != kNoId &&
            problem_.registry().actions.at(expected.followup_action).params.type == ActionType::Scour;
        if (expected.option_kind != FixedOptionKind::EldritchSideIntent && !protected_scour)
            throw StrategyEvalUnsupported("unsupported native programme occurrence");
        admission_.cheap_programs_only = protected_scour;
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
        const auto expected_key = planner_operator_semantic_key(expected);
        bool admitted = false;
        bool preserves_held = false;
        bool matched_semantic = false;
        std::string admission_detail;
        for (const std::uint32_t index : batch.admitted_operators) {
            const PlannerOperator& candidate =
                calc_->operators().at(index);
            if (planner_operator_semantic_key(candidate) != expected_key)
                continue;
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
