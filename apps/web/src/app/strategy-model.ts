import { ItemSnapshot, itemSnapshotRarity } from "./workspace/persistence";
import type { Catalog, CatalogEntry } from "./engine-protocol";

export const DEFAULT_STRATEGY_BASE =
    "Metadata/Items/Armours/BodyArmours/BodyInt17";

export type StrategyNodeKind = "start" | "operation" | "router" | "terminal";
export type TerminalKind = "success" | "failure" | "stop";

export interface StrategyPosition {
    x: number;
    y: number;
}

export interface StrategyStartMod {
    mod_key: string;
    fractured?: boolean;
    crafted?: boolean;
}

export interface StrategyBaseState {
    base_key: string;
    item_level: number;
    rarity: "normal" | "magic" | "rare";
    with_implicits?: boolean;
    quality?: number;
    item_flags?: number;
    generic_influence_bits?: number;
    prefixes?: StrategyStartMod[];
    suffixes?: StrategyStartMod[];
}

export interface StrategyOperation {
    type: string;
    params: Record<string, unknown>;
}

export interface StrategyNode {
    id: string;
    kind: StrategyNodeKind;
    name?: string;
    operation?: StrategyOperation;
    terminal?: TerminalKind;
    reason?: string;
    position: StrategyPosition;
    notes?: string;
}

export type StrategyCondition = {
    type: string;
    conditions?: StrategyCondition[];
    children?: StrategyCondition[];
    group?: string;
    family_mod_key?: string;
    family_label?: string;
    min_tier?: number;
    fractured?: boolean;
    rarity?: string;
    value?: number;
    count?: number;
    min?: number;
    max?: number;
    [key: string]: unknown;
};

export interface StrategyEdge {
    id: string;
    from: string;
    to: string;
    priority: number;
    condition?: StrategyCondition;
    label?: string;
    is_default?: boolean;
}

/**
 * Editing model for the condition builder. A condition is authored as a tree of
 * leaves (atomic checks) and groups (combine children with ALL / ANY / AT LEAST
 * N), with an optional NOT on any node. This maps onto the engine's
 * `all`/`any`/`at_least`/`not` composites without the user writing JSON.
 */
export type ConditionGroupMode = "all" | "any" | "at_least";

/** Atomic, non-composite condition types the builder offers as leaves. */
export const LEAF_CONDITION_TYPES = [
    "has_mod_family",
    "rarity_is",
    "open_prefix_count",
    "open_suffix_count",
    "prefix_count_range",
    "suffix_count_range",
    "always",
] as const;

export interface ConditionLeaf {
    kind: "leaf";
    negate: boolean;
    cond: StrategyCondition;
}

export interface ConditionGroupNode {
    kind: "group";
    negate: boolean;
    mode: ConditionGroupMode;
    /** Required matches when mode is `at_least`. */
    count: number;
    children: ConditionTree[];
}

export type ConditionTree = ConditionLeaf | ConditionGroupNode;

const COMPOSITE_TYPES = new Set([
    "all",
    "all_of",
    "any",
    "any_of",
    "at_least",
]);

export function defaultLeafCondition(type: string): StrategyCondition {
    switch (type) {
        case "has_mod_family":
            return { type, family_mod_key: "", min_tier: 1 };
        case "rarity_is":
            return { type, rarity: "rare" };
        case "open_prefix_count":
        case "open_suffix_count":
            return { type, min: 1, max: 3 };
        case "prefix_count_range":
        case "suffix_count_range":
            return { type, min: 1, max: 1 };
        default:
            return { type: "always" };
    }
}

function childConditions(condition: StrategyCondition): StrategyCondition[] {
    return condition.conditions ?? condition.children ?? [];
}

function parseNode(condition: StrategyCondition): ConditionTree {
    const type = condition.type;
    if (type === "not") {
        const inner = childConditions(condition)[0];
        const node = inner
            ? parseNode(inner)
            : ({ kind: "leaf", negate: false, cond: { type: "always" } } as ConditionLeaf);
        node.negate = !node.negate;
        return node;
    }
    if (COMPOSITE_TYPES.has(type)) {
        const children = childConditions(condition).map(parseNode);
        const mode: ConditionGroupMode =
            type === "any" || type === "any_of"
                ? "any"
                : type === "at_least"
                  ? "at_least"
                  : "all";
        return {
            kind: "group",
            negate: false,
            mode,
            count:
                mode === "at_least"
                    ? Math.max(0, Math.min(children.length, condition.count ?? 1))
                    : children.length,
            children,
        };
    }
    return { kind: "leaf", negate: false, cond: { ...condition } };
}

/** Parse a stored condition into a root group ready for editing. */
export function parseConditionTree(
    condition?: StrategyCondition,
): ConditionGroupNode {
    if (!condition || !condition.type) {
        return { kind: "group", negate: false, mode: "all", count: 0, children: [] };
    }
    const root = parseNode(condition);
    if (root.kind === "group" && !root.negate) {
        return root;
    }
    return {
        kind: "group",
        negate: false,
        mode: "all",
        count: 1,
        children: [root],
    };
}

function compileNode(node: ConditionTree): StrategyCondition {
    let base: StrategyCondition;
    if (node.kind === "leaf") {
        base = node.cond?.type ? { ...node.cond } : { type: "always" };
    } else {
        const children = node.children.map(compileNode);
        if (children.length === 0) {
            base = { type: "always" };
        } else if (node.mode === "at_least") {
            base = {
                type: "at_least",
                count: Math.max(0, Math.min(children.length, node.count)),
                conditions: children,
            };
        } else if (children.length === 1) {
            base = children[0];
        } else {
            base = { type: node.mode === "any" ? "any" : "all", conditions: children };
        }
    }
    return node.negate ? { type: "not", conditions: [base] } : base;
}

/** Compile an edited condition tree back into a stored condition. */
export function compileConditionTree(root: ConditionGroupNode): StrategyCondition {
    return compileNode(root);
}

export interface StrategyViewport {
    panX: number;
    panY: number;
    zoom: number;
}

export interface StrategyDocument {
    version: "v1";
    name: string;
    description: string;
    start_node_id: string;
    base_state: StrategyBaseState;
    nodes: StrategyNode[];
    edges: StrategyEdge[];
    ui?: {
        viewport?: StrategyViewport;
    };
}

export type ValidationSeverity = "error" | "warning";

export interface StrategyValidationIssue {
    severity: ValidationSeverity;
    code: string;
    message: string;
    nodeId?: string;
    edgeId?: string;
}

interface ExportedSlot {
    mod_id?: number;
    flags?: number;
}

interface ExportedItemState {
    quality?: number;
    item_flags?: number;
    generic_influence_bits?: number;
    prefixes?: ExportedSlot[];
    suffixes?: ExportedSlot[];
}

export function cloneStrategy(strategy: StrategyDocument): StrategyDocument {
    return structuredClone(strategy);
}

export function isStrategyDocument(value: unknown): value is StrategyDocument {
    if (!value || typeof value !== "object") {
        return false;
    }
    const candidate = value as Partial<StrategyDocument>;
    return (
        candidate.version === "v1" &&
        Array.isArray(candidate.nodes) &&
        Array.isArray(candidate.edges) &&
        typeof candidate.start_node_id === "string" &&
        Boolean(candidate.base_state)
    );
}

export function createDefaultStrategy(
    baseKey = DEFAULT_STRATEGY_BASE,
    itemLevel = 86,
): StrategyDocument {
    return {
        version: "v1",
        name: "Chaos until three prefixes",
        description: "Repeat Chaos Orbs until the item has three prefixes.",
        start_node_id: "start",
        base_state: {
            base_key: baseKey,
            item_level: itemLevel,
            rarity: "rare",
        },
        nodes: [
            {
                id: "start",
                kind: "start",
                name: "Start",
                position: { x: 60, y: 105 },
            },
            {
                id: "chaos",
                kind: "operation",
                name: "Chaos Orb",
                operation: { type: "chaos", params: {} },
                position: { x: 315, y: 105 },
            },
            {
                id: "success",
                kind: "terminal",
                name: "Target reached",
                terminal: "success",
                reason: "Three prefixes",
                position: { x: 590, y: 24 },
            },
        ],
        edges: [
            {
                id: "begin",
                from: "start",
                to: "chaos",
                priority: 0,
                condition: { type: "always" },
                label: "start",
            },
            {
                id: "done",
                from: "chaos",
                to: "success",
                priority: 0,
                condition: { type: "prefix_count_range", min: 3, max: 3 },
                label: "3 prefixes",
            },
            {
                id: "repeat",
                from: "chaos",
                to: "chaos",
                priority: 999,
                condition: { type: "always" },
                label: "else",
                is_default: true,
            },
        ],
        ui: {
            viewport: { panX: 24, panY: 24, zoom: 1 },
        },
    };
}

export function createBlankStrategy(
    baseKey = DEFAULT_STRATEGY_BASE,
    itemLevel = 86,
): StrategyDocument {
    return {
        version: "v1",
        name: "Untitled strategy",
        description: "",
        start_node_id: "",
        base_state: {
            base_key: baseKey,
            item_level: itemLevel,
            rarity: "normal",
            with_implicits: true,
        },
        nodes: [],
        edges: [],
        ui: {
            viewport: { panX: 24, panY: 24, zoom: 1 },
        },
    };
}

export function createStrategyFromItemSnapshot(
    snapshot: ItemSnapshot,
    modKeyForId: (modId: number) => string | undefined,
): StrategyDocument {
    const state = (snapshot.state ?? {}) as ExportedItemState;
    const toMods = (slots: ExportedSlot[] | undefined): StrategyStartMod[] =>
        (slots ?? []).flatMap((slot) => {
            const key =
                typeof slot.mod_id === "number" ? modKeyForId(slot.mod_id) : undefined;
            if (!key) {
                return [];
            }
            const flags = slot.flags ?? 0;
            return [
                {
                    mod_key: key,
                    fractured: (flags & 1) !== 0 || undefined,
                    crafted: (flags & 2) !== 0 || undefined,
                },
            ];
        });

    return {
        version: "v1",
        name: "Imported Emulator item",
        description: "Strategy start state imported from the Emulator.",
        start_node_id: "start",
        base_state: {
            base_key: snapshot.base,
            item_level: snapshot.itemLevel,
            rarity: itemSnapshotRarity(snapshot) as StrategyBaseState["rarity"],
            with_implicits: true,
            quality: state.quality ?? 0,
            item_flags: state.item_flags ?? 0,
            generic_influence_bits: state.generic_influence_bits ?? 0,
            prefixes: toMods(state.prefixes),
            suffixes: toMods(state.suffixes),
        },
        nodes: [
            {
                id: "start",
                kind: "start",
                name: "Emulator item",
                position: { x: 80, y: 105 },
            },
        ],
        edges: [],
        ui: {
            viewport: { panX: 24, panY: 24, zoom: 1 },
        },
    };
}

export function nextGraphId(prefix: string, ids: Iterable<string>): string {
    const used = new Set(ids);
    let index = 1;
    while (used.has(`${prefix}_${index}`)) {
        index += 1;
    }
    return `${prefix}_${index}`;
}

export interface StrategyLabelContext {
    catalog?: Catalog | null;
    /** Session-specific display text for authored bench/unveil modifier keys. */
    modifierNames?: ReadonlyMap<string, string>;
}

function stringParam(
    params: Record<string, unknown>,
    key: string,
): string {
    const value = params[key];
    return typeof value === "string" ? value : "";
}

function entryName(entries: CatalogEntry[] | undefined, key: string): string {
    return entries?.find((entry) => entry.key === key)?.name ?? "";
}

function keyedDisplayName(
    entries: CatalogEntry[] | undefined,
    key: string,
): string {
    return entryName(entries, key) || key;
}

function titleCaseKey(key: string): string {
    return key
        .split("_")
        .map((part) => part.charAt(0).toUpperCase() + part.slice(1))
        .join(" ");
}

export function operationLabel(
    operation?: StrategyOperation,
    context: StrategyLabelContext = {},
): string {
    if (!operation) {
        return "Choose operation";
    }
    const params = operation.params ?? {};
    const catalog = context.catalog;
    switch (operation.type) {
        case "condition_check_only":
            return "Condition router";
        case "essence": {
            const key = stringParam(params, "essence_key");
            return key ? keyedDisplayName(catalog?.essences, key) : "Essence";
        }
        case "fossil": {
            const keys = Array.isArray(params.fossils)
                ? params.fossils.filter(
                      (key): key is string => typeof key === "string" && key.length > 0,
                  )
                : [];
            return keys.length
                ? keys
                      .map((key) => keyedDisplayName(catalog?.fossils, key))
                      .join(" + ")
                : "Fossils";
        }
        case "bench": {
            const key = stringParam(params, "mod_key");
            const name =
                context.modifierNames?.get(key) ||
                keyedDisplayName(catalog?.bench, key);
            return key ? `Bench: ${name}` : "Bench craft";
        }
        case "unveil": {
            const key = stringParam(params, "mod_key");
            const name = context.modifierNames?.get(key) || key;
            return key ? `Unveil: ${name}` : "Unveil";
        }
        case "harvest_reforge":
        case "harvest_augment": {
            const key = stringParam(params, "target_tag");
            const name = keyedDisplayName(catalog?.harvestTags, key);
            const verb =
                operation.type === "harvest_reforge" ? "Reforge" : "Augment";
            return key ? `Harvest ${verb}: ${name}` : `Harvest ${verb}`;
        }
        case "harvest_resist": {
            const source = stringParam(params, "source_tag");
            const target = stringParam(params, "target_tag");
            if (!source || !target) return "Harvest Resistance";
            return `Harvest Resistance: ${keyedDisplayName(
                catalog?.harvestTags,
                source,
            )} → ${keyedDisplayName(catalog?.harvestTags, target)}`;
        }
        case "influence_exalt": {
            const key = stringParam(params, "influence");
            const name = keyedDisplayName(catalog?.influences, key);
            return key ? `Influence Exalt: ${name}` : "Influence Exalt";
        }
        case "eldritch_ember":
        case "eldritch_ichor": {
            const tier = Number(params.tier);
            const base = titleCaseKey(operation.type);
            return Number.isFinite(tier) && tier > 0
                ? `${base} · Tier ${tier}`
                : base;
        }
        default:
            return titleCaseKey(operation.type);
    }
}

/** Empty authored text is automatic; non-empty text is a manual override. */
export function automaticStrategyNodeLabel(
    node: StrategyNode,
    context: StrategyLabelContext = {},
): string {
    return node.kind === "operation"
        ? operationLabel(node.operation, context)
        : node.id;
}

export function strategyNodeLabel(
    node: StrategyNode,
    context: StrategyLabelContext = {},
): string {
    return node.name !== undefined && node.name !== ""
        ? node.name
        : automaticStrategyNodeLabel(node, context);
}

export function conditionLabel(condition?: StrategyCondition, fallback = ""): string {
    if (!condition) {
        return fallback || "always";
    }
    switch (condition.type) {
        case "always":
            return fallback || "always";
        case "has_mod_group":
            return condition.group ? `has ${condition.group}` : "has mod group";
        case "has_mod_family": {
            const tier =
                typeof condition.min_tier === "number" && condition.min_tier > 0
                    ? ` T${condition.min_tier}+`
                    : "";
            const fractured = condition.fractured ? " fractured" : "";
            return condition.family_label
                ? `${condition.family_label}${tier}${fractured}`
                : condition.family_mod_key
                  ? `has${fractured} modifier${tier}`
                  : "has modifier";
        }
        case "rarity_is":
            return `rarity is ${condition.rarity ?? "?"}`;
        case "open_prefix_count":
            return rangeLabel("open prefixes", condition);
        case "open_suffix_count":
            return rangeLabel("open suffixes", condition);
        case "prefix_count_range":
            return rangeLabel("prefixes", condition);
        case "suffix_count_range":
            return rangeLabel("suffixes", condition);
        case "all":
        case "all_of":
            return `ALL of ${(condition.conditions ?? condition.children ?? []).length}`;
        case "any":
        case "any_of":
            return `ANY of ${(condition.conditions ?? condition.children ?? []).length}`;
        case "not":
            return "NOT condition";
        case "at_least":
            return `AT LEAST ${condition.count ?? 1}`;
        default:
            return condition.type || "condition";
    }
}

/** Empty authored text is automatic; non-empty text is a manual override. */
export function automaticStrategyEdgeLabel(edge: StrategyEdge): string {
    return conditionLabel(edge.condition);
}

export function strategyEdgeLabel(edge: StrategyEdge): string {
    return edge.label !== undefined && edge.label !== ""
        ? edge.label
        : automaticStrategyEdgeLabel(edge);
}

function rangeLabel(label: string, condition: StrategyCondition): string {
    const min = condition.min ?? condition.value ?? condition.count ?? 0;
    const max = condition.max;
    return max === undefined || max === min
        ? `${label} = ${min}`
        : `${label} ${min}-${max}`;
}

export function validateStrategy(
    strategy: StrategyDocument,
): StrategyValidationIssue[] {
    const issues: StrategyValidationIssue[] = [];
    const nodeById = new Map<string, StrategyNode>();
    const edgeIds = new Set<string>();

    for (const node of strategy.nodes) {
        if (!node.id) {
            issues.push({
                severity: "error",
                code: "node-id",
                message: "A node has no id.",
            });
            continue;
        }
        if (nodeById.has(node.id)) {
            issues.push({
                severity: "error",
                code: "duplicate-node",
                message: `Duplicate node id: ${node.id}.`,
                nodeId: node.id,
            });
        }
        nodeById.set(node.id, node);
        if (node.kind === "operation" && !node.operation?.type) {
            issues.push({
                severity: "error",
                code: "operation-missing",
                message: `${node.id} has no crafting operation.`,
                nodeId: node.id,
            });
        }
        if (node.kind === "terminal" && !node.terminal) {
            issues.push({
                severity: "error",
                code: "terminal-missing",
                message: `${node.id} has no terminal result type.`,
                nodeId: node.id,
            });
        }
    }

    const starts = strategy.nodes.filter((node) => node.kind === "start");
    if (starts.length !== 1) {
        issues.push({
            severity: "error",
            code: "start-count",
            message: `The graph needs exactly one start node (found ${starts.length}).`,
        });
    }
    if (
        !strategy.start_node_id ||
        nodeById.get(strategy.start_node_id)?.kind !== "start"
    ) {
        issues.push({
            severity: "error",
            code: "start-reference",
            message: "start_node_id must reference the start node.",
            nodeId: strategy.start_node_id,
        });
    }

    const outgoing = new Map<string, StrategyEdge[]>();
    const incoming = new Map<string, StrategyEdge[]>();
    for (const edge of strategy.edges) {
        if (!edge.id || edgeIds.has(edge.id)) {
            issues.push({
                severity: "error",
                code: "duplicate-edge",
                message: edge.id
                    ? `Duplicate edge id: ${edge.id}.`
                    : "An edge has no id.",
                edgeId: edge.id,
            });
        }
        edgeIds.add(edge.id);
        const from = nodeById.get(edge.from);
        const to = nodeById.get(edge.to);
        if (!from || !to) {
            issues.push({
                severity: "error",
                code: "edge-endpoint",
                message: `${edge.id || "An edge"} references a missing node.`,
                edgeId: edge.id,
            });
            continue;
        }
        if (from.kind === "terminal") {
            issues.push({
                severity: "error",
                code: "terminal-outgoing",
                message: `${from.id} is terminal and cannot have outgoing edges.`,
                nodeId: from.id,
                edgeId: edge.id,
            });
        }
        outgoing.set(edge.from, [...(outgoing.get(edge.from) ?? []), edge]);
        incoming.set(edge.to, [...(incoming.get(edge.to) ?? []), edge]);
        validateCondition(edge.condition, edge, issues);
    }

    for (const node of strategy.nodes) {
        const edges = outgoing.get(node.id) ?? [];
        if (node.kind !== "terminal" && edges.length === 0) {
            issues.push({
                severity: "warning",
                code: "dead-end",
                message: `${node.id} has no outgoing edge.`,
                nodeId: node.id,
            });
        }
        if (edges.filter((edge) => edge.is_default).length > 1) {
            issues.push({
                severity: "error",
                code: "multiple-defaults",
                message: `${node.id} has more than one default edge.`,
                nodeId: node.id,
            });
        }
    }

    const reachable = walkForward(strategy.start_node_id, outgoing);
    for (const node of strategy.nodes) {
        if (!reachable.has(node.id)) {
            issues.push({
                severity: "warning",
                code: "unreachable",
                message: `${node.id} is not reachable from the start node.`,
                nodeId: node.id,
            });
        }
    }

    const reachableTerminals = strategy.nodes.filter(
        (node) => reachable.has(node.id) && node.kind === "terminal",
    );
    if (reachableTerminals.length === 0) {
        issues.push({
            severity: "error",
            code: "no-terminal",
            message: "No terminal node is reachable from the start node.",
        });
    }
    if (!reachableTerminals.some((node) => node.terminal === "success")) {
        issues.push({
            severity: "warning",
            code: "no-success",
            message: "No success terminal is reachable from the start node.",
        });
    }

    const terminalIds = new Set(
        strategy.nodes
            .filter((node) => node.kind === "terminal")
            .map((node) => node.id),
    );
    const canReachTerminal = walkBackward(terminalIds, incoming);
    for (const node of strategy.nodes) {
        if (reachable.has(node.id) && !canReachTerminal.has(node.id)) {
            issues.push({
                severity: "warning",
                code: "no-terminal-path",
                message: `${node.id} cannot reach any terminal.`,
                nodeId: node.id,
            });
        }
    }

    return issues;
}

function validateCondition(
    condition: StrategyCondition | undefined,
    edge: StrategyEdge,
    issues: StrategyValidationIssue[],
    depth = 0,
): void {
    if (edge.is_default) {
        return;
    }
    if (!condition?.type) {
        issues.push({
            severity: "error",
            code: "condition-missing",
            message: `${edge.id} has no condition.`,
            edgeId: edge.id,
        });
        return;
    }
    if (depth > 32) {
        issues.push({
            severity: "error",
            code: "condition-depth",
            message: `${edge.id} has a condition nested too deeply.`,
            edgeId: edge.id,
        });
        return;
    }
    const supported = new Set([
        "always",
        "has_mod_group",
        "has_mod_family",
        "rarity_is",
        "open_prefix_count",
        "open_suffix_count",
        "prefix_count_range",
        "suffix_count_range",
        "all",
        "all_of",
        "any",
        "any_of",
        "not",
        "at_least",
    ]);
    if (!supported.has(condition.type)) {
        issues.push({
            severity: "error",
            code: "condition-type",
            message: `${edge.id} uses unsupported condition ${condition.type}.`,
            edgeId: edge.id,
        });
    }
    if (condition.type === "has_mod_group" && !condition.group) {
        issues.push({
            severity: "error",
            code: "condition-group",
            message: `${edge.id} needs a mod group key.`,
            edgeId: edge.id,
        });
    }
    if (
        condition.type === "has_mod_family" &&
        !condition.family_mod_key
    ) {
        issues.push({
            severity: "error",
            code: "condition-family",
            message: `${edge.id} needs a modifier family.`,
            edgeId: edge.id,
        });
    }
    if (
        condition.type === "has_mod_family" &&
        (typeof condition.min_tier !== "number" ||
            condition.min_tier < 0 ||
            !Number.isInteger(condition.min_tier))
    ) {
        issues.push({
            severity: "error",
            code: "condition-tier",
            message: `${edge.id} has an invalid minimum tier.`,
            edgeId: edge.id,
        });
    }
    if (condition.type === "rarity_is" && !condition.rarity) {
        issues.push({
            severity: "error",
            code: "condition-rarity",
            message: `${edge.id} needs a rarity.`,
            edgeId: edge.id,
        });
    }
    const children = condition.conditions ?? condition.children;
    if (
        ["all", "all_of", "any", "any_of", "not", "at_least"].includes(
            condition.type,
        )
    ) {
        if (!children?.length) {
            issues.push({
                severity: "error",
                code: "condition-children",
                message: `${edge.id} has an empty composite condition.`,
                edgeId: edge.id,
            });
        } else {
            children.forEach((child) =>
                validateCondition(child, edge, issues, depth + 1),
            );
        }
    }
}

function walkForward(
    start: string,
    outgoing: Map<string, StrategyEdge[]>,
): Set<string> {
    const visited = new Set<string>();
    const pending = start ? [start] : [];
    while (pending.length) {
        const id = pending.pop()!;
        if (visited.has(id)) {
            continue;
        }
        visited.add(id);
        for (const edge of outgoing.get(id) ?? []) {
            pending.push(edge.to);
        }
    }
    return visited;
}

function walkBackward(
    starts: Set<string>,
    incoming: Map<string, StrategyEdge[]>,
): Set<string> {
    const visited = new Set<string>();
    const pending = [...starts];
    while (pending.length) {
        const id = pending.pop()!;
        if (visited.has(id)) {
            continue;
        }
        visited.add(id);
        for (const edge of incoming.get(id) ?? []) {
            pending.push(edge.from);
        }
    }
    return visited;
}
