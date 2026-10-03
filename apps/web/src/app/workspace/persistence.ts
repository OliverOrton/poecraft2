/*
 * Persistence for the workspace, split deliberately so layout and domain
 * content survive independently:
 *   - layout (dockview arrangement) → localStorage (small, synchronous)
 *   - stash items (saved resources)  → IndexedDB "stash" store
 *   - drafts (unsaved/dirty work)    → IndexedDB "drafts" store, keyed by docId
 *
 * Drafts power crash/reload recovery and never appear in the Stash.
 */

import type { HistoryData } from "../edit-history";
import type { CraftSpend } from "../craft-costs";
import type { StrategyDocument } from "../strategy-model";

const DB_NAME = "poecraft";
// Bump this whenever the schema changes (new/removed object stores). It must
// never be lower than a version already created in a browser, or opening the
// DB fails with VersionError. Strategy records reuse the existing stash/drafts
// stores via a resourceType discriminator, so no new store is needed here.
const DB_VERSION = 2;
const LAYOUT_KEY = "poecraft.layout";

/** Exported item state plus the session identity needed to reopen it. */
export interface ItemSnapshot {
    resourceIdentity?: string;
    base: string;
    itemLevel: number;
    cluster?: import("../engine-protocol").ClusterConfiguration;
    /** Current engine rarity, used when reopening or rebuilding the item. */
    rarity?: string;
    state: unknown;
}

/** New records store the configuration explicitly; native exports also bind it. */
export function itemSnapshotCluster(snapshot: Pick<ItemSnapshot, "state" | "cluster">): import("../engine-protocol").ClusterConfiguration | undefined {
    if (snapshot.cluster) return snapshot.cluster;
    const c = (snapshot.state as {cluster?: {passive_key?: unknown; passive_count?: unknown}} | null)?.cluster;
    return c && typeof c.passive_key === "string" && typeof c.passive_count === "number"
        ? {passiveKey: c.passive_key, passiveCount: c.passive_count} : undefined;
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
    cluster?: import("../engine-protocol").ClusterConfiguration;
    rarity: string;
    /** Exported item state, or null for an untouched document. */
    state: unknown | null;
    history: { action: string; applied: boolean; added: number; removed: number }[];
    undoHistory?: HistoryData<EmulatorHistoryState>;
    /** The Emulator has revealed its stored offers and must finish that choice. */
    unveilRevealed?: boolean;
    savedStateKey?: string | null;
    /** Stash id this draft was last saved as, if any. */
    savedRef: string | null;
    /** Stash name this draft was last saved as, if any. */
    savedName: string | null;
    dirty: boolean;
    updatedAt: number;
}

export interface CraftHistoryEntry {
    action: string;
    applied: boolean;
    added: number;
    removed: number;
    detail?: string;
    /** Native consumption keys; absent when legacy/failed metadata leaves consumption unknown. */
    costKeys?: string[];
}

export interface EmulatorHistoryState {
    snapshot: ItemSnapshot;
    entry: CraftHistoryEntry;
    spend?: CraftSpend;
    /** Full recorded workspace resources affected by this history branch. */
    resources?: ItemStashRecord[];
}

export interface StrategyHistoryState {
    strategy: StrategyDocument;
    hasChosenBase: boolean;
}

export interface StrategyDraftRecord {
    docId: string;
    strategy: unknown | null;
    /** Present only until pc-strategy-editor converts an Emulator snapshot. */
    sourceItem: ItemSnapshot | null;
    savedRef: string | null;
    savedName: string | null;
    dirty: boolean;
    /** Strategy Builder runner surface; absent legacy drafts use Simulator. */
    builderMode?: "simulator" | "calculator";
    undoHistory?: HistoryData<StrategyHistoryState>;
    savedStateKey?: string | null;
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
    /** Versioned Calculator-only goals; scalar fields remain a legacy mirror. */
    goalList?: import("../calculator-goal-set").CalculatorGoalList;
    goalImplicitKeys?: string[];
    goalInfluenceBits?: number;
    goalCorrupted?: boolean;
    awakenerDonorId?: string;
    resourceIdentity?: string;
    docId: string;
    base: string;
    itemLevel: number;
    cluster?: import("../engine-protocol").ClusterConfiguration;
    /** Exported item state, or null before a base is chosen. */
    state: unknown | null;
    goalRarity: "normal" | "magic" | "rare";
    /** Absent legacy drafts retain clean final-item semantics. */
    allowExtraModifiers?: boolean;
    slots: CalculatorGoalSlot[];
    /** Minimum slots that define success; absent legacy drafts mean all. */
    minSatisfiedSlots?: number;
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

export async function putStash(record: StashRecord): Promise<unknown> {
    const db = await openDb();
    return new Promise<void>((resolve, reject) => {
        const transaction = db.transaction("stash", "readwrite");
        const store = transaction.objectStore("stash");
        let failure: Error | undefined;
        const read = store.get(record.id);
        read.onsuccess = () => {
            const old = read.result as StashRecord | undefined;
            if (old && old.resourceType !== "strategy" &&
                Number((old.state as {lifecycle?: number})?.lifecycle ?? 0) !== 0 &&
                JSON.stringify(old) !== JSON.stringify(record)) {
                failure = new Error("This resource was consumed or destroyed. Use the craft's Undo to restore it; a stale editor cannot overwrite it.");
                transaction.abort();
                return;
            }
            store.put(record);
        };
        transaction.oncomplete = () => resolve();
        transaction.onabort = () => reject(failure ?? transaction.error);
    });
}

/** Commit all craft resources and the receiver draft in one IndexedDB
 * transaction. Compare before values so another document cannot silently
 * overwrite a consumed donor or a later edit during Undo/Redo. */
export async function commitWorkspaceResources(
    before: ItemStashRecord[], after: ItemStashRecord[], draft: DraftRecord,
): Promise<void> {
    const beforeIds = new Set(before.map(item => item.id));
    const afterIds = new Set(after.map(item => item.id));
    if (beforeIds.size !== before.length || afterIds.size !== after.length ||
        [...beforeIds].some(id => !afterIds.has(id)))
        throw new Error("Resource transaction identities must be unique and preserve consumed records.");
    const db = await openDb();
    await new Promise<void>((resolve, reject) => {
        const transaction = db.transaction(["stash", "drafts"], "readwrite");
        const stash = transaction.objectStore("stash");
        let failure: Error | undefined;
        const created = after.filter(item => !beforeIds.has(item.id));
        let remaining = before.length + created.length;
        const write = () => {
            for (const resource of after) stash.put(resource);
            transaction.objectStore("drafts").put(draft);
        };
        transaction.oncomplete = () => resolve();
        transaction.onabort = () => reject(failure ?? transaction.error ?? new Error("Workspace transaction aborted"));
        transaction.onerror = () => { /* onabort owns rejection */ };
        if (!remaining) write();
        for (const item of created) {
            const request = stash.get(item.id);
            request.onsuccess = () => {
                if (request.result !== undefined) {
                    failure = new Error(`Resource identity ${item.id} already exists.`);
                    transaction.abort();
                } else if (--remaining === 0) write();
            };
        }
        for (const expected of before) {
            const request = stash.get(expected.id);
            request.onsuccess = () => {
                if (JSON.stringify(request.result) !== JSON.stringify(expected)) {
                    failure = new Error(`Resource ${expected.name} changed in another document. Reload it before continuing.`);
                    transaction.abort();
                    return;
                }
                if (--remaining === 0) write();
            };
        }
    });
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
