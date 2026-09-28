import type { PointerEvent as ReactPointerEvent } from "react";
import { disconnectReact, renderReact } from "../react-host";
import { formatModText } from "../mod-text";
import { GameIcon } from "./pc-game-icon";
import {
    StrategyLabelContext,
    StrategyNode,
    StrategyValidationIssue,
    operationLabel,
    strategyNodeLabel,
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
    labelContext?: StrategyLabelContext;
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
        const {
            node,
            selected,
            active,
            taken,
            issues,
            annotation,
            annotationStale,
            labelContext,
        } = this.view;
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
                ? operationLabel(node.operation, labelContext)
                : node.kind === "terminal"
                  ? node.terminal ?? "terminal"
                  : node.kind === "router"
                    ? "Condition router"
                    : "Initial item state";

        const operation = node.operation;
        const materialKey = operation?.type === "essence" ? operation.params?.essence_key : undefined;
        const material = labelContext?.catalog?.essences.find(entry => entry.key === materialKey);
        const icon = material ? "name:" + material.name : operation?.type === "influence_exalt" && operation.params?.influence
            ? "influence:" + operation.params.influence : "action:" + operation?.type;

        const emit = (name: string, detail: unknown) => this.dispatchEvent(new CustomEvent(name, {bubbles: true, detail}));
        const startPointer = (name: string, event: ReactPointerEvent) => {
            event.preventDefault(); event.stopPropagation();
            emit(name, {id: node.id, clientX: event.clientX, clientY: event.clientY});
        };
        renderReact(this, <>
            <button className="pc-node-port pc-node-input" title="Connect here" aria-label="Input" />
            <div className="pc-node-header" onPointerDown={event => startPointer("strategy-node-drag-start", event)}>
                <span className="pc-node-kind">{node.kind}</span>
                <span className="pc-node-header-detail">
                    {annotation && <span className={"pc-node-eval-badge is-" + annotation.kind} title={annotation.title}>{annotation.label}</span>}
                    {!!issues.length && <span className="pc-node-warning" title="Validation warning">!</span>}
                </span>
            </div>
            <div className="pc-node-title">
                {node.kind === "operation" && <GameIcon assetKey={icon} />}
                {formatModText(strategyNodeLabel(node, labelContext))}
            </div>
            <div className="pc-node-subtitle">{formatModText(subtitle)}</div>
            {node.kind !== "terminal" && <button className="pc-node-port pc-node-output" title="Drag to connect" aria-label="Output"
                onPointerDown={event => startPointer("strategy-connect-start", event)} />}
        </>);
        this.onpointerdown = event => {
            if ((event.target as HTMLElement).closest(".pc-node-port, .pc-node-header")) return;
            event.stopPropagation();
            emit("strategy-select", {kind: "node", id: node.id});
        };
    }
    disconnectedCallback(): void { disconnectReact(this); }
}
customElements.define("pc-strategy-node", PcStrategyNode);
