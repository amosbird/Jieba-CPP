#include <jieba_common.h>

#include <span>

namespace Jieba
{

class DartsDict;

namespace
{

enum State
{
    B = 0,
    E = 1,
    M = 2,
    S = 3
};

constexpr size_t STATE_COUNT = 4;

/// Short alias used to keep the serialized hmm model file compact.
constexpr double Z = -3.14e+100;

/// Defines start_prob, trans_prob, emit_probs.
///
/// The HMM tables are indexed by raw Unicode codepoint (`emit_probs[state][rune]`).
/// `Rune` is `uint16_t` and `decodeUTF8Rune` returns either the raw codepoint or
/// the sentinel `0xFFFF` (for >BMP) / `0xFFFD` (for invalid UTF-8); both fit, so
/// every possible `Rune` value is a valid index into the table.
#include "jieba_hmm_model.dat"

std::vector<State> viterbi(std::span<const Rune> runes)
{
    std::vector<State> states;
    size_t len = runes.size();
    if (len == 0)
        return states;

    states.resize(len, B);
    if (len == 1)
    {
        states[0] = S;
        return states;
    }

    std::vector<std::array<double, STATE_COUNT>> weight(len);
    std::vector<std::array<State, STATE_COUNT>> path(len);

    for (size_t s = 0; s < STATE_COUNT; s++)
        weight[0][s] = start_prob[s] + emit_probs[s][runes[0]];

    for (size_t i = 1; i < len; i++)
    {
        for (size_t curr = 0; curr < STATE_COUNT; curr++)
        {
            weight[i][curr] = Z;
            double emit = emit_probs[curr][runes[i]];
            for (size_t prev = 0; prev < STATE_COUNT; prev++)
            {
                double score = weight[i - 1][prev] + trans_prob[prev][curr] + emit;
                if (score > weight[i][curr])
                {
                    weight[i][curr] = score;
                    path[i][curr] = static_cast<State>(prev);
                }
            }
        }
    }

    State state = weight[len - 1][E] >= weight[len - 1][S] ? E : S;
    for (size_t i = len; i-- > 0;)
    {
        states[i] = state;
        state = path[i][state];
    }

    return states;
}

}

struct HMMSegment
{
    static void cut(const DartsDict & /* dict */, const Runes & runes, size_t begin, size_t end, RuneRanges & ranges);
};

/// `coarse_grained` segmentation of a single-character block: ASCII is handled uniformly
/// by `segmentWithAsciiHandling` (alphanumeric runs kept together, non-word ASCII dropped),
/// and each non-ASCII subrange is segmented by the HMM (Viterbi) model.
void HMMSegment::cut(const DartsDict & /* dict */, const Runes & runes_data, size_t begin, size_t end, RuneRanges & ranges)
{
    if (begin >= end || runes_data.empty())
        return;

    const auto & runes = runes_data.getRunes();

    auto hmm_cut = [&](size_t sub_begin, size_t sub_end)
    {
        std::span<const Rune> span(&runes[sub_begin], sub_end - sub_begin);
        std::vector<State> states = viterbi(span);

        size_t word_start = 0;
        for (size_t k = 0; k < states.size(); k++)
        {
            if (states[k] == E || states[k] == S)
            {
                ranges.emplace_back(RuneRange{sub_begin + word_start, sub_begin + k});
                word_start = k + 1;
            }
        }

        if (word_start < states.size())
            ranges.emplace_back(RuneRange{sub_begin + word_start, sub_end - 1});
    };

    segmentWithAsciiHandling(runes, begin, end, ranges, hmm_cut);
}

}
