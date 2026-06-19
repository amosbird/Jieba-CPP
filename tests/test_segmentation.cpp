// Segmentation correctness: coarse (cut) and fine (cutAll) granularity, plus the
// shared ASCII handling (alphanumeric runs kept together, ASCII punctuation dropped,
// identical across granularities).

#include <jieba.h>

#include "test_main.h"

#include <algorithm>

using namespace jiebacpp_test;

namespace
{

std::vector<std::string> toStrings(const std::vector<std::string_view> & v)
{
    std::vector<std::string> out;
    out.reserve(v.size());
    for (auto sv : v)
        out.emplace_back(sv);
    return out;
}

} // namespace

int main()
{
    Jieba::Jieba jieba;

    // --- coarse_grained (cut): one non-overlapping segmentation ---
    CHECK_EQ(toStrings(jieba.cut("")), (std::vector<std::string>{}));
    CHECK_EQ(toStrings(jieba.cut("他来到了网易杭研大厦")),
             (std::vector<std::string>{"他", "来到", "了", "网易", "杭研", "大厦"}));
    CHECK_EQ(toStrings(jieba.cut("我来自北京邮电大学")),
             (std::vector<std::string>{"我", "来自", "北京邮电大学"}));
    CHECK_EQ(toStrings(jieba.cut("南京市长江大桥")),
             (std::vector<std::string>{"南京市", "长江大桥"}));

    // --- fine_grained (cutAll): overlapping sub-words ---
    {
        auto t = toStrings(jieba.cutAll("北京邮电大学"));
        auto has = [&](const std::string & w) { return std::find(t.begin(), t.end(), w) != t.end(); };
        CHECK(has("北京"));
        CHECK(has("邮电"));
        CHECK(has("大学"));
        CHECK(has("北京邮电大学"));
        // fine_grained emits strictly more tokens than coarse for this input
        CHECK(t.size() > jieba.cut("北京邮电大学").size());
    }

    // --- ASCII handling, identical across granularities ---
    // Alphanumeric runs stay as one token.
    CHECK_EQ(toStrings(jieba.cut("5G")), (std::vector<std::string>{"5G"}));
    CHECK_EQ(toStrings(jieba.cutAll("5G")), (std::vector<std::string>{"5G"}));
    CHECK_EQ(toStrings(jieba.cut("iPhone6s")), (std::vector<std::string>{"iPhone6s"}));

    // ASCII punctuation is dropped (not emitted as standalone tokens), in both modes.
    CHECK_EQ(toStrings(jieba.cut("hello,world")), (std::vector<std::string>{"hello", "world"}));
    CHECK_EQ(toStrings(jieba.cutAll("hello,world")), (std::vector<std::string>{"hello", "world"}));
    CHECK_EQ(toStrings(jieba.cut("a;b;c")), (std::vector<std::string>{"a", "b", "c"}));

    // Mixed ASCII + Chinese.
    CHECK_EQ(toStrings(jieba.cut("5G网络")), (std::vector<std::string>{"5G", "网络"}));

    // Full-width Chinese punctuation is a separator.
    CHECK_EQ(toStrings(jieba.cut("你好，世界")), (std::vector<std::string>{"你好", "世界"}));

    // Distinct dictionary words keep distinct entries (regression for the old
    // non-injective rune encoding that collapsed e.g. 一 U+4E00 / 仰 U+4EF0).
    CHECK_EQ(toStrings(jieba.cut("一头")), (std::vector<std::string>{"一头"}));
    CHECK_EQ(toStrings(jieba.cut("仰头")), (std::vector<std::string>{"仰头"}));

    TEST_MAIN_RETURN();
}
