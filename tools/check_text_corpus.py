#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Validate a UTF-8 text corpus through one compiled NanoTTS frontend."""
from __future__ import annotations

import argparse
import pathlib
import subprocess
import sys


def phrases(path: pathlib.Path) -> list[tuple[int, str]]:
    result: list[tuple[int, str]] = []
    for line_number, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        text = raw.strip()
        if text and not text.startswith("#"):
            result.append((line_number, text))
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("corpus", type=pathlib.Path)
    parser.add_argument("--lang", required=True)
    parser.add_argument("--nanotts", type=pathlib.Path,
                        default=pathlib.Path("build/nanotts"))
    parser.add_argument("--show", action="store_true")
    args = parser.parse_args()

    cli = args.nanotts.resolve()
    if not cli.is_file():
        raise SystemExit(f"NanoTTS CLI not found: {cli}")
    items = phrases(args.corpus)
    if not items:
        raise SystemExit("corpus contains no phrases")

    failures = 0
    for line_number, text in items:
        completed = subprocess.run(
            [str(cli), "--lang", args.lang, "--text", text, "--dump-phones"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
            check=False,
        )
        if completed.returncode != 0 or not completed.stdout.strip():
            failures += 1
            detail = completed.stderr.strip() or "no events emitted"
            print(f"{line_number}: {text!r}: {detail}", file=sys.stderr)
        elif args.show:
            event_count = sum(1 for line in completed.stdout.splitlines() if line.strip())
            print(f"{line_number}: {event_count:3d} events  {text}")

    accepted = len(items) - failures
    print(f"accepted {accepted}/{len(items)} {args.lang} text inputs")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
