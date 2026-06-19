/*
 * pc-emulator — the first emulator document. Owns one engine session, action
 * context, and live item, and drives the craft bar, mod list, and debug weight
 * table. Crafts are applied one operation at a time and recorded in history.
 *
 * This is the real WASM-backed slice (no mock engine): every control maps to an
 * EngineClient call against the shared worker.
 */

import { getEngine } from "../engine-service";
import { EngineClient } from "../engine-client";
import { BaseInfo, CraftAction, ModInfo } from "../engine-protocol";
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

    async connectedCallback(): Promise<void> {
        this.renderShell();
        this.setStatus("Loading engine…");
        const engine = await getEngine();
        this.client = engine.client;
        this.dataId = engine.dataId;
        this.bases = (await this.client.listBases(this.dataId)).filter(
            (base) => base.support === 0,
        );
        if (!this.bases.some((b) => b.path === this.base)) {
            this.base = this.bases[0]?.path ?? this.base;
        }
        this.populateBaseList();
        await this.rebuildSession();
        this.setStatus("");
    }

    // --- engine lifecycle ---------------------------------------------------

    private async rebuildSession(): Promise<void> {
        if (this.session) {
            await this.client.closeContext(this.context);
            await this.client.closeSession(this.session);
        }
        this.modCache.clear();
        this.session = await this.client.createSession(
            this.dataId,
            this.base,
            this.itemLevel,
        );
        this.context = await this.client.createContext(this.session, 0);
        this.item = 0;
        this.history = [];
        await this.createItem();
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
        await this.refresh();
    }

    private async applyAction(type: CraftAction["type"]): Promise<void> {
        const outcome = await this.client.apply(this.context, this.item, { type });
        this.history.push({
            action: type,
            applied: outcome.applied,
            added: outcome.added,
            removed: outcome.removed,
        });
        await this.refresh();
    }

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
        this.querySelectorAll<HTMLButtonElement>("button[data-action], button[data-create]").forEach(
            (button) => {
                button.disabled = busy;
            },
        );
    }

    private async guard(work: () => Promise<void>): Promise<void> {
        if (this.busy) {
            return;
        }
        this.setBusy(true);
        try {
            await work();
        } catch (error) {
            this.setStatus(error instanceof Error ? error.message : String(error));
        } finally {
            this.setBusy(false);
        }
    }

    private populateBaseList(): void {
        const combobox = this.querySelector<PcCombobox>("pc-combobox")!;
        combobox.setOptions(
            this.bases.map((base) => ({ value: base.path, label: baseLabel(base.path) })),
        );
        combobox.setValue(this.base);
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
                            <option value="rare" selected>rare</option>
                        </select>
                    </label>
                    <button data-create>Create item</button>
                    <span class="pc-craft-actions">
                        ${ACTIONS.map((a) => `<button data-action="${a}">${a}</button>`).join("")}
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
        this.querySelector("[data-create]")!.addEventListener("click", () => {
            void this.guard(() => this.createItem());
        });
        this.querySelectorAll<HTMLButtonElement>("button[data-action]").forEach((button) => {
            button.addEventListener("click", () => {
                void this.guard(() => this.applyAction(button.dataset.action as CraftAction["type"]));
            });
        });
    }
}

customElements.define("pc-emulator", PcEmulator);
