/*
 * pc-workspace — the desktop-like workspace host. Wraps dockview-core to manage
 * documents in tabs and resizable splits, persists the layout independently of
 * document content, and owns the Stash + dirty-close handling.
 *
 * Document content lives in IndexedDB drafts (keyed by docId); the dockview
 * layout (arrangement + docId params) lives in localStorage. Restoring the
 * layout re-creates panels, and each document reloads its own draft — so layout
 * and content survive reloads independently.
 */

import {
    createDockview,
    themeAbyss,
    DockviewApi,
    IContentRenderer,
    ITabRenderer,
    GroupPanelPartInitParameters,
    TabPartInitParameters,
    CreateComponentOptions,
} from "dockview-core";

import {
    ItemSnapshot,
    StashRecord,
    DraftRecord,
    deleteDraft,
    deleteStrategyDraft,
    itemSnapshotRarity,
    putCalculatorDraft,
    putDraft,
    putStrategyDraft,
    putStash,
    saveLayout,
    loadLayout,
} from "../workspace/persistence";
import {
    StrategyDocument,
    isStrategyDocument,
} from "../strategy-model";
import {
    DocumentHandlers,
    OpenMode,
    WorkspaceApi,
    setWorkspace,
} from "../workspace/registry";
import { openDirtyModal } from "../workspace/dirty-modal";

import "./pc-calculator";
import "./pc-emulator";
import "./pc-strategy-editor";
import "./pc-stash";

const STASH_PANEL_ID = "stash";

function newDocId(): string {
    return `doc-${crypto.randomUUID()}`;
}

/** Wraps a custom element as a dockview content renderer. */
class ElementRenderer implements IContentRenderer {
    readonly element: HTMLElement;
    constructor(tag: string) {
        this.element = document.createElement(tag);
    }
    init(params: GroupPanelPartInitParameters): void {
        const docId = params.params?.docId as string | undefined;
        if (docId) {
            this.element.setAttribute("doc-id", docId);
        }
    }
}

export class PcWorkspace extends HTMLElement implements WorkspaceApi {
    private api!: DockviewApi;
    private readonly tabs = new Map<string, PcTab>();
    private readonly handlers = new Map<string, DocumentHandlers>();
    private readonly titles = new Map<string, string>();
    private readonly dirty = new Map<string, boolean>();
    private readonly stashListeners = new Set<() => void>();

    connectedCallback(): void {
        this.classList.add("dockview-theme-abyss");
        this.api = createDockview(this, {
            theme: themeAbyss,
            defaultTabComponent: "pc-tab",
            createComponent: (options: CreateComponentOptions): IContentRenderer => {
                switch (options.name) {
                    case "stash":
                        return new ElementRenderer("pc-stash");
                    case "strategy":
                        return new ElementRenderer("pc-strategy-editor");
                    case "calculator":
                        return new ElementRenderer("pc-calculator");
                    case "emulator":
                    default:
                        return new ElementRenderer("pc-emulator");
                }
            },
            createTabComponent: (): ITabRenderer => new PcTab(this),
        });

        setWorkspace(this);
        this.api.onDidLayoutChange(() => saveLayout(this.api.toJSON()));

        const saved = loadLayout();
        let restored = false;
        if (saved) {
            try {
                this.api.fromJSON(saved as never);
                restored = this.api.panels.length > 0;
            } catch {
                restored = false;
            }
        }
        if (!restored) {
            void this.openEmulator();
        }
    }

    disconnectedCallback(): void {
        setWorkspace(null);
    }

    // --- WorkspaceApi -------------------------------------------------------

    registerDocument(docId: string, handlers: DocumentHandlers): void {
        this.handlers.set(docId, handlers);
    }

    unregisterDocument(docId: string): void {
        this.handlers.delete(docId);
    }

    async openEmulator(
        seed?: ItemSnapshot,
        mode: OpenMode = "copy",
        savedRef?: string,
        savedName?: string,
    ): Promise<void> {
        const docId = newDocId();
        if (seed) {
            // Pre-seed the draft so the freshly created emulator loads this item.
            const draft: DraftRecord = {
                docId,
                base: seed.base,
                itemLevel: seed.itemLevel,
                rarity: itemSnapshotRarity(seed),
                state: seed.state,
                history: [],
                savedRef: mode === "edit" ? (savedRef ?? null) : null,
                savedName: mode === "edit" ? (savedName ?? null) : null,
                dirty: mode === "copy",
                updatedAt: Date.now(),
            };
            await putDraft(draft);
        }
        const title = savedName ?? "Untitled";
        this.titles.set(docId, title);
        this.api.addPanel({
            id: docId,
            component: "emulator",
            title,
            params: { docId },
        });
    }

    async openStrategy(
        seed?: StrategyDocument | ItemSnapshot,
        mode: OpenMode = "copy",
        savedRef?: string,
        savedName?: string,
    ): Promise<void> {
        const docId = newDocId();
        if (seed) {
            const strategySeed = isStrategyDocument(seed);
            await putStrategyDraft({
                docId,
                strategy: strategySeed ? seed : null,
                sourceItem: strategySeed ? null : seed,
                savedRef: mode === "edit" ? (savedRef ?? null) : null,
                savedName: mode === "edit" ? (savedName ?? null) : null,
                dirty: mode === "copy",
                updatedAt: Date.now(),
            });
        }
        const title = savedName ?? "Untitled strategy";
        this.titles.set(docId, title);
        this.api.addPanel({
            id: docId,
            component: "strategy",
            title,
            params: { docId },
        });
    }

    async openCalculator(seed?: ItemSnapshot): Promise<void> {
        const docId = newDocId();
        if (seed) {
            // Pre-seed the draft so the fresh calculator loads this item.
            await putCalculatorDraft({
                docId,
                base: seed.base,
                itemLevel: seed.itemLevel,
                state: seed.state,
                goalRarity: "rare",
                slots: [],
                actionId: "",
                fossilKeys: [],
                updatedAt: Date.now(),
            });
        }
        const title = "Calculator";
        this.titles.set(docId, title);
        this.api.addPanel({
            id: docId,
            component: "calculator",
            title,
            params: { docId },
        });
    }

    openStash(): void {
        const existing = this.api.getPanel(STASH_PANEL_ID);
        if (existing) {
            existing.api.setActive();
            return;
        }
        this.api.addPanel({
            id: STASH_PANEL_ID,
            component: "stash",
            title: "Stash",
            params: {},
        });
    }

    async saveToStash(record: StashRecord): Promise<void> {
        await putStash(record);
        this.stashListeners.forEach((listener) => listener());
    }

    notifyDirty(docId: string, dirty: boolean, title: string): void {
        this.dirty.set(docId, dirty);
        this.titles.set(docId, title);
        const tab = this.tabs.get(docId);
        tab?.setDirty(dirty);
        const panel = this.api.getPanel(docId);
        panel?.api.setTitle(title);
    }

    onStashChange(listener: () => void): () => void {
        this.stashListeners.add(listener);
        return () => this.stashListeners.delete(listener);
    }

    // --- tab + close handling ----------------------------------------------

    registerTab(docId: string, tab: PcTab): void {
        this.tabs.set(docId, tab);
        tab.setDirty(this.dirty.get(docId) ?? false);
    }

    unregisterTab(docId: string): void {
        this.tabs.delete(docId);
    }

    async requestClose(docId: string): Promise<void> {
        const isDirty = this.dirty.get(docId) ?? false;
        if (isDirty) {
            const choice = await openDirtyModal(this.titles.get(docId) ?? "this item");
            if (choice === "cancel") {
                return;
            }
            if (choice === "save") {
                const handler = this.handlers.get(docId);
                const saved = handler ? await handler.save() : true;
                if (!saved) {
                    return; // save cancelled → keep the document open
                }
            }
        }
        const handler = this.handlers.get(docId);
        await handler?.dispose();
        await deleteDraft(docId);
        await deleteStrategyDraft(docId);
        this.handlers.delete(docId);
        this.dirty.delete(docId);
        this.titles.delete(docId);
        this.api.getPanel(docId)?.api.close();
    }
}

/** Custom tab: title, dirty indicator, and a close button that routes through
 * the workspace dirty-close flow. */
class PcTab implements ITabRenderer {
    readonly element: HTMLElement;
    private readonly titleEl: HTMLElement;
    private readonly dot: HTMLElement;
    private docId = "";

    constructor(private readonly host: PcWorkspace) {
        this.element = document.createElement("div");
        this.element.className = "pc-tab";
        this.element.innerHTML = `
            <span class="pc-tab-dirty" hidden>●</span>
            <span class="pc-tab-title"></span>
            <span class="pc-tab-close" title="Close">×</span>`;
        this.titleEl = this.element.querySelector(".pc-tab-title")!;
        this.dot = this.element.querySelector(".pc-tab-dirty")!;
        this.element.querySelector(".pc-tab-close")!.addEventListener(
            "mousedown",
            (event) => {
                event.stopPropagation();
                event.preventDefault();
                void this.host.requestClose(this.docId);
            },
        );
    }

    init(params: TabPartInitParameters): void {
        this.docId = params.api.id;
        this.titleEl.textContent = params.title ?? "";
        params.api.onDidTitleChange((event) => {
            this.titleEl.textContent = event.title;
        });
        this.host.registerTab(this.docId, this);
    }

    setDirty(dirty: boolean): void {
        this.dot.hidden = !dirty;
    }

    dispose(): void {
        this.host.unregisterTab(this.docId);
    }
}

customElements.define("pc-workspace", PcWorkspace);
