#include "solver_phase_lower.hpp"
#include "solver_quotient_bellman.hpp"
#include "poecraft/bitset.h"

#include <bit>
#include <cmath>
#include <chrono>
#include <numeric>

namespace poecraft::solver {
using namespace quotient;
namespace {
constexpr std::uint64_t version = 0x50524f424c4f0006ull;
using PreparationClock = std::chrono::steady_clock;
std::uint64_t elapsed_ns(PreparationClock::time_point start) {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(PreparationClock::now()-start).count();
}
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
unsigned filter_observation(const pc_item_state& item, const std::array<std::uint32_t,2>& mods) {
    for (unsigned side=0;side<2;++side) {
        const auto* slots=side ? item.suffixes : item.prefixes;
        for (unsigned i=0;i<(side ? item.suffix_count : item.prefix_count);++i)
            for (unsigned mode=0;mode<mods.size();++mode)
                if (slots[i].mod_id==mods[mode]) return mode+1;
    }
    return 0;
}
bool in_frame(const CalcContext& calc, const pc_item_state& item, std::uint32_t fracture,
        const std::array<std::uint32_t,2>& filters = {kNoId,kNoId}) {
    // The explicit pool reads generic influence and cannot-roll metamods;
    // Eldritch tiers select a side, never its weights. Both side choices are
    // retained below. Other cosmetic item coordinates are not pool inputs.
    if (item.generic_influence_bits || item.item_flags || item.rarity > PC_RARITY_RARE ||
        item.prefix_count > 3 || item.suffix_count > 3) return false;
    unsigned fractures = 0, filter_count = 0;
    const auto side = [&](const pc_mod_slot* slots, unsigned count) {
        for (unsigned i = 0; i < count; ++i) {
            const auto& slot = slots[i];
            if (slot.mod_id >= calc.session().mod_count ||
                (slot.flags & PC_MOD_SLOT_VEILED)) return false;
            if (calc.session().metamod_type.at(slot.mod_id) >= 0) {
                if (std::find(filters.begin(),filters.end(),slot.mod_id)==filters.end() ||
                    !(slot.flags&PC_MOD_SLOT_CRAFTED) || (slot.flags&PC_MOD_SLOT_FRACTURED) ||
                    ++filter_count>1) return false;
            }
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
std::uint64_t coordinate(unsigned base, unsigned crafted, unsigned jp, unsigned js, unsigned filter = 0) {
    return base | (std::uint64_t(crafted | (jp << 5) | (js << 7) | (filter << 9)) << 32);
}
struct CraftedObservation { unsigned goals = 0, jp = 0, js = 0; bool distinct = true; };
CraftedObservation crafted_observation(const CalcContext& calc, const pc_item_state& item) {
    CraftedObservation result;
    unsigned seen = 0;
    for (unsigned side = 0; side < 2; ++side) {
        const auto* slots = side ? item.suffixes : item.prefixes;
        for (unsigned i = 0; i < (side ? item.suffix_count : item.prefix_count); ++i) {
            const auto hit = mod_mask(calc, slots[i].mod_id);
            result.distinct &= std::popcount(hit) <= 1 && !(seen & hit);
            seen |= hit;
            if (!(slots[i].flags & PC_MOD_SLOT_CRAFTED) || (slots[i].flags & PC_MOD_SLOT_FRACTURED)) continue;
            if (hit) result.goals |= hit;
            else ++(side ? result.js : result.jp);
        }
    }
    return result;
}
StableKey potential_identity(const PreparedPhaseLowerView& support, const std::vector<double>& values,
        std::uint32_t mod, bool retained, double restart_lower, bool joint, PhaseContinuation continuation,
        PhaseRetention retention, const std::vector<std::uint64_t>& coordinates, unsigned crafted_domain, unsigned crafted_limit, PhasePreparationOptions options,
        const std::array<std::uint32_t,2>& filters) {
    auto key = support.identity;
    key.insert(key.end(), {version, mod, retained, joint, 0 /* unchanged Imprint exclusion */});
    key.push_back(static_cast<unsigned>(continuation));
    key.insert(key.end(), {static_cast<unsigned>(retention), crafted_domain, crafted_limit, options.minimum_reforge_occupancy, options.filter_modes});
    key.insert(key.end(),filters.begin(),filters.end());
    key.insert(key.end(), coordinates.begin(), coordinates.end());
    key.push_back(std::bit_cast<std::uint64_t>(restart_lower));
    for (double x : values) key.push_back(std::bit_cast<std::uint64_t>(x));
    return key;
}
struct Cell { unsigned rarity, mask, p, s; std::uint32_t id; bool goal;
    unsigned base = 0, crafted = 0, jp = 0, js = 0, filter = 0; };
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

unsigned phase_refill_minimum(unsigned prefixes, unsigned suffixes, unsigned target,
        const std::function<bool(unsigned, unsigned)>& uniformly_nonempty) {
    if (prefixes > 3 || suffixes > 3 || target > 6 || !uniformly_nonempty)
        throw std::invalid_argument("invalid native refill occupancy domain");
    for (unsigned total = prefixes+suffixes; total < target; ++total)
        for (unsigned p = prefixes; p <= 3; ++p) {
            if (p > total || total-p < suffixes || total-p > 3) continue;
            if (!uniformly_nonempty(p,total-p)) return total;
        }
    return std::max(prefixes+suffixes,target);
}

double phase_joint_assignment_upper(const std::vector<std::array<double, 3>>& conditional,
        unsigned positions) {
    // math: obligation CLM-0015 — Caller establishes uniform conditional histories,
    // distinct goal draws, retained/forced goals and native side limits.
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
    // math: uses CLM-0014 — Independent caps only; caller proves complete native
    // event coverage. Recompute this minimum for every changed potential.
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
        std::vector<PhasePriceReactivation> reactivated, bool joint, PhaseContinuation mode,
        PhaseRetention refinement, std::vector<std::uint64_t> projection, unsigned crafted_domain, unsigned crafted_limit,
        std::array<std::uint32_t,2> filters, std::vector<PhaseNonemptyWitness> nonempty, std::vector<PhaseRefillWitness> refills, PhasePreparationOptions options, PhasePreparationStats stats)
    : identity(potential_identity(*support, table, mod, retained, restart_lower, joint, mode, refinement, projection, crafted_domain, crafted_limit, options, filters)), values(std::move(table)),
      proposal(std::move(proposed)), proposal_refusal(std::move(refusal)), fractured_mod(mod),
      fractured_mask(mask), retained_scour(retained), restart_boundary_lower(restart_lower), draws(std::move(weights)),
      reused_draw_owner(std::move(reused)), joint_events(std::move(events)), reactivations(std::move(reactivated)),
      joint_refinement(joint), continuation(mode), retention(refinement), preparation_options(options), preparation_stats(stats), coordinates(std::move(projection)),
      crafted_goal_domain(crafted_domain), crafted_count_limit(crafted_limit), filter_mods(filters), nonempty_witnesses(std::move(nonempty)), refill_witnesses(std::move(refills)), relations(std::move(rows)),
      model_rounds(rounds), retained_reservation(reservation), peak_additional_bytes(peak),
      native_action_relations(action_relations), support_(std::move(support)),
      charge_(support_->store_->ledger(), ProofMemoryCategory::Certificate, reservation) {
    for (unsigned i = 0; i < coordinates.size(); ++i)
        if (coordinates[i] != UINT64_MAX) coordinate_index_.emplace(coordinates[i], i);
}
bool PreparedPhasePotential::compatible(const CalcContext& calc, const PhaseLowerPrices& prices,
        const pc_item_state& item, bool consider_imprint) const {
    if (consider_imprint || !(in_frame(calc,item,fractured_mod,filter_mods) ||
        (continuation == PhaseContinuation::CoupledFresh && in_frame(calc,item,kNoId,filter_mods)))) return false;
    if (retention != PhaseRetention::None) {
        const auto crafted = crafted_observation(calc, item);
        if (!crafted.distinct || (crafted.goals & ~crafted_goal_domain) ||
            std::popcount(crafted.goals)+crafted.jp+crafted.js > crafted_count_limit) return false;
    }
    auto context_item = item;
    context_item.searing_exarch_tier = support_->searing;
    context_item.eater_of_worlds_tier = support_->eater;
    // The new relation explicitly covers every Eldritch side/phase. The old
    // support view and anchored production guard retain exact-phase equality.
    return support_->compatible(calc, prices, context_item);
}
std::uint32_t PreparedPhasePotential::projected_cell(const CalcContext& calc, const pc_item_state& item) const {
    const bool framed = in_frame(calc, item, fractured_mod,filter_mods);
    if (!framed && (continuation != PhaseContinuation::CoupledFresh || !in_frame(calc, item, kNoId,filter_mods)))
        throw std::invalid_argument("program exit outside certified probability regions");
    const auto offset = framed ? 0 : 3*support_->values.size()*16+2;
    const auto base = static_cast<std::uint32_t>(offset+index(support_->values.size(), item.rarity, item_mask(calc, item),
        item.prefix_count, item.suffix_count));
    if (retention == PhaseRetention::None) return base;
    const auto crafted = crafted_observation(calc, item);
    if (!crafted.distinct || (crafted.goals & ~crafted_goal_domain))
        throw std::invalid_argument("program exit outside crafted goal domain");
    return coordinate_index_.at(coordinate(base, crafted.goals, crafted.jp, crafted.js, filter_observation(item,filter_mods)));
}
double PreparedPhasePotential::projected_value(const CalcContext& calc, const pc_item_state& item) const {
    return values.at(projected_cell(calc, item));
}
std::optional<double> PreparedPhasePotential::projected_summary_value(unsigned rarity, unsigned mask,
        unsigned p, unsigned s, bool fresh, unsigned crafted, unsigned jp, unsigned js) const {
    if (rarity > PC_RARITY_RARE || p > 3 || s > 3 || jp > 3 || js > 3 ||
        mask >= support_->values.size() || (crafted & ~crafted_goal_domain) ||
        std::popcount(crafted)+jp+js > crafted_count_limit) return std::nullopt;
    const auto offset = fresh ? 3*support_->values.size()*16+2 : 0;
    const auto key = coordinate(static_cast<unsigned>(offset+index(support_->values.size(),rarity,mask,p,s)),crafted,jp,js);
    const auto found = coordinate_index_.find(key);
    return found == coordinate_index_.end() ? std::nullopt : std::optional<double>(values[found->second]);
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
        bool joint_refinement, std::shared_ptr<const PreparedPhasePotential> reuse_draws, PhaseContinuation continuation,
        PhaseRetention retention, bool retain_diagnostics, PhasePreparationOptions preparation_options,
        std::optional<CoupledFractureFrame> frame) {
    const auto preparation_start = PreparationClock::now();
    PhasePreparationStats stats;
    checkpoint(budget);
    if (continuation != PhaseContinuation::PriceOnly && (!joint_refinement || !retain_scour))
        throw std::invalid_argument("native continuation requires joint/no-op and retention coverage");
    if (retention != PhaseRetention::None && continuation != PhaseContinuation::CoupledFresh)
        throw std::invalid_argument("crafted retention requires both checked regions");
    if (preparation_options.filter_modes && (retention != PhaseRetention::AnnulNonempty || preparation_options.filter_modes>3))
        throw std::invalid_argument("filter continuation requires complete crafted/loss/pool coverage");
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
    const bool fresh_source = fracture == kNoId;
    if (frame) {
        if (continuation != PhaseContinuation::CoupledFresh ||
            (!fresh_source && fracture != frame->mod))
            throw std::invalid_argument("coupled frame disagrees with source domain");
        fracture = frame->mod;
    }
    if (!in_frame(calc, anchor, fresh_source ? kNoId : fracture) || calc.session().rare_affix_cap != 3 ||
        calc.layout().slots.size() > 5)
        throw std::invalid_argument("uncovered native probability frame");
    const auto fm = mod_mask(calc, fracture);
    if (!fm) throw std::invalid_argument("phase probability requires the measured fractured goal frame");
    if (calc.session().metamod_type.at(fracture) >= 0 ||
        modifier_is_veiled_template(calc.session(), fracture))
        throw std::invalid_argument("coupled frame requires a natural goal modifier");
    const unsigned fs = calc.session().gen_type[fracture];
    const auto fracture_exclusions=modifier_exclusion_effect_signature(calc.session(),fracture);
    const auto masks = static_cast<std::uint32_t>(support->values.size());
    const auto grid = 3*masks*16;
    const bool coupled = continuation == PhaseContinuation::CoupledFresh;
    const unsigned fresh_offset = grid+2;
    unsigned extent = coupled ? fresh_offset+grid : grid+2;
    const auto maximum_cap = retention == PhaseRetention::None ? 32ull << 20 : 64ull << 20;
    auto cap = std::min<std::uint64_t>(maximum_cap, budget.max_scratch_bytes);
    // Coupling retains both regions' diagnostic rows and event caps while
    // checking. Reserve their overlap before native generation.
    const std::uint64_t native_scratch = (retention != PhaseRetention::None ? 8ull : (coupled ? 6ull : 2ull)) << 20;
    if (cap < (6ull << 20)) throw std::length_error("phase probability reservation refused");
    ScopedProofMemoryCharge scratch(support->store_->ledger(), ProofMemoryCategory::Scratch, native_scratch);
    std::vector<std::array<unsigned, 2>> minimum(masks, {99, 99});
    std::vector<unsigned> mod_goals(calc.session().mod_count);
    std::vector<unsigned> goal_side(calc.layout().slots.size(), 99);
    for (unsigned mod = 0; mod < calc.session().mod_count; ++mod) mod_goals[mod] = mod_mask(calc, mod);
    // Additive support observes only side and the full goal-hit mask. Preserve
    // first occurrence order; native pool weights and loss multiplicities still
    // use the complete modifier population. The frame above has at most 5 goals.
    std::array<std::pair<int, unsigned>, 64> additive_support{};
    std::array<std::uint32_t, 2> additive_seen{};
    unsigned additive_count = 0;
    for (unsigned mod = 0; mod < mod_goals.size(); ++mod) {
        const auto side = calc.session().gen_type[mod];
        if (side < 0 || side > 1) continue;
        const auto hit = mod_goals[mod];
        const auto bit = std::uint32_t{1} << hit;
        if (additive_seen[side] & bit) continue;
        additive_seen[side] |= bit;
        additive_support[additive_count++] = {side, hit};
    }
    unsigned crafted_domain = 0, crafted_limit = 0;
    std::array<std::uint32_t,2> filter_mods{kNoId,kNoId};
    for (const auto& action : calc.registry().actions) if (action.params.type==ActionType::Bench && action.params.mod_id<calc.session().mod_count) {
        const auto mod=action.params.mod_id;
        const auto code=calc.session().metamod_type[mod];
        for (unsigned mode=0;mode<2;++mode) if ((preparation_options.filter_modes&(1u<<mode)) && code>=0 &&
            code==(mode ? calc.session().data->metamod_no_caster_code : calc.session().data->metamod_no_attack_code)) {
            if (filter_mods[mode]!=kNoId && filter_mods[mode]!=mod)
                throw std::invalid_argument("multiple native identities for selected filter");
            if (mod_goals[mod] || calc.session().gen_type[mod]<0 || calc.session().gen_type[mod]>1)
                throw std::invalid_argument("selected filter must occupy one non-goal explicit slot");
            filter_mods[mode]=mod;
        }
    }
    if (retention != PhaseRetention::None) {
        const auto observation = crafted_observation(calc, anchor);
        if (!observation.distinct) throw std::invalid_argument("crafted projection needs distinct satisfying affixes");
        crafted_domain = observation.goals;
        // Explicit initial-domain bound plus native induction, NOT an inference
        // from missing multimod alone. In-frame Bench requires zero existing
        // crafted affixes and creates one. Other in-frame producers create no
        // crafted slots; losses/renewals only preserve or remove them. Multimod
        // setup is still a covered first exit. Inputs beyond this bound fall back.
        crafted_limit = std::max(1u, unsigned(std::popcount(observation.goals))+observation.jp+observation.js);
        for (unsigned mod = 0; mod < mod_goals.size(); ++mod) {
            if (std::popcount(mod_goals[mod]) > 1)
                throw std::invalid_argument("overlapping goals need a richer removal category");
            if (!mod_goals[mod]) continue;
            // Same-goal members must exclude one another. This proves that
            // a goal bit is one eligible affix for loss arithmetic, uniformly.
            const auto exclusion = modifier_exclusion_effect_signature(calc.session(), mod);
            for (unsigned other = 0; other < mod_goals.size(); ++other)
                if ((mod_goals[other] & mod_goals[mod]) && !pc_bitset_test(exclusion.data(), other))
                    throw std::invalid_argument("duplicate satisfying carriers need a richer removal category");
        }
        for (const auto& action : calc.registry().actions)
            if (action.params.type == ActionType::Bench && action.params.mod_id < mod_goals.size() &&
                calc.session().metamod_type[action.params.mod_id] < 0)
                crafted_domain |= mod_goals[action.params.mod_id];
    }
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
            if (feasible) graph_cells.push_back({id, 1, {id}, goal});
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
            if (feasible) graph_cells.push_back({id, 1, {id}, goal});
            if (feasible && !goal) cells.push_back({r, m, p, s, id, false});
        }
    std::vector<std::uint64_t> coordinates;
    std::unordered_map<std::uint64_t, unsigned> coordinate_index;
    for (auto& c : cells) c.base = c.id;
    if (retention != PhaseRetention::None) {
        coordinates.resize(extent, UINT64_MAX);
        const auto old_graph = graph_cells;
        // Keep the old base-cell IDs for zero crafted coordinates. Additional
        // cells are sparse feasible categories, not another physical graph.
        for (const auto& g : old_graph) {
            if (g.cell_id == grid || g.cell_id == grid+1) continue;
            const auto base = g.cell_id;
            const auto local = base >= fresh_offset ? base-fresh_offset : base;
            const unsigned r = local/(masks*16), m = (local/16)%masks, p = (local/4)%4, s = local%4;
            const unsigned allowed = m & crafted_domain & (base >= fresh_offset ? ~0u : ~fm);
            for (unsigned cm = 0; cm < masks; ++cm) if ((cm & ~allowed) == 0)
                for (unsigned jp = 0; jp <= p-minimum[m][0]; ++jp)
                    for (unsigned js = 0; js <= s-minimum[m][1]; ++js) {
                        if (std::popcount(cm)+jp+js > crafted_limit) continue;
                        const auto key = coordinate(base, cm, jp, js);
                        const unsigned id = cm || jp || js ? extent++ : base;
                        if (id >= coordinates.size()) coordinates.resize(id+1, UINT64_MAX);
                        coordinates[id] = key; coordinate_index.emplace(key, id);
                        if (id == base) continue;
                        graph_cells.push_back({id, 1, {id}, g.terminal});
                        if (!g.terminal) cells.push_back({r,m,p,s,id,false,base,cm,jp,js});
                    }
        }
    }
    // A filter is an actual member of the removable crafted-junk count.
    // It neither adds a free slot nor erases the junk-free terminal condition.
    {
    const auto ordinary_cells=cells;
    for (const auto& c:ordinary_cells) for (unsigned mode=0;mode<2;++mode) if (filter_mods[mode]!=kNoId) {
        const unsigned side=calc.session().gen_type[filter_mods[mode]];
        if (!(side ? c.js : c.jp)) continue;
        auto filtered=c; filtered.id=extent++; filtered.filter=mode+1;
        const auto key=coordinate(c.base,c.crafted,c.jp,c.js,filtered.filter);
        coordinates.push_back(key); coordinate_index.emplace(key,filtered.id);
        graph_cells.push_back({filtered.id,1,{filtered.id},false}); cells.push_back(filtered);
    }
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
        const auto value = proposal.values[c.base >= fresh_offset ? c.base-fresh_offset : c.base];
        if (!std::isfinite(value) || value < 0) {
            if (refusal.kind.empty()) refusal = {"numeric_inconclusive", "clean proposal value", c.id};
        } else candidate[c.id] = value;
    }
    std::vector<CalcContext::NativeGoalDrawBound> draws;
    std::map<std::tuple<unsigned, unsigned, bool, unsigned>, unsigned> draw_cache;
    const auto reused_count = reuse_draws ? reuse_draws->draw_count() : 0;
    for (unsigned i = 0; i < reused_count; ++i) {
        const auto& w = reuse_draws->draw(i);
        draw_cache[{w.action, w.slot, w.guaranteed, w.pool_filter_mod}] = i;
    }
    const auto draw_at = [&](unsigned i) -> const CalcContext::NativeGoalDrawBound& {
        return i < reused_count ? reuse_draws->draw(i) : draws.at(i-reused_count);
    };
    const auto get_draw = [&](unsigned a, unsigned slot, bool guaranteed, unsigned filter) -> const auto& {
        const auto key = std::tuple{a, slot, guaranteed, filter};
        const auto found = draw_cache.find(key);
        if (found != draw_cache.end()) return draw_at(found->second);
        checkpoint(budget);
        const auto weight_start = PreparationClock::now();
        auto result = calc.phase_goal_draw_bound(anchor, a, slot, guaranteed, filter);
        stats.native_weight_ns += elapsed_ns(weight_start);
        draw_cache[key] = static_cast<unsigned>(reused_count+draws.size());
        draws.push_back(std::move(result));
        return draws.back();
    };
    bool probability_frame_escape = false;
    std::vector<PhaseNonemptyWitness> nonempty_witnesses;
    std::vector<PhaseRefillWitness> refill_witnesses;
    std::map<std::tuple<unsigned,unsigned,unsigned>,unsigned> refill_cache;
    std::map<std::tuple<unsigned,unsigned,unsigned,int,unsigned>,bool> nonempty_cache;
    const bool uniform_annul = retention == PhaseRetention::Annul || retention == PhaseRetention::AnnulNonempty;
    const bool prove_nonempty = retention == PhaseRetention::CraftedNonempty || retention == PhaseRetention::AnnulNonempty;
    const auto nonempty_add = [&](unsigned a,unsigned p,unsigned s,int phase,unsigned filter) {
        const auto key=std::tuple{a,p,s,phase,filter};
        if (const auto found=nonempty_cache.find(key);found!=nonempty_cache.end()) return found->second;
        for (unsigned slot=0;slot<goal_side.size();++slot) {
            const unsigned side=goal_side[slot];
            if ((phase>=0 && side!=unsigned(phase)) || (side ? s : p)>=3) continue;
            const auto& w=get_draw(a,slot,false,filter);
            if (w.frame_escape) continue;
            auto remaining=w.other_weight;
            for (unsigned blocker_side=0;blocker_side<2;++blocker_side)
                for (unsigned i=0;i<(blocker_side ? s : p);++i)
                    remaining-=std::min(remaining,w.strongest_other_removal[blocker_side][i]);
            // A positive residual of NON-target mass proves an available
            // native draw even if every target modifier has been excluded.
            // Integer totals and every possible source blocker are covered.
            if (remaining) {
                nonempty_witnesses.push_back({a,p,s,draw_cache.at({a,slot,false,filter}),phase,remaining});
                nonempty_cache.emplace(key,true); return true;
            }
        }
        nonempty_cache.emplace(key,false); return false;
    };
    const auto refill_minimum = [&](unsigned a, unsigned p, unsigned s) {
        const auto key=std::tuple{a,p,s};
        if (const auto found=refill_cache.find(key);found!=refill_cache.end()) return found->second;
        const auto facts=action_transition_facts(calc.registry().actions[a].params.type);
        // Alchemy has no forced draw, tag-changing affix or metamod in frame.
        // Its natural Rare pool and insertion rules match get_draw. Every
        // previous draw is a blocker in nonempty_add, including hidden members;
        // its residual excludes target mass and never assumes a target survives.
        const auto result=phase_refill_minimum(p,s,facts.minimum_refill_target,
            [&](unsigned pp,unsigned ss) { return nonempty_add(a,pp,ss,-1,kNoId); });
        refill_witnesses.push_back({a,p,s,facts.minimum_refill_target,result});
        refill_cache.emplace(key,result); return result;
    };
    const auto upper = [&](unsigned a, unsigned slot, unsigned p, unsigned s, unsigned count, unsigned filter) {
        const auto& natural = get_draw(a, slot, false,filter);
        probability_frame_escape |= natural.frame_escape;
        double u = draw_upper(natural, p, s);
        const auto type = calc.registry().actions[a].params.type;
        if (type == ActionType::HarvestReforge || type == ActionType::HarvestAugment) {
            const auto& targeted = get_draw(a, slot, true,filter);
            probability_frame_escape |= targeted.frame_escape;
            u = std::max(u, draw_upper(targeted, p, s));
        }
        return u == 0 || count == 0 ? 0 : std::min(1.0, std::nextafter(count*u, std::numeric_limits<double>::infinity()));
    };
    std::vector<PhaseJointEventWitness> joint_events;
    std::map<std::tuple<unsigned, unsigned, unsigned, unsigned>, unsigned> joint_cache;
    const auto joint_upper = [&](unsigned a, unsigned subset, unsigned side,
            unsigned forced, bool has_forced, unsigned output_limit, unsigned frame_mask, unsigned frame_side, unsigned filter) {
        // Same-side event only. Native hit masks must require DISTINCT draws;
        // a multi-goal modifier refuses this shortcut, leaving marginal caps.
        for (auto hit : mod_goals) if (std::popcount(hit & subset) > 1) return 1.0;
        const auto key = std::tuple{a, subset, frame_mask, filter};
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
                const auto& natural = get_draw(a, slot, false,filter);
                probability_frame_escape |= natural.frame_escape;
                position[j] = draw_upper(natural, side ? 3 : same, side ? same : 3);
                if (type == ActionType::HarvestReforge) {
                    const auto& guaranteed = get_draw(a, slot, true,filter);
                    probability_frame_escape |= guaranteed.frame_escape;
                    position[j] = std::max(position[j], draw_upper(guaranteed, side ? 3 : same, side ? same : 3));
                }
            }
            w.conditional.push_back(position);
            w.natural_draws.push_back(draw_cache.at({a, slot, false,filter}));
            if (type == ActionType::HarvestReforge) w.guaranteed_draws.push_back(draw_cache.at({a, slot, true,filter}));
            w.marginal_upper = std::min(w.marginal_upper, upper(a, slot, side ? 3 : output_limit-1,
                side ? output_limit-1 : 3, output_limit, filter));
        }
        if (probability_frame_escape) return 1.0;
        w.joint_upper = phase_joint_assignment_upper(w.conditional, w.positions);
        w.capacity = capacity(w.joint_upper);
        joint_cache[key] = static_cast<unsigned>(joint_events.size());
        joint_events.push_back(std::move(w));
        return joint_events.back().joint_upper;
    };
    // At most two rarities x two regions plus one Alchemy occupancy cutoff.
    // Reserve bounded geometry storage before generating any template. This
    // cache lives only inside this compatible preparation; it contains neither
    // event probabilities nor selected minima, allocations or scalar values.
    constexpr std::uint64_t geometry_reservation=64ull<<10;
    ScopedProofMemoryCharge geometry_charge(support->store_->ledger(),ProofMemoryCategory::Scratch,
        preparation_options.reuse_renewal_support ? geometry_reservation : 0);
    std::map<std::tuple<unsigned,unsigned,unsigned>,std::vector<Group>> renewal_geometry;
    const auto cap_entries=preparation_options.reuse_renewal_support ? 2ull*masks*calc.registry().actions.size() : 0;
    // Exact-mask caps depend on native integer weights, action/forced draws
    // and retained region; never on the current value vector. Keep their
    // frame-refusal bit as well. Eldritch side-dependent events are excluded.
    if (cap_entries*sizeof(std::uint32_t)>cap/16) throw std::length_error("renewal capacity workspace exceeds bounded reservation");
    ScopedProofMemoryCharge capacity_charge(support->store_->ledger(),ProofMemoryCategory::Scratch,
        cap_entries*2*sizeof(std::uint32_t));
    std::vector<std::uint32_t> renewal_capacities(cap_entries,UINT32_MAX);
    stats.event_cap_bytes=renewal_capacities.capacity()*sizeof(std::uint32_t);
    if (stats.event_cap_bytes>cap_entries*2*sizeof(std::uint32_t)) throw std::length_error("unexpected renewal capacity allocation");
    // This dense array never grows. Release the preallocation overlap once
    // its actual capacity is known; no generation or coverage is omitted.
    capacity_charge.reset();
    capacity_charge=ScopedProofMemoryCharge(support->store_->ledger(),ProofMemoryCategory::Scratch,stats.event_cap_bytes);
    std::vector<PhasePotentialRelation> final_relations;
    std::set<std::pair<unsigned, unsigned>> detailed;
    std::vector<PhasePriceReactivation> reactivations;
    std::uint64_t combined_peak = support->memory_snapshot().peak_total_bytes, action_relations = 0;
    unsigned rounds = 0;
    bool exporting = false;
    unsigned accepted_rounds = 0;
    std::uint64_t accepted_relation_count = 0, accepted_report_bytes = 0, export_reservation = 0;
    const unsigned max_rounds=retention==PhaseRetention::None ? 32 : 64;
    // Repeated price-only rounds cost more than building the retained native
    // relations once. Keep the legacy non-retention/control models separate;
    // full retention still re-minimizes every relation at the final vector.
    const bool eager_relations = joint_refinement && retention != PhaseRetention::None;
    unsigned anchor_cell = static_cast<unsigned>((fresh_source ? fresh_offset : 0)+index(masks, anchor.rarity,
        item_mask(calc, anchor), anchor.prefix_count, anchor.suffix_count));
    if (retention != PhaseRetention::None) {
        const auto crafted = crafted_observation(calc, anchor);
        anchor_cell = coordinate_index.at(coordinate(anchor_cell, crafted.goals,
            crafted.jp, crafted.js, filter_observation(anchor, filter_mods)));
    }
    bool last_checked=false; double last_improvement=0;
    stats.projection_ns = elapsed_ns(preparation_start);
    for (; rounds < max_rounds || exporting; ++rounds) {
        const auto relation_start = PreparationClock::now();
        checkpoint(budget);
        if (renewal_geometry.size()==5) {
            // All supported ordinary-renewal geometries are installed; this
            // cache now only copies them. Its measured capacity is fixed.
            geometry_charge.reset();
            geometry_charge=ScopedProofMemoryCharge(support->store_->ledger(),ProofMemoryCategory::Scratch,stats.geometry_bytes);
        }
        if (exporting) {
            // The final vector already passed complete native and quotient
            // checking. Reconstruct its diagnostics with NO numerical graph
            // alive. Large reports never overlap construction/solve storage.
            scratch.reset();
            const auto available=cap-support->memory_snapshot().total_bytes;
            if (available<(2ull<<20)) throw std::length_error("final native report reservation refused");
            export_reservation=native_scratch+accepted_report_bytes;
            if (export_reservation>available-(1ull<<20)) throw std::length_error("final native report reservation exceeds matched cap");
            scratch=ScopedProofMemoryCharge(support->store_->ledger(),ProofMemoryCategory::Scratch,export_reservation);
            combined_peak=std::max(combined_peak,support->memory_snapshot().total_bytes);
        }
        if (support->memory_snapshot().total_bytes >= cap)
            throw std::length_error("phase live evidence exhausts additional reservation");
        const auto graph_cap = cap-support->memory_snapshot().total_bytes;
        QuotientBellmanGraph graph(graph_cap, QuotientBellmanMode::LowerOnly, cap);
        if (!exporting) graph.install_cells(graph_cells);
        QuotientLowerQuery query;
        query.request_identity = {version}; query.caller_scope = {version};
        query.coefficients = LowerCoefficientModel::ExactBinaryModel;
        auto boundary = restart_boundary;
        boundary.cell_id = grid+1;
        // The numerical model has a short local identity; full native context
        // and the existing boundary producer are bound by the returned view.
        boundary.evidence_identity = {version, 0x4652455348};
        if (!coupled) query.boundaries.push_back(std::move(boundary));
        query.roots = {anchor_cell};
        std::vector<PhasePotentialRelation> relations;
        std::uint64_t report_bytes=0;
        if (exporting) {
            if (accepted_relation_count*sizeof(PhasePotentialRelation)+native_scratch>export_reservation)
                throw std::length_error("final native report storage exceeds reservation");
            relations.reserve(accepted_relation_count);
            report_bytes=relations.capacity()*sizeof(PhasePotentialRelation);
        }
        std::uint64_t relation_count=0;
        std::uint64_t relation_payload_bytes=0;
        std::vector<double> minimum_rhs(extent, std::numeric_limits<double>::infinity());
        PhaseProposalRefusal first_violation;
        const auto keep_record = [&](PhasePotentialRelation record) {
            ++relation_count;
            relation_payload_bytes+=sizeof(PhasePotentialRelation)+record.targets.capacity()*4+
                record.probabilities.capacity()*8+record.events.capacity()*sizeof(PhasePotentialRelation::Event);
            minimum_rhs[record.cell]=std::min(minimum_rhs[record.cell],record.rhs);
            if (rounds==0 && first_violation.kind.empty()) {
                double upper=0;
                for (unsigned i=0;i<record.targets.size();++i) {
                    const auto term=std::nextafter(record.probabilities[i]*candidate[record.targets[i]],std::numeric_limits<double>::infinity());
                    upper=std::nextafter(upper+term,std::numeric_limits<double>::infinity());
                }
                if (candidate[record.cell]>std::nextafter(record.cost+upper,std::numeric_limits<double>::infinity()))
                    first_violation={"violated_inequality","clean completion exceeds outward upper of frozen native-mask RHS",
                        record.cell,record.action,UINT32_MAX,candidate[record.cell],record.cost,upper,record.targets,record.probabilities};
            }
            if (exporting) {
                report_bytes+=record.targets.capacity()*4+record.probabilities.capacity()*8+
                    record.events.capacity()*sizeof(PhasePotentialRelation::Event);
                if (relation_count>accepted_relation_count || report_bytes+native_scratch>export_reservation)
                    throw std::length_error("final native report payload exceeds reservation");
                relations.push_back(std::move(record));
            }
        };
        std::vector<PhasePriceReactivation> shortcuts;
        for (const auto& c : cells) {
            checkpoint(budget);
            const unsigned offset = c.base >= fresh_offset ? fresh_offset : 0;
            const unsigned frame_mask = offset ? 0 : fm, frame_side = offset ? 2 : fs;
            QuotientLowerSource source{c.id, {c.id}, {query.caller_scope, 1, true, {}, {}}, {}};
            CanonicalActionSet native_scope{{version, c.id}, 1, true, {}, {}};
            std::vector<CanonicalActionCover> native_cover;
            struct Row { double cost; unsigned action; std::vector<Group> groups; bool probability, price; PhaseRelationReason reason;
                std::vector<PhasePotentialRelation::Event> events; int phase_branch; unsigned removable; };
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
                unsigned setup_filter=0;
                if (type==ActionType::Bench) for (unsigned mode=0;mode<2;++mode)
                    if (filter_mods[mode]!=kNoId && action.params.mod_id==filter_mods[mode]) setup_filter=mode+1;
                bool special_escape = false;
                if (type == ActionType::Bench && action.params.mod_id < calc.session().mod_count)
                    special_escape = !setup_filter && calc.session().metamod_type[action.params.mod_id] >= 0;
                if (type == ActionType::Fossil) for (auto fossil : action.params.fossil_indices) {
                    const auto& data = *calc.session().data;
                    special_escape |= (fossil < data.fossil_mirrors.size() && data.fossil_mirrors[fossil]) ||
                        data.string_at(data.fossil_name_sids.at(fossil)) == "Bloodstained Fossil";
                    for (auto mod : calc.session().fossil_forced_mod_ids.at(fossil))
                        special_escape |= calc.session().metamod_type.at(mod) >= 0;
                }
                if (type == ActionType::Unveil || (type == ActionType::Fracture && frame_mask)) continue; // exact frame predicates
                const auto lim = c.rarity == PC_RARITY_MAGIC ? 1u : 3u;
                // do_bench checks crafted capacity before add_direct_mod checks
                // side capacity. Multimod is a different, retained first exit.
                // Hidden group conflicts may make more members inapplicable;
                // those keep the no-op alternative below.
                if (type==ActionType::Bench && retention!=PhaseRetention::None) {
                    const auto mod=action.params.mod_id;
                    if (mod>=calc.session().mod_count || !(calc.session().flags[mod]&(1<<1)) ||
                        std::find(calc.session().bench_mod_ids.begin(),calc.session().bench_mod_ids.end(),mod)==calc.session().bench_mod_ids.end()) continue;
                    const auto side=calc.session().gen_type[mod];
                    const unsigned crafts=std::popcount(c.crafted)+c.jp+c.js;
                    const bool multimod=calc.session().metamod_type[mod]>=0 &&
                        calc.session().metamod_type[mod]==calc.session().data->metamod_multimod_code;
                    if (side<0 || side>1 || (side ? c.s : c.p)>=lim ||
                        (multimod ? crafts>=3 : crafts>0)) continue;
                    if (frame_mask && pc_bitset_test(fracture_exclusions.data(),mod)) continue;
                }
                if (action.legality.requires_open_affix && c.p == lim && c.s == lim) continue;
                if (action.legality.min_total_affixes > c.p+c.s) continue;
                // Phase is hidden in this uniform view. Cover each legal native
                // branch by a separate optimistic choice, never average phases.
                const bool split_phase = (type == ActionType::EldritchChaos &&
                    continuation != PhaseContinuation::PriceOnly) || (prove_nonempty && type == ActionType::EldritchExalt) ||
                    (uniform_annul && type == ActionType::EldritchAnnul);
                for (int phase = -1; phase < (split_phase ? 2 : 0); ++phase) {
                    const auto support_start = PreparationClock::now();
                    const auto facts = action_transition_facts(type);
                    const bool applied_reforge = facts.applied_rarity != 255;
                    const unsigned source_filter=c.filter ? filter_mods[c.filter-1] : kNoId;
                    const unsigned filter_side=c.filter ? calc.session().gen_type[source_filter] : 2;
                    // Native ordinary renewal clears removable filters before
                    // requesting a pool. Eldritch's opposite side survives.
                    const unsigned pool_filter=facts.renewal &&
                        !(type==ActionType::EldritchChaos && phase>=0 && unsigned(phase)!=filter_side)
                        ? kNoId : source_filter;
                    std::vector<Group> groups;
                    const bool loss_law = uniform_annul && (type == ActionType::Annul || type == ActionType::EldritchAnnul);
                    unsigned removable = 0;
                    bool price_escape = false;
                    auto reason = PhaseRelationReason::NativeEffect;
                    const auto escape = [&](PhaseRelationReason why = PhaseRelationReason::UnsupportedEffect) {
                        groups = {{0, mass, {grid}}}; price_escape = true; reason = why;
                    };
                    const auto add_typed_group = [&](unsigned m, unsigned p, unsigned s, unsigned r,
                            unsigned cm, unsigned jp, unsigned js, unsigned filter=0) {
                        if (p > 3 || s > 3 || p < minimum[m][0] || s < minimum[m][1] || (m & frame_mask) != frame_mask) return;
                        const unsigned limit = r == PC_RARITY_MAGIC ? 1 : (r == PC_RARITY_RARE ? 3 : 0);
                        if (p > limit || s > limit) return;
                        auto target = static_cast<std::uint32_t>(offset+index(masks, r, m, p, s));
                        if (retention != PhaseRetention::None) {
                            const auto found = coordinate_index.find(coordinate(target, cm, jp, js,filter));
                            if (found == coordinate_index.end()) return; // proved infeasible category
                            target = found->second;
                        }
                        auto found = std::find_if(groups.begin(), groups.end(), [&](const auto& g) { return g.mask == m; });
                        if (found == groups.end() || loss_law) { groups.push_back({m, mass, {}}); found = std::prev(groups.end()); }
                        found->cells.push_back(target);
                    };
                    const auto add_group = [&](unsigned m, unsigned p, unsigned s, unsigned r) {
                        add_typed_group(m,p,s,r,c.crafted,c.jp,c.js,c.filter);
                    };
                    bool probabilistic = false, renewal = false;
                    unsigned preserved = c.mask, draws_per_side = 1;
                    if (action.synthetic) {
                        groups = {{0, mass, {coupled ? fresh_offset : grid+1}}};
                        reason = coupled ? PhaseRelationReason::NativeEffect : PhaseRelationReason::FixedIndependentBoundary;
                    } else if (type == ActionType::Fracture || special_escape || (!setup_filter && (action.sets_flags & kProtectionFlags)) ||
                        type == ActionType::InfluenceExalt || type == ActionType::VeiledExalt || type == ActionType::VeiledChaos) {
                        escape(PhaseRelationReason::NativeDomainEscape);
                    } else if (!eager_relations && cost >= candidate[c.id] && !detailed.contains({c.id, a})) {
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
                        else add_typed_group(frame_mask, frame_side == 0, frame_side == 1,
                            frame_mask ? PC_RARITY_MAGIC : PC_RARITY_NORMAL, 0,0,0);
                    } else if (type == ActionType::Annul || type == ActionType::EldritchAnnul ||
                               type == ActionType::RemoveCraftedModifiers) {
                        if (retention == PhaseRetention::None) {
                            // Explicit predecessor control: favorable deletion.
                            for (unsigned m = 0; m < masks; ++m) if ((m | c.mask) == c.mask)
                                for (unsigned p = 0; p <= c.p; ++p) for (unsigned s = 0; s <= c.s; ++s)
                                    if (type == ActionType::RemoveCraftedModifiers || p+s+1 == c.p+c.s)
                                        add_group(m, p, s, c.rarity);
                            add_group(c.mask, c.p, c.s, c.rarity);
                        } else if (type == ActionType::RemoveCraftedModifiers) {
                            unsigned lost_p = c.jp, lost_s = c.js;
                            for (unsigned slot = 0; slot < goal_side.size(); ++slot)
                                if (c.crafted & (1u << slot)) ++(goal_side[slot] ? lost_s : lost_p);
                            // Native cleanup removes all and only unfractured
                            // crafted affixes, including their satisfying goals.
                            add_typed_group(c.mask & ~c.crafted, c.p-lost_p, c.s-lost_s, c.rarity, 0,0,0);
                        } else {
                            for (unsigned slot = 0; slot < goal_side.size(); ++slot) {
                                const auto bit = 1u << slot;
                                if (!(c.mask & bit) || (frame_mask & bit)) continue;
                                const auto side = goal_side[slot];
                                if (loss_law && phase>=0 && side!=static_cast<unsigned>(phase)) continue;
                                add_typed_group(c.mask & ~bit, c.p-(side == 0), c.s-(side == 1),
                                    c.rarity, c.crafted & ~bit, c.jp, c.js,c.filter);
                            }
                            for (unsigned side = 0; side < 2; ++side) {
                                if (loss_law && phase>=0 && side!=static_cast<unsigned>(phase)) continue;
                                const unsigned junk = (side ? c.s : c.p)-minimum[c.mask][side];
                                const unsigned crafted = side ? c.js : c.jp;
                                for (unsigned j=0; j<(loss_law ? junk-crafted : unsigned(junk>crafted)); ++j)
                                    add_group(c.mask, c.p-(side == 0), c.s-(side == 1), c.rarity);
                                const unsigned filtered=c.filter && side==filter_side;
                                for (unsigned j=0; j<(loss_law ? crafted-filtered : unsigned(crafted>filtered)); ++j) add_typed_group(c.mask, c.p-(side == 0), c.s-(side == 1),
                                    c.rarity, c.crafted, c.jp-(side == 0), c.js-(side == 1),c.filter);
                                if (filtered) add_typed_group(c.mask,c.p-(side==0),c.s-(side==1),
                                    c.rarity,c.crafted,c.jp-(side==0),c.js-(side==1),0);
                            }
                            if (loss_law) {
                                // Native do_annul / exact_outcomes: next_below(N)
                                // chooses uniformly among nonfractured affixes on
                                // the selected side(s). In-frame metamod locks are
                                // absent. Distinct, mutually excluding goal bits
                                // are single affixes; each junk occurrence is one
                                // further category, including crafted junk.
                                removable = (phase<0 ? c.p+c.s : (phase==0 ? c.p : c.s)) -
                                    unsigned(frame_mask && (phase<0 || frame_side==static_cast<unsigned>(phase)));
                                if (groups.size()!=removable) throw std::invalid_argument("incomplete native Annul categories");
                                if (removable) for (auto& g:groups) g.capacity=(mass+removable-1)/removable;
                                probabilistic = removable>0;
                            }
                            if (groups.empty()) add_group(c.mask,c.p,c.s,c.rarity);
                        }
                    } else if (type == ActionType::Bench) {
                        if (action.params.mod_id >= mod_goals.size()) { escape(); }
                        else {
                            const auto side = calc.session().gen_type[action.params.mod_id];
                            const auto hit = mod_goals[action.params.mod_id];
                            if (retention == PhaseRetention::None) add_group(c.mask | hit, c.p+(side == 0), c.s+(side == 1), c.rarity);
                            else if (!(hit & c.mask) && !c.crafted && !c.jp && !c.js) {
                                // In-frame filters occupy crafted capacity and
                                // forbid another bench; fractured crafted flags
                                // may further forbid it, so allowing it is weaker.
                                add_typed_group(c.mask | hit, c.p+(side == 0), c.s+(side == 1), c.rarity,
                                    c.crafted | hit, c.jp+(!hit && side == 0), c.js+(!hit && side == 1),setup_filter);
                            }
                            add_group(c.mask, c.p, c.s, c.rarity);
                        }
                    } else if (type == ActionType::Augment || type == ActionType::Regal ||
                               type == ActionType::Exalt || type == ActionType::EldritchExalt) {
                        probabilistic = true;
                        const auto r = type == ActionType::Regal ? PC_RARITY_RARE : c.rarity;
                        for (unsigned signature = 0; signature < additive_count; ++signature) {
                            const auto [side, hit] = additive_support[signature];
                            if (prove_nonempty && type == ActionType::EldritchExalt && phase>=0 && side!=phase) continue;
                            if (retention != PhaseRetention::None && (hit & c.mask)) continue;
                            add_group(c.mask | hit, c.p+(side == 0), c.s+(side == 1), r);
                        }
                        const bool certainly_adds = prove_nonempty &&
                            (type == ActionType::Exalt || type == ActionType::EldritchExalt) && nonempty_add(a,c.p,c.s,phase,pool_filter);
                        if (!certainly_adds) add_group(c.mask, c.p, c.s, r); // preserve genuine/unknown empty-pool calls
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
                                        add_typed_group(m,p,s,r,c.crafted & unchanged_mask,
                                            phase == 1 ? c.jp : 0, phase == 0 ? c.js : 0,
                                            pool_filter==kNoId ? 0 : c.filter);
                                    }
                                }
                                add_group(c.mask, c.p, c.s, c.rarity);
                            }
                        } else {
                            const auto minimum_total = applied_reforge && preparation_options.minimum_reforge_occupancy ?
                                refill_minimum(a,frame_side==0,frame_side==1) : 0;
                            const auto geometry_key=std::tuple{offset,unsigned(r),minimum_total};
                            const auto cached=renewal_geometry.find(geometry_key);
                            if (preparation_options.reuse_renewal_support && cached!=renewal_geometry.end()) {
                                groups=cached->second; ++stats.geometry_hits;
                            } else {
                                for (unsigned m = 0; m < masks; ++m)
                                    for (unsigned p = 0; p <= draws_per_side; ++p)
                                        for (unsigned s = 0; s <= draws_per_side; ++s)
                                            if (p+s >= minimum_total) add_typed_group(m,p,s,r,0,0,0);
                                if (preparation_options.reuse_renewal_support) {
                                    // Group insertion uses only region, native rarity/capacity,
                                    // overlap-aware feasibility and zero removable crafts.
                                    // Caller source, forced events and no-op alternatives
                                    // are deliberately applied AFTER taking this copy.
                                    if (renewal_geometry.size()>=5) throw std::length_error("renewal geometry scope exceeds reservation");
                                    auto& saved=renewal_geometry[geometry_key]; saved=groups;
                                    std::uint64_t bytes=128+saved.capacity()*sizeof(Group);
                                    for (const auto& g:saved) bytes+=g.cells.capacity()*sizeof(std::uint32_t);
                                    stats.geometry_bytes+=bytes; ++stats.geometry_templates;
                                    if (stats.geometry_bytes>geometry_reservation) throw std::length_error("renewal geometry exceeds reservation");
                                }
                            }
                            // Refill exhaustion does not undo an applied rarity change.
                            // Unknown application contracts retain their old relaxation.
                            if (!applied_reforge) add_group(c.mask, c.p, c.s, c.rarity);
                        }
                    } else { escape(); }
                    if (groups.empty()) { stats.support_ns += elapsed_ns(support_start); continue; } // impossible effect in the declared frame
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
                        if (!probabilistic || loss_law) continue;
                        const bool reuse_cap=preparation_options.reuse_renewal_support && renewal && type!=ActionType::EldritchChaos;
                        const auto cap_index=((offset ? calc.registry().actions.size() : 0)+a)*masks+g.mask;
                        if (reuse_cap && renewal_capacities[cap_index]!=UINT32_MAX) {
                            const auto entry=renewal_capacities[cap_index];
                            g.capacity=entry & ((1u<<25)-1);
                            probability_frame_escape |= bool(entry & (1u<<31));
                            ++stats.event_cap_hits;
                        } else {
                        const bool earlier_escape=probability_frame_escape;
                        probability_frame_escape=false;
                        double event_upper = 1;
                        for (unsigned slot = 0; slot < goal_side.size(); ++slot) {
                            const auto bit = 1u << slot;
                            if (!(g.mask & bit) || (preserved & bit)) continue;
                            if (forced & bit) continue;
                            const unsigned side = goal_side[slot];
                            const unsigned p = renewal ? 3-(side == 0) : c.p;
                            const unsigned s = renewal ? 3-(side == 1) : c.s;
                            // One retained fracture occupies one output slot,
                            // even if its modifier hits several goal bits. The
                            // existing history-uniform draw upper also covers
                            // Harvest's guaranteed draw within those slots.
                            const unsigned draw_count = draws_per_side -
                                unsigned(renewal && type != ActionType::EldritchChaos && frame_side == side);
                            const auto u = (side ? s : p) >= (renewal ? 3u : (type == ActionType::Regal ? 3u : lim))
                                ? 0 : upper(a, slot, p, s, draw_count,pool_filter);
                            event_upper = std::min(event_upper, u);
                        }
                        if (joint_refinement && renewal) {
                            for (unsigned side = 0; side < 2; ++side) {
                                unsigned subset = 0;
                                for (unsigned slot = 0; slot < goal_side.size(); ++slot)
                                    if (goal_side[slot] == side && (g.mask & ~preserved & ~forced & (1u << slot))) subset |= 1u << slot;
                                if (std::popcount(subset) >= 2)
                                    event_upper = std::min(event_upper, joint_upper(a, subset, side, forced, has_forced, draws_per_side, frame_mask, frame_side,pool_filter));
                            }
                        }
                        // One exact-mask event entails every one of its new goals.
                        // min marginal bounds is valid under ANY joint dependence.
                        g.capacity = capacity(event_upper);
                        if (reuse_cap) renewal_capacities[cap_index]=g.capacity |
                            (probability_frame_escape ? 1u<<31 : 0);
                        probability_frame_escape |= earlier_escape;
                        }
                        // Reused caps contain no observed choice. Rebuild the
                        // legal/unknown no-op alternative on every source and
                        // every candidate; applied Alchemy cannot roll back.
                        if (joint_refinement && renewal && !applied_reforge) {
                            g.cells.push_back(c.id);
                            std::sort(g.cells.begin(),g.cells.end());
                            g.cells.erase(std::unique(g.cells.begin(),g.cells.end()),g.cells.end());
                        }
                    }
                    if (probability_frame_escape) escape(PhaseRelationReason::NativeDomainEscape);
                    if (probabilistic && !price_escape) reason = PhaseRelationReason::ProbabilityEnvelope;
                    stats.support_ns += elapsed_ns(support_start);
                    const auto allocation_start = PreparationClock::now();
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
                    if (found == rows.end()) rows.emplace(std::move(key), Row{cost, a, std::move(selected), probabilistic && !price_escape, price_escape, reason, std::move(events), split_phase ? phase : -2, removable});
                    else if (cost < found->second.cost) found->second = {cost, a, std::move(selected), probabilistic && !price_escape, price_escape, reason, std::move(events), split_phase ? phase : -2, removable};
                    stats.allocation_ns += elapsed_ns(allocation_start);
                }
            } // complete registry actions and native phase branches
            const auto coverage_start = PreparationClock::now();
            const auto coverage = validate_canonical_action_coverage(native_scope, native_cover);
            stats.coverage_ns += elapsed_ns(coverage_start);
            const auto quotient_rows_start = PreparationClock::now();
            if (!coverage.empty()) throw std::invalid_argument(coverage);
            unsigned n = 0;
            for (const auto& [pattern, row] : rows) {
                (void)pattern;
                // Request/source/revision are already separate provenance fields.
                // These local IDs need not duplicate those coordinates per row.
                const StableKey action_key{n++}, evidence{row.action};
                QuotientBellmanRowInput input;
                input.source_cell_id = c.id; input.operator_index = row.action; input.cost = row.cost;
                PhasePotentialRelation record{c.id, row.action, row.cost, row.cost, {}, {}};
                record.probability_aware = row.probability; record.independent_price = row.price;
                record.reason = row.reason; record.phase_branch = row.phase_branch; record.removable_affixes = row.removable;
                record.events = row.events;
                for (const auto& group : row.groups) {
                    const double p = std::ldexp(static_cast<double>(group.capacity), -24);
                    const auto target = *std::min_element(group.cells.begin(), group.cells.end(),
                        [&](auto l, auto r) { return candidate[l] < candidate[r]; });
                    // These rows are witnesses for THIS frozen vector. Retain
                    // its minimum only; every candidate change reconstructs all
                    // event/occupancy/self minima before simultaneous checking.
                    // The selected row is not an all-vector native kernel.
                    // The observed minimum has already been selected for this
                    // frozen vector. A singleton choice is an ordinary weighted
                    // transition; reuse the quotient's shared transition spans.
                    input.transitions.push_back({{}, target, p});
                    record.targets.push_back(target); record.probabilities.push_back(p);
                    record.rhs = down(record.rhs+down(p*candidate[target]));
                }
                if (exporting) { keep_record(std::move(record)); continue; }
                // h <= nonnegative cost + h is an identity for EVERY vector.
                // Keep its complete native/event diagnostic (and reselect its
                // value-dependent minima next round), but allocate no duplicate
                // numerical row/query payload for this tautology.
                if (record.targets.size()==1 && record.targets[0]==c.id && record.probabilities[0]==1.0) {
                    keep_record(std::move(record)); continue;
                }
                source.expected_actions.actions.push_back(action_key);
                if (row.price) {
                    // Existing independent immediate-price constraint. No
                    // stochastic kernel is needed for the zero continuation.
                    source.constraints.push_back({{action_key,false,{}},LowerConstraintKind::Scalar,
                        0,row.cost,evidence,LowerEvidenceKind::IndependentLower});
                    keep_record(std::move(record)); continue;
                }
                input.lower_provenance = QuotientLowerRowProvenance{query.request_identity,
                    source.source_identity, action_key, evidence, LowerEvidenceKind::ExactDeclaredKernel};
                // Keep the potentially throwing arena reservation outside
                // aggregate temporary construction (including unwind cleanup).
                std::uint64_t row_id;
                try { row_id = graph.append_row(std::move(input)); }
                catch (const ProofMemoryLimit& error) { throw std::length_error(std::string(error.what())+
                    "; round="+std::to_string(rounds)+" source="+std::to_string(c.id)+" rows="+std::to_string(relation_count)); }
                catch (const std::length_error& error) { throw std::length_error(std::string(error.what())+
                    "; round="+std::to_string(rounds)+" source="+std::to_string(c.id)+" rows="+std::to_string(relation_count)); }
                source.constraints.push_back({{action_key, false, {}}, LowerConstraintKind::Row,
                    row_id, 0, evidence, LowerEvidenceKind::ExactDeclaredKernel});
                keep_record(std::move(record));
            }
            if (!exporting) query.sources.push_back(std::move(source));
            stats.quotient_rows_ns += elapsed_ns(quotient_rows_start);
        }
        stats.relation_ns += elapsed_ns(relation_start);
        if (exporting) {
            if (relation_count!=accepted_relation_count) throw std::logic_error("final native report scope changed");
            stats.diagnostic_export_ns=elapsed_ns(relation_start);
            final_relations=std::move(relations); break;
        }
        query.model_revision = graph.model_revision();
        auto local_budget = budget; local_budget.max_scratch_bytes = graph_cap;
        local_budget.retain_ranked_constraints = false; // this owner retains its native relation diagnostics
        std::vector<double> dense_candidate;
        dense_candidate.reserve(graph_cells.size());
        for (const auto& cell : graph_cells) dense_candidate.push_back(candidate[cell.cell_id]);
        const auto resource_context = [&](const std::exception& e) {
            return std::string(e.what())+"; cells="+std::to_string(cells.size())+
                " rows="+std::to_string(relation_count)+" round="+std::to_string(rounds);
        };
        QuotientLowerResult checked;
        const auto check_start = PreparationClock::now();
        try { checked = graph.check_lower(query, dense_candidate, local_budget); }
        catch (const std::length_error& e) { throw std::length_error(resource_context(e)); }
        combined_peak = std::max(combined_peak, graph.proof_store()->ledger().snapshot().peak_total_bytes + support->memory_snapshot().total_bytes);
        stats.check_ns += elapsed_ns(check_start);
        if (checked.checked) ++stats.candidate_pass_rounds;
        else ++stats.candidate_fail_rounds;
        if (rounds == 0 && !checked.checked && refusal.kind.empty())
            refusal=first_violation.kind.empty() ? PhaseProposalRefusal{"numeric_inconclusive",checked.reason} : std::move(first_violation);
        bool reactivated = false;
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
            accepted_rounds=rounds+1;
            accepted_relation_count=relation_count;
            accepted_report_bytes=relation_payload_bytes;
            if (retain_diagnostics) { exporting=true; continue; }
            break;
        }
        if (reactivated) continue;
        if (checked.checked) {
            stats.checked_source_lowers[rounds] = candidate[anchor_cell];
            stats.checked_source_ns[rounds] = elapsed_ns(preparation_start);
        }
        if (checked.checked && preparation_options.accept_checked_subsolution &&
            stats.eligible_solve_rounds + stats.refused_solve_rounds > 0 &&
            candidate[anchor_cell] >= preparation_options.minimum_checked_source_lower) {
            // Every native minimum was rebuilt at this candidate and checked.
            // This endpoint promises a subsolution, not refinement optimality;
            // never publish the repaired vector against the old frozen rows.
            accepted_rounds=rounds+1;
            accepted_relation_count=relation_count;
            accepted_report_bytes=relation_payload_bytes;
            stats.accepted_early_subsolution=true;
            if (retain_diagnostics) { exporting=true; continue; }
            break;
        }
        const bool candidate_checked = static_cast<bool>(checked.checked);
        last_checked=candidate_checked; last_improvement=0;
        // The feasibility report/certificate is no longer queried. Keeping
        // its ranked constraints alive while solving duplicates proof scratch.
        auto initializer = preparation_options.checked_numerical_reuse ? std::move(checked.checked) : nullptr;
        checked = {};
        std::optional<QuotientLowerProposal> numerical_proposal;
        if (!initializer && preparation_options.untrusted_numerical_reuse)
            numerical_proposal.emplace(QuotientLowerProposal{query.request_identity,query.caller_scope,
                query.model_revision,graph.proof_store()->price_generation(),query.coefficients,&graph_cells,&dense_candidate});
        QuotientLowerResult repaired;
        const auto solve_start = PreparationClock::now();
        try { repaired = graph.solve_lower(query, local_budget, std::move(initializer), numerical_proposal ? &*numerical_proposal : nullptr); }
        catch (const std::length_error& e) { throw std::length_error(resource_context(e)); }
        combined_peak = std::max(combined_peak, graph.proof_store()->ledger().snapshot().peak_total_bytes + support->memory_snapshot().total_bytes);
        const auto numerical_ns = elapsed_ns(solve_start);
        stats.solve_ns += numerical_ns;
        stats.numerical_sweeps += repaired.sweeps;
        stats.seeded_rounds += repaired.initializer_used;
        if (repaired.initializer_refused && !stats.seed_refusals) stats.first_seed_refusal=repaired.initializer_reason;
        stats.seed_refusals += repaired.initializer_refused;
        stats.untrusted_rounds += repaired.untrusted_initializer_used;
        stats.zero_fallbacks += repaired.proposal_zero_fallback;
        stats.cold_solve_rounds += !repaired.initializer_used && !repaired.untrusted_initializer_used;
        if (candidate_checked) {
            ++stats.eligible_solve_rounds;
            stats.eligible_solve_ns += numerical_ns;
            stats.eligible_sweeps += repaired.sweeps;
            stats.eligible_work += repaired.numerical_transition_work;
        } else {
            ++stats.refused_solve_rounds;
            stats.refused_solve_ns += numerical_ns;
            stats.refused_sweeps += repaired.sweeps;
            stats.refused_work += repaired.numerical_transition_work;
        }
        if (!repaired.checked) throw std::runtime_error("probabilistic quotient repair: "+repaired.reason);
        std::vector<double> repaired_projection(extent, 0);
        for (unsigned i = 0; i < graph_cells.size(); ++i)
            repaired_projection[graph_cells[i].cell_id] = repaired.checked->values_by_state.at(i);
        if (candidate_checked && joint_refinement) {
            // Feasibility alone is insufficient after removing a temporary
            // cap: the old, smaller candidate may still pass. Re-solve the
            // reoptimized model and continue whenever it can improve a cell.
            bool improves = false;
            for (const auto& c : cells) {
                const auto next = repaired_projection[c.id];
                last_improvement=std::max(last_improvement,next-candidate[c.id]);
                improves |= next > candidate[c.id] + 64*std::numeric_limits<double>::epsilon()*std::max(1.0, next);
            }
            if (!improves) {
                accepted_rounds=rounds+1;
                accepted_relation_count=relation_count;
                accepted_report_bytes=relation_payload_bytes;
                if (retain_diagnostics) { exporting=true; continue; }
                break;
            }
        }
        candidate = std::move(repaired_projection);
    }
    if (!accepted_rounds) throw std::runtime_error("probabilistic joint/price refinement did not close within bounded rounds; checked="+
        std::to_string(last_checked)+" improvement="+std::to_string(last_improvement));
    if (!coupled) candidate.resize(grid);
    checkpoint(budget);
    if (!retain_diagnostics) {
        // Acceptance has finished. Ordinary consumers and reusable controls
        // retain checked values/identity and native pool evidence, not reports.
        std::vector<PhasePotentialRelation>().swap(final_relations);
        std::vector<PhaseJointEventWitness>().swap(joint_events);
        std::vector<PhasePriceReactivation>().swap(reactivations);
        std::vector<PhaseNonemptyWitness>().swap(nonempty_witnesses);
        std::vector<PhaseRefillWitness>().swap(refill_witnesses);
    }
    std::uint64_t bytes = 65536 + support->identity.capacity()*8 + candidate.capacity()*24 +
        proposal.values.capacity()*8 + coordinates.capacity()*56 + draws.capacity()*sizeof(CalcContext::NativeGoalDrawBound) +
        final_relations.capacity()*sizeof(PhasePotentialRelation) +
        joint_events.capacity()*sizeof(PhaseJointEventWitness) + reactivations.capacity()*sizeof(PhasePriceReactivation) +
        nonempty_witnesses.capacity()*sizeof(PhaseNonemptyWitness) + refill_witnesses.capacity()*sizeof(PhaseRefillWitness);
    for (const auto& event : joint_events) bytes += 4*(event.natural_draws.capacity()+event.guaranteed_draws.capacity()) +
        sizeof(std::array<double, 3>)*event.conditional.capacity();
    for (const auto& row : final_relations) bytes += row.targets.capacity()*4 + row.probabilities.capacity()*8 +
        row.events.capacity()*sizeof(PhasePotentialRelation::Event);
    // Transfer the existing buffers to immutable ownership. The completed
    // graph is gone; discard construction-only state before reserving the
    // returned view. Charging both the old workspace and the moved buffers
    // would count a nonexistent second native report.
    decltype(cells){}.swap(cells); decltype(graph_cells){}.swap(graph_cells);
    decltype(minimum){}.swap(minimum); decltype(mod_goals){}.swap(mod_goals);
    decltype(goal_side){}.swap(goal_side); decltype(coordinate_index){}.swap(coordinate_index);
    draw_cache.clear(); nonempty_cache.clear(); refill_cache.clear(); joint_cache.clear(); detailed.clear();
    renewal_geometry.clear(); decltype(renewal_capacities){}.swap(renewal_capacities);
    geometry_charge.reset(); capacity_charge.reset();
    scratch.reset();
    scratch=ScopedProofMemoryCharge(support->store_->ledger(),ProofMemoryCategory::Scratch,
        65536+8*(expected_boundary_evidence.capacity()+fracture_exclusions.capacity()));
    if (bytes > cap-support->memory_snapshot().total_bytes)
        throw std::length_error("phase retained evidence exceeds matched reservation");
    combined_peak = std::max(combined_peak, support->memory_snapshot().total_bytes+bytes);
    stats.total_ns = elapsed_ns(preparation_start);
    auto result = std::shared_ptr<const PreparedPhasePotential>(new PreparedPhasePotential(
        std::move(support), std::move(candidate), proposal, std::move(refusal), fracture, fm,
        retain_scour, restart_boundary.lower, std::move(draws), std::move(final_relations), accepted_rounds, bytes,
        combined_peak, action_relations, std::move(reuse_draws), std::move(joint_events),
        std::move(reactivations), joint_refinement, continuation, retention, std::move(coordinates), crafted_domain, crafted_limit,
        filter_mods, std::move(nonempty_witnesses), std::move(refill_witnesses), preparation_options, stats));
    return result;
}
} // namespace poecraft::solver
