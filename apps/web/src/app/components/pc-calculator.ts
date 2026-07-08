/*
 * pc-calculator — the Calculator workspace document (solver S6). Given one
 * item, one goal split, and one action, it shows the calculation engine's
 * exact outcome distribution: per-goal-slot hit odds, outcome classes over
 * goal-relevant features, and the action's expected cost against the
 * workspace price table. The engine is the only rules authority; this tab
 * renders pc_calc_action_outcomes results verbatim.
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
import { buildModifierOptions, titleCase } from "../modifier-options";
import type { ModifierFamilyOption } from "./pc-condition-editor";
import { PcBasePicker, BasePickerSelection } from "./pc-base-picker";
import { PcModList, SlotMod } from "./pc-mod-list";
import { PcModifierPicker } from "./pc-modifier-picker";
import { ComboOption, PcCombobox } from "./pc-combobox";
import "./pc-base-picker";
import "./pc-mod-list";
import "./pc-modifier-picker";
import "./pc-combobox";

const DEFAULT_BASE = "Metadata/Items/Armours/BodyArmours/BodyInt17";
const MAX_GOAL_SLOTS = 8;
const MAX_FOSSILS = 4;
const MAX_OUTCOME_ROWS = 40;
const REACH_KIND_CRAFTED = 2;

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
    private item = 0;
    private solver = 0;
    private modCache: ModInfo[] = [];
    private familyLabels = new Map<number, string>();
    private modifierOptions: ModifierFamilyOption[] = [];
    private pickerActions: SolverActionInfo[] = [];

    private goalRarity: "normal" | "magic" | "rare" = "rare";
    private slots: CalculatorGoalSlot[] = [];
    private actionId = "";
    private fossilKeys: string[] = [];
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
            await this.client.closeSession(this.session);
            this.session = 0;
        }
        this.modCache = [];
        this.familyLabels.clear();
        this.modifierOptions = [];
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
        this.session = session;
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
            // picked action (including fossil loadouts) can be calculated.
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
            // The registry depends only on the session, so the picker list
            // survives goal edits; fetch it once per session.
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
        const session = this.session;
        this.item = 0;
        this.solver = 0;
        this.session = 0;
        if (solver && this.client) {
            await this.client.closeSolver(solver);
        }
        if (item && this.client) {
            await this.client.closeItem(item);
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
        this.renderAction();
        await this.recalc();
        await this.persist();
    }

    private async actionChanged(): Promise<void> {
        this.renderAction();
        await this.recalc();
        await this.persist();
    }

    /** "fossil:<a>+<b>" with sorted keys, matching solver_registry.cpp. */
    private effectiveActionId(): string {
        if (this.fossilKeys.length > 1) {
            return `fossil:${[...this.fossilKeys].sort().join("+")}`;
        }
        return this.actionId;
    }

    /** Cost keys for the effective action; combos mirror the registry's
     * per-fossil + resonator-size vector. */
    private effectiveCostKeys(): string[] {
        if (this.fossilKeys.length > 1) {
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
        const actionId = this.effectiveActionId();
        if (this.solver && this.item && actionId) {
            try {
                this.calc = await this.client.solverCalc(
                    this.solver,
                    this.item,
                    actionId,
                );
            } catch (error) {
                this.calcError =
                    error instanceof Error ? error.message : String(error);
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
        const prefixes = (info.prefix_mod_ids as number[]).map((id) =>
            this.toSlot(id, fracturedP),
        );
        const suffixes = (info.suffix_mod_ids as number[]).map((id) =>
            this.toSlot(id, fracturedS),
        );
        const implicits = (info.implicit_mod_ids as number[]).map((id) =>
            this.toSlot(id, new Set()),
        );
        this.modList.setModel({
            rarity: info.rarity as string,
            influences: [],
            prefixes,
            suffixes,
            implicits,
            maxPrefix: (info.max_prefix as number) ?? prefixes.length,
            maxSuffix: (info.max_suffix as number) ?? suffixes.length,
        });
        this.renderGoal();
        this.renderAction();
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

    private renderGoal(): void {
        const list = this.querySelector(".pc-calc-slots");
        if (!list) return;
        if (this.slots.length === 0) {
            list.innerHTML =
                '<li class="pc-empty">Add the modifiers the finished item needs.</li>';
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

        const picker = this.querySelector<PcModifierPicker>(
            "pc-modifier-picker",
        );
        picker?.setOptions(
            this.slots.length < MAX_GOAL_SLOTS ? this.modifierOptions : [],
            this.slots
                .map((slot) => slot.familyModKey ?? "")
                .filter(Boolean),
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

    private renderAction(): void {
        const host = this.querySelector(".pc-calc-action");
        if (!host) return;
        const combo = host.querySelector<PcCombobox>('[data-role="action"]');
        if (combo) {
            combo.setOptions(
                this.pickerActions.map((action) => ({
                    value: action.id,
                    label: this.actionLabel(action),
                })),
            );
            combo.setValue(this.actionId);
        }
        const hint = host.querySelector<HTMLElement>(".pc-calc-action-hint");
        if (hint) {
            hint.hidden = this.slots.length > 0;
        }
        this.renderFossils();
    }

    private renderFossils(): void {
        const host = this.querySelector<HTMLElement>(".pc-calc-fossils");
        if (!host) return;
        if (!this.actionId.startsWith("fossil:")) {
            host.hidden = true;
            host.innerHTML = "";
            return;
        }
        host.hidden = false;
        const nameOf = (key: string): string =>
            this.catalog?.fossils.find((entry) => entry.key === key)?.name ??
            key;
        host.innerHTML = `
            <span class="pc-help">Resonator loadout</span>
            <span class="pc-selected-fossils">
                ${this.fossilKeys
                    .map(
                        (key, index) =>
                            `<span class="pc-chip">${escapeHtml(nameOf(key))}${
                                this.fossilKeys.length > 1
                                    ? `<button data-fossil-remove="${index}" title="Remove">&times;</button>`
                                    : ""
                            }</span>`,
                    )
                    .join("")}
            </span>
            ${
                this.fossilKeys.length < MAX_FOSSILS
                    ? '<pc-combobox data-role="add-fossil" placeholder="Add fossil…"></pc-combobox>'
                    : ""
            }`;
        const addBox = host.querySelector<PcCombobox>(
            '[data-role="add-fossil"]',
        );
        if (addBox) {
            const taken = new Set(this.fossilKeys);
            addBox.setOptions(
                this.pickerActions
                    .filter(
                        (action) =>
                            action.id.startsWith("fossil:") &&
                            !taken.has(action.id.slice("fossil:".length)),
                    )
                    .map((action) => ({
                        value: action.id.slice("fossil:".length),
                        label: this.actionLabel(action).replace(
                            /^Fossil · /,
                            "",
                        ),
                    })),
            );
            addBox.addEventListener("change", (event) => {
                const key = (event as CustomEvent<{ value: string }>).detail
                    .value;
                if (
                    key &&
                    !this.fossilKeys.includes(key) &&
                    this.fossilKeys.length < MAX_FOSSILS
                ) {
                    this.fossilKeys = [...this.fossilKeys, key];
                    void this.guard(() => this.actionChanged());
                }
            });
        }
        host.querySelectorAll<HTMLButtonElement>("[data-fossil-remove]").forEach(
            (button) => {
                button.addEventListener("click", () => {
                    const index = Number(button.dataset.fossilRemove);
                    this.fossilKeys = this.fossilKeys.filter(
                        (_, i) => i !== index,
                    );
                    void this.guard(() => this.actionChanged());
                });
            },
        );
    }

    /** Human label for a registry action id, resolved through the catalog. */
    private actionLabel(action: SolverActionInfo): string {
        const id = action.id;
        const colon = id.indexOf(":");
        const kind = colon < 0 ? id : id.slice(0, colon);
        const rest = colon < 0 ? "" : id.slice(colon + 1);
        switch (kind) {
            case "essence": {
                const name = this.catalog?.essences.find(
                    (entry) => entry.key === rest,
                )?.name;
                return `Essence · ${name ?? rest}`;
            }
            case "fossil":
                return `Fossil · ${action.display_name}`;
            case "bench": {
                const name = this.catalog?.bench.find(
                    (entry) => entry.key === rest,
                )?.name;
                return `Bench · ${name ?? rest}`;
            }
            case "harvest_reforge":
                return `Harvest · Reforge ${titleCase(rest)}`;
            case "harvest_augment":
                return `Harvest · Augment ${titleCase(rest)}`;
            case "harvest_resist": {
                const [from, to] = rest.split(":");
                return `Harvest · Resist ${titleCase(from ?? "")} → ${titleCase(to ?? "")}`;
            }
            case "eldritch_ember":
                return `Eldritch · Ember T${rest}`;
            case "eldritch_ichor":
                return `Eldritch · Ichor T${rest}`;
            case "influence_exalt":
                return `Influence · ${titleCase(rest)} Exalt`;
            case "restart":
                return "Restart (fresh base)";
            default:
                return titleCase(id);
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
        if (!this.effectiveActionId()) {
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
        const keys = this.effectiveCostKeys();
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
                    <input type="number" min="0" step="any" data-price-key="${escapeAttribute(key)}"
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
                    `<th title="${escapeAttribute(this.slotLabel(this.slots[index]))}">G${index + 1}</th>`,
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
            "button[data-cmd]",
        ).forEach((button) => {
            button.disabled = busy;
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
                <div class="pc-calc-body">
                    <section class="pc-calc-item">
                        <h3>Item</h3>
                        <pc-mod-list></pc-mod-list>
                    </section>
                    <section class="pc-calc-config">
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
                        <pc-modifier-picker></pc-modifier-picker>
                        <pc-combobox data-role="group-slot"
                            placeholder="Or add a whole mod group…"></pc-combobox>
                        <h3>Action</h3>
                        <p class="pc-calc-action-hint pc-help">Add a goal modifier first — odds are computed against the goal.</p>
                        <div class="pc-calc-action">
                            <pc-combobox data-role="action"
                                placeholder="Search actions…"></pc-combobox>
                            <div class="pc-calc-fossils" hidden></div>
                        </div>
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
        this.querySelector<PcModifierPicker>(
            "pc-modifier-picker",
        )?.addEventListener("modifier-select", (event) => {
            const detail = (
                event as CustomEvent<{ value: string; fractured: boolean }>
            ).detail;
            if (
                this.slots.length >= MAX_GOAL_SLOTS ||
                this.slots.some((slot) => slot.familyModKey === detail.value)
            ) {
                return;
            }
            this.slots = [
                ...this.slots,
                { familyModKey: detail.value, minTier: 0 },
            ];
            void this.guard(() => this.goalChanged());
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
        this.querySelector<PcCombobox>(
            '[data-role="action"]',
        )?.addEventListener("change", (event) => {
            const value = (event as CustomEvent<{ value: string }>).detail
                .value;
            if (!value || value === this.actionId) {
                return;
            }
            this.actionId = value;
            this.fossilKeys = value.startsWith("fossil:")
                ? [value.slice("fossil:".length)]
                : [];
            void this.guard(() => this.actionChanged());
        });
        this.renderGoal();
        this.renderAction();
        this.renderResults();
        this.setBusy(this.busy);
    }
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

function escapeAttribute(text: string): string {
    return escapeHtml(text);
}

customElements.define("pc-calculator", PcCalculator);
