#include "common.h"

#include <jieba.h>

int main(int argc, char ** argv)
{
    const auto options = parseOptions(argc, argv);
    const auto corpus = loadCorpus(options.corpus);
    Jieba::Jieba jieba;

    auto run = [&](size_t iterations, bool dump_tokens)
    {
        uint64_t checksum = 0;
        size_t token_count = 0;
        for (size_t iteration = 0; iteration < iterations; ++iteration)
        {
            for (const auto document : corpus.documents)
            {
                const std::string_view document_view(document);
                const auto tokens = options.mode == "exact" ? jieba.cut(document_view) : jieba.cutAll(document_view);
                token_count += tokens.size();
                for (const auto token : tokens)
                {
                    consumeToken(checksum, token);
                    if (dump_tokens)
                        std::cout << token << '\t';
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
    printResult("jieba-cpp", options.mode, corpus.input_bytes, corpus.documents.size(), options.iterations, token_count, checksum, elapsed);
}
