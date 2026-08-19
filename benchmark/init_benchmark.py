#!/usr/bin/env python3
import argparse
import csv
import re
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


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--corpus", type=Path, required=True)
    parser.add_argument("--rounds", type=int, default=10)
    parser.add_argument("--output", type=Path, default=ROOT / "results" / "init.csv")
    args = parser.parse_args()

    rows = []
    for name, executable in RUNNERS.items():
        for round_number in range(args.rounds):
            command = [
                "/usr/bin/time", "-f", "elapsed_s=%e max_rss_kb=%M",
                str(executable), "--corpus", str(args.corpus), "--mode", "exact", "--iterations", "0", "--warmup", "0",
            ]
            process = subprocess.run(command, text=True, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE, check=True)
            match = re.search(r"elapsed_s=([0-9.]+) max_rss_kb=(\d+)", process.stderr)
            if not match:
                raise RuntimeError(process.stderr)
            elapsed, rss = match.groups()
            rows.append({"implementation": name, "round": round_number, "elapsed_s": elapsed, "max_rss_kb": rss})
            print(f"{name:18s} {round_number:02d} init={elapsed}s rss={int(rss) / 1024:.1f} MiB")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    with args.output.open("w", newline="") as output:
        writer = csv.DictWriter(output, fieldnames=["implementation", "round", "elapsed_s", "max_rss_kb"])
        writer.writeheader()
        writer.writerows(rows)


if __name__ == "__main__":
    main()
