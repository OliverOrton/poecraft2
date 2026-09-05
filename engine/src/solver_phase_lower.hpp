#pragma once

#include "solver_quotient_lower.hpp"
#include "solver_calc_types.hpp"

namespace poecraft::solver {

using PhaseLowerPrices = std::unordered_map<std::string, double>;

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
    const std::uint64_t retained_reservation;
    bool compatible(const CalcContext&, const PhaseLowerPrices&, const pc_item_state&) const;
    std::optional<double> lookup(const CalcContext&, const PhaseLowerPrices&,
                                 const pc_item_state&) const;
    quotient::ProofMemorySnapshot memory_snapshot() const { return store_->ledger().snapshot(); }
private:
    friend class PhaseLowerProducer;
    PreparedPhaseLowerView(quotient::StableKey identity, std::vector<double> values,
        std::vector<PhasePrimitiveWitness> primitives, std::uint32_t families,
        std::uint8_t searing, std::uint8_t eater, bool original,
        std::shared_ptr<const SessionImpl> session,
        std::shared_ptr<quotient::ProofStore> store, std::uint64_t reservation);
    std::shared_ptr<const SessionImpl> session_;
    quotient::StableKey context_;
    std::shared_ptr<quotient::ProofStore> store_;
    quotient::ScopedProofMemoryCharge charge_;
};

struct PhaseProgramLowerRecord {
    quotient::StableKey source, post_phase, operator_identity, donor_identity;
    std::string operator_id;
    double cost_lower = 0, goal_probability_upper = 1;
    double failure_lower_min = 0, failure_lower_max = 0, lower = 0;
    std::uint64_t goal_weight = 0, total_weight = 0, physical_exits = 0;
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

class PhaseLowerProducer {
public:
    static std::shared_ptr<const PreparedPhaseLowerView> prepare(
        CalcContext&, const PhaseLowerPrices&, const pc_item_state& phase,
        const std::vector<double>& existing_candidate,
        const quotient::QuotientLowerBudget& budget = {});
    static PhaseProgramLowerWitness compose(CalcContext&, const PhaseLowerPrices&,
        const pc_item_state& source, const std::string& operator_id,
        const PreparedPhaseLowerView&, const quotient::QuotientLowerBudget& budget = {});
};

} // namespace poecraft::solver
