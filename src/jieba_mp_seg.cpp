#include <jieba_dict.h>

namespace Jieba
{

struct MPSegment
{
    static void cut(const DartsDict & dict, const Runes & runes, size_t begin, size_t end, RuneRanges & ranges);
};

void MPSegment::cut(const DartsDict & dict, const Runes & runes_data, size_t begin, size_t end, RuneRanges & ranges)
{
    if (begin >= end || runes_data.empty())
        return;

    const auto & runes = runes_data.getRunes();
    std::span<const Rune> span(&runes[begin], end - begin);
    auto dag = dict.buildDAG(span);
    auto route = dag.calculateBestPath([&](uint32_t weight_index) { return dict.weightAt(weight_index); });
    for (size_t i = 0; i < dag.size();)
    {
        size_t next = i + route.lengths[i];
        ranges.push_back({begin + i, begin + next - 1});
        i = next;
    }
}

}
