#include "solver_executable_fragment_engine.hpp"

#include "../src/handles_internal.hpp"
#include "../src/solver_internal.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace poecraft {
namespace solver {
namespace fragment_v1 {

namespace {

std::string read_text(const std::string& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) throw std::runtime_error("unable to read " + path);
    std::ostringstream out;
    out << stream.rdbuf();
    return out.str();
}

std::string json_string(const std::string& value) {
    std::ostringstream out;
    out << '"';
    for (const unsigned char ch : value) {
        switch (ch) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (ch < 0x20) {
                out << "\\u" << std::hex << std::setw(4)
                    << std::setfill('0') << static_cast<unsigned>(ch)
                    << std::dec << std::setfill(' ');
            } else {
                out << static_cast<char>(ch);
            }
        }
    }
    out << '"';
    return out.str();
}

bool numeric_condition_parameter(const std::string& key) {
    return key == "min" || key == "max" || key == "value" ||
           key == "count" || key == "min_tier" || key == "version";
}

bool boolean_condition_parameter(const std::string& key) {
    return key == "fractured" || key == "crafted";
}

std::string condition_json(const FragmentConditionV1& condition) {
    std::ostringstream out;
    out << "{\"type\":" << json_string(condition.type);
    for (const auto& [key, value] : condition.parameters) {
        out << ',' << json_string(key) << ':';
        if (numeric_condition_parameter(key)) {
            out << value;
        } else if (boolean_condition_parameter(key) &&
                   (value == "true" || value == "false")) {
            out << value;
        } else {
            out << json_string(value);
        }
    }
    if (!condition.children.empty()) {
        out << ",\"conditions\":[";
        for (std::size_t index = 0;
             index < condition.children.size(); ++index) {
            if (index != 0) out << ',';
            out << condition_json(condition.children[index]);
        }
        out << ']';
    }
    if (condition.type == "at_least") {
        out << ",\"count\":" << condition.at_least_count;
    }
    out << '}';
    return out.str();
}

void append_mod_slot_words(
        std::vector<std::uint64_t>& words,
        const pc_mod_slot& slot) {
    words.push_back(slot.mod_id);
    words.push_back(slot.group_id);
    words.push_back(slot.flags);
    words.push_back(slot.roll_count);
    for (std::uint32_t index = 0; index < slot.roll_count; ++index) {
        words.push_back(static_cast<std::uint32_t>(slot.rolls[index]));
    }
    words.push_back(slot.veiled_option_count);
    for (std::uint32_t index = 0;
         index < slot.veiled_option_count; ++index) {
        words.push_back(slot.veiled_option_mod_ids[index]);
    }
    words.push_back(slot.veiled_chosen_mod_id);
}

ExactStateKeyV1 exact_item_key(const pc_item_state& item) {
    ExactStateKeyV1 key;
    auto& words = key.words;
    words.reserve(128);
    words.push_back(item.rarity);
    words.push_back(item.quality);
    words.push_back(item.item_flags);
    words.push_back(item.prefix_count);
    words.push_back(item.suffix_count);
    words.push_back(item.implicit_count);
    words.push_back(item.enchantment_count);
    for (std::uint32_t index = 0; index < item.prefix_count; ++index) {
        append_mod_slot_words(words, item.prefixes[index]);
    }
    for (std::uint32_t index = 0; index < item.suffix_count; ++index) {
        append_mod_slot_words(words, item.suffixes[index]);
    }
    for (std::uint32_t index = 0; index < item.implicit_count; ++index) {
        append_mod_slot_words(words, item.implicits[index]);
    }
    for (std::uint32_t index = 0;
         index < item.enchantment_count; ++index) {
        append_mod_slot_words(words, item.enchantments[index]);
    }
    words.push_back(item.generic_influence_bits);
    words.push_back(item.searing_exarch_tier);
    words.push_back(item.eater_of_worlds_tier);
    words.push_back(item.socket_count);
    for (std::uint32_t index = 0; index < item.socket_count; ++index) {
        words.push_back(item.socket_colors[index]);
    }
    words.push_back(item.link_mask);
    return key;
}

std::string exact_key_text(const ExactStateKeyV1& key) {
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (const std::uint64_t word : key.words) {
        out << std::setw(16) << word;
    }
    out << ':';
    for (const unsigned char ch : key.opaque_bytes) {
        out << std::setw(2) << static_cast<unsigned>(ch);
    }
    return out.str();
}

FragmentConditionV1 condition(
        std::string type,
        StableParametersV1 parameters = {}) {
    FragmentConditionV1 out;
    out.type = std::move(type);
    out.parameters = std::move(parameters);
    return out;
}

FragmentConditionV1 exact_terminal_condition() {
    FragmentConditionV1 out;
    out.type = "all";
    out.children = {
        condition("rarity_is", {{"rarity", "magic"}}),
        condition(
            "has_mod_family",
            {{"family_mod_key", kCleanOneGoalRenewalGoalModV1},
             {"min_tier", "1"}}),
        condition("prefix_count_range", {{"min", "1"}, {"max", "1"}}),
        condition("suffix_count_range", {{"min", "0"}, {"max", "0"}}),
    };
    return out;
}

FragmentConditionV1 exact_clean_condition() {
    FragmentConditionV1 out;
    out.type = "all";
    out.children = {
        condition("rarity_is", {{"rarity", "normal"}}),
        condition("prefix_count_range", {{"min", "0"}, {"max", "0"}}),
        condition("suffix_count_range", {{"min", "0"}, {"max", "0"}}),
    };
    return out;
}

FragmentEdgeV1 edge(
        std::string id,
        std::string target,
        FragmentConditionV1 when,
        const std::uint32_t priority,
        const bool fail_closed = false) {
    FragmentEdgeV1 out;
    out.edge_id = std::move(id);
    out.target_node_id = std::move(target);
    out.condition = std::move(when);
    out.priority = priority;
    out.certification_fail_closed_default = fail_closed;
    return out;
}

FragmentNodeV1 exit_node(
        std::string id,
        const FragmentExitKindV1 kind,
        std::string label) {
    FragmentNodeV1 out;
    out.node_id = std::move(id);
    out.kind = FragmentNodeKindV1::Exit;
    out.exit.kind = kind;
    out.exit.label = std::move(label);
    return out;
}

class EnginePrimitiveOracle final : public ExactPrimitiveOracleV1 {
public:
    EnginePrimitiveOracle(
            std::shared_ptr<const SessionImpl> session,
            std::unique_ptr<CalcContext> calc,
            const std::uint32_t transmute,
            const std::uint32_t scour)
        : session_(std::move(session)),
          calc_(std::move(calc)),
          transmute_(transmute),
          scour_(scour) {}

    ExactStateV1 remember_state(const std::uint32_t state_id) const {
        pc_item_state item;
        if (!calc_->materialize(state_id, item)) {
            throw std::runtime_error(
                "engine exact state could not be materialized");
        }
        ExactStateV1 exact;
        exact.key = exact_item_key(item);
        exact.diagnostic_carrier_projection =
            "rarity:" + std::to_string(item.rarity) +
            ":prefixes:" + std::to_string(item.prefix_count) +
            ":suffixes:" + std::to_string(item.suffix_count);
        const auto [state_pos, state_inserted] =
            state_by_key_.emplace(exact.key, state_id);
        if (!state_inserted && state_pos->second != state_id) {
            throw std::runtime_error(
                "complete exact item key mapped to two engine states");
        }
        const auto [item_pos, item_inserted] =
            item_by_key_.emplace(exact.key, item);
        if (!item_inserted &&
            exact_item_key(item_pos->second) != exact.key) {
            throw std::runtime_error("exact item key collision");
        }
        return exact;
    }

    PrimitiveExpansionV1 expand_primitive(
            const ExactStateV1& state,
            const std::string& stable_action_identity) const override {
        PrimitiveExpansionV1 out;
        const auto retained = state_by_key_.find(state.key);
        if (retained == state_by_key_.end()) {
            out.action_known = false;
            out.refusal_reason = "unknown_exact_state";
            return out;
        }
        std::uint32_t action = kNoId;
        if (stable_action_identity == "transmute") {
            action = transmute_;
        } else if (stable_action_identity == "scour") {
            action = scour_;
        } else {
            out.action_known = false;
            return out;
        }
        const ActionDescriptor& descriptor =
            calc_->registry().actions.at(action);
        out.legal = action_legal(
            *session_, descriptor, calc_->state(retained->second));
        if (!out.legal) return out;
        const OutcomeDistribution& distribution =
            calc_->outcomes(retained->second, action);
        out.supported = distribution.supported;
        out.legal = distribution.applicable;
        if (!out.supported || !out.legal) return out;
        for (const OutcomeEntry& entry : distribution.entries) {
            const ExactStateV1 successor = remember_state(entry.state);
            const std::string outcome_id =
                "exact-successor:" + exact_key_text(successor.key);
            out.authoritative_outcomes.push_back(
                {outcome_id, entry.probability});
            PrimitivePhysicalOutcomeV1 physical;
            physical.physical_outcome_id = outcome_id;
            physical.probability = entry.probability;
            physical.successor = successor;
            for (const std::string& resource : descriptor.cost_keys) {
                physical.resource_quantities.push_back({resource, 1.0});
            }
            out.physical_outcomes.push_back(std::move(physical));
        }
        return out;
    }

    std::optional<bool> evaluate_condition(
            const FragmentConditionV1& condition,
            const ExactStateV1& state) const override {
        const auto item = item_by_key_.find(state.key);
        if (item == item_by_key_.end()) return std::nullopt;
        try {
            const CanonicalIdentityV1 identity =
                canonical_fragment_condition_identity_v1(condition);
            auto compiled = conditions_.find(identity.canonical_bytes);
            if (compiled == conditions_.end()) {
                const std::string json =
                    std::string{
                        "{\"version\":\"v1\",\"name\":\"fragment "
                        "condition\",\"start_node_id\":\"start\","}
                    + "\"base_state\":{\"base_key\":" +
                    json_string(kCleanOneGoalRenewalBaseV1) +
                    ",\"item_level\":86,\"rarity\":\"normal\"},"
                    "\"nodes\":[{\"id\":\"start\",\"kind\":"
                    "\"start\"},{\"id\":\"route\",\"kind\":"
                    "\"router\"},{\"id\":\"yes\",\"kind\":"
                    "\"terminal\",\"terminal\":\"success\"},{\"id\":"
                    "\"no\",\"kind\":\"terminal\",\"terminal\":"
                    "\"failure\"}],\"edges\":[{\"id\":\"begin\"," 
                    "\"from\":\"start\",\"to\":\"route\","
                    "\"priority\":0,\"condition\":{\"type\":"
                    "\"always\"}},{\"id\":\"match\"," 
                    "\"from\":\"route\",\"to\":\"yes\","
                    "\"priority\":0,\"condition\":" +
                    condition_json(condition) +
                    "},{\"id\":\"default\",\"from\":\"route\"," 
                    "\"to\":\"no\",\"priority\":1,\"is_default\":"
                    "true}]}";
                const auto strategy = compile_strategy_json(
                    session_, json.data(), json.size());
                const StrategyNode& route =
                    strategy->nodes.at(strategy->node_by_id.at("route"));
                if (route.edges.empty()) return std::nullopt;
                compiled = conditions_.emplace(
                    identity.canonical_bytes,
                    route.edges.front().condition).first;
            }
            return evaluate_compiled_condition(
                compiled->second, *session_, item->second);
        } catch (const std::exception& error) {
            std::printf(
                "fragment engine condition refusal: %s\n",
                error.what());
            return std::nullopt;
        } catch (...) {
            return std::nullopt;
        }
    }

    CalcContext& calc() const { return *calc_; }
    std::uint32_t transmute() const { return transmute_; }

private:
    std::shared_ptr<const SessionImpl> session_;
    mutable std::unique_ptr<CalcContext> calc_;
    std::uint32_t transmute_ = kNoId;
    std::uint32_t scour_ = kNoId;
    mutable std::map<ExactStateKeyV1, std::uint32_t> state_by_key_;
    mutable std::map<ExactStateKeyV1, pc_item_state> item_by_key_;
    mutable std::map<std::string, CompiledCondition> conditions_;
};

} // namespace

EngineBackedRenewalBuildResultV1
build_clean_one_goal_transmute_scour_renewal_v1(
        const std::string& compiled_artifact_directory) {
    EngineBackedRenewalBuildResultV1 result;
    try {
        const std::string manifest = read_text(
            compiled_artifact_directory + "/manifest.json");
        const std::string strings = read_text(
            compiled_artifact_directory + "/strings.json");
        const std::string game = read_text(
            compiled_artifact_directory + "/game-data.json");
        const auto data = load_data_impl(manifest, strings, game);
        const auto base = data->base_by_path.find(
            kCleanOneGoalRenewalBaseV1);
        if (base == data->base_by_path.end()) {
            throw std::runtime_error("renewal base is absent from artifact");
        }
        auto session = std::make_shared<SessionImpl>();
        session->data = data;
        session->base_index = base->second;
        session->item_level = 86;
        build_session(*session);

        const auto mod = data->mod_pos_by_key.find(
            kCleanOneGoalRenewalGoalModV1);
        if (mod == data->mod_pos_by_key.end()) {
            throw std::runtime_error("renewal goal mod is absent from artifact");
        }
        const auto session_mod = session->session_id_by_global_id.find(
            data->mod_global_ids[mod->second]);
        if (session_mod == session->session_id_by_global_id.end()) {
            throw std::runtime_error("renewal goal mod is absent from session");
        }
        if (session->gen_type[session_mod->second] != PC_SIDE_PREFIX) {
            throw std::runtime_error(
                "renewal goal mod changed generation side");
        }

        ActionRegistry registry = build_action_registry(*session);
        const std::uint32_t transmute =
            registry.index_by_id.at("transmute");
        const std::uint32_t scour = registry.index_by_id.at("scour");
        GoalSpec goal;
        GoalSlot slot;
        slot.family_id = session->family_id[session_mod->second];
        slot.min_tier = 1;
        goal.slots = {slot};
        goal.rarity = PC_RARITY_MAGIC;
        goal.min_satisfied_slots = 1;
        goal.primitive_actions_explicit = true;

        auto calc = std::make_unique<CalcContext>(
            session, goal, std::move(registry),
            std::vector<std::uint32_t>{transmute, scour},
            false, false, true, std::optional<std::uint32_t>{20000},
            std::vector<CountObservation>{}, false,
            std::vector<std::uint64_t>{}, true);
        auto oracle = std::make_shared<EnginePrimitiveOracle>(
            session, std::move(calc), transmute, scour);

        pc_item_state clean;
        pc_item_clear(&clean);
        const std::uint32_t clean_state = oracle->calc().intern_item(clean);
        const ExactStateV1 exact_clean = oracle->remember_state(clean_state);
        const OutcomeDistribution& transmute_outcomes =
            oracle->calc().outcomes(clean_state, oracle->transmute());
        if (!transmute_outcomes.supported ||
            !transmute_outcomes.applicable ||
            transmute_outcomes.entries.empty()) {
            throw std::runtime_error(
                "engine Transmute row is unavailable on clean entry");
        }

        double terminal_probability = 0.0;
        double goal_plus_junk_probability = 0.0;
        double other_nonterminal_probability = 0.0;
        for (const OutcomeEntry& entry : transmute_outcomes.entries) {
            const AbstractState& state = oracle->calc().state(entry.state);
            const bool terminal = oracle->calc().is_goal_state(state);
            const bool goal_present =
                state.slot_status[0] == static_cast<std::uint8_t>(
                    GoalSlotStatus::Satisfied);
            if (terminal) {
                terminal_probability += entry.probability;
            } else if (goal_present &&
                       state.prefix_count + state.suffix_count > 1) {
                goal_plus_junk_probability += entry.probability;
            } else {
                other_nonterminal_probability += entry.probability;
            }
        }
        if (!(terminal_probability > 0.0) ||
            !(terminal_probability < 1.0) ||
            !(goal_plus_junk_probability > 0.0) ||
            !(other_nonterminal_probability > 0.0)) {
            throw std::runtime_error(
                "engine renewal control lost terminal/junk/miss coverage");
        }

        EngineBackedRenewalFixtureV1 fixture;
        fixture.context.exact_entry = exact_clean;
        fixture.context.caller_action_scope_identity =
            "primitive-actions:scour,transmute:v1";
        fixture.context.disabled_action_family_identity =
            "disabled-families:none:v1";
        fixture.context.exact_goal_identity =
            std::string{"magic-exact-one-family:"} +
            kCleanOneGoalRenewalGoalModV1 +
            ":min-tier=1:no-junk:v1";
        fixture.context.clean_base_state.base_metadata_path =
            kCleanOneGoalRenewalBaseV1;
        fixture.context.clean_base_state.item_level = 86;
        fixture.context.mechanics_artifact_identity =
            "compiled-artifact-manifest-v1:" + manifest;
        fixture.context.resource_vocabulary = {"scour", "transmute"};
        fixture.context.prices = {{"scour", 0.05}, {"transmute", 0.05}};

        fixture.ir.fragment_id = kCleanOneGoalRenewalCaseV1;
        fixture.ir.exact_entry_product_state_identity =
            canonical_exact_entry_identity_v1(exact_clean, {});
        fixture.ir.caller_action_scope_identity =
            fixture.context.caller_action_scope_identity;
        fixture.ir.disabled_action_family_identity =
            fixture.context.disabled_action_family_identity;
        fixture.ir.exact_goal_identity = fixture.context.exact_goal_identity;
        fixture.ir.mechanics_artifact_identity =
            fixture.context.mechanics_artifact_identity;
        fixture.ir.exact_state_key_semantics_version =
            "pc-item-state-full-value-v1";
        fixture.ir.refinement_semantics_version =
            "calc-distinguish-modifier-identity-v1";
        fixture.ir.condition_semantics_version =
            "native-strategy-condition-v1";
        fixture.ir.clean_base_state = fixture.context.clean_base_state;
        fixture.ir.entry_node_id = "transmute";

        FragmentNodeV1 transmute_node;
        transmute_node.node_id = "transmute";
        transmute_node.kind = FragmentNodeKindV1::PrimitiveOperation;
        transmute_node.stable_action_identity = "transmute";
        const FragmentConditionV1 terminal = exact_terminal_condition();
        FragmentConditionV1 retry;
        retry.type = "not";
        retry.children = {terminal};
        transmute_node.edges = {
            edge("exact-terminal", "success", terminal, 0),
            edge("nonterminal", "scour", retry, 1),
            edge(
                "fail-closed", "failure", condition("always"), 2,
                true),
        };

        FragmentNodeV1 scour_node;
        scour_node.node_id = "scour";
        scour_node.kind = FragmentNodeKindV1::PrimitiveOperation;
        scour_node.stable_action_identity = "scour";
        scour_node.edges = {
            edge("clean", "transmute", exact_clean_condition(), 0),
            edge(
                "fail-closed", "failure", condition("always"), 1,
                true),
        };
        fixture.ir.nodes = {
            transmute_node,
            scour_node,
            exit_node(
                "success", FragmentExitKindV1::FinalSuccess,
                "exact-final-success"),
            exit_node(
                "failure", FragmentExitKindV1::CertificationFailure,
                "certification-failure"),
        };
        fixture.oracle = std::move(oracle);
        fixture.exact_terminal_probability = terminal_probability;
        fixture.exact_goal_plus_junk_probability =
            goal_plus_junk_probability;
        fixture.exact_other_nonterminal_probability =
            other_nonterminal_probability;
        fixture.transmute_physical_outcomes =
            transmute_outcomes.entries.size();
        fixture.base_identity =
            std::string{kCleanOneGoalRenewalBaseV1} + "@86";
        fixture.goal_identity = fixture.context.exact_goal_identity;
        std::ostringstream forward;
        forward << "renewal-forward-v1:p_bits=" << std::hex
                << std::bit_cast<std::uint64_t>(terminal_probability)
                << ":transmute_bits="
                << std::bit_cast<std::uint64_t>(
                       1.0 / terminal_probability)
                << ":scour_bits="
                << std::bit_cast<std::uint64_t>(
                       (1.0 - terminal_probability) /
                       terminal_probability);
        fixture.forward_reference_identity = forward.str();
        result.fixture = std::move(fixture);
    } catch (const std::exception& error) {
        result.refusal = error.what();
    }
    return result;
}

EngineBackedFragmentEvaluationResultV1
EngineBackedFragmentEvaluatorV1::evaluate(
        const FlattenedFragmentCandidateV1& candidate,
        const std::string& compiled_artifact_directory,
        const std::string& economy_json_override,
        const EngineBackedFragmentEvaluationLimitsV1& limits) const {
    EngineBackedFragmentEvaluationResultV1 output;
    try {
        const std::string manifest = read_text(
            compiled_artifact_directory + "/manifest.json");
        const std::string strings = read_text(
            compiled_artifact_directory + "/strings.json");
        const std::string game = read_text(
            compiled_artifact_directory + "/game-data.json");
        const auto data = load_data_impl(manifest, strings, game);
        const auto base = data->base_by_path.find(
            kCleanOneGoalRenewalBaseV1);
        if (base == data->base_by_path.end()) {
            throw std::runtime_error(
                "evaluation base is absent from artifact");
        }
        auto session = std::make_shared<SessionImpl>();
        session->data = data;
        session->base_index = base->second;
        session->item_level = 86;
        build_session(*session);

        const std::string strategy_json =
            candidate.ordinary_strategy_json();
        const auto strategy = compile_strategy_json(
            session, strategy_json.data(), strategy_json.size());
        const std::string default_economy_json =
            "{\"version\":\"v1\",\"id\":"
            "\"verified-fragment-independent-evaluation-v1\","
            "\"prices\":{\"transmute\":0.05,\"scour\":0.05}}";
        const std::string& economy_json = economy_json_override.empty()
            ? default_economy_json
            : economy_json_override;
        StrategyEvalOptions options;
        options.max_states = limits.max_states;
        options.max_pairs = limits.max_pairs;
        options.max_transitions = limits.max_transitions;
        options.max_owned_bytes = limits.max_owned_bytes;
        options.economy = load_economy_json(
            economy_json.data(), economy_json.size());
        const StrategyEvalResult evaluated = evaluate_strategy(
            *strategy, options);
        const StrategyEvalResult forward =
            evaluate_strategy_forward_reference_for_test(
                *strategy, options);

        constexpr double kTolerance = 1e-9;
        double forward_maximum_delta = 0.0;
        const auto compare = [&](const double left, const double right) {
            if (!std::isfinite(left) || !std::isfinite(right)) {
                throw std::runtime_error(
                    "independent evaluation produced nonfinite output");
            }
            const double delta = std::fabs(left - right);
            forward_maximum_delta = std::max(
                forward_maximum_delta, delta);
            if (delta > kTolerance *
                    std::max({1.0, std::fabs(left), std::fabs(right)})) {
                throw std::runtime_error(
                    "exact evaluator and forward reference disagree");
            }
        };
        compare(evaluated.success_probability, forward.success_probability);
        compare(evaluated.failure_probability, forward.failure_probability);
        compare(evaluated.stop_probability, forward.stop_probability);
        compare(
            evaluated.action_not_applied_probability,
            forward.action_not_applied_probability);
        compare(
            evaluated.no_matching_edge_probability,
            forward.no_matching_edge_probability);
        compare(
            evaluated.unresolved_probability,
            forward.unresolved_probability);
        compare(evaluated.expected_actions, forward.expected_actions);
        compare(
            evaluated.total_expected_cost,
            forward.total_expected_cost);
        if (evaluated.expected_consumption.size() !=
            forward.expected_consumption.size()) {
            throw std::runtime_error(
                "exact evaluator resource vocabulary disagrees with forward "
                "reference");
        }
        for (const auto& [key, value] : evaluated.expected_consumption) {
            const auto found = forward.expected_consumption.find(key);
            if (found == forward.expected_consumption.end()) {
                throw std::runtime_error(
                    "forward reference omitted resource: " + key);
            }
            compare(value, found->second);
        }

        const double terminal_mass =
            evaluated.success_probability +
            evaluated.failure_probability + evaluated.stop_probability +
            evaluated.action_not_applied_probability +
            evaluated.no_matching_edge_probability +
            evaluated.unresolved_probability;
        const double maximum_mass_error = std::max({
            std::fabs(1.0 - terminal_mass),
            std::fabs(evaluated.residual_mass),
            std::fabs(evaluated.max_mass_conservation_error),
        });
        const bool zero_non_success =
            evaluated.failure_probability == 0.0 &&
            evaluated.stop_probability == 0.0 &&
            evaluated.action_not_applied_probability == 0.0 &&
            evaluated.no_matching_edge_probability == 0.0 &&
            evaluated.unresolved_probability == 0.0;
        const bool cost_reconciled =
            std::fabs(evaluated.cost_dot_product_difference) <= kTolerance &&
            std::fabs(evaluated.action_descriptor_visits_difference) <=
                kTolerance &&
            std::fabs(evaluated.action_descriptor_applied_difference) <=
                kTolerance &&
            std::fabs(evaluated.node_operation_visits_difference) <=
                kTolerance;
        if (!evaluated.converged || !forward.converged ||
            !evaluated.cost_complete || !forward.cost_complete ||
            !cost_reconciled || !zero_non_success ||
            std::fabs(evaluated.success_probability - 1.0) > kTolerance ||
            maximum_mass_error > kTolerance) {
            throw std::runtime_error(
                "flattened fragment failed independent exact acceptance");
        }
        for (const auto& [key, difference] :
             evaluated.material_quantity_differences) {
            if (std::fabs(difference) > kTolerance) {
                throw std::runtime_error(
                    "material reconciliation failed: " + key);
            }
        }

        IndependentFragmentEvaluationV1 summary;
        summary.candidate_identity = candidate.candidate_identity();
        summary.strategy_json_bytes = strategy_json.size();
        summary.compiled_nodes = strategy->nodes.size();
        for (const StrategyNode& node : strategy->nodes) {
            summary.compiled_edges += node.edges.size();
        }
        summary.converged = evaluated.converged;
        summary.proper = zero_non_success && maximum_mass_error <= kTolerance;
        summary.cost_complete = evaluated.cost_complete;
        summary.cost_reconciled = cost_reconciled;
        summary.success_probability = evaluated.success_probability;
        summary.failure_probability = evaluated.failure_probability;
        summary.stop_probability = evaluated.stop_probability;
        summary.action_not_applied_probability =
            evaluated.action_not_applied_probability;
        summary.no_matching_edge_probability =
            evaluated.no_matching_edge_probability;
        summary.unresolved_probability = evaluated.unresolved_probability;
        summary.expected_actions = evaluated.expected_actions;
        summary.expected_consumption = evaluated.expected_consumption;
        summary.total_expected_cost = evaluated.total_expected_cost;
        summary.maximum_mass_error = maximum_mass_error;
        summary.forward_maximum_delta = forward_maximum_delta;

        FlattenedFragmentCandidateV1 evaluated_candidate(
            FlattenedFragmentCandidateV1::ConstructionToken{},
            candidate.candidate_identity(), strategy_json,
            evaluated.total_expected_cost);
        output.candidate = std::move(evaluated_candidate);
        output.evaluation = std::move(summary);
    } catch (const std::exception& error) {
        output.refusal = error.what();
    }
    return output;
}

} // namespace fragment_v1
} // namespace solver
} // namespace poecraft
