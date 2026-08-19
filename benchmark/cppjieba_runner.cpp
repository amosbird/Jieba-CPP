#include "common.h"

#include <cppjieba/Jieba.hpp>

#ifndef IMPLEMENTATION_NAME
#  define IMPLEMENTATION_NAME "cppjieba"
#endif

#ifndef DICT_PATH
#  error DICT_PATH is required
#endif
#ifndef HMM_PATH
#  error HMM_PATH is required
#endif
#ifndef USER_DICT_PATH
#  define USER_DICT_PATH ""
#endif
#ifndef DAT_CACHE_PATH
#  define DAT_CACHE_PATH ""
#endif

int main(int argc, char ** argv)
{
    const auto options = parseOptions(argc, argv);
    const auto corpus = loadCorpus(options.corpus);
#ifdef CPPJIEBA_DATRIE
    const cppjieba::Jieba jieba(DICT_PATH, HMM_PATH, USER_DICT_PATH, "", "", DAT_CACHE_PATH);
#else
    const cppjieba::Jieba jieba(DICT_PATH, HMM_PATH, USER_DICT_PATH, "", "");
#endif

    auto run = [&](size_t iterations, bool dump_tokens)
    {
        uint64_t checksum = 0;
        size_t token_count = 0;
        std::vector<cppjieba::Word> tokens;
        for (size_t iteration = 0; iteration < iterations; ++iteration)
        {
            for (const auto & text : corpus.documents)
            {
                tokens.clear();
                if (options.mode == "exact")
                    jieba.Cut(text, tokens, true);
                else
                    jieba.CutForSearch(text, tokens, true);
                token_count += tokens.size();
                for (const auto & token : tokens)
                {
                    consumeToken(checksum, token.word);
                    if (dump_tokens)
                        std::cout << token.word << '\t';
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
    printResult(IMPLEMENTATION_NAME, options.mode, corpus.input_bytes, corpus.documents.size(), options.iterations, token_count, checksum, elapsed);
}
