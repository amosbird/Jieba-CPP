#include <jieba_dict.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace
{

struct Edge
{
    size_t length;
    double weight;
};

std::pair<double, size_t> exhaustive(const std::vector<std::vector<Edge>> & dag, size_t node)
{
    if (node == dag.size())
        return {0.0, 0};
    double best = -3.14e100;
    size_t best_length = 0;
    for (const auto & edge : dag[node])
    {
        const double weight = edge.weight + exhaustive(dag, node + edge.length).first;
        if (weight > best)
        {
            best = weight;
            best_length = edge.length;
        }
    }
    return {best, best_length};
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size)
{
    if (size == 0)
        return 0;
    const size_t node_count = data[0] % 12 + 1;
    Jieba::DAG dag(node_count);
    std::vector<std::vector<Edge>> reference(node_count);
    std::vector<double> weights;
    size_t cursor = 1;
    for (size_t node = 0; node < node_count; ++node)
    {
        dag.startNode(node);
        const size_t edge_count = cursor < size ? data[cursor++] % 8 + 1 : 1;
        for (size_t edge = 0; edge < edge_count; ++edge)
        {
            const size_t max_length = std::min(Jieba::MAX_WORD_LENGTH, node_count - node);
            const size_t length = cursor < size ? data[cursor++] % max_length + 1 : 1;
            const int8_t raw_weight = cursor < size ? static_cast<int8_t>(data[cursor++]) : -1;
            const double weight = raw_weight;
            const auto weight_index = static_cast<uint32_t>(weights.size());
            weights.push_back(weight);
            dag.addEdge(static_cast<uint8_t>(length), weight_index);
            reference[node].push_back({length, weight});
        }
    }

    const auto route = dag.calculateBestPath([&](uint32_t weight_index) { return weights[weight_index]; });
    for (size_t node = 0; node < node_count; ++node)
    {
        const auto expected = exhaustive(reference, node);
        if (std::abs(route.weights[node] - expected.first) > 1e-12 || route.lengths[node] != expected.second)
            __builtin_trap();
    }
    return 0;
}
