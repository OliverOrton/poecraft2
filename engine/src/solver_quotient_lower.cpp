#include "solver_quotient_bellman.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <set>

namespace poecraft::solver::quotient {
namespace {

/* Binary64 operations, round-to-nearest, no fast-math/FP contraction (native
 * build contract). One adjacent representable number encloses each rounded
 * nonnegative sum/product. Exact zero/identity operations avoid artificial
 * negative bounds and preserve zero-cost self-loop inequalities. */
double down(double x) {
    return x == 0.0 ? 0.0 : std::nextafter(x, 0.0);
}
double add_down(double a, double b) {
    return a == 0.0 ? b : b == 0.0 ? a : down(a + b);
}
double mul_down(double a, double b) {
    return a == 0.0 || b == 0.0 ? 0.0 :
        a == 1.0 ? b : b == 1.0 ? a : down(a * b);
}
double add_up(double a, double b) {
    return a == 0.0 ? b : b == 0.0 ? a :
        std::nextafter(a + b, std::numeric_limits<double>::infinity());
}

/* Exact dyadic mass check, including subnormals. Units are 2^-1074;
 * accumulation has 1152 bits, enough for this bounded arena. Unlike a rounded
 * sum == 1 or a tolerance, this detects the micro row's +21/2^60 defect. */
struct ExactMass {
    std::array<std::uint64_t, 18> limbs{};
    bool valid = true;
    void add(double value) {
        if (!std::isfinite(value) || value < 0.0 || value > 1.0) {
            valid = false;
            return;
        }
        const auto bits = std::bit_cast<std::uint64_t>(value);
        const auto exponent = (bits >> 52) & 2047;
        const auto mantissa = (bits & ((1ull << 52) - 1)) |
            (exponent ? (1ull << 52) : 0);
        const auto shift = exponent ? exponent - 1 : 0;
        for (unsigned bit = 0; bit < 53; ++bit) {
            if (!(mantissa & (1ull << bit))) continue;
            std::size_t word = (shift + bit) / 64;
            std::uint64_t carry = 1ull << ((shift + bit) % 64);
            while (carry && word < limbs.size()) {
                const auto before = limbs[word];
                limbs[word] += carry;
                carry = limbs[word] < before ? 1 : 0;
                ++word;
            }
            if (carry) valid = false;
        }
    }
    bool one() const {
        if (!valid) return false;
        for (std::size_t word = 0; word < limbs.size(); ++word)
            if (limbs[word] != (word == 1074 / 64
                    ? 1ull << (1074 % 64) : 0)) return false;
        return true;
    }
};

/* Exact binary64 dot product in units of 2^-2148. The fixed 68-limb
 * accumulator covers all finite binary64 products plus 156 carry bits. */
struct ExactDot {
    std::array<std::uint64_t, 68> limbs{};
    bool valid = true;
    void add_product(double a, double b) {
        if (!std::isfinite(a) || !std::isfinite(b) || a < 0 || b < 0) {
            valid = false;
            return;
        }
        const auto decode = [](double value) {
            const auto bits = std::bit_cast<std::uint64_t>(value);
            const auto exponent = (bits >> 52) & 2047;
            return std::pair<std::uint64_t, std::uint64_t>{
                (bits & ((1ull << 52) - 1)) | (exponent ? 1ull << 52 : 0),
                exponent ? exponent - 1 : 0};
        };
        const auto [am, as] = decode(a);
        const auto [bm, bs] = decode(b);
        for (unsigned i = 0; i < 53; ++i) {
            if (!(am & (1ull << i))) continue;
            for (unsigned j = 0; j < 53; ++j) {
                if (!(bm & (1ull << j))) continue;
                const auto bit = as + bs + i + j;
                std::size_t word = bit / 64;
                std::uint64_t carry = 1ull << (bit % 64);
                while (carry && word < limbs.size()) {
                    const auto before = limbs[word];
                    limbs[word] += carry;
                    carry = limbs[word] < before ? 1 : 0;
                    ++word;
                }
                if (carry) valid = false;
            }
        }
    }
    bool at_most(const ExactDot& other) const {
        if (!valid || !other.valid) return false;
        for (std::size_t i = limbs.size(); i-- > 0;) {
            if (limbs[i] != other.limbs[i]) return limbs[i] < other.limbs[i];
        }
        return true;
    }
};

std::uint64_t key_bytes(const StableKey& key) {
    return key.capacity() * sizeof(std::uint64_t);
}

struct ResultStorageCharge {
    std::shared_ptr<ProofStore> owner;
    ScopedProofMemoryCharge charge;
    ResultStorageCharge(std::shared_ptr<ProofStore> store, std::uint64_t bytes)
        : owner(std::move(store)),
          charge(owner->ledger(), ProofMemoryCategory::Scratch, bytes) {}
};

} // namespace

QuotientLowerCertificate::QuotientLowerCertificate(
        std::shared_ptr<ProofStore> owner, StableKey identity, StableKey model_key,
        std::uint64_t revision, LowerCoefficientModel model,
        std::vector<double> values)
    : request_identity(std::move(identity)), model_identity(std::move(model_key)),
      model_revision(revision), price_generation(owner->price_generation()),
      coefficients(model), values_by_state(std::move(values)),
      owner_(std::move(owner)),
      charge_(owner_->ledger(), ProofMemoryCategory::Certificate,
          sizeof(*this) + key_bytes(request_identity) + key_bytes(model_identity) +
              values_by_state.capacity() * sizeof(double)) {}

StableKey quotient_lower_model_identity(const QuotientLowerQuery& query) {
    StableKey key{1, query.model_revision,
        static_cast<std::uint64_t>(query.coefficients)};
    const auto append = [&](const StableKey& part) {
        key.push_back(part.size());
        key.insert(key.end(), part.begin(), part.end());
    };
    append(query.request_identity);
    append(query.caller_scope);
    key.push_back(query.roots.size());
    for (auto root : query.roots) key.push_back(root);
    key.push_back(query.sources.size());
    for (const auto& source : query.sources) {
        key.push_back(source.cell_id);
        append(source.source_identity);
        const auto& set = source.expected_actions;
        append(set.scope_identity);
        key.push_back(set.generation);
        key.push_back(set.complete);
        key.push_back(set.actions.size());
        for (const auto& action : set.actions) append(action);
        key.push_back(set.families.size());
        for (const auto& family : set.families) {
            append(family.identity);
            key.push_back(family.open);
            key.push_back(family.members.size());
            for (const auto& member : family.members) append(member);
        }
        key.push_back(source.constraints.size());
        for (const auto& constraint : source.constraints) {
            append(constraint.cover.identity);
            key.push_back(constraint.cover.family);
            key.push_back(constraint.cover.excluded_members.size());
            for (const auto& member : constraint.cover.excluded_members) append(member);
            key.push_back(static_cast<std::uint64_t>(constraint.kind));
            key.push_back(constraint.row);
            key.push_back(std::bit_cast<std::uint64_t>(constraint.lower));
            append(constraint.evidence_identity);
            key.push_back(static_cast<std::uint64_t>(constraint.evidence));
        }
    }
    key.push_back(query.boundaries.size());
    for (const auto& boundary : query.boundaries) {
        key.push_back(boundary.cell_id);
        append(boundary.source_identity);
        append(boundary.evidence_identity);
        key.push_back(std::bit_cast<std::uint64_t>(boundary.lower));
        key.push_back(static_cast<std::uint64_t>(boundary.evidence));
    }
    return key;
}

bool QuotientBellmanGraph::lower_certificate_current(
        const QuotientLowerCertificate& certificate,
        const QuotientLowerQuery& query) const {
    return certificate.owner_ == proof_store() &&
        certificate.price_generation == proof_store()->price_generation() &&
        query.model_revision == model_revision_ &&
        certificate.model_identity == quotient_lower_model_identity(query);
}

const char* quotient_lower_status_name(QuotientLowerStatus status) {
    switch (status) {
    case QuotientLowerStatus::CoverageIncomplete: return "coverage_incomplete";
    case QuotientLowerStatus::NumericInconclusive: return "numeric_inconclusive";
    case QuotientLowerStatus::CheckedFiniteLower: return "checked_finite_lower";
    case QuotientLowerStatus::StaleModel: return "stale_model";
    case QuotientLowerStatus::Cancelled: return "cancelled";
    case QuotientLowerStatus::ResourceCap: return "resource_cap";
    }
    return "numeric_inconclusive";
}

QuotientLowerResult QuotientBellmanGraph::solve_lower(
        const QuotientLowerQuery& query,
        const QuotientLowerBudget& budget) const {
    return run_lower(query, budget, nullptr);
}

QuotientLowerResult QuotientBellmanGraph::check_lower(
        const QuotientLowerQuery& query, const std::vector<double>& values,
        const QuotientLowerBudget& budget) const {
    return run_lower(query, budget, &values);
}

QuotientLowerResult QuotientBellmanGraph::run_lower(
        const QuotientLowerQuery& query, const QuotientLowerBudget& budget,
        const std::vector<double>* candidate) const {
    QuotientLowerResult out;
    const auto release_diagnostics = [&] {
        std::vector<double>().swap(out.candidate_values_by_state);
        std::vector<QuotientLowerLimitingConstraint>().swap(out.ranked_constraints);
        out.storage_charge.reset();
    };
    const auto refuse = [&](QuotientLowerStatus status, const char* reason) {
        out.status = status;
        out.reason = reason;
    };
    if (mode_ != QuotientBellmanMode::LowerOnly || query.roots.empty() ||
        query.request_identity.empty() || query.caller_scope.empty()) {
        out.reason = "lower-only graph, roots and full request scope required";
        return out;
    }
    if (query.model_revision != model_revision_) {
        refuse(QuotientLowerStatus::StaleModel, "query revision is stale");
        return out;
    }
    if (candidate && candidate->size() != cell_by_state_.size()) {
        refuse(QuotientLowerStatus::NumericInconclusive,
            "candidate dimension does not match this graph");
        return out;
    }
    const auto cancelled = [&] {
        return budget.cancelled && budget.cancelled();
    };
    if (cancelled()) {
        refuse(QuotientLowerStatus::Cancelled, "cancelled before query");
        return out;
    }
    try {
        const auto count = cell_by_state_.size();
        /* Include borrowed snapshot and peak temporary partition/ranking
         * storage in this query reservation; they are not uncharged sinks. */
        std::uint64_t bytes = 4096 + count * 128 +
            transition_cache_.rows.size() * sizeof(double) +
            query.sources.capacity() * sizeof(QuotientLowerSource) +
            query.boundaries.capacity() * sizeof(QuotientLowerBoundary) +
            query.roots.capacity() * sizeof(std::uint32_t) +
            key_bytes(query.request_identity) + key_bytes(query.caller_scope);
        for (const auto& source : query.sources) {
            bytes += 512 + key_bytes(source.source_identity) +
                key_bytes(source.expected_actions.scope_identity);
            for (const auto& action : source.expected_actions.actions)
                bytes += 256 + 4 * key_bytes(action);
            for (const auto& family : source.expected_actions.families) {
                bytes += 256 + 4 * key_bytes(family.identity);
                for (const auto& member : family.members)
                    bytes += 256 + 4 * key_bytes(member);
            }
            for (const auto& constraint : source.constraints) {
                bytes += 512 + 4 * key_bytes(constraint.cover.identity) +
                    key_bytes(constraint.evidence_identity);
                for (const auto& member : constraint.cover.excluded_members)
                    bytes += 256 + 4 * key_bytes(member);
            }
        }
        for (const auto& boundary : query.boundaries)
            bytes += 128 + key_bytes(boundary.source_identity) +
                key_bytes(boundary.evidence_identity);
        out.scratch_bytes = bytes;
        if (bytes > budget.max_scratch_bytes)
            throw ProofMemoryLimit(bytes, budget.max_scratch_bytes);
        ScopedProofMemoryCharge scratch(proof_store()->ledger(),
            ProofMemoryCategory::Scratch, bytes);
        std::vector<const QuotientLowerSource*> sources(count, nullptr);
        std::vector<std::uint8_t> known(count, 0);
        std::vector<double> values(count, 0.0);
        for (const auto& [id, record] : cells_) {
            (void)id;
            if (record.cell.terminal) known[record.state] = 1;
        }
        for (const auto& source : query.sources) {
            const auto state = state_for_cell(source.cell_id);
            if (!state || known[*state] ||
                source.source_identity != semantic_identity_for_cell(source.cell_id) ||
                source.expected_actions.scope_identity != query.caller_scope) {
                out.reason = "duplicate, terminal or mismatched modeled source";
                return out;
            }
            std::vector<CanonicalActionCover> cover;
            for (const auto& constraint : source.constraints)
                cover.push_back(constraint.cover);
            out.reason = validate_canonical_action_coverage(
                source.expected_actions, cover);
            if (!out.reason.empty()) return out;
            sources[*state] = &source;
            known[*state] = 1;
        }
        for (const auto& boundary : query.boundaries) {
            const auto state = state_for_cell(boundary.cell_id);
            if (!state || known[*state] ||
                boundary.source_identity != semantic_identity_for_cell(boundary.cell_id) ||
                boundary.evidence != LowerEvidenceKind::IndependentLower ||
                boundary.evidence_identity.empty() ||
                !std::isfinite(boundary.lower) || boundary.lower < 0.0) {
                out.reason = "frontier requires independent exact-key evidence";
                return out;
            }
            values[*state] = boundary.lower;
            known[*state] = 1;
        }
        for (const auto root : query.roots) {
            const auto state = state_for_cell(root);
            if (!state || !known[*state]) {
                out.reason = "uncovered query root";
                return out;
            }
        }
        std::vector<double> mass_upper(transition_cache_.rows.size(), 1.0);
        for (std::uint32_t state = 0; state < count; ++state) {
            if (!sources[state]) continue;
            for (const auto& constraint : sources[state]->constraints) {
                if (constraint.evidence_identity.empty()) {
                    out.reason = "constraint lacks lower provenance";
                    return out;
                }
                if (constraint.kind == LowerConstraintKind::Scalar) {
                    if (constraint.evidence != LowerEvidenceKind::IndependentLower ||
                        !std::isfinite(constraint.lower) || constraint.lower < 0.0) {
                        out.reason = "scalar/family requires independent lower evidence";
                        return out;
                    }
                    continue;
                }
                if (constraint.cover.family) {
                    out.reason = "residual families require whole-family scalar evidence";
                    return out;
                }
                if (constraint.kind == LowerConstraintKind::Inapplicable) {
                    if (constraint.evidence != LowerEvidenceKind::ExactInapplicability) {
                        out.reason = "unsupported is not exact native inapplicability";
                        return out;
                    }
                    continue;
                }
                if (constraint.evidence != LowerEvidenceKind::ExactDeclaredKernel ||
                    constraint.row >= lower_row_bindings_.size()) {
                    out.reason = "row lacks declared complete kernel evidence";
                    return out;
                }
                const auto& binding = lower_row_bindings_[constraint.row];
                const auto& provenance = binding.provenance;
                const auto& sparse = transition_cache_.rows.at(constraint.row);
                const auto& cell = cells_.at(cell_by_state_[state]);
                if (!sparse.admitted || sparse.owner_state != state ||
                    provenance.request_identity != query.request_identity ||
                    provenance.source_identity != cell.cell.semantic_identity.value() ||
                    provenance.action_identity != constraint.cover.identity ||
                    provenance.evidence_identity != constraint.evidence_identity ||
                    binding.source_generation != cell.source_generation ||
                    binding.price_generation != proof_store()->price_generation()) {
                    refuse(QuotientLowerStatus::StaleModel,
                        "row source, price, request or action evidence is stale");
                    return out;
                }
                for (const auto& dependency : binding.targets) {
                    const auto& target = cells_.at(dependency.cell_id);
                    if (target.target_generation != dependency.generation) {
                        refuse(QuotientLowerStatus::StaleModel,
                            "row target generation is stale");
                        return out;
                    }
                    if (!known[target.state]) {
                        out.reason = "row target has no modeled value or independent frontier";
                        return out;
                    }
                }
                ExactMass mass;
                double upper = 0.0;
                for (std::uint32_t i = 0; i < sparse.transition_count; ++i) {
                    const double p = transition_cache_.probabilities[
                        sparse.transition_offset + i];
                    mass.add(p);
                    upper = add_up(upper, p);
                }
                for (std::uint32_t i = 0; i < sparse.choice_count; ++i) {
                    const double p = transition_cache_.choices[
                        sparse.choice_offset + i].probability;
                    mass.add(p);
                    upper = add_up(upper, p);
                }
                if (!(upper > 0.0) || !std::isfinite(upper) ||
                    (query.coefficients == LowerCoefficientModel::ExactBinaryModel &&
                     !mass.one())) {
                    refuse(QuotientLowerStatus::NumericInconclusive,
                        "declared exact binary probability mass is not one");
                    return out;
                }
                mass_upper[constraint.row] = mass.one() ? 1.0 : upper;
            }
        }

        const auto rhs = [&](const QuotientLowerConstraint& constraint,
                             const std::vector<double>& x) {
            if (constraint.kind == LowerConstraintKind::Scalar)
                return constraint.lower;
            if (constraint.kind == LowerConstraintKind::Inapplicable)
                return std::numeric_limits<double>::infinity();
            const auto& row = transition_cache_.rows[constraint.row];
            double future = 0.0;
            for (std::uint32_t i = 0; i < row.transition_count; ++i) {
                const auto offset = row.transition_offset + i;
                future = add_down(future, mul_down(
                    transition_cache_.probabilities[offset],
                    x[transition_cache_.successors[offset]]));
            }
            /* The sparse owner embeds ordinary self transitions. Choices
             * store self separately; it remains eligible with finite offers. */
            for (std::uint32_t i = 0; i < row.choice_count; ++i) {
                const auto& choice = transition_cache_.choices[row.choice_offset + i];
                double best = choice.has_self ? x[row.owner_state] :
                    std::numeric_limits<double>::infinity();
                for (std::uint32_t j = 0; j < choice.successor_count; ++j)
                    best = std::min(best, x[transition_cache_.choice_successors[
                        choice.successor_offset + j]]);
                future = add_down(future, mul_down(choice.probability, best));
            }
            if (query.coefficients == LowerCoefficientModel::NormalizedStoredReference &&
                mass_upper[constraint.row] != 1.0)
                future = down(future / mass_upper[constraint.row]);
            return add_down(priced_rows_[constraint.row].cost, future);
        };
        const auto feasible = [&](const std::vector<double>& x) {
            if (x.size() != count) return false;
            for (std::uint32_t state = 0; state < count; ++state) {
                if (!std::isfinite(x[state]) || x[state] < 0.0) return false;
                if (!sources[state]) {
                    if (x[state] != values[state]) return false;
                    continue;
                }
                for (const auto& constraint : sources[state]->constraints) {
                    if (constraint.kind == LowerConstraintKind::Inapplicable) continue;
                    if (constraint.kind == LowerConstraintKind::Scalar) {
                        if (x[state] > constraint.lower) return false;
                        continue;
                    }
                    const auto& row = transition_cache_.rows[constraint.row];
                    const double cost = priced_rows_[constraint.row].cost;
                    const bool normalize = query.coefficients ==
                        LowerCoefficientModel::NormalizedStoredReference;
                    ExactDot left, right;
                    if (!normalize) {
                        left.add_product(x[state], 1.0);
                        right.add_product(cost, 1.0);
                    }
                    const auto term = [&](double p, double future) {
                        if (normalize) {
                            // Cross-multiply by the EXACT positive row mass.
                            left.add_product(p, x[state]);
                            right.add_product(p, cost);
                        }
                        right.add_product(p, future);
                    };
                    for (std::uint32_t i = 0; i < row.transition_count; ++i) {
                        const auto offset = row.transition_offset + i;
                        term(transition_cache_.probabilities[offset],
                             x[transition_cache_.successors[offset]]);
                    }
                    for (std::uint32_t i = 0; i < row.choice_count; ++i) {
                        const auto& choice = transition_cache_.choices[row.choice_offset + i];
                        double best = choice.has_self ? x[state] :
                            std::numeric_limits<double>::infinity();
                        for (std::uint32_t j = 0; j < choice.successor_count; ++j)
                            best = std::min(best, x[transition_cache_.choice_successors[
                                choice.successor_offset + j]]);
                        term(choice.probability, best);
                    }
                    if (!left.at_most(right)) return false;
                }
            }
            return true;
        };
        if (candidate) {
            out.candidate_values_by_state = *candidate;
        } else {
            auto proposal = values; // Zero on modeled states; no x >= L0.
            for (std::uint32_t sweep = 0; sweep < budget.max_sweeps; ++sweep) {
                if (cancelled()) {
                    refuse(QuotientLowerStatus::Cancelled, "cancelled between complete sweeps");
                    return out;
                }
                bool changed = false;
                for (std::uint32_t state = 0; state < count; ++state) {
                    if (!sources[state]) continue;
                    double best = std::numeric_limits<double>::infinity();
                    for (const auto& constraint : sources[state]->constraints) {
                        double value;
                        if (constraint.kind == LowerConstraintKind::Row) {
                            std::uint32_t work = 0;
                            value = solve_detail::evaluate_sparse_policy_row(
                                transition_cache_, priced_rows_, proposal,
                                constraint.row, work);
                        } else value = rhs(constraint, proposal);
                        best = std::min(best, value);
                    }
                    // Closed zero-cost components may have no finite proposal.
                    if (!std::isfinite(best)) best = 0.0;
                    changed = changed || best != proposal[state];
                    proposal[state] = best;
                }
                ++out.sweeps;
                if (!changed) { out.candidate_stable = true; break; }
            }
            out.candidate_values_by_state = std::move(proposal);
        }
        auto accepted = out.candidate_values_by_state;
        bool checked = feasible(accepted);
        if (!checked && !candidate) {
            /* A proposal repair, not an acceptance tolerance. Every repaired
             * vector is checked against ALL raw inequalities afterwards. */
            for (std::uint32_t state = 0; state < count; ++state)
                if (sources[state]) accepted[state] *= 1.0 - 1e-10;
            checked = feasible(accepted);
            if (!checked) {
                accepted = values;
                checked = feasible(accepted);
                out.reason = "proposal refused; checked zero modeled-state fallback";
            }
        }
        if (!checked) {
            release_diagnostics();
            refuse(QuotientLowerStatus::NumericInconclusive,
                "exact raw inequalities refuse candidate; no residual tolerance");
            return out;
        }
        const bool cancelled_at_acceptance = cancelled();
        if (cancelled_at_acceptance || model_revision_ != query.model_revision) {
            release_diagnostics();
            refuse(cancelled_at_acceptance ? QuotientLowerStatus::Cancelled :
                QuotientLowerStatus::StaleModel, "query changed before acceptance");
            return out;
        }
        for (const auto& source : query.sources)
            for (const auto& constraint : source.constraints)
                if (constraint.kind != LowerConstraintKind::Inapplicable)
                    out.ranked_constraints.push_back({source.cell_id,
                        constraint.cover, constraint.kind, rhs(constraint, accepted)});
        std::sort(out.ranked_constraints.begin(), out.ranked_constraints.end(),
            [](const auto& a, const auto& b) {
                return std::tie(a.cell_id, a.rhs_lower, a.cover.identity) <
                    std::tie(b.cell_id, b.rhs_lower, b.cover.identity);
            });
        out.storage_charge = std::make_shared<ResultStorageCharge>(proof_store(),
            bytes); // Conservative bound on diagnostics, retained by copies.
        out.checked.reset(new QuotientLowerCertificate(
            proof_store(), query.request_identity, quotient_lower_model_identity(query),
            model_revision_, query.coefficients, std::move(accepted)));
        out.status = QuotientLowerStatus::CheckedFiniteLower;
        return out;
    } catch (const ProofMemoryLimit&) {
        release_diagnostics();
        out.checked.reset();
        refuse(QuotientLowerStatus::ResourceCap, "proof memory reservation refused");
        return out;
    }
}

} // namespace poecraft::solver::quotient
