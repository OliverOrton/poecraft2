/*
 * Persistence for the workspace, split deliberately so layout and domain
 * content survive independently:
 *   - layout (dockview arrangement) → localStorage (small, synchronous)
 *   - stash items (saved resources)  → IndexedDB "stash" store
 *   - drafts (unsaved/dirty work)    → IndexedDB "drafts" store, keyed by docId
 *
 * Drafts power crash/reload recovery and never appear in the Stash.
 */

const DB_NAME = "poecraft";
// Bump this whenever the schema changes (new/removed object stores). It must
// never be lower than a version already created in a browser, or opening the
// DB fails with VersionError. Strategy records reuse the existing stash/drafts
// stores via a resourceType discriminator, so no new store is needed here.
const DB_VERSION = 2;
const LAYOUT_KEY = "poecraft.layout";

/** Exported item state plus the session identity needed to reopen it. */
export interface ItemSnapshot {
    base: string;
    itemLevel: number;
    /** Current engine rarity, used when reopening or rebuilding the item. */
    rarity?: string;
    state: unknown;
}

/** Resolve current rarity from new snapshots or legacy state-only records. */
export function itemSnapshotRarity(snapshot: ItemSnapshot): string {
    if (snapshot.rarity) {
        return snapshot.rarity;
    }
    const code = (snapshot.state as { rarity?: unknown } | null)?.rarity;
    return code === 0 ? "normal" : code === 1 ? "magic" : "rare";
}

export interface ItemStashRecord extends ItemSnapshot {
    id: string;
    name: string;
    createdAt: number;
    resourceType?: "item";
}

export interface StrategyStashRecord {
    id: string;
    name: string;
    description: string;
    resourceType: "strategy";
    strategy: unknown;
    createdAt: number;
}

export type StashRecord = ItemStashRecord | StrategyStashRecord;

export interface DraftRecord {
    docId: string;
    base: string;
    itemLevel: number;
    rarity: string;
    /** Exported item state, or null for an untouched document. */
    state: unknown | null;
    history: { action: string; applied: boolean; added: number; removed: number }[];
    /** Stash id this draft was last saved as, if any. */
    savedRef: string | null;
    /** Stash name this draft was last saved as, if any. */
    savedName: string | null;
    dirty: boolean;
    updatedAt: number;
}

export interface StrategyDraftRecord {
    docId: string;
    strategy: unknown | null;
    /** Present only until pc-strategy-editor converts an Emulator snapshot. */
    sourceItem: ItemSnapshot | null;
    savedRef: string | null;
    savedName: string | null;
    dirty: boolean;
    updatedAt: number;
}

/** One Calculator goal slot; exactly one of familyModKey/group identifies it,
 * matching the solver goal-spec vocabulary. */
export interface CalculatorGoalSlot {
    familyModKey?: string;
    group?: string;
    /** family_tier_index threshold (1 = best); 0 accepts any tier. */
    minTier: number;
}

/** Calculator documents are never Stash resources; the draft only powers
 * reload recovery, so there is no savedRef/dirty machinery. */
export interface CalculatorDraftRecord {
    docId: string;
    base: string;
    itemLevel: number;
    /** Exported item state, or null before a base is chosen. */
    state: unknown | null;
    goalRarity: "normal" | "magic" | "rare";
    slots: CalculatorGoalSlot[];
    actionId: string;
    /** Extra fossils layered onto a selected fossil action (loadout). */
    fossilKeys: string[];
    updatedAt: number;
}

let dbPromise: Promise<IDBDatabase> | null = null;

function openDb(): Promise<IDBDatabase> {
    if (dbPromise) {
        return dbPromise;
    }
    dbPromise = new Promise((resolve, reject) => {
        const request = indexedDB.open(DB_NAME, DB_VERSION);
        request.onupgradeneeded = () => {
            const db = request.result;
            if (!db.objectStoreNames.contains("stash")) {
                db.createObjectStore("stash", { keyPath: "id" });
            }
            if (!db.objectStoreNames.contains("drafts")) {
                db.createObjectStore("drafts", { keyPath: "docId" });
            }
        };
        request.onsuccess = () => resolve(request.result);
        request.onerror = () => reject(request.error);
    });
    return dbPromise;
}

function tx<T>(
    store: string,
    mode: IDBTransactionMode,
    run: (store: IDBObjectStore) => IDBRequest<T>,
): Promise<T> {
    return openDb().then(
        (db) =>
            new Promise<T>((resolve, reject) => {
                const transaction = db.transaction(store, mode);
                const request = run(transaction.objectStore(store));
                request.onsuccess = () => resolve(request.result);
                request.onerror = () => reject(request.error);
            }),
    );
}

// --- stash ------------------------------------------------------------------

export function putStash(record: StashRecord): Promise<unknown> {
    return tx("stash", "readwrite", (store) => store.put(record));
}

export function listStash(): Promise<StashRecord[]> {
    return tx<StashRecord[]>("stash", "readonly", (store) => store.getAll());
}

export function getStash(id: string): Promise<StashRecord | undefined> {
    return tx("stash", "readonly", (store) => store.get(id));
}

export function deleteStash(id: string): Promise<unknown> {
    return tx("stash", "readwrite", (store) => store.delete(id));
}

// --- drafts -----------------------------------------------------------------

export function putDraft(record: DraftRecord): Promise<unknown> {
    return tx("drafts", "readwrite", (store) => store.put(record));
}

export function getDraft(docId: string): Promise<DraftRecord | undefined> {
    return tx("drafts", "readonly", (store) => store.get(docId));
}

export function deleteDraft(docId: string): Promise<unknown> {
    return tx("drafts", "readwrite", (store) => store.delete(docId));
}

// --- strategy drafts -------------------------------------------------------

export function putStrategyDraft(record: StrategyDraftRecord): Promise<unknown> {
    return tx("drafts", "readwrite", (store) => store.put(record));
}

export function getStrategyDraft(
    docId: string,
): Promise<StrategyDraftRecord | undefined> {
    return tx("drafts", "readonly", (store) => store.get(docId));
}

export function deleteStrategyDraft(docId: string): Promise<unknown> {
    return tx("drafts", "readwrite", (store) => store.delete(docId));
}

// --- calculator drafts (same store; deleteDraft(docId) removes these too) ---

export function putCalculatorDraft(
    record: CalculatorDraftRecord,
): Promise<unknown> {
    return tx("drafts", "readwrite", (store) => store.put(record));
}

export function getCalculatorDraft(
    docId: string,
): Promise<CalculatorDraftRecord | undefined> {
    return tx("drafts", "readonly", (store) => store.get(docId));
}

export function isStrategyStashRecord(
    record: StashRecord,
): record is StrategyStashRecord {
    return record.resourceType === "strategy";
}

// --- layout -----------------------------------------------------------------

export function saveLayout(layout: unknown): void {
    try {
        localStorage.setItem(LAYOUT_KEY, JSON.stringify(layout));
    } catch {
        /* storage full or unavailable — layout is best-effort */
    }
}

export function loadLayout(): unknown | null {
    const raw = localStorage.getItem(LAYOUT_KEY);
    if (!raw) {
        return null;
    }
    try {
        return JSON.parse(raw);
    } catch {
        return null;
    }
}

export function clearLayout(): void {
    localStorage.removeItem(LAYOUT_KEY);
}
