/*
 * pc-mod-list — presentational view of an item: fixed prefix / suffix slot grid
 * with in-game-style text. Empty slots stay in place as the item fills so the
 * stepper-style emulator flow doesn't reflow on every craft.
 *
 * Implicits render above explicits as a free-form list (their count is fixed
 * per base; we just show whatever the engine returned).
 */

export interface SlotMod {
    sessionModId: number;
    key: string;
    displayName: string;
    tierIndex: number;
    textLines: string[];
    classificationTags: string[];
    fractured: boolean;
    crafted: boolean;
}

export interface ModListModel {
    rarity: string;
    influences: string[];
    implicits: SlotMod[];
    prefixes: SlotMod[];
    suffixes: SlotMod[];
    maxPrefix: number;
    maxSuffix: number;
}

export class PcModList extends HTMLElement {
    setModel(model: ModListModel): void {
        const explicitCount = model.prefixes.length + model.suffixes.length;
        this.innerHTML = `
            <div class="pc-mod-list pc-item-rarity-${model.rarity}">
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
                ${renderImplicits(model.implicits)}
                ${renderSlotGroup("Prefixes", "prefix", model.prefixes, model.maxPrefix)}
                ${renderSlotGroup("Suffixes", "suffix", model.suffixes, model.maxSuffix)}
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
        <div class="pc-mod-group">
            <h4>Implicits</h4>
            <ul class="pc-mod-slots">
                ${mods.map((mod) => renderFilledSlot(mod, "implicit")).join("")}
            </ul>
        </div>`;
}

function renderSlotGroup(
    title: string,
    side: "prefix" | "suffix",
    mods: SlotMod[],
    capacity: number,
): string {
    if (capacity === 0) {
        // Normal items have 0 slots — render nothing.
        return "";
    }
    const slots: string[] = [];
    for (let i = 0; i < capacity; i += 1) {
        const mod = mods[i];
        slots.push(mod ? renderFilledSlot(mod, side) : renderEmptySlot(side));
    }
    return `
        <div class="pc-mod-group">
            <h4>${title}</h4>
            <ul class="pc-mod-slots">${slots.join("")}</ul>
        </div>`;
}

function renderFilledSlot(mod: SlotMod, side: "prefix" | "suffix" | "implicit"): string {
    const lines = mod.textLines.length > 0 ? mod.textLines : [mod.key];
    const tagBits = mod.classificationTags.map(formatTag);
    if (mod.crafted) tagBits.push("Crafted");
    if (mod.fractured) tagBits.push("Fractured");
    const sideTitle =
        side === "implicit" ? "Implicit" : side === "prefix" ? "Prefix" : "Suffix";
    const helper =
        side === "implicit"
            ? `${sideTitle} Modifier`
            : `${sideTitle} Modifier "${escapeHtml(mod.displayName || mod.key)}" ${
                  mod.tierIndex ? `(Tier: ${mod.tierIndex})` : ""
              }`;
    const fractureAttributes =
        side === "implicit"
            ? ""
            : ` data-side="${side}" data-mod-id="${mod.sessionModId}" data-mod-key="${escapeAttribute(mod.key)}" data-fractured="${mod.fractured ? "true" : "false"}" title="${mod.fractured ? "Fractured modifier" : "Right-click to mark this modifier as fractured"}"`;
    return `
        <li class="pc-mod-slot pc-mod-${side} is-filled ${mod.crafted ? "is-crafted" : ""} ${
        mod.fractured ? "is-fractured" : ""
    }"${fractureAttributes}>
            <div class="pc-mod-slot-helper">${helper}</div>
            <div class="pc-mod-slot-lines">
                ${lines
                    .map(
                        (line) =>
                            `<div class="pc-mod-slot-line">${escapeHtml(line)}</div>`,
                    )
                    .join("")}
            </div>
            ${
                tagBits.length > 0
                    ? `<div class="pc-mod-slot-tags">${tagBits
                          .map((tag) => `<span>${escapeHtml(tag)}</span>`)
                          .join("")}</div>`
                    : ""
            }
        </li>`;
}

function renderEmptySlot(side: "prefix" | "suffix"): string {
    const label = side === "prefix" ? "Empty Prefix" : "Empty Suffix";
    return `
        <li class="pc-mod-slot pc-mod-${side} is-empty">
            <div class="pc-mod-slot-line pc-mod-slot-empty">${label}</div>
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
