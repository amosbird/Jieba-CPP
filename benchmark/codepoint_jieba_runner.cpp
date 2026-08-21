#include "common.h"

#include <jieba_common.h>

int main(int argc, char ** argv)
{
    const auto options = parseOptions(argc, argv);
    const auto corpus = loadCorpus(options.corpus);

    auto run = [&](size_t iterations, bool dump_tokens)
    {
        uint64_t checksum = 0;
        size_t token_count = 0;
        for (size_t iteration = 0; iteration < iterations; ++iteration)
        {
            for (const auto & document : corpus.documents)
            {
                size_t offset = 0;
                while (offset < document.size())
                {
                    size_t length = 0;
                    Jieba::decodeUTF8Rune(document.data() + offset, document.size() - offset, length);
                    const std::string_view token(document.data() + offset, length);
                    ++token_count;
                    consumeToken(checksum, token);
                    if (dump_tokens)
                        std::cout << token << '\t';
                    offset += length;
                }
                if (dump_tokens)
                    std::cout << '\n';
            }
        }
        return std::pair{token_count, checksum};
    };

    if (options.dump_tokens)
    {
        run(1, true);
        return 0;
    }
    run(options.warmup, false);
    const auto start = std::chrono::steady_clock::now();
    const auto [token_count, checksum] = run(options.iterations, false);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    printResult("jieba-codepoint", "codepoint", corpus.input_bytes, corpus.documents.size(), options.iterations, token_count, checksum, elapsed);
}
