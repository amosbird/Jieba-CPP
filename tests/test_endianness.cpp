// Endianness mirror check (runs on any host, no big-endian hardware required).
//
// The big-endian dictionary `dict_be.dat.zst` must be the exact element-wise
// byte-swap of the little-endian `dict_le.dat.zst`: same header field values,
// same float64 weights, same uint32 trie elements, only stored in the opposite
// byte order. The little-endian file is already validated by the segmentation
// tests, so this symmetry is what guarantees the big-endian build is correct on
// a big-endian host without needing to run there.
//
// We decompress both files, then compare:
//   * header: parse <dQQ vs >dQQ, fields must be equal;
//   * weights: float64 LE vs float64 BE, element-wise equal;
//   * trie:    uint32 LE vs uint32 BE, element-wise equal.

#include "test_main.h"

#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <zstd.h>

using namespace jiebacpp_test;

namespace
{

std::vector<unsigned char> readFile(const std::string & path)
{
    std::ifstream f(path, std::ios::binary);
    return std::vector<unsigned char>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

std::vector<unsigned char> zstdDecompress(const std::vector<unsigned char> & in)
{
    unsigned long long size = ZSTD_getFrameContentSize(in.data(), in.size());
    std::vector<unsigned char> out(static_cast<size_t>(size));
    size_t got = ZSTD_decompress(out.data(), out.size(), in.data(), in.size());
    if (ZSTD_isError(got) || got != size)
        out.clear();
    return out;
}

template <typename T>
T loadBE(const unsigned char * p)
{
    T v = 0;
    for (size_t i = 0; i < sizeof(T); ++i)
        v = static_cast<T>((v << 8) | p[i]);
    return v;
}

template <typename T>
T loadLE(const unsigned char * p)
{
    T v = 0;
    for (size_t i = 0; i < sizeof(T); ++i)
        v |= static_cast<T>(static_cast<T>(p[i]) << (8 * i));
    return v;
}

} // namespace

int main(int argc, char ** argv)
{
    if (argc < 2)
    {
        std::fprintf(stderr, "usage: %s <data-dir>\n", argv[0]);
        return 2;
    }
    const std::string dir = argv[1];

    auto le = zstdDecompress(readFile(dir + "/dict_le.dat.zst"));
    auto be = zstdDecompress(readFile(dir + "/dict_be.dat.zst"));

    CHECK(!le.empty());
    CHECK(!be.empty());
    CHECK_EQ(le.size(), be.size());
    if (le.empty() || be.empty() || le.size() != be.size())
        TEST_MAIN_RETURN();

    // Header: min_weight (f64) + num_elems (u64) + da_size (u64).
    uint64_t num_elems_le = loadLE<uint64_t>(le.data() + 8);
    uint64_t num_elems_be = loadBE<uint64_t>(be.data() + 8);
    uint64_t da_size_le = loadLE<uint64_t>(le.data() + 16);
    uint64_t da_size_be = loadBE<uint64_t>(be.data() + 16);
    CHECK_EQ(num_elems_le, num_elems_be);
    CHECK_EQ(da_size_le, da_size_be);

    uint64_t min_weight_le = loadLE<uint64_t>(le.data());
    uint64_t min_weight_be = loadBE<uint64_t>(be.data());
    CHECK_EQ(min_weight_le, min_weight_be); // bit pattern of the f64 must match

    const size_t num_elems = num_elems_le;
    const size_t da_size = da_size_le;

    // weights: num_elems x float64
    const size_t weights_off = 24;
    for (size_t i = 0; i < num_elems; ++i)
    {
        uint64_t a = loadLE<uint64_t>(le.data() + weights_off + 8 * i);
        uint64_t b = loadBE<uint64_t>(be.data() + weights_off + 8 * i);
        if (a != b)
        {
            CHECK_EQ(a, b);
            break; // report once, don't flood
        }
    }

    // trie: da_size x uint32
    const size_t trie_off = weights_off + 8 * num_elems;
    CHECK_EQ(trie_off + 4 * da_size, le.size());
    for (size_t i = 0; i < da_size; ++i)
    {
        uint32_t a = loadLE<uint32_t>(le.data() + trie_off + 4 * i);
        uint32_t b = loadBE<uint32_t>(be.data() + trie_off + 4 * i);
        if (a != b)
        {
            CHECK_EQ(a, b);
            break;
        }
    }

    TEST_MAIN_RETURN();
}
