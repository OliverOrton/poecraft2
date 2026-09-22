// Test-only manual control shared by the real-worker probe and its DOM test.
// default_finish leaves the Calculator's own automatic deadline untouched.
export function finishVerifiedCalculatorProbe(control: string, calculator: HTMLElement): boolean {
    if (control !== "finish") return false;
    const button = calculator.querySelector<HTMLButtonElement>('[data-solve-cmd="finish"]');
    if (!button || button.disabled) return false;
    button.click();
    return true;
}
