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

export interface ExpectedConsumptionEntry {
    key: string;
    quantity: number;
}

export interface ExpectedConsumptionPresentation {
    rowsHtml: string;
    total: number;
    complete: boolean;
    missingKeys: string[];
}

export type DisplayPriceSource =
    | "override"
    | "quote"
    | "recipe"
    | "zero"
    | "fallback";

export function priceSourceLabel(source: DisplayPriceSource): string {
    switch (source) {
        case "override":
            return "Override";
        case "recipe":
            return "Certified recipe";
        case "zero":
            return "Certified zero";
        case "fallback":
            return "User fallback";
        default:
            return "Market quote";
    }
}

/**
 * Shared editable price-row presentation for exact-odds surfaces. Quantities
 * come from the engine; the caller supplies the workspace price lookup.
 */
export function presentExpectedConsumption(
    entries: ExpectedConsumptionEntry[],
    priceFor: (key: string) => number | undefined,
    sourceFor?: (key: string) => DisplayPriceSource | null,
): ExpectedConsumptionPresentation {
    let total = 0;
    const missingKeys: string[] = [];
    const rowsHtml = entries
        .map(({ key, quantity }) => {
            const price = priceFor(key);
            const source = sourceFor?.(key) ?? null;
            if (price === undefined) {
                missingKeys.push(key);
            } else {
                total += price * quantity;
            }
            return `<div class="pc-calc-cost-row">
                <span class="pc-calc-cost-key">${formatConsumptionQuantity(quantity)} × ${escapeHtml(key)}</span>
                <input type="number" min="0" step="any" data-price-key="${escapeAttribute(key)}"
                    value="${price ?? ""}" placeholder="Set price">
                <span class="pc-calc-cost-sub ${price === undefined ? "is-missing" : ""}">${
                    price === undefined
                        ? "Missing price"
                        : `${formatChaosValue(price * quantity)}${source ? ` &middot; ${priceSourceLabel(source)}` : ""}`
                }</span>
            </div>`;
        })
        .join("");
    return {
        rowsHtml,
        total,
        complete: missingKeys.length === 0,
        missingKeys,
    };
}

export function formatConsumptionQuantity(quantity: number): string {
    if (!Number.isFinite(quantity)) return "—";
    const digits = quantity >= 1000 ? 2 : quantity >= 1 ? 4 : 6;
    return quantity.toLocaleString("en-US", {
        useGrouping: true,
        minimumFractionDigits: Math.min(2, digits),
        maximumFractionDigits: digits,
    });
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
