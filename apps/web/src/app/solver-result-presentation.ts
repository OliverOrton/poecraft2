import type { SolveOptions, SolveSummary } from "./engine-protocol";
import { formatChaosValue } from "./odds-presentation";

export interface SolveVerificationPresentation {
    completedRuns: number;
    empiricalCost: number;
    delta: number;
}

export interface SolveResultMarkupOptions {
    summary: SolveSummary;
    admittedActionIds: readonly string[];
    excludedActions: number;
    economyLabel: string | null;
    terminationDetail: string;
    hasCompiledStrategy: boolean;
    busy: boolean;
    verification: SolveVerificationPresentation | null;
}

function objectRecord(value: unknown): Record<string, unknown> | null {
    return value !== null && typeof value === "object"
        ? (value as Record<string, unknown>)
        : null;
}

function finiteCost(value: number | null): string {
    return value !== null && Number.isFinite(value) && value < 1e12
        ? formatChaosValue(value)
        : "Unavailable";
}

function conservativeFixed(value: number, digits: number): string {
    const scale = 10 ** digits;
    const rounded = Math.ceil((value - Number.EPSILON) * scale) / scale;
    return rounded.toFixed(digits);
}

function conservativePercent(value: number): string {
    return conservativeFixed(value * 100, 2).replace(/\.00$/, "").replace(/(\.\d)0$/, "$1");
}

function policyQualityLabel(summary: SolveSummary): string {
    switch (summary.policy_status) {
        case "exact":
            return "Exact optimal policy";
        case "bounded_near_optimal":
            return "Bounded near-optimal policy";
        case "bounded_feasible":
            return "Bounded feasible policy";
        default:
            return "No executable policy";
    }
}

function terminationLabel(summary: SolveSummary): string {
    switch (summary.termination) {
        case "exact_closed":
            return "Exact proof closed";
        case "target_gap":
            return "Requested gap target met";
        case "refused_resource_cap":
            return "Resource cap reached";
        case "no_executable_policy":
            return "No executable policy could be certified";
        default:
            return "Solve stopped without a classified termination";
    }
}

function requestedTargetLabel(summary: SolveSummary): string {
    const targets: string[] = [];
    if (summary.requested_absolute_optimality_gap > 0) {
        targets.push(
            `absolute gap <= ${formatChaosValue(summary.requested_absolute_optimality_gap)}`,
        );
    }
    if (summary.requested_relative_optimality_gap > 0) {
        const relative = summary.requested_relative_optimality_gap;
        targets.push(
            `relative gap <= ${conservativePercent(relative)}% (${conservativeFixed(1 + relative, 2)}x)`,
        );
    }
    return targets.length > 0 ? targets.join(" or ") : "No gap target requested";
}

function firedTargetLabel(summary: SolveSummary): string {
    if (!summary.target_met || summary.target_fired === "none") {
        return "No gap target fired";
    }
    switch (summary.target_fired) {
        case "absolute":
            return "Absolute-gap criterion fired";
        case "relative":
            return "Relative-gap criterion fired";
        case "both":
            return "Absolute- and relative-gap criteria fired together";
        default:
            return "No gap target fired";
    }
}

function certificateText(summary: SolveSummary): string {
    if (summary.policy_status === "exact") {
        return "Exact optimum certified.";
    }
    if (!summary.policy_available) {
        return "No executable policy certificate was returned.";
    }
    const relative = summary.relative_optimality_gap;
    if (relative === null || !Number.isFinite(relative) || relative < 0) {
        return "An absolute bound is available. A multiplicative certificate requires a positive optimal-cost lower bound.";
    }
    const factor = conservativeFixed(1 + relative, 2);
    const percent = conservativePercent(relative);
    return `Certified within ${factor}x of optimal. At most ${percent}% more expensive than optimal. A weak lower bound can make this certificate pessimistic.`;
}

export function certifiedFactorLabel(relativeGap: number | null): string {
    return relativeGap !== null &&
        Number.isFinite(relativeGap) &&
        relativeGap >= 0
        ? `${conservativeFixed(1 + relativeGap, 2)}x`
        : "Unavailable";
}

export function shouldCompileSolvePolicy(summary: SolveSummary): boolean {
    return summary.policy_available;
}

export function solveGapTargetOptions(
    absoluteGap: number,
    relativeGapPercent: number,
): SolveOptions | undefined {
    const options: SolveOptions = {};
    if (Number.isFinite(absoluteGap) && absoluteGap > 0) {
        options.max_absolute_optimality_gap = absoluteGap;
    }
    if (Number.isFinite(relativeGapPercent) && relativeGapPercent > 0) {
        options.max_relative_optimality_gap = relativeGapPercent / 100;
    }
    return Object.keys(options).length > 0 ? options : undefined;
}

/** Describe a stop without turning a bounded policy into an exactness claim. */
export function solveTerminationDetail(
    summary: SolveSummary,
    telemetry: unknown,
): string {
    const root = objectRecord(telemetry);
    const optimization = objectRecord(root?.optimization);
    const capHits = Array.isArray(optimization?.cap_hits)
        ? optimization.cap_hits.filter(
              (value): value is string => typeof value === "string",
          )
        : [];
    if (summary.termination === "refused_resource_cap") {
        const cap = capHits.length > 0 ? ` (${capHits.join(", ")})` : "";
        return summary.policy_available
            ? `The solve reached a resource cap${cap}. The returned executable policy remains bounded; the cap is not an exactness claim.`
            : `The solve reached a resource cap${cap} before it could certify an executable policy.`;
    }
    if (summary.termination === "target_gap") {
        return "The solve stopped after a completed lower/upper round satisfied the requested certificate target.";
    }
    if (summary.termination === "exact_closed") {
        return "The lower and upper bounds closed within the named numerical proof tolerance.";
    }
    if (summary.termination === "no_executable_policy") {
        return "The solver could not establish a proper executable fallback, so no finite policy upper bound is claimed.";
    }
    return "The solve stopped without a classified termination reason.";
}

/** Stable DOM contract for exact and bounded solver results. */
export function solveResultMarkup(options: SolveResultMarkupOptions): string {
    const {
        summary,
        admittedActionIds,
        excludedActions,
        economyLabel,
        terminationDetail,
        hasCompiledStrategy,
        busy,
        verification,
    } = options;
    const evaluatedPolicyCost = finiteCost(summary.evaluated_policy_cost);
    const lowerBound = finiteCost(summary.lower_bound);
    const upperBound = finiteCost(summary.upper_bound);
    const absoluteGap = finiteCost(summary.absolute_optimality_gap);
    const multiplicativeFactor = certifiedFactorLabel(
        summary.relative_optimality_gap,
    );
    const statusClass = summary.policy_status === "exact" ? "is-success" : "is-warning";
    const admitted = admittedActionIds
        .map((id) => `<li><code>${escapeHtml(id)}</code></li>`)
        .join("");

    return `<section class="pc-calc-solve-result" data-policy-status="${summary.policy_status}" data-termination="${summary.termination}">
        <div class="pc-calc-solve-headline">
            <span>Returned policy expected cost</span>
            <strong data-solve-result="evaluated-policy-cost">${evaluatedPolicyCost}</strong>
        </div>
        <div class="pc-calc-solve-state ${statusClass}">
            <strong data-solve-result="policy-quality">${policyQualityLabel(summary)}</strong>
            &middot; ${summary.expanded_states.toLocaleString()} states
            &middot; ${summary.sweeps.toLocaleString()} sweeps
            &middot; residual ${summary.residual.toExponential(2)}
        </div>
        <p class="pc-calc-solve-certificate" data-solve-result="certificate">${escapeHtml(certificateText(summary))}</p>
        <dl class="pc-calc-solve-bounds">
            <div><dt>Optimal-cost lower bound</dt><dd data-solve-result="lower-bound">${lowerBound}</dd></div>
            <div><dt>Certified policy upper bound</dt><dd data-solve-result="upper-bound">${upperBound}</dd></div>
            <div><dt>Absolute optimality gap</dt><dd data-solve-result="absolute-gap">${absoluteGap}</dd></div>
            <div><dt>Certified multiplicative factor</dt><dd data-solve-result="multiplicative-factor">${multiplicativeFactor}</dd></div>
            <div><dt>Termination</dt><dd data-solve-result="termination-reason">${escapeHtml(terminationLabel(summary))}</dd></div>
            <div><dt>Requested target</dt><dd data-solve-result="requested-target">${escapeHtml(requestedTargetLabel(summary))}</dd></div>
            <div><dt>Firing criterion</dt><dd data-solve-result="firing-criterion">${escapeHtml(firedTargetLabel(summary))}</dd></div>
            <div><dt>Economy</dt><dd data-solve-result="economy">${economyLabel ? escapeHtml(economyLabel) : "Unavailable"}</dd></div>
        </dl>
        <p class="pc-calc-solve-termination-detail">${escapeHtml(terminationDetail)}</p>
        <strong class="pc-calc-solve-skipped">${excludedActions.toLocaleString()} unpriced actions excluded before Solve</strong>
        <span class="pc-calc-solve-native-skipped">${summary.skipped_actions.toLocaleString()} of ${admittedActionIds.length.toLocaleString()} admitted priced actions skipped by the native solver</span>
        <details class="pc-calc-solve-admitted-actions">
            <summary>Admitted priced action identity &middot; ${admittedActionIds.length.toLocaleString()}</summary>
            <ul>${admitted || "<li>None</li>"}</ul>
        </details>
        ${
            hasCompiledStrategy
                ? `<div class="pc-calc-solve-actions">
                    <button data-solve-cmd="open" ${busy ? "disabled" : ""}>Open in Strategy Board</button>
                    <button data-solve-cmd="verify" ${busy ? "disabled" : ""}>Verify 10,000 runs</button>
                </div>`
                : ""
        }
        ${
            verification
                ? `<div class="pc-calc-solve-verification">
                    <span>Evaluated policy <strong>${evaluatedPolicyCost}</strong></span>
                    <span>10,000-run empirical mean <strong>${formatChaosValue(verification.empiricalCost)}</strong></span>
                    <span>Delta <strong>${verification.delta >= 0 ? "+" : "&minus;"}${formatChaosValue(Math.abs(verification.delta))}</strong></span>
                    <small>Simulation is corroboration; exact policy evaluation is authoritative.</small>
                </div>`
                : ""
        }
    </section>`;
}

function escapeHtml(text: string): string {
    return text
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;");
}
