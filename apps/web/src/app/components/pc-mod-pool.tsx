import type { ReactNode } from "react";
import { disconnectReact, renderReact } from "../react-host";
import { modTextLabel } from "../mod-text";
/*
 * pc-mod-pool — browse session-visible mods grouped into families with collapsible
 * tier lists. Reads the engine's candidate pool to show live weights and to
 * disable rows that aren't currently rollable. Used by the emulator to replace
 * the old debug weight table and the in-bar candidate-pool select.
 *
 * Mounted with `allow-direct-craft` to enable click-to-craft (force-add the mod
 * regardless of pool — intentional for the emulator's scratch use case). The
 * strategy builder will mount without that attribute later.
 *
 * Mounted with `select-goal` (the Calculator's goal editor) every explicit
 * tier is clickable regardless of the current item — a goal mod need not be
 * addable right now — and clicks always emit `craft-mod` (the host treats it
 * as "require this tier or better"); remove/fracture gestures are disabled.
 */

import { ModInfo, PoolDebug } from "../engine-protocol";
import { visibleModTags } from "../item-display";
import {
    genericInfluenceDisplayName,
    genericInfluenceDisplayRank,
} from "../influence-presentation";

type Tab = "prefix" | "suffix" | "implicit" | "enchantment";
type Section = "base" | "influenced" | "crafted" | "essence" | "fossil" | "veiled" | "unveiled";
export type ModPoolMode = "inspect" | "direct" | "goal";

interface PoolModel {
    mods: ModInfo[];           // session-mod metadata in session-mod-id order
    item: {
        rarity: string;
        prefixOnItem: Set<number>;
        suffixOnItem: Set<number>;
        implicitOnItem: Set<number>;
        enchantmentOnItem: Set<number>;
        fracturedPrefixOnItem: Set<number>;
        fracturedSuffixOnItem: Set<number>;
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
    selectedTier: number | undefined;
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
const REACH_KIND_VEILED = 6;
const REACH_KIND_UNVEILED = 7;
const REACH_KIND_ENCHANTMENT = 12;

export class PcModPool extends HTMLElement {
    private model: PoolModel | null = null;
    private allowDirectCraft = false;
    private selectMode = false;
    private mode: ModPoolMode = "inspect";
    private tab: Tab = "prefix";
    private search = "";
    /** Host-authored goal thresholds, keyed by the persisted representative
     * family mod key. Item selections are derived from PoolModel instead. */
    private selectedTiers = new Map<string, number>();
    private sectionOpen: Record<string, boolean> = {
        base: true,
        crafted: false,
        essence: false,
        fossil: false,
    };
    private expanded = new Set<string>();

    connectedCallback(): void {
        this.mode = this.hasAttribute("allow-direct-craft")
            ? "direct"
            : this.hasAttribute("select-goal")
              ? "goal"
              : "inspect";
        this.allowDirectCraft = this.mode === "direct";
        this.selectMode = this.mode === "goal";
        this.renderShell();
        if (this.model) {
            // Dockview detaches inactive panels and re-attaches them on tab
            // switch; restore the body from the retained model instead of
            // showing "Loading" until the host's next setModel.
            this.render();
        }
    }

    setModel(model: PoolModel): void {
        this.model = model;
        this.render();
    }

    setSelectedTiers(
        selections: ReadonlyArray<{
            familyModKey: string;
            minTier: number;
        }>,
    ): void {
        this.selectedTiers = new Map(
            selections.map((selection) => [
                selection.familyModKey,
                selection.minTier,
            ]),
        );
        this.render();
    }

    setInteractionMode(mode: ModPoolMode): void {
        if (this.mode === mode) return;
        this.mode = mode;
        this.allowDirectCraft = mode === "direct";
        this.selectMode = mode === "goal";
        if (this.isConnected) {
            this.renderShell();
            this.render();
        }
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
            new CustomEvent<{
                key: string;
                side: "prefix" | "suffix";
                fractured?: boolean;
            }>(
                "craft-mod",
                { detail: { key: modKey, side }, bubbles: true },
            ),
        );
    }

    private dispatchFracture(
        modKey: string,
        modId: number,
        side: "prefix" | "suffix",
        onItem: boolean,
    ): void {
        this.dispatchEvent(
            new CustomEvent<{
                key: string;
                modId: number;
                side: "prefix" | "suffix";
                onItem: boolean;
            }>("fracture-mod", {
                detail: { key: modKey, modId, side, onItem },
                bubbles: true,
            }),
        );
    }

    private dispatchRemove(
        modId: number,
        side: "prefix" | "suffix",
    ): void {
        this.dispatchEvent(
            new CustomEvent<{ modId: number; side: "prefix" | "suffix" }>(
                "remove-mod",
                { detail: { modId, side }, bubbles: true },
            ),
        );
    }


    private renderShell(): void { this.render(); }
    private render(): void {
        renderReact(this, <div className="pc-mod-pool">
            <div className="pc-mod-pool-header">
                <h3>Modifier Pool <span>· {this.mode === "goal" ? "Editing goal" : this.mode === "direct" ? "Editing input item" : "Viewing item"}</span></h3>
                {this.allowDirectCraft ? <span className="pc-mod-pool-hint">click to add · click highlighted tier to remove</span>
                    : this.selectMode ? <span className="pc-mod-pool-hint">choose a tier or better</span> : null}
            </div>
            <input className="pc-mod-pool-search" type="search" aria-label="Search modifier pool" placeholder="Search mods, groups, stat text…"
                value={this.search} onChange={event => { this.search = event.target.value; this.render(); }} />
            <div className="pc-mod-pool-tabs" role="tablist" aria-label="Modifier pool side">
                {(["prefix", "suffix", "implicit", "enchantment"] as const).map(tab => <button key={tab} role="tab" aria-selected={this.tab === tab}
                    data-tab={tab} className={"pc-tab " + (this.tab === tab ? "is-active" : "")}
                    onClick={() => this.setActiveTab(tab)}>{tab === "prefix" ? "Prefixes" : tab === "suffix" ? "Suffixes" : tab === "implicit" ? "Implicits" : "Enchantments"}</button>)}
            </div>
            <div className="pc-mod-pool-body">{this.renderBody()}</div>
        </div>);
    }
    disconnectedCallback(): void { disconnectReact(this); }

    private renderBody(): ReactNode {
        if (!this.model) return <p className="pc-empty">Loading mods…</p>;
        const search = this.search.trim().toLowerCase();
        const filtered = this.buildFamilies().filter(family => !search ||
            [family.label, family.sourceLabel, ...family.tags, ...family.tiers.flatMap(tier => [modTextLabel(tier.text_lines), tier.key])]
                .some(value => value.toLowerCase().includes(search)));
        if (this.tab === "implicit" || this.tab === "enchantment") return this.renderFamilyList(filtered);
        const inSection = (section: Section) => filtered.filter(family => family.category === section);
        const groups = new Map<string, FamilyView[]>();
        for (const family of inSection("influenced")) {
            const label = family.sourceLabel || "Influenced";
            groups.set(label, [...(groups.get(label) ?? []), family]);
        }
        return <>
            {this.renderSection("base", "Base Mod Pool", inSection("base"))}
            {[...groups].sort(([a], [b]) => influenceOrder(a) - influenceOrder(b) || a.localeCompare(b))
                .map(([label, entries]) => this.renderSection("influence:" + label, label + " Mods", entries))}
            {this.renderSection("crafted", "Crafted Mods", inSection("crafted"))}
            {this.renderSection("essence", "Essence Mods", inSection("essence"))}
            {this.renderSection("fossil", "Fossil Mods", inSection("fossil"))}
            {this.renderSection("veiled", "Veiled Mods", inSection("veiled"))}
            {this.renderSection("unveiled", "Unveiled Mods", inSection("unveiled"))}
        </>;
    }

    private buildFamilies(): FamilyView[] {
        if (!this.model) return [];
        const tabFilter = (info: ModInfo): boolean => {
            if (info.reach_kind === REACH_KIND_ENCHANTMENT) return this.tab === "enchantment";
            if (this.tab === "enchantment") return false;
            if (this.tab === "implicit") return info.reach_kind === REACH_KIND_BASE_IMPLICIT || info.generation_type === -1;
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
                (this.tab === "implicit" || this.tab === "enchantment") ? "base" : categoryFor(info.reach_kind);
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
            const tags = visibleModTags(
                sorted.flatMap((tier) => tier.classification_tags),
            ).sort();
            const selectedThreshold = sorted
                .map((tier) => this.selectedTiers.get(tier.key))
                .find((tier) => tier !== undefined);
            const itemTier = sorted.find((tier) =>
                this.tab === "prefix"
                    ? this.model?.item.prefixOnItem.has(tier.session_mod_id)
                    : this.tab === "suffix"
                      ? this.model?.item.suffixOnItem.has(tier.session_mod_id)
                      : (this.tab === "enchantment" ? this.model?.item.enchantmentOnItem : this.model?.item.implicitOnItem)?.has(
                            tier.session_mod_id,
                        ),
            );
            out.push({
                key,
                label:
                    modTextLabel(first?.text_lines ?? []) ||
                    first?.key ||
                    `Family ${family.familyId}`,
                onItem: sorted.some((tier) =>
                    this.tab === "prefix"
                        ? this.model?.item.prefixOnItem.has(tier.session_mod_id)
                        : this.tab === "suffix"
                          ? this.model?.item.suffixOnItem.has(tier.session_mod_id)
                          : (this.tab === "enchantment" ? this.model?.item.enchantmentOnItem : this.model?.item.implicitOnItem)?.has(
                                tier.session_mod_id,
                            ),
                ),
                selectedTier:
                    this.selectMode
                        ? selectedThreshold
                        : itemTier?.family_tier_index,
                category: family.category,
                sourceLabel: sourceLabel(first),
                tags,
                tiers: sorted,
            });
        }
        out.sort((a, b) => a.label.localeCompare(b.label));
        return out;
    }


    private renderSection(key: string, title: string, families: FamilyView[]): ReactNode {
        const open = this.sectionOpen[key] ?? false;
        return <div key={key} className="pc-mod-pool-section">
            <button className="pc-mod-pool-section-toggle" data-section={key} aria-expanded={open}
                onClick={() => { this.sectionOpen[key] = !open; this.render(); }}>
                <span>{title}</span><span className="pc-mod-pool-count">{families.length}</span><span className="pc-chev">{open ? "▾" : "▸"}</span>
            </button>
            {open && this.renderFamilyList(families)}
        </div>;
    }
    private renderFamilyList(families: FamilyView[]): ReactNode {
        if (!families.length) return <p className="pc-empty">No matching mods.</p>;
        return <ul className="pc-mod-family-list">{families.map(family => this.renderFamily(family))}</ul>;
    }
    private renderFamily(family: FamilyView): ReactNode {
        const expanded = this.expanded.has(family.key);
        return <li key={family.key} className={`pc-mod-family ${family.onItem ? "is-on-item" : ""} ${family.selectedTier !== undefined ? "is-selected" : ""}`}
            data-family-key={family.key}>
            <button className="pc-mod-family-header" aria-expanded={expanded} onClick={() => {
                if (expanded) this.expanded.delete(family.key); else this.expanded.add(family.key);
                this.render();
            }}>
                <span className="pc-mod-family-copy"><span className="pc-mod-family-name">{family.label}</span>
                    {!!family.tags.length && <span className="pc-mod-family-tags">
                        {family.tags.map(tag => <span key={tag}>{formatTag(tag)}</span>)}
                    </span>}
                </span>
                <span className="pc-mod-family-meta">
                    {family.selectedTier !== undefined && <span className="pc-mod-selected-tier">{family.selectedTier > 0 ? "T" + family.selectedTier : "ANY TIER"}</span>}
                    {family.tiers.length} tier{family.tiers.length !== 1 ? "s" : ""}
                    {family.sourceLabel && <span>{family.sourceLabel}</span>}
                    {family.onItem && <span className="pc-mod-on-item">ON ITEM</span>}
                </span>
                <span className="pc-chev">{expanded ? "▾" : "▸"}</span>
            </button>
            {expanded && <ul className="pc-mod-tier-list">{family.tiers.map(tier => this.renderTier(family, tier))}</ul>}
        </li>;
    }

    private renderTier(family: FamilyView, tier: ModInfo): ReactNode {
        if (!this.model) return "";
        const item = this.model.item;
        const weight = this.model.poolWeights.get(tier.session_mod_id);
        const text = modTextLabel(tier.text_lines) || tier.key;
        const tierOnItem =
            this.tab === "prefix"
                ? item.prefixOnItem.has(tier.session_mod_id)
                : this.tab === "suffix"
                  ? item.suffixOnItem.has(tier.session_mod_id)
                  : (this.tab === "enchantment" ? item.enchantmentOnItem : item.implicitOnItem).has(tier.session_mod_id);
        const conflictingGroupOnItem = item.groupOnItem.has(
            tier.primary_group_id,
        );
        const tierFractured =
            this.tab === "prefix"
                ? item.fracturedPrefixOnItem.has(tier.session_mod_id)
                : this.tab === "suffix"
                  ? item.fracturedSuffixOnItem.has(tier.session_mod_id)
                  : false;
        let blockedReason: string | null = null;
        if ((this.tab === "prefix" || this.tab === "suffix") && !this.selectMode) {
            if (family.onItem && !tierOnItem) {
                blockedReason = "Another tier from this family is on the item";
            } else if (!family.onItem && conflictingGroupOnItem) {
                blockedReason = "A conflicting modifier group is on the item";
            } else if (this.tab === "prefix") {
                const open =
                    item.prefixOnItem.size < item.maxPrefix && item.maxPrefix > 0;
                if (!tierOnItem && !open) blockedReason = "No open prefix slot";
            } else {
                const open =
                    item.suffixOnItem.size < item.maxSuffix && item.maxSuffix > 0;
                if (!tierOnItem && !open) blockedReason = "No open suffix slot";
            }
            if (
                !blockedReason &&
                !tierOnItem &&
                weight === undefined &&
                family.category !== "crafted"
            ) {
                blockedReason = "Not currently rollable";
            }
        }
        const clickable =
            (this.allowDirectCraft || this.selectMode) &&
            (this.tab === "prefix" || this.tab === "suffix") &&
            !blockedReason;

        const tags = visibleModTags(tier.classification_tags);
        const tierSelected = family.selectedTier !== undefined && family.selectedTier === tier.family_tier_index;
        return <li key={tier.session_mod_id}
            className={`pc-mod-tier ${blockedReason ? "is-blocked" : ""} ${tierOnItem ? "is-on-item" : ""} ${tierFractured ? "is-fractured" : ""} ${tierSelected ? "is-selected" : ""}`}
            data-mod-key={tier.key} data-mod-id={tier.session_mod_id} data-side={this.tab} data-on-item={tierOnItem}
            data-fractured={tierFractured} data-can-fracture={family.category !== "crafted"} title={blockedReason ?? undefined}
            onContextMenu={event => {
                if (!clickable || this.selectMode || tierFractured || family.category === "crafted" || (this.tab !== "prefix" && this.tab !== "suffix")) return;
                event.preventDefault();
                this.dispatchFracture(tier.key, tier.session_mod_id, this.tab, tierOnItem);
            }}>
            <button className="pc-mod-tier-btn" disabled={!clickable} onClick={() => {
                if (!clickable || (this.tab !== "prefix" && this.tab !== "suffix")) return;
                if (!this.selectMode && tierOnItem) this.dispatchRemove(tier.session_mod_id, this.tab);
                else this.dispatchCraft(tier.key, this.tab);
            }}>
                <span className="pc-mod-tier-label">
                    <span className="pc-mod-tier-rank">T{tier.family_tier_index || "?"}</span>
                    <span className="pc-mod-tier-copy"><span className="pc-mod-tier-text">{text}</span>
                        {!!tags.length && <span className="pc-mod-tier-tags">{tags.map(tag => <span key={tag}>{formatTag(tag)}</span>)}</span>}
                    </span>
                </span>
                <span className="pc-mod-tier-meta"><span>iLvl {tier.required_level}</span>
                    {tierOnItem && !this.selectMode
                        ? tierFractured ? <span className="pc-mod-tier-fractured">FRACTURED</span> : <span className="pc-mod-tier-remove">{clickable ? "REMOVE" : "ON ITEM"}</span>
                        : weight !== undefined ? <span className="pc-mod-tier-weight">{weight.toLocaleString()}</span>
                        : <span className="pc-mod-tier-weight pc-mod-tier-zero">—</span>}
                </span>
            </button>
        </li>;
    }
}
function influenceOrder(label: string): number {
    return genericInfluenceDisplayRank(label);
}

function categoryFor(reachKind: number): Section | null {
    if (reachKind === REACH_KIND_BASE) return "base";
    if (reachKind === 10 || reachKind === 11) return "influenced";
    if (reachKind === REACH_KIND_INFLUENCE) return "influenced";
    if (reachKind === REACH_KIND_CRAFTED) return "crafted";
    if (reachKind === REACH_KIND_ESSENCE) return "essence";
    if (reachKind === REACH_KIND_FOSSIL) return "fossil";
    if (reachKind === REACH_KIND_VEILED) return "veiled";
    if (reachKind === REACH_KIND_UNVEILED) return "unveiled";
    return null;
}

function sourceLabel(info: ModInfo | undefined): string {
    if (!info) return "";
    if (info.reach_kind === REACH_KIND_ENCHANTMENT) return "Enchantment";
    if (info.reach_kind === 10) return "Retained influence (above item level)";
    if (info.reach_kind === 11) return "Elevated (retained)";
    if (info.reach_kind === REACH_KIND_INFLUENCE) {
        const parts = info.reach_via.split(":");
        return influenceLabel(parts[parts.length - 1] || "Influenced");
    }
    if (info.reach_kind === REACH_KIND_CRAFTED) return "Bench";
    if (info.reach_kind === REACH_KIND_ESSENCE) return "Essence";
    if (info.reach_kind === REACH_KIND_FOSSIL) return "Fossil";
    if (info.reach_kind === REACH_KIND_VEILED) return "Veiled";
    if (info.reach_kind === REACH_KIND_UNVEILED) return "Unveiled";
    return "";
}

function influenceLabel(value: string): string {
    return genericInfluenceDisplayName(value);
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
