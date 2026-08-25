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
 * Input and Goal share the same fixed-slot item frame in a compact context
 * rail. Goal rows carry engine-returned marginal probabilities inline; the
 * results column owns the combined result, costs, and outcome distribution.
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
    BestiaryActionInfo,
    BestiaryCalculation,
    BestiarySolverOptionInfo,
    CalcResult,
    Catalog,
    EngineError,
    EconomyIdentity,
    ModInfo,
    SolveProgress,
    SolveSummary,
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
import {
    getActionPrice,
    getActionPriceResolution,
    getFallbackPrice,
    onPricesChange,
    pinEconomy,
    setFallbackPrice,
    setPrice,
} from "../workspace/prices";
import type { PinnedEconomy } from "../workspace/economy-service";
import { influenceLabels } from "../item-display";
import { buildCalculatorTargetModel } from "../calculator-goal-model";
import {
    HARVEST_AUGMENT,
    HARVEST_REFORGE,
    harvestTagsFor,
} from "../harvest-crafts";
import {
    estimatedActionSpendPerSuccess,
    formatChaosValue,
    formatExpectedAttempts,
    formatProbabilityExact,
    formatRawProbability,
    presentExpectedConsumption,
} from "../odds-presentation";
import {
    buildModifierKeyIndex,
    buildModifierOptions,
    titleCase,
    type ModifierFamilyOption,
} from "../modifier-options";
import {
    craftActionLabel,
    groupEssences,
    resistanceEntries,
} from "../craft-choices";
import {
    buildCalculatorSolverGoal,
    prepareSolverStrategy,
    pricedSolverActionIds,
    solvePriceReadiness,
} from "../solve-workspace";
import {
    certifiedFactorLabel,
    shouldCompileSolvePolicy,
    calculatorSolveOptions,
    solveResultMarkup,
    solveTerminationDetail,
} from "../solver-result-presentation";
import { cloneStrategy, type StrategyDocument } from "../strategy-model";
import { PcBasePicker, BasePickerSelection } from "./pc-base-picker";
import { PcModList, SlotMod } from "./pc-mod-list";
import { PcModPool } from "./pc-mod-pool";
import "./pc-base-picker";
import "./pc-mod-list";
import "./pc-mod-pool";

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
    "remove_crafted_modifiers",
    "fracture",
];

type CraftPanel =
    | "basic"
    | "essence"
    | "harvest"
    | "fossil"
    | "eldritch"
    | "influenced"
    | "veiled"
    | "bestiary";

const CRAFT_PANELS: Array<[CraftPanel, string]> = [
    ["basic", "Basic currency"],
    ["essence", "Essences"],
    ["harvest", "Harvest"],
    ["fossil", "Fossil"],
    ["eldritch", "Eldritch"],
    ["influenced", "Influenced"],
    ["veiled", "Veiled"],
    ["bestiary", "Bestiary"],
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
const RARITY_CODES = { normal: 0, magic: 1, rare: 2 } as const;

function solveElapsedLabel(elapsedMs: number): string {
    const seconds = Math.max(0, Math.floor(elapsedMs / 1000));
    const minutes = Math.floor(seconds / 60);
    const remainder = seconds % 60;
    return minutes > 0 ? `${minutes}m ${remainder}s` : `${remainder}s`;
}

function solveMemoryLabel(bytes: number): string {
    return `${(Math.max(0, bytes) / (1024 * 1024)).toFixed(1)} MiB`;
}

export class PcCalculator extends HTMLElement {
    private client!: EngineClient;
    private dataId = 0;
    private bases: BaseInfo[] = [];
    private catalog: Catalog | null = null;
    private bestiaryActions: BestiaryActionInfo[] = [];
    private bestiaryOption: BestiarySolverOptionInfo | null = null;

    private docId = "";
    private base = DEFAULT_BASE;
    private itemLevel = 86;
    private freshRarity = "rare";

    private session = 0;
    private context = 0;
    private item = 0;
    private solver = 0;
    private modCache: ModInfo[] = [];
    private modifierOptions: ModifierFamilyOption[] = [];
    private modKeyToFamily = new Map<string, string>();
    private pickerActions: SolverActionInfo[] = [];

    private itemRarity = "normal";
    private itemInfluences: string[] = [];
    private itemPrefixes: SlotMod[] = [];
    private itemSuffixes: SlotMod[] = [];
    private itemImplicits: SlotMod[] = [];
    private itemMaxPrefix = 3;
    private itemMaxSuffix = 3;
    private activeContext: "input" | "goal" = "goal";

    private goalRarity: "normal" | "magic" | "rare" = "rare";
    private slots: CalculatorGoalSlot[] = [];
    private minSatisfiedSlots = 1;
    private actionId = "";
    private fossilKeys: string[] = [];
    private activeCraftPanel: CraftPanel = "basic";
    private mechanicValues = new Map<string, string>();
    private calc: CalcResult | null = null;
    private calcError = "";
    private bestiaryCalc: BestiaryCalculation | null = null;
    private solveSummary: SolveSummary | null = null;
    private solvedStrategy: StrategyDocument | null = null;
    private solveEconomy: PinnedEconomy | null = null;
    private solveError: { heading: string; detail: string } | null = null;
    private verification: {
        completedRuns: number;
        empiricalCost: number;
        delta: number;
    } | null = null;
    private solveExcludedActions = 0;
    private solveAdmittedActionIds: string[] = [];
    private solveMissingPriceKeys: string[] = [];
    private solveStopDetail = "";
    private solveTelemetry: unknown = null;
    private solveAbsoluteGapTarget = 0;
    private solveRelativeGapPercentTarget = 0;
    private solveAllowEconomicRestart = false;
    private solveConsiderImprintPrograms = false;
    private solveRunning = false;
    private solveProgress: SolveProgress | null = null;
    private solveElapsedMs = 0;
    private solveAbort: AbortController | null = null;
    private solveCancelled = false;
    private verificationRunning = false;

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
        this.unsubscribePrices = onPricesChange(() => {
            this.renderResults();
            this.renderSolvePanel();
        });
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
        const bestiary = await this.client.bestiaryPresentation(this.dataId);
        this.bestiaryActions = bestiary.actions.filter(
            (action) => action.calculator_available,
        );
        this.bestiaryOption = bestiary.solver_options[0] ?? null;
        if (this.disposed) {
            return;
        }

        const draft = await getCalculatorDraft(this.docId);
        if (draft) {
            this.base = draft.base;
            this.itemLevel = draft.itemLevel;
            this.goalRarity = draft.goalRarity;
            this.slots = draft.slots;
            this.minSatisfiedSlots =
                draft.minSatisfiedSlots ?? draft.slots.length;
            this.normalizeSuccessThreshold();
            this.actionId = draft.actionId;
            this.fossilKeys = draft.fossilKeys;
            this.activeCraftPanel = panelForAction(this.actionId);
        }
        if (!this.bases.some((b) => b.path === this.base)) {
            this.base = this.bases[0]?.path ?? this.base;
        }
        this.renderBaseSummary();
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
        this.modifierOptions = this.catalog
            ? buildModifierOptions(cache, this.catalog)
            : [];
        this.modKeyToFamily = buildModifierKeyIndex(cache);
        // Drop goal slots that do not exist in the new session. A goal that
        // meant "all" continues to mean all after the base changes.
        const oldSlotCount = this.slots.length;
        const followedAll =
            oldSlotCount === 0 ||
            this.effectiveMinSatisfiedSlots() === oldSlotCount;
        this.slots = this.slots.filter((slot) =>
            slot.group
                ? this.groupIdByKey(slot.group) !== undefined
                : this.modifierOptions.some(
                      (option) => option.value === slot.familyModKey,
                  ),
        );
        this.normalizeSuccessThreshold(followedAll);
    }

    /** (Re)open the solver for the current goal. Requires >= 1 slot. */
    private async openSolver(): Promise<void> {
        this.clearSolveResult();
        await this.closeSolverHandle();
        this.calc = null;
        this.bestiaryCalc = null;
        this.calcError = "";
        if (this.disposed || this.slots.length === 0) {
            return;
        }
        const goal = this.solverGoal("odds");
        try {
            // The engine owns bounded goal-relevant fossil synthesis. Keep a
            // hand-selected loadout materialized for exact odds even when it
            // falls outside the current automatic beam.
            this.solver = await this.client.openSolver(this.session, goal);
        } catch (error) {
            this.calcError =
                error instanceof Error ? error.message : String(error);
            return;
        }
        if (this.disposed) {
            return;
        }
        // Goal-relevant fossil actions change with the target, so the displayed
        // price/action envelope is refreshed from this exact solver handle.
        this.pickerActions = await this.client.solverActions(this.solver);
    }
    private solverGoal(
        mode: "odds" | "product_envelope" | "scoped_solve",
        actions?: readonly string[],
    ): SolverGoal {
        return buildCalculatorSolverGoal(
            {
                rarity: this.goalRarity,
                minSatisfiedSlots: this.effectiveMinSatisfiedSlots(),
                slots: this.slots.map((slot) =>
                    slot.group
                        ? { group: slot.group, min_tier: slot.minTier }
                        : {
                              family_mod_key: slot.familyModKey ?? "",
                              min_tier: slot.minTier,
                          },
                ),
            },
            mode,
            this.actionId,
            actions,
        );
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

    private clearSolveResult(): void {
        this.solveSummary = null;
        this.solvedStrategy = null;
        this.solveEconomy = null;
        this.solveError = null;
        this.verification = null;
        this.solveExcludedActions = 0;
        this.solveAdmittedActionIds = [];
        this.solveMissingPriceKeys = [];
        this.solveStopDetail = "";
        this.solveTelemetry = null;
        this.solveProgress = null;
        this.solveElapsedMs = 0;
        this.solveCancelled = false;
    }

    private async newItem(): Promise<void> {
        this.clearSolveResult();
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

    private async inputChanged(): Promise<void> {
        this.clearSolveResult();
        await this.refresh();
        await this.recalc();
        await this.persist();
    }

    private async addInputMod(
        key: string,
        side: "prefix" | "suffix",
        fractured = false,
    ): Promise<void> {
        const info = this.modCache.find((mod) => mod.key === key);
        if (info?.reach_kind === REACH_KIND_CRAFTED && !fractured) {
            await this.client.apply(this.context, this.item, {
                type: "bench",
                mod_key: key,
            });
        } else {
            await this.client.addMod(this.item, this.session, {
                key,
                side,
                fractured,
            });
        }
        await this.inputChanged();
    }

    private async removeInputMod(
        modId: number,
        side: "prefix" | "suffix",
    ): Promise<void> {
        await this.client.removeMod(this.item, { modId, side });
        await this.inputChanged();
    }

    private async fractureInputMod(
        key: string,
        modId: number,
        side: "prefix" | "suffix",
        onItem: boolean,
    ): Promise<void> {
        if (onItem) {
            await this.client.setModFractured(this.item, { modId, side });
            await this.inputChanged();
            return;
        }
        await this.addInputMod(key, side, true);
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
        if (
            this.actionId.startsWith("fossil:") &&
            !this.pickerActions.some((action) => action.id === this.actionId)
        ) {
            await this.openSolver();
        }
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

    private selectContext(context: "input" | "goal"): void {
        this.activeContext = context;
        this.renderContextSelection();
        this.querySelector<PcModPool>("pc-mod-pool")?.setInteractionMode(
            context === "input" ? "direct" : "goal",
        );
    }

    private renderContextSelection(): void {
        this.querySelectorAll<HTMLElement>("[data-context-card]").forEach(
            (card) => {
                const active = card.dataset.contextCard === this.activeContext;
                card.classList.toggle("is-active", active);
                card.setAttribute("aria-selected", String(active));
                const state = card.querySelector<HTMLElement>(
                    ".pc-calc-context-state",
                );
                if (state) state.textContent = active ? "Editing" : "Select";
            },
        );
    }

    /** "fossil:<a>+<b>" with sorted keys, matching solver_registry.cpp. */
    private fossilComboId(): string {
        return this.fossilKeys.length
            ? `fossil:${[...this.fossilKeys].sort().join("+")}`
            : "";
    }

    /** Cost keys for the selected action; fossil loadouts mirror the
     * registry's per-fossil + resonator-size vector. */
    private imprintCreationCostKeys(): string[] {
        return (
            this.bestiaryActions.find((action) => action.operation === "create")
                ?.cost_keys ?? []
        );
    }

    private selectedCostKeys(): string[] {
        const bestiary = this.bestiaryActions.find(
            (action) => action.id === this.actionId,
        );
        if (bestiary) return bestiary.cost_keys;
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

    private async startSolve(): Promise<void> {
        const imprintCostKeys = this.bestiaryOption
            ? this.imprintCreationCostKeys()
            : [];
        const pinned = pinEconomy(
            [
                ...this.pickerActions.flatMap((action) => action.cost_keys),
                ...imprintCostKeys,
            ],
        );
        const pinnedPrice = (key: string): number | undefined =>
            pinned.snapshot.prices[key];
        const readiness = solvePriceReadiness(this.pickerActions, pinnedPrice);
        if (!this.solver || !this.item || this.slots.length === 0) {
            this.solveError = {
                heading: "Solve is not ready.",
                detail: "Choose an input item and define at least one goal modifier.",
            };
            this.renderSolvePanel();
            return;
        }
        if (readiness.pricedActions === 0) {
            this.solveError = {
                heading: "Solve needs at least one priced action.",
                detail: "Set a chaos-equivalent price in the shared action price table.",
            };
            this.renderSolvePanel();
            return;
        }
        if (readiness.missingFractureBasePrice) {
            this.solveError = {
                heading: "Set a fresh-base price to plan Fracture.",
                detail: "Goal-relevant Fracture miss recovery replaces the failed item with a fresh base. Set the base price in the shared action price table, or remove the Fracture price before solving.",
            };
            this.renderSolvePanel();
            return;
        }
        this.clearSolveResult();
        this.solveEconomy = pinned;
        this.solveRunning = true;
        const solveStartedAt = performance.now();
        this.solveElapsedMs = 0;
        const refreshSolveElapsed = (): void => {
            this.solveElapsedMs = performance.now() - solveStartedAt;
            const elapsed = this.querySelector<HTMLElement>(
                ".pc-calc-solve-elapsed b",
            );
            if (elapsed) {
                elapsed.textContent = solveElapsedLabel(this.solveElapsedMs);
            }
        };
        const solveClock = window.setInterval(() => {
            if (this.solveRunning) refreshSolveElapsed();
        }, 1000);
        const solveAbort = new AbortController();
        this.solveAbort = solveAbort;
        this.setStatus("Solving — may take a while on large goals.");
        this.renderSolvePanel();
        let economy = 0;
        let envelopeSolver = 0;
        let solveSolver = 0;
        try {
            /* Build the native product envelope before selecting its priced
             * subset. The ordinary Calculator handle stays exhaustive so
             * exact single-action odds and craft panels do not lose actions
             * merely because Solve uses a smaller abstraction. */
            envelopeSolver = await this.client.openSolver(
                this.session,
                this.solverGoal("product_envelope"),
            );
            const relevantActions =
                await this.client.solverActions(envelopeSolver);
            await this.client.closeSolver(envelopeSolver);
            envelopeSolver = 0;
            const candidateIds = pricedSolverActionIds(
                relevantActions,
                pinnedPrice,
            );
            if (candidateIds.length === 0) {
                throw new Error("No priced solver actions are available.");
            }
            this.solveExcludedActions =
                relevantActions.length - candidateIds.length;
            this.solveAdmittedActionIds = [...candidateIds];
            this.solveMissingPriceKeys = Array.from(
                new Set(
                    relevantActions.flatMap((action) =>
                        action.cost_keys.filter(
                            (key) => pinnedPrice(key) === undefined,
                        ),
                    ),
                ),
            ).sort((left, right) => left.localeCompare(right));
            const solveGoal = this.solverGoal("scoped_solve", candidateIds);
            solveSolver = await this.client.openSolver(
                this.session,
                solveGoal,
            );
            economy = await this.client.loadEconomy(pinned.snapshot);
            const solveOptions = calculatorSolveOptions(
                this.solveAbsoluteGapTarget,
                this.solveRelativeGapPercentTarget,
                this.solveAllowEconomicRestart,
                this.solveConsiderImprintPrograms,
            );
            const result = await this.client.solverSolve(
                solveSolver,
                this.item,
                economy,
                solveOptions,
                {
                    signal: solveAbort.signal,
                    /* Reserve the last minute of the five-minute product
                     * boundary for compile/certify/exact evaluation of the
                     * best executable incumbent found during discovery. */
                    boundedFinishAfterMs: 4 * 60 * 1000,
                    onProgress: (progress) => {
                        this.solveProgress = progress;
                        refreshSolveElapsed();
                        const phase =
                            progress.phase === "expanding"
                                ? `${progress.expanded_states.toLocaleString()} expanded, ${progress.discovered_states.toLocaleString()} discovered`
                                : progress.phase === "iterating"
                                  ? `${progress.sweeps.toLocaleString()} sweeps`
                                  : progress.phase === "refining"
                                    ? `${progress.refinement_states.toLocaleString()} strict states refined`
                                  : progress.phase === "compiling"
                                    ? "compiling selected policy"
                                  : progress.phase === "certifying"
                                    ? `${progress.certification_discovered_pairs.toLocaleString()} certification pairs`
                                    : "publishing result";
                        this.setStatus(
                            `Solving · ${phase} · ${solveElapsedLabel(this.solveElapsedMs)}`,
                        );
                        this.renderSolvePanel();
                    },
                },
            );
            if (result.cancelled) {
                this.solveCancelled = true;
                return;
            }
            result.economy = pinned.identity;
            this.solveSummary = result;
            let telemetry: unknown = null;
            try {
                telemetry = await this.client.solverTelemetry(
                    solveSolver,
                );
            } catch {
                // The solve summary remains usable if telemetry retrieval
                // itself fails.
            }
            this.solveTelemetry = telemetry;
            this.solveStopDetail = solveTerminationDetail(result, telemetry);
            if (!shouldCompileSolvePolicy(result)) {
                this.solveError = {
                    heading: "No executable policy was returned.",
                    detail: this.solveStopDetail,
                };
                return;
            }
            try {
                const compiled = await this.client.solverCompileStrategy(
                    solveSolver,
                );
                this.solvedStrategy = prepareSolverStrategy(compiled);
                this.solvedStrategy.economy = pinned.identity;
            } catch (error) {
                const detail = engineErrorDetail(error);
                this.solveError =
                    error instanceof EngineError && error.code === 4
                        ? {
                              heading: "The returned policy could not be compiled.",
                              detail,
                          }
                        : {
                              heading:
                                  "The returned policy's Strategy Board document could not be prepared.",
                              detail,
                          };
            }
        } catch (error) {
            this.solveError = {
                heading: "Solve could not complete.",
                detail: engineErrorDetail(error),
            };
        } finally {
            window.clearInterval(solveClock);
            refreshSolveElapsed();
            this.solveAbort = null;
            const releases: Promise<unknown>[] = [];
            if (solveSolver) {
                releases.push(this.client.closeSolver(solveSolver));
            }
            if (envelopeSolver) {
                releases.push(this.client.closeSolver(envelopeSolver));
            }
            if (economy) {
                releases.push(this.client.closeEconomy(economy));
            }
            await Promise.all(releases);
            this.solveRunning = false;
            this.renderSolvePanel();
        }
    }

    private cancelSolve(): void {
        if (!this.solveRunning || !this.solveAbort) return;
        this.setStatus("Cancelling solve...");
        this.solveAbort.abort();
    }

    private async openSolvedStrategy(): Promise<void> {
        if (!this.solvedStrategy) return;
        await workspace().openStrategy(
            cloneStrategy(this.solvedStrategy),
            "copy",
        );
    }

    private async verifySolvedStrategy(): Promise<void> {
        const evaluatedPolicyCost =
            this.solveSummary?.evaluated_policy_cost ?? null;
        if (!this.solveSummary || !this.solvedStrategy ||
            evaluatedPolicyCost === null) return;
        this.verification = null;
        this.solveError = null;
        this.verificationRunning = true;
        this.setStatus("Verifying policy · 0 / 10,000 runs");
        this.renderSolvePanel();
        let economy = 0;
        let strategy = 0;
        let simulator = 0;
        const pinned = this.solveEconomy ?? pinEconomy();
        try {
            economy = await this.client.loadEconomy(pinned.snapshot);
            strategy = await this.client.compileStrategy(
                this.session,
                this.solvedStrategy,
            );
            simulator = await this.client.createSimulator(
                this.session,
                strategy,
                economy,
            );
            const result = await this.client.runStrategy(
                simulator,
                {
                    target_runs: 10_000,
                    max_actions_per_run: 100_000,
                },
                {
                    onProgress: (progress) =>
                        this.setStatus(
                            `Verifying policy · ${progress.done.toLocaleString()} / ${progress.total.toLocaleString()} runs`,
                        ),
                },
            );
            result.economy = pinned.identity;
            const completedRuns = result.summary.completed_runs;
            if (result.cancelled || completedRuns !== 10_000) {
                throw new Error(
                    `Verification completed ${completedRuns.toLocaleString()} of 10,000 runs.`,
                );
            }
            if (result.summary.cost_status !== "complete") {
                throw new Error(
                    "Verification could not calculate a complete cost for every run.",
                );
            }
            const empiricalCost =
                result.summary.known_total_cost / completedRuns;
            this.verification = {
                completedRuns,
                empiricalCost,
                delta: empiricalCost - evaluatedPolicyCost,
            };
        } catch (error) {
            this.solveError = {
                heading: "Verification could not complete.",
                detail: engineErrorDetail(error),
            };
        } finally {
            if (simulator) await this.client.closeSimulator(simulator);
            if (strategy) await this.client.closeStrategy(strategy);
            if (economy) await this.client.closeEconomy(economy);
            this.verificationRunning = false;
            this.renderSolvePanel();
        }
    }

    private async recalc(): Promise<void> {
        this.calc = null;
        this.bestiaryCalc = null;
        this.calcError = "";
        if (this.item && this.actionId) {
            try {
                const bestiary = this.bestiaryActions.find(
                    (action) => action.id === this.actionId,
                );
                if (bestiary) {
                    this.bestiaryCalc = await this.client.bestiaryCalculate(
                        this.dataId,
                        this.item,
                        bestiary.id,
                    );
                } else if (this.solver) {
                    this.calc = await this.client.solverCalc(
                        this.solver,
                        this.item,
                        this.actionId,
                    );
                }
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
        this.renderGoal(); // per-slot odds live inline on the goal rows
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
            minSatisfiedSlots: this.effectiveMinSatisfiedSlots(),
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
        this.itemRarity = info.rarity as string;
        this.itemInfluences = influenceLabels(
            Number(info.generic_influence_bits ?? 0),
            Number(info.searing_exarch_tier ?? 0),
            Number(info.eater_of_worlds_tier ?? 0),
            this.catalog,
        );
        this.itemMaxPrefix = (info.max_prefix as number) ?? prefixIds.length;
        this.itemMaxSuffix = (info.max_suffix as number) ?? suffixIds.length;
        this.itemPrefixes = prefixIds.map((id) =>
            this.toSlot(id, fracturedP),
        );
        this.itemSuffixes = suffixIds.map((id) =>
            this.toSlot(id, fracturedS),
        );
        this.itemImplicits = implicitIds.map((id) =>
            this.toSlot(id, new Set()),
        );
        this.renderItem();

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
                maxPrefix: this.itemMaxPrefix,
                maxSuffix: this.itemMaxSuffix,
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
            tierIndex: info.family_tier_index,
            textLines: info.text_lines,
            classificationTags: info.classification_tags,
            fractured: fractured.has(id),
            crafted: info.reach_kind === REACH_KIND_CRAFTED,
        };
    }

    private renderItem(): void {
        this.inputModList?.setModel({
            kind: "concrete",
            baseName: this.baseDisplayName(),
            itemLevel: this.itemLevel,
            rarity: this.itemRarity,
            influences: this.itemInfluences,
            implicits: this.itemImplicits,
            prefixes: this.itemPrefixes,
            suffixes: this.itemSuffixes,
            maxPrefix: this.itemMaxPrefix,
            maxSuffix: this.itemMaxSuffix,
        });
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

    private effectiveMinSatisfiedSlots(): number {
        if (this.slots.length === 0) return 0;
        return Math.max(
            1,
            Math.min(this.slots.length, Math.floor(this.minSatisfiedSlots)),
        );
    }

    private normalizeSuccessThreshold(followAll = false): void {
        if (this.slots.length === 0) {
            this.minSatisfiedSlots = 1;
            return;
        }
        this.minSatisfiedSlots = followAll
            ? this.slots.length
            : this.effectiveMinSatisfiedSlots();
    }

    private successTargetLabel(): string {
        const rarity = titleCase(this.goalRarity);
        if (this.slots.length === 1) {
            return `${rarity} + ${this.slotLabel(this.slots[0])}`;
        }
        const required = this.effectiveMinSatisfiedSlots();
        const modifiers =
            required === this.slots.length
                ? `all ${this.slots.length} modifiers`
                : `at least ${required} of ${this.slots.length} modifiers`;
        return `${rarity} + ${modifiers}`;
    }

    /** A pool tier was clicked: require that family at that tier or better. */
    private addGoalFromPool(modKey: string): void {
        const familyKey = this.modKeyToFamily.get(modKey);
        const info = this.modCache.find((mod) => mod.key === modKey);
        if (!familyKey || !info) {
            return;
        }
        const minTier = info.family_tier_index || 0;
        const oldSlotCount = this.slots.length;
        const followedAll =
            oldSlotCount === 0 ||
            this.effectiveMinSatisfiedSlots() === oldSlotCount;
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
        this.normalizeSuccessThreshold(followedAll);
        void this.guard(() => this.goalChanged());
    }

    private syncModPoolSelections(): void {
        this.querySelector<PcModPool>("pc-mod-pool")?.setSelectedTiers(
            this.slots.flatMap((slot) =>
                slot.familyModKey
                    ? [
                          {
                              familyModKey: slot.familyModKey,
                              minTier: slot.minTier,
                          },
                      ]
                    : [],
            ),
        );
    }

    private renderGoal(): void {
        const showOdds = Boolean(
            this.calc && this.calc.legal && this.calc.supported,
        );
        this.syncModPoolSelections();
        this.goalModList?.setModel(
            buildCalculatorTargetModel({
                baseName: this.baseDisplayName(),
                itemLevel: this.itemLevel,
                rarity: this.goalRarity,
                slots: this.slots,
                modifierOptions: this.modifierOptions,
                maxPrefix: this.itemMaxPrefix,
                maxSuffix: this.itemMaxSuffix,
                slotProbabilities: showOdds
                    ? this.calc?.slot_satisfied
                    : undefined,
                formatProbability: formatProbabilityExact,
                groupLabel: (groupKey) => {
                    const id = this.groupIdByKey(groupKey);
                    return (
                        (id !== undefined && this.catalog?.groupNameById[id]) ||
                        groupKey
                    );
                },
            }),
        );

        const rarity = this.querySelector<HTMLSelectElement>(
            '[data-role="goal-rarity"]',
        );
        if (rarity) {
            rarity.value = this.goalRarity;
        }
        const threshold = this.querySelector<HTMLSelectElement>(
            '[data-role="success-threshold"]',
        );
        if (threshold) {
            if (this.slots.length === 0) {
                threshold.innerHTML = '<option value="0">Add modifiers</option>';
                threshold.disabled = true;
            } else {
                threshold.disabled = false;
                threshold.innerHTML = Array.from(
                    { length: this.slots.length },
                    (_, offset) => this.slots.length - offset,
                )
                    .map((count) => {
                        const label =
                            count === this.slots.length
                                ? `All ${count}`
                                : `At least ${count} of ${this.slots.length}`;
                        return `<option value="${count}">${label}</option>`;
                    })
                    .join("");
                threshold.value = String(this.effectiveMinSatisfiedSlots());
            }
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
        const harvestReforgeTags = harvestTagsFor(
            this.catalog.harvestTags,
            HARVEST_REFORGE,
        );
        const harvestAugmentTags = harvestTagsFor(
            this.catalog.harvestTags,
            HARVEST_AUGMENT,
        );
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
                            <label>
                                <span>Reforge</span>
                                <select data-mechanic="harvest-reforge-tag">${options(
                                harvestReforgeTags,
                                selected(
                                    "harvest-reforge-tag",
                                    harvestReforgeTags[0]?.key,
                                ),
                            )}</select></label>
                            <button data-derive-action="harvest_reforge">Reforge</button>
                            <label>
                                <span>Augment</span>
                                <select data-mechanic="harvest-augment-tag">${options(
                                harvestAugmentTags,
                                selected(
                                    "harvest-augment-tag",
                                    harvestAugmentTags[0]?.key,
                                ),
                            )}</select></label>
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
                case "bestiary":
                    return `
                        <div class="pc-craft-options">
                            ${this.bestiaryActions
                                .map(
                                    (action) =>
                                        `<button data-select-action="${escapeHtml(action.id)}">${escapeHtml(action.display_name)}</button>`,
                                )
                                .join("")}
                        </div>
                        <div class="pc-fracture-hint">
                            ${this.bestiaryActions
                                .map(
                                    (action) =>
                                        `${escapeHtml(action.display_name)}: ${
                                            action.cost_keys.length
                                                ? action.cost_keys
                                                      .map(escapeHtml)
                                                      .join(" + ")
                                                : "no beast cost"
                                        }`,
                                )
                                .join(" ? ")}
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
                return `harvest_reforge:${value("harvest-reforge-tag")}`;
            case "harvest_augment":
                return `harvest_augment:${value("harvest-augment-tag")}`;
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
            "harvest-reforge-tag": ["harvest_reforge"],
            "harvest-augment-tag": ["harvest_augment"],
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
        const bestiary = this.bestiaryActions.find(
            (action) => action.id === id,
        );
        if (bestiary) return bestiary.display_name;
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
        if (!this.actionId) {
            host.innerHTML = '<p class="pc-empty">Pick an action.</p>';
            return;
        }
        const selectedBestiary = this.bestiaryActions.some(
            (action) => action.id === this.actionId,
        );
        if (selectedBestiary) {
            if (!this.bestiaryCalc) {
                host.innerHTML = '<p class="pc-empty">Calculating...</p>';
                return;
            }
            host.innerHTML = this.renderBestiaryResult(this.bestiaryCalc);
            this.bindPriceInputs(host);
            return;
        }
        if (this.slots.length === 0) {
            host.innerHTML =
                '<p class="pc-empty">Define a goal to compute odds against.</p>';
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
            ${this.renderExactResult(calc)}
            ${this.renderCost(calc.success_probability)}
            ${this.renderOutcomes(calc)}`;
        this.bindPriceInputs(host);
    }

    private bindPriceInputs(host: HTMLElement): void {
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

    private renderSolvePanel(): void {
        const host = this.querySelector<HTMLElement>(".pc-calc-solve-panel");
        if (!host) return;
        const readiness = solvePriceReadiness(
            this.pickerActions,
            getActionPrice,
        );
        const solveCostCounts = new Map<string, number>();
        for (const key of readiness.costKeys) solveCostCounts.set(key, 1);
        if (this.bestiaryOption) {
            for (const key of this.imprintCreationCostKeys()) {
                solveCostCounts.set(key, (solveCostCounts.get(key) ?? 0) + 1);
            }
        }
        const solveCostEntries = Array.from(
            solveCostCounts,
            ([key, quantity]) => ({ key, quantity }),
        ).sort((a, b) => a.key.localeCompare(b.key));
        const solveCostKeys = solveCostEntries.map((entry) => entry.key);
        const solveMissingKeys = solveCostKeys.filter(
            (key) => getActionPrice(key) === undefined,
        );
        const priceTable = presentExpectedConsumption(
            solveCostEntries,
            getActionPrice,
            (key) => getActionPriceResolution(key).source,
        );
        const fallbackPrice = getFallbackPrice();
        const canStart =
            Boolean(this.solver && this.item && this.slots.length) &&
            readiness.pricedActions > 0 &&
            !readiness.missingFractureBasePrice &&
            !this.busy;
        const progress = this.solveProgress;
        const progressLower = progress
            ? solveBoundLabel(progress.lower_bound)
            : "Pending";
        const progressUpper = progress
            ? solveBoundLabel(progress.upper_bound)
            : "Pending";
        const progressGap = progress
            ? solveBoundLabel(progress.absolute_optimality_gap)
            : "Pending";
        const progressFactor = progress
            ? certifiedFactorLabel(progress.relative_optimality_gap)
            : "Unavailable";
        const progressMarkup =
            this.solveRunning && progress
                ? `<section class="pc-calc-solve-progress" aria-live="polite">
                    <strong>${progress.phase === "expanding" ? "Expanding reachable states" : progress.phase === "iterating" ? "Optimizing policy" : progress.phase === "refining" ? "Refining exact policy" : progress.phase === "compiling" ? "Compiling selected policy" : progress.phase === "certifying" ? "Certifying compiled policy" : "Publishing result"}</strong>
                    <span class="pc-calc-solve-elapsed">Elapsed <b>${solveElapsedLabel(this.solveElapsedMs)}</b></span>
                    <span>Expanded <b>${progress.expanded_states.toLocaleString()}</b></span>
                    <span>Discovered <b>${progress.discovered_states.toLocaleString()}</b></span>
                    <span>Frontier <b>${progress.frontier_states.toLocaleString()}</b></span>
                    <span>Sweeps <b>${progress.sweeps.toLocaleString()}</b></span>
                    <span>Focused round <b>${progress.focused_round.toLocaleString()}</b></span>
                    <span>Rows <b>${progress.state_action_rows.toLocaleString()}</b></span>
                    <span>Transitions <b>${progress.transition_entries.toLocaleString()}</b></span>
                    <span>Reforge work <b>${progress.reforge_work.toLocaleString()}</b></span>
                    <span>Refined states <b>${progress.refinement_states.toLocaleString()}</b></span>
                    <span>Refinement classes <b>${progress.refinement_classes.toLocaleString()}</b></span>
                    <span>Certification pairs <b>${progress.certification_discovered_pairs.toLocaleString()}</b></span>
                    <span>Solver-owned memory <b>${solveMemoryLabel(progress.live_owned_bytes)}</b></span>
                    <span>Lower <b>${progressLower}</b></span>
                    <span>Upper <b>${progressUpper}</b></span>
                    <span>Gap <b>${progressGap}</b></span>
                    <span>Factor <b>${progressFactor}</b></span>
                </section>`
                : "";
        const summary = this.solveSummary;
        const resultMarkup = summary
            ? solveResultMarkup({
                  summary,
                  admittedActionIds: this.solveAdmittedActionIds,
                  excludedActions: this.solveExcludedActions,
                  missingPriceKeys: this.solveMissingPriceKeys,
                  economyLabel: summary.economy
                      ? economyIdentityLabel(summary.economy)
                      : null,
                  terminationDetail: this.solveStopDetail,
                  productActionScope: "goal_relevant",
                  goalProgressGatedReforges: true,
                  considerImprintPrograms: this.solveConsiderImprintPrograms,
                  hasCompiledStrategy: this.solvedStrategy !== null,
                  compiledOperationTypes: this.solvedStrategy
                      ? this.solvedStrategy.nodes.flatMap((node) =>
                            node.kind === "operation" && node.operation?.type
                                ? [node.operation.type]
                                : [],
                        )
                      : [],
                  busy: this.busy,
                  verification: this.verification,
                  telemetry: this.solveTelemetry,
              })
            : "";
        const errorMarkup = this.solveError
            ? `<div class="pc-calc-solve-error">
                <strong>${escapeHtml(this.solveError.heading)}</strong>
                <pre>${escapeHtml(this.solveError.detail)}</pre>
            </div>`
            : "";
        const idleMessage = !this.slots.length
            ? "Define a goal to enable Solve."
            : readiness.missingFractureBasePrice
              ? "Set the fresh-base price required for Fracture miss recovery."
            : readiness.pricedActions === 0
              ? "Set at least one action price to enable Solve."
              : this.solveRunning
                ? "Solving — may take a while on large goals."
              : this.verificationRunning
                  ? "Running 10,000 verification simulations."
                  : this.solveCancelled
                    ? "Solve cancelled. Adjust the goal or prices, then start again."
                  : `${readiness.pricedActions.toLocaleString()} of ${readiness.totalActions.toLocaleString()} actions priced.`;
        host.innerHTML = `
            <header class="pc-calc-solve-header">
                <div>
                    <h3>Solve to Strategy</h3>
                    <p>${idleMessage}</p>
                </div>
                ${
                    this.solveRunning
                        ? '<button class="pc-calc-solve-cancel" data-solve-cmd="cancel">Cancel</button>'
                        : `<button class="pc-calc-solve-start" data-solve-cmd="start" ${canStart ? "" : "disabled"}>Start solve</button>`
                }
            </header>
            <div class="pc-calc-solve-targets">
                <label>
                    <span>Absolute gap target <small>chaos</small></span>
                    <input type="number" min="0" step="any" data-solve-target="absolute"
                        value="${this.solveAbsoluteGapTarget || ""}" placeholder="Disabled">
                </label>
                <label>
                    <span>Relative gap target <small>%</small></span>
                    <input type="number" min="0" step="any" data-solve-target="relative"
                        value="${this.solveRelativeGapPercentTarget || ""}" placeholder="Disabled">
                </label>
                <label class="pc-calc-solve-restart-option">
                    <input type="checkbox" data-solve-economic-restart
                        ${this.solveAllowEconomicRestart ? "checked" : ""}>
                    <span>Allow abandoning this item and buying a fresh base</span>
                </label>
                <label class="pc-calc-solve-restart-option">
                    <input type="checkbox" data-solve-consider-imprints
                        ${this.solveConsiderImprintPrograms ? "checked" : ""}>
                    <span>Consider automatic Imprint checkpoint/retry programs</span>
                </label>
                <p>Either positive target may stop the solve after a complete lower/upper round. Targets do not change Bellman comparisons or exact results.</p>
            </div>
            ${
                readiness.missingFractureBasePrice
                    ? '<p class="pc-calc-solve-warning"><strong>Fracture needs a replacement base:</strong> set the <code>base</code> price below so a missed fracture has a priced recovery route.</p>'
                    : ""
            }
            ${progressMarkup}
            ${
                solveCostKeys.length
                    ? `<details class="pc-calc-solve-price-table">
                        <summary>Action price table · ${solveMissingKeys.length.toLocaleString()} missing</summary>
                        <label class="pc-calc-price-fallback">
                            <span>Unquoted real-action fallback</span>
                            <input type="number" min="0" step="any" data-price-fallback
                                value="${fallbackPrice ?? ""}" placeholder="Disabled">
                            <small>Must be greater than zero. Used only for engine-listed cost keys after overrides and certified recipes.</small>
                        </label>
                        <div class="pc-calc-solve-price-rows">${priceTable.rowsHtml}</div>
                        <p class="pc-help">Every row discloses its source. Missing prices exclude dependent actions; they are never treated as zero.</p>
                    </details>`
                    : ""
            }
            ${resultMarkup}
            ${errorMarkup}`;

        host.querySelectorAll<HTMLInputElement>("[data-price-key]").forEach(
            (input) => {
                input.addEventListener("change", () => {
                    const key = input.dataset.priceKey ?? "";
                    const value = Number(input.value);
                    setPrice(key, input.value === "" ? null : value);
                });
            },
        );
        host.querySelector<HTMLInputElement>("[data-price-fallback]")
            ?.addEventListener("change", (event) => {
                const input = event.currentTarget as HTMLInputElement;
                const value = Number(input.value);
                setFallbackPrice(input.value === "" ? null : value);
            });
        host.querySelectorAll<HTMLInputElement>("[data-solve-target]").forEach(
            (input) => {
                input.addEventListener("change", () => {
                    const value = Number(input.value);
                    const target =
                        input.value !== "" && Number.isFinite(value) && value > 0
                            ? value
                            : 0;
                    if (input.dataset.solveTarget === "absolute") {
                        this.solveAbsoluteGapTarget = target;
                    } else {
                        this.solveRelativeGapPercentTarget = target;
                    }
                    if (target === 0) input.value = "";
                });
            },
        );
        host.querySelector<HTMLInputElement>("[data-solve-economic-restart]")
            ?.addEventListener("change", (event) => {
                this.solveAllowEconomicRestart =
                    (event.currentTarget as HTMLInputElement).checked;
            });
        host.querySelector<HTMLInputElement>("[data-solve-consider-imprints]")
            ?.addEventListener("change", (event) => {
                this.solveConsiderImprintPrograms =
                    (event.currentTarget as HTMLInputElement).checked;
            });
        host.querySelectorAll<HTMLButtonElement>("[data-solve-cmd]").forEach(
            (button) => {
                button.addEventListener("click", () => {
                    const command = button.dataset.solveCmd;
                    if (command === "cancel") {
                        this.cancelSolve();
                        return;
                    }
                    void this.guard(async () => {
                        if (command === "start") await this.startSolve();
                        if (command === "open") await this.openSolvedStrategy();
                        if (command === "verify") {
                            await this.verifySolvedStrategy();
                        }
                    });
                });
            },
        );
    }

    private renderExactResult(calc: CalcResult): string {
        return `<section class="pc-calc-answer">
            <span class="pc-calc-answer-kicker">Exact result</span>
            <span class="pc-calc-answer-target">${escapeHtml(this.successTargetLabel())}</span>
            <strong class="pc-calc-answer-value">${formatProbabilityExact(calc.success_probability)}</strong>
            <span class="pc-calc-answer-action">after ${escapeHtml(this.actionLabel(this.actionId))}</span>
            <div class="pc-calc-answer-details">
                <span>
                    <small>Engine probability</small>
                    <strong>p = ${formatRawProbability(calc.success_probability)}</strong>
                </span>
                <span>
                    <small>Failure chance</small>
                    <strong>${formatProbabilityExact(1 - calc.success_probability)}</strong>
                </span>
                <span>
                    <small>Expected attempts</small>
                    <strong>${formatExpectedAttempts(calc.success_probability)}</strong>
                </span>
            </div>
            <p class="pc-calc-answer-note">Expected attempts assumes every try starts from this same input item.</p>
        </section>`;
    }

    private renderBestiaryResult(calc: BestiaryCalculation): string {
        const result = calc.result;
        const checkpoint = result.checkpoint_present
            ? "Present after this action"
            : result.consumed_checkpoint_count
              ? "Consumed by restoration"
              : "Absent after this action";
        const consumption = result.consumed_price_keys.length
            ? result.consumed_price_keys.map(escapeHtml).join(" + ")
            : "Nothing consumed";
        const refusal = result.applied
            ? ""
            : `<p class="pc-calc-error"><strong>Engine refusal:</strong> ${escapeHtml(result.refusal_reason)} (${escapeHtml(result.refusal_key)})</p>
               <p class="pc-help">The live item, checkpoint, and costs are preserved.</p>`;
        const cost = result.cost_keys.length
            ? this.renderCost(calc.probability)
            : `<section class="pc-calc-section pc-calc-cost"><h4>Cost</h4><p>Restoration is beast-free.</p></section>`;
        return `<section class="pc-calc-answer">
            <span class="pc-calc-answer-kicker">Deterministic engine result</span>
            <strong class="pc-calc-answer-value">${formatProbabilityExact(calc.probability)}</strong>
            <span class="pc-calc-answer-action">${escapeHtml(this.actionLabel(result.action_id))}</span>
            <div class="pc-calc-answer-details">
                <span><small>Applied</small><strong>${result.applied ? "Yes" : "No"}</strong></span>
                <span><small>Checkpoint</small><strong>${checkpoint}</strong></span>
                <span><small>Consumption</small><strong>${consumption}</strong></span>
            </div>
            ${refusal}
        </section>${cost}`;
    }

    private renderCost(successProbability: number): string {
        const keys = this.selectedCostKeys();
        if (keys.length === 0) {
            return "";
        }
        const counts = new Map<string, number>();
        for (const key of keys) {
            counts.set(key, (counts.get(key) ?? 0) + 1);
        }
        const priced = presentExpectedConsumption(
            Array.from(counts, ([key, quantity]) => ({ key, quantity })),
            getActionPrice,
            (key) => getActionPriceResolution(key).source,
        );
        const spendPerSuccess = estimatedActionSpendPerSuccess(
            priced.total,
            successProbability,
        );
        return `<section class="pc-calc-section pc-calc-cost">
            <h4>Cost estimates</h4>
            ${priced.rowsHtml}
            <div class="pc-calc-cost-metrics">
                <span>
                    <small>Cost per attempt</small>
                    <strong>${priced.complete ? formatChaosValue(priced.total) : "set prices above"}</strong>
                </span>
                <span>
                    <small>Estimated action spend per success</small>
                    <strong>${
                        priced.complete
                            ? Number.isFinite(spendPerSuccess)
                                ? formatChaosValue(spendPerSuccess)
                                : "No finite estimate"
                            : "set prices above"
                    }</strong>
                </span>
            </div>
            <p class="pc-help pc-calc-cost-note">Uses ${formatExpectedAttempts(successProbability)} attempts at the current success rate. Base, reset, cleanup, and recovery costs are not included unless they are part of the selected action.</p>
        </section>`;
    }

    private renderOutcomes(calc: CalcResult): string {
        const required = this.effectiveMinSatisfiedSlots();
        const rarityCode = RARITY_CODES[this.goalRarity];
        const satisfiedCount = (outcome: CalcOutcome) =>
            this.slots.reduce(
                (count, _, index) =>
                    count + (outcome.slots[index] === 2 ? 1 : 0),
                0,
            );
        const isSuccess = (outcome: CalcOutcome) =>
            outcome.rarity === rarityCode &&
            satisfiedCount(outcome) >= required;
        const probabilityWhere = (
            predicate: (outcome: CalcOutcome) => boolean,
        ) =>
            calc.outcomes.reduce(
                (sum, outcome) =>
                    predicate(outcome) ? sum + outcome.probability : sum,
                0,
            );

        const coverage = Array(this.slots.length + 1).fill(0) as number[];
        for (const outcome of calc.outcomes) {
            coverage[satisfiedCount(outcome)] += outcome.probability;
        }
        const coverageRows = coverage
            .map((probability, count) => ({ count, probability }))
            .reverse()
            .map(({ count, probability }) => {
                const qualifies = count >= required;
                const width = Math.max(0, Math.min(100, probability * 100));
                return `<div class="pc-calc-coverage-row ${qualifies ? "is-qualifying" : ""}">
                    <span class="pc-calc-coverage-label">${count} of ${this.slots.length} goal modifiers</span>
                    ${qualifies ? '<span class="pc-calc-threshold-badge">threshold</span>' : ""}
                    <span class="pc-calc-coverage-value">${formatProbabilityExact(probability)}</span>
                    <span class="pc-calc-coverage-track"><span style="width:${width}%"></span></span>
                </div>`;
            })
            .join("");

        const missSignals = [
            {
                label: "Below modifier threshold",
                probability: probabilityWhere(
                    (outcome) => satisfiedCount(outcome) < required,
                ),
            },
            {
                label: "At least one goal below the required tier",
                probability: probabilityWhere((outcome) =>
                    this.slots.some(
                        (_, index) => outcome.slots[index] === 1,
                    ),
                ),
            },
            {
                label: "At least one goal absent",
                probability: probabilityWhere((outcome) =>
                    this.slots.some(
                        (_, index) => outcome.slots[index] === 0,
                    ),
                ),
            },
            {
                label: "Goal family blocked by another modifier",
                probability: probabilityWhere((outcome) =>
                    this.slots.some(
                        (_, index) =>
                            outcome.slots[index] === 0 &&
                            Boolean((outcome.blocked >> index) & 1),
                    ),
                ),
            },
            {
                label: `Finished item is not ${titleCase(this.goalRarity)}`,
                probability: probabilityWhere(
                    (outcome) => outcome.rarity !== rarityCode,
                ),
            },
        ].filter((signal) => signal.probability > 1e-12);
        const missRows = missSignals.length
            ? missSignals
                  .map(
                      (signal) => `<div class="pc-calc-miss-row">
                        <span>${escapeHtml(signal.label)}</span>
                        <strong>${formatProbabilityExact(signal.probability)}</strong>
                    </div>`,
                  )
                  .join("")
            : '<p class="pc-help">No miss signals in the returned distribution.</p>';

        const sorted = [...calc.outcomes].sort((a, b) => {
            const successOrder = Number(isSuccess(b)) - Number(isSuccess(a));
            if (successOrder !== 0) return successOrder;
            const coverageOrder = satisfiedCount(b) - satisfiedCount(a);
            return coverageOrder || b.probability - a.probability;
        });
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
                (outcome) => `<tr class="${isSuccess(outcome) ? "is-success" : ""}">
                    <td class="pc-calc-p">${formatProbabilityExact(outcome.probability)}</td>
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
        const slotLegend = this.slots
            .map(
                (slot, index) =>
                    `<span><strong>G${index + 1}</strong> ${escapeHtml(this.slotLabel(slot))}</span>`,
            )
            .join("");
        return `<section class="pc-calc-section">
            <h4>Goal coverage</h4>
            <div class="pc-calc-coverage">${coverageRows}</div>
            <p class="pc-help pc-calc-coverage-help">Modifier coverage only; finished rarity is included in the exact success result.</p>
            <div class="pc-calc-misses">
                <h5>Miss signals <span>can overlap</span></h5>
                ${missRows}
            </div>
            <details class="pc-calc-technical">
                <summary>
                    <span>Technical distribution</span>
                    <span>${calc.outcomes.length} abstract classes</span>
                </summary>
                <div class="pc-calc-technical-body">
                    <div class="pc-calc-goal-legend">${slotLegend}</div>
                    <table class="pc-calc-table">
                        <thead><tr>
                            <th>Chance</th><th>Rarity</th><th>Affixes</th>
                            ${slotHeaders}<th>Flags</th>
                        </tr></thead>
                        <tbody>${rows}</tbody>
                    </table>
                    ${
                        restCount > 0
                            ? `<p class="pc-help">&hellip;and ${restCount} more classes totalling ${formatProbabilityExact(restProbability)}.</p>`
                            : ""
                    }
                </div>
            </details>
        </section>`;
    }

    // --- shell / chrome -----------------------------------------------------

    private get modPool(): PcModPool {
        return this.querySelector("pc-mod-pool")!;
    }

    private get inputModList(): PcModList | null {
        return this.querySelector<PcModList>(
            'pc-mod-list[data-role="input-item"]',
        );
    }

    private get goalModList(): PcModList | null {
        return this.querySelector<PcModList>(
            'pc-mod-list[data-role="goal-item"]',
        );
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
            "button[data-cmd], button[data-solve-cmd], button[data-craft-panel], button[data-select-action], button[data-derive-action], button[data-fossil-add]",
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
            this.renderSolvePanel();
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
                <div class="pc-craft-bar pc-calc-toolbar">
                    <span class="pc-calc-workbench-title">Calculator workbench</span>
                    <span class="pc-help">Select the input item or goal to control the shared modifier pool.</span>
                    <span class="pc-calc-status" hidden></span>
                </div>
                <div class="pc-advanced-crafts"></div>
                <div class="pc-calc-body">
                    <aside class="pc-calc-contexts" role="tablist" aria-label="Modifier pool context">
                        <article class="pc-calc-context-card pc-calc-input-context"
                            data-context-card="input" role="tab" tabindex="0">
                            <header class="pc-calc-context-header">
                                <h3>Input item</h3>
                                <span class="pc-calc-context-state">Select</span>
                            </header>
                            <pc-mod-list data-role="input-item"></pc-mod-list>
                            <div class="pc-calc-input-actions">
                                <button data-cmd="change-base">Change base…</button>
                                <span class="pc-calc-new-item">
                                    <select data-role="fresh-rarity" aria-label="New item rarity">
                                        ${["normal", "magic", "rare"]
                                            .map(
                                                (rarity) =>
                                                    `<option value="${rarity}" ${rarity === this.freshRarity ? "selected" : ""}>${titleCase(rarity)}</option>`,
                                            )
                                            .join("")}
                                    </select>
                                    <button data-cmd="new-item">New item</button>
                                </span>
                            </div>
                            <p class="pc-help pc-calc-import-help">Emulator and Stash items enter Calculator through their Odds action.</p>
                        </article>
                        <article class="pc-calc-context-card pc-calc-goal"
                            data-context-card="goal" role="tab" tabindex="0">
                            <header class="pc-calc-context-header">
                                <h3>Goal item</h3>
                                <span class="pc-calc-context-state">Select</span>
                            </header>
                            <div class="pc-calc-goal-controls">
                                <label>
                                    <span>Finished rarity</span>
                                    <select data-role="goal-rarity">
                                        <option value="normal">Normal</option>
                                        <option value="magic">Magic</option>
                                        <option value="rare">Rare</option>
                                    </select>
                                </label>
                                <label>
                                    <span>Success means</span>
                                    <select data-role="success-threshold"></select>
                                </label>
                            </div>
                            <pc-mod-list data-role="goal-item"></pc-mod-list>
                        </article>
                    </aside>
                    <section class="pc-calc-pool">
                        <pc-mod-pool></pc-mod-pool>
                    </section>
                    <section class="pc-calc-results">
                        <h3>Odds</h3>
                        <div class="pc-calc-output"></div>
                    </section>
                    <section class="pc-calc-solve">
                        <div class="pc-calc-solve-panel"></div>
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
        this.querySelector<HTMLSelectElement>(
            '[data-role="success-threshold"]',
        )?.addEventListener("change", (event) => {
            this.minSatisfiedSlots = Number(
                (event.currentTarget as HTMLSelectElement).value,
            );
            this.normalizeSuccessThreshold();
            void this.guard(() => this.goalChanged());
        });
        this.modPool.addEventListener("craft-mod", (event) => {
            const detail = (
                event as CustomEvent<{ key: string; side: "prefix" | "suffix" }>
            ).detail;
            if (this.activeContext === "input") {
                void this.guard(() =>
                    this.addInputMod(detail.key, detail.side),
                );
            } else {
                this.addGoalFromPool(detail.key);
            }
        });
        this.modPool.addEventListener("remove-mod", (event) => {
            if (this.activeContext !== "input") return;
            const detail = (
                event as CustomEvent<{
                    modId: number;
                    side: "prefix" | "suffix";
                }>
            ).detail;
            void this.guard(() =>
                this.removeInputMod(detail.modId, detail.side),
            );
        });
        this.modPool.addEventListener("fracture-mod", (event) => {
            if (this.activeContext !== "input") return;
            const detail = (
                event as CustomEvent<{
                    key: string;
                    modId: number;
                    side: "prefix" | "suffix";
                    onItem: boolean;
                }>
            ).detail;
            void this.guard(() =>
                this.fractureInputMod(
                    detail.key,
                    detail.modId,
                    detail.side,
                    detail.onItem,
                ),
            );
        });
        this.modPool.addEventListener("tab-change", () => {
            void this.guard(() => this.refresh());
        });
        this.inputModList?.addEventListener("fracture-mod", (event) => {
            this.selectContext("input");
            const detail = (
                event as CustomEvent<{
                    key: string;
                    modId: number;
                    side: "prefix" | "suffix";
                }>
            ).detail;
            void this.guard(() =>
                this.fractureInputMod(
                    detail.key,
                    detail.modId,
                    detail.side,
                    true,
                ),
            );
        });
        this.goalModList?.addEventListener("target-tier-change", (event) => {
            this.selectContext("goal");
            const detail = (
                event as CustomEvent<{
                    familyModKey: string;
                    minTier: number;
                }>
            ).detail;
            const slot = this.slots.find(
                (entry) => entry.familyModKey === detail.familyModKey,
            );
            if (!slot) return;
            slot.minTier = detail.minTier;
            this.syncModPoolSelections();
            void this.guard(() => this.goalChanged());
        });
        this.goalModList?.addEventListener("target-remove", (event) => {
            this.selectContext("goal");
            const detail = (
                event as CustomEvent<{
                    familyModKey?: string;
                    slotIndex?: number;
                }>
            ).detail;
            const index = detail.familyModKey
                ? this.slots.findIndex(
                      (slot) => slot.familyModKey === detail.familyModKey,
                  )
                : (detail.slotIndex ?? -1);
            if (index < 0 || index >= this.slots.length) return;
            const oldSlotCount = this.slots.length;
            const followedAll =
                this.effectiveMinSatisfiedSlots() === oldSlotCount;
            this.slots = this.slots.filter((_, slotIndex) => slotIndex !== index);
            this.normalizeSuccessThreshold(followedAll);
            this.syncModPoolSelections();
            void this.guard(() => this.goalChanged());
        });
        this.querySelectorAll<HTMLElement>("[data-context-card]").forEach(
            (card) => {
                const context = card.dataset.contextCard as "input" | "goal";
                card.addEventListener("click", () => this.selectContext(context));
                card.addEventListener("keydown", (event) => {
                    if (event.key === "Enter" || event.key === " ") {
                        event.preventDefault();
                        this.selectContext(context);
                    }
                });
            },
        );
        this.selectContext(this.activeContext);
        this.renderItem();
        this.renderGoal();
        this.renderActionPanels();
        this.renderResults();
        this.renderSolvePanel();
        this.setBusy(this.busy);
    }

    private baseDisplayName(): string {
        return (
            this.bases.find((base) => base.path === this.base)?.name ??
            baseLabel(this.base)
        );
    }

    private renderBaseSummary(): void {
        const summary = this.querySelector<HTMLElement>(".pc-calc-base");
        if (summary) {
            summary.textContent = `${this.baseDisplayName()} · iLvl ${this.itemLevel}`;
        }
    }
}

function panelForAction(id: string): CraftPanel {
    if (id.startsWith("bestiary:")) return "bestiary";
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

function baseLabel(path: string): string {
    return path.split("/").pop() ?? path;
}

function engineErrorDetail(error: unknown): string {
    if (error instanceof EngineError) return error.detail;
    return error instanceof Error ? error.message : String(error);
}

function solveBoundLabel(value: number | null): string {
    return value !== null && Number.isFinite(value) && value < 1e12
        ? formatChaosValue(value)
        : "Pending";
}

function economyIdentityLabel(economy: EconomyIdentity): string {
    const cutoff = economy.source_cutoff_at_utc
        ? new Date(economy.source_cutoff_at_utc).toLocaleString()
        : "manual prices";
    return `${economy.league_name} · pinned ${cutoff}`;
}

function escapeHtml(text: string): string {
    return text
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;");
}

customElements.define("pc-calculator", PcCalculator);
