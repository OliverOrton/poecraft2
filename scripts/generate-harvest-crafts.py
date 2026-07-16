#!/usr/bin/env python3
"""Generate native Harvest craft allowlists from the economy recipe manifest."""

from __future__ import annotations

import argparse
import json
from pathlib import Path


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--recipes", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()

    recipes = json.loads(args.recipes.read_text(encoding="utf-8"))

    def entries(member: str) -> str:
        return "".join(
            f'    std::string_view{{"{tag}"}},\n'
            for tag in sorted(recipes[member])
        )

    text = f"""#ifndef POECRAFT_HARVEST_CRAFTS_GENERATED_HPP
#define POECRAFT_HARVEST_CRAFTS_GENERATED_HPP

#include <array>
#include <cstddef>
#include <string_view>

namespace poecraft {{

inline constexpr std::array<std::string_view, {len(recipes['reforge'])}>
    kHarvestReforgeTags = {{
{entries('reforge')}}};

inline constexpr std::array<std::string_view, {len(recipes['augment'])}>
    kHarvestAugmentTags = {{
{entries('augment')}}};

template <std::size_t N>
constexpr bool harvest_tag_allowed(
    const std::array<std::string_view, N>& allowed,
    const std::string_view tag) {{
    for (const std::string_view candidate : allowed) {{
        if (candidate == tag) return true;
    }}
    return false;
}}

constexpr bool harvest_reforge_tag_allowed(const std::string_view tag) {{
    return harvest_tag_allowed(kHarvestReforgeTags, tag);
}}

constexpr bool harvest_augment_tag_allowed(const std::string_view tag) {{
    return harvest_tag_allowed(kHarvestAugmentTags, tag);
}}

}} // namespace poecraft

#endif
"""
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(text, encoding="utf-8", newline="\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
