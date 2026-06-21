import assert from "node:assert/strict";

import {
    cloneStrategy,
    compileConditionTree,
    createBlankStrategy,
    createDefaultStrategy,
    defaultLeafCondition,
    createStrategyFromItemSnapshot,
    parseConditionTree,
    validateStrategy,
    type ConditionGroupNode,
    type ConditionLeaf,
} from "../src/app/strategy-model";

{
    const strategy = createBlankStrategy("Metadata/Items/Test", 82);
    assert.equal(strategy.base_state.base_key, "Metadata/Items/Test");
    assert.equal(strategy.base_state.item_level, 82);
    assert.equal(strategy.start_node_id, "");
    assert.deepEqual(strategy.nodes, []);
    assert.deepEqual(strategy.edges, []);
    assert.equal(defaultLeafCondition("has_mod_family").min_tier, 1);
    console.log("  ok - new editor strategies start with a base and blank board");
}

{
    const strategy = createDefaultStrategy();
    const issues = validateStrategy(strategy);
    assert.deepEqual(
        issues.filter((issue) => issue.severity === "error"),
        [],
        "the Phase 10 chaos-repeat graph should be editor-valid",
    );
    assert.equal(strategy.edges.find((edge) => edge.id === "repeat")?.is_default, true);
    console.log("  ok - editor creates the Phase 10 chaos-repeat graph");
}

{
    const strategy = createDefaultStrategy();
    strategy.nodes[1].position = { x: 913, y: 417 };
    strategy.edges[1].condition = {
        type: "all",
        conditions: [
            { type: "rarity_is", rarity: "rare" },
            { type: "open_suffix_count", min: 1, max: 3 },
        ],
    };
    const reopened = JSON.parse(JSON.stringify(cloneStrategy(strategy)));
    assert.deepEqual(reopened.nodes[1].position, { x: 913, y: 417 });
    assert.deepEqual(reopened.edges[1].condition, strategy.edges[1].condition);
    console.log("  ok - strategy layout and semantics round-trip");
}

{
    const invalid = createDefaultStrategy();
    invalid.edges = invalid.edges.filter((edge) => edge.to !== "success");
    const issues = validateStrategy(invalid);
    assert.ok(issues.some((issue) => issue.code === "unreachable"));
    assert.ok(issues.some((issue) => issue.code === "no-terminal"));
    assert.ok(issues.some((issue) => issue.code === "no-terminal-path"));
    console.log("  ok - invalid graphs surface actionable warnings");
}

{
    const imported = createStrategyFromItemSnapshot(
        {
            base: "Metadata/Items/Test",
            itemLevel: 84,
            rarity: "rare",
            state: {
                rarity: 2,
                quality: 20,
                item_flags: 1,
                generic_influence_bits: 4,
                prefixes: [{ mod_id: 3, flags: 1 }],
                suffixes: [{ mod_id: 7, flags: 2 }],
            },
        },
        (id) => ({ 3: "PrefixKey", 7: "SuffixKey" })[id],
    );
    assert.equal(imported.base_state.base_key, "Metadata/Items/Test");
    assert.deepEqual(imported.base_state.prefixes, [
        { mod_key: "PrefixKey", fractured: true, crafted: undefined },
    ]);
    assert.deepEqual(imported.base_state.suffixes, [
        { mod_key: "SuffixKey", fractured: undefined, crafted: true },
    ]);
    assert.equal(imported.nodes.length, 1);
    assert.equal(imported.nodes[0].kind, "start");
    assert.deepEqual(imported.edges, []);
    assert.ok(
        validateStrategy(imported).some((issue) => issue.code === "dead-end"),
    );
    console.log("  ok - Emulator item imports as a stable-key start state only");
}

{
    // A simple leaf condition parses into a one-child root group and compiles
    // back to the bare leaf (single-child ALL/ANY groups are unwrapped).
    const leaf = { type: "has_mod_group", group: "IncreasedLife" };
    const tree = parseConditionTree(leaf);
    assert.equal(tree.kind, "group");
    assert.equal(tree.children.length, 1);
    assert.deepEqual(compileConditionTree(tree), leaf);
    console.log("  ok - leaf condition round-trips through the builder tree");
}

{
    // ALL / ANY / AT LEAST / NOT all survive a parse -> compile round-trip.
    const samples = [
        {
            type: "all",
            conditions: [
                { type: "has_mod_group", group: "IncreasedLife" },
                { type: "rarity_is", rarity: "rare" },
            ],
        },
        {
            type: "any",
            conditions: [
                { type: "open_prefix_count", min: 1, max: 3 },
                { type: "open_suffix_count", min: 1, max: 3 },
            ],
        },
        {
            type: "at_least",
            count: 2,
            conditions: [
                { type: "has_mod_group", group: "A" },
                { type: "has_mod_group", group: "B" },
                { type: "has_mod_group", group: "C" },
            ],
        },
        {
            type: "all",
            conditions: [
                {
                    type: "has_mod_family",
                    family_mod_key: "Metadata/Mods/TestFamilyTier1",
                    min_tier: 3,
                },
                {
                    type: "any",
                    conditions: [
                        { type: "open_prefix_count", min: 1, max: 3 },
                        { type: "open_suffix_count", min: 1, max: 3 },
                    ],
                },
            ],
        },
    ];
    for (const sample of samples) {
        assert.deepEqual(compileConditionTree(parseConditionTree(sample)), sample);
    }
    console.log("  ok - composite conditions round-trip through the builder tree");
}

{
    const strategy = createDefaultStrategy();
    strategy.edges[1].condition = {
        type: "has_mod_family",
        family_mod_key: "Metadata/Mods/TestFamilyTier1",
        min_tier: 2,
    };
    assert.equal(
        validateStrategy(strategy).filter((issue) => issue.severity === "error")
            .length,
        0,
    );
    strategy.edges[1].condition.min_tier = -1;
    assert.ok(
        validateStrategy(strategy).some(
            (issue) => issue.code === "condition-tier",
        ),
    );
    console.log("  ok - modifier family minimum tier validates");
}

{
    const fractured = {
        type: "has_mod_family",
        family_mod_key: "Metadata/Mods/TestFamilyTier1",
        family_label: "Test family",
        min_tier: 2,
        fractured: true,
    };
    assert.deepEqual(
        compileConditionTree(parseConditionTree(fractured)),
        fractured,
    );
    console.log("  ok - fractured modifier condition round-trips");
}

{
    // NOT toggles the negate flag on the wrapped node and compiles back out.
    const negated = {
        type: "not",
        conditions: [{ type: "has_mod_group", group: "IncreasedLife" }],
    };
    const root = parseConditionTree(negated);
    // Root is wrapped because the negated leaf is not a plain group.
    const child = root.children[0] as ConditionLeaf;
    assert.equal(child.kind, "leaf");
    assert.equal(child.negate, true);
    assert.deepEqual(compileConditionTree(root), negated);
    console.log("  ok - NOT condition round-trips through the builder tree");
}

{
    // An empty root group means "always".
    const empty: ConditionGroupNode = {
        kind: "group",
        negate: false,
        mode: "all",
        count: 0,
        children: [],
    };
    assert.deepEqual(compileConditionTree(empty), { type: "always" });
    console.log("  ok - empty condition group compiles to always");
}
