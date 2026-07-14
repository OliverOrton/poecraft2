/*
 * pc-mod-list — presentational view of an item: fixed prefix / suffix slot grid
 * with in-game-style text. Empty slots stay in place as the item fills so the
 * stepper-style emulator flow doesn't reflow on every craft.
 *
 * Implicits render above explicits as a free-form list (their count is fixed
 * per base; we just show whatever the engine returned).
 */

import { placeStableSlots, visibleModTags } from "../item-display";

export interface SlotMod {
    sessionModId: number;
    key: string;
    tierIndex: number;
    textLines: string[];
    classificationTags: string[];
    fractured: boolean;
    crafted: boolean;
}

export interface ModListModel {
    baseName?: string;
    itemLevel?: number;
    rarity: string;
    influences: string[];
    implicits: SlotMod[];
    prefixes: SlotMod[];
    suffixes: SlotMod[];
    maxPrefix: number;
    maxSuffix: number;
}

export class PcModList extends HTMLElement {
    private prefixSlotIds: Array<number | undefined> = [];
    private suffixSlotIds: Array<number | undefined> = [];

    setModel(model: ModListModel): void {
        const explicitCount = model.prefixes.length + model.suffixes.length;
        const prefixLayout = placeStableSlots(
            model.prefixes,
            model.maxPrefix,
            this.prefixSlotIds,
            (mod) => mod.sessionModId,
        );
        const suffixLayout = placeStableSlots(
            model.suffixes,
            model.maxSuffix,
            this.suffixSlotIds,
            (mod) => mod.sessionModId,
        );
        this.prefixSlotIds = prefixLayout.ids;
        this.suffixSlotIds = suffixLayout.ids;
        const title = model.baseName
            ? `<div class="pc-item-title-line">
                <strong>${escapeHtml(model.baseName)}</strong>
                ${model.itemLevel ? `<span>iLvl ${model.itemLevel}</span>` : ""}
            </div>`
            : "";
        this.innerHTML = `
            <div class="pc-mod-list pc-item-rarity-${model.rarity}">
                <header class="pc-item-card-header">
                    ${title}
                    <div class="pc-mod-list-header">
                        <span class="pc-item-heading">
                            <span class="pc-rarity pc-rarity-${model.rarity}">${model.rarity}</span>
                            ${
                                model.influences.length
                                    ? `<span class="pc-item-influences">${model.influences
                                          .map(
                                              (influence) =>
                                                  `<span class="pc-item-influence">${escapeHtml(influence)}</span>`,
                                          )
                                          .join("")}</span>`
                                    : ""
                            }
                        </span>
                        <span class="pc-mod-count">${explicitCount} explicit · ${model.prefixes.length}P / ${model.suffixes.length}S</span>
                    </div>
                </header>
                ${renderImplicits(model.implicits)}
                <div class="pc-mod-explicit-ledger">
                    ${renderSlotGroup("Prefixes", "prefix", prefixLayout.slots, model.prefixes.length)}
                    ${renderSlotGroup("Suffixes", "suffix", suffixLayout.slots, model.suffixes.length)}
                </div>
            </div>`;
        this.querySelectorAll<HTMLElement>(
            '.pc-mod-slot.is-filled[data-side][data-mod-id]',
        ).forEach((row) => {
            row.addEventListener("contextmenu", (event) => {
                event.preventDefault();
                if (row.dataset.fractured === "true") return;
                const side = row.dataset.side;
                if (side !== "prefix" && side !== "suffix") return;
                this.dispatchEvent(
                    new CustomEvent("fracture-mod", {
                        bubbles: true,
                        detail: {
                            key: row.dataset.modKey ?? "",
                            modId: Number(row.dataset.modId),
                            side,
                        },
                    }),
                );
            });
        });
    }
}

function renderImplicits(mods: SlotMod[]): string {
    if (mods.length === 0) return "";
    return `
        <section class="pc-mod-group pc-mod-group-implicit">
            <h4><span>Implicits</span><span>${mods.length}</span></h4>
            <ul class="pc-mod-slots">
                ${mods
                    .map((mod, index) =>
                        renderFilledSlot(mod, "implicit", index),
                    )
                    .join("")}
            </ul>
        </section>`;
}

function renderSlotGroup(
    title: string,
    side: "prefix" | "suffix",
    slots: Array<SlotMod | undefined>,
    occupied: number,
): string {
    if (slots.length === 0) {
        // Normal items have 0 slots — render nothing.
        return "";
    }
    const rows: string[] = [];
    for (let i = 0; i < slots.length; i += 1) {
        const mod = slots[i];
        rows.push(
            mod
                ? renderFilledSlot(mod, side, i)
                : renderEmptySlot(side, i),
        );
    }
    return `
        <section class="pc-mod-group pc-mod-group-${side}">
            <h4><span>${title}</span><span>${occupied}/${slots.length}</span></h4>
            <ul class="pc-mod-slots">${rows.join("")}</ul>
        </section>`;
}

function renderFilledSlot(
    mod: SlotMod,
    side: "prefix" | "suffix" | "implicit",
    index: number,
): string {
    const lines = mod.textLines.length > 0 ? mod.textLines : [mod.key];
    const tagBits = visibleModTags(mod.classificationTags).map(formatTag);
    const sideCode = side === "implicit" ? "I" : side === "prefix" ? "P" : "S";
    const slotCode = `${sideCode}${index + 1}`;
    const tierLabel = mod.tierIndex
        ? `T${mod.tierIndex}`
        : mod.crafted
          ? "C"
          : "—";
    const stateBits = [
        mod.fractured
            ? '<span class="pc-mod-state is-fractured">Fractured</span>'
            : "",
        mod.crafted
            ? '<span class="pc-mod-state is-crafted">Crafted</span>'
            : "",
    ].filter(Boolean);
    const fractureAttributes =
        side === "implicit"
            ? ""
            : ` data-side="${side}" data-mod-id="${mod.sessionModId}" data-mod-key="${escapeAttribute(mod.key)}" data-fractured="${mod.fractured ? "true" : "false"}" title="${mod.fractured ? "Fractured modifier" : "Right-click to mark this modifier as fractured"}"`;
    return `
        <li class="pc-mod-slot pc-mod-${side} is-filled ${mod.crafted ? "is-crafted" : ""} ${
        mod.fractured ? "is-fractured" : ""
    }"${fractureAttributes}>
            <span class="pc-mod-slot-rail" aria-hidden="true"></span>
            <span class="pc-mod-slot-meta">
                <strong>${slotCode}</strong>
                <span>${tierLabel}</span>
            </span>
            <div class="pc-mod-slot-content">
                <div class="pc-mod-slot-lines">
                    ${lines
                        .map(
                            (line) =>
                                `<div class="pc-mod-slot-line">${escapeHtml(line)}</div>`,
                        )
                        .join("")}
                </div>
                ${
                    tagBits.length > 0 || stateBits.length > 0
                        ? `<div class="pc-mod-slot-tags">${tagBits
                              .map((tag) => `<span>${escapeHtml(tag)}</span>`)
                              .join("")}${stateBits.join("")}</div>`
                        : ""
                }
            </div>
        </li>`;
}

function renderEmptySlot(side: "prefix" | "suffix", index: number): string {
    const code = `${side === "prefix" ? "P" : "S"}${index + 1}`;
    const label = side === "prefix" ? "Open prefix" : "Open suffix";
    return `
        <li class="pc-mod-slot pc-mod-${side} is-empty">
            <span class="pc-mod-slot-rail" aria-hidden="true"></span>
            <span class="pc-mod-slot-meta"><strong>${code}</strong></span>
            <div class="pc-mod-slot-content">
                <div class="pc-mod-slot-line pc-mod-slot-empty">${label}</div>
            </div>
        </li>`;
}

function escapeHtml(text: string): string {
    return text
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;");
}

function escapeAttribute(text: string): string {
    return escapeHtml(text).replace(/"/g, "&quot;");
}

function formatTag(tag: string): string {
    return tag
        .replace(/_/g, " ")
        .replace(/\b\w/g, (character) => character.toUpperCase());
}

customElements.define("pc-mod-list", PcModList);
