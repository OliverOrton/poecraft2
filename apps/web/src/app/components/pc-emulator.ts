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
import { BaseInfo, Catalog, CraftAction, ModInfo } from "../engine-protocol";
import {
    craftActionLabel,
    groupEssences,
    resistanceEntries,
} from "../craft-choices";
import {
    DraftRecord,
    ItemStashRecord,
    ItemSnapshot,
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

const BASIC_ACTIONS: CraftAction["type"][] = [
    "transmute",
    "augment",
    "alteration",
    "regal",
    "alchemy",
    "chaos",
    "exalt",
    "annul",
    "scour",
    "fracture",
];

type CraftPanel =
    | "basic"
    | "essence"
    | "harvest"
    | "fossil"
    | "eldritch"
    | "influenced"
    | "veiled";

const CRAFT_PANELS: Array<[CraftPanel, string]> = [
    ["basic", "Basic currency"],
    ["essence", "Essences"],
    ["harvest", "Harvest"],
    ["fossil", "Fossil"],
    ["eldritch", "Eldritch"],
    ["influenced", "Influenced"],
    ["veiled", "Veiled"],
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
    private catalog: Catalog | null = null;

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
        this.docId = this.getAttribute("doc-id") ?? `doc-${crypto.randomUUID()}`;
        this.renderShell();
        this.setBusy(true);
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
        this.catalog = await this.client.catalog(this.dataId);
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

    private async applyConfiguredAction(action: CraftAction): Promise<void> {
        const outcome = await this.client.apply(this.context, this.item, action);
        this.history.push({
            action: action.type,
            applied: outcome.applied,
            added: outcome.added,
            removed: outcome.removed,
        });
        await this.markChanged();
    }

    private async craftMod(
        key: string,
        side: "prefix" | "suffix",
        fractured = false,
    ): Promise<void> {
        const info = this.modCache.find((mod) => mod.key === key);
        let applied = true;
        if (info?.reach_kind === REACH_KIND_CRAFTED && !fractured) {
            const outcome = await this.client.apply(this.context, this.item, {
                type: "bench",
                mod_key: key,
            });
            applied = outcome.applied;
        } else {
            await this.client.addMod(this.item, this.session, {
                key,
                side,
                fractured,
            });
        }
        this.history.push({
            action: `${fractured ? "fracture" : "add"} ${side} ${key}`,
            applied,
            added: applied ? 1 : 0,
            removed: 0,
        });
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
            this.history.push({
                action: `fracture ${side} ${key}`,
                applied: true,
                added: 0,
                removed: 0,
            });
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
        this.history.push({
            action: `remove ${side} ${info?.key ?? modId}`,
            applied: true,
            added: 0,
            removed: 1,
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
        this.veiledOptions =
            (info.veiled_option_mod_ids as number[] | undefined) ?? [];
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
        const name = prompt("Save item to Stash as:", this.savedName ?? "New item");
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

    private async useInStrategy(): Promise<void> {
        await workspace().openStrategy(await this.snapshot(), "copy");
    }

    private async openInCalculator(): Promise<void> {
        await workspace().openCalculator(await this.snapshot());
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
            rarity: info.rarity as string,
            influences: influenceLabels(
                Number(info.generic_influence_bits ?? 0),
                Number(info.searing_exarch_tier ?? 0),
                Number(info.eater_of_worlds_tier ?? 0),
                this.catalog,
            ),
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
        this.renderMechanicControls();
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
        this.querySelectorAll<HTMLButtonElement>(
            "button[data-cmd], button[data-craft-panel], button[data-simple-action], button[data-config-action], button[data-fossil-add], button[data-fossil-remove]",
        ).forEach((button) => {
            if (busy) {
                button.dataset.disabledBeforeBusy ??= String(button.disabled);
                button.disabled = true;
            } else if (button.dataset.disabledBeforeBusy !== undefined) {
                button.disabled = button.dataset.disabledBeforeBusy === "true";
                delete button.dataset.disabledBeforeBusy;
            }
        });
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

    private renderMechanicControls(): void {
        const host = this.querySelector(".pc-advanced-crafts");
        if (!host || !this.catalog) return;
        host.querySelectorAll<HTMLSelectElement>("select[data-mechanic]").forEach(
            (select) => {
                const name = select.dataset.mechanic;
                if (name) this.mechanicValues.set(name, select.value);
            },
        );
        const selected = (name: string, fallback = "") =>
            this.mechanicValues.get(name) ?? fallback;
        const options = (
            entries: Array<{ key: string; name: string }>,
            selected?: string,
        ) =>
            entries
                .map(
                    (entry) =>
                        `<option value="${escapeHtml(entry.key)}" ${entry.key === selected ? "selected" : ""}>${escapeHtml(entry.name)}</option>`,
                )
                .join("");
        const essenceGroups = groupEssences(this.catalog.essences);
        const selectedEssenceType =
            essenceGroups.some(
                (group) => group.type === selected("essence-type"),
            )
                ? selected("essence-type")
                : (essenceGroups[0]?.type ?? "");
        const essenceTiers =
            essenceGroups.find((group) => group.type === selectedEssenceType)
                ?.tiers ?? [];
        const selectedEssenceKey =
            essenceTiers.some(
                (entry) => entry.key === selected("essence-key"),
            )
                ? selected("essence-key")
                : (essenceTiers[0]?.key ?? "");
        const unveilEntries = this.veiledOptions
            .map((id) => this.modCache[id])
            .filter((mod): mod is ModInfo => Boolean(mod))
            .map((mod) => ({
                key: mod.key,
                name: mod.text_lines.join(" / ") || mod.key,
            }));
        const panel = (() => {
            switch (this.activeCraftPanel) {
                case "basic":
                    return `
                        <div class="pc-craft-options">
                            ${BASIC_ACTIONS.map(
                                (type) =>
                                    `<button data-simple-action="${type}">${craftActionLabel(type)}</button>`,
                            ).join("")}
                        </div>
                        <div class="pc-fracture-hint">Use Fracture for the real random orb outcome. Right-click an item modifier to mark that exact mod fractured, or right-click a pool tier to add it fractured.</div>`;
                case "essence":
                    return `
                        <div class="pc-mechanic-row">
                            <label>
                                <span>Type</span>
                                <select data-mechanic="essence-type">${options(
                                    essenceGroups.map((group) => ({
                                        key: group.type,
                                        name: group.type,
                                    })),
                                    selectedEssenceType,
                                )}</select>
                            </label>
                            <label>
                                <span>Tier</span>
                                <select data-mechanic="essence-key">${options(
                                    essenceTiers.map((entry) => ({
                                        key: entry.key,
                                        name: entry.tier,
                                    })),
                                    selectedEssenceKey,
                                )}</select>
                            </label>
                            <button data-config-action="essence">Apply essence</button>
                        </div>`;
                case "harvest":
                    return `
                        <div class="pc-mechanic-row">
                            <select data-mechanic="harvest-tag">${options(
                                this.catalog.harvestTags,
                                selected(
                                    "harvest-tag",
                                    this.catalog.harvestTags[0]?.key,
                                ),
                            )}</select>
                            <button data-config-action="harvest_reforge">Reforge</button>
                            <button data-config-action="harvest_augment">Augment</button>
                        </div>
                        <div class="pc-mechanic-row">
                            <select data-mechanic="resist-from">${options(
                                resistanceEntries(),
                                selected("resist-from", "fire"),
                            )}</select>
                            <span>&rarr;</span>
                            <select data-mechanic="resist-to">${options(
                                resistanceEntries(),
                                selected("resist-to", "cold"),
                            )}</select>
                            <button data-config-action="harvest_resist">Convert resistance</button>
                        </div>`;
                case "fossil":
                    return `
                        <div class="pc-mechanic-row">
                            <select data-mechanic="fossil">${options(
                                this.catalog.fossils,
                                selected("fossil", this.catalog.fossils[0]?.key),
                            )}</select>
                            <button data-fossil-add ${this.selectedFossils.length >= 4 ? "disabled" : ""}>Add fossil</button>
                            <button data-config-action="fossil" ${this.selectedFossils.length ? "" : "disabled"}>Craft</button>
                        </div>
                        <div class="pc-selected-fossils">
                            ${
                                this.selectedFossils.length
                                    ? this.selectedFossils
                                          .map(
                                              (key, index) =>
                                                  `<span class="pc-chip">${escapeHtml(
                                                      this.catalog?.fossils.find(
                                                          (entry) => entry.key === key,
                                                      )?.name ?? key,
                                                  )}<button data-fossil-remove="${index}" title="Remove">&times;</button></span>`,
                                          )
                                          .join("")
                                    : '<span class="pc-help">Choose up to four fossils.</span>'
                            }
                        </div>`;
                case "eldritch":
                    return `
                        <div class="pc-mechanic-row">
                            <select data-mechanic="eldritch-tier">${[1, 2, 3, 4]
                                .map(
                                    (tier) =>
                                        `<option value="${tier}" ${String(tier) === selected("eldritch-tier", "1") ? "selected" : ""}>Tier ${tier}</option>`,
                                )
                                .join("")}</select>
                            <button data-config-action="eldritch_ember">Apply Ember</button>
                            <button data-config-action="eldritch_ichor">Apply Ichor</button>
                        </div>
                        <div class="pc-craft-options">
                            ${["eldritch_exalt", "eldritch_chaos", "eldritch_annul"]
                                .map(
                                    (type) =>
                                        `<button data-simple-action="${type}">${craftActionLabel(type as CraftAction["type"])}</button>`,
                                )
                                .join("")}
                        </div>`;
                case "influenced":
                    return `
                        <div class="pc-mechanic-row">
                            <select data-mechanic="influence">${options(
                                this.catalog.influences,
                                selected(
                                    "influence",
                                    this.catalog.influences[0]?.key,
                                ),
                            )}</select>
                            <button data-config-action="influence_exalt">Influenced exalt</button>
                        </div>`;
                case "veiled":
                    return `
                        <div class="pc-craft-options">
                            <button data-simple-action="veiled_chaos">Veiled chaos</button>
                            <button data-simple-action="veiled_exalt">Veiled exalt</button>
                        </div>
                        ${
                            unveilEntries.length
                                ? `<div class="pc-mechanic-row">
                                    <select data-mechanic="unveil">${options(
                                        unveilEntries,
                                        selected(
                                            "unveil",
                                            unveilEntries[0]?.key,
                                        ),
                                    )}</select>
                                    <button data-config-action="unveil">Unveil</button>
                                </div>`
                                : '<span class="pc-help">Apply a veiled modifier to choose an unveil.</span>'
                        }`;
            }
        })();
        host.innerHTML = `
            <div class="pc-craft-panel-tabs">
                ${CRAFT_PANELS.map(
                    ([key, label]) =>
                        `<button data-craft-panel="${key}" class="${key === this.activeCraftPanel ? "is-active" : ""}">${label}</button>`,
                ).join("")}
            </div>
            <div class="pc-craft-panel-body">${panel}</div>`;
        host.querySelectorAll<HTMLSelectElement>("select[data-mechanic]").forEach(
            (select) => {
                select.addEventListener("change", () => {
                    const name = select.dataset.mechanic;
                    if (!name) return;
                    this.mechanicValues.set(name, select.value);
                    if (name === "essence-type") {
                        this.mechanicValues.delete("essence-key");
                        this.renderMechanicControls();
                    }
                });
            },
        );
        host.querySelectorAll<HTMLButtonElement>("[data-craft-panel]").forEach(
            (button) => {
                button.addEventListener("click", () => {
                    this.activeCraftPanel = button.dataset
                        .craftPanel as CraftPanel;
                    this.renderMechanicControls();
                });
            },
        );
        host.querySelectorAll<HTMLButtonElement>("[data-simple-action]").forEach(
            (button) => {
                button.addEventListener("click", () => {
                    void this.guard(() =>
                        this.applyAction(
                            button.dataset.simpleAction as CraftAction["type"],
                        ),
                    );
                });
            },
        );
        host.querySelectorAll<HTMLButtonElement>("[data-config-action]").forEach(
            (button) => {
                button.addEventListener("click", () => {
                    const type = button.dataset.configAction as CraftAction["type"];
                    const value = (name: string) =>
                        host.querySelector<HTMLSelectElement>(
                            `[data-mechanic="${name}"]`,
                        )?.value ?? "";
                    let action: CraftAction = { type };
                    if (type === "essence")
                        action = { type, essence: value("essence-key") };
                    else if (type === "fossil")
                        action = { type, fossils: [...this.selectedFossils] };
                    else if (
                        type === "harvest_reforge" ||
                        type === "harvest_augment"
                    )
                        action = { type, target_tag: value("harvest-tag") };
                    else if (type === "harvest_resist")
                        action = {
                            type,
                            source_tag: value("resist-from"),
                            target_tag: value("resist-to"),
                        };
                    else if (
                        type === "eldritch_ember" ||
                        type === "eldritch_ichor"
                    )
                        action = {
                            type,
                            tier: Number(value("eldritch-tier")),
                        };
                    else if (type === "influence_exalt")
                        action = { type, influence: value("influence") };
                    else if (type === "unveil")
                        action = { type, mod_key: value("unveil") };
                    void this.guard(() => this.applyConfiguredAction(action));
                });
            },
        );
        host.querySelector<HTMLButtonElement>("[data-fossil-add]")?.addEventListener(
            "click",
            () => {
                const key =
                    host.querySelector<HTMLSelectElement>(
                        '[data-mechanic="fossil"]',
                    )?.value ?? "";
                if (
                    key &&
                    this.selectedFossils.length < 4 &&
                    !this.selectedFossils.includes(key)
                ) {
                    this.selectedFossils = [...this.selectedFossils, key];
                    this.renderMechanicControls();
                }
            },
        );
        host.querySelectorAll<HTMLButtonElement>("[data-fossil-remove]").forEach(
            (button) => {
                button.addEventListener("click", () => {
                    const index = Number(button.dataset.fossilRemove);
                    this.selectedFossils = this.selectedFossils.filter(
                        (_, entryIndex) => entryIndex !== index,
                    );
                    this.renderMechanicControls();
                });
            },
        );
        this.setBusy(this.busy);
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
                    <span class="pc-emu-save">
                        <span class="pc-emu-name">Unsaved</span>
                        <button data-cmd="save">Save</button>
                        <button data-cmd="save-as">Save As</button>
                        <button data-cmd="duplicate">Duplicate</button>
                        <button data-cmd="strategy">Use in Strategy</button>
                        <button data-cmd="calculator">Odds</button>
                    </span>
                    <span class="pc-emu-status" hidden></span>
                </div>
                <div class="pc-advanced-crafts"></div>
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
                    if (cmd === "create") await this.createItem();
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

function influenceLabels(
    genericBits: number,
    searingExarchTier: number,
    eaterOfWorldsTier: number,
    catalog: Catalog | null,
): string[] {
    const labels: string[] = [];
    for (const influence of catalog?.influences ?? []) {
        const code = influence.code ?? 0;
        if (code > 0 && (genericBits & (1 << (code - 1))) !== 0) {
            labels.push(influence.name);
        }
    }
    if (searingExarchTier > 0) {
        labels.push(`Searing Exarch T${searingExarchTier}`);
    }
    if (eaterOfWorldsTier > 0) {
        labels.push(`Eater of Worlds T${eaterOfWorldsTier}`);
    }
    return labels;
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
