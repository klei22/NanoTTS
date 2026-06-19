#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Render the Indonesian evaluation corpus and compute reproducible WAV metrics.

This development tool requires Python 3 and NumPy. eSpeak is optional and is
used only as a process-separated IPA/reference producer when --espeak is set.
The metrics are diagnostic; they are not a substitute for native-listener
transcription tests.
"""
from __future__ import annotations

import argparse
import csv
import json
import pathlib
import shutil
import subprocess
import wave

import numpy as np


def read_pcm16(path: pathlib.Path) -> tuple[np.ndarray, int]:
    with wave.open(str(path), "rb") as wav:
        if wav.getnchannels() != 1 or wav.getsampwidth() != 2:
            raise ValueError(f"expected mono PCM16 WAV: {path}")
        rate = wav.getframerate()
        data = wav.readframes(wav.getnframes())
    return np.frombuffer(data, dtype="<i2").astype(np.float32) / 32768.0, rate


def metrics(path: pathlib.Path) -> dict[str, float]:
    x, rate = read_pcm16(path)
    if x.size == 0:
        return {k: 0.0 for k in ("duration", "peak", "rms", "active_rms",
                                  "centroid_hz", "hf3_ratio", "clip_fraction")}
    peak = float(np.max(np.abs(x)))
    rms = float(np.sqrt(np.mean(x * x)))
    frame = max(1, int(round(rate * 0.020)))
    count = x.size // frame
    framed = x[:count * frame].reshape(count, frame) if count else x.reshape(1, -1)
    frame_rms = np.sqrt(np.mean(framed * framed, axis=1) + 1e-20)
    active = frame_rms[frame_rms > 10.0 ** (-45.0 / 20.0)]
    active_rms = float(np.sqrt(np.mean(active * active))) if active.size else 0.0
    nonzero = np.flatnonzero(np.abs(x) > 1e-5)
    y = x[nonzero[0]:nonzero[-1] + 1] if nonzero.size else x
    nfft = 1 << max(11, min(18, (max(2, y.size) - 1).bit_length()))
    z = np.zeros(nfft, np.float32)
    used = min(y.size, nfft)
    z[:used] = y[:used]
    z *= np.hanning(nfft).astype(np.float32)
    power = np.abs(np.fft.rfft(z)) ** 2
    freqs = np.fft.rfftfreq(nfft, 1.0 / rate)
    total = float(power.sum()) + 1e-30
    return {
        "duration": float(x.size / rate),
        "peak": peak,
        "rms": rms,
        "active_rms": active_rms,
        "centroid_hz": float(np.sum(power * freqs) / total),
        "hf3_ratio": float(np.sum(power[freqs >= 3000.0]) / total),
        "clip_fraction": float(np.mean(np.abs(x) >= 0.959)),
    }


def run(command: list[str]) -> None:
    result = subprocess.run(command, stdout=subprocess.DEVNULL,
                            stderr=subprocess.PIPE, text=True, encoding="utf-8")
    if result.returncode:
        raise RuntimeError("command failed: " + " ".join(command) + "\n" + result.stderr)


def summarize(rows: list[dict], system: str) -> dict[str, float]:
    keys = ("duration", "peak", "rms", "active_rms", "centroid_hz",
            "hf3_ratio", "clip_fraction")
    return {key: float(np.mean([row[system][key] for row in rows])) for key in keys}


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--idtts", type=pathlib.Path, default=pathlib.Path("build/idtts"))
    parser.add_argument("--baseline", type=pathlib.Path,
                        help="optional older idtts CLI for paired comparison")
    parser.add_argument("--corpus", type=pathlib.Path,
                        default=pathlib.Path("tests/data/indonesian_intelligibility.tsv"))
    parser.add_argument("--output", type=pathlib.Path,
                        default=pathlib.Path("build/intelligibility-eval"))
    parser.add_argument("--espeak", nargs="?", const="espeak-ng",
                        help="also render external eSpeak IPA and reference audio")
    args = parser.parse_args()

    candidate = args.idtts.resolve()
    if not candidate.is_file():
        raise SystemExit(f"idtts CLI not found: {candidate}")
    baseline = args.baseline.resolve() if args.baseline else None
    if baseline and not baseline.is_file():
        raise SystemExit(f"baseline CLI not found: {baseline}")
    espeak = shutil.which(args.espeak) if args.espeak else None
    if args.espeak and not espeak:
        raise SystemExit(f"eSpeak executable not found: {args.espeak}")

    args.output.mkdir(parents=True, exist_ok=True)
    with args.corpus.open(encoding="utf-8", newline="") as stream:
        corpus = list(csv.DictReader(stream, delimiter="\t"))
    systems = ["candidate_text"]
    if baseline:
        systems.append("baseline_text")
    if espeak:
        systems += ["candidate_ipa", "espeak_reference"]
    for system in systems:
        (args.output / system).mkdir(exist_ok=True)

    results: list[dict] = []
    for row in corpus:
        stem = f"{row['id']}_{row['category']}"
        text = row["text"]
        candidate_wav = args.output / "candidate_text" / f"{stem}.wav"
        run([str(candidate), "--text", text, "-o", str(candidate_wav)])
        record = dict(row)
        record["candidate_text"] = metrics(candidate_wav)
        if baseline:
            wav = args.output / "baseline_text" / f"{stem}.wav"
            run([str(baseline), "--text", text, "-o", str(wav)])
            record["baseline_text"] = metrics(wav)
        if espeak:
            generated = subprocess.run(
                [espeak, "-q", "-v", "id", "--ipa=1", "--sep=_", text],
                check=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                text=True, encoding="utf-8").stdout.strip()
            record["ipa"] = generated
            wav = args.output / "candidate_ipa" / f"{stem}.wav"
            run([str(candidate), "--ipa", generated, "-o", str(wav)])
            record["candidate_ipa"] = metrics(wav)
            wav = args.output / "espeak_reference" / f"{stem}.wav"
            run([espeak, "-v", "id", "-s", "165", "-w", str(wav), text])
            record["espeak_reference"] = metrics(wav)
        results.append(record)

    summary = {system: summarize(results, system) for system in systems}
    (args.output / "results.json").write_text(
        json.dumps({"summary": summary, "cases": results}, ensure_ascii=False, indent=2),
        encoding="utf-8")
    with (args.output / "summary.csv").open("w", encoding="utf-8", newline="") as stream:
        writer = csv.writer(stream)
        writer.writerow(["system", "metric", "mean"])
        for system, values in summary.items():
            for key, value in values.items():
                writer.writerow([system, key, f"{value:.9g}"])
    for system, values in summary.items():
        print(system)
        for key, value in values.items():
            print(f"  {key:14s} {value:.6g}")
    print(f"wrote {args.output / 'results.json'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
