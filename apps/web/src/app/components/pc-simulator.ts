import { StrategyResult } from "../engine-protocol";
import { presentSampledSuccess } from "../odds-presentation";

export interface SimulatorView {
    running: boolean;
    disabled: boolean;
    progress: { done: number; total: number } | null;
    result: StrategyResult | null;
}

export class PcSimulator extends HTMLElement {
    private view: SimulatorView = {
        running: false,
        disabled: false,
        progress: null,
        result: null,
    };
    private runCount = 1000;
    private maxActionsPerRun = 100_000;
    private runStartMs = 0;
    private runEndMs = 0;
    private timer = 0;

    connectedCallback(): void {
        this.render();
    }

    disconnectedCallback(): void {
        this.stopTimer();
    }

    setView(view: SimulatorView): void {
        const wasRunning = this.view.running;
        this.view = view;
        if (view.running && !wasRunning) {
            this.beginMeasurement();
        } else if (!view.running && wasRunning) {
            this.endMeasurement();
        }
        this.render();
    }

    beginMeasurement(): void {
        this.runStartMs = performance.now();
        this.runEndMs = 0;
        this.startTimer();
    }

    endMeasurement(): void {
        if (!this.runStartMs || this.runEndMs) return;
        this.runEndMs = performance.now();
        this.stopTimer();
    }

    private startTimer(): void {
        this.stopTimer();
        this.timer = window.setInterval(() => this.updateElapsed(), 100);
    }

    private stopTimer(): void {
        if (this.timer) {
            window.clearInterval(this.timer);
            this.timer = 0;
        }
    }

    private elapsedMs(): number {
        if (!this.runStartMs) return 0;
        return (this.runEndMs || performance.now()) - this.runStartMs;
    }

    private updateElapsed(): void {
        const el = this.querySelector("[data-sim-elapsed]");
        if (el) el.textContent = formatElapsed(this.elapsedMs());
    }

    private render(): void {
        const { running, disabled, progress, result } = this.view;
        const summary = result?.summary;
        const completed = summary?.completed_runs ?? 0;
        const sampledSuccess = summary
            ? presentSampledSuccess(summary.success_count, completed)
            : null;
        const avgActions =
            completed > 0 ? (summary!.total_actions / completed).toFixed(2) : "—";
        const avgCost =
            summary && summary.costed_action_count > 0
                ? (summary.known_total_cost / completed).toFixed(2)
                : "—";
        const actionsPerSecond =
            summary && !running && this.elapsedMs() > 0
                ? formatThroughput(
                      (summary.total_actions * 1000) / this.elapsedMs(),
                  )
                : "—";
        this.innerHTML = `
            <div class="pc-simulator-controls">
                <button data-cmd="run-once" ${running || disabled ? "disabled" : ""}>Run once</button>
                <label>
                    <span>Runs</span>
                    <input type="number" min="1" max="1000000" data-field="runs"
                        value="${this.runCount}">
                </label>
                <label>
                    <span>Max actions / run</span>
                    <input type="number" min="1" max="1000000" data-field="max-actions"
                        value="${this.maxActionsPerRun}">
                </label>
                <button data-cmd="run-many" ${running || disabled ? "disabled" : ""}>Run N</button>
                <button data-cmd="cancel" ${running ? "" : "disabled"}>Cancel</button>
                <span class="pc-sim-progress">${
                    running && progress
                        ? `${progress.done.toLocaleString()} / ${progress.total.toLocaleString()}`
                        : result?.cancelled
                          ? "Cancelled"
                          : ""
                }</span>
            </div>
            <div class="pc-sim-summary">
                ${metric("Runs", completed.toLocaleString())}
                ${metric("Success", summary ? summary.success_count.toLocaleString() : "—")}
                ${metric("Sample success rate", sampledSuccess?.rate ?? "—")}
                ${metric("95% sample interval", sampledSuccess?.interval95 ?? "—")}
                ${metric("Total actions", summary ? summary.total_actions.toLocaleString() : "—")}
                ${metric("Avg actions", avgActions)}
                ${metric("Actions / sec", actionsPerSecond)}
                ${metric("Avg known cost", avgCost)}
                ${metric("Cost", summary?.cost_status ?? "—")}
                <div class="pc-sim-metric"><span>Time</span><strong data-sim-elapsed>${formatElapsed(
                    this.elapsedMs(),
                )}</strong></div>
            </div>
            ${result?.economy ? `<div class="pc-sim-economy">Costs pinned to ${escapeHtml(result.economy.league_name)} · ${escapeHtml(result.economy.source_cutoff_at_utc ? new Date(result.economy.source_cutoff_at_utc).toLocaleString() : "manual prices")}</div>` : ""}
            ${
                result
                    ? `<div class="pc-failure-summary"><span>Simulator sample · ${result.sampled_accounting.sample_count.toLocaleString()} runs · seed ${result.sampled_accounting.seed}</span>${[
                          ...result.sampled_accounting.actions.map(
                              (entry) =>
                                  `${escapeHtml(entry.action_id)} ${entry.average_per_invocation.toLocaleString("en-US", { maximumFractionDigits: 4 })}/run`,
                          ),
                          ...result.sampled_accounting.materials.map(
                              (entry) =>
                                  `${escapeHtml(entry.price_key)} ${entry.average_per_invocation.toLocaleString("en-US", { maximumFractionDigits: 4 })}/run`,
                          ),
                      ]
                          .slice(0, 6)
                          .map((entry) => `<span>${entry}</span>`)
                          .join("")}</div>`
                    : ""
            }
            ${
                result?.failure_summaries.length
                    ? `<div class="pc-failure-summary">${result.failure_summaries
                          .slice(0, 4)
                          .map(
                              (entry) =>
                                  `<span>${escapeHtml(entry.node_id || "run")}: ${entry.count} · ${escapeHtml(entry.detail || String(entry.failure_reason))}</span>`,
                          )
                          .join("")}</div>`
                    : ""
            }`;
        const runs = this.querySelector<HTMLInputElement>('[data-field="runs"]')!;
        runs.addEventListener("change", () => {
            this.runCount = Math.max(1, Math.floor(Number(runs.value) || 1));
            runs.value = String(this.runCount);
        });
        const maxActions = this.querySelector<HTMLInputElement>(
            '[data-field="max-actions"]',
        )!;
        maxActions.addEventListener("change", () => {
            this.maxActionsPerRun = clampActions(maxActions.value);
            maxActions.value = String(this.maxActionsPerRun);
        });
        this.querySelector('[data-cmd="run-once"]')?.addEventListener("click", () =>
            this.emitRun(1),
        );
        this.querySelector('[data-cmd="run-many"]')?.addEventListener("click", () => {
            this.runCount = Math.max(
                1,
                Math.floor(
                    Number(
                        this.querySelector<HTMLInputElement>(
                            '[data-field="runs"]',
                        )?.value,
                    ) || 1,
                ),
            );
            this.emitRun(this.runCount);
        });
        this.querySelector('[data-cmd="cancel"]')?.addEventListener("click", () => {
            this.dispatchEvent(
                new CustomEvent("strategy-cancel", { bubbles: true }),
            );
        });
    }

    private emitRun(count: number): void {
        this.maxActionsPerRun = clampActions(
            this.querySelector<HTMLInputElement>('[data-field="max-actions"]')
                ?.value,
        );
        this.dispatchEvent(
            new CustomEvent("strategy-run", {
                bubbles: true,
                detail: {
                    count,
                    maxActionsPerRun: this.maxActionsPerRun,
                },
            }),
        );
    }
}

function clampActions(value: string | undefined): number {
    return Math.min(
        1_000_000,
        Math.max(1, Math.floor(Number(value) || 100_000)),
    );
}

function formatElapsed(ms: number): string {
    if (ms <= 0) return "—";
    if (ms < 1000) return `${Math.round(ms)} ms`;
    if (ms < 60_000) return `${(ms / 1000).toFixed(1)} s`;
    const minutes = Math.floor(ms / 60_000);
    const seconds = Math.round((ms % 60_000) / 1000);
    return `${minutes}m ${seconds}s`;
}

function formatThroughput(actionsPerSecond: number): string {
    return Math.round(actionsPerSecond).toLocaleString();
}

function metric(label: string, value: string): string {
    return `<div class="pc-sim-metric"><span>${label}</span><strong>${value}</strong></div>`;
}

function escapeHtml(value: string): string {
    return value.replace(/&/g, "&amp;").replace(/</g, "&lt;");
}

customElements.define("pc-simulator", PcSimulator);
