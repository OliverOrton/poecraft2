/*
 * pc-mod-list — presentational concrete-item and target-item slot ledgers.
 * Both modes share the same fixed prefix / suffix frame. Concrete mode shows
 * rolled modifiers and emits fracture actions; target mode shows authored
 * family thresholds and emits stable goal-edit actions.
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

export interface ConcreteModListModel {
    kind: "concrete";
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
    kind: "target";
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

export class PcModList extends HTMLElement {
    private concretePrefixSlotIds: Array<number | undefined> = [];
    private concreteSuffixSlotIds: Array<number | undefined> = [];
    private targetPrefixSlotIds: Array<string | undefined> = [];
    private targetSuffixSlotIds: Array<string | undefined> = [];

    setModel(model: PcModListModel): void {
        if (model.kind === "target") {
            this.renderTarget(model);
            return;
        }
        this.renderConcrete(model);
    }

    private renderConcrete(model: ConcreteModListModel): void {
        const explicitCount = model.prefixes.length + model.suffixes.length;
        const prefixLayout = placeStableSlots(
            model.prefixes,
            model.maxPrefix,
            this.concretePrefixSlotIds,
            (mod) => mod.sessionModId,
        );
        const suffixLayout = placeStableSlots(
            model.suffixes,
            model.maxSuffix,
            this.concreteSuffixSlotIds,
            (mod) => mod.sessionModId,
        );
        this.concretePrefixSlotIds = prefixLayout.ids;
        this.concreteSuffixSlotIds = suffixLayout.ids;
        this.innerHTML = `
            <div class="pc-mod-list pc-item-rarity-${model.rarity}" data-mode="concrete">
                ${renderHeader(model, `${explicitCount} explicit · ${model.prefixes.length}P / ${model.suffixes.length}S`)}
                ${renderImplicits(model.implicits)}
                <div class="pc-mod-explicit-ledger">
                    ${renderConcreteSlotGroup("Prefixes", "prefix", prefixLayout.slots, model.prefixes.length)}
                    ${renderConcreteSlotGroup("Suffixes", "suffix", suffixLayout.slots, model.suffixes.length)}
                </div>
            </div>`;
        this.bindConcreteEvents();
    }

    private renderTarget(model: TargetModListModel): void {
        const prefixLayout = placeStableSlots(
            model.prefixes,
            Math.max(model.maxPrefix, model.prefixes.length),
            this.targetPrefixSlotIds,
            (mod) => mod.familyModKey,
        );
        const suffixLayout = placeStableSlots(
            model.suffixes,
            Math.max(model.maxSuffix, model.suffixes.length),
            this.targetSuffixSlotIds,
            (mod) => mod.familyModKey,
        );
        this.targetPrefixSlotIds = prefixLayout.ids;
        this.targetSuffixSlotIds = suffixLayout.ids;
        const requirementCount =
            model.prefixes.length +
            model.suffixes.length +
            model.otherRequirements.length;
        this.innerHTML = `
            <div class="pc-mod-list pc-mod-list-target pc-item-rarity-${model.rarity}" data-mode="target">
                ${renderHeader(model, `${requirementCount} requirements · ${model.prefixes.length}P / ${model.suffixes.length}S`, true)}
                <div class="pc-mod-explicit-ledger">
                    ${renderTargetSlotGroup("Prefixes", "prefix", prefixLayout.slots, model.prefixes.length, model.maxPrefix)}
                    ${renderTargetSlotGroup("Suffixes", "suffix", suffixLayout.slots, model.suffixes.length, model.maxSuffix)}
                    ${renderOtherRequirements(model.otherRequirements)}
                </div>
            </div>`;
        this.bindTargetEvents();
    }

    private bindConcreteEvents(): void {
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

    private bindTargetEvents(): void {
        this.querySelectorAll<HTMLSelectElement>(
            "select[data-target-family-key]",
        ).forEach((select) => {
            select.addEventListener("change", () => {
                this.dispatchEvent(
                    new CustomEvent("target-tier-change", {
                        bubbles: true,
                        detail: {
                            familyModKey: select.dataset.targetFamilyKey ?? "",
                            minTier: Number(select.value),
                        },
                    }),
                );
            });
        });
        this.querySelectorAll<HTMLButtonElement>(
            "button[data-target-family-key], button[data-target-slot-index]",
        ).forEach((button) => {
            button.addEventListener("click", () => {
                const familyModKey = button.dataset.targetFamilyKey;
                const slotIndex = button.dataset.targetSlotIndex;
                this.dispatchEvent(
                    new CustomEvent("target-remove", {
                        bubbles: true,
                        detail: familyModKey
                            ? { familyModKey }
                            : { slotIndex: Number(slotIndex) },
                    }),
                );
            });
        });
    }
}

function renderHeader(
    model: Pick<PcModListModel, "baseName" | "itemLevel" | "rarity"> &
        Partial<Pick<ConcreteModListModel, "influences">>,
    countLabel: string,
    target = false,
): string {
    const title = model.baseName
        ? `<div class="pc-item-title-line">
                <strong>${escapeHtml(model.baseName)}</strong>
                ${model.itemLevel ? `<span>iLvl ${model.itemLevel}</span>` : ""}
            </div>`
        : "";
    const influences = model.influences ?? [];
    return `
        <header class="pc-item-card-header">
            ${title}
            <div class="pc-mod-list-header">
                <span class="pc-item-heading">
                    <span class="pc-rarity pc-rarity-${model.rarity}">${escapeHtml(model.rarity)}</span>
                    ${target ? '<span class="pc-item-target-badge">TARGET</span>' : ""}
                    ${
                        influences.length
                            ? `<span class="pc-item-influences">${influences
                                  .map(
                                      (influence) =>
                                          `<span class="pc-item-influence">${escapeHtml(influence)}</span>`,
                                  )
                                  .join("")}</span>`
                            : ""
                    }
                </span>
                <span class="pc-mod-count">${escapeHtml(countLabel)}</span>
            </div>
        </header>`;
}

function renderImplicits(mods: SlotMod[]): string {
    if (mods.length === 0) return "";
    return `
        <section class="pc-mod-group pc-mod-group-implicit">
            <h4><span>Implicits</span><span>${mods.length}</span></h4>
            <ul class="pc-mod-slots">
                ${mods
                    .map((mod, index) =>
                        renderConcreteFilledSlot(mod, "implicit", index),
                    )
                    .join("")}
            </ul>
        </section>`;
}

function renderConcreteSlotGroup(
    title: string,
    side: "prefix" | "suffix",
    slots: Array<SlotMod | undefined>,
    occupied: number,
): string {
    if (slots.length === 0) return "";
    return `
        <section class="pc-mod-group pc-mod-group-${side}">
            <h4><span>${title}</span><span>${occupied}/${slots.length}</span></h4>
            <ul class="pc-mod-slots">${slots
                .map((mod, index) =>
                    mod
                        ? renderConcreteFilledSlot(mod, side, index)
                        : renderEmptySlot(side, index, false),
                )
                .join("")}</ul>
        </section>`;
}

function renderConcreteFilledSlot(
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

function renderTargetSlotGroup(
    title: string,
    side: "prefix" | "suffix",
    slots: Array<TargetSlotMod | undefined>,
    occupied: number,
    capacity: number,
): string {
    if (slots.length === 0) return "";
    return `
        <section class="pc-mod-group pc-mod-group-${side}">
            <h4><span>${title}</span><span>${occupied}/${capacity}</span></h4>
            <ul class="pc-mod-slots">${slots
                .map((mod, index) =>
                    mod
                        ? renderTargetFilledSlot(mod, side, index)
                        : renderEmptySlot(side, index, true),
                )
                .join("")}</ul>
        </section>`;
}

function renderTargetFilledSlot(
    mod: TargetSlotMod,
    side: "prefix" | "suffix",
    index: number,
): string {
    const slotCode = `${side === "prefix" ? "P" : "S"}${index + 1}`;
    const tierCode = mod.minTier > 0 ? `T${mod.minTier}` : "ANY";
    const tags = visibleModTags(mod.classificationTags).map(formatTag);
    if (mod.sourceLabel && !tags.includes(mod.sourceLabel)) {
        tags.push(mod.sourceLabel);
    }
    return `
        <li class="pc-mod-slot pc-mod-${side} pc-mod-target-slot is-filled" data-target-family="${escapeAttribute(mod.familyModKey)}">
            <span class="pc-mod-slot-rail" aria-hidden="true"></span>
            <span class="pc-mod-slot-meta">
                <strong>${slotCode}</strong>
                <span>${tierCode}</span>
            </span>
            <div class="pc-mod-slot-content">
                <div class="pc-mod-slot-lines">
                    ${mod.textLines
                        .map(
                            (line) =>
                                `<div class="pc-mod-slot-line">${escapeHtml(line)}</div>`,
                        )
                        .join("")}
                </div>
                ${
                    tags.length
                        ? `<div class="pc-mod-slot-tags">${tags
                              .map((tag) => `<span>${escapeHtml(tag)}</span>`)
                              .join("")}</div>`
                        : ""
                }
                <div class="pc-mod-target-actions">
                    <select data-target-family-key="${escapeAttribute(mod.familyModKey)}" aria-label="Minimum tier for ${escapeAttribute(mod.textLines[0] ?? mod.familyModKey)}">
                        <option value="0"${mod.minTier === 0 ? " selected" : ""}>Any tier</option>
                        ${mod.tiers
                            .map(
                                (tier) =>
                                    `<option value="${tier.tier}"${tier.tier === mod.minTier ? " selected" : ""}>T${tier.tier} or better</option>`,
                            )
                            .join("")}
                    </select>
                    ${mod.probabilityLabel ? `<span class="pc-mod-target-probability">${escapeHtml(mod.probabilityLabel)}</span>` : ""}
                    <button type="button" class="pc-icon-button pc-mod-target-remove" data-target-family-key="${escapeAttribute(mod.familyModKey)}" aria-label="Remove requirement">×</button>
                </div>
            </div>
        </li>`;
}

function renderOtherRequirements(
    requirements: TargetOtherRequirement[],
): string {
    if (requirements.length === 0) return "";
    return `
        <section class="pc-mod-group pc-mod-group-other">
            <h4><span>Other requirements</span><span>${requirements.length}</span></h4>
            <ul class="pc-mod-other-requirements">
                ${requirements
                    .map(
                        (requirement) => `
                            <li>
                                <div>
                                    <strong>${escapeHtml(requirement.label)}</strong>
                                    <span>${requirement.minTier > 0 ? `T${requirement.minTier} or better` : "Any matching tier"}</span>
                                </div>
                                ${requirement.probabilityLabel ? `<span class="pc-mod-target-probability">${escapeHtml(requirement.probabilityLabel)}</span>` : ""}
                                <button type="button" class="pc-icon-button pc-mod-target-remove" data-target-slot-index="${requirement.slotIndex}" aria-label="Remove requirement">×</button>
                            </li>`,
                    )
                    .join("")}
            </ul>
        </section>`;
}

function renderEmptySlot(
    side: "prefix" | "suffix",
    index: number,
    target: boolean,
): string {
    const code = `${side === "prefix" ? "P" : "S"}${index + 1}`;
    const label = target
        ? side === "prefix"
            ? "No prefix requirement"
            : "No suffix requirement"
        : side === "prefix"
          ? "Open prefix"
          : "Open suffix";
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
