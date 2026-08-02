#pragma once

#include "solver_quotient_partition.hpp"
#include "solver_sparse_policy.hpp"

#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace poecraft {
namespace solver {
namespace quotient {

struct QuotientBellmanCellInput {
    std::uint32_t cell_id = 0;
    std::uint64_t generation = 0;
    StableKey semantic_identity;
    bool terminal = false;

    bool operator==(const QuotientBellmanCellInput&) const = default;
};

struct QuotientBellmanTransitionInput {
    StableKey label;
    std::uint32_t target_cell_id = 0;
    double probability = 0.0;

    bool operator==(const QuotientBellmanTransitionInput&) const = default;
};

/*
 * Rows are append-only. `certified == false` is deliberately useful: the row
 * remains an admitted lower-relaxation alternative but can never support an
 * executable upper policy. A certified row must carry the full immutable
 * proof identity installed beside its stable sparse-row id.
 */
struct QuotientBellmanRowInput {
    std::uint32_t source_cell_id = 0;
    std::uint32_t operator_index = kNoId;
    double cost = kInfinity;
    bool admitted = true;
    bool certified = false;
    std::optional<CertifiedRowIdentity> proof_identity;
    std::vector<QuotientBellmanTransitionInput> transitions;
};

enum class QuotientBellmanStatus : std::uint8_t {
    Complete = 0,
    EmptyGraph,
    InvalidCell,
    InvalidRow,
    MissingReachableCertificate,
    ImproperPolicy,
    DidNotConverge,
    ResourceCap,
};

struct QuotientBellmanTelemetry {
    std::uint64_t proof_payload_reuses = 0;
    std::uint64_t row_reprojections = 0;
    std::uint64_t source_splits = 0;
    std::uint64_t target_splits = 0;
    std::uint64_t reverse_invalidations = 0;
    std::uint64_t exact_carriers_replayed = 0;
    std::uint64_t bellman_rows_evaluated = 0;
    std::uint64_t policy_improvements = 0;
    std::uint64_t improper_policy_repairs = 0;
    std::uint64_t scc_evaluations = 0;
    std::uint64_t reference_adapter_invocations = 0;
    ProofMemorySnapshot memory;
};

struct QuotientPublicationAudit {
    bool reachable_selected_rows_current = false;
    bool closed_target_dependencies = false;
    bool terminal_reachable_bottom_sccs = false;
    bool proper_every_entry = false;

    bool complete() const {
        return reachable_selected_rows_current &&
               closed_target_dependencies &&
               terminal_reachable_bottom_sccs &&
               proper_every_entry;
    }
};

struct QuotientBellmanResult {
    QuotientBellmanStatus status = QuotientBellmanStatus::EmptyGraph;
    bool executable_upper = false;
    bool proper = false;
    std::string failure_reason;
    std::vector<std::uint32_t> reachable_cell_ids;
    std::vector<double> values_by_state;
    std::vector<std::uint64_t> selected_rows_by_state;
    std::vector<double> lower_relaxation_by_state;
    QuotientPublicationAudit publication_audit;
};

/*
 * Stable-row Bellman projection for proof-carrying quotient cells. It owns a
 * SolveTransitionCache so state-to-row spans, sparse row ids, and the proof
 * dependency sidecar have the same lifetime and append-only identity.
 */
class QuotientBellmanGraph {
public:
    explicit QuotientBellmanGraph(
        std::uint64_t max_owned_bytes =
            std::numeric_limits<std::uint64_t>::max());

    void install_cells(std::vector<QuotientBellmanCellInput> cells);
    void supersede_cell(QuotientBellmanCellInput cell);
    std::uint64_t append_row(QuotientBellmanRowInput row);

    std::uint64_t invalidate_source_split(
        std::uint32_t source_cell_id,
        std::uint64_t replacement_generation);
    std::uint64_t invalidate_target_split(
        std::uint32_t target_cell_id,
        std::uint64_t replacement_generation);
    void note_price_change(const std::map<std::uint64_t, double>& row_costs);
    void set_external_row_kernel_bytes(std::uint64_t bytes) {
        external_row_kernel_bytes_ = bytes;
        refresh_row_kernel_bytes();
    }

    QuotientBellmanResult solve(
        const std::vector<std::uint32_t>& entry_cell_ids,
        std::uint32_t max_policy_iterations = 1000,
        std::uint32_t max_component_iterations = 100000,
        double improvement_epsilon = 1e-12);

    const SolveTransitionCache& transition_cache() const {
        return transition_cache_;
    }
    const std::vector<solve_detail::PricedSparseRow>& priced_rows() const {
        return priced_rows_;
    }
    const QuotientBellmanTelemetry& telemetry() const {
        return telemetry_;
    }
    std::optional<std::uint32_t> state_index_for_cell(
            std::uint32_t cell_id) const {
        return state_for_cell(cell_id);
    }
    std::optional<std::uint32_t> cell_id_for_state(
            std::uint32_t state) const {
        return state < cell_by_state_.size()
                   ? std::optional<std::uint32_t>{cell_by_state_[state]}
                   : std::nullopt;
    }
    std::shared_ptr<ProofStore> proof_store() const {
        return transition_cache_.quotient_proofs;
    }

private:
    struct CellRecord {
        QuotientBellmanCellInput cell;
        std::uint32_t state = kNoId;
        std::uint64_t source_generation = 0;
        std::uint64_t target_generation = 0;
    };

    SolveTransitionCache transition_cache_;
    std::vector<PricedSparseRow> priced_rows_;
    std::map<std::uint32_t, CellRecord> cells_;
    std::vector<std::uint32_t> cell_by_state_;
    std::vector<std::vector<std::uint32_t>> reverse_predecessors_;
    std::uint64_t action_generation_ = 1;
    std::uint64_t admission_generation_ = 1;
    std::uint64_t external_row_kernel_bytes_ = 0;
    QuotientBellmanTelemetry telemetry_;

    std::optional<std::uint32_t> state_for_cell(std::uint32_t cell_id) const;
    bool row_certificate_current(std::uint64_t row) const;
    void refresh_row_kernel_bytes();
};

const char* quotient_bellman_status_name(QuotientBellmanStatus status);

} // namespace quotient
} // namespace solver
} // namespace poecraft
