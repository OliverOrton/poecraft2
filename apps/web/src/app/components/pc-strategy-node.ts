import {
    StrategyNode,
    StrategyValidationIssue,
    operationLabel,
} from "../strategy-model";
import { StrategyNodeAnnotation } from "../strategy-eval-presentation";

export interface StrategyNodeView {
    node: StrategyNode;
    selected: boolean;
    active: boolean;
    taken: boolean;
    issues: StrategyValidationIssue[];
    annotation?: StrategyNodeAnnotation;
    annotationStale?: boolean;
}

export class PcStrategyNode extends HTMLElement {
    private view: StrategyNodeView | null = null;

    setView(view: StrategyNodeView): void {
        this.view = view;
        this.render();
    }

    connectedCallback(): void {
        this.render();
    }

    private render(): void {
        if (!this.view) {
            return;
        }
        const { node, selected, active, taken, issues, annotation, annotationStale } =
            this.view;
        this.dataset.nodeId = node.id;
        this.className = [
            "pc-strategy-node",
            `is-${node.kind}`,
            node.terminal ? `is-${node.terminal}` : "",
            selected ? "is-selected" : "",
            active ? "is-active" : "",
            taken ? "is-taken" : "",
            issues.length ? "has-warning" : "",
            annotation ? "has-annotation" : "",
            annotationStale ? "is-annotation-stale" : "",
        ]
            .filter(Boolean)
            .join(" ");
        this.style.left = `${node.position.x}px`;
        this.style.top = `${node.position.y}px`;

        const subtitle =
            node.kind === "operation"
                ? operationLabel(node.operation)
                : node.kind === "terminal"
                  ? node.terminal ?? "terminal"
                  : node.kind === "router"
                    ? "Condition router"
                    : "Initial item state";
        this.innerHTML = `
            <button class="pc-node-port pc-node-input" title="Connect here" aria-label="Input"></button>
            <div class="pc-node-header">
                <span class="pc-node-kind">${escapeHtml(node.kind)}</span>
                <span class="pc-node-header-detail">
                    ${annotation ? `<span class="pc-node-eval-badge is-${annotation.kind}" title="${escapeHtml(annotation.title)}">${escapeHtml(annotation.label)}</span>` : ""}
                    ${issues.length ? '<span class="pc-node-warning" title="Validation warning">!</span>' : ""}
                </span>
            </div>
            <div class="pc-node-title">${escapeHtml(node.name || node.id)}</div>
            <div class="pc-node-subtitle">${escapeHtml(subtitle)}</div>
            ${node.kind === "terminal" ? "" : '<button class="pc-node-port pc-node-output" title="Drag to connect" aria-label="Output"></button>'}`;

        this.addEventListener("pointerdown", (event) => {
            if ((event.target as HTMLElement).closest(".pc-node-port")) {
                return;
            }
            event.stopPropagation();
            this.dispatchEvent(
                new CustomEvent("strategy-select", {
                    bubbles: true,
                    detail: { kind: "node", id: node.id },
                }),
            );
        });

        this.querySelector(".pc-node-header")?.addEventListener(
            "pointerdown",
            (event) => {
                event.preventDefault();
                event.stopPropagation();
                this.dispatchEvent(
                    new CustomEvent("strategy-node-drag-start", {
                        bubbles: true,
                        detail: {
                            id: node.id,
                            clientX: (event as PointerEvent).clientX,
                            clientY: (event as PointerEvent).clientY,
                        },
                    }),
                );
            },
        );
        this.querySelector(".pc-node-output")?.addEventListener(
            "pointerdown",
            (event) => {
                event.preventDefault();
                event.stopPropagation();
                this.dispatchEvent(
                    new CustomEvent("strategy-connect-start", {
                        bubbles: true,
                        detail: {
                            id: node.id,
                            clientX: (event as PointerEvent).clientX,
                            clientY: (event as PointerEvent).clientY,
                        },
                    }),
                );
            },
        );
    }
}

function escapeHtml(text: string): string {
    return text
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;");
}

customElements.define("pc-strategy-node", PcStrategyNode);
