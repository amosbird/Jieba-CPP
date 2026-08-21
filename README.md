# Jieba-CPP

[![CI](https://github.com/amosbird/Jieba-CPP/actions/workflows/ci.yml/badge.svg)](https://github.com/amosbird/Jieba-CPP/actions/workflows/ci.yml)
[![Release](https://img.shields.io/github/v/release/amosbird/Jieba-CPP)](https://github.com/amosbird/Jieba-CPP/releases/latest)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A compact C++ implementation of Jieba Chinese word segmentation with an embedded
dictionary and HMM model. Tokenization requires no runtime data files.

Jieba-CPP is used by ClickHouse's `chinese` text-index tokenizer.

## Features

- Exact segmentation with dictionary dynamic programming and HMM fallback
- Full segmentation with overlapping dictionary candidates
- Search segmentation with shorter subwords
- Embedded, reproducible dictionary and HMM model
- Borrowed `std::string_view` results; token text is not copied
- Thread-safe concurrent tokenization after construction
- Little- and big-endian dictionary images

## Usage

```cpp
#include <jieba.h>

Jieba::Jieba jieba;

auto exact = jieba.cut("我来自北京邮电大学");
// ["我", "来自", "北京邮电大学"]

auto full = jieba.cutAll("北京邮电大学");
auto search = jieba.cutForSearch("中华人民共和国");
```

The returned views refer to the input buffer, which must remain alive while the
results are used.

ASCII letters and digits are kept in contiguous runs such as `5G` and
`iPhone6s`. ASCII punctuation and Unicode punctuation/separators are not emitted
as tokens.

## Implementation

Input is decoded into 16-bit BMP code points and byte offsets. Dictionary prefixes
are resolved with a `darts-clone` double-array trie. Exact segmentation uses a flat
DAG and reverse dynamic programming; unknown single-character runs use a four-state
HMM.

DAG edges store dictionary weight indexes instead of `double` values. DAG topology
and DP routes use separate contiguous arrays, avoiding per-node allocations. Prefix
matches are bounded by the maximum supported word length of 32 code points.

## Performance

Measurements were taken on one x86-64 host using the `jieba-rs` copy of
*Fortress Besieged*, filtered to Han characters. Each runner processed the same
626,589-byte corpus on one pinned CPU, with `-O3 -DNDEBUG -march=native`, 5 warmup
iterations, 10 randomized rounds, and 100 measured iterations per round. Every
token was consumed by the same checksum.

### Exact segmentation

| Implementation | Version | Median MiB/s | Output equal to cppjieba |
|---|---|---:|---:|
| **Jieba-CPP** | this revision | **29.1** | 241/241 documents |
| jieba-rs | `d653b210` | 18.2 | 135/241 documents |
| cppjieba DATrie | `a0db4099` | 11.4 | 241/241 documents |
| cppjieba | `8f171de5` | 8.9 | 241/241 documents |
| Nexaloid | `8194df8d` | 5.4 | 3/241 documents |

Only Jieba-CPP, cppjieba, and the DATrie fork produced identical token streams
on every Han-only document. Throughput comparisons with other rows do not imply
identical segmentation semantics.

### Initialization and peak RSS

| Implementation | Initialization | Peak RSS |
|---|---:|---:|
| **Jieba-CPP** | **0.02 s** | 16.9 MiB |
| cppjieba DATrie | 0.02 s | **9.1 MiB** |
| jieba-rs | 0.15 s | 45.0 MiB |
| cppjieba | 0.66 s | 124.4 MiB |
| Nexaloid | 0.23 s | 270.7 MiB |

The DATrie fork loads an external cache with file-backed `mmap`; Jieba-CPP embeds
a compressed dictionary and decompresses it into private memory. Peak RSS therefore
reflects deployment and loading strategy as well as data-structure size.

Results are specific to this machine, corpus, compiler, and exact-mode harness.
Reproduction scripts, pinned revisions, correctness checks, and corpus preparation
are in [`benchmark/`](benchmark/README.md).

## Building

Requirements:

- C++23 compiler with `#embed` support (GCC 15+ or Clang 19+)
- zstd
- `darts-clone` (fetched automatically by the standalone build)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

CMake integration:

```cmake
find_package(JiebaCPP 1.0 REQUIRED)
target_link_libraries(your_target PRIVATE JiebaCPP::jieba)
```

For an installed package, set `CMAKE_PREFIX_PATH` if Jieba-CPP is not installed
in a standard system prefix. It requires zstd at runtime; `darts.h` is included
in the development package.

## Testing

The tests cover segmentation, BMP trie-key encoding, malformed UTF-8, flat-DAG
layout, exhaustive small-DAG dynamic programming, randomized DAGs, concurrency,
endianness, and generated-data reproducibility.

Optional libFuzzer targets exercise arbitrary UTF-8 input and structured DAGs:

```bash
cmake -S . -B build-fuzz -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DJIEBACPP_BUILD_TESTS=OFF \
  -DJIEBACPP_BUILD_FUZZERS=ON
cmake --build build-fuzz -j
./build-fuzz/fuzz_segmentation -max_total_time=60
./build-fuzz/fuzz_dag -max_total_time=60
```

## Embedded data

The dictionary and HMM source files are pinned to
`cppjieba@eed6bfe483105d1db4bfbebaf796f60c173d6e84`. Generator scripts verify
source SHA-256 hashes and produce deterministic output:

```bash
pip install numpy zstandard dartsclone
python3 tools/generate_dict.py
python3 tools/generate_hmm_model.py
```

The dictionary contains a `darts-clone` trie and a parallel array of logarithmic
weights, compressed with zstd. Trie keys encode each BMP code point into three
nonzero, order-preserving bytes. Separate little- and big-endian images allow
native loading without runtime byte swapping.

## License

The implementation, dictionary, and HMM model are distributed under the MIT
License. See [`LICENSE`](LICENSE).
