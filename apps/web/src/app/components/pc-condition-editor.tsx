/*
 * Recursive edge-condition composer. The graph owns routing; this editor only
 * authors the selected edge's condition tree and preserves its stored shape.
 */

import {
    LEAF_CONDITION_TYPES,
    type ConditionGroupMode,
    type ConditionGroupNode,
    type ConditionLeaf,
    type ConditionTree,
    type StrategyCondition,
    type StrategyEdge,
    compileConditionTree,
    conditionLabel,
    defaultLeafCondition,
    parseConditionTree,
} from "../strategy-model";
import type { BaseInfo } from "../engine-protocol";
import type { ModifierFamilyOption } from "../modifier-options";
import "./pc-modifier-picker";

const LEAF_LABELS: Record<string, string> = {
    base_is: "Actual base",
    has_mod_family: "Has modifier",
    item_flag: "Item flag",
    eldritch_tier: "Eldritch tier",
    rarity_is: "Rarity",
    open_prefix_count: "Open prefixes",
    open_suffix_count: "Open suffixes",
    prefix_count_range: "Prefix count",
    suffix_count_range: "Suffix count",
    always: "Always",
};

const ITEM_FLAGS = [
    "corrupted",
    "mirrored",
    "split",
    "synthesised",
    "fractured",
    "crafted",
    "veiled",
    "veiled_prefix",
    "veiled_suffix",
    "multimod",
    "no_attack",
    "no_caster",
    "prefixes_locked",
    "suffixes_locked",
    "influenced",
    "eldritch_implicit",
] as const;

const RANGE_TYPES = new Set([
    "open_prefix_count",
    "open_suffix_count",
    "prefix_count_range",
    "suffix_count_range",
]);


import type { ReactNode } from "react";
import { disconnectReact, renderReact } from "../react-host";
import { formatModText } from "../mod-text";
import { ModifierPicker } from "./pc-modifier-picker";

export class PcConditionEditor extends HTMLElement {
    private edge: StrategyEdge | null = null;
    private root: ConditionGroupNode = emptyRoot();
    private modifierFamilies: ModifierFamilyOption[] = [];
    private bases: BaseInfo[] = [];
    private jsonError = "";
    private jsonDraft = "";
    private jsonOpen = false;

    connectedCallback(): void { this.render(); }
    disconnectedCallback(): void { disconnectReact(this); }
    setEdge(edge: StrategyEdge | null): void {
        const changed = this.edge?.id !== edge?.id;
        this.edge = edge;
        this.root = editableRoot(edge?.condition);
        if (changed) { this.jsonOpen = false; this.jsonError = ""; }
        this.jsonDraft = JSON.stringify(this.currentCondition(), null, 2);
        this.render();
    }
    setBases(bases: BaseInfo[]): void { this.bases = bases; this.render(); }
    setModifierFamilies(options: ModifierFamilyOption[]): void { this.modifierFamilies = options; this.render(); }

    private render(): void {
        if (!this.edge) { renderReact(this, null); return; }
        const fallback = Boolean(this.edge.is_default);
        const summary = fallback ? "When no guarded edge matches" : conditionLabel(this.currentCondition());
        renderReact(this, <section className="pc-condition-editor">
            <div className={"pc-condition-summary " + (fallback ? "is-fallback" : "")}>
                <span className="pc-condition-summary-dot" />
                <div><span>{fallback ? "Default route" : "Condition"}</span><strong title={summary}>{formatModText(summary)}</strong></div>
            </div>
            <div className="pc-edge-kind">
                <button data-action="guarded" className={fallback ? "" : "is-active"} aria-pressed={!fallback}
                    onClick={() => { this.edge!.is_default = false; this.commit(); }}>Guarded edge</button>
                <button data-action="default" className={fallback ? "is-active" : ""} aria-pressed={fallback}
                    onClick={() => { this.edge!.is_default = true; this.commit(); }}>Default fallback</button>
            </div>
            {fallback ? <p className="pc-condition-empty">Taken when no guarded edge above it matches.</p> : <>
                <div className="pc-cond-builder">{this.renderNode(this.root, [], 1, true)}</div>
                <details className="pc-condition-json" open={this.jsonOpen}
                    onToggle={event => { this.jsonOpen = event.currentTarget.open; }}>
                    <summary>Advanced JSON</summary>
                    <textarea data-action="json" aria-label="Condition JSON" spellCheck={false} value={this.jsonDraft}
                        onChange={event => { this.jsonDraft = event.target.value; this.render(); }} />
                    <div className="pc-condition-json-actions">
                        <button data-action="apply-json" onClick={() => this.applyJson()}>Apply JSON</button>
                        <span className="pc-condition-json-error" role="alert">{this.jsonError}</span>
                    </div>
                </details>
            </>}
        </section>);
    }

    private edit(path: number[], mutate: (node: ConditionTree) => void): void {
        const node = this.nodeAt(path);
        if (!node) return;
        mutate(node);
        this.commit();
    }

    private renderNode(node: ConditionTree, path: number[], siblingCount: number, isRoot = false): ReactNode {
        if (isModifierSetNode(node)) return this.renderModifierSet(node, path, isRoot, siblingCount);
        const key = pathKey(path);
        if (node.kind === "leaf") return <div key={key} className={"pc-cond-leaf " + (node.negate ? "is-negated" : "")}
            data-node-path={key}>
            <div className="pc-cond-leaf-head">
                {this.typeSelect(node.cond.type, path)}
                {this.nodeActions(node, path, siblingCount)}
            </div>
            <div className="pc-cond-leaf-body">{this.leafFields(node, path)}</div>
        </div>;
        return <div key={key} className={"pc-cond-group " + (isRoot ? "is-root " : "") + (node.negate ? "is-negated" : "")}
            data-node-path={key} data-depth={path.length}>
            <div className="pc-cond-group-head">
                <span className="pc-cond-group-title">{isRoot ? "Match" : "Group " + path.map(i => i + 1).join(".")}</span>
                <div className="pc-cond-logic" role="group" aria-label={isRoot ? "Root condition logic" : "Nested condition logic"}>
                    {(["all", "any", "at_least"] as const).map(mode => <button key={mode} data-action="group-mode"
                        data-path={key} data-mode={mode} className={node.mode === mode ? "is-active" : ""} aria-pressed={node.mode === mode}
                        onClick={() => this.edit(path, group => { if (group.kind === "group") group.mode = mode; })}>
                        {mode === "all" ? "ALL" : mode === "any" ? "ANY" : "N OF"}
                    </button>)}
                </div>
                {node.mode === "at_least" && this.countInput(node, path, "group-count")}
                {!isRoot && this.nodeActions(node, path, siblingCount)}
            </div>
            <div className="pc-cond-children">
                {node.children.length ? node.children.map((child, index) => <div className="pc-cond-branch" key={index}>
                    {index > 0 && <span className="pc-cond-join">{node.mode === "all" ? "AND" : node.mode === "any" ? "OR" : "OF"}</span>}
                    {this.renderNode(child, [...path, index], node.children.length)}
                </div>) : <p className="pc-condition-empty is-compact">No conditions — always passes.</p>}
            </div>
            <div className="pc-cond-add">
                <button data-action="add-leaf" data-path={key} onClick={() => this.edit(path, group => {
                    if (group.kind === "group") group.children.push(newLeaf("rarity_is"));
                })}>+ Condition</button>
                <button data-action="add-group" data-path={key} onClick={() => this.edit(path, group => {
                    if (group.kind === "group") group.children.push({kind: "group", negate: false, mode: "all", count: 1, children: [newLeaf("rarity_is")]});
                })}>+ Group</button>
            </div>
        </div>;
    }

    private typeSelect(type: string, path: number[]): ReactNode {
        return <select data-action="condition-type" data-path={pathKey(path)} aria-label="Condition type" value={type}
            onChange={event => {
                const replacement = newLeaf(event.target.value);
                replacement.negate = this.nodeAt(path)?.negate ?? false;
                this.replaceNode(path, replacement);
                this.commit();
            }}>
            {!LEAF_CONDITION_TYPES.includes(type as typeof LEAF_CONDITION_TYPES[number]) && <option value={type}>Advanced: {type}</option>}
            {LEAF_CONDITION_TYPES.map(value => <option key={value} value={value}>{LEAF_LABELS[value]}</option>)}
        </select>;
    }
    private nodeActions(node: ConditionTree, path: number[], count: number): ReactNode {
        const index = path.at(-1) ?? 0;
        const key = pathKey(path);
        return <>
            <button data-action="toggle-not" data-path={key} className={"pc-cond-not " + (node.negate ? "is-active" : "")}
                aria-pressed={node.negate} onClick={() => this.edit(path, target => { target.negate = !target.negate; })}>NOT</button>
            <div className="pc-cond-node-actions">
                {([-1, 1] as const).map(offset => <button key={offset} className="pc-cond-icon" data-action={offset === -1 ? "move-up" : "move-down"}
                    title={offset === -1 ? "Move up" : "Move down"} disabled={offset === -1 ? index === 0 : index >= count - 1}
                    onClick={() => {
                        const ref = this.parentRef(path);
                        if (!ref) return;
                        const [child] = ref.parent.children.splice(ref.index, 1);
                        ref.parent.children.splice(ref.index + offset, 0, child);
                        this.commit();
                    }}>{offset === -1 ? "↑" : "↓"}</button>)}
                <button className="pc-cond-icon" data-action="duplicate-node" title="Duplicate" onClick={() => {
                    const ref = this.parentRef(path); if (!ref) return;
                    ref.parent.children.splice(ref.index + 1, 0, structuredClone(node)); this.commit();
                }}>⎘</button>
                <button className="pc-cond-remove" data-action="remove-node" title="Remove" onClick={() => {
                    const ref = this.parentRef(path); if (!ref) return;
                    ref.parent.children.splice(ref.index, 1); this.commit();
                }}>×</button>
            </div>
        </>;
    }

    private leafFields(leaf: ConditionLeaf, path: number[]): ReactNode {
        const condition = leaf.cond;
        const key = pathKey(path);
        const set = (field: string, value: unknown) => this.edit(path, node => { if (node.kind === "leaf") node.cond[field] = value; });
        if (condition.type === "rarity_is" || condition.type === "item_flag") {
            const rarity = condition.type === "rarity_is";
            const values = rarity ? ["normal", "magic", "rare"] : ITEM_FLAGS;
            return <label className="pc-cond-inline-field"><span>{rarity ? "Required rarity" : "Required flag"}</span>
                <select data-action={rarity ? "rarity" : "item-flag"} data-path={key} value={String(rarity ? condition.rarity : condition.flag)}
                    onChange={event => set(rarity ? "rarity" : "flag", event.target.value)}>
                    {values.map(value => <option key={value} value={value}>{titleCase(value.replaceAll("_", " "))}</option>)}
                </select>
            </label>;
        }
        if (condition.type === "base_is") return <label className="pc-field"><span>Actual output base</span>
            <select aria-label="Actual output base" value={String(condition.base_key ?? "")}
                onChange={event => set("base_key", event.target.value)}><option value="">Choose base</option>
                {this.bases.map(base => <option key={base.path} value={base.path}>{base.name}</option>)}</select></label>;
        if (RANGE_TYPES.has(condition.type) || condition.type === "eldritch_tier") {
            const eldritch = condition.type === "eldritch_tier";
            const min = condition.min ?? (eldritch ? 1 : condition.value ?? condition.count ?? 0);
            const max = condition.max ?? (eldritch ? 4 : min);
            return <div className="pc-cond-range">
                {eldritch && <label><span>Influence</span><select data-action="eldritch-side" data-path={key}
                    value={String(condition.side ?? "searing")} onChange={event => set("side", event.target.value)}>
                    <option value="searing">Searing Exarch</option><option value="eater">Eater of Worlds</option>
                </select></label>}
                <label><span>{eldritch ? "Minimum tier" : "Minimum"}</span><input type="number" min={0} max={eldritch ? 4 : 6}
                    data-action="min" data-path={key} value={min} onChange={event => set("min", Number(event.target.value))} /></label>
                <span className="pc-cond-range-separator">to</span>
                <label><span>{eldritch ? "Maximum tier" : "Maximum"}</span><input type="number" min={0} max={eldritch ? 4 : 6}
                    data-action="max" data-path={key} value={max} onChange={event => set("max", Number(event.target.value))} /></label>
            </div>;
        }
        return <p className="pc-condition-empty is-compact">{condition.type === "always"
            ? "Always passes." : "Preserved condition. Edit in Advanced JSON."}</p>;
    }

    private countInput(group: ConditionGroupNode, path: number[], action: string): ReactNode {
        return <label className="pc-cond-count-wrap">
            <input type="number" aria-label="Required matches" min={1} max={Math.max(1, group.children.length)}
                data-action={action} data-path={pathKey(path)} value={Math.max(1, Math.min(group.children.length || 1, group.count || 1))}
                onChange={event => this.edit(path, node => { if (node.kind === "group") node.count = Math.max(1, Number(event.target.value) || 1); })} />
            <span>of {group.children.length}</span>
        </label>;
    }

    private renderModifierSet(node: ConditionTree, path: number[], isRoot: boolean, siblings: number): ReactNode {
        const members = node.kind === "leaf" ? [{leaf: node, path}] : node.children.map((leaf, index) => ({leaf: leaf as ConditionLeaf, path: [...path, index]}));
        const selected = members.filter(({leaf}) => leaf.cond.family_mod_key);
        const mode = node.kind === "group" ? node.mode : "all";
        return <div className={"pc-cond-leaf pc-cond-modifier-set " + (isRoot ? "is-root " : "") + (node.negate ? "is-negated" : "")}
            data-node-path={pathKey(path)}>
            <div className="pc-cond-leaf-head">
                {this.typeSelect("has_mod_family", path)}
                {!isRoot && this.nodeActions(node, path, siblings)}
            </div>
            <div className="pc-cond-leaf-body">
                <div className="pc-cond-modifier-logic"><span>Require</span>
                    <div className="pc-cond-logic" role="group" aria-label="Modifier matching">
                        {(["all", "at_least"] as const).map(value => <button key={value} data-action="modifier-set-mode" data-mode={value}
                            className={mode === value ? "is-active" : ""} aria-pressed={mode === value} onClick={() => {
                                if (node.kind === "group") node.mode = value;
                                else {
                                    const member = structuredClone(node); member.negate = false;
                                    this.replaceNode(path, {kind: "group", negate: node.negate, mode: value, count: 1, children: [member]});
                                }
                                this.commit();
                            }}>{value === "all" ? "ALL" : "N OF"}</button>)}
                    </div>
                    {node.kind === "group" && mode === "at_least" ? this.countInput(node, path, "modifier-set-count")
                        : <span className="pc-cond-modifier-count">{selected.length} selected</span>}
                </div>
                <div className="pc-cond-selected-modifiers">
                    {selected.map(({leaf, path: memberPath}) => {
                        const condition = leaf.cond;
                        const family = this.modifierFamilies.find(option => option.value === condition.family_mod_key);
                        const tier = Math.max(0, Number(condition.min_tier ?? 1) || 0);
                        const label = formatModText(family?.tiers.find(choice => choice.tier === tier)?.label ||
                            family?.label || condition.family_label || condition.family_mod_key || "");
                        return <div key={pathKey(memberPath)} className={"pc-cond-selected-modifier " + (condition.fractured ? "is-fractured" : "")}>
                            <span className="pc-cond-tier-badge" title="Required tier or better">{tier ? `T${tier}` : "Any"}</span>
                            <div className="pc-cond-selected-modifier-copy"><strong>{label}</strong>
                                <span>{family?.side === "prefix" ? "Prefix" : family?.side === "suffix" ? "Suffix" : "Modifier"}
                                    {family?.sourceLabel ? " · " + family.sourceLabel : ""}</span>
                            </div>
                            <label><span>Tier</span><select data-action="modifier-tier" data-path={pathKey(memberPath)} aria-label={"Required tier: " + label}
                                value={tier} onChange={event => this.edit(memberPath, member => {
                                    if (member.kind === "leaf") member.cond.min_tier = Math.max(0, Number(event.target.value) || 0);
                                })}>
                                <option value={0}>Any tier</option>
                                {(family?.tiers.length ? family.tiers : [{tier, label}]).map(choice => <option key={choice.tier}
                                    value={choice.tier} title={formatModText(choice.label)}>T{choice.tier} or better</option>)}
                            </select></label>
                            <label className="pc-cond-fractured-toggle"><input type="checkbox" data-action="modifier-fractured"
                                data-path={pathKey(memberPath)} checked={Boolean(condition.fractured)} onChange={event => this.edit(memberPath, member => {
                                    if (member.kind === "leaf") member.cond.fractured = event.target.checked || undefined;
                                })} /><span>Fractured</span></label>
                            <button data-action="remove-modifier-member" data-path={pathKey(memberPath)} title="Remove modifier"
                                onClick={() => { this.removeModifierMember(memberPath); this.commit(); }}>×</button>
                        </div>;
                    })}
                </div>
                <ModifierPicker options={this.modifierFamilies} selected={new Set(selected.map(({leaf}) => leaf.cond.family_mod_key!))}
                    buttonLabel={selected.length ? "+ Add modifier" : "Choose modifiers"} onSelect={(value, fractured) => this.addModifier(path, value, fractured)} />
            </div>
            {isRoot && <div className="pc-cond-add">
                <button data-action="add-root-condition" onClick={() => this.wrapRootModifierSet(newLeaf("rarity_is"))}>+ Condition</button>
                <button data-action="add-root-group" onClick={() => this.wrapRootModifierSet({
                    kind: "group", negate: false, mode: "all", count: 1, children: [newLeaf("rarity_is")],
                })}>+ Group</button>
            </div>}
        </div>;
    }

    private addModifier(path: number[], value: string, fractured: boolean): void {
        const family = this.modifierFamilies.find(option => option.value === value);
        const current = this.nodeAt(path);
        if (!family || !current || !isModifierSetNode(current)) return;
        const member = newLeaf("has_mod_family");
        member.cond = {type: "has_mod_family", family_mod_key: family.value, family_label: family.label, min_tier: 1, fractured: fractured || undefined};
        if (current.kind === "group") {
            const blank = current.children.find(child => isModifierLeaf(child) && !child.cond.family_mod_key) as ConditionLeaf | undefined;
            if (blank) blank.cond = member.cond;
            else current.children.push(member);
        } else if (!current.cond.family_mod_key) current.cond = member.cond;
        else {
            const existing = structuredClone(current); existing.negate = false;
            this.replaceNode(path, {kind: "group", negate: current.negate, mode: "all", count: 2, children: [existing, member]});
        }
        this.commit();
    }
    private wrapRootModifierSet(sibling: ConditionTree): void {
        const modifierSet = structuredClone(this.root);
        modifierSet.negate = false;
        this.root = {
            kind: "group",
            negate: false,
            mode: "all",
            count: 2,
            children: [modifierSet, sibling],
        };
        this.commit();
    }

    private replaceNode(path: number[], replacement: ConditionTree): void {
        if (!path.length) {
            this.root =
                replacement.kind === "group" && !replacement.negate
                    ? replacement
                    : {
                          kind: "group",
                          negate: false,
                          mode: "all",
                          count: 1,
                          children: [replacement],
                      };
            return;
        }
        const ref = this.parentRef(path);
        if (ref) ref.parent.children[ref.index] = replacement;
    }

    private removeModifierMember(path: number[]): void {
        const ref = this.parentRef(path);
        if (ref && isModifierSetGroup(ref.parent)) {
            const group = ref.parent;
            const groupPath = path.slice(0, -1);
            group.children.splice(ref.index, 1);
            if (group.children.length <= 1) {
                const replacement =
                    group.children[0] ?? newLeaf("has_mod_family");
                replacement.negate =
                    Boolean(replacement.negate) !== Boolean(group.negate);
                this.replaceNode(groupPath, replacement);
            }
            return;
        }
        const leaf = this.leafAt(path);
        if (leaf?.cond.type === "has_mod_family") {
            leaf.cond = defaultLeafCondition("has_mod_family");
        }
    }

    private applyJson(): void {
        try {
            const parsed = JSON.parse(this.jsonDraft) as StrategyCondition;
            if (!parsed || typeof parsed.type !== "string") {
                throw new Error("Condition JSON needs a string type.");
            }
            this.jsonError = "";
            this.root = editableRoot(parsed);
            this.commit();
        } catch (reason) {
            this.jsonError =
                reason instanceof Error ? reason.message : String(reason);
            this.render();
        }
    }

    private currentCondition(): StrategyCondition {
        if (this.edge?.is_default) return { type: "always" };
        return compileConditionTree(this.root);
    }

    private commit(): void {
        if (!this.edge) return;
        normalizeGroupCounts(this.root);
        const condition = this.currentCondition();
        this.edge.condition = condition;
        this.jsonDraft = JSON.stringify(condition, null, 2);
        this.render();
        this.dispatchEvent(
            new CustomEvent("condition-change", {
                bubbles: true,
                detail: {
                    ...this.edge,
                    is_default: Boolean(this.edge.is_default),
                    condition,
                },
            }),
        );
    }

    private nodeAt(path: number[]): ConditionTree | undefined {
        let node: ConditionTree = this.root;
        for (const index of path) {
            if (node.kind !== "group") return undefined;
            node = node.children[index];
            if (!node) return undefined;
        }
        return node;
    }

    private groupAt(path: number[]): ConditionGroupNode | undefined {
        const node = this.nodeAt(path);
        return node?.kind === "group" ? node : undefined;
    }

    private leafAt(path: number[]): ConditionLeaf | undefined {
        const node = this.nodeAt(path);
        return node?.kind === "leaf" ? node : undefined;
    }

    private parentRef(
        path: number[],
    ): { parent: ConditionGroupNode; index: number } | undefined {
        if (!path.length) return undefined;
        const parent = this.groupAt(path.slice(0, -1));
        const index = path.at(-1);
        return parent && index !== undefined ? { parent, index } : undefined;
    }
}

function emptyRoot(): ConditionGroupNode {
    return {
        kind: "group",
        negate: false,
        mode: "all",
        count: 0,
        children: [],
    };
}

function editableRoot(condition?: StrategyCondition): ConditionGroupNode {
    return !condition?.type || condition.type === "always"
        ? emptyRoot()
        : parseConditionTree(condition);
}

function newLeaf(type: string): ConditionLeaf {
    return {
        kind: "leaf",
        negate: false,
        cond: defaultLeafCondition(type),
    };
}

function isModifierLeaf(node: ConditionTree): node is ConditionLeaf {
    return node.kind === "leaf" && node.cond.type === "has_mod_family";
}

function isModifierSetGroup(node: ConditionGroupNode): boolean {
    return (
        (node.mode === "all" || node.mode === "at_least") &&
        node.children.length > 0 &&
        node.children.every(
            (child) => isModifierLeaf(child) && !child.negate,
        )
    );
}

function isModifierSetNode(node: ConditionTree): boolean {
    return (
        isModifierLeaf(node) ||
        (node.kind === "group" && isModifierSetGroup(node))
    );
}

function normalizeGroupCounts(node: ConditionTree): void {
    if (node.kind !== "group") return;
    if (node.mode === "at_least") {
        node.count = Math.max(
            1,
            Math.min(node.children.length || 1, node.count || 1),
        );
    } else {
        node.count = node.children.length;
    }
    node.children.forEach(normalizeGroupCounts);
}

function pathKey(path: number[]): string {
    return path.length ? path.join(".") : "root";
}

function titleCase(value: string): string {
    return value.charAt(0).toUpperCase() + value.slice(1);
}

customElements.define("pc-condition-editor", PcConditionEditor);
