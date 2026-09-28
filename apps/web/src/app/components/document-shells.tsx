import { ControllerElement as Element } from "../react-host";
import "./pc-craft-controls";
import { GameIcon } from "./pc-game-icon";

export function BaseSelectionShell({kind}: {kind: "emulator" | "calculator"}) {
    return <div className={`pc-${kind} pc-emulator-picking`}><Element tag="pc-base-picker" /></div>;
}

export function EmulatorShell({baseName, itemLevel}: {baseName: string; itemLevel: number}) {
    return <div className="pc-emulator">
        <div className="pc-craft-bar">
            <button data-cmd="change-base">Change base…</button>
            <span className="pc-emu-base">{baseName} · iLvl {itemLevel}</span>
            <button data-cmd="create">Create item</button>
            <button data-cmd="undo" title="Undo (Ctrl/⌘ Z)" disabled>Undo</button>
            <button data-cmd="redo" title="Redo (Ctrl/⌘ Shift Z)" disabled>Redo</button>
            <span className="pc-emu-save"><span className="pc-emu-name">Unsaved</span>
                <button data-cmd="save">Save</button><button data-cmd="save-as">Save As</button>
                <button data-cmd="duplicate">Duplicate</button><button data-cmd="strategy">Use in Strategy</button><button data-cmd="calculator">Odds</button>
            </span><span className="pc-emu-status" hidden />
        </div>
        <Element tag="pc-craft-controls" className="pc-advanced-crafts" />
        <div className="pc-emu-body">
            <section className="pc-emu-item"><h3>Item</h3><Element tag="pc-mod-list" /></section>
            <section className="pc-emu-pool"><Element tag="pc-mod-pool" allow-direct-craft="" /></section>
            <section className="pc-emu-side"><h3>Craft history</h3><ul className="pc-emu-history" /></section>
        </div>
    </div>;
}

export function CalculatorShell({freshRarity, allowExtraModifiers}: {freshRarity: string; allowExtraModifiers: boolean}) {
    return <div className="pc-calculator">
        <div className="pc-craft-bar pc-calc-toolbar"><span className="pc-calc-workbench-title">Calculator</span><span className="pc-calc-status" hidden /></div>
        <Element tag="pc-craft-controls" className="pc-advanced-crafts" />
        <div className="pc-calc-body">
            <aside className="pc-calc-contexts" role="tablist" aria-label="Modifier pool context">
                <article className="pc-calc-context-card pc-calc-input-context" data-context-card="input" role="tab" tabIndex={0}>
                    <header className="pc-calc-context-header"><h3>Input item</h3><span className="pc-calc-context-state">Select</span></header>
                    <Element tag="pc-mod-list" data-role="input-item" />
                    <div className="pc-calc-input-actions"><button data-cmd="change-base">Change base…</button>
                        <span className="pc-calc-new-item"><select data-role="fresh-rarity" aria-label="New item rarity" defaultValue={freshRarity}>
                            <option value="normal">Normal</option><option value="magic">Magic</option><option value="rare">Rare</option>
                        </select><button data-cmd="new-item">New item</button></span>
                    </div>
                </article>
                <article className="pc-calc-context-card pc-calc-goal" data-context-card="goal" role="tab" tabIndex={0}>
                    <header className="pc-calc-context-header"><h3>Goal item</h3><span className="pc-calc-context-state">Select</span></header>
                    <div className="pc-calc-goal-controls">
                        <label><span>Finished rarity</span><select data-role="goal-rarity">
                            <option value="normal">Normal</option><option value="magic">Magic</option><option value="rare">Rare</option>
                        </select></label>
                        <label><span>Success means</span><select data-role="success-threshold" /></label>
                    </div>
                    <Element tag="pc-mod-list" data-role="goal-item" />
                    <label className="pc-calc-extra-modifiers"><input type="checkbox" data-role="allow-extra-modifiers" defaultChecked={allowExtraModifiers} />
                        <span>Allow extra modifiers</span></label>
                </article>
            </aside>
            <section className="pc-calc-pool"><Element tag="pc-mod-pool" /></section>
            <section className="pc-calc-results"><h3>Odds</h3><div className="pc-calc-output" /></section>
            <section className="pc-calc-solve"><div className="pc-calc-solve-panel" /></section>
        </div>
    </div>;
}

export function StrategyShell({palette}: {palette: Array<[string, string]>}) {
    return <div className="pc-strategy-editor">
        <div className="pc-strategy-toolbar">
            <strong className="pc-strategy-title">Strategy Builder</strong><span className="pc-strategy-saved">Unsaved</span>
            <div className="pc-strategy-mode" role="group" aria-label="Strategy evaluation mode">
                <button data-mode="simulator" aria-pressed="false">Simulator</button><button data-mode="calculator" aria-pressed="false">Calculator</button>
            </div>
            <button data-cmd="save">Save</button><button data-cmd="save-as">Save As</button><button data-cmd="duplicate">Duplicate</button>
            <button data-cmd="change-base">Change base…</button><button data-cmd="delete">Delete selected</button>
            <button data-cmd="undo" title="Undo (Ctrl/⌘ Z)" disabled>Undo</button><button data-cmd="redo" title="Redo (Ctrl/⌘ Shift Z)" disabled>Redo</button>
            <button data-cmd="auto-layout">Auto layout</button><button data-cmd="fit-view">Fit view</button><span className="pc-strategy-status" />
        </div>
        <div className="pc-strategy-main">
            <aside className="pc-strategy-palette"><h3>Palette</h3><p>Drag or double-click to add</p>
                <div className="pc-palette-list">{palette.map(([type,label]) => <button key={type} draggable data-palette={type}>
                    {type.startsWith("operation:") && <GameIcon assetKey={"action:" + type.slice("operation:".length)} />}{label}
                </button>)}</div>
            </aside>
            <Element tag="pc-strategy-board" />
            <aside className="pc-strategy-inspector"><div className="pc-inspector-content" /><div className="pc-validation" /></aside>
        </div>
        <section className="pc-strategy-runner"><div className="pc-strategy-simulator-surface">
            <Element tag="pc-simulator" /><Element tag="pc-run-trace" />
        </div><Element tag="pc-strategy-odds" hidden /></section>
    </div>;
}
