/*
 * Root application shell. Hosts the dockview workspace and a thin toolbar to
 * open new Emulator documents and the Stash.
 */

import { PcWorkspace } from "./pc-workspace";
import "./pc-workspace";

export class PcApp extends HTMLElement {
    connectedCallback(): void {
        this.innerHTML = `
            <header class="pc-titlebar">
                <span class="pc-brand">poecraft</span>
                <button data-cmd="new-emulator">+ Emulator</button>
                <button data-cmd="new-strategy">+ Strategy</button>
                <button data-cmd="new-calculator">+ Calculator</button>
                <button data-cmd="open-stash">Stash</button>
            </header>
            <main class="pc-main">
                <pc-workspace></pc-workspace>
            </main>`;

        const workspace = this.querySelector<PcWorkspace>("pc-workspace")!;
        this.querySelector('[data-cmd="new-emulator"]')!.addEventListener("click", () => {
            void workspace.openEmulator();
        });
        this.querySelector('[data-cmd="new-strategy"]')!.addEventListener(
            "click",
            () => {
                void workspace.openStrategy();
            },
        );
        this.querySelector('[data-cmd="new-calculator"]')!.addEventListener(
            "click",
            () => {
                void workspace.openCalculator();
            },
        );
        this.querySelector('[data-cmd="open-stash"]')!.addEventListener("click", () => {
            workspace.openStash();
        });
    }
}

customElements.define("pc-app", PcApp);
