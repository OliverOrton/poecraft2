from __future__ import annotations

import argparse
from pathlib import Path

from poecraft_ingest.natural_t1_corpus import generate_corpus


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Generate a deterministic engine-certified natural-T1 corpus."
    )
    parser.add_argument("config", type=Path)
    parser.add_argument("--output", type=Path)
    args = parser.parse_args()
    result = generate_corpus(args.config, output_override=args.output)
    report = result["report"]
    print(
        f"generated {report['counters']['accepts']} cases from "
        f"{report['counters']['attempts']} attempts"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
