#include "solver_executable_fragment.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <iomanip>
#include <limits>
#include <map>
#include <set>
#include <sstream>
#include <tuple>

namespace poecraft {
namespace solver {
namespace fragment_v1 {

namespace {

constexpr std::uint32_t kNoIndex =
    std::numeric_limits<std::uint32_t>::max();

void append_size(std::string& out, const std::size_t value) {
    out += std::to_string(value);
    out.push_back(':');
}

void append_string(std::string& out, const std::string& value) {
    append_size(out, value.size());
    out += value;
}

template <typename Value>
void append_integer(std::string& out, const Value value) {
    append_string(out, std::to_string(value));
}

void append_double(std::string& out, const double value) {
    append_integer(out, std::bit_cast<std::uint64_t>(value));
}

std::uint64_t fnv1a64(const std::string& bytes) {
    std::uint64_t value = 1469598103934665603ull;
    for (const unsigned char byte : bytes) {
        value ^= byte;
        value *= 1099511628211ull;
    }
    return value;
}

CanonicalIdentityV1 identity_from_bytes(std::string bytes) {
    const std::uint64_t digest = fnv1a64(bytes);
    return {digest, std::move(bytes)};
}

std::string hex_bytes(const std::string& bytes) {
    constexpr char digits[] = "0123456789abcdef";
    std::string encoded;
    encoded.reserve(bytes.size() * 2);
    for (const unsigned char byte : bytes) {
        encoded.push_back(digits[byte >> 4]);
        encoded.push_back(digits[byte & 0x0f]);
    }
    return encoded;
}

StableParametersV1 canonical_parameters(StableParametersV1 value) {
    std::sort(value.begin(), value.end());
    return value;
}

bool duplicate_parameter_key(const StableParametersV1& parameters) {
    std::set<std::string> keys;
    for (const auto& [key, value] : parameters) {
        (void)value;
        if (key.empty() || !keys.insert(key).second) return true;
    }
    return false;
}

bool valid_exact_state_identity(const ExactStateV1& state) {
    return !state.key.words.empty() || !state.key.opaque_bytes.empty();
}

std::optional<std::string> parameter_value(
        const StableParametersV1& parameters,
        const std::string& key) {
    const auto found = std::find_if(
        parameters.begin(), parameters.end(),
        [&](const auto& parameter) { return parameter.first == key; });
    if (found == parameters.end()) return std::nullopt;
    return found->second;
}

void append_parameters(
        std::string& out,
        const StableParametersV1& input) {
    const StableParametersV1 parameters = canonical_parameters(input);
    append_size(out, parameters.size());
    for (const auto& [key, value] : parameters) {
        append_string(out, key);
        append_string(out, value);
    }
}

void append_exact_key(std::string& out, const ExactStateKeyV1& key) {
    append_size(out, key.words.size());
    for (const std::uint64_t word : key.words) append_integer(out, word);
    append_string(out, key.opaque_bytes);
}

void append_i64_vector(
        std::string& out,
        const std::vector<std::int64_t>& values) {
    append_size(out, values.size());
    for (const std::int64_t value : values) append_integer(out, value);
}

void append_string_vector(
        std::string& out,
        const std::vector<std::string>& values) {
    append_size(out, values.size());
    for (const std::string& value : values) append_string(out, value);
}

bool known_condition_type(const std::string& type) {
    static const std::set<std::string> known = {
        "always",
        "has_mod_group",
        "has_mod_family",
        "observation_signature",
        "mod_count",
        "mod_family_count",
        "item_flag",
        "influence_bits",
        "eldritch_tier",
        "has_unveil_option",
        "rarity_is",
        "open_prefix_count",
        "open_suffix_count",
        "prefix_count_range",
        "suffix_count_range",
        "all",
        "all_of",
        "any",
        "any_of",
        "not",
        "at_least",
    };
    return known.contains(type);
}

bool composite_condition_type(const std::string& type) {
    return type == "all" || type == "all_of" || type == "any" ||
           type == "any_of" || type == "not" || type == "at_least";
}

void append_condition(
        std::string& out,
        const FragmentConditionV1& condition) {
    append_string(out, condition.type);
    append_parameters(out, condition.parameters);
    append_integer(out, condition.at_least_count);
    std::vector<CanonicalIdentityV1> children;
    children.reserve(condition.children.size());
    for (const FragmentConditionV1& child : condition.children) {
        children.push_back(canonical_fragment_condition_identity_v1(child));
    }
    if (condition.type != "not") {
        std::sort(
            children.begin(), children.end(),
            [](const CanonicalIdentityV1& left,
               const CanonicalIdentityV1& right) {
                return left.canonical_bytes < right.canonical_bytes;
            });
    }
    append_size(out, children.size());
    for (const CanonicalIdentityV1& child : children) {
        append_string(out, child.canonical_bytes);
    }
}

std::optional<FragmentStructuralRefusalV1> validate_condition(
        const FragmentConditionV1& condition,
        const std::string& witness) {
    if (!known_condition_type(condition.type)) {
        return FragmentStructuralRefusalV1{
            "unknown_condition", witness + ":" + condition.type};
    }
    if (duplicate_parameter_key(condition.parameters)) {
        return FragmentStructuralRefusalV1{
            "duplicate_condition_parameter", witness};
    }
    const bool composite = composite_condition_type(condition.type);
    if (composite && condition.children.empty()) {
        return FragmentStructuralRefusalV1{
            "empty_composite_condition", witness};
    }
    if (!composite && !condition.children.empty()) {
        return FragmentStructuralRefusalV1{
            "unexpected_condition_children", witness};
    }
    if (condition.type == "not" && condition.children.size() != 1) {
        return FragmentStructuralRefusalV1{
            "invalid_not_condition", witness};
    }
    if (condition.type == "at_least" &&
        condition.at_least_count > condition.children.size()) {
        return FragmentStructuralRefusalV1{
            "invalid_at_least_condition", witness};
    }
    if (condition.type != "at_least" && condition.at_least_count != 0) {
        return FragmentStructuralRefusalV1{
            "unexpected_condition_threshold", witness};
    }
    for (std::size_t index = 0; index < condition.children.size(); ++index) {
        if (auto refusal = validate_condition(
                condition.children[index],
                witness + "/" + std::to_string(index))) {
            return refusal;
        }
    }
    return std::nullopt;
}

std::string product_key_bytes(const ProductStateKeyV1& key) {
    std::string out;
    append_exact_key(out, key.exact_item_key);
    append_i64_vector(out, key.hard_execution_state);
    append_string(out, key.controller_node_id);
    append_i64_vector(out, key.controller_memory);
    return out;
}

void append_exit_descriptor(
        std::string& out,
        const FragmentExitDescriptorV1& descriptor) {
    append_integer(out, static_cast<std::uint32_t>(descriptor.kind));
    append_string(out, descriptor.label);
    append_parameters(out, descriptor.parameters);
}

std::string exit_key_bytes(const ExitIdentityV1& key) {
    std::string out;
    append_exit_descriptor(out, key.descriptor);
    append_exact_key(out, key.exact_item_key);
    append_i64_vector(out, key.hard_execution_state);
    append_i64_vector(out, key.controller_memory);
    return out;
}

FragmentExitDescriptorV1 canonical_exit_descriptor(
        FragmentExitDescriptorV1 descriptor) {
    descriptor.parameters = canonical_parameters(
        std::move(descriptor.parameters));
    return descriptor;
}

void canonicalize_condition(FragmentConditionV1& condition) {
    condition.parameters = canonical_parameters(
        std::move(condition.parameters));
    for (FragmentConditionV1& child : condition.children) {
        canonicalize_condition(child);
    }
    if (condition.type != "not") {
        std::sort(
            condition.children.begin(), condition.children.end(),
            [](const FragmentConditionV1& left,
               const FragmentConditionV1& right) {
                return canonical_fragment_condition_identity_v1(left)
                           .canonical_bytes <
                       canonical_fragment_condition_identity_v1(right)
                           .canonical_bytes;
            });
    }
}

ExecutableFragmentIRV1 canonicalize_ir(ExecutableFragmentIRV1 ir) {
    for (FragmentNodeV1& node : ir.nodes) {
        node.exit = canonical_exit_descriptor(std::move(node.exit));
        for (FragmentEdgeV1& edge : node.edges) {
            canonicalize_condition(edge.condition);
        }
        std::sort(
            node.edges.begin(), node.edges.end(),
            [](const FragmentEdgeV1& left,
               const FragmentEdgeV1& right) {
                return std::tie(left.priority, left.edge_id) <
                       std::tie(right.priority, right.edge_id);
            });
    }
    std::sort(
        ir.nodes.begin(), ir.nodes.end(),
        [](const FragmentNodeV1& left, const FragmentNodeV1& right) {
            return left.node_id < right.node_id;
        });
    return ir;
}

bool stable_action_identity(const std::string& value) {
    if (value.empty() || value == "restart" || value == "economic_restart") {
        return false;
    }
    if (value.rfind("fragment:", 0) == 0 ||
        value.rfind("operator_index:", 0) == 0 ||
        value.rfind("operator:", 0) == 0 ||
        value.find("imprint") != std::string::npos) {
        return false;
    }
    return !std::all_of(value.begin(), value.end(), [](const char ch) {
        return ch >= '0' && ch <= '9';
    });
}

ResourceVectorV1 vector_from_map(const std::map<std::string, double>& values) {
    ResourceVectorV1 out;
    out.reserve(values.size());
    for (const auto& [key, value] : values) out.push_back({key, value});
    return out;
}

ResourceVectorV1 canonical_resource_vector(ResourceVectorV1 vector) {
    std::sort(
        vector.begin(), vector.end(),
        [](const auto& left, const auto& right) {
            return left.first < right.first;
        });
    return vector;
}

bool finite_nonnegative_vector(const ResourceVectorV1& vector) {
    std::set<std::string> keys;
    for (const auto& [key, value] : vector) {
        if (key.empty() || !keys.insert(key).second ||
            !std::isfinite(value) || value < 0.0) {
            return false;
        }
    }
    return true;
}

std::uint64_t estimated_graph_bytes(
        const std::size_t states,
        const std::size_t rows,
        const std::uint64_t transitions,
        const std::size_t exits) {
    constexpr std::uint64_t state_bytes = 320;
    constexpr std::uint64_t row_bytes = 256;
    constexpr std::uint64_t transition_bytes = 224;
    constexpr std::uint64_t exit_bytes = 224;
    const long double estimated =
        static_cast<long double>(states) * state_bytes +
        static_cast<long double>(rows) * row_bytes +
        static_cast<long double>(transitions) * transition_bytes +
        static_cast<long double>(exits) * exit_bytes;
    if (estimated >=
        static_cast<long double>(
            std::numeric_limits<std::uint64_t>::max())) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return static_cast<std::uint64_t>(estimated);
}

std::uint64_t estimated_dense_solve_work(const std::size_t order) {
    const long double estimated =
        static_cast<long double>(order) * order *
        std::max<std::size_t>(order, 1) * 2.0L +
        static_cast<long double>(order) * order;
    if (estimated >=
        static_cast<long double>(
            std::numeric_limits<std::uint64_t>::max())) {
        return std::numeric_limits<std::uint64_t>::max();
    }
    return std::max<std::uint64_t>(
        1, static_cast<std::uint64_t>(estimated));
}

std::optional<std::vector<double>> solve_dense_system(
        std::vector<std::vector<double>> matrix,
        std::vector<double> rhs) {
    const std::size_t order = matrix.size();
    if (rhs.size() != order) return std::nullopt;
    for (std::size_t row = 0; row < order; ++row) {
        if (matrix[row].size() != order) return std::nullopt;
    }
    for (std::size_t column = 0; column < order; ++column) {
        std::size_t pivot = column;
        for (std::size_t row = column + 1; row < order; ++row) {
            if (std::fabs(matrix[row][column]) >
                std::fabs(matrix[pivot][column])) {
                pivot = row;
            }
        }
        if (!std::isfinite(matrix[pivot][column]) ||
            std::fabs(matrix[pivot][column]) <= 1e-15) {
            return std::nullopt;
        }
        if (pivot != column) {
            std::swap(matrix[pivot], matrix[column]);
            std::swap(rhs[pivot], rhs[column]);
        }
        const double divisor = matrix[column][column];
        for (std::size_t entry = column; entry < order; ++entry) {
            matrix[column][entry] /= divisor;
        }
        rhs[column] /= divisor;
        for (std::size_t row = 0; row < order; ++row) {
            if (row == column) continue;
            const double factor = matrix[row][column];
            if (factor == 0.0) continue;
            for (std::size_t entry = column; entry < order; ++entry) {
                matrix[row][entry] -= factor * matrix[column][entry];
            }
            rhs[row] -= factor * rhs[column];
        }
    }
    for (double& value : rhs) {
        if (!std::isfinite(value)) return std::nullopt;
        if (value < 0.0 && value > -1e-11) value = 0.0;
        if (value < 0.0) return std::nullopt;
    }
    return rhs;
}

double residual_for(
        const std::vector<std::vector<double>>& matrix,
        const std::vector<double>& solution,
        const std::vector<double>& rhs) {
    double residual = 0.0;
    for (std::size_t row = 0; row < matrix.size(); ++row) {
        long double observed = 0.0L;
        for (std::size_t column = 0; column < matrix.size(); ++column) {
            observed += static_cast<long double>(matrix[row][column]) *
                        static_cast<long double>(solution[column]);
        }
        residual = std::max(
            residual,
            std::fabs(static_cast<double>(observed) - rhs[row]));
    }
    return residual;
}

} // namespace

bool ExactStateKeyV1::operator<(const ExactStateKeyV1& other) const {
    return std::tie(words, opaque_bytes) <
           std::tie(other.words, other.opaque_bytes);
}

bool ProductStateKeyV1::operator<(const ProductStateKeyV1& other) const {
    return std::tie(
               exact_item_key, hard_execution_state, controller_node_id,
               controller_memory) <
           std::tie(
               other.exact_item_key, other.hard_execution_state,
               other.controller_node_id, other.controller_memory);
}

bool ExitIdentityV1::operator<(const ExitIdentityV1& other) const {
    return std::tie(
               descriptor.kind, descriptor.label, descriptor.parameters,
               exact_item_key, hard_execution_state, controller_memory) <
           std::tie(
               other.descriptor.kind, other.descriptor.label,
               other.descriptor.parameters, other.exact_item_key,
               other.hard_execution_state, other.controller_memory);
}

CanonicalIdentityV1 canonical_fragment_condition_identity_v1(
        const FragmentConditionV1& condition) {
    std::string bytes;
    append_condition(bytes, condition);
    return identity_from_bytes(std::move(bytes));
}

CanonicalIdentityV1 canonical_fragment_ir_identity_v1(
        const ExecutableFragmentIRV1& input) {
    const ExecutableFragmentIRV1 ir = canonicalize_ir(input);
    std::string bytes;
    append_integer(bytes, ir.schema_version);
    append_string(bytes, ir.fragment_id);
    append_string(bytes, ir.exact_entry_product_state_identity);
    append_string(bytes, ir.caller_action_scope_identity);
    append_string(bytes, ir.disabled_action_family_identity);
    append_string(bytes, ir.exact_goal_identity);
    append_string(bytes, ir.mechanics_artifact_identity);
    append_string(bytes, ir.exact_state_key_semantics_version);
    append_string(bytes, ir.refinement_semantics_version);
    append_string(bytes, ir.condition_semantics_version);
    append_string(bytes, ir.verifier_version);
    append_string(bytes, ir.properness_version);
    append_string(bytes, ir.tolerance_version);
    append_string(bytes, ir.entry_node_id);
    append_string_vector(bytes, ir.controller_memory_schema);
    append_i64_vector(bytes, ir.initial_controller_memory);
    append_size(bytes, ir.nodes.size());
    for (const FragmentNodeV1& node : ir.nodes) {
        append_string(bytes, node.node_id);
        append_integer(bytes, static_cast<std::uint32_t>(node.kind));
        append_string(bytes, node.stable_action_identity);
        append_string_vector(bytes, node.observed_choice_order);
        append_exit_descriptor(bytes, node.exit);
        append_size(bytes, node.edges.size());
        for (const FragmentEdgeV1& edge : node.edges) {
            append_string(bytes, edge.edge_id);
            append_string(bytes, edge.target_node_id);
            append_string(
                bytes,
                canonical_fragment_condition_identity_v1(edge.condition)
                    .canonical_bytes);
            append_integer(bytes, edge.priority);
            append_integer(
                bytes,
                edge.certification_fail_closed_default ? 1u : 0u);
        }
    }
    return identity_from_bytes(std::move(bytes));
}

std::string canonical_exact_entry_identity_v1(
        const ExactStateV1& state,
        const std::vector<std::int64_t>& initial_controller_memory) {
    std::string bytes;
    append_exact_key(bytes, state.key);
    append_i64_vector(bytes, state.hard_execution_state);
    append_i64_vector(bytes, initial_controller_memory);
    const CanonicalIdentityV1 identity = identity_from_bytes(bytes);
    std::ostringstream out;
    out << "exact-entry-v1:" << std::hex << std::setfill('0')
        << std::setw(16) << identity.digest << ':' << hex_bytes(bytes);
    return out.str();
}

FragmentStructuralValidationV1 validate_executable_fragment_ir_v1(
        const ExecutableFragmentIRV1& ir) {
    const auto refuse = [](std::string code, std::string witness) {
        FragmentStructuralValidationV1 result;
        result.refusal = {std::move(code), std::move(witness)};
        return result;
    };
    if (ir.schema_version != kExecutableFragmentSchemaVersionV1) {
        return refuse("unsupported_schema_version", std::to_string(ir.schema_version));
    }
    if (ir.fragment_id.empty()) return refuse("missing_fragment_id", "fragment_id");
    if (ir.exact_entry_product_state_identity.empty()) {
        return refuse("missing_exact_entry", "exact_entry_product_state_identity");
    }
    if (ir.exact_entry_product_state_identity.rfind("symbolic:", 0) == 0 ||
        ir.exact_entry_product_state_identity.rfind("domain:", 0) == 0) {
        return refuse(
            "symbolic_entry_domain_not_supported",
            ir.exact_entry_product_state_identity);
    }
    if (ir.caller_action_scope_identity.empty() ||
        ir.disabled_action_family_identity.empty() ||
        ir.exact_goal_identity.empty() ||
        ir.mechanics_artifact_identity.empty() ||
        ir.exact_state_key_semantics_version.empty() ||
        ir.refinement_semantics_version.empty() ||
        ir.condition_semantics_version.empty()) {
        return refuse("missing_semantic_identity", ir.fragment_id);
    }
    if (ir.verifier_version != kExecutableFragmentVerifierVersionV1 ||
        ir.properness_version != kExecutableFragmentPropernessVersionV1 ||
        ir.tolerance_version != kExecutableFragmentToleranceVersionV1) {
        return refuse("unsupported_verification_version", ir.fragment_id);
    }
    if (ir.controller_memory_schema.size() !=
        ir.initial_controller_memory.size()) {
        return refuse("controller_memory_shape_mismatch", ir.fragment_id);
    }
    std::set<std::string> memory_names;
    for (const std::string& name : ir.controller_memory_schema) {
        if (name.empty() || !memory_names.insert(name).second) {
            return refuse("invalid_controller_memory_schema", name);
        }
    }
    if (ir.nodes.empty()) return refuse("missing_nodes", ir.fragment_id);

    std::map<std::string, const FragmentNodeV1*> nodes;
    std::set<std::string> exit_descriptors;
    for (const FragmentNodeV1& node : ir.nodes) {
        if (node.node_id.empty() || !nodes.emplace(node.node_id, &node).second) {
            return refuse("duplicate_node", node.node_id);
        }
        switch (node.kind) {
        case FragmentNodeKindV1::Route:
        case FragmentNodeKindV1::PrimitiveOperation:
        case FragmentNodeKindV1::ObservedChoice:
        case FragmentNodeKindV1::Exit:
            break;
        default:
            return refuse("invalid_node_kind", node.node_id);
        }
        if (node.kind == FragmentNodeKindV1::Exit) {
            switch (node.exit.kind) {
            case FragmentExitKindV1::FinalSuccess:
            case FragmentExitKindV1::Subgoal:
            case FragmentExitKindV1::Recoverable:
            case FragmentExitKindV1::CertificationFailure:
                break;
            default:
                return refuse("invalid_exit_kind", node.node_id);
            }
            if (!node.edges.empty() || !node.stable_action_identity.empty() ||
                node.exit.label.empty() ||
                duplicate_parameter_key(node.exit.parameters)) {
                return refuse("invalid_exit_node", node.node_id);
            }
            std::string descriptor;
            append_exit_descriptor(descriptor, node.exit);
            if (!exit_descriptors.insert(descriptor).second) {
                return refuse("duplicate_exit_descriptor", node.node_id);
            }
            continue;
        }
        if (node.kind == FragmentNodeKindV1::PrimitiveOperation) {
            if (node.stable_action_identity.rfind("fragment:", 0) == 0) {
                return refuse("nested_fragment_call", node.node_id);
            }
            if (node.stable_action_identity == "restart" ||
                node.stable_action_identity == "economic_restart") {
                return refuse("implicit_restart", node.node_id);
            }
            if (node.stable_action_identity.find("imprint") !=
                std::string::npos) {
                return refuse("imprint_not_supported", node.node_id);
            }
            if (!stable_action_identity(node.stable_action_identity)) {
                return refuse("unstable_action_identity", node.node_id);
            }
        } else if (!node.stable_action_identity.empty()) {
            return refuse("unexpected_action_identity", node.node_id);
        }
        if (node.kind == FragmentNodeKindV1::ObservedChoice) {
            std::set<std::string> choices;
            if (node.observed_choice_order.empty()) {
                return refuse("missing_observed_choice_order", node.node_id);
            }
            for (const std::string& choice : node.observed_choice_order) {
                if (choice.empty() || !choices.insert(choice).second) {
                    return refuse("duplicate_observed_choice", node.node_id);
                }
            }
            std::vector<const FragmentEdgeV1*> ordered_choice_edges;
            for (const FragmentEdgeV1& edge : node.edges) {
                if (!edge.certification_fail_closed_default) {
                    ordered_choice_edges.push_back(&edge);
                }
            }
            std::sort(
                ordered_choice_edges.begin(), ordered_choice_edges.end(),
                [](const FragmentEdgeV1* left,
                   const FragmentEdgeV1* right) {
                    return std::tie(left->priority, left->edge_id) <
                           std::tie(right->priority, right->edge_id);
                });
            if (ordered_choice_edges.size() !=
                node.observed_choice_order.size()) {
                return refuse(
                    "observed_choice_order_mismatch", node.node_id);
            }
            for (std::size_t index = 0;
                 index < ordered_choice_edges.size(); ++index) {
                const FragmentConditionV1& observed =
                    ordered_choice_edges[index]->condition;
                const auto mod_key = parameter_value(
                    observed.parameters, "mod_key");
                if (observed.type != "has_unveil_option" || !mod_key ||
                    *mod_key != node.observed_choice_order[index]) {
                    return refuse(
                        "observed_choice_order_mismatch", node.node_id);
                }
            }
        } else if (!node.observed_choice_order.empty()) {
            return refuse("unexpected_observed_choice_order", node.node_id);
        }
        if (node.edges.empty()) return refuse("missing_edges", node.node_id);
        std::set<std::string> edge_ids;
        std::set<std::uint32_t> priorities;
        std::uint32_t defaults = 0;
        std::uint32_t maximum_priority = 0;
        std::uint32_t default_priority = 0;
        for (const FragmentEdgeV1& edge : node.edges) {
            if (edge.edge_id.empty() || !edge_ids.insert(edge.edge_id).second) {
                return refuse("duplicate_edge", node.node_id + ":" + edge.edge_id);
            }
            if (!priorities.insert(edge.priority).second) {
                return refuse("duplicate_edge_priority", node.node_id);
            }
            maximum_priority = std::max(maximum_priority, edge.priority);
            if (auto condition_refusal = validate_condition(
                    edge.condition, node.node_id + ":" + edge.edge_id)) {
                FragmentStructuralValidationV1 result;
                result.refusal = std::move(*condition_refusal);
                return result;
            }
            if (edge.certification_fail_closed_default) {
                ++defaults;
                default_priority = edge.priority;
                if (edge.condition.type != "always") {
                    return refuse("invalid_default_condition", node.node_id);
                }
            }
        }
        if (defaults != 1 || default_priority != maximum_priority) {
            return refuse("implicit_default", node.node_id);
        }
    }
    const auto entry = nodes.find(ir.entry_node_id);
    if (entry == nodes.end() ||
        entry->second->kind == FragmentNodeKindV1::Exit) {
        return refuse("missing_entry", ir.entry_node_id);
    }
    if (exit_descriptors.empty()) return refuse("missing_exit", ir.fragment_id);

    for (const FragmentNodeV1& node : ir.nodes) {
        if (node.kind == FragmentNodeKindV1::Exit) continue;
        for (const FragmentEdgeV1& edge : node.edges) {
            const auto target = nodes.find(edge.target_node_id);
            if (target == nodes.end()) {
                return refuse("missing_edge_target", edge.target_node_id);
            }
            if (edge.certification_fail_closed_default &&
                (target->second->kind != FragmentNodeKindV1::Exit ||
                 target->second->exit.kind !=
                     FragmentExitKindV1::CertificationFailure)) {
                return refuse("default_not_fail_closed", node.node_id);
            }
        }
    }

    std::set<std::string> reachable;
    std::vector<std::string> pending = {ir.entry_node_id};
    bool reachable_exit = false;
    while (!pending.empty()) {
        const std::string current = pending.back();
        pending.pop_back();
        if (!reachable.insert(current).second) continue;
        const FragmentNodeV1& node = *nodes.at(current);
        if (node.kind == FragmentNodeKindV1::Exit) {
            reachable_exit = true;
            continue;
        }
        for (const FragmentEdgeV1& edge : node.edges) {
            pending.push_back(edge.target_node_id);
        }
    }
    if (!reachable_exit) return refuse("missing_reachable_exit", ir.fragment_id);

    FragmentStructuralValidationV1 result;
    result.valid = true;
    result.identity = canonical_fragment_ir_identity_v1(ir);
    return result;
}

VerifiedLeafFragmentV1::VerifiedLeafFragmentV1(
        ConstructionToken,
        ExecutableFragmentIRV1 ir,
        CanonicalIdentityV1 ir_identity,
        CanonicalIdentityV1 certificate_identity,
        std::vector<VerifiedProductRowV1> rows,
        std::vector<VerifiedExitV1> exits,
        ResourceVectorV1 expected_resources,
        ResourceVectorV1 expected_action_counts,
        std::optional<double> priced_expected_cost,
        const double exit_probability_sum,
        const double max_probability_mass_error,
        const double max_resource_residual,
        const std::uint32_t strongly_connected_components,
        const std::uint32_t positive_probability_cyclic_components,
        const std::uint64_t work_items,
        const std::uint64_t peak_estimated_bytes)
    : ir_(std::move(ir)),
      ir_identity_(std::move(ir_identity)),
      certificate_identity_(std::move(certificate_identity)),
      rows_(std::move(rows)),
      exits_(std::move(exits)),
      expected_resources_(std::move(expected_resources)),
      expected_action_counts_(std::move(expected_action_counts)),
      priced_expected_cost_(priced_expected_cost),
      exit_probability_sum_(exit_probability_sum),
      max_probability_mass_error_(max_probability_mass_error),
      max_resource_residual_(max_resource_residual),
      strongly_connected_components_(strongly_connected_components),
      positive_probability_cyclic_components_(
          positive_probability_cyclic_components),
      work_items_(work_items),
      peak_estimated_bytes_(peak_estimated_bytes) {}

LeafVerificationResultV1 ExactLeafFragmentVerifierV1::verify(
        const ExecutableFragmentIRV1& input_ir,
        const LeafVerificationContextV1& input_context,
        const ExactPrimitiveOracleV1& oracle,
        const LeafVerificationLimitsV1& limits) const {
    const LeafVerificationContextV1 context = input_context;
    LeafVerificationResultV1 result;
    std::uint64_t work_items = 0;
    std::uint64_t transition_count = 0;
    std::uint64_t peak_bytes = 0;
    const auto refuse = [&](std::string code, std::string witness,
                            std::vector<std::string> component = {}) {
        constexpr std::size_t kMaximumWitnessBytes = 512;
        constexpr std::size_t kMaximumComponentWitnesses = 64;
        if (witness.size() > kMaximumWitnessBytes) {
            witness.resize(kMaximumWitnessBytes);
        }
        if (component.size() > kMaximumComponentWitnesses) {
            component.resize(kMaximumComponentWitnesses);
        }
        for (std::string& member : component) {
            if (member.size() > kMaximumWitnessBytes) {
                member.resize(kMaximumWitnessBytes);
            }
        }
        result.refusal = {
            std::move(code), std::move(witness), std::move(component),
            work_items, peak_bytes};
        return result;
    };
    const auto interrupted = [&]() -> std::optional<std::string> {
        if (limits.canceled && limits.canceled()) return "canceled";
        if (limits.deadline &&
            std::chrono::steady_clock::now() >= *limits.deadline) {
            return "time_limit";
        }
        if (work_items >= limits.max_work_items) return "max_work_items";
        return std::nullopt;
    };
    const auto reserve_work = [&](const std::uint64_t amount) {
        if (work_items >= limits.max_work_items ||
            amount >= limits.max_work_items - work_items) {
            work_items = limits.max_work_items;
            return false;
        }
        work_items += amount;
        return true;
    };
    if (std::bit_cast<std::uint64_t>(limits.probability_sum_tolerance) !=
            std::bit_cast<std::uint64_t>(
                kExecutableFragmentProbabilityToleranceV1) ||
        std::bit_cast<std::uint64_t>(limits.residual_tolerance) !=
            std::bit_cast<std::uint64_t>(
                kExecutableFragmentResidualToleranceV1)) {
        return refuse("unsupported_tolerance_configuration", "limits");
    }
    if (limits.max_states == 0) return refuse("max_states", "entry");
    if (limits.max_transitions == 0) {
        return refuse("max_transitions", "entry");
    }
    if (limits.max_estimated_bytes == 0) {
        return refuse("max_estimated_bytes", "entry");
    }
    if (auto stop = interrupted()) return refuse(*stop, "before_validation");

    const FragmentStructuralValidationV1 structural =
        validate_executable_fragment_ir_v1(input_ir);
    if (!structural.valid) {
        return refuse(structural.refusal.code, structural.refusal.witness);
    }
    if (!valid_exact_state_identity(context.exact_entry)) {
        return refuse("invalid_exact_entry", input_ir.fragment_id);
    }
    if (context.has_live_imprint_checkpoint) {
        return refuse("imprint_checkpoint_not_supported", input_ir.fragment_id);
    }
    if (input_ir.exact_entry_product_state_identity !=
        canonical_exact_entry_identity_v1(
            context.exact_entry, context.initial_controller_memory)) {
        return refuse("exact_entry_identity_mismatch", input_ir.fragment_id);
    }
    if (input_ir.initial_controller_memory !=
            context.initial_controller_memory ||
        input_ir.caller_action_scope_identity !=
            context.caller_action_scope_identity ||
        input_ir.disabled_action_family_identity !=
            context.disabled_action_family_identity ||
        input_ir.exact_goal_identity != context.exact_goal_identity ||
        input_ir.mechanics_artifact_identity !=
            context.mechanics_artifact_identity) {
        return refuse("verification_context_identity_mismatch", input_ir.fragment_id);
    }
    ExecutableFragmentIRV1 ir = canonicalize_ir(input_ir);
    std::map<std::string, const FragmentNodeV1*> node_by_id;
    for (const FragmentNodeV1& node : ir.nodes) {
        node_by_id.emplace(node.node_id, &node);
    }

    struct StateRecord {
        ProductStateKeyV1 key;
        ExactStateV1 state;
        std::vector<std::int64_t> controller_memory;
    };
    std::vector<StateRecord> states;
    std::map<ProductStateKeyV1, std::uint32_t> state_id;
    std::vector<VerifiedProductRowV1> rows;
    std::map<ExitIdentityV1, std::uint32_t> exit_id;
    std::vector<ExitIdentityV1> exit_keys;

    const auto update_memory = [&]() -> bool {
        const std::uint64_t estimated = estimated_graph_bytes(
            states.size(), rows.size(), transition_count, exit_keys.size());
        peak_bytes = std::max(peak_bytes, estimated);
        return estimated <= limits.max_estimated_bytes;
    };
    bool state_payload_mismatch = false;
    const auto intern_state = [&](const ExactStateV1& exact,
                                  std::vector<std::int64_t> memory,
                                  const std::string& node_id)
            -> std::optional<std::uint32_t> {
        ProductStateKeyV1 key{
            exact.key, exact.hard_execution_state, node_id, memory};
        const auto found = state_id.find(key);
        if (found != state_id.end()) {
            const ExactStateV1& retained = states[found->second].state;
            if (retained.true_condition_identities !=
                    exact.true_condition_identities ||
                retained.offered_choice_ids != exact.offered_choice_ids) {
                state_payload_mismatch = true;
                return std::nullopt;
            }
            return found->second;
        }
        if (states.size() >= limits.max_states) return std::nullopt;
        const std::uint32_t id = static_cast<std::uint32_t>(states.size());
        states.push_back({key, exact, std::move(memory)});
        state_id.emplace(states.back().key, id);
        return id;
    };
    const auto entry = intern_state(
        context.exact_entry, context.initial_controller_memory,
        ir.entry_node_id);
    if (!entry) return refuse("max_states", "entry");
    if (!update_memory()) return refuse("max_estimated_bytes", "entry");

    struct AggregateTransition {
        double probability = 0.0;
        std::optional<ProductStateKeyV1> state;
        std::optional<ExitIdentityV1> exit;
        std::map<std::string, double> weighted_resources;
    };

    const auto choose_edge = [&](
            const FragmentNodeV1& node,
            const ExactStateV1& exact,
            std::string& failure) -> const FragmentEdgeV1* {
        const FragmentEdgeV1* default_edge = nullptr;
        for (const FragmentEdgeV1& edge : node.edges) {
            if (edge.certification_fail_closed_default) {
                default_edge = &edge;
                continue;
            }
            const std::optional<bool> matched =
                oracle.evaluate_condition(edge.condition, exact);
            if (!matched) {
                failure = node.node_id + ":" + edge.edge_id;
                return nullptr;
            }
            if (*matched) return &edge;
        }
        return default_edge;
    };

    std::size_t next_state = 0;
    while (next_state < states.size()) {
        if (auto stop = interrupted()) {
            return refuse(*stop, product_key_bytes(states[next_state].key));
        }
        ++work_items;
        const StateRecord current = states[next_state];
        const FragmentNodeV1& node = *node_by_id.at(
            current.key.controller_node_id);
        VerifiedProductRowV1 row;
        row.source = current.key;
        row.primitive_action =
            node.kind == FragmentNodeKindV1::PrimitiveOperation;
        row.selected_action_identity = row.primitive_action
            ? node.stable_action_identity
            : (node.kind == FragmentNodeKindV1::ObservedChoice
                   ? "control:observed-choice:" + node.node_id
                   : "control:route:" + node.node_id);
        std::map<std::string, AggregateTransition> aggregate;

        const auto add_transition = [&](
                const double probability,
                const ExactStateV1& successor,
                std::vector<std::int64_t> memory,
                const FragmentEdgeV1& edge,
                const ResourceVectorV1& resources) -> bool {
            const FragmentNodeV1& target = *node_by_id.at(edge.target_node_id);
            std::string key_bytes;
            AggregateTransition* value = nullptr;
            if (target.kind == FragmentNodeKindV1::Exit) {
                ExitIdentityV1 exit{
                    canonical_exit_descriptor(target.exit), successor.key,
                    successor.hard_execution_state, memory};
                key_bytes = "E" + exit_key_bytes(exit);
                AggregateTransition& retained = aggregate[key_bytes];
                if (!retained.exit) retained.exit = exit;
                value = &retained;
                if (!exit_id.contains(exit)) {
                    const std::uint32_t id =
                        static_cast<std::uint32_t>(exit_keys.size());
                    exit_id.emplace(exit, id);
                    exit_keys.push_back(std::move(exit));
                }
            } else {
                const auto target_id = intern_state(
                    successor, std::move(memory), target.node_id);
                if (!target_id) return false;
                const ProductStateKeyV1& target_key =
                    states[*target_id].key;
                key_bytes = "S" + product_key_bytes(target_key);
                AggregateTransition& retained = aggregate[key_bytes];
                if (!retained.state) retained.state = target_key;
                value = &retained;
            }
            value->probability += probability;
            for (const auto& [resource, quantity] : resources) {
                value->weighted_resources[resource] +=
                    probability * quantity;
            }
            return true;
        };

        if (node.kind == FragmentNodeKindV1::PrimitiveOperation) {
            const PrimitiveExpansionV1 expansion =
                oracle.expand_primitive(
                    current.state, node.stable_action_identity);
            if (!expansion.action_known) {
                return refuse("unknown_action", node.stable_action_identity);
            }
            if (!expansion.legal) {
                return refuse("illegal_action", node.stable_action_identity);
            }
            if (!expansion.supported) {
                return refuse(
                    "unsupported_primitive",
                    expansion.refusal_reason.empty()
                        ? node.stable_action_identity
                        : expansion.refusal_reason);
            }
            std::vector<AuthoritativePhysicalOutcomeV1> authoritative =
                expansion.authoritative_outcomes;
            std::sort(
                authoritative.begin(), authoritative.end(),
                [](const auto& left, const auto& right) {
                    return left.physical_outcome_id < right.physical_outcome_id;
                });
            long double authoritative_sum = 0.0L;
            for (std::size_t index = 0; index < authoritative.size(); ++index) {
                const auto& outcome = authoritative[index];
                if (outcome.physical_outcome_id.empty() ||
                    !std::isfinite(outcome.probability) ||
                    outcome.probability < 0.0) {
                    return refuse(
                        "invalid_authoritative_outcome",
                        node.stable_action_identity);
                }
                if (index != 0 &&
                    authoritative[index - 1].physical_outcome_id ==
                        outcome.physical_outcome_id) {
                    return refuse(
                        "duplicate_authoritative_outcome",
                        outcome.physical_outcome_id);
                }
                authoritative_sum += outcome.probability;
            }
            if (std::fabs(static_cast<double>(authoritative_sum) - 1.0) >
                limits.probability_sum_tolerance) {
                return refuse(
                    "authoritative_probability_sum_mismatch",
                    node.stable_action_identity);
            }
            std::vector<PrimitivePhysicalOutcomeV1> physical =
                expansion.physical_outcomes;
            std::sort(
                physical.begin(), physical.end(),
                [](const auto& left, const auto& right) {
                    return left.physical_outcome_id < right.physical_outcome_id;
                });
            for (std::size_t index = 0; index < physical.size(); ++index) {
                if (index != 0 &&
                    physical[index - 1].physical_outcome_id ==
                        physical[index].physical_outcome_id) {
                    return refuse(
                        "duplicate_physical_outcome",
                        physical[index].physical_outcome_id);
                }
            }
            if (physical.size() != authoritative.size()) {
                return refuse(
                    "authoritative_outcome_missing",
                    node.stable_action_identity);
            }
            long double physical_sum = 0.0L;
            for (std::size_t index = 0; index < physical.size(); ++index) {
                const PrimitivePhysicalOutcomeV1& outcome = physical[index];
                const AuthoritativePhysicalOutcomeV1& expected =
                    authoritative[index];
                if (outcome.physical_outcome_id !=
                    expected.physical_outcome_id) {
                    return refuse(
                        "authoritative_outcome_identity_mismatch",
                        outcome.physical_outcome_id);
                }
                if (!std::isfinite(outcome.probability) ||
                    outcome.probability < 0.0 ||
                    std::bit_cast<std::uint64_t>(outcome.probability) !=
                        std::bit_cast<std::uint64_t>(expected.probability)) {
                    return refuse(
                        "authoritative_probability_bits_mismatch",
                        outcome.physical_outcome_id);
                }
                if (!finite_nonnegative_vector(outcome.resource_quantities)) {
                    return refuse(
                        "invalid_resource_quantity",
                        outcome.physical_outcome_id);
                }
                for (const auto& [resource, quantity] :
                     outcome.resource_quantities) {
                    (void)quantity;
                    if (!context.resource_vocabulary.contains(resource)) {
                        return refuse("unknown_resource_key", resource);
                    }
                }
                if (!valid_exact_state_identity(outcome.successor)) {
                    return refuse(
                        "invalid_exact_successor",
                        outcome.physical_outcome_id);
                }
                physical_sum += outcome.probability;
                if (transition_count >= limits.max_transitions) {
                    return refuse(
                        "max_transitions", node.stable_action_identity);
                }
                ++transition_count;
                ++work_items;
                if (auto stop = interrupted()) {
                    return refuse(*stop, outcome.physical_outcome_id);
                }
                std::string route_failure;
                const FragmentEdgeV1* edge = choose_edge(
                    node, outcome.successor, route_failure);
                if (edge == nullptr) {
                    return refuse("unexpressible_predicate", route_failure);
                }
                const std::vector<std::int64_t> next_memory =
                    outcome.next_controller_memory
                        ? *outcome.next_controller_memory
                        : current.controller_memory;
                if (next_memory.size() !=
                    ir.controller_memory_schema.size()) {
                    return refuse(
                        "controller_memory_shape_mismatch",
                        outcome.physical_outcome_id);
                }
                row.physical_outcomes.push_back({
                    outcome.physical_outcome_id,
                    std::bit_cast<std::uint64_t>(outcome.probability),
                    outcome.successor.key,
                    outcome.successor.hard_execution_state,
                    next_memory,
                    edge->edge_id,
                    canonical_resource_vector(
                        outcome.resource_quantities)});
                if (outcome.probability == 0.0) {
                    continue;
                }
                if (!add_transition(
                        outcome.probability, outcome.successor, next_memory,
                        *edge, outcome.resource_quantities)) {
                    return refuse(
                        state_payload_mismatch
                            ? "exact_state_payload_mismatch"
                            : "max_states",
                        outcome.physical_outcome_id);
                }
            }
            if (std::fabs(static_cast<double>(physical_sum) - 1.0) >
                limits.probability_sum_tolerance) {
                return refuse(
                    "physical_probability_sum_mismatch",
                    node.stable_action_identity);
            }
        } else {
            if (transition_count >= limits.max_transitions) {
                return refuse("max_transitions", node.node_id);
            }
            ++transition_count;
            std::string route_failure;
            const FragmentEdgeV1* edge = choose_edge(
                node, current.state, route_failure);
            if (edge == nullptr) {
                return refuse("unexpressible_predicate", route_failure);
            }
            if (!add_transition(
                    1.0, current.state, current.controller_memory,
                    *edge, {})) {
                return refuse(
                    state_payload_mismatch
                        ? "exact_state_payload_mismatch"
                        : "max_states",
                    node.node_id);
            }
        }
        long double row_sum = 0.0L;
        for (const auto& [key, aggregated] : aggregate) {
            (void)key;
            VerifiedTransitionV1 transition;
            transition.probability = aggregated.probability;
            transition.target_state = aggregated.state;
            transition.target_exit = aggregated.exit;
            transition.probability_weighted_resource_mass =
                vector_from_map(aggregated.weighted_resources);
            row_sum += transition.probability;
            row.transitions.push_back(std::move(transition));
        }
        row.probability_sum = static_cast<double>(row_sum);
        if (row.transitions.empty() ||
            std::fabs(row.probability_sum - 1.0) >
                limits.probability_sum_tolerance) {
            return refuse("row_probability_sum_mismatch", node.node_id);
        }
        rows.push_back(std::move(row));
        ++next_state;
        if (!update_memory()) {
            return refuse("max_estimated_bytes", node.node_id);
        }
    }

    if (rows.size() != states.size()) {
        return refuse("incomplete_product_graph", ir.fragment_id);
    }
    if (!reserve_work(states.size() + transition_count)) {
        return refuse("max_work_items", "properness");
    }
    if (auto stop = interrupted()) return refuse(*stop, "properness");

    std::vector<std::vector<std::uint32_t>> adjacency(states.size());
    std::vector<std::uint8_t> has_exit(states.size(), 0);
    for (std::size_t row = 0; row < rows.size(); ++row) {
        for (const VerifiedTransitionV1& transition : rows[row].transitions) {
            if (!(transition.probability > 0.0)) continue;
            if (transition.target_exit) {
                has_exit[row] = 1;
            } else if (transition.target_state) {
                adjacency[row].push_back(state_id.at(*transition.target_state));
            }
        }
        std::sort(adjacency[row].begin(), adjacency[row].end());
        adjacency[row].erase(
            std::unique(adjacency[row].begin(), adjacency[row].end()),
            adjacency[row].end());
    }

    std::vector<std::uint32_t> index(states.size(), kNoIndex);
    std::vector<std::uint32_t> lowlink(states.size(), kNoIndex);
    std::vector<std::uint8_t> on_stack(states.size(), 0);
    std::vector<std::uint32_t> stack;
    std::vector<std::vector<std::uint32_t>> components;
    std::uint32_t next_index = 0;
    std::function<void(std::uint32_t)> visit = [&](const std::uint32_t state) {
        index[state] = next_index;
        lowlink[state] = next_index;
        ++next_index;
        stack.push_back(state);
        on_stack[state] = 1;
        for (const std::uint32_t target : adjacency[state]) {
            if (index[target] == kNoIndex) {
                visit(target);
                lowlink[state] = std::min(lowlink[state], lowlink[target]);
            } else if (on_stack[target]) {
                lowlink[state] = std::min(lowlink[state], index[target]);
            }
        }
        if (lowlink[state] != index[state]) return;
        components.emplace_back();
        while (true) {
            const std::uint32_t member = stack.back();
            stack.pop_back();
            on_stack[member] = 0;
            components.back().push_back(member);
            if (member == state) break;
        }
        std::sort(components.back().begin(), components.back().end());
    };
    for (std::uint32_t state = 0; state < states.size(); ++state) {
        if (index[state] == kNoIndex) visit(state);
    }
    if (auto stop = interrupted()) return refuse(*stop, "properness");
    std::vector<std::uint32_t> component_by_state(states.size(), kNoIndex);
    for (std::uint32_t component = 0; component < components.size(); ++component) {
        for (const std::uint32_t state : components[component]) {
            component_by_state[state] = component;
        }
    }
    std::uint32_t cyclic_components = 0;
    std::vector<std::string> canonical_closed;
    for (std::uint32_t component = 0; component < components.size(); ++component) {
        bool cyclic = components[component].size() > 1;
        bool open = false;
        std::vector<std::string> witness;
        for (const std::uint32_t state : components[component]) {
            witness.push_back(product_key_bytes(states[state].key));
            if (has_exit[state]) open = true;
            for (const std::uint32_t target : adjacency[state]) {
                if (target == state) cyclic = true;
                if (component_by_state[target] != component) open = true;
            }
        }
        if (cyclic) ++cyclic_components;
        if (!open) {
            std::sort(witness.begin(), witness.end());
            if (canonical_closed.empty() || witness < canonical_closed) {
                canonical_closed = std::move(witness);
            }
        }
    }
    if (!canonical_closed.empty()) {
        return refuse(
            "improper_closed_nonexit_scc", canonical_closed.front(),
            canonical_closed);
    }

    const std::size_t order = states.size();
    const long double dense_bytes =
        static_cast<long double>(order) * order * sizeof(double) * 2.0L;
    if (dense_bytes > limits.max_estimated_bytes ||
        dense_bytes + peak_bytes > limits.max_estimated_bytes) {
        peak_bytes = std::numeric_limits<std::uint64_t>::max();
        return refuse("max_estimated_bytes", "linear_system");
    }
    peak_bytes = std::max(
        peak_bytes,
        static_cast<std::uint64_t>(dense_bytes + peak_bytes));
    std::vector<std::vector<double>> matrix(
        order, std::vector<double>(order, 0.0));
    for (std::size_t row = 0; row < order; ++row) {
        matrix[row][row] = 1.0;
        for (const VerifiedTransitionV1& transition : rows[row].transitions) {
            if (transition.target_state) {
                matrix[row][state_id.at(*transition.target_state)] -=
                    transition.probability;
            }
        }
    }

    std::sort(exit_keys.begin(), exit_keys.end());
    exit_id.clear();
    for (std::uint32_t index = 0; index < exit_keys.size(); ++index) {
        exit_id.emplace(exit_keys[index], index);
    }
    std::vector<std::vector<double>> exit_probabilities(
        exit_keys.size(), std::vector<double>(order, 0.0));
    const long double exit_solution_bytes =
        static_cast<long double>(exit_keys.size()) * order * sizeof(double);
    if (exit_solution_bytes + peak_bytes >
        limits.max_estimated_bytes) {
        peak_bytes = std::numeric_limits<std::uint64_t>::max();
        return refuse("max_estimated_bytes", "exit_solutions");
    }
    peak_bytes = static_cast<std::uint64_t>(
        exit_solution_bytes + peak_bytes);
    double max_residual = 0.0;
    for (std::size_t exit = 0; exit < exit_keys.size(); ++exit) {
        if (!reserve_work(estimated_dense_solve_work(order))) {
            return refuse("max_work_items", "absorption");
        }
        if (auto stop = interrupted()) return refuse(*stop, "absorption");
        std::vector<double> rhs(order, 0.0);
        for (std::size_t row = 0; row < order; ++row) {
            for (const VerifiedTransitionV1& transition : rows[row].transitions) {
                if (transition.target_exit &&
                    *transition.target_exit == exit_keys[exit]) {
                    rhs[row] += transition.probability;
                }
            }
        }
        auto solution = solve_dense_system(matrix, rhs);
        if (!solution) return refuse("singular_absorption_system", "exit");
        max_residual = std::max(
            max_residual, residual_for(matrix, *solution, rhs));
        exit_probabilities[exit] = std::move(*solution);
        if (auto stop = interrupted()) return refuse(*stop, "absorption");
    }
    double max_mass_error = 0.0;
    for (std::size_t state = 0; state < order; ++state) {
        long double mass = 0.0L;
        for (const auto& probabilities : exit_probabilities) {
            mass += probabilities[state];
        }
        max_mass_error = std::max(
            max_mass_error,
            std::fabs(static_cast<double>(mass) - 1.0));
    }
    if (max_mass_error > limits.probability_sum_tolerance) {
        return refuse("exit_probability_sum_mismatch", ir.fragment_id);
    }
    const double exit_probability_sum = [&]() {
        long double sum = 0.0L;
        for (const auto& probabilities : exit_probabilities) {
            sum += probabilities.front();
        }
        return static_cast<double>(sum);
    }();

    std::set<std::string> resource_keys;
    std::set<std::string> action_keys;
    for (const VerifiedProductRowV1& row : rows) {
        if (row.primitive_action) action_keys.insert(row.selected_action_identity);
        for (const VerifiedTransitionV1& transition : row.transitions) {
            for (const auto& [resource, mass] :
                 transition.probability_weighted_resource_mass) {
                (void)mass;
                resource_keys.insert(resource);
            }
        }
    }
    std::map<std::string, double> expected_resources;
    std::map<std::string, double> expected_actions;
    for (const std::string& resource : resource_keys) {
        if (!reserve_work(estimated_dense_solve_work(order))) {
            return refuse("max_work_items", "resources:" + resource);
        }
        if (auto stop = interrupted()) {
            return refuse(*stop, "resources:" + resource);
        }
        std::vector<double> rhs(order, 0.0);
        for (std::size_t row = 0; row < order; ++row) {
            for (const VerifiedTransitionV1& transition : rows[row].transitions) {
                const auto found = std::find_if(
                    transition.probability_weighted_resource_mass.begin(),
                    transition.probability_weighted_resource_mass.end(),
                    [&](const auto& entry) { return entry.first == resource; });
                if (found !=
                    transition.probability_weighted_resource_mass.end()) {
                    rhs[row] += found->second;
                }
            }
        }
        auto solution = solve_dense_system(matrix, rhs);
        if (!solution) return refuse("singular_resource_system", resource);
        max_residual = std::max(
            max_residual, residual_for(matrix, *solution, rhs));
        expected_resources[resource] = solution->front();
    }
    for (const std::string& action : action_keys) {
        if (!reserve_work(estimated_dense_solve_work(order))) {
            return refuse("max_work_items", "actions:" + action);
        }
        if (auto stop = interrupted()) {
            return refuse(*stop, "actions:" + action);
        }
        std::vector<double> rhs(order, 0.0);
        for (std::size_t row = 0; row < order; ++row) {
            if (rows[row].primitive_action &&
                rows[row].selected_action_identity == action) {
                rhs[row] = 1.0;
            }
        }
        auto solution = solve_dense_system(matrix, rhs);
        if (!solution) return refuse("singular_action_system", action);
        max_residual = std::max(
            max_residual, residual_for(matrix, *solution, rhs));
        expected_actions[action] = solution->front();
    }
    if (max_residual > limits.residual_tolerance) {
        return refuse("linear_residual_exceeded", ir.fragment_id);
    }

    std::vector<VerifiedExitV1> verified_exits;
    verified_exits.reserve(exit_keys.size());
    std::map<std::string, double> joint_resource_totals;
    for (std::size_t exit = 0; exit < exit_keys.size(); ++exit) {
        VerifiedExitV1 verified_exit;
        verified_exit.identity = exit_keys[exit];
        verified_exit.probability_from_entry = exit_probabilities[exit].front();
        std::map<std::string, double> joint;
        for (const std::string& resource : resource_keys) {
            if (!reserve_work(estimated_dense_solve_work(order))) {
                return refuse(
                    "max_work_items", "joint_resources:" + resource);
            }
            if (auto stop = interrupted()) {
                return refuse(*stop, "joint_resources:" + resource);
            }
            std::vector<double> rhs(order, 0.0);
            for (std::size_t row = 0; row < order; ++row) {
                for (const VerifiedTransitionV1& transition :
                     rows[row].transitions) {
                    const auto found = std::find_if(
                        transition.probability_weighted_resource_mass.begin(),
                        transition.probability_weighted_resource_mass.end(),
                        [&](const auto& entry) {
                            return entry.first == resource;
                        });
                    if (found ==
                        transition.probability_weighted_resource_mass.end()) {
                        continue;
                    }
                    double eventual = 0.0;
                    if (transition.target_exit) {
                        eventual = *transition.target_exit == exit_keys[exit]
                            ? 1.0
                            : 0.0;
                    } else if (transition.target_state) {
                        eventual = exit_probabilities[exit][
                            state_id.at(*transition.target_state)];
                    }
                    rhs[row] += found->second * eventual;
                }
            }
            auto solution = solve_dense_system(matrix, rhs);
            if (!solution) {
                return refuse("singular_joint_resource_system", resource);
            }
            max_residual = std::max(
                max_residual, residual_for(matrix, *solution, rhs));
            joint[resource] = solution->front();
            joint_resource_totals[resource] += solution->front();
        }
        verified_exit.joint_resource_mass_from_entry = vector_from_map(joint);
        verified_exits.push_back(std::move(verified_exit));
    }
    for (const auto& [resource, expected] : expected_resources) {
        const double delta = std::fabs(
            joint_resource_totals[resource] - expected);
        max_residual = std::max(max_residual, delta);
        if (delta > limits.residual_tolerance *
                std::max(1.0, std::fabs(expected))) {
            return refuse("joint_resource_reconciliation_failed", resource);
        }
    }
    if (max_residual > limits.residual_tolerance) {
        return refuse("linear_residual_exceeded", ir.fragment_id);
    }

    std::optional<double> priced_expected_cost = 0.0;
    for (const auto& [resource, expected] : expected_resources) {
        const auto price = context.prices.find(resource);
        if (price == context.prices.end() ||
            !std::isfinite(price->second) || price->second < 0.0) {
            priced_expected_cost.reset();
            break;
        }
        *priced_expected_cost += price->second * expected;
        if (!std::isfinite(*priced_expected_cost)) {
            priced_expected_cost.reset();
            break;
        }
    }

    std::vector<std::size_t> row_order(rows.size());
    for (std::size_t index = 0; index < rows.size(); ++index) {
        row_order[index] = index;
    }
    std::sort(
        row_order.begin(), row_order.end(),
        [&](const std::size_t left, const std::size_t right) {
            return rows[left].source < rows[right].source;
        });
    std::string certificate_bytes;
    append_string(certificate_bytes, structural.identity.canonical_bytes);
    append_string(certificate_bytes, kExecutableFragmentVerifierVersionV1);
    append_string(certificate_bytes, kExecutableFragmentPropernessVersionV1);
    append_size(certificate_bytes, rows.size());
    for (const std::size_t index : row_order) {
        const VerifiedProductRowV1& row = rows[index];
        append_string(certificate_bytes, product_key_bytes(row.source));
        append_string(certificate_bytes, row.selected_action_identity);
        append_integer(certificate_bytes, row.primitive_action ? 1u : 0u);
        append_size(certificate_bytes, row.physical_outcomes.size());
        for (const VerifiedPhysicalOutcomeV1& outcome : row.physical_outcomes) {
            append_string(certificate_bytes, outcome.physical_outcome_id);
            append_integer(certificate_bytes, outcome.probability_bits);
            append_exact_key(
                certificate_bytes, outcome.successor_exact_item_key);
            append_i64_vector(
                certificate_bytes,
                outcome.successor_hard_execution_state);
            append_i64_vector(
                certificate_bytes, outcome.next_controller_memory);
            append_string(certificate_bytes, outcome.selected_edge_id);
            append_size(
                certificate_bytes, outcome.resource_quantities.size());
            for (const auto& [resource, quantity] :
                 outcome.resource_quantities) {
                append_string(certificate_bytes, resource);
                append_double(certificate_bytes, quantity);
            }
        }
        append_size(certificate_bytes, row.transitions.size());
        for (const VerifiedTransitionV1& transition : row.transitions) {
            append_double(certificate_bytes, transition.probability);
            append_string(
                certificate_bytes,
                transition.target_state
                    ? "S" + product_key_bytes(*transition.target_state)
                    : "E" + exit_key_bytes(*transition.target_exit));
            append_size(
                certificate_bytes,
                transition.probability_weighted_resource_mass.size());
            for (const auto& [resource, mass] :
                 transition.probability_weighted_resource_mass) {
                append_string(certificate_bytes, resource);
                append_double(certificate_bytes, mass);
            }
        }
    }
    append_size(certificate_bytes, verified_exits.size());
    for (const VerifiedExitV1& exit : verified_exits) {
        append_string(certificate_bytes, exit_key_bytes(exit.identity));
        append_double(certificate_bytes, exit.probability_from_entry);
        for (const auto& [resource, mass] :
             exit.joint_resource_mass_from_entry) {
            append_string(certificate_bytes, resource);
            append_double(certificate_bytes, mass);
        }
    }
    const CanonicalIdentityV1 certificate_identity =
        identity_from_bytes(std::move(certificate_bytes));

    VerifiedLeafFragmentV1 verified(
        VerifiedLeafFragmentV1::ConstructionToken{},
        std::move(ir), structural.identity, certificate_identity,
        std::move(rows), std::move(verified_exits),
        vector_from_map(expected_resources), vector_from_map(expected_actions),
        priced_expected_cost, exit_probability_sum, max_mass_error,
        max_residual, static_cast<std::uint32_t>(components.size()),
        cyclic_components, work_items, peak_bytes);
    result.verified = std::move(verified);
    return result;
}

} // namespace fragment_v1
} // namespace solver
} // namespace poecraft
