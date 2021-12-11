#ifndef SERVICE_UTILS_H
#define SERVICE_UTILS_H

#ifdef _DEBUG

#include <string>
#include <filesystem>

namespace fs = std::filesystem;
using namespace std::literals;

template <typename... Args>
void debug_output(std::wstring_view const &fmt, Args &&...args)
{
    auto msg = std::format(fmt, args...);
    
    // test only
    // std::wcout << msg << std::endl;

    OutputDebugString(msg.data());
}

namespace freeze::detail
{	
	std::string to_utf8(const wchar_t*, int);
    std::string to_utf8(std::wstring const&);
	std::wstring to_utf16(std::string const&);
}

namespace freeze::detail
{
    DWORD make_ip_address(std::wstring const&);
    std::string parse_ip_address(DWORD);
}

namespace freeze::detail
{
	fs::path to_normal(fs::path const& path);
	bool check_exist(fs::path const& path);
	bool normal_check_exists(fs::path const& path);
}

#else
#define debug_output(...)
#endif

#endif
