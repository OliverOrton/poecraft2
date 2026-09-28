import assert from 'node:assert/strict';

/** Check comparison geometry and tool isolation, without taking visual-approval screenshots. */
export async function checkCalculatorLayout(page) {
    await page.waitForFunction(() => document.querySelector('pc-calculator')?.item && !document.querySelector('pc-calculator')?.busy);
    const viewport = page.viewportSize();
    for (const size of [{width: 1366, height: 768}, {width: 1920, height: 1080}]) {
        await page.setViewportSize(size);
        await page.waitForFunction(() => Math.abs(document.querySelector('.pc-calculator').getBoundingClientRect().bottom - innerHeight) < 1);
        const measure = () => page.locator('.pc-calc-context-card').evaluateAll(cards => cards.map(card => {
            const rect = card.getBoundingClientRect();
            const scroll = card.querySelector('.pc-calc-item-scroll');
            return {x: rect.x, y: rect.y, right: rect.right, bottom: rect.bottom,
                itemTop: scroll.getBoundingClientRect().top,
                slotHeights: [...card.querySelectorAll('.pc-mod-slot')].map(slot => slot.getBoundingClientRect().height)};
        }));
        const before = await measure();
        assert.equal(before.length, 2);
        assert.equal(before[0].y, before[1].y);
        assert.equal(before[0].itemTop, before[1].itemTop);
        assert.ok(before[0].right <= before[1].x && before[1].right <= size.width);
        assert.ok(before.every(card => card.y >= 0 && card.bottom <= size.height));
        assert.ok(before.every(card => card.slotHeights.length === 6 && card.slotHeights.every(height => height === 96)));
        await page.locator('[data-calc-tool="craft"]').click();
        await page.locator('pc-calculator [data-craft-panel="harvest"]').click();
        await page.locator('[data-calc-pane="craft"]').evaluate(pane => { pane.scrollTop = pane.scrollHeight; });
        assert.deepEqual(await measure(), before, 'Tool content must not move the item comparison');
        await page.locator('[data-calc-tool="modifiers"]').click();
    }
    await page.setViewportSize(viewport);
}
