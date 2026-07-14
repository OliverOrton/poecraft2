import {
    StrategyEdge,
    StrategyNode,
    strategyEdgeLabel,
    strategyEdgeLabelLines,
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
            const labelLines = strategyEdgeLabelLines(edge);
            const annotation = this.view.annotations?.get(edge.id);
            const point = edgeLabelPoint(from, to, route);
            const lineHeight = 13;
            const annotationOffset = annotation ? 14 : 0;
            const labelStartY =
                point.y -
                ((labelLines.length - 1) * lineHeight + annotationOffset) / 2;
            const widestLine = Math.max(
                ...labelLines.map((line) => line.length),
                annotation?.label.length ?? 0,
            );
            const backdropWidth = Math.max(66, widestLine * 6.2 + 18);
            const backdropHeight =
                labelLines.length * lineHeight + annotationOffset + 4;
            const labelX = point.x - backdropWidth / 2 + 9;
            const title = annotation ? `${label}\n${annotation.title}` : label;
            return [
                `<g data-edge-id="${escapeAttribute(edge.id)}">
                    <title>${escapeText(title)}</title>
                    <path class="pc-edge-hit" d="${d}"></path>
                    <path class="${classes}" d="${d}" marker-end="url(#pc-edge-arrow)"></path>
                    <rect class="pc-edge-label-backdrop" x="${point.x - backdropWidth / 2}" y="${labelStartY - 10}" width="${backdropWidth}" height="${backdropHeight}" rx="5"></rect>
                    <text class="pc-edge-label ${annotation ? "has-annotation" : ""} ${this.view.annotationsStale ? "is-stale" : ""}" x="${labelX}" y="${labelStartY}">
                        ${labelLines.map((line, index) => `<tspan class="pc-edge-condition-line" x="${labelX}"${index === 0 ? "" : ` dy="${lineHeight}"`}>${escapeText(line)}</tspan>`).join("")}
                        ${annotation ? `<tspan class="pc-edge-eval-label" x="${point.x}" dy="14">${escapeText(annotation.label)}</tspan>` : ""}
                    </text>
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
    return value.replace(/&/g, "&amp;").replace(/</g, "&lt;");
}

function escapeAttribute(value: string): string {
    return escapeText(value).replace(/"/g, "&quot;");
}

customElements.define("pc-edge-layer", PcEdgeLayer);
