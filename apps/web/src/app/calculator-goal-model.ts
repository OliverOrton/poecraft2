import type { ModifierFamilyOption } from "./modifier-options";
import type {
    TargetModListModel,
    TargetOtherRequirement,
    TargetSlotMod,
} from "./components/pc-mod-list";
import type { CalculatorGoalSlot } from "./workspace/persistence";

export interface CalculatorTargetModelInput {
    baseName: string;
    itemLevel: number;
    rarity: "normal" | "magic" | "rare";
    slots: readonly CalculatorGoalSlot[];
    modifierOptions: readonly ModifierFamilyOption[];
    maxPrefix: number;
    maxSuffix: number;
    slotProbabilities?: readonly number[];
    formatProbability?: (probability: number) => string;
    groupLabel: (groupKey: string) => string;
}

/** Adapt persisted Calculator v1 slots to the presentational target ledger. */
export function buildCalculatorTargetModel(
    input: CalculatorTargetModelInput,
): TargetModListModel {
    const byKey = new Map(
        input.modifierOptions.map((option) => [option.value, option]),
    );
    const prefixes: TargetSlotMod[] = [];
    const suffixes: TargetSlotMod[] = [];
    const otherRequirements: TargetOtherRequirement[] = [];

    input.slots.forEach((slot, slotIndex) => {
        const option = slot.familyModKey
            ? byKey.get(slot.familyModKey)
            : undefined;
        const probability = input.slotProbabilities?.[slotIndex];
        const probabilityLabel =
            probability !== undefined && input.formatProbability
                ? input.formatProbability(probability)
                : undefined;
        if (!option || !slot.familyModKey) {
            const key = slot.group ?? slot.familyModKey ?? "Unknown requirement";
            otherRequirements.push({
                slotIndex,
                label: slot.group ? input.groupLabel(slot.group) : key,
                minTier: slot.minTier,
                probabilityLabel,
            });
            return;
        }

        const selectedTier = option.tiers.find(
            (tier) => tier.tier === slot.minTier,
        );
        const target: TargetSlotMod = {
            familyModKey: slot.familyModKey,
            textLines: [
                slot.minTier > 0
                    ? (selectedTier?.label ?? option.label)
                    : option.label,
            ],
            classificationTags: option.tags,
            sourceLabel: option.sourceLabel,
            minTier: slot.minTier,
            tiers: option.tiers.map((tier) => ({
                tier: tier.tier,
                label: tier.label,
            })),
            probabilityLabel,
        };
        (option.side === "prefix" ? prefixes : suffixes).push(target);
    });

    return {
        kind: "target",
        baseName: input.baseName,
        itemLevel: input.itemLevel,
        rarity: input.rarity,
        prefixes,
        suffixes,
        otherRequirements,
        maxPrefix: input.maxPrefix,
        maxSuffix: input.maxSuffix,
    };
}
