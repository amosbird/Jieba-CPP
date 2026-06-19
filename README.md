# Jieba-CPP

A compact C++ reimplementation of the [jieba](https://github.com/fxsjy/jieba)
Chinese word segmentation algorithm, with an embedded dictionary and HMM model
and no runtime data dependencies.

It is the backend for the `chinese` tokenizer in
[ClickHouse](https://github.com/ClickHouse/ClickHouse) and is maintained as a
standalone library so it can be developed, tested and reused independently.

## What it is (and isn't)

- The **algorithm** is a from-scratch C++ reimplementation following the design
  of `fxsjy/jieba` (dictionary-based maximum-probability segmentation with an
  HMM fallback for out-of-vocabulary runs). It is *not* a fork of `cppjieba`.
- The embedded **dictionary** (`data/dict_le.dat.zst`) and **HMM model**
  (`data/jieba_hmm_model.dat`) are generated from the `dict/jieba.dict.utf8`
  and `dict/hmm_model.utf8` files of a pinned
  [`cppjieba`](https://github.com/yanyiwu/cppjieba) commit. `cppjieba` is MIT
  licensed; see [`LICENSE`](LICENSE).

## API

```cpp
#include <jieba.h>

Jieba::Jieba jieba;

// coarse_grained: one segmentation into non-overlapping words (default).
jieba.cut("我来自北京邮电大学");      // -> ["我", "来自", "北京邮电大学"]

// fine_grained: additionally enumerates overlapping dictionary sub-words.
jieba.cutAll("北京邮电大学");         // -> [..., "北京", "邮电", "大学", "北京邮电大学", ...]

// search-engine mode: like cut, but splits long words into shorter sub-words.
jieba.cutForSearch("中华人民共和国");
```

All three return `std::vector<std::string_view>` whose elements point into the
input buffer (no copies). ASCII handling is shared across granularities:
alphanumeric runs are kept whole (`5G`, `iPhone6s`), ASCII punctuation is
dropped, and the library does no word-splitting of Latin text — it is intended
for Chinese.

## Why the data is pinned

`tools/generate_dict.py` and `tools/generate_hmm_model.py` download their source
data from a hard-coded `cppjieba` commit and verify it against a hard-coded
SHA-256 before processing. The commit is pinned on purpose:

- **Reproducibility** — regenerating the embedded files always produces the
  exact bytes committed here, regardless of when the script runs. A moving ref
  would drift.
- **Stability of tokenization** — changing the dictionary or model changes how
  text is segmented. A consumer that builds an index over segmented tokens would
  silently get different results after an upgrade. So the data is versioned with
  the library, not tracked against upstream `master`.

If the embedded data is ever intentionally upgraded, downstream users that need
stable behavior (e.g. an existing text index) should pin to a tagged release.

Current pin: `cppjieba@eed6bfe483105d1db4bfbebaf796f60c173d6e84`.

## Building

Requirements:

- A C++23 compiler with `#embed` support (Clang ≥ 19 or GCC ≥ 15).
- [abseil](https://github.com/abseil/abseil-cpp) (`flat_hash_set`), `libzstd`.
  Both are found via `find_package`; abseil is fetched automatically if missing.
- [darts-clone](https://github.com/s-yata/darts-clone) — header only, fetched
  automatically.

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Link against the library with CMake:

```cmake
add_subdirectory(Jieba-CPP)
target_link_libraries(your_target PRIVATE JiebaCPP::jieba)
```

### Use inside ClickHouse

ClickHouse vendors this library as a submodule and builds it with its own
`-cmake` wrapper (using its in-tree abseil/zstd/darts-clone targets), so the
sources and headers here are kept byte-identical to that copy. The standalone
`CMakeLists.txt` in this repo is for independent development and CI.

## Regenerating the embedded data

```bash
pip install numpy zstandard dartsclone
python3 tools/generate_dict.py        # writes data/dict_le.dat.zst
python3 tools/generate_hmm_model.py   # writes data/jieba_hmm_model.dat
```

Both scripts are deterministic; re-running them reproduces the committed files
byte-for-byte (the `test_reproducible` test asserts this when run with network
access).

## On-disk format

The dictionary is a little-endian image of a [darts-clone](https://github.com/s-yata/darts-clone)
double-array trie plus a parallel array of `double` weights, zstd-compressed.
Trie keys encode each BMP code point as 3 bytes (`0x80..0xFF` only, so no `0x00`
bytes, injective and lexicographically order-preserving). The HMM model is a
generated C++ source `#include`d directly. The format is little-endian only;
big-endian platforms (e.g. s390x) are not supported.

## Layout

```
include/   public headers (jieba.h, jieba_common.h, jieba_dict.h)
src/       library sources
data/      embedded dictionary + HMM model
tools/     generators for the embedded data
tests/     unit tests (segmentation, encoding invariants, reproducibility)
```

## License

The library code, the embedded dictionary, and the embedded HMM model derive
from `cppjieba`, which is distributed under the MIT License. See
[`LICENSE`](LICENSE).
