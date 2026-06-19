/*
 * Root application shell. Hosts the emulator document. The dockview workspace
 * (multiple documents, tabs/splits, layout persistence, Stash) grows from here
 * in the next milestone.
 */

import "./pc-emulator";

export class PcApp extends HTMLElement {
    connectedCallback(): void {
        this.innerHTML = `
            <header class="pc-titlebar">
                <span class="pc-brand">poecraft</span>
                <span class="pc-titlebar-doc">Emulator</span>
            </header>
            <main class="pc-main">
                <pc-emulator></pc-emulator>
            </main>`;
    }
}

customElements.define("pc-app", PcApp);
