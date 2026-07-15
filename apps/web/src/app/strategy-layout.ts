import type {
    StrategyDocument,
    StrategyEdge,
    StrategyNode,
} from "./strategy-model";

const DEFAULT_NODE_WIDTH = 210;
const DEFAULT_NODE_HEIGHT = 132;

export interface StrategyLayoutOptions {
    nodeSizes?: ReadonlyMap<string, { width: number; height: number }>;
    edgeCardSize?: (edge: StrategyEdge) => { width: number; height: number };
}

/** True when every node has a finite position that the board can render. */
export function hasStrategyPositions(strategy: StrategyDocument): boolean {
    return strategy.nodes.every((node) => {
        const position = (node as Partial<StrategyNode>).position;
        return (
            position !== undefined &&
            Number.isFinite(position.x) &&
            Number.isFinite(position.y)
        );
    });
}

/**
 * Assign positions only when an imported strategy omitted them. Returns true
 * when layout was applied.
 */
export function ensureStrategyPositions(
    strategy: StrategyDocument,
    options: StrategyLayoutOptions = {},
): boolean {
    if (hasStrategyPositions(strategy)) return false;
    autoLayoutStrategy(strategy, options);
    return true;
}

/**
 * Shared layered (Sugiyama-style) layout used by Strategy Builder's Auto
 * layout command and by solver-compiled policies before they enter the board.
 */
export function autoLayoutStrategy(
    strategy: StrategyDocument,
    options: StrategyLayoutOptions = {},
): void {
    const nodes = strategy.nodes;
    if (!nodes.length) return;
    const originalOrder = new Map(nodes.map((node, index) => [node.id, index]));
    const nodeSize = (id: string): { width: number; height: number } =>
        options.nodeSizes?.get(id) ?? {
            width: DEFAULT_NODE_WIDTH,
            height: DEFAULT_NODE_HEIGHT,
        };
    const edgeCardSize =
        options.edgeCardSize ?? (() => ({ width: 300, height: 110 }));
    const nodeIds = new Set(nodes.map((node) => node.id));
    const outgoing = new Map<string, Set<string>>();
    for (const edge of strategy.edges) {
        if (edge.from === edge.to) continue;
        if (!nodeIds.has(edge.from) || !nodeIds.has(edge.to)) continue;
        (outgoing.get(edge.from) ??
            outgoing.set(edge.from, new Set()).get(edge.from)!).add(edge.to);
    }

    // Drop back edges while deriving a DAG and topological order. Iterative
    // DFS with an explicit stack: compiled solver policies can chain thousands
    // of nodes deep, past the call-stack limit.
    const state = new Map<string, "active" | "done">();
    const forward = new Map<string, string[]>();
    const incoming = new Map<string, string[]>();
    const listOf = (map: Map<string, string[]>, key: string): string[] => {
        let list = map.get(key);
        if (!list) {
            list = [];
            map.set(key, list);
        }
        return list;
    };
    const topo: string[] = [];
    const visit = (rootId: string): void => {
        if (state.has(rootId)) return;
        const stack: { id: string; targets: string[]; index: number }[] = [];
        const enter = (id: string): void => {
            state.set(id, "active");
            stack.push({ id, targets: [...(outgoing.get(id) ?? [])], index: 0 });
        };
        enter(rootId);
        while (stack.length) {
            const frame = stack[stack.length - 1];
            if (frame.index >= frame.targets.length) {
                state.set(frame.id, "done");
                topo.push(frame.id);
                stack.pop();
                continue;
            }
            const target = frame.targets[frame.index];
            frame.index += 1;
            if (state.get(target) === "active") continue;
            listOf(forward, frame.id).push(target);
            listOf(incoming, target).push(frame.id);
            if (!state.has(target)) enter(target);
        }
    };
    const startId = nodeIds.has(strategy.start_node_id)
        ? strategy.start_node_id
        : nodes[0].id;
    visit(startId);
    for (const node of nodes) {
        visit(node.id);
    }
    topo.reverse();

    // Longest-path layering puts every node after all forward predecessors.
    const level = new Map<string, number>();
    for (const id of topo) {
        const current = level.get(id) ?? 0;
        level.set(id, current);
        for (const target of forward.get(id) ?? []) {
            level.set(target, Math.max(level.get(target) ?? 0, current + 1));
        }
    }

    const positionOf = (node: StrategyNode): { x: number; y: number } => {
        const position = (node as Partial<StrategyNode>).position;
        return position &&
            Number.isFinite(position.x) &&
            Number.isFinite(position.y)
            ? position
            : { x: originalOrder.get(node.id) ?? 0, y: originalOrder.get(node.id) ?? 0 };
    };
    const columns: StrategyNode[][] = [];
    for (const node of nodes) {
        const index = level.get(node.id) ?? 0;
        (columns[index] ??= []).push(node);
    }
    for (const column of columns) {
        column?.sort((a, b) => {
            const ap = positionOf(a);
            const bp = positionOf(b);
            return (
                ap.y - bp.y ||
                ap.x - bp.x ||
                (originalOrder.get(a.id) ?? 0) - (originalOrder.get(b.id) ?? 0)
            );
        });
    }

    // Barycenter sweeps reduce crossings between adjacent columns. Sorting a
    // column only changes that column's row indices, so re-index just the
    // sorted column instead of rescanning every column (large compiled
    // policies made the full rescan quadratic).
    const rowIndex = new Map<string, number>();
    const indexColumn = (column: StrategyNode[]): void =>
        column.forEach((node, index) => rowIndex.set(node.id, index));
    const sortColumn = (
        column: StrategyNode[],
        neighbors: Map<string, string[]>,
    ): void => {
        const scores = new Map<string, number>();
        column.forEach((node, index) => {
            const rows = (neighbors.get(node.id) ?? [])
                .map((id) => rowIndex.get(id))
                .filter((row): row is number => row !== undefined);
            scores.set(
                node.id,
                rows.length
                    ? rows.reduce((sum, row) => sum + row, 0) / rows.length
                    : index,
            );
        });
        column.sort((a, b) => scores.get(a.id)! - scores.get(b.id)!);
        indexColumn(column);
    };
    columns.forEach((column) => indexColumn(column ?? []));
    for (let pass = 0; pass < 4; pass += 1) {
        for (let index = 1; index < columns.length; index += 1) {
            sortColumn(columns[index] ?? [], incoming);
        }
        for (let index = columns.length - 2; index >= 0; index -= 1) {
            sortColumn(columns[index] ?? [], forward);
        }
    }

    // Reserve enough horizontal room for the condition cards between nodes,
    // and track how much vertical space each gap's card stack needs so rows
    // can spread far enough that every card gets its own clear slot.
    const gapWidths = new Array<number>(
        Math.max(0, columns.length - 1),
    ).fill(150);
    const gapCardDemand = new Array<number>(
        Math.max(0, columns.length - 1),
    ).fill(0);
    for (const edge of strategy.edges) {
        if (edge.from === edge.to) continue;
        const fromLevel = level.get(edge.from);
        const toLevel = level.get(edge.to);
        if (
            fromLevel === undefined ||
            toLevel === undefined ||
            toLevel <= fromLevel
        ) {
            continue;
        }
        const card = edgeCardSize(edge);
        const gap = Math.min(
            gapWidths.length - 1,
            Math.max(fromLevel, Math.floor((fromLevel + toLevel - 1) / 2)),
        );
        if (gap >= 0) {
            // Card width plus room on both sides for edges to rise or fall
            // between port height and card height without clipping anything.
            gapWidths[gap] = Math.max(gapWidths[gap], card.width + 170);
            gapCardDemand[gap] += card.height + 14;
        }
    }
    const columnX: number[] = [];
    let x = 80;
    columns.forEach((_, index) => {
        columnX[index] = x;
        x += DEFAULT_NODE_WIDTH + (gapWidths[index] ?? 150);
    });

    // Self-loop cards render above their node; reserve headroom for them.
    const loopHeadroom = new Map<string, number>();
    for (const edge of strategy.edges) {
        if (edge.from !== edge.to || !nodeIds.has(edge.from)) continue;
        const card = edgeCardSize(edge);
        loopHeadroom.set(
            edge.from,
            Math.max(loopHeadroom.get(edge.from) ?? 0, card.height + 48),
        );
    }

    const baseRowGap = 110;
    const nodeSpan = (node: StrategyNode): number =>
        nodeSize(node.id).height + (loopHeadroom.get(node.id) ?? 0);
    const baseHeights = columns.map((column) =>
        (column ?? []).reduce((sum, node) => sum + nodeSpan(node), 0) +
        Math.max(0, (column?.length ?? 0) - 1) * baseRowGap,
    );
    // A column must be tall enough for the busier of its two adjacent gaps,
    // otherwise that gap's cards would spill past the rows they belong to.
    const rowGaps = columns.map((column, index) => {
        const rows = column?.length ?? 0;
        if (rows < 2) return baseRowGap;
        const demand = Math.max(
            gapCardDemand[index - 1] ?? 0,
            gapCardDemand[index] ?? 0,
        );
        return (
            baseRowGap +
            Math.max(0, demand - baseHeights[index]) / (rows - 1)
        );
    });
    const columnHeights = columns.map(
        (column, index) =>
            baseHeights[index] +
            Math.max(0, (column?.length ?? 0) - 1) *
                (rowGaps[index] - baseRowGap),
    );
    const tallest = Math.max(0, ...columnHeights);
    columns.forEach((column, index) => {
        let y = 90 + Math.max(0, (tallest - columnHeights[index]) / 2);
        for (const node of column ?? []) {
            y += loopHeadroom.get(node.id) ?? 0;
            node.position = { x: columnX[index], y: Math.round(y) };
            y += nodeSize(node.id).height + rowGaps[index];
        }
    });
}
