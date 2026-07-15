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
import type { ModifierFamilyOption } from "../modifier-options";
import "./pc-modifier-picker";
import type { PcModifierPicker } from "./pc-modifier-picker";

interface ModifierSetMember {
    leaf: ConditionLeaf;
    path: number[];
}

const LEAF_LABELS: Record<string, string> = {
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

export class PcConditionEditor extends HTMLElement {
    private edge: StrategyEdge | null = null;
    private root: ConditionGroupNode = emptyRoot();
    private modifierFamilies: ModifierFamilyOption[] = [];

    connectedCallback(): void {
        this.render();
    }

    setEdge(edge: StrategyEdge | null): void {
        this.edge = edge;
        this.root = editableRoot(edge?.condition);
        this.render();
    }

    setModifierFamilies(options: ModifierFamilyOption[]): void {
        this.modifierFamilies = options;
        this.render();
    }

    private render(): void {
        if (!this.edge) {
            this.innerHTML = "";
            return;
        }

        const isDefault = Boolean(this.edge.is_default);
        const condition = this.currentCondition();
        const summary = isDefault ? "Fallback when no guarded edge matches" : conditionLabel(condition);
        this.innerHTML = `
            <section class="pc-condition-editor">
                <div class="pc-condition-summary ${isDefault ? "is-fallback" : ""}">
                    <span class="pc-condition-summary-dot"></span>
                    <div>
                        <span>${isDefault ? "Default route" : "Live condition"}</span>
                        <strong title="${escapeAttribute(summary)}">${escapeHtml(summary)}</strong>
                    </div>
                </div>
                <div class="pc-edge-kind">
                    <button data-action="guarded" class="${isDefault ? "" : "is-active"}">
                        Guarded edge
                    </button>
                    <button data-action="default" class="${isDefault ? "is-active" : ""}">
                        Default fallback
                    </button>
                </div>
                ${
                    isDefault
                        ? `<div class="pc-condition-empty">
                            <strong>Fallback route</strong>
                            <span>Taken only when no guarded edge above it matches. Switching back restores the condition tree while this editor stays open.</span>
                        </div>`
                        : `
                            <div class="pc-condition-heading">
                                <div>
                                    <strong>Condition builder</strong>
                                    <span>Build nested boolean logic. Edge priority still controls routing order.</span>
                                </div>
                            </div>
                            <div class="pc-cond-builder">
                                ${this.renderGroup(this.root, [], true)}
                            </div>
                            <details class="pc-condition-json">
                                <summary>Advanced JSON</summary>
                                <textarea data-action="json" spellcheck="false">${escapeHtml(
                                    JSON.stringify(condition, null, 2),
                                )}</textarea>
                                <div class="pc-condition-json-actions">
                                    <button data-action="apply-json">Apply JSON</button>
                                    <span class="pc-condition-json-error"></span>
                                </div>
                            </details>`
                }
            </section>`;
        this.bind();
    }

    private renderGroup(
        group: ConditionGroupNode,
        path: number[],
        isRoot: boolean,
        siblingCount = 1,
    ): string {
        if (isModifierSetGroup(group)) {
            return this.renderModifierSet(group, path, isRoot, siblingCount);
        }
        const key = pathKey(path);
        return `
            <div class="pc-cond-group ${isRoot ? "is-root" : ""}" data-node-path="${key}">
                <div class="pc-cond-group-head">
                    <span class="pc-cond-group-title">${isRoot ? "Match" : "Group"}</span>
                    <div class="pc-cond-logic" role="group" aria-label="${isRoot ? "Root condition logic" : "Nested condition logic"}">
                        ${(["all", "any", "at_least"] as ConditionGroupMode[])
                            .map(
                                (mode) => `<button data-action="group-mode" data-path="${key}"
                                    data-mode="${mode}" class="${group.mode === mode ? "is-active" : ""}"
                                    aria-pressed="${group.mode === mode}">
                                    ${mode === "all" ? "ALL" : mode === "any" ? "ANY" : "N OF"}
                                </button>`,
                            )
                            .join("")}
                    </div>
                    ${
                        group.mode === "at_least"
                            ? `<label class="pc-cond-count-wrap">
                                <input type="number" data-action="group-count" data-path="${key}"
                                    min="1" max="${Math.max(1, group.children.length)}"
                                    value="${Math.max(1, Math.min(group.children.length || 1, group.count || 1))}">
                                <span>of ${group.children.length}</span>
                            </label>`
                            : ""
                    }
                    ${
                        isRoot
                            ? ""
                            : `<button data-action="toggle-not" data-path="${key}"
                                    class="pc-cond-not ${group.negate ? "is-active" : ""}"
                                    aria-pressed="${group.negate}">NOT</button>
                                ${this.renderNodeActions(path, siblingCount)}`
                    }
                </div>
                <div class="pc-cond-children">
                    ${
                        group.children.length
                            ? group.children
                                  .map((child, index) =>
                                      this.renderNode(
                                          child,
                                          [...path, index],
                                          group.children.length,
                                      ),
                                  )
                                  .join("")
                            : `<div class="pc-condition-empty is-compact">
                                <span>No rules yet. An empty group always passes.</span>
                            </div>`
                    }
                </div>
                <div class="pc-cond-add">
                    <button data-action="add-leaf" data-path="${key}">+ Condition</button>
                    <button data-action="add-group" data-path="${key}">+ Group</button>
                </div>
            </div>`;
    }

    private renderNode(
        node: ConditionTree,
        path: number[],
        siblingCount: number,
    ): string {
        return node.kind === "group"
            ? this.renderGroup(node, path, false, siblingCount)
            : this.renderLeaf(node, path, siblingCount);
    }

    private renderLeaf(
        leaf: ConditionLeaf,
        path: number[],
        siblingCount: number,
    ): string {
        if (leaf.cond.type === "has_mod_family") {
            return this.renderModifierSet(leaf, path, false, siblingCount);
        }
        const key = pathKey(path);
        const knownType = LEAF_CONDITION_TYPES.includes(
            leaf.cond.type as (typeof LEAF_CONDITION_TYPES)[number],
        );
        return `
            <div class="pc-cond-leaf" data-node-path="${key}">
                <div class="pc-cond-leaf-head">
                    <select data-action="condition-type" data-path="${key}" aria-label="Condition type">
                        ${
                            knownType
                                ? ""
                                : `<option value="${escapeAttribute(leaf.cond.type)}" selected>
                                    Advanced: ${escapeHtml(leaf.cond.type)}
                                </option>`
                        }
                        ${LEAF_CONDITION_TYPES.map(
                            (value) =>
                                `<option value="${value}" ${leaf.cond.type === value ? "selected" : ""}>
                                    ${LEAF_LABELS[value]}
                                </option>`,
                        ).join("")}
                    </select>
                    <button data-action="toggle-not" data-path="${key}"
                        class="pc-cond-not ${leaf.negate ? "is-active" : ""}"
                        aria-pressed="${leaf.negate}">NOT</button>
                    ${this.renderNodeActions(path, siblingCount)}
                </div>
                <div class="pc-cond-leaf-body">
                    ${this.renderLeafBody(leaf.cond, path)}
                </div>
            </div>`;
    }

    private renderNodeActions(path: number[], siblingCount: number): string {
        const key = pathKey(path);
        const index = path.at(-1) ?? 0;
        return `<div class="pc-cond-node-actions">
            <button class="pc-cond-icon" data-action="move-up" data-path="${key}"
                title="Move up" ${index === 0 ? "disabled" : ""}>&uarr;</button>
            <button class="pc-cond-icon" data-action="move-down" data-path="${key}"
                title="Move down" ${index >= siblingCount - 1 ? "disabled" : ""}>&darr;</button>
            <button class="pc-cond-icon" data-action="duplicate-node" data-path="${key}"
                title="Duplicate">&#x2398;</button>
            <button class="pc-cond-remove" data-action="remove-node" data-path="${key}"
                title="Remove">&times;</button>
        </div>`;
    }

    private renderLeafBody(
        condition: StrategyCondition,
        path: number[],
    ): string {
        const key = pathKey(path);
        if (condition.type === "rarity_is") {
            return `<label class="pc-cond-inline-field">
                <span>Required rarity</span>
                <select data-action="rarity" data-path="${key}">
                    ${["normal", "magic", "rare"]
                        .map(
                            (rarity) =>
                                `<option value="${rarity}" ${condition.rarity === rarity ? "selected" : ""}>
                                    ${titleCase(rarity)}
                                </option>`,
                        )
                        .join("")}
                </select>
            </label>`;
        }
        if (condition.type === "item_flag") {
            return `<label class="pc-cond-inline-field">
                <span>Required flag</span>
                <select data-action="item-flag" data-path="${key}">
                    ${ITEM_FLAGS.map(
                        (flag) =>
                            `<option value="${flag}" ${condition.flag === flag ? "selected" : ""}>
                                ${titleCase(flag.replaceAll("_", " "))}
                            </option>`,
                    ).join("")}
                </select>
            </label>`;
        }
        if (condition.type === "eldritch_tier") {
            const min = condition.min ?? 1;
            const max = condition.max ?? 4;
            return `<div class="pc-cond-range">
                <label><span>Influence</span>
                    <select data-action="eldritch-side" data-path="${key}">
                        <option value="searing" ${condition.side === "searing" ? "selected" : ""}>Searing Exarch</option>
                        <option value="eater" ${condition.side === "eater" ? "selected" : ""}>Eater of Worlds</option>
                    </select>
                </label>
                <label><span>Minimum tier</span>
                    <input type="number" min="0" max="4" data-action="min"
                        data-path="${key}" value="${min}">
                </label>
                <span class="pc-cond-range-separator">to</span>
                <label><span>Maximum tier</span>
                    <input type="number" min="0" max="4" data-action="max"
                        data-path="${key}" value="${max}">
                </label>
            </div>`;
        }
        if (RANGE_TYPES.has(condition.type)) {
            const min = condition.min ?? condition.value ?? condition.count ?? 0;
            const max = condition.max ?? min;
            return `<div class="pc-cond-range">
                <label><span>Minimum</span>
                    <input type="number" min="0" max="6" data-action="min"
                        data-path="${key}" value="${min}">
                </label>
                <span class="pc-cond-range-separator">to</span>
                <label><span>Maximum</span>
                    <input type="number" min="0" max="6" data-action="max"
                        data-path="${key}" value="${max}">
                </label>
            </div>`;
        }
        if (condition.type === "always") {
            return `<span class="pc-help">This rule always passes.</span>`;
        }
        return `<div class="pc-cond-advanced">
            <code>${escapeHtml(condition.type)}</code>
            <span>This condition is preserved. Edit its payload in Advanced JSON or choose a supported visual type above.</span>
        </div>`;
    }

    private renderModifierSet(
        node: ConditionLeaf | ConditionGroupNode,
        path: number[],
        isRoot: boolean,
        siblingCount: number,
    ): string {
        const key = pathKey(path);
        const members: ModifierSetMember[] =
            node.kind === "leaf"
                ? [{ leaf: node, path }]
                : node.children.map((child, index) => ({
                      leaf: child as ConditionLeaf,
                      path: [...path, index],
                  }));
        const mode = node.kind === "group" ? node.mode : "all";
        const count =
            mode === "at_least" && node.kind === "group"
                ? Math.max(1, Math.min(members.length, node.count || 1))
                : members.length;
        return `
            <div class="pc-cond-leaf pc-cond-modifier-set ${isRoot ? "is-root" : ""}"
                data-node-path="${key}">
                <div class="pc-cond-leaf-head">
                    <select data-action="condition-type" data-path="${key}" aria-label="Condition type">
                        ${LEAF_CONDITION_TYPES.map(
                            (value) =>
                                `<option value="${value}" ${value === "has_mod_family" ? "selected" : ""}>
                                    ${LEAF_LABELS[value]}
                                </option>`,
                        ).join("")}
                    </select>
                    ${
                        isRoot
                            ? ""
                            : `<button data-action="toggle-not" data-path="${key}"
                                class="pc-cond-not ${node.negate ? "is-active" : ""}"
                                aria-pressed="${node.negate}">NOT</button>
                                ${this.renderNodeActions(path, siblingCount)}`
                    }
                </div>
                <div class="pc-cond-leaf-body">
                    <div class="pc-cond-modifier-logic">
                        <span>Require</span>
                        <div class="pc-cond-logic" role="group" aria-label="Modifier matching">
                            <button data-action="modifier-set-mode" data-path="${key}" data-mode="all"
                                class="${mode === "all" ? "is-active" : ""}"
                                aria-pressed="${mode === "all"}">ALL</button>
                            <button data-action="modifier-set-mode" data-path="${key}" data-mode="at_least"
                                class="${mode === "at_least" ? "is-active" : ""}"
                                aria-pressed="${mode === "at_least"}">N OF</button>
                        </div>
                        ${
                            mode === "at_least"
                                ? `<label class="pc-cond-count-wrap">
                                    <input type="number" data-action="modifier-set-count" data-path="${key}"
                                        min="1" max="${Math.max(1, members.length)}" value="${count}">
                                    <span>of ${members.length}</span>
                                </label>`
                                : `<span class="pc-cond-modifier-count">${members.filter(({ leaf }) => leaf.cond.family_mod_key).length} selected</span>`
                        }
                    </div>
                    <div class="pc-cond-selected-modifiers">
                        ${members
                            .filter(({ leaf }) => Boolean(leaf.cond.family_mod_key))
                            .map(({ leaf, path: memberPath }) =>
                                this.renderSelectedModifier(leaf, memberPath),
                            )
                            .join("")}
                        ${
                            members.some(({ leaf }) => !leaf.cond.family_mod_key)
                                ? `<div class="pc-condition-empty is-compact">
                                    <span>Choose one or more modifier families.</span>
                                </div>`
                                : ""
                        }
                    </div>
                    <pc-modifier-picker data-modifier-set-path="${key}"></pc-modifier-picker>
                </div>
                ${
                    isRoot
                        ? `<div class="pc-cond-add">
                            <button data-action="add-root-condition">+ Condition</button>
                            <button data-action="add-root-group">+ Group</button>
                        </div>`
                        : ""
                }
            </div>`;
    }

    private renderSelectedModifier(
        leaf: ConditionLeaf,
        path: number[],
    ): string {
        const condition = leaf.cond;
        const key = pathKey(path);
        const family = this.modifierFamilies.find(
            (option) => option.value === condition.family_mod_key,
        );
        const label =
            family?.label || condition.family_label || condition.family_mod_key || "";
        const selectedTier = Math.max(1, Number(condition.min_tier) || 1);
        const tiers = family?.tiers ?? [];
        return `<div class="pc-cond-selected-modifier ${condition.fractured ? "is-fractured" : ""}"
            title="${escapeAttribute(label)}">
            <div class="pc-cond-selected-modifier-copy">
                <strong>${escapeHtml(label)}</strong>
                <span>
                    ${family?.side === "prefix" ? "Prefix" : family?.side === "suffix" ? "Suffix" : "Modifier"}
                    ${family?.sourceLabel ? ` &middot; ${escapeHtml(family.sourceLabel)}` : ""}
                </span>
            </div>
            <label>
                <span>Minimum tier</span>
                <select data-action="modifier-tier" data-path="${key}">
                    ${
                        tiers.length
                            ? tiers
                                  .map(
                                      (tier) =>
                                          `<option value="${tier.tier}" ${tier.tier === selectedTier ? "selected" : ""}
                                              title="${escapeAttribute(tier.label)}">T${tier.tier}+</option>`,
                                  )
                                  .join("")
                            : `<option value="${selectedTier}">T${selectedTier}+</option>`
                    }
                </select>
            </label>
            <label class="pc-cond-fractured-toggle">
                <input type="checkbox" data-action="modifier-fractured"
                    data-path="${key}" ${condition.fractured ? "checked" : ""}>
                <span>Fractured</span>
            </label>
            <button data-action="remove-modifier-member" data-path="${key}"
                title="Remove modifier">&times;</button>
        </div>`;
    }

    private bind(): void {
        this.querySelector('[data-action="guarded"]')?.addEventListener(
            "click",
            () => {
                if (!this.edge) return;
                this.edge.is_default = false;
                this.commit();
            },
        );
        this.querySelector('[data-action="default"]')?.addEventListener(
            "click",
            () => {
                if (!this.edge) return;
                this.edge.is_default = true;
                this.commit();
            },
        );
        this.querySelector('[data-action="apply-json"]')?.addEventListener(
            "click",
            () => this.applyJson(),
        );

        this.querySelectorAll<HTMLButtonElement>(
            '[data-action="group-mode"]',
        ).forEach((button) => {
            button.addEventListener("click", () => {
                const group = this.groupAt(parsePath(button.dataset.path));
                if (!group) return;
                group.mode = button.dataset.mode as ConditionGroupMode;
                if (group.mode === "at_least") {
                    group.count = Math.max(
                        1,
                        Math.min(group.children.length || 1, group.count || 1),
                    );
                }
                this.commit();
            });
        });
        this.querySelectorAll<HTMLInputElement>(
            '[data-action="group-count"]',
        ).forEach((input) => {
            input.addEventListener("change", () => {
                const group = this.groupAt(parsePath(input.dataset.path));
                if (!group) return;
                group.count = Math.max(
                    1,
                    Math.min(group.children.length || 1, Number(input.value) || 1),
                );
                this.commit();
            });
        });
        this.querySelectorAll<HTMLButtonElement>(
            '[data-action="modifier-set-mode"]',
        ).forEach((button) => {
            button.addEventListener("click", () => {
                const path = parsePath(button.dataset.path);
                const node = this.nodeAt(path);
                if (!node || !isModifierSetNode(node)) return;
                const mode = button.dataset.mode as "all" | "at_least";
                if (node.kind === "group") {
                    node.mode = mode;
                    node.count =
                        mode === "all"
                            ? node.children.length
                            : Math.max(
                                  1,
                                  Math.min(
                                      node.children.length,
                                      node.count || 1,
                                  ),
                              );
                } else {
                    const member = structuredClone(node);
                    member.negate = false;
                    this.replaceNode(path, {
                        kind: "group",
                        negate: node.negate,
                        mode,
                        count: 1,
                        children: [member],
                    });
                }
                this.commit();
            });
        });
        this.querySelectorAll<HTMLInputElement>(
            '[data-action="modifier-set-count"]',
        ).forEach((input) => {
            input.addEventListener("change", () => {
                const node = this.groupAt(parsePath(input.dataset.path));
                if (!node || !isModifierSetGroup(node)) return;
                node.count = Math.max(
                    1,
                    Math.min(node.children.length, Number(input.value) || 1),
                );
                this.commit();
            });
        });
        this.querySelectorAll<HTMLButtonElement>(
            '[data-action="add-leaf"]',
        ).forEach((button) => {
            button.addEventListener("click", () => {
                const group = this.groupAt(parsePath(button.dataset.path));
                if (!group) return;
                group.children.push(newLeaf("rarity_is"));
                this.commit();
            });
        });
        this.querySelectorAll<HTMLButtonElement>(
            '[data-action="add-group"]',
        ).forEach((button) => {
            button.addEventListener("click", () => {
                const group = this.groupAt(parsePath(button.dataset.path));
                if (!group) return;
                group.children.push({
                    kind: "group",
                    negate: false,
                    mode: "all",
                    count: 1,
                    children: [newLeaf("rarity_is")],
                });
                this.commit();
            });
        });
        this.querySelector('[data-action="add-root-condition"]')?.addEventListener(
            "click",
            () => this.wrapRootModifierSet(newLeaf("rarity_is")),
        );
        this.querySelector('[data-action="add-root-group"]')?.addEventListener(
            "click",
            () =>
                this.wrapRootModifierSet({
                    kind: "group",
                    negate: false,
                    mode: "all",
                    count: 1,
                    children: [newLeaf("rarity_is")],
                }),
        );

        this.querySelectorAll<HTMLSelectElement>(
            '[data-action="condition-type"]',
        ).forEach((select) => {
            select.addEventListener("change", () => {
                const path = parsePath(select.dataset.path);
                const node = this.nodeAt(path);
                if (!node) return;
                const replacement = newLeaf(select.value);
                replacement.negate = node.negate;
                this.replaceNode(path, replacement);
                this.commit();
            });
        });
        this.querySelectorAll<HTMLButtonElement>(
            '[data-action="toggle-not"]',
        ).forEach((button) => {
            button.addEventListener("click", () => {
                const node = this.nodeAt(parsePath(button.dataset.path));
                if (!node) return;
                node.negate = !node.negate;
                this.commit();
            });
        });
        this.querySelectorAll<HTMLButtonElement>(
            '[data-action="remove-node"]',
        ).forEach((button) => {
            button.addEventListener("click", () => {
                const ref = this.parentRef(parsePath(button.dataset.path));
                if (!ref) return;
                ref.parent.children.splice(ref.index, 1);
                this.commit();
            });
        });
        this.querySelectorAll<HTMLButtonElement>(
            '[data-action="duplicate-node"]',
        ).forEach((button) => {
            button.addEventListener("click", () => {
                const path = parsePath(button.dataset.path);
                const node = this.nodeAt(path);
                const ref = this.parentRef(path);
                if (!node || !ref) return;
                ref.parent.children.splice(ref.index + 1, 0, structuredClone(node));
                this.commit();
            });
        });
        for (const [action, offset] of [
            ["move-up", -1],
            ["move-down", 1],
        ] as const) {
            this.querySelectorAll<HTMLButtonElement>(
                `[data-action="${action}"]`,
            ).forEach((button) => {
                button.addEventListener("click", () => {
                    const ref = this.parentRef(parsePath(button.dataset.path));
                    if (!ref) return;
                    const target = ref.index + offset;
                    if (target < 0 || target >= ref.parent.children.length) return;
                    const [node] = ref.parent.children.splice(ref.index, 1);
                    ref.parent.children.splice(target, 0, node);
                    this.commit();
                });
            });
        }

        this.bindLeafControls();
        this.bindModifierPickers();
    }

    private bindLeafControls(): void {
        this.querySelectorAll<HTMLSelectElement>('[data-action="rarity"]')
            .forEach((select) => {
                select.addEventListener("change", () => {
                    const leaf = this.leafAt(parsePath(select.dataset.path));
                    if (!leaf) return;
                    leaf.cond.rarity = select.value;
                    this.commit();
                });
            });
        this.querySelectorAll<HTMLSelectElement>('[data-action="item-flag"]')
            .forEach((select) => {
                select.addEventListener("change", () => {
                    const leaf = this.leafAt(parsePath(select.dataset.path));
                    if (!leaf) return;
                    leaf.cond.flag = select.value;
                    this.commit();
                });
            });
        this.querySelectorAll<HTMLSelectElement>('[data-action="eldritch-side"]')
            .forEach((select) => {
                select.addEventListener("change", () => {
                    const leaf = this.leafAt(parsePath(select.dataset.path));
                    if (!leaf) return;
                    leaf.cond.side = select.value;
                    this.commit();
                });
            });
        for (const action of ["min", "max"] as const) {
            this.querySelectorAll<HTMLInputElement>(
                `[data-action="${action}"]`,
            ).forEach((input) => {
                input.addEventListener("change", () => {
                    const leaf = this.leafAt(parsePath(input.dataset.path));
                    if (!leaf) return;
                    leaf.cond[action] = Number(input.value);
                    this.commit();
                });
            });
        }
        this.querySelectorAll<HTMLSelectElement>(
            '[data-action="modifier-tier"]',
        ).forEach((select) => {
            select.addEventListener("change", () => {
                const leaf = this.leafAt(parsePath(select.dataset.path));
                if (!leaf) return;
                leaf.cond.min_tier = Math.max(1, Number(select.value) || 1);
                this.commit();
            });
        });
        this.querySelectorAll<HTMLInputElement>(
            '[data-action="modifier-fractured"]',
        ).forEach((input) => {
            input.addEventListener("change", () => {
                const leaf = this.leafAt(parsePath(input.dataset.path));
                if (!leaf) return;
                leaf.cond.fractured = input.checked || undefined;
                this.commit();
            });
        });
        this.querySelectorAll<HTMLButtonElement>(
            '[data-action="remove-modifier-member"]',
        ).forEach((button) => {
            button.addEventListener("click", () => {
                this.removeModifierMember(parsePath(button.dataset.path));
                this.commit();
            });
        });
    }

    private bindModifierPickers(): void {
        this.querySelectorAll<PcModifierPicker>(
            "pc-modifier-picker[data-modifier-set-path]",
        ).forEach((picker) => {
            const path = parsePath(picker.dataset.modifierSetPath);
            const node = this.nodeAt(path);
            if (!node || !isModifierSetNode(node)) return;
            const members = modifierSetLeaves(node);
            const selected = members.flatMap((leaf) =>
                leaf.cond.family_mod_key ? [leaf.cond.family_mod_key] : [],
            );
            picker.setButtonLabel(
                selected.length ? "+ Add modifier" : "Choose modifiers",
            );
            picker.setOptions(this.modifierFamilies, selected);
            picker.addEventListener("modifier-select", (event) => {
                const detail = (
                    event as CustomEvent<{ value: string; fractured?: boolean }>
                ).detail;
                const family = this.modifierFamilies.find(
                    (option) => option.value === detail.value,
                );
                if (!family) return;
                const current = this.nodeAt(path);
                if (!current || !isModifierSetNode(current)) return;
                const member = newLeaf("has_mod_family");
                member.cond = {
                    type: "has_mod_family",
                    family_mod_key: family.value,
                    family_label: family.label,
                    min_tier: 1,
                    fractured: Boolean(detail.fractured) || undefined,
                };
                if (current.kind === "group") {
                    const blank = current.children.find(
                        (child) =>
                            isModifierLeaf(child) &&
                            !child.cond.family_mod_key,
                    ) as ConditionLeaf | undefined;
                    if (blank) {
                        blank.cond = member.cond;
                    } else {
                        current.children.push(member);
                    }
                    current.count =
                        current.mode === "all"
                            ? current.children.length
                            : Math.max(
                                  1,
                                  Math.min(
                                      current.children.length,
                                      current.count || 1,
                                  ),
                              );
                } else if (!current.cond.family_mod_key) {
                    current.cond = member.cond;
                } else {
                    const existing = structuredClone(current);
                    existing.negate = false;
                    this.replaceNode(path, {
                        kind: "group",
                        negate: current.negate,
                        mode: "all",
                        count: 2,
                        children: [existing, member],
                    });
                }
                this.commit();
            });
        });
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
        const textarea = this.querySelector<HTMLTextAreaElement>(
            '[data-action="json"]',
        );
        const error = this.querySelector<HTMLElement>(
            ".pc-condition-json-error",
        );
        if (!textarea || !error) return;
        try {
            const parsed = JSON.parse(textarea.value) as StrategyCondition;
            if (!parsed || typeof parsed.type !== "string") {
                throw new Error("Condition JSON needs a string type.");
            }
            error.textContent = "";
            this.root = editableRoot(parsed);
            this.commit();
        } catch (reason) {
            error.textContent =
                reason instanceof Error ? reason.message : String(reason);
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

function modifierSetLeaves(
    node: ConditionLeaf | ConditionGroupNode,
): ConditionLeaf[] {
    return node.kind === "leaf"
        ? [node]
        : node.children.filter(isModifierLeaf);
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

function parsePath(value?: string): number[] {
    return !value || value === "root"
        ? []
        : value
              .split(".")
              .map(Number)
              .filter((part) => Number.isInteger(part) && part >= 0);
}

function titleCase(value: string): string {
    return value.charAt(0).toUpperCase() + value.slice(1);
}

function escapeHtml(value: string): string {
    return value
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;");
}

function escapeAttribute(value: string): string {
    return escapeHtml(value).replace(/"/g, "&quot;");
}

customElements.define("pc-condition-editor", PcConditionEditor);
