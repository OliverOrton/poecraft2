import assert from 'node:assert/strict';
import { mkdir } from 'node:fs/promises';
import { resolve } from 'node:path';

function luminance(value) {
    const rgb = value.match(/[\d.]+/g).slice(0, 3).map(Number).map(x => {
        const channel = x / 255;
        return channel <= .04045 ? channel / 12.92 : ((channel + .055) / 1.055) ** 2.4;
    });
    return .2126 * rgb[0] + .7152 * rgb[1] + .0722 * rgb[2];
}
function contrast(a, b) {
    const x = luminance(a), y = luminance(b);
    return (Math.max(x, y) + .05) / (Math.min(x, y) + .05);
}

/** Uses the existing real app fixtures/runner; no extra server or native workload. */
export async function checkWorkbenchPresentation(page) {
    await page.locator('pc-emulator [data-craft-panel="essence"]').click();
    await page.locator('pc-emulator [data-mechanic="essence-type"]').filter({hasText: /^Woe$/}).click();
    await page.locator('pc-emulator [data-mechanic="essence-key"]').filter({hasText: /^Screaming$/}).click();
    await page.evaluate(() => document.fonts.ready);
    const primary = page.locator('pc-emulator [data-config-action="essence"]');
    await primary.locator('xpath=self::button[not(@disabled)]').waitFor();
    const styles = await primary.evaluate(element => {
        const style = getComputedStyle(element), root = getComputedStyle(document.documentElement);
        const probe = document.createElement('span');
        document.body.append(probe);
        const color = token => { probe.style.color = root.getPropertyValue(token); return getComputedStyle(probe).color; };
        const result = {text: style.color, fill: style.backgroundColor, radius: style.borderRadius,
            page: color('--pc-surface-page'), panel: color('--pc-surface-panel'), card: color('--pc-surface-card'),
            body: color('--pc-text-body'), muted: color('--pc-text-muted'), focus: color('--pc-focus'),
            boundary: color('--pc-border-control'), field: color('--pc-surface-field')};
        probe.remove();
        return result;
    });
    assert.equal(styles.radius, '4px');
    assert.ok(contrast(styles.text, styles.fill) >= 4.5, 'Execution text contrast');
    for (const surface of [styles.page, styles.panel, styles.card]) {
        assert.ok(contrast(styles.body, surface) >= 4.5);
        assert.ok(contrast(styles.muted, surface) >= 4.5);
        assert.ok(contrast(styles.focus, surface) >= 3);
    }
    assert.ok(contrast(styles.boundary, styles.field) >= 3);
    await primary.hover();
    const hover = await primary.evaluate(element => ({text: getComputedStyle(element).color, fill: getComputedStyle(element).backgroundColor}));
    assert.ok(contrast(hover.text, hover.fill) >= 4.5);
    await primary.focus();
    await page.keyboard.press('Tab');
    await page.keyboard.press('Shift+Tab');
    assert.equal(await primary.evaluate(element => element.matches(':focus-visible')), true);
    assert.equal(await primary.evaluate(element => getComputedStyle(element).outlineWidth), '2px');
    const slots = await page.locator('pc-emulator .pc-mod-explicit-ledger .pc-mod-slot').evaluateAll(elements =>
        elements.map(element => ({height: element.getBoundingClientRect().height, radius: getComputedStyle(element).borderRadius})));
    assert.ok(slots.every(slot => slot.height === 72 && slot.radius === '0px'));
    assert.ok(await page.evaluate(() => document.fonts.check('400 14px "Noto Sans"') && document.fonts.check('700 14px "Noto Sans"')));
    return {primaryContrast: contrast(styles.text, styles.fill), hoverContrast: contrast(hover.text, hover.fill),
        mutedCardContrast: contrast(styles.muted, styles.card), focusCardContrast: contrast(styles.focus, styles.card),
        fieldBoundaryContrast: contrast(styles.boundary, styles.field), keyboardFocus: true, stableSquareSlots: true};
}

/** Optional evidence capture during the existing smoke run, in its fresh profile. */
export async function captureUiCheckpoint(page, name) {
    const directory = process.env.POECRAFT_UI_CAPTURE_DIR;
    if (!directory) return;
    await mkdir(directory, {recursive: true});
    const viewport = page.viewportSize();
    await page.setViewportSize({width: 1536, height: 864});
    await page.evaluate(() => document.fonts.ready);
    await page.screenshot({path: resolve(directory, `${page.context().browser().browserType().name()}-${name}.png`)});
    await page.setViewportSize(viewport);
}
