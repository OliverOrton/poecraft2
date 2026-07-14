import {
    StrategyDocument,
    StrategyLabelContext,
    StrategyValidationIssue,
    StrategyViewport,
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
    private annotations: StrategyBoardAnnotations | null = null;
    private labelContext: StrategyLabelContext = {};

    connectedCallback(): void {
        if (!this.innerHTML) {
            this.innerHTML = `
                <div class="pc-board-viewport" tabindex="0">
                    <div class="pc-board-grid"></div>
                    <div class="pc-board-content">
                        <pc-edge-layer></pc-edge-layer>
                        <div class="pc-board-nodes"></div>
                    </div>
                    <div class="pc-board-stale-chip" hidden>Stale · graph changed</div>
                    <div class="pc-board-hint">Drag from a node output to another node · wheel to zoom · drag empty space to pan</div>
                </div>`;
            this.bind();
        }
        this.render();
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
        viewport.addEventListener("pointerdown", (event) => {
            if (
                (event.target as HTMLElement).closest(
                    "pc-strategy-node, [data-edge-id]",
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
                    1.75,
                    Math.max(0.45, this.viewport.zoom * (event.deltaY > 0 ? 0.9 : 1.1)),
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
        });
        this.addEventListener("strategy-connect-start", (event) => {
            const detail = (
                event as CustomEvent<{
                    id: string;
                    clientX: number;
                    clientY: number;
                }>
            ).detail;
            this.connectingFrom = detail.id;
            const point = this.clientToGraph(detail.clientX, detail.clientY);
            this.edgeLayer.setPreview({ from: detail.id, ...point });
            window.addEventListener("pointermove", this.onConnectMove);
            window.addEventListener("pointerup", this.endConnect, { once: true });
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
        this.drag = null;
        window.removeEventListener("pointermove", this.onNodeDrag);
    };

    private readonly onConnectMove = (event: PointerEvent): void => {
        if (!this.connectingFrom) return;
        this.edgeLayer.setPreview({
            from: this.connectingFrom,
            ...this.clientToGraph(event.clientX, event.clientY),
        });
    };

    private readonly endConnect = (event: PointerEvent): void => {
        window.removeEventListener("pointermove", this.onConnectMove);
        const from = this.connectingFrom;
        this.connectingFrom = null;
        this.edgeLayer.setPreview(null);
        if (!from) return;
        const target = document
            .elementFromPoint(event.clientX, event.clientY)
            ?.closest<PcStrategyNode>("pc-strategy-node");
        const to = target?.dataset.nodeId;
        if (to) {
            this.dispatchEvent(
                new CustomEvent("strategy-edge-create", {
                    bubbles: true,
                    detail: { from, to },
                }),
            );
        }
    };

    private render(): void {
        if (!this.isConnected || !this.strategy) {
            return;
        }
        this.applyTransform();
        const warningNodeIds = new Map<string, StrategyValidationIssue[]>();
        const warningEdgeIds = new Set<string>();
        for (const issue of this.issues) {
            if (issue.nodeId) {
                warningNodeIds.set(issue.nodeId, [
                    ...(warningNodeIds.get(issue.nodeId) ?? []),
                    issue,
                ]);
            }
            if (issue.edgeId) warningEdgeIds.add(issue.edgeId);
        }

        const container = this.querySelector(".pc-board-nodes")!;
        const staleChip = this.querySelector<HTMLElement>(
            ".pc-board-stale-chip",
        );
        if (staleChip) staleChip.hidden = !this.annotations?.stale;
        container.replaceChildren(
            ...this.strategy.nodes.map((node) => {
                const element = document.createElement(
                    "pc-strategy-node",
                ) as PcStrategyNode;
                element.setView({
                    node,
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
        });
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
