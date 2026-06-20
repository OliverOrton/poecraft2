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
                <p class="pc-modal-message"></p>
                <div class="pc-modal-actions">
                    <button data-choice="save">Save</button>
                    <button data-choice="discard">Discard</button>
                    <button data-choice="cancel">Cancel</button>
                </div>
            </div>`;
        overlay.querySelector(".pc-modal-message")!.textContent =
            `“${name}” has unsaved changes.`;

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

/** Small app-native text prompt. Browser/Electron shells do not consistently
 * support window.prompt, and saves should remain testable and keyboard-safe. */
export function openTextModal(
    title: string,
    initialValue: string,
    actionLabel = "Save",
): Promise<string | null> {
    return new Promise((resolve) => {
        const overlay = document.createElement("div");
        overlay.className = "pc-modal-overlay";
        overlay.innerHTML = `
            <form class="pc-modal pc-text-modal" role="dialog" aria-modal="true">
                <label class="pc-field">
                    <span></span>
                    <input name="value" autocomplete="off">
                </label>
                <div class="pc-modal-actions">
                    <button type="submit" data-action="accept"></button>
                    <button type="button" data-action="cancel">Cancel</button>
                </div>
            </form>`;
        const form = overlay.querySelector<HTMLFormElement>("form")!;
        const label = overlay.querySelector<HTMLElement>(".pc-field span")!;
        const input = overlay.querySelector<HTMLInputElement>('input[name="value"]')!;
        const accept = overlay.querySelector<HTMLButtonElement>(
            '[data-action="accept"]',
        )!;
        label.textContent = title;
        accept.textContent = actionLabel;
        input.value = initialValue;

        const finish = (value: string | null): void => {
            overlay.remove();
            resolve(value);
        };
        form.addEventListener("submit", (event) => {
            event.preventDefault();
            const value = input.value.trim();
            if (value) finish(value);
        });
        overlay
            .querySelector('[data-action="cancel"]')!
            .addEventListener("click", () => finish(null));
        overlay.addEventListener("mousedown", (event) => {
            if (event.target === overlay) finish(null);
        });
        document.body.appendChild(overlay);
        input.focus();
        input.select();
    });
}
