import {
    StrategyEdge,
    StrategyNode,
    strategyEdgeCardPresentation,
    strategyEdgeLabel,
} from "../strategy-model";
import { StrategyEdgeAnnotation } from "../strategy-eval-presentation";

const NODE_WIDTH = 210;
const PORT_Y = 54;

export interface EdgeLayerView {
    nodes: StrategyNode[];
    edges: StrategyEdge[];
    selectedEdgeId: string | null;
    highlightedEdgeIds: Set<string>;
    warningEdgeIds: Set<string>;
    annotations?: Map<string, StrategyEdgeAnnotation>;
    annotationsStale?: boolean;
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

    setView(view: EdgeLayerView): void {
        this.view = view;
        this.render();
    }

    setPreview(preview: { from: string; x: number; y: number } | null): void {
        this.preview = preview;
        this.render();
    }

    connectedCallback(): void {
        this.render();
    }

    private render(): void {
        const nodes = new Map(this.view.nodes.map((node) => [node.id, node]));
        const routes = edgeRoutes(this.view.edges);
        const paths = this.view.edges.flatMap((edge) => {
            const from = nodes.get(edge.from);
            const to = nodes.get(edge.to);
            if (!from || !to) {
                return [];
            }
            const route = routes.get(edge.id) ?? 0;
            const d = edgePath(from, to, route);
            const classes = [
                "pc-edge-path",
                edge.id === this.view.selectedEdgeId ? "is-selected" : "",
                this.view.highlightedEdgeIds.has(edge.id) ? "is-taken" : "",
                this.view.warningEdgeIds.has(edge.id) ? "has-warning" : "",
            ]
                .filter(Boolean)
                .join(" ");
            const label = strategyEdgeLabel(edge);
            const presentation = strategyEdgeCardPresentation(edge);
            const annotation = this.view.annotations?.get(edge.id);
            const point = edgeLabelPoint(from, to, route);
            const rowHeight = 19;
            const headerHeight = presentation.header ? 27 : 0;
            const annotationHeight = annotation ? 20 : 0;
            const widestText = Math.max(
                presentation.header.length + 8,
                ...presentation.rows.map(
                    (row) => row.label.length + row.depth * 3,
                ),
            );
            const cardWidth = presentation.compact
                ? Math.max(78, Math.min(300, widestText * 6.1 + 26))
                : Math.max(224, Math.min(320, widestText * 6.1 + 34));
            const rowHeights = presentation.rows.map((row) => {
                if (row.kind === "group") return rowHeight;
                const availableWidth = Math.max(
                    72,
                    cardWidth - 30 - row.depth * 10,
                );
                const charactersPerLine = Math.max(
                    12,
                    Math.floor(availableWidth / 6.1),
                );
                const lines = Math.max(
                    1,
                    Math.ceil(row.label.length / charactersPerLine),
                );
                return lines * 12 + 7;
            });
            const cardHeight = presentation.compact
                ? Math.max(30, rowHeights[0] ?? 30) + annotationHeight
                : headerHeight + rowHeights.reduce((sum, height) => sum + height, 0) +
                  10 + annotationHeight;
            const title = annotation ? `${label}\n${annotation.title}` : label;
            return [
                `<g class="pc-edge-group ${edge.id === this.view.selectedEdgeId ? "is-selected" : ""}" data-edge-id="${escapeAttribute(edge.id)}">
                    <title>${escapeText(title)}</title>
                    <path class="pc-edge-hit" d="${d}"></path>
                    <path class="${classes}" d="${d}" marker-end="url(#pc-edge-arrow)"></path>
                    <foreignObject class="pc-edge-card-object" x="${point.x - cardWidth / 2}" y="${point.y - cardHeight / 2}" width="${cardWidth}" height="${cardHeight}">
                        <div xmlns="http://www.w3.org/1999/xhtml" class="pc-edge-card ${presentation.compact ? "is-compact" : "is-tree"} ${presentation.manual ? "is-manual" : ""} ${annotation ? "has-annotation" : ""} ${this.view.annotationsStale ? "is-stale" : ""}">
                            ${presentation.header ? `<div class="pc-edge-card-head"><span class="pc-edge-card-operator">${escapeText(presentation.header)}</span>${presentation.count === undefined ? "" : `<span>${presentation.count} rule${presentation.count === 1 ? "" : "s"}</span>`}</div>` : ""}
                            <div class="pc-edge-card-rows">
                                ${presentation.rows.map((row, index) => `<div class="pc-edge-card-row is-${row.kind}" style="--pc-edge-depth: ${Math.min(row.depth, 8)}; --pc-edge-row-height: ${rowHeights[index]}px"><span class="pc-edge-card-branch"></span>${row.kind === "group" ? `<span class="pc-edge-card-operator">${escapeText(row.label)}</span><span class="pc-edge-card-count">${row.count ?? 0} rule${row.count === 1 ? "" : "s"}</span>` : `<span class="pc-edge-card-leaf">${escapeText(row.label)}</span>`}</div>`).join("")}
                            </div>
                            ${annotation ? `<div class="pc-edge-card-eval">${escapeText(annotation.label)}</div>` : ""}
                        </div>
                    </foreignObject>
                </g>`,
            ];
        });

        if (this.preview) {
            const from = nodes.get(this.preview.from);
            if (from) {
                const x1 = from.position.x + NODE_WIDTH;
                const y1 = from.position.y + PORT_Y;
                const d = curvePath(x1, y1, this.preview.x, this.preview.y);
                paths.push(`<path class="pc-edge-preview" d="${d}"></path>`);
            }
        }

        this.innerHTML = `
            <svg viewBox="0 0 3000 2000" preserveAspectRatio="none">
                <defs>
                    <marker id="pc-edge-arrow" markerWidth="7" markerHeight="7"
                        refX="6" refY="3.5" orient="auto">
                        <path d="M0,0 L7,3.5 L0,7 z"></path>
                    </marker>
                </defs>
                ${paths.join("")}
            </svg>`;
        this.querySelectorAll<SVGGElement>("[data-edge-id]").forEach((group) => {
            group.addEventListener("pointerdown", (event) => {
                event.stopPropagation();
                this.dispatchEvent(
                    new CustomEvent("strategy-select", {
                        bubbles: true,
                        detail: { kind: "edge", id: group.dataset.edgeId },
                    }),
                );
            });
        });
    }
}

function edgePath(
    from: StrategyNode,
    to: StrategyNode,
    route: number,
): string {
    const x1 = from.position.x + NODE_WIDTH;
    const y1 = from.position.y + PORT_Y;
    const x2 = to.position.x;
    const y2 = to.position.y + PORT_Y;
    if (from.id === to.id) {
        const height = 105 + Math.abs(route) * 0.9;
        const width = 120 + route;
        return `M ${x1} ${y1} C ${x1 + width} ${y1 - height}, ${x2 - width} ${y2 - height}, ${x2} ${y2}`;
    }
    return curvePath(x1, y1, x2, y2, route);
}

function curvePath(
    x1: number,
    y1: number,
    x2: number,
    y2: number,
    route = 0,
): string {
    const bend = Math.max(80, Math.abs(x2 - x1) * 0.48);
    return `M ${x1} ${y1} C ${x1 + bend} ${y1 + route}, ${x2 - bend} ${y2 + route}, ${x2} ${y2}`;
}

function edgeLabelPoint(
    from: StrategyNode,
    to: StrategyNode,
    route: number,
): { x: number; y: number } {
    if (from.id === to.id) {
        return {
            x: from.position.x + NODE_WIDTH / 2,
            y: from.position.y - 55 - Math.abs(route) * 0.7,
        };
    }
    return {
        x: (from.position.x + NODE_WIDTH + to.position.x) / 2,
        y: (from.position.y + to.position.y) / 2 + PORT_Y - 8 + route * 0.75,
    };
}

function edgeRoutes(edges: StrategyEdge[]): Map<string, number> {
    const groups = new Map<string, StrategyEdge[]>();
    for (const edge of edges) {
        const key = `${edge.from}\u0000${edge.to}`;
        groups.set(key, [...(groups.get(key) ?? []), edge]);
    }
    const routes = new Map<string, number>();
    for (const [key, group] of groups) {
        const [from, to] = key.split("\u0000");
        const reverse = groups.has(`${to}\u0000${from}`);
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
