#include "solver_quotient_partition.hpp"

#include "solver_solve_types.hpp"

#include <algorithm>
#include <bit>
#include <cmath>
#include <map>
#include <numeric>
#include <set>
#include <tuple>
#include <utility>

namespace poecraft {
namespace solver {
namespace quotient {

namespace {

using solve_detail::WideFloat;

constexpr std::uint64_t kInternalLabel = 0x706371696e7431ull;
constexpr std::uint64_t kExternalLabel = 0x70637165787431ull;
constexpr std::uint64_t kCellIdentity = 0x70637163656c31ull;

using Locator = std::pair<StableKey, std::uint64_t>;

void append_key(StableKey& out, const StableKey& value) {
    out.push_back(value.size());
    out.insert(out.end(), value.begin(), value.end());
}

void fail(
        QuotientPartitionResult& result,
        const QuotientPartitionStatus status,
        std::string reason) {
    result.status = status;
    result.failure_reason = std::move(reason);
}

bool finite_probability(const double value) {
    return std::isfinite(value) && value >= 0.0;
}

StableKey encoded_internal_label(const StableKey& label) {
    StableKey out{kInternalLabel};
    append_key(out, label);
    return out;
}

StableKey encoded_external_label(
        const StableKey& label,
        const StableKey& frontier) {
    StableKey out{kExternalLabel};
    append_key(out, label);
    append_key(out, frontier);
    return out;
}

bool read_key(
        const StableKey& encoded,
        std::size_t& offset,
        StableKey& value) {
    if (offset >= encoded.size()) return false;
    const std::uint64_t count = encoded[offset++];
    if (count > encoded.size() - offset) return false;
    value.assign(
        encoded.begin() + static_cast<std::ptrdiff_t>(offset),
        encoded.begin() + static_cast<std::ptrdiff_t>(offset + count));
    offset += static_cast<std::size_t>(count);
    return true;
}

bool decode_partition_label(
        const StableKey& encoded,
        StableKey& label,
        std::optional<StableKey>& frontier) {
    if (encoded.empty()) return false;
    std::size_t offset = 1;
    if (!read_key(encoded, offset, label)) return false;
    if (encoded.front() == kInternalLabel) {
        frontier.reset();
        return offset == encoded.size();
    }
    if (encoded.front() != kExternalLabel) return false;
    StableKey external;
    if (!read_key(encoded, offset, external) || external.empty() ||
        offset != encoded.size()) {
        return false;
    }
    frontier = std::move(external);
    return true;
}

StableKey cell_semantic_identity(const QuotientCell& cell) {
    StableKey out{kCellIdentity, cell.coarse_state, cell.terminal ? 1u : 0u};
    append_key(out, cell.coarse_parent);
    append_key(
        out,
        refinement::canonical_observation_identity(
            cell.coarse_parent,
            cell.observation_requirement,
            cell.observed_features));
    append_key(out, cell.immediate_identity);
    append_key(out, cell.coverage.strict_kernel_identity);
    append_key(out, cell.coverage.replay_authority_identity);
    append_key(out, cell.coverage.normalized_enumeration_identity);
    out.push_back(cell.coverage.ranges.size());
    for (const CoverageRange& range : cell.coverage.ranges) {
        append_key(out, range.range_identity);
        out.push_back(range.begin);
        out.push_back(range.count);
        out.push_back(
            std::bit_cast<std::uint64_t>(range.total_probability));
    }
    out.push_back(cell.coverage.exact_source_count);
    out.push_back(std::bit_cast<std::uint64_t>(
        cell.coverage.exact_total_probability));
    return out;
}

struct PreparedNode {
    CertifiedCarrierNode node;
    FeatureSignature observed;
    StableKey observation_identity;
    const CoverageCarrier* carrier = nullptr;
};

CoverageDescriptor class_coverage(
        const CoverageDescriptor& authority,
        std::vector<const CoverageCarrier*> carriers) {
    std::sort(
        carriers.begin(), carriers.end(),
        [](const CoverageCarrier* left, const CoverageCarrier* right) {
            return std::tie(
                       left->range_identity, left->enumeration_index) <
                   std::tie(
                       right->range_identity, right->enumeration_index);
        });
    CoverageDescriptor out;
    out.strict_kernel_identity = authority.strict_kernel_identity;
    out.replay_authority_identity = authority.replay_authority_identity;
    out.normalized_enumeration_identity =
        authority.normalized_enumeration_identity;
    WideFloat total{0.0};
    for (std::size_t begin = 0; begin < carriers.size();) {
        std::size_t end = begin + 1;
        WideFloat range_total{carriers[begin]->probability};
        while (end < carriers.size() &&
               carriers[end]->range_identity ==
                   carriers[begin]->range_identity &&
               carriers[end]->enumeration_index ==
                   carriers[end - 1]->enumeration_index + 1) {
            range_total += WideFloat{carriers[end]->probability};
            ++end;
        }
        out.ranges.push_back({
            carriers[begin]->range_identity,
            carriers[begin]->enumeration_index,
            end - begin,
            range_total.value()});
        total += range_total;
        begin = end;
    }
    out.exact_source_count = carriers.size();
    out.exact_total_probability = total.value();
    return canonical_coverage_descriptor(std::move(out));
}

FeatureSignature relevant_difference(
        const FeatureSignature& left,
        const FeatureSignature& right) {
    FeatureSignature result;
    for (const refinement::FeatureAtom& atom : left) {
        if (std::find(right.begin(), right.end(), atom) == right.end()) {
            result.push_back(atom);
        }
    }
    for (const refinement::FeatureAtom& atom : right) {
        if (std::find(left.begin(), left.end(), atom) == left.end()) {
            result.push_back(atom);
        }
    }
    return refinement::canonical_feature_signature(std::move(result));
}

struct PreviousInterval {
    std::uint64_t begin = 0;
    std::uint64_t end = 0;
    std::uint32_t cell_id = 0;
};

std::optional<std::uint32_t> previous_cell_for(
        const std::map<StableKey, std::vector<PreviousInterval>>& intervals,
        const CoverageCarrier& carrier) {
    const auto range = intervals.find(carrier.range_identity);
    if (range == intervals.end()) return std::nullopt;
    const auto found = std::upper_bound(
        range->second.begin(), range->second.end(),
        carrier.enumeration_index,
        [](const std::uint64_t index, const PreviousInterval& interval) {
            return index < interval.begin;
        });
    if (found == range->second.begin()) return std::nullopt;
    const PreviousInterval& candidate = *std::prev(found);
    if (carrier.enumeration_index >= candidate.end) return std::nullopt;
    return candidate.cell_id;
}

} // namespace

const QuotientCell* QuotientPartitionState::find_cell(
        const std::uint32_t cell_id) const {
    const auto found = std::lower_bound(
        cells.begin(), cells.end(), cell_id,
        [](const QuotientCell& cell, const std::uint32_t id) {
            return cell.cell_id < id;
        });
    return found != cells.end() && found->cell_id == cell_id
               ? &*found
               : nullptr;
}

StableKey canonical_quotient_cell_identity(const QuotientCell& cell) {
    return cell_semantic_identity(cell);
}

QuotientPartitionResult refine_certified_quotient_partition(
        const CoverageDescriptor& coverage_descriptor,
        const std::vector<CoverageCarrier>& exact_coverage,
        std::vector<CertifiedCarrierNode> nodes,
        std::vector<CertifiedEntry> entries,
        const QuotientPartitionState* previous,
        ProofStore* proof_store,
        const refinement::ClosedPartitionLimits limits) {
    QuotientPartitionResult result;
    if (nodes.empty()) {
        fail(
            result, QuotientPartitionStatus::EmptySlice,
            "certified quotient partition slice is empty");
        return result;
    }

    CoverageDescriptor canonical_coverage;
    try {
        canonical_coverage =
            canonical_coverage_descriptor(coverage_descriptor);
    } catch (const std::exception& error) {
        fail(
            result, QuotientPartitionStatus::InvalidCoverage,
            error.what());
        return result;
    }
    if (canonical_coverage != coverage_descriptor ||
        exact_coverage.size() != canonical_coverage.exact_source_count) {
        fail(
            result, QuotientPartitionStatus::InvalidCoverage,
            "coverage replay does not match its canonical descriptor");
        return result;
    }

    std::map<Locator, const CoverageCarrier*> carrier_by_locator;
    std::map<StableKey, const CoverageCarrier*> carrier_by_key;
    WideFloat coverage_total{0.0};
    for (const CoverageCarrier& carrier : exact_coverage) {
        if (carrier.stable_key.empty() || carrier.range_identity.empty() ||
            !finite_probability(carrier.probability) ||
            !carrier_by_locator.emplace(
                Locator{carrier.range_identity, carrier.enumeration_index},
                &carrier).second ||
            !carrier_by_key.emplace(carrier.stable_key, &carrier).second) {
            fail(
                result, QuotientPartitionStatus::InvalidCoverage,
                "coverage replay contains an invalid or duplicate carrier");
            return result;
        }
        coverage_total += WideFloat{carrier.probability};
    }
    for (const CoverageRange& range : canonical_coverage.ranges) {
        WideFloat range_total{0.0};
        for (std::uint64_t index = range.begin;
             index < range.begin + range.count; ++index) {
            const auto carrier = carrier_by_locator.find(
                Locator{range.range_identity, index});
            if (carrier == carrier_by_locator.end()) {
                fail(
                    result, QuotientPartitionStatus::InvalidCoverage,
                    "coverage replay omits a declared enumeration position");
                return result;
            }
            range_total += WideFloat{carrier->second->probability};
        }
        if (range_total.value() != range.total_probability) {
            fail(
                result, QuotientPartitionStatus::InvalidCoverage,
                "coverage replay range mass does not reconcile");
            return result;
        }
    }
    if (coverage_total.value() !=
        canonical_coverage.exact_total_probability) {
        fail(
            result, QuotientPartitionStatus::InvalidCoverage,
            "coverage replay total mass does not reconcile");
        return result;
    }
    result.telemetry.exact_carriers_replayed = exact_coverage.size();
    result.telemetry.exact_coverage_mass = coverage_total.value();

    std::map<StableKey, std::uint32_t> node_by_key;
    for (std::uint32_t index = 0; index < nodes.size(); ++index) {
        if (nodes[index].stable_key.empty() ||
            !node_by_key.emplace(nodes[index].stable_key, index).second) {
            fail(
                result, QuotientPartitionStatus::InvalidCarrierGraph,
                "certified carrier graph has an empty or duplicate key");
            return result;
        }
    }
    if (node_by_key.size() != carrier_by_key.size()) {
        fail(
            result, QuotientPartitionStatus::InvalidCoverage,
            "coverage replay and carrier graph populations differ");
        return result;
    }

    std::vector<PreparedNode> prepared;
    std::vector<refinement::ClosedPartitionNode> closed;
    prepared.reserve(nodes.size());
    closed.reserve(nodes.size());
    for (CertifiedCarrierNode& node : nodes) {
        const auto coverage = carrier_by_key.find(node.stable_key);
        if (coverage == carrier_by_key.end() || node.coarse_parent.empty()) {
            fail(
                result, QuotientPartitionStatus::InvalidCoverage,
                "carrier node is absent from coverage or has no parent");
            return result;
        }
        node.observation_requirement =
            refinement::canonical_observation_requirement(
                std::move(node.observation_requirement));
        node.exact_features = refinement::canonical_feature_signature(
            std::move(node.exact_features));
        FeatureSignature observed = refinement::canonical_feature_signature(
            refinement::observe_features(
                node.exact_features, node.observation_requirement));
        StableKey observation = refinement::canonical_observation_identity(
            node.coarse_parent,
            node.observation_requirement,
            node.exact_features);
        if (observation.empty() ||
            (!node.terminal && node.immediate_identity.empty())) {
            fail(
                result, QuotientPartitionStatus::InvalidCarrierGraph,
                "carrier node lacks observation or immediate identity");
            return result;
        }
        if (node.terminal && !node.arcs.empty()) {
            fail(
                result, QuotientPartitionStatus::InvalidCarrierGraph,
                "terminal carrier has outgoing arcs");
            return result;
        }

        refinement::ClosedPartitionNode shared;
        shared.stable_key = node.stable_key;
        shared.observation_key = observation;
        shared.immediate_key = node.terminal
                                   ? StableKey{0x7063717465726d31ull}
                                   : node.immediate_identity;
        shared.terminal = node.terminal;
        shared.arcs.reserve(node.arcs.size());
        for (const CertifiedCarrierArc& arc : node.arcs) {
            if (!finite_probability(arc.probability) ||
                arc.internal_successor.has_value() ==
                    arc.external_frontier.has_value()) {
                fail(
                    result, QuotientPartitionStatus::OpenFrontier,
                    "carrier arc must name exactly one closed or external target");
                return result;
            }
            if (arc.internal_successor.has_value()) {
                const auto successor =
                    node_by_key.find(*arc.internal_successor);
                if (successor == node_by_key.end()) {
                    fail(
                        result, QuotientPartitionStatus::OpenFrontier,
                        "carrier arc names an unknown internal successor");
                    return result;
                }
                shared.arcs.push_back({
                    encoded_internal_label(arc.label),
                    successor->second,
                    arc.probability});
            } else {
                if (arc.external_frontier->empty()) {
                    fail(
                        result, QuotientPartitionStatus::OpenFrontier,
                        "external frontier identity is empty");
                    return result;
                }
                shared.arcs.push_back({
                    encoded_external_label(
                        arc.label, *arc.external_frontier),
                    std::nullopt,
                    arc.probability});
            }
        }
        prepared.push_back({
            std::move(node), std::move(observed),
            std::move(observation), coverage->second});
        closed.push_back(std::move(shared));
    }

    refinement::ClosedPartitionResult partition =
        refinement::refine_closed_probabilistic_partition(
            std::move(closed), limits);
    result.shared_partition_status = partition.status;
    result.telemetry.initial_classes = partition.initial_class_count;
    result.telemetry.final_classes = partition.final_class_count;
    result.telemetry.refinement_rounds = partition.rounds;
    result.telemetry.partition_estimated_bytes =
        partition.estimated_memory_bytes;
    result.telemetry.partition_peak_estimated_bytes =
        partition.peak_estimated_memory_bytes;
    if (partition.status != refinement::ClosedPartitionStatus::Complete) {
        fail(
            result,
            partition.status == refinement::ClosedPartitionStatus::ResourceCap
                ? QuotientPartitionStatus::ResourceCap
                : QuotientPartitionStatus::SharedPartitionFailure,
            partition.failure_reason);
        return result;
    }

    std::map<StableKey, std::vector<PreviousInterval>> previous_intervals;
    std::map<std::uint32_t, const QuotientCell*> previous_by_id;
    if (previous != nullptr) {
        std::uint64_t previous_count = 0;
        for (const QuotientCell& cell : previous->cells) {
            if (!previous_by_id.emplace(cell.cell_id, &cell).second ||
                cell.coverage.strict_kernel_identity !=
                    canonical_coverage.strict_kernel_identity ||
                cell.coverage.replay_authority_identity !=
                    canonical_coverage.replay_authority_identity ||
                cell.coverage.normalized_enumeration_identity !=
                    canonical_coverage.normalized_enumeration_identity) {
                fail(
                    result, QuotientPartitionStatus::NonMonotoneRefinement,
                    "previous quotient partition has incompatible identity");
                return result;
            }
            previous_count += cell.coverage.exact_source_count;
            for (const CoverageRange& range : cell.coverage.ranges) {
                previous_intervals[range.range_identity].push_back({
                    range.begin, range.begin + range.count, cell.cell_id});
            }
        }
        if (previous_count > exact_coverage.size()) {
            fail(
                result, QuotientPartitionStatus::NonMonotoneRefinement,
                "previous quotient partition exceeds the live population");
            return result;
        }
        for (auto& [unused, intervals] : previous_intervals) {
            (void)unused;
            std::sort(
                intervals.begin(), intervals.end(),
                [](const PreviousInterval& left,
                   const PreviousInterval& right) {
                    return std::tie(left.begin, left.end, left.cell_id) <
                           std::tie(right.begin, right.end, right.cell_id);
                });
            for (std::size_t index = 1; index < intervals.size(); ++index) {
                if (intervals[index - 1].end > intervals[index].begin) {
                    fail(
                        result,
                        QuotientPartitionStatus::NonMonotoneRefinement,
                        "previous quotient coverage overlaps");
                    return result;
                }
            }
        }
    }

    const std::uint32_t class_count = partition.final_class_count;
    std::vector<std::vector<std::uint32_t>> members(class_count);
    std::vector<std::optional<std::uint32_t>> old_by_class(class_count);
    std::map<std::uint32_t, std::set<std::uint32_t>> children_by_old;
    std::uint64_t matched_previous_carriers = 0;
    for (std::uint32_t node = 0; node < prepared.size(); ++node) {
        const std::uint32_t class_id = partition.class_by_node.at(node);
        members.at(class_id).push_back(node);
        if (previous == nullptr) continue;
        const std::optional<std::uint32_t> old = previous_cell_for(
            previous_intervals, *prepared[node].carrier);
        if (!old.has_value()) continue;
        ++matched_previous_carriers;
        if (old_by_class[class_id].has_value() &&
            *old_by_class[class_id] != *old) {
            fail(
                result, QuotientPartitionStatus::NonMonotoneRefinement,
                "shared partition attempted to merge previous cells");
            return result;
        }
        old_by_class[class_id] = *old;
        children_by_old[*old].insert(class_id);
    }
    if (previous != nullptr &&
        (matched_previous_carriers !=
             std::accumulate(
                 previous->cells.begin(), previous->cells.end(),
                 std::uint64_t{0},
                 [](const std::uint64_t total,
                    const QuotientCell& cell) {
                     return total + cell.coverage.exact_source_count;
                 }) ||
         children_by_old.size() != previous->cells.size())) {
        fail(
            result, QuotientPartitionStatus::NonMonotoneRefinement,
            "new quotient partition omits previous coverage");
        return result;
    }

    result.state.partition_generation =
        previous == nullptr ? 1 : previous->partition_generation + 1;
    result.state.next_cell_id =
        previous == nullptr ? class_count : previous->next_cell_id;
    std::vector<QuotientCell> by_class(class_count);
    for (std::uint32_t class_id = 0; class_id < class_count; ++class_id) {
        if (members[class_id].empty()) {
            fail(
                result, QuotientPartitionStatus::SharedPartitionFailure,
                "shared partition returned an empty quotient class");
            return result;
        }
        const PreparedNode& authority = prepared[members[class_id].front()];
        QuotientCell cell;
        cell.coarse_state = authority.node.coarse_state;
        cell.coarse_parent = authority.node.coarse_parent;
        cell.observation_requirement =
            authority.node.observation_requirement;
        cell.observed_features = authority.observed;
        cell.immediate_identity = authority.node.terminal
                                      ? StableKey{0x7063717465726d31ull}
                                      : authority.node.immediate_identity;
        cell.terminal = authority.node.terminal;
        std::vector<const CoverageCarrier*> class_carriers;
        class_carriers.reserve(members[class_id].size());
        for (const std::uint32_t member : members[class_id]) {
            class_carriers.push_back(prepared[member].carrier);
        }
        try {
            cell.coverage = class_coverage(
                canonical_coverage, std::move(class_carriers));
        } catch (const std::exception& error) {
            fail(
                result, QuotientPartitionStatus::InvalidCoverage,
                error.what());
            return result;
        }
        cell.semantic_identity = cell_semantic_identity(cell);
        by_class[class_id] = std::move(cell);
    }

    std::set<std::uint32_t> split_old_cells;
    std::set<std::uint32_t> replaced_old_cells;
    if (previous == nullptr) {
        for (std::uint32_t class_id = 0;
             class_id < class_count; ++class_id) {
            by_class[class_id].cell_id = class_id;
            by_class[class_id].generation = 1;
            ++result.telemetry.new_cells;
        }
    } else {
        for (const auto& [old, children] : children_by_old) {
            if (children.size() > 1) split_old_cells.insert(old);
        }
        for (std::uint32_t class_id = 0;
             class_id < class_count; ++class_id) {
            if (!old_by_class[class_id].has_value()) {
                by_class[class_id].cell_id =
                    result.state.next_cell_id++;
                by_class[class_id].generation = 1;
                ++result.telemetry.new_cells;
                continue;
            }
            const std::uint32_t old_id = *old_by_class[class_id];
            const QuotientCell* old = previous->find_cell(old_id);
            if (old == nullptr) {
                fail(
                    result, QuotientPartitionStatus::NonMonotoneRefinement,
                    "previous quotient cell id is missing");
                return result;
            }
            if (split_old_cells.contains(old_id)) {
                by_class[class_id].cell_id = result.state.next_cell_id++;
                by_class[class_id].generation = old->generation + 1;
                ++result.telemetry.new_cells;
            } else {
                by_class[class_id].cell_id = old_id;
                by_class[class_id].generation = old->generation;
                if (by_class[class_id].semantic_identity !=
                    old->semantic_identity) {
                    ++by_class[class_id].generation;
                    replaced_old_cells.insert(old_id);
                    ++result.telemetry.superseded_cells;
                } else {
                    ++result.telemetry.retained_cells;
                }
            }
        }
    }

    std::vector<std::uint32_t> cell_id_by_class(class_count);
    for (std::uint32_t class_id = 0; class_id < class_count; ++class_id) {
        cell_id_by_class[class_id] = by_class[class_id].cell_id;
    }
    for (const refinement::ClosedPartitionClass& shared_class :
         partition.classes) {
        QuotientCell& cell = by_class.at(shared_class.class_id);
        cell.arcs.reserve(shared_class.arcs.size());
        for (const refinement::ClosedPartitionProjectedArc& arc :
             shared_class.arcs) {
            StableKey label;
            std::optional<StableKey> frontier;
            if (!decode_partition_label(arc.label, label, frontier)) {
                fail(
                    result, QuotientPartitionStatus::SharedPartitionFailure,
                    "shared partition returned an invalid projected label");
                return result;
            }
            if (arc.successor_class.has_value() == frontier.has_value()) {
                fail(
                    result, QuotientPartitionStatus::SharedPartitionFailure,
                    "shared partition lost target closure provenance");
                return result;
            }
            cell.arcs.push_back({
                std::move(label),
                arc.successor_class.has_value()
                    ? std::optional<std::uint32_t>{
                          cell_id_by_class.at(*arc.successor_class)}
                    : std::nullopt,
                std::move(frontier),
                arc.probability});
        }
    }

    std::sort(
        entries.begin(), entries.end(),
        [](const CertifiedEntry& left, const CertifiedEntry& right) {
            return std::tuple{
                       left.carrier,
                       std::bit_cast<std::uint64_t>(left.probability)} <
                   std::tuple{
                       right.carrier,
                       std::bit_cast<std::uint64_t>(right.probability)};
        });
    std::map<std::uint32_t, WideFloat> entry_by_cell;
    WideFloat entry_total{0.0};
    for (const CertifiedEntry& entry : entries) {
        const auto node = node_by_key.find(entry.carrier);
        if (node == node_by_key.end() ||
            !finite_probability(entry.probability)) {
            fail(
                result, QuotientPartitionStatus::InvalidCarrierGraph,
                "entry distribution contains an invalid carrier");
            return result;
        }
        entry_by_cell[cell_id_by_class[
            partition.class_by_node[node->second]]] +=
            WideFloat{entry.probability};
        entry_total += WideFloat{entry.probability};
    }
    if (std::abs(entry_total.value() - 1.0) >
        limits.probability_sum_tolerance) {
        fail(
            result, QuotientPartitionStatus::InvalidCarrierGraph,
            "entry distribution is not stochastic");
        return result;
    }
    result.telemetry.exact_entry_mass = entry_total.value();
    for (const auto& [cell_id, probability] : entry_by_cell) {
        result.state.entries.push_back({cell_id, probability.value()});
    }

    std::map<std::uint32_t, std::vector<std::uint32_t>> by_coarse;
    for (std::uint32_t node = 0; node < prepared.size(); ++node) {
        by_coarse[prepared[node].node.coarse_state].push_back(node);
    }
    for (auto& [coarse, candidates] : by_coarse) {
        std::sort(
            candidates.begin(), candidates.end(),
            [&](const std::uint32_t left, const std::uint32_t right) {
                return prepared[left].node.stable_key <
                       prepared[right].node.stable_key;
            });
        std::optional<std::pair<std::uint32_t, std::uint32_t>> witness;
        for (std::uint32_t pass = 0; pass < 2 && !witness; ++pass) {
            for (std::size_t left = 0;
                 left < candidates.size() && !witness; ++left) {
                for (std::size_t right = left + 1;
                     right < candidates.size(); ++right) {
                    const std::uint32_t left_node = candidates[left];
                    const std::uint32_t right_node = candidates[right];
                    if (partition.class_by_node[left_node] ==
                        partition.class_by_node[right_node]) {
                        continue;
                    }
                    if (pass == 0 &&
                        partition.initial_class_by_node[left_node] !=
                            partition.initial_class_by_node[right_node]) {
                        continue;
                    }
                    witness = {left_node, right_node};
                    break;
                }
            }
        }
        if (!witness.has_value()) continue;
        const PreparedNode& left = prepared[witness->first];
        const PreparedNode& right = prepared[witness->second];
        refinement::CounterexampleKind kind =
            refinement::CounterexampleKind::SuccessorProjection;
        if (left.observation_identity != right.observation_identity) {
            kind = refinement::CounterexampleKind::Observation;
        } else if (left.node.immediate_identity !=
                   right.node.immediate_identity) {
            kind = refinement::CounterexampleKind::ActionCost;
        }
        result.counterexamples.push_back({
            kind,
            coarse,
            left.node.stable_key,
            right.node.stable_key,
            relevant_difference(left.observed, right.observed)});
    }

    result.state.cells = std::move(by_class);
    std::sort(
        result.state.cells.begin(), result.state.cells.end(),
        [](const QuotientCell& left, const QuotientCell& right) {
            return left.cell_id < right.cell_id;
        });
    for (const QuotientCell& cell : result.state.cells) {
        result.telemetry.retained_coverage_ranges +=
            cell.coverage.ranges.size();
    }

    result.telemetry.source_splits = split_old_cells.size();
    result.telemetry.target_splits = split_old_cells.size();
    result.telemetry.requirement_replacements =
        replaced_old_cells.size();
    if (proof_store != nullptr) {
        for (const std::uint32_t cell : split_old_cells) {
            result.telemetry.source_invalidations +=
                proof_store->invalidate_source(cell);
        }
        for (const std::uint32_t cell : replaced_old_cells) {
            result.telemetry.source_invalidations +=
                proof_store->invalidate_source(cell);
        }
        for (const std::uint32_t cell : split_old_cells) {
            result.telemetry.reverse_invalidations +=
                proof_store->invalidate_target(cell);
        }
        for (const std::uint32_t cell : replaced_old_cells) {
            result.telemetry.reverse_invalidations +=
                proof_store->invalidate_target(cell);
        }
    }

    result.status = QuotientPartitionStatus::Complete;
    result.closed = true;
    result.lumpable = true;
    return result;
}

} // namespace quotient
} // namespace solver
} // namespace poecraft
