#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
BENCH="$ROOT/benchmark"
BUILD="$BENCH/build"
THIRD_PARTY="$BENCH/third_party"

mkdir -p "$BUILD"

(
    cd "$THIRD_PARTY/nexaloid/core"
    zig build -Doptimize=ReleaseFast
)
(
    cd "$THIRD_PARTY/nexaloid"
    zig build-lib -O ReleaseFast -mcpu native -dynamic -lc --name nexaloid_plugin_hmm_lattice tools/hmm_lattice_plugin.zig
)

cmake -S "$BENCH" -B "$BUILD" -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" -j

CARGO_TARGET_DIR="$BUILD/rust" cargo build \
    --manifest-path "$BENCH/jieba_rs_runner/Cargo.toml" \
    --release
cp "$BUILD/rust/release/jieba-benchmark-runner" "$BUILD/bench_jieba_rs"
