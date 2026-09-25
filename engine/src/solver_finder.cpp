#include "solver_finder.hpp"

#include "json.hpp"
#include "solver_policy_refinement_helpers.hpp"
#include "solver_options_helpers.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <string_view>
#include <stdexcept>
#include <unordered_set>

namespace poecraft::solver {
namespace {

bool same_json(const json::Value& left, const json::Value& right) {
    if (left.type != right.type) return false;
    switch (left.type) {
    case json::Type::Null: return true;
    case json::Type::Bool: return left.boolean == right.boolean;
    case json::Type::Number: return left.number == right.number;
    case json::Type::String: return left.string == right.string;
    case json::Type::Array:
        if (left.array.size() != right.array.size()) return false;
        for (std::size_t i = 0; i < left.array.size(); ++i)
            if (!same_json(left.array[i], right.array[i])) return false;
        return true;
    case json::Type::Object:
        if (left.object.size() != right.object.size()) return false;
        for (std::size_t i = 0; i < left.object.size(); ++i)
            if (left.object[i].first != right.object[i].first ||
                !same_json(left.object[i].second, right.object[i].second))
                return false;
        return true;
    }
    return false;
}

std::string text_member(const json::Value& value, const char* key) {
    const json::Value* member = value.find(key);
    return member != nullptr && member->type == json::Type::String
        ? member->string : std::string{};
}

std::string json_string(const std::string& value) {
    constexpr char hex[] = "0123456789abcdef";
    std::string out = "\"";
    for (const unsigned char c : value) {
        if (c == '"' || c == '\\') {
            out += '\\';
            out += static_cast<char>(c);
        } else if (c < 0x20) {
            out += "\\u00";
            out += hex[c >> 4];
            out += hex[c & 15];
        } else {
            out += static_cast<char>(c);
        }
    }
    out += '"';
    return out;
}

std::string stable_finder_hash(const std::string_view value) {
    std::uint64_t hash = 14695981039346656037ull;
    for (const unsigned char c : value) {
        hash ^= c;
        hash *= 1099511628211ull;
    }
    return std::to_string(hash);
}

} // namespace

FinderCandidatePreparation prepare_finder_candidate(
    const CalcContext& problem,
    std::shared_ptr<const SessionImpl> session,
    const pc_item_state& original_start,
    const std::string& strategy_json,
    const FinderControlGraph* native_control) {
    FinderCandidatePreparation prepared;
    try {
        if (session == nullptr || strategy_json.empty()) {
            prepared.refusal = "finder candidate has no session or graph";
            return prepared;
        }
        prepared.strategy = compile_strategy_json(
            std::move(session), strategy_json.data(), strategy_json.size());
        if (exact_item_state_key(prepared.strategy->start_item) !=
            exact_item_state_key(original_start)) {
            prepared.refusal = "finder candidate changes the original start item";
            prepared.strategy.reset();
            return prepared;
        }
        std::unordered_map<std::string, std::uint32_t> trusted_steps;
        if (native_control != nullptr) {
            // Only a compiler-owned control object, passed in the same
            // native call as its graph, can authorize dependency steps. A
            // declaration inside ordinary JSON has no such authority.
            if (compile_finder_control_json(problem, original_start,
                    *native_control, SolveOptions{}) != strategy_json) {
                prepared.refusal = "finder native occurrence does not match graph bytes";
                prepared.strategy.reset();
                return prepared;
            }
            for (std::size_t i = 0; i < native_control->nodes.size(); ++i) {
                const FinderControlNode& node = native_control->nodes[i];
                if (node.kind != FinderControlKind::RunNativeProgram)
                    continue;
                const FinderProgramBinding& binding =
                    native_control->programs.at(node.binding);
                const PlannerOperator& option =
                    problem.operators().at(binding.operator_index);
                for (std::size_t step = 0;
                     step < option.primitive_program.size(); ++step) {
                    trusted_steps.emplace("c" + std::to_string(i) +
                        (step == 0 ? std::string{} :
                            "_o" + std::to_string(step)),
                        option.primitive_program[step]);
                }
            }
        }
        const json::Value graph = json::Parser(
            strategy_json.data(), strategy_json.size()).parse();
        const std::string goal_text = compile_finder_goal_condition(problem);
        const json::Value trusted_goal = json::Parser(
            goal_text.data(), goal_text.size()).parse();
        std::unordered_set<std::string> successes;
        for (const json::Value& node : graph.at("nodes").as_array()) {
            if (text_member(node, "kind") == "terminal" &&
                text_member(node, "terminal") == "success") {
                successes.insert(text_member(node, "id"));
            }
        }
        std::size_t guarded_ingress = 0;
        for (const json::Value& edge : graph.at("edges").as_array()) {
            if (!successes.contains(text_member(edge, "to"))) continue;
            const json::Value* is_default = edge.find("is_default");
            const json::Value* condition = edge.find("condition");
            // The ordinary compiler replaces a default edge's authored
            // condition with Always. Raw goal decoration is not an executable
            // guard, even if it is byte-for-byte the requested predicate.
            if ((is_default != nullptr && is_default->type == json::Type::Bool &&
                 is_default->boolean) || condition == nullptr ||
                !same_json(*condition, trusted_goal)) {
                prepared.refusal =
                    "finder success ingress is not the original native goal";
                prepared.strategy.reset();
                return prepared;
            }
            ++guarded_ingress;
        }
        if (guarded_ingress == 0) {
            prepared.refusal = "finder candidate has no guarded success ingress";
            prepared.strategy.reset();
            return prepared;
        }
        for (const StrategyNode& node : prepared.strategy->nodes) {
            if (node.kind != StrategyNodeKind::Operation) continue;
            const std::uint32_t action = resolve_strategy_action(
                node, problem.registry());
            const bool standalone = action != kNoId &&
                std::find(problem.candidates().begin(),
                          problem.candidates().end(), action) !=
                    problem.candidates().end();
            const auto occurrence = trusted_steps.find(node.id);
            const bool bound_dependency = occurrence != trusted_steps.end() &&
                occurrence->second == action;
            if (!standalone && !bound_dependency) {
                prepared.refusal =
                    "finder operation is outside the requested action scope";
                prepared.strategy.reset();
                return prepared;
            }
        }
    } catch (const std::exception& ex) {
        prepared.refusal = ex.what();
        prepared.strategy.reset();
    }
    return prepared;
}

bool finder_evaluation_accepted(const StrategyEvalResult& result) {
    constexpr double tolerance = 1e-9;
    return result.converged && result.cost_complete &&
           std::isfinite(result.total_expected_cost) &&
           result.total_expected_cost >= 0.0 &&
           result.success_probability >= 1.0 - tolerance &&
           result.failure_probability <= tolerance &&
           result.stop_probability <= tolerance &&
           result.action_not_applied_probability <= tolerance &&
           result.no_matching_edge_probability <= tolerance &&
           result.unresolved_probability <= tolerance;
}

std::vector<double> score_finder_sketch_batch(
    const std::vector<FinderScoreFeatures>& features) {
    std::vector<double> scores;
    scores.reserve(features.size());
    for (const FinderScoreFeatures& feature : features) {
        double score = feature.first_price + feature.second_price +
            0.01 * feature.goal_slots;
        if (feature.recovery && feature.goal_slots >= 3) score -= 0.5;
        if (feature.renewal) score -= 0.1;
        scores.push_back(std::isfinite(score)
            ? score : std::numeric_limits<double>::max());
    }
    return scores;
}

PolicyFinderWork::PolicyFinderWork(
    CalcContext& problem,
    std::shared_ptr<const SessionImpl> session,
    const pc_item_state& original_start,
    std::unordered_map<std::string, double> prices,
    const SolveOptions& limits,
    const FinderRankingMode ranking,
    const FinderGrammarMode grammar)
    : problem_(problem), session_(std::move(session)),
      original_start_(original_start),
      economy_(std::make_shared<EconomyImpl>()), limits_(limits),
      ranking_(ranking), grammar_(grammar) {
    if (session_ == nullptr) {
        throw std::invalid_argument("finder requires a session");
    }
    economy_->id = "finder-request";
    economy_->prices = std::move(prices);
    std::string request_key = compile_finder_goal_condition(problem_);
    const auto start_key = exact_item_state_key(original_start_);
    request_key.append(
        reinterpret_cast<const char*>(start_key.data()),
        start_key.size() * sizeof(start_key.front()));
    for (const std::uint32_t action : problem_.candidates())
        request_key += ':' + problem_.registry().actions.at(action).id;
    problem_identity_ = stable_finder_hash(request_key);

    const auto search_started = std::chrono::steady_clock::now();
    const std::uint32_t start_state = problem_.intern_item(original_start_);
    if (problem_.is_goal_state(problem_.state(start_state))) {
        // A completed request still needs an ordinary guarded, independently
        // checked artifact. It needs no priced operation or legacy solve.
        Sketch completed{{}, 0.0};
        seen_.insert(sketch_identity(completed));
        record_generated(completed);
        frontier_.push_back(std::move(completed));
        ++counters_.generated;
        counters_.search_ns = static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - search_started).count());
        update_peak();
        return;
    }
    retention_pending_ = grammar_ == FinderGrammarMode::ConditionalRetention;
    if (retention_pending_) retention_status_ = "pending";
    for (const std::uint32_t index : problem_.candidates()) {
        if (index >= problem_.registry().actions.size()) continue;
        const ActionDescriptor& action = problem_.registry().actions[index];
        if (action.synthetic || action.uses_companion_state) continue;
        const bool root_legal =
            action_legal(*session_, action, problem_.state(start_state));
        double cost = 0.0;
        bool priced = true;
        for (const std::string& key : action.cost_keys) {
            const auto it = economy_->prices.find(key);
            if (it == economy_->prices.end() ||
                !std::isfinite(it->second) || it->second < 0.0) {
                priced = false;
                break;
            }
            cost += it->second;
        }
        if (priced && std::isfinite(cost))
            ranked_.push_back({index, cost, root_legal});
    }
    std::stable_sort(ranked_.begin(), ranked_.end(),
        [&](const RankedAction& left, const RankedAction& right) {
            if (ranking_ == FinderRankingMode::Heuristic &&
                left.price != right.price) return left.price < right.price;
            return problem_.registry().actions[left.index].id <
                problem_.registry().actions[right.index].id;
        });
    std::unordered_set<std::uint32_t> selected;
    const auto add_single = [&](const RankedAction& action) {
        if (selected.insert(action.index).second) {
            Sketch candidate{{action.index}, action.price};
            seen_.insert(sketch_identity(candidate));
            record_generated(candidate);
            frontier_.push_back(std::move(candidate));
            ++counters_.generated;
        }
    };
    /* A renewal seed is useful even when a cheaper one-shot descriptor is
     * ranked first. Selection is by native descriptor identity, not a saved
     * strategy or a case name. */
    for (const RankedAction& action : ranked_)
        if (action.root_legal &&
            problem_.registry().actions[action.index].params.type ==
                ActionType::Chaos) add_single(action);
    for (const RankedAction& action : ranked_) {
        if (frontier_.size() >= 4) break;
        if (action.root_legal) add_single(action);
    }
    for (const RankedAction& first : ranked_) {
        if (!first.root_legal ||
            problem_.registry().actions[first.index].params.type !=
                ActionType::Chaos) continue;
        const Sketch seed{{first.index}, first.price};
        const std::string seed_id = sketch_identity(seed);
        pending_.push_back({first.index, first.price,
            HoleKind::Recovery, seed_id});
        if (grammar_ != FinderGrammarMode::PrimitiveOnly &&
            problem_.goal().slots.size() >= 2)
            pending_.push_back({first.index, first.price,
                HoleKind::Progress, seed_id});
        break;
    }
    /* These are unresolved second-stage holes, not checkable graphs. The
     * finite beam expands them only after servicing complete root seeds. */
    for (const RankedAction& setup : ranked_) {
        if (pending_.size() >= 16) break;
        if (!setup.root_legal) continue;
        const ActionType type =
            problem_.registry().actions[setup.index].params.type;
        const pc_rarity reached_rarity = type == ActionType::Transmute
            ? PC_RARITY_MAGIC
            : type == ActionType::Alchemy || type == ActionType::Regal
                ? PC_RARITY_RARE : PC_RARITY_NORMAL;
        if (reached_rarity != problem_.goal().rarity ||
            reached_rarity == PC_RARITY_NORMAL) continue;
        const Sketch seed{{setup.index}, setup.price};
        pending_.push_back({setup.index, setup.price,
            HoleKind::Renewal, sketch_identity(seed)});
    }
    counters_.search_ns = static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - search_started).count());
    update_peak();
}

PolicyFinderWork::~PolicyFinderWork() = default;

std::uint64_t PolicyFinderWork::retained_owned_bytes() const {
    std::uint64_t bytes = sizeof(*this) + 4096 +
        problem_.estimated_owned_bytes() +
        (validation_calc_ == nullptr ? 0 :
            validation_calc_->estimated_owned_bytes()) +
        refinement::economy_owned_bytes(economy_->prices, economy_->id.capacity()) +
        ranked_.capacity() * sizeof(RankedAction) +
        frontier_.size() * sizeof(Sketch) +
        pending_.capacity() * sizeof(PartialSketch) +
        candidate_records_.capacity() * sizeof(CandidateRecord) +
        seen_.bucket_count() * sizeof(void*) +
        problem_identity_.capacity() +
        retention_status_.capacity() +
        checking_graph_.capacity() + last_refusal_.capacity() +
        last_refusal_kind_.capacity();
    for (const Sketch& sketch : frontier_) {
        bytes += sketch.actions.capacity() * sizeof(std::uint32_t);
        bytes += sketch.parent_identity.capacity();
        if (sketch.control.has_value())
            bytes += sketch.control->nodes.capacity() *
                sizeof(FinderControlNode) +
                sketch.control->programs.capacity() *
                sizeof(FinderProgramBinding);
    }
    if (active_sketch_.has_value()) {
        bytes += active_sketch_->actions.capacity() * sizeof(std::uint32_t);
        bytes += active_sketch_->parent_identity.capacity();
        if (active_sketch_->control.has_value())
            bytes += active_sketch_->control->nodes.capacity() *
                sizeof(FinderControlNode) +
                active_sketch_->control->programs.capacity() *
                sizeof(FinderProgramBinding);
    }
    for (const std::string& identity : seen_)
        bytes += sizeof(std::string) + identity.capacity() + 32;
    for (const PartialSketch& partial : pending_)
        bytes += partial.parent_identity.capacity();
    for (const CandidateRecord& record : candidate_records_) {
        bytes += record.identity.capacity() +
            record.parent_identity.capacity() + record.status.capacity() +
            record.refusal.capacity() + record.graph_hash.capacity() +
            record.actions.capacity() * sizeof(std::uint32_t);
    }
    if (checking_strategy_ != nullptr)
        bytes += refinement::strategy_impl_owned_bytes(*checking_strategy_);
    if (best_.has_value()) {
        bytes += best_->strategy_json.capacity();
        if (best_->native_control.has_value())
            bytes += best_->native_control->nodes.capacity() *
                    sizeof(FinderControlNode) +
                best_->native_control->programs.capacity() *
                    sizeof(FinderProgramBinding);
    }
    return bytes;
}

void PolicyFinderWork::release_validation() {
    if (validation_calc_ != nullptr) {
        const std::uint64_t now =
            validation_calc_->telemetry().reforge_logical_work_v1;
        const std::uint64_t delta = now >= validation_reforge_accounted_
            ? now - validation_reforge_accounted_ : 0;
        counters_.logical_reforge_work += std::min(
            delta, limits_.max_reforge_work -
                counters_.logical_reforge_work);
    }
    validation_calc_.reset();
    validation_cursor_ = 0;
    validation_state_ = kNoId;
    validation_reforge_accounted_ = 0;
    validation_limits_ = {};
}

bool PolicyFinderWork::validate_active_programme(
        const std::uint32_t max_work_items) {
    if (!active_sketch_.has_value() ||
        !active_sketch_->control.has_value() ||
        active_sketch_->control->programs.empty())
        return true;
    const FinderControlGraph& control = *active_sketch_->control;
    const StrategyPolicyEntryCertificate& census =
        checker_->result().policy_entries;
    if (!census.requested || census.entries.empty() ||
        census.reached_decisions == 0 || census.refused_entries != 0)
        throw StrategyEvalUnsupported(
            "finder programme has no complete reached entry census");
    if (validation_calc_ == nullptr) {
        CandidateRecord& record = candidate_records_.at(*active_record_);
        record.programme_entries = static_cast<std::uint32_t>(
            census.entries.size());
        for (const auto& entry : census.entries)
            record.positive_programme_entries +=
                entry.root_expected_visits > 0.0;
        // Modifier identity is retained in this private admission context.
        // The ordinary finder projection may merge members, so its one
        // materialized representative cannot authorize every reached item.
        validation_calc_ = std::make_unique<CalcContext>(
            session_, problem_.goal(), problem_.registry(),
            problem_.candidates(), false, false, false,
            std::nullopt, std::vector<CountObservation>{}, false,
            std::vector<std::uint64_t>{}, true);
        const std::uint64_t other = retained_owned_bytes() -
            validation_calc_->estimated_owned_bytes() +
            checker_->live_owned_bytes();
        if (other >= limits_.max_solver_owned_bytes)
            throw std::length_error(
                "finder has no exact programme admission memory");
        validation_limits_.max_solver_owned_bytes =
            limits_.max_solver_owned_bytes - other;
        validation_limits_.max_state_action_rows =
            limits_.max_state_action_rows;
        validation_limits_.max_transitions = limits_.max_transitions;
        validation_limits_.max_imprint_program_depth =
            limits_.max_imprint_program_depth;
        validation_limits_.max_imprint_program_work =
            limits_.max_imprint_program_work;
        validation_limits_.consider_imprint_programs =
            limits_.consider_imprint_programs;
        validation_limits_.prices = &economy_->prices;
        update_peak();
    }
    const std::uint32_t budget = std::max<std::uint32_t>(
        1, max_work_items);
    for (std::uint32_t item = 0; item < budget &&
         validation_cursor_ < census.entries.size(); ++item) {
        const StrategyPolicyEntryResult& entry =
            census.entries[validation_cursor_];
        if (!entry.available() ||
            !(entry.root_expected_visits > 0.0) ||
            entry.checkpoint_active || entry.observed_offer_active)
            throw StrategyEvalUnsupported(
                "finder programme entry has incomplete exact root coverage");
        const auto bound_node = std::find_if(control.nodes.begin(),
            control.nodes.end(), [&](const FinderControlNode& node) {
                const std::size_t index = &node - control.nodes.data();
                return node.kind == FinderControlKind::RunNativeProgram &&
                    entry.compiled_node_id ==
                        "c" + std::to_string(index);
            });
        if (bound_node == control.nodes.end())
            throw StrategyEvalUnsupported(
                "finder programme entry has no trusted occurrence");
        const FinderProgramBinding& binding =
            control.programs.at(bound_node->binding);
        const PlannerOperator& expected =
            problem_.operators().at(binding.operator_index);
        if (validation_state_ == kNoId) {
            validation_state_ = validation_calc_->intern_item(entry.item);
            pc_item_state reproduced;
            if (!validation_calc_->materialize(
                    validation_state_, reproduced) ||
                exact_item_state_key(reproduced) !=
                    exact_item_state_key(entry.item))
                throw StrategyEvalUnsupported(
                    "finder programme exact entry cannot be rematerialized");
        }
        StateLocalAutomaticBatch batch;
        if (!validation_calc_->advance_state_local_automatic_candidates(
                validation_state_, validation_limits_, batch, 1)) {
            update_peak();
            continue;
        }
        if (batch.status != StateLocalAutomaticBatchStatus::Complete)
            throw std::length_error(
                "finder programme admission resource deferred");
        const auto expected_key = planner_operator_semantic_key(expected);
        bool admitted = false;
        bool preserves_held = false;
        for (const std::uint32_t index : batch.admitted_operators) {
            const PlannerOperator& candidate =
                validation_calc_->operators().at(index);
            if (planner_operator_semantic_key(candidate) != expected_key)
                continue;
            const OptionKernel& kernel = validation_calc_->option_kernel(
                validation_state_, index);
            admitted = kernel.supported && kernel.legal &&
                kernel.terminates_almost_surely &&
                kernel.automatic.eligible && !kernel.exits.empty() &&
                kernel.expected_resources == expected.resource_quantities;
            if (admitted) {
                preserves_held = (satisfied_goal_mask(
                    validation_calc_->state(validation_state_)) &
                    binding.held_goal_mask) == binding.held_goal_mask &&
                    std::all_of(kernel.exits.begin(), kernel.exits.end(),
                        [&](const OutcomeEntry& exit) {
                            return (satisfied_goal_mask(
                                validation_calc_->state(exit.state)) &
                                binding.held_goal_mask) ==
                                binding.held_goal_mask;
                        });
            }
            break;
        }
        if (!admitted || !preserves_held)
            throw StrategyEvalUnsupported(
                "finder programme is unadmitted or loses held goals at a reached exact item");
        ++candidate_records_.at(*active_record_)
            .validated_programme_entries;
        ++validation_cursor_;
        validation_state_ = kNoId;
        const std::uint64_t now =
            validation_calc_->telemetry().reforge_logical_work_v1;
        const std::uint64_t delta = now >= validation_reforge_accounted_
            ? now - validation_reforge_accounted_ : 0;
        counters_.logical_reforge_work += std::min(
            delta, limits_.max_reforge_work -
                counters_.logical_reforge_work);
        validation_reforge_accounted_ = now;
        update_peak();
    }
    if (validation_cursor_ < census.entries.size()) return false;
    release_validation();
    return true;
}

bool PolicyFinderWork::exhausted() const {
    return frontier_.empty() && !retention_pending_ &&
        pending_cursor_ >= pending_.size();
}

std::string PolicyFinderWork::sketch_identity(const Sketch& sketch) const {
    std::string key = sketch.return_to_first ? "return:1" : "return:0";
    for (const std::uint32_t action : sketch.actions)
        key += ':' + std::to_string(action);
    if (sketch.control.has_value()) {
        key += ":control:" + std::to_string(sketch.control->entry);
        for (const FinderProgramBinding& binding : sketch.control->programs)
            key += ":program:" + std::to_string(binding.operator_index) +
                ':' + std::to_string(binding.admitted_state) +
                ':' + std::to_string(binding.held_goal_mask);
        for (const FinderControlNode& node : sketch.control->nodes) {
            key += ':' + std::to_string(static_cast<unsigned>(node.kind)) +
                ',' + std::to_string(node.binding) +
                ',' + std::to_string(node.on_true) +
                ',' + std::to_string(node.on_false) +
                ',' + std::to_string(node.next);
        }
    }
    return key;
}

std::uint64_t PolicyFinderWork::elapsed_ns() const {
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - born_).count());
}

void PolicyFinderWork::record_generated(const Sketch& sketch) {
    CandidateRecord record;
    record.identity = sketch_identity(sketch);
    record.parent_identity = sketch.parent_identity;
    record.actions = sketch.actions;
    record.score = sketch.score;
    record.generated_ns = elapsed_ns();
    record.conditional = sketch.control.has_value();
    if (sketch.control.has_value())
        record.native_program = std::any_of(
            sketch.control->nodes.begin(), sketch.control->nodes.end(),
            [](const FinderControlNode& node) {
                return node.kind == FinderControlKind::RunScourAlchemy ||
                    node.kind == FinderControlKind::RunNativeProgram;
            });
    candidate_records_.push_back(std::move(record));
}

void PolicyFinderWork::schedule_feedback_program() {
    if (!active_sketch_.has_value() || !active_sketch_->feedback_parent ||
        finish_requested_ || active_sketch_->actions.size() < 2)
        return;
    const auto first = std::find_if(ranked_.begin(), ranked_.end(),
        [&](const RankedAction& action) {
            return action.index == active_sketch_->actions[0];
        });
    if (first != ranked_.end())
        pending_.push_back({first->index, first->price,
            HoleKind::ProgressProgram,
            sketch_identity(*active_sketch_)});
}

void PolicyFinderWork::generate_retention_candidate() {
    retention_pending_ = false;
    const auto started = std::chrono::steady_clock::now();
    const std::uint64_t work_before =
        problem_.telemetry().reforge_logical_work_v1;
    const auto finish = [&] {
        const std::uint64_t work_after =
            problem_.telemetry().reforge_logical_work_v1;
        const std::uint64_t delta = work_after >= work_before
            ? work_after - work_before : 0;
        counters_.logical_reforge_work = std::min(
            limits_.max_reforge_work,
            counters_.logical_reforge_work + std::min(
                delta, limits_.max_reforge_work -
                    counters_.logical_reforge_work));
        counters_.search_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started).count());
        try {
            update_peak();
        } catch (const std::length_error& ex) {
            retention_status_ = std::string("native_construction_capacity:") +
                ex.what();
            last_refusal_kind_ = "capacity";
            last_refusal_ = ex.what();
            ++counters_.censored;
            done_ = true;
        }
    };
    try {
        if (!problem_.goal().automatic_candidates ||
            !session_->eldritch_eligible ||
            original_start_.rarity != PC_RARITY_RARE) {
            retention_status_ = "native_family_not_requested_or_ineligible";
            finish();
            return;
        }
        std::array<std::vector<std::uint32_t>, 2> side_slots;
        for (std::uint32_t slot = 0;
             slot < problem_.goal().slots.size(); ++slot) {
            const std::int8_t side = goal_slot_side(
                *session_, problem_.goal().slots[slot]);
            if (side != PC_SIDE_PREFIX && side != PC_SIDE_SUFFIX) {
                retention_status_ = "goal_side_not_pure";
                finish();
                return;
            }
            side_slots[side].push_back(slot);
        }
        const std::uint32_t held_side =
            side_slots[PC_SIDE_PREFIX].size() >=
                    side_slots[PC_SIDE_SUFFIX].size()
                ? PC_SIDE_PREFIX : PC_SIDE_SUFFIX;
        const std::uint32_t target_side = held_side == PC_SIDE_PREFIX
            ? PC_SIDE_SUFFIX : PC_SIDE_PREFIX;
        if (side_slots[held_side].size() < 2) {
            retention_status_ = "held_side_has_fewer_than_two_goals";
            finish();
            return;
        }
        const auto chaos = std::find_if(ranked_.begin(), ranked_.end(),
            [&](const RankedAction& action) {
                return action.root_legal &&
                    problem_.registry().actions[action.index].params.type ==
                        ActionType::Chaos;
            });
        if (chaos == ranked_.end()) {
            retention_status_ = "no_priced_root_acquisition";
            finish();
            return;
        }
        std::uint32_t held_mask = 0;
        for (const std::uint32_t slot : side_slots[held_side])
            held_mask |= 1u << slot;
        const std::uint32_t root = problem_.intern_item(original_start_);
        const OutcomeDistribution& acquisition =
            problem_.outcomes(root, chaos->index);
        if (!acquisition.supported || !acquisition.applicable ||
            !acquisition.choice_groups.empty()) {
            retention_status_ = "root_acquisition_law_unavailable";
            finish();
            return;
        }
        std::uint32_t source = kNoId;
        const std::uint32_t preferred_count =
            std::max<std::uint32_t>(1, side_slots[target_side].size());
        for (const OutcomeEntry& exit : acquisition.entries) {
            if (!(exit.probability > 0.0)) continue;
            const AbstractState& state = problem_.state(exit.state);
            const std::uint32_t count = target_side == PC_SIDE_PREFIX
                ? state.prefix_count : state.suffix_count;
            if ((satisfied_goal_mask(state) & held_mask) != held_mask ||
                count == 0 || problem_.is_goal_state(state))
                continue;
            pc_item_state exact;
            if (!problem_.materialize(exit.state, exact)) continue;
            if (source == kNoId || count == preferred_count) {
                source = exit.state;
                if (count == preferred_count) break;
            }
        }
        if (source == kNoId) {
            retention_status_ = "no_reached_materializable_held_context";
            finish();
            return;
        }
        AutomaticAdmissionLimits admission;
        admission.max_state_action_rows = limits_.max_state_action_rows;
        admission.max_transitions = limits_.max_transitions;
        const std::uint64_t non_calc_owned = retained_owned_bytes() -
            problem_.estimated_owned_bytes();
        if (non_calc_owned >= limits_.max_solver_owned_bytes)
            throw std::length_error(
                "finder has no native programme construction memory");
        admission.max_solver_owned_bytes =
            limits_.max_solver_owned_bytes - non_calc_owned;
        admission.max_imprint_program_depth =
            limits_.max_imprint_program_depth;
        admission.max_imprint_program_work =
            limits_.max_imprint_program_work;
        admission.consider_imprint_programs = limits_.consider_imprint_programs;
        admission.prices = &economy_->prices;
        const ActionType intended = side_slots[target_side].empty()
            ? ActionType::EldritchAnnul : ActionType::EldritchChaos;
        const auto admitted_program = [&](const std::uint32_t state,
                const bool direct) -> std::uint32_t {
            const StateLocalAutomaticBatch batch =
                problem_.admit_state_local_automatic_candidates(
                    state, admission);
            if (batch.status != StateLocalAutomaticBatchStatus::Complete) {
                retention_status_ = "native_admission_resource_deferred:" +
                    batch.resource_cap;
                return kNoId;
            }
            for (const std::uint32_t index : batch.admitted_operators) {
                const PlannerOperator& option = problem_.operators().at(index);
                if (option.kind != PlannerOperatorKind::FixedOption ||
                    option.option_kind != FixedOptionKind::EldritchSideIntent ||
                    option.automatic_kind != AutomaticCandidateKind::EldritchSide ||
                    option.intended_side != target_side ||
                    option.primitive_program.empty() ||
                    (direct && option.primitive_program.size() != 1) ||
                    problem_.registry().actions.at(
                        option.primitive_program.back()).params.type != intended)
                    continue;
                const OptionKernel& kernel = problem_.option_kernel(state, index);
                if (kernel.supported && kernel.legal &&
                    kernel.automatic.eligible && !kernel.exits.empty())
                    return index;
            }
            return kNoId;
        };
        const std::uint32_t initial = admitted_program(source, false);
        if (initial == kNoId) {
            if (retention_status_ == "pending")
                retention_status_ = "no_admitted_held_side_program";
            finish();
            return;
        }
        std::uint32_t ready = source;
        const std::vector<std::uint32_t> setup =
            problem_.operators().at(initial).primitive_program;
        for (std::size_t step = 0; step + 1 < setup.size(); ++step) {
            const OutcomeDistribution& law =
                problem_.outcomes(ready, setup[step]);
            if (!law.supported || !law.applicable ||
                !law.choice_groups.empty() || law.entries.size() != 1 ||
                std::abs(law.entries.front().probability - 1.0) > 1e-12) {
                retention_status_ = "setup_is_not_deterministic";
                finish();
                return;
            }
            ready = law.entries.front().state;
        }
        const std::uint32_t direct = setup.size() == 1
            ? initial : admitted_program(ready, true);
        if (direct == kNoId) {
            if (retention_status_ == "pending")
                retention_status_ = "no_admitted_direct_continuation";
            finish();
            return;
        }
        const AbstractState& source_state = problem_.state(source);
        const AbstractState& ready_state = problem_.state(ready);
        const auto tiers = [](const AbstractState& state) {
            return static_cast<std::uint32_t>(state.searing_exarch_tier) |
                (static_cast<std::uint32_t>(state.eater_of_worlds_tier) << 8u);
        };
        FinderControlGraph control;
        control.entry = 0;
        control.programs.push_back({initial, source, held_mask});
        if (ready != source)
            control.programs.push_back({direct, ready, held_mask});
        const auto append = [&](const FinderControlKind kind,
                const std::uint32_t binding = kNoId) {
            const std::uint32_t index = static_cast<std::uint32_t>(
                control.nodes.size());
            control.nodes.push_back({kind, binding});
            return index;
        };
        const std::uint32_t goal = append(FinderControlKind::TestGoal);
        std::vector<std::uint32_t> held_tests;
        for (const std::uint32_t slot : side_slots[held_side])
            held_tests.push_back(append(FinderControlKind::TestSlot, slot));
        const std::uint32_t count_test = append(
            FinderControlKind::TestSideCountAtLeast,
            (target_side << 8u) | preferred_count);
        const std::uint32_t ready_test = append(
            FinderControlKind::TestEldritchTiers, tiers(ready_state));
        const std::uint32_t source_test = ready == source ? kNoId : append(
            FinderControlKind::TestEldritchTiers, tiers(source_state));
        const std::uint32_t initial_run = append(
            FinderControlKind::RunNativeProgram, 0);
        const std::uint32_t direct_run = ready == source ? initial_run : append(
            FinderControlKind::RunNativeProgram, 1);
        const std::uint32_t acquire = append(
            FinderControlKind::RunPrimitive, chaos->index);
        const std::uint32_t success = append(FinderControlKind::GoalTerminal);
        control.nodes[goal].on_true = success;
        control.nodes[goal].on_false = held_tests.front();
        for (std::size_t i = 0; i < held_tests.size(); ++i) {
            control.nodes[held_tests[i]].on_true =
                i + 1 == held_tests.size() ? count_test : held_tests[i + 1];
            control.nodes[held_tests[i]].on_false = acquire;
        }
        control.nodes[count_test].on_true = ready_test;
        control.nodes[count_test].on_false = acquire;
        control.nodes[ready_test].on_true = direct_run;
        control.nodes[ready_test].on_false =
            source_test == kNoId ? acquire : source_test;
        if (source_test != kNoId) {
            control.nodes[source_test].on_true = initial_run;
            control.nodes[source_test].on_false = acquire;
        }
        control.nodes[initial_run].next = goal;
        control.nodes[direct_run].next = goal;
        control.nodes[acquire].next = goal;
        Sketch sketch;
        sketch.actions = {chaos->index};
        sketch.score = chaos->price;
        sketch.control = std::move(control);
        sketch.parent_identity = "native-held-side-acquisition";
        if (seen_.insert(sketch_identity(sketch)).second) {
            record_generated(sketch);
            frontier_.push_back(std::move(sketch));
            ++counters_.generated;
            retention_status_ = "generated_native_held_side";
        } else {
            ++counters_.duplicates;
            retention_status_ = "duplicate_native_held_side";
        }
        finish();
    } catch (const std::exception& ex) {
        retention_status_ = std::string("native_construction_refused:") + ex.what();
        finish();
    }
}

void PolicyFinderWork::expand_next_partial() {
    if (pending_cursor_ >= pending_.size()) return;
    const auto started = std::chrono::steady_clock::now();
    const PartialSketch partial = pending_[pending_cursor_++];
    if (partial.hole == HoleKind::Progress ||
        partial.hole == HoleKind::ProgressProgram) {
        const auto annul = std::find_if(ranked_.begin(), ranked_.end(),
            [&](const RankedAction& action) {
                return problem_.registry().actions[action.index].params.type ==
                    ActionType::Annul;
            });
        if (annul != ranked_.end()) {
            const std::uint32_t slot =
                partial.hole == HoleKind::Progress ? 0u : 1u;
            if (slot < problem_.goal().slots.size() &&
                frontier_.size() < 16 && seen_.size() < 256) {
                Sketch child;
                child.actions = {partial.first, annul->index};
                child.score = partial.first_price + annul->price;
                child.parent_identity = partial.parent_identity;
                if (slot == 0) {
                    child.feedback_parent = true;
                    child.control = FinderControlGraph{
                        {
                            {FinderControlKind::TestGoal, kNoId, 5, 1},
                            {FinderControlKind::TestSlot, 0, 2, 4},
                            {FinderControlKind::TestAffixCountAtLeast4,
                                kNoId, 3, 4},
                            {FinderControlKind::RunPrimitive, annul->index,
                                kNoId, kNoId, 0},
                            {FinderControlKind::RunPrimitive, partial.first,
                                kNoId, kNoId, 0},
                            {FinderControlKind::GoalTerminal},
                        }, 0};
                } else {
                    std::vector<std::uint32_t> program;
                    try {
                        program = finder_scour_alchemy_program(
                            problem_.session(), problem_.goal(),
                            problem_.registry(), problem_.candidates());
                    } catch (const std::invalid_argument&) {
                        program.clear();
                    }
                    if (program.size() == 2) {
                        const auto scour = std::find_if(ranked_.begin(), ranked_.end(),
                            [&](const RankedAction& action) {
                                return action.index == program[0];
                            });
                        const auto alchemy = std::find_if(ranked_.begin(), ranked_.end(),
                            [&](const RankedAction& action) {
                                return action.index == program[1];
                            });
                        if (scour != ranked_.end() && alchemy != ranked_.end()) {
                            child.actions.insert(child.actions.end(),
                                program.begin(), program.end());
                            child.score += scour->price + alchemy->price;
                            child.control = FinderControlGraph{
                                {
                                    {FinderControlKind::TestGoal, kNoId, 7, 1},
                                    {FinderControlKind::TestSlot, 0, 2, 5},
                                    {FinderControlKind::TestAffixCountAtLeast4,
                                        kNoId, 3, 4},
                                    {FinderControlKind::RunPrimitive, annul->index,
                                        kNoId, kNoId, 0},
                                    {FinderControlKind::RunPrimitive, partial.first,
                                        kNoId, kNoId, 0},
                                    {FinderControlKind::TestSlot, 1, 6, 4},
                                    {FinderControlKind::RunScourAlchemy,
                                        kNoId, kNoId, kNoId, 0},
                                    {FinderControlKind::GoalTerminal},
                                }, 0};
                        }
                    }
                }
                if (child.control.has_value()) {
                    if (seen_.insert(sketch_identity(child)).second) {
                        record_generated(child);
                        frontier_.push_back(std::move(child));
                        ++counters_.generated;
                    } else ++counters_.duplicates;
                }
            }
        }
        counters_.search_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - started).count());
        update_peak();
        return;
    }
    const ActionType first_type =
        problem_.registry().actions[partial.first].params.type;
    const pc_rarity reached_rarity = first_type == ActionType::Transmute
        ? PC_RARITY_MAGIC
        : first_type == ActionType::Alchemy ||
              first_type == ActionType::Regal
            ? PC_RARITY_RARE : PC_RARITY_NORMAL;
    std::vector<Sketch> children;
    std::vector<FinderScoreFeatures> features;
    for (const RankedAction& second : ranked_) {
        if (children.size() >= 16) break;
        const ActionDescriptor& action =
            problem_.registry().actions[second.index];
        if (partial.hole == HoleKind::Recovery) {
            if (action.params.type != ActionType::Annul) continue;
        } else if (!action_transition_facts(action.params.type).renewal ||
                   (action.legality.rarity_mask &
                    (1u << reached_rarity)) == 0) {
            continue;
        }
        const bool recovery = partial.hole == HoleKind::Recovery;
        children.push_back({{partial.first, second.index},
            0.0, recovery});
        children.back().parent_identity = partial.parent_identity;
        features.push_back({partial.first_price, second.price,
            static_cast<std::uint32_t>(problem_.goal().slots.size()),
            !recovery, recovery});
    }
    const std::vector<double> scores = ranking_ == FinderRankingMode::Heuristic
        ? score_finder_sketch_batch(features)
        : std::vector<double>(features.size(), 0.0);
    std::uint64_t scratch =
        children.capacity() * sizeof(Sketch) +
        features.capacity() * sizeof(FinderScoreFeatures) +
        scores.capacity() * sizeof(double);
    for (const Sketch& child : children)
        scratch += child.actions.capacity() * sizeof(std::uint32_t);
    const std::uint64_t transient = retained_owned_bytes() + scratch;
    counters_.peak_owned_bytes = std::max(
        counters_.peak_owned_bytes, transient);
    if (transient > limits_.max_solver_owned_bytes)
        throw std::length_error("finder search batch exceeded memory cap");
    for (std::size_t i = 0; i < children.size(); ++i)
        children[i].score = scores[i];
    std::stable_sort(children.begin(), children.end(),
        [&](const Sketch& left, const Sketch& right) {
            if (left.score != right.score) return left.score < right.score;
            const auto& registry = problem_.registry().actions;
            return registry[left.actions.back()].id <
                registry[right.actions.back()].id;
        });
    for (Sketch& child : children) {
        if (frontier_.size() >= 16 || seen_.size() >= 256) break;
        if (!seen_.insert(sketch_identity(child)).second) {
            ++counters_.duplicates;
            continue;
        }
        record_generated(child);
        frontier_.push_back(std::move(child));
        ++counters_.generated;
    }
    counters_.search_ns += static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now() - started).count());
    update_peak();
}

void PolicyFinderWork::update_peak() {
    const std::uint64_t live = retained_owned_bytes() +
        (checker_ == nullptr ? 0 : checker_->live_owned_bytes());
    const std::uint64_t peak = retained_owned_bytes() +
        (checker_ == nullptr ? 0 : checker_->peak_owned_bytes());
    counters_.live_owned_bytes = live;
    counters_.peak_owned_bytes = std::max(
        counters_.peak_owned_bytes, peak);
    if (live > limits_.max_solver_owned_bytes ||
        (checker_ != nullptr && peak > limits_.max_solver_owned_bytes)) {
        throw std::length_error("finder exceeded max_solver_owned_bytes");
    }
}

void PolicyFinderWork::start_next_candidate() {
    if (counters_.considered >= 8 || counters_.checked >= 8 ||
        counters_.logical_reforge_work >= limits_.max_reforge_work) {
        done_ = true;
        return;
    }
    if (frontier_.empty() && retention_pending_)
        generate_retention_candidate();
    if (done_) return;
    while (frontier_.empty() && pending_cursor_ < pending_.size())
        expand_next_partial();
    if (exhausted() || counters_.considered >= 8 ||
        counters_.checked >= 8 ||
        counters_.logical_reforge_work >= limits_.max_reforge_work) {
        done_ = true;
        return;
    }
    active_sketch_ = std::move(frontier_.front());
    frontier_.pop_front();
    const Sketch& sketch = *active_sketch_;
    const std::string identity = sketch_identity(sketch);
    const auto receipt = std::find_if(candidate_records_.begin(),
        candidate_records_.end(), [&](const CandidateRecord& record) {
            return record.identity == identity;
        });
    if (receipt == candidate_records_.end())
        throw std::logic_error("finder candidate has no generation receipt");
    active_record_ = static_cast<std::size_t>(
        receipt - candidate_records_.begin());
    candidate_records_[*active_record_].status = "compiling";
    candidate_records_[*active_record_].started_ns = elapsed_ns();
    ++counters_.considered;
    const auto compile_started = std::chrono::steady_clock::now();
    try {
        checking_graph_ = sketch.control.has_value()
            ? compile_finder_control_json(
                problem_, original_start_, *sketch.control, limits_)
            : compile_finder_candidate_json(
                problem_, original_start_, sketch.actions, limits_,
                sketch.return_to_first);
        candidate_records_[*active_record_].graph_hash =
            stable_finder_hash(checking_graph_);
        FinderCandidatePreparation prepared = prepare_finder_candidate(
            problem_, session_, original_start_, checking_graph_,
            sketch.control.has_value() && !sketch.control->programs.empty()
                ? &*sketch.control : nullptr);
        if (!prepared.ready()) {
            throw std::runtime_error(
                "native finder emitted an unbound candidate: " +
                prepared.refusal);
        }
        checking_strategy_ = std::move(prepared.strategy);
        const std::uint64_t owned = retained_owned_bytes();
        if (owned >= limits_.max_solver_owned_bytes) {
            throw std::length_error("finder has no evaluator memory");
        }
        StrategyEvalOptions options;
        options.economy = economy_;
        options.max_sweeps = limits_.max_sweeps;
        options.max_states = std::min(
            limits_.max_discovered_states,
            limits_.candidate_evaluation_limits.max_states == 0
                ? limits_.max_discovered_states
                : limits_.candidate_evaluation_limits.max_states);
        options.max_pairs = static_cast<std::uint32_t>(std::min<std::uint64_t>(
            limits_.max_state_action_rows,
            limits_.candidate_evaluation_limits.max_pairs == 0
                ? limits_.max_state_action_rows
                : limits_.candidate_evaluation_limits.max_pairs));
        options.max_transitions = static_cast<std::uint32_t>(
            std::min<std::uint64_t>(
                limits_.max_transitions,
                limits_.candidate_evaluation_limits.max_transitions == 0
                    ? limits_.max_transitions
                    : limits_.candidate_evaluation_limits.max_transitions));
        options.max_reforge_work = limits_.max_reforge_work -
            counters_.logical_reforge_work;
        options.max_owned_bytes = std::min<std::uint64_t>(
            limits_.max_solver_owned_bytes - owned,
            limits_.candidate_evaluation_limits.max_owned_bytes == 0
                ? limits_.max_solver_owned_bytes
                : limits_.candidate_evaluation_limits.max_owned_bytes);
        options.max_output_json_bytes = limits_.max_strategy_json_bytes;
        if (sketch.control.has_value() &&
            !sketch.control->programs.empty()) {
            options.graph_local_provenance.strategy_json = checking_graph_;
            for (std::size_t node = 0;
                 node < sketch.control->nodes.size(); ++node) {
                const FinderControlNode& control_node =
                    sketch.control->nodes[node];
                if (control_node.kind != FinderControlKind::RunNativeProgram)
                    continue;
                const FinderProgramBinding& binding =
                    sketch.control->programs.at(control_node.binding);
                const auto key = planner_operator_semantic_key(
                    problem_.operators().at(binding.operator_index));
                const std::string id = "c" + std::to_string(node);
                options.graph_local_provenance.decisions.push_back(
                    {id, key, false, false});
                StrategyPolicyDecisionRequest request;
                request.compiled_node_id = id;
                request.selected_operator_identity = key;
                request.graph_local = true;
                options.policy_decision_entries.push_back(
                    std::move(request));
            }
            if (retained_owned_bytes() +
                    3 * checking_graph_.size() >=
                limits_.max_solver_owned_bytes)
                throw std::length_error(
                    "finder has no programme census memory");
        }
        checker_ = std::make_unique<StrategyEvalWork>(
            checking_strategy_, options);
        candidate_records_[*active_record_].status = "checking";
        counters_.compile_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - compile_started).count());
        update_peak();
    } catch (const StrategyEvalUnsupported& ex) {
        candidate_records_[*active_record_].status = "refused_compile";
        candidate_records_[*active_record_].refusal = ex.what();
        candidate_records_[*active_record_].finished_ns = elapsed_ns();
        last_refusal_ = ex.what();
        last_refusal_kind_ = "unsupported";
        ++counters_.refused;
        counters_.compile_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - compile_started).count());
        charge_active_work();
        checker_.reset();
        checking_strategy_.reset();
        checking_graph_.clear();
        active_sketch_.reset();
        active_record_.reset();
    } catch (const std::length_error& ex) {
        candidate_records_[*active_record_].status = "censored_capacity";
        candidate_records_[*active_record_].refusal = ex.what();
        candidate_records_[*active_record_].finished_ns = elapsed_ns();
        last_refusal_ = ex.what();
        last_refusal_kind_ = "capacity";
        ++counters_.censored;
        counters_.compile_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - compile_started).count());
        charge_active_work();
        checker_.reset();
        checking_strategy_.reset();
        checking_graph_.clear();
        active_sketch_.reset();
        active_record_.reset();
    }
}

void PolicyFinderWork::charge_active_work() {
    if (checker_ == nullptr) return;
    const std::uint64_t amount =
        checker_->diagnostic_result().reforge_logical_work_v1;
    counters_.logical_reforge_work = amount >
        limits_.max_reforge_work - counters_.logical_reforge_work
        ? limits_.max_reforge_work
        : counters_.logical_reforge_work + amount;
}

void PolicyFinderWork::complete_active_candidate() {
    const StrategyEvalResult& result = checker_->result();
    CandidateRecord& record = candidate_records_.at(*active_record_);
    record.finished_ns = elapsed_ns();
    record.work = checker_->diagnostic_result().reforge_logical_work_v1;
    record.peak_owned_bytes = checker_->peak_owned_bytes();
    record.status = finder_evaluation_accepted(result)
        ? "accepted" : "refused_check";
    ++counters_.checked;
    charge_active_work();
    schedule_feedback_program();
    if (finder_evaluation_accepted(result)) {
        record.checked_cost = result.total_expected_cost;
        record.has_checked_cost = true;
        ++counters_.accepted;
        if (!best_.has_value() ||
            result.total_expected_cost < best_->expected_cost) {
            FinderCheckedPolicy accepted;
            // Move the already charged graph buffer into the winning bundle.
            // A copy here would coexist with the checker, parsed graph and
            // previous winner before the next memory audit.
            accepted.strategy_json = std::move(checking_graph_);
            if (active_sketch_.has_value() &&
                active_sketch_->control.has_value() &&
                !active_sketch_->control->programs.empty())
                accepted.native_control =
                    std::move(active_sketch_->control);
            accepted.expected_cost = result.total_expected_cost;
            accepted.expected_actions = result.expected_actions;
            accepted.success_probability = result.success_probability;
            accepted.evaluation_peak_bytes =
                checker_->peak_owned_bytes();
            best_ = std::move(accepted);
        }
    } else {
        ++counters_.refused;
        last_refusal_kind_ = "failed_check";
        last_refusal_ = "candidate failed native properness, mass or price check";
        record.refusal = last_refusal_;
    }
    // A completed native check creates the next structural expansion
    // opportunity. The deeper programme branch is not admitted solely from
    // its parent's heuristic score or an assumed local cost improvement.
    checker_.reset();
    checking_strategy_.reset();
    std::string{}.swap(checking_graph_);
    active_sketch_.reset();
    active_record_.reset();
    update_peak();
}

void PolicyFinderWork::step(const std::uint32_t max_work_items) {
    if (done_) return;
    if (finish_requested_) {
        if (checker_ != nullptr) ++counters_.censored;
        if (active_record_.has_value()) {
            CandidateRecord& record = candidate_records_[*active_record_];
            record.status = "censored_finish";
            record.finished_ns = elapsed_ns();
            if (checker_ != nullptr) {
                record.work =
                    checker_->diagnostic_result().reforge_logical_work_v1;
                record.peak_owned_bytes = checker_->peak_owned_bytes();
            }
        }
        charge_active_work();
        release_validation();
        checker_.reset();
        checking_strategy_.reset();
        std::string{}.swap(checking_graph_);
        active_sketch_.reset();
        active_record_.reset();
        done_ = true;
        update_peak();
        return;
    }
    if (checker_ == nullptr) {
        start_next_candidate();
        if (checker_ == nullptr) {
            if (exhausted() || counters_.considered >= 8 ||
                counters_.checked >= 8)
                done_ = true;
            update_peak();
            return;
        }
    }
    const auto check_started = std::chrono::steady_clock::now();
    try {
        if (!checker_->progress().done)
            checker_->step(std::max<std::uint32_t>(1, max_work_items));
        const bool validation_complete = checker_->progress().done &&
            validate_active_programme(max_work_items);
        counters_.check_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - check_started).count());
        update_peak();
        if (validation_complete) complete_active_candidate();
    } catch (const StrategyEvalUnsupported& ex) {
        if (active_record_.has_value()) {
            CandidateRecord& record = candidate_records_[*active_record_];
            record.status = "refused_check";
            record.refusal = ex.what();
            record.finished_ns = elapsed_ns();
            record.work = checker_->diagnostic_result().reforge_logical_work_v1;
            record.peak_owned_bytes = checker_->peak_owned_bytes();
        }
        counters_.check_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - check_started).count());
        last_refusal_ = ex.what();
        last_refusal_kind_ = "unsupported";
        ++counters_.refused;
        charge_active_work();
        release_validation();
        checker_.reset();
        checking_strategy_.reset();
        std::string{}.swap(checking_graph_);
        active_sketch_.reset();
        active_record_.reset();
    } catch (const std::length_error& ex) {
        if (active_record_.has_value()) {
            CandidateRecord& record = candidate_records_[*active_record_];
            record.status = "censored_capacity";
            record.refusal = ex.what();
            record.finished_ns = elapsed_ns();
            record.work = checker_->diagnostic_result().reforge_logical_work_v1;
            record.peak_owned_bytes = checker_->peak_owned_bytes();
        }
        counters_.check_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - check_started).count());
        last_refusal_ = ex.what();
        last_refusal_kind_ = "capacity";
        ++counters_.censored;
        schedule_feedback_program();
        charge_active_work();
        release_validation();
        checker_.reset();
        checking_strategy_.reset();
        std::string{}.swap(checking_graph_);
        active_sketch_.reset();
        active_record_.reset();
    }
    if (checker_ == nullptr &&
        (exhausted() || counters_.considered >= 8 ||
         counters_.checked >= 8)) done_ = true;
    update_peak();
}

void PolicyFinderWork::request_bounded_finish() {
    finish_requested_ = true;
}

FinderProgress PolicyFinderWork::progress() const {
    FinderProgress result = counters_;
    result.done = done_;
    result.finish_requested = finish_requested_;
    result.live_owned_bytes = retained_owned_bytes() +
        (checker_ == nullptr ? 0 : checker_->live_owned_bytes());
    result.pending_holes = static_cast<std::uint32_t>(
        pending_.size() - pending_cursor_);
    return result;
}

const std::optional<FinderCheckedPolicy>& PolicyFinderWork::best() const {
    return best_;
}

std::string PolicyFinderWork::telemetry_json() const {
    const FinderProgress state = progress();
    std::string result = std::string(
        "{\"version\":1,\"lane\":\"strategy_finder\","
        "\"ranking\":\"") +
        (ranking_ == FinderRankingMode::Heuristic ? "heuristic" : "uninformed") +
        "\",\"grammar\":\"" +
        (grammar_ == FinderGrammarMode::ConditionalRetention
            ? "conditional-retention" :
            grammar_ == FinderGrammarMode::Conditional
                ? "conditional" : "primitive") +
        "\",\"considered\":" + std::to_string(state.considered) +
        ",\"checked\":" + std::to_string(state.checked) +
        ",\"generated\":" + std::to_string(state.generated) +
        ",\"duplicates\":" + std::to_string(state.duplicates) +
        ",\"pending_holes\":" + std::to_string(state.pending_holes) +
        ",\"accepted\":" + std::to_string(state.accepted) +
        ",\"refused\":" + std::to_string(state.refused) +
        ",\"censored\":" + std::to_string(state.censored) +
        ",\"logical_reforge_work\":" +
        std::to_string(state.logical_reforge_work) +
        ",\"search_ns\":" + std::to_string(state.search_ns) +
        ",\"compile_ns\":" + std::to_string(state.compile_ns) +
        ",\"check_ns\":" + std::to_string(state.check_ns) +
        ",\"last_refusal_kind\":" + json_string(last_refusal_kind_) +
        ",\"last_refusal\":" + json_string(last_refusal_) +
        ",\"retention_status\":" + json_string(retention_status_) +
        ",\"live_owned_bytes\":" +
        std::to_string(state.live_owned_bytes) +
        ",\"peak_owned_bytes\":" +
        std::to_string(state.peak_owned_bytes) +
        ",\"best_checked_cost\":" +
        (best_.has_value() ? std::to_string(best_->expected_cost) : "null") +
        ",\"done\":" + (state.done ? "true" : "false") +
        ",\"problem_identity\":" + json_string(problem_identity_) +
        ",\"candidates\":[";
    for (std::size_t i = 0; i < candidate_records_.size(); ++i) {
        const CandidateRecord& record = candidate_records_[i];
        if (i != 0) result += ',';
        result += "{\"identity\":" + json_string(record.identity) +
            ",\"parent\":" + json_string(record.parent_identity) +
            ",\"status\":" + json_string(record.status) +
            ",\"refusal\":" + json_string(record.refusal) +
            ",\"graph_hash\":" + json_string(record.graph_hash) +
            ",\"conditional\":" +
                (record.conditional ? "true" : "false") +
            ",\"native_program\":" +
                (record.native_program ? "true" : "false") +
            ",\"programme_entries\":" +
                std::to_string(record.programme_entries) +
            ",\"positive_programme_entries\":" +
                std::to_string(record.positive_programme_entries) +
            ",\"validated_programme_entries\":" +
                std::to_string(record.validated_programme_entries) +
            ",\"generated_ns\":" + std::to_string(record.generated_ns) +
            ",\"started_ns\":" +
                (record.started_ns == 0 ? "null" :
                 std::to_string(record.started_ns)) +
            ",\"finished_ns\":" +
                (record.finished_ns == 0 ? "null" :
                 std::to_string(record.finished_ns)) +
            ",\"logical_reforge_work\":" + std::to_string(record.work) +
            ",\"peak_owned_bytes\":" +
                std::to_string(record.peak_owned_bytes);
        std::ostringstream numeric;
        numeric << std::setprecision(17) << record.score;
        result += ",\"score\":" + numeric.str();
        if (record.has_checked_cost) {
            numeric.str("");
            numeric.clear();
            numeric << std::setprecision(17) << record.checked_cost;
            result += ",\"checked_cost\":" + numeric.str();
        } else result += ",\"checked_cost\":null";
        result += ",\"actions\":[";
        for (std::size_t action = 0; action < record.actions.size(); ++action) {
            if (action != 0) result += ',';
            result += json_string(
                problem_.registry().actions.at(record.actions[action]).id);
        }
        result += "]}";
    }
    result += "]}";
    if (result.size() > limits_.max_telemetry_json_bytes)
        throw std::length_error(
            "finder telemetry exceeded max_telemetry_json_bytes");
    return result;
}

} // namespace poecraft::solver
