#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <chrono>
#include <cstdint>

namespace poecraft::solver::solve_detail {

// Run-local ordering evidence. These numbers never enter a Bellman lower,
// executable boundary or action-retirement ledger. Root acquisition family is
// a semantic feature; no case, base, modifier id or saved answer is a label.
struct DirtyGuidance {
    struct Bucket { double log_residual = 0; std::uint32_t completed = 0; };
    std::array<Bucket, 3> buckets{};
    std::uint64_t version = 0, updates = 0, lookups = 0;
    std::uint64_t lookup_ns = 0, update_ns = 0;

    double predict(const double estimate, const unsigned family, const bool adaptive) {
        const auto begin=std::chrono::steady_clock::now();
        ++lookups;
        const double value=!adaptive || !std::isfinite(estimate) ? estimate :
            estimate * std::exp(buckets.at(family).log_residual);
        lookup_ns+=std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now()-begin).count();
        return value;
    }

    // Only a complete independent evaluation of this frozen controller at
    // this same root calibrates its private-model estimate. A timeout or an
    // unrelated continuation is not a cost label.
    void observe(const double predicted, const double evaluated, const unsigned family) {
        if (!(predicted > 0) || !(evaluated > 0) ||
            !std::isfinite(predicted) || !std::isfinite(evaluated)) return;
        const auto begin=std::chrono::steady_clock::now();
        auto& bucket = buckets.at(family);
        const double residual = std::clamp(std::log(evaluated / predicted), -2.0, 2.0);
        const double gain = 1.0 / (4.0 + std::min(bucket.completed, 12u));
        bucket.log_residual += gain * (residual - bucket.log_residual);
        ++bucket.completed; ++version; ++updates;
        update_ns+=std::chrono::duration_cast<std::chrono::nanoseconds>(
            std::chrono::steady_clock::now()-begin).count();
    }
};

} // namespace poecraft::solver::solve_detail
