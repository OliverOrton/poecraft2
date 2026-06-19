#include <poecraft/api.h>
#include <poecraft/simulator.h>

#include <type_traits>

static_assert(PC_ABI_VERSION == 1u);
static_assert(std::is_standard_layout_v<pc_error_info>);
static_assert(std::is_standard_layout_v<pc_simulation_options>);

int main() {
    return PC_RESULT_UNSUPPORTED_FEATURE == 4 ? 0 : 1;
}
