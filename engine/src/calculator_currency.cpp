#include "calculator_currency.hpp"
#include "currency_outcomes.hpp"
#include "poecraft/bitset.h"

#include <cmath>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>

namespace poecraft::solver {
std::string calculate_currency_json(const CalcContext& source,
        const pc_item_state& receiver, const std::string& action,
        const SessionImpl* donor_session, const pc_item_state* donor) {
    if (action != "awakener" && action != "dominance" && action != "vaal")
        throw std::invalid_argument("Unknown single-action currency");
    if (receiver.memory_strands || receiver.lifecycle != PC_ITEM_LIVE)
        throw std::invalid_argument("Calculation requires a live item without memory strands");
    const auto& session = source.session();
    // Include concrete retained/upgrade tiers and all influence signatures.
    // The refill kernel builds its pool from the concrete item, so no donor
    // identity or retained conflict group is reconstructed from coarse junk.
    std::vector<std::uint64_t> reachable(session.words, 0);
    for (std::uint32_t mod = 0; mod < session.mod_count; ++mod)
        if (session.gen_type[mod] <= 1) pc_bitset_set(reachable.data(), mod);
    // There is no continuation after this observation. The native refill DP
    // retains complete physical exclusion groups through every draw; only its
    // terminal rows merge junk that has identical goal/count/flag observations.
    // Admitting Chaos as a future action here would unnecessarily distinguish
    // hundreds of thousands of final junk configurations on jewellery.
    CalcContext calc(source.shared_session(), source.goal(), source.registry(),
        {}, true, false, false, std::nullopt, {}, false, reachable,
        false, false, true, false, true);
    // Oversized requests fail explicitly; never publish truncated mass.
    calc.set_solve_resource_caps(250000, 100000000, false, 512ull * 1024 * 1024);
    std::map<std::uint32_t, long double> mass;
    std::map<std::uint32_t, long double> implicit_present;
    std::map<std::uint32_t, std::uint64_t> implicit_weights;
    std::uint64_t total_implicit_weight = 0;
    const auto observe_implicits = [&](const pc_item_state& item, long double p) {
        std::set<std::uint32_t> ids;
        for (unsigned i = 0; i < item.implicit_count; ++i) ids.insert(item.implicits[i].mod_id);
        for (const auto id : ids) implicit_present[id] += p;
    };
    const auto project = [&](pc_item_state item) {
        // These three mechanics retain enchantments. Only explicit structure
        // is observed here; no enchantment/socket continuation is asserted.
        item.enchantment_count = 0;
        return calc.intern_item(item);
    };
    const auto add = [&](const pc_item_state& item, long double p) {
        mass[project(item)] += p;
    };
    const auto refill = [&](pc_item_state base, long double p,
                            std::uint8_t target, bool blocks, bool clear) {
        base.enchantment_count = 0;
        const auto distribution = calc.concrete_refill({base, target, blocks, clear});
        if (!distribution->supported || !distribution->applicable)
            throw std::invalid_argument("Exact currency refill is unavailable for this input");
        for (const auto& entry : distribution->entries)
            mass[entry.state] += p * entry.probability;
    };
    bool legal = !(receiver.item_flags & (PC_ITEM_CORRUPTED | PC_ITEM_MIRRORED));
    if (action == "awakener") {
        if (!donor_session || !donor)
            throw std::invalid_argument("Choose an Awakener donor from Stash");
        const auto choices = awakener_choices(session, *donor_session, *donor, receiver);
        const long double p = 1.0L / (choices.donor.size() * choices.receiver.size());
        for (const auto a : choices.donor) for (const auto b : choices.receiver)
            refill(awakener_base(session, *donor, receiver, a, b), p, 0, false, false);
    } else if (!legal) {
        add(receiver, 1);
    } else if (action == "dominance") {
        const auto choices = dominance_choices(session, receiver);
        legal = choices.size() >= 2;
        if (!legal) add(receiver, 1);
        else {
            const long double p = 1.0L / (choices.size() * (choices.size() - 1));
            for (std::size_t a = 0; a < choices.size(); ++a)
                for (std::size_t b = 0; b < choices.size(); ++b)
                    if (a != b) add(dominance_result(session, receiver, choices[a], choices[b]), p);
        }
    } else {
        const auto weights = vaal_implicit_weights(session);
        for (const auto& [id, weight] : weights) {
            implicit_weights[id] = weight;
            total_implicit_weight += weight;
        }
        auto unchanged = receiver;
        unchanged.item_flags |= PC_ITEM_CORRUPTED;
        add(unchanged, 0.5L); // unchanged + ignored socket branch
        observe_implicits(unchanged, 0.75L); // reforge also retains implicits
        refill(unchanged, 0.25L, 6, true, true);
        const auto replacements = std::max<unsigned>(1, receiver.implicit_count);
        for (const auto& [id, weight] : weights) {
            const long double p = 0.25L * weight / total_implicit_weight / replacements;
            for (unsigned removed = 0; removed < replacements; ++removed) {
                const auto next = vaal_implicit_result(session, receiver, id, removed);
                add(next, p);
                observe_implicits(next, p);
            }
        }
    }
    long double total = 0, success = 0;
    std::array<long double, kMaxGoalSlots> slots{};
    for (const auto& [id, p] : mass) {
        total += p;
        const auto& state = calc.state(id);
        if (legal && calc.is_goal_state(state)) success += p;
        for (std::size_t i = 0; i < slots.size(); ++i)
            if (state.slot_status[i] == 2) slots[i] += p;
    }
    if (std::abs(total - 1.0L) > 1e-10L)
        throw std::logic_error("Currency calculation failed probability conservation");
    std::ostringstream out;
    out << std::setprecision(17) << "{\"ok\":true,\"supported\":true,\"legal\":"
        << (legal ? "true" : "false") << ",\"success_probability\":" << double(success)
        << ",\"slot_satisfied\":[";
    for (std::size_t i = 0; i < slots.size(); ++i) out << (i ? "," : "") << double(slots[i]);
    out << "],\"outcomes\":[";
    bool comma = false;
    for (const auto& [id, p] : mass) {
        const auto& state = calc.state(id);
        out << (comma ? "," : "") << "{\"state\":" << id
            << ",\"probability\":" << double(p) << ",\"rarity\":" << unsigned(state.rarity)
            << ",\"prefixes\":" << unsigned(state.prefix_count)
            << ",\"suffixes\":" << unsigned(state.suffix_count)
            << ",\"flags\":" << state.flags << ",\"blocked\":" << state.blocked_mask
            << ",\"is_goal\":" << (calc.is_goal_state(state) ? "true" : "false") << ",\"slots\":[";
        for (std::size_t i = 0; i < slots.size(); ++i) out << (i ? "," : "") << unsigned(state.slot_status[i]);
        out << "]}";
        comma = true;
    }
    out << "]";
    if (action == "vaal" && legal) {
        out << ",\"vaal_branches\":{\"implicit\":0.25,\"sockets\":0.25,\"reforge\":0.25,\"unchanged\":0.25},\"implicit_outcomes\":[";
        comma = false;
        for (const auto& [id, p] : implicit_present) {
            const auto weight = implicit_weights[id];
            out << (comma ? "," : "") << "{\"mod\":" << id << ",\"weight\":" << weight
                << ",\"added_probability\":" << double(0.25L * weight / total_implicit_weight)
                << ",\"present_probability\":" << double(p) << "}";
            comma = true;
        }
        out << "]";
    }
    out << "}";
    return out.str();
}
}
