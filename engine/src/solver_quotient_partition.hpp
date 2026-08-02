#pragma once

#include "solver_quotient_proof.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace poecraft {
namespace solver {
namespace quotient {

/*
 * Transient exact carrier graph submitted to the existing closed
 * probabilistic partition. Internal targets must name another carrier in the
 * same submission. A target outside the submission must carry an immutable
 * external-frontier identity; an unlabeled open edge is rejected before the
 * shared partition authority is called.
 */
struct CertifiedCarrierArc {
    StableKey label;
    std::optional<StableKey> internal_successor;
    std::optional<StableKey> external_frontier;
    double probability = 0.0;

    bool operator==(const CertifiedCarrierArc&) const = default;
};

struct CertifiedCarrierNode {
    StableKey stable_key;
    std::uint32_t coarse_state = 0;
    StableKey coarse_parent;
    ObservationRequirement observation_requirement;
    FeatureSignature exact_features;
    StableKey immediate_identity;
    bool terminal = false;
    std::vector<CertifiedCarrierArc> arcs;

    bool operator==(const CertifiedCarrierNode&) const = default;
};

struct CertifiedEntry {
    StableKey carrier;
    double probability = 0.0;

    bool operator==(const CertifiedEntry&) const = default;
};

struct QuotientCellArc {
    StableKey label;
    std::optional<std::uint32_t> target_cell_id;
    std::optional<StableKey> external_frontier;
    double probability = 0.0;

    bool operator==(const QuotientCellArc&) const = default;
};

/*
 * Persistent cells retain replay ranges rather than exact member keys. The
 * keys are materialized only in a live CoverageReplaySlice when a later split
 * requires them.
 */
struct QuotientCell {
    std::uint32_t cell_id = 0;
    std::uint64_t generation = 0;
    StableKey semantic_identity;
    std::uint32_t coarse_state = 0;
    StableKey coarse_parent;
    ObservationRequirement observation_requirement;
    FeatureSignature observed_features;
    StableKey immediate_identity;
    bool terminal = false;
    CoverageDescriptor coverage;
    std::vector<QuotientCellArc> arcs;

    bool operator==(const QuotientCell&) const = default;
};

struct QuotientEntry {
    std::uint32_t cell_id = 0;
    double probability = 0.0;

    bool operator==(const QuotientEntry&) const = default;
};

struct QuotientPartitionState {
    std::uint64_t partition_generation = 0;
    std::uint32_t next_cell_id = 0;
    std::vector<QuotientCell> cells;
    std::vector<QuotientEntry> entries;

    const QuotientCell* find_cell(std::uint32_t cell_id) const;

    bool operator==(const QuotientPartitionState&) const = default;
};

enum class QuotientPartitionStatus : std::uint8_t {
    Complete = 0,
    EmptySlice,
    InvalidCoverage,
    InvalidCarrierGraph,
    OpenFrontier,
    NonMonotoneRefinement,
    SharedPartitionFailure,
    ResourceCap,
};

struct QuotientPartitionTelemetry {
    std::uint64_t exact_carriers_replayed = 0;
    std::uint64_t retained_coverage_ranges = 0;
    std::uint32_t initial_classes = 0;
    std::uint32_t final_classes = 0;
    std::uint32_t refinement_rounds = 0;
    std::uint32_t source_splits = 0;
    std::uint32_t target_splits = 0;
    std::uint64_t source_invalidations = 0;
    std::uint64_t reverse_invalidations = 0;
    std::uint64_t requirement_replacements = 0;
    std::uint64_t partition_estimated_bytes = 0;
    std::uint64_t partition_peak_estimated_bytes = 0;
    double exact_coverage_mass = 0.0;
    double exact_entry_mass = 0.0;
};

struct QuotientPartitionResult {
    QuotientPartitionStatus status = QuotientPartitionStatus::EmptySlice;
    refinement::ClosedPartitionStatus shared_partition_status =
        refinement::ClosedPartitionStatus::EmptyGraph;
    bool closed = false;
    bool lumpable = false;
    bool properness_checked = false;
    std::string failure_reason;
    QuotientPartitionTelemetry telemetry;
    QuotientPartitionState state;
    std::vector<refinement::RefinementCounterexample> counterexamples;
};

StableKey canonical_quotient_cell_identity(const QuotientCell& cell);

/*
 * `exact_coverage` must be the live replay of `coverage_descriptor` and must
 * cover every submitted node exactly once. `previous` is optional on first
 * construction. When supplied, refinement must be split-only; unchanged
 * cells retain stable IDs, while split children receive deterministic new
 * IDs. Proof-store invalidation is deterministic and optional for pure
 * structural tests.
 */
QuotientPartitionResult refine_certified_quotient_partition(
    const CoverageDescriptor& coverage_descriptor,
    const std::vector<CoverageCarrier>& exact_coverage,
    std::vector<CertifiedCarrierNode> nodes,
    std::vector<CertifiedEntry> entries,
    const QuotientPartitionState* previous,
    ProofStore* proof_store,
    refinement::ClosedPartitionLimits limits = {});

} // namespace quotient
} // namespace solver
} // namespace poecraft
