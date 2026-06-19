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
import { PcBasePicker, BasePickerSelection } from "./pc-base-picker";
import { PcModList, SlotMod } from "./pc-mod-list";
import { PcModPool } from "./pc-mod-pool";
import "./pc-base-picker";
import "./pc-mod-list";
import "./pc-mod-pool";

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

const REACH_KIND_CRAFTED = 2;

export class PcEmulator extends HTMLElement {
    private client!: EngineClient;
    private dataId = 0;
    private bases: BaseInfo[] = [];

    private docId = "";
    private base = DEFAULT_BASE;
    private itemLevel = 86;
    private rarity = "normal";

    private session = 0;
    private context = 0;
    private item = 0;
    private history: HistoryEntry[] = [];
    private modCache: ModInfo[] = [];
    private familyLabels = new Map<number, string>();
    private busy = false;
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
        this.hasBase = Boolean(draft?.state);

        if (!this.hasBase) {
            this.pickerOpen = true;
            this.renderShell();
            this.setStatus("");
            workspace().notifyDirty(this.docId, this.dirty, this.docTitle);
            return;
        }

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
        this.familyLabels.clear();
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
        const labels = new Map<number, string>();
        for (const info of cache
            .slice()
            .sort((a, b) => b.required_level - a.required_level)) {
            if (labels.has(info.family_id)) continue;
            labels.set(
                info.family_id,
                info.text_lines.join(" / ") || info.key,
            );
        }
        this.familyLabels = labels;
    }

    private async rebuildSession(): Promise<void> {
        await this.openSession();
        this.history = [];
        if (this.item) {
            await this.client.closeItem(this.item);
        }
        this.rarity = "normal";
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
        this.rarity = "normal";
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

    private async craftMod(key: string, side: "prefix" | "suffix"): Promise<void> {
        await this.client.addMod(this.item, this.session, { key, side });
        this.history.push({
            action: `add ${side} ${key}`,
            applied: true,
            added: 1,
            removed: 0,
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
        const info = await this.client.itemInfo(this.item, this.session);
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
            state: this.item ? await this.client.exportItem(this.item) : null,
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

    // --- refresh / render ---------------------------------------------------

    private async refresh(): Promise<void> {
        const info = await this.client.itemInfo(this.item, this.session);
        this.rarity = info.rarity as string;
        const fracturedP = new Set(info.fractured_prefix_mod_ids as number[]);
        const fracturedS = new Set(info.fractured_suffix_mod_ids as number[]);
        const prefixIds = info.prefix_mod_ids as number[];
        const suffixIds = info.suffix_mod_ids as number[];
        const implicitIds = info.implicit_mod_ids as number[];

        const prefixes = prefixIds.map((id) => this.toSlot(id, fracturedP));
        const suffixes = suffixIds.map((id) => this.toSlot(id, fracturedS));
        const implicits = implicitIds.map((id) => this.toSlot(id, new Set()));

        this.modList.setModel({
            rarity: info.rarity as string,
            prefixes,
            suffixes,
            implicits,
            maxPrefix: (info.max_prefix as number) ?? prefixes.length,
            maxSuffix: (info.max_suffix as number) ?? suffixes.length,
        });

        const tab = this.modPool.getActiveTab();
        const poolAction: CraftAction["type"] =
            tab === "implicit" ? "chaos" : tab === "prefix" ? "chaos" : "chaos";
        const pool =
            tab === "implicit"
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
                groupOnItem,
                maxPrefix: (info.max_prefix as number) ?? prefixes.length,
                maxSuffix: (info.max_suffix as number) ?? suffixes.length,
            },
            pool,
            poolWeights,
        });
        this.renderHistory();
    }

    private toSlot(id: number, fractured: Set<number>): SlotMod {
        const info = this.modCache[id];
        if (!info) {
            return {
                sessionModId: id,
                key: String(id),
                displayName: "",
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
            displayName:
                this.familyLabels.get(info.family_id) ||
                info.text_lines.join(" / ") ||
                info.key,
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
        this.renderSavedName();
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
        if (this.history.length === 0) {
            el.innerHTML = '<li class="pc-empty">No crafts yet.</li>';
            return;
        }
        el.innerHTML = this.history
            .map((entry, index) => ({ entry, number: index + 1 }))
            .reverse()
            .map(({ entry, number }) => {
                const detail = entry.applied
                    ? `+${entry.added} / -${entry.removed}`
                    : "no-op";
                return `<li class="${entry.applied ? "" : "pc-history-noop"}">
                    <span class="pc-history-n">${number}</span>
                    <span class="pc-history-action">${escapeHtml(entry.action)}</span>
                    <span class="pc-history-detail">${detail}</span>
                </li>`;
            })
            .join("");
    }

    private renderShell(): void {
        if (this.pickerOpen) {
            this.innerHTML = `
                <div class="pc-emulator pc-emulator-picking">
                    <pc-base-picker></pc-base-picker>
                </div>`;
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
                    this.afterPickerClose();
                }
            });
            return;
        }
        this.innerHTML = `
            <div class="pc-emulator">
                <div class="pc-craft-bar">
                    <button data-cmd="change-base">Change base…</button>
                    <span class="pc-emu-base">${escapeHtml(baseLabel(this.base))} · iLvl ${this.itemLevel}</span>
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
                    <section class="pc-emu-pool">
                        <pc-mod-pool allow-direct-craft></pc-mod-pool>
                    </section>
                    <section class="pc-emu-side">
                        <h3>Craft history</h3>
                        <ul class="pc-emu-history"></ul>
                    </section>
                </div>
            </div>`;
        this.syncControls();

        this.querySelectorAll<HTMLButtonElement>("button[data-cmd]").forEach((button) => {
            button.addEventListener("click", () => {
                const cmd = button.dataset.cmd;
                if (cmd === "change-base") {
                    this.pickerOpen = true;
                    this.renderShell();
                    return;
                }
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
        this.modPool.addEventListener("craft-mod", (event) => {
            const detail = (event as CustomEvent<{ key: string; side: "prefix" | "suffix" }>).detail;
            void this.guard(() => this.craftMod(detail.key, detail.side));
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
