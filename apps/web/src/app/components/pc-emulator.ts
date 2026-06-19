/*
 * pc-emulator — an emulator document. Owns one engine session, action context,
 * and live item, and drives the craft bar, mod list, and debug weight table.
 *
 * Document lifecycle:
 *   - identified by a docId (the dockview panel id);
 *   - content auto-saved as an IndexedDB draft on every change (crash recovery);
 *   - dirty until saved to the Stash; reports dirty/title to the workspace;
 *   - can be saved, saved-as, or duplicated.
 *
 * Real WASM-backed slice: every control maps to an EngineClient call.
 */

import { getEngine } from "../engine-service";
import { EngineClient } from "../engine-client";
import { BaseInfo, CraftAction, ModInfo } from "../engine-protocol";
import {
    DraftRecord,
    ItemSnapshot,
    StashRecord,
    getDraft,
    putDraft,
} from "../workspace/persistence";
import { workspace } from "../workspace/registry";
import { PcCombobox } from "./pc-combobox";
import { PcModList, ModRow } from "./pc-mod-list";
import { PcWeightTable } from "./pc-weight-table";
import "./pc-combobox";
import "./pc-mod-list";
import "./pc-weight-table";

const ACTIONS: CraftAction["type"][] = [
    "transmute",
    "augment",
    "alteration",
    "regal",
    "alchemy",
    "chaos",
    "exalt",
    "annul",
    "scour",
];

const DEFAULT_BASE = "Metadata/Items/Armours/BodyArmours/BodyInt17";

interface HistoryEntry {
    action: string;
    applied: boolean;
    added: number;
    removed: number;
}

function baseLabel(path: string): string {
    return path.split("/").pop() ?? path;
}

export class PcEmulator extends HTMLElement {
    private client!: EngineClient;
    private dataId = 0;
    private bases: BaseInfo[] = [];

    private docId = "";
    private base = DEFAULT_BASE;
    private itemLevel = 86;
    private rarity = "rare";
    private poolAction: CraftAction["type"] = "chaos";

    private session = 0;
    private context = 0;
    private item = 0;
    private history: HistoryEntry[] = [];
    private modCache = new Map<number, ModInfo>();
    private busy = false;

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
        this.docId = this.getAttribute("doc-id") ?? `doc-${crypto.randomUUID()}`;
        this.renderShell();
        workspace().registerDocument(this.docId, {
            save: () => this.save(),
            dispose: () => this.disposeEngine(),
        });
        this.setStatus("Loading engine…");
        const engine = await getEngine();
        if (this.disposed) {
            return;
        }
        this.client = engine.client;
        this.dataId = engine.dataId;
        this.bases = (await this.client.listBases(this.dataId)).filter(
            (base) => base.support === 0,
        );
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
        }
        if (!this.bases.some((b) => b.path === this.base)) {
            this.base = this.bases[0]?.path ?? this.base;
        }
        this.syncControls();
        this.populateBaseList();

        await this.openSession();
        if (this.disposed) {
            return;
        }
        const item = draft?.state
            ? await this.client.importItem(draft.state)
            : await this.client.createItem(this.session, {
                rarity: this.rarity,
                withImplicits: true,
            });
        if (this.disposed) {
            await this.client.closeItem(item);
            return;
        }
        this.item = item;
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
        this.setStatus("");
    }

    disconnectedCallback(): void {
        // Note: dockview also fires this during drag-between-groups, so we must
        // not destroy persistent state here — the draft already holds it.
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
        this.modCache.clear();
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
    }

    private async rebuildSession(): Promise<void> {
        await this.openSession();
        this.history = [];
        if (this.item) {
            await this.client.closeItem(this.item);
        }
        this.item = await this.client.createItem(this.session, {
            rarity: this.rarity,
            withImplicits: true,
        });
        await this.markChanged();
    }

    private async createItem(): Promise<void> {
        if (this.item) {
            await this.client.closeItem(this.item);
        }
        this.item = await this.client.createItem(this.session, {
            rarity: this.rarity,
            withImplicits: true,
        });
        this.history = [];
        await this.markChanged();
    }

    private async applyAction(type: CraftAction["type"]): Promise<void> {
        const outcome = await this.client.apply(this.context, this.item, { type });
        this.history.push({
            action: type,
            applied: outcome.applied,
            added: outcome.added,
            removed: outcome.removed,
        });
        await this.markChanged();
    }

    private async markChanged(): Promise<void> {
        if (!this.initializing) {
            this.dirty = true;
        }
        await this.refresh();
        await this.persist();
        if (!this.initializing) {
            workspace().notifyDirty(this.docId, this.dirty, this.docTitle);
        }
    }

    private async snapshot(): Promise<ItemSnapshot> {
        const info = await this.client.itemInfo(this.item);
        return {
            base: this.base,
            itemLevel: this.itemLevel,
            rarity: info.rarity as string,
            state: await this.client.exportItem(this.item),
        };
    }

    private async persist(): Promise<void> {
        const draft: DraftRecord = {
            docId: this.docId,
            base: this.base,
            itemLevel: this.itemLevel,
            rarity: this.rarity,
            state: await this.client.exportItem(this.item),
            history: this.history,
            savedRef: this.savedRef,
            savedName: this.savedName,
            dirty: this.dirty,
            updatedAt: Date.now(),
        };
        await putDraft(draft);
    }

    // --- save / save-as / duplicate ----------------------------------------

    private async save(): Promise<boolean> {
        if (!this.savedRef) {
            return this.saveAs();
        }
        const snapshot = await this.snapshot();
        const record: StashRecord = {
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
        const name = prompt("Save item to Stash as:", this.savedName ?? "New item");
        if (!name) {
            return false;
        }
        const record: StashRecord = {
            id: `stash-${crypto.randomUUID()}`,
            name,
            ...(await this.snapshot()),
            createdAt: Date.now(),
        };
        await workspace().saveToStash(record);
        await this.markSaved(record);
        return true;
    }

    private async markSaved(record: StashRecord): Promise<void> {
        this.savedRef = record.id;
        this.savedName = record.name;
        this.savedCreatedAt = record.createdAt;
        this.dirty = false;
        await this.persist();
        workspace().notifyDirty(this.docId, false, this.docTitle);
        this.renderSavedName();
    }

    private async duplicate(): Promise<void> {
        await workspace().openEmulator(await this.snapshot(), "copy");
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
    }

    // --- mod resolution -----------------------------------------------------

    private async resolveMod(id: number): Promise<ModInfo> {
        const cached = this.modCache.get(id);
        if (cached) {
            return cached;
        }
        const info = await this.client.modInfo(this.session, id);
        this.modCache.set(id, info);
        return info;
    }

    private async toRows(ids: number[], fractured: Set<number>): Promise<ModRow[]> {
        return Promise.all(
            ids.map(async (id) => ({
                key: (await this.resolveMod(id)).key,
                fractured: fractured.has(id),
            })),
        );
    }

    // --- refresh / render ---------------------------------------------------

    private async refresh(): Promise<void> {
        const info = await this.client.itemInfo(this.item);
        const fracturedP = new Set(info.fractured_prefix_mod_ids as number[]);
        const fracturedS = new Set(info.fractured_suffix_mod_ids as number[]);
        const [prefixes, suffixes, implicits] = await Promise.all([
            this.toRows(info.prefix_mod_ids as number[], fracturedP),
            this.toRows(info.suffix_mod_ids as number[], fracturedS),
            this.toRows(info.implicit_mod_ids as number[], new Set()),
        ]);
        this.modList.setModel({
            rarity: info.rarity as string,
            prefixes,
            suffixes,
            implicits,
        });
        const pool = await this.client.debugPool(this.context, this.item, {
            action: { type: this.poolAction },
        });
        this.weightTable.setData(pool);
        this.renderHistory();
    }

    private get modList(): PcModList {
        return this.querySelector("pc-mod-list")!;
    }

    private get weightTable(): PcWeightTable {
        return this.querySelector("pc-weight-table")!;
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
        this.querySelectorAll<HTMLButtonElement>("button[data-action], button[data-cmd]").forEach(
            (button) => {
                button.disabled = busy;
            },
        );
    }

    private async guard(work: () => Promise<void>): Promise<void> {
        if (this.busy || this.disposed) {
            return;
        }
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
        }
    }

    private syncControls(): void {
        this.querySelector<HTMLInputElement>(".pc-ilvl")!.value = String(this.itemLevel);
        this.querySelector<HTMLSelectElement>(".pc-rarity")!.value = this.rarity;
        this.querySelector<HTMLSelectElement>(".pc-pool-action")!.value = this.poolAction;
        this.renderSavedName();
    }

    private populateBaseList(): void {
        const combobox = this.querySelector<PcCombobox>("pc-combobox")!;
        combobox.setOptions(
            this.bases.map((base) => ({ value: base.path, label: baseLabel(base.path) })),
        );
        combobox.setValue(this.base);
    }

    private renderSavedName(): void {
        const el = this.querySelector(".pc-emu-name");
        if (el) {
            el.textContent = this.savedName ? `Saved: ${this.savedName}` : "Unsaved";
        }
    }

    private renderHistory(): void {
        const el = this.querySelector(".pc-emu-history")!;
        if (this.history.length === 0) {
            el.innerHTML = '<p class="pc-empty">No crafts yet.</p>';
            return;
        }
        el.innerHTML = this.history
            .map((entry, index) => {
                const detail = entry.applied
                    ? `+${entry.added} / -${entry.removed}`
                    : "no-op";
                return `<li class="${entry.applied ? "" : "pc-history-noop"}">
                    <span class="pc-history-n">${index + 1}</span>
                    <span class="pc-history-action">${entry.action}</span>
                    <span class="pc-history-detail">${detail}</span>
                </li>`;
            })
            .join("");
    }

    private renderShell(): void {
        this.innerHTML = `
            <div class="pc-emulator">
                <div class="pc-craft-bar">
                    <label>Base
                        <pc-combobox placeholder="Search bases…"></pc-combobox>
                    </label>
                    <label>iLvl
                        <input class="pc-ilvl" type="number" min="1" max="100" value="${this.itemLevel}" />
                    </label>
                    <label>Rarity
                        <select class="pc-rarity">
                            <option value="normal">normal</option>
                            <option value="magic">magic</option>
                            <option value="rare">rare</option>
                        </select>
                    </label>
                    <button data-cmd="create">Create item</button>
                    <span class="pc-craft-actions">
                        ${ACTIONS.map((a) => `<button data-action="${a}">${a}</button>`).join("")}
                    </span>
                    <span class="pc-emu-save">
                        <span class="pc-emu-name">Unsaved</span>
                        <button data-cmd="save">Save</button>
                        <button data-cmd="save-as">Save As</button>
                        <button data-cmd="duplicate">Duplicate</button>
                    </span>
                    <span class="pc-emu-status" hidden></span>
                </div>
                <div class="pc-emu-body">
                    <section class="pc-emu-item">
                        <h3>Item</h3>
                        <pc-mod-list></pc-mod-list>
                    </section>
                    <section class="pc-emu-side">
                        <h3>Craft history</h3>
                        <ul class="pc-emu-history"></ul>
                        <h3>Candidate pool
                            <select class="pc-pool-action">
                                ${ACTIONS.map(
                                    (a) =>
                                        `<option value="${a}" ${a === this.poolAction ? "selected" : ""}>${a}</option>`,
                                ).join("")}
                            </select>
                        </h3>
                        <pc-weight-table></pc-weight-table>
                    </section>
                </div>
            </div>`;

        this.querySelector("pc-combobox")!.addEventListener("change", (event) => {
            const value = (event as CustomEvent<{ value: string }>).detail.value;
            void this.guard(async () => {
                this.base = value;
                await this.rebuildSession();
            });
        });
        this.querySelector<HTMLInputElement>(".pc-ilvl")!.addEventListener("change", (event) => {
            const value = Number((event.target as HTMLInputElement).value);
            if (Number.isFinite(value) && value > 0) {
                void this.guard(async () => {
                    this.itemLevel = value;
                    await this.rebuildSession();
                });
            }
        });
        this.querySelector<HTMLSelectElement>(".pc-rarity")!.addEventListener("change", (event) => {
            this.rarity = (event.target as HTMLSelectElement).value;
        });
        this.querySelector<HTMLSelectElement>(".pc-pool-action")!.addEventListener("change", (event) => {
            this.poolAction = (event.target as HTMLSelectElement).value as CraftAction["type"];
            void this.guard(() => this.refresh());
        });
        this.querySelectorAll<HTMLButtonElement>("button[data-cmd]").forEach((button) => {
            button.addEventListener("click", () => {
                const cmd = button.dataset.cmd;
                void this.guard(async () => {
                    if (cmd === "create") await this.createItem();
                    else if (cmd === "save") await this.save();
                    else if (cmd === "save-as") await this.saveAs();
                    else if (cmd === "duplicate") await this.duplicate();
                });
            });
        });
        this.querySelectorAll<HTMLButtonElement>("button[data-action]").forEach((button) => {
            button.addEventListener("click", () => {
                void this.guard(() => this.applyAction(button.dataset.action as CraftAction["type"]));
            });
        });
    }
}

customElements.define("pc-emulator", PcEmulator);
