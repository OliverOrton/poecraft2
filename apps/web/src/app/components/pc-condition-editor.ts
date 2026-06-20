/*
 * Edge condition editor. Conditions on one edge are ANDed. "Has modifier"
 * owns a readable multi-select and can require every selected modifier or N of
 * them; OR routing belongs on a separate graph edge.
 */

import {
    LEAF_CONDITION_TYPES,
    StrategyCondition,
    StrategyEdge,
    defaultLeafCondition,
} from "../strategy-model";
import "./pc-modifier-picker";
import type { PcModifierPicker } from "./pc-modifier-picker";

export interface ModifierTierOption {
    tier: number;
    label: string;
    requiredLevel: number;
}

export interface ModifierFamilyOption {
    value: string;
    label: string;
    side: "prefix" | "suffix";
    sourceLabel: string;
    tags: string[];
    tiers: ModifierTierOption[];
}

interface ModifierSelection {
    key: string;
    label: string;
    minTier: number;
}

interface ModifierEntry {
    kind: "modifiers";
    negate: boolean;
    mode: "all" | "at_least";
    count: number;
    modifiers: ModifierSelection[];
}

interface LeafEntry {
    kind: "leaf";
    negate: boolean;
    condition: StrategyCondition;
}

interface AdvancedEntry {
    kind: "advanced";
    condition: StrategyCondition;
}

type EditorEntry = ModifierEntry | LeafEntry | AdvancedEntry;

const LEAF_LABELS: Record<string, string> = {
    has_mod_family: "Has modifier",
    rarity_is: "Rarity",
    open_prefix_count: "Open prefixes",
    open_suffix_count: "Open suffixes",
    prefix_count_range: "Prefix count",
    suffix_count_range: "Suffix count",
    always: "Always",
};

const RANGE_TYPES = new Set([
    "open_prefix_count",
    "open_suffix_count",
    "prefix_count_range",
    "suffix_count_range",
]);

export class PcConditionEditor extends HTMLElement {
    private edge: StrategyEdge | null = null;
    private entries: EditorEntry[] = [];
    private modifierFamilies: ModifierFamilyOption[] = [];

    connectedCallback(): void {
        this.render();
    }

    setEdge(edge: StrategyEdge | null): void {
        this.edge = edge;
        this.entries = parseEditorEntries(edge?.condition);
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
        this.innerHTML = `
            <section class="pc-condition-editor">
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
                            <span>Taken only when no guarded edge above it matches.</span>
                        </div>`
                        : `
                            <div class="pc-condition-heading">
                                <div>
                                    <strong>Conditions</strong>
                                    <span>Every condition below must match. Use another edge for OR.</span>
                                </div>
                            </div>
                            <div class="pc-cond-builder">
                                ${
                                    this.entries.length
                                        ? this.entries
                                              .map((entry, index) =>
                                                  this.renderEntry(entry, index),
                                              )
                                              .join("")
                                        : `<div class="pc-condition-empty is-compact">
                                            <span>No conditions. This edge always matches.</span>
                                        </div>`
                                }
                                <div class="pc-cond-add">
                                    <button data-action="add-condition">+ Condition</button>
                                </div>
                            </div>
                            <details class="pc-condition-json">
                                <summary>Advanced JSON</summary>
                                <textarea data-action="json" spellcheck="false">${escapeHtml(
                                    JSON.stringify(this.currentCondition(), null, 2),
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

    private renderEntry(entry: EditorEntry, index: number): string {
        if (entry.kind === "advanced") {
            return `
                <div class="pc-cond-leaf pc-cond-advanced" data-entry="${index}">
                    <div class="pc-cond-leaf-head">
                        <strong>Advanced condition</strong>
                        <button data-action="remove" class="pc-cond-remove" title="Remove condition">×</button>
                    </div>
                    <code>${escapeHtml(entry.condition.type)}</code>
                    <span>Edit this expression through Advanced JSON.</span>
                </div>`;
        }
        const type =
            entry.kind === "modifiers" ? "has_mod_family" : entry.condition.type;
        return `
            <div class="pc-cond-leaf ${entry.kind === "modifiers" ? "is-modifier-set" : ""}"
                data-entry="${index}">
                <div class="pc-cond-leaf-head">
                    <select data-action="condition-type" aria-label="Condition type">
                        ${LEAF_CONDITION_TYPES.map(
                            (value) =>
                                `<option value="${value}" ${value === type ? "selected" : ""}>
                                    ${LEAF_LABELS[value]}
                                </option>`,
                        ).join("")}
                    </select>
                    <button data-action="negate" class="pc-cond-not ${entry.negate ? "is-active" : ""}"
                        title="Negate this condition">NOT</button>
                    <button data-action="remove" class="pc-cond-remove"
                        title="Remove condition">×</button>
                </div>
                <div class="pc-cond-leaf-body">
                    ${
                        entry.kind === "modifiers"
                            ? this.renderModifierEntry(entry)
                            : this.renderLeafBody(entry.condition)
                    }
                </div>
            </div>`;
    }

    private renderModifierEntry(entry: ModifierEntry): string {
        const count = entry.modifiers.length;
        return `
            <div class="pc-cond-modifier-logic">
                <span>Require</span>
                <div class="pc-cond-logic" role="group" aria-label="Modifier matching">
                    <button data-action="modifier-mode" data-mode="all"
                        class="${entry.mode === "all" ? "is-active" : ""}">ALL</button>
                    <button data-action="modifier-mode" data-mode="at_least"
                        class="${entry.mode === "at_least" ? "is-active" : ""}">N OF</button>
                </div>
                ${
                    entry.mode === "at_least"
                        ? `<label class="pc-cond-count-wrap">
                            <input type="number" data-action="modifier-count" min="1"
                                max="${Math.max(1, count)}" value="${Math.min(
                                  Math.max(1, entry.count),
                                  Math.max(1, count),
                              )}">
                            <span>of ${count}</span>
                        </label>`
                        : `<span class="pc-cond-modifier-count">${count} selected</span>`
                }
            </div>
            <div class="pc-cond-selected-modifiers">
                ${
                    count
                        ? entry.modifiers
                              .map((modifier, modifierIndex) =>
                                  this.renderSelectedModifier(
                                      modifier,
                                      modifierIndex,
                                  ),
                              )
                              .join("")
                        : '<p class="pc-empty">Choose one or more modifiers.</p>'
                }
            </div>
            <pc-modifier-picker></pc-modifier-picker>`;
    }

    private renderSelectedModifier(
        modifier: ModifierSelection,
        index: number,
    ): string {
        const family = this.modifierFamilies.find(
            (option) => option.value === modifier.key,
        );
        const label = family?.label || modifier.label || modifier.key;
        const tiers = family?.tiers ?? [];
        const selectedTier = Math.max(1, modifier.minTier || 1);
        return `
            <div class="pc-cond-selected-modifier" data-modifier-index="${index}">
                <div class="pc-cond-selected-modifier-copy">
                    <strong>${escapeHtml(label)}</strong>
                    <span>
                        ${family?.side === "prefix" ? "Prefix" : family?.side === "suffix" ? "Suffix" : ""}
                        ${family?.sourceLabel ? ` · ${escapeHtml(family.sourceLabel)}` : ""}
                    </span>
                </div>
                <label>
                    <span>Minimum tier</span>
                    <select data-action="modifier-tier">
                        ${tiers
                            .map(
                                (tier) =>
                                    `<option value="${tier.tier}" ${
                                        tier.tier === selectedTier ? "selected" : ""
                                    } title="${escapeAttribute(tier.label)}">T${tier.tier}+</option>`,
                            )
                            .join("")}
                    </select>
                </label>
                <button data-action="remove-modifier" title="Remove modifier">×</button>
            </div>`;
    }

    private renderLeafBody(condition: StrategyCondition): string {
        if (condition.type === "rarity_is") {
            return `<select data-action="rarity" aria-label="Required rarity">
                ${["normal", "magic", "rare"]
                    .map(
                        (rarity) =>
                            `<option value="${rarity}" ${condition.rarity === rarity ? "selected" : ""}>
                                ${titleCase(rarity)}
                            </option>`,
                    )
                    .join("")}
            </select>`;
        }
        if (RANGE_TYPES.has(condition.type)) {
            const min = condition.min ?? condition.value ?? condition.count ?? 0;
            const max = condition.max ?? min;
            return `<div class="pc-cond-range">
                <label><span>Minimum</span>
                    <input type="number" min="0" max="6" data-action="min" value="${min}">
                </label>
                <span class="pc-cond-range-separator">to</span>
                <label><span>Maximum</span>
                    <input type="number" min="0" max="6" data-action="max" value="${max}">
                </label>
            </div>`;
        }
        return `<span class="pc-help">This condition always passes.</span>`;
    }

    private bind(): void {
        this.querySelector('[data-action="guarded"]')?.addEventListener(
            "click",
            () => {
                if (!this.edge) return;
                this.edge.is_default = false;
                this.entries = parseEditorEntries(this.edge.condition);
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
        this.querySelector('[data-action="add-condition"]')?.addEventListener(
            "click",
            () => {
                this.entries.push(emptyModifierEntry());
                this.commit();
            },
        );
        this.querySelector('[data-action="apply-json"]')?.addEventListener(
            "click",
            () => this.applyJson(),
        );

        this.querySelectorAll<HTMLElement>("[data-entry]").forEach((host) => {
            const index = Number(host.dataset.entry);
            const entry = this.entries[index];
            if (!entry) return;
            host.querySelector('[data-action="remove"]')?.addEventListener(
                "click",
                () => {
                    this.entries.splice(index, 1);
                    this.commit();
                },
            );
            if (entry.kind === "advanced") return;
            host.querySelector('[data-action="negate"]')?.addEventListener(
                "click",
                () => {
                    entry.negate = !entry.negate;
                    this.commit();
                },
            );
            host.querySelector<HTMLSelectElement>(
                '[data-action="condition-type"]',
            )?.addEventListener("change", (event) => {
                const type = (event.currentTarget as HTMLSelectElement).value;
                this.entries[index] =
                    type === "has_mod_family"
                        ? emptyModifierEntry()
                        : {
                              kind: "leaf",
                              negate: entry.negate,
                              condition: defaultLeafCondition(type),
                          };
                this.commit();
            });
            if (entry.kind === "modifiers") {
                this.bindModifierEntry(host, entry);
            } else {
                this.bindLeafEntry(host, entry);
            }
        });
    }

    private bindModifierEntry(host: HTMLElement, entry: ModifierEntry): void {
        host.querySelectorAll<HTMLButtonElement>(
            '[data-action="modifier-mode"]',
        ).forEach((button) => {
            button.addEventListener("click", () => {
                entry.mode = button.dataset.mode as "all" | "at_least";
                if (entry.mode === "at_least") {
                    entry.count = Math.min(
                        Math.max(1, entry.count || 1),
                        Math.max(1, entry.modifiers.length),
                    );
                }
                this.commit();
            });
        });
        host.querySelector<HTMLInputElement>(
            '[data-action="modifier-count"]',
        )?.addEventListener("change", (event) => {
            entry.count = Math.min(
                Math.max(
                    1,
                    Number((event.currentTarget as HTMLInputElement).value) || 1,
                ),
                Math.max(1, entry.modifiers.length),
            );
            this.commit();
        });
        host.querySelectorAll<HTMLElement>("[data-modifier-index]").forEach(
            (row) => {
                const index = Number(row.dataset.modifierIndex);
                row.querySelector<HTMLSelectElement>(
                    '[data-action="modifier-tier"]',
                )?.addEventListener("change", (event) => {
                    entry.modifiers[index].minTier = Math.max(
                        1,
                        Number((event.currentTarget as HTMLSelectElement).value) ||
                            1,
                    );
                    this.commit();
                });
                row.querySelector('[data-action="remove-modifier"]')?.addEventListener(
                    "click",
                    () => {
                        entry.modifiers.splice(index, 1);
                        entry.count = Math.min(
                            Math.max(1, entry.count),
                            Math.max(1, entry.modifiers.length),
                        );
                        this.commit();
                    },
                );
            },
        );
        const picker = host.querySelector<PcModifierPicker>("pc-modifier-picker");
        picker?.setOptions(
            this.modifierFamilies,
            entry.modifiers.map((modifier) => modifier.key),
        );
        picker?.addEventListener("modifier-select", (event) => {
            const key = (event as CustomEvent<{ value: string }>).detail.value;
            const family = this.modifierFamilies.find(
                (option) => option.value === key,
            );
            if (!family || entry.modifiers.some((modifier) => modifier.key === key)) {
                return;
            }
            entry.modifiers.push({
                key,
                label: family.label,
                minTier: 1,
            });
            if (entry.mode === "all") entry.count = entry.modifiers.length;
            this.commit();
        });
    }

    private bindLeafEntry(host: HTMLElement, entry: LeafEntry): void {
        host.querySelector<HTMLSelectElement>('[data-action="rarity"]')
            ?.addEventListener("change", (event) => {
                entry.condition.rarity = (
                    event.currentTarget as HTMLSelectElement
                ).value;
                this.commit();
            });
        for (const action of ["min", "max"] as const) {
            host.querySelector<HTMLInputElement>(`[data-action="${action}"]`)
                ?.addEventListener("change", (event) => {
                    entry.condition[action] = Number(
                        (event.currentTarget as HTMLInputElement).value,
                    );
                    this.commit();
                });
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
            this.entries = parseEditorEntries(parsed);
            this.commit();
        } catch (reason) {
            error.textContent =
                reason instanceof Error ? reason.message : String(reason);
        }
    }

    private currentCondition(): StrategyCondition {
        if (this.edge?.is_default) return { type: "always" };
        const conditions = this.entries.map(compileEditorEntry);
        if (!conditions.length) return { type: "always" };
        if (conditions.length === 1) return conditions[0];
        return { type: "all", conditions };
    }

    private commit(): void {
        if (!this.edge) return;
        const condition = this.currentCondition();
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
}

function emptyModifierEntry(): ModifierEntry {
    return {
        kind: "modifiers",
        negate: false,
        mode: "all",
        count: 1,
        modifiers: [],
    };
}

function childConditions(condition: StrategyCondition): StrategyCondition[] {
    return condition.conditions ?? condition.children ?? [];
}

function unwrapNot(condition: StrategyCondition): {
    condition: StrategyCondition;
    negate: boolean;
} {
    if (condition.type !== "not") return { condition, negate: false };
    return {
        condition: childConditions(condition)[0] ?? { type: "always" },
        negate: true,
    };
}

function modifierSelection(condition: StrategyCondition): ModifierSelection {
    return {
        key: condition.family_mod_key ?? "",
        label: condition.family_label ?? "",
        minTier: Math.max(1, Number(condition.min_tier) || 1),
    };
}

function modifierGroup(
    condition: StrategyCondition,
    negate: boolean,
): ModifierEntry | null {
    if (!["all", "all_of", "at_least"].includes(condition.type)) return null;
    const children = childConditions(condition);
    if (
        !children.length ||
        children.some((child) => child.type !== "has_mod_family")
    ) {
        return null;
    }
    return {
        kind: "modifiers",
        negate,
        mode: condition.type === "at_least" ? "at_least" : "all",
        count:
            condition.type === "at_least"
                ? Math.min(
                      Math.max(1, Number(condition.count) || 1),
                      children.length,
                  )
                : children.length,
        modifiers: children.map(modifierSelection),
    };
}

function parseEditorEntries(condition?: StrategyCondition): EditorEntry[] {
    if (!condition?.type || condition.type === "always") return [];
    const top =
        condition.type === "all" || condition.type === "all_of"
            ? childConditions(condition)
            : [condition];
    const entries: EditorEntry[] = [];
    for (const raw of top) {
        const unwrapped = unwrapNot(raw);
        const grouped = modifierGroup(unwrapped.condition, unwrapped.negate);
        if (grouped) {
            entries.push(grouped);
            continue;
        }
        if (unwrapped.condition.type === "has_mod_family") {
            const previous = entries.at(-1);
            if (
                previous?.kind === "modifiers" &&
                previous.mode === "all" &&
                previous.negate === unwrapped.negate
            ) {
                previous.modifiers.push(modifierSelection(unwrapped.condition));
                previous.count = previous.modifiers.length;
            } else {
                entries.push({
                    kind: "modifiers",
                    negate: unwrapped.negate,
                    mode: "all",
                    count: 1,
                    modifiers: [modifierSelection(unwrapped.condition)],
                });
            }
            continue;
        }
        if (
            LEAF_CONDITION_TYPES.includes(
                unwrapped.condition.type as (typeof LEAF_CONDITION_TYPES)[number],
            )
        ) {
            entries.push({
                kind: "leaf",
                negate: unwrapped.negate,
                condition: { ...unwrapped.condition },
            });
        } else {
            entries.push({ kind: "advanced", condition: raw });
        }
    }
    return entries;
}

function compileEditorEntry(entry: EditorEntry): StrategyCondition {
    if (entry.kind === "advanced") return entry.condition;
    let condition: StrategyCondition;
    if (entry.kind === "leaf") {
        condition = { ...entry.condition };
    } else {
        const modifiers = entry.modifiers.length
            ? entry.modifiers.map((modifier) => ({
                  type: "has_mod_family",
                  family_mod_key: modifier.key,
                  family_label: modifier.label,
                  min_tier: Math.max(1, modifier.minTier),
              }))
            : [
                  {
                      type: "has_mod_family",
                      family_mod_key: "",
                      family_label: "",
                      min_tier: 1,
                  },
              ];
        if (modifiers.length === 1) {
            condition = modifiers[0];
        } else if (entry.mode === "at_least") {
            condition = {
                type: "at_least",
                count: Math.min(Math.max(1, entry.count), modifiers.length),
                conditions: modifiers,
            };
        } else {
            condition = { type: "all", conditions: modifiers };
        }
    }
    return entry.negate
        ? { type: "not", conditions: [condition] }
        : condition;
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
