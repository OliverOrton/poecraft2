#pragma once

#include "solver_action_coverage.hpp"
#include "solver_quotient_proof.hpp"

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace poecraft::solver::quotient {

enum class QuotientBellmanMode { Executable, LowerOnly };
enum class LowerCoefficientModel {
    RawStoredCoefficients,
    NormalizedStoredReference,
    ExactBinaryModel,
};

/* These are declared input contracts, not certificates minted by a solver.
 * The private native adapter supplies ExactDeclaredKernel only for complete
 * native queries under its admitted action scope. Raw native coefficients
 * still have coefficient-model-only assurance after arithmetic acceptance. */
enum class LowerEvidenceKind {
    Unproved, ExactDeclaredKernel, IndependentLower, ExactInapplicability,
};

struct QuotientLowerRowProvenance {
    StableKey request_identity;
    StableKey source_identity;
    StableKey action_identity;
    StableKey evidence_identity;
    LowerEvidenceKind kind = LowerEvidenceKind::Unproved;
};

struct QuotientBellmanChoiceInput {
    double probability = 0.0;
    bool has_self = false;
    std::vector<std::uint32_t> target_cell_ids;
};

enum class LowerConstraintKind { Row, Scalar, Inapplicable };
struct QuotientLowerConstraint {
    CanonicalActionCover cover;
    LowerConstraintKind kind = LowerConstraintKind::Scalar;
    std::uint64_t row = std::numeric_limits<std::uint64_t>::max();
    double lower = 0.0;
    StableKey evidence_identity;
    LowerEvidenceKind evidence = LowerEvidenceKind::Unproved;
};

struct QuotientLowerSource {
    std::uint32_t cell_id = 0;
    StableKey source_identity;
    CanonicalActionSet expected_actions;
    std::vector<QuotientLowerConstraint> constraints;
};

struct QuotientLowerBoundary {
    std::uint32_t cell_id = 0;
    StableKey source_identity;
    StableKey evidence_identity;
    double lower = 0.0;
    LowerEvidenceKind evidence = LowerEvidenceKind::Unproved;
};

struct QuotientLowerQuery {
    /* Bind goal, economy, artifact, physical/action/program scope and numeric
     * version in the full key. No compiled strategy or route is required. */
    StableKey request_identity;
    StableKey caller_scope;
    std::uint64_t model_revision = 0;
    LowerCoefficientModel coefficients =
        LowerCoefficientModel::RawStoredCoefficients;
    std::vector<std::uint32_t> roots;
    std::vector<QuotientLowerSource> sources;
    std::vector<QuotientLowerBoundary> boundaries;
};

struct QuotientLowerBudget {
    std::uint32_t max_sweeps = 10000;
    std::uint64_t max_scratch_bytes = 16ull * 1024 * 1024;
    std::function<bool()> cancelled;
};

enum class QuotientLowerStatus {
    CoverageIncomplete, NumericInconclusive, CheckedFiniteLower,
    StaleModel, Cancelled, ResourceCap,
};

struct QuotientLowerLimitingConstraint {
    std::uint32_t cell_id = 0;
    CanonicalActionCover cover;
    LowerConstraintKind kind = LowerConstraintKind::Scalar;
    double rhs_lower = 0.0;
};

/* Immutable, separately owned evidence. It keeps its accounting owner alive;
 * releasing the last reader releases the existing ProofStore charge. There
 * is deliberately no conversion to QuotientBellmanResult or public lower. */
class QuotientLowerCertificate {
public:
    const StableKey request_identity;
    const StableKey model_identity;
    const std::uint64_t model_revision;
    const std::uint64_t price_generation;
    const LowerCoefficientModel coefficients;
    const std::vector<double> values_by_state;
private:
    friend class QuotientBellmanGraph;
    QuotientLowerCertificate(
        std::shared_ptr<ProofStore> owner, StableKey identity, StableKey model_key,
        std::uint64_t revision, LowerCoefficientModel model,
        std::vector<double> values);
    std::shared_ptr<ProofStore> owner_;
    ScopedProofMemoryCharge charge_;
};

struct QuotientLowerResult {
    QuotientLowerStatus status = QuotientLowerStatus::CoverageIncomplete;
    std::string reason;
    std::vector<double> candidate_values_by_state;
    std::shared_ptr<const QuotientLowerCertificate> checked;
    std::vector<QuotientLowerLimitingConstraint> ranked_constraints;
    std::uint32_t sweeps = 0;
    bool candidate_stable = false;
    /* No claim of greatest proper-policy solution, even at a zero-loop fixed
     * point. Candidate residuals are diagnostic, never acceptance tolerances. */
    std::uint64_t scratch_bytes = 0;
    /* Diagnostic result storage, distinct from the immutable certificate. */
    std::shared_ptr<void> storage_charge;
};

StableKey quotient_lower_model_identity(const QuotientLowerQuery& query);

const char* quotient_lower_status_name(QuotientLowerStatus status);

} // namespace poecraft::solver::quotient
