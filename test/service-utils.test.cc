#include "dep.h"
#include "sdep.h"
#include "utils.h"
#include "gtest/gtest.h"

TEST(ServiceUtils, DebugOutput)
{
    debug_output(L"int: {}, wchar_t*: {}"sv, 123, L"def");
}

// int main(int argc, char **argv)
// {
//     printf("Running main() from %s\n", __FILE__);
//     testing::InitGoogleTest(&argc, argv);
//     return RUN_ALL_TESTS();
// }
