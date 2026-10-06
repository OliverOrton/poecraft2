import {ControllerElement, disposeReact, renderReact} from "../react-host";
import {getEngine} from "../engine-service";
import type {EngineClient} from "../engine-client";
import type {BaseInfo, CalcResult, Catalog, ItemEdit, ModInfo} from "../engine-protocol";
import {CalculatorRequestLifetime, calculatorItemGoal, newCalculatorGoal, validateCalculatorGoalList,
    type CalculatorGoalDraft} from "../calculator-goal-set";
import {buildCalculatorTargetModel} from "../calculator-goal-model";
import {buildModifierKeyIndex, buildModifierOptions} from "../modifier-options";
import {concreteItemFacts} from "../item-card-model";
import {calculateAuthoredRecombination, recombinationOdds} from "../recombination-calculator";
import {getRecombinationDraft, putRecombinationDraft, type ItemSnapshot} from "../workspace/persistence";
import {workspace} from "../workspace/registry";
import {PcModList, type ConcreteModListModel, type ItemPropertyChange, type SlotMod} from "./pc-mod-list";
import {PcModPool} from "./pc-mod-pool";
import {PcBasePicker, type BasePickerSelection} from "./pc-base-picker";

type Input = "a" | "b";
type Focus = Input | "goal";
interface AuthoredInput {
    snapshot: ItemSnapshot;
    session: number;
    item: number;
    mods: ModInfo[];
    info: Record<string, unknown>;
    card: ConcreteModListModel;
}
const names = {a: "Input A", b: "Input B", goal: "Goal result"};
const percent = (value: number) => `${(value * 100).toLocaleString(undefined, {maximumFractionDigits: 4})}%`;

function RecombinationShell({id}: {id: string}) {
    const input = (side: Input) => <section className={`pc-recomb-input pc-recomb-${side}`} data-recomb-card={side} aria-label={names[side]}>
        <header className="pc-recomb-card-heading"><h3>{names[side]}</h3><button data-recomb-focus={side} aria-pressed="false">Edit {side.toUpperCase()}</button></header>
        <div className="pc-recomb-input-actions"><button data-recomb-base={side}>Change base</button><button data-recomb-clear={side}>Clear mods</button></div>
        <ControllerElement tag="pc-mod-list" data-recomb-item={side} />
    </section>;
    return <div className="pc-recombination-calculator">
        <header className="pc-craft-bar"><span className="pc-calc-workbench-title">Recombination</span><span>Combine two authored items</span></header>
        <p className="pc-help pc-recomb-intro">Random recombination keeps either input’s base and carrier properties. Each attempt consumes both items. This calculator leaves your inputs editable.</p>
        <div className="pc-recomb-layout">
            {input("a")}
            <div className="pc-recomb-middle">
                <section className="pc-recomb-goal" data-recomb-card="goal" aria-label="Goal result">
                    <header className="pc-recomb-card-heading"><h3>Goal result</h3><button data-recomb-focus="goal" aria-pressed="true">Edit goal</button></header>
                    <div className="pc-recomb-goal-controls">
                        <label><input type="checkbox" data-recomb-base-care /> Result base matters</label>
                        <label className="pc-recomb-required-base">Required base <select data-recomb-required-base aria-label="Required result base"><option value="a">Input A’s base</option><option value="b">Input B’s base</option></select></label>
                        <label><input type="checkbox" data-recomb-extras defaultChecked /> Allow extra explicit modifiers</label>
                        <label>Success means <select data-recomb-threshold aria-label="Required goal modifier count" /></label>
                    </div>
                    <ControllerElement tag="pc-mod-list" data-recomb-item="goal" />
                    <p className="pc-help">Select modifier tiers or better. All selected implicits and item properties are required. No requirements means any supported rare result matches.</p>
                </section>
                <section className="pc-recomb-picker" aria-label="Shared modifier selector">
                    <nav className="pc-recomb-focus-tabs" aria-label="Item to edit">{(["a", "goal", "b"] as Focus[]).map(focus =>
                        <button key={focus} data-recomb-focus={focus} aria-pressed={focus === "goal"}>{names[focus]}</button>)}</nav>
                    <p className="pc-recomb-focus-label" aria-live="polite" />
                    <ControllerElement tag="pc-mod-pool" select-goal="" />
                </section>
            </div>
            {input("b")}
            <section className="pc-recomb-odds" aria-label="Recombination odds">
            <header className="pc-recomb-card-heading"><h3>Odds</h3><div><button className="pc-button-primary" data-recomb-calculate>Calculate odds</button><button data-recomb-cancel hidden>Cancel</button></div></header>
                <div data-recomb-output aria-live="polite" />
                <details className="pc-recomb-assumptions"><summary>Model and supported scope</summary>
                    <p>Estimated game odds from the native random recombination model. Modifier selection uses provisional spawn weights. Selected tiers and recorded rolls are preserved; unverified upgrades are omitted.</p>
                    <p>Supports rare ordinary equipment of the same item class. Fractured or special explicit modifiers, generic influence, corruption, mirroring, foresight, jewel and cluster capacities, unresolved cross-side exclusions and the exceptional one-prefix / one-suffix pair are refused by the engine.</p>
                    <p>Goal odds observe explicit structure, selected implicits and represented item properties. Numerical rolls, memory strands, sockets, enchantments and defence percentiles are unobserved. Gold and dust costs are unknown; acquisition and retry costs are excluded.</p>
                </details>
            </section>
        </div>
        <p className="pc-recomb-error" role="alert" hidden />
        <div className="pc-recomb-base-overlay" hidden><section className="pc-recomb-base-dialog" role="dialog" aria-modal="true" aria-labelledby={`${id}-base-title`} tabIndex={-1}>
            <header className="pc-recomb-card-heading"><h3 className="pc-recomb-base-title" id={`${id}-base-title`}>Choose input base</h3><button data-recomb-close-picker>Cancel</button></header>
            <ControllerElement tag="pc-base-picker" />
        </section></div>
    </div>;
}

export class PcRecombinationCalculator extends HTMLElement {
    private client!: EngineClient;
    private data = 0;
    private catalog: Catalog | null = null;
    private bases: BaseInfo[] = [];
    private inputs = new Map<Input, AuthoredInput>();
    private goal: CalculatorGoalDraft = {...newCalculatorGoal("recomb-goal", "Goal result"), allowExtraModifiers: true};
    private focus: Focus = "goal";
    private baseCare = false;
    private requiredBase: Input = "a";
    private pickerSide: Input | null = null;
    private returnFocus: HTMLElement | null = null;
    private lifetime = new CalculatorRequestLifetime();
    private result: CalcResult | null = null;
    private oddsNotice = "Choose your inputs and goal, then calculate odds.";
    private error = "";
    private busy = true;
    private calculating = false;
    private started = false;
    private disposed = false;
    private docId = "";
    private currentWork: Promise<void> | null = null;
    private calculations = new Set<Promise<void>>();

    connectedCallback(): void {
        if (this.started) return;
        this.started = true;
        this.docId = this.getAttribute("doc-id") ?? `doc-${crypto.randomUUID()}`;
        renderReact(this, <RecombinationShell id={this.docId} />);
        this.bind();
        workspace().registerDocument(this.docId, {save: async () => true, dispose: () => this.dispose()});
        this.currentWork = this.initialize().catch(error => { this.error = String(error instanceof Error ? error.message : error); })
            .finally(() => {this.busy = false; if (!this.disposed) this.render();});
        this.render();
    }

    private async initialize(): Promise<void> {
        const engine = await getEngine();
        this.client = engine.client; this.data = engine.dataId;
        if (this.disposed) return;
        this.catalog = await this.client.catalog(this.data);
        this.bases = (await this.client.listBases(this.data)).filter(base => base.support === 0);
        const draft = await getRecombinationDraft(this.docId);
        if (this.disposed) return;
        if (draft) {
            if (draft.version !== "recombination_calculator_v1") throw new Error("Unsupported recombination draft version.");
            validateCalculatorGoalList({version: "calculator_goal_list_v1", activeGoalId: draft.goal.id, goals: [draft.goal]});
            this.goal = structuredClone(draft.goal);
            this.baseCare = draft.baseCare; this.requiredBase = draft.requiredBase;
        }
        const base = this.bases.find(base => base.path === "Metadata/Items/Armours/BodyArmours/BodyInt17") ?? this.bases[0];
        if (!base) throw new Error("No ordinary equipment bases are available.");
        for (const side of ["a", "b"] as const) {
            if (this.disposed) return;
            const snapshot = draft?.inputs[side] ?? {base: base.path, itemLevel: 86, state: null};
            this.inputs.set(side, await this.openInput(snapshot));
        }
        if (!this.disposed) await this.persist();
    }

    private async openInput(snapshot: ItemSnapshot): Promise<AuthoredInput> {
        const session = await this.client.createSession(this.data, snapshot.base, snapshot.itemLevel, snapshot.cluster);
        let item = 0;
        try {
            item = snapshot.state ? await this.client.importItem(snapshot.state, session) :
                await this.client.createItem(session, {rarity: "rare", withImplicits: true});
            const count = await this.client.modCount(session);
            const mods = await Promise.all(Array.from({length: count}, (_, id) => this.client.modInfo(session, id)));
            const input = {snapshot, session, item, mods, info: {}, card: {} as ConcreteModListModel};
            await this.refreshInput(input);
            return input;
        } catch (error) {
            try {if (item) await this.client.closeItem(item);} finally {await this.client.closeSession(session);}
            throw error;
        }
    }

    private async refreshInput(input: AuthoredInput): Promise<void> {
        const info = await this.client.itemInfo(input.item, input.session);
        const state = await this.client.exportItem(input.item, input.session);
        input.info = info;
        input.snapshot = {...input.snapshot, rarity: String(info.rarity), state};
        const fractured = new Set([...(info.fractured_prefix_mod_ids as number[]), ...(info.fractured_suffix_mod_ids as number[])]);
        const slots = (key: string): SlotMod[] => ((info[`${key === "prefixes" ? "prefix" : key === "suffixes" ? "suffix" : key === "implicits" ? "implicit" : "enchantment"}_mod_ids`] as number[]) ?? []).map((id, index) => {
            const mod = input.mods[id];
            return {sessionModId: id, key: mod.key, tierIndex: mod.family_tier_index, textLines: mod.text_lines,
                classificationTags: mod.classification_tags, fractured: fractured.has(id), crafted: mod.reach_kind === 2,
                veiled: mod.reach_kind === 6, rollValues: (state as Record<string, Array<{rolls?: number[]}>>)?.[key]?.[index]?.rolls};
        });
        input.card = {...concreteItemFacts(info, this.catalog, {baseKey: input.snapshot.base,
            baseName: this.baseName(input.snapshot.base), itemLevel: input.snapshot.itemLevel}),
            properties: {influences: this.catalog?.genericInfluences ?? [], influenceBits: Number(info.generic_influence_bits), corrupted: Boolean(Number(info.item_flags) & 1)},
            prefixes: slots("prefixes"), suffixes: slots("suffixes"), implicits: slots("implicits"), enchantments: slots("enchantments")};
    }

    private baseName(base: string): string {return this.bases.find(entry => entry.path === base)?.name ?? base;}
    private list(focus: Focus): PcModList | null {return this.querySelector<PcModList>(`[data-recomb-item="${focus}"]`);}
    private get pool(): PcModPool {return this.querySelector<PcModPool>("pc-mod-pool")!;}
    private identity(): string {
        return JSON.stringify([[this.inputs.get("a")?.snapshot, this.inputs.get("b")?.snapshot], calculatorItemGoal(this.goal),
            this.baseCare ? this.inputs.get(this.requiredBase)?.snapshot.base : null]);
    }
    private invalidate(): void {
        this.lifetime.invalidate(); this.result = null; this.calculating = false;
        this.oddsNotice = "Inputs or goal changed. Calculate updated odds.";
        this.renderOdds();
    }

    private edit(task: () => Promise<void>): void {
        if (this.busy || this.disposed) return;
        this.invalidate(); this.error = ""; this.busy = true; this.render();
        this.currentWork = task().then(() => this.persist()).catch(error => {
            this.error = error instanceof Error ? error.message : String(error);
        }).finally(() => {this.busy = false; if (!this.disposed) this.render();});
    }
    private goalEdit(task: () => void): void {
        this.edit(async () => {task();});
    }
    private select(focus: Focus, side?: "prefix" | "suffix" | "implicit"): void {
        this.focus = focus;
        this.renderFocus(); this.renderPool();
        if (side) {this.pool.setActiveTab(side); this.pool.querySelector<HTMLInputElement>("input")?.focus();}
    }
    private bind(): void {
        this.addEventListener("click", event => {
            const button = (event.target as HTMLElement).closest<HTMLButtonElement>("button");
            if (!button || button.disabled) return;
            if (button.dataset.recombFocus) this.select(button.dataset.recombFocus as Focus);
            if (button.dataset.recombBase) this.openPicker(button.dataset.recombBase as Input, button);
            if (button.hasAttribute("data-recomb-close-picker")) this.closePicker();
            if (button.dataset.recombClear) {
                const side = button.dataset.recombClear as Input;
                this.edit(async () => {
                    const old = this.inputs.get(side)!;
                    const replacement = await this.openInput({...old.snapshot, state: null});
                    this.inputs.set(side, replacement); await this.closeInput(old);
                });
            }
            if (button.hasAttribute("data-recomb-calculate")) this.calculate();
            if (button.hasAttribute("data-recomb-cancel")) {
                this.invalidate(); this.oddsNotice = "Calculation cancelled. Pending native work will finish in the background."; this.renderOdds();
            }
        });
        this.addEventListener("change", event => {
            const control = event.target as HTMLInputElement;
            if (control.hasAttribute("data-recomb-base-care")) this.goalEdit(() => {this.baseCare = control.checked;});
            if (control.hasAttribute("data-recomb-required-base")) this.goalEdit(() => {this.requiredBase = control.value as Input;});
            if (control.hasAttribute("data-recomb-extras")) this.goalEdit(() => {this.goal.allowExtraModifiers = control.checked;});
            if (control.hasAttribute("data-recomb-threshold")) this.goalEdit(() => {this.goal.minSatisfiedSlots = Number(control.value);});
        });
        for (const focus of ["a", "b", "goal"] as const) {
            const list = this.list(focus)!;
            list.addEventListener("choose-mods", event => this.select(focus, (event as CustomEvent).detail.side));
            list.addEventListener("item-properties-change", event => {
                const detail = (event as CustomEvent<ItemPropertyChange>).detail;
                this.select(focus);
                if (focus === "goal") this.goalEdit(() => {
                    if (detail.rarity !== undefined) this.goal.goalRarity = detail.rarity;
                    if (detail.influence_bits !== undefined) this.goal.goalInfluenceBits = detail.influence_bits ?? undefined;
                    if (detail.corrupted !== undefined) this.goal.goalCorrupted = detail.corrupted ?? undefined;
                });
                else this.edit(async () => {
                    const input = this.inputs.get(focus)!;
                    await this.client.editItem(input.item, input.session, detail as ItemEdit); await this.refreshInput(input);
                });
            });
            if (focus === "goal") {
                list.addEventListener("target-tier-change", event => {this.select("goal"); this.goalEdit(() => {
                    const detail = (event as CustomEvent).detail;
                    const slot = this.goal.slots.find(slot => slot.familyModKey === detail.familyModKey);
                    if (slot) slot.minTier = detail.minTier;
                });});
                list.addEventListener("target-remove", event => {this.select("goal"); this.goalEdit(() => {
                    const detail = (event as CustomEvent).detail;
                    this.goal.slots = this.goal.slots.filter((slot, index) => detail.familyModKey ? slot.familyModKey !== detail.familyModKey : index !== detail.slotIndex);
                    if (detail.implicitKey) this.goal.goalImplicitKeys = this.goal.goalImplicitKeys?.filter(key => key !== detail.implicitKey);
                    this.goal.minSatisfiedSlots = undefined;
                });});
            } else {
                list.addEventListener("remove-item-mod", event => this.remove(focus, (event as CustomEvent).detail));
                list.addEventListener("fracture-mod", event => this.fracture(focus, (event as CustomEvent).detail));
            }
        }
        this.pool.addEventListener("craft-mod", event => {
            const detail = (event as CustomEvent).detail;
            const focus = this.focus;
            if (focus === "goal") this.addGoal(detail.key);
            else this.edit(async () => {
                const input = this.inputs.get(focus)!;
                if (detail.side === "enchantment") throw new Error("Enchantment editing is outside this calculator’s supported goal scope.");
                await this.client.editItem(input.item, input.session, detail.side === "implicit" ? {add_implicit: detail.key} : {add_explicit: detail.key});
                await this.refreshInput(input);
            });
        });
        this.pool.addEventListener("remove-mod", event => {if (this.focus !== "goal") this.remove(this.focus, (event as CustomEvent).detail);});
        this.pool.addEventListener("fracture-mod", event => {if (this.focus !== "goal") this.fracture(this.focus, (event as CustomEvent).detail);});
        this.querySelector<PcBasePicker>("pc-base-picker")!.addEventListener("cancel", () => this.closePicker());
        this.querySelector<PcBasePicker>("pc-base-picker")!.addEventListener("confirm", event => {
            if (!this.pickerSide) return;
            const side = this.pickerSide, selection = (event as CustomEvent<BasePickerSelection>).detail;
            this.edit(async () => {
                const replacement = await this.openInput({base: selection.base, itemLevel: selection.itemLevel, state: null});
                const old = this.inputs.get(side); this.inputs.set(side, replacement);
                if (old) await this.closeInput(old);
                this.closePicker(); this.select(side);
            });
        });
        this.addEventListener("keydown", event => {
            if (!this.pickerSide) return;
            if (event.key === "Escape") {event.preventDefault(); this.closePicker();}
            if (event.key === "Tab") {
                const nodes = Array.from(this.querySelectorAll<HTMLElement>(".pc-recomb-base-dialog button:not(:disabled), .pc-recomb-base-dialog select:not(:disabled), .pc-recomb-base-dialog input:not(:disabled)"));
                const first = nodes[0], last = nodes[nodes.length - 1];
                if (event.shiftKey && document.activeElement === first) {event.preventDefault(); last?.focus();}
                else if (!event.shiftKey && document.activeElement === last) {event.preventDefault(); first?.focus();}
            }
        });
    }

    private remove(side: Input, detail: {modId: number; side: "prefix" | "suffix" | "implicit"}): void {
        this.select(side);
        this.edit(async () => {
            const input = this.inputs.get(side)!;
            if (detail.side === "implicit") await this.client.editItem(input.item, input.session, {remove_implicit: input.mods[detail.modId].key});
            else await this.client.removeMod(input.item, detail);
            await this.refreshInput(input);
        });
    }
    private fracture(side: Input, detail: {key: string; modId: number; side: "prefix" | "suffix"; onItem?: boolean}): void {
        this.select(side);
        this.edit(async () => {
            const input = this.inputs.get(side)!;
            if (detail.onItem === false) await this.client.editItem(input.item, input.session, {add_explicit: detail.key, fractured: true});
            else await this.client.setModFractured(input.item, detail);
            await this.refreshInput(input);
        });
    }
    private addGoal(key: string): void {
        this.goalEdit(() => {
            const mods = this.inputs.get("a")!.mods, mod = mods.find(mod => mod.key === key);
            if (!mod) throw new Error("This modifier is unavailable in the input A goal-interpreting session.");
            if ([4, 8, 9].includes(mod.reach_kind)) {
                const keys = this.goal.goalImplicitKeys ?? [];
                if (!keys.includes(key) && keys.length >= 8) throw new Error("Goals support at most eight implicits.");
                this.goal.goalImplicitKeys = keys.includes(key) ? keys.filter(entry => entry !== key) : [...keys, key];
                if (mod.reach_kind === 8) this.goal.goalCorrupted = true;
            } else {
                const family = buildModifierKeyIndex(mods).get(key);
                if (!family) throw new Error("This modifier has no supported explicit goal family.");
                const slot = this.goal.slots.find(slot => slot.familyModKey === family);
                if (slot) slot.minTier = mod.family_tier_index;
                else {
                    if (this.goal.slots.length >= 8) throw new Error("Goals support at most eight explicit requirements.");
                    this.goal.slots.push({familyModKey: family, minTier: mod.family_tier_index});
                }
                this.goal.minSatisfiedSlots = undefined;
            }
        });
    }

    private openPicker(side: Input, origin: HTMLElement): void {
        if (this.busy) return;
        this.pickerSide = side; this.returnFocus = origin;
        const picker = this.querySelector<PcBasePicker>("pc-base-picker")!;
        picker.setBases(this.bases);
        const snapshot = this.inputs.get(side)?.snapshot;
        if (snapshot) picker.setSelection(snapshot.base, snapshot.itemLevel);
        this.querySelector<HTMLElement>(".pc-recomb-base-overlay")!.hidden = false;
        this.querySelector<HTMLElement>(".pc-recomb-base-title")!.textContent = `Choose ${names[side]} base · creates a fresh rare item`;
        this.querySelector<HTMLElement>(".pc-recomb-layout")!.inert = true;
        this.querySelector<HTMLButtonElement>("[data-recomb-close-picker]")!.focus();
    }
    private closePicker(): void {
        this.pickerSide = null;
        this.querySelector<HTMLElement>(".pc-recomb-base-overlay")!.hidden = true;
        this.querySelector<HTMLElement>(".pc-recomb-layout")!.inert = false;
        this.returnFocus?.focus(); this.returnFocus = null;
    }

    private calculate(): void {
        if (this.busy || this.calculating || this.inputs.size !== 2 || this.disposed) return;
        this.error = ""; this.result = null; this.calculating = true;
        const identity = this.identity(), token = this.lifetime.freeze(identity);
        const inputs = structuredClone([this.inputs.get("a")!.snapshot, this.inputs.get("b")!.snapshot]) as [ItemSnapshot, ItemSnapshot];
        const goals = {version: "calculator_goal_set_v1" as const, actions: [], goals: [{id: this.goal.id, goal: structuredClone(calculatorItemGoal(this.goal))}]};
        const current = () => this.lifetime.accepts(token, this.identity(), this.disposed);
        this.oddsNotice = "Calculating native pair odds…"; this.renderOdds();
        const work = calculateAuthoredRecombination(this.client, this.data, inputs, goals).then(result => {
            if (!current()) return;
            recombinationOdds(result, this.baseCare ? this.inputs.get(this.requiredBase)!.snapshot.base : undefined);
            this.result = result; this.oddsNotice = "";
        }).catch(error => {
            if (current()) this.oddsNotice = `Odds unavailable: ${error instanceof Error ? error.message : String(error)}`;
        }).finally(() => {
            this.calculations.delete(work);
            if (current()) {this.calculating = false; this.renderOdds();}
        });
        this.calculations.add(work);
    }

    private render(): void {
        if (this.disposed) return;
        for (const side of ["a", "b"] as const) {
            const input = this.inputs.get(side);
            if (input) this.list(side)?.setModel({...input.card, readOnly: this.busy});
        }
        const a = this.inputs.get("a");
        if (a && this.catalog) {
            const required = this.baseCare ? this.inputs.get(this.requiredBase)?.snapshot.base : undefined;
            const target = buildCalculatorTargetModel({baseKey: required, baseName: required ? this.baseName(required) : "Either input base",
                itemLevel: 0, rarity: this.goal.goalRarity, slots: this.goal.slots,
                modifierOptions: buildModifierOptions(a.mods, this.catalog), maxPrefix: 3, maxSuffix: 3, groupLabel: key => key});
            this.list("goal")?.setModel({...target, properties: {influences: this.catalog.genericInfluences,
                influenceBits: this.goal.goalInfluenceBits, corrupted: this.goal.goalCorrupted},
                implicits: (this.goal.goalImplicitKeys ?? []).map(key => ({key, textLines: a.mods.find(mod => mod.key === key)?.text_lines ?? [key]}))});
        }
        this.querySelectorAll<HTMLButtonElement>("[data-recomb-base], [data-recomb-clear]").forEach(button => {button.disabled = this.busy;});
        const checkbox = this.querySelector<HTMLInputElement>("[data-recomb-base-care]")!; checkbox.checked = this.baseCare; checkbox.disabled = this.busy;
        const base = this.querySelector<HTMLSelectElement>("[data-recomb-required-base]")!; base.value = this.requiredBase; base.disabled = this.busy || !this.baseCare;
        const extras = this.querySelector<HTMLInputElement>("[data-recomb-extras]")!; extras.checked = this.goal.allowExtraModifiers === true; extras.disabled = this.busy;
        const threshold = this.querySelector<HTMLSelectElement>("[data-recomb-threshold]")!;
        threshold.replaceChildren(...Array.from({length: Math.max(1, this.goal.slots.length)}, (_, index) => {
            const count = this.goal.slots.length - index;
            const option = document.createElement("option"); option.value = String(count);
            option.textContent = count === this.goal.slots.length ? `All ${count} modifiers` : `At least ${count} of ${this.goal.slots.length}`; return option;
        }));
        threshold.value = String(this.goal.minSatisfiedSlots ?? this.goal.slots.length); threshold.disabled = this.busy || !this.goal.slots.length;
        const error = this.querySelector<HTMLElement>(".pc-recomb-error")!; error.hidden = !this.error; error.textContent = this.error;
        this.pool.inert = this.busy;
        this.renderFocus(); this.renderPool(); this.renderOdds();
    }
    private renderFocus(): void {
        this.querySelectorAll<HTMLButtonElement>("[data-recomb-focus]").forEach(button => button.setAttribute("aria-pressed", String(button.dataset.recombFocus === this.focus)));
        this.querySelectorAll<HTMLElement>("[data-recomb-card]").forEach(card => card.classList.toggle("is-editing", card.dataset.recombCard === this.focus));
        this.querySelector<HTMLElement>(".pc-recomb-focus-label")!.textContent = `Editing ${names[this.focus]}${this.focus === "goal" ? " · goal catalog uses input A’s native session" : " · picker adds modifiers to this input"}`;
    }
    private renderPool(): void {
        const input = this.inputs.get(this.focus === "goal" ? "a" : this.focus);
        if (!input) return;
        const ids = (side: string) => (input.info[`${side}_mod_ids`] as number[]) ?? [];
        this.pool.setInteractionMode(this.focus === "goal" ? "goal" : "direct");
        this.pool.setModel({mods: input.mods, allowUnrollable: true, pool: null, poolWeights: new Map(), item: {
            rarity: input.card.rarity, prefixOnItem: new Set(ids("prefix")), suffixOnItem: new Set(ids("suffix")),
            implicitOnItem: new Set(ids("implicit")), enchantmentOnItem: new Set(ids("enchantment")),
            fracturedPrefixOnItem: new Set(input.info.fractured_prefix_mod_ids as number[]), fracturedSuffixOnItem: new Set(input.info.fractured_suffix_mod_ids as number[]),
            groupOnItem: new Set([...ids("prefix"), ...ids("suffix")].map(id => input.mods[id].primary_group_id)), maxPrefix: input.card.maxPrefix, maxSuffix: input.card.maxSuffix,
        }});
        this.pool.setSelectedTiers(this.focus === "goal" ? this.goal.slots.flatMap(slot => slot.familyModKey ? [{familyModKey: slot.familyModKey, minTier: slot.minTier}] : []) : []);
        this.pool.setSelectedImplicits(this.focus === "goal" ? this.goal.goalImplicitKeys ?? [] : []);
    }
    private renderOdds(): void {
        if (this.disposed || !this.querySelector("[data-recomb-output]")) return;
        const button = this.querySelector<HTMLButtonElement>("[data-recomb-calculate]")!;
        button.disabled = this.busy || this.calculating || this.inputs.size !== 2;
        this.querySelector<HTMLButtonElement>("[data-recomb-cancel]")!.hidden = !this.calculating;
        const odds = this.result ? recombinationOdds(this.result, this.baseCare ? this.inputs.get(this.requiredBase)?.snapshot.base : undefined) : null;
        renderReact(this.querySelector<HTMLElement>("[data-recomb-output]")!, odds ? <>
            <div className="pc-recomb-answer"><strong>{percent(odds.success)}</strong><span>Chance of the goal result{this.baseCare ? " on the required base" : " on either base"}</span><span>Estimated game odds · native model enumeration</span></div>
            <table><thead><tr><th>Result carrier / base</th><th>Base chance</th><th>Goal on this carrier</th></tr></thead><tbody>{odds.carriers.map(carrier => <tr key={carrier.carrier}>
                <td>{carrier.carrier === 0 ? "A" : "B"} · {this.baseName(carrier.base)} · iLvl {carrier.itemLevel}</td><td>{percent(carrier.probability)}</td><td>{percent(carrier.goalMass)}</td>
            </tr>)}</tbody></table>
            <p className="pc-help">Each row is a share of all attempts, including its random carrier chance. A base requirement counts every carrier with that base.</p>
            <p>{odds.success > 0 ? `${(1 / odds.success).toLocaleString(undefined, {maximumFractionDigits: 2})} expected attempts with fresh copies of both inputs each time.` : "The goal has zero probability in the supported model."}</p>
        </> : <p className="pc-recomb-odds-notice" role={this.oddsNotice.startsWith("Odds unavailable") ? "alert" : undefined}>{this.busy ? "Preparing authored items…" : this.oddsNotice}</p>);
    }
    private async persist(): Promise<void> {
        if (this.disposed || this.inputs.size !== 2) return;
        await putRecombinationDraft({version: "recombination_calculator_v1", docId: this.docId,
            inputs: {a: this.inputs.get("a")!.snapshot, b: this.inputs.get("b")!.snapshot}, goal: this.goal,
            baseCare: this.baseCare, requiredBase: this.requiredBase, updatedAt: Date.now()});
    }
    private async closeInput(input: AuthoredInput): Promise<void> {
        try {await this.client.closeItem(input.item);} finally {await this.client.closeSession(input.session);}
    }
    private async dispose(): Promise<void> {
        if (this.disposed) return;
        this.disposed = true; this.lifetime.invalidate();
        workspace().unregisterDocument(this.docId);
        await Promise.allSettled([...(this.currentWork ? [this.currentWork] : []), ...this.calculations]);
        await Promise.allSettled([...this.inputs.values()].map(input => this.closeInput(input)));
        this.inputs.clear(); disposeReact(this);
    }
}
customElements.define("pc-recombination-calculator", PcRecombinationCalculator);
