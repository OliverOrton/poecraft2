/*
 * Workspace singleton accessor. Lets leaf documents (emulator, stash) talk to
 * the workspace host without a circular import on the pc-workspace module.
 */

import { ItemSnapshot, StashRecord } from "./persistence";

export type OpenMode = "edit" | "copy";

export interface DocumentHandlers {
    /** Persist the document to the Stash. Returns false if the user cancelled. */
    save(): Promise<boolean>;
    /** Release worker/WASM resources before the panel is permanently removed. */
    dispose(): Promise<void>;
}

export interface WorkspaceApi {
    /** Register a document's imperative handlers, keyed by docId. */
    registerDocument(docId: string, handlers: DocumentHandlers): void;
    unregisterDocument(docId: string): void;
    /** Open a new emulator document, optionally seeded from a stash item. */
    openEmulator(
        seed?: ItemSnapshot,
        mode?: OpenMode,
        savedRef?: string,
        savedName?: string,
    ): Promise<void>;
    /** Open (or focus) the Stash document. */
    openStash(): void;
    /** Persist a stash record and notify open Stash documents. */
    saveToStash(record: StashRecord): Promise<void>;
    /** Report a document's dirty state and title so its tab can update. */
    notifyDirty(docId: string, dirty: boolean, title: string): void;
    /** Subscribe to stash changes; returns an unsubscribe function. */
    onStashChange(listener: () => void): () => void;
}

let current: WorkspaceApi | null = null;

export function setWorkspace(api: WorkspaceApi | null): void {
    current = api;
}

export function workspace(): WorkspaceApi {
    if (!current) {
        throw new Error("workspace is not initialised");
    }
    return current;
}
