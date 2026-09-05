#pragma once

#include "solver_refinement.hpp"

#include <map>
#include <set>
#include <string>
#include <vector>

namespace poecraft::solver {

/* Immutable caller/generation snapshot supplied by the action authority, not
 * inferred from the rows that happened to finish. An open generated family
 * always retains a residual constraint, including when every known member
 * has been expanded. Identity binds the generator and membership contract. */
struct CanonicalActionFamily {
    refinement::StableKey identity;
    std::vector<refinement::StableKey> members;
    bool open = true;
};

struct CanonicalActionSet {
    refinement::StableKey scope_identity;
    std::uint64_t generation = 0;
    bool complete = false;
    std::vector<refinement::StableKey> actions;
    std::vector<CanonicalActionFamily> families;
};

struct CanonicalActionCover {
    refinement::StableKey identity;
    bool family = false;
    std::vector<refinement::StableKey> excluded_members;
};

/* Full-key equality and a disjoint partition, never a cardinality proof.
 * Exact inapplicability still supplies the action's explicit cover. Disabled
 * caller actions must be absent from the authoritative expected set. */
inline std::string validate_canonical_action_coverage(
        const CanonicalActionSet& expected,
        const std::vector<CanonicalActionCover>& actual) {
    using Key = refinement::StableKey;
    if (!expected.complete || expected.scope_identity.empty() ||
        expected.generation == 0) return "action scope is not complete";
    std::set<Key> universe;
    for (const Key& action : expected.actions) {
        if (action.empty() || !universe.insert(action).second)
            return "duplicate or empty expected action";
    }
    std::map<Key, const CanonicalActionFamily*> families;
    for (const auto& family : expected.families) {
        if (family.identity.empty() ||
            !families.emplace(family.identity, &family).second)
            return "duplicate or empty expected family";
        for (const Key& member : family.members) {
            if (member.empty() || !universe.insert(member).second)
                return "overlapping expected family membership";
        }
    }
    std::set<Key> explicit_actions;
    std::map<Key, const CanonicalActionCover*> residuals;
    for (const auto& cover : actual) {
        if (cover.family) {
            if (!families.contains(cover.identity) ||
                !residuals.emplace(cover.identity, &cover).second)
                return "unknown or duplicate residual family";
        } else if (!cover.excluded_members.empty() ||
                   !universe.contains(cover.identity) ||
                   !explicit_actions.insert(cover.identity).second) {
            return "unknown, disabled, or duplicate action";
        }
    }
    for (const Key& action : expected.actions)
        if (!explicit_actions.contains(action)) return "missing action";
    for (const auto& [key, family] : families) {
        std::set<Key> exclusions;
        for (const Key& member : family->members)
            if (explicit_actions.contains(member)) exclusions.insert(member);
        const bool required = family->open ||
            exclusions.size() != family->members.size();
        const auto found = residuals.find(key);
        if (required != (found != residuals.end()))
            return "missing residual family or stale replaced placeholder";
        if (!required) continue;
        const auto& supplied = found->second->excluded_members;
        const std::set<Key> unique(supplied.begin(), supplied.end());
        if (unique.size() != supplied.size() || unique != exclusions)
            return "residual family exclusions do not match replacements";
    }
    return {};
}

} // namespace poecraft::solver
