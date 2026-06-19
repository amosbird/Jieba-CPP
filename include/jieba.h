#pragma once

#include <jieba_dict.h>

namespace Jieba
{

class Jieba
{
public:
    /// All three methods segment `sentence` (UTF-8) into tokens that are views into the
    /// input buffer. They differ only in the segmentation strategy:

    /// `coarse_grained`: one segmentation into non-overlapping words (dictionary MP +
    /// HMM for unknown runs). This is the default and produces the fewest tokens.
    std::vector<std::string_view> cut(std::string_view sentence);

    /// "Search-engine" mode: like `cut`, but additionally splits long words into their
    /// shorter dictionary sub-words to improve recall. (Not currently exposed by the SQL
    /// tokenizer, which uses `cut`/`cutAll`.)
    std::vector<std::string_view> cutForSearch(std::string_view sentence);

    /// `fine_grained`: enumerates all overlapping dictionary words (full segmentation).
    /// Produces the most tokens and the largest index, with the highest recall.
    std::vector<std::string_view> cutAll(std::string_view sentence);

private:
    template <typename Segment>
    std::vector<std::string_view> cutImpl(std::string_view sentence);

    DartsDict dict;
};

}
