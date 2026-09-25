#include "solver_finder.hpp"

#include "json.hpp"
#include "solver_policy_refinement_helpers.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
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

} // namespace

FinderCandidatePreparation prepare_finder_candidate(
    const CalcContext& problem,
    std::shared_ptr<const SessionImpl> session,
    const pc_item_state& original_start,
    const std::string& strategy_json) {
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
            const json::Value* condition = edge.find("condition");
            if (condition == nullptr ||
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
            if (action == kNoId ||
                std::find(problem.candidates().begin(),
                          problem.candidates().end(), action) ==
                    problem.candidates().end()) {
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
    const SolveOptions& limits)
    : problem_(problem), session_(std::move(session)),
      original_start_(original_start),
      economy_(std::make_shared<EconomyImpl>()), limits_(limits) {
    if (session_ == nullptr) {
        throw std::invalid_argument("finder requires a session");
    }
    economy_->id = "finder-request";
    economy_->prices = std::move(prices);

    const auto search_started = std::chrono::steady_clock::now();
    const std::uint32_t start_state = problem_.intern_item(original_start_);
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
            if (left.price != right.price) return left.price < right.price;
            return problem_.registry().actions[left.index].id <
                problem_.registry().actions[right.index].id;
        });
    std::unordered_set<std::uint32_t> selected;
    const auto add_single = [&](const RankedAction& action) {
        if (selected.insert(action.index).second) {
            frontier_.push_back({{action.index}, action.price});
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
        pending_.push_back({first.index, first.price, HoleKind::Recovery});
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
        pending_.push_back({setup.index, setup.price, HoleKind::Renewal});
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
        refinement::economy_owned_bytes(economy_->prices, economy_->id.capacity()) +
        ranked_.capacity() * sizeof(RankedAction) +
        frontier_.capacity() * sizeof(Sketch) +
        pending_.capacity() * sizeof(PartialSketch) +
        checking_graph_.capacity() + last_refusal_.capacity() +
        last_refusal_kind_.capacity();
    for (const Sketch& sketch : frontier_)
        bytes += sketch.actions.capacity() * sizeof(std::uint32_t);
    if (checking_strategy_ != nullptr)
        bytes += refinement::strategy_impl_owned_bytes(*checking_strategy_);
    if (best_.has_value()) bytes += best_->strategy_json.capacity();
    return bytes;
}

bool PolicyFinderWork::exhausted() const {
    return cursor_ >= frontier_.size() &&
        (pending_cursor_ >= pending_.size() || frontier_.size() >= 16);
}

void PolicyFinderWork::expand_next_partial() {
    if (pending_cursor_ >= pending_.size()) return;
    const auto started = std::chrono::steady_clock::now();
    const PartialSketch partial = pending_[pending_cursor_++];
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
        features.push_back({partial.first_price, second.price,
            static_cast<std::uint32_t>(problem_.goal().slots.size()),
            !recovery, recovery});
    }
    const std::vector<double> scores =
        score_finder_sketch_batch(features);
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
        const bool duplicate = std::any_of(frontier_.begin(),
            frontier_.end(), [&](const Sketch& previous) {
                return previous.return_to_first == child.return_to_first &&
                    previous.actions == child.actions;
            });
        if (duplicate) {
            ++counters_.duplicates;
            continue;
        }
        if (frontier_.size() >= 16) break;
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
    while (cursor_ >= frontier_.size() &&
           pending_cursor_ < pending_.size() &&
           frontier_.size() < 16)
        expand_next_partial();
    if (exhausted() || counters_.checked >= 8 ||
        counters_.logical_reforge_work >= limits_.max_reforge_work) {
        done_ = true;
        return;
    }
    const Sketch& sketch = frontier_[cursor_++];
    ++counters_.considered;
    const auto compile_started = std::chrono::steady_clock::now();
    try {
        checking_graph_ = compile_finder_candidate_json(
            problem_, original_start_, sketch.actions, limits_,
            sketch.return_to_first);
        FinderCandidatePreparation prepared = prepare_finder_candidate(
            problem_, session_, original_start_, checking_graph_);
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
        checker_ = std::make_unique<StrategyEvalWork>(
            checking_strategy_, options);
        counters_.compile_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - compile_started).count());
        update_peak();
    } catch (const StrategyEvalUnsupported& ex) {
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
    } catch (const std::length_error& ex) {
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
    ++counters_.checked;
    charge_active_work();
    if (finder_evaluation_accepted(result)) {
        ++counters_.accepted;
        if (!best_.has_value() ||
            result.total_expected_cost < best_->expected_cost) {
            FinderCheckedPolicy accepted;
            accepted.strategy_json = checking_graph_;
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
    }
    checker_.reset();
    checking_strategy_.reset();
    std::string{}.swap(checking_graph_);
    update_peak();
}

void PolicyFinderWork::step(const std::uint32_t max_work_items) {
    if (done_) return;
    if (finish_requested_) {
        if (checker_ != nullptr) ++counters_.censored;
        charge_active_work();
        checker_.reset();
        checking_strategy_.reset();
        std::string{}.swap(checking_graph_);
        done_ = true;
        update_peak();
        return;
    }
    if (checker_ == nullptr) {
        start_next_candidate();
        if (checker_ == nullptr) {
            if (exhausted() || counters_.checked >= 8)
                done_ = true;
            update_peak();
            return;
        }
    }
    const auto check_started = std::chrono::steady_clock::now();
    try {
        checker_->step(std::max<std::uint32_t>(1, max_work_items));
        counters_.check_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - check_started).count());
        update_peak();
        if (checker_->progress().done) complete_active_candidate();
    } catch (const StrategyEvalUnsupported& ex) {
        counters_.check_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - check_started).count());
        last_refusal_ = ex.what();
        last_refusal_kind_ = "unsupported";
        ++counters_.refused;
        charge_active_work();
        checker_.reset();
        checking_strategy_.reset();
        std::string{}.swap(checking_graph_);
    } catch (const std::length_error& ex) {
        counters_.check_ns += static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::nanoseconds>(
                std::chrono::steady_clock::now() - check_started).count());
        last_refusal_ = ex.what();
        last_refusal_kind_ = "capacity";
        ++counters_.censored;
        charge_active_work();
        checker_.reset();
        checking_strategy_.reset();
        std::string{}.swap(checking_graph_);
    }
    if (checker_ == nullptr &&
        (exhausted() || counters_.checked >= 8)) done_ = true;
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
    const std::string result = "{\"version\":1,\"lane\":\"strategy_finder\","
        "\"considered\":" + std::to_string(state.considered) +
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
        ",\"live_owned_bytes\":" +
        std::to_string(state.live_owned_bytes) +
        ",\"peak_owned_bytes\":" +
        std::to_string(state.peak_owned_bytes) +
        ",\"best_checked_cost\":" +
        (best_.has_value() ? std::to_string(best_->expected_cost) : "null") +
        ",\"done\":" + (state.done ? "true" : "false") + "}";
    if (result.size() > limits_.max_telemetry_json_bytes)
        throw std::length_error(
            "finder telemetry exceeded max_telemetry_json_bytes");
    return result;
}

} // namespace poecraft::solver
