// Encoding invariants:
//   1. The 3-byte rune encoding (encodeRuneIntoBuffer) is injective over the whole BMP,
//      produces no 0x00 bytes (a darts-clone key requirement), and is lexicographically
//      order-preserving (required by Darts::build). This guards the historical collision
//      bug where the old encoding mapped distinct runes onto the same trie key.
//   2. decodeUTF8Rune returns the raw codepoint, clamps astral (>BMP) to 0xFFFF and
//      maps invalid bytes to 0xFFFD, never reading out of bounds.
//   3. Segmentation over astral characters and malformed UTF-8 does not throw and keeps
//      token byte offsets accurate.

#include <jieba.h>
#include <jieba_common.h>
#include <jieba_dict.h>

#include "test_main.h"

#include <array>
#include <cstring>
#include <set>
#include <string>

using namespace jiebacpp_test;
using Jieba::Rune;

int main()
{
    // --- 1. injective, \0-free, order-preserving 3-byte encoding over the BMP ---
    {
        std::set<std::array<char, Jieba::BYTES_PER_RUNE>> seen;
        std::array<char, Jieba::BYTES_PER_RUNE> prev{};
        bool have_prev = false;
        size_t collisions = 0;
        size_t zero_bytes = 0;
        size_t order_violations = 0;

        for (uint32_t r = 0; r <= 0xFFFF; ++r)
        {
            std::array<char, Jieba::BYTES_PER_RUNE> buf{};
            Jieba::encodeRuneIntoBuffer(static_cast<Rune>(r), buf.data());

            for (char c : buf)
                if (c == 0)
                    ++zero_bytes;

            if (!seen.insert(buf).second)
                ++collisions;

            if (have_prev && !(std::memcmp(prev.data(), buf.data(), Jieba::BYTES_PER_RUNE) < 0))
                ++order_violations;

            prev = buf;
            have_prev = true;
        }

        CHECK_EQ(collisions, size_t{0});       // injective
        CHECK_EQ(zero_bytes, size_t{0});       // no NUL bytes
        CHECK_EQ(order_violations, size_t{0}); // strictly increasing
        CHECK_EQ(seen.size(), size_t{0x10000});
    }

    // --- 2. decodeUTF8Rune raw codepoint, astral clamp, invalid -> replacement ---
    {
        size_t len = 0;

        // ASCII
        CHECK_EQ(Jieba::decodeUTF8Rune("A", 1, len), Rune{'A'});
        CHECK_EQ(len, size_t{1});

        // 3-byte BMP: 北 U+5317
        const char * bei = "北";
        CHECK_EQ(Jieba::decodeUTF8Rune(bei, std::strlen(bei), len), Rune{0x5317});
        CHECK_EQ(len, size_t{3});

        // 4-byte astral: 😀 U+1F600 -> clamp to 0xFFFF, consume 4 bytes
        const char * grin = "😀";
        CHECK_EQ(Jieba::decodeUTF8Rune(grin, std::strlen(grin), len), Rune{0xFFFF});
        CHECK_EQ(len, size_t{4});

        // Invalid lead byte -> replacement char, consume 1 byte
        const char bad[] = {static_cast<char>(0xFF), 0};
        CHECK_EQ(Jieba::decodeUTF8Rune(bad, 1, len), Rune{0xFFFD});
        CHECK_EQ(len, size_t{1});

        // Truncated 3-byte sequence (lead says 3 bytes, only 1 available) -> replacement
        const char trunc[] = {static_cast<char>(0xE5), 0};
        CHECK_EQ(Jieba::decodeUTF8Rune(trunc, 1, len), Rune{0xFFFD});
        CHECK_EQ(len, size_t{1});
    }

    // --- 3. astral + malformed input: no throw, accurate token byte offsets ---
    {
        Jieba::Jieba jieba;

        // The astral char becomes its own token; neighbours keep correct byte slices.
        auto t = jieba.cut("北京😀大学");
        bool found_grin = false;
        for (auto sv : t)
        {
            // every token must be a non-empty, in-range slice
            CHECK(!sv.empty());
            if (sv == std::string_view("😀"))
                found_grin = true;
        }
        CHECK(found_grin);

        // Malformed bytes in the middle must not throw and must not corrupt the
        // surrounding tokens' byte boundaries.
        std::string malformed = "北京";
        malformed.push_back(static_cast<char>(0xFF));
        malformed += "大学";
        auto t2 = jieba.cut(malformed);
        bool found_bei = false;
        bool found_daxue = false;
        for (auto sv : t2)
        {
            found_bei |= (sv == std::string_view("北京"));
            found_daxue |= (sv == std::string_view("大学"));
        }
        CHECK(found_bei);
        CHECK(found_daxue);
    }

    TEST_MAIN_RETURN();
}
