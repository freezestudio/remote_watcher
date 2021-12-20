#ifndef SERVICE_UTILS_H
#define SERVICE_UTILS_H

#if defined(_DEBUG) || defined(DEBUG)

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
	bool check_exists(fs::path const& path);
	bool normal_check_exists(fs::path const& path);
}

namespace freeze::detail
{
	enum class response_type : int
	{
		result,
		status,
		overlapped,
	};
	std::wstring to_str(response_type t);
	std::wstring to_str(DWORD notify);
}

namespace freeze::detail
{
    bool save_ip(DWORD);
    DWORD read_ip();
    bool save_latest_folder(std::wstring const&);
    std::wstring read_latest_folder();
	bool save_token(std::wstring const&);
	std::string read_token();
}

#else
#define debug_output(...)
#endif

#endif
