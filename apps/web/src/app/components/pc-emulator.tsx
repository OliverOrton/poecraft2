import { PcCraftControls, type CraftPanel } from "./pc-craft-controls";
import { disposeReact, renderReact } from "../react-host";
import { BaseSelectionShell, EmulatorShell } from "./document-shells";
import { EditHistory, historyShortcut } from "../edit-history";
import { resolveCraftValues } from "../craft-choices";
import { modTextLabel } from "../mod-text";
import { addCraftSpend, emptyCraftSpend, NativeCraftCosts } from "../craft-costs";
import type { PcCraftSpend } from "./pc-craft-spend";
import "./pc-craft-spend";
/*
 * pc-emulator — an emulator document. Owns one engine session, action context,
 * and live item, and drives the craft bar, the item view, and the modifier-pool
 * browser.
 *
 * Document lifecycle:
 *   - identified by a docId (the dockview panel id);
 *   - content auto-saved as an IndexedDB draft on every change (crash recovery);
 *   - dirty until saved to the Stash; reports dirty/title to the workspace;
 *   - can be saved, saved-as, or duplicated.
 */

import { getEngine } from "../engine-service";
import { readItemCard } from "../item-preview";
import { EngineClient } from "../engine-client";
import {
    BaseInfo,
    BestiaryActionInfo,
    Catalog,
    CraftAction,
    ModInfo,
} from "../engine-protocol";
import {
    DraftRecord,
    CraftHistoryEntry,
    EmulatorHistoryState,
    ItemStashRecord,
    ItemSnapshot,
    getDraft,
    getStash,
    listStash,
    putDraft,
} from "../workspace/persistence";
import { workspace } from "../workspace/registry";
import { openTextModal } from "../workspace/dirty-modal";
import { influenceLabels } from "../item-display";
import { PcBasePicker, BasePickerSelection } from "./pc-base-picker";
import { PcModList, SlotMod, type ConcreteModListModel } from "./pc-mod-list";
import { PcModPool } from "./pc-mod-pool";
import "./pc-base-picker";
import "./pc-mod-list";
import "./pc-mod-pool";





const DEFAULT_BASE = "Metadata/Items/Armours/BodyArmours/BodyInt17";

const REACH_KIND_CRAFTED = 2;

export class PcEmulator extends HTMLElement {
    private shellVersion = 0;
    private client!: EngineClient;
    private dataId = 0;
    private bases: BaseInfo[] = [];
    private catalog: Catalog | null = null;
    private bestiaryActions: BestiaryActionInfo[] = [];
    private checkpointPresent = false;
    private memoryStrands = 0;
    private donors: ItemStashRecord[] = [];
    private donorModel: ConcreteModListModel | undefined;

    private docId = "";
    private base = DEFAULT_BASE;
    private itemLevel = 86;
    private rarity = "normal";

    private session = 0;
    private context = 0;
    private item = 0;
    private history: CraftHistoryEntry[] = []; // Read-only logs from drafts predating snapshots.
    private undoHistory = new EditHistory<EmulatorHistoryState>();
    private pendingHistoryEntry: CraftHistoryEntry | null = null;
    private spend = emptyCraftSpend();
    private craftCosts = new NativeCraftCosts();
    private savedStateKey: string | null = null;
    private modCache: ModInfo[] = [];
    private veiledOptions: number[] = [];
    private activeCraftPanel: CraftPanel = "basic";
    private selectedFossils: string[] = [];
    private mechanicValues = new Map<string, string>();
    private busy = true;
    private pickerOpen = false;
    private hasBase = false;

    private dirty = false;
    private savedRef: string | null = null;
    private savedName: string | null = null;
    private savedCreatedAt = 0;
    private initializing = true;
    private disposed = false;
    private connectedOnce = false;
    private currentWork: Promise<void> | null = null;

    async connectedCallback(): Promise<void> {
        if (this.connectedOnce) {
            return;
        }
        this.connectedOnce = true;
        this.tabIndex = -1;
        this.addEventListener("keydown", event => {
            const command = historyShortcut(event);
            if (!command || this.pickerOpen || this.busy) return;
            event.preventDefault();
            void this.guard(() => this.restoreHistory(this.undoHistory.cursor + (command === "undo" ? -1 : 1)));
        });
        this.docId = this.getAttribute("doc-id") ?? `doc-${crypto.randomUUID()}`;
        this.renderShell();
        this.setBusy(true);
        workspace().registerDocument(this.docId, {
            save: () => this.save(),
            dispose: () => this.disposeEngine(),
        });
        this.setStatus("Loading engine…");
        const engine = await getEngine().catch(error => {
            this.setStatus(error instanceof Error ? error.message : String(error));
            this.setBusy(false);
            throw error;
        });
        if (this.disposed) {
            return;
        }
        this.client = engine.client;
        this.dataId = engine.dataId;
        this.catalog = await this.client.catalog(this.dataId);
        this.bases = (await this.client.listBases(this.dataId)).filter(
            (base) => base.support === 0,
        );
        this.bestiaryActions = (await this.client.bestiaryPresentation(
            this.dataId,
        )).actions.filter((action) => action.emulator_available);
        if (this.disposed) {
            return;
        }

        const draft = await getDraft(this.docId);
        if (draft) {
            this.base = draft.base;
            this.itemLevel = draft.itemLevel;
            this.rarity = draft.rarity;
            this.history = draft.history;
            this.savedRef = draft.savedRef;
            this.savedName = draft.savedName;
            this.dirty = draft.dirty;
            this.savedStateKey = draft.savedStateKey ?? null;
        }
        if (!this.bases.some((b) => b.path === this.base)) {
            this.base = this.bases[0]?.path ?? this.base;
        }
        this.hasBase = Boolean(draft?.state);

        if (!this.hasBase) {
            this.pickerOpen = true;
            this.renderShell();
            this.setBusy(false);
            this.setStatus("");
            workspace().notifyDirty(this.docId, this.dirty, this.docTitle);
            return;
        }

        await this.openSession();
        if (this.disposed) {
            return;
        }
        const item = draft?.state
            ? await this.client.importItem(draft.state, this.session)
            : await this.client.createItem(this.session, {
                rarity: this.rarity,
                withImplicits: true,
            });
        if (this.disposed) {
            await this.client.closeItem(item);
            return;
        }
        this.item = item;
        const snapshot = await this.snapshot();
        const entry = draft?.undoHistory?.entries[draft.undoHistory.cursor]?.entry ?? {
            action: "Opened item", applied: true, added: 0, removed: 0, detail: "starting state",
        };
        const recoveredHistory = draft?.undoHistory && {
            ...draft.undoHistory,
            entries: draft.undoHistory.entries.map(frame => ({...frame,
                // Even the oldest retained frame may follow trimmed paid crafts.
                spend: frame.spend ?? {counts: {}, untracked: true},
            })),
        };
        this.spend = recoveredHistory?.entries[recoveredHistory.cursor]?.spend ?? {counts: {}, untracked: this.history.length > 0};
        const resources = recoveredHistory?.entries[recoveredHistory.cursor]?.resources;
        this.undoHistory.restore(recoveredHistory, {snapshot, entry, spend: this.spend, ...(resources ? {resources} : {})});
        if (!this.dirty) this.savedStateKey = JSON.stringify(snapshot);
        this.initializing = false;
        try {
            await this.refresh();
            if (this.disposed) {
                return;
            }
            await this.persist();
        } catch (error) {
            if (this.disposed) {
                return;
            }
            throw error;
        }

        workspace().notifyDirty(this.docId, this.dirty, this.docTitle);
        this.setBusy(false);
        this.setStatus("");
    }

    disconnectedCallback(): void {
        // Dockview also fires this during drag-between-groups; persistent state
        // is in the draft already, so nothing to tear down here.
    }

    private get docTitle(): string {
        return this.savedName ?? "Untitled";
    }

    // --- engine lifecycle ---------------------------------------------------

    private async openSession(): Promise<void> {
        if (this.disposed) {
            return;
        }
        if (this.session) {
            await this.client.closeContext(this.context);
            await this.client.closeSession(this.session);
            this.context = 0;
            this.session = 0;
        }
        this.modCache = [];
        const session = await this.client.createSession(
            this.dataId,
            this.base,
            this.itemLevel,
        );
        if (this.disposed) {
            await this.client.closeSession(session);
            return;
        }
        const context = await this.client.createContext(session, 0);
        if (this.disposed) {
            await this.client.closeContext(context);
            await this.client.closeSession(session);
            return;
        }
        this.session = session;
        this.context = context;
        await this.cacheAllMods();
    }

    private async cacheAllMods(): Promise<void> {
        const count = await this.client.modCount(this.session);
        const cache: ModInfo[] = new Array(count);
        // Resolve in parallel; the worker is single-threaded so the requests
        // are serialised there, but this avoids a chain of awaits on the main
        // thread and lets the worker dispatch them back-to-back.
        await Promise.all(
            Array.from({ length: count }, async (_, id) => {
                cache[id] = await this.client.modInfo(this.session, id);
            }),
        );
        if (this.disposed) {
            return;
        }
        this.modCache = cache;
    }

    private async rebuildSession(): Promise<void> {
        await this.openSession();
        if (this.item) {
            await this.client.closeItem(this.item);
        }
        this.rarity = "normal";
        this.item = await this.client.createItem(this.session, {
            rarity: this.rarity,
            withImplicits: true,
        });
        this.pendingHistoryEntry = {action: "Create item", applied: true, added: 0, removed: 0, detail: this.baseDisplayName(), costKeys: []};
        await this.markChanged();
    }

    private async createItem(): Promise<void> {
        if (this.item) {
            await this.client.closeItem(this.item);
        }
        this.rarity = "normal";
        this.item = await this.client.createItem(this.session, {
            rarity: this.rarity,
            withImplicits: true,
        });
        this.pendingHistoryEntry = {action: "Create item", applied: true, added: 0, removed: 0, detail: this.baseDisplayName(), costKeys: []};
        await this.markChanged();
    }

    private async applyAction(type: CraftAction["type"]): Promise<void> {
        await this.applyConfiguredAction({type});
    }

    private async loadDonors(): Promise<void> {
        this.donors = (await listStash()).filter((record): record is ItemStashRecord =>
            record.resourceType !== "strategy" && record.id !== this.savedRef &&
            Number((record.state as {lifecycle?: number})?.lifecycle ?? 0) === 0);
        if (!this.donors.some(donor => donor.id === this.mechanicValues.get("awakener-donor")))
            this.mechanicValues.set("awakener-donor", this.donors[0]?.id ?? "");
        this.donorModel = undefined;
        const selected = this.donors.find(donor => donor.id === this.mechanicValues.get("awakener-donor"));
        if (selected) this.donorModel = await readItemCard(this.client, this.dataId, this.catalog, selected, selected.name);
        this.renderMechanicControls();
    }

    private async applyAwakener(): Promise<void> {
        const donor = this.donors.find(record => record.id === this.mechanicValues.get("awakener-donor"));
        if (!donor) throw new Error("Choose an available donor from Stash.");
        const donorSession = await this.client.createSession(this.dataId, donor.base, donor.itemLevel);
        let donorItem = 0, receiverItem = 0;
        try {
            donorItem = await this.client.importItem(donor.state, donorSession);
            receiverItem = await this.client.importItem(await this.client.exportItem(this.item, this.session), this.session);
            const result = await this.client.multiItemApply(this.context, {action: "awakener", resources: [
                {identity: donor.id, role: "donor", session: donorSession, item: donorItem},
                {identity: this.savedRef ?? this.docId, role: "receiver", session: this.session, item: receiverItem},
            ]});
            const snapshot = {base: this.base, itemLevel: this.itemLevel, rarity: "rare", state: await this.client.exportItem(receiverItem, this.session)};
            const history = this.undoHistory.export();
            const before = [...(history.entries[history.cursor]?.resources ?? [])];
            const receiverRecord = this.savedRef ? await getStash(this.savedRef) : undefined;
            const involved = [donor, ...(receiverRecord && receiverRecord.resourceType !== "strategy" ? [receiverRecord] : [])];
            for (const resource of involved) {
                if (!before.some(record => record.id === resource.id)) before.push(resource);
                for (const frame of history.entries) {
                    frame.resources ??= [];
                    if (!frame.resources.some(record => record.id === resource.id)) frame.resources.push(resource);
                }
            }
            const consumed = {...donor, state: await this.client.exportItem(donorItem, donorSession)};
            const after = before.map(resource => resource.id === donor.id ? consumed :
                resource.id === this.savedRef ? {...resource, ...snapshot} : resource);
            const spend = addCraftSpend(this.spend, result.cost_keys);
            const nextHistory = new EditHistory<EmulatorHistoryState>();
            nextHistory.restore(history, history.entries[history.cursor]);
            nextHistory.record({snapshot, resources: after, spend, entry: {action: "Awakener's Orb", applied: true, added: 0, removed: 0,
                costKeys: result.cost_keys, detail: `Consumed ${donor.name}`}});
            await workspace().commitResources(before, after, this.draftFor(snapshot, nextHistory.export()));
            const previous = this.item;
            this.item = receiverItem; receiverItem = 0;
            this.undoHistory = nextHistory; this.spend = spend;
            this.dirty = true;
            await this.client.closeItem(previous);
            await this.refresh();
            await this.loadDonors();
            workspace().notifyDirty(this.docId, true, this.docTitle);
        } finally {
            if (donorItem) await this.client.closeItem(donorItem);
            if (receiverItem) await this.client.closeItem(receiverItem);
            await this.client.closeSession(donorSession);
        }
    }

    private async setMemoryStrands(count: number): Promise<void> {
        const state = await this.client.exportItem(this.item, this.session) as Record<string, unknown>;
        const replacement = await this.client.importItem({...state, memory_strands: count}, this.session);
        const old = this.item;
        this.item = replacement;
        await this.client.closeItem(old);
        this.pendingHistoryEntry = {action: "Set memory strands", applied: true, added: 0, removed: 0, costKeys: [], detail: String(count)};
        await this.markChanged();
    }

    private async applyConfiguredAction(action: CraftAction): Promise<void> {
        // Pricing metadata must never prevent an otherwise supported craft.
        const keys = await this.craftCosts.forAction(this.client, this.session, action).catch(() => undefined);
        const outcome = await this.client.apply(this.context, this.item, action);
        this.pendingHistoryEntry = {
            action: action.type,
            applied: outcome.applied,
            added: outcome.added,
            removed: outcome.removed,
            costKeys: outcome.applied ? outcome.cost_keys ?? keys : [],
        };
        await this.markChanged();
    }

    private async applyBestiaryAction(
        action: BestiaryActionInfo,
    ): Promise<void> {
        const outcome = await this.client.bestiaryApply(
            this.dataId,
            this.item,
            action.id,
        );
        this.pendingHistoryEntry = {
            action: action.display_name,
            applied: outcome.applied,
            added: 0,
            removed: 0,
            costKeys: outcome.consumed_price_keys,
            detail: outcome.applied
                ? action.checkpoint_effect === "create"
                    ? "checkpoint saved"
                    : "checkpoint consumed"
                : outcome.refusal_reason,
        };
        this.checkpointPresent = outcome.checkpoint_present;
        await this.markChanged();
    }

    private async craftMod(
        key: string,
        side: "prefix" | "suffix",
        fractured = false,
    ): Promise<void> {
        const info = this.modCache.find((mod) => mod.key === key);
        if (info?.reach_kind === REACH_KIND_CRAFTED && !fractured) {
            await this.applyConfiguredAction({type: "bench", mod_key: key});
            return;
        }
        await this.client.addMod(this.item, this.session, {key, side, fractured});
        this.pendingHistoryEntry = {
            action: `${fractured ? "fracture" : "add"} ${side} ${key}`,
            applied: true,
            added: 1,
            removed: 0,
            costKeys: [],
        };
        await this.markChanged();
    }

    private async fractureMod(
        key: string,
        modId: number,
        side: "prefix" | "suffix",
        onItem: boolean,
    ): Promise<void> {
        if (onItem) {
            await this.client.setModFractured(this.item, { modId, side });
            this.pendingHistoryEntry = {
                action: `fracture ${side} ${key}`,
                applied: true,
                added: 0,
                removed: 0,
                costKeys: [],
            };
            await this.markChanged();
            return;
        }
        await this.craftMod(key, side, true);
    }

    private async removeMod(
        modId: number,
        side: "prefix" | "suffix",
    ): Promise<void> {
        const info = this.modCache[modId];
        await this.client.removeMod(this.item, { modId, side });
        this.pendingHistoryEntry = {
            action: `remove ${side} ${info?.key ?? modId}`,
            applied: true,
            added: 0,
            removed: 1,
            costKeys: [],
        };
        await this.markChanged();
    }

    private async markChanged(): Promise<void> {
        const snapshot = await this.snapshot();
        if (this.pendingHistoryEntry) {
            this.spend = addCraftSpend(this.spend, this.pendingHistoryEntry.costKeys);
            const resources = this.undoHistory.at(this.undoHistory.cursor)?.resources;
            this.undoHistory.record({snapshot, entry: this.pendingHistoryEntry, spend: this.spend, ...(resources ? {resources} : {})});
            this.pendingHistoryEntry = null;
        }
        this.dirty = this.savedStateKey !== JSON.stringify(snapshot);
        await this.refresh();
        await this.persist();
        workspace().notifyDirty(this.docId, this.dirty, this.docTitle);
    }

    private async restoreHistory(index: number): Promise<void> {
        const frame = this.undoHistory.at(index);
        if (!frame || index === this.undoHistory.cursor) return;
        const snapshot = frame.snapshot;
        // Prepare the replacement before releasing the live item. Import also
        // restores native Imprint checkpoints and any pending unveil choices.
        let item = 0;
        let session = 0;
        let context = 0;
        let mods = this.modCache;
        const differentBase = snapshot.base !== this.base || snapshot.itemLevel !== this.itemLevel;
        try {
            if (differentBase) {
                session = await this.client.createSession(this.dataId, snapshot.base, snapshot.itemLevel);
                context = await this.client.createContext(session, 0);
                const count = await this.client.modCount(session);
                mods = await Promise.all(Array.from({length: count}, (_, id) => this.client.modInfo(session, id)));
            }
            item = await this.client.importItem(snapshot.state, session || this.session);
            await this.client.itemInfo(item, session || this.session);
            if (this.disposed) return;
            const oldResources = this.undoHistory.at(this.undoHistory.cursor)?.resources ?? [];
            if (oldResources.length || frame.resources?.length) {
                const history = this.undoHistory.export(); history.cursor = index;
                await workspace().commitResources(oldResources, frame.resources ?? [], this.draftFor(snapshot, history));
            }
            const previous = {item: this.item, session: this.session, context: this.context};
            this.item = item;
            item = 0;
            if (differentBase) {
                this.session = session;
                this.context = context;
                session = context = 0;
                this.modCache = mods;
            }
            this.base = snapshot.base;
            this.itemLevel = snapshot.itemLevel;
            this.undoHistory.go(index);
            this.spend = frame.spend ?? {counts: {}, untracked: index > 0};
            this.dirty = this.savedStateKey !== JSON.stringify(snapshot);
            await this.client.closeItem(previous.item);
            if (differentBase) {
                await this.client.closeContext(previous.context);
                await this.client.closeSession(previous.session);
            }
            this.syncControls();
            await this.refresh();
            await this.persist();
            workspace().notifyDirty(this.docId, this.dirty, this.docTitle);
        } finally {
            if (item) await this.client.closeItem(item);
            if (context) await this.client.closeContext(context);
            if (session) await this.client.closeSession(session);
        }
    }

    private async snapshot(): Promise<ItemSnapshot> {
        const info = await this.client.itemInfo(this.item, this.session);
        this.veiledOptions =
            (info.veiled_option_mod_ids as number[] | undefined) ?? [];
        return {
            base: this.base,
            itemLevel: this.itemLevel,
            rarity: info.rarity as string,
            state: await this.client.exportItem(this.item, this.session),
        };
    }

    private draftFor(snapshot: ItemSnapshot, history = this.undoHistory.export()): DraftRecord {
        return {
            docId: this.docId,
            base: snapshot.base,
            itemLevel: snapshot.itemLevel,
            rarity: snapshot.rarity ?? this.rarity,
            state: snapshot.state,
            history: this.history,
            undoHistory: history,
            savedStateKey: this.savedStateKey,
            savedRef: this.savedRef,
            savedName: this.savedName,
            dirty: this.savedStateKey !== JSON.stringify(snapshot),
            updatedAt: Date.now(),
        };
    }

    private async persist(): Promise<void> {
        await putDraft(this.draftFor(this.item ? await this.snapshot() : {base: this.base, itemLevel: this.itemLevel, rarity: this.rarity, state: null}));
    }

    // --- save / save-as / duplicate ----------------------------------------

    private async save(): Promise<boolean> {
        if (!this.savedRef) {
            return this.saveAs();
        }
        const snapshot = await this.snapshot();
        const record: ItemStashRecord = {
            id: this.savedRef,
            name: this.savedName ?? "Untitled",
            ...snapshot,
            createdAt: this.savedCreatedAt || Date.now(),
        };
        await workspace().saveToStash(record);
        await this.markSaved(record);
        return true;
    }

    private async saveAs(): Promise<boolean> {
        const name = await openTextModal("Save item to Stash as:", this.savedName ?? "New item");
        if (!name) {
            return false;
        }
        const record: ItemStashRecord = {
            id: `stash-${crypto.randomUUID()}`,
            name,
            ...(await this.snapshot()),
            createdAt: Date.now(),
        };
        await workspace().saveToStash(record);
        await this.markSaved(record);
        return true;
    }

    private async markSaved(record: ItemStashRecord): Promise<void> {
        // Saving this receiver establishes a new CAS baseline for the current
        // frame. Keep its other resource receipts and historical item states.
        if (record.id === this.savedRef) {
            const history = this.undoHistory.export();
            for (const [index, frame] of history.entries.entries()) {
                frame.resources = frame.resources?.map(resource => resource.id !== record.id ? resource :
                    index === history.cursor ? record : {...resource, name: record.name, createdAt: record.createdAt});
            }
            if (history.entries.length) this.undoHistory.restore(history, history.entries[history.cursor]);
        }
        this.savedRef = record.id;
        this.savedName = record.name;
        this.savedCreatedAt = record.createdAt;
        this.savedStateKey = JSON.stringify({base: record.base, itemLevel: record.itemLevel, rarity: record.rarity, state: record.state});
        this.dirty = this.savedStateKey !== JSON.stringify(await this.snapshot());
        await this.persist();
        workspace().notifyDirty(this.docId, this.dirty, this.docTitle);
        this.renderSavedName();
    }

    private async duplicate(): Promise<void> {
        await workspace().openEmulator(await this.snapshot(), "copy");
    }

    private async useInStrategy(): Promise<void> {
        await workspace().openStrategy(await this.snapshot(), "copy");
    }

    private async openInCalculator(): Promise<void> {
        await workspace().openCalculator({...await this.snapshot(), resourceIdentity: this.savedRef ?? this.docId});
    }

    private async disposeEngine(): Promise<void> {
        if (this.disposed) {
            return;
        }
        this.disposed = true;
        workspace().unregisterDocument(this.docId);
        if (this.currentWork) {
            try {
                await this.currentWork;
            } catch {
                // The operation's guard already surfaced the error. Continue
                // releasing every native handle.
            }
        }
        const item = this.item;
        const context = this.context;
        const session = this.session;
        this.item = 0;
        this.context = 0;
        this.session = 0;
        if (item && this.client) {
            await this.client.closeItem(item);
        }
        if (context && this.client) {
            await this.client.closeContext(context);
        }
        if (session && this.client) {
            await this.client.closeSession(session);
        }
        disposeReact(this);
    }

    // --- refresh / render ---------------------------------------------------

    private async refresh(): Promise<void> {
        const info = await this.client.itemInfo(this.item, this.session);
        this.checkpointPresent = Boolean(info.checkpoint_present);
        this.memoryStrands = Number(info.memory_strands ?? 0);
        this.rarity = info.rarity as string;
        this.veiledOptions =
            (info.veiled_option_mod_ids as number[] | undefined) ?? [];
        const fracturedP = new Set(info.fractured_prefix_mod_ids as number[]);
        const fracturedS = new Set(info.fractured_suffix_mod_ids as number[]);
        const prefixIds = info.prefix_mod_ids as number[];
        const suffixIds = info.suffix_mod_ids as number[];
        const implicitIds = info.implicit_mod_ids as number[];

        const prefixes = prefixIds.map((id) => this.toSlot(id, fracturedP));
        const suffixes = suffixIds.map((id) => this.toSlot(id, fracturedS));
        const implicits = implicitIds.map((id) => this.toSlot(id, new Set()));

        this.modList.setModel({
            kind: "concrete",
            baseKey: this.base,
            baseName: this.baseDisplayName(),
            itemLevel: this.itemLevel,
            rarity: info.rarity as string,
            itemFlags: Number(info.item_flags ?? 0),
            memoryStrands: Number(info.memory_strands ?? 0),
            lifecycle: Number(info.lifecycle ?? 0),
            influences: influenceLabels(
                Number(info.generic_influence_bits ?? 0),
                Number(info.searing_exarch_tier ?? 0),
                Number(info.eater_of_worlds_tier ?? 0),
                this.catalog,
            ),
            prefixes,
            suffixes,
            implicits,
            enchantments: ((info.enchantment_mod_ids as number[]) ?? []).map(id => this.toSlot(id, new Set())),
            maxPrefix: (info.max_prefix as number) ?? prefixes.length,
            maxSuffix: (info.max_suffix as number) ?? suffixes.length,
        });

        const tab = this.modPool.getActiveTab();
        const poolAction: CraftAction["type"] =
            tab === "implicit" ? "chaos" : tab === "prefix" ? "chaos" : "chaos";
        const pool =
            (tab === "implicit" || tab === "enchantment") || Number(info.memory_strands ?? 0) > 0 || Number(info.lifecycle ?? 0) !== 0 || ((info.enchantment_mod_ids as number[]) ?? []).length > 0
                ? null
                : await this.client.debugPool(this.context, this.item, {
                      action: { type: poolAction },
                  });
        const poolWeights = new Map<number, number>();
        if (pool) {
            for (const entry of pool.entries) {
                if (!entry.accepted) continue;
                poolWeights.set(entry.session_mod_id, entry.final_weight);
            }
        }
        const prefixOnItem = new Set(prefixIds);
        const suffixOnItem = new Set(suffixIds);
        const implicitOnItem = new Set(implicitIds);
        const groupOnItem = new Set<number>();
        for (const id of [...prefixIds, ...suffixIds]) {
            const cached = this.modCache[id];
            if (cached) groupOnItem.add(cached.primary_group_id);
        }

        this.modPool.setModel({
            mods: this.modCache,
            item: {
                rarity: info.rarity as string,
                prefixOnItem,
                suffixOnItem,
                implicitOnItem,
                enchantmentOnItem: new Set((info.enchantment_mod_ids as number[]) ?? []),
                fracturedPrefixOnItem: fracturedP,
                fracturedSuffixOnItem: fracturedS,
                groupOnItem,
                maxPrefix: (info.max_prefix as number) ?? prefixes.length,
                maxSuffix: (info.max_suffix as number) ?? suffixes.length,
            },
            pool,
            poolWeights,
        });
        this.renderHistory();
        this.syncControls();
        if (this.catalog) this.querySelector<PcCraftSpend>("pc-craft-spend")?.setModel({spend: this.spend, catalog: this.catalog});
        this.renderMechanicControls();
    }

    private toSlot(id: number, fractured: Set<number>): SlotMod {
        const info = this.modCache[id];
        if (!info) {
            return {
                sessionModId: id,
                key: String(id),
                tierIndex: 0,
                textLines: [],
                classificationTags: [],
                fractured: fractured.has(id),
                crafted: false,
            };
        }
        return {
            sessionModId: id,
            key: info.key,
            tierIndex: info.family_tier_index,
            textLines: info.text_lines,
            classificationTags: info.classification_tags,
            fractured: fractured.has(id),
            crafted: info.reach_kind === REACH_KIND_CRAFTED,
        };
    }

    private get modList(): PcModList {
        return this.querySelector("pc-mod-list")!;
    }

    private get modPool(): PcModPool {
        return this.querySelector("pc-mod-pool")!;
    }

    private baseDisplayName(): string {
        return (
            this.bases.find((base) => base.path === this.base)?.name ??
            baseLabel(this.base)
        );
    }

    private setStatus(text: string): void {
        const el = this.querySelector(".pc-emu-status");
        if (el) {
            el.textContent = text;
            (el as HTMLElement).hidden = text === "";
        }
    }

    private setBusy(busy: boolean): void {
        this.busy = busy;
        this.querySelectorAll<HTMLElement>(".pc-advanced-crafts, pc-mod-pool, pc-mod-list").forEach(element => {
            element.inert = busy;
        });
        this.querySelectorAll<HTMLButtonElement>(
            "button[data-cmd], button[data-craft-panel], button[data-simple-action], button[data-config-action], button[data-bestiary-action], button[data-fossil-add], button[data-fossil-remove]",
        ).forEach((button) => {
            if (busy) {
                button.dataset.disabledBeforeBusy ??= String(button.disabled);
                button.disabled = true;
            } else if (button.dataset.disabledBeforeBusy !== undefined) {
                button.disabled = button.dataset.disabledBeforeBusy === "true";
                delete button.dataset.disabledBeforeBusy;
            }
        });
        this.syncHistoryButtons();
    }

    private syncHistoryButtons(): void {
        for (const command of ["undo", "redo"] as const) {
            const button = this.querySelector<HTMLButtonElement>(`[data-cmd="${command}"]`);
            if (button) button.disabled = this.busy || !(command === "undo" ? this.undoHistory.canUndo : this.undoHistory.canRedo);
        }
        this.querySelectorAll<HTMLButtonElement>("[data-history-index]").forEach(button => {
            button.disabled = this.busy || Number(button.dataset.historyIndex) === this.undoHistory.cursor;
        });
    }

    private async guard(work: () => Promise<void>): Promise<void> {
        if (this.busy || this.disposed) {
            return;
        }
        const restoreFocus = this.contains(document.activeElement);
        this.setBusy(true);
        const pending = work();
        this.currentWork = pending;
        try {
            await pending;
        } catch (error) {
            this.setStatus(error instanceof Error ? error.message : String(error));
        } finally {
            if (this.currentWork === pending) {
                this.currentWork = null;
            }
            this.setBusy(false);
            if (restoreFocus && this.isConnected && document.activeElement === document.body) {
                this.focus({preventScroll: true});
            }
        }
    }

    private syncControls(): void {
        this.renderSavedName();
        const base = this.querySelector(".pc-emu-base");
        if (base) base.textContent = `${this.baseDisplayName()} · iLvl ${this.itemLevel}`;
    }

    private renderSavedName(): void {
        const el = this.querySelector(".pc-emu-name");
        if (el) {
            el.textContent = this.savedName ? `Saved: ${this.savedName}` : "Unsaved";
        }
    }

    private renderHistory(): void {
        const el = this.querySelector(".pc-emu-history");
        if (!el) return;
        const entries = this.undoHistory.export().entries;
        el.innerHTML = entries.map(({entry}, index) => {
            const detail = entry.detail ?? (entry.applied ? `+${entry.added} / -${entry.removed}` : "no-op");
            const current = index === this.undoHistory.cursor;
            return `<li class="${entry.applied ? "" : "pc-history-noop"} ${current ? "is-current" : index > this.undoHistory.cursor ? "is-future" : ""}">
                <button type="button" data-history-index="${index}" ${current ? 'aria-current="step"' : ""} title="Restore item after this step">
                    <span class="pc-history-n">${index}</span><span class="pc-history-action">${escapeHtml(entry.action)}</span>
                    <span class="pc-history-detail">${current ? "Current · " : ""}${escapeHtml(detail)}</span>
                </button></li>`;
        }).reverse().join("") + this.history.slice().reverse().map(entry =>
            `<li class="pc-history-legacy"><span class="pc-history-action">${escapeHtml(entry.action)}</span><span class="pc-history-detail">Earlier log · no snapshot</span></li>`
        ).join("");
        el.querySelectorAll<HTMLButtonElement>("[data-history-index]").forEach(button => {
            button.addEventListener("click", () => {
                void this.guard(() => this.restoreHistory(Number(button.dataset.historyIndex)));
            });
        });
        this.syncHistoryButtons();
    }

    private renderMechanicControls(): void {
        const host = this.querySelector<PcCraftControls>(".pc-advanced-crafts");
        if (!host || !this.catalog) return;
        this.mechanicValues = resolveCraftValues(this.catalog, this.mechanicValues);
        const unveils = this.veiledOptions.map(id => this.modCache[id]).filter((mod): mod is ModInfo => Boolean(mod))
            .map(mod => ({key: mod.key, name: modTextLabel(mod.text_lines) || mod.key}));
        if (!unveils.some(mod => mod.key === this.mechanicValues.get("unveil"))) this.mechanicValues.set("unveil", unveils[0]?.key ?? "");
        host.setModel({
            mode: "emulator", catalog: this.catalog, panel: this.activeCraftPanel,
            itemClass: this.bases.find(base => base.path === this.base)?.item_class_key,
            values: this.mechanicValues, fossils: this.selectedFossils, bestiary: this.bestiaryActions,
            checkpoint: this.checkpointPresent,
            memoryStrands: this.memoryStrands,
            onMemoryStrands: count => { void this.guard(() => this.setMemoryStrands(count)); },
            donors: this.donors.map(record => ({key: record.id, name: record.name})), donorModel: this.donorModel,
            onAwakener: () => { void this.guard(() => this.applyAwakener()); },
            unveils,
            onPanel: panel => { this.activeCraftPanel = panel; this.renderMechanicControls(); if (panel === "awakener") void this.guard(() => this.loadDonors()); },
            onValue: (name, value) => {
                this.mechanicValues.set(name, value);
                if (name === "awakener-donor") { void this.guard(() => this.loadDonors()); return; }
                if (name === "essence-type") this.mechanicValues.delete("essence-key");
                this.renderMechanicControls();
            },
            onSimple: id => { void this.guard(() => this.applyAction(id as CraftAction["type"])); },
            onBestiary: id => {
                const action = this.bestiaryActions.find(entry => entry.id === id);
                if (action) void this.guard(() => this.applyBestiaryAction(action));
            },
            onConfigured: id => {
                const type = id as CraftAction["type"];
                const value = (name: string) => this.mechanicValues.get(name) ?? "";
                let action: CraftAction = {type};
                if (type === "essence") action = {type, essence: value("essence-key")};
                else if (type === "fossil") action = {type, fossils: [...this.selectedFossils]};
                else if (type === "harvest_reforge" || type === "harvest_augment") action = {type, target_tag: value(type === "harvest_reforge" ? "harvest-reforge-tag" : "harvest-augment-tag")};
                else if (type === "harvest_resist") action = {type, source_tag: value("resist-from"), target_tag: value("resist-to")};
                else if (type === "eldritch_ember" || type === "eldritch_ichor") action = {type, tier: Number(value("eldritch-tier"))};
                else if (type === "influence_exalt") action = {type, influence: value("influence")};
                else if (type === "unveil") action = {type, mod_key: value("unveil")};
                void this.guard(() => this.applyConfiguredAction(action));
            },
            onAddFossil: key => {
                if (key && this.selectedFossils.length < 4 && !this.selectedFossils.includes(key)) {
                    this.selectedFossils = [...this.selectedFossils, key]; this.renderMechanicControls();
                }
            },
            onRemoveFossil: index => {
                this.selectedFossils = this.selectedFossils.filter((_, entryIndex) => entryIndex !== index);
                this.renderMechanicControls();
            },
        });
        this.setBusy(this.busy);
    }


    private renderShell(): void {
        if (this.pickerOpen) {
            renderReact(this, <BaseSelectionShell key={++this.shellVersion} kind="emulator" />);
            const picker = this.querySelector<PcBasePicker>("pc-base-picker")!;
            picker.setBases(this.bases);
            picker.setSelection(this.base, this.itemLevel);
            picker.addEventListener("confirm", (event) => {
                const detail = (event as CustomEvent<BasePickerSelection>).detail;
                void this.guard(() => this.applyPickerSelection(detail));
            });
            picker.addEventListener("cancel", () => {
                if (this.hasBase) {
                    this.pickerOpen = false;
                    this.renderShell();
                    void this.guard(() => this.refresh());
                }
            });
            return;
        }
        renderReact(this, <EmulatorShell key={++this.shellVersion} baseName={this.baseDisplayName()} itemLevel={this.itemLevel} />);
        this.syncControls();
        this.renderMechanicControls();
        this.setBusy(this.busy);

        this.querySelectorAll<HTMLButtonElement>("button[data-cmd]").forEach((button) => {
            button.addEventListener("click", () => {
                const cmd = button.dataset.cmd;
                if (cmd === "change-base") {
                    this.pickerOpen = true;
                    this.renderShell();
                    return;
                }
                void this.guard(async () => {
                    if (cmd === "undo" || cmd === "redo") await this.restoreHistory(this.undoHistory.cursor + (cmd === "undo" ? -1 : 1));
                    else if (cmd === "create") await this.createItem();
                    else if (cmd === "save") await this.save();
                    else if (cmd === "save-as") await this.saveAs();
                    else if (cmd === "duplicate") await this.duplicate();
                    else if (cmd === "strategy") await this.useInStrategy();
                    else if (cmd === "calculator") await this.openInCalculator();
                });
            });
        });
        this.modPool.addEventListener("craft-mod", (event) => {
            const detail = (
                event as CustomEvent<{
                    key: string;
                    side: "prefix" | "suffix";
                    fractured?: boolean;
                }>
            ).detail;
            void this.guard(() =>
                this.craftMod(
                    detail.key,
                    detail.side,
                    Boolean(detail.fractured),
                ),
            );
        });
        this.modPool.addEventListener("fracture-mod", (event) => {
            const detail = (
                event as CustomEvent<{
                    key: string;
                    modId: number;
                    side: "prefix" | "suffix";
                    onItem: boolean;
                }>
            ).detail;
            void this.guard(() =>
                this.fractureMod(
                    detail.key,
                    detail.modId,
                    detail.side,
                    detail.onItem,
                ),
            );
        });
        this.modList.addEventListener("fracture-mod", (event) => {
            const detail = (
                event as CustomEvent<{
                    key: string;
                    modId: number;
                    side: "prefix" | "suffix";
                }>
            ).detail;
            void this.guard(() =>
                this.fractureMod(
                    detail.key,
                    detail.modId,
                    detail.side,
                    true,
                ),
            );
        });
        this.modPool.addEventListener("remove-mod", (event) => {
            const detail = (
                event as CustomEvent<{
                    modId: number;
                    side: "prefix" | "suffix";
                }>
            ).detail;
            void this.guard(() => this.removeMod(detail.modId, detail.side));
        });
        this.modPool.addEventListener("tab-change", () => {
            void this.guard(() => this.refresh());
        });
    }

    private async applyPickerSelection(sel: BasePickerSelection): Promise<void> {
        this.base = sel.base;
        this.itemLevel = sel.itemLevel;
        const firstTime = !this.hasBase;
        this.hasBase = true;
        this.pickerOpen = false;
        this.renderShell();
        await this.rebuildSession();
        if (firstTime) {
            this.initializing = false;
        }
        this.afterPickerClose();
    }

    private afterPickerClose(): void {
        // Re-attach mod-pool listeners are already set up in renderShell().
    }
}

function baseLabel(path: string): string {
    return path.split("/").pop() ?? path;
}

function escapeHtml(text: string): string {
    return text
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;");
}

customElements.define("pc-emulator", PcEmulator);
