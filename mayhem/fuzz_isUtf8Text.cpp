#include <stdint.h>
#include <stdio.h>
#include <climits>

#include <fuzzer/FuzzedDataProvider.h>

namespace das
{
    extern bool isUtf8Text(const char *src, uint32_t length);
}
extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    FuzzedDataProvider provider(data, size);
    std::string str = provider.ConsumeRandomLengthString();
    const char* cstr = str.c_str();
    das::isUtf8Text(cstr, strlen(cstr));

    return 0;
}