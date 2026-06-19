"""Stat-id + value-range -> in-game display text.

Ported from the prior poecraft project's translator and trimmed to the range
case only (the engine returns rolled values per slot at runtime; ingest only
needs the family-tier display lines).

Input is the RePoE stat_translations.json schema:

    {
        "ids": ["accuracy_rating"],
        "English": [
            {
                "condition": [{"min": 1}],
                "format": ["+#"],
                "index_handlers": [[]],
                "string": "+{0} to Accuracy Rating"
            },
            ...
        ]
    }

`translate_range(stats)` takes a list of {"id", "min", "max"} dicts and returns
the rendered text lines (one entry per output line — most mods are one line,
some are two when stats translate independently).
"""

from __future__ import annotations

from typing import Any, Iterable


Stat = dict[str, Any]
Entry = dict[str, Any]


class StatTranslator:
    def __init__(self) -> None:
        self._by_ids: dict[tuple[str, ...], list[Entry]] = {}

    def load(self, entries: Iterable[dict[str, Any]]) -> None:
        for entry in entries:
            ids = entry.get("ids") or []
            if not ids:
                continue
            english = entry.get("English") or []
            if not english:
                continue
            key = tuple(str(i) for i in ids)
            self._by_ids[key] = english

    def translate_range(self, stats: list[Stat]) -> list[str]:
        if not stats:
            return []
        ids = tuple(str(s.get("id", "")) for s in stats)
        entries = self._by_ids.get(ids)
        if entries is not None:
            return _render(entries, stats)
        lines: list[str] = []
        for stat in stats:
            sub = self._by_ids.get((str(stat.get("id", "")),))
            if sub is not None:
                lines.extend(_render(sub, [stat]))
            else:
                lines.append(_fallback_line(stat))
        return lines


def _render(entries: list[Entry], stats: list[Stat]) -> list[str]:
    mins = [_num(s.get("min", 0)) for s in stats]
    for entry in entries:
        if _check(entry.get("condition") or [], mins):
            return _format(entry, stats)
    if entries:
        return _format(entries[0], stats)
    return []


def _check(conditions: list[dict[str, Any]], values: list[float]) -> bool:
    for i, cond in enumerate(conditions):
        if i >= len(values):
            break
        v = values[i]
        if "min" in cond and v < cond["min"]:
            return False
        if "max" in cond and v > cond["max"]:
            return False
        if cond.get("negated") and v != 0:
            return False
    return True


def _format(entry: Entry, stats: list[Stat]) -> list[str]:
    template = entry.get("string", "")
    formats = entry.get("format") or []
    handlers = entry.get("index_handlers") or []
    parts = template
    for i, stat in enumerate(stats):
        lo = _num(stat.get("min", 0))
        hi = _num(stat.get("max", 0))
        for handler in (handlers[i] if i < len(handlers) else []):
            lo, hi = _apply_handler(handler, lo, hi)
        if lo > hi:
            lo, hi = hi, lo
        fmt = formats[i] if i < len(formats) else "#"
        parts = parts.replace(f"{{{i}}}", _emit_range(lo, hi, fmt))
    if not parts:
        return []
    return [line for line in parts.split("\n") if line.strip()]


def _apply_handler(handler: str, lo: float, hi: float) -> tuple[float, float]:
    if handler == "negate":
        return -hi, -lo
    if handler == "divide_by_one_hundred":
        return lo / 100, hi / 100
    if handler == "divide_by_one_hundred_2dp":
        return round(lo / 100, 2), round(hi / 100, 2)
    if handler == "divide_by_one_hundred_and_negate":
        return -hi / 100, -lo / 100
    if handler == "per_minute_to_per_second":
        return round(lo / 60, 1), round(hi / 60, 1)
    if handler == "milliseconds_to_seconds":
        return round(lo / 1000, 2), round(hi / 1000, 2)
    if handler == "milliseconds_to_seconds_0dp":
        return round(lo / 1000), round(hi / 1000)
    if handler == "milliseconds_to_seconds_1dp":
        return round(lo / 1000, 1), round(hi / 1000, 1)
    if handler == "milliseconds_to_seconds_2dp":
        return round(lo / 1000, 2), round(hi / 1000, 2)
    if handler == "deciseconds_to_seconds":
        return round(lo / 10, 1), round(hi / 10, 1)
    if handler == "divide_by_ten":
        return lo / 10, hi / 10
    if handler == "divide_by_ten_0dp":
        return lo // 10, hi // 10
    if handler == "divide_by_five":
        return lo / 5, hi / 5
    if handler == "divide_by_two_0dp":
        return lo // 2, hi // 2
    if handler == "divide_by_fifteen_0dp":
        return lo // 15, hi // 15
    if handler == "divide_by_twenty_then_double_0dp":
        return (lo // 20) * 2, (hi // 20) * 2
    if handler == "multiply_by_four":
        return lo * 4, hi * 4
    if handler == "60%_of_value":
        return round(lo * 0.6), round(hi * 0.6)
    if handler == "negate_and_double":
        return -hi * 2, -lo * 2
    if handler == "mod_value_to_item_class":
        return lo, hi
    if handler == "canonical_stat":
        return lo, hi
    if handler == "old_leech_percent":
        return lo / 100, hi / 100
    if handler == "old_leech_permyriad":
        return lo / 100, hi / 100
    if handler == "passive_hash":
        return lo, hi
    if handler == "tree_expansion_jewel_passive":
        return lo, hi
    return lo, hi


def _emit_range(lo: float, hi: float, fmt: str) -> str:
    if lo == hi:
        return _emit_value(lo, fmt)
    sign = "+" if fmt.startswith("+") else ""
    body = f"({_strip_sign(_emit_value(lo, fmt))}-{_strip_sign(_emit_value(hi, fmt))})"
    return f"{sign}{body}"


def _emit_value(value: float, fmt: str) -> str:
    if isinstance(value, float) and value.is_integer():
        value = int(value)
    if fmt in ("+#", "+d"):
        if isinstance(value, int):
            return f"+{value}" if value >= 0 else str(value)
        return f"+{value}" if value >= 0 else str(value)
    if fmt in ("#", "d", ""):
        return str(value)
    return str(value)


def _strip_sign(s: str) -> str:
    return s[1:] if s.startswith("+") else s


def _fallback_line(stat: Stat) -> str:
    sid = stat.get("id", "?")
    lo = stat.get("min", "?")
    hi = stat.get("max", "?")
    if lo == hi:
        return f"{sid}: {lo}"
    return f"{sid}: ({lo}-{hi})"


def _num(value: Any) -> float:
    if isinstance(value, bool):
        return float(value)
    if isinstance(value, (int, float)):
        return float(value)
    try:
        return float(value)
    except (TypeError, ValueError):
        return 0.0
