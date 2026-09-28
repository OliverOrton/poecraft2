import type { ReactNode } from "react";
import type { BestiaryActionInfo, Catalog, CatalogEntry } from "../engine-protocol";
import { craftActionLabel, groupEssences, resistanceEntries } from "../craft-choices";
import { HARVEST_AUGMENT, HARVEST_REFORGE, harvestTagsFor } from "../harvest-crafts";
import { disconnectReact, renderReact } from "../react-host";
import { GameIcon } from "./pc-game-icon";

export type CraftPanel = "basic" | "essence" | "harvest" | "fossil" | "eldritch" | "influenced" | "veiled" | "bestiary";
const PANELS: Array<[CraftPanel, string, string]> = [
    ["basic", "Basic currency", "chaos"], ["essence", "Essences", "essence"], ["harvest", "Harvest", "harvest_reforge"],
    ["fossil", "Fossil", "fossil"], ["eldritch", "Eldritch", "eldritch_ember"], ["influenced", "Influenced", "influence_exalt"],
    ["veiled", "Veiled", "veiled_exalt"], ["bestiary", "Bestiary", "bestiary:imprint"],
];
const BASIC = ["transmute", "augment", "alteration", "regal", "alchemy", "chaos", "exalt", "annul", "scour", "remove_crafted_modifiers", "fracture"];

export interface CraftControlsModel {
    mode: "emulator" | "calculator";
    catalog: Catalog;
    panel: CraftPanel;
    values: ReadonlyMap<string, string>;
    fossils: string[];
    bestiary: BestiaryActionInfo[];
    checkpoint?: boolean;
    unveils?: CatalogEntry[];
    selectedAction?: string;
    selectedLabel?: string;
    onPanel: (panel: CraftPanel) => void;
    onValue: (name: string, value: string) => void;
    onSimple: (id: string) => void;
    onConfigured: (type: string) => void;
    onBestiary: (id: string) => void;
    onAddFossil: () => void;
    onRemoveFossil: (index: number) => void;
}

/** Both workbenches share presentation; native actions stay in their controllers. */
export function CraftControls({model: m}: {model: CraftControlsModel}) {
    const calculator = m.mode === "calculator";
    const value = (name: string, fallback = "") => m.values.get(name) ?? fallback;
    const select = (name: string, entries: Array<{key: string; name: string}>, fallback?: string, label?: string, art = false) => {
        const selected = entries.some(entry => entry.key === value(name, fallback)) ? value(name, fallback) : entries[0]?.key ?? "";
        return <label>{label && <span>{label}</span>}<span className="pc-craft-choice">
            {art && <GameIcon assetKey={"name:" + (entries.find(entry => entry.key === selected)?.name ?? "")} />}
            <select data-mechanic={name} aria-label={label ?? craftActionLabel(name.replace(/-/g, "_"))} value={selected}
                onChange={event => m.onValue(name, event.currentTarget.value)}>
                {entries.map(entry => <option key={entry.key} value={entry.key}>{entry.name}</option>)}
            </select></span></label>;
    };
    const action = (id: string, label = craftActionLabel(id), configured = false, assetKey = "action:" + id) => {
        const selected = configured ? m.selectedAction?.startsWith(id + ":") : m.selectedAction === id;
        const attribute = calculator ? (configured ? "data-derive-action" : "data-select-action") : (configured ? "data-config-action" : "data-simple-action");
        return <button key={id} {...{[attribute]: id}} className={selected ? "is-selected" : ""}
            onClick={() => configured ? m.onConfigured(id) : m.onSimple(id)}><GameIcon assetKey={assetKey} />{label}</button>;
    };
    let panel: ReactNode;
    switch (m.panel) {
        case "basic": panel = <><div className="pc-craft-options">{BASIC.map(id => action(id))}{calculator && action("restart", "Restart (fresh base)")}</div>
            {!calculator && <div className="pc-fracture-hint">Fracture rolls a random modifier. Right-click an item modifier or pool tier to set an exact fracture.</div>}</>; break;
        case "essence": {
            const groups = groupEssences(m.catalog.essences);
            const group = groups.find(group => group.type === value("essence-type")) ?? groups[0];
            const tiers = group?.tiers ?? [];
            const essence = tiers.find(tier => tier.key === value("essence-key")) ?? tiers[0];
            const asset = "name:" + (m.catalog.essences.find(entry => entry.key === essence?.key)?.name ?? "");
            panel = <div className="pc-mechanic-row">
                {select("essence-type", groups.map(group => ({key: group.type, name: group.type})), group?.type, "Type")}
                {select("essence-key", tiers.map(tier => ({key: tier.key, name: tier.tier})), essence?.key, "Tier")}
                {action("essence", calculator ? "Use essence" : "Apply essence", true, asset)}
            </div>; break;
        }
        case "harvest": panel = <><div className="pc-mechanic-row">
            {select("harvest-reforge-tag", harvestTagsFor(m.catalog.harvestTags, HARVEST_REFORGE), undefined, "Reforge")}{action("harvest_reforge", "Reforge", true)}
            {select("harvest-augment-tag", harvestTagsFor(m.catalog.harvestTags, HARVEST_AUGMENT), undefined, "Augment")}{action("harvest_augment", "Augment", true)}
        </div><div className="pc-mechanic-row">
            {select("resist-from", resistanceEntries(), "fire", "From")}<span>→</span>{select("resist-to", resistanceEntries(), "cold", "To")}
            {action("harvest_resist", "Convert resistance", true)}
        </div></>; break;
        case "fossil": panel = <><div className="pc-mechanic-row">
            {select("fossil", m.catalog.fossils, undefined, "Fossil", true)}<button data-fossil-add disabled={m.fossils.length >= 4} onClick={m.onAddFossil}>Add fossil</button>
            {!calculator && <button data-config-action="fossil" disabled={!m.fossils.length} onClick={() => m.onConfigured("fossil")}><GameIcon assetKey="action:fossil" />Craft</button>}
        </div><div className="pc-selected-fossils">{m.fossils.map((key, index) => {
            const name = m.catalog.fossils.find(entry => entry.key === key)?.name ?? key;
            return <span className="pc-chip" key={key}><GameIcon assetKey={"name:" + name} />{name}<button data-fossil-remove={index} title="Remove" aria-label={"Remove " + name} onClick={() => m.onRemoveFossil(index)}>×</button></span>;
        })}{!m.fossils.length && <span className="pc-help">Choose up to four fossils.</span>}</div></>; break;
        case "eldritch": panel = <><div className="pc-mechanic-row">
            {select("eldritch-tier", [1,2,3,4].map(tier => ({key: String(tier), name: "Tier " + tier})), "1", "Tier")}
            {action("eldritch_ember", "Ember", true)}{action("eldritch_ichor", "Ichor", true)}
        </div><div className="pc-craft-options">{["eldritch_exalt", "eldritch_chaos", "eldritch_annul"].map(id => action(id))}</div></>; break;
        case "influenced": panel = <div className="pc-mechanic-row">
            {select("influence", m.catalog.influences, undefined, "Influence")}
            {action("influence_exalt", "Influenced exalt", true, "influence:" + value("influence", m.catalog.influences[0]?.key))}
        </div>; break;
        case "bestiary": panel = <><div className="pc-craft-options">{m.bestiary.map(entry => <button key={entry.id}
            {...{[calculator ? "data-select-action" : "data-bestiary-action"]: entry.id}} className={m.selectedAction === entry.id ? "is-selected" : ""}
            onClick={() => m.onBestiary(entry.id)}><GameIcon assetKey={"action:" + entry.id} />{entry.display_name}</button>)}</div>
            {!calculator && <div className="pc-fracture-hint">Imprint checkpoint: {m.checkpoint ? "active" : "none"}.</div>}
            <div className="pc-fracture-hint">{m.bestiary.map(entry => `${entry.display_name}: ${entry.cost_keys.length ? entry.cost_keys.join(" + ") : "no beast cost"}`).join(" · ")}</div></>; break;
        case "veiled": panel = <><div className="pc-craft-options">{action("veiled_chaos", "Veiled Chaos")}{action("veiled_exalt", "Veiled Exalt")}{calculator && action("unveil")}</div>
            {!calculator && (m.unveils?.length ? <div className="pc-mechanic-row">{select("unveil", m.unveils, undefined, "Modifier")}{action("unveil", "Unveil", true)}</div> : <span className="pc-help">Apply a veiled modifier to choose an unveil.</span>)}</>; break;
    }
    return <><div className="pc-craft-panel-tabs">{PANELS.map(([key, label, icon]) => <button key={key} data-craft-panel={key}
        className={key === m.panel ? "is-active" : ""} onClick={() => m.onPanel(key)}><GameIcon assetKey={"action:" + icon} />{label}</button>)}
        {calculator && <span className="pc-calc-selected">{m.selectedAction ? "Selected: " + m.selectedLabel : "No action selected"}</span>}
    </div><div className="pc-craft-panel-body">{panel}</div></>;
}

export class PcCraftControls extends HTMLElement {
    private model: CraftControlsModel | null = null;
    connectedCallback(): void { this.render(); }
    disconnectedCallback(): void { disconnectReact(this); }
    setModel(model: CraftControlsModel): void { this.model = model; this.render(); }
    private render(): void { if (this.model) renderReact(this, <CraftControls model={this.model} />); }
}
customElements.define("pc-craft-controls", PcCraftControls);
