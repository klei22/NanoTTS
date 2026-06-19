#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Check separated Indonesian eSpeak IPA against an idtts CLI build.

This is a development/interoperability tool. It does not link either program
and is not needed by the idtts runtime.
"""

from __future__ import annotations

import argparse
import pathlib
import shutil
import subprocess
import sys
import tempfile


def find_espeak(requested: str | None) -> str:
    if requested:
        found = shutil.which(requested)
        if not found:
            raise SystemExit(f"eSpeak executable not found: {requested}")
        return found
    for name in ("espeak-ng", "espeak"):
        found = shutil.which(name)
        if found:
            return found
    raise SystemExit("neither espeak-ng nor espeak was found in PATH")


def load_phrases(path: pathlib.Path) -> list[str]:
    phrases: list[str] = []
    for raw in path.read_text(encoding="utf-8").splitlines():
        text = raw.strip()
        if text and not text.startswith("#"):
            phrases.append(text)
    return phrases


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("corpus", type=pathlib.Path,
                        help="UTF-8 file with one Indonesian phrase per line")
    parser.add_argument("--idtts", type=pathlib.Path, default=pathlib.Path("build/idtts"),
                        help="path to the idtts CLI")
    parser.add_argument("--espeak", help="espeak-ng/espeak executable name or path")
    parser.add_argument("--show", action="store_true",
                        help="print generated IPA for every accepted phrase")
    args = parser.parse_args()

    executable = find_espeak(args.espeak)
    cli = args.idtts.resolve()
    if not cli.is_file():
        raise SystemExit(f"idtts CLI not found: {cli}")

    phrases = load_phrases(args.corpus)
    if not phrases:
        raise SystemExit("corpus contains no phrases")

    failures = 0
    for line_number, phrase in enumerate(phrases, 1):
        generated = subprocess.run(
            [executable, "-q", "-v", "id", "--ipa=1", "--sep=_", phrase],
            check=False,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            encoding="utf-8",
        )
        if generated.returncode != 0:
            failures += 1
            print(f"{line_number}: eSpeak failed: {generated.stderr.strip()}",
                  file=sys.stderr)
            continue

        ipa = generated.stdout.strip()
        # A temporary file avoids quoting differences and exercises the same
        # UTF-8 file path used for corpus-scale checks.
        with tempfile.NamedTemporaryFile("w", encoding="utf-8", delete=False) as tmp:
            tmp.write(ipa)
            tmp_path = pathlib.Path(tmp.name)
        try:
            checked = subprocess.run(
                [str(cli), "--ipa-file", str(tmp_path), "--dump-phones"],
                check=False,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.PIPE,
                text=True,
                encoding="utf-8",
            )
        finally:
            tmp_path.unlink(missing_ok=True)

        if checked.returncode != 0:
            failures += 1
            print(f"{line_number}: {phrase!r}", file=sys.stderr)
            print(f"  IPA: {ipa}", file=sys.stderr)
            print(f"  idtts: {checked.stderr.strip()}", file=sys.stderr)
        elif args.show:
            print(f"{phrase}\t{ipa}")

    print(f"checked {len(phrases)} phrase(s); failures: {failures}")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
