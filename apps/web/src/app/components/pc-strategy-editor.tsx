import { itemSnapshotCluster } from "../workspace/persistence";
import { strategyClusterConfiguration } from "../strategy-model";
import { disposeReact, renderReact } from "../react-host";
import { StrategyShell } from "./document-shells";
import { EditHistory, historyShortcut } from "../edit-history";
import { modTextLabel } from "../mod-text";
/*
 * Strategy Builder document. Owns graph authoring state, manual Stash saves,
 * Emulator start-state import, validation, and runs through the native Phase
 * 10 simulator exposed by EngineClient.
 */

import { EngineClient } from "../engine-client";
import {
    BaseInfo,
    Catalog,
    EngineError,
    EngineErrorInfo,
    ModInfo,
    StrategyEvalResult,
    StrategyEvalProgress,
    StrategyResult,
} from "../engine-protocol";
import { getEngine } from "../engine-service";
import { beginDiagnosticRun } from '../tester-diagnostics';
import {
    HARVEST_AUGMENT,
    HARVEST_REFORGE,
    harvestTagsFor,
} from "../harvest-crafts";
import { pinEconomy } from "../workspace/prices";
import type { PinnedEconomy } from "../workspace/economy-service";
import {
    buildModifierOptions,
    type ModifierFamilyOption,
} from "../modifier-options";
import {
    StrategyDocument,
    StrategyEdge,
    StrategyLabelContext,
    StrategyNode,
    StrategyPosition,
    StrategyValidationIssue,
    StrategyViewport,
    cloneStrategy,
    pinStrategyFeeder,
    createBlankStrategy,
    createStrategyFromItemSnapshot,
    isStrategyDocument,
    nextGraphId,
    operationLabel,
    automaticStrategyEdgeLabel,
    automaticStrategyNodeLabel,
    validateStrategy,
} from "../strategy-model";
import {
    StrategyDraftRecord,
    StrategyHistoryState,
    StrategyStashRecord,
    getStrategyDraft,
    putStrategyDraft,
    listStash,
    type ItemStashRecord,
} from "../workspace/persistence";
import { workspace } from "../workspace/registry";
import { openTextModal } from "../workspace/dirty-modal";
import {
    StrategyBuilderMode,
    buildStrategyBoardAnnotations,
    buildSolverCostAnnotations,
    normalizeStrategyBuilderMode,
    strategyStructuralSignature,
} from "../strategy-eval-presentation";
import { autoLayoutStrategy } from "../strategy-layout";
import { BasePickerSelection, PcBasePicker } from "./pc-base-picker";
import { ComboOption, PcCombobox } from "./pc-combobox";
import "./pc-base-picker";
import "./pc-combobox";
import { PcConditionEditor } from "./pc-condition-editor";
import { PcRunTrace } from "./pc-run-trace";
import {
    BOARD_MIN_ZOOM,
    PcStrategyBoard,
    TraceHighlight,
} from "./pc-strategy-board";
import { NODE_WIDTH, estimateEdgeCardSize } from "./pc-edge-layer";
import { PcSimulator } from "./pc-simulator";
import { PcStrategyOdds } from "./pc-strategy-odds";
import "./pc-condition-editor";
import "./pc-run-trace";
import "./pc-simulator";
import "./pc-strategy-board";
import "./pc-strategy-odds";

type Selection = { kind: "node" | "edge"; id: string } | null;

const OPERATIONS = [
    "invoke_feeder",
    "move_resource",
    "discard_resource",
    "recombination",
    "acquire_resource",
    "awakener",
    "foulborn_augment",
    "foulborn_regal",
    "foulborn_exalt",
    "vaal",
    "dominance",
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
    "essence",
    "fossil",
    "bench",
    "veiled_chaos",
    "veiled_exalt",
    "unveil",
    "harvest_reforge",
    "harvest_augment",
    "harvest_resist",
    "eldritch_ember",
    "eldritch_ichor",
    "eldritch_exalt",
    "bestiary:imprint",
    "bestiary:restore_imprint",
    "eldritch_chaos",
    "eldritch_annul",
    "influence_exalt",
    "fracture",
    "condition_check_only",
    "restart",
];

const PALETTE: Array<[string, string]> = [
    ["start", "Start state"],
    ["operation:invoke_feeder", "Run saved feeder"],
    ["operation:move_resource", "Move / recycle item"],
    ["operation:discard_resource", "Discard item"],
    ["operation:recombination", "Recombine pair (held)"],
    ["operation:acquire_resource", "Acquire donor"],
    ["operation:awakener", "Awakener's Orb"],
    ["operation:foulborn_augment", "Foulborn Augmentation"],
    ["operation:foulborn_regal", "Foulborn Regal"],
    ["operation:foulborn_exalt", "Foulborn Exalted"],
    ["operation:vaal", "Vaal Orb"],
    ["operation:dominance", "Orb of Dominance"],
    ["operation:restart", "Restart · fresh base"],
    ["operation:transmute", "Orb of Transmutation"],
    ["operation:chaos", "Chaos Orb"],
    ["operation:alchemy", "Alchemy Orb"],
    ["operation:alteration", "Alteration Orb"],
    ["operation:augment", "Augmentation Orb"],
    ["operation:regal", "Regal Orb"],
    ["operation:exalt", "Exalted Orb"],
    ["operation:annul", "Orb of Annulment"],
    ["operation:bestiary:imprint", "Create Imprint"],
    ["operation:bestiary:restore_imprint", "Restore Imprint"],
    ["operation:scour", "Orb of Scouring"],
    ["operation:remove_crafted_modifiers", "Remove crafted modifiers"],
    ["operation:fracture", "Fracturing Orb"],
    ["operation:essence", "Essence"],
    ["operation:fossil", "Fossils"],
    ["operation:bench", "Bench craft"],
    ["operation:harvest_reforge", "Harvest reforge"],
    ["operation:harvest_augment", "Harvest augment"],
    ["operation:harvest_resist", "Harvest resistance"],
    ["operation:veiled_chaos", "Veiled chaos"],
    ["operation:veiled_exalt", "Veiled exalt"],
    ["operation:unveil", "Unveil"],
    ["operation:eldritch_ember", "Eldritch ember"],
    ["operation:eldritch_ichor", "Eldritch ichor"],
    ["operation:eldritch_exalt", "Eldritch exalt"],
    ["operation:eldritch_chaos", "Eldritch chaos"],
    ["operation:eldritch_annul", "Eldritch annul"],
    ["operation:influence_exalt", "Influenced exalt"],
    ["router", "Condition router"],
    ["terminal:success", "Success terminal"],
    ["terminal:failure", "Failure terminal"],
    ["terminal:stop", "Stop terminal"],
];

export class PcStrategyEditor extends HTMLElement {
    private resourceOptions: ItemStashRecord[] = [];
    private feederOptions: StrategyStashRecord[] = [];
    private shellVersion = 0;
    private client!: EngineClient;
    private dataId = 0;
    private bases: BaseInfo[] = [];
    private catalog: Catalog | null = null;
    private essenceOptions: ComboOption[] = [];
    private fossilOptions: ComboOption[] = [];
    private benchOptions: ComboOption[] = [];
    private sessionBenchOptions: ComboOption[] = [];
    private harvestReforgeOptions: ComboOption[] = [];
    private harvestAugmentOptions: ComboOption[] = [];
    private influenceOptions: ComboOption[] = [];
    private fossilNames = new Map<string, string>();
    private modifierOptions: ModifierFamilyOption[] = [];
    private modifierFamilyLabels = new Map<number, string>();
    private unveilOptions: ComboOption[] = [];
    private modifierBaseKey = "";
    private modifierLoading = false;
    private docId = "";
    private strategy: StrategyDocument = createBlankStrategy();
    private undoHistory = new EditHistory<StrategyHistoryState>();
    private savedStateKey: string | null = null;
    private editGroup = "";
    private draggingNode = false;
    private issues: StrategyValidationIssue[] = [];
    private selection: Selection = null;
    private highlight: TraceHighlight = {
        nodeIds: new Set(),
        edgeIds: new Set(),
        activeNodeId: null,
    };
    private result: StrategyResult | null = null;
    private traceResult: StrategyResult | null = null;
    private progress: { done: number; total: number } | null = null;
    private dirty = true;
    private savedRef: string | null = null;
    private savedName: string | null = null;
    private running = false;
    private engineReady = false;
    private runController: AbortController | null = null;
    private currentRun: Promise<void> | null = null;
    private persistTimer = 0;
    private progressFrame = 0;
    private disposed = false;
    private connectedOnce = false;
    private hasChosenBase = false;
    private mode: StrategyBuilderMode = "simulator";
    private structuralSignature = "";
    private evalResult: StrategyEvalResult | null = null;
    private evalEconomy: PinnedEconomy | null = null;
    private evalError: EngineErrorInfo | null = null;
    private evalInvalid = false;
    private evaluating = false;
    private evalProgressText: string | null = null;
    private evalStartedAt = 0;
    private evalController: AbortController | null = null;
    private evalStale = false;
    private evalTimer = 0;
    private evalRequestVersion = 0;
    private currentEvaluation: Promise<void> | null = null;
    private evaluationPromises = new Set<Promise<void>>();

    async connectedCallback(): Promise<void> {
        if (this.connectedOnce) {
            this.updateView();
            return;
        }
        this.connectedOnce = true;
        this.tabIndex = -1;
        this.addEventListener("keydown", event => {
            const command = historyShortcut(event);
            if (!command || this.draggingNode) return;
            event.preventDefault();
            this.restoreHistory(this.undoHistory.cursor + (command === "undo" ? -1 : 1));
        });
        this.addEventListener("focusout", () => { this.editGroup = ""; });
        this.docId =
            this.getAttribute("doc-id") ?? `strategy-${crypto.randomUUID()}`;
        this.renderShell();
        this.issues = validateStrategy(this.strategy);
        workspace().registerDocument(this.docId, {
            save: () => this.save(),
            dispose: () => this.disposeDocument(),
        });
        this.setStatus("Loading engine…");
        try {
            const engine = await getEngine();
            if (this.disposed) return;
            this.client = engine.client;
            this.dataId = engine.dataId;
            this.engineReady = true;
            this.bases = (await this.client.listBases(this.dataId)).filter(
                (base) => base.support === 0,
            );
            this.catalog = await this.client.catalog(this.dataId);
            const stash = await listStash();
            this.feederOptions = stash.filter((record): record is StrategyStashRecord => record.resourceType === "strategy" && isStrategyDocument(record.strategy));
            this.resourceOptions = stash.filter((record): record is ItemStashRecord =>
                record.resourceType !== "strategy" && !Number((record.state as {lifecycle?: number})?.lifecycle ?? 0));
            this.essenceOptions = this.catalog.essences.map((entry) => ({
                value: entry.key,
                label: entry.name,
            }));
            this.fossilOptions = this.catalog.fossils.map((entry) => ({
                value: entry.key,
                label: entry.name,
            }));
            this.benchOptions = this.catalog.bench.map((entry) => ({
                value: entry.key,
                label: entry.name,
            }));
            this.harvestReforgeOptions = harvestTagsFor(
                this.catalog.harvestTags,
                HARVEST_REFORGE,
            ).map((entry) => ({ value: entry.key, label: entry.name }));
            this.harvestAugmentOptions = harvestTagsFor(
                this.catalog.harvestTags,
                HARVEST_AUGMENT,
            ).map((entry) => ({ value: entry.key, label: entry.name }));
            this.influenceOptions = this.catalog.influences.map((entry) => ({
                value: entry.key,
                label: entry.name,
            }));
            this.fossilNames = new Map(
                this.catalog.fossils.map((entry) => [entry.key, entry.name]),
            );
            const draft = await getStrategyDraft(this.docId);
            await this.restore(draft);
            this.undoHistory.restore(draft?.undoHistory, this.historyState());
            this.savedStateKey = draft?.savedStateKey ?? (!this.dirty ? JSON.stringify(this.historyState()) : null);
            this.structuralSignature = strategyStructuralSignature(this.strategy);
            if (this.disposed) return;
            this.renderShell();
            this.issues = validateStrategy(this.strategy);
            await this.persist();
            workspace().notifyDirty(this.docId, this.dirty, this.docTitle);
            this.setStatus("");
            this.updateView();
            void this.ensureModifiers();
            if (this.mode === "calculator") this.requestEvaluation(0);
        } catch (error) {
            this.setStatus(error instanceof Error ? error.message : String(error));
        }
    }

    private get docTitle(): string {
        return this.strategy.name.trim() || this.savedName || "Untitled strategy";
    }

    private async restore(draft?: StrategyDraftRecord): Promise<void> {
        this.mode = normalizeStrategyBuilderMode(draft?.builderMode);
        this.savedRef = draft?.savedRef ?? null;
        this.savedName = draft?.savedName ?? null;
        this.dirty = draft?.dirty ?? true;
        if (draft?.strategy && isStrategyDocument(draft.strategy)) {
            this.strategy = cloneStrategy(draft.strategy);
            this.hasChosenBase = draft.undoHistory?.entries[draft.undoHistory.cursor]?.hasChosenBase ?? true;
            return;
        }
        if (draft?.sourceItem) {
            const session = await this.client.createSession(
                this.dataId,
                draft.sourceItem.base,
                draft.sourceItem.itemLevel,
                itemSnapshotCluster(draft.sourceItem),
            );
            try {
                const count = await this.client.modCount(session);
                const mods: ModInfo[] = await Promise.all(
                    Array.from({ length: count }, (_, id) =>
                        this.client.modInfo(session, id),
                    ),
                );
                this.strategy = createStrategyFromItemSnapshot(
                    draft.sourceItem,
                    (id) => mods[id]?.key,
                );
            } finally {
                await this.client.closeSession(session);
            }
            this.dirty = true;
            this.hasChosenBase = true;
            return;
        }
        const preferred =
            this.bases.find((base) => base.path === this.strategy.base_state.base_key)
                ?.path ??
            this.bases[0]?.path ??
            this.strategy.base_state.base_key;
        this.strategy = createBlankStrategy(preferred, 86);
        this.hasChosenBase = false;
    }

    private openBasePicker(): void {
        const start =
            this.strategy.nodes.find(
                (node) => node.id === this.strategy.start_node_id,
            ) ?? this.strategy.nodes.find((node) => node.kind === "start");
        if (!start) {
            this.addNode("start", this.nextOpenPosition());
            return;
        }
        this.selection = { kind: "node", id: start.id };
        this.setStatus("");
        this.updateView();
    }

    private applyBaseSelection(selection: BasePickerSelection): void {
        const changed =
            selection.base !== this.strategy.base_state.base_key ||
            selection.itemLevel !== this.strategy.base_state.item_level ||
            JSON.stringify(selection.cluster) !== JSON.stringify(strategyClusterConfiguration(this.strategy));
        this.strategy.base_state.base_key = selection.base;
        this.strategy.base_state.item_level = selection.itemLevel;
        this.strategy.base_state.cluster = selection.cluster ? {passive_key: selection.cluster.passiveKey, passive_count: selection.cluster.passiveCount} : undefined;
        this.strategy.base_state.with_implicits = true;
        if (changed && this.hasChosenBase) {
            this.strategy.base_state.prefixes = [];
            this.strategy.base_state.suffixes = [];
        }
        this.hasChosenBase = true;
        this.modifierBaseKey = "";
        this.modifierOptions = [];
        this.modifierFamilyLabels.clear();
        this.unveilOptions = [];
        this.sessionBenchOptions = [];
        this.markChanged();
        void this.ensureModifiers();
    }

    /**
     * Build the "Has modifier" dropdown for the current base. Enumerates the
     * base's session mods and groups them into display families exactly the way
     * the emulator's mod pool does (by engine family id, across every source —
     * base, influenced, crafted, essence, fossil), so the same complete mod list
     * is offered here. Each option's stored value is the family's stable group
     * key (what the engine's has_mod_group condition resolves). Cached per
     * base/iLvl and pushed to a live condition editor when it finishes.
     */
    private async ensureModifiers(): Promise<void> {
        if (!this.engineReady || !this.catalog || this.modifierLoading) return;
        const base = this.strategy.base_state.base_key;
        const itemLevel = this.strategy.base_state.item_level;
        const key = `${base}|${itemLevel}|${JSON.stringify(this.strategy.base_state.cluster ?? null)}`;
        if (key === this.modifierBaseKey && this.modifierOptions.length) return;
        this.modifierLoading = true;
        try {
            const session = await this.client.createSession(
                this.dataId,
                base,
                itemLevel,
                strategyClusterConfiguration(this.strategy),
            );
            try {
                const count = await this.client.modCount(session);
                const mods: ModInfo[] = await Promise.all(
                    Array.from({ length: count }, (_, id) =>
                        this.client.modInfo(session, id),
                    ),
                );
                if (this.disposed) return;
                if (`${this.strategy.base_state.base_key}|${this.strategy.base_state.item_level}` !== key) return;
                this.modifierOptions = buildModifierOptions(mods, this.catalog);
                this.modifierFamilyLabels = familyLabelsById(mods);
                this.unveilOptions = mods
                    .filter((mod) => mod.reach_kind === 7)
                    .map((mod) => ({
                        value: mod.key,
                        label: modTextLabel(mod.text_lines) || mod.key,
                    }))
                    .sort((a, b) => a.label.localeCompare(b.label));
                this.sessionBenchOptions = mods
                    .filter((mod) => mod.reach_kind === 2)
                    .map((mod) => ({
                        value: mod.key,
                        label: modTextLabel(mod.text_lines) || mod.key,
                    }))
                    .sort((a, b) => a.label.localeCompare(b.label));
                this.modifierBaseKey = key;
            } finally {
                await this.client.closeSession(session);
            }
        } catch {
            // Leave the dropdown empty if the session can't be built; the user
            // can still author other condition types or use advanced JSON.
        } finally {
            this.modifierLoading = false;
            if (!this.disposed && `${this.strategy.base_state.base_key}|${this.strategy.base_state.item_level}` !== key) {
                void this.ensureModifiers();
            }
        }
        if (this.disposed) return;
        this.querySelector<PcConditionEditor>(
            "pc-condition-editor",
        )?.setModifierFamilies(this.modifierOptions);
        if (this.isConnected) this.updateView();
    }

    private renderShell(): void {
        renderReact(this, <StrategyShell key={++this.shellVersion} palette={PALETTE} />);
        this.bindShell();
    }

    private bindShell(): void {
        this.querySelectorAll<HTMLButtonElement>("[data-palette]").forEach(
            (button) => {
                button.addEventListener("dragstart", (event) => {
                    event.dataTransfer?.setData(
                        "application/x-poecraft-node",
                        button.dataset.palette ?? "",
                    );
                });
                button.addEventListener("dblclick", () => {
                    this.addNode(
                        button.dataset.palette ?? "",
                        this.nextOpenPosition(),
                    );
                });
            },
        );
        this.querySelectorAll<HTMLButtonElement>("[data-cmd]").forEach((button) => {
            button.addEventListener("click", () => {
                const cmd = button.dataset.cmd;
                if (cmd === "undo" || cmd === "redo") this.restoreHistory(this.undoHistory.cursor + (cmd === "undo" ? -1 : 1));
                else if (cmd === "save") void this.save();
                else if (cmd === "save-as") void this.saveAs();
                else if (cmd === "duplicate")
                    void workspace().openStrategy(cloneStrategy(this.strategy));
                else if (cmd === "change-base") this.openBasePicker();
                else if (cmd === "delete") this.deleteSelection();
                else if (cmd === "auto-layout") this.autoLayout();
                else if (cmd === "fit-view") this.fitView();
            });
        });
        this.querySelectorAll<HTMLButtonElement>(".pc-strategy-mode [data-mode]").forEach(
            (button) => {
                button.addEventListener("click", () => {
                    this.setMode(
                        normalizeStrategyBuilderMode(button.dataset.mode),
                    );
                });
            },
        );
        this.board.addEventListener("strategy-select", (event) => {
            this.selection = (event as CustomEvent<Selection>).detail;
            this.updateView();
        });
        this.board.addEventListener("strategy-add-node", (event) => {
            const detail = (
                event as CustomEvent<{
                    paletteType: string;
                    position: StrategyPosition;
                }>
            ).detail;
            this.addNode(detail.paletteType, detail.position);
        });
        this.board.addEventListener("strategy-node-drag-start", () => { this.draggingNode = true; });
        this.board.addEventListener("strategy-node-drag-end", () => {
            this.draggingNode = false;
            this.markChanged(false);
        });
        this.board.addEventListener("strategy-edge-reconnect", event => {
            const {id, endpoint, nodeId} = (event as CustomEvent<{id: string; endpoint: "from" | "to"; nodeId: string}>).detail;
            const edge = this.strategy.edges.find(edge => edge.id === id);
            if (!edge || edge[endpoint] === nodeId || !this.strategy.nodes.some(node => node.id === nodeId)) return;
            edge[endpoint] = nodeId;
            this.selection = {kind: "edge", id};
            this.markChanged();
        });
        this.board.addEventListener("strategy-node-move", (event) => {
            const detail = (
                event as CustomEvent<{
                    id: string;
                    position: StrategyPosition;
                }>
            ).detail;
            const node = this.strategy.nodes.find((entry) => entry.id === detail.id);
            if (!node) return;
            node.position = detail.position;
            this.markChanged(true, false);
        });
        this.board.addEventListener("strategy-edge-create", (event) => {
            const detail = (
                event as CustomEvent<{ from: string; to: string }>
            ).detail;
            this.addEdge(detail.from, detail.to);
        });
        this.board.addEventListener("strategy-delete-selection", () =>
            this.deleteSelection(),
        );
        this.board.addEventListener("strategy-viewport-change", (event) => {
            this.strategy.ui ??= {};
            this.strategy.ui.viewport = (
                event as CustomEvent<StrategyViewport>
            ).detail;
            this.schedulePersist();
        });
        this.simulator.addEventListener("strategy-run", (event) => {
            const detail = (
                event as CustomEvent<{
                    count: number;
                    maxActionsPerRun: number;
                }>
            ).detail;
            this.currentRun = this.run(
                detail.count,
                detail.maxActionsPerRun,
            );
        });
        this.simulator.addEventListener("strategy-cancel", () =>
            this.runController?.abort(),
        );
        this.odds.addEventListener("strategy-recost", () => {
            if (!this.evalResult) return;
            this.requestEvaluation(0);
        });
        this.trace.addEventListener("trace-highlight", (event) => {
            const detail = (
                event as CustomEvent<{
                    nodeIds: string[];
                    edgeIds: string[];
                    activeNodeId: string | null;
                }>
            ).detail;
            this.highlight = {
                nodeIds: new Set(detail.nodeIds),
                edgeIds: new Set(detail.edgeIds),
                activeNodeId: detail.activeNodeId,
            };
            this.board.setView(
                this.strategy,
                this.selection,
                this.issues,
                this.highlight,
                this.currentAnnotations,
                this.labelContext,
            );
        });
    }

    private updateView(): void {
        if (!this.isConnected) return;
        this.syncHistoryButtons();
        this.issues = validateStrategy(this.strategy);
        this.board.setView(
            this.strategy,
            this.selection,
            this.issues,
            this.highlight,
            this.currentAnnotations,
            this.labelContext,
        );
        const invalid =
            !this.engineReady ||
            this.issues.some((issue) => issue.severity === "error");
        this.simulator.setView({
            running: this.running,
            disabled: invalid,
            progress: this.progress,
            result: this.result,
        });
        if (this.traceResult !== this.result) {
            this.traceResult = this.result;
            this.trace.setResult(this.result);
        }
        this.syncModeAndOddsView();
        const saved = this.querySelector(".pc-strategy-saved")!;
        saved.textContent = this.savedRef
            ? `${this.dirty ? "Modified" : "Saved"}: ${this.savedName}`
            : "Unsaved";
        this.renderInspector();
        this.renderValidation();
    }

    private syncModeAndOddsView(): void {
        const simulatorSurface = this.querySelector<HTMLElement>(
            ".pc-strategy-simulator-surface",
        );
        if (!simulatorSurface) return;
        simulatorSurface.hidden = this.mode !== "simulator";
        this.odds.hidden = this.mode !== "calculator";
        this.querySelectorAll<HTMLButtonElement>(".pc-strategy-mode [data-mode]").forEach(
            (button) => {
                const active = button.dataset.mode === this.mode;
                button.classList.toggle("is-active", active);
                button.setAttribute("aria-pressed", String(active));
            },
        );
        const selectedNode =
            this.selection?.kind === "node"
                ? this.strategy.nodes.find(
                      (node) => node.id === this.selection!.id,
                  )
                : undefined;
        this.odds.setView({
            result: this.evalResult,
            error: this.evalError,
            invalid: this.evalInvalid,
            evaluating: this.evaluating,
            progressText: this.evalProgressText,
            stale: this.evalStale,
            selectedNodeId: selectedNode?.id ?? null,
            selectedNodeKind: selectedNode?.kind ?? null,
            targetLabels: this.evalTargetLabels(),
            prices: this.evalEconomy?.snapshot.prices ?? {},
            economy: this.evalResult?.economy ?? null,
        });
    }

    private renderInspector(): void {
        const host = this.querySelector(".pc-inspector-content")!;
        if (!this.selection) {
            host.innerHTML = `
                <h3>Strategy</h3>
                <label class="pc-field">
                    <span>Name</span>
                    <input data-field="strategy-name" value="${escapeAttribute(this.strategy.name)}">
                </label>
                <label class="pc-field">
                    <span>Description</span>
                    <textarea data-field="strategy-description">${escapeHtml(this.strategy.description)}</textarea>
                </label>
                <dl class="pc-strategy-meta">
                    <div><dt>Base</dt><dd>${escapeHtml(
                        this.bases.find(
                            (base) =>
                                base.path === this.strategy.base_state.base_key,
                        )?.name ?? baseLabel(this.strategy.base_state.base_key),
                    )}</dd></div>
                    <div><dt>Item level</dt><dd>${this.strategy.base_state.item_level}</dd></div>
                    <div><dt>Nodes</dt><dd>${this.strategy.nodes.length}</dd></div>
                    <div><dt>Edges</dt><dd>${this.strategy.edges.length}</dd></div>
                </dl>`;
            host
                .querySelector<HTMLInputElement>('[data-field="strategy-name"]')
                ?.addEventListener("change", (event) => {
                    this.strategy.name = (
                        event.currentTarget as HTMLInputElement
                    ).value;
                    this.markChanged();
                });
            host
                .querySelector<HTMLTextAreaElement>(
                    '[data-field="strategy-description"]',
                )
                ?.addEventListener("change", (event) => {
                    this.strategy.description = (
                        event.currentTarget as HTMLTextAreaElement
                    ).value;
                    this.markChanged();
                });
            return;
        }
        if (this.selection.kind === "node") {
            const node = this.strategy.nodes.find(
                (entry) => entry.id === this.selection!.id,
            );
            if (!node) {
                this.selection = null;
                this.renderInspector();
                return;
            }
            host.innerHTML = this.nodeInspector(node);
            this.bindNodeInspector(node, host as HTMLElement);
            return;
        }
        const edge = this.strategy.edges.find(
            (entry) => entry.id === this.selection!.id,
        );
        if (!edge) {
            this.selection = null;
            this.renderInspector();
            return;
        }
        host.innerHTML = `
            <div class="pc-inspector-heading">
                <div>
                    <h3>Edge</h3>
                    <div class="pc-inspector-id">${escapeHtml(edge.from)} → ${escapeHtml(edge.to)}</div>
                </div>
            </div>
            <pc-condition-editor></pc-condition-editor>
            <details class="pc-edge-routing">
                <summary>Routing order and board label</summary>
                <div class="pc-edge-routing-fields">
                    <label class="pc-field">
                        <span>Board label</span>
                        <input data-field="edge-label" value="${escapeAttribute(edge.label ?? "")}"
                            placeholder="${escapeAttribute(automaticStrategyEdgeLabel(edge))}">
                    </label>
                    <label class="pc-field">
                        <span>Priority</span>
                        <input type="number" data-field="edge-priority" value="${edge.priority}">
                    </label>
                </div>
            </details>`;
        host
            .querySelector<HTMLInputElement>('[data-field="edge-label"]')
            ?.addEventListener("input", (event) => {
                edge.label = (event.currentTarget as HTMLInputElement).value;
                this.onLabelEdited();
            });
        host
            .querySelector<HTMLInputElement>('[data-field="edge-priority"]')
            ?.addEventListener("change", (event) => {
                edge.priority = Number(
                    (event.currentTarget as HTMLInputElement).value,
                );
                this.markChanged();
            });
        const editor = host.querySelector<PcConditionEditor>(
            "pc-condition-editor",
        )!;
        editor.setEdge(edge);
        editor.setModifierFamilies(this.modifierOptions);
        editor.addEventListener("condition-change", (event) => {
            const changed = (event as CustomEvent<StrategyEdge>).detail;
            edge.condition = changed.condition;
            edge.is_default = changed.is_default;
            this.onConditionEdited();
        });
    }

    /**
     * Apply a condition edit without re-rendering the inspector — the condition
     * editor owns and re-renders its own builder, so rebuilding it here would
     * reset its state mid-edit.
     */
    private onConditionEdited(): void {
        this.markChanged(false);
        this.board.setView(
            this.strategy,
            this.selection,
            this.issues,
            this.highlight,
            this.currentAnnotations,
            this.labelContext,
        );
        const edge =
            this.selection?.kind === "edge"
                ? this.strategy.edges.find(
                      (entry) => entry.id === this.selection!.id,
                  )
                : undefined;
        const labelInput = this.querySelector<HTMLInputElement>(
            '[data-field="edge-label"]',
        );
        if (edge && labelInput) {
            labelInput.placeholder = automaticStrategyEdgeLabel(edge);
        }
        this.simulator.setView({
            running: this.running,
            disabled:
                !this.engineReady ||
                this.issues.some((issue) => issue.severity === "error"),
            progress: this.progress,
            result: this.result,
        });
        this.syncModeAndOddsView();
        this.renderValidation();
    }

    private nodeInspector(node: StrategyNode): string {
        const common = `
            <h3>${escapeHtml(node.kind)} node</h3>
            <div class="pc-inspector-id">${escapeHtml(node.id)}</div>
            <label class="pc-field">
                <span>Display name</span>
                <input data-field="node-name" value="${escapeAttribute(node.name ?? "")}"
                    placeholder="${escapeAttribute(automaticStrategyNodeLabel(node, this.labelContext))}">
            </label>`;
        if (node.kind === "start") {
            return `${common}
                <pc-base-picker allow-clusters compact confirm-label="Apply base"></pc-base-picker>
                <label class="pc-field">
                    <span>Rarity</span>
                    <select data-field="start-rarity">
                        ${["normal", "magic", "rare"]
                            .map(
                                (rarity) =>
                                    `<option ${rarity === this.strategy.base_state.rarity ? "selected" : ""}>${rarity}</option>`,
                            )
                            .join("")}
                    </select>
                </label>
                <label class="pc-field"><span>Memory strands</span><input data-field="start-memory" type="number" min="0" max="100" step="1" value="${this.strategy.base_state.memory_strands ?? 0}"></label>
                <p class="pc-help">Strand-bearing random crafts and solving are unavailable while their laws remain unresolved.</p>
                <label class="pc-field"><span>Donor template from Stash</span><select data-donor-template>${this.resourceOptions.map(record => `<option value="${escapeAttribute(record.id)}">${escapeHtml(record.name)}</option>`).join("")}</select></label>
                <button data-add-donor ${this.resourceOptions.length ? "" : "disabled"}>Add donor template</button>
                <p class="pc-help">Templates do not consume Stash items. Add Acquire donor before Awakener; every acquisition has a separate price key. Missing prices remain unknown.</p>
                ${(this.strategy.resources ?? []).map(resource => `<label class="pc-field"><span>${escapeHtml(resource.id)} · ${escapeHtml(resource.name ?? "Donor")} acquisition price key</span><input data-resource-price="${escapeAttribute(resource.id)}" value="${escapeAttribute(resource.acquisition_price_key)}"></label>`).join("")}
                <label class="pc-field"><span>Saved feeder and output contract</span><select data-feeder-template>${this.feederOptions.flatMap(record => (record.strategy as StrategyDocument).output_contracts?.map(contract => `<option value="${escapeAttribute(JSON.stringify([record.id, contract.id]))}">${escapeHtml(record.name)} · ${escapeHtml(contract.name ?? contract.id)}</option>`) ?? []).join("")}</select></label>
                <button data-add-feeder>Add paid feeder slot</button>
                <p class="pc-help">Each invocation buys a fresh starting item and runs the pinned child. Failure, cost limits and output mismatches remain visible. Move or discard an occupied output before invoking again.</p>
                ${(this.strategy.resources ?? []).filter(resource => resource.feeder).map(resource => `<p class="pc-help">${escapeHtml(resource.id)}: ${escapeHtml(resource.feeder!.strategy_id)} · revision ${escapeHtml(resource.feeder!.revision)} · ${escapeHtml(resource.feeder!.output_contract_id)}</p>`).join("")}
                <button data-add-output-contract>Add output contract</button>
                ${(this.strategy.output_contracts ?? []).map(contract => `<details><summary>${escapeHtml(contract.name ?? contract.id)} · ${escapeHtml(contract.base_key)}</summary><pc-condition-editor data-output-contract="${escapeAttribute(contract.id)}"></pc-condition-editor></details>`).join("")}
                <div class="pc-start-mod-summary">
                    ${(this.strategy.base_state.prefixes?.length ?? 0)} prefixes ·
                    ${(this.strategy.base_state.suffixes?.length ?? 0)} suffixes
                </div>`;
        }
        if (node.kind === "operation") {
            const type = node.operation?.type ?? "chaos";
            const params = node.operation?.params ?? {};
            const harvestOptions =
                type === "harvest_augment"
                    ? this.harvestAugmentOptions
                    : this.harvestReforgeOptions;
            return `${common}
                <label class="pc-field">
                    <span>Operation</span>
                    <select data-field="operation">
                        ${OPERATIONS.map(
                            (operation) =>
                                `<option value="${operation}" ${operation === type ? "selected" : ""}>${escapeHtml(operationLabel({ type: operation, params: {} }))}</option>`,
                        ).join("")}
                    </select>
                </label>
                ${type === "awakener" || type === "acquire_resource" || type === "invoke_feeder" || type === "discard_resource" ? `<label class="pc-field"><span>${type === "awakener" ? "Donor resource" : "Resource slot"}</span><select data-field="resource-id"><option value="">Choose slot</option>${type === "discard_resource" ? `<option value="current" ${params.resource_id === "current" ? "selected" : ""}>current</option>` : ""}${(this.strategy.resources ?? []).map(resource => `<option value="${escapeAttribute(resource.id)}" ${resource.id === (type === "awakener" ? (params.roles as {donor?: string} | undefined)?.donor : params.resource_id) ? "selected" : ""}>${escapeHtml(resource.id)} · ${escapeHtml(resource.name ?? "Donor")}</option>`).join("")}</select></label><p class="pc-help">${type === "awakener" ? "Consumes an acquired donor; receiver is the current item." : type === "invoke_feeder" ? "Buys a fresh start, runs the pinned child under the parent limits and checks its actual output. The slot must be empty." : type === "discard_resource" ? "Destroys this live item without resale credit or replacement." : "Acquires one replacement donor and charges its acquisition price. It does not replace an available donor."}</p>` : ""}
                ${type === "move_resource" || type === "recombination" ? (type === "move_resource" ? ["from", "to"] : ["input_a", "input_b", "output"]).map(field => `<label class="pc-field"><span>${escapeHtml(field.replaceAll("_", " "))}</span><select data-slot-field="${field}"><option value="">Choose slot</option>${["current", ...(this.strategy.resources ?? []).map(resource => resource.id)].map(id => `<option value="${escapeAttribute(id)}" ${params[field] === id ? "selected" : ""}>${escapeHtml(id)}</option>`).join("")}</select></label>`).join("") + `<p class="pc-help">${type === "move_resource" ? "Moves the real item without acquiring a replacement. Source must be live and destination empty; moving to current requires the current base and item level." : "Consumes both inputs and creates one output. Execution is held until the native pair Apply is qualified; mixed-base outputs keep their actual base."}</p>` : ""}
                ${
                    type === "essence"
                        ? `<label class="pc-field">
                            <span>Essence</span>
                            <pc-combobox data-field="essence-combo" placeholder="Search essences…"></pc-combobox>
                        </label>`
                        : ""
                }
                ${
                    type === "fossil"
                        ? `<div class="pc-field">
                            <span>Fossils</span>
                            <div class="pc-chip-row">${this.renderFossilChips(
                                params.fossils as string[] | undefined,
                            )}</div>
                            <pc-combobox data-field="fossil-combo" placeholder="Add fossil…"></pc-combobox>
                        </div>`
                        : ""
                }
                ${
                    type === "bench" || type === "unveil"
                        ? `<label class="pc-field">
                            <span>${type === "bench" ? "Bench modifier" : "Unveil choice"}</span>
                            <pc-combobox data-field="mod-key-combo" placeholder="Search modifiersâ€¦"></pc-combobox>
                        </label>`
                        : ""
                }
                ${
                    type === "harvest_reforge" || type === "harvest_augment"
                        ? `<label class="pc-field">
                            <span>Harvest tag</span>
                            <select data-field="target-tag">
                                ${harvestOptions
                                    .map(
                                        (option) =>
                                            `<option value="${escapeAttribute(option.value)}" ${option.value === params.target_tag ? "selected" : ""}>${escapeHtml(option.label)}</option>`,
                                    )
                                    .join("")}
                            </select>
                        </label>`
                        : ""
                }
                ${
                    type === "harvest_resist"
                        ? `${this.renderResistanceSelect("source-tag", "From", String(params.source_tag ?? "fire"))}
                           ${this.renderResistanceSelect("target-tag", "To", String(params.target_tag ?? "cold"))}`
                        : ""
                }
                ${
                    type === "eldritch_ember" || type === "eldritch_ichor"
                        ? `<label class="pc-field">
                            <span>Tier</span>
                            <select data-field="eldritch-tier">
                                ${[1, 2, 3, 4]
                                    .map(
                                        (tier) =>
                                            `<option value="${tier}" ${tier === Number(params.tier ?? 1) ? "selected" : ""}>${tier}</option>`,
                                    )
                                    .join("")}
                            </select>
                        </label>`
                        : ""
                }
                ${
                    type === "influence_exalt"
                        ? `<label class="pc-field">
                            <span>Influence</span>
                            <select data-field="influence">
                                ${this.influenceOptions
                                    .map(
                                        (option) =>
                                            `<option value="${escapeAttribute(option.value)}" ${option.value === params.influence ? "selected" : ""}>${escapeHtml(option.label)}</option>`,
                                    )
                                    .join("")}
                            </select>
                        </label>`
                        : ""
                }`;
        }
        if (node.kind === "terminal") {
            return `${common}
                <label class="pc-field">
                    <span>Result</span>
                    <select data-field="terminal">
                        ${["success", "failure", "stop"]
                            .map(
                                (terminal) =>
                                    `<option ${terminal === node.terminal ? "selected" : ""}>${terminal}</option>`,
                            )
                            .join("")}
                    </select>
                </label>
                <label class="pc-field">
                    <span>Reason</span>
                    <input data-field="terminal-reason" value="${escapeAttribute(node.reason ?? "")}">
                </label>`;
        }
        return `${common}<p class="pc-help">Routes by edge conditions without mutating the item.</p>`;
    }

    private renderFossilChips(keys: string[] | undefined): string {
        const list = keys ?? [];
        if (!list.length) {
            return '<span class="pc-help">No fossils selected.</span>';
        }
        return list
            .map(
                (key, index) =>
                    `<span class="pc-chip">${escapeHtml(
                        this.fossilNames.get(key) ?? key,
                    )}<button data-fossil-remove="${index}" title="Remove">×</button></span>`,
            )
            .join("");
    }

    private async addDonorTemplate(stashId: string): Promise<void> {
        const record = this.resourceOptions.find(resource => resource.id === stashId);
        if (!record) return;
        const session = await this.client.createSession(this.dataId, record.base, record.itemLevel, itemSnapshotCluster(record));
        let item = 0;
        try {
            item = await this.client.importItem(record.state, session);
            const snapshot = {...record, state: await this.client.exportItem(item, session)};
            const base_state = createStrategyFromItemSnapshot(snapshot, () => undefined).base_state;
            const id = nextGraphId("donor", (this.strategy.resources ?? []).map(resource => resource.id));
            this.strategy.resources ??= [];
            if (this.strategy.resources.length >= 7) throw new Error("The native runner supports seven resource slots.");
            this.strategy.resources.push({id, name: record.name, base_state, acquisition_price_key: `resource:${id}`});
            this.markChanged();
        } catch (error) {
            this.setStatus(error instanceof Error ? error.message : String(error));
        } finally {
            if (item) await this.client.closeItem(item);
            await this.client.closeSession(session);
        }
    }

    private renderResistanceSelect(
        field: string,
        label: string,
        selected: string,
    ): string {
        return `<label class="pc-field">
            <span>${label}</span>
            <select data-field="${field}">
                ${["fire", "cold", "lightning"]
                    .map(
                        (tag) =>
                            `<option value="${tag}" ${tag === selected ? "selected" : ""}>${tag}</option>`,
                    )
                    .join("")}
            </select>
        </label>`;
    }

    private bindNodeInspector(node: StrategyNode, host: HTMLElement): void {
        host.querySelector<HTMLButtonElement>("[data-add-feeder]")?.addEventListener("click", () => {
            try {
                const selected = JSON.parse(host.querySelector<HTMLSelectElement>("[data-feeder-template]")?.value ?? "null") as [string, string] | null;
                const record = this.feederOptions.find(record => record.id === selected?.[0]);
                if (!record || !selected) return;
                const id = nextGraphId("feeder", (this.strategy.resources ?? []).map(resource => resource.id));
                this.strategy.resources ??= [];
                if (this.strategy.resources.length >= 7) throw new Error("The native runner supports seven resource slots.");
                this.strategy.resources.push(pinStrategyFeeder(id, record, selected[1]));
                this.markChanged();
            } catch (error) { this.setStatus(error instanceof Error ? error.message : String(error)); }
        });
        host.querySelector<HTMLButtonElement>("[data-add-output-contract]")?.addEventListener("click", () => {
            this.strategy.output_contracts ??= [];
            if (this.strategy.output_contracts.length >= 32) { this.setStatus("The native compiler supports 32 output contracts."); return; }
            const id = nextGraphId("output", this.strategy.output_contracts.map(contract => contract.id));
            this.strategy.output_contracts.push({id, base_key: this.strategy.base_state.base_key, predicate: {type: "rarity_is", rarity: this.strategy.base_state.rarity}});
            this.markChanged();
        });
        host.querySelectorAll<PcConditionEditor>("[data-output-contract]").forEach(editor => {
            const contract = this.strategy.output_contracts?.find(contract => contract.id === editor.dataset.outputContract);
            if (!contract) return;
            editor.setEdge({id: contract.id, from: "", to: "", priority: 0, condition: contract.predicate});
            editor.setModifierFamilies(this.modifierOptions);
            editor.addEventListener("condition-change", event => {
                contract.predicate = (event as CustomEvent<StrategyEdge>).detail.condition ?? {type: "always"};
                this.markChanged(false);
            });
        });
        host.querySelectorAll<HTMLSelectElement>("[data-slot-field]").forEach(select => select.addEventListener("change", () => {
            if (node.operation) { node.operation.params[select.dataset.slotField!] = select.value; this.markChanged(); }
        }));
        host.querySelector<HTMLButtonElement>("[data-add-donor]")?.addEventListener("click", () => {
            const selected = host.querySelector<HTMLSelectElement>("[data-donor-template]")?.value;
            if (selected) void this.addDonorTemplate(selected);
        });
        host.querySelectorAll<HTMLInputElement>("[data-resource-price]").forEach(input => input.addEventListener("change", () => {
            const resource = this.strategy.resources?.find(resource => resource.id === input.dataset.resourcePrice);
            if (resource) { resource.acquisition_price_key = input.value.trim() || `resource:${resource.id}`; this.markChanged(); }
        }));
        host.querySelector<HTMLInputElement>("[data-field=start-memory]")?.addEventListener("change", event => {
            const count = Number((event.currentTarget as HTMLInputElement).value);
            if (Number.isInteger(count) && count >= 0 && count <= 100) { this.strategy.base_state.memory_strands = count; this.markChanged(); }
        });
        host.querySelector<HTMLSelectElement>("[data-field=resource-id]")?.addEventListener("change", event => {
            if (!node.operation) return;
            const id = (event.currentTarget as HTMLSelectElement).value;
            node.operation.params = node.operation.type === "awakener" ? {roles: {donor: id, receiver: "current"}} : {resource_id: id};
            this.markChanged();
        });
        host.querySelector<HTMLInputElement>('[data-field="node-name"]')
            ?.addEventListener("input", (event) => {
                node.name = (event.currentTarget as HTMLInputElement).value;
                this.onLabelEdited();
            });
        const basePicker = host.querySelector<PcBasePicker>("pc-base-picker");
        if (basePicker) {
            basePicker.setBases(this.bases);
            basePicker.setSelection(
                this.strategy.base_state.base_key,
                this.strategy.base_state.item_level,
                strategyClusterConfiguration(this.strategy),
            );
            basePicker.addEventListener("confirm", (event) => {
                this.applyBaseSelection(
                    (event as CustomEvent<BasePickerSelection>).detail,
                );
            });
        }
        host.querySelector<HTMLSelectElement>('[data-field="start-rarity"]')
            ?.addEventListener("change", (event) => {
                this.strategy.base_state.rarity = (
                    event.currentTarget as HTMLSelectElement
                ).value as "normal" | "magic" | "rare";
                this.markChanged();
            });
        host.querySelector<HTMLSelectElement>('[data-field="operation"]')
            ?.addEventListener("change", (event) => {
                const type = (event.currentTarget as HTMLSelectElement).value;
                node.operation = {
                    type,
                    params:
                        type === "fossil"
                            ? { fossils: [] }
                            : type === "essence"
                              ? { essence_key: "" }
                              : type === "bench" || type === "unveil"
                                ? { mod_key: "" }
                                : type === "harvest_reforge" ||
                                    type === "harvest_augment"
                                  ? {
                                        target_tag:
                                            (type === "harvest_augment"
                                                ? this.harvestAugmentOptions
                                                : this.harvestReforgeOptions)[0]
                                                ?.value ?? "life",
                                    }
                                  : type === "harvest_resist"
                                    ? {
                                          source_tag: "fire",
                                          target_tag: "cold",
                                      }
                                    : type === "eldritch_ember" ||
                                        type === "eldritch_ichor"
                                      ? { tier: 1 }
                                      : type === "influence_exalt"
                                        ? {
                                              influence:
                                                  this.influenceOptions[0]
                                                      ?.value ?? "crusader",
                                          }
                              : {},
                };
                this.markChanged();
            });
        const essenceCombo = host.querySelector<PcCombobox>(
            'pc-combobox[data-field="essence-combo"]',
        );
        if (essenceCombo) {
            essenceCombo.setOptions(this.essenceOptions);
            essenceCombo.setValue(String(node.operation?.params.essence_key ?? ""));
            essenceCombo.addEventListener("change", (event) => {
                node.operation ??= { type: "essence", params: {} };
                node.operation.params.essence_key = (
                    event as CustomEvent<{ value: string }>
                ).detail.value;
                this.markChanged();
            });
        }
        const fossilCombo = host.querySelector<PcCombobox>(
            'pc-combobox[data-field="fossil-combo"]',
        );
        if (fossilCombo) {
            fossilCombo.setOptions(this.fossilOptions);
            fossilCombo.addEventListener("change", (event) => {
                const value = (event as CustomEvent<{ value: string }>).detail.value;
                if (!value) return;
                node.operation ??= { type: "fossil", params: { fossils: [] } };
                const current =
                    (node.operation.params.fossils as string[] | undefined) ?? [];
                if (!current.includes(value)) {
                    node.operation.params.fossils = [...current, value];
                    this.markChanged();
                }
            });
        }
        const modKeyCombo = host.querySelector<PcCombobox>(
            'pc-combobox[data-field="mod-key-combo"]',
        );
        if (modKeyCombo) {
            modKeyCombo.setOptions(
                node.operation?.type === "bench"
                    ? this.sessionBenchOptions
                    : this.unveilOptions,
            );
            modKeyCombo.setValue(String(node.operation?.params.mod_key ?? ""));
            modKeyCombo.addEventListener("change", (event) => {
                if (!node.operation) return;
                node.operation.params.mod_key = (
                    event as CustomEvent<{ value: string }>
                ).detail.value;
                this.markChanged();
            });
        }
        for (const field of ["target-tag", "source-tag"] as const) {
            host.querySelector<HTMLSelectElement>(`[data-field="${field}"]`)
                ?.addEventListener("change", (event) => {
                    if (!node.operation) return;
                    node.operation.params[field.replace("-", "_")] = (
                        event.currentTarget as HTMLSelectElement
                    ).value;
                    this.markChanged();
                });
        }
        host.querySelector<HTMLSelectElement>('[data-field="eldritch-tier"]')
            ?.addEventListener("change", (event) => {
                if (!node.operation) return;
                node.operation.params.tier = Number(
                    (event.currentTarget as HTMLSelectElement).value,
                );
                this.markChanged();
            });
        host.querySelector<HTMLSelectElement>('[data-field="influence"]')
            ?.addEventListener("change", (event) => {
                if (!node.operation) return;
                node.operation.params.influence = (
                    event.currentTarget as HTMLSelectElement
                ).value;
                this.markChanged();
            });
        host.querySelectorAll<HTMLButtonElement>("[data-fossil-remove]").forEach(
            (button) => {
                button.addEventListener("click", () => {
                    const index = Number(button.dataset.fossilRemove);
                    node.operation ??= { type: "fossil", params: { fossils: [] } };
                    const current =
                        (node.operation.params.fossils as string[] | undefined) ?? [];
                    node.operation.params.fossils = current.filter(
                        (_, i) => i !== index,
                    );
                    this.markChanged();
                });
            },
        );
        host.querySelector<HTMLSelectElement>('[data-field="terminal"]')
            ?.addEventListener("change", (event) => {
                node.terminal = (event.currentTarget as HTMLSelectElement)
                    .value as "success" | "failure" | "stop";
                this.markChanged();
            });
        host.querySelector<HTMLInputElement>('[data-field="terminal-reason"]')
            ?.addEventListener("change", (event) => {
                node.reason = (event.currentTarget as HTMLInputElement).value;
                this.markChanged();
            });
    }

    private renderValidation(): void {
        const host = this.querySelector(".pc-validation")!;
        if (!this.issues.length) {
            host.innerHTML =
                '<h3>Validation</h3><p class="pc-validation-ok">Graph is valid.</p>';
            return;
        }
        // Solver-compiled boards can carry thousands of warnings; rendering a
        // row for each on every view update stalls the page. Errors sort
        // ahead of warnings so nothing blocking hides behind the cap.
        const MAX_ISSUE_ROWS = 80;
        const ranked = this.issues
            .map((issue, index) => ({ issue, index }))
            .sort(
                (a, b) =>
                    Number(b.issue.severity === "error") -
                        Number(a.issue.severity === "error") ||
                    a.index - b.index,
            )
            .slice(0, MAX_ISSUE_ROWS);
        const hidden = this.issues.length - ranked.length;
        host.innerHTML = `
            <h3>Validation <span>${this.issues.length}</span></h3>
            <ul>
                ${ranked
                    .map(
                        ({ issue, index }) => `
                        <li class="is-${issue.severity}">
                            <button data-issue="${index}">
                                <strong>${issue.severity}</strong>
                                <span>${escapeHtml(issue.message)}</span>
                            </button>
                        </li>`,
                    )
                    .join("")}
                ${hidden > 0 ? `<li class="is-warning"><span class="pc-validation-more">… ${hidden.toLocaleString()} more issues not shown</span></li>` : ""}
            </ul>`;
        host.querySelectorAll<HTMLButtonElement>("[data-issue]").forEach(
            (button) => {
                button.addEventListener("click", () => {
                    const issue = this.issues[Number(button.dataset.issue)];
                    if (issue.nodeId) {
                        this.selection = { kind: "node", id: issue.nodeId };
                    } else if (issue.edgeId) {
                        this.selection = { kind: "edge", id: issue.edgeId };
                    }
                    this.updateView();
                });
            },
        );
    }

    private addNode(paletteType: string, position: StrategyPosition): void {
        const nodeIds = this.strategy.nodes.map((node) => node.id);
        let node: StrategyNode;
        if (paletteType === "start") {
            node = {
                id: nextGraphId("start", nodeIds),
                kind: "start",
                name: "Start",
                position,
            };
            if (!this.strategy.nodes.some((entry) => entry.kind === "start")) {
                this.strategy.start_node_id = node.id;
            }
        } else if (paletteType === "router") {
            node = {
                id: nextGraphId("router", nodeIds),
                kind: "router",
                name: "Condition router",
                position,
            };
        } else if (paletteType.startsWith("terminal:")) {
            const terminal = paletteType.split(":")[1] as
                | "success"
                | "failure"
                | "stop";
            node = {
                id: nextGraphId(terminal, nodeIds),
                kind: "terminal",
                name: terminal.charAt(0).toUpperCase() + terminal.slice(1),
                terminal,
                reason: "",
                position,
            };
        } else {
            const type = paletteType.slice("operation:".length) || "chaos";
            const operation = {
                type,
                params:
                    type === "essence"
                        ? { essence_key: "" }
                        : type === "fossil"
                          ? { fossils: [] }
                          : {},
            };
            node = {
                id: nextGraphId(type, nodeIds),
                kind: "operation",
                name: "",
                operation,
                position,
            };
        }
        this.strategy.nodes.push(node);
        this.selection = { kind: "node", id: node.id };
        this.markChanged();
    }

    private addEdge(from: string, to: string): void {
        const ids = this.strategy.edges.map((edge) => edge.id);
        const outgoing = this.strategy.edges.filter((edge) => edge.from === from);
        const edge: StrategyEdge = {
            id: nextGraphId("edge", ids),
            from,
            to,
            priority: outgoing.length,
            condition: { type: "always" },
            label: "",
        };
        this.strategy.edges.push(edge);
        this.selection = { kind: "edge", id: edge.id };
        this.markChanged();
    }

    private deleteSelection(): void {
        if (!this.selection) return;
        if (this.selection.kind === "edge") {
            this.strategy.edges = this.strategy.edges.filter(
                (edge) => edge.id !== this.selection!.id,
            );
        } else {
            const id = this.selection.id;
            this.strategy.nodes = this.strategy.nodes.filter(
                (node) => node.id !== id,
            );
            this.strategy.edges = this.strategy.edges.filter(
                (edge) => edge.from !== id && edge.to !== id,
            );
            const starts = this.strategy.nodes.filter(
                (node) => node.kind === "start",
            );
            if (starts.length === 1) {
                this.strategy.start_node_id = starts[0].id;
            }
        }
        this.selection = null;
        this.markChanged();
    }

    /**
     * Layered (Sugiyama-style) layout: drop back edges, place each node one
     * column after its last predecessor, order rows to reduce crossings, and
     * size the gaps so edge condition cards fit between columns.
     */
    private autoLayout(): void {
        if (!this.strategy.nodes.length) return;
        autoLayoutStrategy(this.strategy, {
            nodeSizes: this.board.measureNodeSizes(),
            edgeCardSize: estimateEdgeCardSize,
        });
        this.markChanged();
        this.fitView();
    }

    /** Pan/zoom the board so the whole graph is visible. */
    private fitView(): void {
        const nodes = this.strategy.nodes;
        if (!nodes.length) return;
        const sizes = this.board.measureNodeSizes();
        let minX = Infinity;
        let minY = Infinity;
        let maxX = -Infinity;
        let maxY = -Infinity;
        for (const node of nodes) {
            const size = sizes.get(node.id) ?? { width: NODE_WIDTH, height: 132 };
            minX = Math.min(minX, node.position.x);
            minY = Math.min(minY, node.position.y);
            maxX = Math.max(maxX, node.position.x + size.width);
            maxY = Math.max(maxY, node.position.y + size.height);
        }
        const viewport = this.board.viewportSize();
        if (!viewport.width || !viewport.height) return;
        const pad = 130;
        const width = maxX - minX + pad * 2;
        const height = maxY - minY + pad * 2;
        const zoom = Math.min(
            1,
            Math.max(
                BOARD_MIN_ZOOM,
                Math.min(viewport.width / width, viewport.height / height),
            ),
        );
        this.strategy.ui ??= {};
        this.strategy.ui.viewport = {
            zoom,
            panX: Math.round(
                (viewport.width - (maxX - minX) * zoom) / 2 - minX * zoom,
            ),
            panY: Math.round(
                (viewport.height - (maxY - minY) * zoom) / 2 - minY * zoom,
            ),
        };
        this.schedulePersist();
        this.updateView();
    }

    private nextOpenPosition(): StrategyPosition {
        const count = this.strategy.nodes.length;
        return {
            x: 180 + (count % 4) * 260,
            y: 140 + Math.floor(count / 4) * 150,
        };
    }

    private setMode(mode: StrategyBuilderMode): void {
        if (this.mode === mode) return;
        this.mode = mode;
        this.schedulePersist();
        if (mode === "calculator") {
            this.requestEvaluation(0);
        } else {
            this.evalRequestVersion += 1;
            this.evalController?.abort();
            window.clearTimeout(this.evalTimer);
            this.evalTimer = 0;
            this.setStatus("");
        }
        this.updateView();
    }

    private requestEvaluation(delay = 300): void {
        this.evalRequestVersion += 1;
        this.evalController?.abort();
        this.scheduleEvaluation(this.evalRequestVersion, delay);
    }

    private scheduleEvaluation(version: number, delay: number): void {
        window.clearTimeout(this.evalTimer);
        this.evalTimer = window.setTimeout(() => {
            this.evalTimer = 0;
            const evaluation = this.evaluateStrategy(version);
            this.currentEvaluation = evaluation;
            this.evaluationPromises.add(evaluation);
            void evaluation.finally(() => {
                this.evaluationPromises.delete(evaluation);
                if (this.currentEvaluation === evaluation) {
                    this.currentEvaluation = null;
                }
            });
        }, delay);
    }

    private async evaluateStrategy(version: number): Promise<void> {
        if (this.disposed || this.mode !== "calculator") return;
        this.issues = validateStrategy(this.strategy);
        if (this.issues.some((issue) => issue.severity === "error")) {
            this.evalInvalid = true;
            this.evalStale = this.evalResult !== null;
            this.setStatus("Fix graph errors before evaluating.");
            this.updateView();
            return;
        }

        const controller = new AbortController();
        this.evalController = controller;
        this.evaluating = true;
        this.evalStartedAt = performance.now();
        this.evalProgressText = "Preparing exact evaluation · 0.0s";
        this.evalInvalid = false;
        this.evalError = null;
        this.setStatus("Evaluating exact graph…");
        this.updateView();
        const pinned = pinEconomy();
        let session = 0;
        const submittedStrategy = structuredClone(this.strategy);
        const report = beginDiagnosticRun('strategy-exact-evaluation', {
            strategy: submittedStrategy, economy: pinned, options: { include_success_normalized: true },
        });
        try {
            session = await this.client.createSession(
                this.dataId,
                submittedStrategy.base_state.base_key,
                submittedStrategy.base_state.item_level,
                strategyClusterConfiguration(submittedStrategy),
            );
            const result = await this.client.strategyEvaluate(
                session,
                submittedStrategy,
                { include_success_normalized: true },
                {
                    signal: controller.signal,
                    economy: pinned.snapshot,
                    onProgress: (progress) => {
                        if (
                            controller.signal.aborted ||
                            this.disposed ||
                            version !== this.evalRequestVersion
                        ) {
                            return;
                        }
                        this.evalProgressText =
                            this.formatEvaluationProgress(progress);
                        this.setStatus(this.evalProgressText);
                        this.syncModeAndOddsView();
                    },
                },
            );
            report.status = controller.signal.aborted ? 'cancelled' : 'completed';
            report.result = structuredClone(result);
            if (
                !this.disposed &&
                this.mode === "calculator" &&
                version === this.evalRequestVersion
            ) {
                result.economy = pinned.identity;
                this.evalResult = result;
                this.evalEconomy = pinned;
                this.evalError = null;
                this.evalStale = false;
                this.evalProgressText = null;
                this.setStatus(
                    result.converged
                        ? "Exact evaluation complete."
                        : "Evaluation ended with unresolved mass.",
                );
            }
        } catch (error) {
            report.status = controller.signal.aborted ? 'cancelled' : 'error';
            report.error = error instanceof Error ? error.message : String(error);
            if (controller.signal.aborted) return;
            if (
                !this.disposed &&
                this.mode === "calculator" &&
                version === this.evalRequestVersion
            ) {
                this.evalResult = null;
                this.evalError =
                    error instanceof EngineError
                        ? { code: error.code, detail: error.detail }
                        : {
                              code: -1,
                              detail:
                                  error instanceof Error
                                      ? error.message
                                      : String(error),
                          };
                this.evalStale = false;
                this.evalProgressText = null;
                this.setStatus(
                    this.evalError.code === 4
                        ? "Exact evaluation refused."
                        : "Exact evaluation could not complete.",
                );
            }
        } finally {
            if (session) await this.client.closeSession(session);
            if (this.evalController === controller) {
                this.evalController = null;
                this.evaluating = false;
                if (controller.signal.aborted) this.evalProgressText = null;
                if (!this.disposed) this.updateView();
            }
        }
    }

    private formatEvaluationProgress(progress: StrategyEvalProgress): string {
        const elapsed = Math.max(
            0,
            (performance.now() - this.evalStartedAt) / 1000,
        ).toFixed(1);
        if (progress.phase === "discovery") {
            return `Discovering exact states · ${progress.discovered_pairs.toLocaleString()} pairs · ${elapsed}s`;
        }
        if (progress.phase === "solving") {
            return `Solving loops · ${progress.solved_sccs.toLocaleString()}/${progress.total_sccs.toLocaleString()} components · ${elapsed}s`;
        }
        if (progress.phase === "fallback") {
            return `Fallback · ${progress.fallback_sweeps.toLocaleString()} sweeps · residual ${progress.residual.toExponential(2)} · ${elapsed}s`;
        }
        if (progress.phase === "finalization") {
            return `Finalizing exact result · ${elapsed}s`;
        }
        return `Exact evaluation complete · ${elapsed}s`;
    }

    private get currentAnnotations() {
        if (this.mode === "calculator" && this.evalResult) {
            return buildStrategyBoardAnnotations(
                this.strategy,
                this.evalResult,
                this.evalStale,
            );
        }
        return buildSolverCostAnnotations(this.strategy);
    }

    private evalTargetLabels(): string[] {
        return (this.evalResult?.targets ?? []).map((target) => {
            if (target.kind === "group") {
                return (
                    this.catalog?.groupNameById[target.group_id] ||
                    `Group ${target.group_id}`
                );
            }
            const family =
                this.modifierFamilyLabels.get(target.family_id) ||
                `Family ${target.family_id}`;
            return target.min_tier > 0
                ? `T${target.min_tier} · ${family}`
                : family;
        });
    }

    private historyState(): StrategyHistoryState {
        return this.historyStateFor(this.strategy);
    }

    private historyStateFor(document: StrategyDocument): StrategyHistoryState {
        const strategy = cloneStrategy(document);
        strategy.ui ??= {};
        delete strategy.ui.viewport; // Pan/zoom do not consume Undo or jump when restoring an edit.
        return {strategy, hasChosenBase: this.hasChosenBase};
    }

    private restoreHistory(index: number): void {
        if (this.disposed || this.draggingNode || index === this.undoHistory.cursor) return;
        const restoreFocus = this.contains(document.activeElement);
        const state = this.undoHistory.go(index);
        if (!state) return;
        this.board.cancelConnection();
        const viewport = this.strategy.ui?.viewport;
        const baseChanged = this.strategy.base_state.base_key !== state.strategy.base_state.base_key ||
            this.strategy.base_state.item_level !== state.strategy.base_state.item_level;
        this.strategy = state.strategy;
        this.strategy.ui ??= {};
        this.strategy.ui.viewport = viewport;
        this.hasChosenBase = state.hasChosenBase;
        if (this.selection && !(this.selection.kind === "edge" ? this.strategy.edges : this.strategy.nodes)
            .some(entry => entry.id === this.selection!.id)) this.selection = null;
        this.editGroup = "";
        if (baseChanged) {
            this.modifierBaseKey = "";
            this.modifierOptions = [];
            this.modifierFamilyLabels.clear();
            this.unveilOptions = [];
            this.sessionBenchOptions = [];
        }
        this.markChanged(true, false);
        if (baseChanged) void this.ensureModifiers();
        if (restoreFocus && document.activeElement === document.body) this.focus({preventScroll: true});
    }

    private syncHistoryButtons(): void {
        for (const command of ["undo", "redo"] as const) {
            const button = this.querySelector<HTMLButtonElement>(`[data-cmd="${command}"]`);
            if (button) button.disabled = !this.engineReady || (command === "undo" ? !this.undoHistory.canUndo : !this.undoHistory.canRedo);
        }
    }

    private markChanged(render = true, record = true, group = ""): void {
        const state = this.historyState();
        if (record) {
            this.undoHistory.record(state, Boolean(group) && this.editGroup === group);
            this.editGroup = group;
        }
        this.syncHistoryButtons();
        const structuralChanged = this.captureStructuralChange();
        this.dirty = JSON.stringify(state) !== this.savedStateKey;
        this.issues = validateStrategy(this.strategy);
        workspace().notifyDirty(this.docId, this.dirty, this.docTitle);
        this.schedulePersist();
        if (structuralChanged) {
            this.evalInvalid = false;
            this.evalStale = this.evalResult !== null;
            if (this.mode === "calculator") this.requestEvaluation();
        }
        if (render) this.updateView();
    }

    /** Update board labels while preserving focus in the authored-text input. */
    private onLabelEdited(): void {
        this.markChanged(false, true, `label:${this.selection?.kind}:${this.selection?.id}`);
        this.board.setView(
            this.strategy,
            this.selection,
            this.issues,
            this.highlight,
            this.currentAnnotations,
            this.labelContext,
        );
        const saved = this.querySelector(".pc-strategy-saved");
        if (saved) {
            saved.textContent = this.savedRef
                ? `${this.dirty ? "Modified" : "Saved"}: ${this.savedName}`
                : "Unsaved";
        }
    }

    private captureStructuralChange(): boolean {
        const nextSignature = strategyStructuralSignature(this.strategy);
        const changed =
            this.structuralSignature !== "" &&
            nextSignature !== this.structuralSignature;
        this.structuralSignature = nextSignature;
        return changed;
    }

    private schedulePersist(): void {
        window.clearTimeout(this.persistTimer);
        this.persistTimer = window.setTimeout(() => {
            void this.persist();
        }, 120);
    }

    private async persist(): Promise<void> {
        if (this.disposed) return;
        await putStrategyDraft({
            docId: this.docId,
            strategy: cloneStrategy(this.strategy),
            sourceItem: null,
            savedRef: this.savedRef,
            savedName: this.savedName,
            dirty: this.dirty,
            builderMode: this.mode,
            undoHistory: this.undoHistory.export(),
            savedStateKey: this.savedStateKey,
            updatedAt: Date.now(),
        });
    }

    private async save(): Promise<boolean> {
        if (!this.savedRef) return this.saveAs();
        const record: StrategyStashRecord = {
            revision: crypto.randomUUID(),
            id: this.savedRef,
            name: this.strategy.name || this.savedName || "Untitled strategy",
            description: this.strategy.description,
            resourceType: "strategy",
            strategy: cloneStrategy(this.strategy),
            createdAt: Date.now(),
        };
        await workspace().saveToStash(record);
        await this.markSaved(record);
        return true;
    }

    private async saveAs(): Promise<boolean> {
        const name = await openTextModal(
            "Save strategy to Stash as:",
            this.strategy.name || this.savedName || "New strategy",
        );
        if (!name) return false;
        this.strategy.name = name;
        this.markChanged();
        const record: StrategyStashRecord = {
            revision: crypto.randomUUID(),
            id: `strategy-${crypto.randomUUID()}`,
            name,
            description: this.strategy.description,
            resourceType: "strategy",
            strategy: cloneStrategy(this.strategy),
            createdAt: Date.now(),
        };
        await workspace().saveToStash(record);
        await this.markSaved(record);
        return true;
    }

    private async markSaved(record: StrategyStashRecord): Promise<void> {
        this.savedRef = record.id;
        this.savedName = record.name;
        this.savedStateKey = JSON.stringify(this.historyStateFor(record.strategy as StrategyDocument));
        this.dirty = this.savedStateKey !== JSON.stringify(this.historyState());
        this.editGroup = "";
        await this.persist();
        workspace().notifyDirty(this.docId, this.dirty, this.docTitle);
        this.updateView();
    }

    private async run(count: number, maxActionsPerRun: number): Promise<void> {
        if (this.running || this.disposed) return;
        this.issues = validateStrategy(this.strategy);
        if (this.issues.some((issue) => issue.severity === "error")) {
            this.setStatus("Fix graph errors before running.");
            this.updateView();
            return;
        }
        this.running = true;
        this.progress = { done: 0, total: count };
        this.runController = new AbortController();
        this.setStatus("Compiling native strategy…");
        this.updateView();
        let session = 0;
        let compiled = 0;
        let simulator = 0;
        let economy = 0;
        const pinned = pinEconomy();
        const submittedStrategy = structuredClone(this.strategy);
        const simulation = {
            target_runs: count, seed: Date.now() >>> 0, max_actions_per_run: maxActionsPerRun,
            max_graph_steps_per_run: 0, retained_trace_count: Math.min(10, count),
            max_trace_entries: Math.min(1000, maxActionsPerRun + 32),
            retained_success_count: 5, retained_failure_count: 5,
        };
        const report = beginDiagnosticRun('strategy-simulation', {
            strategy: submittedStrategy, economy: pinned, simulation,
        });
        try {
            session = await this.client.createSession(
                this.dataId,
                submittedStrategy.base_state.base_key,
                submittedStrategy.base_state.item_level,
                strategyClusterConfiguration(submittedStrategy),
            );
            compiled = await this.client.compileStrategy(
                session,
                submittedStrategy,
            );
            economy = await this.client.loadEconomy(pinned.snapshot);
            simulator = await this.client.createSimulator(
                session,
                compiled,
                economy,
            );
            this.setStatus(`Running ${count.toLocaleString()} simulation${count === 1 ? "" : "s"}…`);
            this.simulator.beginMeasurement();
            this.result = await this.client.runStrategy(
                simulator,
                simulation,
                {
                    chunkSize: count === 1 ? 1 : Math.min(500, count),
                    signal: this.runController.signal,
                    onProgress: (progress) => {
                        this.progress = progress;
                        this.scheduleProgressRender();
                    },
                },
            );
            report.status = this.result.cancelled ? 'cancelled' : 'completed';
            report.result = structuredClone(this.result);
            this.result.economy = pinned.identity;
            this.simulator.endMeasurement();
            this.progress = {
                done: this.result.progress.completed_runs,
                total: this.result.progress.target_runs,
            };
            this.setStatus(this.result.cancelled ? "Run cancelled." : "Run complete.");
        } catch (error) {
            report.status = 'error';
            report.error = error instanceof Error ? error.message : String(error);
            this.result = null;
            this.setStatus(error instanceof Error ? error.message : String(error));
        } finally {
            window.cancelAnimationFrame(this.progressFrame);
            this.progressFrame = 0;
            if (simulator) await this.client.closeSimulator(simulator);
            if (economy) await this.client.closeEconomy(economy);
            if (compiled) await this.client.closeStrategy(compiled);
            if (session) await this.client.closeSession(session);
            this.running = false;
            this.runController = null;
            this.currentRun = null;
            this.updateView();
        }
    }

    private async disposeDocument(): Promise<void> {
        if (this.disposed) return;
        this.disposed = true;
        window.clearTimeout(this.persistTimer);
        window.clearTimeout(this.evalTimer);
        this.evalRequestVersion += 1;
        this.evalController?.abort();
        window.cancelAnimationFrame(this.progressFrame);
        this.progressFrame = 0;
        workspace().unregisterDocument(this.docId);
        this.runController?.abort();
        if (this.currentRun) {
            try {
                await this.currentRun;
            } catch {
                // The run path already reports its own error.
            }
        }
        if (this.evaluationPromises.size) {
            await Promise.allSettled([...this.evaluationPromises]);
        }
        disposeReact(this);
    }

    private setStatus(text: string): void {
        const status = this.querySelector(".pc-strategy-status");
        if (status) status.textContent = text;
    }

    private scheduleProgressRender(): void {
        if (this.progressFrame || this.disposed) return;
        this.progressFrame = window.requestAnimationFrame(() => {
            this.progressFrame = 0;
            if (this.disposed) return;
            this.simulator.setView({
                running: this.running,
                disabled: false,
                progress: this.progress,
                result: this.result,
            });
        });
    }

    private get labelContext(): StrategyLabelContext {
        return {
            catalog: this.catalog,
            modifierNames: new Map(
                [...this.sessionBenchOptions, ...this.unveilOptions].map(
                    (option) => [option.value, option.label],
                ),
            ),
        };
    }

    private get board(): PcStrategyBoard {
        return this.querySelector<PcStrategyBoard>("pc-strategy-board")!;
    }

    private get simulator(): PcSimulator {
        return this.querySelector<PcSimulator>("pc-simulator")!;
    }

    private get trace(): PcRunTrace {
        return this.querySelector<PcRunTrace>("pc-run-trace")!;
    }

    private get odds(): PcStrategyOdds {
        return this.querySelector<PcStrategyOdds>("pc-strategy-odds")!;
    }
}

function baseLabel(path: string): string {
    return path.split("/").pop() ?? path;
}

function familyLabelsById(mods: ModInfo[]): Map<number, string> {
    const labels = new Map<number, { tier: number; label: string }>();
    for (const mod of mods) {
        if (mod.generation_type !== 0 && mod.generation_type !== 1) continue;
        const label = modTextLabel(mod.text_lines) || mod.group_display_name || mod.key;
        const current = labels.get(mod.family_id);
        if (!current || mod.family_tier_index < current.tier) {
            labels.set(mod.family_id, {
                tier: mod.family_tier_index,
                label,
            });
        }
    }
    return new Map(
        Array.from(labels, ([familyId, entry]) => [familyId, entry.label]),
    );
}

// Reach-kind codes from the engine (mirrors pc-mod-pool / engine ModInfo).
function escapeHtml(value: string): string {
    return value
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;");
}

function escapeAttribute(value: string): string {
    return escapeHtml(value).replace(/"/g, "&quot;");
}

customElements.define("pc-strategy-editor", PcStrategyEditor);
