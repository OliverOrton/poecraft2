/*
 * pc-weight-table — presentational view of an action's debug candidate pool.
 * Shows each weighted candidate and the totals, matching the engine debug
 * output the native/Python/WASM suites assert against.
 */

import { PoolDebug } from "../engine-protocol";

const SIDE_LABEL: Record<number, string> = { 0: "prefix", 1: "suffix" };

export class PcWeightTable extends HTMLElement {
    setData(pool: PoolDebug): void {
        const entries = pool.entries
            .slice()
            .sort((a, b) => b.final_weight - a.final_weight);
        const rows = entries
            .map(
                (entry) => `
                <tr>
                    <td class="pc-mod-${SIDE_LABEL[entry.generation_type] ?? "prefix"}">${entry.key}</td>
                    <td class="num">${entry.spawn_weight}</td>
                    <td class="num">${entry.generation_multiplier_pct}%</td>
                    <td class="num">${entry.final_weight}</td>
                </tr>`,
            )
            .join("");
        this.innerHTML = `
            <div class="pc-weight-table">
                <div class="pc-weight-summary">
                    <span>candidates <b>${pool.summary.candidate_count}</b></span>
                    <span>prefix Σ <b>${pool.summary.prefix_total_weight}</b></span>
                    <span>suffix Σ <b>${pool.summary.suffix_total_weight}</b></span>
                    <span>combined Σ <b>${pool.summary.combined_total_weight}</b></span>
                </div>
                <table>
                    <thead>
                        <tr><th>mod</th><th class="num">spawn</th><th class="num">gen×</th><th class="num">weight</th></tr>
                    </thead>
                    <tbody>${rows}</tbody>
                </table>
            </div>`;
    }
}

customElements.define("pc-weight-table", PcWeightTable);
