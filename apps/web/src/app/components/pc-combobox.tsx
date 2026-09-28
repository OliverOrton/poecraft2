import { useId, useState, useEffect } from "react";
import { disconnectReact, renderReact } from "../react-host";
import { formatModText } from "../mod-text";
import { GameIcon } from "./pc-game-icon";

export interface ComboOption { value: string; label: string; assetKey?: string; }
const MAX_VISIBLE = 200;

export function Combobox({options, value, placeholder, onSelect}: {
    options: ComboOption[]; value: string; placeholder: string; onSelect: (value: string) => void;
}) {
    const label = formatModText(options.find(option => option.value === value)?.label ?? value);
    const [query, setQuery] = useState(label);
    const [open, setOpen] = useState(false);
    const [active, setActive] = useState(0);
    const id = useId();
    useEffect(() => setQuery(label), [label]);
    const needle = query.trim().toLowerCase();
    const filtered = options.filter(option => !needle || formatModText(option.label).toLowerCase().includes(needle));
    const shown = filtered.slice(0, MAX_VISIBLE);
    function select(option: ComboOption) {
        setQuery(formatModText(option.label));
        setOpen(false);
        onSelect(option.value);
    }
    return <div className="pc-combobox">
        <input className="pc-combobox-input" type="text" role="combobox" placeholder={placeholder} aria-label={placeholder}
            aria-controls={id} aria-expanded={open} aria-autocomplete="list"
            aria-activedescendant={open && shown[active] ? id + "-" + active : undefined}
            autoComplete="off" spellCheck={false} value={query}
            onFocus={() => { setOpen(true); setActive(0); }}
            onBlur={() => { setOpen(false); setQuery(label); }}
            onChange={event => { event.stopPropagation(); setQuery(event.target.value); setOpen(true); setActive(0); }}
            onKeyDown={event => {
                if (event.key === "Escape") { setOpen(false); setQuery(label); }
                else if (event.key === "ArrowDown") { event.preventDefault(); setOpen(true); setActive(Math.min(shown.length - 1, active + 1)); }
                else if (event.key === "ArrowUp") { event.preventDefault(); setActive(Math.max(0, active - 1)); }
                else if (event.key === "Enter" && open && shown[active]) { event.preventDefault(); select(shown[active]); }
            }} />
        <ul id={id} className="pc-combobox-list" role="listbox" hidden={!open}>
            {shown.map((option, index) => <li key={option.value} id={id + "-" + index} role="option" aria-selected={option.value === value}
                data-value={option.value} className={index === active ? "is-active" : option.value === value ? "selected" : ""}
                onMouseDown={event => { event.preventDefault(); select(option); }}><GameIcon assetKey={option.assetKey ?? "name:" + option.label} />{formatModText(option.label)}</li>)}
            {filtered.length > shown.length && <li className="pc-combobox-more">…and {filtered.length - shown.length} more — keep typing</li>}
            {!shown.length && <li className="pc-combobox-more">No matching options</li>}
        </ul>
    </div>;
}

export class PcCombobox extends HTMLElement {
    private options: ComboOption[] = [];
    private value = "";
    connectedCallback(): void {
        // Text-entry change events are not committed option selections. The
        // adapter publishes only its CustomEvent with a stable option value.
        this.onchange = event => { if (!(event instanceof CustomEvent)) event.stopImmediatePropagation(); };
        this.render();
    }
    disconnectedCallback(): void { disconnectReact(this); }
    setOptions(options: ComboOption[]): void { this.options = options; this.render(); }
    setValue(value: string): void { this.value = value; this.render(); }
    getValue(): string { return this.value; }
    private render(): void {
        renderReact(this, <Combobox options={this.options} value={this.value} placeholder={this.getAttribute("placeholder") ?? "Search…"}
            onSelect={value => {
                this.value = value;
                this.render();
                this.dispatchEvent(new CustomEvent("change", { detail: { value }, bubbles: true }));
            }} />);
    }
}
customElements.define("pc-combobox", PcCombobox);
