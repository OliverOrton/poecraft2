import assert from "node:assert/strict";

import {
    cloneStrategy,
    compileConditionTree,
    conditionLabel,
    conditionLabelLines,
    createBlankStrategy,
    createDefaultStrategy,
    defaultLeafCondition,
    createStrategyFromItemSnapshot,
    operationLabel,
    parseConditionTree,
    strategyEdgeCardPresentation,
    strategyEdgeLabel,
    strategyEdgeLabelLines,
    strategyNodeLabel,
    validateStrategy,
    type ConditionGroupNode,
    type ConditionLeaf,
    type StrategyEdge,
    type StrategyNode,
} from "../src/app/strategy-model";
import type { Catalog } from "../src/app/engine-protocol";

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
                searing_exarch_tier: 2,
                eater_of_worlds_tier: 3,
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
    assert.equal(imported.base_state.searing_exarch_tier, 2);
    assert.equal(imported.base_state.eater_of_worlds_tier, 3);
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
    const strategy = createDefaultStrategy();
    strategy.edges[1].condition = {
        type: "all",
        conditions: [
            { type: "has_mod_group", group: "IncreasedLife", min_tier: 1 },
            { type: "item_flag", flag: "prefixes_locked" },
            { type: "eldritch_tier", side: "searing", min: 1, max: 4 },
            { type: "mod_count", mod_keys: ["Metadata/Mods/Test"], min: 1, max: 1 },
            {
                type: "mod_family_count",
                family_mod_keys: ["Metadata/Mods/TestFamilyTier1"],
                min: 1,
                max: 1,
            },
            { type: "influence_bits", value: 0 },
            { type: "has_unveil_option", mod_key: "Metadata/Mods/Unveiled" },
        ],
    };
    assert.deepEqual(
        validateStrategy(strategy).filter((issue) => issue.severity === "error"),
        [],
    );
    assert.match(conditionLabel(strategy.edges[1].condition), /prefixes locked/);
    strategy.edges[1].condition.conditions![4].family_mod_keys = [];
    assert.ok(
        validateStrategy(strategy).some(
            (issue) => issue.code === "condition-mod-keys",
        ),
    );
    console.log("  ok - S6 Phase 4 condition vocabulary validates and labels");
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
    const observationSignature = {
        type: "observation_signature",
        version: 1,
        requirement: {
            item_features: 1,
            modifier_tag_ids: [7],
            affix_observations: [
                {
                    features: 3,
                    selector: {
                        required_affix_traits: 1,
                        forbidden_affix_traits: 0,
                        required_item_traits: 0,
                        forbidden_item_traits: 0,
                        required_tag_ids: [7],
                    },
                },
            ],
        },
        signature: [
            {
                feature: 0,
                subject: 0,
                value: ["0000000000000002"],
            },
        ],
        goal_status_tier_class_by_mod: [
            {
                mod_key: "Metadata/Mods/TestGoal",
                value: ["0000000000000001", "0000000000000002"],
            },
        ],
        count_observation_count: 1,
        count_observation_membership_by_mod: [
            {
                mod_key: "Metadata/Mods/TestCountMember",
                value: ["0000000000000001"],
            },
        ],
    };
    assert.deepEqual(
        compileConditionTree(parseConditionTree(observationSignature)),
        observationSignature,
    );
    const strategy = createDefaultStrategy();
    strategy.edges[1].condition = structuredClone(observationSignature);
    assert.deepEqual(
        validateStrategy(strategy).filter((issue) => issue.severity === "error"),
        [],
    );
    assert.equal(conditionLabel(strategy.edges[1].condition), "exact policy state");
    assert.deepEqual(
        cloneStrategy(strategy).edges[1].condition,
        observationSignature,
    );
    console.log(
        "  ok - engine-authored observation signatures validate and round-trip opaquely",
    );
}

{
    const modifiers = [
        {
            type: "has_mod_family",
            family_mod_key: "Metadata/Mods/LifeTier1",
            family_label: "+# to maximum Life",
            min_tier: 2,
        },
        {
            type: "has_mod_family",
            family_mod_key: "Metadata/Mods/FireResistTier1",
            family_label: "+#% to Fire Resistance",
            min_tier: 3,
            fractured: true,
        },
    ];
    const allSet = { type: "all", conditions: modifiers };
    const nOfSet = { type: "at_least", count: 1, conditions: modifiers };
    assert.deepEqual(
        compileConditionTree(parseConditionTree(allSet)),
        allSet,
    );
    assert.deepEqual(
        compileConditionTree(parseConditionTree(nOfSet)),
        nOfSet,
    );
    assert.deepEqual(
        strategyEdgeCardPresentation({
            id: "modifier-set",
            from: "a",
            to: "b",
            priority: 0,
            condition: nOfSet,
            label: "",
        }),
        {
            title:
                "AT LEAST 1 OF (+# to maximum Life T2+; +#% to Fire Resistance T3+ fractured)",
            header: "AT LEAST 1",
            count: 2,
            compact: false,
            manual: false,
            rows: [
                {
                    kind: "leaf",
                    label: "+# to maximum Life T2+",
                    depth: 0,
                },
                {
                    kind: "leaf",
                    label: "+#% to Fire Resistance T3+ fractured",
                    depth: 0,
                },
            ],
        },
    );
    console.log("  ok - multi-modifier ALL and N OF sets round-trip and expand");
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

const labelCatalog: Catalog = {
    groupKeyById: [],
    groupNameById: [],
    essences: [
        { key: "EssenceA", name: "Essence of Accuracy" },
        { key: "EssenceB", name: "Deafening Essence of Contempt" },
    ],
    fossils: [
        { key: "Jagged", name: "Jagged Fossil" },
        { key: "Dense", name: "Dense Fossil" },
    ],
    bench: [{ key: "CraftedLife", name: "+# to maximum Life" }],
    harvestTags: [
        { key: "life", name: "Life" },
        { key: "fire", name: "Fire" },
        { key: "cold", name: "Cold" },
    ],
    influences: [{ key: "adjudicator", name: "Warlord" }],
};

const labelContext = {
    catalog: labelCatalog,
    modifierNames: new Map([["UnveiledLife", "+# to maximum Life (unveiled)"]]),
};

{
    assert.equal(
        operationLabel(
            { type: "essence", params: { essence_key: "EssenceB" } },
            labelContext,
        ),
        "Deafening Essence of Contempt",
    );
    assert.equal(
        operationLabel(
            { type: "fossil", params: { fossils: ["Jagged", "Dense"] } },
            labelContext,
        ),
        "Jagged Fossil + Dense Fossil",
    );
    assert.equal(
        operationLabel(
            { type: "bench", params: { mod_key: "CraftedLife" } },
            labelContext,
        ),
        "Bench: +# to maximum Life",
    );
    assert.equal(
        operationLabel(
            { type: "harvest_reforge", params: { target_tag: "life" } },
            labelContext,
        ),
        "Harvest Reforge: Life",
    );
    assert.equal(
        operationLabel(
            {
                type: "harvest_resist",
                params: { source_tag: "fire", target_tag: "cold" },
            },
            labelContext,
        ),
        "Harvest Resistance: Fire → Cold",
    );
    assert.equal(
        operationLabel(
            { type: "influence_exalt", params: { influence: "adjudicator" } },
            labelContext,
        ),
        "Influence Exalt: Warlord",
    );
    assert.equal(
        operationLabel(
            { type: "unveil", params: { mod_key: "UnveiledLife" } },
            labelContext,
        ),
        "Unveil: +# to maximum Life (unveiled)",
    );
    console.log("  ok - operation labels use live parameters and catalog names");
}

{
    const node: StrategyNode = {
        id: "stable-node-id",
        kind: "operation",
        name: "",
        operation: { type: "essence", params: { essence_key: "EssenceA" } },
        position: { x: 0, y: 0 },
    };
    assert.equal(strategyNodeLabel(node, labelContext), "Essence of Accuracy");
    node.operation!.params.essence_key = "EssenceB";
    assert.equal(
        strategyNodeLabel(node, labelContext),
        "Deafening Essence of Contempt",
    );
    node.name = "My preserved node name";
    node.operation!.params.essence_key = "EssenceA";
    assert.equal(strategyNodeLabel(node, labelContext), "My preserved node name");

    const reopened = JSON.parse(JSON.stringify(node)) as StrategyNode;
    assert.equal(reopened.id, "stable-node-id");
    assert.equal(strategyNodeLabel(reopened, labelContext), "My preserved node name");
    reopened.name = "";
    assert.equal(strategyNodeLabel(reopened, labelContext), "Essence of Accuracy");
    console.log("  ok - node manual overrides persist and clear back to automatic");
}

{
    const edge: StrategyEdge = {
        id: "stable-edge-id",
        from: "a",
        to: "b",
        priority: 7,
        condition: { type: "rarity_is", rarity: "rare" },
        label: "",
    };
    assert.equal(strategyEdgeLabel(edge), "rarity is rare");
    edge.condition = { type: "open_prefix_count", min: 1, max: 2 };
    assert.equal(strategyEdgeLabel(edge), "open prefixes 1-2");
    edge.label = "My preserved edge label";
    edge.condition = { type: "always" };
    assert.equal(strategyEdgeLabel(edge), "My preserved edge label");

    const reopened = JSON.parse(JSON.stringify(edge)) as StrategyEdge;
    assert.equal(reopened.id, "stable-edge-id");
    assert.equal(reopened.priority, 7);
    assert.equal(strategyEdgeLabel(reopened), "My preserved edge label");
    reopened.label = "";
    assert.equal(strategyEdgeLabel(reopened), "always");
    console.log("  ok - edge manual overrides persist and clear back to automatic");
}

{
    const condition = {
        type: "all",
        conditions: [
            { type: "rarity_is", rarity: "rare" },
            {
                type: "any",
                conditions: [
                    { type: "open_prefix_count", min: 1, max: 2 },
                    {
                        type: "not",
                        conditions: [{ type: "open_suffix_count", min: 0, max: 0 }],
                    },
                ],
            },
            {
                type: "at_least",
                count: 1,
                conditions: [
                    { type: "prefix_count_range", min: 2, max: 3 },
                    { type: "suffix_count_range", min: 2, max: 3 },
                ],
            },
        ],
    };
    assert.equal(
        conditionLabel(condition),
        "ALL (rarity is rare; ANY (open prefixes 1-2; NOT (open suffixes = 0)); AT LEAST 1 OF (prefixes 2-3; suffixes 2-3))",
    );
    assert.deepEqual(conditionLabelLines(condition), [
        "ALL",
        "|- rarity is rare",
        "|- ANY",
        "| |- open prefixes 1-2",
        "| |- NOT",
        "| | |- open suffixes = 0",
        "|- AT LEAST 1 OF",
        "| |- prefixes 2-3",
        "| |- suffixes 2-3",
    ]);

    const edge: StrategyEdge = {
        id: "complete-chain",
        from: "a",
        to: "b",
        priority: 0,
        condition,
        label: "",
    };
    assert.deepEqual(
        strategyEdgeLabelLines(edge),
        conditionLabelLines(condition),
    );
    assert.deepEqual(strategyEdgeCardPresentation(edge), {
        title:
            "ALL (rarity is rare; ANY (open prefixes 1-2; NOT (open suffixes = 0)); AT LEAST 1 OF (prefixes 2-3; suffixes 2-3))",
        header: "ALL",
        count: 3,
        compact: false,
        manual: false,
        rows: [
            { kind: "leaf", label: "rarity is rare", depth: 0 },
            { kind: "group", label: "ANY", depth: 0, count: 2 },
            { kind: "leaf", label: "open prefixes 1-2", depth: 1 },
            { kind: "group", label: "NOT", depth: 1, count: 1 },
            { kind: "leaf", label: "open suffixes = 0", depth: 2 },
            { kind: "group", label: "AT LEAST 1", depth: 0, count: 2 },
            { kind: "leaf", label: "prefixes 2-3", depth: 1 },
            { kind: "leaf", label: "suffixes 2-3", depth: 1 },
        ],
    });
    edge.label =
        "A deliberately long manual override that wraps without changing mode";
    assert.equal(strategyEdgeLabel(edge), edge.label);
    assert.deepEqual(strategyEdgeLabelLines(edge), [
        "A deliberately long manual",
        "override that wraps without",
        "changing mode",
    ]);
    assert.deepEqual(strategyEdgeCardPresentation(edge), {
        title:
            "A deliberately long manual override that wraps without changing mode",
        header: "LABEL",
        compact: false,
        manual: true,
        rows: [
            {
                kind: "leaf",
                label: "A deliberately long manual",
                depth: 0,
            },
            {
                kind: "leaf",
                label: "override that wraps without",
                depth: 0,
            },
            { kind: "leaf", label: "changing mode", depth: 0 },
        ],
    });
    assert.deepEqual(
        strategyEdgeCardPresentation({
            ...edge,
            label: "",
            condition: { type: "rarity_is", rarity: "rare" },
        }),
        {
            title: "rarity is rare",
            header: "",
            compact: true,
            manual: false,
            rows: [
                { kind: "leaf", label: "rarity is rare", depth: 0 },
            ],
        },
    );
    console.log(
        "  ok - full nested condition chains are readable without losing overrides",
    );
}

{
    const strategy = createBlankStrategy();
    strategy.nodes = [
        {
            id: "manual-node",
            kind: "operation",
            name: "Saved manual node",
            operation: { type: "essence", params: { essence_key: "EssenceA" } },
            position: { x: 10, y: 20 },
        },
        {
            id: "automatic-node",
            kind: "operation",
            name: "",
            operation: { type: "essence", params: { essence_key: "EssenceB" } },
            position: { x: 30, y: 40 },
        },
    ];
    strategy.edges = [
        {
            id: "manual-edge",
            from: "manual-node",
            to: "automatic-node",
            priority: 0,
            condition: { type: "rarity_is", rarity: "rare" },
            label: "Saved manual edge",
        },
        {
            id: "automatic-edge",
            from: "automatic-node",
            to: "manual-node",
            priority: 1,
            condition: { type: "rarity_is", rarity: "magic" },
            label: "",
        },
    ];

    const reopened = JSON.parse(
        JSON.stringify(cloneStrategy(strategy)),
    ) as typeof strategy;
    assert.equal(reopened.nodes[0].name, "Saved manual node");
    assert.equal(reopened.nodes[1].name, "");
    assert.equal(reopened.edges[0].label, "Saved manual edge");
    assert.equal(reopened.edges[1].label, "");
    assert.equal(
        strategyNodeLabel(reopened.nodes[1], labelContext),
        "Deafening Essence of Contempt",
    );
    assert.equal(strategyEdgeLabel(reopened.edges[1]), "rarity is magic");
    console.log("  ok - save/reopen preserves manual overrides and automatic mode");
}
