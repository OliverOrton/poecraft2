/*
 * pc-combobox — a reusable searchable single-select. Filters a (possibly large)
 * option list as the user types and emits a `change` CustomEvent with the
 * selected value. Presentational and engine-agnostic.
 */

export interface ComboOption {
    value: string;
    label: string;
}

const MAX_VISIBLE = 200;

export class PcCombobox extends HTMLElement {
    private options: ComboOption[] = [];
    private filtered: ComboOption[] = [];
    private value = "";
    private open = false;
    private input!: HTMLInputElement;
    private list!: HTMLUListElement;

    connectedCallback(): void {
        this.innerHTML = `
            <div class="pc-combobox">
                <input class="pc-combobox-input" type="text"
                    placeholder="${this.getAttribute("placeholder") ?? "Search…"}"
                    autocomplete="off" spellcheck="false" />
                <ul class="pc-combobox-list" hidden></ul>
            </div>`;
        this.input = this.querySelector(".pc-combobox-input")!;
        this.list = this.querySelector(".pc-combobox-list")!;

        this.input.addEventListener("focus", () => this.show());
        this.input.addEventListener("input", () => {
            this.filter(this.input.value);
            this.show();
        });
        this.input.addEventListener("blur", () => {
            // Delay so a click on an option registers before the list closes.
            setTimeout(() => this.hide(), 150);
        });
        this.list.addEventListener("mousedown", (event) => {
            const target = (event.target as HTMLElement).closest("li");
            if (target?.dataset.value !== undefined) {
                this.select(target.dataset.value);
            }
        });
    }

    setOptions(options: ComboOption[]): void {
        this.options = options;
        this.filter(this.input?.value ?? "");
    }

    setValue(value: string): void {
        this.value = value;
        const match = this.options.find((option) => option.value === value);
        if (this.input) {
            this.input.value = match ? match.label : value;
        }
    }

    getValue(): string {
        return this.value;
    }

    private filter(query: string): void {
        const needle = query.trim().toLowerCase();
        this.filtered = needle
            ? this.options.filter((option) =>
                  option.label.toLowerCase().includes(needle),
              )
            : this.options.slice();
        this.renderList();
    }

    private renderList(): void {
        const shown = this.filtered.slice(0, MAX_VISIBLE);
        const overflow = this.filtered.length - shown.length;
        this.list.innerHTML =
            shown
                .map(
                    (option) =>
                        `<li data-value="${option.value}" class="${
                            option.value === this.value ? "selected" : ""
                        }">${option.label}</li>`,
                )
                .join("") +
            (overflow > 0
                ? `<li class="pc-combobox-more">…and ${overflow} more — keep typing</li>`
                : "");
    }

    private select(value: string): void {
        this.setValue(value);
        this.hide();
        this.dispatchEvent(
            new CustomEvent("change", { detail: { value }, bubbles: true }),
        );
    }

    private show(): void {
        this.open = true;
        this.list.hidden = false;
    }

    private hide(): void {
        this.open = false;
        this.list.hidden = true;
    }
}

customElements.define("pc-combobox", PcCombobox);
