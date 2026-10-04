#pragma once
#include "recombination.hpp"

namespace poecraft {
inline constexpr char kRecombinationConstraintAuthority[] = "native-metadata-exclusive-output-constraints-v1";
enum class RecombExclusivity { NonExclusive, Exclusive, Unresolved };
enum class RecombOrigin { Natural, EssenceOnly, Metamod, Delve, Unveiled, VeilTemplate,
    Elevated, BeastAspect, CraftedUnresolved, InfluencedNatural, NonNaturalUnresolved, NotExplicit };
struct RecombModConstraint {
    RecombExclusivity exclusivity = RecombExclusivity::Unresolved;
    RecombOrigin origin = RecombOrigin::NonNaturalUnresolved;
    bool positive_source_spawn_proxy = false, natural_on_source = false, natural_on_compatible_base = false;
    bool guaranteed_natural_essence_source = false;
    bool explicit_modifier = false;
};
RecombModConstraint classify_recombination_mod(const SessionImpl&, std::uint32_t mod);
const char* recombination_origin_name(RecombOrigin);
const char* recombination_exclusivity_name(RecombExclusivity);
/* Known output constraint only: no count/order/weight law is inferred. */
bool recombination_mods_can_coexist(const RecombOccurrence&, RecombExclusivity,
    const RecombOccurrence&, RecombExclusivity);
void validate_random_recomb_carrier_session(const SessionImpl&);
/* Structural item legality is independent of pair probabilities/categories. */
void validate_recombination_item_structure(const CraftResource&);
void validate_recombination_resource_pair(const CraftResource&, const CraftResource&);
std::string inspect_random_recombination_constraints(const CraftResource&, const CraftResource&);
}
