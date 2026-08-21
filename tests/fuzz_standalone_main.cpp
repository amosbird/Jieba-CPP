#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <vector>

extern "C" int LLVMFuzzerTestOneInput(const uint8_t * data, size_t size);

int main(int argc, char ** argv)
{
    for (int i = 1; i < argc; ++i)
    {
        std::ifstream input(argv[i], std::ios::binary);
        std::vector<uint8_t> data((std::istreambuf_iterator<char>(input)), {});
        LLVMFuzzerTestOneInput(data.data(), data.size());
    }
    return 0;
}
