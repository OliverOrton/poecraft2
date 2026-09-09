"""Exact synthetic review checks and the exported auxiliary route, not native proof."""
import argparse
from collections import defaultdict
from fractions import Fraction as F
import json
from pathlib import Path


def synthetic_checks():
    # Same compressed marker does not make physical continuation values equal.
    assert F(20) > F(2)
    # A producing-row expectation cannot be broadcast to every exit member.
    assert (F(2) + F(20)) / 2 == 11 > 2
    # Complete aggregation preserves the expectation when each event is uniform.
    assert sum(p * b for p, b in [(F(1, 4), 4), (F(1, 4), 4), (F(1, 2), 12)]) == 8
    # Repeating A is a policy; taking A once then the cheap alternative is cheaper.
    assert F(1) / (1 - F(1, 2)) == 2 > 1 + F(1, 2) * F(1, 10)
    # A post-observation self option must remain available.
    assert 1 + F(1, 2) * 100 == 51 > F(1) / (1 - F(1, 2))
    # An unchanged family fixes the complete minimum despite another improvement.
    assert min(1, 6) == min(1, 100) == 1
    # Dropping unsupported mass and renormalizing overstates the bound.
    assert F(1, 10) * 10 + F(9, 10) * 0 == 1 < 10
    # A real predecessor gain with the other action retained, not a lookup count.
    assert min(6, 2 + F(1, 4) * 4) == 3 < min(6, 2 + F(1, 4) * 20) == 6


def auxiliary_route(path):
    model = json.loads(path.read_text(encoding="utf-8"))
    rows = defaultdict(list)
    for row in model["relations"]:
        rows[row["cell"]].append(row)
    selected, memo, active = {}, {}, set()

    def value(cell):
        if cell in memo:
            return memo[cell]
        if cell in active:
            raise ValueError("cyclic route needs the existing SCC evaluator")
        if not rows[cell]:
            assert model["values"][cell] == 0
            memo[cell] = F(0)  # terminal/zero first-exit boundary of THIS model
            return memo[cell]
        active.add(cell)
        row = min(rows[cell], key=lambda r: (r["rhs"], r["action"]))
        selected[cell] = row
        assert sum(F(p) for _, p in row["exits"]) == 1
        result = F(row["cost"]) + sum(F(p) * value(t) for t, p in row["exits"])
        active.remove(cell)
        memo[cell] = result
        return result

    ceiling = value(model["source_cell"])
    assert not any(r["action"] == "chaos" for r in selected.values())
    assert 0 <= float(ceiling) - model["source_lower"] < 1e-6
    return {"scope": "exported optimistic relation model; not a native upper",
            "source_lower": model["source_lower"], "ceiling": float(ceiling),
            "exact_ceiling": str(ceiling), "reachable_cells": len(memo),
            "selected_actions": sorted({r["action"] for r in selected.values()}),
            "selected_rows": list(selected.values())}


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("model", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    synthetic_checks()
    receipt = {"synthetic_checks": 8, "auxiliary_route": auxiliary_route(args.model)}
    args.output.write_bytes((json.dumps(receipt, indent=2) + "\n").encode())
    print("8 synthetic checks and exact acyclic auxiliary route passed")
