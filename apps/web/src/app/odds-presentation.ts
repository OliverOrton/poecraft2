/*
 * Presentation-only probability and cost helpers shared by odds surfaces.
 * Inputs are engine results and chaos-equivalent price totals; this module
 * does not decide success, legality, outcome weights, or action costs.
 */

function formatDecimal(
    value: number,
    maximumFractionDigits: number,
    useGrouping = false,
): string {
    return value.toLocaleString("en-US", {
        useGrouping,
        maximumFractionDigits,
    });
}

/** Percentage with enough precision to preserve the current WASM result. */
export function formatProbabilityExact(probability: number): string {
    if (!Number.isFinite(probability)) return "—";
    return `${formatDecimal(probability * 100, 6)}%`;
}

/** Unit-interval value returned by the engine, without binary float noise. */
export function formatRawProbability(probability: number): string {
    if (!Number.isFinite(probability)) return "—";
    return formatDecimal(probability, 10);
}

export function expectedAttempts(probability: number): number {
    return probability > 0 ? 1 / probability : Number.POSITIVE_INFINITY;
}

export function formatExpectedAttempts(probability: number): string {
    const attempts = expectedAttempts(probability);
    if (!Number.isFinite(attempts)) return "∞";
    return formatDecimal(attempts, attempts >= 1000 ? 2 : 4, true);
}

export function estimatedActionSpendPerSuccess(
    actionCost: number,
    probability: number,
): number {
    return probability > 0
        ? actionCost / probability
        : Number.POSITIVE_INFINITY;
}

export function formatChaosValue(value: number): string {
    if (!Number.isFinite(value)) return "∞";
    const digits = value >= 1000 ? 2 : value >= 1 ? 4 : 6;
    return `${formatDecimal(value, digits, true)}c`;
}
