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


import { disconnectReact, renderReact } from "../react-host";
import { GameIcon, GameItemName } from "./pc-game-icon";

export class PcStash extends HTMLElement {
    private unsubscribe: (() => void) | null = null;
    private records: StashRecord[] = [];
    private filter = "all";
    private refreshVersion = 0;
    connectedCallback(): void {
        this.unsubscribe?.();
        this.unsubscribe = workspace().onStashChange(() => void this.refresh());
        this.render();
        void this.refresh();
    }
    disconnectedCallback(): void {
        this.unsubscribe?.(); this.unsubscribe = null;
        this.refreshVersion++;
        disconnectReact(this);
    }
    private async refresh(): Promise<void> {
        const version = ++this.refreshVersion;
        const records = await listStash();
        if (version !== this.refreshVersion) return;
        this.records = records.sort((a,b) => b.createdAt - a.createdAt);
        this.render();
    }
    private render(): void {
        const records = this.records.filter(record => this.filter === "all" ||
            (this.filter === "strategy") === isStrategyStashRecord(record));
        renderReact(this, <div className="pc-stash">
            <h3>Stash</h3>
            <div className="pc-stash-filters">
                {(["all", "item", "strategy"] as const).map(filter => <button key={filter} data-filter={filter}
                    className={this.filter === filter ? "is-active" : ""} aria-pressed={this.filter === filter}
                    onClick={() => { this.filter = filter; this.render(); }}>
                    {filter === "all" ? "All" : filter === "item" ? "Items" : "Strategies"}
                </button>)}
            </div>
            <div className="pc-stash-list">{records.length ? records.map(record => {
                const strategy = isStrategyStashRecord(record);
                const graph = strategy && isStrategyDocument(record.strategy) ? record.strategy : null;
                const lifecycle = !strategy ? Number((record.state as {lifecycle?: number})?.lifecycle ?? 0) : 0;
                const routes = graph?.edges.filter(edge => graph.nodes.find(node => node.id === edge.to)?.terminal === "success").length ?? 0;
                const detail = strategy ? graph ? `${graph.nodes.length} nodes · ${routes} success route${routes === 1 ? "" : "s"}` : "Invalid saved strategy"
                    : <><GameItemName assetKey={record.base} fallback={baseLabel(record.base)} /> · iLvl {record.itemLevel}{lifecycle ? lifecycle === 1 ? " · Consumed" : " · Destroyed" : ""}</>;
                const entries = [["open", "Edit"], ["copy", "Import copy"], ...(!strategy ? [["odds", "Odds"]] : []), ["delete", "Delete"]];
                return <div className="pc-stash-item" key={record.id}>
                    <GameIcon assetKey={strategy ? graph?.base_state.base_key ?? "" : record.base} size="item" />
                    <div className="pc-stash-meta"><span className="pc-stash-name">{record.name}</span><span className="pc-stash-base">{detail}</span></div>
                    <div className="pc-stash-actions">{entries.map(([action, label]) => <button key={action}
                        disabled={!!lifecycle && action !== "delete"}
                        onClick={() => void this.handle(action, record)}>{label}</button>)}</div>
                </div>;
            }) : <p className="pc-empty">No saved resources in this view.</p>}</div>
        </div>);
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
            resourceIdentity: record.id,
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
