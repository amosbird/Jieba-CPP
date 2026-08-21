#include <jieba.h>

#include <string_view>

int main()
{
    Jieba::Jieba jieba;
    const auto tokens = jieba.cut("我来自北京邮电大学");
    return tokens.size() == 3 && tokens[2] == std::string_view{"北京邮电大学"} ? 0 : 1;
}
