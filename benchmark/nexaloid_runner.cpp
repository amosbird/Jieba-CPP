#include "common.h"

#include <nexaloid.h>

#include <cstring>

#ifndef NEXALOID_DICT_PATH
#  error NEXALOID_DICT_PATH is required
#endif
#ifndef NEXALOID_PLUGIN_PATH
#  error NEXALOID_PLUGIN_PATH is required
#endif
#ifndef NEXALOID_HMM_PATH
#  error NEXALOID_HMM_PATH is required
#endif

struct CallbackState
{
    size_t tokens = 0;
    uint64_t checksum = 0;
    bool dump_tokens = false;
};

void onToken(const NxToken * token, const char * text, size_t text_len, void * user_data)
{
    auto & state = *static_cast<CallbackState *>(user_data);
    if (token->end_byte > text_len || token->start_byte > token->end_byte)
        throw std::runtime_error("Nexaloid returned an invalid token range");
    ++state.tokens;
    consumeToken(state.checksum, std::string_view(text + token->start_byte, token->end_byte - token->start_byte));
    if (state.dump_tokens)
        std::cout << std::string_view(text + token->start_byte, token->end_byte - token->start_byte) << '\t';
}

int main(int argc, char ** argv)
{
    const auto options = parseOptions(argc, argv);
    const auto corpus = loadCorpus(options.corpus);
    NxConfig config{};
    config.dict_path = NEXALOID_DICT_PATH;
    NxEngine * engine = nullptr;
    if (nx_engine_new(&config, &engine) != NX_OK)
        throw std::runtime_error("nx_engine_new failed");
    const std::string plugin_config = std::string("{\"artifact\":\"") + NEXALOID_HMM_PATH + "\",\"hmm_score\":-14.0}";
    if (nx_load_plugin(engine, NEXALOID_PLUGIN_PATH, plugin_config.c_str()) != NX_OK)
        throw std::runtime_error("nx_load_plugin failed");

    auto run = [&](size_t iterations, bool dump_tokens)
    {
        CallbackState state;
        state.dump_tokens = dump_tokens;
        const NxMode mode = options.mode == "exact" ? NX_MODE_ACCURATE : NX_MODE_SEARCH;
        for (size_t iteration = 0; iteration < iterations; ++iteration)
            for (const auto & document : corpus.documents)
            {
                if (nx_tokenize(engine, document.data(), document.size(), mode, onToken, &state) != NX_OK)
                    throw std::runtime_error("nx_tokenize failed");
                if (dump_tokens)
                    std::cout << '\n';
            }
        return state;
    };

    if (options.dump_tokens)
    {
        run(1, true);
        nx_engine_free(engine);
        return 0;
    }
    run(options.warmup, false);
    const auto start = std::chrono::steady_clock::now();
    const auto state = run(options.iterations, false);
    const auto elapsed = std::chrono::steady_clock::now() - start;
    nx_engine_free(engine);
    printResult("nexaloid", options.mode, corpus.input_bytes, corpus.documents.size(), options.iterations, state.tokens, state.checksum, elapsed);
}
