#include "tests.hpp"
#include "../src/solver_quotient_bellman.hpp"

#include <cmath>
#include <type_traits>

using namespace poecraft::solver;
using namespace poecraft::solver::quotient;

namespace {
constexpr std::uint64_t cap = 16ull * 1024 * 1024;
const StableKey request{100, 200, 300, 400, 500};
const StableKey scope{400};

QuotientBellmanCellInput cell(std::uint32_t id, bool goal = false) {
    return {id, 1, StableKey{9000, id}, goal};
}

QuotientLowerConstraint scalar(std::uint64_t action, double lower) {
    return {{{action}, false, {}}, LowerConstraintKind::Scalar,
        std::numeric_limits<std::uint64_t>::max(), lower, {701, action},
        LowerEvidenceKind::IndependentLower};
}

QuotientLowerConstraint row(QuotientBellmanGraph& graph,
        std::uint32_t source, std::uint64_t action, double cost,
        std::vector<QuotientBellmanTransitionInput> transitions,
        std::vector<QuotientBellmanChoiceInput> choices = {}) {
    QuotientBellmanRowInput input;
    input.source_cell_id = source;
    input.operator_index = static_cast<std::uint32_t>(action);
    input.cost = cost;
    input.transitions = std::move(transitions);
    input.choices = std::move(choices);
    input.lower_provenance = QuotientLowerRowProvenance{
        request, {9000, source}, {action}, {700, source, action},
        LowerEvidenceKind::ExactDeclaredKernel};
    auto constraint = scalar(action, 0);
    constraint.kind = LowerConstraintKind::Row;
    constraint.row = graph.append_row(std::move(input));
    constraint.evidence = LowerEvidenceKind::ExactDeclaredKernel;
    constraint.evidence_identity = {700, source, action};
    return constraint;
}

QuotientLowerSource source(std::uint32_t id,
        std::vector<std::uint64_t> actions,
        std::vector<QuotientLowerConstraint> constraints) {
    QuotientLowerSource out;
    out.cell_id = id;
    out.source_identity = {9000, id};
    out.expected_actions = {scope, 1, true, {}, {}};
    for (auto action : actions) out.expected_actions.actions.push_back({action});
    out.constraints = std::move(constraints);
    return out;
}

QuotientLowerQuery query(QuotientBellmanGraph& graph,
                        std::vector<QuotientLowerSource> sources) {
    QuotientLowerQuery out;
    out.request_identity = request;
    out.caller_scope = scope;
    out.coefficients = LowerCoefficientModel::ExactBinaryModel;
    out.model_revision = graph.model_revision();
    out.roots = {0};
    out.sources = std::move(sources);
    return out;
}

double root(const QuotientLowerResult& result) {
    PC_CHECK(result.status == QuotientLowerStatus::CheckedFiniteLower);
    return result.checked ? result.checked->values_by_state.at(0) : -1.0;
}

void cyclic_and_revisions() {
    QuotientBellmanGraph graph(cap, QuotientBellmanMode::LowerOnly);
    graph.install_cells({cell(0), cell(1), cell(2, true)});
    auto a = row(graph, 0, 10, 1, {{{1}, 1, .5}, {{2}, 2, .5}});
    auto b = row(graph, 1, 20, 2, {{{1}, 0, .25}, {{2}, 2, .75}});
    auto q = query(graph, {source(0, {10, 11}, {a, scalar(11, 20)}),
                           source(1, {20}, {b})});
    const auto result = graph.solve_lower(q);
    PC_CHECK(std::abs(root(result) - 16.0 / 7) < 1e-8);
    PC_CHECK(std::abs(result.checked->values_by_state[1] - 18.0 / 7) < 1e-8);
    // Smaller than an independently proper but suboptimal 20-cost incumbent.
    PC_CHECK(root(result) < 20.0);
    PC_CHECK(!graph.solve({0}).executable_upper);
    PC_CHECK(!graph.project_unique_certified_policy({0}).proper);
    PC_CHECK(graph.lower_certificate_current(*result.checked, q));
    PC_CHECK(!graph.check_lower(q, {20, 18.0 / 7, 0}).checked);
    PC_CHECK(graph.check_lower(q, {}).status == QuotientLowerStatus::NumericInconclusive);
    PC_CHECK(graph.lower_certificate_current(*result.checked, q));
    auto capped = QuotientLowerBudget{};
    capped.max_scratch_bytes = 1;
    auto memory_before = graph.proof_store()->ledger().snapshot().total_bytes;
    PC_CHECK(graph.solve_lower(q, capped).status == QuotientLowerStatus::ResourceCap);
    PC_CHECK(graph.proof_store()->ledger().snapshot().total_bytes == memory_before);
    int calls = 0;
    QuotientLowerBudget cancel;
    cancel.cancelled = [&] { return ++calls == 3; };
    PC_CHECK(graph.solve_lower(q, cancel).status == QuotientLowerStatus::Cancelled);
    PC_CHECK(graph.proof_store()->ledger().snapshot().total_bytes == memory_before);
    PC_CHECK(graph.lower_certificate_current(*result.checked, q));
    auto changed = q;
    changed.sources[0].constraints[1].lower = 1;
    PC_CHECK(!graph.lower_certificate_current(*result.checked, changed));
    graph.invalidate_target_split(1, 2);
    PC_CHECK(graph.solve_lower(q).status == QuotientLowerStatus::StaleModel);
    q.model_revision = graph.model_revision();
    PC_CHECK(graph.solve_lower(q).status == QuotientLowerStatus::StaleModel);
    PC_CHECK(!graph.lower_certificate_current(*result.checked, q));
}

void action_and_family_coverage() {
    QuotientBellmanGraph graph(cap, QuotientBellmanMode::LowerOnly);
    graph.install_cells({cell(0), cell(1, true)});
    auto q = query(graph, {source(0, {10, 11}, {scalar(10, 8), scalar(10, 8)})});
    PC_CHECK(!graph.solve_lower(q).checked);
    q.sources[0].constraints[1] = scalar(11, 2);
    PC_CHECK(root(graph.solve_lower(q)) == 2);
    q.sources[0].constraints.push_back(scalar(12, 0));
    PC_CHECK(!graph.solve_lower(q).checked); // caller-disabled action

    auto exact = row(graph, 0, 10, 7, {{{1}, 1, 1}});
    auto family = scalar(90, 2);
    family.cover.family = true;
    q = query(graph, {source(0, {}, {family})});
    q.sources[0].expected_actions.families = {{{90}, {{10}, {11}}, false}};
    PC_CHECK(root(graph.solve_lower(q)) == 2);
    q.sources[0].constraints.push_back(exact);
    PC_CHECK(!graph.solve_lower(q).checked); // stale cheap member escape
    q.sources[0].constraints[0].cover.excluded_members = {{10}};
    PC_CHECK(root(graph.solve_lower(q)) == 2); // member 11 remains
    q.sources[0].constraints.erase(q.sources[0].constraints.begin());
    PC_CHECK(!graph.solve_lower(q).checked); // can't delete whole family
    q.sources[0].constraints.push_back(scalar(11, 9));
    PC_CHECK(root(graph.solve_lower(q)) == 7);
    q.sources[0].expected_actions.families[0].open = true;
    PC_CHECK(!graph.solve_lower(q).checked); // partial synthesis remains open
    family.cover.excluded_members = {{10}, {11}};
    q.sources[0].constraints.push_back(family);
    PC_CHECK(root(graph.solve_lower(q)) == 2);

    q = query(graph, {source(0, {10, 11}, {exact, scalar(11, 0)})});
    auto& illegal = q.sources[0].constraints[1];
    illegal.kind = LowerConstraintKind::Inapplicable;
    illegal.evidence = LowerEvidenceKind::Unproved;
    PC_CHECK(!graph.solve_lower(q).checked);
    illegal.evidence = LowerEvidenceKind::ExactInapplicability;
    PC_CHECK(root(graph.solve_lower(q)) == 7);
    // A representative, different hidden modifier member, or phase is not a
    // uniform class certificate, even if visible goal/occupancy keys match.
    q.sources[0].source_identity.push_back(999);
    PC_CHECK(!graph.solve_lower(q).checked);
}

void choices_and_programs() {
    QuotientBellmanGraph graph(cap, QuotientBellmanMode::LowerOnly);
    graph.install_cells({cell(0), cell(1), cell(2, true)});
    auto offer = row(graph, 0, 10, 1, {{{1}, 2, .5}}, {{.5, true, {1}}});
    auto q = query(graph, {source(0, {10}, {offer})});
    q.boundaries = {{1, {9000, 1}, {99}, 100, LowerEvidenceKind::IndependentLower}};
    PC_CHECK(std::abs(root(graph.solve_lower(q)) - 2) < 1e-8);
    PC_CHECK(!graph.check_lower(q, {51, 100, 0}).checked);
    PC_CHECK(graph.check_lower(q, {1.999999, 100, 0}).checked != nullptr);
    graph.invalidate_target_split(0, 2);
    q.model_revision = graph.model_revision();
    PC_CHECK(graph.solve_lower(q).status == QuotientLowerStatus::StaleModel);

    // Choice after seeing each offer: min per observed outcome. Choosing one
    // recipe before the observation is represented by two different rows.
    QuotientBellmanGraph timing(cap, QuotientBellmanMode::LowerOnly);
    timing.install_cells({cell(0), cell(1), cell(2, true)});
    auto after = row(timing, 0, 1, 0, {}, {{.5, false, {1, 2}}, {.5, false, {2, 1}}});
    auto before_a = row(timing, 0, 2, 0, {{{1}, 1, .5}, {{2}, 2, .5}});
    auto before_b = row(timing, 0, 3, 0, {{{1}, 2, .5}, {{2}, 1, .5}});
    auto tq = query(timing, {source(0, {1}, {after})});
    tq.boundaries = {{1, {9000, 1}, {99}, 100, LowerEvidenceKind::IndependentLower}};
    PC_CHECK(root(timing.solve_lower(tq)) == 0);
    tq.sources = {source(0, {2, 3}, {before_a, before_b})};
    PC_CHECK(std::abs(root(timing.solve_lower(tq)) - 50) < 1e-7);
    tq.sources[0].constraints.pop_back();
    PC_CHECK(!timing.solve_lower(tq).checked); // selected recipe not all choices

    QuotientBellmanGraph program(cap, QuotientBellmanMode::LowerOnly);
    program.install_cells({cell(0), cell(1), cell(2, true)});
    auto setup = row(program, 0, 10, .25, {{{1}, 1, 1}});
    auto draw = row(program, 1, 20, 3.5, {{{2}, 2, 1}});
    auto pq = query(program, {source(0, {10}, {setup}), source(1, {20}, {draw})});
    PC_CHECK(std::abs(root(program.solve_lower(pq)) - 3.75) < 1e-8);
    pq.request_identity.push_back(777); // phase/economy/scope assumptions
    PC_CHECK(!program.solve_lower(pq).checked);
    pq.request_identity = request;
    program.note_price_change({{draw.row, 1.5}});
    pq.model_revision = program.model_revision();
    PC_CHECK(!program.solve_lower(pq).checked); // must rebind priced evidence
}

void inconsistent_bounds_and_zero_cost() {
    QuotientBellmanGraph graph(cap, QuotientBellmanMode::LowerOnly);
    graph.install_cells({cell(0), cell(1), cell(2), cell(3, true)});
    auto a = row(graph, 0, 10, 0, {{{1}, 1, 1}});
    auto b = row(graph, 1, 20, 0, {{{1}, 2, 1}});
    auto q = query(graph, {source(0, {10}, {a})});
    q.boundaries = {{1, {9000, 1}, {99}, 10, LowerEvidenceKind::IndependentLower}};
    auto old = graph.solve_lower(q);
    PC_CHECK(root(old) == 10);
    q.sources.push_back(source(1, {20}, {b}));
    q.boundaries = {{2, {9000, 2}, {99}, 0, LowerEvidenceKind::IndependentLower}};
    PC_CHECK(root(graph.solve_lower(q)) == 0);
    PC_CHECK(!graph.check_lower(q, {10, 10, 0, 0}).checked);
    PC_CHECK(old.checked->values_by_state[0] == 10); // independent external max

    QuotientBellmanGraph zero(cap, QuotientBellmanMode::LowerOnly);
    zero.install_cells({cell(0), cell(1, true)});
    auto loop = row(zero, 0, 10, 0, {{{1}, 0, 1}});
    auto exit = row(zero, 0, 11, 10, {{{1}, 1, 1}});
    auto zq = query(zero, {source(0, {10, 11}, {loop, exit})});
    PC_CHECK(zero.check_lower(zq, {0, 0}).checked != nullptr);
    PC_CHECK(root(zero.solve_lower(zq)) == 10);
    PC_CHECK(!zero.check_lower(zq, {11, 0}).checked);
    zq.sources = {source(0, {10}, {loop})};
    PC_CHECK(root(zero.solve_lower(zq)) == 0); // finite weak; no infinity claim
}

void numerical_and_memory() {
    QuotientBellmanGraph graph(cap, QuotientBellmanMode::LowerOnly);
    graph.install_cells({cell(0), cell(1), cell(2, true)});
    const double tiny = std::ldexp(1.0, -40);
    auto rare = row(graph, 0, 10, 1, {{{1}, 1, tiny}, {{2}, 2, 1 - tiny}});
    auto q = query(graph, {source(0, {10}, {rare})});
    q.boundaries = {{1, {9000, 1}, {99}, std::ldexp(1.0, 50),
                    LowerEvidenceKind::IndependentLower}};
    PC_CHECK(std::abs(root(graph.solve_lower(q)) - 1025) < 1e-5);
    auto retry = row(graph, 0, 11, tiny, {{{1}, 0, 1 - tiny}, {{2}, 2, tiny}});
    q = query(graph, {source(0, {11}, {retry})});
    // A tiny relative residual cannot justify an amplified value error.
    PC_CHECK(!graph.check_lower(q, {1.01, 0, 0}).checked);
    PC_CHECK(std::abs(root(graph.solve_lower(q)) - 1) < 1e-8);

    auto defect = row(graph, 0, 12, 1,
        {{{1}, 1, .5}, {{2}, 2, .5}, {{3}, 2, std::ldexp(1.0, -60)}});
    q = query(graph, {source(0, {12}, {defect})});
    q.boundaries = {{1, {9000, 1}, {99}, 2, LowerEvidenceKind::IndependentLower}};
    PC_CHECK(graph.solve_lower(q).status == QuotientLowerStatus::NumericInconclusive);
    q.coefficients = LowerCoefficientModel::RawStoredCoefficients;
    auto raw = graph.solve_lower(q);
    PC_CHECK(root(raw) > 1.999999);
    q.coefficients = LowerCoefficientModel::NormalizedStoredReference;
    auto normalized = graph.solve_lower(q);
    PC_CHECK(root(normalized) > 1.999999);
    PC_CHECK(raw.checked->model_identity != normalized.checked->model_identity);
    const auto before = graph.proof_store()->ledger().snapshot().total_bytes;
    { const auto temporary = graph.solve_lower(q);
      PC_CHECK(graph.proof_store()->ledger().snapshot().total_bytes > before); }
    PC_CHECK(graph.proof_store()->ledger().snapshot().total_bytes == before);
    graph.invalidate_source_split(0, 2);
    q.model_revision = graph.model_revision();
    PC_CHECK(!graph.solve_lower(q).checked);

    QuotientBellmanGraph small(8192, QuotientBellmanMode::LowerOnly);
    small.install_cells({cell(0), cell(1, true)});
    auto sq = query(small, {source(0, {10}, {scalar(10, 0)})});
    const auto rows_before = small.transition_cache().rows.size();
    bool refused = false;
    try {
        std::vector<QuotientBellmanTransitionInput> many(1000, {{1}, 1, .001});
        row(small, 0, 10, 1, std::move(many));
    } catch (const ProofMemoryLimit&) { refused = true; }
    PC_CHECK(refused);
    PC_CHECK(small.transition_cache().rows.size() == rows_before);
    PC_CHECK(small.model_revision() == sq.model_revision);
}
} // namespace

void run_solver_quotient_lower_tests() {
    cyclic_and_revisions();
    action_and_family_coverage();
    choices_and_programs();
    inconsistent_bounds_and_zero_cost();
    numerical_and_memory();
    static_assert(!std::is_convertible_v<QuotientLowerResult, QuotientBellmanResult>);
    static_assert(!std::is_convertible_v<QuotientLowerCertificate, double>);
}
