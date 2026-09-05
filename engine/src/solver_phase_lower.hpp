#pragma once

#include "solver_quotient_lower.hpp"
#include "solver_calc_types.hpp"

namespace poecraft::solver {

class SolveWork;

using PhaseLowerPrices = std::unordered_map<std::string, double>;

enum class PhaseTableRole { Acquisition, MaskCompletion, CleanCompletion };
struct PhaseLowerProposal {
    PhaseTableRole role = PhaseTableRole::Acquisition;
    std::uint32_t mask_count = 0, required = 0;
    std::vector<double> values;
};
struct PhaseProposalRefusal {
    std::string kind, detail;
    std::uint32_t cell = UINT32_MAX, action = UINT32_MAX, successor = UINT32_MAX;
    double lhs = 0, cost = 0, continuation = 0;
    std::vector<std::uint32_t> targets;
    std::vector<double> probabilities;
};
PhaseLowerProposal phase_completion_proposal(const std::vector<double>& acquisition,
                                             std::uint32_t required);

/* Directed operations are private proof arithmetic, never ordinary prices or
 * transition coefficients. Integer weights, including their full denominator,
 * enter here before any binary64 probability is constructed. */
struct PhaseProbabilityInterval { double lower = 0, upper = 1; };
PhaseProbabilityInterval phase_weight_probability(std::uint64_t part, std::uint64_t total);
double phase_two_exit_lower(double cost, PhaseProbabilityInterval probability,
                            double success, double failure);
double phase_price_lower(const ActionDescriptor&, const PhaseLowerPrices&);

struct PhasePrimitiveWitness {
    std::string action_id;
    std::uint32_t reachable_goals = 0;
    double price_lower = 0;
    bool priced = false;
};

/* A checked pointwise expectation relaxation. It deliberately grants every
 * reachable goal simultaneously and preserves all source goals. Consequently
 * hidden exclusions, protection, offers and correlated draws cannot improve
 * on it. The old probabilistic table is only a candidate, not its authority. */
class PreparedPhaseLowerView {
public:
    PreparedPhaseLowerView(const PreparedPhaseLowerView&) = delete;
    PreparedPhaseLowerView(PreparedPhaseLowerView&&) = delete;
    const quotient::StableKey identity;
    const std::vector<double> values;
    const std::vector<PhasePrimitiveWitness> primitives;
    const std::uint32_t family_mask;
    const std::uint8_t searing, eater;
    const bool original_candidate_accepted;
    const PhaseLowerProposal proposal;
    const PhaseProposalRefusal proposal_refusal;
    const std::uint64_t retained_reservation;
    bool compatible(const CalcContext&, const PhaseLowerPrices&, const pc_item_state&) const;
    std::optional<double> lookup(const CalcContext&, const PhaseLowerPrices&,
                                 const pc_item_state&) const;
    quotient::ProofMemorySnapshot memory_snapshot() const { return store_->ledger().snapshot(); }
private:
    friend class PhaseLowerProducer;
    friend class PreparedPhasePotential;
    friend class PreparedPhaseRestartLower;
    PreparedPhaseLowerView(quotient::StableKey identity, std::vector<double> values,
        std::vector<PhasePrimitiveWitness> primitives, std::uint32_t families,
        std::uint8_t searing, std::uint8_t eater, bool original,
        PhaseLowerProposal proposal, PhaseProposalRefusal refusal,
        std::shared_ptr<const SessionImpl> session,
        std::shared_ptr<quotient::ProofStore> store, std::uint64_t reservation);
    std::shared_ptr<const SessionImpl> session_;
    quotient::StableKey context_;
    std::shared_ptr<quotient::ProofStore> store_;
    quotient::ScopedProofMemoryCharge charge_;
};

/* Positive exact-fresh continuation evidence is issued only by the existing
 * preparation owner. A freely assembled QuotientLowerBoundary is not native
 * evidence. The public zero factory needs no additional semantic assumption. */
class PreparedPhaseRestartLower {
public:
    const quotient::QuotientLowerBoundary record;
    PreparedPhaseRestartLower(const PreparedPhaseRestartLower&) = delete;
    PreparedPhaseRestartLower(PreparedPhaseRestartLower&&) = delete;
private:
    friend class SolveWork;
    friend class PhaseLowerProducer;
    PreparedPhaseRestartLower(quotient::QuotientLowerBoundary, const PreparedPhaseLowerView&);
    std::shared_ptr<quotient::ProofStore> store_;
    quotient::ScopedProofMemoryCharge charge_;
};

struct PhaseProgramLowerRecord {
    quotient::StableKey source, post_phase, operator_identity, donor_identity;
    std::string operator_id;
    double cost_lower = 0, goal_probability_upper = 1;
    double failure_lower_min = 0, failure_lower_max = 0, lower = 0;
    std::uint64_t goal_weight = 0, total_weight = 0, physical_exits = 0;
    struct Exit { std::uint64_t weight; std::uint32_t mask, cell; double lower; bool goal; };
    std::vector<Exit> exits;
    double support_control_lower = 0;
};

class PhaseProgramLowerWitness {
public:
    PhaseProgramLowerWitness(const PhaseProgramLowerWitness&) = delete;
    PhaseProgramLowerWitness(PhaseProgramLowerWitness&&) = delete;
    const PhaseProgramLowerRecord record;
private:
    friend class PhaseLowerProducer;
    PhaseProgramLowerWitness(PhaseProgramLowerRecord record,
                              std::shared_ptr<quotient::ProofStore> store);
    std::shared_ptr<quotient::ProofStore> store_;
    quotient::ScopedProofMemoryCharge charge_;
};

struct PhasePotentialRelation {
    std::uint32_t cell = 0, action = 0;
    double cost = 0, rhs = 0;
    std::vector<std::uint32_t> targets;
    std::vector<double> probabilities;
    bool probability_aware = false, independent_price = false;
};

/* Existing clean-table indexing, with an exact fractured carrier frame.
 * This is a private checked potential, never an exact native transition row. */
class PreparedPhasePotential {
public:
    const quotient::StableKey identity;
    const std::vector<double> values;
    const PhaseLowerProposal proposal;
    const PhaseProposalRefusal proposal_refusal;
    const std::uint32_t fractured_mod, fractured_mask;
    const bool retained_scour;
    const double restart_boundary_lower;
    const std::vector<CalcContext::NativeGoalDrawBound> draws;
    const std::vector<PhasePotentialRelation> relations;
    const std::uint32_t model_rounds;
    const std::uint64_t retained_reservation;
    const std::uint64_t peak_additional_bytes, native_action_relations;
    bool compatible(const CalcContext&, const PhaseLowerPrices&, const pc_item_state&, bool consider_imprint) const;
    std::optional<double> lookup(const CalcContext&, const PhaseLowerPrices&, const pc_item_state&, bool consider_imprint) const;
    double projected_value(const CalcContext&, const pc_item_state&) const;
    quotient::ProofMemorySnapshot memory_snapshot() const;
private:
    friend class PhaseLowerProducer;
    PreparedPhasePotential(std::shared_ptr<const PreparedPhaseLowerView>,
        std::vector<double>, PhaseLowerProposal, PhaseProposalRefusal,
        std::uint32_t mod, std::uint32_t mask, bool retained, double restart_lower,
        std::vector<CalcContext::NativeGoalDrawBound>, std::vector<PhasePotentialRelation>,
        std::uint32_t rounds, std::uint64_t reservation, std::uint64_t peak, std::uint64_t action_relations);
    std::shared_ptr<const PreparedPhaseLowerView> support_;
    quotient::ScopedProofMemoryCharge charge_;
};

class PhaseLowerProducer {
public:
    static std::shared_ptr<const PreparedPhaseLowerView> prepare(
        CalcContext&, const PhaseLowerPrices&, const pc_item_state& phase,
        const PhaseLowerProposal& existing_candidate,
        const quotient::QuotientLowerBudget& budget = {});
    static PhaseProgramLowerWitness compose(CalcContext&, const PhaseLowerPrices&,
        const pc_item_state& source, const std::string& operator_id,
        const PreparedPhaseLowerView&, const quotient::QuotientLowerBudget& budget = {});
    static std::shared_ptr<const PreparedPhasePotential> prepare_probabilistic(
        CalcContext&, const PhaseLowerPrices&, const pc_item_state&,
        const PhaseLowerProposal&, std::shared_ptr<const PreparedPhaseLowerView>,
        const PreparedPhaseRestartLower& restart_boundary,
        bool consider_imprint_programs, bool retain_scour,
        const quotient::QuotientLowerBudget& budget = {});
    static PreparedPhaseRestartLower zero_restart_boundary(const PreparedPhaseLowerView&);
    static PhaseProgramLowerWitness compose(CalcContext&, const PhaseLowerPrices&,
        const pc_item_state&, const std::string&, const PreparedPhasePotential&,
        const quotient::QuotientLowerBudget& budget = {});
private:
    static PhaseProgramLowerWitness compose_impl(CalcContext&, const PhaseLowerPrices&,
        const pc_item_state&, const std::string&, const PreparedPhaseLowerView&,
        const PreparedPhasePotential*, const quotient::QuotientLowerBudget&);
};

} // namespace poecraft::solver
