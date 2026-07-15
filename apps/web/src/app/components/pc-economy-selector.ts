import {
    economyService,
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
            return "Fresh";
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
        this.unsubscribe?.();
        document.removeEventListener("pointerdown", this.outside);
        document.removeEventListener("keydown", this.keydown);
    }

    private leagueRow(league: LeagueIndexEntry): string {
        const state = economyService.getState();
        const selected = state.selectedProfile === league.league_key;
        const switching = state.switchingTo === league.league_key;
        const badge = league.temporary
            ? league.ruleset === "hardcore"
                ? "HC"
                : "SC"
            : league.ruleset === "hardcore"
              ? "HC"
              : "Perm";
        return `<button
            class="pc-economy-row${selected ? " is-selected" : ""}"
            data-profile="${escapeHtml(league.league_key)}"
            role="menuitemradio"
            aria-checked="${selected}"
            ${switching ? "disabled" : ""}>
            <span class="pc-economy-badge">${badge}</span>
            <span>${escapeHtml(league.display_name)}</span>
            <span class="pc-economy-row-state">${switching ? "Switching…" : selected ? "✓" : ""}</span>
        </button>`;
    }

    private group(label: string, leagues: LeagueIndexEntry[]): string {
        if (!leagues.length) return "";
        return `<div class="pc-economy-group-label">${label}</div>
            ${leagues.map((league) => this.leagueRow(league)).join("")}`;
    }

    private render(): void {
        const state = economyService.getState();
        const selected = state.index?.leagues.find(
            (league) => league.league_key === state.selectedProfile,
        );
        const name = selected?.display_name ?? "Custom / manual";
        const age = ageLabel(economyService.sourceAgeMs(), true);
        const fullState = `${statusLabel(state.status)} · ${ageLabel(economyService.sourceAgeMs())}`;
        const leagues = state.index?.leagues ?? [];
        const current = leagues.filter((league) => league.active && league.temporary);
        const permanent = leagues.filter((league) => league.active && !league.temporary);
        const archived = leagues.filter((league) => league.archived);
        const manualSelected = state.selectedProfile === MANUAL_PROFILE;
        const error = this.localError || state.error;
        const lowConfidence =
            state.sourceSnapshot?.metadata.low_confidence_keys.length ?? 0;
        const footerState = `${fullState}${lowConfidence ? ` · ${lowConfidence} low-confidence` : ""}`;

        this.innerHTML = `
            <button class="pc-economy-trigger" aria-haspopup="menu" aria-expanded="${this.open}" title="${escapeHtml(fullState)}">
                <span class="pc-economy-status is-${state.status}" aria-hidden="true"></span>
                <span>${escapeHtml(name)}</span>
                <span class="pc-economy-age">· ${age}</span>
                <span aria-hidden="true">⌄</span>
            </button>
            ${
                this.open
                    ? `<div class="pc-economy-popover" role="menu" aria-label="Economy league">
                        ${this.group("Current leagues", current)}
                        ${this.group("Permanent", permanent)}
                        ${this.group("Archived", archived)}
                        <button class="pc-economy-row${manualSelected ? " is-selected" : ""}" data-profile="${MANUAL_PROFILE}" role="menuitemradio" aria-checked="${manualSelected}">
                            <span class="pc-economy-badge">Local</span>
                            <span>Custom / manual</span>
                            <span class="pc-economy-row-state">${manualSelected ? "✓" : ""}</span>
                        </button>
                        ${error ? `<div class="pc-economy-error" role="status">${escapeHtml(error)}</div>` : ""}
                        <div class="pc-economy-footer">
                            <span>${escapeHtml(footerState)}</span>
                            <button data-refresh ${state.switchingTo ? "disabled" : ""}>Refresh</button>
                        </div>
                    </div>`
                    : ""
            }`;

        this.querySelector<HTMLButtonElement>(".pc-economy-trigger")?.addEventListener(
            "click",
            () => {
                this.open = !this.open;
                this.localError = null;
                this.render();
            },
        );
        this.querySelectorAll<HTMLButtonElement>("[data-profile]").forEach((button) => {
            button.addEventListener("click", () => {
                const profile = button.dataset.profile!;
                void economyService
                    .selectProfile(profile)
                    .then(() => {
                        this.open = false;
                        this.localError = null;
                        this.render();
                    })
                    .catch((error: unknown) => {
                        this.localError =
                            error instanceof Error ? error.message : String(error);
                        this.render();
                    });
            });
        });
        this.querySelector<HTMLButtonElement>("[data-refresh]")?.addEventListener(
            "click",
            () => {
                this.localError = null;
                void economyService.refresh().catch((error: unknown) => {
                    this.localError =
                        error instanceof Error ? error.message : String(error);
                    this.render();
                });
            },
        );
    }
}

customElements.define("pc-economy-selector", PcEconomySelector);
