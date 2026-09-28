/** Bounded, immutable snapshots of authored state. New edits discard the redo branch. */
export interface HistoryData<T> {
    entries: T[];
    cursor: number;
}

export class EditHistory<T> {
    private entries: string[] = [];
    private index = -1;
    constructor(private readonly limit = 100, private readonly byteLimit = 8 * 1024 * 1024) {}

    get cursor(): number { return this.index; }
    get length(): number { return this.entries.length; }
    get canUndo(): boolean { return this.index > 0; }
    get canRedo(): boolean { return this.index < this.entries.length - 1; }

    at(index: number): T | undefined {
        return this.entries[index] === undefined ? undefined : JSON.parse(this.entries[index]);
    }

    reset(value: T): void {
        this.entries = [JSON.stringify(value)];
        this.index = 0;
    }

    record(value: T, replace = false): void {
        const encoded = JSON.stringify(value);
        if (encoded === this.entries[this.index]) return;
        this.entries.splice(this.index + 1);
        if (replace && this.index > 0) this.entries[this.index] = encoded;
        else { this.entries.push(encoded); this.index++; }
        // UTF-16 strings use up to two bytes per code unit. Retain at least the current state.
        let bytes = this.entries.reduce((sum, entry) => sum + entry.length * 2, 0);
        while (this.entries.length > 1 && (this.entries.length > this.limit || bytes > this.byteLimit)) {
            bytes -= this.entries.shift()!.length * 2;
            this.index--;
        }
    }

    go(index: number): T | undefined {
        if (!Number.isInteger(index) || index < 0 || index >= this.entries.length) return undefined;
        this.index = index;
        return this.at(index);
    }

    export(): HistoryData<T> {
        return { entries: this.entries.map(entry => JSON.parse(entry)), cursor: this.index };
    }

    /** Older drafts have no snapshots. Callers reset to their current document in that case. */
    restore(data: HistoryData<T> | undefined, current: T): void {
        this.reset(current);
        if (!data || !Array.isArray(data.entries) || !Number.isInteger(data.cursor) ||
            data.cursor < 0 || data.cursor >= data.entries.length ||
            JSON.stringify(data.entries[data.cursor]) !== JSON.stringify(current)) return;
        const encoded = data.entries.map(entry => JSON.stringify(entry));
        if (encoded.length > this.limit || encoded.some(entry => typeof entry !== "string") ||
            encoded.reduce((sum, entry) => sum + entry.length * 2, 0) > this.byteLimit) return;
        this.entries = encoded;
        this.index = data.cursor;
    }
}

/** Leave text editing shortcuts to the focused control's native undo stack. */
export function historyShortcut(event: KeyboardEvent): "undo" | "redo" | null {
    if (event.defaultPrevented || event.altKey || !(event.ctrlKey || event.metaKey) ||
        (event.target instanceof Element && event.target.closest("input, textarea, select, [contenteditable]:not([contenteditable=false])"))) return null;
    const key = event.key.toLowerCase();
    if (key === "z") return event.shiftKey ? "redo" : "undo";
    if (key === "y" && !event.shiftKey) return "redo";
    return null;
}
