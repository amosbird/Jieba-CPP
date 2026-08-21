#include <jieba_dict.h>

#include "test_main.h"

#include <array>
#include <cmath>
#include <functional>
#include <vector>

using namespace jiebacpp_test;

namespace
{

struct ReferenceEdge
{
    size_t length;
    double weight;
};

std::pair<double, size_t> exhaustiveBest(const std::vector<std::vector<ReferenceEdge>> & dag, size_t node)
{
    if (node == dag.size())
        return {0.0, 0};

    double best_weight = -3.14e100;
    size_t best_length = 0;
    for (const auto & edge : dag[node])
    {
        const double weight = edge.weight + exhaustiveBest(dag, node + edge.length).first;
        if (weight > best_weight)
        {
            best_weight = weight;
            best_length = edge.length;
        }
    }
    return {best_weight, best_length};
}

void checkAgainstExhaustive(const std::vector<std::vector<ReferenceEdge>> & reference)
{
    Jieba::DAG dag(reference.size());
    for (size_t node = 0; node < reference.size(); ++node)
    {
        dag.startNode(node);
        for (const auto & edge : reference[node])
            dag.addEdge(static_cast<uint8_t>(edge.length), static_cast<uint32_t>(edge.weight + 1000));
    }

    auto route = dag.calculateBestPath([](uint32_t weight_index) { return static_cast<double>(weight_index) - 1000; });
    for (size_t node = 0; node < reference.size(); ++node)
    {
        const auto expected = exhaustiveBest(reference, node);
        CHECK(std::abs(route.weights[node] - expected.first) < 1e-12);
        CHECK_EQ(route.lengths[node], expected.second);
    }

    size_t node = 0;
    while (node < dag.size())
    {
        CHECK(route.lengths[node] > 0);
        node += route.lengths[node];
        CHECK(node <= dag.size());
    }
    CHECK_EQ(node, dag.size());
}

} // namespace

int main()
{
    checkAgainstExhaustive({{{1, -1.0}}});
    checkAgainstExhaustive({{{1, -2.0}, {2, -1.0}}, {{1, -3.0}}});
    /// Equal paths must preserve edge insertion order because production DP uses strict `>`.
    checkAgainstExhaustive({{{1, 0.0}, {2, 0.0}}, {{1, 0.0}}});

    /// Bounded exhaustive test: all 5-node DAGs with mandatory length-1 edges,
    /// optional length-2 edges, and weights from a small set including ties.
    constexpr size_t node_count = 5;
    constexpr std::array<double, 3> weights{-1.0, 0.0, 1.0};
    const size_t optional_edges = node_count - 1;
    size_t topology_count = 0;
    for (size_t mask = 0; mask < (size_t{1} << optional_edges); ++mask)
    {
        size_t edge_count = node_count;
        for (size_t node = 0; node < optional_edges; ++node)
            edge_count += (mask >> node) & 1;

        size_t combinations = 1;
        for (size_t edge = 0; edge < edge_count; ++edge)
            combinations *= weights.size();

        for (size_t encoded_weights = 0; encoded_weights < combinations; ++encoded_weights)
        {
            size_t value = encoded_weights;
            std::vector<std::vector<ReferenceEdge>> reference(node_count);
            for (size_t node = 0; node < node_count; ++node)
            {
                reference[node].push_back({1, weights[value % weights.size()]});
                value /= weights.size();
                if (node < optional_edges && ((mask >> node) & 1))
                {
                    reference[node].push_back({2, weights[value % weights.size()]});
                    value /= weights.size();
                }
            }
            checkAgainstExhaustive(reference);
        }
        ++topology_count;
    }
    CHECK_EQ(topology_count, size_t{16});

    TEST_MAIN_RETURN();
}
