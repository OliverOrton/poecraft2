/*
 * Searchable modifier-family picker for strategy conditions. It mirrors the
 * emulator modifier pool's prefix/suffix tabs, family rows, source labels,
 * tags, and tier counts without exposing direct crafting controls.
 */

import type { ModifierFamilyOption } from "./pc-condition-editor";

type Side = "prefix" | "suffix";

export class PcModifierPicker extends HTMLElement {
    private options: ModifierFamilyOption[] = [];
    private selected = new Set<string>();
    private side: Side = "prefix";
    private search = "";
    private open = false;

    connectedCallback(): void {
        this.render();
    }

    setOptions(options: ModifierFamilyOption[], selected: Iterable<string>): void {
        this.options = options;
        this.selected = new Set(selected);
        this.render();
    }

    private render(): void {
        const needle = this.search.trim().toLowerCase();
        const visible = this.options.filter((option) => {
            if (option.side !== this.side || this.selected.has(option.value)) {
                return false;
            }
            if (!needle) return true;
            return [
                option.label,
                option.sourceLabel,
                ...option.tags,
                ...option.tiers.map((tier) => tier.label),
            ].some((value) => value.toLowerCase().includes(needle));
        });

        this.innerHTML = `
            <div class="pc-modifier-picker">
                <button type="button" data-action="toggle" class="pc-modifier-picker-toggle">
                    + Add modifier
                </button>
                ${
                    this.open
                        ? `<div class="pc-modifier-picker-popover">
                            <input type="text" data-action="search"
                                placeholder="Search mods, tags, stat text…"
                                value="${escapeAttribute(this.search)}">
                            <div class="pc-mod-pool-tabs" role="tablist">
                                <button type="button" data-side="prefix"
                                    class="pc-tab ${this.side === "prefix" ? "is-active" : ""}">Prefixes</button>
                                <button type="button" data-side="suffix"
                                    class="pc-tab ${this.side === "suffix" ? "is-active" : ""}">Suffixes</button>
                            </div>
                            <div class="pc-modifier-picker-list">
                                ${
                                    visible.length
                                        ? renderSections(visible)
                                        : '<p class="pc-empty">No matching modifiers.</p>'
                                }
                            </div>
                        </div>`
                        : ""
                }
            </div>`;

        this.querySelector('[data-action="toggle"]')?.addEventListener(
            "click",
            () => {
                this.open = !this.open;
                this.render();
                if (this.open) {
                    this.querySelector<HTMLInputElement>(
                        '[data-action="search"]',
                    )?.focus();
                }
            },
        );
        this.querySelector<HTMLInputElement>('[data-action="search"]')?.addEventListener(
            "input",
            (event) => {
                this.search = (event.currentTarget as HTMLInputElement).value;
                this.render();
                const input = this.querySelector<HTMLInputElement>(
                    '[data-action="search"]',
                );
                input?.focus();
                input?.setSelectionRange(this.search.length, this.search.length);
            },
        );
        this.querySelectorAll<HTMLButtonElement>("[data-side]").forEach((button) => {
            button.addEventListener("click", () => {
                this.side = button.dataset.side as Side;
                this.render();
            });
        });
        this.querySelectorAll<HTMLButtonElement>("[data-modifier-key]").forEach(
            (button) => {
                button.addEventListener("click", () => {
                    const value = button.dataset.modifierKey ?? "";
                    if (!value) return;
                    this.open = false;
                    this.search = "";
                    this.dispatchEvent(
                        new CustomEvent("modifier-select", {
                            bubbles: true,
                            detail: { value, fractured: false },
                        }),
                    );
                });
                button.addEventListener("contextmenu", (event) => {
                    event.preventDefault();
                    const value = button.dataset.modifierKey ?? "";
                    if (!value) return;
                    this.open = false;
                    this.search = "";
                    this.dispatchEvent(
                        new CustomEvent("modifier-select", {
                            bubbles: true,
                            detail: { value, fractured: true },
                        }),
                    );
                });
            },
        );
    }
}

function renderSections(options: ModifierFamilyOption[]): string {
    const sections = new Map<string, ModifierFamilyOption[]>();
    for (const option of options) {
        const title =
            option.sourceKind === "base"
                ? "Base Mod Pool"
                : option.sourceKind === "influence"
                  ? `${option.sourceLabel || "Influenced"} Mods`
                  : option.sourceKind === "crafted"
                    ? "Crafted Mods"
                    : option.sourceKind === "essence"
                      ? "Essence Mods"
                      : "Fossil Mods";
        sections.set(title, [...(sections.get(title) ?? []), option]);
    }
    return Array.from(sections.entries())
        .map(
            ([title, entries]) => `
                <section class="pc-modifier-picker-section">
                    <h4>
                        <span>${escapeHtml(title)}</span>
                        <span>${entries.length}</span>
                    </h4>
                    ${entries.map((option) => renderFamily(option)).join("")}
                </section>`,
        )
        .join("");
}

function renderFamily(option: ModifierFamilyOption): string {
    return `
        <button type="button" class="pc-modifier-family"
            data-modifier-key="${escapeAttribute(option.value)}"
            title="Click to add · right-click to require fractured">
            <span class="pc-mod-family-copy">
                <span class="pc-mod-family-name">${escapeHtml(option.label)}</span>
                ${
                    option.tags.length
                        ? `<span class="pc-mod-family-tags">${option.tags
                              .map(
                                  (tag) =>
                                      `<span>${escapeHtml(formatTag(tag))}</span>`,
                              )
                              .join("")}</span>`
                        : ""
                }
            </span>
            <span class="pc-mod-family-meta">
                ${option.tiers.length} tier${option.tiers.length === 1 ? "" : "s"}
                ${option.sourceLabel ? `<span>${escapeHtml(option.sourceLabel)}</span>` : ""}
            </span>
        </button>`;
}

function formatTag(value: string): string {
    return value
        .replace(/_/g, " ")
        .replace(/\b\w/g, (character) => character.toUpperCase());
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

customElements.define("pc-modifier-picker", PcModifierPicker);
