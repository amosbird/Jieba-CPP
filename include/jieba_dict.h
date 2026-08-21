#pragma once

#include <cstdint>
#include <limits>
#include <span>
#include <vector>
#include <darts.h>
#include <jieba_common.h>

namespace Jieba
{

constexpr uint32_t FALLBACK_WEIGHT_INDEX = std::numeric_limits<uint32_t>::max();

struct DAGEdge
{
    using length_type = uint8_t;

    uint32_t weight_index = 0;
    length_type length = 0;
};

struct DAGRoute
{
    explicit DAGRoute(size_t size)
        : weights(size, -3.14e+100)
        , lengths(size)
    {
    }

    std::vector<double> weights;
    std::vector<uint8_t> lengths;
};

class DAG
{
public:
    explicit DAG(size_t size)
        : edge_starts(size)
    {
        /// shortcut: Chinese dictionary text averages close to two edges per position;
        /// fall back to one edge per position if doubling would exceed the container limit.
        edges.reserve(size <= edges.max_size() / 2 ? size * 2 : size);
    }

    size_t size() const { return edge_starts.size(); }
    size_t edgeCount() const { return edges.size(); }

    void startNode(size_t index) { edge_starts[index] = edges.size(); }
    void addEdge(uint8_t length, uint32_t weight_index) { edges.push_back({weight_index, length}); }
    void setFirstEdgeWeight(size_t index, uint32_t weight_index) { edges[edge_starts[index]].weight_index = weight_index; }

    std::span<const DAGEdge> edgesAt(size_t index) const
    {
        const size_t edge_begin = edge_starts[index];
        const size_t edge_end = index + 1 < edge_starts.size() ? edge_starts[index + 1] : edges.size();
        return std::span(edges).subspan(edge_begin, edge_end - edge_begin);
    }

    template <typename WeightAt>
    DAGRoute calculateBestPath(WeightAt && weight_at) const
    {
        DAGRoute route(size());
        for (size_t i = size(); i-- > 0;)
        {
            for (const auto & edge : edgesAt(i))
            {
                const size_t next = i + edge.length;
                double weight = weight_at(edge.weight_index);
                if (next < size())
                    weight += route.weights[next];
                if (weight > route.weights[i])
                {
                    route.weights[i] = weight;
                    route.lengths[i] = edge.length;
                }
            }
        }
        return route;
    }

private:
    std::vector<size_t> edge_starts;
    std::vector<DAGEdge> edges;
};

/// On-disk header layout. Sizes match the format produced by `generate_dict.py`
/// (8 bytes each), so we use fixed-width types here regardless of the platform's `size_t`.
struct DartsHeader
{
    double min_weight = 0;
    uint64_t num_elems = 0;
    uint64_t da_size = 0;
};

/// Number of bytes used to encode a single `Rune` in a `darts-clone` trie key.
///
/// `darts-clone` cannot store `0x00` bytes inside keys, so we encode each
/// `uint16_t` codepoint as 3 bytes in big-endian, with the high bit set in
/// every byte:
///
///     byte0 = ((rune >> 12) & 0x0F) | 0x80   // bits 12..15 -> 0x80..0x8F
///     byte1 = ((rune >>  6) & 0x3F) | 0x80   // bits  6..11 -> 0x80..0xBF
///     byte2 = ( rune        & 0x3F) | 0x80   // bits  0.. 5 -> 0x80..0xBF
///
/// This encoding is:
///   * injective (no two distinct `Rune` values map to the same 3-byte sequence),
///   * free of `0x00` bytes (every byte is in `0x80..0xFF`),
///   * endian-independent (bytes are emitted in an explicit order rather than
///     reinterpreting `uint16_t` memory),
///   * lexicographically order-preserving (sorting by `Rune` value matches
///     sorting by encoded byte sequence — required for `Darts::build`).
///
/// The same encoding is produced by `generate_dict.py` when building the trie
/// keys. Runtime callers use `encodeRuneKey` to materialise the lookup key
/// before calling `Darts::DoubleArray::exactMatchSearch` / `commonPrefixSearch`.
constexpr size_t BYTES_PER_RUNE = 3;
constexpr size_t MAX_WORD_LENGTH = 32;
static_assert(MAX_WORD_LENGTH <= std::numeric_limits<DAGEdge::length_type>::max());

/// See the comment on `BYTES_PER_RUNE` for the encoding scheme.
inline void encodeRuneIntoBuffer(Rune rune, char * out)
{
    out[0] = static_cast<char>(((rune >> 12) & 0x0F) | 0x80);
    out[1] = static_cast<char>(((rune >>  6) & 0x3F) | 0x80);
    out[2] = static_cast<char>(( rune        & 0x3F) | 0x80);
}

/// Encode a sequence of runes into the contiguous byte form expected by the
/// `darts-clone` trie. The returned buffer is exactly `runes.size() * BYTES_PER_RUNE`
/// bytes long.
inline std::vector<char> encodeRuneKey(std::span<const Rune> runes)
{
    std::vector<char> key(runes.size() * BYTES_PER_RUNE);
    for (size_t i = 0; i < runes.size(); ++i)
        encodeRuneIntoBuffer(runes[i], key.data() + i * BYTES_PER_RUNE);
    return key;
}

class DartsDict
{
public:
    DartsDict();

    /// `elems` and the internal `da` array point into `storage`, so copying or moving
    /// the object would leave those borrowed pointers dangling at the old buffer.
    /// The dictionary is only ever held as a single long-lived instance, so forbid both.
    DartsDict(const DartsDict &) = delete;
    DartsDict & operator=(const DartsDict &) = delete;
    DartsDict(DartsDict &&) = delete;
    DartsDict & operator=(DartsDict &&) = delete;

    double find(std::span<const Rune> key) const;
    DAG buildDAG(std::span<const Rune> runes) const;
    double weightAt(uint32_t index) const { return index == FALLBACK_WEIGHT_INDEX ? min_weight : elems[index]; }

private:
    /// Decompressed dictionary buffer. Held as uint64_t so that the underlying
    /// storage is guaranteed to be 8-byte aligned, which matches the alignment
    /// requirements of the embedded `double` weights array.
    std::vector<uint64_t> storage;

    ::Darts::DoubleArray da;
    const double * elems = nullptr;
    double min_weight = 0;
    uint64_t num_elems = 0;
};

}
