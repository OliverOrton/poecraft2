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
        list.innerHTML = records
            .map(
                (record) => `
                <div class="pc-stash-item" data-id="${record.id}">
                    <div class="pc-stash-meta">
                        <span class="pc-stash-name">${record.name}</span>
                        <span class="pc-stash-base">${baseLabel(record.base)} · iLvl ${record.itemLevel}</span>
                    </div>
                    <div class="pc-stash-actions">
                        <button data-act="open">Edit</button>
                        <button data-act="copy">Import copy</button>
                        <button data-act="delete">Delete</button>
                    </div>
                </div>`,
            )
            .join("");

        const byId = new Map(records.map((record) => [record.id, record]));
        list.querySelectorAll<HTMLButtonElement>("button[data-act]").forEach((button) => {
            button.addEventListener("click", () => {
                const id = button.closest<HTMLElement>(".pc-stash-item")!.dataset.id!;
                const record = byId.get(id)!;
                void this.handle(button.dataset.act!, record);
            });
        });
    }

    private async handle(action: string, record: StashRecord): Promise<void> {
        const snapshot = {
            base: record.base,
            itemLevel: record.itemLevel,
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
