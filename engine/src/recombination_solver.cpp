#include "recombination_solver.hpp"
#include "recombination_constraints.hpp"
#include "calculator_currency.hpp"
#include "recombination_calculator.hpp"
#include "json.hpp"
#include "currency_outcomes.hpp"
#include "poecraft/bitset.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <sstream>
#include <iomanip>
#include <tuple>
#include <stdexcept>

namespace poecraft {
namespace {
void require(bool ok, const char* why) { if (!ok) throw std::invalid_argument(why); }
bool finite_cost(double cost) { return std::isfinite(cost) && cost >= 0; }
struct Action {
    RecombDecisionKind kind;
    unsigned argument = 0;
    double cost = 0;
    std::vector<std::pair<unsigned, double>> exits;
    double child_actions = 0;
    std::vector<double> resource_rewards;
};
double reward(const Action& action, unsigned j) {
    if (j == 0) return action.cost;
    if (j == 1) return action.kind == RecombDecisionKind::Recombine;
    if (j == 2) return action.kind == RecombDecisionKind::Discard;
    if (j == 3) return action.child_actions;
    if (!action.resource_rewards.empty()) return action.resource_rewards.at(j-4);
    return action.kind == RecombDecisionKind::Acquire && action.argument == j-4;
}
struct State { std::vector<unsigned> items; std::vector<Action> actions; bool terminal = false; };
struct WorkGuard {
    const RecombSolverRequest& request;
    std::uint64_t spent = 0;
    void tick(std::uint64_t amount = 1) {
        if (request.cancelled && request.cancelled()) throw std::length_error("Inventory solve cancelled");
        if (amount > request.max_work - spent) throw std::length_error("Inventory numerical work cap reached");
        spent += amount;
    }
};

// Canonicalize represented fields, never padding or inactive slot bytes. Slot
// order remains represented: no unproved quotient by a desired-mod mask.
pc_item_state root_item(const pc_item_state& item, const SessionImpl& from, const SessionImpl& root) {
    require(from.data == root.data && from.base_index == root.base_index && from.item_level == root.item_level,
            "Recombination solver outcome leaves the selected base/level scope");
    auto result = item;
    const auto map = [&](pc_mod_slot* slots, unsigned count) {
        for (unsigned i = 0; i < count; ++i) {
            auto& slot = slots[i];
            const auto global = from.data->mod_global_ids.at(from.global_index.at(slot.mod_id));
            slot.mod_id = root.session_id_by_global_id.at(global);
            slot.group_id = static_cast<std::uint16_t>(root.primary_group.at(slot.mod_id));
            for (unsigned j = 0; j < slot.veiled_option_count; ++j) {
                const auto g = from.data->mod_global_ids.at(from.global_index.at(slot.veiled_option_mod_ids[j]));
                slot.veiled_option_mod_ids[j] = root.session_id_by_global_id.at(g);
            }
            if (slot.veiled_chosen_mod_id != PC_MOD_NONE) {
                const auto g = from.data->mod_global_ids.at(from.global_index.at(slot.veiled_chosen_mod_id));
                slot.veiled_chosen_mod_id = root.session_id_by_global_id.at(g);
            }
        }
    };
    map(result.prefixes, result.prefix_count); map(result.suffixes, result.suffix_count);
    map(result.implicits, result.implicit_count); map(result.enchantments, result.enchantment_count);
    return result;
}
bool admissible(const Action& action, const std::vector<bool>& winning) {
    for (const auto& [next, probability] : action.exits)
        if (probability > 0 && !winning.at(next)) return false;
    return !action.exits.empty();
}
// Greatest almost-sure reachability set. A positive branch into a losing state
// cannot be omitted merely because another branch hits the goal.
std::vector<bool> winning_states(const std::vector<State>& states) {
    std::vector<bool> winning(states.size(), true);
    for (;;) {
        std::vector<std::vector<unsigned>> reverse(states.size());
        std::queue<unsigned> pending; std::vector<bool> reaches(states.size(), false);
        for (unsigned s = 0; s < states.size(); ++s) {
            if (states[s].terminal) { reaches[s] = true; pending.push(s); }
            for (const auto& action : states[s].actions) if (admissible(action, winning))
                for (const auto& [t, p] : action.exits) if (p > 0) reverse[t].push_back(s);
        }
        while (!pending.empty()) {
            const auto t = pending.front(); pending.pop();
            for (auto s : reverse[t]) if (!reaches[s]) { reaches[s] = true; pending.push(s); }
        }
        bool changed = false;
        for (unsigned s = 0; s < states.size(); ++s)
            if (winning[s] && !reaches[s]) { winning[s] = false; changed = true; }
        if (!changed) return winning;
    }
}
std::vector<unsigned> bootstrap_policy(const std::vector<State>& states, const std::vector<bool>& winning) {
    const auto none = std::numeric_limits<unsigned>::max();
    std::vector<unsigned> rank(states.size(), none), policy(states.size(), none);
    for (unsigned s = 0; s < states.size(); ++s) if (states[s].terminal) rank[s] = 0;
    for (unsigned depth = 1; depth <= states.size(); ++depth) {
        bool changed = false;
        for (unsigned s = 0; s < states.size(); ++s) if (winning[s] && rank[s] == none) {
            for (unsigned a = 0; a < states[s].actions.size(); ++a) {
                const auto& action = states[s].actions[a]; if (!admissible(action, winning)) continue;
                if (std::any_of(action.exits.begin(), action.exits.end(),
                        [&](auto exit) { return exit.second > 0 && rank[exit.first] < depth; })) {
                    rank[s] = depth; policy[s] = a; changed = true; break;
                }
            }
        }
        if (!changed) break;
    }
    for (unsigned s = 0; s < states.size(); ++s)
        require(!winning[s] || states[s].terminal || policy[s] != none, "Proper policy construction failed");
    return policy;
}
bool policy_is_proper(const std::vector<State>& states, const std::vector<bool>& winning,
        const std::vector<unsigned>& policy) {
    std::vector<std::vector<unsigned>> reverse(states.size());
    std::queue<unsigned> pending; std::vector<bool> reaches(states.size(), false);
    for (unsigned s = 0; s < states.size(); ++s) if (winning[s]) {
        if (states[s].terminal) { reaches[s] = true; pending.push(s); continue; }
        if (policy[s] >= states[s].actions.size()) return false;
        const auto& action = states[s].actions[policy[s]];
        if (!admissible(action, winning)) return false;
        for (const auto& [t, p] : action.exits) if (p > 0) reverse[t].push_back(s);
    }
    while (!pending.empty()) {
        const auto t = pending.front(); pending.pop();
        for (auto s : reverse[t]) if (!reaches[s]) { reaches[s] = true; pending.push(s); }
    }
    for (unsigned s = 0; s < states.size(); ++s) if (winning[s] && !reaches[s]) return false;
    return true;
}
using Values = std::vector<std::vector<double>>;
Values evaluate_policy(const std::vector<State>& states, const std::vector<bool>& winning,
        const std::vector<unsigned>& policy, unsigned acquisitions, WorkGuard& work) {
    require(policy_is_proper(states, winning, policy), "Nonterminating inventory policy refused");
    std::vector<unsigned> active; std::vector<int> index(states.size(), -1);
    for (unsigned s = 0; s < states.size(); ++s) if (winning[s] && !states[s].terminal) {
        index[s] = int(active.size()); active.push_back(s);
    }
    const unsigned n = static_cast<unsigned>(active.size()), rhs = acquisitions + 4;
    Values matrix(n, std::vector<double>(n + rhs, 0));
    for (unsigned i = 0; i < n; ++i) {
        const auto& action = states[active[i]].actions.at(policy[active[i]]);
        matrix[i][i] = 1;
        for (unsigned j=0;j<rhs;++j) matrix[i][n+j] = reward(action,j);
        for (const auto& [t, p] : action.exits) if (index[t] >= 0) matrix[i][unsigned(index[t])] -= p;
    }
    // Solve (I-P) for costs and all resource counters using the same factorization.
    for (unsigned column = 0; column < n; ++column) {
        work.tick(n + rhs);
        unsigned pivot = column;
        for (unsigned row = column + 1; row < n; ++row)
            if (std::abs(matrix[row][column]) > std::abs(matrix[pivot][column])) pivot = row;
        require(std::abs(matrix[pivot][column]) > 1e-13, "Inventory policy evaluation is numerically unresolved");
        std::swap(matrix[pivot], matrix[column]);
        const double divisor = matrix[column][column];
        for (unsigned j = column; j < n + rhs; ++j) matrix[column][j] /= divisor;
        for (unsigned row = 0; row < n; ++row) if (row != column) {
            const double factor = matrix[row][column]; if (!factor) continue;
            work.tick(n + rhs - column);
            for (unsigned j = column; j < n + rhs; ++j) matrix[row][j] -= factor * matrix[column][j];
        }
    }
    Values values(states.size(), std::vector<double>(rhs, 0));
    for (unsigned i = 0; i < n; ++i) for (unsigned j = 0; j < rhs; ++j) {
        const double v = matrix[i][n + j];
        require(std::isfinite(v) && v >= -1e-8, "Invalid inventory policy expectation");
        values[active[i]][j] = std::max(0.0, v);
    }
    for (unsigned s : active) {
        const auto& action = states[s].actions[policy[s]];
        for (unsigned j = 0; j < rhs; ++j) {
            double expected = reward(action,j);
            for (const auto& [t, p] : action.exits) expected += p * values[t][j];
            require(std::abs(expected - values[s][j]) <= 1e-8 * std::max(1.0, values[s][j]),
                    "Inventory fixed-policy residual exceeds numerical acceptance");
        }
    }
    return values;
}
}
std::shared_ptr<const RecombCheckedFeeder> check_recomb_feeder(
        std::shared_ptr<const SessionImpl> session, const std::string& id, const std::string& revision,
        const std::string& document, const std::string& output_id, double paid_start,
        std::shared_ptr<const EconomyImpl> economy, const std::function<bool()>& cancelled) {
    require(session && economy && !economy->id.empty() && !id.empty() && !revision.empty() && finite_cost(paid_start),
            "Checked feeder requires pinned identity, prices and paid starting item");
    require(document.size()<=256*1024, "Checked feeder document byte cap reached");
    auto strategy = compile_strategy_json(session,document.data(),document.size());
    require(strategy->resources.empty() && strategy->nodes.size()<=256, "Checked feeder excludes nested inventory children");
    const auto selected = std::find_if(strategy->output_contracts.begin(),strategy->output_contracts.end(),
        [&](const auto& c){return c.id==output_id;});
    require(selected!=strategy->output_contracts.end() && selected->resource_id=="current",
            "Checked feeder requires a single current-item output contract");
    RecombSolverRequest bounds; bounds.cancelled=cancelled; WorkGuard work{bounds};
    std::vector<State> graph;
    std::vector<pc_item_state> items;
    std::vector<unsigned> nodes;
    std::map<std::pair<unsigned,std::string>,unsigned> ids;
    const auto intern = [&](unsigned node,const pc_item_state& item) {
        work.tick(); validate_recombination_item_structure({"child","current",session,item});
        for(unsigned side=0;side<2;++side) {
            const auto* slots=side?item.suffixes:item.prefixes;const auto count=side?item.suffix_count:item.prefix_count;
            for(unsigned i=0;i<count;++i)require(!(slots[i].flags&PC_MOD_SLOT_VEILED) && slots[i].veiled_option_count==0 &&
                slots[i].veiled_chosen_mod_id==PC_MOD_NONE,"Checked feeder excludes random initial unveil offers");
        }
        auto key=std::make_pair(node,random_recomb_item_key(item));
        if (auto f=ids.find(key);f!=ids.end()) return f->second;
        if (graph.size()>=256) throw std::length_error("Checked feeder full-item state cap reached");
        const auto index=unsigned(graph.size()); ids.emplace(key,index);
        graph.push_back({{}, {}, false}); items.push_back(item); nodes.push_back(node); return index;
    };
    const auto root=intern(strategy->start_node,strategy->start_item);
    std::vector<std::string> keys;
    for (const auto& node : strategy->nodes) {
        require(!node.source_only && node.input_edges[0].empty() && node.input_edges[1].empty(), "Checked feeder excludes item supplies");
        if (node.kind == StrategyNodeKind::Operation) {
            require(node.action_type==int(ActionType::Annul) || node.action_type==int(ActionType::Scour) ||
                    node.action_type==int(ActionType::RemoveCraftedModifiers), "Checked feeder action has no full represented output law");
            for (const auto& key:node.price_keys) if (std::find(keys.begin(),keys.end(),key)==keys.end()) keys.push_back(key);
        }
    }
    ActionContextImpl context(0); context.session=session;
    for (unsigned st=0;st<graph.size();++st) {
        work.tick(); const auto item=items[st]; const auto& node=strategy->nodes[nodes[st]];
        if (node.kind==StrategyNodeKind::Terminal) {
            require(node.terminal_kind==PC_TERMINAL_SUCCESS && item.lifecycle==PC_ITEM_LIVE &&
                    selected->base_key==session->data->string_at(session->data->base_metadata_path_sid[session->base_index]) &&
                    evaluate_compiled_condition(selected->predicate,*session,item), "Checked feeder has an unaccepted positive terminal output");
            graph[st].terminal=true; continue;
        }
        std::map<unsigned,double> mass;
        const auto route = [&](const pc_item_state& output,long double p) {
            work.tick(); require(p>0 && std::isfinite(double(p)), "Invalid full-item feeder probability");
            const StrategyEdge* edge=nullptr; const StrategyEdge* fallback=nullptr;
            for (const auto& candidate:node.edges) {
                if (candidate.is_default) { fallback=&candidate; continue; }
                if (evaluate_compiled_condition(candidate.condition,*session,output)) {edge=&candidate;break;}
            }
            if (!edge) edge=fallback;
            require(edge!=nullptr, "Checked feeder has a positive no-match failure");
            mass[intern(edge->target,output)]+=double(p);
        };
        Action a{RecombDecisionKind::Child,0,0,{}}; a.resource_rewards.resize(keys.size(),0);
        if (node.kind==StrategyNodeKind::Operation) {
            for (const auto& key:node.price_keys) {
                const auto price=economy->prices.find(key);
                require(price!=economy->prices.end() && finite_cost(price->second), "Checked feeder material price is incomplete");
                a.cost+=price->second;
                ++a.resource_rewards[std::find(keys.begin(),keys.end(),key)-keys.begin()];
            }
            a.child_actions=1;
            require(visit_full_item_removal_outcomes(context,item,node.action,route).applied,
                    "Checked feeder has a positive failed action; failed-child recovery is held");
        } else route(item,1);
        double total=0; for (const auto& row:mass) total+=row.second;
        require(std::abs(total-1)<=1e-10,"Checked feeder drops full-item output mass");
        a.exits.assign(mass.begin(),mass.end()); graph[st].actions.push_back(std::move(a));
    }
    const auto winning=winning_states(graph);
    require(std::all_of(winning.begin(),winning.end(),[](bool b){return b;}), "Checked feeder does not terminate successfully almost surely");
    const std::vector<unsigned> policy(graph.size(),0);
    const auto values=evaluate_policy(graph,winning,policy,unsigned(keys.size()),work);
    // One separate reward per terminal physical item supplies complete absorption probabilities.
    std::map<std::string,unsigned> output_index;
    for (unsigned st=0;st<graph.size();++st) if (graph[st].terminal) output_index.emplace(random_recomb_item_key(items[st]),unsigned(output_index.size()));
    auto absorption_graph=graph;
    for (auto& state:absorption_graph) for (auto& a:state.actions) {
        a.resource_rewards.assign(output_index.size(),0);
        for (const auto& [next,p]:a.exits) if (graph[next].terminal)
            a.resource_rewards[output_index.at(random_recomb_item_key(items[next]))]+=p;
    }
    const auto absorption=evaluate_policy(absorption_graph,winning,policy,unsigned(output_index.size()),work);
    auto checked=std::shared_ptr<RecombCheckedFeeder>(new RecombCheckedFeeder);
    checked->session_=session; checked->economy_=economy;
    checked->strategy_id_=id; checked->revision_=revision; checked->document_=document; checked->output_contract_id_=output_id;
    checked->cost_=paid_start+values[root][0]; checked->actions_=values[root][3]; checked->acquisition_cost_=paid_start;
    if (graph[root].terminal) checked->outcomes_.push_back({items[root],1});
    else for (const auto& [key,index]:output_index) {
        const auto probability=absorption[root][4+index];
        if (!probability) continue;
        for (unsigned st=0;st<graph.size();++st) if (graph[st].terminal && random_recomb_item_key(items[st])==key) {
            checked->outcomes_.push_back({items[st],probability}); break;
        }
    }
    double total=0; for (const auto& o:checked->outcomes_) total+=o.probability;
    require(std::abs(total-1)<=1e-10,"Checked feeder complete output absorption failed");
    for (unsigned k=0;k<keys.size();++k) checked->materials_[keys[k]]=values[root][4+k];
    return checked;
}
RecombSolverResult solve_random_recomb_inventory(const RecombSolverRequest& request) {
    require(request.session != nullptr && !request.price_identity.empty(), "Inventory solver session/prices are missing");
    validate_random_recomb_carrier_session(*request.session);
    const bool complete_attempt=request.recombination_cost_complete && request.recombination_cost_chaos.has_value();
    require((complete_attempt || request.allow_incomplete_costs) &&
            (!request.recombination_cost_chaos || finite_cost(*request.recombination_cost_chaos)), "Recombination gold/dust/attempt cost is incomplete");
    require(!request.acquisitions.empty() && request.acquisitions.size() <= 32, "Inventory solver requires a bounded acquisition catalogue");
    require(request.initial_items.size() <= 2 && request.initial_item_costs.size() == request.initial_items.size(),
            "Inventory solver initial item/cost arity differs");
    require(request.max_items >= 1 && request.max_items <= 128 && request.max_states >= 1 &&
            request.max_states <= 256 && request.max_policy_iterations >= 1 &&
            request.max_policy_iterations <= 64, "Inventory solver resource caps are invalid");
    require(request.max_work >= 1 && request.max_work <= 500000000, "Inventory numerical work cap is invalid");
    require(request.model_id == kRandomRecombModel || request.model_id == kRandomRecombExtendedModel ||
            request.model_id == kRandomRecombBlockingModel, "Unsupported declared recombination inventory model");
    require(bool(request.scenario) == (request.model_id == kRandomRecombBlockingModel), "Scenario/model identity mismatch");
    if (request.scenario) {
        require(!request.scenario->id.empty(), "Blocking analysis requires an explicit scenario identity");
        for (auto alpha : request.scenario->prefix_first)
            require(std::isfinite(alpha) && alpha >= 0 && alpha <= 1, "Invalid explicit first-side scenario probability");
    }
    WorkGuard work{request}; work.tick();
    validate_random_recomb_goal_projection(request.goal_set_json.data(), request.goal_set_json.size());
    RecombSolverResult result; result.model_id = request.model_id; result.price_identity = request.price_identity; result.cost_complete = complete_attempt;
    const auto goals = solver::bind_calculator_goal_set(request.session, request.goal_set_json.data(), request.goal_set_json.size());
    std::vector<std::uint64_t> reachable(request.session->words, 0);
    for (unsigned mod = 0; mod < request.session->mod_count; ++mod)
        if (request.session->gen_type[mod] <= 1) pc_bitset_set(reachable.data(), mod);
    solver::ActionRegistry registry;
    solver::CalcContext observer(request.session, goals.front().explicit_goal, std::move(registry), {}, true, false,
        false, std::nullopt, {}, false, reachable, false, false, false, false, false, nullptr, true, false, true);
    std::map<std::string, unsigned> item_ids;
    std::vector<bool> item_goals;
    const auto intern_item = [&](const pc_item_state& item) {
        work.tick();
        validate_recombination_item_structure({"validation", "item", request.session, item});
        const auto key = random_recomb_item_key(item);
        if (const auto found = item_ids.find(key); found != item_ids.end()) return found->second;
        if (result.items.size() >= request.max_items) throw std::length_error("Inventory item discovery cap reached; no outcomes dropped");
        auto projected = item; projected.memory_strands = 0; projected.enchantment_count = 0;
        const auto observed = solver::observe_calculator_terminal_law_json(observer, goals,
            [&](const solver::CalculatorTerminalSink& sink) { sink(projected, 1); });
        const auto parsed = json::Parser(observed.data(), observed.size()).parse();
        const auto p = parsed.at("any_goal_probability").as_number();
        require(p == 0 || p == 1, "Inventory terminal observation is not deterministic");
        const auto id = static_cast<unsigned>(result.items.size());
        item_ids.emplace(key, id); result.items.push_back(item); item_goals.push_back(p == 1);
        return id;
    };
    std::map<std::pair<std::string,std::string>,std::string> feeder_revisions;
    std::set<std::string> acquisition_ids;
    std::vector<std::vector<std::pair<unsigned,double>>> acquisition_items;
    for (const auto& offer : request.acquisitions) {
        require(!offer.id.empty() && acquisition_ids.insert(offer.id).second && !offer.quote_identity.empty() &&
            (offer.source_kind == "purchase" || offer.source_kind == "completed_feeder" || offer.source_kind == "checked_feeder") &&
            (offer.cost_complete || request.allow_incomplete_costs) && finite_cost(offer.total_cost_chaos), "Incomplete/invalid exact-item acquisition quote");
        result.cost_complete = result.cost_complete && offer.cost_complete;
        require(bool(offer.checked_feeder) == (offer.source_kind == "checked_feeder"), "Feeder provenance/certificate mismatch");
        std::vector<std::pair<unsigned,double>> law;
        if (offer.checked_feeder) {
            const auto& child = *offer.checked_feeder;
            const auto revision=std::make_pair(child.strategy_id(),child.revision());
            if(auto old=feeder_revisions.find(revision);old!=feeder_revisions.end())
                require(old->second==child.document(),"One feeder revision contains conflicting documents");
            feeder_revisions[revision]=child.document();
            require(child.economy()->id==request.price_identity,"Checked feeder price identity mismatch");
            require(child.session() == request.session && std::abs(child.total_cost()-offer.total_cost_chaos) <= 1e-10,
                    "Checked feeder session/cost identity mismatch");
            for (const auto& output : child.outcomes()) law.emplace_back(intern_item(output.item),output.probability);
        } else law.emplace_back(intern_item(offer.item),1);
        acquisition_items.push_back(std::move(law));
    }
    std::vector<unsigned> initial;
    for (unsigned i = 0; i < request.initial_items.size(); ++i) {
        require(finite_cost(request.initial_item_costs[i]), "Initial input cost is incomplete");
        initial.push_back(intern_item(request.initial_items[i])); result.entry_cost_chaos += request.initial_item_costs[i];
    }
    require(std::isfinite(result.entry_cost_chaos), "Initial input cost overflow");
    std::map<std::vector<unsigned>, unsigned> state_ids; std::vector<State> states;
    const auto intern_state = [&](std::vector<unsigned> inventory) {
        work.tick();
        require(inventory.size() <= 2, "Inventory capacity exceeded"); std::sort(inventory.begin(), inventory.end());
        if (const auto found = state_ids.find(inventory); found != state_ids.end()) return found->second;
        if (states.size() >= request.max_states) throw std::length_error("Inventory state discovery cap reached; no outcomes dropped");
        const bool terminal = std::any_of(inventory.begin(), inventory.end(), [&](unsigned id) { return item_goals.at(id); });
        const auto id = static_cast<unsigned>(states.size()); state_ids.emplace(inventory, id);
        states.push_back({std::move(inventory), {}, terminal}); return id;
    };
    result.initial_state = intern_state(std::move(initial));
    std::set<std::string> exclusions;
    for (unsigned s = 0; s < states.size(); ++s) {
        work.tick();
        if (states[s].terminal) continue;
        // Interning successors can reallocate states; hold only value copies.
        const auto inventory = states[s].items; std::vector<Action> actions;
        if (inventory.size() < 2) for (unsigned a = 0; a < request.acquisitions.size(); ++a) {
            std::map<unsigned,double> mass;
            for (auto [item,p] : acquisition_items[a]) {
                auto next = inventory; next.push_back(item); mass[intern_state(std::move(next))] += p;
            }
            Action acquisition{RecombDecisionKind::Acquire,a,request.acquisitions[a].total_cost_chaos,{mass.begin(),mass.end()}};
            if (request.acquisitions[a].checked_feeder) acquisition.child_actions = request.acquisitions[a].checked_feeder->child_actions();
            actions.push_back(std::move(acquisition));
        }
        for (unsigned i = 0; i < inventory.size(); ++i) {
            auto next = inventory; next.erase(next.begin() + i);
            actions.push_back({RecombDecisionKind::Discard, i, 0, {{intern_state(std::move(next)), 1}}});
        }
        if (inventory.size() == 2) {
            std::optional<RandomRecombPair> pair;
            try {
                pair = prepare_random_recomb_pair(
                    {"solver-input-a", "a", request.session, result.items[inventory[0]]},
                    {"solver-input-b", "b", request.session, result.items[inventory[1]]},
                    request.scenario ? &*request.scenario : nullptr);
                if (pair->model_id == kRandomRecombExtendedModel && request.model_id != kRandomRecombExtendedModel) {
                    exclusions.insert("Pair requires explicitly selected native-constraints model v2"); pair.reset();
                }
            } catch (const std::invalid_argument& error) { exclusions.insert(error.what()); }
            if (pair) {
                std::map<unsigned, double> mass; double total = 0;
                for (const auto& outcome : enumerate_random_recomb_pair(*pair)) {
                    require(std::isfinite(outcome.probability) && outcome.probability > 0, "Invalid inventory transition probability");
                    const auto& from = *pair->carriers[outcome.carrier].output_session;
                    const auto item = root_item(materialize_random_recomb_outcome(*pair, outcome), from, *request.session);
                    const auto t = intern_state({intern_item(item)});
                    mass[t] += outcome.probability; total += outcome.probability;
                }
                require(std::abs(total - 1) <= 1e-10, "Inventory pair transition loses probability mass");
                actions.push_back({RecombDecisionKind::Recombine, 0, request.recombination_cost_chaos.value_or(0),
                    {mass.begin(), mass.end()}});
            }
        }
        states[s].actions = std::move(actions);
    }
    const auto winning = winning_states(states);
    require(winning.at(result.initial_state), "No proper policy in the declared bounded inventory scope");
    auto policy = bootstrap_policy(states, winning);
    auto values = evaluate_policy(states, winning, policy, static_cast<unsigned>(request.acquisitions.size()), work);
    for (unsigned iteration = 0; result.cost_complete && iteration < request.max_policy_iterations; ++iteration) {
        result.policy_iterations = iteration + 1; auto next = policy; bool changed = false;
        for (unsigned s = 0; s < states.size(); ++s) if (winning[s] && !states[s].terminal) {
            double best = values[s][0];
            for (unsigned a = 0; a < states[s].actions.size(); ++a) {
                const auto& action = states[s].actions[a]; if (!admissible(action, winning)) continue;
                double cost = action.cost;
                for (const auto& [t, p] : action.exits) cost += p * values[t][0];
                if (cost < best - 1e-10 * std::max(1.0, best)) { best = cost; next[s] = a; changed = true; }
            }
        }
        if (!changed) { result.search_converged = true; break; }
        if (!policy_is_proper(states, winning, next)) { exclusions.insert("Policy improvement proposed a nonterminating policy"); break; }
        policy = std::move(next);
        values = evaluate_policy(states, winning, policy, static_cast<unsigned>(request.acquisitions.size()), work);
    }
    const auto& entry = values[result.initial_state];
    result.expected_cost_chaos = entry[0] + result.entry_cost_chaos;
    result.expected_recombinations = entry[1]; result.expected_discards = entry[2];
    result.expected_child_actions = entry[3];
    result.expected_acquisitions.assign(entry.begin() + 4, entry.end());
    require(std::isfinite(result.expected_cost_chaos), "Inventory expected cost overflow");
    for (unsigned s = 0; s < states.size(); ++s) {
        result.inventories.push_back(states[s].items); result.terminal.push_back(states[s].terminal);
        if (!winning[s] || states[s].terminal) continue;
        const auto& action = states[s].actions[policy[s]];
        RecombPolicyDecision decision; decision.state = s; decision.kind = action.kind;
        if (action.kind == RecombDecisionKind::Acquire) decision.acquisition = action.argument;
        if (action.kind == RecombDecisionKind::Recombine) {
            decision.input_a = states[s].items[0]; decision.input_b = states[s].items[1];
        }
        if (action.kind == RecombDecisionKind::Discard) decision.input_a = action.argument;
        decision.outcomes = action.exits; result.policy.push_back(std::move(decision));
    }
    result.work_spent = work.spent;
    result.item_is_goal = std::move(item_goals);
    result.items.shrink_to_fit();
    result.exclusions.assign(exclusions.begin(), exclusions.end()); return result;
}
namespace {
std::string bridge_quote(const std::string& s) {
    std::string out="\"";
    for (unsigned char c:s) {
        if (c=='"'||c=='\\') {out+='\\';out+=char(c);}
        else if(c<32){const char* h="0123456789abcdef";out+="\\u00";out+=h[c>>4];out+=h[c&15];}
        else out+=char(c);
    } return out+'"';
}
std::string bridge_economy(const RecombSolverRequest& request) {
    std::map<std::string,double> prices;
    const auto bind=[&](const std::string& key,double value) {
        if (auto old=prices.find(key);old!=prices.end()) require(old->second==value,"Conflicting checked feeder price snapshots");
        prices[key]=value;
    };
    for(unsigned i=0;i<request.acquisitions.size();++i) {
        const auto& a=request.acquisitions[i];
        bind("recomb-acquisition/"+std::to_string(i),a.checked_feeder ? a.checked_feeder->acquisition_cost() : a.total_cost_chaos);
        if(a.checked_feeder) {
            require(a.checked_feeder->economy()->id==request.price_identity,"Checked feeder price identity differs");
            for(const auto& [key,value]:a.checked_feeder->economy()->prices) bind(key,value);
        }
    }
    std::ostringstream out;out<<std::setprecision(17)<<"{\"version\":\"v1\",\"id\":"<<bridge_quote(request.price_identity)<<",\"prices\":{";
    bool comma=false;for(const auto& [key,value]:prices){if(comma)out<<',';comma=true;out<<bridge_quote(key)<<':'<<value;}return out.str()+"}}";
}
unsigned bridge_item(const RecombSolverResult& result,const pc_item_state& item) {
    const auto key=random_recomb_item_key(item);
    for(unsigned i=0;i<result.items.size();++i) if(random_recomb_item_key(result.items[i])==key)return i;
    throw std::invalid_argument("Checked export encounters an unrepresented positive physical item");
}
unsigned bridge_state(const RecombSolverResult& result,std::vector<unsigned> items) {
    std::sort(items.begin(),items.end());
    for(unsigned i=0;i<result.inventories.size();++i)if(result.inventories[i]==items)return i;
    throw std::invalid_argument("Checked export encounters an unrepresented inventory continuation");
}
void check_bridge_scope(const RecombSolverRequest& request,const RecombSolverResult& result) {
    require(!request.scenario && request.model_id!=kRandomRecombBlockingModel,
            "Advanced estimated scenarios are analysis-only; random Builder export remains held");
    require(result.model_id==request.model_id && result.price_identity==request.price_identity && result.cost_complete &&
        request.recombination_cost_complete && request.recombination_cost_chaos && result.item_is_goal.size()==result.items.size(),
        "Checked export request/result identity or economic status mismatch");
    require(request.model_id==kRandomRecombModel || request.model_id==kRandomRecombExtendedModel,"Unsupported executable provider");
}
}
RecombBuilderExport export_recomb_builder_policy(const RecombSolverRequest& request,const RecombSolverResult& result) {
    check_bridge_scope(request,result);
    struct Context {unsigned state;std::array<int,2> slots;};
    std::vector<Context> contexts;
    std::map<std::pair<unsigned,std::array<int,2>>,unsigned> ids;
    const auto intern=[&](unsigned state,std::array<int,2> slots) {
        std::vector<unsigned> inventory;for(auto id:slots)if(id>=0)inventory.push_back(unsigned(id));
        require(bridge_state(result,inventory)==state,"Export physical slots disagree with canonical inventory");
        const auto key=std::make_pair(state,slots);
        if(auto f=ids.find(key);f!=ids.end())return f->second;
        if(contexts.size()>=request.max_states)throw std::length_error("Builder export context cap reached");
        const auto i=unsigned(contexts.size());ids.emplace(key,i);contexts.push_back({state,slots});return i;
    };
    std::array<int,2> initial{-1,-1};
    for(unsigned i=0;i<request.initial_items.size();++i)initial[i]=int(bridge_item(result,request.initial_items[i]));
    const auto root=intern(result.initial_state,initial);
    std::vector<std::string> nodes,edges;
    const auto node=[&](const std::string& id,const std::string& kind,const std::string& extra="") {
        nodes.push_back("{\"id\":"+bridge_quote(id)+",\"kind\":"+bridge_quote(kind)+",\"position\":{\"x\":0,\"y\":0}"+extra+'}');
    };
    unsigned edge_count=0;
    const auto edge=[&](const std::string& from,const std::string& to,const std::string& condition="") {
        edges.push_back("{\"id\":\"edge/"+std::to_string(edge_count++)+"\",\"from\":"+bridge_quote(from)+",\"to\":"+bridge_quote(to)+
            (condition.empty()?",\"is_default\":true":",\"condition\":"+condition)+'}');
    };
    const auto op=[&](const std::string& id,const std::string& name,const std::string& type,const std::string& params) {
        node(id,"operation",",\"name\":"+bridge_quote(name)+",\"operation\":{\"type\":"+bridge_quote(type)+",\"params\":"+params+'}');
    };
    const auto full=[&](unsigned id) {return "{\"type\":\"full_item_is\",\"base_state\":"+random_recomb_base_state_json(result.items.at(id),*request.session)+'}';};
    std::vector<const RecombPolicyDecision*> decisions(result.inventories.size(),nullptr);
    for(const auto& d:result.policy) {require(d.state<decisions.size() && !decisions[d.state],"Duplicate policy state");decisions[d.state]=&d;}
    node("start","start");node("success","terminal",",\"terminal\":\"success\"");edge("start","state/"+std::to_string(root));
    for(unsigned c=0;c<contexts.size();++c) {
        const auto context=contexts[c]; const auto id="state/"+std::to_string(c);
        if(result.terminal.at(context.state)) {
            int selected=-1;
            for(unsigned i=0;i<2;++i)if(context.slots[i]>=0 && result.item_is_goal.at(context.slots[i])){selected=int(i);break;}
            require(selected>=0,"Export terminal has no accepted physical output");
            op(id,"Completed item","move_resource","{\"from\":\"slot/"+std::to_string(selected)+"\",\"to\":\"finished\"}");edge(id,"success");continue;
        }
        const auto* d=decisions.at(context.state);require(d,"Export missing nonterminal policy decision");
        if(d->kind==RecombDecisionKind::Acquire) {
            const auto& offer=request.acquisitions.at(d->acquisition);
            require(offer.source_kind=="purchase" || offer.checked_feeder,"Unchecked feeder quote cannot become an executable saved feeder");
            const unsigned vacant=context.slots[0]<0?0:1;require(context.slots[vacant]<0,"Export acquisition overwrites a physical resource");
            const auto resource="offer/"+std::to_string(d->acquisition);
            op(id,offer.checked_feeder?"Saved feeder":"Donor item",offer.checked_feeder?"invoke_feeder":"acquire_resource","{\"resource_id\":"+bridge_quote(resource)+'}');
            const auto move="move/"+std::to_string(c),route="route/"+std::to_string(c);
            op(move,"Retain acquired item","move_resource","{\"from\":"+bridge_quote(resource)+",\"to\":\"slot/"+std::to_string(vacant)+"\"}");
            node(route,"router");edge(id,move);edge(move,route);
            for(const auto& [target,p]:d->outcomes) {
                require(p>0,"Export contains a nonpositive acquisition outcome");
                auto next=result.inventories.at(target);
                for(auto held:context.slots)if(held>=0){auto f=std::find(next.begin(),next.end(),unsigned(held));require(f!=next.end(),"Acquisition loses held input");next.erase(f);}
                require(next.size()==1,"Acquisition must return one actual full item");auto slots=context.slots;slots[vacant]=int(next[0]);
                edge(route,"state/"+std::to_string(intern(target,slots)),full(next[0]));
            }
        } else if(d->kind==RecombDecisionKind::Recombine) {
            require(context.slots[0]>=0 && context.slots[1]>=0,"Export pair lacks two physical inputs");
            std::ostringstream params;params<<std::setprecision(17)<<"{\"input_a\":\"slot/0\",\"input_b\":\"slot/1\",\"output\":\"slot/0\",\"use_declared_inputs\":true,\"all_in_attempt_cost_chaos\":"<<*request.recombination_cost_chaos<<'}';
            op(id,"Recombination","recombination",params.str());
            for(const auto& [target,p]:d->outcomes) {
                require(p>0 && result.inventories.at(target).size()==1,"Pair must consume two and create one actual output");
                const unsigned item=result.inventories[target][0];
                edge(id,"state/"+std::to_string(intern(target,{int(item),-1})),full(item));
            }
        } else if(d->kind==RecombDecisionKind::Discard) {
            const auto spec=result.inventories.at(context.state).at(d->input_a);
            unsigned slot=0;
            if(context.slots[0]!=int(spec) || (d->input_a==1 && context.slots[0]==context.slots[1]))slot=1;
            require(context.slots[slot]==int(spec) && d->outcomes.size()==1 && d->outcomes[0].second==1,"Invalid physical discard policy");
            op(id,"Discard item","discard_resource","{\"resource_id\":\"slot/"+std::to_string(slot)+"\"}");
            auto slots=context.slots;slots[slot]=-1;
            edge(id,"state/"+std::to_string(intern(d->outcomes[0].first,slots)));
        } else throw std::invalid_argument("Unsupported export decision");
    }
    pc_item_state empty{};pc_item_clear(&empty);
    const auto base=random_recomb_base_state_json(empty,*request.session);
    std::ostringstream out;out<<std::setprecision(17)<<"{\"version\":\"v1\",\"name\":\"Recombination policy\",\"description\":\"Checked bounded estimated policy; declared acquisition and all-in attempt scenario costs.\",\"start_node_id\":\"start\",\"start_item_present\":false,\"base_state\":"<<base<<",\"resources\":[";
    for(unsigned slot=0;slot<3;++slot) {
        if(slot)out<<',';const auto id=slot<2?"slot/"+std::to_string(slot):"finished";
        out<<"{\"id\":"<<bridge_quote(id)<<",\"acquisition_price_key\":\"held/"<<slot<<"\",\"base_state\":";
        if(slot<request.initial_items.size())out<<random_recomb_base_state_json(request.initial_items[slot],*request.session)<<",\"initially_owned\":true,\"initial_cost_chaos\":"<<request.initial_item_costs.at(slot);
        else out<<base;out<<'}';
    }
    for(unsigned i=0;i<request.acquisitions.size();++i) {
        const auto& a=request.acquisitions[i];out<<",{\"id\":\"offer/"<<i<<"\",\"acquisition_price_key\":\"recomb-acquisition/"<<i<<"\",\"base_state\":";
        if(a.checked_feeder) {
            const auto child=compile_strategy_json(request.session,a.checked_feeder->document().data(),a.checked_feeder->document().size());
            out<<random_recomb_base_state_json(child->start_item,*request.session)<<",\"feeder\":{\"strategy_id\":"<<bridge_quote(a.checked_feeder->strategy_id())<<",\"revision\":"<<bridge_quote(a.checked_feeder->revision())
                <<",\"document_json\":"<<bridge_quote(a.checked_feeder->document())<<",\"output_contract_id\":"<<bridge_quote(a.checked_feeder->output_contract_id())<<'}';
        }else out<<random_recomb_base_state_json(a.item,*request.session);out<<'}';
    }
    out<<"],\"nodes\":[";for(unsigned i=0;i<nodes.size();++i){if(i)out<<',';out<<nodes[i];}
    out<<"],\"edges\":[";for(unsigned i=0;i<edges.size();++i){if(i)out<<',';out<<edges[i];}
    out<<"],\"output_contracts\":[{\"id\":\"complete\",\"base_key\":"<<bridge_quote(request.session->data->string_at(request.session->data->base_metadata_path_sid[request.session->base_index]))
        <<",\"resource_id\":\"finished\",\"predicate\":{\"type\":\"any\",\"conditions\":[";
    bool comma=false;for(unsigned i=0;i<result.items.size();++i)if(result.item_is_goal[i]){if(comma)out<<',';comma=true;out<<full(i);}out<<"]}}]}";
    return check_recomb_builder_policy(request,result,out.str());
}
RecombBuilderExport check_recomb_builder_policy(const RecombSolverRequest& request,const RecombSolverResult& result,const std::string& document) {
    check_bridge_scope(request,result);
    require(document.size()<=1024*1024,"Checked Builder document byte cap reached");
    auto strategy=compile_strategy_json(request.session,document.data(),document.size());
    require(!strategy->start_item_present,"Checked inventory export cannot create a free Start item");
    const auto economy_json=bridge_economy(request);auto economy=load_economy_json(economy_json.data(),economy_json.size());
    WorkGuard work{request};
    struct Physical {unsigned node;std::vector<int> slots;int incoming=-1;};
    std::vector<Physical> physical;
    std::vector<State> graph;
    std::map<std::tuple<unsigned,std::vector<int>,int>,unsigned> ids;
    const auto slot=[&](const std::string& id) {
        for(unsigned i=0;i<strategy->resources.size();++i)if(strategy->resources[i].id==id)return i;
        throw std::invalid_argument("Checked Builder references unavailable current/physical slot");
    };
    const auto intern=[&](unsigned node,std::vector<int> slots,int incoming) {
        work.tick();require(std::count_if(slots.begin(),slots.end(),[](int i){return i>=0;})<=2,"Checked Builder exceeds two live physical items");
        const auto key=std::make_tuple(node,slots,incoming);
        if(auto f=ids.find(key);f!=ids.end())return f->second;
        if(graph.size()>=request.max_states)throw std::length_error("Independent Builder checker state cap reached");
        const auto i=unsigned(graph.size());ids.emplace(key,i);physical.push_back({node,std::move(slots),incoming});graph.push_back({{},{},false});return i;
    };
    std::vector<int> initial(strategy->resources.size(),-1);std::vector<std::pair<std::string,double>> owned;
    for(unsigned i=0;i<strategy->resources.size();++i)if(strategy->resources[i].initially_owned) {
        const auto& r=strategy->resources[i];const auto item=root_item(r.item,*r.session,*request.session);
        initial[i]=int(bridge_item(result,item));owned.emplace_back(random_recomb_item_key(item),r.initial_cost);
    }
    std::vector<std::pair<std::string,double>> expected_owned;
    for(unsigned i=0;i<request.initial_items.size();++i)expected_owned.emplace_back(random_recomb_item_key(request.initial_items[i]),request.initial_item_costs[i]);
    std::sort(owned.begin(),owned.end());std::sort(expected_owned.begin(),expected_owned.end());require(owned==expected_owned,"Builder root ownership/paid-cost identity differs");
    const auto root=intern(strategy->start_node,initial,-1);
    for(unsigned st=0;st<graph.size();++st) {
        work.tick();const auto state=physical[st];const auto& node=strategy->nodes.at(state.node);
        require(!node.source_only && node.input_edges[0].empty() && node.input_edges[1].empty(),"Restricted checked Builder excludes lazy dependency graphs");
        if(node.kind==StrategyNodeKind::Terminal) {
            bool goal=false;for(auto id:state.slots)if(id>=0)goal|=result.item_is_goal.at(id);
            require(node.terminal_kind==PC_TERMINAL_SUCCESS && goal,"Checked Builder has a positive non-goal/failure terminal");graph[st].terminal=true;continue;
        }
        std::vector<std::pair<Physical,double>> outcomes;
        Action a{RecombDecisionKind::Child,0,0,{}};a.resource_rewards.assign(request.acquisitions.size()+1,0);
        if(node.kind!=StrategyNodeKind::Operation)outcomes.push_back({state,1});
        else {
            a.resource_rewards.back()=1;
            if(node.action_type==kStrategyAcquireResourceOperation || node.action_type==kStrategyInvokeFeederOperation) {
                const unsigned target=slot(node.resource_id);require(state.slots[target]<0,"Checked Builder reuses/overwrites a live acquired item");
                const auto& definition=strategy->resources[target];unsigned offer=unsigned(request.acquisitions.size());
                for(unsigned i=0;i<request.acquisitions.size();++i)if(definition.acquisition_price_key=="recomb-acquisition/"+std::to_string(i)){offer=i;break;}
                require(offer<request.acquisitions.size(),"Checked Builder acquisition has no pinned quote/contract");const auto& source=request.acquisitions[offer];
                a.cost=source.total_cost_chaos;a.resource_rewards[offer]=1;
                std::vector<RecombFeederOutcome> law;
                if(source.checked_feeder) {
                    require(node.action_type==kStrategyInvokeFeederOperation && definition.feeder &&
                        definition.feeder->source_json==source.checked_feeder->document() && definition.feeder_strategy_id==source.checked_feeder->strategy_id() &&
                        definition.feeder_revision==source.checked_feeder->revision() && definition.output_contract.id==source.checked_feeder->output_contract_id(),"Checked Builder feeder revision/content/output differs");
                    law=source.checked_feeder->outcomes();a.child_actions=source.checked_feeder->child_actions();a.resource_rewards.back()+=a.child_actions;
                }else {
                    require(source.source_kind=="purchase" && node.action_type==kStrategyAcquireResourceOperation && !definition.feeder &&
                        random_recomb_item_key(root_item(definition.item,*definition.session,*request.session))==random_recomb_item_key(source.item),"Unchecked feeder or purchase-item identity mismatch");
                    law.push_back({source.item,1});
                }
                for(const auto& output:law){auto next=state;next.slots[target]=int(bridge_item(result,output.item));outcomes.push_back({std::move(next),output.probability});}
            }else if(node.action_type==kStrategyMoveResourceOperation) {
                const auto from=slot(node.source_resource_id),to=slot(node.resource_id);require(from!=to && state.slots[from]>=0 && state.slots[to]<0,"Checked Builder move clones/overwrites an item");
                auto next=state;next.slots[to]=next.slots[from];next.slots[from]=-1;outcomes.push_back({std::move(next),1});
            }else if(node.action_type==kStrategyDiscardResourceOperation) {
                const auto target=slot(node.resource_id);require(state.slots[target]>=0,"Checked Builder discards an absent item");auto next=state;next.slots[target]=-1;
                a.kind=RecombDecisionKind::Discard;outcomes.push_back({std::move(next),1});
            }else if(node.action_type==kStrategyRecombinationOperation) {
                require(node.use_declared_inputs && node.all_in_attempt_cost && *node.all_in_attempt_cost==*request.recombination_cost_chaos,"Checked Builder provider/attempt-price binding differs");
                const auto ai=slot(node.source_resource_id),bi=slot(node.second_resource_id),oi=slot(node.resource_id);
                require(ai!=bi && state.slots[ai]>=0 && state.slots[bi]>=0 && (oi==ai || oi==bi || state.slots[oi]<0),"Checked Builder pair aliases/overwrites physical resources");
                const auto pair=prepare_random_recomb_pair({"checked-a","a",request.session,result.items[state.slots[ai]]},{"checked-b","b",request.session,result.items[state.slots[bi]]});
                require(pair.model_id==request.model_id || (request.model_id==kRandomRecombExtendedModel && pair.model_id==kRandomRecombModel),"Checked Builder pair provider model mismatch");
                for(const auto& output:enumerate_random_recomb_pair(pair)) {
                    work.tick();const auto item=root_item(materialize_random_recomb_outcome(pair,output),*pair.carriers[output.carrier].output_session,*request.session);
                    auto next=state;next.slots[ai]=-1;next.slots[bi]=-1;next.slots[oi]=int(bridge_item(result,item));outcomes.push_back({std::move(next),output.probability});
                }
                a.kind=RecombDecisionKind::Recombine;a.cost=*request.recombination_cost_chaos;
            }else throw std::invalid_argument("Builder operation is outside the restricted checked inventory language");
        }
        std::map<unsigned,double> mass;
        for(auto& [next,p]:outcomes) {
            work.tick();int observed=next.incoming;
            if(node.action_type==kStrategyRecombinationOperation)observed=int(slot(node.resource_id));
            const StrategyEdge* selected=nullptr;const StrategyEdge* fallback=nullptr;
            for(const auto& e:node.edges) {
                if(e.is_default){fallback=&e;continue;}
                if(e.condition.kind==ConditionKind::Always || (observed>=0 && next.slots[observed]>=0 &&
                    evaluate_compiled_condition(e.condition,*request.session,result.items[next.slots[observed]]))){selected=&e;break;}
            }
            if(!selected)selected=fallback;require(selected,"Checked Builder drops a positive no-match outcome");
            if(!selected->input_resource_id.empty())next.incoming=int(slot(selected->input_resource_id));
            else if(node.kind!=StrategyNodeKind::Router)next.incoming=-1;
            next.node=selected->target;mass[intern(next.node,std::move(next.slots),next.incoming)]+=p;
        }
        double total=0;for(const auto& row:mass)total+=row.second;require(std::abs(total-1)<=1e-10,"Checked Builder row loses probability mass");
        a.exits.assign(mass.begin(),mass.end());graph[st].actions.push_back(std::move(a));
    }
    const auto winning=winning_states(graph);require(std::all_of(winning.begin(),winning.end(),[](bool b){return b;}),"Checked Builder policy is not proper");
    const std::vector<unsigned> policy(graph.size(),0);const auto values=evaluate_policy(graph,winning,policy,unsigned(request.acquisitions.size()+1),work);
    RecombBuilderExport checked;checked.strategy_json=document;checked.economy_json=economy_json;
    checked.checked_cost=values[root][0]+result.entry_cost_chaos;checked.checked_recombinations=values[root][1];checked.checked_discards=values[root][2];checked.checked_child_actions=values[root][3];
    checked.checked_acquisitions.assign(values[root].begin()+4,values[root].end()-1);checked.checked_builder_actions=values[root].back();
    const auto same=[](double a,double b){return std::abs(a-b)<=1e-8*std::max(1.,std::abs(b));};
    require(same(checked.checked_cost,result.expected_cost_chaos) && same(checked.checked_recombinations,result.expected_recombinations) &&
        same(checked.checked_discards,result.expected_discards) && same(checked.checked_child_actions,result.expected_child_actions),"Builder export/evaluated-policy reward mismatch");
    require(checked.checked_acquisitions.size()==result.expected_acquisitions.size(),"Builder acquisition counter arity differs");
    for(unsigned i=0;i<checked.checked_acquisitions.size();++i)require(same(checked.checked_acquisitions[i],result.expected_acquisitions[i]),"Builder export/source acquisition counter mismatch");
    return checked;
}

}
