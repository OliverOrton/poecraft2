/*
 * Strategy Builder document. Owns graph authoring state, manual Stash saves,
 * Emulator start-state import, validation, and runs through the native Phase
 * 10 simulator exposed by EngineClient.
 */

import { EngineClient } from "../engine-client";
import { BaseInfo, Catalog, ModInfo, StrategyResult } from "../engine-protocol";
import { getEngine } from "../engine-service";
import {
    StrategyDocument,
    StrategyEdge,
    StrategyNode,
    StrategyPosition,
    StrategyValidationIssue,
    StrategyViewport,
    cloneStrategy,
    createBlankStrategy,
    createStrategyFromItemSnapshot,
    isStrategyDocument,
    nextGraphId,
    operationLabel,
    validateStrategy,
} from "../strategy-model";
import {
    StrategyDraftRecord,
    StrategyStashRecord,
    getStrategyDraft,
    putStrategyDraft,
} from "../workspace/persistence";
import { workspace } from "../workspace/registry";
import { openTextModal } from "../workspace/dirty-modal";
import { BasePickerSelection, PcBasePicker } from "./pc-base-picker";
import { ComboOption, PcCombobox } from "./pc-combobox";
import "./pc-base-picker";
import "./pc-combobox";
import {
    ModifierFamilyOption,
    PcConditionEditor,
} from "./pc-condition-editor";
import { PcRunTrace } from "./pc-run-trace";
import {
    PcStrategyBoard,
    TraceHighlight,
} from "./pc-strategy-board";
import { PcSimulator } from "./pc-simulator";
import "./pc-condition-editor";
import "./pc-run-trace";
import "./pc-simulator";
import "./pc-strategy-board";

type Selection = { kind: "node" | "edge"; id: string } | null;

const OPERATIONS = [
    "transmute",
    "augment",
    "alteration",
    "regal",
    "alchemy",
    "chaos",
    "exalt",
    "annul",
    "scour",
    "essence",
    "fossil",
    "condition_check_only",
];

const PALETTE: Array<[string, string]> = [
    ["start", "Start state"],
    ["operation:chaos", "Chaos Orb"],
    ["operation:alchemy", "Alchemy Orb"],
    ["operation:alteration", "Alteration Orb"],
    ["operation:augment", "Augmentation Orb"],
    ["operation:regal", "Regal Orb"],
    ["operation:exalt", "Exalted Orb"],
    ["operation:annul", "Orb of Annulment"],
    ["operation:scour", "Orb of Scouring"],
    ["operation:essence", "Essence"],
    ["operation:fossil", "Fossils"],
    ["router", "Condition router"],
    ["terminal:success", "Success terminal"],
    ["terminal:failure", "Failure terminal"],
    ["terminal:stop", "Stop terminal"],
];

export class PcStrategyEditor extends HTMLElement {
    private client!: EngineClient;
    private dataId = 0;
    private bases: BaseInfo[] = [];
    private catalog: Catalog | null = null;
    private essenceOptions: ComboOption[] = [];
    private fossilOptions: ComboOption[] = [];
    private fossilNames = new Map<string, string>();
    private modifierOptions: ModifierFamilyOption[] = [];
    private modifierBaseKey = "";
    private modifierLoading = false;
    private docId = "";
    private strategy: StrategyDocument = createBlankStrategy();
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
    private disposed = false;
    private connectedOnce = false;
    private pickerOpen = true;
    private hasChosenBase = false;

    async connectedCallback(): Promise<void> {
        if (this.connectedOnce) return;
        this.connectedOnce = true;
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
            this.essenceOptions = this.catalog.essences.map((entry) => ({
                value: entry.key,
                label: entry.name,
            }));
            this.fossilOptions = this.catalog.fossils.map((entry) => ({
                value: entry.key,
                label: entry.name,
            }));
            this.fossilNames = new Map(
                this.catalog.fossils.map((entry) => [entry.key, entry.name]),
            );
            const draft = await getStrategyDraft(this.docId);
            await this.restore(draft);
            if (this.disposed) return;
            this.renderShell();
            this.issues = validateStrategy(this.strategy);
            await this.persist();
            workspace().notifyDirty(this.docId, this.dirty, this.docTitle);
            this.setStatus("");
            this.updateView();
            void this.ensureModifiers();
        } catch (error) {
            this.setStatus(error instanceof Error ? error.message : String(error));
        }
    }

    private get docTitle(): string {
        return this.strategy.name.trim() || this.savedName || "Untitled strategy";
    }

    private async restore(draft?: StrategyDraftRecord): Promise<void> {
        this.savedRef = draft?.savedRef ?? null;
        this.savedName = draft?.savedName ?? null;
        this.dirty = draft?.dirty ?? true;
        if (draft?.strategy && isStrategyDocument(draft.strategy)) {
            this.strategy = cloneStrategy(draft.strategy);
            this.hasChosenBase = true;
            this.pickerOpen = false;
            return;
        }
        if (draft?.sourceItem) {
            const session = await this.client.createSession(
                this.dataId,
                draft.sourceItem.base,
                draft.sourceItem.itemLevel,
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
            this.pickerOpen = false;
            return;
        }
        const preferred =
            this.bases.find((base) => base.path === this.strategy.base_state.base_key)
                ?.path ??
            this.bases[0]?.path ??
            this.strategy.base_state.base_key;
        this.strategy = createBlankStrategy(preferred, 86);
        this.hasChosenBase = false;
        this.pickerOpen = true;
    }

    private openBasePicker(): void {
        this.pickerOpen = true;
        this.renderShell();
        this.setStatus("");
    }

    private applyBaseSelection(selection: BasePickerSelection): void {
        const changed =
            selection.base !== this.strategy.base_state.base_key ||
            selection.itemLevel !== this.strategy.base_state.item_level;
        this.strategy.base_state.base_key = selection.base;
        this.strategy.base_state.item_level = selection.itemLevel;
        this.strategy.base_state.with_implicits = true;
        if (changed && this.hasChosenBase) {
            this.strategy.base_state.prefixes = [];
            this.strategy.base_state.suffixes = [];
        }
        this.hasChosenBase = true;
        this.pickerOpen = false;
        this.modifierBaseKey = "";
        this.modifierOptions = [];
        this.renderShell();
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
        const key = `${base}|${itemLevel}`;
        if (key === this.modifierBaseKey && this.modifierOptions.length) return;
        this.modifierLoading = true;
        try {
            const session = await this.client.createSession(
                this.dataId,
                base,
                itemLevel,
            );
            try {
                const count = await this.client.modCount(session);
                const mods: ModInfo[] = await Promise.all(
                    Array.from({ length: count }, (_, id) =>
                        this.client.modInfo(session, id),
                    ),
                );
                if (this.disposed) return;
                this.modifierOptions = buildModifierOptions(mods, this.catalog);
                this.modifierBaseKey = key;
            } finally {
                await this.client.closeSession(session);
            }
        } catch {
            // Leave the dropdown empty if the session can't be built; the user
            // can still author other condition types or use advanced JSON.
        } finally {
            this.modifierLoading = false;
        }
        if (this.disposed) return;
        this.querySelector<PcConditionEditor>(
            "pc-condition-editor",
        )?.setModifierFamilies(this.modifierOptions);
    }

    private renderShell(): void {
        if (this.pickerOpen) {
            this.innerHTML = `
                <div class="pc-strategy-editor pc-strategy-picking">
                    <pc-base-picker
                        picker-title="${this.hasChosenBase ? "Change strategy base" : "Choose a strategy base"}"
                        picker-subtitle="Pick the base item and item level the strategy will start from."
                        confirm-label="${this.hasChosenBase ? "Apply base" : "Create strategy"}">
                    </pc-base-picker>
                    <span class="pc-strategy-status"></span>
                </div>`;
            const picker = this.querySelector<PcBasePicker>("pc-base-picker")!;
            picker.setBases(this.bases);
            picker.setSelection(
                this.strategy.base_state.base_key,
                this.strategy.base_state.item_level,
            );
            picker.addEventListener("confirm", (event) => {
                const detail = (event as CustomEvent<BasePickerSelection>).detail;
                this.applyBaseSelection(detail);
            });
            picker.addEventListener("cancel", () => {
                if (!this.hasChosenBase) return;
                this.pickerOpen = false;
                this.renderShell();
                this.updateView();
            });
            return;
        }
        this.innerHTML = `
            <div class="pc-strategy-editor">
                <div class="pc-strategy-toolbar">
                    <strong class="pc-strategy-title">Strategy Builder</strong>
                    <span class="pc-strategy-saved">Unsaved</span>
                    <button data-cmd="save">Save</button>
                    <button data-cmd="save-as">Save As</button>
                    <button data-cmd="duplicate">Duplicate</button>
                    <button data-cmd="change-base">Change base…</button>
                    <button data-cmd="delete">Delete selected</button>
                    <button data-cmd="auto-layout">Auto layout</button>
                    <span class="pc-strategy-status"></span>
                </div>
                <div class="pc-strategy-main">
                    <aside class="pc-strategy-palette">
                        <h3>Palette</h3>
                        <p>Drag onto the board</p>
                        <div class="pc-palette-list">
                            ${PALETTE.map(
                                ([type, label]) =>
                                    `<button draggable="true" data-palette="${type}">${label}</button>`,
                            ).join("")}
                        </div>
                    </aside>
                    <pc-strategy-board></pc-strategy-board>
                    <aside class="pc-strategy-inspector">
                        <div class="pc-inspector-content"></div>
                        <div class="pc-validation"></div>
                    </aside>
                </div>
                <section class="pc-strategy-runner">
                    <pc-simulator></pc-simulator>
                    <pc-run-trace></pc-run-trace>
                </section>
            </div>`;
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
                if (cmd === "save") void this.save();
                else if (cmd === "save-as") void this.saveAs();
                else if (cmd === "duplicate")
                    void workspace().openStrategy(cloneStrategy(this.strategy));
                else if (cmd === "change-base") this.openBasePicker();
                else if (cmd === "delete") this.deleteSelection();
                else if (cmd === "auto-layout") this.autoLayout();
            });
        });
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
            this.markChanged();
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
            this.markChanged(false);
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
            );
        });
    }

    private updateView(): void {
        if (!this.isConnected || this.pickerOpen) return;
        this.issues = validateStrategy(this.strategy);
        this.board.setView(
            this.strategy,
            this.selection,
            this.issues,
            this.highlight,
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
        const saved = this.querySelector(".pc-strategy-saved")!;
        saved.textContent = this.savedRef
            ? `${this.dirty ? "Modified" : "Saved"}: ${this.savedName}`
            : "Unsaved";
        this.renderInspector();
        this.renderValidation();
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
                            placeholder="Uses condition summary when empty">
                    </label>
                    <label class="pc-field">
                        <span>Priority</span>
                        <input type="number" data-field="edge-priority" value="${edge.priority}">
                    </label>
                </div>
            </details>`;
        host
            .querySelector<HTMLInputElement>('[data-field="edge-label"]')
            ?.addEventListener("change", (event) => {
                edge.label = (event.currentTarget as HTMLInputElement).value;
                this.markChanged();
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
        this.dirty = true;
        this.issues = validateStrategy(this.strategy);
        workspace().notifyDirty(this.docId, true, this.docTitle);
        this.schedulePersist();
        this.board.setView(
            this.strategy,
            this.selection,
            this.issues,
            this.highlight,
        );
        this.simulator.setView({
            running: this.running,
            disabled:
                !this.engineReady ||
                this.issues.some((issue) => issue.severity === "error"),
            progress: this.progress,
            result: this.result,
        });
        this.renderValidation();
    }

    private nodeInspector(node: StrategyNode): string {
        const common = `
            <h3>${escapeHtml(node.kind)} node</h3>
            <div class="pc-inspector-id">${escapeHtml(node.id)}</div>
            <label class="pc-field">
                <span>Display name</span>
                <input data-field="node-name" value="${escapeAttribute(node.name ?? "")}">
            </label>`;
        if (node.kind === "start") {
            return `${common}
                <div class="pc-field">
                    <span>Base</span>
                    <div class="pc-start-base">
                        <strong>${escapeHtml(
                            this.bases.find(
                                (base) =>
                                    base.path ===
                                    this.strategy.base_state.base_key,
                            )?.name ??
                                baseLabel(this.strategy.base_state.base_key),
                        )}</strong>
                        <button data-cmd="change-node-base">Change…</button>
                    </div>
                </div>
                <label class="pc-field">
                    <span>Item level</span>
                    <input type="number" min="1" max="100" data-field="item-level" value="${this.strategy.base_state.item_level}">
                </label>
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
                <div class="pc-start-mod-summary">
                    ${(this.strategy.base_state.prefixes?.length ?? 0)} prefixes ·
                    ${(this.strategy.base_state.suffixes?.length ?? 0)} suffixes
                </div>`;
        }
        if (node.kind === "operation") {
            const type = node.operation?.type ?? "chaos";
            const params = node.operation?.params ?? {};
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

    private bindNodeInspector(node: StrategyNode, host: HTMLElement): void {
        host.querySelector<HTMLInputElement>('[data-field="node-name"]')
            ?.addEventListener("change", (event) => {
                node.name = (event.currentTarget as HTMLInputElement).value;
                this.markChanged();
            });
        host.querySelector('[data-cmd="change-node-base"]')?.addEventListener(
            "click",
            () => this.openBasePicker(),
        );
        host.querySelector<HTMLInputElement>('[data-field="item-level"]')
            ?.addEventListener("change", (event) => {
                this.strategy.base_state.item_level = Number(
                    (event.currentTarget as HTMLInputElement).value,
                );
                this.markChanged();
                void this.ensureModifiers();
            });
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
                              : {},
                };
                node.name = operationLabel(node.operation);
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
        host.innerHTML = `
            <h3>Validation <span>${this.issues.length}</span></h3>
            <ul>
                ${this.issues
                    .map(
                        (issue, index) => `
                        <li class="is-${issue.severity}">
                            <button data-issue="${index}">
                                <strong>${issue.severity}</strong>
                                <span>${escapeHtml(issue.message)}</span>
                            </button>
                        </li>`,
                    )
                    .join("")}
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
            const type = paletteType.split(":")[1] || "chaos";
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
                name: operationLabel(operation),
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
            label: "always",
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

    private autoLayout(): void {
        const outgoing = new Map<string, string[]>();
        for (const edge of this.strategy.edges) {
            if (edge.from === edge.to) continue;
            outgoing.set(edge.from, [...(outgoing.get(edge.from) ?? []), edge.to]);
        }
        const levels = new Map<string, number>();
        const pending = [this.strategy.start_node_id];
        levels.set(this.strategy.start_node_id, 0);
        while (pending.length) {
            const id = pending.shift()!;
            const nextLevel = (levels.get(id) ?? 0) + 1;
            for (const target of outgoing.get(id) ?? []) {
                if (!levels.has(target) || nextLevel < levels.get(target)!) {
                    levels.set(target, nextLevel);
                    pending.push(target);
                }
            }
        }
        let orphanLevel = Math.max(0, ...levels.values()) + 1;
        const grouped = new Map<number, StrategyNode[]>();
        for (const node of this.strategy.nodes) {
            const level = levels.get(node.id) ?? orphanLevel;
            grouped.set(level, [...(grouped.get(level) ?? []), node]);
        }
        for (const [level, nodes] of grouped) {
            nodes.forEach((node, index) => {
                node.position = { x: 100 + level * 330, y: 100 + index * 170 };
            });
        }
        orphanLevel += 1;
        void orphanLevel;
        this.markChanged();
    }

    private nextOpenPosition(): StrategyPosition {
        const count = this.strategy.nodes.length;
        return {
            x: 180 + (count % 4) * 260,
            y: 140 + Math.floor(count / 4) * 150,
        };
    }

    private markChanged(render = true): void {
        this.dirty = true;
        this.issues = validateStrategy(this.strategy);
        workspace().notifyDirty(this.docId, true, this.docTitle);
        this.schedulePersist();
        if (render) this.updateView();
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
            updatedAt: Date.now(),
        });
    }

    private async save(): Promise<boolean> {
        if (!this.savedRef) return this.saveAs();
        const record: StrategyStashRecord = {
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
        const record: StrategyStashRecord = {
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
        this.dirty = false;
        await this.persist();
        workspace().notifyDirty(this.docId, false, this.docTitle);
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
        try {
            session = await this.client.createSession(
                this.dataId,
                this.strategy.base_state.base_key,
                this.strategy.base_state.item_level,
            );
            compiled = await this.client.compileStrategy(
                session,
                cloneStrategy(this.strategy),
            );
            simulator = await this.client.createSimulator(session, compiled);
            this.setStatus(`Running ${count.toLocaleString()} simulation${count === 1 ? "" : "s"}…`);
            this.result = await this.client.runStrategy(
                simulator,
                {
                    target_runs: count,
                    seed: Date.now() >>> 0,
                    max_actions_per_run: maxActionsPerRun,
                    max_graph_steps_per_run: 0,
                    retained_trace_count: Math.min(10, count),
                    max_trace_entries: Math.min(1000, maxActionsPerRun + 32),
                    retained_success_count: 5,
                    retained_failure_count: 5,
                },
                {
                    chunkSize: count === 1 ? 1 : Math.min(500, count),
                    signal: this.runController.signal,
                    onProgress: (progress) => {
                        this.progress = progress;
                        this.updateView();
                    },
                },
            );
            this.progress = {
                done: this.result.progress.completed_runs,
                total: this.result.progress.target_runs,
            };
            this.setStatus(this.result.cancelled ? "Run cancelled." : "Run complete.");
        } catch (error) {
            this.result = null;
            this.setStatus(error instanceof Error ? error.message : String(error));
        } finally {
            if (simulator) await this.client.closeSimulator(simulator);
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
        workspace().unregisterDocument(this.docId);
        this.runController?.abort();
        if (this.currentRun) {
            try {
                await this.currentRun;
            } catch {
                // The run path already reports its own error.
            }
        }
    }

    private setStatus(text: string): void {
        const status = this.querySelector(".pc-strategy-status");
        if (status) status.textContent = text;
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
}

function baseLabel(path: string): string {
    return path.split("/").pop() ?? path;
}

// Reach-kind codes from the engine (mirrors pc-mod-pool / engine ModInfo).
const REACH_INFLUENCE = 1;
const REACH_CRAFTED = 2;
const REACH_ESSENCE = 3;
const REACH_FOSSIL = 5;

function modSourceLabel(mod: ModInfo): string {
    switch (mod.reach_kind) {
        case REACH_INFLUENCE: {
            const parts = mod.reach_via.split(":");
            return titleCase(parts[parts.length - 1] || "influenced");
        }
        case REACH_CRAFTED:
            return "Bench";
        case REACH_ESSENCE:
            return "Essence";
        case REACH_FOSSIL:
            return "Fossil";
        default:
            return "";
    }
}

function titleCase(value: string): string {
    return value
        .replace(/_/g, " ")
        .replace(/\b\w/g, (character) => character.toUpperCase());
}

/**
 * Group session mods into the same display families as the Emulator. The
 * representative T1 mod key is the stable persisted family identity, and each
 * option carries its complete T1..Tn threshold list.
 */
function buildModifierOptions(
    mods: ModInfo[],
    catalog: Catalog,
): ModifierFamilyOption[] {
    const families = new Map<string, ModInfo[]>();
    for (const mod of mods) {
        if (mod.generation_type !== 0 && mod.generation_type !== 1) continue;
        const familyId = Number.isFinite(mod.family_id)
            ? mod.family_id
            : mod.primary_group_id;
        const influence =
            mod.reach_kind === REACH_INFLUENCE ? `:${mod.reach_influence}` : "";
        const key = `${mod.reach_kind}${influence}:${mod.generation_type}:${familyId}`;
        const slot = families.get(key);
        if (slot) slot.push(mod);
        else families.set(key, [mod]);
    }

    const options: ModifierFamilyOption[] = [];
    for (const tiers of families.values()) {
        tiers.sort((a, b) => a.family_tier_index - b.family_tier_index);
        const rep = tiers[0];
        const text =
            rep.text_lines.join(" / ") ||
            catalog.groupNameById[rep.primary_group_id] ||
            rep.key;
        const side = rep.generation_type === 0 ? "P" : "S";
        const source = modSourceLabel(rep);
        options.push({
            value: rep.key,
            label: text,
            side: side === "P" ? "prefix" : "suffix",
            sourceLabel: source,
            tags: Array.from(
                new Set(tiers.flatMap((tier) => tier.classification_tags)),
            ).sort(),
            tiers: tiers.map((tier) => ({
                tier: tier.family_tier_index,
                label: tier.text_lines.join(" / ") || tier.key,
                requiredLevel: tier.required_level,
            })),
        });
    }
    options.sort((a, b) => a.label.localeCompare(b.label));
    return options;
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

customElements.define("pc-strategy-editor", PcStrategyEditor);
