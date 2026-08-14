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

        // Overlong 2-byte encodings are invalid and consume only their lead byte.
        const char overlong_nul[] = {static_cast<char>(0xC0), static_cast<char>(0x80)};
        CHECK_EQ(Jieba::decodeUTF8Rune(overlong_nul, sizeof(overlong_nul), len), Rune{0xFFFD});
        CHECK_EQ(len, size_t{1});

        const char overlong_q[] = {static_cast<char>(0xC1), static_cast<char>(0xB1)};
        CHECK_EQ(Jieba::decodeUTF8Rune(overlong_q, sizeof(overlong_q), len), Rune{0xFFFD});
        CHECK_EQ(len, size_t{1});

        // Overlong 3-byte encoding and UTF-16 surrogate codepoints are invalid.
        const char overlong_three[] = {static_cast<char>(0xE0), static_cast<char>(0x80), static_cast<char>(0x80)};
        CHECK_EQ(Jieba::decodeUTF8Rune(overlong_three, sizeof(overlong_three), len), Rune{0xFFFD});
        CHECK_EQ(len, size_t{1});

        const char surrogate[] = {static_cast<char>(0xED), static_cast<char>(0xA0), static_cast<char>(0x80)};
        CHECK_EQ(Jieba::decodeUTF8Rune(surrogate, sizeof(surrogate), len), Rune{0xFFFD});
        CHECK_EQ(len, size_t{1});

        // Overlong and out-of-range 4-byte encodings are invalid; valid astral input remains clamped.
        const char overlong_four[]
            = {static_cast<char>(0xF0), static_cast<char>(0x80), static_cast<char>(0x80), static_cast<char>(0x80)};
        CHECK_EQ(Jieba::decodeUTF8Rune(overlong_four, sizeof(overlong_four), len), Rune{0xFFFD});
        CHECK_EQ(len, size_t{1});

        const char out_of_range[]
            = {static_cast<char>(0xF4), static_cast<char>(0x90), static_cast<char>(0x80), static_cast<char>(0x80)};
        CHECK_EQ(Jieba::decodeUTF8Rune(out_of_range, sizeof(out_of_range), len), Rune{0xFFFD});
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

        // Overlong encodings stay as malformed byte spans; they must not disappear as NUL
        // separators or mutate into ASCII letters and merge with neighbouring tokens.
        std::string overlong = "a";
        overlong.append("\xC0\x80", 2);
        overlong += "b";
        overlong.append("\xC1\xB1", 2);
        overlong += "c";
        auto t3 = jieba.cut(overlong);
        CHECK_EQ(t3.size(), size_t{5});
        CHECK_EQ(t3[0], std::string_view("a"));
        CHECK_EQ(t3[1], std::string_view(overlong.data() + 1, 2));
        CHECK_EQ(t3[2], std::string_view("b"));
        CHECK_EQ(t3[3], std::string_view(overlong.data() + 4, 2));
        CHECK_EQ(t3[4], std::string_view("c"));
    }

    TEST_MAIN_RETURN();
}
