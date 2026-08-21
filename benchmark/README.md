# Jieba tokenizer benchmark

This benchmark compares five dictionary/HMM Chinese tokenizers:

- `amosbird/Jieba-CPP`
- `yanyiwu/cppjieba`
- `byronhe/cppjieba` (Darts double-array trie fork)
- `messense/jieba-rs`
- `Nexaloid/Nexaloid`

The benchmark runs each implementation as a separate native executable over the same newline-delimited UTF-8 corpus. Each runner reports the same CSV columns and consumes every token through the same checksum.

## Reproduce

```bash
./benchmark/fetch.sh
./benchmark/build.sh
./benchmark/prepare_corpus.py benchmark/corpus/weicheng.txt
./benchmark/prepare_corpus.py benchmark/corpus/weicheng-han.txt --han-only
./benchmark/run.py --corpus benchmark/corpus/weicheng-han.txt --rounds 10 --cpu 4
./benchmark/correctness.py --corpus benchmark/corpus/weicheng-han.txt
./benchmark/codepoint.py --corpus benchmark/corpus/weicheng.txt --rounds 10 --cpu 4
```

Use `weicheng-han.txt` for the strict exact-mode ranking: `Jieba-CPP`, `cppjieba`, and the DATrie fork produce byte-identical token streams there. Use the unfiltered corpus as a realistic mixed-text workload, but report output differences alongside throughput.

`sample.txt` is only a build and smoke-test corpus.

## Unicode code-point baseline

`codepoint.py` compares byte-identical one-Unicode-code-point token streams against
[`simdutf`](https://github.com/simdutf/simdutf), pinned in `versions.lock`. `simdutf`
is used by Chromium, Node.js, WebKit and other production systems and provides a SIMD
UTF-8-to-UTF-32 baseline. This benchmark uses valid UTF-8 corpus input because simdutf's
fast conversion API deliberately does not define replacement semantics for malformed UTF-8.
Both runners reuse output storage and feed every token through the same checksum.

## Fuzzing

With a Clang installation that includes libFuzzer runtimes:

```bash
cmake -S . -B build-fuzz -G Ninja \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DJIEBACPP_BUILD_TESTS=OFF \
  -DJIEBACPP_BUILD_FUZZERS=ON
cmake --build build-fuzz -j
./build-fuzz/fuzz_dag -max_total_time=60
./build-fuzz/fuzz_segmentation -max_total_time=60
```

Set `JIEBACPP_STANDALONE_FUZZERS=ON` to build corpus-replay executables on a
compiler installation without libFuzzer. Both forms use ASan and UBSan.

The exact revisions are recorded in `versions.lock`. Nexaloid uses `Accurate` and `Search` modes with its bundled dictionary and HMM candidate plugin; its token boundaries are not expected to match Jieba exactly, so it is reported as an extended comparison rather than part of the strict output-equivalence group.
