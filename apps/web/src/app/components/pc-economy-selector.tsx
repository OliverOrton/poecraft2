import { disconnectReact, renderReact } from "../react-host";
import {
    economyService,
    economyDeliveryMode,
    economyCompatibility,
    MANUAL_PROFILE,
    type EconomyStatus,
    type LeagueIndexEntry,
} from "../workspace/economy-service";

function escapeHtml(value: string): string {
    return value
        .replaceAll("&", "&amp;")
        .replaceAll("<", "&lt;")
        .replaceAll(">", "&gt;")
        .replaceAll('"', "&quot;");
}

function ageLabel(milliseconds: number | null, compact = false): string {
    if (milliseconds === null) return compact ? "manual" : "Manual prices only";
    const minutes = Math.max(0, Math.floor(milliseconds / 60_000));
    if (minutes < 60) return compact ? `${minutes}m` : `Updated ${minutes}m ago`;
    const hours = Math.floor(minutes / 60);
    if (hours < 48) return compact ? `${hours}h` : `Updated ${hours}h ago`;
    const days = Math.floor(hours / 24);
    return compact ? `${days}d` : `Updated ${days}d ago`;
}

function statusLabel(status: EconomyStatus): string {
    switch (status) {
        case "fresh":
            return "Published";
        case "stale":
            return "Stale";
        case "offline":
            return "Offline cache";
        case "manual-only":
            return "Manual only";
        default:
            return "Loading";
    }
}

export class PcEconomySelector extends HTMLElement {
    private open = false;
    private localError: string | null = null;
    private unsubscribe: (() => void) | null = null;
    private readonly outside = (event: Event): void => {
        if (this.open && !this.contains(event.target as Node)) {
            this.open = false;
            this.render();
        }
    };
    private readonly keydown = (event: KeyboardEvent): void => {
        if (event.key === "Escape" && this.open) {
            this.open = false;
            this.render();
            this.querySelector<HTMLButtonElement>(".pc-economy-trigger")?.focus();
        }
    };

    connectedCallback(): void {
        this.unsubscribe = economyService.onChange(() => this.render());
        document.addEventListener("pointerdown", this.outside);
        document.addEventListener("keydown", this.keydown);
        void economyService.initialize();
        this.render();
    }

    disconnectedCallback(): void {
        disconnectReact(this);
        this.unsubscribe?.();
        document.removeEventListener("pointerdown", this.outside);
        document.removeEventListener("keydown", this.keydown);
    }

    private render(): void {
        const state = economyService.getState();
        const selected = state.index?.leagues.find(
            (league) => league.league_key === state.selectedProfile,
        );
        const name = selected?.display_name ?? "Custom / manual";
        const age = ageLabel(economyService.sourceAgeMs(), true);
        const cutoff = state.sourceSnapshot?.metadata.source_cutoff_at_utc;
        const delivery = economyDeliveryMode() === 'bundled' ? 'Bundled snapshot' : 'Live delivery';
        const compatibility = state.sourceSnapshot ? economyCompatibility(state.sourceSnapshot) : null;
        const fullState = `${delivery} — prices as of ${cutoff ?? 'manual / unknown'} · ${statusLabel(state.status)}${compatibility === 'unqualified' ? ' · game-data pairing unqualified' : ''}`;
        const leagues = state.index?.leagues ?? [];
        const current = leagues.filter((league) => league.active && league.temporary);
        const permanent = leagues.filter((league) => league.active && !league.temporary);
        const archived = leagues.filter((league) => league.archived);
        const manualSelected = state.selectedProfile === MANUAL_PROFILE;
        const error = this.localError || state.error;
        const lowConfidence =
            state.sourceSnapshot?.metadata.low_confidence_keys.length ?? 0;
        const footerState = `${fullState}${lowConfidence ? ` · ${lowConfidence} low-confidence` : ""}`;


        const choose = (profile: string) => {
            void economyService.selectProfile(profile).then(() => {
                this.open = false; this.localError = null; this.render();
            }).catch((error: unknown) => { this.localError = error instanceof Error ? error.message : String(error); this.render(); });
        };
        const group = (label: string, entries: LeagueIndexEntry[]) => entries.length ? <div key={label}>
            <div className="pc-economy-group-label">{label}</div>
            {entries.map(league => <button key={league.league_key}
                className={"pc-economy-row" + (state.selectedProfile === league.league_key ? " is-selected" : "")}
                data-profile={league.league_key} role="menuitemradio" aria-checked={state.selectedProfile === league.league_key}
                disabled={state.switchingTo === league.league_key} onClick={() => choose(league.league_key)}>
                <span className="pc-economy-badge">{league.ruleset === "hardcore" ? "HC" : league.temporary ? "SC" : "Perm"}</span>
                <span>{league.display_name}</span>
                <span className="pc-economy-row-state">{state.switchingTo === league.league_key ? "Switching…" : state.selectedProfile === league.league_key ? "✓" : ""}</span>
            </button>)}
        </div> : null;
        renderReact(this, <>
            <button className="pc-economy-trigger" aria-haspopup="menu" aria-expanded={this.open} title={fullState}
                onClick={() => { this.open = !this.open; this.localError = null; this.render(); }}>
                <span className={"pc-economy-status is-" + state.status} aria-hidden="true" />
                <span>{name}</span>
                <span className="pc-economy-age">· {cutoff ? delivery + " · " + cutoff.slice(0,10) : age}</span>
                <span aria-hidden="true">⌄</span>
            </button>
            {this.open && <div className="pc-economy-popover" role="menu" aria-label="Economy league">
                {group("Current leagues", current)}{group("Permanent", permanent)}{group("Archived", archived)}
                <button className={"pc-economy-row" + (manualSelected ? " is-selected" : "")} data-profile={MANUAL_PROFILE}
                    role="menuitemradio" aria-checked={manualSelected} onClick={() => choose(MANUAL_PROFILE)}>
                    <span className="pc-economy-badge">Local</span><span>Custom / manual</span><span className="pc-economy-row-state">{manualSelected ? "✓" : ""}</span>
                </button>
                {error && <div className="pc-economy-error" role="status">{error}</div>}
                <div className="pc-economy-footer"><span>{footerState}</span>
                    <button data-refresh disabled={Boolean(state.switchingTo)} onClick={() => {
                        this.localError = null;
                        void economyService.refresh().catch((error: unknown) => {
                            this.localError = error instanceof Error ? error.message : String(error); this.render();
                        });
                    }}>Refresh</button>
                </div>
            </div>}
        </>);
    }
}
customElements.define("pc-economy-selector", PcEconomySelector);
