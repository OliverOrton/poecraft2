/*
 * Minimal Save / Discard / Cancel dialog for closing a dirty document.
 * Resolves with the chosen action.
 */

export type DirtyChoice = "save" | "discard" | "cancel";

export function openDirtyModal(name: string): Promise<DirtyChoice> {
    return new Promise((resolve) => {
        const overlay = document.createElement("div");
        overlay.className = "pc-modal-overlay";
        overlay.innerHTML = `
            <div class="pc-modal" role="dialog" aria-modal="true">
                <p>“${name}” has unsaved changes.</p>
                <div class="pc-modal-actions">
                    <button data-choice="save">Save</button>
                    <button data-choice="discard">Discard</button>
                    <button data-choice="cancel">Cancel</button>
                </div>
            </div>`;

        const finish = (choice: DirtyChoice): void => {
            overlay.remove();
            document.removeEventListener("keydown", onKey);
            resolve(choice);
        };
        const onKey = (event: KeyboardEvent): void => {
            if (event.key === "Escape") {
                finish("cancel");
            }
        };

        overlay.addEventListener("mousedown", (event) => {
            if (event.target === overlay) {
                finish("cancel");
            }
        });
        overlay.querySelectorAll<HTMLButtonElement>("button[data-choice]").forEach(
            (button) => {
                button.addEventListener("click", () =>
                    finish(button.dataset.choice as DirtyChoice),
                );
            },
        );
        document.addEventListener("keydown", onKey);
        document.body.appendChild(overlay);
    });
}
