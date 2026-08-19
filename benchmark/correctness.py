#!/usr/bin/env python3
import argparse
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


def output(executable, corpus, mode):
    return subprocess.check_output(
        [str(executable), "--corpus", str(corpus), "--mode", mode, "--dump-tokens"],
        text=True,
    ).splitlines()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--corpus", type=Path, required=True)
    parser.add_argument("--limit", type=int, default=100)
    args = parser.parse_args()

    for mode in ("exact", "search"):
        results = {name: output(executable, args.corpus, mode)[: args.limit] for name, executable in RUNNERS.items()}
        baseline = results["cppjieba"]
        print(f"mode={mode}")
        for name, lines in results.items():
            comparable = min(len(baseline), len(lines))
            equal = sum(a == b for a, b in zip(baseline, lines))
            ratio = 100.0 * equal / comparable if comparable else 0.0
            print(f"  {name:18s} exact_documents={equal}/{comparable} ({ratio:.1f}%)")
            if name != "cppjieba":
                for index, (expected, actual) in enumerate(zip(baseline, lines)):
                    if expected != actual:
                        print(f"    first_difference[{index}] cppjieba={expected!r}")
                        print(f"    first_difference[{index}] {name}={actual!r}")
                        break


if __name__ == "__main__":
    main()
