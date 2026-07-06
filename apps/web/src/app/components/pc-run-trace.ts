import { StrategyResult, StrategyTrace } from "../engine-protocol";

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
];

export class PcRunTrace extends HTMLElement {
    private result: StrategyResult | null = null;
    private mode: ResultMode = "distribution";
    private traceIndex = 0;
    private stepIndex = 0;

    connectedCallback(): void {
        this.render();
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
        if (this.mode === "trace") this.bindTrace();
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
                                            ACTION_NAMES[row.action_type] ??
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
                <button data-cmd="prev" ${this.stepIndex === 0 ? "disabled" : ""}>←</button>
                <span>Step ${this.stepIndex + 1} / ${trace.entries.length}</span>
                <button data-cmd="next" ${this.stepIndex >= trace.entries.length - 1 ? "disabled" : ""}>→</button>
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
                    <details>
                        <summary>Item snapshot</summary>
                        <pre>${escapeHtml(JSON.stringify(entry.item, null, 2))}</pre>
                    </details>
                </div>
            </div>`;
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
