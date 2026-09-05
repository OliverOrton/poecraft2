#include "solver_phase_lower.hpp"
#include "solver_quotient_bellman.hpp"
#include "poecraft/bitset.h"

#include <bit>
#include <cmath>
#include <numeric>

namespace poecraft::solver {
using namespace quotient;
namespace {
constexpr std::uint64_t version = 0x50524f424c4f0003ull;
constexpr std::uint32_t mass = 1u << 24;
double down(double x) { return x == 0 ? 0 : std::max(0.0, std::nextafter(x, 0.0)); }
void checkpoint(const QuotientLowerBudget& b) {
    if (b.cancelled && b.cancelled()) throw std::runtime_error("phase probability cancelled");
}
std::uint32_t mod_mask(const CalcContext& calc, std::uint32_t mod) {
    std::uint32_t result = 0;
    if (mod >= calc.session().mod_count) throw std::invalid_argument("foreign phase modifier");
    for (std::uint32_t slot = 0; slot < calc.layout().slots.size(); ++slot)
        if (pc_bitset_test(calc.layout().slots[slot].satisfying_mask.data(), mod)) result |= 1u << slot;
    return result;
}
std::uint32_t item_mask(const CalcContext& calc, const pc_item_state& item) {
    std::uint32_t result = 0;
    for (unsigned i = 0; i < item.prefix_count; ++i) result |= mod_mask(calc, item.prefixes[i].mod_id);
    for (unsigned i = 0; i < item.suffix_count; ++i) result |= mod_mask(calc, item.suffixes[i].mod_id);
    return result;
}
bool in_frame(const CalcContext& calc, const pc_item_state& item, std::uint32_t fracture) {
    // The explicit pool reads generic influence and cannot-roll metamods;
    // Eldritch tiers select a side, never its weights. Both side choices are
    // retained below. Other cosmetic item coordinates are not pool inputs.
    if (item.generic_influence_bits || item.item_flags || item.rarity > PC_RARITY_RARE ||
        item.prefix_count > 3 || item.suffix_count > 3) return false;
    unsigned fractures = 0;
    const auto side = [&](const pc_mod_slot* slots, unsigned count) {
        for (unsigned i = 0; i < count; ++i) {
            const auto& slot = slots[i];
            if (slot.mod_id >= calc.session().mod_count ||
                (slot.flags & PC_MOD_SLOT_VEILED) ||
                calc.session().metamod_type.at(slot.mod_id) >= 0) return false;
            if (slot.flags & PC_MOD_SLOT_FRACTURED) {
                if (slot.mod_id != fracture) return false;
                ++fractures;
            }
        }
        return true;
    };
    return side(item.prefixes, item.prefix_count) && side(item.suffixes, item.suffix_count) && fractures == (fracture == kNoId ? 0u : 1u);
}
std::size_t index(std::uint32_t masks, unsigned rarity, unsigned mask, unsigned p, unsigned s) {
    return ((rarity*masks+mask)*4+p)*4+s;
}
StableKey potential_identity(const PreparedPhaseLowerView& support, const std::vector<double>& values,
        std::uint32_t mod, bool retained, double restart_lower, bool joint, PhaseContinuation continuation) {
    auto key = support.identity;
    key.insert(key.end(), {version, mod, retained, joint, 0 /* unchanged Imprint exclusion */});
    key.push_back(static_cast<unsigned>(continuation));
    key.push_back(std::bit_cast<std::uint64_t>(restart_lower));
    for (double x : values) key.push_back(std::bit_cast<std::uint64_t>(x));
    return key;
}
struct Cell { unsigned rarity, mask, p, s; std::uint32_t id; bool goal; };
struct Group { unsigned mask; std::uint32_t capacity = mass; std::vector<std::uint32_t> cells; };

// Every conditional history has at most p/s blockers. No target mass is
// deleted on behalf of hypothetical satisfying members. A side-conditioned
// upper also bounds an ordinary draw which can choose the other side.
double draw_upper(const CalcContext::NativeGoalDrawBound& w, unsigned p, unsigned s) {
    auto other = w.other_weight;
    for (unsigned side = 0; side < 2; ++side)
        for (unsigned i = 0; i < (side ? s : p); ++i)
            other -= std::min(other, w.strongest_other_removal[side].at(i));
    if (!w.target_weight) return 0;
    if (w.target_weight > UINT64_MAX-other) throw std::overflow_error("phase denominator overflow");
    return phase_weight_probability(w.target_weight, w.target_weight+other).upper;
}
std::uint32_t capacity(double upper) {
    if (!(upper >= 0 && upper <= 1)) throw std::invalid_argument("unproved phase event probability");
    return static_cast<std::uint32_t>(std::min<double>(mass, std::ceil(std::ldexp(upper, 24))));
}
} // namespace

double phase_joint_assignment_upper(const std::vector<std::array<double, 3>>& conditional,
        unsigned positions) {
    if (positions > 3 || conditional.size() > 3)
        throw std::invalid_argument("joint event exceeds native side capacity");
    for (const auto& goal : conditional) for (unsigned j = 0; j < positions; ++j)
        if (!(goal[j] >= 0 && goal[j] <= 1)) throw std::invalid_argument("invalid conditional upper");
    if (conditional.size() > positions) return 0; // requires independently proved distinct draws
    const auto up = [](double x) { return x == 0 ? 0 : std::nextafter(x, std::numeric_limits<double>::infinity()); };
    double total = 0;
    const auto visit = [&](auto&& self, unsigned goal, unsigned occupied, double product) -> void {
        if (goal == conditional.size()) { total = up(total+product); return; }
        for (unsigned j = 0; j < positions; ++j) if (!(occupied & (1u << j))) {
            const double factor = conditional[goal][j];
            // Preserve positive mass even if a generic arithmetic fixture
            // underflows. Native uint64-ratio products (at most three factors)
            // are far above this range, but zero must still mean exact zero.
            const double next = product == 0 || factor == 0 ? 0 :
                std::nextafter(product*factor, std::numeric_limits<double>::infinity());
            self(self, goal+1, occupied | (1u << j), next);
        }
    };
    visit(visit, 0, 0, 1);
    return std::min(1.0, total);
}
bool phase_price_shortcut_limiting(double cost, double value) {
    // This tolerance only schedules extra proof work; it grants no numerical
    // authority. Include exact equality and the quotient's downward repair.
    return cost <= value + 64*std::numeric_limits<double>::epsilon()*std::max(1.0, cost);
}
std::vector<std::pair<unsigned, std::uint32_t>> phase_minimum_event_allocation(
        const std::vector<double>& values, const std::vector<std::uint32_t>& capacities) {
    if (values.size() != capacities.size()) throw std::invalid_argument("event dimensions differ");
    std::vector<unsigned> order(values.size()); std::iota(order.begin(), order.end(), 0);
    for (unsigned i : order) if (!std::isfinite(values[i]) || values[i] < 0 || capacities[i] > mass)
        throw std::invalid_argument("invalid frozen event");
    std::stable_sort(order.begin(), order.end(), [&](auto a, auto b) { return values[a] < values[b]; });
    std::vector<std::pair<unsigned, std::uint32_t>> result;
    unsigned remaining = mass;
    for (auto i : order) {
        const auto take = std::min(remaining, capacities[i]);
        if (take) result.emplace_back(i, take);
        remaining -= take;
        if (!remaining) break;
    }
    if (remaining) throw std::invalid_argument("native mask events do not cover complete probability mass");
    // Exchange argument: moving mass from a dearer occupied event to a cheaper
    // unsaturated event never increases expectation. Independent caps only.
    return result;
}
const char* phase_relation_reason(PhaseRelationReason reason) {
    switch (reason) {
    case PhaseRelationReason::ProbabilityEnvelope: return "probability_envelope";
    case PhaseRelationReason::NativeEffect: return "native_effect";
    case PhaseRelationReason::CandidatePriceShortcut: return "candidate_price_shortcut";
    case PhaseRelationReason::NativeDomainEscape: return "native_domain_escape";
    case PhaseRelationReason::UnsupportedEffect: return "unsupported_effect";
    case PhaseRelationReason::FixedIndependentBoundary: return "fixed_independent_boundary";
    }
    throw std::invalid_argument("unknown phase relation reason");
}

PreparedPhaseRestartLower::PreparedPhaseRestartLower(QuotientLowerBoundary value, const PreparedPhaseLowerView& support)
    : record(std::move(value)), store_(support.store_),
      charge_(store_->ledger(), ProofMemoryCategory::Certificate, sizeof(*this) + 256 +
        8*(record.source_identity.capacity()+record.evidence_identity.capacity())) {}
PreparedPhaseRestartLower PhaseLowerProducer::zero_restart_boundary(const PreparedPhaseLowerView& support) {
    pc_item_state fresh{}; pc_item_clear(&fresh);
    auto evidence = support.identity;
    evidence.insert(evidence.end(), {0x46524553484c4f57ull, 0});
    return PreparedPhaseRestartLower({0, exact_item_state_key(fresh), std::move(evidence), 0,
        LowerEvidenceKind::IndependentLower}, support);
}

PreparedPhasePotential::PreparedPhasePotential(std::shared_ptr<const PreparedPhaseLowerView> support,
        std::vector<double> table, PhaseLowerProposal proposed, PhaseProposalRefusal refusal,
        std::uint32_t mod, std::uint32_t mask, bool retained, double restart_lower,
        std::vector<CalcContext::NativeGoalDrawBound> weights, std::vector<PhasePotentialRelation> rows,
        std::uint32_t rounds, std::uint64_t reservation, std::uint64_t peak, std::uint64_t action_relations,
        std::shared_ptr<const PreparedPhasePotential> reused, std::vector<PhaseJointEventWitness> events,
        std::vector<PhasePriceReactivation> reactivated, bool joint, PhaseContinuation mode)
    : identity(potential_identity(*support, table, mod, retained, restart_lower, joint, mode)), values(std::move(table)),
      proposal(std::move(proposed)), proposal_refusal(std::move(refusal)), fractured_mod(mod),
      fractured_mask(mask), retained_scour(retained), restart_boundary_lower(restart_lower), draws(std::move(weights)),
      reused_draw_owner(std::move(reused)), joint_events(std::move(events)), reactivations(std::move(reactivated)),
      joint_refinement(joint), continuation(mode), relations(std::move(rows)),
      model_rounds(rounds), retained_reservation(reservation), peak_additional_bytes(peak),
      native_action_relations(action_relations), support_(std::move(support)),
      charge_(support_->store_->ledger(), ProofMemoryCategory::Certificate, reservation) {}
bool PreparedPhasePotential::compatible(const CalcContext& calc, const PhaseLowerPrices& prices,
        const pc_item_state& item, bool consider_imprint) const {
    if (consider_imprint) return false;
    auto context_item = item;
    context_item.searing_exarch_tier = support_->searing;
    context_item.eater_of_worlds_tier = support_->eater;
    // The new relation explicitly covers every Eldritch side/phase. The old
    // support view and anchored production guard retain exact-phase equality.
    return (in_frame(calc, item, fractured_mod) ||
        (continuation == PhaseContinuation::CoupledFresh && in_frame(calc, item, kNoId))) && support_->compatible(calc, prices, context_item);
}
std::uint32_t PreparedPhasePotential::projected_cell(const CalcContext& calc, const pc_item_state& item) const {
    const bool framed = in_frame(calc, item, fractured_mod);
    if (!framed && (continuation != PhaseContinuation::CoupledFresh || !in_frame(calc, item, kNoId)))
        throw std::invalid_argument("program exit outside certified probability regions");
    const auto offset = framed ? 0 : 3*support_->values.size()*16+2;
    return static_cast<std::uint32_t>(offset+index(support_->values.size(), item.rarity, item_mask(calc, item),
        item.prefix_count, item.suffix_count));
}
double PreparedPhasePotential::projected_value(const CalcContext& calc, const pc_item_state& item) const {
    return values.at(projected_cell(calc, item));
}
std::optional<double> PreparedPhasePotential::lookup(const CalcContext& calc, const PhaseLowerPrices& prices,
        const pc_item_state& item, bool consider_imprint) const {
    if (!compatible(calc, prices, item, consider_imprint)) return std::nullopt;
    return projected_value(calc, item);
}
std::optional<QuotientLowerBoundary> PreparedPhasePotential::whole_scope_source_lower(
        const CalcContext& calc, const PhaseLowerPrices& prices, const pc_item_state& item, bool consider_imprint) const {
    const auto value = lookup(calc, prices, item, consider_imprint);
    if (!value) return std::nullopt;
    // Construction covers all priced primitives, the unchanged program grammar
    // and first-exit boundaries. A restricted quotient/program certificate has
    // no access to this issuer. This source lower bounds every legal Q/family.
    return QuotientLowerBoundary{0, exact_item_state_key(item), identity, *value, LowerEvidenceKind::IndependentLower};
}
ProofMemorySnapshot PreparedPhasePotential::memory_snapshot() const { return support_->memory_snapshot(); }
std::size_t PreparedPhasePotential::draw_count() const { return draws.size() + (reused_draw_owner ? reused_draw_owner->draw_count() : 0); }
const CalcContext::NativeGoalDrawBound& PreparedPhasePotential::draw(std::size_t i) const {
    const auto prior = reused_draw_owner ? reused_draw_owner->draw_count() : 0;
    return i < prior ? reused_draw_owner->draw(i) : draws.at(i-prior);
}

std::shared_ptr<const PreparedPhasePotential> PhaseLowerProducer::prepare_probabilistic(
        CalcContext& calc, const PhaseLowerPrices& prices, const pc_item_state& anchor,
        const PhaseLowerProposal& proposal, std::shared_ptr<const PreparedPhaseLowerView> support,
        const PreparedPhaseRestartLower& issued_restart_boundary,
        bool consider_imprint, bool retain_scour, const QuotientLowerBudget& budget,
        bool joint_refinement, std::shared_ptr<const PreparedPhasePotential> reuse_draws, PhaseContinuation continuation) {
    checkpoint(budget);
    if (continuation != PhaseContinuation::PriceOnly && (!joint_refinement || !retain_scour))
        throw std::invalid_argument("native continuation requires joint/no-op and retention coverage");
    const auto& restart_boundary = issued_restart_boundary.record;
    if (!support || !support->compatible(calc, prices, anchor))
        throw std::invalid_argument("probabilistic phase support/context mismatch");
    if (reuse_draws && (!reuse_draws->compatible(calc, prices, anchor, consider_imprint) ||
        reuse_draws->support_.get() != support.get()))
        throw std::invalid_argument("foreign shared native draw evidence");
    pc_item_state fresh{}; pc_item_clear(&fresh);
    auto expected_boundary_evidence = support->identity;
    expected_boundary_evidence.push_back(0x46524553484c4f57ull);
    expected_boundary_evidence.push_back(std::bit_cast<std::uint64_t>(restart_boundary.lower));
    if (restart_boundary.evidence != LowerEvidenceKind::IndependentLower ||
        restart_boundary.source_identity != exact_item_state_key(fresh) ||
        restart_boundary.evidence_identity != expected_boundary_evidence ||
        !std::isfinite(restart_boundary.lower) || restart_boundary.lower < 0)
        throw std::invalid_argument("restart needs matching existing exact-source lower evidence");
    if (consider_imprint) throw std::invalid_argument("unmodelled Imprint restore memory");
    for (auto i : calc.candidate_operators()) {
        const auto& op = calc.operators().at(i);
        if (op.kind == PlannerOperatorKind::FixedOption && op.option_kind == FixedOptionKind::ImprintRetry)
            throw std::invalid_argument("authored Imprint restore memory is uncovered");
    }
    std::uint32_t fracture = kNoId;
    for (unsigned i = 0; i < anchor.prefix_count; ++i)
        if (anchor.prefixes[i].flags & PC_MOD_SLOT_FRACTURED) fracture = anchor.prefixes[i].mod_id;
    for (unsigned i = 0; i < anchor.suffix_count; ++i)
        if (anchor.suffixes[i].flags & PC_MOD_SLOT_FRACTURED) fracture = anchor.suffixes[i].mod_id;
    if (!in_frame(calc, anchor, fracture) || calc.session().rare_affix_cap != 3 ||
        calc.layout().slots.size() > 5)
        throw std::invalid_argument("uncovered native probability frame");
    const auto fm = mod_mask(calc, fracture);
    if (!fm) throw std::invalid_argument("phase probability requires the measured fractured goal frame");
    const unsigned fs = calc.session().gen_type[fracture];
    const auto masks = static_cast<std::uint32_t>(support->values.size());
    const auto grid = 3*masks*16;
    const bool coupled = continuation == PhaseContinuation::CoupledFresh;
    const unsigned fresh_offset = grid+2;
    const unsigned extent = coupled ? fresh_offset+grid : grid+2;
    auto cap = std::min<std::uint64_t>(32ull << 20, budget.max_scratch_bytes);
    // Coupling retains both regions' diagnostic rows and event caps while
    // checking. Reserve their overlap before native generation.
    const std::uint64_t native_scratch = (coupled ? 6ull : 2ull) << 20;
    if (cap < (6ull << 20)) throw std::length_error("phase probability reservation refused");
    ScopedProofMemoryCharge scratch(support->store_->ledger(), ProofMemoryCategory::Scratch, native_scratch);
    std::vector<std::array<unsigned, 2>> minimum(masks, {99, 99});
    std::vector<unsigned> mod_goals(calc.session().mod_count);
    std::vector<unsigned> goal_side(calc.layout().slots.size(), 99);
    for (unsigned mod = 0; mod < calc.session().mod_count; ++mod) mod_goals[mod] = mod_mask(calc, mod);
    for (unsigned side = 0; side < 2; ++side) {
        std::vector<unsigned> costs(masks, 99); costs[0] = 0;
        unsigned side_mask = 0;
        for (unsigned mod = 0; mod < mod_goals.size(); ++mod)
            if (calc.session().gen_type[mod] == side) side_mask |= mod_goals[mod];
        for (unsigned bit = 0; bit < goal_side.size(); ++bit) if (side_mask & (1u << bit)) {
            if (goal_side[bit] != 99) throw std::invalid_argument("mixed-side goal event is uncovered");
            goal_side[bit] = side;
        }
        for (unsigned m = 0; m < masks; ++m)
            for (unsigned mod = 0; mod < mod_goals.size(); ++mod)
                if (calc.session().gen_type[mod] == side)
                    costs[m | mod_goals[mod]] = std::min(costs[m | mod_goals[mod]], costs[m]+1);
        // Native overlap-aware lower on required affix count; conflict-related
        // impossibilities remain optimistic cells, never silently disappear.
        for (unsigned m = 0; m < masks; ++m) {
            for (unsigned produced = 0; produced < masks; ++produced)
                if (((produced | m) & side_mask) == (produced & side_mask))
                    minimum[m][side] = std::min(minimum[m][side], costs[produced]);
        }
    }
    std::vector<Cell> cells;
    std::vector<QuotientBellmanCellInput> graph_cells;
    for (unsigned r = 0; r < 3; ++r) for (unsigned m = 0; m < masks; ++m)
        for (unsigned p = 0; p < 4; ++p) for (unsigned s = 0; s < 4; ++s) {
            const unsigned limit = r == PC_RARITY_RARE ? 3 : (r == PC_RARITY_MAGIC ? 1 : 0);
            const auto id = static_cast<std::uint32_t>(index(masks, r, m, p, s));
            const bool feasible = (m & fm) == fm && p <= limit && s <= limit &&
                p >= minimum[m][0] && s >= minimum[m][1] && (fs ? s : p) >= 1;
            const bool goal = r == calc.goal().rarity && std::popcount(m) >= calc.goal().required_satisfied_slots() &&
                p+s == std::popcount(m);
            if (feasible) graph_cells.push_back({id, 1, {version, id}, goal});
            if (feasible && !goal) cells.push_back({r, m, p, s, id, false});
        }
    graph_cells.push_back({grid, 1, {version, grid}, true}); // other outside-frame zero
    // In coupled mode the old boundary slot is an unused zero, not an
    // independent certificate for the new fresh-region variable.
    graph_cells.push_back({grid+1, 1, restart_boundary.source_identity, coupled});
    if (coupled) for (unsigned r = 0; r < 3; ++r) for (unsigned m = 0; m < masks; ++m)
        for (unsigned p = 0; p < 4; ++p) for (unsigned s = 0; s < 4; ++s) {
            const unsigned limit = r == PC_RARITY_RARE ? 3 : (r == PC_RARITY_MAGIC ? 1 : 0);
            const auto id = static_cast<std::uint32_t>(fresh_offset+index(masks, r, m, p, s));
            const bool feasible = p <= limit && s <= limit && p >= minimum[m][0] && s >= minimum[m][1];
            const bool goal = r == calc.goal().rarity && std::popcount(m) >= calc.goal().required_satisfied_slots() &&
                p+s == std::popcount(m);
            if (feasible) graph_cells.push_back({id, 1, {version, id}, goal});
            if (feasible && !goal) cells.push_back({r, m, p, s, id, false});
        }
    std::vector<double> candidate(extent, 0);
    if (!coupled) candidate[grid+1] = restart_boundary.lower;
    PhaseProposalRefusal refusal;
    if (proposal.role != PhaseTableRole::CleanCompletion || proposal.mask_count != masks ||
        proposal.values.size() != grid || proposal.required != calc.goal().required_satisfied_slots())
        refusal.kind = "wrong_table_role_or_dimensions";
    else for (const auto& cell : graph_cells) {
        if (cell.cell_id < grid && cell.terminal && proposal.values[cell.cell_id] != 0) {
            const auto id = cell.cell_id;
            const auto s = id%4, p = (id/4)%4, m = (id/16)%masks, r = id/(masks*16);
            if ((m & fm) == fm && r == calc.goal().rarity &&
                std::popcount(m) >= proposal.required && p+s == std::popcount(m) &&
                p >= minimum[m][0] && s >= minimum[m][1]) {
                refusal = {"nonzero_terminal", "clean completion proposal terminal", id, UINT32_MAX, UINT32_MAX, proposal.values[id]};
                break;
            }
        }
    }
    if (proposal.values.size() == grid) for (const auto& c : cells) {
        const auto value = proposal.values[c.id >= fresh_offset ? c.id-fresh_offset : c.id];
        if (!std::isfinite(value) || value < 0) {
            if (refusal.kind.empty()) refusal = {"numeric_inconclusive", "clean proposal value", c.id};
        } else candidate[c.id] = value;
    }
    std::vector<CalcContext::NativeGoalDrawBound> draws;
    std::map<std::tuple<unsigned, unsigned, bool>, unsigned> draw_cache;
    const auto reused_count = reuse_draws ? reuse_draws->draw_count() : 0;
    for (unsigned i = 0; i < reused_count; ++i) {
        const auto& w = reuse_draws->draw(i);
        draw_cache[{w.action, w.slot, w.guaranteed}] = i;
    }
    const auto draw_at = [&](unsigned i) -> const CalcContext::NativeGoalDrawBound& {
        return i < reused_count ? reuse_draws->draw(i) : draws.at(i-reused_count);
    };
    const auto get_draw = [&](unsigned a, unsigned slot, bool guaranteed) -> const auto& {
        const auto key = std::tuple{a, slot, guaranteed};
        const auto found = draw_cache.find(key);
        if (found != draw_cache.end()) return draw_at(found->second);
        checkpoint(budget);
        auto result = calc.phase_goal_draw_bound(anchor, a, slot, guaranteed);
        draw_cache[key] = static_cast<unsigned>(reused_count+draws.size());
        draws.push_back(std::move(result));
        return draws.back();
    };
    bool probability_frame_escape = false;
    const auto upper = [&](unsigned a, unsigned slot, unsigned p, unsigned s, unsigned count) {
        const auto& natural = get_draw(a, slot, false);
        probability_frame_escape |= natural.frame_escape;
        double u = draw_upper(natural, p, s);
        const auto type = calc.registry().actions[a].params.type;
        if (type == ActionType::HarvestReforge || type == ActionType::HarvestAugment) {
            const auto& targeted = get_draw(a, slot, true);
            probability_frame_escape |= targeted.frame_escape;
            u = std::max(u, draw_upper(targeted, p, s));
        }
        return u == 0 || count == 0 ? 0 : std::min(1.0, std::nextafter(count*u, std::numeric_limits<double>::infinity()));
    };
    std::vector<PhaseJointEventWitness> joint_events;
    std::map<std::tuple<unsigned, unsigned, unsigned>, unsigned> joint_cache;
    const auto joint_upper = [&](unsigned a, unsigned subset, unsigned side,
            unsigned forced, bool has_forced, unsigned output_limit, unsigned frame_mask, unsigned frame_side) {
        // Same-side event only. Native hit masks must require DISTINCT draws;
        // a multi-goal modifier refuses this shortcut, leaving marginal caps.
        for (auto hit : mod_goals) if (std::popcount(hit & subset) > 1) return 1.0;
        const auto key = std::tuple{a, subset, frame_mask};
        if (const auto found = joint_cache.find(key); found != joint_cache.end())
            return joint_events[found->second].joint_upper;
        PhaseJointEventWitness w;
        w.action = a; w.subset = subset; w.retained = frame_mask; w.forced = forced; w.side = side;
        w.initial_same_side = frame_side == side; w.positions = output_limit-w.initial_same_side;
        w.uniform_history = has_forced;
        w.other_side_blockers = 3;
        const auto type = calc.registry().actions[a].params.type;
        for (unsigned slot = 0; slot < goal_side.size(); ++slot) if (subset & (1u << slot)) {
            std::array<double, 3> position{};
            for (unsigned j = 0; j < w.positions; ++j) {
                // Applied ordinary renewal clears all non-fractured modifiers.
                // Forced modifiers precede random draws; for those actions use
                // the uniform worst-history count instead of a depth claim.
                const unsigned same = has_forced ? output_limit-1 : w.initial_same_side+j;
                const auto& natural = get_draw(a, slot, false);
                probability_frame_escape |= natural.frame_escape;
                position[j] = draw_upper(natural, side ? 3 : same, side ? same : 3);
                if (type == ActionType::HarvestReforge) {
                    const auto& guaranteed = get_draw(a, slot, true);
                    probability_frame_escape |= guaranteed.frame_escape;
                    position[j] = std::max(position[j], draw_upper(guaranteed, side ? 3 : same, side ? same : 3));
                }
            }
            w.conditional.push_back(position);
            w.natural_draws.push_back(draw_cache.at({a, slot, false}));
            if (type == ActionType::HarvestReforge) w.guaranteed_draws.push_back(draw_cache.at({a, slot, true}));
            w.marginal_upper = std::min(w.marginal_upper, upper(a, slot, side ? 3 : output_limit-1,
                side ? output_limit-1 : 3, output_limit));
        }
        if (probability_frame_escape) return 1.0;
        w.joint_upper = phase_joint_assignment_upper(w.conditional, w.positions);
        w.capacity = capacity(w.joint_upper);
        joint_cache[key] = static_cast<unsigned>(joint_events.size());
        joint_events.push_back(std::move(w));
        return joint_events.back().joint_upper;
    };
    std::vector<PhasePotentialRelation> final_relations;
    std::set<std::pair<unsigned, unsigned>> detailed;
    std::vector<PhasePriceReactivation> reactivations;
    std::uint64_t combined_peak = support->memory_snapshot().peak_total_bytes, action_relations = 0;
    unsigned rounds = 0;
    for (; rounds < 32; ++rounds) {
        checkpoint(budget);
        if (support->memory_snapshot().total_bytes >= cap)
            throw std::length_error("phase live evidence exhausts additional reservation");
        const auto graph_cap = cap-support->memory_snapshot().total_bytes;
        QuotientBellmanGraph graph(graph_cap, QuotientBellmanMode::LowerOnly, cap);
        graph.install_cells(graph_cells);
        QuotientLowerQuery query;
        query.request_identity = {version, rounds, retain_scour}; query.caller_scope = {version, 1};
        query.coefficients = LowerCoefficientModel::ExactBinaryModel;
        auto boundary = restart_boundary;
        boundary.cell_id = grid+1;
        // The numerical model has a short local identity; full native context
        // and the existing boundary producer are bound by the returned view.
        boundary.evidence_identity = {version, 0x4652455348};
        if (!coupled) query.boundaries.push_back(std::move(boundary));
        query.roots = {static_cast<std::uint32_t>(index(masks, anchor.rarity, item_mask(calc, anchor),
            anchor.prefix_count, anchor.suffix_count))};
        std::vector<PhasePotentialRelation> relations;
        std::vector<PhasePriceReactivation> shortcuts;
        for (const auto& c : cells) {
            checkpoint(budget);
            const unsigned offset = c.id >= fresh_offset ? fresh_offset : 0;
            const unsigned frame_mask = offset ? 0 : fm, frame_side = offset ? 2 : fs;
            QuotientLowerSource source{c.id, {version, c.id}, {query.caller_scope, 1, true, {}, {}}, {}};
            CanonicalActionSet native_scope{{version, c.id}, 1, true, {}, {}};
            std::vector<CanonicalActionCover> native_cover;
            struct Row { double cost; unsigned action; std::vector<Group> groups; bool probability, price; PhaseRelationReason reason;
                std::vector<PhasePotentialRelation::Event> events; int phase_branch; };
            std::map<StableKey, Row> rows;
            for (unsigned a = 0; a < calc.registry().actions.size(); ++a) {
                const auto& action = calc.registry().actions[a];
                if (action.synthetic && action.id != "restart")
                    throw std::invalid_argument("uncovered synthetic phase action");
                const double cost = phase_price_lower(action, prices);
                if (!std::isfinite(cost)) continue;
                ++action_relations;
                native_scope.actions.push_back({version, a}); native_cover.push_back({{version, a}, false, {}});
                if (!(action.legality.rarity_mask & (1u << c.rarity))) continue;
                const auto type = action.params.type;
                bool special_escape = false;
                if (type == ActionType::Bench && action.params.mod_id < calc.session().mod_count)
                    special_escape = calc.session().metamod_type[action.params.mod_id] >= 0;
                if (type == ActionType::Fossil) for (auto fossil : action.params.fossil_indices) {
                    const auto& data = *calc.session().data;
                    special_escape |= (fossil < data.fossil_mirrors.size() && data.fossil_mirrors[fossil]) ||
                        data.string_at(data.fossil_name_sids.at(fossil)) == "Bloodstained Fossil";
                    for (auto mod : calc.session().fossil_forced_mod_ids.at(fossil))
                        special_escape |= calc.session().metamod_type.at(mod) >= 0;
                }
                if (type == ActionType::Unveil || (type == ActionType::Fracture && frame_mask)) continue; // exact frame predicates
                const auto lim = c.rarity == PC_RARITY_MAGIC ? 1u : 3u;
                if (action.legality.requires_open_affix && c.p == lim && c.s == lim) continue;
                if (action.legality.min_total_affixes > c.p+c.s) continue;
                // Phase is hidden in this uniform view. Cover each legal native
                // branch by a separate optimistic choice, never average phases.
                const bool split_phase = type == ActionType::EldritchChaos &&
                    continuation != PhaseContinuation::PriceOnly;
                for (int phase = -1; phase < (split_phase ? 2 : 0); ++phase) {
                    std::vector<Group> groups;
                    bool price_escape = false;
                    auto reason = PhaseRelationReason::NativeEffect;
                    const auto escape = [&](PhaseRelationReason why = PhaseRelationReason::UnsupportedEffect) {
                        groups = {{0, mass, {grid}}}; price_escape = true; reason = why;
                    };
                    const auto add_group = [&](unsigned m, unsigned p, unsigned s, unsigned r) {
                        if (p > 3 || s > 3 || p < minimum[m][0] || s < minimum[m][1] || (m & frame_mask) != frame_mask) return;
                        const unsigned limit = r == PC_RARITY_MAGIC ? 1 : (r == PC_RARITY_RARE ? 3 : 0);
                        if (p > limit || s > limit) return;
                        auto found = std::find_if(groups.begin(), groups.end(), [&](const auto& g) { return g.mask == m; });
                        if (found == groups.end()) { groups.push_back({m, mass, {}}); found = std::prev(groups.end()); }
                        found->cells.push_back(static_cast<std::uint32_t>(offset+index(masks, r, m, p, s)));
                    };
                    bool probabilistic = false, renewal = false;
                    unsigned preserved = c.mask, draws_per_side = 1;
                    if (action.synthetic) {
                        groups = {{0, mass, {coupled ? fresh_offset : grid+1}}};
                        reason = coupled ? PhaseRelationReason::NativeEffect : PhaseRelationReason::FixedIndependentBoundary;
                    } else if (type == ActionType::Fracture || special_escape || (action.sets_flags & kProtectionFlags) ||
                        type == ActionType::InfluenceExalt || type == ActionType::VeiledExalt || type == ActionType::VeiledChaos) {
                        escape(PhaseRelationReason::NativeDomainEscape);
                    } else if (cost >= candidate[c.id] && !detailed.contains({c.id, a})) {
                        // A priced escape has an independent immediate-cost floor.
                        // Evaluating it first avoids constructing unused relations.
                        escape(PhaseRelationReason::CandidatePriceShortcut);
                        shortcuts.push_back({c.id, a, rounds, cost, candidate[c.id]});
                    } else if (type == ActionType::HarvestAugment || type == ActionType::HarvestResist) {
                        escape();
                    } else if (type == ActionType::EldritchEmber || type == ActionType::EldritchIchor) {
                        add_group(c.mask, c.p, c.s, c.rarity);
                    } else if (type == ActionType::Scour) {
                        if (!retain_scour) escape(); // old clean projection's explicitly free fracture loss
                        else add_group(frame_mask, frame_side == 0, frame_side == 1, frame_mask ? PC_RARITY_MAGIC : PC_RARITY_NORMAL);
                    } else if (type == ActionType::Annul || type == ActionType::EldritchAnnul ||
                               type == ActionType::RemoveCraftedModifiers) {
                        // Grant the best legal loss/cleanup. Keep the exact fracture.
                        for (unsigned m = 0; m < masks; ++m) if ((m | c.mask) == c.mask)
                            for (unsigned p = 0; p <= c.p; ++p) for (unsigned s = 0; s <= c.s; ++s)
                                if (type == ActionType::RemoveCraftedModifiers || p+s+1 == c.p+c.s)
                                    add_group(m, p, s, c.rarity);
                        add_group(c.mask, c.p, c.s, c.rarity); // failed native call
                    } else if (type == ActionType::Bench) {
                        if (action.params.mod_id >= mod_goals.size()) { escape(); }
                        else {
                            const auto side = calc.session().gen_type[action.params.mod_id];
                            add_group(c.mask | mod_goals[action.params.mod_id], c.p+(side == 0), c.s+(side == 1), c.rarity);
                            add_group(c.mask, c.p, c.s, c.rarity);
                        }
                    } else if (type == ActionType::Augment || type == ActionType::Regal ||
                               type == ActionType::Exalt || type == ActionType::EldritchExalt) {
                        probabilistic = true;
                        const auto r = type == ActionType::Regal ? PC_RARITY_RARE : c.rarity;
                        for (unsigned mod = 0; mod < mod_goals.size(); ++mod) {
                            const auto side = calc.session().gen_type[mod];
                            if (side < 0 || side > 1) continue;
                            add_group(c.mask | mod_goals[mod], c.p+(side == 0), c.s+(side == 1), r);
                        }
                        add_group(c.mask, c.p, c.s, r); // empty pool / failed application remains covered
                    } else if (action_transition_facts(type).renewal) {
                        probabilistic = renewal = true;
                        preserved = frame_mask;
                        const auto r = type == ActionType::Transmute || type == ActionType::Alteration ? PC_RARITY_MAGIC : PC_RARITY_RARE;
                        draws_per_side = r == PC_RARITY_MAGIC ? 1 : 3;
                        if (type == ActionType::EldritchChaos) {
                            if (!split_phase) { probabilistic = false; escape(); }
                            else {
                                unsigned unchanged_mask = 0;
                                if (phase >= 0) for (unsigned slot = 0; slot < goal_side.size(); ++slot)
                                    if (goal_side[slot] != static_cast<unsigned>(phase)) unchanged_mask |= 1u << slot;
                                preserved = frame_mask | (c.mask & unchanged_mask);
                                // do_eldritch_chaos / preserved_reforge_base: no
                                // dominance reforges both sides; otherwise only the
                                // dominant side, retaining its fracture and every
                                // opposite affix. Target is 2 or 3 on that side.
                                // Empty-pool early stopping remains optimistic here.
                                for (unsigned m = 0; m < masks; ++m) {
                                    if ((m & unchanged_mask) != (c.mask & unchanged_mask)) continue;
                                    for (unsigned p = 0; p <= 3; ++p) for (unsigned s = 0; s <= 3; ++s) {
                                        if ((phase == 0 && s != c.s) || (phase == 1 && p != c.p)) continue;
                                        add_group(m, p, s, r);
                                    }
                                }
                                add_group(c.mask, c.p, c.s, c.rarity);
                            }
                        } else {
                            for (unsigned m = 0; m < masks; ++m)
                                for (unsigned p = 0; p <= draws_per_side; ++p)
                                    for (unsigned s = 0; s <= draws_per_side; ++s) add_group(m, p, s, r);
                            // Failed no-op and pathological empty-pool histories.
                            add_group(c.mask, c.p, c.s, c.rarity);
                        }
                    } else { escape(); }
                    if (groups.empty()) continue; // impossible effect in the declared frame
                    probability_frame_escape = false;
                    unsigned forced = 0;
                    bool has_forced = false;
                    if (type == ActionType::Essence && action.params.essence_index < calc.session().essence_guaranteed_mod_ids.size()) {
                        const auto mod = calc.session().essence_guaranteed_mod_ids[action.params.essence_index];
                        if (mod < mod_goals.size()) { forced |= mod_goals[mod]; has_forced = true; }
                    }
                    if (type == ActionType::Fossil) for (auto fossil : action.params.fossil_indices)
                        for (auto mod : calc.session().fossil_forced_mod_ids.at(fossil)) {
                            forced |= mod_goals.at(mod); has_forced = true;
                        }
                    for (auto& g : groups) {
                        std::sort(g.cells.begin(), g.cells.end());
                        g.cells.erase(std::unique(g.cells.begin(), g.cells.end()), g.cells.end());
                        if (!probabilistic) continue;
                        double event_upper = 1;
                        for (unsigned slot = 0; slot < goal_side.size(); ++slot) {
                            const auto bit = 1u << slot;
                            if (!(g.mask & bit) || (preserved & bit)) continue;
                            if (forced & bit) continue;
                            const unsigned side = goal_side[slot];
                            const unsigned p = renewal ? 3-(side == 0) : c.p;
                            const unsigned s = renewal ? 3-(side == 1) : c.s;
                            const auto u = (side ? s : p) >= (renewal ? 3u : (type == ActionType::Regal ? 3u : lim))
                                ? 0 : upper(a, slot, p, s, draws_per_side);
                            event_upper = std::min(event_upper, u);
                        }
                        if (joint_refinement && renewal) {
                            for (unsigned side = 0; side < 2; ++side) {
                                unsigned subset = 0;
                                for (unsigned slot = 0; slot < goal_side.size(); ++slot)
                                    if (goal_side[slot] == side && (g.mask & ~preserved & ~forced & (1u << slot))) subset |= 1u << slot;
                                if (std::popcount(subset) >= 2)
                                    event_upper = std::min(event_upper, joint_upper(a, subset, side, forced, has_forced, draws_per_side, frame_mask, frame_side));
                            }
                            // Capacities describe APPLIED redraws. A failed/no-op
                            // may retain every source goal: grant an observed self
                            // choice on each outcome so either mode is covered.
                            g.cells.push_back(c.id);
                            std::sort(g.cells.begin(), g.cells.end());
                            g.cells.erase(std::unique(g.cells.begin(), g.cells.end()), g.cells.end());
                        }
                        // One exact-mask event entails every one of its new goals.
                        // min marginal bounds is valid under ANY joint dependence.
                        g.capacity = capacity(event_upper);
                    }
                    if (probability_frame_escape) escape(PhaseRelationReason::NativeDomainEscape);
                    if (probabilistic && !price_escape) reason = PhaseRelationReason::ProbabilityEnvelope;
                    // Union of all exact-mask events is normalized. Greedily fill
                    // the cheapest frozen-value events up to their proved capacities.
                    const auto best_value = [&](const Group& g) {
                        double best = std::numeric_limits<double>::infinity();
                        for (auto id : g.cells) best = std::min(best, candidate[id]);
                        return best;
                    };
                    std::vector<double> event_values;
                    std::vector<std::uint32_t> event_caps;
                    std::vector<PhasePotentialRelation::Event> events;
                    for (const auto& g : groups) {
                        event_values.push_back(best_value(g)); event_caps.push_back(g.capacity);
                        const auto target = *std::min_element(g.cells.begin(), g.cells.end(),
                            [&](auto a, auto b) { return candidate[a] < candidate[b]; });
                        events.push_back({g.mask, target, g.capacity});
                    }
                    std::vector<Group> selected;
                    StableKey key;
                    std::map<unsigned, unsigned> frozen_mass;
                    for (auto [group_id, probability] : phase_minimum_event_allocation(event_values, event_caps))
                        frozen_mass[events[group_id].minimum_cell] += probability;
                    // Numerically identical frozen relations share the cheapest
                    // price after COMPLETE native coverage. Do not keep distinct
                    // physical-choice lists in a value-specific quotient witness.
                    for (auto [target, probability] : frozen_mass) {
                        key.push_back(target); key.push_back(probability);
                        selected.push_back({0, probability, {target}});
                    }
                    const auto found = rows.find(key);
                    if (found == rows.end()) rows.emplace(std::move(key), Row{cost, a, std::move(selected), probabilistic && !price_escape, price_escape, reason, std::move(events), split_phase ? phase : -2});
                    else if (cost < found->second.cost) found->second = {cost, a, std::move(selected), probabilistic && !price_escape, price_escape, reason, std::move(events), split_phase ? phase : -2};
                }
            } // complete registry actions and native phase branches
            const auto coverage = validate_canonical_action_coverage(native_scope, native_cover);
            if (!coverage.empty()) throw std::invalid_argument(coverage);
            unsigned n = 0;
            for (const auto& [pattern, row] : rows) {
                (void)pattern;
                // Request/source/revision are already separate provenance fields.
                // These local IDs need not duplicate those coordinates per row.
                const StableKey action_key{n++}, evidence{row.action};
                source.expected_actions.actions.push_back(action_key);
                QuotientBellmanRowInput input;
                input.source_cell_id = c.id; input.operator_index = row.action; input.cost = row.cost;
                PhasePotentialRelation record{c.id, row.action, row.cost, row.cost, {}, {}};
                record.probability_aware = row.probability; record.independent_price = row.price;
                record.reason = row.reason; record.phase_branch = row.phase_branch;
                record.events = row.events;
                for (const auto& group : row.groups) {
                    const double p = std::ldexp(static_cast<double>(group.capacity), -24);
                    const auto target = *std::min_element(group.cells.begin(), group.cells.end(),
                        [&](auto l, auto r) { return candidate[l] < candidate[r]; });
                    // These rows are witnesses for THIS frozen vector. Retain
                    // its minimum only; every candidate change reconstructs all
                    // event/occupancy/self minima before simultaneous checking.
                    // The selected row is not an all-vector native kernel.
                    input.choices.push_back({p, false, {target}});
                    record.targets.push_back(target); record.probabilities.push_back(p);
                    record.rhs = down(record.rhs+down(p*candidate[target]));
                }
                input.lower_provenance = QuotientLowerRowProvenance{query.request_identity,
                    source.source_identity, action_key, evidence, LowerEvidenceKind::ExactDeclaredKernel};
                // Keep the potentially throwing arena reservation outside
                // aggregate temporary construction (including unwind cleanup).
                const auto row_id = graph.append_row(std::move(input));
                source.constraints.push_back({{action_key, false, {}}, LowerConstraintKind::Row,
                    row_id, 0, evidence, LowerEvidenceKind::ExactDeclaredKernel});
                relations.push_back(std::move(record));
            }
            query.sources.push_back(std::move(source));
        }
        query.model_revision = graph.model_revision();
        auto local_budget = budget; local_budget.max_scratch_bytes = graph_cap;
        std::vector<double> dense_candidate;
        dense_candidate.reserve(graph_cells.size());
        for (const auto& cell : graph_cells) dense_candidate.push_back(candidate[cell.cell_id]);
        auto checked = graph.check_lower(query, dense_candidate, local_budget);
        combined_peak = std::max(combined_peak, graph.proof_store()->ledger().snapshot().peak_total_bytes + support->memory_snapshot().total_bytes);
        if (rounds == 0 && !checked.checked && refusal.kind.empty()) {
            refusal = {"numeric_inconclusive", checked.reason};
            for (const auto& row : relations) {
                double continuation_upper = 0;
                for (unsigned i = 0; i < row.targets.size(); ++i) {
                    const double term = std::nextafter(row.probabilities[i]*candidate[row.targets[i]],
                        std::numeric_limits<double>::infinity());
                    continuation_upper = std::nextafter(continuation_upper+term, std::numeric_limits<double>::infinity());
                }
                const double rhs_upper = std::nextafter(row.cost+continuation_upper, std::numeric_limits<double>::infinity());
                if (candidate[row.cell] > rhs_upper) {
                    refusal = {"violated_inequality", "clean completion exceeds outward upper of frozen native-mask RHS",
                        row.cell, row.action, UINT32_MAX, candidate[row.cell], row.cost, continuation_upper,
                        row.targets, row.probabilities};
                    break;
                }
            }
        }
        bool reactivated = false;
        std::vector<double> minimum_rhs(extent, std::numeric_limits<double>::infinity());
        for (const auto& row : relations) minimum_rhs[row.cell] = std::min(minimum_rhs[row.cell], row.rhs);
        if (joint_refinement) for (const auto& shortcut : shortcuts)
            // Compare with the minimum raw RHS, not a globally shrunken
            // repaired candidate (the quotient may lower it by 1e-10).
            if (phase_price_shortcut_limiting(shortcut.cost, minimum_rhs[shortcut.cell])) {
                detailed.insert({shortcut.cell, shortcut.action});
                auto record = shortcut; record.minimum_rhs = minimum_rhs[shortcut.cell];
                reactivations.push_back(record); reactivated = true;
            }
        if (checked.checked && !joint_refinement) {
            // Rows were rebuilt from THIS frozen vector, including cheap-price
            // partitions and event order. The quotient checks all simultaneous
            // dependencies; the rows are not promoted to all-vector kernels.
            final_relations = std::move(relations);
            break;
        }
        if (reactivated) continue;
        auto repaired = graph.solve_lower(query, local_budget);
        combined_peak = std::max(combined_peak, graph.proof_store()->ledger().snapshot().peak_total_bytes + support->memory_snapshot().total_bytes);
        if (!repaired.checked) throw std::runtime_error("probabilistic quotient repair: "+repaired.reason);
        std::vector<double> repaired_projection(extent, 0);
        for (unsigned i = 0; i < graph_cells.size(); ++i)
            repaired_projection[graph_cells[i].cell_id] = repaired.checked->values_by_state.at(i);
        if (checked.checked && joint_refinement) {
            // Feasibility alone is insufficient after removing a temporary
            // cap: the old, smaller candidate may still pass. Re-solve the
            // reoptimized model and continue whenever it can improve a cell.
            bool improves = false;
            for (const auto& c : cells) {
                const auto next = repaired_projection[c.id];
                improves |= next > candidate[c.id] + 64*std::numeric_limits<double>::epsilon()*std::max(1.0, next);
            }
            if (!improves) { final_relations = std::move(relations); break; }
        }
        candidate = std::move(repaired_projection);
    }
    if (rounds == 32) throw std::runtime_error("probabilistic joint/price refinement did not close within bounded rounds");
    if (!coupled) candidate.resize(grid);
    checkpoint(budget);
    std::uint64_t bytes = 65536 + support->identity.capacity()*8 + candidate.capacity()*24 +
        proposal.values.capacity()*8 + draws.capacity()*sizeof(CalcContext::NativeGoalDrawBound) +
        final_relations.capacity()*sizeof(PhasePotentialRelation) +
        joint_events.capacity()*sizeof(PhaseJointEventWitness) + reactivations.capacity()*sizeof(PhasePriceReactivation);
    for (const auto& event : joint_events) bytes += 4*(event.natural_draws.capacity()+event.guaranteed_draws.capacity()) +
        sizeof(std::array<double, 3>)*event.conditional.capacity();
    for (const auto& row : final_relations) bytes += row.targets.capacity()*4 + row.probabilities.capacity()*8 +
        row.events.capacity()*sizeof(PhasePotentialRelation::Event);
    if (bytes > cap-support->memory_snapshot().total_bytes)
        throw std::length_error("phase retained evidence exceeds matched reservation");
    combined_peak = std::max(combined_peak, support->memory_snapshot().total_bytes+bytes);
    auto result = std::shared_ptr<const PreparedPhasePotential>(new PreparedPhasePotential(
        std::move(support), std::move(candidate), proposal, std::move(refusal), fracture, fm,
        retain_scour, restart_boundary.lower, std::move(draws), std::move(final_relations), rounds+1, bytes,
        combined_peak, action_relations, std::move(reuse_draws), std::move(joint_events),
        std::move(reactivations), joint_refinement, continuation));
    return result;
}
} // namespace poecraft::solver
