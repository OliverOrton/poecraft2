#include "solver_phase_lower.hpp"
#include "solver_quotient_bellman.hpp"
#include "poecraft/bitset.h"

#include <bit>
#include <cfenv>
#include <cmath>
#include <map>
#include <stdexcept>

namespace poecraft::solver {
using namespace quotient;
namespace {
constexpr std::uint64_t version = 0x5048415345000001ull;
constexpr std::uint64_t maximum = 16ull << 20;
double down(double x) { return x == 0 ? 0 : std::max(0.0, std::nextafter(x, 0.0)); }
double up(double x) { return std::nextafter(x, std::numeric_limits<double>::infinity()); }
void numeric_mode() {
    static_assert(std::numeric_limits<double>::is_iec559);
    if (std::fegetround() != FE_TONEAREST)
        throw std::invalid_argument("phase lower requires binary64 round-to-nearest");
}
void checkpoint(const QuotientLowerBudget& budget) {
    if (budget.cancelled && budget.cancelled()) throw std::runtime_error("phase lower cancelled");
}
void append(StableKey& key, const std::string& text) {
    key.push_back(text.size());
    for (std::size_t i = 0; i < text.size(); i += 8) {
        std::uint64_t word = 0;
        for (std::size_t j = 0; j < 8 && i+j < text.size(); ++j)
            word |= std::uint64_t(static_cast<unsigned char>(text[i+j])) << (j*8);
        key.push_back(word);
    }
}
void append(StableKey& key, const StableKey& other) {
    key.push_back(other.size()); key.insert(key.end(), other.begin(), other.end());
}
std::uint64_t context_workspace(const CalcContext& calc, const PhaseLowerPrices& prices) {
    // Deliberately conservative: charge even the borrowed calculator once as
    // scratch here to bound deep runtime-contract copies and mask temporaries.
    // It is still shared storage in the native owner/process accounting.
    std::uint64_t bytes = calc.estimated_owned_bytes();
    if (bytes > maximum/3) throw std::length_error("phase lower context exceeds bounded proof workspace");
    bytes = 3*bytes + 65536;
    for (const auto& [key, value] : prices) {
        (void)value;
        if (key.size() > maximum || bytes > maximum-std::min<std::uint64_t>(maximum, 256+8*key.size()))
            throw std::length_error("phase lower price evidence exceeds bounded workspace");
        bytes += 256+8*key.size();
    }
    return bytes;
}
std::uint32_t mask_for_item(const CalcContext& calc, const pc_item_state& item) {
    std::uint32_t mask = 0;
    const auto side = [&](const pc_mod_slot* mods, unsigned count) {
        for (unsigned i = 0; i < count; ++i) {
            if (mods[i].mod_id >= calc.session().mod_count)
                throw std::invalid_argument("phase lower foreign modifier");
            for (std::size_t slot = 0; slot < calc.layout().slots.size(); ++slot)
                if (pc_bitset_test(calc.layout().slots[slot].satisfying_mask.data(), mods[i].mod_id))
                    mask |= 1u << slot;
        }
    };
    if (item.prefix_count > PC_MAX_PREFIXES || item.suffix_count > PC_MAX_SUFFIXES)
        throw std::invalid_argument("phase lower invalid occupancy");
    side(item.prefixes, item.prefix_count); side(item.suffixes, item.suffix_count);
    return mask;
}
StableKey context_key(const CalcContext& calc, const PhaseLowerPrices& prices) {
    const auto& session = calc.session();
    StableKey result{version, session.base_index, session.item_level,
        static_cast<std::uint64_t>(calc.goal().rarity), calc.goal().required_satisfied_slots(),
        calc.goal().automatic_candidates, calc.goal().automatic_candidate_kind_mask,
        calc.goal().disabled_action_families, calc.layout().slots.size()};
    append(result, session.data->artifact_data_hash);
    append(result, session.data->artifact_game_data_hash);
    for (const auto& slot : calc.layout().slots) append(result, slot.satisfying_mask);
    // Registry and session are immutable within a CalcContext. Full descriptors
    // and full prices bind reuse; no hash equality or incumbent participates.
    result.push_back(calc.registry().actions.size());
    for (const auto& action : calc.registry().actions) {
        append(result, action.id);
        result.push_back(static_cast<std::uint64_t>(action.params.type));
        result.push_back(action.params.essence_index); result.push_back(action.params.mod_id);
        result.push_back(action.params.target_tag_id); result.push_back(action.params.source_tag_id);
        result.push_back(static_cast<std::uint64_t>(action.params.influence_code)); result.push_back(action.params.tier);
        result.push_back(action.params.fossil_indices.size());
        for (auto fossil : action.params.fossil_indices) result.push_back(fossil);
        result.push_back(action.synthetic); result.push_back(action.uses_companion_state);
        result.push_back(action.cost_keys.size());
        for (const auto& key : action.cost_keys) append(result, key);
    }
    result.push_back(calc.candidates().size());
    for (auto candidate : calc.candidates()) result.push_back(candidate);
    // Static authored programs are request dependencies. State-local generated
    // additions do not change the already-covered grammar or primitive universe.
    result.push_back(calc.static_candidate_operator_count());
    for (std::size_t i = 0; i < calc.static_candidate_operator_count(); ++i)
        append(result, planner_operator_semantic_key(calc.operators().at(calc.candidate_operators().at(i))));
    const std::map<std::string, double> ordered(prices.begin(), prices.end());
    result.push_back(ordered.size());
    for (const auto& [key, value] : ordered) { append(result, key); result.push_back(std::bit_cast<std::uint64_t>(value)); }
    return result;
}
StableKey table_identity(StableKey context, std::uint8_t searing, std::uint8_t eater,
                         const std::vector<double>& values) {
    context.push_back(searing); context.push_back(eater); context.push_back(values.size());
    for (double value : values) context.push_back(std::bit_cast<std::uint64_t>(value));
    return context;
}
bool monotone(const std::vector<double>& values) {
    for (std::size_t mask = 0; mask < values.size(); ++mask) {
        if (!std::isfinite(values[mask]) || values[mask] < 0) return false;
        for (std::size_t bit = 1; bit < values.size(); bit <<= 1)
            if (values[mask] < values[mask | bit]) return false;
    }
    return true;
}
void grammar_coverage(const CalcContext& calc) {
    // These are the runtime's nine program productions, including conditional
    // prefixes, arbitrary observed selections, retry and return-to-entry.
    // A monotone acquisition potential telescopes through every constituent.
    // Imprint's private restore only returns to the initial item; remembering
    // the union of goals ever acquired makes that edge free and non-increasing.
    // Nonnegative extra checkpoint costs may be dropped, never subtracted.
    static_assert(static_cast<unsigned>(FixedOptionKind::TemporaryBenchRepeat) == 8);
    static_assert(static_cast<unsigned>(AutomaticCandidateKind::Veiled) == 10);
    if (calc.goal().automatic_candidate_kind_mask & ~kAllAutomaticCandidateKindsMask)
        throw std::invalid_argument("phase lower uncovered generated family");
    for (auto index : calc.candidate_operators()) {
        const auto& op = calc.operators().at(index);
        if (op.kind != PlannerOperatorKind::Primitive && op.kind != PlannerOperatorKind::FixedOption)
            throw std::invalid_argument("phase lower uncovered operator production");
        if (op.kind == PlannerOperatorKind::FixedOption &&
            static_cast<unsigned>(op.option_kind) > 8)
            throw std::invalid_argument("phase lower uncovered runtime production");
        const auto runtime = planner_operator_runtime_semantics(op, calc.registry());
        if (runtime.execution_paths.empty())
            throw std::invalid_argument("phase lower missing runtime choices");
    }
}
} // namespace

PhaseProbabilityInterval phase_weight_probability(std::uint64_t part, std::uint64_t total) {
    numeric_mode();
    if (!total || part > total) throw std::invalid_argument("invalid complete integer mass");
    if (!part) return {0, 0};
    if (part == total) return {1, 1};
    // Enclose integer-to-double conversion as well as division. This remains
    // valid above 2^53; no rounded native row is reconstructed or normalized.
    const double n = static_cast<double>(part), d = static_cast<double>(total);
    return {down(down(n) / up(d)), std::min(1.0, up(up(n) / down(d)))};
}
double phase_two_exit_lower(double cost, PhaseProbabilityInterval p, double success, double failure) {
    numeric_mode();
    if (!std::isfinite(cost) || cost < 0 || !std::isfinite(success) || success < 0 ||
        !std::isfinite(failure) || failure < 0 || !(0 <= p.lower && p.lower <= p.upper && p.upper <= 1))
        throw std::invalid_argument("invalid phase expected-potential inputs");
    const double endpoint = success <= failure ? p.upper : p.lower;
    return down(cost + down(down(endpoint * success) + down(down(1-endpoint) * failure)));
}
double phase_price_lower(const ActionDescriptor& action, const PhaseLowerPrices& prices) {
    numeric_mode();
    double sum = 0;
    for (const auto& key : action.cost_keys) {
        const auto found = prices.find(key);
        if (found == prices.end() || !std::isfinite(found->second) || found->second < 0)
            return std::numeric_limits<double>::infinity();
        sum = sum == 0 ? found->second : (found->second == 0 ? sum : down(sum + found->second));
        if (!std::isfinite(sum)) throw std::overflow_error("phase lower price overflow");
    }
    return sum;
}

PreparedPhaseLowerView::PreparedPhaseLowerView(StableKey context, std::vector<double> table,
        std::vector<PhasePrimitiveWitness> actions, std::uint32_t families,
        std::uint8_t exarch, std::uint8_t worlds, bool original,
        std::shared_ptr<const SessionImpl> session, std::shared_ptr<ProofStore> store,
        std::uint64_t reservation)
    : identity(table_identity(context, exarch, worlds, table)), values(std::move(table)),
      primitives(std::move(actions)), family_mask(families), searing(exarch), eater(worlds),
      original_candidate_accepted(original), retained_reservation(reservation),
      session_(std::move(session)), context_(std::move(context)), store_(std::move(store)),
      charge_(store_->ledger(), ProofMemoryCategory::Certificate, reservation) {}

bool PreparedPhaseLowerView::compatible(const CalcContext& calc, const PhaseLowerPrices& prices,
                                        const pc_item_state& item) const {
    if (session_.get() != &calc.session() || searing != item.searing_exarch_tier ||
        eater != item.eater_of_worlds_tier) return false;
    const auto bytes = context_workspace(calc, prices);
    ScopedProofMemoryCharge scratch(store_->ledger(), ProofMemoryCategory::Scratch, bytes);
    return context_ == context_key(calc, prices);
}
std::optional<double> PreparedPhaseLowerView::lookup(const CalcContext& calc,
        const PhaseLowerPrices& prices, const pc_item_state& item) const {
    if (!compatible(calc, prices, item)) return std::nullopt;
    return values.at(mask_for_item(calc, item));
}

std::shared_ptr<const PreparedPhaseLowerView> PhaseLowerProducer::prepare(
        CalcContext& calc, const PhaseLowerPrices& prices, const pc_item_state& phase,
        const std::vector<double>& candidate, const QuotientLowerBudget& budget) {
    numeric_mode(); checkpoint(budget);
    for (const auto& [key, value] : prices) {
        (void)key;
        if (!std::isfinite(value) || value < 0) throw std::invalid_argument("phase lower needs nonnegative finite prices");
    }
    if (calc.layout().slots.empty() || calc.layout().slots.size() > 8)
        throw std::invalid_argument("phase lower finite projection cap is eight goals");
    const auto cap = std::min(maximum, budget.max_scratch_bytes);
    if (cap < (2ull << 20)) throw std::length_error("phase lower reservation refused");
    auto store = std::make_shared<ProofStore>(cap);
    // Reserve native evidence, identity, candidate and checker workspace before
    // allocation. The other half is the existing transient quotient owner.
    ScopedProofMemoryCharge workspace(store->ledger(), ProofMemoryCategory::Scratch, cap/2);
    if (context_workspace(calc, prices) > cap/2)
        throw std::length_error("phase lower evidence does not fit reserved workspace");
    grammar_coverage(calc);
    auto context = context_key(calc, prices);
    std::vector<PhasePrimitiveWitness> witnesses;
    std::map<std::uint32_t, double> cheapest;
    const std::uint32_t size = 1u << calc.layout().slots.size();
    for (const auto& action : calc.registry().actions) {
        checkpoint(budget);
        if (action.uses_companion_state || (!action.synthetic &&
                (static_cast<int>(action.params.type) < 0 || static_cast<int>(action.params.type) > 25)))
            throw std::invalid_argument("phase lower unknown primitive relation");
        const auto reach = action_explicit_affix_reachable_mask(calc.session(), action, true);
        std::uint32_t goals = 0;
        for (std::size_t slot = 0; slot < calc.layout().slots.size(); ++slot)
            for (std::size_t word = 0; word < reach.size(); ++word)
                if (reach[word] & calc.layout().slots[slot].satisfying_mask.at(word)) { goals |= 1u << slot; break; }
        const double cost = phase_price_lower(action, prices);
        witnesses.push_back({action.id, goals, std::isfinite(cost) ? cost : 0, std::isfinite(cost)});
        if (!std::isfinite(cost)) continue;
        const auto [it, inserted] = cheapest.emplace(goals, cost);
        if (!inserted) it->second = std::min(it->second, cost);
    }
    // Every accepted generated program is composed only of registry actions;
    // program_has_prices in automatic admission checks ALL constituents. An
    // unpriced descriptor cannot occur on an executable program path. Validate
    // the same condition for the currently selected authored/static programs.
    for (auto index : calc.candidate_operators()) {
        const auto& op = calc.operators().at(index);
        bool wrapper_priced = true;
        for (const auto& [key, quantity] : op.resource_quantities) {
            if (!std::isfinite(quantity) || quantity < 0) throw std::invalid_argument("negative program resource");
            wrapper_priced &= prices.contains(key);
        }
        if (!wrapper_priced) continue;
        for (auto action : planner_operator_runtime_semantics(op, calc.registry()).action_dependencies)
            if (!witnesses.at(action).priced)
                throw std::invalid_argument("priced program has an unpriced primitive dependency");
    }
    if (cheapest.empty()) throw std::invalid_argument("phase lower has no priced primitive cover");
    std::vector<double> table;
    bool original = false;
    {
        QuotientBellmanGraph graph(cap/2, QuotientBellmanMode::LowerOnly);
        std::vector<QuotientBellmanCellInput> cells;
        for (std::uint32_t mask = 0; mask < size; ++mask)
            cells.push_back({mask, 1, {version, mask},
                std::popcount(mask) >= calc.goal().required_satisfied_slots()});
        graph.install_cells(std::move(cells));
        QuotientLowerQuery query;
        query.request_identity = {version}; query.caller_scope = {version, 1};
        query.coefficients = LowerCoefficientModel::ExactBinaryModel; query.roots = {0};
        for (const auto& [reach, cost] : cheapest) {
            query.request_identity.push_back(reach); query.request_identity.push_back(std::bit_cast<std::uint64_t>(cost));
        }
        for (std::uint32_t mask = 0; mask < size; ++mask) {
            checkpoint(budget);
            if (std::popcount(mask) >= calc.goal().required_satisfied_slots()) continue;
            QuotientLowerSource source{mask, {version, mask}, {query.caller_scope, 1, true, {}, {}}, {}};
            for (const auto& [reach, cost] : cheapest) {
                const StableKey action{version, reach}, evidence{version, mask, reach};
                source.expected_actions.actions.push_back(action);
                QuotientBellmanRowInput row;
                row.source_cell_id = mask; row.operator_index = reach; row.cost = cost;
                row.transitions = {{{version, mask | reach}, mask | reach, 1.0}};
                row.lower_provenance = QuotientLowerRowProvenance{query.request_identity,
                    source.source_identity, action, evidence, LowerEvidenceKind::ExactDeclaredKernel};
                source.constraints.push_back({{action, false, {}}, LowerConstraintKind::Row,
                    graph.append_row(std::move(row)), 0, evidence, LowerEvidenceKind::ExactDeclaredKernel});
            }
            query.sources.push_back(std::move(source));
        }
        query.model_revision = graph.model_revision();
        auto checked = graph.check_lower(query, candidate, budget);
        original = bool(checked.checked) && monotone(candidate);
        if (!original) checked = graph.solve_lower(query, budget);
        if (!checked.checked || !monotone(checked.checked->values_by_state))
            throw std::runtime_error("phase lower finite checker refused: " + checked.reason);
        table = checked.checked->values_by_state;
    }
    checkpoint(budget);
    std::uint64_t bytes = sizeof(PreparedPhaseLowerView) + 2048 +
        3 * context.capacity() * sizeof(std::uint64_t) + table.capacity() * sizeof(double) +
        witnesses.capacity() * sizeof(PhasePrimitiveWitness);
    for (const auto& action : witnesses) bytes += action.action_id.capacity() + 1;
    if (bytes > cap/2) throw std::length_error("phase lower retained snapshot exceeds reservation");
    auto result = std::shared_ptr<const PreparedPhaseLowerView>(new PreparedPhaseLowerView(
        std::move(context), std::move(table), std::move(witnesses),
        calc.goal().automatic_candidates ? calc.goal().automatic_candidate_kind_mask : 0,
        phase.searing_exarch_tier, phase.eater_of_worlds_tier, original,
        calc.shared_session(), std::move(store), bytes));
    workspace.reset();
    return result;
}

PhaseProgramLowerWitness::PhaseProgramLowerWitness(PhaseProgramLowerRecord value,
        std::shared_ptr<ProofStore> store)
    : record(std::move(value)), store_(std::move(store)),
      charge_(store_->ledger(), ProofMemoryCategory::Certificate, sizeof(*this) +
        record.operator_id.capacity() + 1 + sizeof(std::uint64_t) *
        (record.source.capacity() + record.post_phase.capacity() +
         record.operator_identity.capacity() + record.donor_identity.capacity())) {}

PhaseProgramLowerWitness PhaseLowerProducer::compose(CalcContext& calc, const PhaseLowerPrices& prices,
        const pc_item_state& source, const std::string& operator_id,
        const PreparedPhaseLowerView& donor, const QuotientLowerBudget& budget) {
    checkpoint(budget); numeric_mode();
    // Account query work in the same retained snapshot budget; never cache a
    // SolveWork or a physical successor graph. Native entries are streamed.
    ScopedProofMemoryCharge workspace(donor.store_->ledger(), ProofMemoryCategory::Scratch,
        std::min<std::uint64_t>(2ull << 20, budget.max_scratch_bytes));
    if (budget.max_scratch_bytes < (2ull << 20)) throw std::length_error("phase program reservation refused");
    const auto candidates = calc.phase_lower_eldritch_programs(source, prices);
    const auto found = std::find_if(candidates.begin(), candidates.end(),
        [&](const auto& op) { return op.id == operator_id; });
    if (found == candidates.end()) throw std::invalid_argument("program absent from native caller-authorized family");
    const auto runtime = planner_operator_runtime_semantics(*found, calc.registry());
    if (runtime.execution_paths.size() != 1 || runtime.execution_paths.front().size() != 2)
        throw std::invalid_argument("phase composition requires the native mandatory two-step path");
    const auto& path = runtime.execution_paths.front();
    const auto& setup = calc.registry().actions.at(path[0].action);
    const auto& draw = calc.registry().actions.at(path[1].action);
    if (setup.params.type != ActionType::EldritchIchor || draw.params.type != ActionType::EldritchExalt ||
        !action_legal(calc.session(), setup, calc.state(calc.intern_item(source))))
        throw std::invalid_argument("phase composition setup is not legal Ichor/Exalt");
    pc_item_state phase = source;
    phase.eater_of_worlds_tier = static_cast<std::uint8_t>(setup.params.tier);
    if (!donor.compatible(calc, prices, phase)) throw std::invalid_argument("incompatible phase donor");
    PhaseProgramLowerRecord result;
    result.source = exact_item_state_key(source); result.post_phase = exact_item_state_key(phase);
    result.operator_identity = planner_operator_semantic_key(*found);
    result.operator_id = found->id; result.donor_identity = donor.identity;
    result.cost_lower = down(phase_price_lower(setup, prices) + phase_price_lower(draw, prices));
    result.failure_lower_min = std::numeric_limits<double>::infinity();
    result.total_weight = calc.phase_lower_add_weights(phase, path[1].action,
        [&](const pc_item_state& exit, std::uint64_t weight) {
            checkpoint(budget); ++result.physical_exits;
            // Phase is preserved by add, and ALL exact physical entries are
            // inspected. No coarse representative or dropped branch is used.
            if (exit.searing_exarch_tier != donor.searing || exit.eater_of_worlds_tier != donor.eater)
                throw std::invalid_argument("program has an uncovered phase exit");
            const auto mask = mask_for_item(calc, exit);
            const auto satisfied = std::popcount(mask);
            const bool goal = exit.rarity == calc.goal().rarity &&
                satisfied >= calc.goal().required_satisfied_slots() &&
                satisfied == exit.prefix_count + exit.suffix_count;
            if (goal) {
                if (weight > std::numeric_limits<std::uint64_t>::max() - result.goal_weight)
                    throw std::overflow_error("program goal mass overflow");
                result.goal_weight += weight;
            } else {
                const double value = donor.values.at(mask);
                result.failure_lower_min = std::min(result.failure_lower_min, value);
                result.failure_lower_max = std::max(result.failure_lower_max, value);
            }
        });
    if (result.goal_weight == result.total_weight) result.failure_lower_min = 0;
    const auto probability = phase_weight_probability(result.goal_weight, result.total_weight);
    result.goal_probability_upper = probability.upper;
    result.lower = phase_two_exit_lower(result.cost_lower, probability, 0, result.failure_lower_min);
    checkpoint(budget);
    return PhaseProgramLowerWitness(std::move(result), donor.store_);
}
} // namespace poecraft::solver
