import assert from 'node:assert/strict';

// Source-reviewed at owner cfea56c0 and combined d0f5df2f. These helpers require the integrator's
// combined components/artifacts and an explicit retained graph/trace fixture.
// They are not wired into the qualified legacy/dock scopes, and never run native
// crafting, Simulator, planner, acquisition or pricing operations themselves.
const NODE_WIDTH = 210, PORT_SIZE = 14;
function inputs(node) {
    if (node.source_only) return [];
    return node.operation?.type === 'recombination'
        ? [{id: 'input_a', y: 36, label: 'Item A / execution'}, {id: 'input_b', y: 78, label: 'Item B'}]
        : [{id: 'input', y: 54, label: 'Input'}];
}
function outputs(node) {
    return node.kind === 'terminal' ? []
        : [{id: 'output', y: 54, label: node.source_only ? 'Paid item output' : 'Output'}];
}

/** Pure rendered connector contract, compared with the caller's exact graph. */
export async function checkIntegratedConnectorGeometry(page, graph, {
    boardSelector = 'pc-strategy-board:visible', requireFullVocabulary = true,
} = {}) {
    assert.ok(graph.nodes.length > 0, 'Connector fixture is nonempty');
    const coverage = {
        sourceOnly: graph.nodes.filter(node => node.source_only).length,
        recombination: graph.nodes.filter(node => node.operation?.type === 'recombination').length,
        terminal: graph.nodes.filter(node => node.kind === 'terminal').length,
        ordinary: graph.nodes.filter(node => !node.source_only && node.operation?.type !== 'recombination' && node.kind !== 'terminal').length,
    };
    if (requireFullVocabulary) {
        for (const [kind, count] of Object.entries(coverage)) assert.ok(count > 0, `Fixture includes ${kind}`);
    }
    const board = page.locator(boardSelector);
    assert.equal(await board.count(), 1);
    const rendered = await board.locator('pc-strategy-node').evaluateAll(nodes => nodes.map(node => ({
        id: node.dataset.nodeId, left: Number.parseFloat(node.style.left), top: Number.parseFloat(node.style.top),
        width: getComputedStyle(node).width,
        ports: [...node.querySelectorAll('.pc-node-port')].map(port => {
            const style = getComputedStyle(port);
            return {id: port.dataset.portId, direction: port.classList.contains('pc-node-input') ? 'input' : 'output',
                top: style.top, width: style.width, height: style.height, radius: style.borderRadius,
                label: port.getAttribute('aria-label')};
        }),
    })));
    assert.deepEqual(rendered.map(node => node.id).sort(), graph.nodes.map(node => node.id).sort());
    for (const node of graph.nodes) {
        const actual = rendered.find(entry => entry.id === node.id);
        assert.equal(actual.width, `${NODE_WIDTH}px`);
        assert.equal(actual.left, node.position.x); assert.equal(actual.top, node.position.y);
        const expected = [...inputs(node).map(port => ({...port, direction: 'input'})),
            ...outputs(node).map(port => ({...port, direction: 'output'}))];
        assert.deepEqual(actual.ports.map(port => [port.direction, port.id]), expected.map(port => [port.direction, port.id]));
        for (const port of expected) {
            const found = actual.ports.find(entry => entry.id === port.id);
            assert.equal(found.top, `${port.y - PORT_SIZE / 2}px`);
            assert.equal(found.width, `${PORT_SIZE}px`); assert.equal(found.height, `${PORT_SIZE}px`);
            assert.equal(found.radius, '50%'); assert.equal(found.label, port.label);
        }
    }
    return {scope: 'resource-v1-connectors', nodes: graph.nodes.length, coverage, fullVocabularyRequired: requireFullVocabulary,
        nodeWidth: NODE_WIDTH, portSize: PORT_SIZE,
        inputA: {center: 36, top: 29}, inputB: {center: 78, top: 71}, ordinary: {center: 54, top: 47},
        sourceInputs: 0, terminalOutputs: 0};
}

/** Caller selects this exact edge. Compare SVG user coordinates, not zoomed
 * screen rectangles, with the retained graph; no new geometry tolerance.
 */
export async function checkIntegratedEdgeGeometry(page, graph, edgeId, boardSelector = 'pc-strategy-board:visible') {
    const edge = graph.edges.find(entry => entry.id === edgeId);
    assert.ok(edge, 'Fixture retains the exact edge ID');
    const from = graph.nodes.find(node => node.id === edge.from), to = graph.nodes.find(node => node.id === edge.to);
    const destination = inputs(to).find(port => port.id === edge.to_port);
    assert.ok(destination, 'Fixture uses an explicit valid destination port');
    assert.equal(edge.from_port, 'output');
    const expected = {from: {x: from.position.x + NODE_WIDTH, y: from.position.y + 54},
        to: {x: to.position.x, y: to.position.y + destination.y}};
    const layer = page.locator(boardSelector).locator('pc-edge-layer');
    // Use dataset matching, preserving the exact authored ID without CSS escaping.
    const path = layer.locator('.pc-edge-group .pc-edge-path');
    const measured = await path.evaluateAll((paths, id) => {
        const target = paths.find(path => path.closest('[data-edge-id]')?.dataset.edgeId === id);
        if (!target) return null;
        const a = target.getPointAtLength(0), b = target.getPointAtLength(target.getTotalLength());
        return {from: {x: a.x, y: a.y}, to: {x: b.x, y: b.y}, itemSupply: target.classList.contains('is-item-supply'),
            dash: getComputedStyle(target).strokeDasharray.match(/[\d.]+/g)?.map(Number) ?? []};
    }, edgeId);
    assert.ok(measured); assert.deepEqual(measured.from, expected.from); assert.deepEqual(measured.to, expected.to);
    assert.equal(measured.itemSupply, edge.kind === 'item');
    if (edge.kind === 'item') assert.deepEqual(measured.dash, [5, 4]);
    const handles = await layer.locator('.pc-edge-handles [data-edge-end]').evaluateAll((elements, id) =>
        elements.filter(element => element.closest('[data-edge-id]')?.dataset.edgeId === id).map(element => ({
            end: element.dataset.edgeEnd, x: Number(element.getAttribute('cx')), y: Number(element.getAttribute('cy')),
            radius: Number(element.getAttribute('r')),
        })), edgeId);
    assert.deepEqual(handles, [{end: 'from', x: expected.from.x + 18, y: expected.from.y, radius: 7},
        {end: 'to', x: expected.to.x - 18, y: expected.to.y, radius: 7}]);
    return {scope: 'integrated-edge', edgeId, fromPort: edge.from_port, toPort: edge.to_port,
        kind: edge.kind, endpoints: expected, reconnectOffset: 18, reconnectRadius: 7};
}

/** Current owner JSON presentation contract. Native resource envelopes remain
 * complete and distinct from entry.item; no inferred total or merged receipt.
 */
export async function checkIntegratedTraceAuthority(page, entry, detailSelector = 'pc-run-trace:visible .pc-trace-detail') {
    const detail = page.locator(detailSelector), snapshots = detail.locator(':scope > details');
    const actual = entry.resources?.find(resource => resource.active_output);
    const first = snapshots.first();
    assert.equal(await first.locator(':scope > summary').textContent(), actual
        ? `Actual output item \u2014 ${actual.base_key ?? actual.resource_id}` : 'Current item snapshot');
    assert.deepEqual(JSON.parse(await first.locator(':scope > pre').textContent()), actual ?? entry.item);
    if (entry.resources?.length) {
        assert.equal(await snapshots.nth(1).locator(':scope > summary').textContent(), 'Resource inventory');
        assert.deepEqual(JSON.parse(await snapshots.nth(1).locator(':scope > pre').textContent()), entry.resources);
    } else assert.equal(await snapshots.count(), 1);
    const values = await detail.locator(':scope > dl > div').evaluateAll(rows => Object.fromEntries(rows.map(row =>
        [row.querySelector('dt').textContent, row.querySelector('dd').textContent])));
    assert.equal(values.Node, entry.node_id); assert.equal(values.Actions, String(entry.cumulative_actions));
    assert.equal(values['Matched edge'], entry.matched_edge_id || '\u2014');
    assert.equal(values['Known cost'], entry.known_cumulative_cost.toFixed(2) + (entry.cost_complete ? '' : ' +'));
    return {scope: 'integrated-trace-authority', activeResourceId: actual?.resource_id ?? null,
        activeIdentity: actual?.identity ?? null, inventoryCount: entry.resources?.length ?? 0,
        cumulativeCostFromNative: true, fullInventoryAndReceiptsRetained: true};
}

/** Real trace controls retain exact highlight identities and boundary states.
 * Caller has already selected this trace; its native entry order is unchanged.
 */
export async function checkIntegratedTraceNavigation(page, trace, traceSelector = 'pc-run-trace:visible') {
    assert.ok(trace.entries.length >= 2, 'Navigation fixture retains at least two steps');
    const host = page.locator(traceSelector);
    await host.evaluate(element => {
        if (element.uiAssertionHighlightListener) throw new Error('Trace assertion listener already active');
        element.uiAssertionHighlights = [];
        element.uiAssertionHighlightListener = event => element.uiAssertionHighlights.push(structuredClone(event.detail));
        element.addEventListener('trace-highlight', element.uiAssertionHighlightListener);
    });
    const expectHighlight = async index => {
        const entries = trace.entries.slice(0, index + 1);
        assert.deepEqual(await host.evaluate(element => element.uiAssertionHighlights.at(-1)), {
            nodeIds: entries.map(entry => entry.node_id),
            edgeIds: entries.map(entry => entry.matched_edge_id).filter(Boolean),
            activeNodeId: entries.at(-1).node_id,
        });
    };
    try {
        await host.locator('[data-step="0"]').click(); await expectHighlight(0);
        assert.equal(await host.locator('[data-cmd="prev"]').isDisabled(), true);
        const next = host.locator('[data-cmd="next"]'); assert.equal(await next.isDisabled(), false);
        await next.focus(); await page.keyboard.press('Tab'); await page.keyboard.press('Shift+Tab');
        assert.equal(await next.evaluate(element => element.matches(':focus-visible')), true);
        assert.equal(await next.evaluate(element => getComputedStyle(element).outlineWidth), '2px');
        await page.keyboard.press('Enter'); await expectHighlight(1);
        const last = trace.entries.length - 1;
        await host.locator(`[data-step="${last}"]`).click(); await expectHighlight(last);
        assert.equal(await host.locator('[data-cmd="next"]').isDisabled(), true);
        assert.equal(await host.locator('[data-cmd="prev"]').isDisabled(), false);
        return {scope: 'integrated-trace-navigation', exactHighlightIds: true, keyboardFocus: true,
            keyboardNext: true, disabledBoundaries: true};
    } finally {
        await host.evaluate(element => {
            element.removeEventListener('trace-highlight', element.uiAssertionHighlightListener);
            delete element.uiAssertionHighlightListener; delete element.uiAssertionHighlights;
        });
    }
}

function cardFacts(model) {
    const rows = side => (model[side] ?? []).map(mod => ({key: mod.key, tierIndex: mod.tierIndex,
        textLines: mod.textLines, classificationTags: mod.classificationTags, fractured: mod.fractured,
        crafted: mod.crafted, veiled: mod.veiled, rollValues: mod.rollValues}));
    return {baseKey: model.baseKey, baseName: model.baseName, itemLevel: model.itemLevel,
        rarity: model.rarity, itemFlags: model.itemFlags, memoryStrands: model.memoryStrands,
        lifecycle: model.lifecycle, influences: model.influences, clusterEnchantmentText: model.clusterEnchantmentText,
        maxPrefix: model.maxPrefix, maxSuffix: model.maxSuffix,
        prefixes: rows('prefixes'), suffixes: rows('suffixes'), implicits: rows('implicits'), enchantments: rows('enchantments')};
}

/** Proposed shared trace-card adapter contract, absent from cfea56c0/d0f5df2f.
 * nativeModel must come from readItemCard using this exact resource's native
 * base/level/item, not a template. Caller supplies the adapter wrapper selector.
 * Session-local mod IDs are deliberately not compared across imported sessions.
 */
export async function checkIntegratedTraceItemCard(page, {resource, nativeModel, wrapperSelector}) {
    assert.ok(resource.identity && resource.base_key && Number.isInteger(resource.item_level) && resource.item);
    assert.equal(nativeModel.baseKey, resource.base_key); assert.equal(nativeModel.itemLevel, resource.item_level);
    const wrapper = page.locator(wrapperSelector); assert.equal(await wrapper.count(), 1);
    assert.equal(await wrapper.getAttribute('data-resource-id'), resource.resource_id);
    assert.equal(await wrapper.getAttribute('data-resource-identity'), resource.identity);
    const card = wrapper.locator('pc-mod-list'); await card.locator('[data-mode="concrete"]').waitFor();
    const model = await card.evaluate(element => structuredClone(element.model));
    assert.deepEqual(cardFacts(model), cardFacts(nativeModel)); assert.equal(model.readOnly, true);
    assert.equal(await card.locator('.pc-item-fracture-mod, .pc-item-remove-mod, .pc-item-add-mod').count(), 0);
    assert.equal(await card.locator('[data-influence-context="required"], [data-influence-context="exact"], [data-influence-context="any"]').count(), 0);
    assert.deepEqual(await card.locator('.pc-item-card-header [data-influence-context="actual"]').allTextContents(), nativeModel.influences);
    return {scope: 'integrated-trace-card', resourceId: resource.resource_id, identity: resource.identity,
        nativeFactsPreserved: true, readOnly: true};
}
