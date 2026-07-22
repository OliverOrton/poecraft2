#include <poecraft/api.h>
#include <poecraft/bestiary.h>
#include <poecraft/simulator.h>

#include <type_traits>

static_assert(PC_ABI_VERSION == 2u);
static_assert(std::is_standard_layout_v<pc_error_info>);
static_assert(std::is_standard_layout_v<pc_simulation_options>);
static_assert(std::is_standard_layout_v<pc_bestiary_craft_state>);
static_assert(PC_BESTIARY_MAX_COST_KEYS == 4);

int main() {
    return PC_RESULT_UNSUPPORTED_FEATURE == 4 ? 0 : 1;
}
