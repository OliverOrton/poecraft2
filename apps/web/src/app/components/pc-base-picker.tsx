import { disconnectReact, renderReact } from "../react-host";
import { GameIcon } from "./pc-game-icon";
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

import { validClusterSelection } from "../cluster-configuration";
import { BaseInfo, ClusterConfiguration } from "../engine-protocol";
import { basePickerAttributeCombo, supportedBasePickerBases } from "../base-picker-model";

export interface BasePickerSelection {
    base: string;
    itemLevel: number;
    cluster?: ClusterConfiguration;
}

interface ClassEntry {
    key: string;
    label: string;
    bases: BaseInfo[];
}

const ALL_SUB = "__all__";

function classLabel(key: string): string {
    return key.replace(/([a-z])([A-Z])/g, "$1 $2").trim() || key;
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
    private cluster?: ClusterConfiguration;

    connectedCallback(): void {
        this.renderShell();
    }

    setBases(bases: BaseInfo[]): void {
        this.bases = supportedBasePickerBases(bases, this.hasAttribute("allow-clusters"));
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
        this.classes = Array.from(byClass.values()).sort((a, b) =>
            a.label.localeCompare(b.label),
        );
        if (!this.classes.some((c) => c.key === this.selectedClass)) {
            this.selectedClass = "";
        }
        this.renderShell();
    }

    setSelection(base: string, itemLevel: number, cluster?: ClusterConfiguration): void {
        this.itemLevel = itemLevel;
        this.cluster = cluster;
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
            const combo = basePickerAttributeCombo(base.path);
            if (combo) combos.add(combo);
        }
        return Array.from(combos).sort();
    }

    private filteredBases(): BaseInfo[] {
        const entry = this.classes.find((c) => c.key === this.selectedClass);
        if (!entry) return [];
        if (this.selectedSub === ALL_SUB) return entry.bases;
        return entry.bases.filter((b) => basePickerAttributeCombo(b.path) === this.selectedSub);
    }


    private renderShell(): void {
        const subs = this.subcategoriesForClass(this.selectedClass);
        const bases = this.filteredBases();
        const clusterCatalog = bases.find(base => base.path === this.selectedBase)?.cluster;
        const canStart = (!clusterCatalog || validClusterSelection(clusterCatalog, this.cluster)) && bases.some(base => base.path === this.selectedBase) &&
            Number.isInteger(this.itemLevel) && this.itemLevel >= 1 && this.itemLevel <= 100;
        const compact = this.hasAttribute("compact");
        renderReact(this, <div className={"pc-base-picker " + (compact ? "is-compact" : "")}>
            {!compact && <h2 className="pc-base-picker-title">{this.getAttribute("picker-title") ?? "Choose a base"}</h2>}
            {this.selectedBase && <div className="pc-base-preview"><GameIcon assetKey={this.selectedBase} size="item" />
                <strong>{this.bases.find(base => base.path === this.selectedBase)?.name}</strong></div>}
            {!compact && this.getAttribute("picker-subtitle") && <p className="pc-base-picker-sub">{this.getAttribute("picker-subtitle")}</p>}
            <div className="pc-base-picker-grid">
                <label className="pc-field"><span>Item Class</span>
                    <select className="pc-bp-class" value={this.selectedClass} onChange={event => {
                        this.selectedClass = event.target.value; this.selectedSub = ALL_SUB; this.selectedBase = ""; this.renderShell();
                    }}>
                        <option value="">— Select Class —</option>
                        {this.classes.map(entry => <option key={entry.key} value={entry.key}>{entry.label}</option>)}
                    </select>
                </label>
                {subs.length > 1 && <label className="pc-field"><span>Sub-category</span>
                    <select className="pc-bp-sub" value={this.selectedSub} onChange={event => {
                        this.selectedSub = event.target.value; this.selectedBase = ""; this.renderShell();
                    }}>
                        <option value={ALL_SUB}>All</option>
                        {subs.map(sub => <option key={sub} value={sub}>{comboLabel(sub)}</option>)}
                    </select>
                </label>}
                <label className="pc-field"><span>Base</span>
                    <select className="pc-bp-base" disabled={!this.selectedClass} value={this.selectedBase} onChange={event => {
                        this.selectedBase = event.target.value; this.cluster = undefined; this.renderShell();
                    }}>
                        <option value="">— Select Base —</option>
                        {bases.map(base => <option key={base.path} value={base.path}>{base.name}</option>)}
                    </select>
                </label>
                {clusterCatalog && <>
                    <label className="pc-field"><span>Small passive type</span>
                        <select value={this.cluster?.passiveKey ?? ""} onChange={event => {
                            this.cluster = {passiveKey: event.target.value, passiveCount: this.cluster?.passiveCount ?? clusterCatalog.minPassiveCount}; this.renderShell();
                        }}><option value="">- Select Passive -</option>
                            {clusterCatalog.passives.map(p => <option key={p.key} value={p.key} disabled={p.tag.startsWith("old_do_not_use_")}>{p.text.join("; ") || p.name}</option>)}
                        </select>
                    </label>
                    <label className="pc-field"><span>Added passive skills</span>
                        <input type="number" min={clusterCatalog.minPassiveCount} max={clusterCatalog.maxPassiveCount} value={this.cluster?.passiveCount ?? clusterCatalog.minPassiveCount}
                            onChange={event => { this.cluster = {passiveKey: this.cluster?.passiveKey ?? "", passiveCount: Number(event.target.value)}; this.renderShell(); }} />
                    </label>
                </>}
                <label className="pc-field"><span>Item Level</span>
                    <input type="number" className="pc-bp-ilvl" min={1} max={100} value={this.itemLevel || ""}
                        onChange={event => { this.itemLevel = Number(event.target.value); this.renderShell(); }} />
                </label>
            </div>
            <div className="pc-base-picker-actions">
                {!compact && <button type="button" className="pc-bp-cancel"
                    onClick={() => this.dispatchEvent(new CustomEvent("cancel", {bubbles: true}))}>Cancel</button>}
                <button type="button" className="pc-bp-confirm" disabled={!canStart} onClick={() => {
                    if (canStart) this.dispatchEvent(new CustomEvent<BasePickerSelection>("confirm", {
                        detail: {base: this.selectedBase, itemLevel: this.itemLevel, cluster: clusterCatalog ? this.cluster : undefined}, bubbles: true,
                    }));
                }}>{this.getAttribute("confirm-label") ?? "Start crafting"}</button>
            </div>
        </div>);
    }
    disconnectedCallback(): void { disconnectReact(this); }
}

customElements.define("pc-base-picker", PcBasePicker);
