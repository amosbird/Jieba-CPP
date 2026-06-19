#pragma once

#include <cstdint>
#include <string_view>
#include <vector>

namespace Jieba
{

/// One Unicode codepoint, clamped to the Basic Multilingual Plane.
/// Codepoints beyond the BMP (which UTF-8 encodes as 4 bytes) are mapped to the
/// sentinel value `0xFFFF` so that the HMM emission table can stay BMP-sized.
using Rune = uint16_t;

/// Stores the offset and length information of a Rune in the original string
struct RuneInfo
{
    size_t offset = 0;
    size_t len = 0;
};

/// Runes container: manages both Rune values and their RuneInfo
class Runes
{
public:
    Runes() = default;
    explicit Runes(size_t reserve_size)
    {
        runes.reserve(reserve_size);
        infos.reserve(reserve_size);
    }

    void add(Rune r, size_t offset, size_t len)
    {
        runes.push_back(r);
        infos.push_back(RuneInfo{offset, len});
    }

    Rune runeAt(size_t i) const { return runes[i]; }
    RuneInfo infoAt(size_t i) const { return infos[i]; }

    size_t size() const { return runes.size(); }
    bool empty() const { return runes.empty(); }

    const std::vector<Rune> & getRunes() const { return runes; }
    const std::vector<RuneInfo> & getInfos() const { return infos; }

    /// Clear both runes and infos
    void clear()
    {
        runes.clear();
        infos.clear();
    }

private:
    std::vector<Rune> runes;
    std::vector<RuneInfo> infos;
};

/// [begin, end], inclusive
struct RuneRange
{
    size_t begin = 0;
    size_t end = 0;

    size_t size() const { return end - begin + 1; }
};

using RuneRanges = std::vector<RuneRange>;

/// Raw ASCII tests on the raw codepoint (no encoding remap).
inline bool isAsciiLetter(Rune r)
{
    return ('a' <= r && r <= 'z') || ('A' <= r && r <= 'Z');
}

inline bool isAsciiDigit(Rune r)
{
    return '0' <= r && r <= '9';
}

inline bool isAsciiAlphaNumeric(Rune r)
{
    return isAsciiLetter(r) || isAsciiDigit(r);
}

/// Match a continuous alphanumeric run starting at `i`, returning the index past its end.
/// Mixed forms such as `5G`, `iPhone6s` and `H2O` are consumed as a single token.
/// A leading run of digits is allowed to contain `.` (so floats like `3.14` stay in one
/// token); once a letter has been seen we stop accepting `.` to avoid swallowing
/// sentence-ending dots.
inline size_t matchAlphaOrDigitSeq(const std::vector<Rune> & runes, size_t i)
{
    if (i >= runes.size() || !isAsciiAlphaNumeric(runes[i]))
        return i;

    bool seen_letter = isAsciiLetter(runes[i]);
    ++i;
    while (i < runes.size())
    {
        Rune r = runes[i];
        if (isAsciiAlphaNumeric(r))
        {
            seen_letter = seen_letter || isAsciiLetter(r);
            ++i;
            continue;
        }
        if (r == '.' && !seen_letter && i + 1 < runes.size() && isAsciiDigit(runes[i + 1]))
        {
            ++i;
            continue;
        }
        break;
    }
    return i;
}

/// Drive segmentation over `[begin, end)` with uniform ASCII handling and hand each
/// maximal non-ASCII (`>= 0x80`) subrange to `handle_non_ascii(sub_begin, sub_end)`:
///   - ASCII alphanumeric runs are kept together as a single token (see `matchAlphaOrDigitSeq`),
///   - ASCII non-word characters (punctuation, control bytes, the `\0` padding that
///     `FixedString` adds to short values) are dropped rather than emitted as standalone
///     tokens — they are noise for a text index.
///
/// Both the HMM (`coarse_grained`) and full-DAG (`fine_grained`) segmenters route their
/// non-ASCII work through this driver, so ASCII semantics are identical across
/// granularities (e.g. `5G` stays one token and `,` is dropped in both).
template <typename HandleNonAscii>
inline void segmentWithAsciiHandling(
    const std::vector<Rune> & runes, size_t begin, size_t end, RuneRanges & ranges, HandleNonAscii && handle_non_ascii)
{
    size_t i = begin;
    while (i < end)
    {
        Rune r = runes[i];

        /// ASCII branch (raw codepoint < 0x80).
        if (r < 0x80)
        {
            if (isAsciiAlphaNumeric(r))
            {
                size_t next = matchAlphaOrDigitSeq(runes, i);
                ranges.emplace_back(RuneRange{i, next - 1});
                i = next;
            }
            else
            {
                /// Drop ASCII non-word characters (punctuation, control bytes, padding).
                ++i;
            }
            continue;
        }

        /// Non-ASCII section (likely Chinese / CJK): hand the maximal run to the callback.
        size_t j = i;
        while (j < end && runes[j] >= 0x80)
            ++j;
        handle_non_ascii(i, j);
        i = j;
    }
}

/// Fast UTF-8 to Unicode decoder (BMP-clamped).
/// - Invalid UTF-8 sequences are replaced with `0xFFFD` (replacement char).
/// - 4-byte sequences (codepoint > 0xFFFF) are clamped to `0xFFFF` so we can keep
///   `Rune = uint16_t` and a BMP-sized HMM emission table.
///
/// The returned value is the raw Unicode codepoint, with no remapping applied.
/// All downstream code (HMM `emit_probs[state][rune]` lookup, the `separators` set,
/// ASCII detection in the HMM segmenter) compares against raw codepoint values.
/// Trie keys for `darts-clone` use a separate, explicit byte encoding (see
/// `encodeRuneKey` in `jieba_dict.h`) because `darts-clone` cannot store `\0`
/// bytes inside keys — that encoding is the only place where bytes are reshuffled.
inline Rune decodeUTF8Rune(const char * str, size_t len, size_t & out_len)
{
    uint8_t b0 = static_cast<uint8_t>(str[0]);

    if (b0 < 0x80)
    {
        out_len = 1;
        return b0;
    }
    if ((b0 & 0xE0) == 0xC0 && len >= 2 && (static_cast<uint8_t>(str[1]) & 0xC0) == 0x80)
    {
        out_len = 2;
        return ((b0 & 0x1F) << 6) | (static_cast<uint8_t>(str[1]) & 0x3F);
    }
    if ((b0 & 0xF0) == 0xE0 && len >= 3 && (static_cast<uint8_t>(str[1]) & 0xC0) == 0x80
        && (static_cast<uint8_t>(str[2]) & 0xC0) == 0x80)
    {
        out_len = 3;
        return ((b0 & 0x0F) << 12) | ((static_cast<uint8_t>(str[1]) & 0x3F) << 6) | (static_cast<uint8_t>(str[2]) & 0x3F);
    }
    if ((b0 & 0xF8) == 0xF0 && len >= 4 && (static_cast<uint8_t>(str[1]) & 0xC0) == 0x80
        && (static_cast<uint8_t>(str[2]) & 0xC0) == 0x80 && (static_cast<uint8_t>(str[3]) & 0xC0) == 0x80)
    {
        out_len = 4;
        return 0xFFFF; // Beyond BMP — clamp to the sentinel value.
    }

    out_len = 1;
    return 0xFFFD; // Invalid UTF-8
}

/// Decode UTF-8 string into Runes (values + infos)
inline Runes decodeUTF8String(std::string_view str)
{
    Runes runes(str.size() / 2);
    size_t pos = 0;
    while (pos < str.size())
    {
        size_t len = 0;
        Rune r = decodeUTF8Rune(str.data() + pos, str.size() - pos, len);
        runes.add(r, pos, len);
        pos += len;
    }
    return runes;
}

}
