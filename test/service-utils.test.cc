#include "common_dep.h"
#include "common_utils.hpp"
#include "service_dep.h"
#include "service_utils.h"
#include "service_utils_ex.h"
#include "gtest/gtest.h"

TEST(ServiceUtils, DebugOutput)
{
    debug_output(L"int: {}, wchar_t*: {}"sv, 123, L"def");
}

TEST(ServiceUtils, GetFolderFiles)
{
    auto test_folder = fs::path{L"f:/templ"};
    auto files = freeze::detail::get_files_without_subdir(test_folder);
    ASSERT_EQ(files.size(), 5);
}

// TEST(ServiceUtilsEx, RegKeyTest)
// {    
// }
