#include "hinekora_internal.hpp"
#include "handles_internal.hpp"
#include "json.hpp"
#include <cmath>
#include <limits>
#include <type_traits>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <stdexcept>

struct pc_hinekora_lock {
    std::shared_ptr<poecraft::HinekoraForesight> impl;
};
namespace {
void error(pc_error_info* out, pc_result code, const char* message) {
    if (!out) return;
    pc_error_info_init(out);
    out->code = code;
    std::snprintf(out->message, sizeof(out->message), "%s", message);
}
void result(const poecraft::ActionOutcome& outcome, pc_action_result* out) {
    *out = {sizeof(*out), PC_ABI_VERSION, outcome.applied ? 1 : 0,
            outcome.added, outcome.removed};
}
template<std::size_t N>
bool same_slots(const pc_mod_slot (&a)[N], const pc_mod_slot (&b)[N]) {
    for (std::size_t i = 0; i < N; ++i) {
        if (a[i].mod_id != b[i].mod_id || a[i].group_id != b[i].group_id ||
            a[i].flags != b[i].flags || a[i].roll_count != b[i].roll_count ||
            a[i].veiled_option_count != b[i].veiled_option_count ||
            a[i].veiled_chosen_mod_id != b[i].veiled_chosen_mod_id ||
            !std::equal(std::begin(a[i].rolls), std::end(a[i].rolls), std::begin(b[i].rolls)) ||
            !std::equal(std::begin(a[i].veiled_option_mod_ids), std::end(a[i].veiled_option_mod_ids), std::begin(b[i].veiled_option_mod_ids))) return false;
    }
    return true;
}
bool same(const pc_item_state& a, const pc_item_state& b) {
    // Every native field is compared exactly, including numeric rolls and
    // metadata. C/C++ padding bytes are not item state and may change on copies.
    return a.rarity == b.rarity && a.quality == b.quality &&
        a.memory_strands == b.memory_strands && a.lifecycle == b.lifecycle &&
        (a.item_flags & ~PC_ITEM_FORESEEN) == (b.item_flags & ~PC_ITEM_FORESEEN) && a.prefix_count == b.prefix_count &&
        a.suffix_count == b.suffix_count && a.implicit_count == b.implicit_count &&
        a.enchantment_count == b.enchantment_count &&
        same_slots(a.prefixes, b.prefixes) && same_slots(a.suffixes, b.suffixes) &&
        same_slots(a.implicits, b.implicits) && same_slots(a.enchantments, b.enchantments) &&
        a.generic_influence_bits == b.generic_influence_bits &&
        a.searing_exarch_tier == b.searing_exarch_tier &&
        a.eater_of_worlds_tier == b.eater_of_worlds_tier &&
        a.socket_count == b.socket_count && a.link_mask == b.link_mask &&
        std::equal(std::begin(a.socket_colors), std::end(a.socket_colors), std::begin(b.socket_colors));
}
bool pending_veil(const pc_item_state& item) {
    const auto pending = [](const pc_mod_slot* slots, unsigned count) {
        for (unsigned i = 0; i < count; ++i)
            if (slots[i].flags & PC_MOD_SLOT_VEILED) return true;
        return false;
    };
    return pending(item.prefixes, item.prefix_count) || pending(item.suffixes, item.suffix_count);
}
bool current(poecraft::HinekoraForesight& f, const pc_item_state* item) {
    if (f.identity != item || !f.active) return false;
    if (!same(f.input, *item)) {
        f.active = false;
        f.refresh_allowed = true;
        f.identity->item_flags &= ~PC_ITEM_FORESEEN;
    } else if (!(item->item_flags & PC_ITEM_FORESEEN)) {
        // Merely clearing the marker cannot create a free fresh outcome.
        f.active = false;
    }
    return f.active;
}
bool same_action(const poecraft::ActionParameters& a,
                 const poecraft::ActionParameters& b) {
    return a.type == b.type && a.essence_index == b.essence_index &&
        a.fossil_indices == b.fossil_indices && a.mod_id == b.mod_id &&
        a.target_tag_id == b.target_tag_id && a.source_tag_id == b.source_tag_id &&
        a.influence_code == b.influence_code && a.tier == b.tier;
}

using poecraft::json::Value;
std::string quoted(const std::string& value) {
    std::string out = "\"";
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') { out += '\\'; out += c; }
        else if (c < 32) {
            char b[7]; std::snprintf(b, sizeof(b), "\\u%04x", c); out += b;
        } else out += c;
    }
    return out + '"';
}
std::string session_identity(const poecraft::SessionImpl& s) {
    const auto& d = *s.data;
    return "[" + std::to_string(d.artifact_schema_version) + "," +
        quoted(d.artifact_data_hash) + "," + quoted(d.artifact_source_hash) + "," +
        quoted(d.artifact_game_data_hash) + "," + quoted(d.artifact_strings_hash) + "," +
        std::to_string(s.base_index) + "," + std::to_string(s.item_level) + "," +
        std::to_string(poecraft::kRareReforgeCountLawVersion) + "," +
        std::to_string(static_cast<unsigned>(s.rare_reforge_count_kind)) + "," +
        std::to_string(s.cluster_index) + "," + std::to_string(s.cluster_passive_index) + "," +
        std::to_string(s.cluster_passive_count) + "]";
}
struct ItemFields {
    std::vector<std::int64_t> values;
    std::size_t cursor = 0;
    bool reading = false;
    template<class T> void field(T& v) {
        if (!reading) { values.push_back(v); return; }
        if (cursor >= values.size() || values[cursor] < std::numeric_limits<T>::min() ||
            values[cursor] > std::numeric_limits<T>::max())
            throw std::invalid_argument("Invalid Lock item field range/capacity");
        v = static_cast<T>(values[cursor++]);
    }
    void slot(pc_mod_slot& s) {
        field(s.mod_id); field(s.group_id); field(s.flags); field(s.roll_count);
        for (auto& r : s.rolls) field(r);
        field(s.veiled_option_count);
        for (auto& id : s.veiled_option_mod_ids) field(id);
        field(s.veiled_chosen_mod_id);
        if (s.roll_count > PC_MAX_ROLL_VALUES || s.veiled_option_count > PC_MAX_VEILED_OPTIONS)
            throw std::invalid_argument("Invalid Lock slot capacity");
    }
    void item(pc_item_state& s) {
        field(s.rarity); field(s.quality); field(s.memory_strands); field(s.lifecycle);
        field(s.item_flags); field(s.prefix_count); field(s.suffix_count);
        field(s.implicit_count); field(s.enchantment_count);
        for (auto& m : s.prefixes) slot(m);
        for (auto& m : s.suffixes) slot(m);
        for (auto& m : s.implicits) slot(m);
        for (auto& m : s.enchantments) slot(m);
        field(s.generic_influence_bits); field(s.searing_exarch_tier); field(s.eater_of_worlds_tier);
        field(s.socket_count); for (auto& c : s.socket_colors) field(c); field(s.link_mask);
        if (s.rarity > PC_RARITY_RARE || s.lifecycle > PC_ITEM_DESTROYED || s.memory_strands > 100 ||
            s.prefix_count > PC_MAX_PREFIXES || s.suffix_count > PC_MAX_SUFFIXES ||
            s.implicit_count > PC_MAX_IMPLICITS || s.enchantment_count > PC_MAX_ENCHANTS ||
            s.socket_count > PC_MAX_SOCKETS || (reading && cursor != values.size()))
            throw std::invalid_argument("Invalid Lock item capacity/state");
    }
};
std::vector<std::int64_t> integers(const Value& value) {
    std::vector<std::int64_t> result;
    for (const auto& v : value.as_array()) {
        const auto n = v.as_number();
        if (!std::isfinite(n) || n != std::floor(n) || n < INT32_MIN || n > UINT32_MAX)
            throw std::invalid_argument("Lock checkpoint requires bounded integer fields");
        result.push_back(static_cast<std::int64_t>(n));
    }
    return result;
}
std::string array_json(const std::vector<std::int64_t>& values) {
    std::string out = "[";
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i) out += ','; out += std::to_string(values[i]);
    }
    return out + ']';
}
std::string item_json(pc_item_state s) {
    ItemFields fields; fields.item(s); return array_json(fields.values);
}
pc_item_state read_item(const Value& value, const poecraft::SessionImpl& session) {
    pc_item_state item{};
    ItemFields fields; fields.values = integers(value); fields.reading = true; fields.item(item);
    const auto check = [&](const pc_mod_slot* slots, unsigned count) {
        for (unsigned i = 0; i < count; ++i) {
            const auto& slot = slots[i];
            if (slot.mod_id >= session.mod_count)
                throw std::invalid_argument("Lock checkpoint modifier is outside its session");
            for (unsigned j = 0; j < slot.veiled_option_count; ++j)
                if (slot.veiled_option_mod_ids[j] >= session.mod_count)
                    throw std::invalid_argument("Invalid Lock veiled option identity");
            if (slot.veiled_chosen_mod_id != PC_MOD_NONE && slot.veiled_chosen_mod_id >= session.mod_count)
                throw std::invalid_argument("Invalid Lock chosen modifier identity");
        }
    };
    check(item.prefixes, item.prefix_count); check(item.suffixes, item.suffix_count);
    check(item.implicits, item.implicit_count); check(item.enchantments, item.enchantment_count);
    return item;
}
std::string action_json(const poecraft::ActionParameters& a) {
    if (!a.fossil_indices.empty()) throw std::invalid_argument("Unsupported Lock fossil request");
    return array_json({static_cast<int>(a.type), a.essence_index, a.mod_id,
        a.target_tag_id, a.source_tag_id, a.influence_code, a.tier});
}
Value read_snapshot(const poecraft::SessionImpl& session, const char* text, std::size_t size) {
    if (!text || !size || size > 16 * 1024 * 1024) throw std::invalid_argument("Invalid Lock checkpoint size");
    auto root = poecraft::json::Parser(text, size).parse();
    if (root.at("version").as_string() != "fixed-currency-lock-v2" &&
        root.at("version").as_string() != "independent-cached-lock-v1")
        throw std::invalid_argument("Unsupported Lock checkpoint version");
    const auto identity = session_identity(session);
    const auto expected = poecraft::json::Parser(identity.c_str(), identity.size()).parse();
    const auto& actual = root.at("session").as_array();
    if (actual.size() != expected.array.size()) throw std::invalid_argument("Lock checkpoint session identity mismatch");
    for (std::size_t i = 0; i < actual.size(); ++i)
        if (actual[i].type != expected.array[i].type ||
            (actual[i].type == poecraft::json::Type::String ? actual[i].string != expected.array[i].string : actual[i].number != expected.array[i].number))
            throw std::invalid_argument("Lock checkpoint runtime/base/level/law/configuration identity mismatch");
    return root;
}

bool currency(poecraft::ActionType type) {
    using T = poecraft::ActionType;
    switch (type) {
    case T::Transmute: case T::Augment: case T::Alteration: case T::Regal:
    case T::Alchemy: case T::Chaos: case T::Exalt: case T::Annul: case T::Scour:
    case T::Essence:
    case T::EldritchEmber: case T::EldritchIchor: case T::EldritchExalt:
    case T::EldritchChaos: case T::EldritchAnnul: case T::InfluenceExalt:
    case T::Fracture: case T::FoulbornAugment: case T::FoulbornRegal:
    case T::FoulbornExalt: case T::Dominance: case T::Vaal:
        return true;
    default: return false;
    }
}
// Complete finite domain of the existing allowlist. Runtime identity pins the
// universe, and aliases resolve to the same native parameters before lookup.
std::vector<poecraft::ActionParameters> requests(const poecraft::SessionImpl& session) {
    using T = poecraft::ActionType;
    std::vector<poecraft::ActionParameters> out;
    for (int type = 0; type <= static_cast<int>(T::DoubleCorruption); ++type) {
        if (!currency(static_cast<T>(type))) continue;
        poecraft::ActionParameters a; a.type = static_cast<T>(type);
        if (a.type == T::Essence) {
            for (unsigned i = 0; i < session.data->essence_count; ++i) {
                a.essence_index = i; out.push_back(a);
            }
        } else if (a.type == T::InfluenceExalt) {
            std::vector<int> codes;
            for (const auto& entry : session.data->influence_exalt_code_by_name)
                if (entry.second > 0) codes.push_back(entry.second);
            std::sort(codes.begin(), codes.end());
            codes.erase(std::unique(codes.begin(), codes.end()), codes.end());
            for (auto code : codes) { a.influence_code = code; out.push_back(a); }
        } else if (a.type == T::EldritchEmber || a.type == T::EldritchIchor) {
            for (unsigned tier = 1; tier <= 4; ++tier) { a.tier = tier; out.push_back(a); }
        } else out.push_back(a);
    }
    return out;
}
const poecraft::HinekoraReservation* reservation(const poecraft::HinekoraForesight& f,
    const poecraft::ActionParameters& action) {
    for (const auto& r : f.reservations) if (same_action(r.action, action)) return &r;
    return nullptr;
}
pc_result check(pc_hinekora_lock_handle lock, const pc_item_state* item,
                pc_error_info* out_error) {
    if (!lock || !item) {
        error(out_error, PC_RESULT_INVALID_ARGUMENT, "null Lock or item");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (lock->impl->identity != item) {
        error(out_error, PC_RESULT_INVALID_ARGUMENT, "Lock belongs to a different item identity");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (!current(*lock->impl, item)) {
        error(out_error, PC_RESULT_UNSUPPORTED_FEATURE, "Lock foresight is consumed or invalidated");
        return PC_RESULT_UNSUPPORTED_FEATURE;
    }
    return PC_RESULT_OK;
}
}
namespace poecraft {
ActionOutcome apply_with_foresight(ActionContextImpl& context,
    pc_item_state* item, const ActionParameters& action) {
    auto& f = context.hinekora_foresight;
    const bool active = f && current(*f, item);
    if ((item->item_flags & PC_ITEM_FORESEEN) && !active)
        throw std::invalid_argument("Foreseeing item requires its original native Lock context; copied/imported foresight cannot be dropped");
    if (f && f->identity == item && !active && !f->refresh_allowed && same(f->input, *item))
        throw std::invalid_argument("Modify the item before applying currency to invalidated unchanged foresight; its information cannot be dropped");
    if (active && f->independent && currency(action.type)) {
        const auto* r = reservation(*f, action);
        if (!r || !r->outcome.applied) return {};
        *item = r->preview;
        f->active = false; f->refresh_allowed = true;
        return r->outcome;
    }
    if (active && f->selected && same_action(f->action, action)) {
        *item = f->preview;
        f->active = false;
        f->refresh_allowed = true;
        return f->outcome;
    }
    pc_item_state working = *item;
    working.item_flags &= ~PC_ITEM_FORESEEN;
    if (active && action.type != ActionType::Scour && action.type != ActionType::RemoveCraftedModifiers) {
        // Another observed/adaptively chosen stochastic request needs a joint
        // law. Probe only for refusal in a private context; never spend RNG or
        // emit a guessed conditional outcome through this fixed-request slice.
        ActionContextImpl probe(0); probe.session = context.session;
        auto candidate = working;
        const auto refusal = apply_action(probe, &candidate, action);
        if (!refusal.applied) return refusal;
        throw std::invalid_argument("Lock fixed-currency scope cannot model a different stochastic request; cross-currency correlations are unresolved");
    }
    auto outcome = apply_action(context, &working, action);
    if (outcome.applied) {
        *item = working;
        if (f && f->identity == item) {
            f->active = false;
            f->refresh_allowed = true;
        }
    }
    return outcome;
}
}
const char* pc_hinekora_lock_cost_key(void) { return "hinekora_lock"; }
int32_t pc_hinekora_lock_currency_supported(int32_t action_type) {
    return currency(static_cast<poecraft::ActionType>(action_type)) ? 1 : 0;
}
pc_result pc_hinekora_lock_apply(pc_action_context_handle context,
    pc_item_state* item, pc_hinekora_lock_handle* out_lock, pc_error_info* out_error) {
    if (out_lock) *out_lock = nullptr;
    if (!context || !item || !out_lock) {
        error(out_error, PC_RESULT_INVALID_ARGUMENT, "null Lock application argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    try {
        const auto& previous = context->impl->hinekora_foresight;
        if (item->item_flags & PC_ITEM_FORESEEN)
            throw std::invalid_argument("Modify the item before applying another Lock; foresight is already live or imported");
        if (previous && ((previous->identity == item && same(previous->input, *item) && !previous->refresh_allowed) ||
            (previous->identity != item && previous->active)))
            throw std::invalid_argument("Modify the item before applying another Lock; decline does not refresh foresight");
        if (pending_veil(*item))
            throw std::invalid_argument("Lock preview with pending Unveil offers requires an approved disclosure model");
        auto f = std::make_shared<poecraft::HinekoraForesight>();
        f->session = context->impl->session; f->identity = item; f->input = *item;
        f->independent = true; f->selected = false;
        poecraft::ActionContextImpl sampled(0); sampled.session = f->session;
        sampled.rng = context->impl->rng;
        bool applicable = false;
        for (const auto& action : requests(*f->session)) {
            poecraft::HinekoraReservation r; r.action = action; r.preview = f->input;
            const auto before = sampled.rng;
            try { r.outcome = poecraft::apply_action(sampled, &r.preview, action); }
            catch (const std::invalid_argument&) { r.outcome = {}; }
            if (!r.outcome.applied) { sampled.rng = before; r.preview = f->input; }
            applicable |= r.outcome.applied;
            f->reservations.push_back(std::move(r));
        }
        if (!applicable) throw std::invalid_argument("No supported currency is applicable to this item; no Lock was consumed");
        auto holder = std::make_unique<pc_hinekora_lock>(); holder->impl = f;
        if (previous) previous->active = false;
        context->impl->rng = sampled.rng;
        context->impl->hinekora_foresight = std::move(f);
        item->item_flags |= PC_ITEM_FORESEEN;
        *out_lock = holder.release();
        error(out_error, PC_RESULT_OK, ""); return PC_RESULT_OK;
    } catch (const std::exception& ex) {
        error(out_error, PC_RESULT_UNSUPPORTED_FEATURE, ex.what()); return PC_RESULT_UNSUPPORTED_FEATURE;
    }
}
pc_result pc_hinekora_lock_observe(pc_hinekora_lock_handle lock,
    const pc_item_state* item, const pc_action_request* request,
    pc_item_state* out_preview, pc_action_result* out_result, pc_error_info* out_error) {
    if (!request || !out_preview || !out_result || out_preview == item) {
        error(out_error, PC_RESULT_INVALID_ARGUMENT, "Lock observation requires request and separate output/result");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    try {
        auto rc = check(lock, item, out_error); if (rc != PC_RESULT_OK) return rc;
        poecraft::ActionContextImpl resolver(0); resolver.session = lock->impl->session;
        poecraft::ActionParameters action;
        rc = poecraft::resolve_foresight_request(resolver, *request, action, out_error);
        if (rc != PC_RESULT_OK) return rc;
        auto& f = *lock->impl;
        const auto* r = reservation(f, action);
        if (!f.independent || !r || !r->outcome.applied) {
            error(out_error, PC_RESULT_UNSUPPORTED_FEATURE, "Unsupported or inapplicable Lock observation; no currency was consumed");
            return PC_RESULT_UNSUPPORTED_FEATURE;
        }
        f.action = r->action; f.preview = r->preview; f.outcome = r->outcome; f.selected = true;
        *out_preview = r->preview; result(r->outcome, out_result);
        error(out_error, PC_RESULT_OK, ""); return PC_RESULT_OK;
    } catch (const std::exception& ex) {
        error(out_error, PC_RESULT_INVALID_ARGUMENT, ex.what()); return PC_RESULT_INVALID_ARGUMENT;
    }
}
pc_result pc_hinekora_lock_create(pc_action_context_handle context,
    pc_item_state* item, const pc_action_request* request,
    pc_hinekora_lock_handle* out_lock, pc_error_info* out_error) {
    if (out_lock) *out_lock = nullptr;
    if (!context || !item || !request || !out_lock) {
        error(out_error, PC_RESULT_INVALID_ARGUMENT, "null Lock creation argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    try {
        poecraft::ActionParameters action;
        auto rc = poecraft::resolve_foresight_request(*context->impl, *request,
                                                     action, out_error);
        if (rc != PC_RESULT_OK) return rc;
        if (!currency(action.type)) {
            error(out_error, PC_RESULT_UNSUPPORTED_FEATURE,
                "Lock fixed-currency preview does not support this operation");
            return PC_RESULT_UNSUPPORTED_FEATURE;
        }
        if (pending_veil(*item)) {
            error(out_error, PC_RESULT_UNSUPPORTED_FEATURE,
                "Lock preview with pending Unveil offers requires an approved disclosure model");
            return PC_RESULT_UNSUPPORTED_FEATURE;
        }
        const auto& previous = context->impl->hinekora_foresight;
        if ((item->item_flags & PC_ITEM_FORESEEN) &&
            (!previous || previous->identity != item || !current(*previous, item))) {
            error(out_error, PC_RESULT_UNSUPPORTED_FEATURE,
                "Foreseeing item requires its original native Lock context");
            return PC_RESULT_UNSUPPORTED_FEATURE;
        }
        if (previous) {
            if (previous->identity == item && same(previous->input, *item) &&
                !previous->refresh_allowed) {
                error(out_error, PC_RESULT_UNSUPPORTED_FEATURE,
                    "Modify the item before applying another Lock; decline does not refresh foresight");
                return PC_RESULT_UNSUPPORTED_FEATURE;
            }
            if (previous->identity != item && previous->active) {
                error(out_error, PC_RESULT_UNSUPPORTED_FEATURE,
                    "One live Lock per context; resolve the existing item first");
                return PC_RESULT_UNSUPPORTED_FEATURE;
            }
        }
        auto foresight = std::make_shared<poecraft::HinekoraForesight>();
        foresight->session = context->impl->session;
        foresight->identity = item;
        foresight->input = *item;
        foresight->input.item_flags &= ~PC_ITEM_FORESEEN;
        foresight->preview = foresight->input;
        foresight->action = action;
        // Fresh private caches avoid copying pointers into context pool caches.
        // Reserve exactly the same marginal law as the selected native action.
        poecraft::ActionContextImpl sampled(0);
        sampled.session = context->impl->session;
        sampled.rng = context->impl->rng;
        foresight->outcome = poecraft::apply_action(sampled, &foresight->preview, action);
        if (!foresight->outcome.applied) {
            error(out_error, PC_RESULT_UNSUPPORTED_FEATURE,
                  "Selected currency is inapplicable; no Lock was consumed");
            return PC_RESULT_UNSUPPORTED_FEATURE;
        }
        auto holder = std::make_unique<pc_hinekora_lock>();
        holder->impl = foresight;
        if (previous) previous->active = false;
        context->impl->rng = sampled.rng;
        context->impl->hinekora_foresight = std::move(foresight);
        item->item_flags |= PC_ITEM_FORESEEN;
        *out_lock = holder.release();
        error(out_error, PC_RESULT_OK, "");
        return PC_RESULT_OK;
    } catch (const std::exception& ex) {
        error(out_error, PC_RESULT_UNSUPPORTED_FEATURE, ex.what());
        return PC_RESULT_UNSUPPORTED_FEATURE;
    }
}
pc_result pc_hinekora_lock_preview(pc_hinekora_lock_handle lock,
    const pc_item_state* item, pc_item_state* out_preview,
    pc_action_result* out_result, pc_error_info* out_error) {
    if (!out_preview || !out_result || out_preview == item) {
        error(out_error, PC_RESULT_INVALID_ARGUMENT, "Lock preview requires separate output and result");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    auto rc = check(lock, item, out_error);
    if (rc != PC_RESULT_OK) return rc;
    if (!lock->impl->selected) {
        error(out_error, PC_RESULT_NOT_FOUND, "Observe a supported request before inspecting or committing a selected preview");
        return PC_RESULT_NOT_FOUND;
    }
    *out_preview = lock->impl->preview;
    result(lock->impl->outcome, out_result);
    error(out_error, PC_RESULT_OK, "");
    return PC_RESULT_OK;
}
pc_result pc_hinekora_lock_commit(pc_hinekora_lock_handle lock,
    pc_item_state* item, pc_action_result* out_result, pc_error_info* out_error) {
    if (!out_result) {
        error(out_error, PC_RESULT_INVALID_ARGUMENT, "null Lock commit result");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    auto rc = check(lock, item, out_error);
    if (rc != PC_RESULT_OK) return rc;
    if (!lock->impl->selected) {
        error(out_error, PC_RESULT_NOT_FOUND, "Observe a supported request before committing a selected preview");
        return PC_RESULT_NOT_FOUND;
    }
    *item = lock->impl->preview;
    lock->impl->active = false;
    lock->impl->refresh_allowed = true;
    result(lock->impl->outcome, out_result);
    error(out_error, PC_RESULT_OK, "");
    return PC_RESULT_OK;
}
pc_result pc_hinekora_lock_status(pc_hinekora_lock_handle lock,
    const pc_item_state* item, int32_t* out_active, pc_error_info* out_error) {
    if (!lock || !item || !out_active) {
        error(out_error, PC_RESULT_INVALID_ARGUMENT, "null Lock status argument");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    if (lock->impl->identity != item) {
        error(out_error, PC_RESULT_INVALID_ARGUMENT, "Lock belongs to a different item identity");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    *out_active = current(*lock->impl, item) ? 1 : 0;
    error(out_error, PC_RESULT_OK, "");
    return PC_RESULT_OK;
}
void pc_hinekora_lock_invalidate(pc_hinekora_lock_handle lock) {
    if (lock && lock->impl->active) {
        lock->impl->identity->item_flags &= ~PC_ITEM_FORESEEN;
        lock->impl->active = false;
    }
}
void pc_hinekora_lock_destroy(pc_hinekora_lock_handle lock) { delete lock; }
void pc_hinekora_lock_release_item(pc_action_context_handle context,
    const pc_item_state* item) {
    if (!context) return;
    const auto& f = context->impl->hinekora_foresight;
    if (f && f->identity == item) {
        f->active = false;
        f->identity = nullptr;
    }
}

pc_result pc_hinekora_lock_export(pc_hinekora_lock_handle lock,
    const pc_item_state* item, char* buffer, size_t capacity,
    size_t* out_length, pc_error_info* out_error) {
    if (!lock || !item || !out_length || lock->impl->identity != item) {
        error(out_error, PC_RESULT_INVALID_ARGUMENT, "Lock snapshot requires its original item identity");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    try {
        auto& f = *lock->impl;
        current(f, item);
        std::string out = "{\"version\":" + quoted(f.independent ? "independent-cached-lock-v1" : "fixed-currency-lock-v2") + ",\"session\":" + session_identity(*f.session);
        out += ",\"current\":" + item_json(*item) + ",\"input\":" + item_json(f.input);
        out += ",\"action\":" + action_json(f.action) + ",\"active\":" + (f.active ? "true" : "false");
        out += ",\"refresh_allowed\":"; out += f.refresh_allowed ? "true" : "false";
        if (f.active && f.selected) {
            out += ",\"preview\":" + item_json(f.preview);
            out += ",\"outcome\":" + array_json({f.outcome.applied ? 1 : 0, f.outcome.added, f.outcome.removed});
        }
        if (f.independent) {
            out += ",\"selected\":"; out += f.selected ? "true" : "false";
            out += ",\"reservations\":[";
            for (std::size_t i = 0; i < f.reservations.size(); ++i) {
                const auto& r = f.reservations[i]; if (i) out += ',';
                out += "[" + action_json(r.action) + "," + item_json(r.preview) + "," +
                    array_json({r.outcome.applied ? 1 : 0, r.outcome.added, r.outcome.removed}) + "]";
            }
            out += ']';
        }
        out += '}';
        *out_length = out.size();
        if (buffer) {
            if (capacity <= out.size()) {
                error(out_error, PC_RESULT_CAPACITY_EXCEEDED, "Lock snapshot output buffer is too small");
                return PC_RESULT_CAPACITY_EXCEEDED;
            }
            std::memcpy(buffer, out.c_str(), out.size() + 1);
        }
        error(out_error, PC_RESULT_OK, ""); return PC_RESULT_OK;
    } catch (const std::exception& ex) {
        error(out_error, PC_RESULT_INVALID_ARGUMENT, ex.what()); return PC_RESULT_INVALID_ARGUMENT;
    }
}
pc_result pc_hinekora_lock_snapshot_item(pc_action_context_handle context,
    const char* text, size_t size, pc_item_state* out_item, pc_error_info* out_error) {
    if (!context || !out_item) {
        error(out_error, PC_RESULT_INVALID_ARGUMENT, "Lock snapshot requires context and output item");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    try {
        const auto root = read_snapshot(*context->impl->session, text, size);
        const auto item = read_item(root.at("current"), *context->impl->session);
        if (root.at("active").as_bool() && pending_veil(item))
            throw std::invalid_argument("Active Lock checkpoints with pending Unveil offers require an approved disclosure model");
        *out_item = item;
        error(out_error, PC_RESULT_OK, ""); return PC_RESULT_OK;
    } catch (const std::exception& ex) {
        error(out_error, PC_RESULT_INVALID_ARGUMENT, ex.what()); return PC_RESULT_INVALID_ARGUMENT;
    }
}
pc_result pc_hinekora_lock_restore(pc_action_context_handle context,
    pc_item_state* item, const pc_action_request* request,
    const char* text, size_t size, pc_hinekora_lock_handle* out_lock,
    pc_error_info* out_error) {
    if (out_lock) *out_lock = nullptr;
    if (!context || !item || !out_lock) {
        error(out_error, PC_RESULT_INVALID_ARGUMENT, "Lock restore requires context, item, request and output");
        return PC_RESULT_INVALID_ARGUMENT;
    }
    try {
        const auto& session = *context->impl->session;
        const auto root = read_snapshot(session, text, size);
        const bool independent = root.at("version").as_string() == "independent-cached-lock-v1";
        const bool selected = !independent || root.at("selected").as_bool();
        poecraft::ActionParameters action;
        if (selected) {
            if (!request) throw std::invalid_argument("Selected Lock checkpoint requires its currency request");
            auto rc = poecraft::resolve_foresight_request(*context->impl, *request, action, out_error);
            if (rc != PC_RESULT_OK) return rc;
            const auto expected_action = action_json(action);
            if (!currency(action.type) || integers(root.at("action")) !=
                integers(poecraft::json::Parser(expected_action.c_str(), expected_action.size()).parse()))
                throw std::invalid_argument("Lock checkpoint currency request identity mismatch");
        } else {
            const auto empty_action = action_json(action);
            if (request || root.find("preview") || root.find("outcome") || integers(root.at("action")) !=
                integers(poecraft::json::Parser(empty_action.c_str(), empty_action.size()).parse()))
                throw std::invalid_argument("Unselected Lock checkpoint contains a selected request or outcome");
        }
        const auto saved_current = read_item(root.at("current"), session);
        if (!same(saved_current, *item) || saved_current.item_flags != item->item_flags)
            throw std::invalid_argument("Lock checkpoint full current item identity mismatch");
        auto f = std::make_shared<poecraft::HinekoraForesight>();
        f->session = context->impl->session; f->identity = item; f->action = action;
        f->independent = independent; f->selected = selected;
        f->input = read_item(root.at("input"), session);
        f->active = root.at("active").as_bool();
        f->refresh_allowed = root.at("refresh_allowed").as_bool();
        if (f->input.item_flags & PC_ITEM_FORESEEN)
            throw std::invalid_argument("Lock checkpoint input has an invalid foresight marker");
        if (pending_veil(f->input) || (f->active && pending_veil(*item)))
            throw std::invalid_argument("Lock checkpoints with pending Unveil offers require an approved disclosure model");
        if (f->active) {
            if (f->refresh_allowed || !(item->item_flags & PC_ITEM_FORESEEN) || !same(f->input, *item))
                throw std::invalid_argument("Lock checkpoint active item identity/state mismatch");
            if (f->selected) {
                f->preview = read_item(root.at("preview"), session);
                if (pending_veil(f->preview))
                    throw std::invalid_argument("Lock preview with pending Unveil offers requires an approved disclosure model");
                const auto outcome = integers(root.at("outcome"));
                if ((f->preview.item_flags & PC_ITEM_FORESEEN) || outcome.size() != 3 || outcome[0] != 1 ||
                    outcome[1] < 0 || outcome[1] > PC_MAX_PREFIXES + PC_MAX_SUFFIXES ||
                    outcome[2] < 0 || outcome[2] > PC_MAX_PREFIXES + PC_MAX_SUFFIXES)
                    throw std::invalid_argument("Invalid Lock checkpoint outcome");
                f->outcome = {true, static_cast<int>(outcome[1]), static_cast<int>(outcome[2])};
            }
        } else if (item->item_flags & PC_ITEM_FORESEEN)
            throw std::invalid_argument("Inactive Lock checkpoint cannot retain a foresight marker");
        if (independent) {
            const auto domain = requests(session);
            const auto& entries = root.at("reservations").as_array();
            if (entries.size() != domain.size()) throw std::invalid_argument("Incomplete Lock reservation domain");
            for (std::size_t i = 0; i < domain.size(); ++i) {
                const auto& row = entries[i].as_array();
                const auto encoded = action_json(domain[i]);
                if (row.size() != 3 || integers(row[0]) != integers(poecraft::json::Parser(encoded.c_str(), encoded.size()).parse()))
                    throw std::invalid_argument("Lock reservation request identity mismatch");
                poecraft::HinekoraReservation r; r.action = domain[i]; r.preview = read_item(row[1], session);
                const auto outcome = integers(row[2]);
                if (outcome.size() != 3 || (outcome[0] != 0 && outcome[0] != 1) || outcome[1] < 0 || outcome[1] > 6 || outcome[2] < 0 || outcome[2] > 6 ||
                    pending_veil(r.preview) || (r.preview.item_flags & PC_ITEM_FORESEEN))
                    throw std::invalid_argument("Invalid Lock reservation outcome");
                r.outcome = {outcome[0] == 1, static_cast<int>(outcome[1]), static_cast<int>(outcome[2])};
                if (!r.outcome.applied && (!same(r.preview, f->input) || outcome[1] || outcome[2]))
                    throw std::invalid_argument("Invalid refused Lock reservation");
                f->reservations.push_back(std::move(r));
            }
            if (f->active && f->selected) {
                const auto* r = reservation(*f, f->action);
                if (!r || !r->outcome.applied || !same(r->preview, f->preview) || r->outcome.added != f->outcome.added || r->outcome.removed != f->outcome.removed)
                    throw std::invalid_argument("Selected Lock outcome disagrees with its reservation");
            }
        }
        auto holder = std::make_unique<pc_hinekora_lock>(); holder->impl = f;
        const auto& previous = context->impl->hinekora_foresight;
        if (previous && previous->active) {
            previous->identity->item_flags &= ~PC_ITEM_FORESEEN;
            previous->active = false;
        }
        // Rebinding the same address must retain the checkpoint's marker.
        *item = saved_current;
        context->impl->hinekora_foresight = std::move(f);
        *out_lock = holder.release();
        error(out_error, PC_RESULT_OK, ""); return PC_RESULT_OK;
    } catch (const std::exception& ex) {
        error(out_error, PC_RESULT_INVALID_ARGUMENT, ex.what()); return PC_RESULT_INVALID_ARGUMENT;
    }
}
