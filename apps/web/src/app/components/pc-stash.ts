/*
 * pc-stash — lists saved items. Each entry can be opened for editing (rebinds
 * to the saved record) or imported as a new unsaved copy. Reflects only the
 * Stash store, never unsaved drafts.
 */

import { StashRecord, deleteStash, listStash } from "../workspace/persistence";
import { workspace } from "../workspace/registry";

function baseLabel(path: string): string {
    return path.split("/").pop() ?? path;
}

export class PcStash extends HTMLElement {
    private unsubscribe: (() => void) | null = null;

    connectedCallback(): void {
        this.innerHTML = `<div class="pc-stash"><h3>Stash</h3><div class="pc-stash-list"></div></div>`;
        this.unsubscribe = workspace().onStashChange(() => void this.refresh());
        void this.refresh();
    }

    disconnectedCallback(): void {
        this.unsubscribe?.();
        this.unsubscribe = null;
    }

    private async refresh(): Promise<void> {
        const records = (await listStash()).sort((a, b) => b.createdAt - a.createdAt);
        const list = this.querySelector(".pc-stash-list")!;
        if (records.length === 0) {
            list.innerHTML = '<p class="pc-empty">No saved items yet. Save one from an emulator.</p>';
            return;
        }
        list.replaceChildren(
            ...records.map((record) => {
                const item = document.createElement("div");
                item.className = "pc-stash-item";

                const meta = document.createElement("div");
                meta.className = "pc-stash-meta";
                const name = document.createElement("span");
                name.className = "pc-stash-name";
                name.textContent = record.name;
                const base = document.createElement("span");
                base.className = "pc-stash-base";
                base.textContent = `${baseLabel(record.base)} · iLvl ${record.itemLevel}`;
                meta.append(name, base);

                const actions = document.createElement("div");
                actions.className = "pc-stash-actions";
                for (const [action, label] of [
                    ["open", "Edit"],
                    ["copy", "Import copy"],
                    ["delete", "Delete"],
                ] as const) {
                    const button = document.createElement("button");
                    button.textContent = label;
                    button.addEventListener("click", () => {
                        void this.handle(action, record);
                    });
                    actions.appendChild(button);
                }
                item.append(meta, actions);
                return item;
            }),
        );
    }

    private async handle(action: string, record: StashRecord): Promise<void> {
        const snapshot = {
            base: record.base,
            itemLevel: record.itemLevel,
            rarity: record.rarity,
            state: record.state,
        };
        if (action === "open") {
            await workspace().openEmulator(snapshot, "edit", record.id, record.name);
        } else if (action === "copy") {
            await workspace().openEmulator(snapshot, "copy");
        } else if (action === "delete") {
            await deleteStash(record.id);
            await this.refresh();
        }
    }
}

customElements.define("pc-stash", PcStash);
