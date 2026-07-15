import {
    StrategyEdge,
    StrategyNode,
    strategyEdgeCardPresentation,
    strategyEdgeLabel,
} from "../strategy-model";
import { StrategyEdgeAnnotation } from "../strategy-eval-presentation";

export const NODE_WIDTH = 210;
const PORT_Y = 54;
const CHAR_WIDTH = 6.1;
const CARD_ROW_HEIGHT = 19;
const CARD_HEADER_HEIGHT = 27;
const CARD_ANNOTATION_HEIGHT = 20;
const CARD_MAX_WIDTH = 320;
/** Rank palette: evaluation order per source node, fallback last. */
const RANK_MARKERS: ReadonlyArray<readonly [string, string]> = [
    ["r0", "#5fae74"],
    ["r1", "#cfa14e"],
    ["r2", "#d0784a"],
    ["r3", "#c65a52"],
    ["r4", "#a06bb0"],
    ["fb", "#6d7a76"],
    ["sel", "#e7b35f"],
    ["taken", "#55d5ff"],
];
const MAX_RANK = 4;

export interface EdgeLayerView {
    nodes: StrategyNode[];
    edges: StrategyEdge[];
    selectedEdgeId: string | null;
    highlightedEdgeIds: Set<string>;
    warningEdgeIds: Set<string>;
    annotations?: Map<string, StrategyEdgeAnnotation>;
    annotationsStale?: boolean;
    canvas?: { width: number; height: number };
    /** Rendered node sizes; routing treats nodes as obstacles. */
    nodeSizes?: Map<string, { width: number; height: number }>;
    /**
     * Large-graph mode: skip condition cards (except the selected edge) and
     * the per-edge collision/obstacle work, which is quadratic in edge count
     * and freezes the page on big solver-compiled policies.
     */
    simplified?: boolean;
}

interface EdgeCardRowLayout {
    kind: "group" | "leaf";
    label: string;
    depth: number;
    count?: number;
    height: number;
}

interface EdgeCardLayout {
    width: number;
    height: number;
    compact: boolean;
    manual: boolean;
    header: string;
    count?: number;
    rows: EdgeCardRowLayout[];
    annotation?: StrategyEdgeAnnotation;
}

interface EdgeCardRect {
    x: number;
    y: number;
    width: number;
    height: number;
    /** Self-loop cards stay anchored above their node. */
    pinned: boolean;
}

interface EdgeRenderItem {
    edge: StrategyEdge;
    kind: "forward" | "back" | "self";
    selected: boolean;
    taken: boolean;
    rank: number;
    fallback: boolean;
    layout: EdgeCardLayout;
    ports: { x1: number; y1: number; x2: number; y2: number };
    route: number;
    classes: string;
    title: string;
    card: EdgeCardRect;
    d: string;
}

/**
 * Footprint of an edge's full condition card, used by auto-layout to reserve
 * enough space between columns and rows for the cards that will sit there.
 */
export function estimateEdgeCardSize(edge: StrategyEdge): {
    width: number;
    height: number;
} {
    const layout = edgeCardLayout(edge, undefined);
    return { width: layout.width, height: layout.height };
}

function edgeCardLayout(
    edge: StrategyEdge,
    annotation: StrategyEdgeAnnotation | undefined,
): EdgeCardLayout {
    const presentation = strategyEdgeCardPresentation(edge);
    const annotationHeight = annotation ? CARD_ANNOTATION_HEIGHT : 0;
    const headerHeight = presentation.header ? CARD_HEADER_HEIGHT : 0;
    const widestText = Math.max(
        presentation.header.length + 8,
        ...presentation.rows.map((row) => row.label.length + row.depth * 3),
    );
    const width = presentation.compact
        ? Math.max(78, Math.min(300, widestText * CHAR_WIDTH + 26))
        : Math.max(224, Math.min(CARD_MAX_WIDTH, widestText * CHAR_WIDTH + 34));
    const rows = presentation.rows.map((row) => {
        if (row.kind === "group") {
            return { ...row, height: CARD_ROW_HEIGHT };
        }
        const availableWidth = Math.max(72, width - 30 - row.depth * 10);
        const charactersPerLine = Math.max(
            12,
            Math.floor(availableWidth / CHAR_WIDTH),
        );
        const lines = Math.max(1, Math.ceil(row.label.length / charactersPerLine));
        return { ...row, height: lines * 12 + 7 };
    });
    const height = presentation.compact
        ? Math.max(30, rows[0]?.height ?? 30) + annotationHeight
        : headerHeight +
          rows.reduce((sum, row) => sum + row.height, 0) +
          10 +
          annotationHeight;
    return {
        width,
        height,
        compact: presentation.compact,
        manual: presentation.manual,
        header: presentation.header,
        count: presentation.count,
        rows,
        annotation,
    };
}

function edgeCardMarkup(
    layout: EdgeCardLayout,
    rankClass: string,
    stale: boolean,
): string {
    const classes = [
        "pc-edge-card",
        layout.compact ? "is-compact" : "is-tree",
        layout.manual ? "is-manual" : "",
        layout.annotation ? "has-annotation" : "",
        stale ? "is-stale" : "",
        rankClass,
    ]
        .filter(Boolean)
        .join(" ");
    return `<div xmlns="http://www.w3.org/1999/xhtml" class="${classes}">
        ${layout.header ? `<div class="pc-edge-card-head"><span class="pc-edge-card-operator">${escapeText(layout.header)}</span>${layout.count === undefined ? "" : `<span>${layout.count} rule${layout.count === 1 ? "" : "s"}</span>`}</div>` : ""}
        <div class="pc-edge-card-rows">
            ${layout.rows.map((row) => `<div class="pc-edge-card-row is-${row.kind}" style="--pc-edge-depth: ${Math.min(row.depth, 8)}; --pc-edge-row-height: ${row.height}px"><span class="pc-edge-card-branch"></span>${row.kind === "group" ? `<span class="pc-edge-card-operator">${escapeText(row.label)}</span><span class="pc-edge-card-count">${row.count ?? 0} rule${row.count === 1 ? "" : "s"}</span>` : `<span class="pc-edge-card-leaf">${escapeText(row.label)}</span>`}</div>`).join("")}
        </div>
        ${layout.annotation ? `<div class="pc-edge-card-eval">${escapeText(layout.annotation.label)}</div>` : ""}
    </div>`;
}

/**
 * Evaluation rank of each edge among the edges leaving the same node:
 * priority ascending, explicit defaults last, ties by document order. An edge
 * is the "fallback" when flagged is_default, or when it is an unconditional
 * trailing edge with siblings.
 */
function edgeRanks(
    edges: StrategyEdge[],
): Map<string, { rank: number; fallback: boolean }> {
    const bySource = new Map<string, { edge: StrategyEdge; index: number }[]>();
    edges.forEach((edge, index) => {
        let group = bySource.get(edge.from);
        if (!group) {
            group = [];
            bySource.set(edge.from, group);
        }
        group.push({ edge, index });
    });
    const ranks = new Map<string, { rank: number; fallback: boolean }>();
    for (const group of bySource.values()) {
        group.sort(
            (a, b) =>
                Number(a.edge.is_default === true) -
                    Number(b.edge.is_default === true) ||
                a.edge.priority - b.edge.priority ||
                a.index - b.index,
        );
        group.forEach(({ edge }, position) => {
            const always =
                !edge.condition || edge.condition.type === "always";
            const fallback =
                edge.is_default === true ||
                (always && group.length > 1 && position === group.length - 1);
            ranks.set(edge.id, {
                rank: Math.min(position, MAX_RANK),
                fallback,
            });
        });
    }
    return ranks;
}

export class PcEdgeLayer extends HTMLElement {
    private view: EdgeLayerView = {
        nodes: [],
        edges: [],
        selectedEdgeId: null,
        highlightedEdgeIds: new Set(),
        warningEdgeIds: new Set(),
        annotations: new Map(),
        annotationsStale: false,
    };
    private preview: { from: string; x: number; y: number } | null = null;
    private delegated = false;

    setView(view: EdgeLayerView): void {
        this.view = view;
        this.render();
    }

    setPreview(preview: { from: string; x: number; y: number } | null): void {
        this.preview = preview;
        const svg = this.querySelector("svg");
        if (!svg) {
            this.render();
            return;
        }
        // Patch the preview path in place: re-rendering every edge on each
        // connect-drag mousemove is far too slow on large graphs.
        svg.querySelector(".pc-edge-preview")?.remove();
        if (!preview) return;
        const from = this.view.nodes.find((node) => node.id === preview.from);
        if (!from) return;
        const path = document.createElementNS(
            "http://www.w3.org/2000/svg",
            "path",
        );
        path.setAttribute("class", "pc-edge-preview");
        path.setAttribute(
            "d",
            chainPath([
                { x: from.position.x + NODE_WIDTH, y: from.position.y + PORT_Y },
                { x: preview.x, y: preview.y },
            ]),
        );
        svg.appendChild(path);
    }

    connectedCallback(): void {
        if (!this.delegated) {
            this.delegated = true;
            // One delegated listener for every edge group: innerHTML swaps in
            // render() would drop per-group listeners anyway, and attaching
            // thousands of them per render is expensive on large graphs.
            this.addEventListener("pointerdown", (event) => {
                const group = (event.target as Element | null)?.closest?.(
                    "[data-edge-id]",
                );
                if (!group || !this.contains(group)) return;
                event.stopPropagation();
                this.dispatchEvent(
                    new CustomEvent("strategy-select", {
                        bubbles: true,
                        detail: {
                            kind: "edge",
                            id: (group as SVGGElement).dataset.edgeId,
                        },
                    }),
                );
            });
        }
        this.render();
    }

    private render(): void {
        const canvas = this.view.canvas ?? { width: 3000, height: 2000 };
        const paths = this.view.simplified
            ? this.simplifiedPaths()
            : this.fullPaths();
        if (this.preview) {
            const from = this.view.nodes.find(
                (node) => node.id === this.preview!.from,
            );
            if (from) {
                const d = chainPath([
                    {
                        x: from.position.x + NODE_WIDTH,
                        y: from.position.y + PORT_Y,
                    },
                    { x: this.preview.x, y: this.preview.y },
                ]);
                paths.push(`<path class="pc-edge-preview" d="${d}"></path>`);
            }
        }
        const markerDefs = RANK_MARKERS.map(
            ([key, color]) => `<marker id="pc-edge-arrow-${key}" markerWidth="7" markerHeight="7"
                        refX="6" refY="3.5" orient="auto">
                        <path d="M0,0 L7,3.5 L0,7 z" fill="${color}"></path>
                    </marker>`,
        ).join("");
        this.innerHTML = `
            <svg style="width:${canvas.width}px;height:${canvas.height}px" viewBox="0 0 ${canvas.width} ${canvas.height}">
                <defs>${markerDefs}</defs>
                ${paths.join("")}
            </svg>`;
    }

    /**
     * Large-graph pass: O(nodes + edges). Lane-offset curves from port to
     * port, no card collision or obstacle detours, and a condition card only
     * for the selected edge.
     */
    private simplifiedPaths(): string[] {
        const nodes = new Map(this.view.nodes.map((node) => [node.id, node]));
        const routes = edgeRoutes(this.view.edges);
        const ranks = edgeRanks(this.view.edges);
        const stale = Boolean(this.view.annotationsStale);
        const paths: string[] = [];
        let selectedMarkup: string | null = null;
        for (const edge of this.view.edges) {
            const from = nodes.get(edge.from);
            const to = nodes.get(edge.to);
            if (!from || !to) continue;
            const route = routes.get(edge.id) ?? 0;
            const selected = edge.id === this.view.selectedEdgeId;
            const taken = this.view.highlightedEdgeIds.has(edge.id);
            const rankInfo = ranks.get(edge.id) ?? { rank: 0, fallback: false };
            const ports = {
                x1: from.position.x + NODE_WIDTH,
                y1: from.position.y + PORT_Y,
                x2: to.position.x,
                y2: to.position.y + PORT_Y,
            };
            const kind: EdgeRenderItem["kind"] =
                from.id === to.id
                    ? "self"
                    : ports.x2 < ports.x1 + 30
                      ? "back"
                      : "forward";
            let d: string;
            if (kind === "self") {
                const height = 105 + Math.abs(route) * 0.9;
                const width = 120 + route;
                d = `M ${ports.x1} ${ports.y1} C ${ports.x1 + width} ${ports.y1 - height}, ${ports.x2 - width} ${ports.y2 - height}, ${ports.x2} ${ports.y2}`;
            } else if (kind === "back") {
                const laneY =
                    Math.max(ports.y1, ports.y2) +
                    118 +
                    Math.abs(route) * 0.9 +
                    (route > 0 ? 25 : 0);
                d = routeBackEdge(ports, laneY, []);
            } else {
                d = chainPath([
                    { x: ports.x1, y: ports.y1 },
                    {
                        x: (ports.x1 + ports.x2) / 2,
                        y: (ports.y1 + ports.y2) / 2 + route * 0.75,
                    },
                    { x: ports.x2, y: ports.y2 },
                ]);
            }
            const rankClass = rankInfo.fallback
                ? "is-fallback"
                : `is-rank-${rankInfo.rank}`;
            const classes = [
                "pc-edge-path",
                rankClass,
                selected ? "is-selected" : "",
                taken ? "is-taken" : "",
                this.view.warningEdgeIds.has(edge.id) ? "has-warning" : "",
            ]
                .filter(Boolean)
                .join(" ");
            const annotation = this.view.annotations?.get(edge.id);
            const label = strategyEdgeLabel(edge);
            const marker = taken
                ? "taken"
                : selected
                  ? "sel"
                  : rankInfo.fallback
                    ? "fb"
                    : `r${rankInfo.rank}`;
            let cardMarkup = "";
            if (selected) {
                const layout = edgeCardLayout(edge, annotation);
                const anchor = edgeAnchor(kind, from, ports, route, layout);
                cardMarkup = `<foreignObject class="pc-edge-card-object" x="${anchor.x - layout.width / 2}" y="${anchor.y - layout.height / 2}" width="${layout.width}" height="${layout.height}">
                        ${edgeCardMarkup(layout, rankClass, stale)}
                    </foreignObject>`;
            }
            const markup = `<g class="pc-edge-group ${selected ? "is-selected" : ""}" data-edge-id="${escapeAttribute(edge.id)}">
                    <title>${escapeText(annotation ? `${label}\n${annotation.title}` : label)}</title>
                    <path class="pc-edge-hit" d="${d}"></path>
                    <path class="${classes}" d="${d}" marker-end="url(#pc-edge-arrow-${marker})"></path>
                    ${cardMarkup}
                </g>`;
            if (selected) {
                selectedMarkup = markup;
            } else {
                paths.push(markup);
            }
        }
        // Selected edge paints last so its path and card sit above siblings.
        if (selectedMarkup) paths.push(selectedMarkup);
        return paths;
    }

    private fullPaths(): string[] {
        const nodes = new Map(this.view.nodes.map((node) => [node.id, node]));
        const routes = edgeRoutes(this.view.edges);
        const ranks = edgeRanks(this.view.edges);
        const stale = Boolean(this.view.annotationsStale);
        const nodeRectById = new Map<string, EdgeCardRect>(
            this.view.nodes.map((node) => [
                node.id,
                {
                    x: node.position.x,
                    y: node.position.y,
                    width:
                        this.view.nodeSizes?.get(node.id)?.width ?? NODE_WIDTH,
                    height:
                        this.view.nodeSizes?.get(node.id)?.height ?? 132,
                    pinned: true,
                },
            ]),
        );
        const nodeRects = [...nodeRectById.values()];
        const { columnOf, gapCenter, gapCount } = clusterColumns(
            this.view.nodes,
            this.view.nodeSizes,
        );

        const items: EdgeRenderItem[] = [];
        for (const edge of this.view.edges) {
            const from = nodes.get(edge.from);
            const to = nodes.get(edge.to);
            if (!from || !to) continue;
            const route = routes.get(edge.id) ?? 0;
            const selected = edge.id === this.view.selectedEdgeId;
            const taken = this.view.highlightedEdgeIds.has(edge.id);
            const annotation = this.view.annotations?.get(edge.id);
            const layout = edgeCardLayout(edge, annotation);
            const rankInfo = ranks.get(edge.id) ?? { rank: 0, fallback: false };
            const ports = {
                x1: from.position.x + NODE_WIDTH,
                y1: from.position.y + PORT_Y,
                x2: to.position.x,
                y2: to.position.y + PORT_Y,
            };
            const kind: EdgeRenderItem["kind"] =
                from.id === to.id
                    ? "self"
                    : ports.x2 < ports.x1 + 30
                      ? "back"
                      : "forward";
            const anchor = edgeAnchor(kind, from, ports, route, layout);
            if (kind === "forward") {
                // Multi-column edges anchor their card in the same gap the
                // auto-layout budgeted for it, not at the raw port midpoint
                // (which can land inside an intermediate node column).
                const fromColumn = columnOf.get(edge.from);
                const toColumn = columnOf.get(edge.to);
                if (
                    fromColumn !== undefined &&
                    toColumn !== undefined &&
                    toColumn - fromColumn > 1
                ) {
                    const gap = Math.max(
                        fromColumn,
                        Math.floor((fromColumn + toColumn - 1) / 2),
                    );
                    const center = gapCenter(gap);
                    if (center !== null) anchor.x = center;
                }
            } else if (kind === "back") {
                // Lane cards sitting on a node column slide to the nearest
                // clear gap between columns.
                const overlapsNode = (centerX: number): boolean =>
                    nodeRects.some(
                        (rect) =>
                            centerX - layout.width / 2 < rect.x + rect.width &&
                            rect.x < centerX + layout.width / 2 &&
                            anchor.y - layout.height / 2 < rect.y + rect.height &&
                            rect.y < anchor.y + layout.height / 2,
                    );
                if (overlapsNode(anchor.x)) {
                    const low = ports.x2 + layout.width / 2 - 40;
                    const high = ports.x1 - layout.width / 2 + 40;
                    let best: number | null = null;
                    for (let gap = 0; gap < gapCount; gap++) {
                        const center = gapCenter(gap);
                        if (center === null || center < low || center > high) {
                            continue;
                        }
                        if (overlapsNode(center)) continue;
                        if (
                            best === null ||
                            Math.abs(center - anchor.x) < Math.abs(best - anchor.x)
                        ) {
                            best = center;
                        }
                    }
                    if (best !== null) anchor.x = best;
                }
            }
            const rankClass = rankInfo.fallback
                ? "is-fallback"
                : `is-rank-${rankInfo.rank}`;
            const classes = [
                "pc-edge-path",
                rankClass,
                selected ? "is-selected" : "",
                taken ? "is-taken" : "",
                this.view.warningEdgeIds.has(edge.id) ? "has-warning" : "",
            ]
                .filter(Boolean)
                .join(" ");
            const label = strategyEdgeLabel(edge);
            items.push({
                edge,
                kind,
                selected,
                taken,
                rank: rankInfo.rank,
                fallback: rankInfo.fallback,
                layout,
                ports,
                route,
                classes,
                title: annotation ? `${label}\n${annotation.title}` : label,
                card: {
                    x: anchor.x - layout.width / 2,
                    y: anchor.y - layout.height / 2,
                    width: layout.width,
                    height: layout.height,
                    pinned: kind === "self",
                },
                d: "",
            });
        }

        // Two-phase placement: forward and self-loop cards settle first, then
        // each back edge drops its card (and therefore its lane) below all
        // content within its horizontal span, so return lanes run through
        // clear bands instead of through the card stacks.
        const forwardItems = items.filter((item) => item.kind !== "back");
        nudgeCards(forwardItems.map((item) => item.card), nodeRects);
        const backItems = items
            .filter((item) => item.kind === "back")
            .sort(
                (a, b) =>
                    a.ports.x1 - a.ports.x2 - (b.ports.x1 - b.ports.x2),
            );
        const settled: EdgeCardRect[] = [
            ...nodeRects,
            ...forwardItems.map((item) => item.card),
        ];
        for (const item of backItems) {
            const spanLeft = item.ports.x2 - 20;
            const spanRight = item.ports.x1 + 20;
            let bottom = Math.max(item.ports.y1, item.ports.y2) + 40;
            for (const rect of settled) {
                if (rect.x + rect.width > spanLeft && rect.x < spanRight) {
                    bottom = Math.max(bottom, rect.y + rect.height);
                }
            }
            item.card.y = bottom + 46;
            settled.push(item.card);
        }

        // Route paths once every card has its final spot: each edge passes
        // through its own card and detours around the other edges' cards and
        // any nodes in the way (its own endpoints excepted — paths terminate
        // at those).
        for (const item of items) {
            const otherCards = items
                .filter((other) => other !== item)
                .map((other) => other.card);
            const ownEnds = [
                nodeRectById.get(item.edge.from),
                nodeRectById.get(item.edge.to),
            ];
            const nodeObstacles = nodeRects.filter(
                (rect) => !ownEnds.includes(rect),
            );
            item.d = routeEdge(item, otherCards, nodeObstacles);
        }

        // Selected edge paints last so its path and card sit above siblings.
        const ordered = [...items].sort(
            (a, b) => Number(a.selected) - Number(b.selected),
        );
        const paths = ordered.map((item) => {
            const marker = item.taken
                ? "taken"
                : item.selected
                  ? "sel"
                  : item.fallback
                    ? "fb"
                    : `r${item.rank}`;
            const rankClass = item.fallback
                ? "is-fallback"
                : `is-rank-${item.rank}`;
            return `<g class="pc-edge-group ${item.selected ? "is-selected" : ""}" data-edge-id="${escapeAttribute(item.edge.id)}">
                    <title>${escapeText(item.title)}</title>
                    <path class="pc-edge-hit" d="${item.d}"></path>
                    <path class="${item.classes}" d="${item.d}" marker-end="url(#pc-edge-arrow-${marker})"></path>
                    <foreignObject class="pc-edge-card-object" x="${item.card.x}" y="${item.card.y}" width="${item.card.width}" height="${item.card.height}">
                        ${edgeCardMarkup(item.layout, rankClass, stale)}
                    </foreignObject>
                </g>`;
        });

        return paths;
    }
}

/** Initial card anchor before the collision pass runs. */
function edgeAnchor(
    kind: EdgeRenderItem["kind"],
    from: StrategyNode,
    ports: { x1: number; y1: number; x2: number; y2: number },
    route: number,
    layout: EdgeCardLayout,
): { x: number; y: number } {
    if (kind === "self") {
        return {
            x: from.position.x + NODE_WIDTH / 2,
            y: from.position.y - 34 - layout.height / 2,
        };
    }
    if (kind === "back") {
        const lane =
            Math.max(ports.y1, ports.y2) +
            118 +
            Math.abs(route) * 0.9 +
            (route > 0 ? 25 : 0);
        return { x: (ports.x1 + ports.x2) / 2, y: lane + layout.height * 0.25 };
    }
    return {
        x: (ports.x1 + ports.x2) / 2,
        y: (ports.y1 + ports.y2) / 2 + route * 0.75,
    };
}

/** Node columns inferred from x positions, with the gaps between them. */
function clusterColumns(
    nodes: StrategyNode[],
    nodeSizes?: Map<string, { width: number; height: number }>,
): {
    columnOf: Map<string, number>;
    gapCenter: (index: number) => number | null;
    gapCount: number;
} {
    const sorted = [...nodes].sort((a, b) => a.position.x - b.position.x);
    const columns: { left: number; right: number }[] = [];
    const columnOf = new Map<string, number>();
    for (const node of sorted) {
        const width = nodeSizes?.get(node.id)?.width ?? NODE_WIDTH;
        const last = columns[columns.length - 1];
        if (!last || node.position.x > last.right + 40) {
            columns.push({
                left: node.position.x,
                right: node.position.x + width,
            });
        } else {
            last.left = Math.min(last.left, node.position.x);
            last.right = Math.max(last.right, node.position.x + width);
        }
        columnOf.set(node.id, columns.length - 1);
    }
    const gapCenter = (index: number): number | null => {
        const before = columns[index];
        const after = columns[index + 1];
        return before && after && after.left - before.right > 60
            ? (before.right + after.left) / 2
            : null;
    };
    return { columnOf, gapCenter, gapCount: Math.max(0, columns.length - 1) };
}

/**
 * Push overlapping cards downward until no two occupy the same space. Fixed
 * rects (nodes) participate as immovable obstacles.
 */
function nudgeCards(cards: EdgeCardRect[], fixed: EdgeCardRect[]): void {
    const pad = 10;
    const placed: EdgeCardRect[] = [...fixed];
    const orderedCards = [...cards].sort((a, b) => a.y - b.y || a.x - b.x);
    for (const card of orderedCards) {
        if (!card.pinned) {
            let moved = true;
            while (moved) {
                moved = false;
                for (const other of placed) {
                    const overlapsX =
                        card.x < other.x + other.width + pad &&
                        other.x < card.x + card.width + pad;
                    const overlapsY =
                        card.y < other.y + other.height + pad &&
                        other.y < card.y + card.height + pad;
                    if (overlapsX && overlapsY) {
                        card.y = other.y + other.height + pad;
                        moved = true;
                    }
                }
            }
        }
        placed.push(card);
    }
}

function routeEdge(
    item: EdgeRenderItem,
    otherCards: EdgeCardRect[],
    nodeRects: EdgeCardRect[],
): string {
    const { x1, y1, x2, y2 } = item.ports;
    if (item.kind === "self") {
        const height = 105 + Math.abs(item.route) * 0.9;
        const width = 120 + item.route;
        return `M ${x1} ${y1} C ${x1 + width} ${y1 - height}, ${x2 - width} ${y2 - height}, ${x2} ${y2}`;
    }
    if (item.kind === "back") {
        const laneY = item.card.y + item.card.height / 2;
        return routeBackEdge(item.ports, laneY, [...otherCards, ...nodeRects]);
    }
    return routeForwardEdge(item.ports, item.card, otherCards, nodeRects);
}

/**
 * Forward edge: a smooth chain that crosses its own card horizontally and
 * detours over or under any other card the path would cut through. Detours
 * are flat across the obstacle with the vertical transition completed outside
 * its horizontal extent, so the path never clips the rect it is avoiding.
 */
function routeForwardEdge(
    ports: { x1: number; y1: number; x2: number; y2: number },
    card: EdgeCardRect,
    otherCards: EdgeCardRect[],
    nodeRects: EdgeCardRect[],
): string {
    const { x1, y1, x2, y2 } = ports;
    const centerY = card.y + card.height / 2;
    // Cross the whole stack of cards sharing this gap in one flat segment,
    // so the vertical transitions happen clear of every stacked card.
    let stackLeft = card.x;
    let stackRight = card.x + card.width;
    for (const other of otherCards) {
        if (other.x < stackRight + 10 && stackLeft - 10 < other.x + other.width) {
            stackLeft = Math.min(stackLeft, other.x);
            stackRight = Math.max(stackRight, other.x + other.width);
        }
    }
    const entryX = clamp(stackLeft - 40, x1 + 10, x2 - 10);
    const exitX = clamp(stackRight + 40, x1 + 10, x2 - 10);
    const obstacles = [...otherCards, ...nodeRects];
    const points: { x: number; y: number }[] =
        exitX - entryX > 20
            ? [
                  { x: x1, y: y1 },
                  { x: entryX, y: centerY },
                  { x: exitX, y: centerY },
                  { x: x2, y: y2 },
              ]
            : [
                  { x: x1, y: y1 },
                  {
                      x: clamp(card.x + card.width / 2, x1 + 10, x2 - 10),
                      y: centerY,
                  },
                  { x: x2, y: y2 },
              ];
    const handled = new Set<EdgeCardRect>();
    for (let iteration = 0; iteration < 5; iteration++) {
        const hit = firstObstacleHit(points, obstacles, handled);
        if (!hit) break;
        handled.add(hit.rect);
        const detourY =
            hit.y < hit.rect.y + hit.rect.height / 2
                ? hit.rect.y - 18
                : hit.rect.y + hit.rect.height + 18;
        const flatLeft = clamp(hit.rect.x - 24, x1 + 10, x2 - 10);
        const flatRight = clamp(
            hit.rect.x + hit.rect.width + 24,
            x1 + 10,
            x2 - 10,
        );
        // Keep waypoints from crowding: shift outward (widening the flat
        // span) rather than dropping the detour.
        const additions = [
            { x: flatLeft, y: detourY, shift: -28 },
            { x: flatRight, y: detourY, shift: 28 },
        ].map((candidate) => {
            let x = candidate.x;
            let guard = 0;
            while (
                guard++ < 4 &&
                points.some((point) => Math.abs(point.x - x) < 26)
            ) {
                x += candidate.shift;
            }
            return { x: clamp(x, x1 + 6, x2 - 6), y: detourY };
        });
        points.push(...additions);
        points.sort((a, b) => a.x - b.x);
    }
    return chainPath(points);
}

/**
 * Backward edge: exit right, drop to a horizontal lane running through the
 * edge's own card, jog over or under other cards sitting on the lane, then
 * rise into the target's input port. Jog heights are validated against every
 * card so a dodge never lands inside a neighbouring one.
 */
function routeBackEdge(
    ports: { x1: number; y1: number; x2: number; y2: number },
    laneY: number,
    obstacles: EdgeCardRect[],
): string {
    const { x1, y1, x2, y2 } = ports;
    const right = x1 + 8;
    const left = x2 - 8;
    const run: { x: number; y: number }[] = [{ x: right, y: laneY }];
    const pad = 10;
    const blockers = obstacles
        .filter(
            (rect) =>
                rect.y - pad < laneY &&
                laneY < rect.y + rect.height + pad &&
                rect.x + rect.width > left + 20 &&
                rect.x < right - 20,
        )
        .sort((a, b) => b.x - a.x);
    for (const rect of blockers.slice(0, 6)) {
        const flatLeft = rect.x - 8;
        const flatRight = rect.x + rect.width + 8;
        const jogY = clearJogHeight(rect, laneY, obstacles, flatLeft, flatRight);
        if (jogY === null) continue;
        const dive = 40 + Math.abs(jogY - laneY) * 0.9;
        const entry = Math.min(right - 10, flatRight + dive);
        const exit = Math.max(left + 10, flatLeft - dive);
        if (exit >= entry) continue;
        run.push({ x: entry, y: laneY });
        run.push({ x: Math.min(flatRight, entry - 10), y: jogY });
        run.push({ x: Math.max(flatLeft, exit + 10), y: jogY });
        run.push({ x: exit, y: laneY });
    }
    run.push({ x: left, y: laneY });
    const lead = `M ${x1} ${y1} C ${x1 + 64} ${y1}, ${x1 + 64} ${laneY}, ${right} ${laneY}`;
    const tail = ` C ${x2 - 64} ${laneY}, ${x2 - 64} ${y2}, ${x2} ${y2}`;
    return lead + chainSegments(run) + tail;
}

/**
 * Height for a lane jog around `rect` whose flat span stays clear of every
 * other card: prefer the nearer side, then walk outward.
 */
function clearJogHeight(
    rect: EdgeCardRect,
    laneY: number,
    obstacles: EdgeCardRect[],
    flatLeft: number,
    flatRight: number,
): number | null {
    const above = rect.y - 20;
    const below = rect.y + rect.height + 20;
    const preferAbove = laneY - rect.y < rect.y + rect.height - laneY;
    const candidates = preferAbove
        ? [above, below, above - 44, below + 44, above - 88, below + 88]
        : [below, above, below + 44, above - 44, below + 88, above - 88];
    for (const candidate of candidates) {
        const blocked = obstacles.some(
            (other) =>
                other !== rect &&
                other.x - 12 < flatRight &&
                flatLeft < other.x + other.width + 12 &&
                other.y - 12 < candidate &&
                candidate < other.y + other.height + 12,
        );
        if (!blocked) return candidate;
    }
    return null;
}

/** Piecewise cubics with horizontal tangents at every point. */
function chainPath(points: { x: number; y: number }[]): string {
    let d = `M ${points[0].x} ${points[0].y}`;
    d += chainSegments(points);
    return d;
}

function chainSegments(points: { x: number; y: number }[]): string {
    let d = "";
    for (let i = 1; i < points.length; i++) {
        const prev = points[i - 1];
        const next = points[i];
        const dx = next.x - prev.x;
        const k = Math.sign(dx || 1) * Math.min(110, Math.max(24, Math.abs(dx) * 0.45));
        d += ` C ${prev.x + k} ${prev.y}, ${next.x - k} ${next.y}, ${next.x} ${next.y}`;
    }
    return d;
}

/** First obstacle a chain of cubics passes through, if any. */
function firstObstacleHit(
    points: { x: number; y: number }[],
    obstacles: EdgeCardRect[],
    handled: Set<EdgeCardRect>,
): { rect: EdgeCardRect; x: number; y: number } | null {
    const pad = 10;
    for (let i = 1; i < points.length; i++) {
        const prev = points[i - 1];
        const next = points[i];
        const dx = next.x - prev.x;
        const k = Math.sign(dx || 1) * Math.min(110, Math.max(24, Math.abs(dx) * 0.45));
        for (let step = 1; step < 14; step++) {
            const t = step / 14;
            const point = cubicPoint(
                prev,
                { x: prev.x + k, y: prev.y },
                { x: next.x - k, y: next.y },
                next,
                t,
            );
            for (const rect of obstacles) {
                if (handled.has(rect)) continue;
                if (
                    point.x > rect.x - pad &&
                    point.x < rect.x + rect.width + pad &&
                    point.y > rect.y - pad &&
                    point.y < rect.y + rect.height + pad
                ) {
                    return { rect, x: point.x, y: point.y };
                }
            }
        }
    }
    return null;
}

function cubicPoint(
    p0: { x: number; y: number },
    c1: { x: number; y: number },
    c2: { x: number; y: number },
    p1: { x: number; y: number },
    t: number,
): { x: number; y: number } {
    const u = 1 - t;
    const a = u * u * u;
    const b = 3 * u * u * t;
    const c = 3 * u * t * t;
    const d = t * t * t;
    return {
        x: a * p0.x + b * c1.x + c * c2.x + d * p1.x,
        y: a * p0.y + b * c1.y + c * c2.y + d * p1.y,
    };
}

function clamp(value: number, min: number, max: number): number {
    return Math.min(max, Math.max(min, value));
}

function edgeRoutes(edges: StrategyEdge[]): Map<string, number> {
    const groups = new Map<string, StrategyEdge[]>();
    const keyOf = (from: string, to: string): string =>
        JSON.stringify([from, to]);
    for (const edge of edges) {
        const key = keyOf(edge.from, edge.to);
        let group = groups.get(key);
        if (!group) {
            group = [];
            groups.set(key, group);
        }
        group.push(edge);
    }
    const routes = new Map<string, number>();
    for (const [key, group] of groups) {
        const [from, to] = JSON.parse(key) as [string, string];
        const reverse = groups.has(keyOf(to, from));
        const reciprocalOffset =
            from === to || !reverse ? 0 : from.localeCompare(to) < 0 ? -36 : 36;
        group.forEach((edge, index) => {
            const lane =
                from === to
                    ? index * 48
                    : (index - (group.length - 1) / 2) * 56;
            routes.set(edge.id, reciprocalOffset + lane);
        });
    }
    return routes;
}

function escapeText(value: string): string {
    return value
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;");
}

function escapeAttribute(value: string): string {
    return escapeText(value).replace(/"/g, "&quot;");
}

customElements.define("pc-edge-layer", PcEdgeLayer);
