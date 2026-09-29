/*
 * pc-mod-list — presentational concrete-item and target-item slot ledgers.
 * Both modes share the same fixed prefix / suffix frame. Concrete mode shows
 * rolled modifiers and emits fracture actions; target mode shows authored
 * family thresholds and emits stable goal-edit actions.
 */

import { isCorrupted, placeStableSlots, visibleModTags } from "../item-display";
import { VeiledInscription } from "./unveil-panel";
import type {CatalogEntry} from "../engine-protocol";

export interface ItemPropertyEditor {
    influences: CatalogEntry[];
    influenceBits?: number;
    corrupted?: boolean;
}
export interface ItemPropertyChange {
    rarity?: "normal" | "magic" | "rare";
    influence_bits?: number | null;
    corrupted?: boolean | null;
}

export interface SlotMod {
    sessionModId: number;
    key: string;
    tierIndex: number;
    textLines: string[];
    classificationTags: string[];
    fractured: boolean;
    crafted: boolean;
    veiled?: boolean;
}

export interface ConcreteModListModel {
    properties?: ItemPropertyEditor;
    itemFlags: number;
    readOnly?: boolean;
    memoryStrands?: number;
    lifecycle?: number;
    kind: "concrete";
    baseKey?: string;
    baseName?: string;
    itemLevel?: number;
    rarity: string;
    influences: string[];
    implicits: SlotMod[];
    enchantments?: SlotMod[];
    prefixes: SlotMod[];
    suffixes: SlotMod[];
    maxPrefix: number;
    maxSuffix: number;
}

export interface TargetTierChoice {
    tier: number;
    label: string;
}

export interface TargetSlotMod {
    familyModKey: string;
    textLines: string[];
    classificationTags: string[];
    sourceLabel: string;
    minTier: number;
    tiers: TargetTierChoice[];
    probabilityLabel?: string;
}

export interface TargetOtherRequirement {
    slotIndex: number;
    label: string;
    minTier: number;
    probabilityLabel?: string;
}

export interface TargetModListModel {
    implicitInfluences?: string[];
    properties?: ItemPropertyEditor;
    implicits?: Array<{key: string; textLines: string[]; sourceLabel?: string; probabilityLabel?: string}>;
    kind: "target";
    baseKey?: string;
    baseName: string;
    itemLevel: number;
    rarity: "normal" | "magic" | "rare";
    prefixes: TargetSlotMod[];
    suffixes: TargetSlotMod[];
    otherRequirements: TargetOtherRequirement[];
    maxPrefix: number;
    maxSuffix: number;
}

export type PcModListModel = ConcreteModListModel | TargetModListModel;


import { useRef, type ReactNode } from "react";
import { disconnectReact, renderReact } from "../react-host";
import { formatModText } from "../mod-text";
import { GameIcon } from "./pc-game-icon";

interface ItemCardProps {
    model: PcModListModel;
    slotHistory?: ItemSlotHistory;
    onFracture?: (detail: {key: string; modId: number; side: "prefix" | "suffix"}) => void;
    onTierChange?: (detail: {familyModKey: string; minTier: number}) => void;
    onRemove?: (detail: {familyModKey: string} | {slotIndex: number} | {implicitKey: string}) => void;
    onProperties?: (detail: ItemPropertyChange) => void;
    onChooseMods?: (side: "prefix" | "suffix" | "implicit") => void;
    onRemoveMod?: (detail: {key: string; modId: number; side: "prefix" | "suffix" | "implicit"}) => void;
}

interface ItemSlotHistory {
    concrete: {prefix: Array<number | undefined>; suffix: Array<number | undefined>};
    target: {prefix: Array<string | undefined>; suffix: Array<string | undefined>};
}

/** One item-card view for emulator, calculator targets, strategies and stash. */
export function ItemCard({ model, slotHistory, onFracture, onTierChange, onRemove, onProperties, onChooseMods, onRemoveMod }: ItemCardProps) {
    const concreteIds = useRef(slotHistory?.concrete ?? {prefix: [] as Array<number | undefined>, suffix: [] as Array<number | undefined>});
    const targetIds = useRef(slotHistory?.target ?? {prefix: [] as Array<string | undefined>, suffix: [] as Array<string | undefined>});
    const target = model.kind === "target";
    const corrupted = model.kind === "concrete" ? isCorrupted(model.itemFlags) : model.properties?.corrupted === true;
    const editable = Boolean(model.properties) && (model.kind === "target" || !model.readOnly);
    const count = model.prefixes.length + model.suffixes.length;
    const countLabel = target
        ? `${count + model.otherRequirements.length + (model.implicits?.length ?? 0)} requirements · ${model.prefixes.length}P / ${model.suffixes.length}S`
        : `${count} explicit · ${model.prefixes.length}P / ${model.suffixes.length}S`;
    function group(side: "prefix" | "suffix") {
        const mods = side === "prefix" ? model.prefixes : model.suffixes;
        const capacity = side === "prefix" ? model.maxPrefix : model.maxSuffix;
        let rows: ReactNode[];
        let length: number;
        if (model.kind === "concrete") {
            const layout = placeStableSlots(mods as SlotMod[], capacity, concreteIds.current[side], mod => mod.sessionModId);
            concreteIds.current[side] = layout.ids;
            length = layout.slots.length;
            rows = layout.slots.map((mod, index) => mod
                ? <ConcreteSlot key={index} mod={mod} side={side} index={index} onFracture={model.readOnly ? undefined : onFracture} onRemove={editable ? onRemoveMod : undefined} />
                : <EmptySlot key={index} side={side} index={index} target={false} />);
        } else {
            const layout = placeStableSlots(mods as TargetSlotMod[], Math.max(capacity, mods.length), targetIds.current[side], mod => mod.familyModKey);
            targetIds.current[side] = layout.ids;
            length = layout.slots.length;
            rows = layout.slots.map((mod, index) => mod
                ? <TargetSlot key={index} mod={mod} side={side} index={index} onTierChange={onTierChange} onRemove={onRemove} />
                : <EmptySlot key={index} side={side} index={index} target />);
        }
        if (!length) return null;
        return <section className={`pc-mod-group pc-mod-group-${side}`}>
            <h4><span>{side === "prefix" ? "Prefixes" : "Suffixes"}</span><span>{mods.length}/{target ? capacity : length}</span>{editable && <button className="pc-item-add-mod" type="button" data-add-mod-side={side} onClick={() => onChooseMods?.(side)}>Add</button>}</h4>
            <ul className="pc-mod-slots">{rows}</ul>
        </section>;
    }
    return <div className={`pc-mod-list ${target ? "pc-mod-list-target" : ""} ${corrupted ? "is-corrupted" : ""} pc-item-rarity-${model.rarity}`} data-mode={model.kind}>
        <header className="pc-item-card-header">
            <GameIcon assetKey={model.baseKey ?? "name:" + model.baseName} size="item" />
            {model.baseName && <div className="pc-item-title-line"><strong>{model.baseName}</strong>{!!model.itemLevel && <span>iLvl {model.itemLevel}</span>}</div>}
            <div className="pc-mod-list-header">
                <span className="pc-item-heading">
                    <span className={`pc-rarity pc-rarity-${model.rarity}`}>{model.rarity}</span>
                    {corrupted && <span className="pc-item-corrupted">Corrupted</span>}
                    {model.kind === "concrete" && !!model.memoryStrands && <span>Memory strands: {model.memoryStrands}</span>}
                    {model.kind === "concrete" && !!model.lifecycle && <span>{model.lifecycle === 1 ? "Consumed" : "Destroyed"}</span>}
                    {target && <span className="pc-item-target-badge">TARGET</span>}
                    {target && model.implicitInfluences?.map(name => <span className="pc-item-influence" key={name}>{name}</span>)}
                    {target && model.properties?.influenceBits !== undefined && <span className="pc-item-influences">
                        {model.properties.influenceBits === 0 ? "No ordinary influence" : model.properties.influences.filter(entry => model.properties!.influenceBits! & (1 << ((entry.code ?? 1) - 1))).map(entry => entry.name).join(" · ")}
                    </span>}
                    {model.kind === "concrete" && !!model.influences.length && <span className="pc-item-influences">
                        {model.influences.map(influence => <span key={influence} className="pc-item-influence"><GameIcon assetKey={"influence:" + influence} />{influence}</span>)}
                    </span>}
                </span>
                <span className="pc-mod-count">{countLabel}</span>
            </div>
        </header>
        {model.properties && <details className="pc-item-properties"><summary>Item properties</summary>
            <label>Rarity<select aria-label="Item rarity" disabled={!editable} value={model.rarity} onChange={event => onProperties?.({rarity: event.target.value as "normal" | "magic" | "rare"})}>
                {["normal", "magic", "rare"].map(rarity => <option key={rarity} value={rarity}>{rarity}</option>)}</select></label>
            <label>Corruption<select aria-label="Item corruption" disabled={!editable} value={model.properties.corrupted === undefined ? "any" : String(model.properties.corrupted)}
                onChange={event => onProperties?.({corrupted: event.target.value === "any" ? null : event.target.value === "true"})}>
                {target && <option value="any">Any</option>}<option value="false">Uncorrupted</option><option value="true">Corrupted</option></select></label>
            <fieldset disabled={!editable}><legend>Influences</legend>
                {target && <label><input type="checkbox" aria-label="Any influence" checked={model.properties.influenceBits === undefined}
                    onChange={event => onProperties?.({influence_bits: event.target.checked ? null : 0})} />Any influence</label>}
                {model.properties.influences.map(influence => {
                    const bit = 1 << ((influence.code ?? 1) - 1);
                    return <label key={influence.key}><input type="checkbox" aria-label={influence.name} checked={Boolean((model.properties!.influenceBits ?? 0) & bit)}
                        onChange={event => onProperties?.({influence_bits: event.target.checked ? (model.properties!.influenceBits ?? 0) | bit : (model.properties!.influenceBits ?? 0) & ~bit})} />{influence.name}</label>;
                })}
                {target && model.properties.influenceBits !== undefined && <small>Exactly the selected influences{model.properties.influenceBits === 0 ? " (none)" : ""}.</small>}
            </fieldset>
        </details>}
        {(!!model.implicits?.length || model.properties) && <section className="pc-mod-group pc-mod-group-implicit">
            <h4><span>Implicits</span><span>{model.implicits?.length ?? 0}</span></h4>
            <ul className="pc-mod-slots">{model.kind === "concrete" ? model.implicits.map((mod, index) => <ConcreteSlot key={mod.key} mod={mod} side="implicit" index={index} onRemove={editable ? onRemoveMod : undefined} />)
                : model.implicits?.map((mod, index) => <li className="pc-mod-slot pc-mod-implicit is-filled" key={mod.key} data-target-implicit={mod.key}>
                    <SlotMeta side="implicit" index={index} /><div className="pc-mod-slot-content"><ModLines lines={mod.textLines} />
                        <div className="pc-mod-target-actions"><span>{mod.sourceLabel}</span>{mod.probabilityLabel && <span>{mod.probabilityLabel}</span>}
                            <button type="button" aria-label="Remove implicit requirement" onClick={() => onRemove?.({implicitKey: mod.key})}>×</button></div></div></li>)}</ul>
            {!model.implicits?.length && <p className="pc-help">{target ? "No implicit requirements" : "No implicits"}</p>}
        </section>}
        {model.kind === "concrete" && !!model.enchantments?.length && <section className="pc-mod-group pc-mod-group-implicit">
            <h4>Enchantments · retained state; stat-total effects unavailable</h4>
            <ul className="pc-mod-slots">{model.enchantments.map((mod, index) => <ConcreteSlot key={index} mod={mod} side="implicit" index={index} />)}</ul>
        </section>}
        <div className="pc-mod-explicit-ledger">
            {group("prefix")}{group("suffix")}
            {model.kind === "target" && !!model.otherRequirements.length && <section className="pc-mod-group pc-mod-group-other">
                <h4><span>Other requirements</span><span>{model.otherRequirements.length}</span></h4>
                <ul className="pc-mod-other-requirements">{model.otherRequirements.map(requirement => <li key={requirement.slotIndex}>
                    <div><strong>{formatModText(requirement.label)}</strong><span>{requirement.minTier > 0 ? `T${requirement.minTier} or better` : "Any matching tier"}</span></div>
                    {requirement.probabilityLabel && <span className="pc-mod-target-probability">{requirement.probabilityLabel}</span>}
                    <button type="button" className="pc-icon-button pc-mod-target-remove" data-target-slot-index={requirement.slotIndex}
                        aria-label="Remove requirement" onClick={() => onRemove?.({slotIndex: requirement.slotIndex})}>×</button>
                </li>)}</ul>
            </section>}
        </div>
    </div>;
}

function ModLines({ lines }: {lines: string[]}) {
    return <div className="pc-mod-slot-lines">{lines.map((line, index) =>
        <div key={index} className="pc-mod-slot-line">{formatModText(line)}</div>)}</div>;
}
function SlotMeta({ side, index, tier }: {side: string; index: number; tier?: string}) {
    return <><span className="pc-mod-slot-rail" aria-hidden="true" /><span className="pc-mod-slot-meta">
        <strong>{side === "implicit" ? "I" : side === "prefix" ? "P" : "S"}{index + 1}</strong>{tier && <span>{tier}</span>}
    </span></>;
}
function ConcreteSlot({ mod, side, index, onFracture, onRemove }: {
    mod: SlotMod; side: "implicit" | "prefix" | "suffix"; index: number; onFracture?: ItemCardProps["onFracture"];
    onRemove?: ItemCardProps["onRemoveMod"];
}) {
    const tags = visibleModTags(mod.classificationTags).map(formatTag);
    return <li className={`pc-mod-slot pc-mod-${side} is-filled ${mod.crafted ? "is-crafted" : ""} ${mod.fractured ? "is-fractured" : ""}`}
        data-side={side === "implicit" ? undefined : side} data-mod-id={mod.sessionModId} data-mod-key={mod.key} data-fractured={mod.fractured}
        title={[...mod.textLines.map(formatModText), side === "implicit" ? "" : mod.fractured ? "Fractured modifier" : onFracture ? "Right-click to mark this modifier as fractured" : ""].filter(Boolean).join("\n")}
        onContextMenu={event => {
            if (side === "implicit" || !onFracture) return;
            event.preventDefault();
            if (!mod.fractured) onFracture?.({key: mod.key, modId: mod.sessionModId, side});
        }}>
        <SlotMeta side={side} index={index} tier={mod.tierIndex ? `T${mod.tierIndex}` : mod.crafted ? "C" : "—"} />
        <div className="pc-mod-slot-content">
            {mod.veiled ? <VeiledInscription index={index} /> : <ModLines lines={mod.textLines.length ? mod.textLines : [mod.key]} />}
            {(!!tags.length || mod.fractured || mod.crafted) && <div className="pc-mod-slot-tags">
                {tags.map(tag => <span key={tag}>{tag}</span>)}
                {mod.fractured && <span className="pc-mod-state is-fractured">Fractured</span>}
                {mod.crafted && <span className="pc-mod-state is-crafted">Crafted</span>}
            </div>}
            {onRemove && <button type="button" className="pc-item-remove-mod" aria-label="Remove modifier" onClick={() => onRemove({key: mod.key, modId: mod.sessionModId, side})}>×</button>}
        </div>
    </li>;
}
function TargetSlot({mod, side, index, onTierChange, onRemove}: {
    mod: TargetSlotMod; side: "prefix" | "suffix"; index: number;
    onTierChange?: ItemCardProps["onTierChange"]; onRemove?: ItemCardProps["onRemove"];
}) {
    const tags = visibleModTags(mod.classificationTags).map(formatTag);
    if (mod.sourceLabel && !tags.includes(mod.sourceLabel)) tags.push(mod.sourceLabel);
    return <li className={`pc-mod-slot pc-mod-${side} pc-mod-target-slot is-filled`} data-target-family={mod.familyModKey}>
        <SlotMeta side={side} index={index} tier={mod.minTier > 0 ? `T${mod.minTier}` : "ANY"} />
        <div className="pc-mod-slot-content">
            <ModLines lines={mod.textLines} />
            {!!tags.length && <div className="pc-mod-slot-tags">{tags.map(tag => <span key={tag}>{tag}</span>)}</div>}
            <div className="pc-mod-target-actions">
                <select data-target-family-key={mod.familyModKey} value={mod.minTier}
                    aria-label={`Minimum tier for ${formatModText(mod.textLines[0] ?? mod.familyModKey)}`}
                    onChange={event => onTierChange?.({familyModKey: mod.familyModKey, minTier: Number(event.target.value)})}>
                    <option value={0}>Any tier</option>
                    {mod.tiers.map(tier => <option key={tier.tier} value={tier.tier} title={formatModText(tier.label)}>T{tier.tier} or better</option>)}
                </select>
                {mod.probabilityLabel && <span className="pc-mod-target-probability">{mod.probabilityLabel}</span>}
                <button type="button" className="pc-icon-button pc-mod-target-remove" data-target-family-key={mod.familyModKey}
                    aria-label="Remove requirement" onClick={() => onRemove?.({familyModKey: mod.familyModKey})}>×</button>
            </div>
        </div>
    </li>;
}
function EmptySlot({side, index, target}: {side: "prefix" | "suffix"; index: number; target: boolean}) {
    return <li className={`pc-mod-slot pc-mod-${side} is-empty`}>
        <SlotMeta side={side} index={index} />
        <div className="pc-mod-slot-content"><div className="pc-mod-slot-line pc-mod-slot-empty">
            {target ? `No ${side} requirement` : `Open ${side}`}
        </div></div>
    </li>;
}
function formatTag(tag: string): string { return tag.replace(/_/g, " ").replace(/\b\w/g, character => character.toUpperCase()); }

export class PcModList extends HTMLElement {
    private model: PcModListModel | null = null;
    private slotHistory: ItemSlotHistory = {concrete: {prefix: [], suffix: []}, target: {prefix: [], suffix: []}};
    connectedCallback(): void { if (this.model) this.setModel(this.model); }
    disconnectedCallback(): void { disconnectReact(this); }
    setReadOnly(readOnly: boolean): void {
        if (this.model?.kind === "concrete" && Boolean(this.model.readOnly) !== readOnly) this.setModel({...this.model, readOnly});
    }
    setModel(model: PcModListModel): void {
        this.model = model;
        const emit = (name: string, detail: unknown) => this.dispatchEvent(new CustomEvent(name, {bubbles: true, detail}));
        renderReact(this, <ItemCard model={model} slotHistory={this.slotHistory} onFracture={detail => emit("fracture-mod", detail)}
            onTierChange={detail => emit("target-tier-change", detail)} onRemove={detail => emit("target-remove", detail)}
            onProperties={detail => emit("item-properties-change", detail)} onChooseMods={side => emit("choose-mods", {side})}
            onRemoveMod={detail => emit("remove-item-mod", detail)} />);
    }
}
customElements.define("pc-mod-list", PcModList);
