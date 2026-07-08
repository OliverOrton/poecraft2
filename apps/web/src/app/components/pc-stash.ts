/*
 * pc-stash — lists manually saved items and strategies. Edit rebinds a
 * document to its saved record; Import creates a new unsaved copy.
 */

import {
    StashRecord,
    deleteStash,
    isStrategyStashRecord,
    listStash,
} from "../workspace/persistence";
import {
    StrategyDocument,
    isStrategyDocument,
} from "../strategy-model";
import { workspace } from "../workspace/registry";

function baseLabel(path: string): string {
    return path.split("/").pop() ?? path;
}

export class PcStash extends HTMLElement {
    private unsubscribe: (() => void) | null = null;

    connectedCallback(): void {
        this.innerHTML = `
            <div class="pc-stash">
                <h3>Stash</h3>
                <div class="pc-stash-filters">
                    <button data-filter="all" class="is-active">All</button>
                    <button data-filter="item">Items</button>
                    <button data-filter="strategy">Strategies</button>
                </div>
                <div class="pc-stash-list"></div>
            </div>`;
        this.querySelectorAll<HTMLButtonElement>("[data-filter]").forEach(
            (button) => {
                button.addEventListener("click", () => {
                    this.querySelectorAll("[data-filter]").forEach((entry) =>
                        entry.classList.toggle("is-active", entry === button),
                    );
                    void this.refresh(button.dataset.filter ?? "all");
                });
            },
        );
        this.unsubscribe = workspace().onStashChange(() => void this.refresh());
        void this.refresh();
    }

    disconnectedCallback(): void {
        this.unsubscribe?.();
        this.unsubscribe = null;
    }

    private async refresh(filter?: string): Promise<void> {
        const selected =
            filter ??
            this.querySelector<HTMLButtonElement>("[data-filter].is-active")
                ?.dataset.filter ??
            "all";
        const records = (await listStash())
            .filter((record) => {
                if (selected === "all") return true;
                if (selected === "strategy") return isStrategyStashRecord(record);
                return !isStrategyStashRecord(record);
            })
            .sort((a, b) => b.createdAt - a.createdAt);
        const list = this.querySelector(".pc-stash-list")!;
        if (records.length === 0) {
            list.innerHTML =
                '<p class="pc-empty">No saved resources in this view.</p>';
            return;
        }
        list.replaceChildren(...records.map((record) => this.renderRecord(record)));
    }

    private renderRecord(record: StashRecord): HTMLElement {
        const item = document.createElement("div");
        item.className = "pc-stash-item";

        const meta = document.createElement("div");
        meta.className = "pc-stash-meta";
        const name = document.createElement("span");
        name.className = "pc-stash-name";
        name.textContent = record.name;
        const detail = document.createElement("span");
        detail.className = "pc-stash-base";
        if (isStrategyStashRecord(record)) {
            if (isStrategyDocument(record.strategy)) {
                const strategy = record.strategy as StrategyDocument;
                const successRoutes = strategy.edges.filter(
                    (edge) =>
                        strategy.nodes.find((node) => node.id === edge.to)
                            ?.terminal === "success",
                ).length;
                detail.textContent = `${strategy.nodes.length} nodes · ${successRoutes} success route${successRoutes === 1 ? "" : "s"}`;
            } else {
                detail.textContent = "Invalid saved strategy";
            }
        } else {
            detail.textContent = `${baseLabel(record.base)} · iLvl ${record.itemLevel}`;
        }
        meta.append(name, detail);

        const actions = document.createElement("div");
        actions.className = "pc-stash-actions";
        const entries: ReadonlyArray<readonly [string, string]> =
            isStrategyStashRecord(record)
                ? [
                      ["open", "Edit"],
                      ["copy", "Import copy"],
                      ["delete", "Delete"],
                  ]
                : [
                      ["open", "Edit"],
                      ["copy", "Import copy"],
                      ["odds", "Odds"],
                      ["delete", "Delete"],
                  ];
        for (const [action, label] of entries) {
            const button = document.createElement("button");
            button.textContent = label;
            button.addEventListener("click", () => {
                void this.handle(action, record);
            });
            actions.appendChild(button);
        }
        item.append(meta, actions);
        return item;
    }

    private async handle(action: string, record: StashRecord): Promise<void> {
        if (isStrategyStashRecord(record)) {
            if (!isStrategyDocument(record.strategy)) {
                return;
            }
            if (action === "open") {
                await workspace().openStrategy(
                    record.strategy,
                    "edit",
                    record.id,
                    record.name,
                );
            } else if (action === "copy") {
                await workspace().openStrategy(record.strategy, "copy");
            } else if (action === "delete") {
                await deleteStash(record.id);
                await this.refresh();
            }
            return;
        }

        const snapshot = {
            base: record.base,
            itemLevel: record.itemLevel,
            rarity: record.rarity,
            state: record.state,
        };
        if (action === "open") {
            await workspace().openEmulator(
                snapshot,
                "edit",
                record.id,
                record.name,
            );
        } else if (action === "copy") {
            await workspace().openEmulator(snapshot, "copy");
        } else if (action === "odds") {
            await workspace().openCalculator(snapshot);
        } else if (action === "delete") {
            await deleteStash(record.id);
            await this.refresh();
        }
    }
}

customElements.define("pc-stash", PcStash);
