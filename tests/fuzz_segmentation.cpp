#include <jieba.h>
#include <jieba_common.h>

#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string_view>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size)
{
    const std::string_view input(reinterpret_cast<const char *>(data), size);
    const auto runes = Jieba::decodeUTF8String(input);
    size_t expected_offset = 0;
    for (const auto & info : runes.getInfos())
    {
        if (info.offset != expected_offset || info.len == 0 || info.offset + info.len > input.size())
            __builtin_trap();
        expected_offset += info.len;
    }
    if (expected_offset != input.size())
        __builtin_trap();

    static Jieba::Jieba jieba;
    for (const auto tokens : {jieba.cut(input), jieba.cutAll(input)})
    {
        for (const auto token : tokens)
        {
            const uintptr_t input_begin = reinterpret_cast<uintptr_t>(input.data());
            const uintptr_t input_end = input_begin + input.size();
            const uintptr_t token_begin = reinterpret_cast<uintptr_t>(token.data());
            if (token.empty() || token_begin < input_begin || token_begin > input_end || token.size() > input_end - token_begin)
                __builtin_trap();
        }
    }
    return 0;
}
