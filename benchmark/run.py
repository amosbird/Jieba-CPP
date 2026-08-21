#!/usr/bin/env python3
import argparse
import csv
import random
import statistics
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent
BUILD = ROOT / "build"
RUNNERS = {
    "jieba-cpp": BUILD / "bench_jieba_cpp",
    "cppjieba": BUILD / "bench_cppjieba",
    "cppjieba-datrie": BUILD / "bench_cppjieba_datrie",
    "jieba-rs": BUILD / "bench_jieba_rs",
    "nexaloid": BUILD / "bench_nexaloid",
}


def run_once(name, executable, corpus, mode, iterations, warmup, cpu):
    command = [str(executable), "--corpus", str(corpus), "--mode", mode, "--iterations", str(iterations), "--warmup", str(warmup)]
    if cpu is not None:
        command = ["taskset", "-c", str(cpu), *command]
    output = subprocess.check_output(command, text=True)
    rows = list(csv.DictReader(output.splitlines()))
    if len(rows) != 1:
        raise RuntimeError(f"unexpected output from {name}: {output}")
    row = rows[0]
    elapsed = int(row["elapsed_ns"])
    processed = int(row["input_bytes"]) * int(row["iterations"])
    row["mib_per_sec"] = processed / (1024 * 1024) / (elapsed / 1e9)
    return row


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--corpus", type=Path, required=True)
    parser.add_argument("--rounds", type=int, default=10)
    parser.add_argument("--iterations", type=int, default=100)
    parser.add_argument("--warmup", type=int, default=5)
    parser.add_argument("--cpu", type=int)
    parser.add_argument("--seed", type=int, default=89945)
    parser.add_argument("--output", type=Path, default=ROOT / "results" / "results.csv")
    args = parser.parse_args()

    for name, executable in RUNNERS.items():
        if not executable.exists():
            raise SystemExit(f"missing {executable}; run benchmark/build.sh")

    random.seed(args.seed)
    raw = []
    jobs = [(name, mode) for mode in ("exact", "search") for name in RUNNERS]
    for round_number in range(args.rounds):
        random.shuffle(jobs)
        for name, mode in jobs:
            row = run_once(name, RUNNERS[name], args.corpus, mode, args.iterations, args.warmup, args.cpu)
            row["round"] = round_number
            raw.append(row)
            print(f"{round_number:02d} {mode:6s} {name:18s} {row['mib_per_sec']:.2f} MiB/s")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    fields = ["round", "implementation", "mode", "input_bytes", "documents", "iterations", "tokens", "checksum", "elapsed_ns", "mib_per_sec"]
    with args.output.open("w", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=fields)
        writer.writeheader()
        writer.writerows(raw)

    print("\nimplementation,mode,median_mib_s,p95_mib_s,min_mib_s")
    for mode in ("exact", "search"):
        for name in RUNNERS:
            values = sorted(float(row["mib_per_sec"]) for row in raw if row["implementation"] == name and row["mode"] == mode)
            p95 = values[min(len(values) - 1, int((len(values) - 1) * 0.95 + 0.5))]
            print(f"{name},{mode},{statistics.median(values):.2f},{p95:.2f},{min(values):.2f}")


if __name__ == "__main__":
    main()
