#include "solver_compile_serialization.hpp"

/*
 * Compile an exact solver policy into the ordinary strategy graph format.
 * Policy states with the same action and continuation region share operation
 * nodes, while a collision-checked decision DAG routes each concrete item to
 * its exact policy region. A final default edge makes abstraction drift or a
 * vocabulary mismatch fail loudly. Expected-cost annotations are presentation
 * metadata consumed by the editor/board, never execution authority.
 */
namespace poecraft {
namespace solver {

std::string compile_policy_strategy_json(
    CalcContext& calc,
    const SolveResult& result,
    const std::string& name,
    PolicyCompilationTelemetry* telemetry,
    const std::uint64_t max_strategy_json_bytes,
    const refinement::RefinedPolicyCompileRouting* refined_routing,
    const std::uint64_t max_compiler_owned_bytes) {
    const std::uint64_t strategy_json_limit = std::min(
        result.options.max_strategy_json_bytes,
        max_strategy_json_bytes);
    std::uint64_t compiler_peak_owned_bytes = 0;
    std::uint64_t compiler_complete_peak_owned_bytes = 0;
    const auto observe_compiler_owned =
        [&](const std::uint64_t bytes) {
            compiler_peak_owned_bytes =
                std::max(compiler_peak_owned_bytes, bytes);
            compiler_complete_peak_owned_bytes = std::max(
                compiler_complete_peak_owned_bytes, bytes);
            if (telemetry != nullptr) {
                telemetry->peak_owned_bytes =
                    compiler_peak_owned_bytes;
                telemetry->previously_accounted_peak_owned_bytes =
                    compiler_peak_owned_bytes;
                telemetry->complete_peak_owned_bytes =
                    compiler_complete_peak_owned_bytes;
            }
            if (bytes > max_compiler_owned_bytes) {
                if (telemetry != nullptr) {
                    telemetry->cap_hit =
                        "max_solver_owned_bytes";
                }
                throw SolverResourceLimit(
                    "max_solver_owned_bytes",
                    max_compiler_owned_bytes);
            }
        };
    const auto observe_complete_compiler_owned =
        [&](const std::uint64_t previously_accounted,
            const std::uint64_t complete) {
            /* Gate 0 is deliberately observational: only the historic
             * partial estimate reaches the cap check above. */
            observe_compiler_owned(previously_accounted);
            compiler_complete_peak_owned_bytes = std::max(
                compiler_complete_peak_owned_bytes,
                std::max(previously_accounted, complete));
            if (telemetry != nullptr) {
                telemetry->complete_peak_owned_bytes =
                    compiler_complete_peak_owned_bytes;
            }
        };
    if (!result.refined_policy_artifact.strategy_json.empty()) {
        if (result.refined_policy_artifact.strategy_json.size() >
            strategy_json_limit) {
            if (telemetry != nullptr) {
                telemetry->cap_hit = "max_strategy_json_bytes";
            }
            gap("retained refined policy exceeded "
                "max_strategy_json_bytes");
        }
        if (telemetry != nullptr) {
            telemetry->working_states =
                result.refined_policy_artifact.working_states;
            telemetry->behavioral_classes =
                result.refined_policy_artifact.behavioral_classes;
            telemetry->policy_regions =
                result.refined_policy_artifact.policy_regions;
            telemetry->nodes = result.refined_policy_artifact.nodes;
            telemetry->edges = result.refined_policy_artifact.edges;
            telemetry->strategy_json_bytes =
                result.refined_policy_artifact.strategy_json.size();
            telemetry->total_condition_bytes =
                result.refined_policy_artifact.total_condition_bytes;
            telemetry->max_condition_bytes =
                result.refined_policy_artifact.max_condition_bytes;
            telemetry->exact_state_fallbacks =
                result.refined_policy_artifact.exact_state_fallbacks;
            telemetry->junk_predicates =
                result.refined_policy_artifact.junk_predicates;
        }
        const std::uint64_t previously_accounted = std::max(
            result.refined_policy_artifact
                .previously_accounted_peak_owned_bytes,
            result.refined_policy_artifact.strategy_json.size() + 1);
        observe_complete_compiler_owned(
            previously_accounted,
            std::max(
                result.refined_policy_artifact
                    .complete_peak_owned_bytes,
                previously_accounted));
        return result.refined_policy_artifact.strategy_json;
    }
    const SessionImpl& session = calc.session();
    const DataImpl& data = *session.data;
    const AbstractLayout& layout = calc.layout();
    const auto primitive_observes_modifier_offer =
        [&](const PlannerOperator& planner) {
            return planner.kind == PlannerOperatorKind::Primitive &&
                   planner.primitive_action <
                       calc.registry().actions.size() &&
                   action_observes_modifier_offer(
                       calc.registry().actions[
                           planner.primitive_action]);
        };
    const auto option_observes_modifier_offer =
        [&](const PlannerOperator& planner) {
            return planner.kind == PlannerOperatorKind::FixedOption &&
                   !planner.primitive_program.empty() &&
                   planner.primitive_program.back() <
                       calc.registry().actions.size() &&
                   action_observes_modifier_offer(
                       calc.registry().actions[
                           planner.primitive_program.back()]);
        };

    if (result.start_state == kNoId ||
        result.start_state >= result.values.size()) {
        gap("solve result has no start state");
    }

    std::vector<SlotVocabulary> vocabulary;
    for (std::size_t i = 0; i < layout.slots.size(); ++i) {
        vocabulary.push_back(
            slot_vocabulary(session, layout.slots[i], i));
    }

    /*
     * A witnessed fixed destructive renewal has one action-local behavior:
     * apply the selected reforge, succeed on the goal, otherwise repeat.
     * The native incumbent already proved the complete gated row. Recheck
     * action legality and the engine-owned preserved-boundary signature for
     * every policy-reachable non-goal carrier before emitting the compact
     * loop. This is a fixed-policy proof, not Bellman state equivalence.
     */
    if (result.primitive_renewal_witness.valid) {
        const PrimitiveRenewalWitness& witness =
            result.primitive_renewal_witness;
        const bool bounded =
            result.policy_status == SolvePolicyStatus::BoundedFeasible ||
            result.policy_status ==
                SolvePolicyStatus::BoundedNearOptimal;
        if (!bounded ||
            !result.options.goal_progress_gated_reforges ||
            witness.operator_index >= calc.operators().size() ||
            witness.primitive_action >=
                calc.registry().actions.size() ||
            witness.kernel_signature.empty() ||
            !(witness.success_probability > 0.0) ||
            !std::isfinite(witness.value) ||
            witness.value < 0.0 ||
            result.start_state >= result.policy_reachable.size() ||
            !result.policy_reachable[result.start_state] ||
            result.policy_reachable.size() != result.values.size() ||
            result.goal_states.size() != result.values.size() ||
            result.policy.size() != result.values.size()) {
            gap("gated primitive renewal witness is incomplete");
        }
        const PlannerOperator& planner =
            calc.operators().at(witness.operator_index);
        if (planner.kind != PlannerOperatorKind::Primitive ||
            planner.primitive_action != witness.primitive_action ||
            result.policy[result.start_state].index !=
                witness.operator_index ||
            result.policy[result.start_state].kind != planner.kind) {
            gap("gated primitive renewal witness action changed");
        }
        const ActionDescriptor& descriptor =
            calc.registry().actions.at(witness.primitive_action);
        if (!action_transition_facts(descriptor.params.type).renewal) {
            gap("gated primitive renewal witness is not destructive");
        }
        std::uint64_t working_states = 0;
        for (std::uint32_t state = 0;
             state < result.values.size(); ++state) {
            if (!result.policy_reachable[state] ||
                result.goal_states[state]) {
                continue;
            }
            ++working_states;
            if (result.policy[state].index != witness.operator_index ||
                result.policy[state].kind != planner.kind ||
                !action_legal(session, descriptor, calc.state(state))) {
                gap("gated primitive renewal policy changed on a "
                    "reachable carrier");
            }
            std::vector<std::uint64_t> signature;
            if (!calc.exact_reforge_kernel_signature(
                    state, witness.primitive_action, signature) ||
                signature != witness.kernel_signature) {
                gap("gated primitive renewal kernel signature changed");
            }
        }
        if (working_states == 0 ||
            working_states !=
                witness.validated_non_goal_states) {
            gap("gated primitive renewal reachable domain changed");
        }
        const double value_tolerance =
            1e-9 * std::max(1.0, std::abs(witness.value));
        if (!std::isfinite(result.evaluated_policy_cost) ||
            std::abs(
                result.evaluated_policy_cost -
                witness.value) > value_tolerance ||
            !std::isfinite(result.upper_bound) ||
            std::abs(
                result.upper_bound - witness.value) >
                value_tolerance) {
            gap("gated primitive renewal value changed");
        }

        pc_item_state start_item;
        if (result.has_exact_start_item) {
            start_item = result.exact_start_item;
        } else if (!calc.materialize(result.start_state, start_item)) {
            gap("gated primitive renewal start cannot be materialized");
        }
        std::vector<std::string> goal_parts{
            rarity_condition(calc.goal().rarity)};
        std::vector<std::string> satisfied;
        satisfied.reserve(vocabulary.size());
        for (const SlotVocabulary& slot : vocabulary) {
            satisfied.push_back(slot.satisfied);
        }
        goal_parts.push_back(
            calc.goal().required_satisfied_slots() ==
                    satisfied.size()
                ? all_of(satisfied)
                : at_least(
                      calc.goal().required_satisfied_slots(),
                      satisfied));
        const std::string goal_condition = all_of(goal_parts);

        std::string json =
            "{\"version\":\"v1\",\"name\":\"" +
            json_escape(name) +
            "\",\"description\":\"Bounded executable fixed "
            "destructive-renewal policy exact within the "
            "zero-progress-reroll restriction; not a global optimum\","
            "\"base_state\":{\"base_key\":\"" +
            json_escape(
                data.string_at(
                    data.base_metadata_path_sid[session.base_index])) +
            "\",\"item_level\":" +
            std::to_string(session.item_level) +
            ",\"rarity\":\"" + rarity_name(start_item.rarity) + "\"";
        std::uint32_t item_flags = 0;
        if (start_item.item_flags & PC_ITEM_CORRUPTED) {
            item_flags |= PC_ITEM_CORRUPTED;
        }
        if (start_item.item_flags & PC_ITEM_MIRRORED) {
            item_flags |= PC_ITEM_MIRRORED;
        }
        if (start_item.item_flags & PC_ITEM_SPLIT) {
            item_flags |= PC_ITEM_SPLIT;
        }
        if (start_item.item_flags & PC_ITEM_SYNTHESISED) {
            item_flags |= PC_ITEM_SYNTHESISED;
        }
        json += ",\"item_flags\":" + std::to_string(item_flags);
        json += ",\"generic_influence_bits\":" +
                std::to_string(start_item.generic_influence_bits);
        json += ",\"searing_exarch_tier\":" +
                std::to_string(start_item.searing_exarch_tier);
        json += ",\"eater_of_worlds_tier\":" +
                std::to_string(start_item.eater_of_worlds_tier);
        const auto append_start_mods =
            [&](const char* field, const pc_mod_slot* mods,
                const std::uint8_t count) {
                json += ",\"";
                json += field;
                json += "\":[";
                for (std::uint8_t i = 0; i < count; ++i) {
                    if (i != 0) json += ',';
                    json += "{\"mod_key\":\"" +
                            json_escape(mod_key_of(
                                session, mods[i].mod_id)) + "\"";
                    if ((mods[i].flags &
                         PC_MOD_SLOT_FRACTURED) != 0) {
                        json += ",\"fractured\":true";
                    }
                    if ((mods[i].flags &
                         PC_MOD_SLOT_CRAFTED) != 0) {
                        json += ",\"crafted\":true";
                    }
                    if ((mods[i].flags &
                         PC_MOD_SLOT_VEILED) != 0) {
                        json += ",\"veiled\":true";
                    }
                    json += '}';
                }
                json += ']';
            };
        append_start_mods(
            "prefixes", start_item.prefixes,
            start_item.prefix_count);
        append_start_mods(
            "suffixes", start_item.suffixes,
            start_item.suffix_count);
        json +=
            "},\"start_node_id\":\"start\",\"nodes\":["
            "{\"id\":\"start\",\"kind\":\"start\"},"
            "{\"id\":\"router\",\"kind\":\"router\"},"
            "{\"id\":\"goal\",\"kind\":\"terminal\","
            "\"terminal\":\"success\"},"
            "{\"id\":\"renewal\",\"kind\":\"operation\","
            "\"expected_cost\":" +
            number(witness.value) +
            ",\"operation\":" +
            operation_json(session, descriptor) +
            "}],\"edges\":["
            "{\"id\":\"e0\",\"from\":\"start\",\"to\":\"router\","
            "\"priority\":0,\"is_default\":true},"
            "{\"id\":\"e1\",\"from\":\"router\",\"to\":\"goal\","
            "\"priority\":0,\"condition\":" +
            goal_condition +
            "},"
            "{\"id\":\"e2\",\"from\":\"router\",\"to\":\"renewal\","
            "\"priority\":1,\"is_default\":true},"
            "{\"id\":\"e3\",\"from\":\"renewal\",\"to\":\"router\","
            "\"priority\":0,\"is_default\":true}]}";
        constexpr std::uint32_t kNodes = 4;
        constexpr std::uint32_t kEdges = 4;
        if (kNodes > result.options.max_compiled_nodes) {
            if (telemetry != nullptr) {
                telemetry->cap_hit = "max_compiled_nodes";
            }
            gap("compiled fixed renewal exceeded max_compiled_nodes");
        }
        if (kEdges > result.options.max_compiled_edges) {
            if (telemetry != nullptr) {
                telemetry->cap_hit = "max_compiled_edges";
            }
            gap("compiled fixed renewal exceeded max_compiled_edges");
        }
        if (json.size() > strategy_json_limit) {
            if (telemetry != nullptr) {
                telemetry->cap_hit = "max_strategy_json_bytes";
            }
            gap("compiled fixed renewal exceeded "
                "max_strategy_json_bytes");
        }
        std::uint64_t complete_owned = owned_string_bytes(json);
        add_owned_bytes(
            complete_owned, owned_string_bytes(goal_condition));
        add_owned_bytes(
            complete_owned,
            static_cast<std::uint64_t>(vocabulary.capacity()) *
                sizeof(SlotVocabulary));
        for (const SlotVocabulary& slot : vocabulary) {
            add_owned_bytes(
                complete_owned, owned_string_bytes(slot.member));
            add_owned_bytes(
                complete_owned, owned_string_bytes(slot.satisfied));
        }
        observe_complete_compiler_owned(
            owned_string_bytes(json), complete_owned);
        if (telemetry != nullptr) {
            telemetry->working_states =
                static_cast<std::uint32_t>(working_states);
            telemetry->behavioral_classes = 1;
            telemetry->policy_regions = 1;
            telemetry->nodes = kNodes;
            telemetry->edges = kEdges;
            telemetry->strategy_json_bytes = json.size();
            telemetry->total_condition_bytes = goal_condition.size();
            telemetry->max_condition_bytes = goal_condition.size();
        }
        return json;
    }

    /* Collect and validate the policy-reachable working states. */
    std::vector<std::uint32_t> compiled_states;
    for (std::uint32_t state_id = 0; state_id < result.values.size();
         ++state_id) {
        if (!result.policy_reachable[state_id] ||
            result.goal_states[state_id]) {
            continue;
        }
        if (result.policy[state_id] == kNoId) {
            gap("policy-reachable state " + std::to_string(state_id) +
                " has no action");
        }
        if (result.policy[state_id] >= calc.operators().size()) {
            gap("policy-reachable state " + std::to_string(state_id) +
                " has an unknown planner operator");
        }
        const PlannerOperator& selected =
            calc.operators().at(result.policy[state_id]);
        if (result.policy[state_id].kind != selected.kind) {
            gap("policy-reachable state " + std::to_string(state_id) +
                " has a mismatched planner-operator tag");
        }
        compiled_states.push_back(state_id);
    }
    if (compiled_states.empty()) gap("policy reaches no working states");

    const auto gated_primitive_reforge =
        [&](const std::uint32_t state_id) {
        if (!result.options.goal_progress_gated_reforges) return false;
        const PlannerOperator& planner =
            calc.operators().at(result.policy[state_id]);
        if (planner.kind != PlannerOperatorKind::Primitive) return false;
        const ActionDescriptor& action =
            calc.registry().actions.at(planner.primitive_action);
        return !action.synthetic &&
               action_transition_facts(action.params.type).renewal;
    };

    /* Abstract-state ids follow transition discovery order, which is allowed
     * to differ between native and WASM. Canonicalize the compile order and
     * emitted node ids by the exact state predicate so policy compression
     * produces the same document for the same solved policy. */
    std::map<std::uint32_t, std::string> state_conditions;
    std::map<std::uint32_t, std::vector<std::uint32_t>> quotient_members;
    std::uint64_t retained_compile_condition_bytes = 0;
    const bool structured_refined_route =
        refined_routing != nullptr;
    std::map<
        std::uint32_t,
        const refinement::RefinedPolicyCompileClass*>
        refined_class_by_representative;
    std::vector<std::string> refined_condition_by_class;
    std::map<std::uint32_t, std::vector<std::uint32_t>>
        refined_classes_by_coarse_parent;
    std::vector<std::uint32_t> refined_parent_order;
    std::map<std::uint32_t, std::string>
        refined_parent_condition;
    std::map<std::uint32_t, std::string>
        refined_parent_router;
    const auto refined_route_owned_bytes = [&]() {
        std::uint64_t bytes = 0;
        add_owned_bytes(
            bytes,
            static_cast<std::uint64_t>(
                refined_condition_by_class.capacity()) *
                sizeof(std::string));
        for (const std::string& condition :
             refined_condition_by_class) {
            add_owned_bytes(
                bytes, owned_string_bytes(condition));
        }
        add_owned_bytes(
            bytes,
            static_cast<std::uint64_t>(
                refined_parent_order.capacity()) *
                sizeof(std::uint32_t));
        add_owned_bytes(
            bytes,
            static_cast<std::uint64_t>(
                refined_class_by_representative.size()) *
                (sizeof(std::uint32_t) +
                 sizeof(const refinement::RefinedPolicyCompileClass*) +
                 3 * sizeof(void*)));
        add_owned_bytes(
            bytes,
            static_cast<std::uint64_t>(
                refined_classes_by_coarse_parent.size()) *
                (sizeof(std::uint32_t) +
                 sizeof(std::vector<std::uint32_t>) +
                 3 * sizeof(void*)));
        for (const auto& [unused, class_ids] :
             refined_classes_by_coarse_parent) {
            (void)unused;
            add_owned_bytes(
                bytes,
                static_cast<std::uint64_t>(
                    class_ids.capacity()) *
                    sizeof(std::uint32_t));
        }
        const auto add_string_map =
            [&](const std::map<std::uint32_t, std::string>& values) {
                add_owned_bytes(
                    bytes,
                    static_cast<std::uint64_t>(values.size()) *
                        (sizeof(std::uint32_t) +
                         sizeof(std::string) +
                         3 * sizeof(void*)));
                for (const auto& [unused, value] : values) {
                    (void)unused;
                    add_owned_bytes(
                        bytes, owned_string_bytes(value));
                }
            };
        add_string_map(refined_parent_condition);
        add_string_map(refined_parent_router);
        return bytes;
    };
    if (structured_refined_route) {
        if (result.policy.size() != result.values.size() ||
            result.expanded.size() != result.values.size() ||
            result.goal_states.size() != result.values.size() ||
            result.policy_reachable.size() != result.values.size() ||
            result.behavioral_representative_by_state.size() !=
                result.values.size()) {
            gap(
                "refined policy compile sidecar disagrees with the "
                "strict state table");
        }
        refined_condition_by_class.resize(
            refined_routing->classes.size());
        std::vector<std::uint32_t> refined_member_owner(
            result.values.size(), kNoId);
        std::vector<std::vector<std::uint64_t>>
            refined_operator_key_by_class(
                refined_routing->classes.size());
        std::vector<std::vector<std::uint64_t>>
            refined_recipe_by_class(
                refined_routing->classes.size());
        for (std::size_t class_index = 0;
             class_index < refined_routing->classes.size();
             ++class_index) {
            const refinement::RefinedPolicyCompileClass& policy_class =
                refined_routing->classes[class_index];
            const bool members_are_canonical =
                std::is_sorted(
                    policy_class.strict_members.begin(),
                    policy_class.strict_members.end()) &&
                std::adjacent_find(
                    policy_class.strict_members.begin(),
                    policy_class.strict_members.end()) ==
                    policy_class.strict_members.end() &&
                std::all_of(
                    policy_class.strict_members.begin(),
                    policy_class.strict_members.end(),
                    [&](std::uint32_t member) {
                        return member < result.values.size();
                    });
            if (policy_class.class_id != class_index ||
                policy_class.coarse_state == kNoId ||
                policy_class.representative_state >=
                    result.values.size() ||
                policy_class.strict_members.empty() ||
                !members_are_canonical ||
                !std::binary_search(
                    policy_class.strict_members.begin(),
                    policy_class.strict_members.end(),
                    policy_class.representative_state)) {
                gap(
                    "refined policy compile sidecar is not canonical");
            }
            const auto [unused, inserted] =
                refined_class_by_representative.emplace(
                    policy_class.representative_state,
                    &policy_class);
            (void)unused;
            if (!inserted) {
                gap(
                    "refined policy compile sidecar repeats a "
                    "representative state");
            }
            for (const std::uint32_t member :
                 policy_class.strict_members) {
                if (refined_member_owner[member] != kNoId) {
                    gap(
                        "refined policy compile sidecar overlaps strict "
                        "member classes");
                }
                refined_member_owner[member] =
                    policy_class.class_id;
            }
            refined_condition_by_class[policy_class.class_id] =
                observation_signature_condition(
                    session,
                    layout,
                    policy_class.required_observations,
                    policy_class.observation_signature);
            if (!policy_class.terminal) {
                refined_classes_by_coarse_parent[
                    policy_class.coarse_state]
                        .push_back(policy_class.class_id);
            }
            observe_compiler_owned(
                refined_route_owned_bytes());
            for (const refinement::ProjectedTransition& transition :
                 policy_class.transitions) {
                if (transition.successor_class >=
                    refined_routing->classes.size()) {
                    gap(
                        "refined route transition has an invalid "
                        "successor");
                }
            }
        }

        for (std::uint32_t state = 0;
             state < result.values.size(); ++state) {
            const bool represented =
                result.expanded[state] != 0;
            const std::uint32_t owner =
                refined_member_owner[state];
            if (represented != (owner != kNoId)) {
                gap(
                    represented
                        ? "refined policy compile sidecar omits a "
                          "represented strict member"
                        : "refined policy compile sidecar contains an "
                          "unrepresented strict member");
            }
            if (owner == kNoId) continue;
            const refinement::RefinedPolicyCompileClass& policy_class =
                refined_routing->classes.at(owner);
            if (result.behavioral_representative_by_state[state] !=
                    policy_class.representative_state ||
                (result.goal_states[state] != 0) !=
                    policy_class.terminal) {
                gap(
                    "refined policy compile sidecar disagrees with its "
                    "strict assignment");
            }
        }

        /*
         * The shared refinement engine deliberately scopes selected-action
         * separation to one coarse policy location. Preserve that control
         * flow in the executable graph instead of flattening every exact
         * subclass onto one router. A parent guard is the union of all
         * policy-reachable strict semantic states represented by that coarse
         * location; no representative modifier identity is selected.
         *
         * These guards are re-evaluated after every operation. An action that
         * destroys an observed feature therefore routes directly into the
         * collapsed successor parent, while a preserving action retains only
         * the semantic strict class that actually survives.
         */
        std::map<
            refinement::StableKey,
            std::uint32_t>
            parent_by_stable_key;
        std::vector<
            std::pair<refinement::StableKey, std::uint32_t>>
            ordered_parents;
        std::map<std::string, std::uint32_t>
            globally_routed_member_owner;
        const auto parent_build_owned_bytes =
            [&](const std::vector<std::string>* current_conditions) {
                std::uint64_t bytes =
                    refined_route_owned_bytes();
                add_owned_bytes(
                    bytes,
                    static_cast<std::uint64_t>(
                        parent_by_stable_key.size()) *
                        (sizeof(refinement::StableKey) +
                         sizeof(std::uint32_t) +
                         3 * sizeof(void*)));
                for (const auto& [key, unused] :
                     parent_by_stable_key) {
                    (void)unused;
                    add_owned_bytes(
                        bytes,
                        static_cast<std::uint64_t>(
                            key.capacity()) *
                            sizeof(std::uint64_t));
                }
                add_owned_bytes(
                    bytes,
                    static_cast<std::uint64_t>(
                        ordered_parents.capacity()) *
                        sizeof(ordered_parents.front()));
                for (const auto& [key, unused] :
                     ordered_parents) {
                    (void)unused;
                    add_owned_bytes(
                        bytes,
                        static_cast<std::uint64_t>(
                            key.capacity()) *
                            sizeof(std::uint64_t));
                }
                add_owned_bytes(
                    bytes,
                    static_cast<std::uint64_t>(
                        globally_routed_member_owner.size()) *
                        (sizeof(std::string) +
                         sizeof(std::uint32_t) +
                         3 * sizeof(void*)));
                for (const auto& [condition, unused] :
                     globally_routed_member_owner) {
                    (void)unused;
                    add_owned_bytes(
                        bytes, owned_string_bytes(condition));
                }
                if (current_conditions != nullptr) {
                    add_owned_bytes(
                        bytes,
                        static_cast<std::uint64_t>(
                            current_conditions->capacity()) *
                            sizeof(std::string));
                    for (const std::string& condition :
                         *current_conditions) {
                        add_owned_bytes(
                            bytes,
                            owned_string_bytes(condition));
                    }
                }
                return bytes;
            };
        for (const auto& [coarse_state, class_ids] :
             refined_classes_by_coarse_parent) {
            const refinement::StableKey& coarse_state_key =
                refined_routing->classes.at(class_ids.front())
                    .coarse_state_key;
            if (coarse_state_key.empty()) {
                gap(
                    "refined coarse parent has no deterministic "
                    "semantic key");
            }
            for (const std::uint32_t class_id : class_ids) {
                if (refined_routing->classes.at(class_id)
                        .coarse_state_key != coarse_state_key) {
                    gap(
                        "refined coarse parent has inconsistent "
                        "semantic keys");
                }
            }
            const auto [same_key, inserted_key] =
                parent_by_stable_key.emplace(
                    coarse_state_key, coarse_state);
            if (!inserted_key &&
                same_key->second != coarse_state) {
                gap(
                    "distinct refined coarse parents share one "
                    "semantic key");
            }

            std::vector<std::string> member_conditions;
            std::optional<bool> retry_basin;
            for (const std::uint32_t class_id : class_ids) {
                const refinement::RefinedPolicyCompileClass& policy_class =
                    refined_routing->classes.at(class_id);
                for (const std::uint32_t member :
                     policy_class.strict_members) {
                    const bool member_retry_basin =
                        calc.state(member).goal_progress_retry_basin != 0;
                    if (retry_basin.has_value() &&
                        *retry_basin != member_retry_basin) {
                        gap(
                            "refined coarse parent mixes ordinary and "
                            "retry-basin control states");
                    }
                    retry_basin = member_retry_basin;
                    const std::string condition =
                        abstract_state_condition(
                            session, layout, vocabulary,
                            calc.state(member));
                    if (!member_retry_basin) {
                        const auto [owner, inserted] =
                            globally_routed_member_owner.emplace(
                                condition, coarse_state);
                        if (!inserted &&
                            owner->second != coarse_state) {
                            gap(
                                "overlapping refined coarse-parent "
                                "guards");
                        }
                    }
                    member_conditions.push_back(condition);
                }
            }
            std::sort(
                member_conditions.begin(), member_conditions.end());
            member_conditions.erase(
                std::unique(
                    member_conditions.begin(),
                    member_conditions.end()),
                member_conditions.end());
            if (member_conditions.empty()) {
                gap(
                    "refined coarse parent has no represented strict "
                    "member");
            }
            /*
             * Retry-basin identity is policy memory, not an observable item
             * predicate. It is entered only through the gated reforge's local
             * control-flow edge, so publishing it on the global item router
             * would overlap the ordinary parent with identical item state.
             */
            if (!retry_basin.value_or(false)) {
                refined_parent_condition.emplace(
                    coarse_state, any_of(member_conditions));
                ordered_parents.emplace_back(
                    coarse_state_key, coarse_state);
            }
            observe_compiler_owned(
                parent_build_owned_bytes(&member_conditions));
        }
        std::sort(ordered_parents.begin(), ordered_parents.end());
        std::uint32_t parent_router_index = 0;
        for (const auto& [unused_key, coarse_state] :
             ordered_parents) {
            (void)unused_key;
            refined_parent_order.push_back(coarse_state);
            refined_parent_router.emplace(
                coarse_state,
                "refined_parent_" +
                    std::to_string(parent_router_index++));
        }
        observe_compiler_owned(
            parent_build_owned_bytes(nullptr));

        const auto fixed_option_recipe =
            [&](const std::uint32_t member,
                const std::uint32_t operator_index,
                const PlannerOperator& planner) {
                const OptionKernel& kernel =
                    calc.option_kernel(member, operator_index);
                const std::vector<ObservedUnveilPreference>
                    empty_preferences;
                const std::vector<ObservedUnveilPreference>&
                    preferences =
                        member <
                                result
                                    .option_unveil_preferences
                                    .size()
                            ? result
                                  .option_unveil_preferences[
                                      member]
                            : empty_preferences;
                return fixed_option_executable_recipe_key(
                    fixed_option_executable_recipe(
                        calc, member, planner, kernel,
                        preferences));
            };
        const auto primitive_recipe =
            [&](const std::uint32_t member,
                const PlannerOperator& planner) {
                std::vector<std::uint64_t> recipe{
                    0x70637072696d7231ull}; /* "pcprimr1" */
                if (planner.primitive_action >=
                    calc.registry().actions.size()) {
                    gap(
                        "refined primitive operator is outside the "
                        "strict action registry");
                }
                if (!primitive_observes_modifier_offer(planner)) {
                    return recipe;
                }
                if (member >= result.unveil_preferences.size() ||
                    result.unveil_preferences[member].empty()) {
                    gap(
                        "refined observed-choice member has no populated "
                        "preference sidecar");
                }
                std::set<std::uint32_t> seen_mods;
                for (const std::uint32_t mod :
                     result.unveil_preferences[member]) {
                    if (mod >= session.mod_count ||
                        !seen_mods.insert(mod).second) {
                        gap(
                            "refined observed-choice preference has an invalid "
                            "or repeated modifier");
                    }
                    recipe.push_back(mod);
                }
                return recipe;
            };

        for (const refinement::RefinedPolicyCompileClass& policy_class :
             refined_routing->classes) {
            if (policy_class.terminal) continue;
            if (!policy_class.selected_action.has_value()) {
                gap(
                    "refined non-terminal route has no selected "
                    "action");
            }
            const auto representative =
                refined_class_by_representative.find(
                    policy_class.representative_state);
            if (representative ==
                    refined_class_by_representative.end() ||
                std::find(
                    compiled_states.begin(),
                    compiled_states.end(),
                    policy_class.representative_state) ==
                    compiled_states.end()) {
                gap(
                    "refined working class is absent from the lifted "
                    "policy");
            }
            const refinement::SelectedAction& selected =
                *policy_class.selected_action;
            if (selected.action_id >= calc.operators().size() ||
                selected.semantic_key.empty()) {
                gap(
                    "refined route has an invalid semantic operator "
                    "decision");
            }
            const PlannerOperator& planner =
                calc.operators().at(selected.action_id);
            const std::vector<std::uint64_t> operator_key =
                planner_operator_semantic_key(planner);
            if (operator_key.empty()) {
                gap(
                    "refined route operator has no semantic key");
            }
            refined_operator_key_by_class[policy_class.class_id] =
                operator_key;

            std::optional<std::vector<std::uint64_t>>
                representative_recipe;
            for (const std::uint32_t member :
                 policy_class.strict_members) {
                const PolicyOperatorRef policy =
                    result.policy[member];
                if (policy.index != selected.action_id ||
                    policy.kind != planner.kind ||
                    policy.index >= calc.operators().size() ||
                    planner_operator_semantic_key(
                        calc.operators().at(policy.index)) !=
                        operator_key) {
                    gap(
                        "refined route member operator disagrees with "
                        "the selected strict decision");
                }
                const std::vector<std::uint64_t> recipe =
                    planner.kind ==
                            PlannerOperatorKind::FixedOption
                        ? fixed_option_recipe(
                              member, selected.action_id,
                              planner)
                        : primitive_recipe(member, planner);
                if (!representative_recipe.has_value()) {
                    representative_recipe = recipe;
                } else if (*representative_recipe != recipe) {
                    gap(
                        "refined route members have different "
                        "executable operator recipes");
                }
            }
            refined_recipe_by_class[policy_class.class_id] =
                *representative_recipe;
        }

        const auto same_refined_decision =
            [&](const refinement::RefinedPolicyCompileClass& left,
                const refinement::RefinedPolicyCompileClass& right) {
                return left.selected_action.has_value() &&
                       right.selected_action.has_value() &&
                       left.selected_action->action_id ==
                           right.selected_action->action_id &&
                       left.selected_action->semantic_key ==
                           right.selected_action->semantic_key &&
                       refined_operator_key_by_class[left.class_id] ==
                           refined_operator_key_by_class[
                               right.class_id] &&
                       refined_recipe_by_class[left.class_id] ==
                           refined_recipe_by_class[
                               right.class_id];
            };
        std::map<
            std::pair<std::uint32_t, std::string>,
            std::uint32_t>
            first_class_by_condition;
        for (const refinement::RefinedPolicyCompileClass& policy_class :
             refined_routing->classes) {
            if (policy_class.terminal) continue;
            const std::string& condition =
                refined_condition_by_class[policy_class.class_id];
            const auto [found, inserted] =
                first_class_by_condition.emplace(
                    std::pair{
                        policy_class.coarse_state, condition},
                    policy_class.class_id);
            if (inserted) continue;
            const refinement::RefinedPolicyCompileClass& first_class =
                refined_routing->classes.at(found->second);
            if (!same_refined_decision(
                    first_class, policy_class)) {
                gap(
                    "indistinguishable_refined_policy_actions");
            }
        }

        /*
         * A policy router only decides the next executable operation. Exact
         * classes with different values or projected kernels may therefore
         * overlap when every matching predicate selects the same semantic
         * action: whichever equal-action edge wins, the operation is
         * identical and successor routing is evaluated again on the concrete
         * result. An overlap between different semantic actions is not
         * executable and remains a hard compiler assertion.
         */
        for (const refinement::RefinedPolicyCompileClass& owner :
             refined_routing->classes) {
            if (owner.terminal) continue;
            for (const std::uint32_t member :
                 owner.strict_members) {
                if (member >= result.values.size()) {
                    gap(
                        "refined route member is outside the strict "
                        "state table");
                }
                bool matched_owner_condition = false;
                bool conflicting_decision = false;
                for (const refinement::RefinedPolicyCompileClass&
                         candidate : refined_routing->classes) {
                    if (candidate.terminal ||
                        candidate.coarse_state !=
                            owner.coarse_state) {
                        continue;
                    }
                    const refinement::AbstractFeatureExtraction
                        extraction =
                            refinement::
                                extract_strict_abstract_features(
                                    session,
                                    layout,
                                    calc.state(member),
                                    candidate
                                        .required_observations);
                    if (!extraction.complete()) continue;
                    if (refinement::observe_features(
                            extraction.features,
                            candidate.required_observations) ==
                        candidate.observation_signature) {
                        matched_owner_condition =
                            matched_owner_condition ||
                            refined_condition_by_class[
                                candidate.class_id] ==
                                refined_condition_by_class[
                                    owner.class_id];
                        conflicting_decision =
                            conflicting_decision ||
                            !same_refined_decision(owner, candidate);
                    }
                }
                if (!matched_owner_condition) {
                    gap(
                        "incomplete_refined_policy_observation_signature");
                }
                if (conflicting_decision) {
                    gap(
                        "overlapping_refined_policy_actions");
                }
            }
        }
    }
    QuotientFeatureIndex feature_index;
    if (!structured_refined_route) {
        const std::size_t state_count = result.values.size();
        if (state_count != 0) {
            feature_index.width =
                quotient_feature_values(layout, calc.state(0)).size();
        }
        feature_index.values.reserve(state_count * feature_index.width);
        feature_index.non_goal_buckets.resize(feature_index.width);
        for (std::uint32_t state = 0; state < state_count; ++state) {
            if (!result.behavioral_representative_by_state.empty()) {
                quotient_members[
                    result.behavioral_representative_by_state[state]]
                    .push_back(state);
            }
            const std::vector<std::uint32_t> values =
                quotient_feature_values(layout, calc.state(state));
            if (values.size() != feature_index.width) {
                gap("quotient feature width changed within one solve");
            }
            feature_index.values.insert(
                feature_index.values.end(), values.begin(), values.end());
            if (!calc.is_goal_state(calc.state(state))) {
                ++feature_index.non_goal_states;
                for (std::size_t feature = 0;
                     feature < values.size(); ++feature) {
                    feature_index.non_goal_buckets[feature][values[feature]]
                        .push_back(state);
                }
            }
        }
    }
    for (const std::uint32_t state_id : compiled_states) {
        if (structured_refined_route) {
            const auto policy_class =
                refined_class_by_representative.find(state_id);
            if (policy_class ==
                    refined_class_by_representative.end() ||
                policy_class->second->terminal) {
                gap(
                    "lifted working state has no refined routing "
                    "class");
            }
            state_conditions.emplace(
                state_id,
                refined_condition_by_class[
                    policy_class->second->class_id]);
        } else if (!result.behavioral_representative_by_state.empty()) {
            state_conditions.emplace(
                state_id,
                quotient_class_condition(
                    calc, vocabulary, state_id,
                    quotient_members.at(state_id), feature_index,
                    result.behavioral_representative_by_state,
                    telemetry == nullptr
                        ? nullptr
                        : &telemetry->exact_state_fallbacks));
        }
    }
    if (structured_refined_route) {
        retained_compile_condition_bytes =
            refined_route_owned_bytes();
        add_owned_bytes(
            retained_compile_condition_bytes,
            static_cast<std::uint64_t>(
                state_conditions.size()) *
                (sizeof(std::uint32_t) +
                 sizeof(std::string) +
                 3 * sizeof(void*)));
        for (const auto& [unused, condition] :
             state_conditions) {
            (void)unused;
            add_owned_bytes(
                retained_compile_condition_bytes,
                owned_string_bytes(condition));
        }
        observe_compiler_owned(
            retained_compile_condition_bytes);
    }
    std::sort(
        compiled_states.begin(), compiled_states.end(),
        [&](const std::uint32_t left, const std::uint32_t right) {
            if (structured_refined_route ||
                !result.behavioral_representative_by_state.empty()) {
                const std::string& left_condition = state_conditions.at(left);
                const std::string& right_condition = state_conditions.at(right);
                if (left_condition != right_condition) {
                    return left_condition < right_condition;
                }
            } else {
                const std::uint32_t* left_values = feature_index.row(left);
                const std::uint32_t* right_values = feature_index.row(right);
                for (std::size_t feature = 0;
                     feature < feature_index.width; ++feature) {
                    if (left_values[feature] != right_values[feature]) {
                        return left_values[feature] < right_values[feature];
                    }
                }
            }
            if (result.policy[left].index != result.policy[right].index) {
                return result.policy[left].index < result.policy[right].index;
            }
            const std::string left_cost = number(result.values[left]);
            const std::string right_cost = number(result.values[right]);
            if (left_cost != right_cost) return left_cost < right_cost;
            return left < right;
        });
    std::vector<std::uint32_t> canonical_state_ids(
        result.values.size(), kNoId);
    for (std::uint32_t i = 0; i < compiled_states.size(); ++i) {
        canonical_state_ids[compiled_states[i]] = i;
    }
    const auto state_node = [&](const std::uint32_t state_id) {
        if (state_id >= canonical_state_ids.size() ||
            canonical_state_ids[state_id] == kNoId) {
            gap("policy node requested for a non-working state");
        }
        return "s" + std::to_string(canonical_state_ids[state_id]);
    };

    /* Exact policy-region compression. States may share one emitted
     * continuation when the selected program is state-independent. Expected
     * cost is only an annotation, so omit it for a shared region whose member
     * values differ. Observation-owned and state-local retry options remain
     * singleton regions so their concrete routing recipes cannot be
     * conflated. */
    std::map<std::string, std::vector<std::uint32_t>> leaders_by_key;
    std::map<std::uint32_t, std::vector<std::uint32_t>> states_by_leader;
    std::vector<std::uint32_t> emitted_states;
    for (const std::uint32_t state_id : compiled_states) {
        const PlannerOperator& planner =
            calc.operators().at(result.policy[state_id]);
        const bool primitive_observed_choice =
            primitive_observes_modifier_offer(planner);
        const bool product_local_fracture =
            calc.product_solver_parent() &&
            planner.kind == PlannerOperatorKind::Primitive &&
            planner.automatic_kind == AutomaticCandidateKind::Fracture;
        const bool state_local_option =
            planner.kind == PlannerOperatorKind::FixedOption &&
            (planner.option_kind == FixedOptionKind::Renewal ||
             planner.option_kind == FixedOptionKind::ProtectedRepeat ||
             planner.option_kind == FixedOptionKind::TemporaryBenchRepeat ||
             planner.option_kind == FixedOptionKind::FracturePrepare ||
             planner.option_kind == FixedOptionKind::ImprintRetry);
        std::uint32_t leader = state_id;
        if (!primitive_observed_choice && !product_local_fracture &&
            !state_local_option &&
            !gated_primitive_reforge(state_id)) {
            const std::string key =
                std::to_string(result.policy[state_id].index);
            std::vector<std::uint32_t>& leaders = leaders_by_key[key];
            if (leaders.empty()) {
                leaders.push_back(state_id);
                emitted_states.push_back(state_id);
            }
            leader = leaders.back();
        } else {
            emitted_states.push_back(state_id);
        }
        states_by_leader[leader].push_back(state_id);
    }
    std::vector<std::uint32_t> policy_region_by_state(
        result.values.size(), kNoId);
    std::map<std::uint32_t, std::optional<std::string>> region_expected_cost;
    std::uint32_t restart_region_leader = kNoId;
    for (const std::uint32_t leader : emitted_states) {
        const std::vector<std::uint32_t>& members =
            states_by_leader.at(leader);
        for (const std::uint32_t member : members) {
            policy_region_by_state[member] = leader;
        }
        const std::string first_value = number(result.values[members.front()]);
        const bool uniform = std::all_of(
            members.begin() + 1, members.end(),
            [&](const std::uint32_t member) {
                return number(result.values[member]) == first_value;
            });
        region_expected_cost.emplace(
            leader, uniform ? std::optional<std::string>{first_value}
                            : std::nullopt);
        const PlannerOperator& planner =
            calc.operators().at(result.policy[leader]);
        if (planner.kind == PlannerOperatorKind::Primitive &&
            planner.primitive_action < calc.registry().actions.size() &&
            calc.registry().actions[planner.primitive_action].synthetic) {
            if (restart_region_leader != kNoId) {
                gap("policy produced multiple Restart regions");
            }
            restart_region_leader = leader;
        }
    }
    std::map<std::uint32_t, std::uint32_t> gated_retry_state_by_state;
    for (const std::uint32_t state_id : compiled_states) {
        if (!gated_primitive_reforge(state_id)) continue;
        const PlannerOperator& planner =
            calc.operators().at(result.policy[state_id]);
        const OutcomeDistribution& distribution = calc.outcomes(
            state_id, planner.primitive_action, true);
        if (!distribution.supported ||
            !distribution.goal_progress_gated) {
            gap("selected gated reforge has no gated exact kernel");
        }
        if (!(distribution.gated_retry_probability > 0.0)) continue;
        const std::uint32_t retry_state =
            distribution.gated_retry_state;
        if (retry_state == kNoId ||
            retry_state >= result.policy_reachable.size() ||
            (!result.behavioral_representative_by_state.empty() &&
             retry_state >=
                 result.behavioral_representative_by_state.size()) ||
            calc.state(retry_state).goal_progress_retry_basin == 0) {
            gap("selected gated reforge retry basin is outside the "
                "executable policy");
        }
        const std::uint32_t retry_policy_state =
            result.behavioral_representative_by_state.empty()
                ? retry_state
                : result.behavioral_representative_by_state[
                      retry_state];
        if (retry_policy_state == kNoId ||
            retry_policy_state >= result.policy_reachable.size() ||
            !result.policy_reachable[retry_policy_state] ||
            result.goal_states[retry_policy_state] ||
            result.policy[retry_policy_state] == kNoId ||
            calc.state(retry_policy_state)
                    .goal_progress_retry_basin == 0) {
            gap("selected gated reforge retry basin is outside the "
                "executable policy");
        }
        gated_retry_state_by_state.emplace(
            state_id, retry_policy_state);
    }
    const auto expected_cost_annotation =
        [&](const std::uint32_t leader) -> std::string {
        const auto found = region_expected_cost.find(leader);
        if (found == region_expected_cost.end() ||
            !found->second.has_value()) {
            return {};
        }
        return ",\"expected_cost\":" + *found->second;
    };

    /* Spell the exact policy domain as a compressed decision tree over the
     * existing v1 condition vocabulary. A region-wide DNF repeats the complete
     * state predicate tens of thousands of times for ordinary reforge
     * policies. This tree checks every abstract feature on every accepted path
     * and defaults to offpolicy at each branch, so sharing changes only the
     * representation, never the set of routed strict states. */
    struct PolicyRouteEntry {
        std::uint32_t state = kNoId;
        std::uint32_t leader = kNoId;
    };
    struct PolicyRouteEdge {
        std::string to;
        std::string condition;
    };
    struct PolicyRouteNode {
        std::string id;
        std::vector<PolicyRouteEdge> edges;
    };
    struct PolicyRouteBranch {
        std::string to;
        std::string guard;
    };
    std::vector<PolicyRouteEntry> policy_route_entries;
    const bool strict_policy_route =
        result.behavioral_representative_by_state.empty();
    if (strict_policy_route) {
        policy_route_entries.reserve(compiled_states.size());
        for (const std::uint32_t state : compiled_states) {
            if (calc.state(state).goal_progress_retry_basin != 0) {
                continue;
            }
            const std::uint32_t leader = policy_region_by_state.at(state);
            if (leader != restart_region_leader) {
                policy_route_entries.push_back({state, leader});
            }
        }
    }
    const bool use_exact_policy_tree =
        strict_policy_route && !policy_route_entries.empty();
    const bool bounded_policy =
        result.policy_status == SolvePolicyStatus::BoundedFeasible ||
        result.policy_status == SolvePolicyStatus::BoundedNearOptimal;
    std::uint32_t bounded_default_restart_action = kNoId;
    if (bounded_policy && restart_region_leader == kNoId) {
        for (std::uint32_t action = 0;
             action < calc.registry().actions.size(); ++action) {
            if (calc.registry().actions[action].synthetic) {
                bounded_default_restart_action = action;
                break;
            }
        }
        if (bounded_default_restart_action == kNoId) {
            gap("bounded policy has no explicit safe Restart default");
        }
    }
    std::uint32_t product_fracture_restart_action = kNoId;
    const bool selected_product_fracture = std::any_of(
        emitted_states.begin(), emitted_states.end(),
        [&](const std::uint32_t state) {
            const PlannerOperator& planner =
                calc.operators().at(result.policy[state]);
            return calc.product_solver_parent() &&
                   planner.kind == PlannerOperatorKind::Primitive &&
                   planner.automatic_kind ==
                       AutomaticCandidateKind::Fracture;
        });
    if (selected_product_fracture) {
        for (std::uint32_t action = 0;
             action < calc.registry().actions.size(); ++action) {
            if (calc.registry().actions[action].synthetic) {
                product_fracture_restart_action = action;
                break;
            }
        }
        if (product_fracture_restart_action == kNoId) {
            gap("product-local Fracture route has no Restart operation");
        }
    }
    const bool dedicated_product_fracture_restart =
        selected_product_fracture && restart_region_leader == kNoId &&
        bounded_default_restart_action == kNoId;
    const std::string product_fracture_restart_node =
        restart_region_leader != kNoId
            ? state_node(restart_region_leader)
            : bounded_default_restart_action != kNoId
                  ? "bounded_default_restart"
                  : "product_fracture_restart";
    const std::string policy_route_default_node =
        strict_policy_route && restart_region_leader != kNoId
            ? state_node(restart_region_leader)
            : bounded_policy ? "bounded_default_restart" : "offpolicy";
    std::vector<std::map<std::uint32_t, std::string>>
        feature_condition_cache(feature_index.width);
    const auto feature_condition =
        [&](const std::uint32_t state,
            const std::size_t feature) -> const std::string& {
        const std::uint32_t value = feature_index.at(state, feature);
        auto& cache = feature_condition_cache.at(feature);
        auto found = cache.find(value);
        if (found != cache.end()) return found->second;
        const std::vector<QuotientFeature> features = quotient_features(
            session, layout, vocabulary, calc.state(state));
        if (features.size() != feature_index.width) {
            gap("policy route feature width changed within one solve");
        }
        for (std::size_t index = 0; index < features.size(); ++index) {
            feature_condition_cache[index].try_emplace(
                static_cast<std::uint32_t>(features[index].value),
                features[index].condition);
        }
        return feature_condition_cache.at(feature).at(value);
    };
    std::vector<PolicyRouteNode> policy_route_nodes;
    std::map<std::string, std::string> policy_route_node_by_signature;
    std::vector<std::size_t> route_features(feature_index.width);
    for (std::size_t feature = 0; feature < route_features.size(); ++feature) {
        route_features[feature] = feature;
    }
    std::function<PolicyRouteBranch(
        const std::vector<PolicyRouteEntry>&,
        const std::vector<std::size_t>&)> build_policy_route;
    build_policy_route =
        [&](const std::vector<PolicyRouteEntry>& entries,
            const std::vector<std::size_t>& features) -> PolicyRouteBranch {
        if (entries.empty()) gap("empty exact policy route partition");
        std::vector<std::string> constants;
        std::vector<std::size_t> varying;
        varying.reserve(features.size());
        for (const std::size_t feature : features) {
            const std::uint32_t first =
                feature_index.at(entries.front().state, feature);
            const bool constant = std::all_of(
                entries.begin() + 1, entries.end(),
                [&](const PolicyRouteEntry& entry) {
                    return feature_index.at(entry.state, feature) == first;
                });
            if (constant) {
                constants.push_back(
                    feature_condition(entries.front().state, feature));
            } else {
                varying.push_back(feature);
            }
        }
        const std::string guard = all_of(constants);
        if (varying.empty()) {
            const std::uint32_t leader = entries.front().leader;
            if (!std::all_of(
                    entries.begin() + 1, entries.end(),
                    [&](const PolicyRouteEntry& entry) {
                        return entry.leader == leader;
                    })) {
                gap("identical exact policy states select different regions");
            }
            return {state_node(leader), guard};
        }

        /* Prefer the widest, then most balanced exact partition. This keeps
         * the router shallow without assigning semantic meaning to hashes or
         * state discovery ids. */
        std::size_t selected = varying.front();
        std::size_t selected_distinct = 0;
        std::uint64_t selected_square_sum =
            std::numeric_limits<std::uint64_t>::max();
        for (const std::size_t feature : varying) {
            std::map<std::uint32_t, std::uint32_t> counts;
            for (const PolicyRouteEntry& entry : entries) {
                ++counts[feature_index.at(entry.state, feature)];
            }
            std::uint64_t square_sum = 0;
            for (const auto& [value, count] : counts) {
                (void)value;
                square_sum += static_cast<std::uint64_t>(count) * count;
            }
            if (counts.size() > selected_distinct ||
                (counts.size() == selected_distinct &&
                 square_sum < selected_square_sum)) {
                selected = feature;
                selected_distinct = counts.size();
                selected_square_sum = square_sum;
            }
        }
        std::vector<std::size_t> child_features;
        child_features.reserve(varying.size() - 1);
        for (const std::size_t feature : varying) {
            if (feature != selected) child_features.push_back(feature);
        }
        std::map<std::uint32_t, std::vector<PolicyRouteEntry>> groups;
        for (const PolicyRouteEntry& entry : entries) {
            groups[feature_index.at(entry.state, selected)].push_back(entry);
        }
        std::vector<PolicyRouteEdge> edges;
        edges.reserve(groups.size());
        for (auto& [value, members] : groups) {
            (void)value;
            const PolicyRouteBranch child =
                build_policy_route(members, child_features);
            edges.push_back({
                child.to,
                all_of({feature_condition(
                            members.front().state, selected),
                        child.guard})});
        }
        std::string signature;
        for (const PolicyRouteEdge& route_edge : edges) {
            signature += std::to_string(route_edge.to.size()) + ":" +
                         route_edge.to + ":" +
                         std::to_string(route_edge.condition.size()) + ":" +
                         route_edge.condition + ";";
        }
        const auto shared = policy_route_node_by_signature.find(signature);
        if (shared != policy_route_node_by_signature.end()) {
            return {shared->second, guard};
        }
        const std::string node_id =
            "policy_route_" + std::to_string(policy_route_nodes.size());
        policy_route_nodes.push_back({node_id, std::move(edges)});
        policy_route_node_by_signature.emplace(std::move(signature), node_id);
        if (policy_route_nodes.size() + 4 >
            result.options.max_compiled_nodes) {
            if (telemetry != nullptr) telemetry->cap_hit = "max_compiled_nodes";
            gap("exact policy router exceeded max_compiled_nodes (" +
                std::to_string(result.options.max_compiled_nodes) + ")");
        }
        return {node_id, guard};
    };
    PolicyRouteBranch policy_route_root;
    if (use_exact_policy_tree) {
        policy_route_root =
            build_policy_route(policy_route_entries, route_features);
    }
    std::map<std::uint32_t, OptionKernel> compiled_option_kernels;
    for (const std::uint32_t state_id : compiled_states) {
        const PlannerOperator& planner =
            calc.operators().at(result.policy[state_id]);
        if (planner.kind != PlannerOperatorKind::FixedOption ||
            (planner.option_kind != FixedOptionKind::Renewal &&
             planner.option_kind != FixedOptionKind::ProtectedRepeat &&
             planner.option_kind != FixedOptionKind::TemporaryBenchRepeat &&
             planner.option_kind != FixedOptionKind::FracturePrepare &&
             planner.option_kind != FixedOptionKind::ImprintRetry)) {
            continue;
        }
        const OptionKernel& kernel = calc.option_kernel(
            state_id, result.policy[state_id].index);
        if (!kernel.supported || !kernel.legal) {
            gap("selected S7.4 option has no legal expansion kernel");
        }
        compiled_option_kernels.emplace(state_id, kernel);
    }
    const auto audited_compiler_owned_bytes =
        [&](const std::string* growing_json) {
            std::uint64_t bytes = 0;
            const auto add_u32_vector =
                [&](const std::vector<std::uint32_t>& values) {
                    add_owned_bytes(
                        bytes,
                        static_cast<std::uint64_t>(values.capacity()) *
                            sizeof(std::uint32_t));
                };
            const auto add_string = [&](const std::string& value) {
                add_owned_bytes(bytes, owned_string_bytes(value));
            };
            const auto add_map_nodes =
                [&](const std::size_t count,
                    const std::size_t payload) {
                    add_owned_bytes(
                        bytes,
                        static_cast<std::uint64_t>(count) *
                            (payload + 3 * sizeof(void*)));
                };

            add_owned_bytes(
                bytes,
                static_cast<std::uint64_t>(vocabulary.capacity()) *
                    sizeof(SlotVocabulary));
            for (const SlotVocabulary& slot : vocabulary) {
                add_string(slot.member);
                add_string(slot.satisfied);
            }
            add_u32_vector(compiled_states);
            add_map_nodes(
                state_conditions.size(),
                sizeof(std::uint32_t) + sizeof(std::string));
            for (const auto& [unused, condition] : state_conditions) {
                (void)unused;
                add_string(condition);
            }
            add_map_nodes(
                quotient_members.size(),
                sizeof(std::uint32_t) +
                    sizeof(std::vector<std::uint32_t>));
            for (const auto& [unused, members] : quotient_members) {
                (void)unused;
                add_u32_vector(members);
            }
            add_owned_bytes(
                bytes,
                static_cast<std::uint64_t>(
                    feature_index.values.capacity()) *
                    sizeof(std::uint32_t));
            add_owned_bytes(
                bytes,
                static_cast<std::uint64_t>(
                    feature_index.non_goal_buckets.capacity()) *
                    sizeof(feature_index.non_goal_buckets.front()));
            for (const auto& buckets : feature_index.non_goal_buckets) {
                add_owned_bytes(
                    bytes,
                    static_cast<std::uint64_t>(buckets.bucket_count()) *
                        sizeof(void*));
                add_map_nodes(
                    buckets.size(),
                    sizeof(std::uint32_t) +
                        sizeof(std::vector<std::uint32_t>));
                for (const auto& [unused, members] : buckets) {
                    (void)unused;
                    add_u32_vector(members);
                }
            }
            add_owned_bytes(bytes, refined_route_owned_bytes());
            add_u32_vector(canonical_state_ids);
            add_map_nodes(
                leaders_by_key.size(),
                sizeof(std::string) +
                    sizeof(std::vector<std::uint32_t>));
            for (const auto& [key, leaders] : leaders_by_key) {
                add_string(key);
                add_u32_vector(leaders);
            }
            add_map_nodes(
                states_by_leader.size(),
                sizeof(std::uint32_t) +
                    sizeof(std::vector<std::uint32_t>));
            for (const auto& [unused, states] : states_by_leader) {
                (void)unused;
                add_u32_vector(states);
            }
            add_u32_vector(emitted_states);
            add_u32_vector(policy_region_by_state);
            add_map_nodes(
                region_expected_cost.size(),
                sizeof(std::uint32_t) +
                    sizeof(std::optional<std::string>));
            for (const auto& [unused, value] : region_expected_cost) {
                (void)unused;
                if (value.has_value()) add_string(*value);
            }
            add_map_nodes(
                gated_retry_state_by_state.size(),
                2 * sizeof(std::uint32_t));
            add_owned_bytes(
                bytes,
                static_cast<std::uint64_t>(
                    policy_route_entries.capacity()) *
                    sizeof(PolicyRouteEntry));
            add_owned_bytes(
                bytes,
                static_cast<std::uint64_t>(
                    feature_condition_cache.capacity()) *
                    sizeof(feature_condition_cache.front()));
            for (const auto& cache : feature_condition_cache) {
                add_map_nodes(
                    cache.size(),
                    sizeof(std::uint32_t) + sizeof(std::string));
                for (const auto& [unused, condition] : cache) {
                    (void)unused;
                    add_string(condition);
                }
            }
            add_owned_bytes(
                bytes,
                static_cast<std::uint64_t>(
                    policy_route_nodes.capacity()) *
                    sizeof(PolicyRouteNode));
            for (const PolicyRouteNode& node : policy_route_nodes) {
                add_string(node.id);
                add_owned_bytes(
                    bytes,
                    static_cast<std::uint64_t>(node.edges.capacity()) *
                        sizeof(PolicyRouteEdge));
                for (const PolicyRouteEdge& route_edge : node.edges) {
                    add_string(route_edge.to);
                    add_string(route_edge.condition);
                }
            }
            add_map_nodes(
                policy_route_node_by_signature.size(),
                2 * sizeof(std::string));
            for (const auto& [signature, node] :
                 policy_route_node_by_signature) {
                add_string(signature);
                add_string(node);
            }
            add_owned_bytes(
                bytes,
                static_cast<std::uint64_t>(route_features.capacity()) *
                    sizeof(std::size_t));
            add_string(policy_route_root.to);
            add_string(policy_route_root.guard);
            add_map_nodes(
                compiled_option_kernels.size(),
                sizeof(std::uint32_t) + sizeof(OptionKernel));
            for (const auto& [unused, kernel] :
                 compiled_option_kernels) {
                (void)unused;
                add_owned_bytes(
                    bytes,
                    static_cast<std::uint64_t>(
                        kernel.expected_resources.capacity()) *
                        sizeof(std::pair<std::string, double>));
                for (const auto& [key, quantity] :
                     kernel.expected_resources) {
                    (void)quantity;
                    add_string(key);
                }
                add_owned_bytes(
                    bytes,
                    static_cast<std::uint64_t>(kernel.exits.capacity()) *
                        sizeof(OutcomeEntry));
                add_owned_bytes(
                    bytes,
                    static_cast<std::uint64_t>(
                        kernel.observation_choice_groups.capacity()) *
                        sizeof(OutcomeChoiceGroup));
                for (const OutcomeChoiceGroup& group :
                     kernel.observation_choice_groups) {
                    add_u32_vector(group.states);
                }
                add_owned_bytes(
                    bytes,
                    static_cast<std::uint64_t>(
                        kernel.observation_choice_options.capacity()) *
                        sizeof(OutcomeChoiceOption));
                add_u32_vector(kernel.retry_states);
                add_u32_vector(kernel.continuation_states);
                add_owned_bytes(
                    bytes,
                    static_cast<std::uint64_t>(
                        kernel.automatic_candidate_attempt_entries
                            .capacity()) *
                        sizeof(OutcomeEntry));
                add_string(kernel.automatic.legality_result);
                add_string(kernel.automatic.reason);
            }
            if (growing_json != nullptr) add_string(*growing_json);
            return bytes;
        };
    observe_complete_compiler_owned(
        retained_compile_condition_bytes,
        audited_compiler_owned_bytes(nullptr));
    std::uint32_t node_count =
        4 + static_cast<std::uint32_t>(policy_route_nodes.size()) +
        static_cast<std::uint32_t>(
            refined_parent_router.size()) +
        (bounded_default_restart_action != kNoId ? 1u : 0u) +
        (dedicated_product_fracture_restart ? 1u : 0u);
    const auto check_node_cap = [&]() {
        if (node_count > result.options.max_compiled_nodes) {
            if (telemetry != nullptr) telemetry->cap_hit = "max_compiled_nodes";
            gap("compiled policy exceeded max_compiled_nodes (" +
                std::to_string(result.options.max_compiled_nodes) + ")");
        }
    };
    check_node_cap();

    const AbstractState& start = calc.state(result.start_state);
    pc_item_state start_item;
    if (result.has_exact_start_item) {
        start_item = result.exact_start_item;
    } else if (!calc.materialize(result.start_state, start_item)) {
        gap("start state cannot be materialized exactly");
    }

    /* --- emit --------------------------------------------------------------- */
    std::string json = "{\"version\":\"v1\",\"name\":\"";
    json += json_escape(name);
    if (result.options.goal_progress_gated_reforges) {
        json +=
            "\",\"description\":\"Exact within the zero-progress-reroll "
            "policy restriction; excluded zero-progress salvage routes are "
            "not globally optimized";
    }
    json += "\",\"base_state\":{\"base_key\":\"";
    json += json_escape(
        data.string_at(data.base_metadata_path_sid[session.base_index]));
    json += "\",\"item_level\":";
    json += std::to_string(session.item_level);
    json += ",\"rarity\":\"";
    json += rarity_name(start.rarity);
    std::uint32_t item_flags = 0;
    if (start.flags & kFlagCorrupted) item_flags |= PC_ITEM_CORRUPTED;
    if (start.flags & kFlagMirrored) item_flags |= PC_ITEM_MIRRORED;
    if (start.flags & kFlagSplit) item_flags |= PC_ITEM_SPLIT;
    if (start.flags & kFlagSynthesised) item_flags |= PC_ITEM_SYNTHESISED;
    json += "\",\"item_flags\":" + std::to_string(item_flags);
    json += ",\"generic_influence_bits\":" +
            std::to_string(start.influence_bits);
    json += ",\"searing_exarch_tier\":" +
            std::to_string(start.searing_exarch_tier);
    json += ",\"eater_of_worlds_tier\":" +
            std::to_string(start.eater_of_worlds_tier);
    const auto start_mods = [&](const char* name, const pc_mod_slot* slots,
                                std::uint8_t count) {
        json += ",\"";
        json += name;
        json += "\":[";
        for (std::uint8_t i = 0; i < count; ++i) {
            if (i > 0) json += ',';
            json += "{\"mod_key\":\"";
            json += json_escape(mod_key_of(session, slots[i].mod_id));
            json += "\"";
            if ((slots[i].flags & PC_MOD_SLOT_FRACTURED) != 0) {
                json += ",\"fractured\":true";
            }
            if ((slots[i].flags & PC_MOD_SLOT_CRAFTED) != 0) {
                json += ",\"crafted\":true";
            }
            if ((slots[i].flags & PC_MOD_SLOT_VEILED) != 0) {
                json += ",\"veiled\":true";
            }
            json += '}';
        }
        json += ']';
    };
    start_mods("prefixes", start_item.prefixes, start_item.prefix_count);
    start_mods("suffixes", start_item.suffixes, start_item.suffix_count);
    json += "},\"start_node_id\":\"start\",\"nodes\":[";
    json += "{\"id\":\"start\",\"kind\":\"start\"},";
    json += "{\"id\":\"router\",\"kind\":\"router\"},";
    json +=
        "{\"id\":\"goal\",\"kind\":\"terminal\",\"terminal\":\"success\"},";
    json += "{\"id\":\"offpolicy\",\"kind\":\"terminal\",\"terminal\":"
            "\"failure\",\"reason\":\"item left the policy-reachable "
            "state set\"}";
    if (bounded_default_restart_action != kNoId) {
        json +=
            ",{\"id\":\"bounded_default_restart\",\"kind\":\"operation\",";
        json += "\"operation\":" + operation_json(
            session,
            calc.registry().actions.at(bounded_default_restart_action));
        json += "}";
    }
    if (dedicated_product_fracture_restart) {
        json +=
            ",{\"id\":\"product_fracture_restart\",\"kind\":\"operation\",";
        json += "\"operation\":" + operation_json(
            session,
            calc.registry().actions.at(product_fracture_restart_action));
        json += "}";
    }
    for (const PolicyRouteNode& route : policy_route_nodes) {
        json += ",{\"id\":\"" + route.id + "\",\"kind\":\"router\"}";
    }
    for (const std::uint32_t coarse_state :
         refined_parent_order) {
        const std::string& router_id =
            refined_parent_router.at(coarse_state);
        json += ",{\"id\":\"" + router_id +
                "\",\"kind\":\"router\"}";
    }
    for (std::uint32_t state_id : emitted_states) {
        const PlannerOperator& planner =
            calc.operators().at(result.policy[state_id]);
        const bool observed_choice =
            primitive_observes_modifier_offer(planner);
        const auto gated_retry =
            gated_retry_state_by_state.find(state_id);
        if (gated_retry != gated_retry_state_by_state.end()) {
            json += ",{\"id\":\"" + state_node(state_id) +
                    "_gated_route\",\"kind\":\"router\"}";
            ++node_count;
            check_node_cap();
        }
        if (observed_choice) {
            if (state_id >= result.unveil_preferences.size() ||
                result.unveil_preferences[state_id].empty()) {
                gap("observed-choice state " +
                    std::to_string(state_id) +
                    " has no resolved option preference");
            }
            json += ",{\"id\":\"";
            json += state_node(state_id);
            json += "\",\"kind\":\"router\"}";
            ++node_count;
            check_node_cap();
            for (std::size_t option = 0;
                 option < result.unveil_preferences[state_id].size();
                 ++option) {
                const std::uint32_t mod_id =
                    result.unveil_preferences[state_id][option];
                json += ",{\"id\":\"";
                json += state_node(state_id);
                json += "_u" + std::to_string(option);
                json += "\",\"kind\":\"operation\"";
                json += expected_cost_annotation(state_id);
                ActionDescriptor selected =
                    calc.registry().actions.at(
                        planner.primitive_action);
                selected.params.mod_id = mod_id;
                json += ",\"operation\":" +
                        operation_json(session, selected) + "}";
                ++node_count;
                check_node_cap();
            }
            continue;
        }
        if (planner.kind == PlannerOperatorKind::FixedOption &&
            planner.option_kind == FixedOptionKind::ImprintRetry) {
            if (planner.primitive_program.empty()) {
                gap("imprint retry option has no attempt program");
            }
            json += ",{\"id\":\"" + state_node(state_id) +
                    "\",\"kind\":\"operation\"" +
                    expected_cost_annotation(state_id) +
                    ",\"operation\":{\"type\":\"bestiary:imprint\"}}";
            ++node_count;
            for (std::size_t step = 0;
                 step < planner.primitive_program.size(); ++step) {
                const std::uint32_t action_index =
                    planner.primitive_program[step];
                json += ",{\"id\":\"" + state_node(state_id) + "_o" +
                        std::to_string(step) +
                        "\",\"kind\":\"operation\",\"operation\":" +
                        operation_json(
                            session,
                            calc.registry().actions.at(action_index)) +
                        accounting_roles_json(planner, action_index) + "}";
                ++node_count;
            }
            json += ",{\"id\":\"" + state_node(state_id) +
                    "_imprint_route\",\"kind\":\"router\"}";
            json += ",{\"id\":\"" + state_node(state_id) +
                    "_restore\",\"kind\":\"operation\",\"operation\":"
                    "{\"type\":\"bestiary:restore_imprint\"}}";
            node_count += 2;
            check_node_cap();
            continue;
        }
        if (planner.kind == PlannerOperatorKind::FixedOption &&
            (planner.option_kind == FixedOptionKind::Renewal ||
             planner.option_kind == FixedOptionKind::ProtectedRepeat ||
             planner.option_kind == FixedOptionKind::TemporaryBenchRepeat)) {
            const bool observed =
                option_observes_modifier_offer(planner);
            const std::size_t primitive_steps =
                planner.primitive_program.size() - (observed ? 1u : 0u);
            if (primitive_steps == 0) {
                gap("renewal option has no executable rolling step");
            }
            for (std::size_t step = 0; step < primitive_steps; ++step) {
                const std::uint32_t action_index =
                    planner.primitive_program[step];
                json += ",{\"id\":\"" + state_node(state_id);
                if (step > 0) json += "_o" + std::to_string(step);
                json += "\",\"kind\":\"operation\"";
                if (step == 0) {
                    json += expected_cost_annotation(state_id);
                }
                json += ",\"operation\":" + operation_json(
                    session, calc.registry().actions.at(action_index));
                json += accounting_roles_json(planner, action_index) + "}";
                ++node_count;
                check_node_cap();
            }
            if (observed) {
                const auto& preferences =
                    result.option_unveil_preferences.at(state_id);
                if (preferences.empty()) {
                    gap("observed renewal has no choice preferences");
                }
                json += ",{\"id\":\"" + state_node(state_id) +
                        "_observe\",\"kind\":\"router\"}";
                ++node_count;
                check_node_cap();
                for (std::size_t observation = 0;
                     observation < preferences.size(); ++observation) {
                    json += ",{\"id\":\"" + state_node(state_id) +
                            "_obs" + std::to_string(observation) +
                            "\",\"kind\":\"router\"}";
                    ++node_count;
                    check_node_cap();
                    for (std::size_t choice = 0;
                         choice < preferences[observation].choices.size();
                         ++choice) {
                        json += ",{\"id\":\"" +
                                state_node(state_id) + "_obs" +
                                std::to_string(observation) + "_u" +
                                std::to_string(choice) +
                                "\",\"kind\":\"operation\",\"operation\":";
                        ActionDescriptor selected =
                            calc.registry().actions.at(
                                planner.primitive_program.back());
                        selected.params.mod_id =
                            preferences[observation].choices[choice].mod_id;
                        json += operation_json(session, selected) + "}";
                        ++node_count;
                        check_node_cap();
                    }
                }
            } else {
                json += ",{\"id\":\"" + state_node(state_id) +
                        "_retry\",\"kind\":\"router\"}";
                ++node_count;
                check_node_cap();
            }
            continue;
        }
        if (planner.kind == PlannerOperatorKind::FixedOption &&
            planner.option_kind == FixedOptionKind::FracturePrepare) {
            const OptionKernel& kernel =
                compiled_option_kernels.at(state_id);
            if (kernel.entry_continues) {
                json += ",{\"id\":\"" + state_node(state_id) +
                        "\",\"kind\":\"operation\"" +
                        expected_cost_annotation(state_id) +
                        ",\"operation\":" + operation_json(
                            session, calc.registry().actions.at(
                                         planner.conditional_action)) +
                        accounting_roles_json(
                            planner, planner.conditional_action, true) + "}";
                ++node_count;
                check_node_cap();
                continue;
            }
            for (std::size_t step = 0;
                 step < planner.primitive_program.size(); ++step) {
                const std::uint32_t action_index =
                    planner.primitive_program[step];
                json += ",{\"id\":\"" + state_node(state_id);
                if (step > 0) json += "_o" + std::to_string(step);
                json += "\",\"kind\":\"operation\"";
                if (step == 0) {
                    json += expected_cost_annotation(state_id);
                }
                json += ",\"operation\":" + operation_json(
                    session, calc.registry().actions.at(action_index));
                json += accounting_roles_json(planner, action_index) + "}";
                ++node_count;
                check_node_cap();
            }
            json += ",{\"id\":\"" + state_node(state_id) +
                    "_fracture_route\",\"kind\":\"router\"}";
            json += ",{\"id\":\"" + state_node(state_id) +
                    "_fracture\",\"kind\":\"operation\",\"operation\":" +
                    operation_json(
                        session, calc.registry().actions.at(
                                     planner.conditional_action)) +
                    accounting_roles_json(
                        planner, planner.conditional_action, true) + "}";
            node_count += 2;
            check_node_cap();
            continue;
        }
        const bool product_local_fracture =
            calc.product_solver_parent() &&
            planner.kind == PlannerOperatorKind::Primitive &&
            planner.automatic_kind == AutomaticCandidateKind::Fracture;
        if (product_local_fracture) {
            if (planner.primitive_program.size() != 1) {
                gap("product-local Fracture must be one primitive operation");
            }
            json += ",{\"id\":\"" + state_node(state_id) +
                    "\",\"kind\":\"operation\"" +
                    expected_cost_annotation(state_id) +
                    ",\"operation\":" +
                    operation_json(
                        session,
                        calc.registry().actions.at(
                            planner.primitive_program.front())) +
                    accounting_roles_json(
                        planner, planner.primitive_program.front()) + "}";
            json += ",{\"id\":\"" + state_node(state_id) +
                    "_fracture_route\",\"kind\":\"router\"}";
            node_count += 2;
            check_node_cap();
            continue;
        }
        const std::vector<std::uint32_t>& program =
            planner.primitive_program;
        if (program.empty()) {
            gap("planner operator " + planner.id + " has no primitive program");
        }
        for (std::size_t step = 0; step < program.size(); ++step) {
            const std::uint32_t action_index = program[step];
            json += ",{\"id\":\"";
            json += state_node(state_id);
            if (step > 0) json += "_o" + std::to_string(step);
            json += "\",\"kind\":\"operation\"";
            if (step == 0) {
                json += expected_cost_annotation(state_id);
            }
            json += ",\"operation\":";
            json += operation_json(
                session, calc.registry().actions.at(action_index));
            json += accounting_roles_json(planner, action_index);
            json += "}";
            ++node_count;
            check_node_cap();
        }
    }
    json += "],\"edges\":[";
    {
        std::uint64_t owned =
            retained_compile_condition_bytes;
        add_owned_bytes(
            owned, owned_string_bytes(json));
        observe_complete_compiler_owned(
            owned, audited_compiler_owned_bytes(&json));
    }

    std::uint32_t edge_counter = 0;
    const auto edge = [&](const std::string& from, const std::string& to,
                          int priority, const std::string& condition,
                          bool is_default,
                          const char* accounting_role = nullptr) {
        if (edge_counter > 0) json += ',';
        json += "{\"id\":\"e";
        json += std::to_string(edge_counter++);
        json += "\",\"from\":\"";
        json += from;
        json += "\",\"to\":\"";
        json += to;
        json += "\",\"priority\":";
        json += std::to_string(priority);
        if (is_default) {
            json += ",\"is_default\":true";
        } else {
            json += ",\"condition\":";
            json += condition;
            if (telemetry != nullptr) {
                add_owned_bytes(
                    telemetry->total_condition_bytes,
                    condition.size());
                telemetry->max_condition_bytes = std::max(
                    telemetry->max_condition_bytes,
                    static_cast<std::uint64_t>(condition.size()));
                const auto count_predicates =
                    [&](const std::string& needle) {
                        std::uint64_t count = 0;
                        std::size_t position = 0;
                        while ((position = condition.find(
                                    needle, position)) !=
                               std::string::npos) {
                            ++count;
                            position += needle.size();
                        }
                        return count;
                    };
                add_owned_bytes(
                    telemetry->junk_predicates,
                    count_predicates(
                        "\"type\":\"mod_count\"") +
                        count_predicates(
                            "\"type\":\"mod_family_count\""));
            }
        }
        if (accounting_role != nullptr) {
            json += ",\"accounting_roles\":[\"";
            json += accounting_role;
            json += "\"]";
        }
        json += "}";
        if (edge_counter > result.options.max_compiled_edges) {
            if (telemetry != nullptr) telemetry->cap_hit = "max_compiled_edges";
            gap("compiled policy exceeded max_compiled_edges (" +
                std::to_string(result.options.max_compiled_edges) + ")");
        }
        if (json.size() > strategy_json_limit) {
            if (telemetry != nullptr) {
                telemetry->cap_hit = "max_strategy_json_bytes";
            }
            gap("compiled policy exceeded max_strategy_json_bytes (" +
                std::to_string(strategy_json_limit) +
                ")");
        }
        std::uint64_t owned =
            retained_compile_condition_bytes;
        add_owned_bytes(
            owned, owned_string_bytes(json));
        observe_complete_compiler_owned(
            owned, audited_compiler_owned_bytes(&json));
    };

    edge("start", "router", 0, "", true);

    /* Goal first: the configured number of satisfied slots at the finished
     * rarity succeeds regardless of junk. */
    {
        std::vector<std::string> parts{rarity_condition(calc.goal().rarity)};
        std::vector<std::string> satisfied;
        for (std::size_t i = 0; i < layout.slots.size(); ++i) {
            satisfied.push_back(vocabulary[i].satisfied);
        }
        parts.push_back(
            calc.goal().required_satisfied_slots() == satisfied.size()
                ? all_of(satisfied)
                : at_least(calc.goal().required_satisfied_slots(),
                           satisfied));
        edge("router", "goal", 0, all_of(parts), false);
    }

    int root_default_priority = 2;
    if (strict_policy_route) {
        if (use_exact_policy_tree) {
            edge(
                "router", policy_route_root.to, 1,
                policy_route_root.guard, false);
        }
    } else if (structured_refined_route) {
        int parent_priority = 1;
        for (const std::uint32_t coarse_state :
             refined_parent_order) {
            const std::string& router_id =
                refined_parent_router.at(coarse_state);
            edge(
                "router", router_id, parent_priority++,
                refined_parent_condition.at(coarse_state), false);
        }
        root_default_priority = parent_priority;
        for (const std::uint32_t coarse_state :
             refined_parent_order) {
            const std::vector<std::uint32_t>& class_ids =
                refined_classes_by_coarse_parent.at(coarse_state);
            const std::string& router_id =
                refined_parent_router.at(coarse_state);
            std::set<std::pair<std::string, std::string>>
                emitted_routes;
            int route_priority = 0;
            for (const std::uint32_t class_id : class_ids) {
                const refinement::RefinedPolicyCompileClass& policy_class =
                    refined_routing->classes.at(class_id);
                if (policy_class.terminal) continue;
                const std::uint32_t state =
                    policy_class.representative_state;
                const std::uint32_t leader =
                    policy_region_by_state.at(state);
                if (leader == kNoId) {
                    gap(
                        "refined policy class has no emitted policy "
                        "region");
                }
                const std::pair<std::string, std::string> route{
                    state_node(leader),
                    refined_condition_by_class[class_id]};
                if (!emitted_routes.insert(route).second) {
                    continue;
                }
                edge(
                    router_id, route.first, route_priority++,
                    route.second, false);
            }
            edge(
                router_id, policy_route_default_node,
                route_priority, "", true);
        }
    } else {
        std::set<std::pair<std::string, std::string>>
            emitted_refined_routes;
        for (const std::uint32_t leader : emitted_states) {
            for (const std::uint32_t state : states_by_leader.at(leader)) {
                if (calc.state(state).goal_progress_retry_basin != 0) {
                    continue;
                }
                const std::pair<std::string, std::string> route{
                    state_node(leader),
                    state_conditions.at(state)};
                if (structured_refined_route &&
                    !emitted_refined_routes.insert(route).second) {
                    continue;
                }
                edge(
                    "router", route.first, 1,
                    route.second, false);
            }
        }
    }
    edge(
        "router", policy_route_default_node,
        root_default_priority, "", true);
    for (const PolicyRouteNode& route : policy_route_nodes) {
        int priority = 0;
        for (const PolicyRouteEdge& route_edge : route.edges) {
            edge(
                route.id, route_edge.to, priority++,
                route_edge.condition, false);
        }
        edge(route.id, policy_route_default_node, priority, "", true);
    }
    if (bounded_default_restart_action != kNoId) {
        edge("bounded_default_restart", "router", 0, "", true);
    }
    if (dedicated_product_fracture_restart) {
        edge("product_fracture_restart", "router", 0, "", true, "retry");
    }
    for (std::uint32_t state_id : emitted_states) {
        const PlannerOperator& planner =
            calc.operators().at(result.policy[state_id]);
        const bool observed_choice =
            primitive_observes_modifier_offer(planner);
        if (planner.kind == PlannerOperatorKind::FixedOption &&
            planner.option_kind == FixedOptionKind::ImprintRetry) {
            const std::string base = state_node(state_id);
            edge(base, base + "_o0", 0, "", true);
            for (std::size_t step = 0;
                 step < planner.primitive_program.size(); ++step) {
                const std::string from =
                    base + "_o" + std::to_string(step);
                const std::string to =
                    step + 1 < planner.primitive_program.size()
                        ? base + "_o" + std::to_string(step + 1)
                        : base + "_imprint_route";
                edge(from, to, 0, "", true);
            }
            const OptionKernel& kernel =
                compiled_option_kernels.at(state_id);
            const std::string route = base + "_imprint_route";
            for (std::size_t i = 0; i < kernel.retry_states.size(); ++i) {
                edge(
                    route, base + "_restore", static_cast<int>(i),
                    abstract_state_condition(
                        session, layout, vocabulary,
                        calc.state(kernel.retry_states[i])),
                    false);
            }
            edge(
                route, "router",
                static_cast<int>(kernel.retry_states.size()), "", true);
            edge(base + "_restore", base, 0, "", true, "retry");
            continue;
        }
        if (planner.kind == PlannerOperatorKind::FixedOption &&
            (planner.option_kind == FixedOptionKind::Renewal ||
             planner.option_kind == FixedOptionKind::ProtectedRepeat ||
             planner.option_kind == FixedOptionKind::TemporaryBenchRepeat)) {
            const bool observed =
                option_observes_modifier_offer(planner);
            const std::size_t primitive_steps =
                planner.primitive_program.size() - (observed ? 1u : 0u);
            for (std::size_t step = 0; step < primitive_steps; ++step) {
                std::string from = state_node(state_id);
                if (step > 0) from += "_o" + std::to_string(step);
                std::string to;
                if (step + 1 < primitive_steps) {
                    to = state_node(state_id) + "_o" +
                         std::to_string(step + 1);
                } else {
                    to = state_node(state_id) +
                         (observed ? "_observe" : "_retry");
                }
                edge(from, to, 0, "", true);
            }
            if (observed) {
                const OptionKernel& kernel =
                    compiled_option_kernels.at(state_id);
                const auto& preferences =
                    result.option_unveil_preferences.at(state_id);
                const std::string dispatcher =
                    state_node(state_id) + "_observe";
                for (std::size_t observation = 0;
                     observation < preferences.size(); ++observation) {
                    const std::string offer =
                        state_node(state_id) + "_obs" +
                        std::to_string(observation);
                    edge(
                        dispatcher, offer,
                        static_cast<int>(observation),
                        abstract_state_condition(
                            session, layout, vocabulary,
                            calc.state(preferences[observation]
                                           .observation_state)),
                        false);
                    for (std::size_t choice = 0;
                         choice < preferences[observation].choices.size();
                         ++choice) {
                        const ObservedUnveilChoice& selected =
                            preferences[observation].choices[choice];
                        const std::string operation =
                            offer + "_u" + std::to_string(choice);
                        edge(
                            offer, operation, static_cast<int>(choice),
                            "{\"type\":\"has_unveil_option\",\"mod_key\":\"" +
                                json_escape(mod_key_of(
                                    session, selected.mod_id)) +
                                "\"}",
                            false);
                        const bool retry_choice =
                            structured_refined_route
                                ? fixed_option_choice_retries_locally(
                                      state_id, kernel,
                                      selected.successor_state,
                                      selected.actual_state)
                                : fixed_option_choice_retries_locally(
                                      state_id, kernel,
                                      selected.successor_state,
                                      selected.actual_state,
                                      result
                                          .behavioral_representative_by_state);
                        edge(
                            operation,
                            retry_choice
                                ? state_node(state_id)
                                : "router",
                            0, "", true);
                    }
                    edge(
                        offer, "offpolicy",
                        static_cast<int>(
                            preferences[observation].choices.size()),
                        "", true);
                }
                /* No-offer and other expressible stopped paths are exits,
                 * not fabricated option failures. */
                edge(
                    dispatcher, "router",
                    static_cast<int>(preferences.size()), "", true);
            } else {
                const OptionKernel& kernel =
                    compiled_option_kernels.at(state_id);
                const std::string retry =
                    state_node(state_id) + "_retry";
                for (std::size_t i = 0; i < kernel.retry_states.size(); ++i) {
                    edge(
                        retry, state_node(state_id),
                        static_cast<int>(i),
                        abstract_state_condition(
                            session, layout, vocabulary,
                            calc.state(kernel.retry_states[i])),
                        false,
                        planner.option_kind == FixedOptionKind::ProtectedRepeat
                            ? "protection_reapplication"
                            : "retry");
                }
                edge(
                    retry, "router",
                    static_cast<int>(kernel.retry_states.size()), "", true);
            }
            continue;
        }
        if (planner.kind == PlannerOperatorKind::FixedOption &&
            planner.option_kind == FixedOptionKind::FracturePrepare) {
            const OptionKernel& kernel =
                compiled_option_kernels.at(state_id);
            if (kernel.entry_continues) {
                edge(state_node(state_id), "router", 0, "", true);
                continue;
            }
            for (std::size_t step = 0;
                 step < planner.primitive_program.size(); ++step) {
                std::string from = state_node(state_id);
                if (step > 0) from += "_o" + std::to_string(step);
                std::string to =
                    step + 1 < planner.primitive_program.size()
                        ? state_node(state_id) + "_o" +
                              std::to_string(step + 1)
                        : state_node(state_id) +
                              "_fracture_route";
                edge(from, to, 0, "", true);
            }
            const std::string route =
                state_node(state_id) + "_fracture_route";
            int priority = 0;
            for (const std::uint32_t continuation :
                 kernel.continuation_states) {
                edge(
                    route,
                    state_node(state_id) + "_fracture",
                    priority++,
                    abstract_state_condition(
                        session, layout, vocabulary,
                        calc.state(continuation)),
                    false);
            }
            for (const std::uint32_t retry_state : kernel.retry_states) {
                edge(
                    route, state_node(state_id), priority++,
                    abstract_state_condition(
                        session, layout, vocabulary,
                        calc.state(retry_state)),
                    false, "retry");
            }
            edge(route, "router", priority, "", true);
            edge(
                state_node(state_id) + "_fracture", "router",
                0, "", true);
            continue;
        }
        const bool product_local_fracture =
            calc.product_solver_parent() &&
            planner.kind == PlannerOperatorKind::Primitive &&
            planner.automatic_kind == AutomaticCandidateKind::Fracture;
        if (product_local_fracture) {
            const AbstractState& source = calc.state(state_id);
            std::vector<std::string> acceptable_hits;
            for (std::uint32_t slot = 0;
                 slot < layout.slots.size(); ++slot) {
                const std::uint32_t bit = 1u << slot;
                if ((planner.relevant_goal_mask & bit) != 0 &&
                    source.slot_status[slot] ==
                        static_cast<std::uint8_t>(
                            GoalSlotStatus::Satisfied) &&
                    (source.fractured_goal_mask & bit) == 0) {
                    acceptable_hits.push_back(with_slot_flags(
                        vocabulary[slot].satisfied, true, false));
                }
            }
            if (acceptable_hits.empty()) {
                gap("selected product-local Fracture has no acceptable hit");
            }
            const std::string base = state_node(state_id);
            const std::string route = base + "_fracture_route";
            edge(base, route, 0, "", true);
            edge(route, "router", 0, any_of(acceptable_hits), false);
            edge(
                route, product_fracture_restart_node,
                1, "", true, "retry");
            continue;
        }
        if (!observed_choice) {
            for (std::size_t step = 0;
                 step < planner.primitive_program.size(); ++step) {
                std::string from = state_node(state_id);
                if (step > 0) from += "_o" + std::to_string(step);
                std::string to = "router";
                if (step + 1 < planner.primitive_program.size()) {
                    to = state_node(state_id) + "_o" +
                         std::to_string(step + 1);
                } else if (
                    gated_retry_state_by_state.count(state_id) != 0) {
                    to = state_node(state_id) + "_gated_route";
                }
                edge(from, to, 0, "", true);
            }
            const auto gated_retry =
                gated_retry_state_by_state.find(state_id);
            if (gated_retry != gated_retry_state_by_state.end()) {
                std::vector<std::string> satisfied;
                satisfied.reserve(vocabulary.size());
                for (const SlotVocabulary& slot : vocabulary) {
                    satisfied.push_back(slot.satisfied);
                }
                const std::string zero_progress =
                    satisfied.empty()
                        ? "{\"type\":\"always\"}"
                        : not_of(any_of(satisfied));
                const std::uint32_t retry_leader =
                    policy_region_by_state.at(gated_retry->second);
                if (retry_leader == kNoId) {
                    gap("gated retry basin has no emitted policy region");
                }
                edge(
                    state_node(state_id) + "_gated_route",
                    state_node(retry_leader), 0,
                    zero_progress, false, "retry");
                edge(
                    state_node(state_id) + "_gated_route",
                    "router", 1, "", true);
            }
            continue;
        }
        const auto& preferences = result.unveil_preferences[state_id];
        for (std::size_t option = 0; option < preferences.size(); ++option) {
            const std::string operation =
                state_node(state_id) + "_u" +
                std::to_string(option);
            const std::string condition =
                "{\"type\":\"has_unveil_option\",\"mod_key\":\"" +
                json_escape(mod_key_of(session, preferences[option])) +
                "\"}";
            edge(state_node(state_id), operation,
                 static_cast<int>(option), condition, false);
            edge(operation, "router", 0, "", true);
        }
        edge(state_node(state_id), "offpolicy",
             static_cast<int>(preferences.size()), "", true);
    }
    json += "]}";
    if (json.size() > strategy_json_limit) {
        if (telemetry != nullptr) telemetry->cap_hit = "max_strategy_json_bytes";
        gap("compiled policy exceeded max_strategy_json_bytes (" +
            std::to_string(strategy_json_limit) + ")");
    }
    {
        std::uint64_t owned =
            retained_compile_condition_bytes;
        add_owned_bytes(
            owned, owned_string_bytes(json));
        observe_complete_compiler_owned(
            owned, audited_compiler_owned_bytes(&json));
    }
    if (telemetry != nullptr) {
        telemetry->working_states = static_cast<std::uint32_t>(
            compiled_states.size());
        if (structured_refined_route) {
            telemetry->behavioral_classes =
                static_cast<std::uint32_t>(std::count_if(
                    refined_routing->classes.begin(),
                    refined_routing->classes.end(),
                    [](const refinement::RefinedPolicyCompileClass& value) {
                        return !value.terminal;
                    }));
        } else if (!result.behavioral_representative_by_state.empty()) {
            telemetry->behavioral_classes =
                static_cast<std::uint32_t>(std::count_if(
                    quotient_members.begin(), quotient_members.end(),
                    [&](const auto& value) {
                        return value.first < result.goal_states.size() &&
                               !result.goal_states[value.first];
                    }));
        } else {
            telemetry->behavioral_classes =
                static_cast<std::uint32_t>(compiled_states.size());
        }
        telemetry->policy_regions = static_cast<std::uint32_t>(
            emitted_states.size());
        telemetry->nodes = node_count;
        telemetry->edges = edge_counter;
        telemetry->strategy_json_bytes = json.size();
    }
    return json;
}

} // namespace solver
} // namespace poecraft
