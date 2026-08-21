#include <jieba.h>

#include "test_main.h"

#include <atomic>
#include <string>
#include <thread>
#include <vector>

using namespace jiebacpp_test;

int main()
{
    Jieba::Jieba jieba;
    const std::vector<std::string> inputs{
        "",
        "南京市长江大桥",
        "中华人民共和国北京邮电大学",
        "5G网络和iPhone6s",
        "你好，世界",
        "北京😀大学",
    };

    std::vector<std::vector<std::string_view>> expected;
    expected.reserve(inputs.size());
    for (const auto & input : inputs)
        expected.push_back(jieba.cut(input));

    std::atomic<bool> failed = false;
    std::vector<std::thread> threads;
    for (size_t thread = 0; thread < 16; ++thread)
    {
        threads.emplace_back([&]
        {
            for (size_t iteration = 0; iteration < 1000; ++iteration)
            {
                for (size_t i = 0; i < inputs.size(); ++i)
                {
                    if (jieba.cut(inputs[i]) != expected[i])
                        failed.store(true, std::memory_order_relaxed);
                }
            }
        });
    }
    for (auto & thread : threads)
        thread.join();

    CHECK(!failed.load(std::memory_order_relaxed));
    TEST_MAIN_RETURN();
}
