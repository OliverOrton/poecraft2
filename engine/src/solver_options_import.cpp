#include "solver_options_runtime_helpers.hpp"

namespace poecraft {
namespace solver {

std::uint32_t CalcContext::import_planner_operator(
    const PlannerOperator& planner,
    const bool state_local) {
    require_import_reference_shape(
        planner.primitive_action,
        planner.primitive_action_id,
        "primitive_action");
    if (planner.primitive_program.size() !=
        planner.primitive_program_action_ids.size()) {
        throw std::invalid_argument(
            "planner operator primitive program is missing canonical ids");
    }
    for (std::size_t i = 0;
         i < planner.primitive_program.size(); ++i) {
        require_import_reference_shape(
            planner.primitive_program[i],
            planner.primitive_program_action_ids[i],
            "primitive_program");
    }
    require_import_reference_shape(
        planner.conditional_action,
        planner.conditional_action_id,
        "conditional_action");
    require_import_reference_shape(
        planner.bestiary_create_action,
        planner.bestiary_create_action_id,
        "bestiary_create_action");
    require_import_reference_shape(
        planner.bestiary_restore_action,
        planner.bestiary_restore_action_id,
        "bestiary_restore_action");
    require_import_reference_shape(
        planner.setup_action,
        planner.setup_action_id,
        "setup_action");
    require_import_reference_shape(
        planner.followup_action,
        planner.followup_action_id,
        "followup_action");
    require_import_reference_shape(
        planner.cleanup_action,
        planner.cleanup_action_id,
        "cleanup_action");
    require_import_reference_shape(
        planner.constructive_finish_action,
        planner.constructive_finish_action_id,
        "constructive_finish_action");

    PlannerOperator mapped = planner;
    const auto map_optional_primitive =
        [&](const std::string& id,
            const char* field) -> std::uint32_t {
        return id.empty()
                   ? kNoId
                   : resolve_imported_primitive(
                         registry_, id, field);
    };
    mapped.primitive_action = map_optional_primitive(
        planner.primitive_action_id, "primitive_action");
    mapped.primitive_program.clear();
    mapped.primitive_program.reserve(
        planner.primitive_program_action_ids.size());
    for (const std::string& id :
         planner.primitive_program_action_ids) {
        if (id.empty()) {
            throw std::invalid_argument(
                "planner operator primitive program contains an empty id");
        }
        mapped.primitive_program.push_back(
            resolve_imported_primitive(
                registry_, id, "primitive_program"));
    }
    mapped.conditional_action = map_optional_primitive(
        planner.conditional_action_id, "conditional_action");
    mapped.setup_action = map_optional_primitive(
        planner.setup_action_id, "setup_action");
    mapped.followup_action = map_optional_primitive(
        planner.followup_action_id, "followup_action");
    mapped.cleanup_action = map_optional_primitive(
        planner.cleanup_action_id, "cleanup_action");
    mapped.constructive_finish_action = map_optional_primitive(
        planner.constructive_finish_action_id,
        "constructive_finish_action");
    mapped.bestiary_create_action =
        planner.bestiary_create_action_id.empty()
            ? kNoId
            : resolve_imported_bestiary(
                  *session_,
                  planner.bestiary_create_action_id,
                  "bestiary_create_action");
    mapped.bestiary_restore_action =
        planner.bestiary_restore_action_id.empty()
            ? kNoId
            : resolve_imported_bestiary(
                  *session_,
                  planner.bestiary_restore_action_id,
                  "bestiary_restore_action");

    if (mapped.kind == PlannerOperatorKind::Primitive) {
        if (mapped.primitive_action == kNoId ||
            mapped.primitive_program.size() != 1 ||
            mapped.primitive_program.front() !=
                mapped.primitive_action) {
            throw std::invalid_argument(
                "primitive planner operator must name its one exact "
                "primitive dependency");
        }
        if (mapped.primitive_action >= operators_.size() ||
            operators_[mapped.primitive_action].kind !=
                PlannerOperatorKind::Primitive ||
            !planner_operator_structurally_equal(
                operators_[mapped.primitive_action], mapped)) {
            throw std::invalid_argument(
                "primitive planner operator semantics differ from the "
                "destination registry wrapper");
        }
        return mapped.primitive_action;
    }
    if (mapped.kind != PlannerOperatorKind::FixedOption) {
        throw std::invalid_argument(
            "planner operator kind is not supported");
    }
    if (mapped.primitive_action != kNoId ||
        mapped.primitive_program.empty()) {
        throw std::invalid_argument(
            "fixed planner option has an invalid primitive dependency "
            "shape");
    }
    /*
     * Imported automatic operators cross CalcContext boundaries. Apply the
     * same complete runtime-path admission contract as initial planner
     * construction before an invalid composite can enter the candidate set.
     */
    (void)planner_operator_runtime_semantics(mapped, registry_);

    for (std::uint32_t index = 0; index < operators_.size(); ++index) {
        if (!planner_operator_structurally_equal(
                operators_[index], mapped)) {
            continue;
        }
        if (state_local) {
            state_local_automatic_operator_indices_.insert(index);
        }
        return index;
    }
    if (operators_.size() >=
        static_cast<std::size_t>(kNoId)) {
        throw std::overflow_error(
            "planner operator index space exhausted");
    }
    const std::uint32_t result =
        static_cast<std::uint32_t>(operators_.size());
    operators_.push_back(std::move(mapped));
    account_new_operator(operators_.back());
    const std::uint64_t template_id =
        option_planner_hash(operators_.back());
    auto& bucket = option_operator_templates_[template_id];
    const std::size_t old_capacity = bucket.capacity();
    bucket.push_back(result);
    account_operator_template_insert(old_capacity, bucket);
    if (state_local) {
        state_local_automatic_operator_indices_.insert(result);
    }
    return result;
}

} // namespace solver
} // namespace poecraft
