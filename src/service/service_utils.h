#ifndef SERVICE_UTILS_H
#define SERVICE_UTILS_H

#if defined(_DEBUG) || defined(DEBUG)

#include <bit>
#include <bitset>
#include <string>
#include <vector>
#include <format>
#include <memory>
#include <utility>
#include <filesystem>
#include <tuple>

namespace fs = std::filesystem;
using namespace std::literals;
using volume_type = std::tuple<std::string, std::string, std::string>;

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
    uint32_t make_ip_address(std::wstring const&);
    std::string parse_ip_address(uint32_t);
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
	std::wstring to_str(uint32_t notify);
}

namespace freeze::detail
{
    bool save_ip(uint32_t);
	uint32_t read_ip();
    bool save_latest_folder(std::wstring const&);
    std::wstring read_latest_folder();
	bool save_token(std::wstring const&);
	std::string read_token();
}

namespace freeze::detail
{
	std::vector<std::string> get_harddisks();
	std::vector<std::string> get_harddisk_names();
	std::vector<volume_type> get_volume_names();
	std::vector<std::string> get_directories_without_subdir(fs::path const& root);
}

#else
#define debug_output(...)
#endif

#endif
