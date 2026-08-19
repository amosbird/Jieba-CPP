#pragma once

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

struct Options
{
    std::string corpus;
    std::string mode = "exact";
    size_t iterations = 1;
    size_t warmup = 1;
    bool dump_tokens = false;
};

inline Options parseOptions(int argc, char ** argv)
{
    Options options;
    for (int i = 1; i < argc; ++i)
    {
        std::string_view arg = argv[i];
        auto next = [&]() -> std::string_view
        {
            if (++i >= argc)
                throw std::runtime_error("missing option value");
            return argv[i];
        };
        if (arg == "--corpus")
            options.corpus = next();
        else if (arg == "--mode")
            options.mode = next();
        else if (arg == "--iterations")
            options.iterations = std::stoull(std::string(next()));
        else if (arg == "--warmup")
            options.warmup = std::stoull(std::string(next()));
        else if (arg == "--dump-tokens")
            options.dump_tokens = true;
        else
            throw std::runtime_error("unknown option: " + std::string(arg));
    }
    if (options.corpus.empty())
        throw std::runtime_error("--corpus is required");
    if (options.mode != "exact" && options.mode != "search")
        throw std::runtime_error("--mode must be exact or search");
    return options;
}

struct Corpus
{
    std::string storage;
    std::vector<std::string> documents;
    size_t input_bytes = 0;
};

inline Corpus loadCorpus(const std::string & path)
{
    std::ifstream input(path, std::ios::binary);
    if (!input)
        throw std::runtime_error("cannot open corpus: " + path);
    Corpus corpus;
    input.seekg(0, std::ios::end);
    corpus.storage.resize(static_cast<size_t>(input.tellg()));
    input.seekg(0);
    input.read(corpus.storage.data(), static_cast<std::streamsize>(corpus.storage.size()));

    size_t begin = 0;
    for (size_t i = 0; i <= corpus.storage.size(); ++i)
    {
        if (i == corpus.storage.size() || corpus.storage[i] == '\n')
        {
            size_t end = i;
            if (end > begin && corpus.storage[end - 1] == '\r')
                --end;
            if (end > begin)
            {
                corpus.documents.emplace_back(corpus.storage.data() + begin, end - begin);
                corpus.input_bytes += end - begin;
            }
            begin = i + 1;
        }
    }
    return corpus;
}

inline void consumeToken(uint64_t & checksum, std::string_view token)
{
    checksum ^= token.size() + 0x9e3779b97f4a7c15ULL + (checksum << 6) + (checksum >> 2);
    if (!token.empty())
    {
        checksum ^= static_cast<unsigned char>(token.front());
        checksum = (checksum << 13) | (checksum >> 51);
        checksum ^= static_cast<unsigned char>(token.back());
    }
}

inline void printResult(
    std::string_view implementation,
    std::string_view mode,
    size_t input_bytes,
    size_t documents,
    size_t iterations,
    size_t tokens,
    uint64_t checksum,
    std::chrono::nanoseconds elapsed)
{
    std::cout << "implementation,mode,input_bytes,documents,iterations,tokens,checksum,elapsed_ns\n";
    std::cout << implementation << ',' << mode << ',' << input_bytes << ',' << documents << ',' << iterations << ',' << tokens << ','
              << checksum << ',' << elapsed.count() << '\n';
}
