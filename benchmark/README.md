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
./benchmark/run.py --corpus benchmark/corpus/sample.txt --rounds 10
```

Use a larger real corpus for publishable numbers. `sample.txt` is only a build and smoke-test corpus.

The exact revisions are recorded in `versions.lock`. Nexaloid uses `Accurate` and `Search` modes with its bundled dictionary and HMM candidate plugin; its token boundaries are not expected to match Jieba exactly, so it is reported as an extended comparison rather than part of the strict output-equivalence group.
