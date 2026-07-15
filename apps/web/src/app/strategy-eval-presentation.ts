import { EngineErrorInfo, StrategyEvalResult } from "./engine-protocol";
import { StrategyDocument } from "./strategy-model";
import { formatChaosValue, formatProbabilityExact } from "./odds-presentation";

export type StrategyBuilderMode = "simulator" | "calculator";

export interface StrategyNodeAnnotation {
    label: string;
    title: string;
    kind: "visits" | "terminal" | "cost";
}

export interface StrategyEdgeAnnotation {
    label: string;
    title: string;
}

export interface StrategyBoardAnnotations {
    stale: boolean;
    nodeBadges: Map<string, StrategyNodeAnnotation>;
    edgeLabels: Map<string, StrategyEdgeAnnotation>;
}

export function normalizeStrategyBuilderMode(
    mode: unknown,
): StrategyBuilderMode {
    return mode === "calculator" ? "calculator" : "simulator";
}

/**
 * Stable signature of the fields that can change exact graph evaluation.
 * Presentation-only names, labels, positions and viewport are omitted.
 */
export function strategyStructuralSignature(
    strategy: StrategyDocument,
): string {
    return JSON.stringify({
        version: strategy.version,
        start_node_id: strategy.start_node_id,
        base_state: strategy.base_state,
        nodes: strategy.nodes.map((node) => ({
            id: node.id,
            kind: node.kind,
            operation: node.operation,
            terminal: node.terminal,
        })),
        edges: strategy.edges.map((edge) => ({
            id: edge.id,
            from: edge.from,
            to: edge.to,
            priority: edge.priority,
            is_default: edge.is_default,
            condition: edge.condition,
        })),
    });
}

/** Presentation-only derivation from engine flows to board labels. */
export function buildStrategyBoardAnnotations(
    strategy: StrategyDocument,
    result: StrategyEvalResult,
    stale: boolean,
): StrategyBoardAnnotations {
    const nodeBadges = new Map<string, StrategyNodeAnnotation>();
    const nodeById = new Map(strategy.nodes.map((node) => [node.id, node]));
    for (const entry of result.nodes) {
        if (nodeById.get(entry.id)?.kind !== "operation") continue;
        nodeBadges.set(entry.id, {
            label: `${formatVisits(entry.expected_visits)}×`,
            title: `${formatCount(entry.expected_visits)} expected visits per strategy run`,
            kind: "visits",
        });
    }
    for (const terminal of result.terminals.by_node) {
        nodeBadges.set(terminal.node_id, {
            label: formatProbabilityExact(terminal.p),
            title: `${formatProbabilityExact(terminal.p)} absorb probability`,
            kind: "terminal",
        });
    }

    const traversalById = new Map(
        result.edges.map((edge) => [edge.id, edge.expected_traversals]),
    );
    const outflowBySource = new Map<string, number>();
    for (const edge of strategy.edges) {
        const traversals = traversalById.get(edge.id) ?? 0;
        outflowBySource.set(
            edge.from,
            (outflowBySource.get(edge.from) ?? 0) + traversals,
        );
    }
    const edgeLabels = new Map<string, StrategyEdgeAnnotation>();
    for (const edge of strategy.edges) {
        const traversals = traversalById.get(edge.id);
        if (traversals === undefined) continue;
        const sourceOutflow = outflowBySource.get(edge.from) ?? 0;
        edgeLabels.set(edge.id, {
            label: formatProbabilityExact(
                sourceOutflow > 0 ? traversals / sourceOutflow : 0,
            ),
            title: `${formatCount(traversals)} expected traversals per strategy run`,
        });
    }
    return { stale, nodeBadges, edgeLabels };
}

/**
 * Solver-compiled operation nodes carry V(s) directly. Present those values
 * through the board's existing annotation channel instead of a second badge
 * layer.
 */
export function buildSolverCostAnnotations(
    strategy: StrategyDocument,
): StrategyBoardAnnotations | null {
    const nodeBadges = new Map<string, StrategyNodeAnnotation>();
    for (const node of strategy.nodes) {
        if (
            node.kind !== "operation" ||
            node.expected_cost === undefined ||
            !Number.isFinite(node.expected_cost)
        ) {
            continue;
        }
        const cost = formatChaosValue(node.expected_cost);
        nodeBadges.set(node.id, {
            label: `~${cost} to go`,
            title: `${cost} expected remaining cost from this solver state`,
            kind: "cost",
        });
    }
    return nodeBadges.size
        ? {
              stale: false,
              nodeBadges,
              edgeLabels: new Map(),
          }
        : null;
}

function formatVisits(value: number): string {
    if (!Number.isFinite(value)) return "—";
    return value.toLocaleString("en-US", {
        useGrouping: true,
        minimumFractionDigits: 2,
        maximumFractionDigits: 2,
    });
}

function formatCount(value: number): string {
    if (!Number.isFinite(value)) return "—";
    return value.toLocaleString("en-US", {
        useGrouping: true,
        maximumFractionDigits: 6,
    });
}

export function escapeStrategyEvalHtml(value: string): string {
    return value
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;");
}

export function strategyEvalRefusalMarkup(error: EngineErrorInfo): string {
    const heading =
        error.code === 4
            ? "This strategy uses actions or conditions Calculator mode cannot evaluate exactly yet."
            : error.code === 5 || error.code === 7 || error.code === 8
              ? "This strategy is too large to evaluate exactly with the current limits."
              : "Calculator mode could not evaluate this strategy exactly.";
    const message = `poecraft engine error ${error.code}: ${error.detail}`;
    return `<div class="pc-strategy-eval-refusal">
        <strong>${heading}</strong>
        <pre>${escapeStrategyEvalHtml(message)}</pre>
    </div>`;
}
