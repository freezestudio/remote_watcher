#include "dep.h"
#include "sdep.h"
#include "utils.h"
#include "gtest/gtest.h"

using namespace std::literals;

TEST(ServiceUtils, DebugOutput)
{
    debug_output(L"int: {}, char*: {}, wchar_t*: {}"sv, 123, "abc", L"def");
}

int main(int argc, char **argv)
{
    printf("Running main() from %s\n", __FILE__);
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
