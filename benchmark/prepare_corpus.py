#!/usr/bin/env python3
"""Build a public-domain benchmark corpus from jieba-rs's bundled 围城 text.

The source is downloaded from the pinned jieba-rs revision. `--han-only` keeps
only U+4E00..U+9FFF so the strict Jieba implementations have identical exact-mode
output; the unfiltered corpus represents real punctuation and mixed text.
"""
import argparse
import urllib.request
from pathlib import Path

REVISION = "d653b2104f1c7b624cd550b4497688ec9bdabce9"
URL = f"https://raw.githubusercontent.com/messense/jieba-rs/{REVISION}/examples/weicheng/src/weicheng.txt"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    parser.add_argument("--han-only", action="store_true")
    args = parser.parse_args()

    text = urllib.request.urlopen(URL).read().decode("utf-8")
    lines = []
    for line in text.splitlines():
        line = line.replace("\u3000", " ").strip()
        if args.han_only:
            line = "".join(char for char in line if "\u4e00" <= char <= "\u9fff")
        if line:
            lines.append(line)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text("\n".join(lines) + "\n")
    print(f"wrote {args.output}: {args.output.stat().st_size} bytes, {len(lines)} documents")


if __name__ == "__main__":
    main()
