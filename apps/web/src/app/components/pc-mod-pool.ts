/*
 * pc-mod-pool — browse session-visible mods grouped into families with collapsible
 * tier lists. Reads the engine's candidate pool to show live weights and to
 * disable rows that aren't currently rollable. Used by the emulator to replace
 * the old debug weight table and the in-bar candidate-pool select.
 *
 * Mounted with `allow-direct-craft` to enable click-to-craft (force-add the mod
 * regardless of pool — intentional for the emulator's scratch use case). The
 * strategy builder will mount without that attribute later.
 */

import { ModInfo, PoolDebug } from "../engine-protocol";

type Tab = "prefix" | "suffix" | "implicit";
type Section = "base" | "influenced" | "crafted" | "essence" | "fossil";

interface PoolModel {
    mods: ModInfo[];           // session-mod metadata in session-mod-id order
    item: {
        rarity: string;
        prefixOnItem: Set<number>;
        suffixOnItem: Set<number>;
        implicitOnItem: Set<number>;
        groupOnItem: Set<number>;
        maxPrefix: number;
        maxSuffix: number;
    };
    pool: PoolDebug | null;     // current candidate pool (for the active tab/side)
    poolWeights: Map<number, number>; // session_mod_id -> final_weight
}

interface FamilyView {
    key: string;
    label: string;
    onItem: boolean;
    category: Section;
    sourceLabel: string;
    tags: string[];
    tiers: ModInfo[];
}

const REACH_KIND_BASE = 0;
const REACH_KIND_CRAFTED = 2;
const REACH_KIND_INFLUENCE = 1;
const REACH_KIND_ESSENCE = 3;
const REACH_KIND_BASE_IMPLICIT = 4;
const REACH_KIND_FOSSIL = 5;

export class PcModPool extends HTMLElement {
    private model: PoolModel | null = null;
    private allowDirectCraft = false;
    private tab: Tab = "prefix";
    private search = "";
    private sectionOpen: Record<Section, boolean> = {
        base: true,
        influenced: false,
        crafted: false,
        essence: false,
        fossil: false,
    };
    private expanded = new Set<string>();

    connectedCallback(): void {
        this.allowDirectCraft = this.hasAttribute("allow-direct-craft");
        this.renderShell();
    }

    setModel(model: PoolModel): void {
        this.model = model;
        this.render();
    }

    setActiveTab(tab: Tab): void {
        if (this.tab !== tab) {
            this.tab = tab;
            this.dispatchTabChange();
            this.render();
        }
    }

    getActiveTab(): Tab {
        return this.tab;
    }

    private dispatchTabChange(): void {
        this.dispatchEvent(
            new CustomEvent<{ tab: Tab }>("tab-change", {
                detail: { tab: this.tab },
                bubbles: true,
            }),
        );
    }

    private dispatchCraft(modKey: string, side: "prefix" | "suffix"): void {
        this.dispatchEvent(
            new CustomEvent<{ key: string; side: "prefix" | "suffix" }>(
                "craft-mod",
                { detail: { key: modKey, side }, bubbles: true },
            ),
        );
    }

    private renderShell(): void {
        this.innerHTML = `
            <div class="pc-mod-pool">
                <div class="pc-mod-pool-header">
                    <h3>Modifier Pool</h3>
                    ${
                        this.allowDirectCraft
                            ? '<span class="pc-mod-pool-hint">click a tier to craft it directly</span>'
                            : ""
                    }
                </div>
                <input class="pc-mod-pool-search" type="text" placeholder="Search mods, groups, stat text…" />
                <div class="pc-mod-pool-tabs" role="tablist">
                    <button data-tab="prefix" class="pc-tab is-active">Prefixes</button>
                    <button data-tab="suffix" class="pc-tab">Suffixes</button>
                    <button data-tab="implicit" class="pc-tab">Implicits</button>
                </div>
                <div class="pc-mod-pool-body"></div>
            </div>`;

        this.querySelector<HTMLInputElement>(".pc-mod-pool-search")!.addEventListener(
            "input",
            (event) => {
                this.search = (event.target as HTMLInputElement).value;
                this.renderBody();
            },
        );
        this.querySelectorAll<HTMLButtonElement>(".pc-tab").forEach((button) => {
            button.addEventListener("click", () => {
                const next = button.dataset.tab as Tab;
                this.setActiveTab(next);
            });
        });
    }

    private render(): void {
        this.querySelectorAll<HTMLButtonElement>(".pc-tab").forEach((button) => {
            button.classList.toggle("is-active", button.dataset.tab === this.tab);
        });
        this.renderBody();
    }

    private renderBody(): void {
        const root = this.querySelector(".pc-mod-pool-body");
        if (!root) return;
        if (!this.model) {
            root.innerHTML = '<p class="pc-empty">Loading mods…</p>';
            return;
        }
        const families = this.buildFamilies();
        const search = this.search.trim().toLowerCase();
        const matches = (f: FamilyView): boolean => {
            if (!search) return true;
            if (f.label.toLowerCase().includes(search)) return true;
            if (f.sourceLabel.toLowerCase().includes(search)) return true;
            if (f.tags.some((tag) => tag.toLowerCase().includes(search))) return true;
            return f.tiers.some((tier) =>
                tier.text_lines.some((line) => line.toLowerCase().includes(search)) ||
                tier.key.toLowerCase().includes(search),
            );
        };
        const filtered = families.filter(matches);

        if (this.tab === "implicit") {
            root.innerHTML = this.renderFamilyList(filtered);
            this.wireFamilyHandlers(root);
            return;
        }

        const inSection = (section: Section) =>
            filtered.filter((family) => family.category === section);

        root.innerHTML =
            this.renderSection("base", "Base Mod Pool", inSection("base")) +
            this.renderSection(
                "influenced",
                "Influenced Mod Pool",
                inSection("influenced"),
            ) +
            this.renderSection("crafted", "Crafted Mods", inSection("crafted")) +
            this.renderSection("essence", "Essence Mods", inSection("essence")) +
            this.renderSection("fossil", "Fossil Mods", inSection("fossil"));

        this.wireSectionHandlers(root);
        this.wireFamilyHandlers(root);
    }

    private buildFamilies(): FamilyView[] {
        if (!this.model) return [];
        const tabFilter = (info: ModInfo): boolean => {
            if (this.tab === "implicit") {
                // generation_type for implicit may be -1; engine returns
                // implicits via reach_kind = base-implicit (4).
                return (
                    info.reach_kind === REACH_KIND_BASE_IMPLICIT ||
                    info.generation_type === -1
                );
            }
            if (this.tab === "prefix") return info.generation_type === 0;
            return info.generation_type === 1;
        };

        const byFamily = new Map<
            string,
            { category: Section; familyId: number; tiers: ModInfo[] }
        >();
        for (const info of this.model.mods) {
            if (!tabFilter(info)) continue;
            const category =
                this.tab === "implicit" ? "base" : categoryFor(info.reach_kind);
            if (!category) continue;
            const familyId = Number.isFinite(info.family_id)
                ? info.family_id
                : info.primary_group_id;
            const influence =
                category === "influenced" ? `:${info.reach_influence}` : "";
            const key = `${category}${influence}:${familyId}`;
            const slot = byFamily.get(key) ?? {
                category,
                familyId,
                tiers: [],
            };
            slot.tiers.push(info);
            byFamily.set(key, slot);
        }
        const out: FamilyView[] = [];
        for (const [key, family] of byFamily) {
            const sorted = family.tiers
                .slice()
                .sort((a, b) => a.family_tier_index - b.family_tier_index);
            const first = sorted[0];
            const tags = Array.from(
                new Set(sorted.flatMap((tier) => tier.classification_tags)),
            ).sort();
            out.push({
                key,
                label:
                    first?.text_lines.join(" / ") ||
                    first?.key ||
                    `Family ${family.familyId}`,
                onItem: this.model.item.groupOnItem.has(
                    first?.primary_group_id ?? 0,
                ),
                category: family.category,
                sourceLabel: sourceLabel(first),
                tags,
                tiers: sorted,
            });
        }
        out.sort((a, b) => {
            if (a.onItem !== b.onItem) return a.onItem ? -1 : 1;
            return a.label.localeCompare(b.label);
        });
        return out;
    }

    private renderSection(
        key: Section,
        title: string,
        families: FamilyView[],
    ): string {
        const open = this.sectionOpen[key];
        return `
            <div class="pc-mod-pool-section">
                <button class="pc-mod-pool-section-toggle" data-section="${key}">
                    <span>${title}</span>
                    <span class="pc-mod-pool-count">${families.length}</span>
                    <span class="pc-chev">${open ? "▾" : "▸"}</span>
                </button>
                ${open ? this.renderFamilyList(families) : ""}
            </div>`;
    }

    private renderFamilyList(families: FamilyView[]): string {
        if (families.length === 0) {
            return '<p class="pc-empty">No matching mods.</p>';
        }
        return `<ul class="pc-mod-family-list">${families
            .map((f) => this.renderFamily(f))
            .join("")}</ul>`;
    }

    private renderFamily(family: FamilyView): string {
        const expanded = this.expanded.has(family.key);
        const tierRows = expanded
            ? family.tiers
                  .map((tier) => this.renderTier(family, tier))
                  .join("")
            : "";
        return `
            <li class="pc-mod-family ${family.onItem ? "is-on-item" : ""}" data-family-key="${escapeHtml(family.key)}">
                <button class="pc-mod-family-header">
                    <span class="pc-mod-family-copy">
                        <span class="pc-mod-family-name">${escapeHtml(family.label)}</span>
                        ${
                            family.tags.length > 0
                                ? `<span class="pc-mod-family-tags">${family.tags
                                      .map(
                                          (tag) =>
                                              `<span>${escapeHtml(formatTag(tag))}</span>`,
                                      )
                                      .join("")}</span>`
                                : ""
                        }
                    </span>
                    <span class="pc-mod-family-meta">
                        ${family.tiers.length} tier${family.tiers.length !== 1 ? "s" : ""}
                        ${
                            family.sourceLabel
                                ? `<span>${escapeHtml(family.sourceLabel)}</span>`
                                : ""
                        }
                        ${family.onItem ? '<span class="pc-mod-on-item">ON ITEM</span>' : ""}
                    </span>
                    <span class="pc-chev">${expanded ? "▾" : "▸"}</span>
                </button>
                ${expanded ? `<ul class="pc-mod-tier-list">${tierRows}</ul>` : ""}
            </li>`;
    }

    private renderTier(family: FamilyView, tier: ModInfo): string {
        if (!this.model) return "";
        const item = this.model.item;
        const weight = this.model.poolWeights.get(tier.session_mod_id);
        const text = tier.text_lines.join(" / ") || tier.key;
        let blockedReason: string | null = null;
        if (this.tab !== "implicit") {
            if (family.onItem) {
                blockedReason = "Group already on item";
            } else if (this.tab === "prefix") {
                const open =
                    item.prefixOnItem.size < item.maxPrefix && item.maxPrefix > 0;
                if (!open) blockedReason = "No open prefix slot";
            } else {
                const open =
                    item.suffixOnItem.size < item.maxSuffix && item.maxSuffix > 0;
                if (!open) blockedReason = "No open suffix slot";
            }
            if (!blockedReason && weight === undefined) {
                blockedReason = "Not currently rollable";
            }
        }
        const clickable = this.allowDirectCraft && this.tab !== "implicit" && !blockedReason;
        const tags = tier.classification_tags
            .map((tag) => `<span>${escapeHtml(formatTag(tag))}</span>`)
            .join("");
        return `
            <li class="pc-mod-tier ${blockedReason ? "is-blocked" : ""}"
                data-mod-key="${escapeHtml(tier.key)}"
                data-side="${this.tab}"
                ${blockedReason ? `title="${escapeHtml(blockedReason)}"` : ""}>
                <button class="pc-mod-tier-btn" ${clickable ? "" : "disabled"}>
                    <span class="pc-mod-tier-label">
                        <span class="pc-mod-tier-rank">T${tier.family_tier_index || "?"}</span>
                        <span class="pc-mod-tier-copy">
                            <span class="pc-mod-tier-text">${escapeHtml(text)}</span>
                            ${tags ? `<span class="pc-mod-tier-tags">${tags}</span>` : ""}
                        </span>
                    </span>
                    <span class="pc-mod-tier-meta">
                        <span>iLvl ${tier.required_level}</span>
                        ${
                            weight !== undefined
                                ? `<span class="pc-mod-tier-weight">${weight.toLocaleString()}</span>`
                                : '<span class="pc-mod-tier-weight pc-mod-tier-zero">—</span>'
                        }
                    </span>
                </button>
            </li>`;
    }

    private wireSectionHandlers(root: Element): void {
        root.querySelectorAll<HTMLButtonElement>(".pc-mod-pool-section-toggle")
            .forEach((button) => {
                button.addEventListener("click", () => {
                    const key = button.dataset.section as Section;
                    this.sectionOpen[key] = !this.sectionOpen[key];
                    this.renderBody();
                });
            });
    }

    private wireFamilyHandlers(root: Element): void {
        root.querySelectorAll<HTMLButtonElement>(".pc-mod-family-header")
            .forEach((button) => {
                button.addEventListener("click", () => {
                    const key =
                        (button.closest(".pc-mod-family") as HTMLElement).dataset
                            .familyKey ?? "";
                    if (this.expanded.has(key)) this.expanded.delete(key);
                    else this.expanded.add(key);
                    this.renderBody();
                });
            });
        root.querySelectorAll<HTMLLIElement>(".pc-mod-tier").forEach((li) => {
            const btn = li.querySelector<HTMLButtonElement>(".pc-mod-tier-btn");
            if (!btn || btn.disabled) return;
            btn.addEventListener("click", () => {
                const key = li.dataset.modKey ?? "";
                const side = li.dataset.side ?? "prefix";
                if (key && (side === "prefix" || side === "suffix")) {
                    this.dispatchCraft(key, side);
                }
            });
        });
    }
}

function categoryFor(reachKind: number): Section | null {
    if (reachKind === REACH_KIND_BASE) return "base";
    if (reachKind === REACH_KIND_INFLUENCE) return "influenced";
    if (reachKind === REACH_KIND_CRAFTED) return "crafted";
    if (reachKind === REACH_KIND_ESSENCE) return "essence";
    if (reachKind === REACH_KIND_FOSSIL) return "fossil";
    return null;
}

function sourceLabel(info: ModInfo | undefined): string {
    if (!info) return "";
    if (info.reach_kind === REACH_KIND_INFLUENCE) {
        const parts = info.reach_via.split(":");
        return formatTag(parts[parts.length - 1] || "Influenced");
    }
    if (info.reach_kind === REACH_KIND_CRAFTED) return "Bench";
    if (info.reach_kind === REACH_KIND_ESSENCE) return "Essence";
    if (info.reach_kind === REACH_KIND_FOSSIL) return "Fossil";
    return "";
}

function formatTag(tag: string): string {
    return tag
        .replace(/_/g, " ")
        .replace(/\b\w/g, (character) => character.toUpperCase());
}

function escapeHtml(text: string): string {
    return text
        .replace(/&/g, "&amp;")
        .replace(/</g, "&lt;")
        .replace(/>/g, "&gt;")
        .replace(/"/g, "&quot;");
}

customElements.define("pc-mod-pool", PcModPool);
