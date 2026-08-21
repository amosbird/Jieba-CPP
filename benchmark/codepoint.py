#!/usr/bin/env python3
"""Compare Unicode code-point token streams and benchmark throughput."""
import argparse
import csv
import random
import statistics
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent
RUNNERS = {
    "Jieba-CPP validated": ROOT / "build/bench_codepoint_jieba",
    "simdutf": ROOT / "build/bench_codepoint_simdutf",
}


def output(executable, corpus):
    return subprocess.check_output(
        [str(executable), "--corpus", str(corpus), "--mode", "codepoint", "--dump-tokens"], text=True
    ).splitlines()


def run(executable, corpus, iterations, warmup, cpu):
    command = [
        "taskset", "-c", str(cpu), str(executable), "--corpus", str(corpus),
        "--mode", "codepoint", "--iterations", str(iterations), "--warmup", str(warmup),
    ]
    row = next(csv.DictReader(subprocess.check_output(command, text=True).splitlines()))
    return int(row["input_bytes"]) * int(row["iterations"]) / 1048576 / (int(row["elapsed_ns"]) / 1e9)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--corpus", type=Path, required=True)
    parser.add_argument("--rounds", type=int, default=10)
    parser.add_argument("--iterations", type=int, default=100)
    parser.add_argument("--warmup", type=int, default=5)
    parser.add_argument("--cpu", type=int, default=4)
    parser.add_argument("--seed", type=int, default=1)
    args = parser.parse_args()

    expected = output(RUNNERS["simdutf"], args.corpus)
    actual = output(RUNNERS["Jieba-CPP validated"], args.corpus)
    if actual != expected:
        for i, (left, right) in enumerate(zip(actual, expected)):
            if left != right:
                raise SystemExit(f"token mismatch in document {i}:\nJieba-CPP={left!r}\nsimdutf={right!r}")
        raise SystemExit(f"document count mismatch: Jieba-CPP={len(actual)} simdutf={len(expected)}")
    print(f"correctness: {len(actual)}/{len(expected)} documents byte-identical")

    values = {name: [] for name in RUNNERS}
    rng = random.Random(args.seed)
    for round_number in range(args.rounds):
        names = list(RUNNERS)
        rng.shuffle(names)
        for name in names:
            value = run(RUNNERS[name], args.corpus, args.iterations, args.warmup, args.cpu)
            values[name].append(value)
            print(f"{round_number:02d} {name:<17} {value:.2f} MiB/s")

    print("\nimplementation,median_mib_per_sec,min_mib_per_sec,max_mib_per_sec")
    for name, samples in values.items():
        print(f"{name},{statistics.median(samples):.2f},{min(samples):.2f},{max(samples):.2f}")


if __name__ == "__main__":
    main()
