/*
 * pc-stash — lists manually saved items and strategies. Edit rebinds a
 * document to its saved record; Import creates a new unsaved copy.
 */

import {
    StashRecord,
    deleteStash,
    isStrategyStashRecord,
    listStash,
    itemSnapshotRarity,
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
import { isCorrupted, influenceLabels } from "../item-display";
import { getEngine } from "../engine-service";
import type { Catalog } from "../engine-protocol";
import { InfluenceBadge, ItemStateBadge } from "./pc-item-badges";

export class PcStash extends HTMLElement {
    private unsubscribe: (() => void) | null = null;
    private records: StashRecord[] = [];
    private filter = "all";
    private refreshVersion = 0;
    private catalog: Catalog | null = null;
    private loading = true;
    private error = "";
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
        this.loading = true;
        this.error = "";
        this.render();
        try {
            const [records, catalog] = await Promise.all([listStash(), this.catalog ?? getEngine()
                .then(engine => engine.client.catalog(engine.dataId)).catch(() => null)]);
            if (version !== this.refreshVersion) return;
            this.records = records.sort((a,b) => b.createdAt - a.createdAt);
            this.catalog = catalog;
        } catch (error) {
            if (version !== this.refreshVersion) return;
            this.error = error instanceof Error ? error.message : String(error);
        }
        if (version !== this.refreshVersion) return;
        this.loading = false;
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
            {this.loading && <p className="pc-help" role="status">Loading saved resources…</p>}
            {this.error && <p className="pc-stash-error" role="alert">Saved resources could not be refreshed. {this.error}</p>}
            <div className="pc-stash-list" aria-busy={this.loading}>{records.length ? records.map(record => {
                const strategy = isStrategyStashRecord(record);
                const graph = strategy && isStrategyDocument(record.strategy) ? record.strategy : null;
                const lifecycle = !strategy ? Number((record.state as {lifecycle?: number})?.lifecycle ?? 0) : 0;
                const corrupted = !strategy && isCorrupted(Number((record.state as {item_flags?: number})?.item_flags ?? 0));
                const state = !strategy ? record.state as Record<string, unknown> | null : null;
                const rarity = !strategy && (record.rarity || typeof state?.rarity === "number" && [0, 1, 2].includes(state.rarity)) ? itemSnapshotRarity(record) : null;
                const influences = !strategy ? influenceLabels(Number(state?.generic_influence_bits),
                    Number(state?.searing_exarch_tier ?? 0), Number(state?.eater_of_worlds_tier ?? 0), this.catalog) : [];
                const routes = graph?.edges.filter(edge => graph.nodes.find(node => node.id === edge.to)?.terminal === "success").length ?? 0;
                const detail = strategy ? graph ? `${graph.nodes.length} nodes · ${routes} success route${routes === 1 ? "" : "s"}` : "Invalid saved strategy"
                    : <><GameItemName assetKey={record.base} fallback={baseLabel(record.base)} /> · iLvl {record.itemLevel}{!!lifecycle && <ItemStateBadge state={lifecycle === 1 ? "consumed" : "destroyed"}>{lifecycle === 1 ? "Consumed" : "Destroyed"}</ItemStateBadge>}</>;
                const entries = [["open", "Edit"], ["copy", "Import copy"], ...(!strategy ? [["odds", "Odds"]] : []), ["delete", "Delete"]];
                return <div className={`pc-stash-item ${corrupted ? "is-corrupted" : ""}`} key={record.id}>
                    <GameIcon assetKey={strategy ? graph?.base_state.base_key ?? "" : record.base} size="item" />
                    <div className="pc-stash-meta"><span className="pc-stash-name">{record.name} {corrupted && <ItemStateBadge state="corrupted">Corrupted</ItemStateBadge>}</span><span className="pc-stash-base">{detail}</span>
                        {!strategy && <span className="pc-item-heading">
                            {rarity && <span className={`pc-rarity pc-rarity-${rarity}`}>{rarity}</span>}
                            {!!(Number(state?.item_flags ?? 0) & 16) && <ItemStateBadge state="foreseeing">Foreseeing</ItemStateBadge>}
                            {!!Number(state?.memory_strands ?? 0) && <ItemStateBadge state="memory">Memory strands: {Number(state?.memory_strands)}</ItemStateBadge>}
                        </span>}
                        {!!influences.length && <span className="pc-item-influences">{influences.map(name => <InfluenceBadge key={name} name={name} />)}</span>}
                    </div>
                    <div className="pc-stash-actions">{entries.map(([action, label]) => <button key={action}
                        disabled={!!lifecycle && action !== "delete"}
                        title={lifecycle && action !== "delete" ? "Consumed or destroyed items cannot be opened or imported." : undefined}
                        onClick={() => void this.handle(action, record)}>{label}</button>)}</div>
                </div>;
            }) : <p className="pc-empty">{this.loading ? "" : this.error ? "Saved resources unavailable." : "No saved resources in this view."}</p>}</div>
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
            cluster: record.cluster,
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
