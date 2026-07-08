/*
 * pc-calculator — the Calculator workspace document (solver S6). Given one
 * item, one goal split, and one action, it shows the calculation engine's
 * exact outcome distribution: per-goal-slot hit odds, outcome classes over
 * goal-relevant features, and the action's expected cost against the
 * workspace price table. The engine is the only rules authority; this tab
 * renders pc_calc_action_outcomes results verbatim.
 *
 * The selection surfaces mirror the Emulator: goal mods are picked from the
 * same modifier-pool browser (pc-mod-pool in select-goal mode — clicking a
 * tier requires that tier or better) and the action comes from the same
 * craft-panel band, except the buttons select a registry action id instead
 * of applying a craft.
 *
 * Document lifecycle mirrors pc-emulator, minus Stash saves: a Calculator is
 * never a saved resource, so the IndexedDB draft exists purely for reload
 * recovery and the document is never dirty.
 */

import { getEngine } from "../engine-service";
import { EngineClient } from "../engine-client";
import {
    BaseInfo,
    CalcOutcome,
    CalcResult,
    Catalog,
    EngineError,
    ModInfo,
    SolverActionInfo,
    SolverGoal,
} from "../engine-protocol";
import {
    CalculatorDraftRecord,
    CalculatorGoalSlot,
    getCalculatorDraft,
    putCalculatorDraft,
} from "../workspace/persistence";
import { workspace } from "../workspace/registry";
import { getPrice, onPricesChange, setPrice } from "../workspace/prices";
import {
    buildModifierKeyIndex,
    buildModifierOptions,
    titleCase,
} from "../modifier-options";
import {
    craftActionLabel,
    groupEssences,
    resistanceEntries,
} from "../craft-choices";
import type { ModifierFamilyOption } from "./pc-condition-editor";
import { PcBasePicker, BasePickerSelection } from "./pc-base-picker";
import { PcModList, SlotMod } from "./pc-mod-list";
import { PcModPool } from "./pc-mod-pool";
import { ComboOption, PcCombobox } from "./pc-combobox";
import "./pc-base-picker";
import "./pc-mod-list";
import "./pc-mod-pool";
import "./pc-combobox";

const DEFAULT_BASE = "Metadata/Items/Armours/BodyArmours/BodyInt17";
const MAX_GOAL_SLOTS = 8;
const MAX_FOSSILS = 4;
const MAX_OUTCOME_ROWS = 40;
const REACH_KIND_CRAFTED = 2;

/** Same craft panels as the Emulator; buttons select instead of apply. */
const BASIC_ACTIONS = [
    "transmute",
    "augment",
    "alteration",
    "regal",
    "alchemy",
    "chaos",
    "exalt",
    "annul",
    "scour",
    "fracture",
];

type CraftPanel =
    | "basic"
    | "essence"
    | "harvest"
    | "fossil"
    | "eldritch"
    | "influenced"
    | "veiled";

const CRAFT_PANELS: Array<[CraftPanel, string]> = [
    ["basic", "Basic currency"],
    ["essence", "Essences"],
    ["harvest", "Harvest"],
    ["fossil", "Fossil"],
    ["eldritch", "Eldritch"],
    ["influenced", "Influenced"],
    ["veiled", "Veiled"],
];

/** Abstract mechanic flags, in solver_internal.hpp bit order. */
const FLAG_LABELS = [
    "corrupted",
    "mirrored",
    "split",
    "synthesised",
    "fractured",
    "crafted",
    "veiled",
    "multimod",
    "no attack",
    "no caster",
    "prefixes locked",
    "suffixes locked",
    "influenced",
    "eldritch",
];

const RARITY_NAMES = ["Normal", "Magic", "Rare"];

export class PcCalculator extends HTMLElement {
    private client!: EngineClient;
    private dataId = 0;
    private bases: BaseInfo[] = [];
    private catalog: Catalog | null = null;

    private docId = "";
    private base = DEFAULT_BASE;
    private itemLevel = 86;
    private freshRarity = "rare";

    private session = 0;
    private context = 0;
    private item = 0;
    private solver = 0;
    private modCache: ModInfo[] = [];
    private familyLabels = new Map<number, string>();
    private modifierOptions: ModifierFamilyOption[] = [];
    private modKeyToFamily = new Map<string, string>();
    private pickerActions: SolverActionInfo[] = [];

    private goalRarity: "normal" | "magic" | "rare" = "rare";
    private slots: CalculatorGoalSlot[] = [];
    private actionId = "";
    private fossilKeys: string[] = [];
    private activeCraftPanel: CraftPanel = "basic";
    private mechanicValues = new Map<string, string>();
    private calc: CalcResult | null = null;
    private calcError = "";

    private busy = true;
    private pickerOpen = false;
    private hasBase = false;
    private disposed = false;
    private connectedOnce = false;
    private currentWork: Promise<void> | null = null;
    private unsubscribePrices: (() => void) | null = null;

    async connectedCallback(): Promise<void> {
        if (this.connectedOnce) {
            return;
        }
        this.connectedOnce = true;
        this.docId = this.getAttribute("doc-id") ?? `doc-${crypto.randomUUID()}`;
        this.renderShell();
        this.setBusy(true);
        workspace().registerDocument(this.docId, {
            save: async () => true, // nothing to save; never dirty
            dispose: () => this.disposeEngine(),
        });
        this.unsubscribePrices = onPricesChange(() => this.renderResults());
        this.setStatus("Loading engine…");
        const engine = await getEngine();
        if (this.disposed) {
            return;
        }
        this.client = engine.client;
        this.dataId = engine.dataId;
        this.catalog = await this.client.catalog(this.dataId);
        this.bases = (await this.client.listBases(this.dataId)).filter(
            (base) => base.support === 0,
        );
        if (this.disposed) {
            return;
        }

        const draft = await getCalculatorDraft(this.docId);
        if (draft) {
            this.base = draft.base;
            this.itemLevel = draft.itemLevel;
            this.goalRarity = draft.goalRarity;
            this.slots = draft.slots;
            this.actionId = draft.actionId;
            this.fossilKeys = draft.fossilKeys;
            this.activeCraftPanel = panelForAction(this.actionId);
        }
        if (!this.bases.some((b) => b.path === this.base)) {
            this.base = this.bases[0]?.path ?? this.base;
        }
        this.hasBase = Boolean(draft?.state);

        if (!this.hasBase) {
            this.pickerOpen = true;
            this.renderShell();
            this.setBusy(false);
            this.setStatus("");
            return;
        }

        await this.openSession();
        if (this.disposed) {
            return;
        }
        const item = draft?.state
            ? await this.client.importItem(draft.state)
            : await this.client.createItem(this.session, {
                rarity: this.freshRarity,
                withImplicits: true,
            });
        if (this.disposed) {
            await this.client.closeItem(item);
            return;
        }
        this.item = item;
        await this.openSolver();
        await this.refresh();
        await this.recalc();
        await this.persist();
        this.setBusy(false);
        this.setStatus("");
    }

    disconnectedCallback(): void {
        // Dockview fires this during drag-between-groups; state lives in the
        // draft, and engine handles are torn down via the dispose handler.
    }

    // --- engine lifecycle ---------------------------------------------------

    private async openSession(): Promise<void> {
        if (this.disposed) {
            return;
        }
        await this.closeSolverHandle();
        if (this.session) {
            await this.client.closeContext(this.context);
            await this.client.closeSession(this.session);
            this.context = 0;
            this.session = 0;
        }
        this.modCache = [];
        this.familyLabels.clear();
        this.modifierOptions = [];
        this.modKeyToFamily.clear();
        this.pickerActions = [];
        const session = await this.client.createSession(
            this.dataId,
            this.base,
            this.itemLevel,
        );
        if (this.disposed) {
            await this.client.closeSession(session);
            return;
        }
        const context = await this.client.createContext(session, 0);
        if (this.disposed) {
            await this.client.closeContext(context);
            await this.client.closeSession(session);
            return;
        }
        this.session = session;
        this.context = context;
        await this.cacheAllMods();
    }

    private async cacheAllMods(): Promise<void> {
        const count = await this.client.modCount(this.session);
        const cache: ModInfo[] = new Array(count);
        await Promise.all(
            Array.from({ length: count }, async (_, id) => {
                cache[id] = await this.client.modInfo(this.session, id);
            }),
        );
        if (this.disposed) {
            return;
        }
        this.modCache = cache;
        const labels = new Map<number, string>();
        for (const info of cache
            .slice()
            .sort((a, b) => b.required_level - a.required_level)) {
            if (labels.has(info.family_id)) continue;
            labels.set(
                info.family_id,
                info.text_lines.join(" / ") || info.key,
            );
        }
        this.familyLabels = labels;
        this.modifierOptions = this.catalog
            ? buildModifierOptions(cache, this.catalog)
            : [];
        this.modKeyToFamily = buildModifierKeyIndex(cache);
        // Drop goal slots that do not exist in the new session.
        this.slots = this.slots.filter((slot) =>
            slot.group
                ? this.groupIdByKey(slot.group) !== undefined
                : this.modifierOptions.some(
                      (option) => option.value === slot.familyModKey,
                  ),
        );
    }

    /** (Re)open the solver for the current goal. Requires >= 1 slot. */
    private async openSolver(): Promise<void> {
        await this.closeSolverHandle();
        this.calc = null;
        this.calcError = "";
        if (this.disposed || this.slots.length === 0) {
            return;
        }
        const goal: SolverGoal = {
            version: "v1",
            rarity: this.goalRarity,
            slots: this.slots.map((slot) =>
                slot.group
                    ? { group: slot.group, min_tier: slot.minTier }
                    : {
                          family_mod_key: slot.familyModKey ?? "",
                          min_tier: slot.minTier,
                      },
            ),
        };
        try {
            // No `actions` subset: the full registry stays available so any
            // selected action (including fossil loadouts) can be calculated.
            this.solver = await this.client.openSolver(this.session, goal);
        } catch (error) {
            this.calcError =
                error instanceof Error ? error.message : String(error);
            return;
        }
        if (this.disposed) {
            return;
        }
        if (this.pickerActions.length === 0) {
            // The registry depends only on the session, so this list survives
            // goal edits; fetched once per session for cost-key lookups.
            this.pickerActions = await this.client.solverActions(this.solver, {
                omitFossilCombos: true,
            });
        }
    }

    private async closeSolverHandle(): Promise<void> {
        if (this.solver) {
            const solver = this.solver;
            this.solver = 0;
            await this.client.closeSolver(solver);
        }
    }

    private async disposeEngine(): Promise<void> {
        if (this.disposed) {
            return;
        }
        this.disposed = true;
        this.unsubscribePrices?.();
        this.unsubscribePrices = null;
        workspace().unregisterDocument(this.docId);
        if (this.currentWork) {
            try {
                await this.currentWork;
            } catch {
                // The operation's guard already surfaced the error.
            }
        }
        const item = this.item;
        const solver = this.solver;
        const context = this.context;
        const session = this.session;
        this.item = 0;
        this.solver = 0;
        this.context = 0;
        this.session = 0;
        if (solver && this.client) {
            await this.client.closeSolver(solver);
        }
        if (item && this.client) {
            await this.client.closeItem(item);
        }
        if (context && this.client) {
            await this.client.closeContext(context);
        }
        if (session && this.client) {
            await this.client.closeSession(session);
        }
    }

    // --- state changes ------------------------------------------------------

    private async newItem(): Promise<void> {
        if (this.item) {
            await this.client.closeItem(this.item);
        }
        this.item = await this.client.createItem(this.session, {
            rarity: this.freshRarity,
            withImplicits: true,
        });
        await this.refresh();
        await this.recalc();
        await this.persist();
    }

    private async applyPickerSelection(sel: BasePickerSelection): Promise<void> {
        this.base = sel.base;
        this.itemLevel = sel.itemLevel;
        this.hasBase = true;
        this.pickerOpen = false;
        this.renderShell();
        await this.openSession();
        if (this.item) {
            await this.client.closeItem(this.item);
            this.item = 0;
        }
        this.item = await this.client.createItem(this.session, {
            rarity: this.freshRarity,
            withImplicits: true,
        });
        await this.openSolver();
        await this.refresh();
        await this.recalc();
        await this.persist();
    }

    private async goalChanged(): Promise<void> {
        await this.openSolver();
        this.renderGoal();
        await this.recalc();
        await this.persist();
    }

    private async actionChanged(): Promise<void> {
        this.renderActionPanels();
        await this.recalc();
        await this.persist();
    }

    /** Select a registry action id from the craft panels. */
    private selectAction(id: string): void {
        this.actionId = id;
        if (!id.startsWith("fossil:")) {
            this.fossilKeys = [];
        }
        void this.guard(() => this.actionChanged());
    }

    /** "fossil:<a>+<b>" with sorted keys, matching solver_registry.cpp. */
    private fossilComboId(): string {
        return this.fossilKeys.length
            ? `fossil:${[...this.fossilKeys].sort().join("+")}`
            : "";
    }

    /** Cost keys for the selected action; fossil loadouts mirror the
     * registry's per-fossil + resonator-size vector. */
    private selectedCostKeys(): string[] {
        if (this.actionId.startsWith("fossil:") && this.fossilKeys.length) {
            return [
                ...[...this.fossilKeys].sort().map((key) => `fossil:${key}`),
                `resonator:${this.fossilKeys.length}`,
            ];
        }
        const entry = this.pickerActions.find(
            (action) => action.id === this.actionId,
        );
        return entry?.cost_keys ?? [];
    }

    private async recalc(): Promise<void> {
        this.calc = null;
        this.calcError = "";
        if (this.solver && this.item && this.actionId) {
            try {
                this.calc = await this.client.solverCalc(
                    this.solver,
                    this.item,
                    this.actionId,
                );
            } catch (error) {
                this.calcError =
                    error instanceof EngineError &&
                    error.message.includes("unknown action")
                        ? "This action is not available for this base and item level."
                        : error instanceof Error
                          ? error.message
                          : String(error);
            }
        }
        this.renderResults();
    }

    private async persist(): Promise<void> {
        const draft: CalculatorDraftRecord = {
            docId: this.docId,
            base: this.base,
            itemLevel: this.itemLevel,
            state: this.item ? await this.client.exportItem(this.item) : null,
            goalRarity: this.goalRarity,
            slots: this.slots,
            actionId: this.actionId,
            fossilKeys: this.fossilKeys,
            updatedAt: Date.now(),
        };
        await putCalculatorDraft(draft);
    }

    // --- refresh / render ---------------------------------------------------

    private async refresh(): Promise<void> {
        if (!this.item) {
            return;
        }
        const info = await this.client.itemInfo(this.item, this.session);
        const fracturedP = new Set(info.fractured_prefix_mod_ids as number[]);
        const fracturedS = new Set(info.fractured_suffix_mod_ids as number[]);
        const prefixIds = info.prefix_mod_ids as number[];
        const suffixIds = info.suffix_mod_ids as number[];
        const implicitIds = info.implicit_mod_ids as number[];
        const prefixes = prefixIds.map((id) => this.toSlot(id, fracturedP));
        const suffixes = suffixIds.map((id) => this.toSlot(id, fracturedS));
        const implicits = implicitIds.map((id) => this.toSlot(id, new Set()));
        this.modList.setModel({
            rarity: info.rarity as string,
            influences: [],
            prefixes,
            suffixes,
            implicits,
            maxPrefix: (info.max_prefix as number) ?? prefixes.length,
            maxSuffix: (info.max_suffix as number) ?? suffixes.length,
        });

        // Feed the goal-selection pool exactly like the Emulator feeds its
        // browser: live chaos-pool weights for the active tab.
        const pool =
            this.modPool.getActiveTab() === "implicit"
                ? null
                : await this.client.debugPool(this.context, this.item, {
                      action: { type: "chaos" },
                  });
        const poolWeights = new Map<number, number>();
        if (pool) {
            for (const entry of pool.entries) {
                if (!entry.accepted) continue;
                poolWeights.set(entry.session_mod_id, entry.final_weight);
            }
        }
        const groupOnItem = new Set<number>();
        for (const id of [...prefixIds, ...suffixIds]) {
            const cached = this.modCache[id];
            if (cached) groupOnItem.add(cached.primary_group_id);
        }
        this.modPool.setModel({
            mods: this.modCache,
            item: {
                rarity: info.rarity as string,
                prefixOnItem: new Set(prefixIds),
                suffixOnItem: new Set(suffixIds),
                implicitOnItem: new Set(implicitIds),
                fracturedPrefixOnItem: fracturedP,
                fracturedSuffixOnItem: fracturedS,
                groupOnItem,
                maxPrefix: (info.max_prefix as number) ?? prefixes.length,
                maxSuffix: (info.max_suffix as number) ?? suffixes.length,
            },
            pool,
            poolWeights,
        });
        this.renderGoal();
        this.renderActionPanels();
    }

    private toSlot(id: number, fractured: Set<number>): SlotMod {
        const info = this.modCache[id];
        if (!info) {
            return {
                sessionModId: id,
                key: String(id),
                displayName: "",
                tierIndex: 0,
                textLines: [],
                classificationTags: [],
                fractured: fractured.has(id),
                crafted: false,
            };
        }
        return {
            sessionModId: id,
            key: info.key,
            displayName:
                this.familyLabels.get(info.family_id) ||
                info.text_lines.join(" / ") ||
                info.key,
            tierIndex: info.family_tier_index,
            textLines: info.text_lines,
            classificationTags: info.classification_tags,
            fractured: fractured.has(id),
            crafted: info.reach_kind === REACH_KIND_CRAFTED,
        };
    }

    private groupIdByKey(key: string): number | undefined {
        const index = this.catalog?.groupKeyById.indexOf(key) ?? -1;
        return index >= 0 ? index : undefined;
    }

    private slotLabel(slot: CalculatorGoalSlot): string {
        if (slot.group) {
            const id = this.groupIdByKey(slot.group);
            return (
                (id !== undefined && this.catalog?.groupNameById[id]) ||
                slot.group
            );
        }
        const option = this.modifierOptions.find(
            (entry) => entry.value === slot.familyModKey,
        );
        if (!option) {
            return slot.familyModKey ?? "?";
        }
        const source = option.sourceLabel ? ` · ${option.sourceLabel}` : "";
        return `${option.label}${source}`;
    }

    /** A pool tier was clicked: require that family at that tier or better. */
    private addGoalFromPool(modKey: string): void {
        const familyKey = this.modKeyToFamily.get(modKey);
        const info = this.modCache.find((mod) => mod.key === modKey);
        if (!familyKey || !info) {
            return;
        }
        const minTier = info.family_tier_index || 0;
        const existing = this.slots.find(
            (slot) => slot.familyModKey === familyKey,
        );
        if (existing) {
            existing.minTier = minTier;
        } else if (this.slots.length < MAX_GOAL_SLOTS) {
            this.slots = [...this.slots, { familyModKey: familyKey, minTier }];
        } else {
            this.setStatus(`Goals are limited to ${MAX_GOAL_SLOTS} modifiers.`);
            return;
        }
        void this.guard(() => this.goalChanged());
    }

    private renderGoal(): void {
        const list = this.querySelector(".pc-calc-slots");
        if (!list) return;
        if (this.slots.length === 0) {
            list.innerHTML =
                '<li class="pc-empty">Click modifiers in the pool to define the goal.</li>';
        } else {
            list.innerHTML = this.slots
                .map((slot, index) => {
                    const option = slot.familyModKey
                        ? this.modifierOptions.find(
                              (entry) => entry.value === slot.familyModKey,
                          )
                        : undefined;
                    const tierChoices = option
                        ? option.tiers
                              .map(
                                  (tier) =>
                                      `<option value="${tier.tier}" ${tier.tier === slot.minTier ? "selected" : ""}>T${tier.tier}${tier.tier === 1 ? " (best)" : " or better"}</option>`,
                              )
                              .join("")
                        : "";
                    const sideClass = option
                        ? option.side === "prefix"
                            ? "pc-side-prefix"
                            : "pc-side-suffix"
                        : "pc-side-group";
                    return `<li class="pc-calc-slot">
                        <span class="pc-calc-slot-side ${sideClass}">${
                            option ? (option.side === "prefix" ? "P" : "S") : "G"
                        }</span>
                        <span class="pc-calc-slot-label">${escapeHtml(this.slotLabel(slot))}</span>
                        ${
                            option
                                ? `<select data-slot-tier="${index}">
                                    <option value="0" ${slot.minTier === 0 ? "selected" : ""}>Any tier</option>
                                    ${tierChoices}
                                </select>`
                                : ""
                        }
                        <button data-slot-remove="${index}" title="Remove">&times;</button>
                    </li>`;
                })
                .join("");
        }
        list.querySelectorAll<HTMLButtonElement>("[data-slot-remove]").forEach(
            (button) => {
                button.addEventListener("click", () => {
                    const index = Number(button.dataset.slotRemove);
                    this.slots = this.slots.filter((_, i) => i !== index);
                    void this.guard(() => this.goalChanged());
                });
            },
        );
        list.querySelectorAll<HTMLSelectElement>("[data-slot-tier]").forEach(
            (select) => {
                select.addEventListener("change", () => {
                    const index = Number(select.dataset.slotTier);
                    const slot = this.slots[index];
                    if (slot) {
                        slot.minTier = Number(select.value);
                        void this.guard(() => this.goalChanged());
                    }
                });
            },
        );

        const groupBox = this.querySelector<PcCombobox>(
            '[data-role="group-slot"]',
        );
        if (groupBox && this.catalog) {
            const taken = new Set(
                this.slots.map((slot) => slot.group).filter(Boolean),
            );
            const seen = new Set<string>();
            const options: ComboOption[] = [];
            for (let id = 0; id < this.catalog.groupKeyById.length; id += 1) {
                const key = this.catalog.groupKeyById[id];
                if (!key || taken.has(key) || seen.has(key)) continue;
                seen.add(key);
                options.push({
                    value: key,
                    label: this.catalog.groupNameById[id] || key,
                });
            }
            options.sort((a, b) => a.label.localeCompare(b.label));
            groupBox.setOptions(options);
        }
        const rarity = this.querySelector<HTMLSelectElement>(
            '[data-role="goal-rarity"]',
        );
        if (rarity) {
            rarity.value = this.goalRarity;
        }
    }

    // --- action panels (Emulator craft bar, selecting instead of applying) ---

    private renderActionPanels(): void {
        const host = this.querySelector(".pc-advanced-crafts");
        if (!host || !this.catalog) return;
        host.querySelectorAll<HTMLSelectElement>("select[data-mechanic]").forEach(
            (select) => {
                const name = select.dataset.mechanic;
                if (name) this.mechanicValues.set(name, select.value);
            },
        );
        const selected = (name: string, fallback = "") =>
            this.mechanicValues.get(name) ?? fallback;
        const options = (
            entries: Array<{ key: string; name: string }>,
            selected?: string,
        ) =>
            entries
                .map(
                    (entry) =>
                        `<option value="${escapeHtml(entry.key)}" ${entry.key === selected ? "selected" : ""}>${escapeHtml(entry.name)}</option>`,
                )
                .join("");
        const essenceGroups = groupEssences(this.catalog.essences);
        const selectedEssenceType = essenceGroups.some(
            (group) => group.type === selected("essence-type"),
        )
            ? selected("essence-type")
            : (essenceGroups[0]?.type ?? "");
        const essenceTiers =
            essenceGroups.find((group) => group.type === selectedEssenceType)
                ?.tiers ?? [];
        const selectedEssenceKey = essenceTiers.some(
            (entry) => entry.key === selected("essence-key"),
        )
            ? selected("essence-key")
            : (essenceTiers[0]?.key ?? "");
        const panel = (() => {
            switch (this.activeCraftPanel) {
                case "basic":
                    return `
                        <div class="pc-craft-options">
                            ${BASIC_ACTIONS.map(
                                (type) =>
                                    `<button data-select-action="${type}">${craftActionLabel(type)}</button>`,
                            ).join("")}
                            <button data-select-action="restart">Restart (fresh base)</button>
                        </div>`;
                case "essence":
                    return `
                        <div class="pc-mechanic-row">
                            <label>
                                <span>Type</span>
                                <select data-mechanic="essence-type">${options(
                                    essenceGroups.map((group) => ({
                                        key: group.type,
                                        name: group.type,
                                    })),
                                    selectedEssenceType,
                                )}</select>
                            </label>
                            <label>
                                <span>Tier</span>
                                <select data-mechanic="essence-key">${options(
                                    essenceTiers.map((entry) => ({
                                        key: entry.key,
                                        name: entry.tier,
                                    })),
                                    selectedEssenceKey,
                                )}</select>
                            </label>
                            <button data-derive-action="essence">Use essence</button>
                        </div>`;
                case "harvest":
                    return `
                        <div class="pc-mechanic-row">
                            <select data-mechanic="harvest-tag">${options(
                                this.catalog.harvestTags,
                                selected(
                                    "harvest-tag",
                                    this.catalog.harvestTags[0]?.key,
                                ),
                            )}</select>
                            <button data-derive-action="harvest_reforge">Reforge</button>
                            <button data-derive-action="harvest_augment">Augment</button>
                        </div>
                        <div class="pc-mechanic-row">
                            <select data-mechanic="resist-from">${options(
                                resistanceEntries(),
                                selected("resist-from", "fire"),
                            )}</select>
                            <span>&rarr;</span>
                            <select data-mechanic="resist-to">${options(
                                resistanceEntries(),
                                selected("resist-to", "cold"),
                            )}</select>
                            <button data-derive-action="harvest_resist">Convert resistance</button>
                        </div>`;
                case "fossil":
                    return `
                        <div class="pc-mechanic-row">
                            <select data-mechanic="fossil">${options(
                                this.catalog.fossils,
                                selected("fossil", this.catalog.fossils[0]?.key),
                            )}</select>
                            <button data-fossil-add ${this.fossilKeys.length >= MAX_FOSSILS ? "disabled" : ""}>Add fossil</button>
                        </div>
                        <div class="pc-selected-fossils">
                            ${
                                this.fossilKeys.length
                                    ? this.fossilKeys
                                          .map(
                                              (key, index) =>
                                                  `<span class="pc-chip">${escapeHtml(
                                                      this.catalog?.fossils.find(
                                                          (entry) => entry.key === key,
                                                      )?.name ?? key,
                                                  )}<button data-fossil-remove="${index}" title="Remove">&times;</button></span>`,
                                          )
                                          .join("")
                                    : '<span class="pc-help">Choose up to four fossils; the loadout is the selected action.</span>'
                            }
                        </div>`;
                case "eldritch":
                    return `
                        <div class="pc-mechanic-row">
                            <select data-mechanic="eldritch-tier">${[1, 2, 3, 4]
                                .map(
                                    (tier) =>
                                        `<option value="${tier}" ${String(tier) === selected("eldritch-tier", "1") ? "selected" : ""}>Tier ${tier}</option>`,
                                )
                                .join("")}</select>
                            <button data-derive-action="eldritch_ember">Ember</button>
                            <button data-derive-action="eldritch_ichor">Ichor</button>
                        </div>
                        <div class="pc-craft-options">
                            ${["eldritch_exalt", "eldritch_chaos", "eldritch_annul"]
                                .map(
                                    (type) =>
                                        `<button data-select-action="${type}">${craftActionLabel(type)}</button>`,
                                )
                                .join("")}
                        </div>`;
                case "influenced":
                    return `
                        <div class="pc-mechanic-row">
                            <select data-mechanic="influence">${options(
                                this.catalog.influences,
                                selected(
                                    "influence",
                                    this.catalog.influences[0]?.key,
                                ),
                            )}</select>
                            <button data-derive-action="influence_exalt">Influenced exalt</button>
                        </div>`;
                case "veiled":
                    return `
                        <div class="pc-craft-options">
                            <button data-select-action="veiled_chaos">Veiled Chaos</button>
                            <button data-select-action="veiled_exalt">Veiled Exalt</button>
                            <button data-select-action="unveil">Unveil</button>
                        </div>`;
            }
        })();
        host.innerHTML = `
            <div class="pc-craft-panel-tabs">
                ${CRAFT_PANELS.map(
                    ([key, label]) =>
                        `<button data-craft-panel="${key}" class="${key === this.activeCraftPanel ? "is-active" : ""}">${label}</button>`,
                ).join("")}
                <span class="pc-calc-selected">${
                    this.actionId
                        ? `Selected: ${escapeHtml(this.actionLabel(this.actionId))}`
                        : "No action selected"
                }</span>
            </div>
            <div class="pc-craft-panel-body">${panel}</div>`;

        // Highlight the control that produced the current selection.
        host.querySelectorAll<HTMLButtonElement>("[data-select-action]").forEach(
            (button) => {
                button.classList.toggle(
                    "is-selected",
                    button.dataset.selectAction === this.actionId,
                );
            },
        );
        host.querySelectorAll<HTMLButtonElement>("[data-derive-action]").forEach(
            (button) => {
                const prefix = `${button.dataset.deriveAction}:`;
                button.classList.toggle(
                    "is-selected",
                    this.actionId.startsWith(prefix),
                );
            },
        );

        host.querySelectorAll<HTMLSelectElement>("select[data-mechanic]").forEach(
            (select) => {
                select.addEventListener("change", () => {
                    const name = select.dataset.mechanic;
                    if (!name) return;
                    this.mechanicValues.set(name, select.value);
                    if (name === "essence-type") {
                        this.mechanicValues.delete("essence-key");
                        this.renderActionPanels();
                        return;
                    }
                    // Re-derive a selected action live when its inputs move.
                    const rederive = this.rederivedActionId(name);
                    if (rederive && rederive !== this.actionId) {
                        this.selectAction(rederive);
                    }
                });
            },
        );
        host.querySelectorAll<HTMLButtonElement>("[data-craft-panel]").forEach(
            (button) => {
                button.addEventListener("click", () => {
                    this.activeCraftPanel = button.dataset
                        .craftPanel as CraftPanel;
                    this.renderActionPanels();
                });
            },
        );
        host.querySelectorAll<HTMLButtonElement>("[data-select-action]").forEach(
            (button) => {
                button.addEventListener("click", () => {
                    this.selectAction(button.dataset.selectAction ?? "");
                });
            },
        );
        host.querySelectorAll<HTMLButtonElement>("[data-derive-action]").forEach(
            (button) => {
                button.addEventListener("click", () => {
                    const id = this.derivedActionId(
                        button.dataset.deriveAction ?? "",
                    );
                    if (id) this.selectAction(id);
                });
            },
        );
        host.querySelector<HTMLButtonElement>("[data-fossil-add]")?.addEventListener(
            "click",
            () => {
                const key =
                    host.querySelector<HTMLSelectElement>(
                        '[data-mechanic="fossil"]',
                    )?.value ?? "";
                if (
                    key &&
                    this.fossilKeys.length < MAX_FOSSILS &&
                    !this.fossilKeys.includes(key)
                ) {
                    this.fossilKeys = [...this.fossilKeys, key];
                    this.selectAction(this.fossilComboId());
                }
            },
        );
        host.querySelectorAll<HTMLButtonElement>("[data-fossil-remove]").forEach(
            (button) => {
                button.addEventListener("click", () => {
                    const index = Number(button.dataset.fossilRemove);
                    this.fossilKeys = this.fossilKeys.filter(
                        (_, entryIndex) => entryIndex !== index,
                    );
                    if (this.fossilKeys.length) {
                        this.selectAction(this.fossilComboId());
                    } else if (this.actionId.startsWith("fossil:")) {
                        this.selectAction("");
                    } else {
                        this.renderActionPanels();
                    }
                });
            },
        );
        this.setBusy(this.busy);
    }

    /** Registry action id from the current mechanic selects. */
    private derivedActionId(kind: string): string {
        const value = (name: string) =>
            this.querySelector<HTMLSelectElement>(
                `.pc-advanced-crafts [data-mechanic="${name}"]`,
            )?.value ??
            this.mechanicValues.get(name) ??
            "";
        switch (kind) {
            case "essence":
                return value("essence-key") ? `essence:${value("essence-key")}` : "";
            case "harvest_reforge":
                return `harvest_reforge:${value("harvest-tag")}`;
            case "harvest_augment":
                return `harvest_augment:${value("harvest-tag")}`;
            case "harvest_resist":
                return `harvest_resist:${value("resist-from")}:${value("resist-to")}`;
            case "eldritch_ember":
                return `eldritch_ember:${value("eldritch-tier") || "1"}`;
            case "eldritch_ichor":
                return `eldritch_ichor:${value("eldritch-tier") || "1"}`;
            case "influence_exalt":
                return `influence_exalt:${value("influence")}`;
            default:
                return "";
        }
    }

    /** If the changed select feeds the currently selected action kind,
     * return the updated id so the selection follows the controls. */
    private rederivedActionId(mechanic: string): string {
        const kindsByMechanic: Record<string, string[]> = {
            "essence-key": ["essence"],
            "harvest-tag": ["harvest_reforge", "harvest_augment"],
            "resist-from": ["harvest_resist"],
            "resist-to": ["harvest_resist"],
            "eldritch-tier": ["eldritch_ember", "eldritch_ichor"],
            influence: ["influence_exalt"],
        };
        for (const kind of kindsByMechanic[mechanic] ?? []) {
            if (this.actionId.startsWith(`${kind}:`)) {
                return this.derivedActionId(kind);
            }
        }
        return "";
    }

    /** Human label for a registry action id, resolved through the catalog. */
    private actionLabel(id: string): string {
        const colon = id.indexOf(":");
        const kind = colon < 0 ? id : id.slice(0, colon);
        const rest = colon < 0 ? "" : id.slice(colon + 1);
        switch (kind) {
            case "essence": {
                const name = this.catalog?.essences.find(
                    (entry) => entry.key === rest,
                )?.name;
                return name ?? rest;
            }
            case "fossil":
                return rest
                    .split("+")
                    .map(
                        (key) =>
                            this.catalog?.fossils.find(
                                (entry) => entry.key === key,
                            )?.name ?? key,
                    )
                    .join(" + ");
            case "bench": {
                const name = this.catalog?.bench.find(
                    (entry) => entry.key === rest,
                )?.name;
                return `Bench: ${name ?? rest}`;
            }
            case "harvest_reforge":
                return `Harvest Reforge ${titleCase(rest)}`;
            case "harvest_augment":
                return `Harvest Augment ${titleCase(rest)}`;
            case "harvest_resist": {
                const [from, to] = rest.split(":");
                return `Harvest Resist ${titleCase(from ?? "")} → ${titleCase(to ?? "")}`;
            }
            case "eldritch_ember":
                return `Eldritch Ember T${rest}`;
            case "eldritch_ichor":
                return `Eldritch Ichor T${rest}`;
            case "influence_exalt": {
                const name = this.catalog?.influences.find(
                    (entry) => entry.key === rest,
                )?.name;
                return `${name ?? titleCase(rest)} Exalt`;
            }
            case "restart":
                return "Restart (fresh base)";
            default:
                return craftActionLabel(id);
        }
    }

    private renderResults(): void {
        const host = this.querySelector<HTMLElement>(".pc-calc-output");
        if (!host) return;
        if (this.calcError) {
            host.innerHTML = `<p class="pc-calc-error">${escapeHtml(this.calcError)}</p>`;
            return;
        }
        if (this.slots.length === 0) {
            host.innerHTML =
                '<p class="pc-empty">Define a goal to compute odds against.</p>';
            return;
        }
        if (!this.actionId) {
            host.innerHTML = '<p class="pc-empty">Pick an action.</p>';
            return;
        }
        const calc = this.calc;
        if (!calc) {
            host.innerHTML = '<p class="pc-empty">Calculating…</p>';
            return;
        }
        if (!calc.legal) {
            host.innerHTML =
                '<p class="pc-calc-error">This action is not legal on the current item.</p>';
            return;
        }
        if (!calc.supported) {
            host.innerHTML =
                '<p class="pc-calc-error">No exact evaluator for this action yet.</p>';
            return;
        }
        host.innerHTML = `
            ${this.renderSlotOdds(calc)}
            ${this.renderCost()}
            ${this.renderOutcomes(calc)}`;
        host.querySelectorAll<HTMLInputElement>("[data-price-key]").forEach(
            (input) => {
                input.addEventListener("change", () => {
                    const key = input.dataset.priceKey ?? "";
                    const value = Number(input.value);
                    setPrice(key, input.value === "" ? null : value);
                });
            },
        );
    }

    private renderSlotOdds(calc: CalcResult): string {
        const rows = this.slots
            .map((slot, index) => {
                const p = calc.slot_satisfied[index] ?? 0;
                return `<div class="pc-calc-odd">
                    <span class="pc-calc-odd-label">${escapeHtml(this.slotLabel(slot))}</span>
                    <span class="pc-calc-odd-bar"><span style="width:${(p * 100).toFixed(2)}%"></span></span>
                    <span class="pc-calc-odd-value">${formatProbability(p)}</span>
                </div>`;
            })
            .join("");
        return `<section class="pc-calc-section">
            <h4>Goal modifier odds after this action</h4>
            ${rows}
        </section>`;
    }

    private renderCost(): string {
        const keys = this.selectedCostKeys();
        if (keys.length === 0) {
            return "";
        }
        const counts = new Map<string, number>();
        for (const key of keys) {
            counts.set(key, (counts.get(key) ?? 0) + 1);
        }
        let total = 0;
        let complete = true;
        const rows = Array.from(counts.entries())
            .map(([key, count]) => {
                const price = getPrice(key);
                if (price === undefined) {
                    complete = false;
                } else {
                    total += price * count;
                }
                return `<div class="pc-calc-cost-row">
                    <span class="pc-calc-cost-key">${count > 1 ? `${count} × ` : ""}${escapeHtml(key)}</span>
                    <input type="number" min="0" step="any" data-price-key="${escapeHtml(key)}"
                        value="${price ?? ""}" placeholder="price">
                    <span class="pc-calc-cost-sub">${price !== undefined ? formatNumber(price * count) : "—"}</span>
                </div>`;
            })
            .join("");
        return `<section class="pc-calc-section">
            <h4>Cost per attempt</h4>
            ${rows}
            <div class="pc-calc-cost-total">
                <span>Total</span>
                <span>${complete ? formatNumber(total) : "set prices above"}</span>
            </div>
        </section>`;
    }

    private renderOutcomes(calc: CalcResult): string {
        const sorted = [...calc.outcomes].sort(
            (a, b) => b.probability - a.probability,
        );
        const shown = sorted.slice(0, MAX_OUTCOME_ROWS);
        const restCount = sorted.length - shown.length;
        const restProbability = sorted
            .slice(MAX_OUTCOME_ROWS)
            .reduce((sum, outcome) => sum + outcome.probability, 0);
        const slotHeaders = this.slots
            .map(
                (_, index) =>
                    `<th title="${escapeHtml(this.slotLabel(this.slots[index]))}">G${index + 1}</th>`,
            )
            .join("");
        const rows = shown
            .map(
                (outcome) => `<tr>
                    <td class="pc-calc-p">${formatProbability(outcome.probability)}</td>
                    <td>${RARITY_NAMES[outcome.rarity] ?? outcome.rarity}</td>
                    <td>${outcome.prefixes}P/${outcome.suffixes}S</td>
                    ${this.slots
                        .map((_, index) =>
                            slotStatusCell(outcome, index),
                        )
                        .join("")}
                    <td class="pc-calc-flags">${flagLabels(outcome.flags)}</td>
                </tr>`,
            )
            .join("");
        return `<section class="pc-calc-section">
            <h4>Outcome classes (${calc.outcomes.length})</h4>
            <table class="pc-calc-table">
                <thead><tr>
                    <th>Chance</th><th>Rarity</th><th>Affixes</th>
                    ${slotHeaders}<th>Flags</th>
                </tr></thead>
                <tbody>${rows}</tbody>
            </table>
            ${
                restCount > 0
                    ? `<p class="pc-help">…and ${restCount} more classes totalling ${formatProbability(restProbability)}.</p>`
                    : ""
            }
        </section>`;
    }

    // --- shell / chrome -----------------------------------------------------

    private get modList(): PcModList {
        return this.querySelector("pc-mod-list")!;
    }

    private get modPool(): PcModPool {
        return this.querySelector("pc-mod-pool")!;
    }

    private setStatus(text: string): void {
        const el = this.querySelector(".pc-calc-status");
        if (el) {
            el.textContent = text;
            (el as HTMLElement).hidden = text === "";
        }
    }

    private setBusy(busy: boolean): void {
        this.busy = busy;
        this.querySelectorAll<HTMLButtonElement>(
            "button[data-cmd], button[data-craft-panel], button[data-select-action], button[data-derive-action], button[data-fossil-add]",
        ).forEach((button) => {
            if (busy) {
                button.dataset.disabledBeforeBusy ??= String(button.disabled);
                button.disabled = true;
            } else if (button.dataset.disabledBeforeBusy !== undefined) {
                button.disabled = button.dataset.disabledBeforeBusy === "true";
                delete button.dataset.disabledBeforeBusy;
            }
        });
    }

    private async guard(work: () => Promise<void>): Promise<void> {
        if (this.busy || this.disposed) {
            return;
        }
        this.setBusy(true);
        const pending = work();
        this.currentWork = pending;
        try {
            await pending;
            this.setStatus("");
        } catch (error) {
            this.setStatus(error instanceof Error ? error.message : String(error));
        } finally {
            if (this.currentWork === pending) {
                this.currentWork = null;
            }
            this.setBusy(false);
        }
    }

    private renderShell(): void {
        if (this.pickerOpen) {
            this.innerHTML = `
                <div class="pc-calculator pc-emulator-picking">
                    <pc-base-picker></pc-base-picker>
                </div>`;
            const picker = this.querySelector<PcBasePicker>("pc-base-picker")!;
            picker.setBases(this.bases);
            picker.setSelection(this.base, this.itemLevel);
            picker.addEventListener("confirm", (event) => {
                const detail = (event as CustomEvent<BasePickerSelection>).detail;
                void this.guard(() => this.applyPickerSelection(detail));
            });
            picker.addEventListener("cancel", () => {
                if (this.hasBase) {
                    this.pickerOpen = false;
                    this.renderShell();
                    void this.guard(async () => {
                        await this.refresh();
                        this.renderResults();
                    });
                }
            });
            return;
        }
        this.innerHTML = `
            <div class="pc-calculator">
                <div class="pc-craft-bar">
                    <button data-cmd="change-base">Change base…</button>
                    <span class="pc-calc-base">${escapeHtml(baseLabel(this.base))} · iLvl ${this.itemLevel}</span>
                    <label class="pc-calc-fresh">
                        <select data-role="fresh-rarity">
                            ${["normal", "magic", "rare"]
                                .map(
                                    (rarity) =>
                                        `<option value="${rarity}" ${rarity === this.freshRarity ? "selected" : ""}>${titleCase(rarity)}</option>`,
                                )
                                .join("")}
                        </select>
                        <button data-cmd="new-item">New item</button>
                    </label>
                    <span class="pc-calc-status" hidden></span>
                </div>
                <div class="pc-advanced-crafts"></div>
                <div class="pc-calc-body">
                    <section class="pc-calc-item">
                        <h3>Item</h3>
                        <pc-mod-list></pc-mod-list>
                        <h3>Goal</h3>
                        <label class="pc-calc-goal-rarity">
                            <span>Finished rarity</span>
                            <select data-role="goal-rarity">
                                <option value="normal">Normal</option>
                                <option value="magic">Magic</option>
                                <option value="rare">Rare</option>
                            </select>
                        </label>
                        <ul class="pc-calc-slots"></ul>
                        <pc-combobox data-role="group-slot"
                            placeholder="Or require any mod from a group…"></pc-combobox>
                    </section>
                    <section class="pc-calc-pool">
                        <pc-mod-pool select-goal></pc-mod-pool>
                    </section>
                    <section class="pc-calc-results">
                        <h3>Odds</h3>
                        <div class="pc-calc-output"></div>
                    </section>
                </div>
            </div>`;

        this.querySelectorAll<HTMLButtonElement>("button[data-cmd]").forEach(
            (button) => {
                button.addEventListener("click", () => {
                    const cmd = button.dataset.cmd;
                    if (cmd === "change-base") {
                        this.pickerOpen = true;
                        this.renderShell();
                        return;
                    }
                    void this.guard(async () => {
                        if (cmd === "new-item") await this.newItem();
                    });
                });
            },
        );
        this.querySelector<HTMLSelectElement>(
            '[data-role="fresh-rarity"]',
        )?.addEventListener("change", (event) => {
            this.freshRarity = (event.currentTarget as HTMLSelectElement).value;
        });
        this.querySelector<HTMLSelectElement>(
            '[data-role="goal-rarity"]',
        )?.addEventListener("change", (event) => {
            this.goalRarity = (event.currentTarget as HTMLSelectElement)
                .value as "normal" | "magic" | "rare";
            void this.guard(() => this.goalChanged());
        });
        this.modPool.addEventListener("craft-mod", (event) => {
            const detail = (
                event as CustomEvent<{ key: string; side: "prefix" | "suffix" }>
            ).detail;
            this.addGoalFromPool(detail.key);
        });
        this.modPool.addEventListener("tab-change", () => {
            void this.guard(() => this.refresh());
        });
        this.querySelector<PcCombobox>(
            '[data-role="group-slot"]',
        )?.addEventListener("change", (event) => {
            const value = (event as CustomEvent<{ value: string }>).detail
                .value;
            if (
                !value ||
                this.slots.length >= MAX_GOAL_SLOTS ||
                this.slots.some((slot) => slot.group === value)
            ) {
                return;
            }
            this.slots = [...this.slots, { group: value, minTier: 0 }];
            const box = this.querySelector<PcCombobox>(
                '[data-role="group-slot"]',
            );
            box?.setValue("");
            void this.guard(() => this.goalChanged());
        });
        this.renderGoal();
        this.renderActionPanels();
        this.renderResults();
        this.setBusy(this.busy);
    }
}

function panelForAction(id: string): CraftPanel {
    if (id.startsWith("essence:")) return "essence";
    if (id.startsWith("fossil:")) return "fossil";
    if (id.startsWith("harvest_")) return "harvest";
    if (id.startsWith("eldritch_")) return "eldritch";
    if (id.startsWith("influence_exalt:")) return "influenced";
    if (id.startsWith("veiled_") || id === "unveil") return "veiled";
    return "basic";
}

function slotStatusCell(outcome: CalcOutcome, index: number): string {
    const blocked = (outcome.blocked >> index) & 1;
    const status = outcome.slots[index] ?? 0;
    if (status === 2) {
        return '<td class="pc-slot-hit" title="satisfied">✓</td>';
    }
    if (status === 1) {
        return '<td class="pc-slot-low" title="present below tier">~</td>';
    }
    if (blocked) {
        return '<td class="pc-slot-blocked" title="blocked by a non-goal mod">⊘</td>';
    }
    return '<td class="pc-slot-miss" title="absent">·</td>';
}

function flagLabels(flags: number): string {
    const labels: string[] = [];
    for (let bit = 0; bit < FLAG_LABELS.length; bit += 1) {
        if (flags & (1 << bit)) {
            labels.push(FLAG_LABELS[bit]);
        }
    }
    return escapeHtml(labels.join(", "));
}

function formatProbability(p: number): string {
    if (p === 0) return "0%";
    if (p >= 0.1) return `${(p * 100).toFixed(1)}%`;
    if (p >= 0.001) return `${(p * 100).toFixed(2)}%`;
    return `${(p * 100).toPrecision(2)}%`;
}

function formatNumber(value: number): string {
    return Number.isInteger(value) ? String(value) : value.toFixed(2);
}

function baseLabel(path: string): string {
    return path.split("/").pop() ?? path;
}

function escapeHtml(text: string): string {
    return text
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;");
}

customElements.define("pc-calculator", PcCalculator);
