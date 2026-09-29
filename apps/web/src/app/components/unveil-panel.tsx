import { useId } from "react";
import { modTextLines } from "../mod-text";

export interface UnveilOption {
    key: string;
    textLines: string[];
    side: "prefix" | "suffix";
}

const INSCRIPTIONS = [
    "ᚹᛟᚱᚦ ᛊᛖᚲᚱᛖᛏ ᛟᚠ ᛏᚺᛖ ᚡᛖᛁᛚ",
    "ᚨᚾᚲᛁᛖᚾᛏ ᛟᚨᚦ ᛒᛟᚢᚾᛞ ᛁᚾ ᚨᛊᚺ",
    "ᛊᚺᚨᛞᛟᚹ ᚨᚾᛞ ᛊᛁᛚᛖᚾᚲᛖ ᚱᛖᛗᚨᛁᚾ",
];

/** Decorative veiled script never contains the engine's hidden offer text. */
export function VeiledInscription({index = 0}: {index?: number}) {
    return <span className="pc-veiled-inscription" role="img" aria-label="Veiled modifier">
        <span aria-hidden="true">{INSCRIPTIONS[index % INSCRIPTIONS.length]}</span>
    </span>;
}

export function UnveilPanel({options, revealed, selectedKey, onReveal, onSelect, onConfirm, onVeiledCurrency}: {
    options: UnveilOption[];
    revealed: boolean;
    selectedKey: string;
    onReveal: () => void;
    onSelect: (key: string) => void;
    onConfirm: () => void;
    onVeiledCurrency: () => void;
}) {
    const id = useId();
    const selected = revealed && options.some(option => option.key === selectedKey);
    return <section className="pc-unveil-panel" aria-labelledby={id}>
        <header className="pc-unveil-heading"><span aria-hidden="true">◇</span><h3 id={id}>Unveiling</h3><span aria-hidden="true">◇</span></header>
        <p className="pc-unveil-instruction" aria-live="polite">{!options.length ? "This item has no veiled modifier."
            : revealed ? "Choose one modifier to unveil." : "A hidden power lies beneath the veil."}</p>
        {!options.length ? <div className="pc-unveil-empty"><VeiledInscription />
            <button type="button" onClick={onVeiledCurrency}>Veiled currency</button>
        </div> : <>
            {revealed ? <fieldset className="pc-unveil-choices">
                <legend className="pc-visually-hidden">Choose an unveiled modifier</legend>
                {options.map((option, index) => <label key={option.key} className={`pc-unveil-choice ${selectedKey === option.key ? "is-selected" : ""}`}>
                    <input type="radio" name={id + "-choice"} value={option.key} checked={selectedKey === option.key}
                        onChange={() => onSelect(option.key)} />
                    <span className="pc-unveil-seal" aria-hidden="true">{["I", "II", "III"][index] ?? index + 1}</span>
                    <span className="pc-unveil-mod"><span className="pc-unveil-side">{option.side}</span>
                        {modTextLines(option.textLines).map((line, lineIndex) => <span className="pc-unveil-mod-line" key={lineIndex}>{line}</span>)}
                    </span>
                </label>)}
            </fieldset> : <div className="pc-unveil-choices" aria-label="Hidden modifiers">
                {options.map((option, index) => <div className="pc-unveil-hidden" key={option.key}>
                    <span className="pc-unveil-seal" aria-hidden="true">◇</span><VeiledInscription index={index} />
                </div>)}
            </div>}
            <div className="pc-unveil-footer">
                {revealed ? <button type="button" className="pc-unveil-confirm" data-config-action="unveil" disabled={!selected} onClick={onConfirm}>Confirm modifier</button>
                    : <button type="button" className="pc-unveil-confirm" data-unveil-reveal onClick={onReveal}>Unveil</button>}
            </div>
        </>}
    </section>;
}
