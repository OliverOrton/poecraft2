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

// Arithmetic only: callers must prove distinct goals and conditional-history
// authority. Each row is one goal's upper at the three possible draw positions.
double phase_joint_assignment_upper(const std::vector<std::array<double, 3>>& conditional,
                                    unsigned positions);
bool phase_price_shortcut_limiting(double cost, double value);
std::vector<std::pair<unsigned, std::uint32_t>> phase_minimum_event_allocation(
    const std::vector<double>& values, const std::vector<std::uint32_t>& capacities);

enum class PhaseRelationReason {
    ProbabilityEnvelope, NativeEffect, CandidatePriceShortcut,
    NativeDomainEscape, UnsupportedEffect, FixedIndependentBoundary
};
const char* phase_relation_reason(PhaseRelationReason);
struct PhaseJointEventWitness {
    std::uint32_t action = 0, subset = 0, retained = 0, forced = 0;
    unsigned side = 0, positions = 0, initial_same_side = 0, other_side_blockers = 0;
    bool uniform_history = false;
    std::vector<std::uint32_t> natural_draws, guaranteed_draws;
    std::vector<std::array<double, 3>> conditional;
    double marginal_upper = 1, joint_upper = 1;
    std::uint32_t capacity = 0;
};
struct PhasePriceReactivation { std::uint32_t cell = 0, action = 0, round = 0; double cost = 0, value = 0, minimum_rhs = 0; };

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
    double prior_potential_lower = 0;
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

enum class PhaseContinuation : std::uint8_t { PriceOnly, SideRetention, CoupledFresh };
enum class PhaseRetention : std::uint8_t { None, Crafted, Annul, CraftedNonempty, AnnulNonempty };
struct PhasePreparationOptions {
    bool minimum_reforge_occupancy = true;
    bool reuse_renewal_support = true;
    unsigned filter_modes = 0; // bit 0 Cannot Roll Attack, bit 1 Cannot Roll Caster
    bool checked_numerical_reuse = false; // internal experiment; cold remains the default
    bool untrusted_numerical_reuse = false;
    bool accept_checked_subsolution = false; // diagnostic endpoint; no auxiliary-optimum claim
    double minimum_checked_source_lower = 0; // stopping target, never a Bellman constraint
};
struct PhasePreparationStats {
    bool accepted_early_subsolution = false;
    // Bounded diagnostic history; values are marked only after complete
    // current-vector checking and with no pending price reactivation.
    std::array<double, 64> checked_source_lowers{};
    std::array<std::uint64_t, 64> checked_source_ns{};
    std::uint64_t total_ns = 0, projection_ns = 0, relation_ns = 0;
    std::uint64_t diagnostic_export_ns = 0; // included in relation_ns; no numerical graph alive
    // Support and allocation are subsets of relation_ns; native weights are
    // a subset of support_ns. These diagnostic times grant no authority.
    std::uint64_t support_ns = 0, allocation_ns = 0, native_weight_ns = 0;
    std::uint64_t check_ns = 0, solve_ns = 0, numerical_sweeps = 0;
    std::uint64_t candidate_pass_rounds = 0, candidate_fail_rounds = 0;
    std::uint64_t eligible_solve_rounds = 0, refused_solve_rounds = 0;
    std::uint64_t eligible_solve_ns = 0, refused_solve_ns = 0;
    std::uint64_t eligible_sweeps = 0, refused_sweeps = 0;
    std::uint64_t eligible_work = 0, refused_work = 0;
    std::uint64_t seeded_rounds = 0, seed_refusals = 0, cold_solve_rounds = 0;
    std::uint64_t untrusted_rounds = 0, zero_fallbacks = 0;
    const char* first_seed_refusal = "none";
    std::uint64_t coverage_ns = 0, quotient_rows_ns = 0;
    std::uint64_t geometry_hits = 0, geometry_templates = 0, geometry_bytes = 0;
    std::uint64_t event_cap_hits = 0, event_cap_bytes = 0;
};
// The callback proves a draw at EVERY native history in this occupancy cell.
// An unknown cell stops the guarantee; reaching the native target is separate
// from application. No conditional probability multiplication is involved.
unsigned phase_refill_minimum(unsigned prefixes, unsigned suffixes, unsigned target,
    const std::function<bool(unsigned, unsigned)>& uniformly_nonempty);
struct PhaseRefillWitness {
    unsigned action = 0, prefixes = 0, suffixes = 0, target = 0, minimum = 0;
};
struct PhaseNonemptyWitness {
    unsigned action = 0, prefixes = 0, suffixes = 0, draw = 0;
    int phase = -1;
    std::uint64_t remaining_other = 0;
};

struct PhasePotentialRelation {
    std::uint32_t cell = 0, action = 0;
    double cost = 0, rhs = 0;
    std::vector<std::uint32_t> targets;
    std::vector<double> probabilities;
    bool probability_aware = false, independent_price = false;
    PhaseRelationReason reason = PhaseRelationReason::NativeEffect;
    struct Event { std::uint32_t mask = 0, minimum_cell = 0, capacity = 0; };
    std::vector<Event> events;
    unsigned removable_affixes = 0; // uniform integer native Annul denominator; zero for other relations
    int phase_branch = -2; // -1 no dominance, 0 prefix, 1 suffix; -2 not split
};

/* Existing clean-table indexing, with an exact fractured carrier frame and
 * an optional separately indexed unfractured continuation region.
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
    const std::shared_ptr<const PreparedPhasePotential> reused_draw_owner;
    const std::vector<PhaseJointEventWitness> joint_events;
    const std::vector<PhasePriceReactivation> reactivations;
    const bool joint_refinement;
    const PhaseContinuation continuation;
    const PhaseRetention retention;
    const PhasePreparationOptions preparation_options;
    const PhasePreparationStats preparation_stats;
    // Sparse refined coordinates: low 32 bits are the legacy region cell;
    // high bits retain removable crafted goals, crafted junk counts and a
    // selected filter kind (one of the actual crafted-junk occurrences).
    // UINT64_MAX is an infeasible legacy coordinate, never a native member.
    const std::vector<std::uint64_t> coordinates;
    const std::uint32_t crafted_goal_domain;
    const unsigned crafted_count_limit;
    const std::array<std::uint32_t, 2> filter_mods;
    const std::vector<PhaseNonemptyWitness> nonempty_witnesses;
    const std::vector<PhaseRefillWitness> refill_witnesses;
    const std::vector<PhasePotentialRelation> relations;
    const std::uint32_t model_rounds;
    const std::uint64_t retained_reservation;
    const std::uint64_t peak_additional_bytes, native_action_relations;
    bool compatible(const CalcContext&, const PhaseLowerPrices&, const pc_item_state&, bool consider_imprint) const;
    std::optional<double> lookup(const CalcContext&, const PhaseLowerPrices&, const pc_item_state&, bool consider_imprint) const;
    std::optional<quotient::QuotientLowerBoundary> whole_scope_source_lower(
        const CalcContext&, const PhaseLowerPrices&, const pc_item_state&, bool consider_imprint) const;
    std::uint32_t projected_cell(const CalcContext&, const pc_item_state&) const;
    double projected_value(const CalcContext&, const pc_item_state&) const;
    quotient::ProofMemorySnapshot memory_snapshot() const;
    std::size_t draw_count() const;
    const CalcContext::NativeGoalDrawBound& draw(std::size_t) const;
private:
    friend class PhaseLowerProducer;
    friend class SolveWork;
    PreparedPhasePotential(std::shared_ptr<const PreparedPhaseLowerView>,
        std::vector<double>, PhaseLowerProposal, PhaseProposalRefusal,
        std::uint32_t mod, std::uint32_t mask, bool retained, double restart_lower,
        std::vector<CalcContext::NativeGoalDrawBound>, std::vector<PhasePotentialRelation>,
        std::uint32_t rounds, std::uint64_t reservation, std::uint64_t peak, std::uint64_t action_relations,
        std::shared_ptr<const PreparedPhasePotential>, std::vector<PhaseJointEventWitness>,
        std::vector<PhasePriceReactivation>, bool joint, PhaseContinuation,
        PhaseRetention, std::vector<std::uint64_t>, std::uint32_t crafted_goal_domain, unsigned crafted_count_limit,
        std::array<std::uint32_t, 2> filter_mods,
        std::vector<PhaseNonemptyWitness>, std::vector<PhaseRefillWitness>, PhasePreparationOptions, PhasePreparationStats);
    std::unordered_map<std::uint64_t, std::uint32_t> coordinate_index_;
    // Geometry only. SolveWork first binds this view to its immutable request
    // and proves the AbstractState fields determine every represented member.
    std::optional<double> projected_summary_value(unsigned rarity, unsigned mask,
        unsigned p, unsigned s, bool fresh, unsigned crafted, unsigned jp, unsigned js) const;
    std::shared_ptr<const PreparedPhaseLowerView> support_;
    quotient::ScopedProofMemoryCharge charge_;
};

class PhaseLowerProducer {
public:
    // Selects a quantified certificate region, never a replacement start item
    // or a policy requirement to acquire this fracture. The coupled fresh
    // region covers all supported unfractured members; other fractures keep
    // the existing independently bounded first-exit relation.
    struct CoupledFractureFrame { std::uint32_t mod; };
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
        const quotient::QuotientLowerBudget& budget = {}, bool joint_refinement = false,
        std::shared_ptr<const PreparedPhasePotential> reuse_draws = {},
        PhaseContinuation continuation = PhaseContinuation::PriceOnly,
        PhaseRetention retention = PhaseRetention::None, bool retain_diagnostics = true,
        PhasePreparationOptions preparation_options = {},
        std::optional<CoupledFractureFrame> frame = {});
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
