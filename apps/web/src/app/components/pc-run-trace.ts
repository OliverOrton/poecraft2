import type {BaseInfo, Catalog, StrategyResult, StrategyTrace} from "../engine-protocol";
import type {EngineClient} from "../engine-client";
import {readItemCard} from "../item-preview";
import {traceResourceSnapshot, type TraceResource} from "../trace-item-preview";
import type {PcModList} from "./pc-mod-list";
import "./pc-mod-list";
const RESOURCE_ACTION_NAMES: Record<number, string> = {
    1003: "Acquire resource", 1004: "Awakener", 1100: "Run feeder",
    1101: "Move / recycle item", 1102: "Discard item", 1103: "Recombine pair",
};

type ResultMode = "distribution" | "trace";

const ACTION_NAMES = [
    "Transmute",
    "Augment",
    "Alteration",
    "Regal",
    "Alchemy",
    "Chaos",
    "Exalt",
    "Annul",
    "Scour",
    "Essence",
    "Fossil",
    "Bench",
    "Veiled Chaos",
    "Veiled Exalt",
    "Unveil",
    "Harvest Reforge",
    "Harvest Augment",
    "Harvest Resist",
    "Eldritch Ember",
    "Eldritch Ichor",
    "Eldritch Exalt",
    "Eldritch Chaos",
    "Eldritch Annul",
    "Influence Exalt",
    "Fracturing Orb",
    "Remove Crafted Modifiers",
    "Foulborn Augmentation",
    "Foulborn Regal",
    "Foulborn Exalted",
    "Remembrance (unavailable)",
    "Unravelling (unavailable)",
    "Orb of Dominance",
    "Tempering (unavailable)",
    "Tailoring (unavailable)",
    "Vaal",
    "Double corruption (unavailable)",
];

export class PcRunTrace extends HTMLElement {
    private result: StrategyResult | null = null;
    private mode: ResultMode = "distribution";
    private traceIndex = 0;
    private stepIndex = 0;
    private previewContext: {client: EngineClient; data: number; catalog: Catalog | null; bases: BaseInfo[]} | null = null;
    private previewVersion = 0;
    private previewDisposed = false;
    private previewWork: Promise<void> = Promise.resolve();

    connectedCallback(): void {
        this.render();
    }

    disconnectedCallback(): void { this.previewVersion += 1; }

    async disposeItemPreview(): Promise<void> {
        this.previewDisposed = true;
        this.previewVersion += 1;
        this.previewContext = null;
        await this.previewWork;
    }

    setItemPreviewContext(client: EngineClient, data: number, catalog: Catalog | null, bases: BaseInfo[]): void {
        if (this.previewDisposed) return;
        const old = this.previewContext;
        if (old?.client === client && old.data === data && old.catalog === catalog && old.bases === bases) return;
        this.previewContext = {client, data, catalog, bases};
        if (this.mode === "trace" && this.isConnected) this.render();
    }

    setResult(result: StrategyResult | null): void {
        this.result = result;
        this.mode = "distribution";
        this.traceIndex = 0;
        this.stepIndex = Math.max(0, (result?.traces[0]?.entries.length ?? 1) - 1);
        this.render();
        this.emitHighlight();
    }

    private render(): void {
        this.previewVersion += 1;
        this.innerHTML = `
            <div class="pc-results-tabs" role="tablist">
                <button data-mode="distribution"
                    class="${this.mode === "distribution" ? "is-active" : ""}">
                    Action distribution
                </button>
                <button data-mode="trace"
                    class="${this.mode === "trace" ? "is-active" : ""}">
                    Trace
                </button>
            </div>
            <div class="pc-results-content">
                ${
                    this.mode === "distribution"
                        ? this.renderDistribution()
                        : this.renderTrace()
                }
            </div>`;
        this.querySelectorAll<HTMLButtonElement>("[data-mode]").forEach((button) => {
            button.addEventListener("click", () => {
                this.mode = button.dataset.mode as ResultMode;
                this.render();
                this.emitHighlight();
            });
        });
        if (this.mode === "trace") {
            this.bindTrace();
            this.readTraceItem();
        }
    }

    private renderDistribution(): string {
        const rows = this.result?.action_distribution ?? [];
        const summary = this.result?.summary;
        if (!this.result) {
            return `
                <div class="pc-run-trace-empty">
                    Run the graph to see action usage across every completed run.
                </div>`;
        }
        if (!rows.length) {
            return `
                <div class="pc-run-trace-empty">
                    The completed runs did not take any crafting actions.
                </div>`;
        }
        const total = summary?.total_actions ?? 0;
        const runs = summary?.completed_runs ?? 0;
        const largest = Math.max(...rows.map((row) => row.count), 1);
        return `
            <div class="pc-action-distribution">
                <div class="pc-action-distribution-head">
                    <span>${total.toLocaleString()} actions across ${runs.toLocaleString()} runs</span>
                    <span>${runs ? (total / runs).toFixed(2) : "0"} average / run</span>
                </div>
                <div class="pc-action-distribution-list">
                    ${rows
                        .map((row) => {
                            const share = total ? (row.count / total) * 100 : 0;
                            const width = (row.count / largest) * 100;
                            return `
                                <div class="pc-action-distribution-row">
                                    <div class="pc-action-distribution-label">
                                        <strong>${escapeHtml(
                                            ({1003: "Acquire resource", 1004: "Awakener"} as Record<number, string>)[row.action_type] ?? (RESOURCE_ACTION_NAMES[row.action_type] ?? ACTION_NAMES[row.action_type]) ??
                                                `Action ${row.action_type}`,
                                        )}</strong>
                                        <span>${escapeHtml(row.node_id)}</span>
                                    </div>
                                    <div class="pc-action-distribution-bar">
                                        <span style="width:${width.toFixed(2)}%"></span>
                                    </div>
                                    <div class="pc-action-distribution-value">
                                        <strong>${row.count.toLocaleString()}</strong>
                                        <span>${share.toFixed(1)}%</span>
                                    </div>
                                </div>`;
                        })
                        .join("")}
                </div>
            </div>`;
    }

    private renderTrace(): string {
        const traces = this.result?.traces ?? [];
        if (!traces.length) {
            return `
                <div class="pc-run-trace-empty">
                    No retained traces are available for this run.
                </div>`;
        }
        const trace = traces[this.traceIndex] ?? traces[0];
        this.stepIndex = Math.min(
            this.stepIndex,
            Math.max(0, trace.entries.length - 1),
        );
        const entry = trace.entries[this.stepIndex];
        if (!entry) return `<div class="pc-run-trace-empty" role="status">No retained steps are available for this trace.</div>`;
        const actualOutput = entry.resources?.find(resource => resource.active_output);
        return `
            <div class="pc-trace-toolbar">
                <label>
                    <span>Retained trace</span>
                    <select data-field="trace">
                        ${traces
                            .map(
                                (_, index) =>
                                    `<option value="${index}" ${index === this.traceIndex ? "selected" : ""}>Run ${index + 1}</option>`,
                            )
                            .join("")}
                    </select>
                </label>
                <button data-cmd="prev" aria-label="Previous step" ${this.stepIndex === 0 ? "disabled" : ""}>←</button>
                <span>Step ${this.stepIndex + 1} / ${trace.entries.length}</span>
                <button data-cmd="next" aria-label="Next step" ${this.stepIndex >= trace.entries.length - 1 ? "disabled" : ""}>→</button>
            </div>
            <div class="pc-trace-body">
                <ol class="pc-trace-steps">
                    ${trace.entries
                        .map(
                            (item, index) => `
                            <li class="${index === this.stepIndex ? "is-active" : ""}">
                                <button data-step="${index}">
                                    <strong>${escapeHtml(item.node_id)}</strong>
                                    <span>${item.matched_edge_id ? `edge ${escapeHtml(item.matched_edge_id)}` : item.terminal_kind ?? "stop"}</span>
                                </button>
                            </li>`,
                        )
                        .join("")}
                </ol>
                <div class="pc-trace-detail">
                    <dl>
                        <div><dt>Node</dt><dd>${escapeHtml(entry.node_id)}</dd></div>
                        <div><dt>Matched edge</dt><dd>${escapeHtml(entry.matched_edge_id || "—")}</dd></div>
                        <div><dt>Actions</dt><dd>${entry.cumulative_actions}</dd></div>
                        <div><dt>Known cost</dt><dd>${entry.known_cumulative_cost.toFixed(2)}${entry.cost_complete ? "" : " +"}</dd></div>
                    </dl>
                    ${actualOutput ? this.renderItemPreview(actualOutput) : ""}
                    <details>
                        <summary>${actualOutput ? "Actual output item — " + escapeHtml(actualOutput.base_key ?? actualOutput.resource_id) : "Current item snapshot"}</summary>
                        <pre>${escapeHtml(JSON.stringify(actualOutput ?? entry.item, null, 2))}</pre>
                    </details>
                    ${entry.resources?.length ? `<details><summary>Resource inventory</summary><pre>${escapeHtml(JSON.stringify(entry.resources, null, 2))}</pre></details>` : ""}
                </div>
            </div>`;
    }

    private renderItemPreview(resource: TraceResource): string {
        const provenance = resource.feeder
            ? `Feeder: ${resource.feeder.strategy_id} / ${resource.feeder.revision} \u00b7 Output ${resource.feeder.output_accepted ? "accepted" : "rejected"}${resource.feeder.cost_complete ? "" : " \u00b7 Feeder cost incomplete"}`
            : resource.recombination
                ? `Recombination: ${resource.recombination.model_id} \u00b7 Game odds estimated \u00b7 Gold/dust costs incomplete`
                : "";
        return `<section class="pc-trace-item-preview" aria-label="Actual output item at this step"
            data-resource-id="${escapeAttribute(resource.resource_id)}"
            data-resource-identity="${escapeAttribute(resource.identity ?? "")}">
            <div class="pc-trace-item-meta">
                <strong>Actual output at this step</strong>
                <span>Resource: ${escapeHtml(resource.resource_id)}</span>
                <span>Physical identity: ${escapeHtml(resource.identity ?? "Unavailable")}</span>
                ${provenance ? `<span>${escapeHtml(provenance)}</span>` : ""}
            </div>
            <div class="pc-trace-item-card" aria-busy="true">
                <p class="pc-trace-item-status" role="status">Reading native item snapshot…</p>
            </div>
        </section>`;
    }

    private readTraceItem(): void {
        const entry = this.result?.traces[this.traceIndex]?.entries[this.stepIndex];
        const resource = entry?.resources?.find(value => value.active_output);
        const host = this.querySelector<HTMLElement>(".pc-trace-item-card");
        if (!resource || !host || !this.isConnected || this.previewDisposed) return;
        const version = this.previewVersion, context = this.previewContext;
        const current = () => !this.previewDisposed && version === this.previewVersion && this.isConnected && host.isConnected;
        const unavailable = (message: string, failed = false) => {
            if (!current()) return;
            host.setAttribute("aria-busy", "false");
            const status = document.createElement("p");
            status.className = "pc-trace-item-status";
            status.setAttribute("role", failed ? "alert" : "status");
            status.textContent = message;
            host.replaceChildren(status);
        };
        const snapshot = traceResourceSnapshot(resource);
        if (!snapshot) {
            unavailable("Item preview unavailable: native base, level, physical identity or item snapshot is missing. Raw receipt retained below.");
            return;
        }
        if (!context) {
            unavailable("Item preview unavailable until the engine is ready. Raw receipt retained below.");
            return;
        }
        // One imported preview at a time. Rapid navigation skips superseded queued
        // reads; already acquired native handles finish through readItemCard's finally.
        this.previewWork = this.previewWork.then(async () => {
            if (!current()) return;
            try {
                const name = context.bases.find(base => base.path === snapshot.base)?.name ?? snapshot.base;
                const model = await readItemCard(context.client, context.data, context.catalog, snapshot, name);
                if (!current()) return;
                host.replaceChildren(document.createElement("pc-mod-list"));
                host.querySelector<PcModList>("pc-mod-list")!.setModel({...model, readOnly: true});
                host.setAttribute("aria-busy", "false");
            } catch (error) {
                unavailable(`Item preview unavailable: ${error instanceof Error ? error.message : String(error)}. Raw receipt retained below.`, true);
            }
        });
    }

    private bindTrace(): void {
        const traces = this.result?.traces ?? [];
        const trace = traces[this.traceIndex];
        if (!trace) return;
        this.querySelector<HTMLSelectElement>('[data-field="trace"]')?.addEventListener(
            "change",
            (event) => {
                this.traceIndex = Number(
                    (event.currentTarget as HTMLSelectElement).value,
                );
                this.stepIndex = Math.max(
                    0,
                    traces[this.traceIndex].entries.length - 1,
                );
                this.render();
                this.emitHighlight();
            },
        );
        this.querySelector('[data-cmd="prev"]')?.addEventListener("click", () => {
            this.stepIndex = Math.max(0, this.stepIndex - 1);
            this.render();
            this.emitHighlight();
        });
        this.querySelector('[data-cmd="next"]')?.addEventListener("click", () => {
            this.stepIndex = Math.min(trace.entries.length - 1, this.stepIndex + 1);
            this.render();
            this.emitHighlight();
        });
        this.querySelectorAll<HTMLButtonElement>("[data-step]").forEach((button) => {
            button.addEventListener("click", () => {
                this.stepIndex = Number(button.dataset.step);
                this.render();
                this.emitHighlight();
            });
        });
    }

    private emitHighlight(): void {
        const trace: StrategyTrace | undefined =
            this.mode === "trace"
                ? this.result?.traces[this.traceIndex]
                : undefined;
        if (!trace?.entries.length) {
            this.dispatchEvent(
                new CustomEvent("trace-highlight", {
                    bubbles: true,
                    detail: {
                        nodeIds: [],
                        edgeIds: [],
                        activeNodeId: null,
                    },
                }),
            );
            return;
        }
        const entries = trace.entries.slice(0, this.stepIndex + 1);
        this.dispatchEvent(
            new CustomEvent("trace-highlight", {
                bubbles: true,
                detail: {
                    nodeIds: entries.map((entry) => entry.node_id),
                    edgeIds: entries
                        .map((entry) => entry.matched_edge_id)
                        .filter(Boolean),
                    activeNodeId: entries.at(-1)?.node_id ?? null,
                },
            }),
        );
    }
}

function escapeHtml(value: string): string {
    return value
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;");
}

customElements.define("pc-run-trace", PcRunTrace);

function escapeAttribute(value: string): string {
    return escapeHtml(value).replace(/"/g, "&quot;").replace(/'/g, "&#39;");
}
