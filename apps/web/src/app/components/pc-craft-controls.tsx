import type { ReactNode } from "react";
import type { BestiaryActionInfo, Catalog, CatalogEntry } from "../engine-protocol";
import { craftActionLabel, groupEssences, resistanceEntries } from "../craft-choices";
import { HARVEST_AUGMENT, HARVEST_REFORGE, harvestTagsFor } from "../harvest-crafts";
import { disconnectReact, renderReact } from "../react-host";
import { GameIcon } from "./pc-game-icon";
import { CraftChoice } from "./craft-choice";

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
    itemClass?: string;
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
    onAddFossil: (key: string) => void;
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
    const action = (id: string, label = craftActionLabel(id), configured = false, disabled = false) => {
        const selected = !configured && m.selectedAction === id;
        const attribute = calculator ? (configured ? "data-derive-action" : "data-select-action") : (configured ? "data-config-action" : "data-simple-action");
        return <button key={id} {...{[attribute]: id}} className={configured ? "pc-craft-apply" : selected ? "is-selected" : ""} disabled={disabled}
            onClick={() => configured ? m.onConfigured(id) : m.onSimple(id)}>{!configured && <GameIcon assetKey={"action:" + id} />}{label}</button>;
    };
    const choices = (name: string, label: string, entries: Array<{key: string; name: string}>, art: (entry: {key: string; name: string}) => string) =>
        <div className="pc-material-options" role="group" aria-label={label}>
            {entries.map(entry => <CraftChoice key={entry.key} name={name} value={entry.key} label={entry.name}
                assetKey={art(entry)} selected={value(name) === entry.key} itemClass={m.itemClass}
                onChoose={() => m.onValue(name, entry.key)} />)}
        </div>;
    let panel: ReactNode;
    switch (m.panel) {
        case "basic": panel = <><div className="pc-craft-options">{BASIC.map(id => action(id))}{calculator && action("restart", "Restart (fresh base)")}</div>
            {!calculator && <div className="pc-fracture-hint">Fracture rolls a random modifier. Right-click an item modifier or pool tier to set an exact fracture.</div>}</>; break;
        case "essence": {
            const groups = groupEssences(m.catalog.essences);
            const group = groups.find(group => group.type === value("essence-type")) ?? groups[0];
            const tiers = group?.tiers ?? [];
            const essence = tiers.find(tier => tier.key === value("essence-key")) ?? tiers[0];
            panel = <div className="pc-material-panel">
                <div className="pc-material-heading">Type</div>
                {choices("essence-type", "Essence type", groups.map(group => ({key: group.type, name: group.type})),
                    entry => entry.key === group?.type ? essence?.key ?? "" : groups.find(group => group.type === entry.key)?.tiers[0]?.key ?? "")}
                <div className="pc-material-heading">Tier</div>
                {choices("essence-key", "Essence tier", tiers.map(tier => ({key: tier.key, name: tier.tier})), entry => entry.key)}
                <div className="pc-material-footer"><span>{m.catalog.essences.find(entry => entry.key === essence?.key)?.name}</span>
                    {action("essence", calculator ? "Calculate odds" : "Apply essence", true, !essence)}
                </div>
            </div>; break;
        }
        case "harvest": panel = <div className="pc-harvest-layout"><div className="pc-harvest-main">
            <section><div className="pc-material-heading">Reforge</div>
                {choices("harvest-reforge-tag", "Reforge modifier type", harvestTagsFor(m.catalog.harvestTags, HARVEST_REFORGE), entry => "harvest:reforge:" + entry.key)}
                <div className="pc-material-footer">{action("harvest_reforge", calculator ? "Calculate reforge" : "Reforge", true)}</div>
            </section>
            <section><div className="pc-material-heading">Augment</div>
                {choices("harvest-augment-tag", "Augment modifier type", harvestTagsFor(m.catalog.harvestTags, HARVEST_AUGMENT), entry => "harvest:augment:" + entry.key)}
                <div className="pc-material-footer">{action("harvest_augment", calculator ? "Calculate augment" : "Augment", true)}</div>
            </section>
        </div><section className="pc-harvest-resistance"><div className="pc-material-heading">Convert resistance</div>
            <div className="pc-material-heading">From</div>
            {choices("resist-from", "Resistance to replace", resistanceEntries(), entry => "harvest:resistance:" + entry.key)}
            <div className="pc-material-heading">To</div>
            {choices("resist-to", "New resistance", resistanceEntries(), entry => "harvest:resistance:" + entry.key)}
            <div className="pc-material-footer">{action("harvest_resist", calculator ? "Calculate conversion" : "Convert resistance", true, value("resist-from") === value("resist-to"))}</div>
        </section></div>; break;
        case "fossil": panel = <div className="pc-material-panel">
            <div className="pc-material-options" role="group" aria-label="Fossils">
                {m.catalog.fossils.map(entry => {
                    const index = m.fossils.indexOf(entry.key);
                    return <CraftChoice key={entry.key} name="fossil" value={entry.key} label={entry.name.replace(/ Fossil$/, "")}
                        assetKey={entry.key} selected={index >= 0} blocked={index < 0 && m.fossils.length >= 4}
                        onChoose={() => index >= 0 ? m.onRemoveFossil(index) : m.onAddFossil(entry.key)} />;
                })}
            </div>
            <div className="pc-material-footer"><span>{m.fossils.length}/4 fossils selected</span>
                {action("fossil", calculator ? "Calculate odds" : "Craft", true, !m.fossils.length)}
            </div>
        </div>; break;
        case "eldritch": panel = <><div className="pc-mechanic-row">
            {select("eldritch-tier", [1,2,3,4].map(tier => ({key: String(tier), name: "Tier " + tier})), "1", "Tier")}
            {action("eldritch_ember", "Ember", true)}{action("eldritch_ichor", "Ichor", true)}
        </div><div className="pc-craft-options">{["eldritch_exalt", "eldritch_chaos", "eldritch_annul"].map(id => action(id))}</div></>; break;
        case "influenced": panel = <div className="pc-material-panel">
            {choices("influence", "Influence", m.catalog.influences, entry => "influence:" + entry.key)}
            <div className="pc-material-footer">{action("influence_exalt", calculator ? "Calculate odds" : "Influenced exalt", true)}</div>
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
