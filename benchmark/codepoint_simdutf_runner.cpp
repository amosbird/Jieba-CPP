#include "common.h"

#include <simdutf.h>

int main(int argc, char ** argv)
{
    const auto options = parseOptions(argc, argv);
    const auto corpus = loadCorpus(options.corpus);
    std::vector<char32_t> utf32;
    for (const auto & document : corpus.documents)
        utf32.resize(std::max(utf32.size(), document.size()));

    auto run = [&](size_t iterations, bool dump_tokens)
    {
        uint64_t checksum = 0;
        size_t token_count = 0;
        for (size_t iteration = 0; iteration < iterations; ++iteration)
        {
            for (const auto & document : corpus.documents)
            {
                const size_t count = simdutf::convert_utf8_to_utf32(document.data(), document.size(), utf32.data());
                if (count == 0 && !document.empty())
                    throw std::runtime_error("simdutf requires valid UTF-8 input");

                size_t offset = 0;
                for (size_t i = 0; i < count; ++i)
                {
                    const char32_t codepoint = utf32[i];
                    const size_t length = codepoint <= 0x7F ? 1 : codepoint <= 0x7FF ? 2 : codepoint <= 0xFFFF ? 3 : 4;
                    const std::string_view token(document.data() + offset, length);
                    ++token_count;
                    consumeToken(checksum, token);
                    if (dump_tokens)
                        std::cout << token << '\t';
                    offset += length;
                }
                if (offset != document.size())
                    throw std::runtime_error("simdutf did not consume the complete document");
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
    printResult("simdutf-codepoint", "codepoint", corpus.input_bytes, corpus.documents.size(), options.iterations, token_count, checksum, elapsed);
}
