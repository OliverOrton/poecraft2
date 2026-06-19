/*
 * pc-mod-list — presentational view of an item's mods, grouped into implicits,
 * prefixes, and suffixes. Fed a resolved view model by the document; does no
 * engine work itself.
 */

export interface ModRow {
    key: string;
    fractured?: boolean;
}

export interface ModListModel {
    rarity: string;
    implicits: ModRow[];
    prefixes: ModRow[];
    suffixes: ModRow[];
}

function rows(label: string, side: string, list: ModRow[]): string {
    if (list.length === 0) {
        return "";
    }
    const items = list
        .map(
            (row) =>
                `<li class="pc-mod pc-mod-${side}">${row.key}${
                    row.fractured ? ' <span class="pc-mod-tag">fractured</span>' : ""
                }</li>`,
        )
        .join("");
    return `<div class="pc-mod-group"><h4>${label}</h4><ul>${items}</ul></div>`;
}

export class PcModList extends HTMLElement {
    setModel(model: ModListModel): void {
        const explicitCount = model.prefixes.length + model.suffixes.length;
        const empty =
            explicitCount === 0 && model.implicits.length === 0
                ? '<p class="pc-empty">No mods.</p>'
                : "";
        this.innerHTML = `
            <div class="pc-mod-list">
                <div class="pc-mod-header">
                    <span class="pc-rarity-${model.rarity}">${model.rarity}</span>
                    <span class="pc-mod-count">${explicitCount} explicit · ${model.prefixes.length}P / ${model.suffixes.length}S</span>
                </div>
                ${rows("Implicits", "implicit", model.implicits)}
                ${rows("Prefixes", "prefix", model.prefixes)}
                ${rows("Suffixes", "suffix", model.suffixes)}
                ${empty}
            </div>`;
    }
}

customElements.define("pc-mod-list", PcModList);
