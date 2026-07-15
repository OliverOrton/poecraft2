import {
    EconomyIdentity,
    EngineErrorInfo,
    StrategyEvalResult,
} from "../engine-protocol";
import {
    formatChaosValue,
    formatConsumptionQuantity,
    formatProbabilityExact,
    formatRawProbability,
    presentExpectedConsumption,
} from "../odds-presentation";
import {
    escapeStrategyEvalHtml,
    strategyEvalRefusalMarkup,
} from "../strategy-eval-presentation";
import { setPrice } from "../workspace/prices";

export interface StrategyOddsView {
    result: StrategyEvalResult | null;
    error: EngineErrorInfo | null;
    invalid: boolean;
    evaluating: boolean;
    progressText: string | null;
    stale: boolean;
    selectedNodeId: string | null;
    selectedNodeKind: string | null;
    targetLabels: string[];
    prices: Readonly<Record<string, number>>;
    economy: EconomyIdentity | null;
}

const EMPTY_VIEW: StrategyOddsView = {
    result: null,
    error: null,
    invalid: false,
    evaluating: false,
    progressText: null,
    stale: false,
    selectedNodeId: null,
    selectedNodeKind: null,
    targetLabels: [],
    prices: {},
    economy: null,
};

const RARITY_NAMES = ["Normal", "Magic", "Rare"];
const SLOT_STATUS = ["Absent", "Below tier", "Satisfied"];

export class PcStrategyOdds extends HTMLElement {
    private view: StrategyOddsView = EMPTY_VIEW;

    connectedCallback(): void {
        this.render();
    }

    setView(view: StrategyOddsView): void {
        this.view = view;
        this.render();
    }

    private render(): void {
        if (!this.isConnected) return;
        const { result, error, invalid, evaluating, stale, progressText } =
            this.view;
        this.classList.toggle("is-stale", stale);
        if (!result) {
            const body = error
                ? strategyEvalRefusalMarkup(error)
                : invalid
                  ? '<p class="pc-strategy-eval-empty">Complete the graph to evaluate exact odds.</p>'
                : evaluating
                  ? `<p class="pc-strategy-eval-empty">${escapeStrategyEvalHtml(progressText ?? "Evaluating exact graph…")}</p>`
                  : '<p class="pc-strategy-eval-empty">Complete the graph to evaluate exact odds.</p>';
            this.innerHTML = `<div class="pc-strategy-eval-placeholder">
                <span class="pc-strategy-eval-kicker">Calculator · Exact graph evaluation</span>
                ${body}
            </div>`;
            return;
        }

        this.innerHTML = `
            <div class="pc-strategy-eval-state">
                <span class="pc-strategy-eval-kicker">Calculator · Exact graph evaluation</span>
                ${stale ? '<span class="pc-strategy-stale">Stale · graph changed</span>' : ""}
                ${evaluating ? `<span class="pc-strategy-evaluating">${escapeStrategyEvalHtml(progressText ?? "Evaluating exact graph…")}</span>` : ""}
            </div>
            <div class="pc-strategy-eval-grid">
                ${this.renderSummary(result, error)}
                ${this.renderCost(result)}
                ${this.renderNodeDetails(result)}
            </div>`;
        this.querySelectorAll<HTMLInputElement>("[data-price-key]").forEach(
            (input) => {
                input.addEventListener("change", () => {
                    const key = input.dataset.priceKey ?? "";
                    const value = Number(input.value);
                    setPrice(key, input.value === "" ? null : value);
                });
            },
        );
        this.querySelector<HTMLButtonElement>("[data-recost]")?.addEventListener(
            "click",
            () =>
                this.dispatchEvent(
                    new CustomEvent("strategy-recost", { bubbles: true }),
                ),
        );
    }

    private renderSummary(
        result: StrategyEvalResult,
        error: EngineErrorInfo | null,
    ): string {
        const unresolved = result.terminals.unresolved;
        const warning = !result.converged || unresolved > 0;
        const missRows = [
            ["Failure", result.terminals.failure],
            ["Stop", result.terminals.stop],
            ["Action not applied", result.terminals.action_not_applied],
            ["No matching edge", result.terminals.no_matching_edge],
            ["Unresolved", unresolved],
        ] as const;
        const attributions = [
            ...result.failures_by_node.map(
                (entry) =>
                    `<li><span>${escapeStrategyEvalHtml(reasonLabel(entry.reason))} · ${escapeStrategyEvalHtml(entry.node_id)}</span><strong>${formatProbabilityExact(entry.p)}</strong></li>`,
            ),
            ...result.unresolved_by_node.map(
                (entry) =>
                    `<li><span>Unresolved · ${escapeStrategyEvalHtml(entry.node_id)}</span><strong>${formatProbabilityExact(entry.mass)}</strong></li>`,
            ),
        ].join("");
        return `<section class="pc-strategy-eval-summary">
            <span class="pc-strategy-eval-kicker">Exact result</span>
            <span class="pc-strategy-eval-answer-label">Success</span>
            <strong class="pc-strategy-eval-answer">${formatProbabilityExact(result.terminals.success)}</strong>
            <span class="pc-strategy-eval-raw">p = ${formatRawProbability(result.terminals.success)}</span>
            <div class="pc-strategy-eval-metrics">
                ${missRows
                    .map(
                        ([label, probability]) => `<span><small>${label}</small><strong>${formatProbabilityExact(probability)}</strong></span>`,
                    )
                    .join("")}
                <span><small>Expected actions</small><strong>${formatCount(result.expected_actions)}</strong></span>
            </div>
            <p class="pc-strategy-eval-convergence ${warning ? "is-warning" : ""}">
                ${result.converged ? "Converged" : "Evaluation did not converge"} · ${result.sweeps === 0 ? "direct SCC solve" : `${result.sweeps.toLocaleString()} fallback sweeps`} · residual ${formatRawProbability(result.residual_mass)}
            </p>
            ${attributions ? `<ul class="pc-strategy-eval-attribution">${attributions}</ul>` : ""}
            ${this.view.invalid ? '<p class="pc-strategy-eval-invalid">Complete the graph to evaluate exact odds. Showing the previous result.</p>' : ""}
            ${error ? strategyEvalRefusalMarkup(error) : ""}
        </section>`;
    }

    private renderCost(result: StrategyEvalResult): string {
        const priced = presentExpectedConsumption(
            result.expected_consumption,
            (key) => this.view.prices[key],
        );
        const total = priced.complete
            ? formatChaosValue(priced.total)
            : `Incomplete · ${formatChaosValue(priced.total)} known`;
        return `<section class="pc-strategy-eval-cost">
            <span class="pc-strategy-eval-kicker">Expected consumption</span>
            <p class="pc-help">Per strategy run · ${this.view.economy ? escapeStrategyEvalHtml(economyLabel(this.view.economy)) : "manual prices"}</p>
            <div class="pc-strategy-eval-cost-rows">
                ${priced.rowsHtml || '<p class="pc-help">No priced actions in this strategy.</p>'}
            </div>
            <div class="pc-strategy-eval-total">
                <span>Total expected cost</span><strong>${total}</strong>
            </div>
            <button data-recost>Re-cost with active league</button>
        </section>`;
    }

    private renderNodeDetails(result: StrategyEvalResult): string {
        const { selectedNodeId, selectedNodeKind, targetLabels } = this.view;
        if (!selectedNodeId || selectedNodeKind !== "operation") {
            return `<section class="pc-strategy-eval-node">
                <span class="pc-strategy-eval-kicker">Node details</span>
                <p class="pc-strategy-eval-empty">Select an operation node to inspect its incoming states.</p>
            </section>`;
        }
        const node = result.nodes.find((entry) => entry.id === selectedNodeId);
        if (!node) {
            return `<section class="pc-strategy-eval-node">
                <span class="pc-strategy-eval-kicker">Node details</span>
                <p class="pc-strategy-eval-empty">No evaluated incoming states for ${escapeStrategyEvalHtml(selectedNodeId)}.</p>
            </section>`;
        }
        const headers = result.targets
            .map((_, index) => `<th title="${escapeStrategyEvalHtml(targetLabels[index] ?? `Target ${index + 1}`)}">G${index + 1}</th>`)
            .join("");
        const legend = result.targets
            .map(
                (_, index) => `<span><strong>G${index + 1}</strong> ${escapeStrategyEvalHtml(targetLabels[index] ?? `Target ${index + 1}`)}</span>`,
            )
            .join("");
        const rows = node.classes
            .map(
                (entry) => `<tr>
                    <td class="pc-calc-p">${formatProbabilityExact(entry.share)}</td>
                    <td>${RARITY_NAMES[entry.rarity] ?? entry.rarity}</td>
                    <td>${entry.prefixes}P/${entry.suffixes}S</td>
                    ${entry.slots.map((status) => `<td title="${SLOT_STATUS[status] ?? status}">${slotStatusShort(status)}</td>`).join("")}
                    <td class="pc-calc-flags">${formatMasks(entry.flags, entry.blocked)}</td>
                </tr>`,
            )
            .join("");
        return `<section class="pc-strategy-eval-node">
            <span class="pc-strategy-eval-kicker">Node details</span>
            <div class="pc-strategy-eval-node-title">
                <strong>${escapeStrategyEvalHtml(selectedNodeId)}</strong>
                <span>${formatConsumptionQuantity(node.expected_visits)} expected visits</span>
            </div>
            ${legend ? `<div class="pc-calc-goal-legend">${legend}</div>` : '<p class="pc-help">No family/group condition targets.</p>'}
            <div class="pc-strategy-eval-table-wrap">
                <table class="pc-calc-table">
                    <thead><tr><th>Share</th><th>Rarity</th><th>Affixes</th>${headers}<th>Flags</th></tr></thead>
                    <tbody>${rows}</tbody>
                </table>
            </div>
            ${node.classes_truncated_share > 0 ? `<p class="pc-help">…other classes · ${formatProbabilityExact(node.classes_truncated_share)}</p>` : ""}
        </section>`;
    }
}

function reasonLabel(reason: string): string {
    return reason === "action_not_applied"
        ? "Action not applied"
        : reason === "no_matching_edge"
          ? "No matching edge"
          : reason;
}

function economyLabel(economy: EconomyIdentity): string {
    const cutoff = economy.source_cutoff_at_utc
        ? new Date(economy.source_cutoff_at_utc).toLocaleString()
        : "manual prices";
    return `${economy.league_name} · pinned ${cutoff}`;
}

function slotStatusShort(status: number): string {
    return status === 2 ? "Yes" : status === 1 ? "Below" : "—";
}

function formatMasks(flags: number, blocked: number): string {
    if (!flags && !blocked) return "—";
    return `${flags ? `F 0x${flags.toString(16)}` : ""}${flags && blocked ? " · " : ""}${blocked ? `B 0x${blocked.toString(16)}` : ""}`;
}

function formatCount(value: number): string {
    return value.toLocaleString("en-US", {
        useGrouping: true,
        maximumFractionDigits: 6,
    });
}

customElements.define("pc-strategy-odds", PcStrategyOdds);
