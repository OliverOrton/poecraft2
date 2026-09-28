import { useRef, useState } from "react";
import type { PcWorkspace } from "./pc-workspace";
import "./pc-workspace";
import "./pc-economy-selector";
import build from "../../generated/build-info.json";
import { diagnostics, downloadDiagnostics } from "../tester-diagnostics";
import { ControllerElement, disconnectReact, renderReact } from "../react-host";

function App() {
    const workspace = useRef<PcWorkspace | null>(null);
    const [copied, setCopied] = useState(false);
    async function copyDiagnostics() {
        try {
            await navigator.clipboard.writeText(JSON.stringify(diagnostics(), null, 2));
            setCopied(true);
        } catch { downloadDiagnostics(); }
    }
    return <>
        <header className="pc-titlebar">
            <span className="pc-brand">poecraft</span>
            <nav className="pc-document-actions" aria-label="Workspace">
                <button data-cmd="new-emulator" onClick={() => void workspace.current?.openEmulator()}>+ Emulator</button>
                <button data-cmd="new-strategy" onClick={() => void workspace.current?.openStrategy()}>+ Strategy</button>
                <button data-cmd="new-calculator" onClick={() => void workspace.current?.openCalculator()}>+ Calculator</button>
                <button data-cmd="open-stash" onClick={() => workspace.current?.openStash()}>Stash</button>
            </nav>
            <details className="pc-build-menu">
                <summary title="Testing build; feature qualification still applies">Beta · {build.build_id.slice(0, 8)}</summary>
                <div className="pc-build-menu-content">
                    <button data-diagnostics onClick={() => void copyDiagnostics()}>{copied ? "Copied" : "Copy diagnostics"}</button>
                    <button data-download-diagnostics onClick={downloadDiagnostics}>Download report</button>
                    <a href="https://github.com/OliverOrton/poecraft2/issues/new?template=tester-bug.yml" target="_blank" rel="noopener">Report bug</a>
                    <a href={build.base + "THIRD_PARTY_NOTICES.txt"} target="_blank" rel="noopener">Credits</a>
                </div>
            </details>
            <ControllerElement tag="pc-economy-selector" />
        </header>
        <main className="pc-main">
            <ControllerElement tag="pc-workspace" configure={element => { workspace.current = element as PcWorkspace; }} />
        </main>
    </>;
}

export class PcApp extends HTMLElement {
    connectedCallback(): void { renderReact(this, <App />); }
    disconnectedCallback(): void { disconnectReact(this); }
}
customElements.define("pc-app", PcApp);
