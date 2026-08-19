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
```

Use `weicheng-han.txt` for the strict exact-mode ranking: `Jieba-CPP`, `cppjieba`, and the DATrie fork produce byte-identical token streams there. Use the unfiltered corpus as a realistic mixed-text workload, but report output differences alongside throughput.

`sample.txt` is only a build and smoke-test corpus.

The exact revisions are recorded in `versions.lock`. Nexaloid uses `Accurate` and `Search` modes with its bundled dictionary and HMM candidate plugin; its token boundaries are not expected to match Jieba exactly, so it is reported as an extended comparison rather than part of the strict output-equivalence group.
