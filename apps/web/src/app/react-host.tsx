import { createElement, type ReactNode } from "react";
import { createRoot, type Root } from "react-dom/client";
import { flushSync } from "react-dom";

const roots = new WeakMap<HTMLElement, Root>();
let rendering = false;

/** Preserve the synchronous controller API while React owns a view's DOM. */
export function renderReact(host: HTMLElement, view: ReactNode): void {
    let root = roots.get(host);
    if (!root) {
        root = createRoot(host);
        roots.set(host, root);
    }
    // A nested custom element may connect during its parent's React commit.
    if (rendering) {
        root.render(view);
        return;
    }
    rendering = true;
    try {
        flushSync(() => root!.render(view));
    } finally {
        rendering = false;
    }
}

/** Dockview reparents documents synchronously: a move must not dispose them. */
export function disconnectReact(host: HTMLElement): void {
    queueMicrotask(() => {
        if (host.isConnected) return;
        disposeReact(host);
    });
}

/** Called by the document owner only after its native work has stopped. */
export function disposeReact(host: HTMLElement): void {
    roots.get(host)?.unmount();
    roots.delete(host);
}

/** Declarative bridge to a controller-backed element during incremental porting. */
export function ControllerElement({ tag, configure, ...props }: {
    tag: string;
    configure?: (element: HTMLElement) => void;
    [key: string]: unknown;
}) {
    return createElement(tag, { ...props, ref: (element: HTMLElement | null) => { if (element) configure?.(element); } });
}
