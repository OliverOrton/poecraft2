import {
    StrategyDocument,
    StrategyLabelContext,
    StrategyValidationIssue,
    StrategyViewport,
    strategyNodeConnectors,
} from "../strategy-model";
import { PcEdgeLayer } from "./pc-edge-layer";
import { PcStrategyNode } from "./pc-strategy-node";
import { StrategyBoardAnnotations } from "../strategy-eval-presentation";
import "./pc-edge-layer";
import "./pc-strategy-node";

interface BoardSelection {
    kind: "node" | "edge";
    id: string;
}

/*
 * Solver-compiled policies can be orders of magnitude larger than authored
 * boards. Above the simplified limits the edge layer skips its quadratic
 * card-collision and obstacle-routing work; above the summary limits the
 * board renders a collapsed summary instead of one element per node, so a
 * huge document still opens, validates, evaluates, and simulates without
 * freezing the page.
 */
export const BOARD_SIMPLIFIED_NODE_LIMIT = 220;
export const BOARD_SIMPLIFIED_EDGE_LIMIT = 320;
export const BOARD_SUMMARY_NODE_LIMIT = 1200;
export const BOARD_SUMMARY_EDGE_LIMIT = 2400;

/*
 * A compiled policy can be thousands of nodes wide, so the old 0.3 zoom-out
 * floor made whole-graph views impossible. The remaining floor exists only to
 * keep the transform and the grid background out of degenerate territory; at
 * 0.01 a node is roughly two pixels, which is already far past readable.
 */
export const BOARD_MIN_ZOOM = 0.01;
export const BOARD_MAX_ZOOM = 1.75;

export interface TraceHighlight {
    nodeIds: Set<string>;
    edgeIds: Set<string>;
    activeNodeId: string | null;
}

export class PcStrategyBoard extends HTMLElement {
    private strategy: StrategyDocument | null = null;
    private selection: BoardSelection | null = null;
    private issues: StrategyValidationIssue[] = [];
    private highlight: TraceHighlight = {
        nodeIds: new Set(),
        edgeIds: new Set(),
        activeNodeId: null,
    };
    private viewport: StrategyViewport = { panX: 24, panY: 24, zoom: 1 };
    private drag:
        | {
              nodeId: string;
              startX: number;
              startY: number;
              originX: number;
              originY: number;
          }
        | null = null;
    private pan:
        | {
              startX: number;
              startY: number;
              originX: number;
              originY: number;
          }
        | null = null;
    private connectingFrom: string | null = null;
    private connectingPort = "output";
    private reconnecting: {id: string; endpoint: "from" | "to"; anchor: string; anchorPort?: string} | null = null;
    private annotations: StrategyBoardAnnotations | null = null;
    private labelContext: StrategyLabelContext = {};
    private renderLargeBoard = false;

    connectedCallback(): void {
        if (!this.innerHTML) {
            this.innerHTML = `
                <div class="pc-board-viewport" tabindex="0">
                    <div class="pc-board-grid"></div>
                    <div class="pc-board-content">
                        <pc-edge-layer></pc-edge-layer>
                        <div class="pc-board-nodes"></div>
                    </div>
                    <div class="pc-board-summary" hidden>
                        <h3>Large strategy graph</h3>
                        <p class="pc-board-summary-counts"></p>
                        <p>Drawing every node at once would lock this page, so the
                        board stays collapsed. Validation, exact evaluation,
                        simulation, and saving still use the full graph.</p>
                        <button type="button" class="pc-board-summary-render">Render the graph anyway (slow)</button>
                    </div>
                    <div class="pc-board-stale-chip" hidden>Stale · graph changed</div>
                    <div class="pc-board-hint">Drag from a node output to another node · wheel to zoom · drag empty space to pan</div>
                </div>`;
            this.bind();
        }
        this.render();
    }

    disconnectedCallback(): void {
        this.endNodeDrag();
        this.cancelConnection();
    }

    setView(
        strategy: StrategyDocument,
        selection: BoardSelection | null,
        issues: StrategyValidationIssue[],
        highlight: TraceHighlight,
        annotations: StrategyBoardAnnotations | null = null,
        labelContext: StrategyLabelContext = {},
    ): void {
        this.strategy = strategy;
        this.selection = selection;
        this.issues = issues;
        this.highlight = highlight;
        this.annotations = annotations;
        this.labelContext = labelContext;
        this.viewport =
            strategy.ui?.viewport ?? this.viewport ?? {
                panX: 24,
                panY: 24,
                zoom: 1,
            };
        this.render();
    }

    private bind(): void {
        const viewport = this.querySelector<HTMLElement>(".pc-board-viewport")!;
        this.querySelector<HTMLButtonElement>(
            ".pc-board-summary-render",
        )!.addEventListener("click", () => {
            this.renderLargeBoard = true;
            this.render();
        });
        viewport.addEventListener("pointerdown", (event) => {
            if (
                (event.target as HTMLElement).closest(
                    "pc-strategy-node, [data-edge-id], .pc-board-summary",
                )
            ) {
                return;
            }
            this.pan = {
                startX: event.clientX,
                startY: event.clientY,
                originX: this.viewport.panX,
                originY: this.viewport.panY,
            };
            viewport.setPointerCapture(event.pointerId);
            this.dispatchEvent(
                new CustomEvent("strategy-select", {
                    bubbles: true,
                    detail: null,
                }),
            );
        });
        viewport.addEventListener("pointermove", (event) => {
            if (!this.pan) return;
            this.viewport.panX =
                this.pan.originX + event.clientX - this.pan.startX;
            this.viewport.panY =
                this.pan.originY + event.clientY - this.pan.startY;
            this.applyTransform();
        });
        viewport.addEventListener("pointerup", () => {
            if (!this.pan) return;
            this.pan = null;
            this.emitViewport();
        });
        viewport.addEventListener(
            "wheel",
            (event) => {
                event.preventDefault();
                const rect = viewport.getBoundingClientRect();
                const beforeX =
                    (event.clientX - rect.left - this.viewport.panX) /
                    this.viewport.zoom;
                const beforeY =
                    (event.clientY - rect.top - this.viewport.panY) /
                    this.viewport.zoom;
                const next = Math.min(
                    BOARD_MAX_ZOOM,
                    Math.max(
                        BOARD_MIN_ZOOM,
                        this.viewport.zoom * (event.deltaY > 0 ? 0.9 : 1.1),
                    ),
                );
                this.viewport.zoom = next;
                this.viewport.panX =
                    event.clientX - rect.left - beforeX * this.viewport.zoom;
                this.viewport.panY =
                    event.clientY - rect.top - beforeY * this.viewport.zoom;
                this.applyTransform();
                this.emitViewport();
            },
            { passive: false },
        );
        viewport.addEventListener("dragover", (event) => {
            event.preventDefault();
            if (event.dataTransfer) {
                event.dataTransfer.dropEffect = "copy";
            }
        });
        viewport.addEventListener("drop", (event) => {
            event.preventDefault();
            const paletteType =
                event.dataTransfer?.getData("application/x-poecraft-node") ?? "";
            if (!paletteType) return;
            const point = this.clientToGraph(event.clientX, event.clientY);
            this.dispatchEvent(
                new CustomEvent("strategy-add-node", {
                    bubbles: true,
                    detail: { paletteType, position: point },
                }),
            );
        });
        viewport.addEventListener("keydown", (event) => {
            if (event.key === "Delete" || event.key === "Backspace") {
                event.preventDefault();
                this.dispatchEvent(
                    new CustomEvent("strategy-delete-selection", {
                        bubbles: true,
                    }),
                );
            }
        });

        this.addEventListener("strategy-node-drag-start", (event) => {
            const detail = (
                event as CustomEvent<{
                    id: string;
                    clientX: number;
                    clientY: number;
                }>
            ).detail;
            const node = this.strategy?.nodes.find((entry) => entry.id === detail.id);
            if (!node) return;
            this.drag = {
                nodeId: detail.id,
                startX: detail.clientX,
                startY: detail.clientY,
                originX: node.position.x,
                originY: node.position.y,
            };
            window.addEventListener("pointermove", this.onNodeDrag);
            window.addEventListener("pointerup", this.endNodeDrag, { once: true });
            window.addEventListener("pointercancel", this.endNodeDrag, { once: true });
        });
        this.addEventListener("strategy-connect-start", (event) => {
            const detail = (
                event as CustomEvent<{
                    id: string;
                    clientX: number;
                    clientY: number;
                }>
            ).detail;
            this.cancelConnection();
            this.connectingFrom = detail.id;
            this.connectingPort = (event as CustomEvent<{port?: string}>).detail.port ?? "output";
            const point = this.clientToGraph(detail.clientX, detail.clientY);
            this.edgeLayer.setPreview({ anchor: detail.id, endpoint: "to", ...point });
            this.bindConnection();
        });
        this.addEventListener("strategy-edge-reconnect-start", event => {
            const detail = (event as CustomEvent<{id: string; endpoint: "from" | "to"; clientX: number; clientY: number}>).detail;
            const edge = this.strategy?.edges.find(edge => edge.id === detail.id);
            if (!edge) return;
            this.cancelConnection();
            const anchor = detail.endpoint === "from" ? edge.to : edge.from;
            this.reconnecting = {id: edge.id, endpoint: detail.endpoint, anchor, anchorPort: detail.endpoint === "from" ? edge.to_port : edge.from_port};
            this.edgeLayer.setPreview({anchor, anchorPort: this.reconnecting.anchorPort, endpoint: detail.endpoint, ...this.clientToGraph(detail.clientX, detail.clientY)});
            this.bindConnection();
        });
    }

    private readonly onNodeDrag = (event: PointerEvent): void => {
        if (!this.drag) return;
        const x =
            this.drag.originX + (event.clientX - this.drag.startX) / this.viewport.zoom;
        const y =
            this.drag.originY + (event.clientY - this.drag.startY) / this.viewport.zoom;
        this.dispatchEvent(
            new CustomEvent("strategy-node-move", {
                bubbles: true,
                detail: {
                    id: this.drag.nodeId,
                    position: { x: Math.round(x), y: Math.round(y) },
                },
            }),
        );
    };

    private readonly endNodeDrag = (): void => {
        const dragged = this.drag !== null;
        this.drag = null;
        window.removeEventListener("pointermove", this.onNodeDrag);
        window.removeEventListener("pointerup", this.endNodeDrag);
        window.removeEventListener("pointercancel", this.endNodeDrag);
        if (dragged) this.dispatchEvent(new CustomEvent("strategy-node-drag-end", {bubbles: true}));
    };

    private bindConnection(): void {
        this.querySelector<HTMLElement>(".pc-board-viewport")?.focus({preventScroll: true});
        window.addEventListener("pointermove", this.onConnectMove);
        window.addEventListener("pointerup", this.endConnect, {once: true});
        window.addEventListener("pointercancel", this.cancelConnection, {once: true});
        window.addEventListener("keydown", this.onConnectionKey);
    }

    private readonly onConnectionKey = (event: KeyboardEvent): void => {
        if (event.key === "Escape") {
            event.preventDefault();
            this.cancelConnection();
        }
    };

    readonly cancelConnection = (): void => {
        window.removeEventListener("pointermove", this.onConnectMove);
        window.removeEventListener("pointerup", this.endConnect);
        window.removeEventListener("pointercancel", this.cancelConnection);
        window.removeEventListener("keydown", this.onConnectionKey);
        this.connectingFrom = null;
        this.reconnecting = null;
        this.querySelector<PcEdgeLayer>("pc-edge-layer")?.setPreview(null);
    };

    private readonly onConnectMove = (event: PointerEvent): void => {
        const connection = this.reconnecting ?? (this.connectingFrom ? {anchor: this.connectingFrom, endpoint: "to" as const} : null);
        if (!connection) return;
        this.edgeLayer.setPreview({...connection, ...this.clientToGraph(event.clientX, event.clientY)});
    };

    private readonly endConnect = (event: PointerEvent): void => {
        const reconnecting = this.reconnecting;
        const from = this.connectingFrom;
        const fromPort = this.connectingPort;
        this.cancelConnection();
        const hit = document.elementFromPoint(event.clientX, event.clientY);
        const target = hit?.closest<PcStrategyNode>("pc-strategy-node");
        const nodeId = target?.dataset.nodeId;
        if (!nodeId) return; // Empty drops leave the original edge intact.
        const targetNode = this.strategy?.nodes.find(node => node.id === nodeId);
        if (!targetNode) return;
        const inputs = strategyNodeConnectors(targetNode).inputs;
        const port = hit?.closest<HTMLElement>("[data-port-id]")?.dataset.portId;
        const toPort = port ?? inputs[0]?.id;
        if (reconnecting?.endpoint !== "from" && !inputs.some(input => input.id === toPort)) return;
        if (reconnecting) {
            // Terminals have no output port. Otherwise keep the same graph validation as new edges.
            if (reconnecting.endpoint === "from" && this.strategy?.nodes.find(node => node.id === nodeId)?.kind === "terminal") return;
            this.dispatchEvent(new CustomEvent("strategy-edge-reconnect", {
                bubbles: true, detail: {id: reconnecting.id, endpoint: reconnecting.endpoint, nodeId, port: reconnecting.endpoint === "from" ? (port ?? "output") : toPort},
            }));
        } else if (from) {
            this.dispatchEvent(new CustomEvent("strategy-edge-create", {bubbles: true, detail: {from, to: nodeId, fromPort, toPort}}));
        }
    };

    private render(): void {
        if (!this.isConnected || !this.strategy) {
            return;
        }
        this.applyTransform();
        const nodeCount = this.strategy.nodes.length;
        const edgeCount = this.strategy.edges.length;
        const summary =
            !this.renderLargeBoard &&
            (nodeCount > BOARD_SUMMARY_NODE_LIMIT ||
                edgeCount > BOARD_SUMMARY_EDGE_LIMIT);
        const simplified =
            nodeCount > BOARD_SIMPLIFIED_NODE_LIMIT ||
            edgeCount > BOARD_SIMPLIFIED_EDGE_LIMIT;
        const summaryEl = this.querySelector<HTMLElement>(".pc-board-summary");
        if (summaryEl) {
            summaryEl.hidden = !summary;
            if (summary) {
                const counts = summaryEl.querySelector(
                    ".pc-board-summary-counts",
                );
                if (counts) {
                    counts.textContent = `${nodeCount.toLocaleString()} nodes · ${edgeCount.toLocaleString()} edges`;
                }
            }
        }
        const container = this.querySelector(".pc-board-nodes")!;
        const staleChip = this.querySelector<HTMLElement>(
            ".pc-board-stale-chip",
        );
        if (staleChip) {
            staleChip.hidden = summary || !this.annotations?.stale;
        }
        if (summary) {
            container.replaceChildren();
            this.edgeLayer.setView({
                nodes: [],
                edges: [],
                selectedEdgeId: null,
                highlightedEdgeIds: new Set(),
                warningEdgeIds: new Set(),
                canvas: { width: 3000, height: 2000 },
            });
            return;
        }
        const warningNodeIds = new Map<string, StrategyValidationIssue[]>();
        const warningEdgeIds = new Set<string>();
        for (const issue of this.issues) {
            if (issue.nodeId) {
                let list = warningNodeIds.get(issue.nodeId);
                if (!list) {
                    list = [];
                    warningNodeIds.set(issue.nodeId, list);
                }
                list.push(issue);
            }
            if (issue.edgeId) warningEdgeIds.add(issue.edgeId);
        }

        const canvas = this.canvasSize();
        const content = this.querySelector<HTMLElement>(".pc-board-content");
        if (content) {
            content.style.width = `${canvas.width}px`;
            content.style.height = `${canvas.height}px`;
        }
        container.replaceChildren(
            ...this.strategy.nodes.map((node) => {
                const element = document.createElement(
                    "pc-strategy-node",
                ) as PcStrategyNode;
                element.setView({
                    node,
                    document: this.strategy ?? undefined,
                    selected:
                        this.selection?.kind === "node" &&
                        this.selection.id === node.id,
                    active: this.highlight.activeNodeId === node.id,
                    taken: this.highlight.nodeIds.has(node.id),
                    issues: warningNodeIds.get(node.id) ?? [],
                    annotation: this.annotations?.nodeBadges.get(node.id),
                    annotationStale: this.annotations?.stale,
                    labelContext: this.labelContext,
                });
                return element;
            }),
        );
        this.edgeLayer.setView({
            nodes: this.strategy.nodes,
            edges: this.strategy.edges,
            selectedEdgeId:
                this.selection?.kind === "edge" ? this.selection.id : null,
            highlightedEdgeIds: this.highlight.edgeIds,
            warningEdgeIds,
            annotations: this.annotations?.edgeLabels,
            annotationsStale: this.annotations?.stale,
            canvas,
            // Reading offsetWidth/offsetHeight forces a synchronous layout of
            // every node element; simplified rendering never uses the sizes.
            nodeSizes: simplified ? undefined : this.measureNodeSizes(),
            simplified,
        });
    }

    /** Canvas large enough to hold every node plus room for cards and lanes. */
    private canvasSize(): { width: number; height: number } {
        let maxX = 0;
        let maxY = 0;
        for (const node of this.strategy?.nodes ?? []) {
            maxX = Math.max(maxX, node.position.x);
            maxY = Math.max(maxY, node.position.y);
        }
        return {
            width: Math.max(3000, Math.ceil(maxX + 900)),
            height: Math.max(2000, Math.ceil(maxY + 800)),
        };
    }

    /** Rendered size of each node element, in graph units. */
    measureNodeSizes(): Map<string, { width: number; height: number }> {
        const sizes = new Map<string, { width: number; height: number }>();
        this.querySelectorAll<PcStrategyNode>("pc-strategy-node").forEach(
            (element) => {
                const id = element.dataset.nodeId;
                if (!id) return;
                sizes.set(id, {
                    width: element.offsetWidth || 210,
                    height: element.offsetHeight || 132,
                });
            },
        );
        return sizes;
    }

    /** Client size of the visible board viewport. */
    viewportSize(): { width: number; height: number } {
        const viewport = this.querySelector<HTMLElement>(".pc-board-viewport");
        const rect = viewport?.getBoundingClientRect();
        return { width: rect?.width ?? 0, height: rect?.height ?? 0 };
    }

    private get edgeLayer(): PcEdgeLayer {
        return this.querySelector<PcEdgeLayer>("pc-edge-layer")!;
    }

    private applyTransform(): void {
        const content = this.querySelector<HTMLElement>(".pc-board-content");
        const grid = this.querySelector<HTMLElement>(".pc-board-grid");
        if (!content || !grid) return;
        content.style.transform = `translate(${this.viewport.panX}px, ${this.viewport.panY}px) scale(${this.viewport.zoom})`;
        grid.style.backgroundPosition = `${this.viewport.panX}px ${this.viewport.panY}px`;
        grid.style.backgroundSize = `${24 * this.viewport.zoom}px ${24 * this.viewport.zoom}px`;
    }

    private clientToGraph(clientX: number, clientY: number): { x: number; y: number } {
        const rect = this.getBoundingClientRect();
        return {
            x: Math.round(
                (clientX - rect.left - this.viewport.panX) / this.viewport.zoom,
            ),
            y: Math.round(
                (clientY - rect.top - this.viewport.panY) / this.viewport.zoom,
            ),
        };
    }

    private emitViewport(): void {
        this.dispatchEvent(
            new CustomEvent("strategy-viewport-change", {
                bubbles: true,
                detail: { ...this.viewport },
            }),
        );
    }
}

customElements.define("pc-strategy-board", PcStrategyBoard);
