#pragma once

#include "solver_model.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>

namespace poecraft::solver::solve_detail {

/*
 * One typed lifecycle for every carrier/operator obligation admitted to the
 * product action envelope. Gate 1 observes the legacy scheduler; later gates
 * may consume this view only after enabling scheduler_view_enabled explicitly.
 * None of the evidence flags is an independent mechanic rule: each names an
 * existing exact authority consulted by the row/admission path.
 */
enum class ActionEnvelopeState : std::uint8_t {
    Queued = 0,
    ExactRowComplete,
    ExactInapplicabilityProved,
    IncumbentDominated,
    RolledBackAfterCap,
    OmittedCallerScope,
    UnresolvedNamedStop,
    Count,
};

enum class ActionEnvelopeLane : std::uint8_t {
    Unassigned = 0,
    RestrictedAnchor,
    IncrementalCarrierLocal,
    IncrementalAutomatic,
    IncrementalPriority,
    IncrementalOperatorMajor,
    IncrementalClosure,
    ExplicitCallerScope,
    Count,
};

enum class ActionEnvelopeProofAuthority : std::uint8_t {
    None = 0,
    ExactRowMaterialization,
    ExactRegistryLegality,
    IndependentGlobalLowerVsVerifiedUpper,
    ExplicitCallerScope,
    TransactionalResourceCap,
    NamedOpenObligation,
    Count,
};

enum class ActionEnvelopeStopOwner : std::uint8_t {
    None = 0,
    ResourceCap,
    SuccessorFrontier,
    MissingVerifiedUpper,
    RequestedBoundedFinish,
    Count,
};

enum ActionEnvelopeEvidence : std::uint32_t {
    EnvelopeEvidenceNone = 0,
    EnvelopeEvidenceCarrierFacts = 1u << 0,
    EnvelopeEvidenceCarrierEffectSummary = 1u << 1,
    EnvelopeEvidenceCarrierSuccessorEnvelope = 1u << 2,
    EnvelopeEvidenceActionRefinementContract = 1u << 3,
    EnvelopeEvidenceExactRegistryLegality = 1u << 4,
    EnvelopeEvidenceExactOptionKernel = 1u << 5,
};

struct ActionEnvelopeEntry {
    std::uint32_t state = kNoId;
    std::uint32_t operator_index = kNoId;
    ActionEnvelopeState lifecycle = ActionEnvelopeState::Queued;
    ActionEnvelopeLane lane = ActionEnvelopeLane::Unassigned;
    ActionEnvelopeProofAuthority authority =
        ActionEnvelopeProofAuthority::None;
    ActionEnvelopeStopOwner stop_owner = ActionEnvelopeStopOwner::None;
    std::uint32_t evidence = EnvelopeEvidenceNone;
    std::uint64_t row_index = std::numeric_limits<std::uint64_t>::max();
    std::uint64_t revision = 0;
    std::string detail;
};

class ActionEnvelopeLedger {
public:
    using Map = std::unordered_map<std::uint64_t, ActionEnvelopeEntry>;
    static constexpr std::size_t kStateCount =
        static_cast<std::size_t>(ActionEnvelopeState::Count);
    static constexpr std::size_t kLaneCount =
        static_cast<std::size_t>(ActionEnvelopeLane::Count);
    static constexpr std::size_t kAuthorityCount =
        static_cast<std::size_t>(ActionEnvelopeProofAuthority::Count);
    static constexpr std::size_t kStopOwnerCount =
        static_cast<std::size_t>(ActionEnvelopeStopOwner::Count);

    bool scheduler_view_enabled = false;

    static constexpr std::uint64_t key(
            const std::uint32_t state,
            const std::uint32_t operator_index) {
        return (static_cast<std::uint64_t>(state) << 32) | operator_index;
    }

    const ActionEnvelopeEntry* find(
            const std::uint32_t state,
            const std::uint32_t operator_index) const {
        const auto found = entries_.find(key(state, operator_index));
        return found == entries_.end() ? nullptr : &found->second;
    }

    bool scheduling_complete(
            const std::uint32_t state,
            const std::uint32_t operator_index) const {
        const ActionEnvelopeEntry* entry = find(state, operator_index);
        if (entry == nullptr) return false;
        /* A materialized row is never scheduled a second time merely because
         * its value proof remains open. The lifecycle can move from
         * ExactRowComplete to UnresolvedNamedStop while row_index continues
         * to own the exact materialization witness. */
        if (entry->row_index !=
            std::numeric_limits<std::uint64_t>::max()) {
            return true;
        }
        switch (entry->lifecycle) {
        case ActionEnvelopeState::ExactRowComplete:
        case ActionEnvelopeState::ExactInapplicabilityProved:
        case ActionEnvelopeState::IncumbentDominated:
        case ActionEnvelopeState::OmittedCallerScope:
            return true;
        case ActionEnvelopeState::Queued:
        case ActionEnvelopeState::RolledBackAfterCap:
        case ActionEnvelopeState::UnresolvedNamedStop:
        case ActionEnvelopeState::Count:
            return false;
        }
        return false;
    }

    void discard_row_references_outside(
            const std::uint64_t retained_row_count) {
        for (auto& [unused_key, entry] : entries_) {
            (void)unused_key;
            if (entry.row_index >= retained_row_count) {
                entry.row_index = std::numeric_limits<std::uint64_t>::max();
            }
        }
    }

    void queue(
            const std::uint32_t state,
            const std::uint32_t operator_index,
            const ActionEnvelopeLane lane,
            const std::uint32_t evidence = EnvelopeEvidenceNone) {
        transition(
            state, operator_index, ActionEnvelopeState::Queued, lane,
            ActionEnvelopeProofAuthority::None,
            ActionEnvelopeStopOwner::None, evidence,
            std::numeric_limits<std::uint64_t>::max(), {});
    }

    void exact_row_complete(
            const std::uint32_t state,
            const std::uint32_t operator_index,
            const ActionEnvelopeLane lane,
            const std::uint64_t row_index,
            const std::uint32_t evidence) {
        transition(
            state, operator_index, ActionEnvelopeState::ExactRowComplete,
            lane, ActionEnvelopeProofAuthority::ExactRowMaterialization,
            ActionEnvelopeStopOwner::None, evidence, row_index, {});
    }

    void exact_inapplicable(
            const std::uint32_t state,
            const std::uint32_t operator_index,
            const ActionEnvelopeLane lane,
            const std::uint32_t evidence) {
        transition(
            state, operator_index,
            ActionEnvelopeState::ExactInapplicabilityProved, lane,
            ActionEnvelopeProofAuthority::ExactRegistryLegality,
            ActionEnvelopeStopOwner::None, evidence,
            std::numeric_limits<std::uint64_t>::max(), {});
    }

    void incumbent_dominated(
            const std::uint32_t state,
            const std::uint32_t operator_index,
            const std::uint64_t row_index,
            const std::uint32_t evidence) {
        const ActionEnvelopeEntry* prior = find(state, operator_index);
        transition(
            state, operator_index, ActionEnvelopeState::IncumbentDominated,
            prior == nullptr ? ActionEnvelopeLane::Unassigned : prior->lane,
            ActionEnvelopeProofAuthority::
                IndependentGlobalLowerVsVerifiedUpper,
            ActionEnvelopeStopOwner::None, evidence, row_index, {});
    }

    void rolled_back_after_cap(
            const std::uint32_t state,
            const std::uint32_t operator_index,
            const ActionEnvelopeLane lane,
            std::string cap_name,
            const std::uint32_t evidence) {
        transition(
            state, operator_index, ActionEnvelopeState::RolledBackAfterCap,
            lane, ActionEnvelopeProofAuthority::TransactionalResourceCap,
            ActionEnvelopeStopOwner::ResourceCap, evidence,
            std::numeric_limits<std::uint64_t>::max(), std::move(cap_name));
    }

    void omitted_by_caller_scope(
            const std::uint32_t operator_index,
            const std::uint32_t evidence) {
        transition(
            kNoId, operator_index, ActionEnvelopeState::OmittedCallerScope,
            ActionEnvelopeLane::ExplicitCallerScope,
            ActionEnvelopeProofAuthority::ExplicitCallerScope,
            ActionEnvelopeStopOwner::None, evidence,
            std::numeric_limits<std::uint64_t>::max(), {});
    }

    void unresolved(
            const std::uint32_t state,
            const std::uint32_t operator_index,
            const ActionEnvelopeStopOwner owner,
            const std::uint64_t row_index,
            std::string detail,
            const std::uint32_t evidence) {
        const ActionEnvelopeEntry* prior = find(state, operator_index);
        transition(
            state, operator_index, ActionEnvelopeState::UnresolvedNamedStop,
            prior == nullptr ? ActionEnvelopeLane::Unassigned : prior->lane,
            ActionEnvelopeProofAuthority::NamedOpenObligation, owner,
            evidence, row_index, std::move(detail));
    }

    void mark_queued_unresolved(
            const ActionEnvelopeStopOwner owner,
            const std::string& detail) {
        for (auto& [unused_key, entry] : entries_) {
            (void)unused_key;
            if (entry.lifecycle != ActionEnvelopeState::Queued) continue;
            entry.lifecycle = ActionEnvelopeState::UnresolvedNamedStop;
            entry.authority = ActionEnvelopeProofAuthority::NamedOpenObligation;
            entry.stop_owner = owner;
            entry.detail = detail;
            entry.revision = ++transition_count_;
        }
    }

    const Map& entries() const { return entries_; }
    std::uint64_t transition_count() const { return transition_count_; }

    void restore_checkpoint(
            const bool restored_scheduler_view_enabled,
            const std::uint64_t restored_transition_count,
            Map restored_entries) {
        std::uint64_t max_revision = 0;
        for (const auto& [stored_key, entry] : restored_entries) {
            if (stored_key != key(entry.state, entry.operator_index) ||
                entry.lifecycle == ActionEnvelopeState::Count ||
                entry.lane == ActionEnvelopeLane::Count ||
                entry.authority == ActionEnvelopeProofAuthority::Count ||
                entry.stop_owner == ActionEnvelopeStopOwner::Count) {
                throw std::invalid_argument(
                    "invalid checkpoint action-envelope entry");
            }
            max_revision = std::max(max_revision, entry.revision);
        }
        if (restored_transition_count < max_revision) {
            throw std::invalid_argument(
                "checkpoint action-envelope revision exceeds transition count");
        }
        scheduler_view_enabled = restored_scheduler_view_enabled;
        transition_count_ = restored_transition_count;
        entries_ = std::move(restored_entries);
    }

    std::array<std::uint64_t, kStateCount> state_counts() const {
        std::array<std::uint64_t, kStateCount> counts{};
        for (const auto& [unused_key, entry] : entries_) {
            (void)unused_key;
            ++counts.at(static_cast<std::size_t>(entry.lifecycle));
        }
        return counts;
    }

    std::uint64_t estimated_owned_bytes() const {
        std::uint64_t bytes =
            entries_.bucket_count() * sizeof(void*) +
            entries_.size() *
                (sizeof(Map::value_type) + 2 * sizeof(void*));
        for (const auto& [unused_key, entry] : entries_) {
            (void)unused_key;
            bytes += entry.detail.capacity() + 1;
        }
        return bytes;
    }

private:
    Map entries_;
    std::uint64_t transition_count_ = 0;

    void transition(
            const std::uint32_t state,
            const std::uint32_t operator_index,
            const ActionEnvelopeState lifecycle,
            const ActionEnvelopeLane lane,
            const ActionEnvelopeProofAuthority authority,
            const ActionEnvelopeStopOwner stop_owner,
            const std::uint32_t evidence,
            const std::uint64_t row_index,
            std::string detail) {
        if (lifecycle == ActionEnvelopeState::Count ||
            lane == ActionEnvelopeLane::Count ||
            authority == ActionEnvelopeProofAuthority::Count ||
            stop_owner == ActionEnvelopeStopOwner::Count) {
            throw std::logic_error("invalid action-envelope ledger state");
        }
        const std::uint64_t identity = key(state, operator_index);
        auto [position, inserted] = entries_.try_emplace(identity);
        ActionEnvelopeEntry& entry = position->second;
        if (inserted) {
            entry.state = state;
            entry.operator_index = operator_index;
        } else if (entry.lifecycle == ActionEnvelopeState::OmittedCallerScope &&
                   lifecycle != ActionEnvelopeState::OmittedCallerScope) {
            throw std::logic_error(
                "caller-omitted action-envelope obligation was reopened");
        } else if (entry.lifecycle == ActionEnvelopeState::ExactInapplicabilityProved &&
                   lifecycle == ActionEnvelopeState::ExactRowComplete) {
            throw std::logic_error(
                "exactly inapplicable action-envelope obligation materialized");
        }
        entry.lifecycle = lifecycle;
        entry.lane = lane;
        entry.authority = authority;
        entry.stop_owner = stop_owner;
        entry.evidence |= evidence;
        entry.row_index = row_index;
        entry.detail = std::move(detail);
        entry.revision = ++transition_count_;
    }
};

} // namespace poecraft::solver::solve_detail
