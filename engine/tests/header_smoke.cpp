#include <poecraft/api.h>
#include <poecraft/hinekora.h>
#include <poecraft/recombination.h>
#include <poecraft/recombination_solver.h>
#include <poecraft/bestiary.h>
#include <poecraft/simulator.h>

#include <type_traits>

static_assert(PC_ABI_VERSION == 3u);
static_assert(PC_RECOMBINATION_PAIR_VERSION == 1u);
static_assert(PC_RECOMBINATION_CONSTRAINT_VERSION == 1u);
static_assert(PC_RECOMBINATION_SOLVER_VERSION == 2u);
static_assert(std::is_standard_layout_v<pc_recombination_solver_options>);
static_assert(std::is_standard_layout_v<pc_recombination_result>);
static_assert(std::is_standard_layout_v<pc_error_info>);
static_assert(std::is_standard_layout_v<pc_simulation_options>);
static_assert(std::is_standard_layout_v<pc_bestiary_craft_state>);
static_assert(PC_BESTIARY_MAX_COST_KEYS == 4);

int main() {
    return PC_RESULT_UNSUPPORTED_FEATURE == 4 ? 0 : 1;
}
