#include "CNetBitStreamAndroid.h"

#include <cstdio>
#include <string>
#include <vector>

static bool ExpectBytes(const char* label, const CNetBitStreamAndroid& stream,
                        const std::vector<unsigned char>& expected)
{
    const int used = stream.GetNumberOfBytesUsed();
    const unsigned char* data = stream.GetData();
    bool ok = used == static_cast<int>(expected.size());
    if (ok)
    {
        for (size_t i = 0; i < expected.size(); ++i)
        {
            if (data[i] != expected[i])
            {
                ok = false;
                break;
            }
        }
    }

    if (!ok)
    {
        std::printf("[FAIL] %s\n", label);
        std::printf("  expected (%zu):", expected.size());
        for (unsigned char byte : expected)
            std::printf(" %02X", byte);
        std::printf("\n  actual   (%d):", used);
        for (int i = 0; i < used; ++i)
            std::printf(" %02X", data[i]);
        std::printf("\n");
    }
    else
    {
        std::printf("[OK]   %s\n", label);
    }
    return ok;
}

static bool TestWriteLength()
{
    struct Case
    {
        unsigned int value;
        std::vector<unsigned char> expected;
    };

    const Case cases[] = {
        {0u, {0x00}},
        {1u, {0x01}},
        {127u, {0x7F}},
        {128u, {0x80, 0x80}},
        {129u, {0x80, 0x81}},
        {0x7EFFu, {0xFE, 0xFF}},
        {0x7F00u, {0xFF, 0x00, 0x7F, 0x00, 0x00}},
        {0x01020304u, {0xFF, 0x04, 0x03, 0x02, 0x01}},
    };

    bool ok = true;
    for (const auto& testCase : cases)
    {
        CNetBitStreamAndroid stream;
        stream.WriteLength(testCase.value);
        std::string label = "WriteLength(" + std::to_string(testCase.value) + ")";
        ok &= ExpectBytes(label.c_str(), stream, testCase.expected);
    }
    return ok;
}

static bool TestReadLength()
{
    struct Case
    {
        unsigned int value;
        std::vector<unsigned char> encoded;
    };

    const Case cases[] = {
        {0u, {0x00}},
        {127u, {0x7F}},
        {128u, {0x80, 0x80}},
        {0x7EFFu, {0xFE, 0xFF}},
        {0x7F00u, {0xFF, 0x00, 0x7F, 0x00, 0x00}},
        {0x01020304u, {0xFF, 0x04, 0x03, 0x02, 0x01}},
    };

    bool ok = true;
    for (const auto& testCase : cases)
    {
        CNetBitStreamAndroid stream(testCase.encoded.data(),
                                    static_cast<unsigned int>(testCase.encoded.size()),
                                    0x06B);
        unsigned int decoded = 0;
        bool result = stream.ReadLength(decoded);
        std::string label = "ReadLength(" + std::to_string(testCase.value) + ")";
        if (!result || decoded != testCase.value)
        {
            ok = false;
            std::printf("[FAIL] %s -> result=%s decoded=%u\n", label.c_str(), result ? "true" : "false", decoded);
        }
        else
        {
            std::printf("[OK]   %s\n", label.c_str());
        }
    }
    return ok;
}

static bool TestWriteStr()
{
    CNetBitStreamAndroid stream;
    stream.WriteStr("abc");
    return ExpectBytes("WriteStr(\"abc\")", stream, {0x03, 'a', 'b', 'c'});
}

static bool TestReadStr()
{
    CNetBitStreamAndroid stream;
    const std::string input = "hello";
    stream.WriteStr(input);

    CNetBitStreamAndroid reader(stream.GetData(),
                                static_cast<unsigned int>(stream.GetNumberOfBytesUsed()),
                                0x06B);
    std::string output;
    bool result = reader.ReadStr(output);
    if (!result || output != input)
    {
        std::printf("[FAIL] ReadStr roundtrip -> result=%s output='%s'\n", result ? "true" : "false",
                    output.c_str());
        return false;
    }
    std::printf("[OK]   ReadStr roundtrip\n");
    return true;
}

static bool TestBitOrder()
{
    CNetBitStreamAndroid stream;
    const bool bits[8] = {true, false, true, true, false, false, true, false};
    for (bool bit : bits)
        stream.WriteBit(bit);
    return ExpectBytes("WriteBit order", stream, {0x4D});
}

int main()
{
    int failures = 0;
    if (!TestWriteLength())
        ++failures;
    if (!TestReadLength())
        ++failures;
    if (!TestWriteStr())
        ++failures;
    if (!TestReadStr())
        ++failures;
    if (!TestBitOrder())
        ++failures;

    if (failures)
    {
        std::printf("Harness failed: %d test group(s)\n", failures);
        return 1;
    }
    std::printf("Harness passed\n");
    return 0;
}
