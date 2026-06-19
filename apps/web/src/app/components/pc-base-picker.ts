/*
 * pc-base-picker — class / subcategory / base / iLvl selection. Engine-agnostic
 * presentational component. The emulator mounts it as an overlay before a base
 * is chosen; the strategy builder will mount it inline later.
 *
 * Wire-up:
 *   - call setBases(bases) with the engine's ordinary-session BaseInfo list;
 *   - call setSelection(base, itemLevel) to preselect;
 *   - listens for a `confirm` CustomEvent with detail {base, itemLevel}.
 */

import { BaseInfo } from "../engine-protocol";

export interface BasePickerSelection {
    base: string;
    itemLevel: number;
}

const SUBCATEGORY_RULES: Array<{ id: string; pattern: RegExp; label: string }> = [
    { id: "str", pattern: /Body|Helmet|Boots|Gloves|Shield/i, label: "Armour" },
    // overridden below per stem
];

interface ClassEntry {
    key: string;
    label: string;
    bases: BaseInfo[];
}

const ALL_SUB = "__all__";

function classLabel(key: string): string {
    return key.replace(/([a-z])([A-Z])/g, "$1 $2").trim() || key;
}

/* Path stems for armour-class bases encode attribute combos:
 * BodyStr* / BodyDex* / BodyInt* / BodyStrDex* / BodyStrInt* / BodyDexInt* / BodyStrDexInt*
 * Used for the subcategory dropdown on classes that have multiple defence types.
 */
function attributeCombo(metadataPath: string): string | null {
    const stem = metadataPath.split("/").pop() ?? "";
    const m = stem.match(/^[A-Za-z]+?(Str|Dex|Int|StrDex|StrInt|DexInt|StrDexInt)\d+/);
    return m ? m[1] : null;
}

function comboLabel(combo: string): string {
    switch (combo) {
    case "Str": return "Armour";
    case "Dex": return "Evasion";
    case "Int": return "Energy Shield";
    case "StrDex": return "Armour / Evasion";
    case "StrInt": return "Armour / Energy Shield";
    case "DexInt": return "Evasion / Energy Shield";
    case "StrDexInt": return "Armour / Evasion / Energy Shield";
    default: return combo;
    }
}

export class PcBasePicker extends HTMLElement {
    private bases: BaseInfo[] = [];
    private classes: ClassEntry[] = [];
    private selectedClass = "";
    private selectedSub = ALL_SUB;
    private selectedBase = "";
    private itemLevel = 86;

    connectedCallback(): void {
        this.renderShell();
    }

    setBases(bases: BaseInfo[]): void {
        this.bases = bases.filter((b) => b.support === 0 && b.name);
        const byClass = new Map<string, ClassEntry>();
        for (const base of this.bases) {
            const key = base.item_class_key || "Other";
            let entry = byClass.get(key);
            if (!entry) {
                entry = { key, label: classLabel(key), bases: [] };
                byClass.set(key, entry);
            }
            entry.bases.push(base);
        }
        for (const entry of byClass.values()) {
            entry.bases.sort((a, b) => a.name.localeCompare(b.name));
        }
        this.classes = Array.from(byClass.values()).sort((a, b) =>
            a.label.localeCompare(b.label),
        );
        if (!this.classes.some((c) => c.key === this.selectedClass)) {
            this.selectedClass = "";
        }
        this.renderShell();
    }

    setSelection(base: string, itemLevel: number): void {
        this.itemLevel = itemLevel;
        this.selectedBase = base;
        const match = this.bases.find((b) => b.path === base);
        if (match) {
            this.selectedClass = match.item_class_key || "Other";
        }
        this.renderShell();
    }

    private subcategoriesForClass(key: string): string[] {
        const entry = this.classes.find((c) => c.key === key);
        if (!entry) return [];
        const combos = new Set<string>();
        for (const base of entry.bases) {
            const combo = attributeCombo(base.path);
            if (combo) combos.add(combo);
        }
        return Array.from(combos).sort();
    }

    private filteredBases(): BaseInfo[] {
        const entry = this.classes.find((c) => c.key === this.selectedClass);
        if (!entry) return [];
        if (this.selectedSub === ALL_SUB) return entry.bases;
        return entry.bases.filter((b) => attributeCombo(b.path) === this.selectedSub);
    }

    private renderShell(): void {
        const subs = this.subcategoriesForClass(this.selectedClass);
        const bases = this.filteredBases();
        const canStart = Boolean(this.selectedBase) && this.itemLevel > 0;

        this.innerHTML = `
            <div class="pc-base-picker">
                <h2 class="pc-base-picker-title">Choose a base</h2>
                <p class="pc-base-picker-sub">Pick an item class, then a base item and item level.</p>
                <div class="pc-base-picker-grid">
                    <label class="pc-field">
                        <span>Item Class</span>
                        <select class="pc-bp-class">
                            <option value="">— Select Class —</option>
                            ${this.classes
                                .map(
                                    (c) =>
                                        `<option value="${c.key}" ${
                                            c.key === this.selectedClass
                                                ? "selected"
                                                : ""
                                        }>${escapeHtml(c.label)}</option>`,
                                )
                                .join("")}
                        </select>
                    </label>
                    ${subs.length > 1 ? this.renderSubField(subs) : ""}
                    <label class="pc-field">
                        <span>Base</span>
                        <select class="pc-bp-base" ${this.selectedClass ? "" : "disabled"}>
                            <option value="">— Select Base —</option>
                            ${bases
                                .map(
                                    (b) =>
                                        `<option value="${b.path}" ${
                                            b.path === this.selectedBase ? "selected" : ""
                                        }>${escapeHtml(b.name)}</option>`,
                                )
                                .join("")}
                        </select>
                    </label>
                    <label class="pc-field">
                        <span>Item Level</span>
                        <input type="number" class="pc-bp-ilvl" min="1" max="100" value="${this.itemLevel}" />
                    </label>
                </div>
                <div class="pc-base-picker-actions">
                    <button type="button" class="pc-bp-cancel">Cancel</button>
                    <button type="button" class="pc-bp-confirm" ${canStart ? "" : "disabled"}>
                        Start crafting
                    </button>
                </div>
            </div>`;

        this.querySelector<HTMLSelectElement>(".pc-bp-class")!.addEventListener(
            "change",
            (event) => {
                this.selectedClass = (event.target as HTMLSelectElement).value;
                this.selectedSub = ALL_SUB;
                this.selectedBase = "";
                this.renderShell();
            },
        );
        this.querySelector<HTMLSelectElement>(".pc-bp-sub")?.addEventListener(
            "change",
            (event) => {
                this.selectedSub = (event.target as HTMLSelectElement).value;
                this.selectedBase = "";
                this.renderShell();
            },
        );
        this.querySelector<HTMLSelectElement>(".pc-bp-base")!.addEventListener(
            "change",
            (event) => {
                this.selectedBase = (event.target as HTMLSelectElement).value;
                this.renderShell();
            },
        );
        this.querySelector<HTMLInputElement>(".pc-bp-ilvl")!.addEventListener(
            "input",
            (event) => {
                const value = Number((event.target as HTMLInputElement).value);
                if (Number.isFinite(value) && value > 0) {
                    this.itemLevel = value;
                }
            },
        );
        this.querySelector<HTMLButtonElement>(".pc-bp-confirm")!.addEventListener(
            "click",
            () => {
                if (!this.selectedBase) return;
                this.dispatchEvent(
                    new CustomEvent<BasePickerSelection>("confirm", {
                        detail: { base: this.selectedBase, itemLevel: this.itemLevel },
                        bubbles: true,
                    }),
                );
            },
        );
        this.querySelector<HTMLButtonElement>(".pc-bp-cancel")!.addEventListener(
            "click",
            () => {
                this.dispatchEvent(new CustomEvent("cancel", { bubbles: true }));
            },
        );
    }

    private renderSubField(subs: string[]): string {
        return `
            <label class="pc-field">
                <span>Sub-category</span>
                <select class="pc-bp-sub">
                    <option value="${ALL_SUB}" ${this.selectedSub === ALL_SUB ? "selected" : ""}>All</option>
                    ${subs
                        .map(
                            (s) =>
                                `<option value="${s}" ${
                                    s === this.selectedSub ? "selected" : ""
                                }>${escapeHtml(comboLabel(s))}</option>`,
                        )
                        .join("")}
                </select>
            </label>`;
    }
}

function escapeHtml(text: string): string {
    return text
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;");
}

customElements.define("pc-base-picker", PcBasePicker);
