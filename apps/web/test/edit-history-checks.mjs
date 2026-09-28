import assert from 'node:assert/strict';

const ready = page => page.waitForFunction(() => {
    const emulator = document.querySelector('pc-emulator');
    return emulator?.item && !emulator.busy;
});
const spend = page => page.evaluate(() => structuredClone(document.querySelector('pc-emulator').spend));
const itemState = page => page.evaluate(async () => {
    const emulator = document.querySelector('pc-emulator');
    return {base: emulator.base, level: emulator.itemLevel, state: await emulator.client.exportItem(emulator.item)};
});
async function emulatorCommand(page, command) {
    await page.locator(`pc-emulator [data-cmd="${command}"]:not(:disabled)`).click();
    await ready(page);
}
async function craft(page, action) {
    await page.locator('pc-emulator [data-craft-panel="basic"]').click();
    await page.locator(`pc-emulator [data-simple-action="${action}"]:not(:disabled)`).click();
    await ready(page);
}

/** Exercise the actual native item snapshots, including their compound Imprint state. */
export async function checkEmulatorHistory(page) {
    await ready(page);
    const rare = await itemState(page);
    assert.deepEqual(await spend(page), {counts: {alchemy: 1}, untracked: false});
    await page.keyboard.press('Control+z');
    await ready(page);
    const normal = await itemState(page);
    assert.equal(normal.state.rarity, 0);
    assert.deepEqual(await spend(page), {counts: {}, untracked: false});
    await page.reload();
    await ready(page);
    assert.deepEqual(await itemState(page), normal);
    await emulatorCommand(page, 'redo');
    assert.deepEqual(await itemState(page), rare);
    assert.deepEqual(await spend(page), {counts: {alchemy: 1}, untracked: false});
    await page.locator('pc-emulator [data-history-index="0"]').click();
    await ready(page);
    assert.deepEqual(await itemState(page), normal);
    await page.locator('pc-emulator [data-history-index="1"]').click();
    await ready(page);
    assert.deepEqual(await itemState(page), rare);
    assert.deepEqual(await spend(page), {counts: {alchemy: 1}, untracked: false});

    // A base change can also be undone; session-local modifier IDs must follow their base.
    await emulatorCommand(page, 'change-base');
    await page.locator('pc-emulator .pc-bp-class').selectOption({label: 'Helmet'});
    await page.locator('pc-emulator .pc-bp-sub').selectOption({label: 'Armour / Energy Shield'});
    await page.locator('pc-emulator .pc-bp-base').selectOption({label: 'Archdemon Crown'});
    await page.locator('pc-emulator .pc-bp-confirm:not(:disabled)').click();
    await ready(page);
    const crown = await itemState(page);
    assert.match(crown.base, /HelmetStrIntRitual3$/);
    await emulatorCommand(page, 'undo');
    assert.deepEqual(await itemState(page), rare);
    assert.deepEqual(await spend(page), {counts: {alchemy: 1}, untracked: false});
    await emulatorCommand(page, 'redo');
    assert.deepEqual(await itemState(page), crown);
    await emulatorCommand(page, 'undo');
    await emulatorCommand(page, 'create');
    assert.equal(await page.locator('pc-emulator [data-cmd="redo"]').isDisabled(), true);
    await emulatorCommand(page, 'undo');
    assert.deepEqual(await itemState(page), rare);
    assert.deepEqual(await spend(page), {counts: {alchemy: 1}, untracked: false});
    await emulatorCommand(page, 'redo');
    await craft(page, 'transmute');
    const magic = await itemState(page);
    const magicSpend = await spend(page);
    assert.deepEqual(magicSpend.counts, {alchemy: 1, transmute: 1});
    await page.locator('pc-emulator [data-craft-panel="bestiary"]').click();
    await page.locator('pc-emulator [data-bestiary-action="bestiary:imprint"]:not(:disabled)').click();
    await ready(page);
    const imprint = await itemState(page);
    assert.equal(imprint.state.bestiary.checkpoint_present, true);
    const imprintSpend = await spend(page);
    assert.equal(imprintSpend.counts['beast:rare'], 3);
    assert.equal(imprintSpend.counts['beast:craicic-croaker'], 1);
    await emulatorCommand(page, 'undo');
    assert.deepEqual(await itemState(page), magic);
    assert.deepEqual(await spend(page), magicSpend);
    await emulatorCommand(page, 'redo');
    assert.deepEqual(await itemState(page), imprint);
    assert.deepEqual(await spend(page), imprintSpend);
    await craft(page, 'regal');
    await page.locator('pc-emulator [data-craft-panel="bestiary"]').click();
    await page.locator('pc-emulator [data-bestiary-action="bestiary:restore_imprint"]:not(:disabled)').click();
    await ready(page);
    assert.equal((await itemState(page)).state.bestiary.checkpoint_present, false);
    await emulatorCommand(page, 'undo');
    assert.equal((await itemState(page)).state.bestiary.checkpoint_present, true);
    await page.locator('pc-emulator [data-history-index="1"]').click();
    await ready(page);
    assert.deepEqual(await itemState(page), rare);
    assert.deepEqual(await spend(page), {counts: {alchemy: 1}, untracked: false});
    await craft(page, 'chaos');
    assert.deepEqual(await spend(page), {counts: {alchemy: 1, chaos: 1}, untracked: false});
    await page.locator('pc-craft-spend summary').click();
    await page.locator('[data-spend-key="alchemy"] input').fill('2');
    await page.locator('[data-spend-key="chaos"] input').fill('3');
    assert.equal(await page.locator('[data-spend-total]').innerText(), '5c');
    await page.locator('.pc-economy-trigger').click();
    const profile = await page.locator('[data-profile][aria-checked="true"]').getAttribute('data-profile');
    await page.getByRole('menuitemradio').filter({hasText: 'Custom / manual'}).click();
    await page.waitForFunction(() => document.querySelector('[data-spend-total]')?.textContent === '0c + unpriced');
    assert.match(await page.locator('pc-craft-spend').innerText(), /2 material prices missing/);
    await page.locator('[data-spend-key="alchemy"] input').fill('2');
    await page.locator('[data-spend-key="chaos"] input').fill('3');
    assert.equal(await page.locator('[data-spend-total]').innerText(), '5c');
    await page.locator('.pc-economy-trigger').click();
    await page.locator(`[data-profile="${profile}"]`).click();
    await page.locator('.pc-economy-popover').waitFor({state: 'detached'});
    assert.deepEqual(await spend(page), {counts: {alchemy: 1, chaos: 1}, untracked: false});
    await page.locator('pc-craft-spend summary').click();
    assert.equal(await page.locator('pc-emulator [data-cmd="redo"]').isDisabled(), true);

    // Recover an older on-disk draft without inventing costs or dropping Undo.
    const current = await itemState(page);
    const priorLength = await page.evaluate(async () => {
        const emulator = document.querySelector('pc-emulator');
        const request = indexedDB.open('poecraft');
        const db = await new Promise(resolve => { request.onsuccess = () => resolve(request.result); });
        const read = db.transaction('drafts').objectStore('drafts').get(emulator.docId);
        const draft = await new Promise(resolve => {read.onsuccess = () => resolve(read.result);});
        for (const frame of draft.undoHistory.entries) { delete frame.spend; delete frame.entry.costKeys; }
        const transaction = db.transaction('drafts', 'readwrite');
        transaction.objectStore('drafts').put(draft);
        await new Promise((resolve, reject) => {transaction.oncomplete = resolve; transaction.onabort = reject;});
        db.close();
        return draft.undoHistory.entries.length;
    });
    await page.reload();
    await ready(page);
    assert.equal(await page.evaluate(() => document.querySelector('pc-emulator').undoHistory.length), priorLength);
    assert.deepEqual(await spend(page), {counts: {}, untracked: true});
    assert.match(await page.locator('pc-craft-spend').innerText(), /Some history steps have no cost data/);
    await emulatorCommand(page, 'undo');
    await emulatorCommand(page, 'redo');
    assert.deepEqual(await itemState(page), current);
    await craft(page, 'chaos');
    assert.deepEqual(await spend(page), {counts: {chaos: 1}, untracked: true});
}

const graph = page => page.evaluate(() => JSON.parse(JSON.stringify(document.querySelector('pc-strategy-editor').strategy)));
const strategyCommand = (page, command) => page.locator(`pc-strategy-editor [data-cmd="${command}"]:not(:disabled)`).click();
async function drag(page, source, target, cancel = false) {
    const a = await source.boundingBox();
    const b = await target.boundingBox();
    assert.ok(a && b);
    await page.mouse.move(a.x + a.width / 2, a.y + a.height / 2);
    await page.mouse.down();
    await page.mouse.move(b.x + b.width / 2, b.y + b.height / 2, {steps: 12});
    if (cancel) await page.keyboard.press('Escape');
    await page.mouse.up();
}

export async function checkStrategyHistory(page) {
    const before = await graph(page);
    // The last edit was the nested condition's tier. It is independently reversible.
    await strategyCommand(page, 'undo');
    assert.match(JSON.stringify((await graph(page)).edges[0].condition), /"min_tier":2/);
    await strategyCommand(page, 'redo');
    assert.deepEqual(await graph(page), before);

    await page.locator('.pc-edge-routing summary').click();
    await page.locator('[data-field="edge-label"]').pressSequentially('Keep these conditions');
    await page.locator('[data-field="edge-label"]').press('Tab');
    await strategyCommand(page, 'undo');
    assert.equal((await graph(page)).edges[0].label, '');
    await strategyCommand(page, 'redo');
    assert.equal((await graph(page)).edges[0].label, 'Keep these conditions');
    await page.locator('.pc-edge-routing summary').click();
    await page.locator('[data-field="edge-priority"]').fill('7');
    await page.locator('[data-field="edge-priority"]').press('Tab');
    await strategyCommand(page, 'fit-view');
    const authored = await graph(page);
    const edge = authored.edges[0];
    const startId = authored.start_node_id;
    const restartId = authored.nodes.find(node => node.operation?.type === 'restart').id;
    const imprintId = authored.nodes.find(node => node.operation?.type === 'bestiary:imprint').id;
    const node = id => page.locator(`pc-strategy-node[data-node-id="${id}"]`);
    const handle = endpoint => page.locator(`pc-edge-layer [data-edge-end="${endpoint}"]`);
    await drag(page, handle('to'), node(imprintId).locator('.pc-node-input'));
    const changed = await graph(page);
    assert.deepEqual(changed.edges[0], {...edge, to: imprintId});
    assert.equal(changed.edges.length, authored.edges.length);
    await strategyCommand(page, 'undo');
    assert.deepEqual((await graph(page)).edges, authored.edges);
    await strategyCommand(page, 'redo');
    assert.deepEqual((await graph(page)).edges, changed.edges);
    await drag(page, handle('from'), node(restartId).locator('.pc-node-output'));
    assert.deepEqual((await graph(page)).edges[0], {...edge, from: restartId, to: imprintId});
    await strategyCommand(page, 'undo');
    await drag(page, handle('to'), node(startId).locator('.pc-node-input'), true);
    assert.deepEqual((await graph(page)).edges, changed.edges);
    assert.equal(await page.locator('.pc-edge-preview').count(), 0);
    assert.equal(await page.locator('pc-strategy-editor [data-cmd="redo"]').isEnabled(), true);
    // Releasing an endpoint on empty space also preserves the complete edge and redo branch.
    await drag(page, handle('to'), page.locator('.pc-board-hint'));
    assert.deepEqual((await graph(page)).edges, changed.edges);
    await page.locator('.pc-board-viewport').focus();
    await page.keyboard.press('Control+z');
    assert.deepEqual((await graph(page)).edges, authored.edges);
    await page.keyboard.press('Control+Shift+z');
    assert.deepEqual((await graph(page)).edges, changed.edges);

    // Many pointer moves must commit one node-position edit. Zoom is preserved by Undo.
    const beforeMove = await graph(page);
    const header = await node(restartId).locator('.pc-node-header').boundingBox();
    assert.ok(header);
    await page.mouse.move(header.x + 30, header.y + 12);
    await page.mouse.down();
    await page.mouse.move(header.x + 70, header.y + 120, {steps: 20});
    await page.mouse.up();
    const afterMove = await graph(page);
    assert.notDeepEqual(afterMove.nodes, beforeMove.nodes);
    await page.mouse.wheel(0, 100);
    await page.waitForFunction(previous => document.querySelector('pc-strategy-editor').strategy.ui.viewport.zoom !== previous,
        beforeMove.ui.viewport.zoom);
    const viewport = (await graph(page)).ui.viewport;
    await strategyCommand(page, 'undo');
    assert.deepEqual((await graph(page)).nodes, beforeMove.nodes);
    assert.deepEqual((await graph(page)).ui.viewport, viewport);
    await strategyCommand(page, 'redo');
    assert.deepEqual((await graph(page)).nodes, afterMove.nodes);

    // Deletion restores the node and all its incident edges together.
    await node(imprintId).locator('.pc-node-title').click();
    const beforeDelete = await graph(page);
    await strategyCommand(page, 'delete');
    assert.equal((await graph(page)).edges.length, 0);
    await strategyCommand(page, 'undo');
    assert.deepEqual(await graph(page), beforeDelete);
    // Undo returns to the same cursor and graph as the pre-deletion draft. Wait
    // for the complete history so that an older draft without Redo cannot pass.
    await page.evaluate(async () => {
        const request = indexedDB.open('poecraft');
        const db = await new Promise(resolve => { request.onsuccess = () => resolve(request.result); });
        const editor = document.querySelector('pc-strategy-editor');
        try {
            for (let attempt = 0; attempt < 100; attempt++) {
                const read = db.transaction('drafts').objectStore('drafts').get(editor.docId);
                const draft = await new Promise(resolve => { read.onsuccess = () => resolve(read.result); });
                if (JSON.stringify(draft?.undoHistory) === JSON.stringify(editor.undoHistory.export()) &&
                    JSON.stringify(draft.strategy) === JSON.stringify(editor.strategy)) return;
                await new Promise(resolve => setTimeout(resolve, 25));
            }
            throw new Error('Strategy draft did not persist');
        } finally { db.close(); }
    });
    await page.reload();
    await page.locator('pc-strategy-editor [data-cmd="redo"]:not(:disabled)').waitFor();
    assert.deepEqual(await graph(page), beforeDelete);
    await strategyCommand(page, 'redo');
    assert.equal((await graph(page)).edges.length, 0);
    await strategyCommand(page, 'undo');
    await page.locator('[data-palette="terminal:success"]').dblclick();
    assert.equal(await page.locator('pc-strategy-editor [data-cmd="redo"]').isDisabled(), true);
}
