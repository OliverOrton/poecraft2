import { ItemSnapshot, itemSnapshotRarity, itemSnapshotCluster } from "./workspace/persistence";
import type { Catalog, CatalogEntry, EconomyIdentity } from "./engine-protocol";
import {
    canonicalInfluenceExaltKey,
    genericInfluenceDisplayName,
} from "./influence-presentation";

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
    veiled?: boolean;
    eldritch?: boolean;
    synth?: boolean;
    rolls?: number[];
    veiled_option_keys?: string[];
    veiled_chosen_key?: string;
}

export interface StrategyBaseState {
    base_key: string;
    item_level: number;
    cluster?: {passive_key: string; passive_count: number};
    rarity: "normal" | "magic" | "rare";
    with_implicits?: boolean;
    quality?: number;
    memory_strands?: number;
    item_flags?: number;
    generic_influence_bits?: number;
    searing_exarch_tier?: number;
    eater_of_worlds_tier?: number;
    prefixes?: StrategyStartMod[];
    suffixes?: StrategyStartMod[];
    /** Exact native implicit list when a compiled policy starts after an Eldritch change. */
    implicits?: StrategyStartMod[];
    enchantments?: StrategyStartMod[];
    socket_colors?: number[];
    link_mask?: number;
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
    /** Paid dependency source, excluded from the sequential control path. */
    source_only?: boolean;
    terminal?: TerminalKind;
    reason?: string;
    position: StrategyPosition;
    notes?: string;
    /** Native solver value V(s), preserved on compiled policy nodes. */
    expected_cost?: number;
    /** Native, non-executable S8.4 classification metadata. */
    accounting_roles?: string[];
}

export type StrategyCondition = {
    type: string;
    base_state?: StrategyBaseState;
    conditions?: StrategyCondition[];
    children?: StrategyCondition[];
    group?: string;
    mod_key?: string;
    mod_keys?: string[];
    family_mod_keys?: string[];
    family_mod_key?: string;
    family_label?: string;
    min_tier?: number;
    fractured?: boolean;
    crafted?: boolean;
    flag?: string;
    side?: string;
    rarity?: string;
    value?: number;
    count?: number;
    min?: number;
    max?: number;
    [key: string]: unknown;
};

export interface StrategyEdge {
    id: string;
    /** Omitted legacy edges retain sequential control semantics. */
    kind?: "control" | "item";
    from_port?: string;
    to_port?: string;
    from: string;
    to: string;
    priority: number;
    condition?: StrategyCondition;
    label?: string;
    is_default?: boolean;
    /** Native, non-executable retry/reapplication classification metadata. */
    accounting_roles?: string[];
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
    "base_is",
    "has_mod_family",
    "item_flag",
    "eldritch_tier",
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
        case "base_is": return {type, base_key: ""};
        case "has_mod_family":
            return { type, family_mod_key: "", min_tier: 1 };
        case "item_flag":
            return { type, flag: "fractured" };
        case "eldritch_tier":
            return { type, side: "searing", min: 1, max: 4 };
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

/** Non-executable provenance for the solver MDP that produced a strategy. */
export type SolverPolicyScope =
    | "unrestricted"
    | "zero_progress_reroll_policy_restriction"
    | "no_economic_restart_policy_restriction"
    | "zero_progress_reroll_and_no_economic_restart_restrictions"
    | "gated_search_with_paid_root_foulborn_salvage_v2";

export interface StrategyOutputContract {
    id: string;
    name?: string;
    /** Checked against the actual native output session, as well as the predicate. */
    base_key: string;
    /** Defaults to the child's fixed current item; explicit slots keep actual sessions. */
    resource_id?: string;
    predicate: StrategyCondition;
}

export interface StrategyFeederReference {
    strategy_id: string;
    revision: string;
    /** Exact embedded saved revision; never resolved from mutable Stash during a run. */
    document_json: string;
    output_contract_id: string;
}

export interface StrategyResource {
    id: string;
    name?: string;
    base_state: StrategyBaseState;
    acquisition_price_key: string;
    initially_owned?: boolean;
    initial_cost_chaos?: number;
    feeder?: StrategyFeederReference;
}

export interface StrategyDocument {
    version: "v1";
    name: string;
    description: string;
    start_node_id: string;
    base_state: StrategyBaseState;
    start_item_present?: boolean;
    /** Templates are unavailable until an explicit priced acquire_resource node. */
    resources?: StrategyResource[];
    output_contracts?: StrategyOutputContract[];
    nodes: StrategyNode[];
    edges: StrategyEdge[];
    solver_policy_scope?: SolverPolicyScope;
    /** Solver provenance; absent on historical strategies. */
    solver_imprint_programs_considered?: boolean;
    economy?: EconomyIdentity;
    ui?: {
        viewport?: StrategyViewport;
    };
}

/** Snapshot one saved revision into a paid output slot. This adds no live item. */
export function pinStrategyFeeder(
    id: string,
    saved: {id: string; name: string; strategy: unknown; revision?: string; createdAt: number},
    outputContractId: string,
): StrategyResource {
    if (!isStrategyDocument(saved.strategy)) throw new Error("Saved feeder is not a strategy document.");
    if (!saved.strategy.output_contracts?.some(contract => contract.id === outputContractId))
        throw new Error("Select a named output contract from the saved child revision.");
    return {
        id, name: saved.name, base_state: structuredClone(saved.strategy.base_state),
        acquisition_price_key: `resource:${id}`,
        feeder: {strategy_id: saved.id, revision: saved.revision ?? `legacy:${saved.createdAt}`,
            document_json: JSON.stringify(saved.strategy), output_contract_id: outputContractId},
    };
}

/** Resource port assignments are explicit slot names alongside control-flow edges. */
export function strategyResourcePorts(node: StrategyNode, document?: StrategyDocument): {inputs: string[]; outputs: string[]} {
    const params = node.operation?.params ?? {};
    const text = (key: string) => typeof params[key] === "string" ? params[key] as string : "?";
    switch (node.operation?.type) {
        case "invoke_feeder": case "acquire_resource": return {inputs: [], outputs: [text("resource_id")]};
        case "move_resource": return {inputs: [text("from")], outputs: [text("to")]};
        case "discard_resource": return {inputs: [text("resource_id")], outputs: []};
        case "awakener": return {inputs: [String((params.roles as {donor?: string})?.donor ?? "?"), "current"], outputs: ["current"]};
        case "recombination": {
            const bound = document ? strategyRecombinationBindings(document, node) : {input_a: text("input_a"), input_b: text("input_b"), output: text("output")};
            return {inputs: [bound.input_a, bound.input_b], outputs: [bound.output]};
        }
        default: return {inputs: [], outputs: []};
    }
}

export interface StrategyConnector { id: string; label: string; y: number }
/** Recombination has exactly two input dots and one output; sources have output only. */
export function strategyNodeConnectors(node: StrategyNode): {inputs: StrategyConnector[]; outputs: StrategyConnector[]} {
    const inputs = node.source_only ? [] : node.operation?.type === "recombination"
        ? [{id: "input_a", label: "Item A / execution", y: 36}, {id: "input_b", label: "Item B", y: 78}]
        : [{id: "input", label: "Input", y: 54}];
    return {inputs, outputs: node.kind === "terminal" ? [] : [{id: "output", label: node.source_only ? "Paid item output" : "Output", y: 54}]};
}
export function strategyOutputSlot(node: StrategyNode): string {
    return strategyResourcePorts(node).outputs[0] ?? "current";
}
/** Shared by initial connections and reconnects; editing does not execute anything. */
export function strategyConnection(document: StrategyDocument, from: string, to: string, toPort?: string): Pick<StrategyEdge, "kind" | "from_port" | "to_port"> {
    const source = document.nodes.find(node => node.id === from), target = document.nodes.find(node => node.id === to);
    if (!source || !target || !strategyNodeConnectors(source).outputs.length || target.source_only)
        throw new Error("Connect an output to an available input dot.");
    const port = toPort ?? strategyNodeConnectors(target).inputs[0]?.id;
    if (!strategyNodeConnectors(target).inputs.some(input => input.id === port)) throw new Error("Choose an available input dot.");
    const item = source.source_only || port === "input_b";
    if (item && (target.operation?.type !== "recombination" || !["input_a", "input_b"].includes(port!)))
        throw new Error("Paid sources supply a Recombination A or B item input.");
    if (item && !source.source_only && !["recombination", "move_resource"].includes(source.operation?.type ?? ""))
        throw new Error("The B input needs a paid source or a real resource output.");
    return {kind: item ? "item" : "control", from_port: "output", to_port: port};
}
export function strategyRecombinationBindings(document: StrategyDocument, node: StrategyNode): {input_a: string; input_b: string; output: string} {
    const params = node.operation?.params ?? {};
    const slot = (port: string, fallback: string) => {
        const edge = document.edges.find(edge => edge.kind === "item" && edge.to === node.id && edge.to_port === port);
        const source = edge && document.nodes.find(source => source.id === edge.from);
        return source ? strategyOutputSlot(source) : String(params[port] ?? fallback);
    };
    return {input_a: slot("input_a", "current"), input_b: slot("input_b", ""), output: String(params.output ?? "")};
}

/** Persist a visible entry when wiring a paid source onto an otherwise blank path. */
export function strategySourceEntryEdge(document: StrategyDocument, edge: StrategyEdge): StrategyEdge | undefined {
    if (edge.kind !== "item" || !document.nodes.some(node => node.id === edge.from && node.source_only)) return;
    const start = document.nodes.find(node => node.id === document.start_node_id && node.kind === "start");
    if (!start || document.edges.some(other => other.from === start.id && other.kind !== "item")) return;
    return {id: nextGraphId("edge", document.edges.map(other => other.id)), from: start.id, to: edge.to,
        ...strategyConnection(document, start.id, edge.to, "input_a"), priority: 0, condition: {type: "always"}};
}

function validateStrategyResources(strategy: StrategyDocument, issues: StrategyValidationIssue[]): void {
    const fail = (message: string, nodeId?: string) => issues.push({severity: "error", code: "resource-contract", message, nodeId});
    const seenRevisions = new Map<string, string>();
    const visit = (document: StrategyDocument, ancestors: Set<string>, depth: number) => {
        if (depth > 16) { fail("Feeder nesting exceeds the native limit."); return; }
        const resources = document.resources ?? [];
        if (!Array.isArray(resources) || resources.length > 7) { fail("Use at most seven explicit resource slots."); return; }
        const ids = new Set<string>(["current"]);
        for (const resource of resources) {
            if (!/^[A-Za-z0-9_-]+$/.test(resource.id) || ids.has(resource.id)) fail(`Invalid or duplicate resource slot: ${resource.id}.`);
            ids.add(resource.id);
            if (!resource.base_state?.base_key || !resource.acquisition_price_key?.trim()) fail(`Resource ${resource.id} requires its starting base and acquisition price key.`);
            const feeder = resource.feeder;
            if (!feeder) continue;
            try {
                if (!feeder.strategy_id || !feeder.revision || !feeder.output_contract_id) throw new Error("Feeder requires a pinned revision and selected output contract.");
                const identity = JSON.stringify([feeder.strategy_id, feeder.revision]);
                if (ancestors.has(identity)) throw new Error("Feeder strategy reference cycle.");
                const previous = seenRevisions.get(identity);
                if (previous !== undefined && previous !== feeder.document_json) throw new Error("One pinned revision contains conflicting documents.");
                seenRevisions.set(identity, feeder.document_json);
                const child: unknown = JSON.parse(feeder.document_json);
                if (!isStrategyDocument(child)) throw new Error("Pinned child is not a strategy document.");
                if (!child.output_contracts?.some(contract => contract.id === feeder.output_contract_id)) throw new Error("Selected feeder output contract is missing from its pinned revision.");
                if (child.base_state.base_key !== resource.base_state.base_key || child.base_state.item_level !== resource.base_state.item_level)
                    throw new Error("Feeder starting session differs from its paid resource template.");
                visit(child, new Set([...ancestors, identity]), depth + 1);
            } catch (error) { fail(`Resource ${resource.id}: ${error instanceof Error ? error.message : String(error)}`); }
        }
        const contracts = document.output_contracts ?? [];
        if (!Array.isArray(contracts) || contracts.length > 32) { fail("Use at most 32 named output contracts."); return; }
        const contractIds = new Set<string>();
        for (const contract of contracts) {
            if (!contract.id || contractIds.has(contract.id) || !contract.base_key || !contract.predicate?.type) fail("Output contracts require unique IDs, actual base identity and a predicate.");
            contractIds.add(contract.id);
            if (!ids.has(contract.resource_id ?? "current")) fail("Output contract references an undeclared slot.");
            validateCondition(contract.predicate, {id: contract.id, from: "", to: "", priority: 0}, issues);
        }
        for (const node of document.nodes) {
            const op = node.operation;
            if (node.source_only !== undefined && (typeof node.source_only !== "boolean" ||
                (node.source_only && (node.kind !== "operation" || !["invoke_feeder", "acquire_resource"].includes(op?.type ?? "")))))
                fail("Output-only sources must be paid acquisition or feeder operations.", node.id);
            if (!op) continue;
            const params = op.params ?? {};
            if (["invoke_feeder", "acquire_resource", "discard_resource"].includes(op.type)) {
                const id = String(params.resource_id ?? "");
                if (!ids.has(id) || (id === "current" && op.type !== "discard_resource")) fail("Choose a declared output/resource slot.", node.id);
                const resource = resources.find(resource => resource.id === id);
                if (op.type === "invoke_feeder" && !resource?.feeder) fail("Invoke feeder needs a pinned child revision.", node.id);
                if (op.type === "acquire_resource" && resource?.feeder) fail("Run the feeder to acquire its paid starting item and execute the child.", node.id);
            }
            if (op.type === "move_resource" && (!ids.has(String(params.from)) || !ids.has(String(params.to)) || params.from === params.to)) fail("Move requires distinct declared source and destination slots.", node.id);
            if (node.source_only && !["invoke_feeder", "acquire_resource"].includes(op.type)) fail("Output-only sources acquire a paid item or run a pinned feeder.", node.id);
            if (op.type === "recombination") {
                const bindings = strategyRecombinationBindings(document, node);
                if (!ids.has(bindings.input_a) || !ids.has(bindings.input_b) || !ids.has(bindings.output) || bindings.output === "current" || bindings.input_a === bindings.input_b)
                    fail("Recombination needs two distinct input slots and a named output slot preserving the actual session.", node.id);
            }
        }
    };
    visit(strategy, new Set(), 0);
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
    mod_key?: string;
    flags?: number;
    rolls?: number[];
    veiled_option_keys?: string[];
    veiled_chosen_key?: string;
}

interface ExportedItemState {
    quality?: number;
    memory_strands?: number;
    item_flags?: number;
    generic_influence_bits?: number;
    searing_exarch_tier?: number;
    eater_of_worlds_tier?: number;
    prefixes?: ExportedSlot[];
    suffixes?: ExportedSlot[];
    implicits?: ExportedSlot[];
    enchantments?: ExportedSlot[];
    socket_colors?: number[];
    link_mask?: number;
    lifecycle?: number;
}

export function cloneStrategy(strategy: StrategyDocument): StrategyDocument {
    return structuredClone(strategy);
}

export function isStrategyDocument(value: unknown): value is StrategyDocument {
    if (!value || typeof value !== "object") {
        return false;
    }
    const candidate = value as Partial<StrategyDocument>;
    const validSolverPolicyScope =
        candidate.solver_policy_scope === undefined ||
        candidate.solver_policy_scope === "unrestricted" ||
        candidate.solver_policy_scope ===
            "no_economic_restart_policy_restriction" ||
        candidate.solver_policy_scope ===
            "zero_progress_reroll_and_no_economic_restart_restrictions" ||
        candidate.solver_policy_scope ===
            "zero_progress_reroll_policy_restriction" ||
        candidate.solver_policy_scope ===
            "gated_search_with_paid_root_foulborn_salvage_v2";
    const validImprintProgramScope =
        candidate.solver_imprint_programs_considered === undefined ||
        typeof candidate.solver_imprint_programs_considered === "boolean";
    return (
        candidate.version === "v1" &&
        Array.isArray(candidate.nodes) &&
        Array.isArray(candidate.edges) &&
        typeof candidate.start_node_id === "string" &&
        Boolean(candidate.base_state) &&
        validSolverPolicyScope &&
        validImprintProgramScope &&
        (candidate.resources === undefined || (Array.isArray(candidate.resources) && candidate.resources.every(resource =>
            resource && typeof resource.id === "string" && typeof resource.acquisition_price_key === "string" && Boolean(resource.base_state)))) &&
        (candidate.output_contracts === undefined || (Array.isArray(candidate.output_contracts) && candidate.output_contracts.every(contract =>
            contract && typeof contract.id === "string" && typeof contract.base_key === "string" && Boolean(contract.predicate))))
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
    const configuration = itemSnapshotCluster(snapshot);
    if (state.lifecycle) throw new Error("A consumed or destroyed resource cannot become a strategy input.");
    const toMods = (slots: ExportedSlot[] | undefined): StrategyStartMod[] =>
        (slots ?? []).flatMap((slot) => {
            const key = slot.mod_key ??
                (typeof slot.mod_id === "number" ? modKeyForId(slot.mod_id) : undefined);
            if (!key) {
                throw new Error("The imported modifier could not be resolved in its session.");
            }
            const flags = slot.flags ?? 0;
            return [
                {
                    mod_key: key,
                    fractured: (flags & 1) !== 0 || undefined,
                    crafted: (flags & 2) !== 0 || undefined,
                    ...((flags & 4) ? {veiled: true} : {}),
                    ...((flags & 8) ? {eldritch: true} : {}),
                    ...((flags & 16) ? {synth: true} : {}),
                    ...(slot.rolls ? {rolls: [...slot.rolls]} : {}),
                    ...(slot.veiled_option_keys ? {veiled_option_keys: [...slot.veiled_option_keys]} : {}),
                    ...(slot.veiled_chosen_key ? {veiled_chosen_key: slot.veiled_chosen_key} : {}),
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
            ...(configuration ? {cluster: {passive_key: configuration.passiveKey, passive_count: configuration.passiveCount}} : {}),
            rarity: itemSnapshotRarity(snapshot) as StrategyBaseState["rarity"],
            with_implicits: true,
            quality: state.quality ?? 0,
            memory_strands: state.memory_strands ?? 0,
            item_flags: state.item_flags ?? 0,
            generic_influence_bits: state.generic_influence_bits ?? 0,
            searing_exarch_tier: state.searing_exarch_tier ?? 0,
            eater_of_worlds_tier: state.eater_of_worlds_tier ?? 0,
            prefixes: toMods(state.prefixes),
            suffixes: toMods(state.suffixes),
            implicits: state.implicits ? toMods(state.implicits) : undefined,
            enchantments: state.enchantments ? toMods(state.enchantments) : undefined,
            socket_colors: state.socket_colors,
            link_mask: state.link_mask,
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

function influenceExaltDisplayName(
    entries: CatalogEntry[] | undefined,
    key: string,
): string {
    const canonical = canonicalInfluenceExaltKey(key);
    return (
        entryName(entries, canonical) ||
        entryName(entries, key) ||
        genericInfluenceDisplayName(canonical)
    );
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
        case "invoke_feeder": return `Run feeder ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ ${stringParam(params, "resource_id") || "choose slot"}`;
        case "move_resource": return `Move ${stringParam(params, "from") || "?"} ÃƒÆ’Ã‚Â¢ÃƒÂ¢Ã¢â€šÂ¬Ã‚Â ÃƒÂ¢Ã¢â€šÂ¬Ã¢â€žÂ¢ ${stringParam(params, "to") || "?"}`;
        case "discard_resource": return `Discard ${stringParam(params, "resource_id") || "choose slot"}`;
        case "acquire_resource": return `Acquire ${stringParam(params, "resource_id") || "donor"}`;
        case "recombination": return "Recombine pair";
        case "restart":
            return "Restart ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â· fresh base";
        case "bestiary:imprint":
            return "Create Imprint";
        case "bestiary:restore_imprint":
            return "Restore Imprint";
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
            )} \u2192 ${keyedDisplayName(catalog?.harvestTags, target)}`;
        }
        case "influence_exalt": {
            const key = stringParam(params, "influence");
            const name = influenceExaltDisplayName(catalog?.influences, key);
            return key ? `Influence Exalt: ${name}` : "Influence Exalt";
        }
        case "eldritch_ember":
        case "eldritch_ichor": {
            const tier = Number(params.tier);
            const base = titleCaseKey(operation.type);
            return Number.isFinite(tier) && tier > 0
                ? `${base} ÃƒÆ’Ã¢â‚¬Å¡Ãƒâ€šÃ‚Â· Tier ${tier}`
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
        case "has_mod_group": {
            const tier =
                typeof condition.min_tier === "number" && condition.min_tier > 0
                    ? ` T${condition.min_tier}+`
                    : "";
            return condition.group ? `has ${condition.group}${tier}` : "has mod group";
        }
        case "has_mod_family": {
            const tier =
                typeof condition.min_tier === "number" && condition.min_tier > 0
                    ? ` T${condition.min_tier}+`
                    : "";
            const fractured = condition.fractured ? " fractured" : "";
            const crafted = condition.crafted ? " crafted" : "";
            return condition.family_label
                ? `${condition.family_label}${tier}${fractured}${crafted}`
                : condition.family_mod_key
                  ? `has${fractured}${crafted} modifier${tier}`
                  : "has modifier";
        }
        case "base_is": return `base is ${String(condition.base_key ?? "?").split("/").at(-1)}`;
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
        case "item_flag":
            return `item is ${String(condition.flag ?? "flagged").replaceAll("_", " ")}`;
        case "influence_bits":
            return `influence bits = ${condition.value ?? 0}`;
        case "eldritch_tier":
            return rangeLabel(`${condition.side ?? "searing"} tier`, condition);
        case "mod_count":
        case "mod_family_count":
            return rangeLabel("matching mods", condition);
        case "has_unveil_option":
            return condition.mod_key ? "has preferred unveil option" : "has unveil option";
        case "observation_signature":
            return "exact policy state";
        case "all":
        case "all_of":
            return compositeConditionLabel("ALL", childConditions(condition));
        case "any":
        case "any_of":
            return compositeConditionLabel("ANY", childConditions(condition));
        case "not": {
            const child = childConditions(condition)[0];
            return child ? `NOT (${conditionLabel(child)})` : "NOT (?)";
        }
        case "at_least":
            return compositeConditionLabel(
                `AT LEAST ${condition.count ?? 1} OF`,
                childConditions(condition),
            );
        default:
            return condition.type || "condition";
    }
}

function compositeConditionLabel(
    operator: string,
    conditions: StrategyCondition[],
): string {
    return `${operator} (${conditions.map((child) => conditionLabel(child)).join("; ")})`;
}

/**
 * Board-friendly form of the same complete condition expression. Composite
 * branches become a small tree and long leaves wrap; no condition is omitted.
 */
export function conditionLabelLines(condition?: StrategyCondition): string[] {
    if (!condition) {
        return ["always"];
    }
    return conditionTreeLabelLines(condition, 0);
}

function conditionTreeLabelLines(
    condition: StrategyCondition,
    depth: number,
): string[] {
    const children = childConditions(condition);
    let heading: string | undefined;
    let visibleChildren = children;
    switch (condition.type) {
        case "all":
        case "all_of":
            heading = children.length > 0 ? "ALL" : "ALL (empty)";
            break;
        case "any":
        case "any_of":
            heading = children.length > 0 ? "ANY" : "ANY (empty)";
            break;
        case "not":
            heading = children.length > 0 ? "NOT" : "NOT (?)";
            visibleChildren = children.slice(0, 1);
            break;
        case "at_least":
            heading =
                children.length > 0
                    ? `AT LEAST ${condition.count ?? 1} OF`
                    : `AT LEAST ${condition.count ?? 1} OF (empty)`;
            break;
        default:
            return wrapConditionLine(conditionLabel(condition), depth);
    }

    return [
        ...wrapConditionLine(heading, depth),
        ...visibleChildren.flatMap((child) =>
            conditionTreeLabelLines(child, depth + 1),
        ),
    ];
}

function wrapConditionLine(text: string, depth: number): string[] {
    const prefix = depth === 0 ? "" : `${"| ".repeat(depth - 1)}|- `;
    const continuation = depth === 0 ? "" : `${"| ".repeat(depth - 1)}|  `;
    return wrapLabelText(text, prefix, continuation);
}

function wrapLabelText(
    text: string,
    firstPrefix = "",
    continuationPrefix = "",
    maxCharacters = 34,
): string[] {
    const words = text.trim().split(/\s+/).filter(Boolean);
    if (words.length === 0) {
        return [firstPrefix];
    }
    const lines: string[] = [];
    let prefix = firstPrefix;
    let content = "";
    for (const word of words) {
        const candidate = content ? `${content} ${word}` : word;
        if (content && prefix.length + candidate.length > maxCharacters) {
            lines.push(`${prefix}${content}`);
            prefix = continuationPrefix;
            content = word;
        } else {
            content = candidate;
        }
    }
    lines.push(`${prefix}${content}`);
    return lines;
}

/** Empty authored text is automatic; non-empty text is a manual override. */
export function automaticStrategyEdgeLabel(edge: StrategyEdge): string {
    return edge.kind === "item" ? `item to ${edge.to_port === "input_b" ? "B" : "A"}` : conditionLabel(edge.condition);
}

export function strategyEdgeLabel(edge: StrategyEdge): string {
    return edge.label !== undefined && edge.label !== ""
        ? edge.label
        : automaticStrategyEdgeLabel(edge);
}

/** Visual wrapping only; authored text and automatic/manual mode are unchanged. */
export function strategyEdgeLabelLines(edge: StrategyEdge): string[] {
    if (edge.label === undefined || edge.label === "") {
        return conditionLabelLines(edge.condition);
    }
    return edge.label.split(/\r?\n/).flatMap((line) => wrapLabelText(line));
}

export interface StrategyEdgeCardRow {
    kind: "group" | "leaf";
    label: string;
    depth: number;
    count?: number;
}

export interface StrategyEdgeCardPresentation {
    title: string;
    header: string;
    count?: number;
    rows: StrategyEdgeCardRow[];
    compact: boolean;
    manual: boolean;
}

/** Structured canvas-card presentation derived from the live edge condition. */
export function strategyEdgeCardPresentation(
    edge: StrategyEdge,
): StrategyEdgeCardPresentation {
    const title = strategyEdgeLabel(edge);
    if (edge.label !== undefined && edge.label !== "") {
        const rows = strategyEdgeLabelLines(edge).map((label) => ({
            kind: "leaf" as const,
            label,
            depth: 0,
        }));
        return {
            title,
            header: rows.length > 1 ? "LABEL" : "",
            rows,
            compact: rows.length === 1,
            manual: true,
        };
    }

    const condition = edge.condition ?? { type: "always" };
    const composite = conditionCompositeHeading(condition);
    if (!composite) {
        return {
            title,
            header: "",
            rows: [
                {
                    kind: "leaf",
                    label: conditionLabel(condition),
                    depth: 0,
                },
            ],
            compact: true,
            manual: false,
        };
    }

    const children = childConditions(condition);
    return {
        title,
        header: composite,
        count: children.length,
        rows: children.flatMap((child) => conditionCardRows(child, 0)),
        compact: false,
        manual: false,
    };
}

function conditionCardRows(
    condition: StrategyCondition,
    depth: number,
): StrategyEdgeCardRow[] {
    const heading = conditionCompositeHeading(condition);
    if (!heading) {
        return [
            {
                kind: "leaf",
                label: conditionLabel(condition),
                depth,
            },
        ];
    }
    const children = childConditions(condition);
    return [
        { kind: "group", label: heading, depth, count: children.length },
        ...children.flatMap((child) => conditionCardRows(child, depth + 1)),
    ];
}

function conditionCompositeHeading(
    condition: StrategyCondition,
): string | undefined {
    switch (condition.type) {
        case "all":
        case "all_of":
            return "ALL";
        case "any":
        case "any_of":
            return "ANY";
        case "not":
            return "NOT";
        case "at_least":
            return `AT LEAST ${condition.count ?? 1}`;
        default:
            return undefined;
    }
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
    validateStrategyResources(strategy, issues);
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
        if (edge.kind === "item") {
            try {
                if (edge.from_port && edge.from_port !== "output") throw new Error("Unknown output connector.");
                if (strategyConnection(strategy, edge.from, edge.to, edge.to_port).kind !== "item") throw new Error("Invalid item connector.");
                if (edge.is_default || (edge.condition?.type && edge.condition.type !== "always")) throw new Error("Item supply uses the consumer's output conditions.");
                if (strategy.edges.some(other => other.id !== edge.id && other.kind === "item" && other.to === edge.to && other.to_port === edge.to_port)) throw new Error("An item input may have only one supply edge.");
            } catch (error) { issues.push({severity: "error", code: "item-connection", message: String(error instanceof Error ? error.message : error), edgeId: edge.id}); }
            continue;
        }
        if ((edge.from_port && edge.from_port !== "output") ||
            (edge.to_port && !strategyNodeConnectors(to).inputs.some(port => port.id === edge.to_port)))
            issues.push({severity: "error", code: "connector", message: "Edge uses an unavailable connector.", edgeId: edge.id});
        if (edge.kind && edge.kind !== "control") issues.push({severity: "error", code: "edge-kind", message: "Unknown edge kind.", edgeId: edge.id});
        if (from.source_only || to.source_only || edge.to_port === "input_b") issues.push({severity: "error", code: "control-connection", message: "Control edges cannot enter/leave paid sources or enter the B item input.", edgeId: edge.id});
        let fromEdges = outgoing.get(edge.from);
        if (!fromEdges) {
            fromEdges = [];
            outgoing.set(edge.from, fromEdges);
        }
        fromEdges.push(edge);
        let toEdges = incoming.get(edge.to);
        if (!toEdges) {
            toEdges = [];
            incoming.set(edge.to, toEdges);
        }
        toEdges.push(edge);
        validateCondition(edge.condition, edge, issues);
    }

    for (const node of strategy.nodes) {
        const edges = outgoing.get(node.id) ?? [];
        if (node.kind !== "terminal" && !node.source_only && edges.length === 0) {
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
        if (!reachable.has(node.id) && !(node.source_only && strategy.edges.some(edge => edge.kind === "item" && edge.from === node.id && reachable.has(edge.to)))) {
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
        "mod_count",
        "mod_family_count",
        "item_flag",
        "influence_bits",
        "eldritch_tier",
        "has_unveil_option",
        /*
         * Engine-authored exact-policy routers carry a versioned observation
         * program. Preserve and accept that payload opaquely; native strategy
         * compilation remains its shape and semantic authority.
         */
        "observation_signature",
        "full_item_is",
        "rarity_is",
        "base_is",
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
        condition.type === "has_mod_group" &&
        condition.min_tier !== undefined &&
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
    if (condition.type === "base_is" && !condition.base_key) issues.push({severity: "error", code: "condition-base", message: `${edge.id} needs an actual base key.`, edgeId: edge.id});
    if (condition.type === "rarity_is" && !condition.rarity) {
        issues.push({
            severity: "error",
            code: "condition-rarity",
            message: `${edge.id} needs a rarity.`,
            edgeId: edge.id,
        });
    }
    if (condition.type === "item_flag" && !condition.flag) {
        issues.push({
            severity: "error",
            code: "condition-flag",
            message: `${edge.id} needs an item flag.`,
            edgeId: edge.id,
        });
    }
    if (condition.type === "mod_count" && !condition.mod_keys?.length) {
        issues.push({
            severity: "error",
            code: "condition-mod-keys",
            message: `${edge.id} needs modifier keys to count.`,
            edgeId: edge.id,
        });
    }
    if (
        condition.type === "mod_family_count" &&
        !condition.family_mod_keys?.length
    ) {
        issues.push({
            severity: "error",
            code: "condition-mod-keys",
            message: `${edge.id} needs modifier family keys to count.`,
            edgeId: edge.id,
        });
    }
    if (condition.type === "has_unveil_option" && !condition.mod_key) {
        issues.push({
            severity: "error",
            code: "condition-mod-key",
            message: `${edge.id} needs an unveil modifier key.`,
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

export function strategyClusterConfiguration(strategy: Pick<StrategyDocument, "base_state">): import("./engine-protocol").ClusterConfiguration | undefined {
    const c = strategy.base_state.cluster;
    return c ? {passiveKey: c.passive_key, passiveCount: c.passive_count} : undefined;
}
