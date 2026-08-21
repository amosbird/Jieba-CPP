#!/usr/bin/env bash
set -euo pipefail

ROOT=$(cd "$(dirname "$0")/.." && pwd)
THIRD_PARTY="$ROOT/benchmark/third_party"
source "$ROOT/benchmark/versions.lock"
mkdir -p "$THIRD_PARTY"

clone_at() {
    local repo=$1
    local revision=$2
    local destination=$3
    if [[ -d "$destination/.git" ]]; then
        git -C "$destination" fetch --depth 1 origin "$revision"
        git -C "$destination" checkout --detach "$revision"
        return
    fi
    git clone --no-checkout "$repo" "$destination"
    git -C "$destination" fetch --depth 1 origin "$revision"
    git -C "$destination" checkout --detach "$revision"
}

if [[ ! -d "$THIRD_PARTY/cppjieba" ]]; then
    curl -L "https://github.com/yanyiwu/cppjieba/archive/$CPPJIEBA.tar.gz" | tar -xz -C "$THIRD_PARTY"
    mv "$THIRD_PARTY/cppjieba-$CPPJIEBA" "$THIRD_PARTY/cppjieba"
fi
curl -L "https://raw.githubusercontent.com/yanyiwu/cppjieba/$CPPJIEBA_DATA/dict/jieba.dict.utf8" -o "$THIRD_PARTY/cppjieba/dict/jieba.dict.utf8"
curl -L "https://raw.githubusercontent.com/yanyiwu/cppjieba/$CPPJIEBA_DATA/dict/hmm_model.utf8" -o "$THIRD_PARTY/cppjieba/dict/hmm_model.utf8"
clone_at https://github.com/byronhe/cppjieba.git "$CPPJIEBA_DATRIE" "$THIRD_PARTY/cppjieba-datrie"
clone_at https://github.com/messense/jieba-rs.git "$JIEBA_RS" "$THIRD_PARTY/jieba-rs"
clone_at https://github.com/Nexaloid/Nexaloid.git "$NEXALOID" "$THIRD_PARTY/nexaloid"
clone_at https://github.com/ClickHouse/simdutf.git "$SIMDUTF" "$THIRD_PARTY/simdutf"
