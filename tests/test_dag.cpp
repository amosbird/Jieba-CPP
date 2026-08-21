#include <jieba_dict.h>

#include "test_main.h"

#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <type_traits>

using namespace jiebacpp_test;

int main()
{
    static_assert(std::is_same_v<Jieba::DAGEdge::length_type, uint8_t>);
    static_assert(sizeof(Jieba::DAGEdge) <= 8);
    static_assert(Jieba::MAX_WORD_LENGTH <= std::numeric_limits<Jieba::DAGEdge::length_type>::max());

    Jieba::DAG dag(3);
    dag.startNode(0);
    dag.addEdge(1, 2);
    dag.addEdge(2, 1);
    dag.startNode(1);
    dag.addEdge(1, 3);
    dag.startNode(2);
    dag.addEdge(1, 4);

    CHECK_EQ(dag.size(), size_t{3});
    CHECK_EQ(dag.edgeCount(), size_t{4});
    CHECK_EQ(dag.edgesAt(0).size(), size_t{2});
    CHECK_EQ(dag.edgesAt(0)[0].length, uint8_t{1});
    CHECK_EQ(dag.edgesAt(0)[1].length, uint8_t{2});
    CHECK_EQ(dag.edgesAt(1).size(), size_t{1});
    CHECK_EQ(dag.edgesAt(1)[0].length, uint8_t{1});
    CHECK_EQ(dag.edgesAt(1)[0].weight_index, uint32_t{3});
    CHECK_EQ(dag.edgesAt(2).size(), size_t{1});
    CHECK_EQ(dag.edgesAt(2)[0].length, uint8_t{1});

    /// Force edge-vector reallocation and verify that node offsets, including the final node,
    /// remain valid. The maximum dictionary word length is 32 runes.
    Jieba::DAG dense_dag(2);
    dense_dag.startNode(0);
    for (uint8_t length = 1; length <= 32; ++length)
        dense_dag.addEdge(length, length);
    dense_dag.startNode(1);
    dense_dag.addEdge(1, 1);

    CHECK_EQ(dense_dag.edgeCount(), size_t{33});
    CHECK_EQ(dense_dag.edgesAt(0).size(), size_t{32});
    CHECK_EQ(dense_dag.edgesAt(0).front().length, uint8_t{1});
    CHECK_EQ(dense_dag.edgesAt(0).back().length, uint8_t{32});
    CHECK_EQ(dense_dag.edgesAt(1).size(), size_t{1});
    CHECK_EQ(dense_dag.edgesAt(1).front().length, uint8_t{1});

    /// Property test the flat offsets against an independent vector-of-vectors model.
    std::mt19937 rng(0x4A494542);
    for (size_t trial = 0; trial < 1000; ++trial)
    {
        const size_t node_count = rng() % 128 + 1;
        Jieba::DAG random_dag(node_count);
        std::vector<std::vector<std::pair<uint8_t, uint32_t>>> expected(node_count);
        for (size_t node = 0; node < node_count; ++node)
        {
            random_dag.startNode(node);
            const size_t edge_count = rng() % Jieba::MAX_WORD_LENGTH + 1;
            for (size_t edge = 0; edge < edge_count; ++edge)
            {
                const auto length = static_cast<uint8_t>(rng() % Jieba::MAX_WORD_LENGTH + 1);
                const uint32_t weight_index = rng();
                random_dag.addEdge(length, weight_index);
                expected[node].emplace_back(length, weight_index);
            }
        }

        for (size_t node = 0; node < node_count; ++node)
        {
            const auto actual = random_dag.edgesAt(node);
            CHECK_EQ(actual.size(), expected[node].size());
            for (size_t edge = 0; edge < actual.size(); ++edge)
            {
                CHECK_EQ(actual[edge].length, expected[node][edge].first);
                CHECK_EQ(actual[edge].weight_index, expected[node][edge].second);
            }
        }
    }

    /// Validate the compact representation produced by the embedded real dictionary.
    Jieba::DartsDict dict;
    for (const std::string input : {"", "南京市长江大桥", "中华人民共和国北京邮电大学", "一二三四五六七八九十"})
    {
        const auto runes = Jieba::decodeUTF8String(input);
        const auto dag_from_dict = dict.buildDAG(runes.getRunes());
        CHECK_EQ(dag_from_dict.size(), runes.size());
        for (size_t node = 0; node < dag_from_dict.size(); ++node)
        {
            const auto node_edges = dag_from_dict.edgesAt(node);
            CHECK(!node_edges.empty());
            CHECK_EQ(node_edges.front().length, uint8_t{1});
            for (const auto & edge : node_edges)
            {
                CHECK(edge.length >= 1);
                CHECK(edge.length <= Jieba::MAX_WORD_LENGTH);
                CHECK(node + edge.length <= dag_from_dict.size());
            }
        }
    }

    TEST_MAIN_RETURN();
}
