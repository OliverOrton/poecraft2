#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace poecraft {
namespace solver {

/* Immutable compiler-side authority for v1 condition composition.
 *
 * Primitive conditions remain opaque leaves because their parsing and
 * execution authority belongs to the strategy compiler/simulator. Composite
 * structure is typed here so route construction can canonicalize and compare
 * complete expressions without reasoning about JSON substrings. */
class ConditionExpr {
public:
    enum class Kind {
        Always,
        Never,
        Opaque,
        All,
        Any,
        Not,
        AtLeast,
    };

    ConditionExpr() : node_(always_node()) {}

    /* Intentionally implicit at the primitive-condition boundary. Existing
     * feature builders produce complete v1 JSON leaves; composites created by
     * route construction use the typed factories below. */
    ConditionExpr(std::string json)
        : node_(make_node(Kind::Opaque, std::move(json), {}, 0)) {}

    static ConditionExpr always() {
        return ConditionExpr(always_node());
    }

    static ConditionExpr never() {
        return ConditionExpr(never_node());
    }

    static ConditionExpr opaque(std::string json) {
        if (json == always_node()->json) return always();
        if (json == never_node()->json) return never();
        return ConditionExpr(
            make_node(Kind::Opaque, std::move(json), {}, 0));
    }

    static ConditionExpr all(std::vector<ConditionExpr> parts) {
        std::vector<ConditionExpr> canonical;
        canonical.reserve(parts.size());
        for (const ConditionExpr& part : parts) {
            if (part.kind() == Kind::Always) continue;
            if (part.kind() == Kind::All) {
                canonical.insert(
                    canonical.end(), part.children().begin(),
                    part.children().end());
            } else {
                canonical.push_back(part);
            }
        }
        deduplicate(canonical);
        if (canonical.empty()) return always();
        if (canonical.size() == 1) return canonical.front();
        return composite(Kind::All, "all", std::move(canonical));
    }

    static ConditionExpr any(std::vector<ConditionExpr> parts) {
        std::vector<ConditionExpr> canonical;
        canonical.reserve(parts.size());
        for (const ConditionExpr& part : parts) {
            if (part.kind() == Kind::Any) {
                canonical.insert(
                    canonical.end(), part.children().begin(),
                    part.children().end());
            } else if (part.kind() != Kind::Never) {
                canonical.push_back(part);
            }
        }
        deduplicate(canonical);
        if (canonical.empty()) return never();
        if (canonical.size() == 1) return canonical.front();
        return composite(Kind::Any, "any", std::move(canonical));
    }

    static ConditionExpr negate(ConditionExpr part) {
        std::vector<ConditionExpr> children;
        children.push_back(std::move(part));
        const std::string json =
            "{\"type\":\"not\",\"conditions\":[" +
            children.front().json() + "]}";
        return ConditionExpr(make_node(
            Kind::Not, json, std::move(children), 0));
    }

    static ConditionExpr at_least(
        const std::size_t count,
        std::vector<ConditionExpr> parts) {
        std::string json =
            "{\"type\":\"at_least\",\"count\":" +
            std::to_string(count) + ",\"conditions\":[";
        for (std::size_t index = 0; index < parts.size(); ++index) {
            if (index != 0) json += ',';
            json += parts[index].json();
        }
        json += "]}";
        return ConditionExpr(make_node(
            Kind::AtLeast, std::move(json), std::move(parts), count));
    }

    Kind kind() const noexcept { return node_->kind; }
    const std::string& json() const noexcept { return node_->json; }
    const std::vector<ConditionExpr>& children() const noexcept {
        return node_->children;
    }
    std::size_t count() const noexcept { return node_->count; }
    std::uint64_t owned_bytes() const noexcept {
        std::uint64_t bytes =
            sizeof(Node) + node_->json.capacity() + 1 +
            node_->children.capacity() * sizeof(ConditionExpr);
        for (const ConditionExpr& child : node_->children) {
            bytes += child.owned_bytes();
        }
        return bytes;
    }

    friend bool operator==(
        const ConditionExpr& left,
        const ConditionExpr& right) noexcept {
        return left.node_ == right.node_ || left.json() == right.json();
    }

private:
    struct Node {
        Kind kind = Kind::Always;
        std::string json;
        std::vector<ConditionExpr> children;
        std::size_t count = 0;
    };

    explicit ConditionExpr(std::shared_ptr<const Node> node)
        : node_(std::move(node)) {}

    static std::shared_ptr<const Node> make_node(
        const Kind kind,
        std::string json,
        std::vector<ConditionExpr> children,
        const std::size_t count) {
        auto node = std::make_shared<Node>();
        node->kind = kind;
        node->json = std::move(json);
        node->children = std::move(children);
        node->count = count;
        return node;
    }

    static const std::shared_ptr<const Node>& always_node() {
        static const std::shared_ptr<const Node> node = make_node(
            Kind::Always, "{\"type\":\"always\"}", {}, 0);
        return node;
    }

    static const std::shared_ptr<const Node>& never_node() {
        static const std::shared_ptr<const Node> node = make_node(
            Kind::Never,
            "{\"type\":\"any\",\"conditions\":[]}", {}, 0);
        return node;
    }

    static void deduplicate(std::vector<ConditionExpr>& parts) {
        std::set<std::string> seen;
        parts.erase(
            std::remove_if(
                parts.begin(), parts.end(),
                [&](const ConditionExpr& part) {
                    return !seen.insert(part.json()).second;
                }),
            parts.end());
    }

    static void append_composite_range(
        std::string& out,
        const char* type,
        const std::vector<ConditionExpr>& parts,
        const std::size_t first,
        const std::size_t last) {
        constexpr std::size_t kMaxCompositeChildren = 1024;
        const std::size_t size = last - first;
        if (size == 1) {
            out += parts[first].json();
            return;
        }
        out += std::string("{\"type\":\"") + type +
               "\",\"conditions\":[";
        if (size <= kMaxCompositeChildren) {
            for (std::size_t index = first; index < last; ++index) {
                if (index != first) out += ',';
                out += parts[index].json();
            }
        } else {
            const std::size_t child_span = std::max<std::size_t>(
                kMaxCompositeChildren,
                (size + kMaxCompositeChildren - 1) /
                    kMaxCompositeChildren);
            bool first_child = true;
            for (std::size_t child_first = first;
                 child_first < last;
                 child_first += child_span) {
                if (!first_child) out += ',';
                first_child = false;
                append_composite_range(
                    out, type, parts, child_first,
                    std::min(last, child_first + child_span));
            }
        }
        out += "]}";
    }

    static ConditionExpr composite(
        const Kind kind,
        const char* type,
        std::vector<ConditionExpr> parts) {
        std::string json;
        append_composite_range(json, type, parts, 0, parts.size());
        return ConditionExpr(make_node(
            kind, std::move(json), std::move(parts), 0));
    }

    std::shared_ptr<const Node> node_;
};

} // namespace solver
} // namespace poecraft
