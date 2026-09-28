import { useState } from "react";
import type { ModifierFamilyOption } from "../modifier-options";
import { disconnectReact, renderReact } from "../react-host";

interface PickerProps {
    options: ModifierFamilyOption[];
    selected: Set<string>;
    buttonLabel: string;
    onSelect: (value: string, fractured: boolean) => void;
}

export function ModifierPicker({ options, selected, buttonLabel, onSelect }: PickerProps) {
    const [side, setSide] = useState<"prefix" | "suffix">("prefix");
    const [search, setSearch] = useState("");
    const [open, setOpen] = useState(false);
    const needle = search.trim().toLowerCase();
    const sections = new Map<string, ModifierFamilyOption[]>();
    for (const option of options) {
        if (option.side !== side || selected.has(option.value)) continue;
        if (needle && ![option.label, option.sourceLabel, ...option.tags, ...option.tiers.map(t => t.label)]
            .some(value => value.toLowerCase().includes(needle))) continue;
        const title = option.sourceKind === "base" ? "Base Mod Pool"
            : option.sourceKind === "influence" ? (option.sourceLabel || "Influenced") + " Mods"
            : option.sourceKind === "crafted" ? "Crafted Mods"
            : option.sourceKind === "essence" ? "Essence Mods" : "Fossil Mods";
        sections.set(title, [...(sections.get(title) ?? []), option]);
    }
    function select(value: string, fractured: boolean) {
        setOpen(false);
        setSearch("");
        onSelect(value, fractured);
    }
    return <div className="pc-modifier-picker">
        <button type="button" data-action="toggle" className="pc-modifier-picker-toggle" aria-expanded={open}
            onClick={() => setOpen(!open)}>{buttonLabel}</button>
        {open && <div className="pc-modifier-picker-popover">
            <input autoFocus type="search" data-action="search" aria-label="Search modifiers"
                placeholder="Search mods, tags, stat text…" value={search} onChange={event => setSearch(event.target.value)} />
            <div className="pc-mod-pool-tabs" role="tablist" aria-label="Modifier side">
                {(["prefix", "suffix"] as const).map(value => <button key={value} type="button" role="tab"
                    aria-selected={side === value} data-side={value} className={"pc-tab " + (side === value ? "is-active" : "")}
                    onClick={() => setSide(value)}>{value === "prefix" ? "Prefixes" : "Suffixes"}</button>)}
            </div>
            <div className="pc-modifier-picker-list">
                {sections.size ? [...sections].map(([title, entries]) => <section key={title} className="pc-modifier-picker-section">
                    <h4><span>{title}</span><span>{entries.length}</span></h4>
                    {entries.map(option => <button key={option.value} type="button" className="pc-modifier-family"
                        data-modifier-key={option.value} title="Click to add · right-click to require fractured"
                        onClick={() => select(option.value, false)}
                        onContextMenu={event => { event.preventDefault(); select(option.value, true); }}>
                        <span className="pc-mod-family-copy">
                            <span className="pc-mod-family-name">{option.label}</span>
                            {!!option.tags.length && <span className="pc-mod-family-tags">
                                {option.tags.map(tag => <span key={tag}>{tag.replace(/_/g, " ").replace(/\b\w/g, c => c.toUpperCase())}</span>)}
                            </span>}
                        </span>
                        <span className="pc-mod-family-meta">{option.tiers.length} tier{option.tiers.length === 1 ? "" : "s"}
                            {option.sourceLabel && <span>{option.sourceLabel}</span>}
                        </span>
                    </button>)}
                </section>) : <p className="pc-empty">No matching modifiers.</p>}
            </div>
        </div>}
    </div>;
}

export class PcModifierPicker extends HTMLElement {
    private options: ModifierFamilyOption[] = [];
    private selected = new Set<string>();
    private buttonLabel = "+ Add modifier";
    connectedCallback(): void { this.render(); }
    disconnectedCallback(): void { disconnectReact(this); }
    setOptions(options: ModifierFamilyOption[], selected: Iterable<string>): void {
        this.options = options;
        this.selected = new Set(selected);
        this.render();
    }
    setButtonLabel(label: string): void { this.buttonLabel = label; this.render(); }
    private render(): void {
        renderReact(this, <ModifierPicker options={this.options} selected={this.selected} buttonLabel={this.buttonLabel}
            onSelect={(value, fractured) => this.dispatchEvent(new CustomEvent("modifier-select", { bubbles: true, detail: { value, fractured } }))} />);
    }
}
customElements.define("pc-modifier-picker", PcModifierPicker);
